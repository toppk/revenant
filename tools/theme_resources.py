"""Shared Ghostty/Xresources theme parsing for the converter and setter."""

import io
from pathlib import Path
import re
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_THEMES = ROOT / ".cache" / "ghostty-themes"
RESOURCE_NAMES = ("foreground", "background", "cursorColor") + tuple(
    f"color{i}" for i in range(16)
)
# xterm patch 411, charproc.c color resources, normal (non-reversed) Xt colors.
XTERM_COLORS = dict(
    zip(
        RESOURCE_NAMES,
        (
            "#000000",
            "#ffffff",
            "#000000",
            "#000000",
            "#cd0000",
            "#00cd00",
            "#cdcd00",
            "#0000ee",
            "#cd00cd",
            "#00cdcd",
            "#e5e5e5",
            "#7f7f7f",
            "#ff0000",
            "#00ff00",
            "#ffff00",
            "#5c5cff",
            "#ff00ff",
            "#00ffff",
            "#ffffff",
        ),
    )
)


def fetch(directory):
    source = (ROOT / "upstream/ghostty/build.zig.zon").read_text()
    match = re.search(r'\.iterm2_themes\s*=\s*\.\{\s*\.url\s*=\s*"([^"]+)"', source)
    if match is None:
        raise ValueError(
            "cannot find theme dependency in upstream/ghostty/build.zig.zon"
        )
    url = match[1]
    if not url.startswith("https://deps.files.ghostty.org/"):
        raise ValueError("unexpected Ghostty theme dependency host")
    archive = subprocess.run(
        ["curl", "--silent", "--show-error", "--fail", "--max-time", "30", url],
        check=True,
        stdout=subprocess.PIPE,
    ).stdout
    if len(archive) > 32 * 1024 * 1024:
        raise ValueError("theme archive exceeds 32 MiB")
    directory.mkdir(parents=True, exist_ok=True)
    count = 0
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as bundle:
        for member in bundle:
            parts = Path(member.name).parts
            if not member.isfile() or len(parts) != 2 or parts[0] != "ghostty":
                continue
            if parts[1] in (".", "..") or member.size > 64 * 1024:
                raise ValueError("invalid theme archive member")
            stream = bundle.extractfile(member)
            assert stream is not None
            (directory / parts[1]).write_bytes(stream.read())
            count += 1
    if not count:
        raise ValueError("no Ghostty themes in archive")
    print(f"Fetched {count} themes from {url} into {directory}", file=sys.stderr)


def parse_ghostty(path):
    colors = {}
    ignored = set()
    for number, line in enumerate(path.read_text().splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        key, value = key.strip(), value.strip()
        if not separator:
            raise ValueError(f"{path}:{number}: expected key = value")
        if key == "palette":
            index, separator, value = value.partition("=")
            if not separator or not index.strip().isdigit() or not 0 <= int(index) < 16:
                raise ValueError(f"{path}:{number}: expected palette index 0..15")
            resource = f"color{int(index)}"
        elif key in ("foreground", "background", "cursor-color"):
            resource = {"cursor-color": "cursorColor"}.get(key, key)
        else:
            ignored.add(key)
            continue
        value = value.strip().removeprefix("#")
        if not re.fullmatch(r"[0-9a-fA-F]{6}", value):
            raise ValueError(f"{path}:{number}: expected a six-digit RGB color")
        colors[resource] = "#" + value.lower()
    required = ["foreground", "background"] + [f"color{i}" for i in range(16)]
    missing = [name for name in required if name not in colors]
    if missing:
        raise ValueError("incomplete theme; missing " + ", ".join(missing))
    colors.setdefault("cursorColor", colors["foreground"])
    return colors, ignored


def convert(path, application_class):
    colors, ignored = parse_ghostty(path)
    return render(colors, application_class, path.name, ignored)


def render(colors, application_class, label="theme", ignored=()):
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]*", application_class):
        raise ValueError("invalid X resource application class")
    if any(
        not re.fullmatch(r"#[0-9a-fA-F]{6}", colors.get(name, ""))
        for name in RESOURCE_NAMES
    ):
        raise ValueError("theme must provide nineteen six-digit RGB colors")
    lines = ["! Theme: " + label.replace("\n", " ").replace("\r", " ")]
    if ignored:
        lines.append("! Unmapped settings: " + ", ".join(sorted(ignored)))
    for name in RESOURCE_NAMES:
        lines.append(f"{application_class}*{name}: {colors[name]}")
    return "\n".join(lines) + "\n"


def load_theme(path):
    """Import Ghostty or Xresources colors; never import other resource settings."""
    text = path.read_text()
    if not any(re.match(r"^[^!#\s][^:]*:", line) for line in text.splitlines()):
        return parse_ghostty(path)[0]
    colors = {}
    for line in text.splitlines():
        match = re.match(
            r"^[^!#\s][^:]*[.*](foreground|background|cursorColor|color[0-9]+)\s*:\s*(#[0-9a-fA-F]{6})\s*$",
            line,
        )
        if match and match[1] in RESOURCE_NAMES:
            colors[match[1]] = match[2].lower()
    required = set(RESOURCE_NAMES) - {"cursorColor"}
    if not required <= colors.keys():
        raise ValueError(
            "incomplete Xresources palette; foreground, background and color0..15 are required"
        )
    colors.setdefault("cursorColor", colors["foreground"])
    return colors
