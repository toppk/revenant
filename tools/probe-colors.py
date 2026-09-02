#!/usr/bin/env python3
"""
probe-colors.py - raw escape-sequence color and palette probe.

Sections
  16      SGR 30-37 / 90-97 / 40-47 / 100-107, bold-vs-bright, attributes
  palette read (OSC 4 query) and change colours 0-15, either live via OSC 4
          or by relaunching a fresh xterm with -xrm 'XTerm*colorN: #rrggbb'
  256     ESC[38;5;Nm   - system, 6x6x6 cube, 24-step grey ramp
  true    ESC[38;2;R;G;Bm - 24-bit ramps, plus a 256 vs 24-bit A/B check

Run with no arguments for the guided tour: one section per screen, Enter for
the next one.  -m 16|palette|256|true shows a single section, -m all dumps
every section at once.

No curses, no tput, no third-party modules: every byte written is a literal
escape sequence.  Pass -q to hide the sequence cheat-sheets.
"""

from __future__ import annotations

import argparse
import colorsys
import os
import re
import select
import shutil
import subprocess
import sys
import termios
import time
import tty

# --------------------------------------------------------------------------
# raw sequences
# --------------------------------------------------------------------------

ESC = "\x1b"
CSI = ESC + "["
OSC = ESC + "]"
ST = ESC + "\\"
BEL = "\x07"
RESET = CSI + "0m"


def sgr(*codes) -> str:
    return CSI + ";".join(str(c) for c in codes) + "m"


def fg256(n: int) -> str:
    return f"{CSI}38;5;{n}m"


def bg256(n: int) -> str:
    return f"{CSI}48;5;{n}m"


def fgrgb(r: int, g: int, b: int) -> str:
    return f"{CSI}38;2;{r};{g};{b}m"


def bgrgb(r: int, g: int, b: int) -> str:
    return f"{CSI}48;2;{r};{g};{b}m"


def osc4_set(n: int, spec: str) -> str:
    """OSC 4 ; index ; colour ST   - change one palette entry."""
    return f"{OSC}4;{n};{spec}{ST}"


def osc4_query(n: int) -> str:
    """OSC 4 ; index ; ? ST   - ask the terminal what it currently is."""
    return f"{OSC}4;{n};?{ST}"


def osc104_reset(n: int | None = None) -> str:
    """OSC 104 ST - reset the whole palette (or one entry)."""
    return f"{OSC}104{'' if n is None else ';' + str(n)}{ST}"


# --------------------------------------------------------------------------
# palettes
# --------------------------------------------------------------------------

def _verify_palette() -> list[str]:
    """Deliberately wrong colours: if you see this, the resources took."""
    out = []
    for i in range(16):
        h = ((i * 5) % 16) / 16.0
        v = 0.60 if i < 8 else 1.0
        r, g, b = colorsys.hsv_to_rgb(h, 0.85, v)
        out.append("#%02x%02x%02x" % (int(r * 255), int(g * 255), int(b * 255)))
    return out


