#!/usr/bin/env python3
"""Capture and measure text rendering for the linear-light readability study.

  text-contrast-study.py capture OUT --terminal PATH [--variant :PATTERN ...]
  text-contrast-study.py analyze OUT

capture runs the terminal under the X display in $DISPLAY with a controlled font
universe (staged DejaVu Sans Mono and Noto Emoji 3.003, packaged JetBrains Mono
Regular/Italic) and saves one screenshot plus geometry per face, size, theme and
fontconfig variant. analyze locates the grid from a control row of background-only
cells, recovers each pixel's coverage from the encoded blend, reports a calculated
linear-light contrast per unit of coverage, and writes the capture above a
simulation that blends the same coverage in linear light (piecewise sRGB). See
docs/maintainers/linear-light-study.md.
"""

import argparse
import glob
import hashlib
import json
import os
import re
import subprocess
import tempfile
import time
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FONTS = {
    "DejaVuSansMono.ttf": ROOT / "font-fixtures-stage/fonts/DejaVuSansMono.ttf",
    "DejaVuSansMono-Oblique.ttf": ROOT
    / "font-fixtures-stage/fonts/DejaVuSansMono-Oblique.ttf",
    "NotoEmoji-Regular-3.003.ttf": ROOT
    / "font-fixtures-stage/fonts/NotoEmoji-Regular-3.003.ttf",
    "JetBrainsMono-Regular.otf": Path(
        "/usr/share/fonts/jetbrains-mono-fonts/JetBrainsMono-Regular.otf"
    ),
    "JetBrainsMono-Italic.otf": Path(
        "/usr/share/fonts/jetbrains-mono-fonts/JetBrainsMono-Italic.otf"
    ),
}
FACES = {"dejavu": "DejaVu Sans Mono", "jetbrains": "JetBrains Mono"}
SIZES = (7, 7.5, 8.25, 9)
THEMES = {
    "dark-on-light": ("#000000", "#ffffff"),
    "light-on-dark": ("#ffffff", "#000000"),
}
# The probe's "text contrast" lines, then a control row whose red-background cells
# (columns 0 and 2) locate the grid independently of any glyph ink.
CORPUS = (
    "\x1b[?25l"
    "Hamburgefonstiv 0Oo1lI|{}[]\r\n"
    "cafe\u0301 n\u0303 a\u0308 q\u0323\u0307 e\u0302\u0323\r\n"
    "\x1b[3mHamburgefonstiv 0Oo1lI|{}[]\x1b[23m\r\n"
    " \U0001f6e0 \U0001f6e0\ufe0e x\U0001f6e0x\r\n"
    "\x1b[41m \x1b[49m \x1b[41m \x1b[49m"
)
ROWS = ["ordinary", "combining", "italic", "emoji"]
CONTROL_ROW = 4
EMOJI_COLUMNS = [1, 3]  # columns 0, 2 and 4 are blank
COLUMNS = 34


def universe(work):
    fonts = work / "universe/fonts"
    fonts.mkdir(parents=True, exist_ok=True)
    for name, source in FONTS.items():
        if not source.exists():
            raise SystemExit(
                f"missing {source}; stage fixtures with tools/stage-font-fixtures"
            )
        target = fonts / name
        if not target.exists():
            target.symlink_to(source)
    conf = work / "universe/fontconfig.conf"
    conf.write_text(
        '<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
        '<fontconfig>\n  <dir prefix="relative">fonts</dir>\n'
        '  <cachedir prefix="xdg">fontconfig</cachedir>\n</fontconfig>\n'
    )
    return conf


