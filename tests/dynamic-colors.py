#!/usr/bin/env python3
"""Drive OSC 10/11/12 sets, resets, queries, and color-scheme reports.

The harness samples pixels at each file checkpoint: column 0 is an empty
cell (default background), columns 2-3 are inverse spaces (default
foreground as background), and the cursor rests on column 4 with
alwaysHighlight so its block shows the cursor color.  Replies are read in
raw mode and summarized as result titles at the end.
"""

import os
from pathlib import Path
import select
import sys
import termios
import time
import tty

case_dir = Path(sys.argv[1])
phase = sys.argv[2]
BEL = b"\x07"
ST = b"\x1b\\"


def write_all(data: bytes) -> None:
    offset = 0
    while offset < len(data):
        offset += os.write(1, data[offset:])


def osc(code: int, payload: str = "", end: bytes = ST) -> None:
    write_all(f"\x1b]{code}".encode() + (b";" + payload.encode() if payload else b"") + end)


def read_until(terminator: bytes, timeout: float) -> bytes:
    reply = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and not reply.endswith(terminator):
        ready, _, _ = select.select([0], [], [], max(0.0, deadline - time.monotonic()))
        if ready:
            reply += os.read(0, 256)
    return reply


def query(code: int, end: bytes = ST, timeout: float = 0.6) -> bytes:
    termios.tcflush(0, termios.TCIFLUSH)
    osc(code, "?", end)
    return read_until(end, timeout)


def reply_of(code: int, rgb: str, end: bytes = ST) -> bytes:
    r, g, b = rgb[0:2], rgb[2:4], rgb[4:6]
    return f"\x1b]{code};rgb:{r}{r}/{g}{g}/{b}{b}".encode() + end


def scheme_query(timeout: float = 0.6) -> bytes:
    termios.tcflush(0, termios.TCIFLUSH)
    write_all(b"\x1b[?996n")
    return read_until(b"n", timeout)


def scheme_wait(timeout: float = 1.5) -> bytes:
    return read_until(b"n", timeout)


def checkpoint(name: str) -> None:
    (case_dir / f"marker-{name}").touch()
    done = case_dir / f"{name}.done"
    deadline = time.monotonic() + 8.0
    while not done.exists():
        if time.monotonic() >= deadline:
            raise RuntimeError(f"harness never acknowledged {name}")
        time.sleep(0.02)


def results(**outcomes: bool) -> None:
    for name, ok in outcomes.items():
        write_all(b"\x1b]2;" + f"result-{name}-{'ok' if ok else 'bad'}".encode() + BEL)


def paint_probe_row() -> None:
    write_all(b"\x1b[H\x1b[2J  \x1b[7m  \x1b[0m")


def phase_colors() -> None:
    paint_probe_row()
    checkpoint("initial")
    osc(10, "#aa5500", BEL)
    osc(11, "#112233")
    osc(12, "#445566", BEL)
    checkpoint("set")
    q10 = query(10, BEL) == reply_of(10, "aa5500", BEL)
    q11 = query(11) == reply_of(11, "112233")
    q12 = query(12) == reply_of(12, "445566")
    osc(110, end=BEL)
    osc(111)
    osc(112, end=BEL)
    checkpoint("reset")
    r10 = query(10) == reply_of(10, "102030")
    r11 = query(11) == reply_of(11, "304050")
    r12 = query(12) == reply_of(12, "506070")
    dark = scheme_query() == b"\x1b[?997;1n"
    osc(11, "#ffffff")
    light = scheme_query() == b"\x1b[?997;2n"
    termios.tcflush(0, termios.TCIFLUSH)
    write_all(b"\x1b[?2031h")
    osc(11, "#000000")
    unsolicited_dark = scheme_wait() == b"\x1b[?997;1n"
    osc(11, "#f0f0f0")
    unsolicited_light = scheme_wait() == b"\x1b[?997;2n"
    osc(111)
    unsolicited_reset = scheme_wait() == b"\x1b[?997;1n"
    checkpoint("final")
    results(
        set_queries=q10 and q11 and q12,
        reset_queries=r10 and r11 and r12,
        scheme=dark and light,
        unsolicited=unsolicited_dark and unsolicited_light and unsolicited_reset,
    )


def phase_setonly() -> None:
    paint_probe_row()
    osc(11, "#112233")
    checkpoint("set")
    silent = query(11) == b"" and query(4 if False else 11, BEL) == b""
    termios.tcflush(0, termios.TCIFLUSH)
    osc(4, "1;?")
    ansi_silent = read_until(ST, 0.6) == b""
    osc(4, "1;#ff0000")
    write_all(b"\x1b[2;1H\x1b[41m  \x1b[0m")
    checkpoint("palette")
    results(silent=silent and ansi_silent)


def phase_getonly() -> None:
    paint_probe_row()
    osc(11, "#112233")
    osc(111, end=BEL)
    checkpoint("unchanged")
    answered = query(11) == reply_of(11, "304050")
    termios.tcflush(0, termios.TCIFLUSH)
    osc(4, "1;?")
    ansi = read_until(ST, 0.6) == b"\x1b]4;1;rgb:cdcd/0000/0000" + ST
    results(answered=answered and ansi)


def phase_denyall() -> None:
    paint_probe_row()
    osc(11, "#112233")
    checkpoint("unchanged")
    silent = query(11) == b""
    termios.tcflush(0, termios.TCIFLUSH)
    osc(4, "1;?")
    ansi_silent = read_until(ST, 0.6) == b""
    osc(4, "1;#ff0000")
    write_all(b"\x1b[2;1H\x1b[41m  \x1b[0m")
    checkpoint("palette")
    results(silent=silent and ansi_silent)


def phase_opacity() -> None:
    paint_probe_row()
    osc(11, "#112233")
    checkpoint("set")
    checkpoint("opacity")


PHASES = {
    "opacity": phase_opacity,
    "colors": phase_colors,
    "setonly": phase_setonly,
    "getonly": phase_getonly,
    "denyall": phase_denyall,
}

saved = termios.tcgetattr(0)
tty.setraw(0)
try:
    write_all(b"\x1b]2;dynamic-start" + BEL)
    time.sleep(0.3)
    PHASES[phase]()
    write_all(b"\x1b]2;dynamic-done" + BEL)
    time.sleep(0.3)
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, saved)