PALETTES: dict[str, dict] = {
    "xterm": {
        "colors": ["#000000", "#cd0000", "#00cd00", "#cdcd00",
                   "#0000ee", "#cd00cd", "#00cdcd", "#e5e5e5",
                   "#7f7f7f", "#ff0000", "#00ff00", "#ffff00",
                   "#5c5cff", "#ff00ff", "#00ffff", "#ffffff"],
        "bg": "#000000", "fg": "#ffffff", "cursor": "#ffffff",
    },
    "tango": {
        "colors": ["#2e3436", "#cc0000", "#4e9a06", "#c4a000",
                   "#3465a4", "#75507b", "#06989a", "#d3d7cf",
                   "#555753", "#ef2929", "#8ae234", "#fce94f",
                   "#729fcf", "#ad7fa8", "#34e2e2", "#eeeeec"],
        "bg": "#2e3436", "fg": "#d3d7cf", "cursor": "#eeeeec",
    },
    "solarized": {
        "colors": ["#073642", "#dc322f", "#859900", "#b58900",
                   "#268bd2", "#d33682", "#2aa198", "#eee8d5",
                   "#002b36", "#cb4b16", "#586e75", "#657b83",
                   "#839496", "#6c71c4", "#93a1a1", "#fdf6e3"],
        "bg": "#002b36", "fg": "#839496", "cursor": "#93a1a1",
    },
    "gruvbox": {
        "colors": ["#282828", "#cc241d", "#98971a", "#d79921",
                   "#458588", "#b16286", "#689d6a", "#a89984",
                   "#928374", "#fb4934", "#b8bb26", "#fabd2f",
                   "#83a598", "#d3869b", "#8ec07c", "#ebdbb2"],
        "bg": "#282828", "fg": "#ebdbb2", "cursor": "#ebdbb2",
    },
    "nord": {
        "colors": ["#3b4252", "#bf616a", "#a3be8c", "#ebcb8b",
                   "#81a1c1", "#b48ead", "#88c0d0", "#e5e9f0",
                   "#4c566a", "#bf616a", "#a3be8c", "#ebcb8b",
                   "#81a1c1", "#b48ead", "#8fbcbb", "#eceff4"],
        "bg": "#2e3440", "fg": "#d8dee9", "cursor": "#d8dee9",
    },
    "verify": {
        "colors": _verify_palette(),
        "bg": "#101820", "fg": "#f0f0f0", "cursor": "#ff00ff",
    },
}

# black-on-swatch looks better for these indices, white for the rest
LIGHT_IDX = {2, 3, 6, 7, 10, 11, 14, 15}


# --------------------------------------------------------------------------
# small helpers
# --------------------------------------------------------------------------

def out(s: str = "") -> None:
    try:
        sys.stdout.write(s + "\n")
    except BrokenPipeError:
        os._exit(0)


def term_width(override: int | None = None) -> int:
    cols = shutil.get_terminal_size((80, 24)).columns
    return max(40, min(override or cols, cols))


def rule(title: str, width: int) -> None:
    bar = "-" * max(0, width - len(title) - 5)
    out("")
    out(f"{sgr(1)}== {title} {bar}{RESET}")


def codes(quiet: bool, *lines: str) -> None:
    if quiet:
        return
    for ln in lines:
        out(f"{sgr(2)}    {ln}{RESET}")


def hex_to_rgb(h: str) -> tuple[int, int, int]:
    h = h.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def rgb_to_256(r: int, g: int, b: int) -> int:
    """Nearest xterm-256 index - used for the truecolour A/B test."""
    if abs(r - g) < 8 and abs(g - b) < 8:
        if r < 8:
            return 16
        if r > 248:
            return 231
        return 232 + round((r - 8) / 247 * 23)
    return (16 + 36 * round(r / 255 * 5)
            + 6 * round(g / 255 * 5)
            + round(b / 255 * 5))


def halfblock_rows(width: int, height: int, fn, indent: str = "  ") -> None:
    """fn(x, y) -> (r,g,b); two pixel rows per text row via U+2580."""
    ch = "\u2580" if USE_UNICODE else "#"
    for y in range(0, height, 2):
        line = [indent]
        for x in range(width):
            tr, tg, tb = fn(x, y)
            br, bg_, bb = fn(x, min(y + 1, height - 1))
            if USE_UNICODE:
                line.append(f"{fgrgb(tr, tg, tb)}{bgrgb(br, bg_, bb)}{ch}")
            else:
                line.append(f"{bgrgb(tr, tg, tb)} ")
        out("".join(line) + RESET)


USE_UNICODE = True


# --------------------------------------------------------------------------
# OSC 4 palette query
# --------------------------------------------------------------------------

def parse_color_reply(index: int, reply: str) -> str | None:
    pattern = re.compile(
        rf"(?:\x1b\]|\x9d)4;{index};"
        r"rgba?:([0-9a-fA-F]+)/([0-9a-fA-F]+)/([0-9a-fA-F]+)"
        r"(?:\x1b\\|\x07)"
    )
    match = pattern.search(reply)
    if not match:
        return None
    parts = []
    for component in match.groups():
        scale = (1 << (4 * len(component))) - 1
        parts.append(round(int(component, 16) / scale * 255))
    return "#%02x%02x%02x" % tuple(parts)