def capture_one(terminal, out, conf, face, size, theme, variant):
    fg, bg = THEMES[theme]
    label = f"{face}-{size}-{theme}" + (
        f"-{variant.strip(':').replace('=', '')}" if variant else ""
    )
    log = out / f"{label}.log"
    home = out / "home"
    home.mkdir(exist_ok=True)
    env = dict(
        os.environ,
        FONTCONFIG_FILE=str(conf),
        XDG_CACHE_HOME=str(conf.parent / "cache"),
        HOME=str(home),
        XENVIRONMENT="/dev/null",
        XFILESEARCHPATH="/dev/null",
    )
    command = [
        str(terminal),
        "-debug",
        "+sb",
        "-fa",
        FACES[face] + variant,
        "-fs",
        str(size),
        "-fg",
        fg,
        "-bg",
        bg,
        "-e",
        "sh",
        "-c",
        'printf "%s" "$0"; sleep 60',
        CORPUS,
    ]
    with open(log, "w") as err:
        proc = subprocess.Popen(command, env=env, stdout=subprocess.DEVNULL, stderr=err)
    try:
        window = None
        for _ in range(200):
            found = re.search(r"shell: realized window=(0x[0-9a-f]+)", log.read_text())
            if found:
                window = found.group(1)
                break
            time.sleep(0.05)
        if window is None:
            raise SystemExit(f"{label}: the terminal did not map a window")
        time.sleep(2.0)
        text = log.read_text()
        tree = subprocess.run(
            ["xwininfo", "-tree", "-id", window], capture_output=True, text=True
        ).stdout
        shell = subprocess.run(
            ["xwininfo", "-id", window], capture_output=True, text=True
        ).stdout
        border = re.search(r"Border width: (\d+)", shell)
        children = [
            [int(v) for v in m]
            for m in re.findall(r"\d+x\d+\+(-?\d+)\+(-?\d+)\s+\+-?\d+\+-?\d+", tree)
        ]
        with tempfile.NamedTemporaryFile(suffix=".xwd") as dump:
            subprocess.run(
                ["xwd", "-silent", "-id", window, "-out", dump.name], check=True
            )
            subprocess.run(["magick", dump.name, str(out / f"{label}.png")], check=True)
        cell = re.search(r"VT100 resolved renderer=(\S+) .* cell=(\d+)x(\d+)", text)
        fitted = re.findall(
            r"fitted Xft face span=(\d+) max-advance=(\d+) scale=(\S+)", text
        )
        info = {
            "label": label,
            "face": FACES[face] + variant,
            "faceSize": size,
            "theme": theme,
            "fg": fg,
            "bg": bg,
            "renderer": cell.group(1),
            "shell_border": int(border.group(1)) if border else None,
            "child_offsets": children,
            "cell": [int(cell.group(2)), int(cell.group(3))],
            "fitted": [
                {"span": int(a), "max_advance": int(b), "scale": float(c)}
                for a, b, c in fitted
            ],
        }
        (out / f"{label}.json").write_text(json.dumps(info, indent=1) + "\n")
        return info
    finally:
        proc.kill()
        proc.wait()


def to_linear(value):
    value /= 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def to_encoded(value):
    encoded = (
        value * 12.92 if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055
    )
    return max(0, min(255, round(encoded * 255)))


def is_control(pixel):
    red, green, blue = pixel
    return red > 120 and green < 60 and blue < 60


def grid_origin(image, cell):
    """The grid origin and cell size measured from the control row's red cells."""
    red = [
        (x, y)
        for y in range(image.height)
        for x in range(image.width)
        if is_control(image.getpixel((x, y)))
    ]
    if not red:
        raise SystemExit("control row not found")
    xs = sorted({x for x, _ in red})
    ys = sorted({y for _, y in red})
    first = [x for x in xs if x < xs[0] + cell[0] + 1]
    runs = [x for x in xs if x not in first]
    measured = {
        "origin": [xs[0], ys[0] - CONTROL_ROW * (ys[-1] - ys[0] + 1)],
        "cell_width": first[-1] - first[0] + 1,
        "cell_height": ys[-1] - ys[0] + 1,
        "second_control_x": runs[0] if runs else None,
    }
    # Column 2 starts two cells after column 0; both sizes must match the log.
    ok = (measured["cell_width"], measured["cell_height"]) == tuple(cell) and measured[
        "second_control_x"
    ] == xs[0] + 2 * cell[0]
    measured["consistent"] = ok
    if not ok:
        raise SystemExit(
            f"control cells disagree with the logged cell {cell}: {measured}"
        )
    return measured


