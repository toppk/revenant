#!/usr/bin/env python3
"""Drive OSC 52 selection writes and queries for xvfb-clipboard.sh.

The phase argument selects a script.  Title changes report what the driver
observed so the shell harness can assert on the terminal log, and generous
pauses leave time for the harness to read selections from outside.
"""

import base64
import os
import select
import sys
import termios
import time
import tty

BEL = b"\x07"
ST = b"\x1b\\"


def write_all(data: bytes) -> None:
    offset = 0
    while offset < len(data):
        offset += os.write(1, data[offset:])


def title(name: str) -> None:
    write_all(b"\x1b]2;" + name.encode() + BEL)


def osc52(target: str, payload: bytes, terminator: bytes = BEL) -> None:
    write_all(b"\x1b]52;" + target.encode() + b";" + payload + terminator)


def set_text(target: str, text: str) -> None:
    osc52(target, base64.b64encode(text.encode()))


def read_reply(deadline: float, terminator: bytes) -> bytes:
    reply = b""
    while time.monotonic() < deadline and not reply.endswith(terminator):
        ready, _, _ = select.select([0], [], [], max(0.0, deadline - time.monotonic()))
        if ready:
            reply += os.read(0, 256)
    return reply


def query(target: str, terminator: bytes = BEL, timeout: float = 1.5) -> bytes:
    termios.tcflush(0, termios.TCIFLUSH)
    osc52(target, b"?", terminator)
    return read_reply(time.monotonic() + timeout, terminator)


def expect_reply(label: str, target: str, text: str, terminator: bytes = BEL) -> None:
    wanted = b"\x1b]52;" + target.encode() + b";" + base64.b64encode(text.encode()) + terminator
    reply = query(target, terminator)
    title(f"{label}-{'ok' if reply == wanted else 'bad'}")


def expect_silence(label: str, target: str) -> None:
    reply = query(target, timeout=1.0)
    title(f"{label}-{'silent' if reply == b'' else 'answered'}")


def pause() -> None:
    time.sleep(0.6)


def phase_denied() -> None:
    set_text("c", "denied-text")
    title("osc52-set-sent")
    pause()
    expect_silence("osc52-query", "c")


def phase_allowed() -> None:
    write_all(b"alpha beta\r\n")
    set_text("c", "hello")
    title("osc52-set-c")
    pause()
    set_text("p", "primary-text")
    title("osc52-set-p")
    pause()
    set_text("s", "select-text")
    title("osc52-set-s")
    pause()
    osc52("c", b"")
    title("osc52-clear-c")
    pause()
    expect_reply("osc52-empty", "c", "")
    set_text("c", "own-text")
    pause()
    expect_reply("osc52-self", "c", "own-text")
    title("osc52-need-owner")
    deadline = time.monotonic() + 5.0
    reply = b""
    wanted = b"\x1b]52;c;" + base64.b64encode(b"from-x") + BEL
    while time.monotonic() < deadline and reply != wanted:
        reply = query("c")
        if reply != wanted:
            time.sleep(0.2)
    title(f"osc52-external-{'ok' if reply == wanted else 'bad'}")
    expect_reply("osc52-st", "c", "from-x", ST)
    osc52("c", b"!!!!")
    title("osc52-invalid-sent")
    pause()
    set_text("c", "replaced")
    title("osc52-replaced")
    pause()
    title("osc52-mouse-window")
    time.sleep(1.5)
    set_text("c", "after-mouse")
    title("osc52-set-after-mouse")
    pause()


def phase_late() -> None:
    title("osc52-late-ready")
    wanted = b"\x1b]52;p;" + base64.b64encode(b"PRIMARY-X") + BEL
    deadline = time.monotonic() + 5.0
    reply = b""
    while time.monotonic() < deadline and reply != wanted:
        reply = query("p")
        if reply != wanted:
            time.sleep(0.2)
    title(f"osc52-late-owners-{'ok' if reply == wanted else 'bad'}")
    expect_reply("osc52-late-first", "c", "")
    time.sleep(0.7)
    expect_reply("osc52-late-primary", "p", "PRIMARY-X")
    expect_reply("osc52-late-second", "c", "")


def phase_setonly() -> None:
    set_text("c", "set-allowed")
    title("osc52-set-c")
    pause()
    expect_silence("osc52-query", "c")


def phase_limit() -> None:
    set_text("c", "L" * 80)
    title("osc52-large-sent")
    pause()
    set_text("c", "small")
    title("osc52-small-sent")
    pause()


PHASES = {
    "denied": phase_denied,
    "allowed": phase_allowed,
    "setonly": phase_setonly,
    "limit": phase_limit,
    "late": phase_late,
}

saved = termios.tcgetattr(0)
tty.setraw(0)
try:
    title("osc52-start")
    time.sleep(0.3)
    PHASES[sys.argv[1]]()
    title("osc52-done")
    time.sleep(0.3)
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, saved)