def query_color(index: int, timeout: float = 0.25) -> str | None:
    """Send OSC 4;n;? and parse the reply. Returns '#rrggbb' or None."""
    if not (sys.stdin.isatty() and sys.stdout.isatty()):
        return None
    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        termios.tcflush(fd, termios.TCIFLUSH)
        sys.stdout.write(osc4_query(index))
        sys.stdout.flush()
        buf = ""
        deadline = time.time() + timeout
        while time.time() < deadline:
            remaining = deadline - time.time()
            if not select.select([fd], [], [], max(0.0, remaining))[0]:
                break
            buf += os.read(fd, 64).decode("latin-1")
            if parse_color_reply(index, buf) is not None:
                break
    except Exception:
        return None
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved)

    return parse_color_reply(index, buf)


def query_palette() -> list[str | None]:
    res: list[str | None] = []
    for i in range(16):
        c = query_color(i)
        res.append(c)
        if c is None and i == 0:            # terminal won't answer; give up
            return [None] * 16
    return res


# --------------------------------------------------------------------------
# test sections
# --------------------------------------------------------------------------

def test_16(w: int, quiet: bool) -> None:
    rule("16 colours - SGR 30-37 / 90-97 / 40-47 / 100-107", w)
    codes(quiet,
          r"fg  ESC[30m .. ESC[37m    bright fg  ESC[90m .. ESC[97m",
          r"bg  ESC[40m .. ESC[47m    bright bg  ESC[100m .. ESC[107m",
          r"off ESC[0m")

    for label, base, brt in (("bg", 40, 100), ("fg", 30, 90)):
        out("")
        for lo, hi, tag in ((0, 8, base), (8, 16, brt)):
            row = f"  {label} "
            for i in range(lo, hi):
                num = tag + (i - lo)
                if label == "bg":
                    text = sgr(30 if i in LIGHT_IDX else 97, num)
                else:
                    text = sgr(num)
                row += f"{text} {num:>3} {RESET} "
            out(row)

    out("")
    out("  bold vs bright (some terminals map ESC[1;3Xm onto colours 8-15):")
    a = "  " + "".join(f"{sgr(1, 30 + i)}[1;{30 + i}m{RESET} " for i in range(8))
    b = "  " + "".join(f"{sgr(90 + i)}[{90 + i}m{RESET}  " for i in range(8))
    out(a)
    out(b)

    out("")
    out("  attributes:")
    attrs = [(1, "bold"), (2, "dim"), (3, "italic"), (4, "underline"),
             (5, "blink"), (7, "reverse"), (8, "conceal"), (9, "strike"),
             (21, "2xunder"), (53, "overline")]
    line = "  "
    for n, name in attrs:
        line += f"{sgr(n)}{name}{RESET}({n}) "
    out(line)
    codes(quiet, r"ESC[1m ESC[2m ESC[3m ESC[4m ESC[5m ESC[7m ESC[9m ESC[53m")


def test_palette(w: int, quiet: bool, args, applied: str | None) -> None:
    rule("palette 0-15 - OSC 4 query / set", w)
    codes(quiet,
          r"query  ESC]4;<n>;?ESC\        reply: ESC]4;<n>;rgb:rrrr/gggg/bbbbESC\ ",
          r"set    ESC]4;<n>;#rrggbbESC\   reset: ESC]104ESC\ ",
          r"xrm    xterm -xrm 'XTerm*color<n>: #rrggbb'")

    live = query_palette()
    out("")
    if live[0] is None:
        out(f"  {sgr(3)}(terminal did not answer the OSC 4 query - "
            f"showing swatches only){RESET}")
    for lo, hi in ((0, 8), (8, 16)):
        row = "  "
        for i in range(lo, hi):
            sw = sgr(30 if i in LIGHT_IDX else 97, (40 + i) if i < 8 else (92 + i))
            row += f"{sw} {i:>2} {RESET}"
            row += f" {live[i] or '???????'}  "
        out(row)

    if applied:
        out("")
        out(f"  applied palette {sgr(1)}{applied}{RESET} to this terminal "
            f"with OSC 4 (undo: --reset-palette)")

    out("")
    out("  equivalent xterm command line:")
    name = applied or args.palette or "verify"
    for chunk in wrap_cmd(build_spawn_cmd(args, "palette", palette_name=name), w - 4):
        out(f"  {sgr(2)}{chunk}{RESET}")