def analyze_one(path):
    info = json.loads(Path(str(path)[:-4] + ".json").read_text())
    image = Image.open(path).convert("RGB")
    cw, ch = info["cell"]
    grid = grid_origin(image, info["cell"])
    ox, oy = grid["origin"]
    fg, bg = int(info["fg"][1:3], 16), int(info["bg"][1:3], 16)
    lf, lb = to_linear(fg), to_linear(bg)
    simulated = image.copy()
    rows = {}
    non_gray = 0
    for r, name in enumerate(ROWS):
        coverage = contrast = partial = inked = 0.0
        deltas = []
        for y in range(oy + r * ch, oy + (r + 1) * ch):
            for x in range(ox, ox + COLUMNS * cw):
                red, green, blue = image.getpixel((x, y))
                non_gray += not (red == green == blue)
                # XRender composes in encoded values, so the blend inverts to coverage exactly.
                alpha = max(0.0, min(1.0, (green - bg) / (fg - bg)))
                if alpha <= 0:
                    continue
                inked += 1
                partial += alpha < 1
                coverage += alpha
                contrast += (to_linear(green) - lb) / (lf - lb)
                linear = to_encoded(alpha * lf + (1 - alpha) * lb)
                simulated.putpixel((x, y), (linear, linear, linear))
                if alpha < 1:
                    deltas.append(abs(linear - green))
        rows[name] = {
            "coverage": round(coverage, 1),
            "linear_contrast_per_coverage": round(contrast / coverage, 3)
            if coverage
            else None,
            "partial_fraction": round(partial / inked, 3) if inked else None,
            "mean_delta_partial": round(sum(deltas) / len(deltas), 1) if deltas else 0,
            "max_delta": max(deltas) if deltas else 0,
        }
    top = oy + 3 * ch
    emoji = {}
    for column in EMOJI_COLUMNS:
        left = ox + column * cw
        # Coverage per pixel column across the blank cell before, the cell, and the blank cell after.
        per_column = [
            round(
                sum(
                    abs(image.getpixel((left + offset, y))[1] - bg)
                    for y in range(top, top + ch)
                )
                / 255
                / abs(fg - bg)
                * 255,
                2,
            )
            for offset in range(-cw, 2 * cw)
        ]
        inside = per_column[cw : 2 * cw]
        emoji[column] = {
            "columns": per_column,
            "outside_left": round(sum(per_column[:cw]), 2),
            "outside_right": round(sum(per_column[2 * cw :]), 2),
            "first_column": inside[0],
            "last_column": inside[-1],
        }
    width = ox + COLUMNS * cw
    height = oy + 4 * ch
    pair = Image.new("RGB", (width, height * 2 + 4), (128, 128, 128))
    pair.paste(image.crop((0, 0, width, height)), (0, 0))
    pair.paste(simulated.crop((0, 0, width, height)), (0, height + 4))
    pair.resize((pair.width * 3, pair.height * 3), Image.NEAREST).save(
        str(path)[:-4] + "-vs-linear.png"
    )
    return {
        "label": info["label"],
        "cell": info["cell"],
        "grid": grid,
        "fitted": info["fitted"],
        "rows": rows,
        "emoji": emoji,
        "non_gray_pixels": non_gray,
    }


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="action", required=True)
    capture = sub.add_parser("capture")
    capture.add_argument("out", type=Path)
    capture.add_argument(
        "--terminal", type=Path, required=True, help="terminal binary to capture"
    )
    capture.add_argument(
        "--variant",
        action="append",
        default=None,
        help="fontconfig suffix for the face, such as :hintstyle=1; repeatable",
    )
    analyze = sub.add_parser("analyze")
    analyze.add_argument("out", type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    if args.action == "capture":
        conf = universe(args.out)
        manifest = {
            name: hashlib.sha256(path.read_bytes()).hexdigest()
            for name, path in FONTS.items()
        }
        (args.out / "fonts.json").write_text(json.dumps(manifest, indent=1) + "\n")
        for variant in args.variant or [""]:
            for face in FACES:
                for size in SIZES:
                    for theme in THEMES:
                        info = capture_one(
                            args.terminal, args.out, conf, face, size, theme, variant
                        )
                        print(
                            info["label"],
                            "cell=%dx%d" % tuple(info["cell"]),
                            info["fitted"],
                        )
        return
    results = [
        analyze_one(p)
        for p in sorted(glob.glob(str(args.out / "*.png")))
        if not p.endswith("-vs-linear.png")
    ]
    (args.out / "summary.json").write_text(json.dumps(results, indent=1) + "\n")
    print(
        f"{'capture':40} origin cell  row       coverage lin/cov partial dmean  "
        "emoji: outside-left outside-right first-col last-col"
    )
    for result in results:
        for name in ("ordinary", "italic", "emoji"):
            row = result["rows"][name]
            emoji = result["emoji"][1]
            extra = (
                f"{emoji['outside_left']} {emoji['outside_right']} "
                f"{emoji['first_column']} {emoji['last_column']}"
                if name == "emoji"
                else ""
            )
            print(
                f"{result['label']:40} {result['grid']['origin'][0]},{result['grid']['origin'][1]}"
                f"    {result['cell'][0]}x{result['cell'][1]:<3} {name:9} "
                f"{row['coverage']:8} {row['linear_contrast_per_coverage']:7} "
                f"{row['partial_fraction']:7} {row['mean_delta_partial']:5}  {extra}"
            )


if __name__ == "__main__":
    main()
