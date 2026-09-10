#!/usr/bin/env python3
"""Drive DEC private mode 2026 through four phases for xvfb-sync-output.sh.

Phase A releases a batch normally, phase B leaves the mode set until the
terminal's timeout releases it, phase C holds a batch across a host resize
and confirms the mode survived, and phase D holds a batch while the harness
Shift-hovers a hyperlink.  Title changes mark each phase and report DECRQM
answers to the shell harness.
"""

import os
import select
import signal
import termios
import time
import tty

SYNC_ON = b"\x1b[?2026h"
SYNC_OFF = b"\x1b[?2026l"
DECRQM = b"\x1b[?2026$p"
LINK = b"\x1b]8;;https://sync.example\x07LINK\x1b]8;;\x07"

winch_count = 0


def on_winch(signum, frame):
    global winch_count
    winch_count += 1


def write_all(data: bytes) -> None:
    offset = 0
    while offset < len(data):
        offset += os.write(1, data[offset:])


def title(name: str) -> None:
    write_all(b"\x1b]2;" + name.encode() + b"\x07")


def frame(label: str, prefix: bytes = b"") -> bytes:
    rows = [f"{label} {row:02d}".ljust(60).encode() for row in range(20)]
    rows[0] = prefix + rows[0]
    return b"\x1b[H" + b"\r\n".join(rows)


def read_reply(deadline: float) -> bytes:
    reply = b""
    while time.monotonic() < deadline and not reply.endswith(b"$y"):
        ready, _, _ = select.select([0], [], [], max(0.0, deadline - time.monotonic()))
        if ready:
            reply += os.read(0, 64)
    return reply


def report_mode(phase: str) -> None:
    write_all(DECRQM)
    reply = read_reply(time.monotonic() + 2.0)
    if reply.endswith(b"\x1b[?2026;1$y"):
        title(f"{phase}-mode-set")
    elif reply.endswith(b"\x1b[?2026;2$y"):
        title(f"{phase}-mode-reset")
    else:
        title(f"{phase}-mode-unknown")


def wait_for_winch(wanted: int, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while winch_count < wanted and time.monotonic() < deadline:
        time.sleep(0.02)


signal.signal(signal.SIGWINCH, on_winch)
saved = termios.tcgetattr(0)
tty.setraw(0)
try:
    title("sync-start")
    time.sleep(0.3)

    title("sync-phase-a")
    write_all(SYNC_ON + frame("PHASE A HALF"))
    time.sleep(0.2)
    write_all(frame("PHASE A MORE"))
    time.sleep(0.2)
    write_all(frame("PHASE A DONE") + SYNC_OFF)
    time.sleep(0.3)

    title("sync-phase-b")
    write_all(SYNC_ON + frame("PHASE B STUCK"))
    time.sleep(1.6)
    report_mode("sync-phase-b")
    write_all(frame("PHASE B AFTER"))
    time.sleep(0.3)

    title("sync-phase-c")
    write_all(SYNC_ON + frame("PHASE C HELD"))
    wait_for_winch(2, 3.0)
    time.sleep(0.1)
    write_all(frame("PHASE C RESIZED"))
    report_mode("sync-phase-c")
    time.sleep(0.1)
    write_all(frame("PHASE C DONE") + SYNC_OFF)
    time.sleep(0.3)

    title("sync-phase-d")
    write_all(frame("PHASE D LINK", LINK))
    time.sleep(0.3)
    write_all(SYNC_ON + frame("PHASE D HELD", LINK))
    time.sleep(0.7)
    report_mode("sync-phase-d")
    write_all(frame("PHASE D DONE", LINK) + SYNC_OFF)
    time.sleep(0.3)

    title("sync-done")
    time.sleep(0.3)
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, saved)