def test_256(w: int, quiet: bool, compact: bool = False) -> None:
    rule("256 colours - ESC[38;5;Nm / ESC[48;5;Nm", w)
    codes(quiet,
          r"fg ESC[38;5;<0-255>m     bg ESC[48;5;<0-255>m",
          r"0-15 system   16-231 6x6x6 cube (16+36r+6g+b)   232-255 greys")

    out("")
    out("  system 0-15:")
    for lo, hi in ((0, 8), (8, 16)):
        row = "  "
        for i in range(lo, hi):
            row += f"{bg256(i)}{fg256(0 if i in LIGHT_IDX else 15)} {i:>3} {RESET}"
        out(row)

    out("")
    out("  cube 16-231:")
    for g in range(6):
        row = "  "
        for r in range(6):
            for b in range(6):
                n = 16 + 36 * r + 6 * g + b
                row += f"{bg256(n)}  {RESET}"
            row += " "
        out(row)

    out("")
    out("  greys 232-255:")
    row = "  "
    for n in range(232, 256):
        row += f"{bg256(n)}  {RESET}"
    out(row + f"  {sgr(2)}232 -> 255{RESET}")

    if compact:
        return

    out("")
    out("  numbered cube (fg on default bg):")
    for start in range(16, 232, 24):
        row = "  "
        for n in range(start, min(start + 24, 232)):
            row += f"{fg256(n)}{n:>4}{RESET}"
        out(row)


def test_true(w: int, quiet: bool, rows: int | None = None) -> None:
    rule("24-bit truecolour - ESC[38;2;R;G;Bm / ESC[48;2;R;G;Bm", w)
    codes(quiet,
          r"fg ESC[38;2;<r>;<g>;<b>m    bg ESC[48;2;<r>;<g>;<b>m",
          r"xterm needs patch #331+ and -xrm 'XTerm*directColor: true'",
          f"TERM={os.environ.get('TERM', '?')}  "
          f"COLORTERM={os.environ.get('COLORTERM', '(unset)')}")

    gw = w - 4

    out("")
    out("  channel ramps:")
    for name, mk in (("red  ", lambda t: (t, 0, 0)),
                     ("green", lambda t: (0, t, 0)),
                     ("blue ", lambda t: (0, 0, t)),
                     ("grey ", lambda t: (t, t, t))):
        row = f"  {name} "
        for x in range(gw - 8):
            t = round(x / max(1, gw - 9) * 255)
            row += bgrgb(*mk(t)) + " "
        out(row + RESET)

    out("")
    out("  hue sweep (saturation top -> bottom):")

    sweep = 8 if not rows or rows >= 34 else (6 if rows >= 28 else 4)

    def hue_fn(x, y, h=sweep):
        hu = x / max(1, gw - 1)
        sat = 0.25 + 0.75 * (y / max(1, h - 1))
        r, g, b = colorsys.hsv_to_rgb(hu, sat, 1.0)
        return int(r * 255), int(g * 255), int(b * 255)

    halfblock_rows(gw - 2, sweep, hue_fn)

    out("")
    out("  A/B check - top row is 24-bit, bottom row is the nearest 256 index.")
    out("  If the two rows look identical (banded), truecolour is NOT active:")
    for label, quant in (("24bit", False), ("256  ", True)):
        row = f"  {label} "
        for x in range(gw - 8):
            t = x / max(1, gw - 9)
            r, g, b = colorsys.hsv_to_rgb(0.58, 0.55, 0.25 + 0.7 * t)
            r, g, b = int(r * 255), int(g * 255), int(b * 255)
            row += (bg256(rgb_to_256(r, g, b)) if quant else bgrgb(r, g, b)) + " "
        out(row + RESET)

    out("")
    out("  adjacent 1/255 steps (smooth = real 24-bit):")
    row = "  "
    for x in range(min(gw - 2, 64)):
        v = 96 + x
        row += bgrgb(v, v, v) + " "
    out(row + RESET)


# --------------------------------------------------------------------------
# --query report
# --------------------------------------------------------------------------

def do_query(args) -> int:
    """Show each slot as the terminal draws it, as 24-bit, and vs the default."""
    live = query_palette()
    defaults = PALETTES["xterm"]["colors"]
    ct = os.environ.get("COLORTERM", "")

    out("")
    out(f"  {sgr(1)}slot     drawn   live (24-bit)    "
        f"xterm default (24-bit){RESET}")
    out(f"  {sgr(2)}{'-' * 61}{RESET}")

    changed, unknown = 0, 0
    for i in range(16):
        got, dflt = live[i], defaults[i]
        drawn = f"{bg256(i)}      {RESET}"                 # via the palette
        dsw = f"{bgrgb(*hex_to_rgb(dflt))}      {RESET}"   # literal RGB

        if got is None:
            unknown += 1
            out(f"  color{i:<4}{drawn}  {sgr(3)}(no reply){RESET}"
                f"{' ' * 7}{dsw} {dflt}")
            continue

        lsw = f"{bgrgb(*hex_to_rgb(got))}      {RESET}"
        same = got.lower() == dflt.lower()
        if not same:
            changed += 1
        mark = (f"{sgr(2)}=  default{RESET}" if same
                else f"{sgr(1)}!= CHANGED{RESET}")
        out(f"  color{i:<4}{drawn}  {lsw} {got}   {dsw} {dflt}   {mark}")

    out("")
    if unknown == 16:
        out(f"  {sgr(1)}terminal did not answer OSC 4 queries{RESET} - the "
            f"live column is unavailable.")
        out(f"  {sgr(2)}xterm needs allowColorOps; tmux/screen usually eat the "
            f"reply.{RESET}")
    else:
        verdict = (f"{sgr(1)}{changed} of 16 differ from xterm's compiled-in "
                   f"defaults{RESET}" if changed
                   else f"all 16 match xterm's compiled-in defaults")
        out(f"  {verdict}")

    out("")
    out(f"  {sgr(2)}'drawn' uses ESC[48;5;<n>m, so it is whatever this window "
        f"actually paints.{RESET}")
    out(f"  {sgr(2)}the two 24-bit columns use ESC[48;2;r;g;bm and ignore the "
        f"palette entirely.{RESET}")
    if not ct:
        out(f"  {sgr(2)}COLORTERM is unset: if the 24-bit columns look banded, "
            f"they are being{RESET}")
        out(f"  {sgr(2)}approximated and cannot be trusted - "
            f"retry with --spawn --direct-color.{RESET}")
    out(RESET)
    return 0


# --------------------------------------------------------------------------
# tour - one section per screen, Enter to advance
# --------------------------------------------------------------------------

CLEAR = CSI + "H" + CSI + "2J" + CSI + "3J"

_KEYMAP = {
    "\r": "next", "\n": "next", " ": "next", "n": "next", "j": "next", "l": "next",
    "p": "prev", "b": "prev", "k": "prev", "h": "prev",
    "q": "quit", "\x03": "quit", "\x04": "quit",
    "r": "redraw",
}


def read_key() -> str:
    """Block for one keystroke; return next / prev / quit / redraw / goto:N."""
    if not sys.stdin.isatty():
        try:
            line = input()
        except (EOFError, KeyboardInterrupt):
            return "quit"
        return _KEYMAP.get(line.strip().lower()[:1] or "\r", "next")

    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = os.read(fd, 1).decode("latin-1")
        if ch == ESC:                                   # arrow key or bare ESC
            seq = ""
            while len(seq) < 4 and select.select([fd], [], [], 0.05)[0]:
                seq += os.read(fd, 1).decode("latin-1")
            if seq[-1:] in ("C", "B"):
                return "next"
            if seq[-1:] in ("D", "A"):
                return "prev"
            return "quit"
    except (OSError, ValueError):
        return "quit"
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved)

    if ch.isdigit():
        return "goto:" + ch
    return _KEYMAP.get(ch.lower(), "next")


def _palette_page(name: str, w: int, args):
    def draw():
        for i, c in enumerate(PALETTES[name]["colors"]):
            sys.stdout.write(osc4_set(i, c))
        sys.stdout.flush()
        test_palette(w, args.quiet, args, name)
        test_16(w, True)
    return draw


def run_tour(args) -> int:
    w = term_width(args.width)
    rows = shutil.get_terminal_size((80, 24)).lines
    compact = args.compact or rows < 40

    pages: list[tuple[str, object]] = [
        ("16 colours", lambda: test_16(w, args.quiet)),
        ("palette 0-15", lambda: test_palette(w, args.quiet, args, None)),
    ]
    if args.all_palettes:
        for name in PALETTES:
            pages.append((f"palette: {name}", _palette_page(name, w, args)))
    pages += [
        ("256 colours", lambda: test_256(w, args.quiet, compact)),
        ("24-bit truecolour", lambda: test_true(w, args.quiet, rows)),
    ]

    n = len(pages)
    i = 0
    try:
        while 0 <= i < n:
            title, draw = pages[i]
            if not args.no_clear:
                sys.stdout.write(CLEAR)
            out(f"{sgr(1)}probe-colors{RESET}  "
                f"TERM={os.environ.get('TERM', '?')}  "
                f"COLORTERM={os.environ.get('COLORTERM', '(unset)')}  {w}x{rows}")
            draw()

            nxt = "finish" if i == n - 1 else "next"
            out("")
            out(f"{sgr(7)} {i + 1}/{n}  {title} {RESET}"
                f"{sgr(2)}  [Enter] {nxt}   [p] back   [q] quit   "
                f"[1-{min(n, 9)}] jump{RESET}")

            key = read_key()
            if key == "quit":
                break
            if key == "next":
                i += 1
            elif key == "prev":
                i = max(0, i - 1)
            elif key.startswith("goto:"):
                j = int(key[5:]) - 1
                if 0 <= j < n:
                    i = j
    finally:
        sys.stdout.write(RESET)
        if args.all_palettes:
            sys.stdout.write(osc104_reset())        # put the palette back
        sys.stdout.flush()
        out("")
    return 0


# --------------------------------------------------------------------------
# xterm -xrm spawning
# --------------------------------------------------------------------------

def xrm_args(args, palette_name: str | None) -> list[str]:
    res: list[str] = []
    cls = args.resource_class

    if palette_name:
        p = PALETTES[palette_name]
        for i, c in enumerate(p["colors"]):
            res += ["-xrm", f"{cls}*color{i}: {c}"]
        res += ["-xrm", f"{cls}*background: {p['bg']}"]
        res += ["-xrm", f"{cls}*foreground: {p['fg']}"]
        res += ["-xrm", f"{cls}*cursorColor: {p['cursor']}"]

    if args.direct_color:
        res += ["-xrm", f"{cls}*directColor: true"]
    if args.term_name:
        res += ["-xrm", f"{cls}*termName: {args.term_name}"]
    if args.font:
        res += ["-xrm", f"{cls}*faceName: {args.font}"]
    if args.font_size:
        res += ["-xrm", f"{cls}*faceSize: {args.font_size}"]
    if args.geometry:
        res += ["-xrm", f"{cls}*geometry: {args.geometry}"]
    res += ["-xrm", f"{cls}*saveLines: 4096"]

    for extra in args.xrm:
        res += ["-xrm", extra]
    return res


def script_path() -> str:
    p = os.path.abspath(sys.argv[0])
    if not os.path.isfile(p):
        sys.exit("cannot locate this script on disk; run it as a file to use --spawn")
    return p


def build_spawn_cmd(args, mode: str, palette_name: str | None = None) -> list[str]:
    cmd = [args.program]
    cmd += xrm_args(args, palette_name)
    if args.hold:
        cmd.append("-hold")
    cmd += [args.exec_flag, sys.executable, script_path(),
            "--mode", mode, "--no-spawn"]
    if args.query:
        cmd.append("--query")
    if args.palette:
        cmd += ["--palette", args.palette]
    if mode != "tour":
        cmd.append("--wait")
    for flag, on in (("--all-palettes", args.all_palettes),
                     ("--no-clear", args.no_clear),
                     ("--compact", args.compact)):
        if on:
            cmd.append(flag)
    if args.width:
        cmd += ["--width", str(args.width)]
    if args.quiet:
        cmd.append("--quiet")
    if args.ascii:
        cmd.append("--ascii")
    return cmd


def shquote(s: str) -> str:
    return s if re.fullmatch(r"[\w@%+=:,./-]+", s) else "'" + s.replace("'", "'\\''") + "'"


def wrap_cmd(cmd: list[str], width: int) -> list[str]:
    lines, cur = [], ""
    for tok in (shquote(c) for c in cmd):
        if cur and len(cur) + len(tok) + 3 > width:
            lines.append(cur + " \\")
            cur = "    " + tok
        else:
            cur = tok if not cur else cur + " " + tok
    lines.append(cur)
    return lines


def do_spawn(args, mode: str) -> int:
    palette_name = args.palette if args.palette else ("verify" if mode == "palette" else None)
    cmd = build_spawn_cmd(args, mode, palette_name)

    if args.dry_run:
        for ln in wrap_cmd(cmd, term_width(args.width) - 2):
            out(ln)
        return 0
    if shutil.which(args.program) is None:
        sys.exit(f"{args.program}: not found in PATH")
    try:
        return subprocess.run(cmd).returncode
    except KeyboardInterrupt:
        return 130


def self_test() -> int:
    good = "noise\x1b]4;7;rgb:1212/3434/5656\x1b\\tail"
    wrong_index = "\x1b]4;6;rgb:ffff/0000/0000\x1b\\"
    if parse_color_reply(7, good) != "#123456":
        return 1
    if parse_color_reply(7, wrong_index) is not None:
        return 1
    args = argparse.Namespace(
        program="xterm+", resource_class="XTerm", palette="solarized",
        direct_color=True, term_name=None, font=None, font_size=None,
        geometry=None, xrm=[], hold=False, exec_flag="-e", query=True,
        all_palettes=False, no_clear=False, compact=False, width=None,
        quiet=False, ascii=False,
    )
    command = build_spawn_cmd(args, "tour", "solarized")
    if "--query" not in command:
        return 1
    if command[command.index("--palette") + 1] != "solarized":
        return 1
    return 0


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

MODES = {"16": "16", "palette": "palette", "256": "256",
         "true": "true", "truecolor": "true", "24bit": "true", "rgb": "true",
         "all": "all", "tour": "tour", "total": "tour"}


def main() -> int:
    global USE_UNICODE

    ap = argparse.ArgumentParser(
        prog="probe-colors.py",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=__doc__)

    ap.add_argument("--mode", "-m", default="tour", choices=sorted(MODES),
                    help="which test to run; 'tour'/'total' walks the sections "
                         "one screen at a time, 'all' dumps them (default: tour)")
    ap.add_argument("--width", "-w", type=int, help="force output width")
    ap.add_argument("--quiet", "-q", action="store_true",
                    help="hide the escape-sequence cheat sheets")
    ap.add_argument("--ascii", action="store_true",
                    help="avoid U+2580 half blocks")
    ap.add_argument("--wait", action="store_true",
                    help="pause for Enter at the end (used when spawning)")

    g = ap.add_argument_group("tour")
    g.add_argument("--all-palettes", action="store_true",
                   help="add one tour page per built-in palette (live, via OSC 4)")
    g.add_argument("--no-clear", action="store_true",
                   help="do not clear the screen between tour pages")
    g.add_argument("--compact", action="store_true",
                   help="shorten long sections to fit a small window")

    g = ap.add_argument_group("palette")
    g.add_argument("--palette", "-p", choices=sorted(PALETTES),
                   help="palette to install (default for --spawn palette: verify)")
    g.add_argument("--list-palettes", action="store_true")
    g.add_argument("--apply-palette", action="store_true",
                   help="install --palette into the CURRENT terminal via OSC 4")
    g.add_argument("--reset-palette", action="store_true",
                   help="send OSC 104 to restore the terminal's own palette")
    g.add_argument("--query", action="store_true",
                   help="just print colours 0-15 as reported by OSC 4")

    g = ap.add_argument_group("new terminal (-xrm)")
    g.add_argument("--spawn", "-s", action="store_true",
                   help="relaunch this test in a new terminal with -xrm resources")
    g.add_argument("--program", default="xterm",
                   help="terminal program to spawn (default: xterm)")
    g.add_argument("--exec-flag", default="-e",
                   help="flag that introduces the command (default: -e)")
    g.add_argument("--resource-class", default="XTerm",
                   help="resource prefix for -xrm (default: XTerm)")
    g.add_argument("--xrm", action="append", default=[], metavar="RES",
                   help="extra -xrm resource, repeatable")
    g.add_argument("--direct-color", action="store_true",
                   help="-xrm 'XTerm*directColor: true' (needs xterm #331+)")
    g.add_argument("--term-name", metavar="TERM",
                   help="-xrm 'XTerm*termName: ...', e.g. xterm-direct")
    g.add_argument("--font", help="-xrm 'XTerm*faceName: ...'")
    g.add_argument("--font-size", help="-xrm 'XTerm*faceSize: ...'")
    g.add_argument("--geometry", help="-xrm 'XTerm*geometry: 100x40'")
    g.add_argument("--hold", action="store_true", help="pass -hold to xterm")
    g.add_argument("--dry-run", action="store_true",
                   help="print the command line instead of running it")
    g.add_argument("--no-spawn", action="store_true", help=argparse.SUPPRESS)
    ap.add_argument("--self-test", action="store_true", help=argparse.SUPPRESS)

    args = ap.parse_args()
    mode = MODES[args.mode]
    USE_UNICODE = not args.ascii

    if args.self_test:
        return self_test()

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    if args.list_palettes:
        for name, p in PALETTES.items():
            sw = "".join(bgrgb(*hex_to_rgb(c)) + "  " for c in p["colors"])
            out(f"  {name:<10} {sw}{RESET}")
        return 0

    if args.reset_palette:
        sys.stdout.write(osc104_reset())
        sys.stdout.flush()
        out("sent OSC 104 (palette reset)")
        return 0

    if args.spawn and not args.no_spawn:
        return do_spawn(args, mode)

    if args.query:
        return do_query(args)

    applied = None
    if args.apply_palette:
        name = args.palette or "verify"
        for i, c in enumerate(PALETTES[name]["colors"]):
            sys.stdout.write(osc4_set(i, c))
        sys.stdout.flush()
        applied = name

    if mode == "tour":
        return run_tour(args)

    w = term_width(args.width)
    out(f"{sgr(1)}probe-colors{RESET}  TERM={os.environ.get('TERM', '?')}  "
        f"COLORTERM={os.environ.get('COLORTERM', '(unset)')}  width={w}")

    if mode in ("16", "all"):
        test_16(w, args.quiet)
    if mode in ("palette", "all"):
        test_palette(w, args.quiet, args, applied)
    if mode in ("256", "all"):
        test_256(w, args.quiet)
    if mode in ("true", "all"):
        test_true(w, args.quiet)

    out(RESET)
    if args.wait:
        try:
            input("press Enter to close ")
        except (EOFError, KeyboardInterrupt):
            pass
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.stdout.write(RESET + "\n")
        sys.exit(130)
