#!/usr/bin/env python3
"""Drive XTWINOPS title reports and the title stack for xvfb-title-stack.sh.

Checkpoints are a file handshake: the driver creates marker-NAME in the case
directory and waits for NAME.done, so the harness can inspect WM_NAME and
WM_ICON_NAME while the labels under test are still current.  Report replies
are read in raw mode and summarized through result titles at the end of each
phase, after every label check has been made.
"""

import os
from pathlib import Path
import select
import sys
import termios
import time
import tty

ST = b"\x1b\\"
case_dir = Path(sys.argv[1])
phase = sys.argv[2]


def write_all(data: bytes) -> None:
    offset = 0
    while offset < len(data):
        offset += os.write(1, data[offset:])


def set_title(text: str) -> None:
    write_all(b"\x1b]2;" + text.encode() + b"\x07")


def csi_t(*params: int) -> None:
    write_all(b"\x1b[" + ";".join(str(p) for p in params).encode() + b"t")


def read_reply(timeout: float) -> bytes:
    reply = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and not reply.endswith(ST):
        ready, _, _ = select.select([0], [], [], max(0.0, deadline - time.monotonic()))
        if ready:
            reply += os.read(0, 256)
    return reply


def query(op: int, timeout: float = 1.0) -> bytes:
    termios.tcflush(0, termios.TCIFLUSH)
    csi_t(op)
    return read_reply(timeout)


def report_of(code: bytes, label: str) -> bytes:
    return b"\x1b]" + code + label.encode() + ST


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
        write_all(b"\x1b]2;" + f"result-{name}-{'ok' if ok else 'bad'}".encode() + b"\x07")


def phase_stack() -> None:
    # Push/pop is permitted by xterm's default policy; reports are not.
    checkpoint("external-labels")
    csi_t(22, 0)
    set_title("beta")
    checkpoint("beta-visible")
    csi_t(23, 0)
    checkpoint("external-restored")
    set_title("alpha")
    csi_t(22, 2)
    set_title("gamma")
    csi_t(22)
    set_title("delta")
    csi_t(23)
    checkpoint("gamma-restored")
    csi_t(23, 2)
    checkpoint("alpha-restored")
    csi_t(23, 2)
    checkpoint("empty-pop-ignored")
    # Twelve pushes overflow the ten-entry ring: the two oldest titles are
    # lost, but xterm still counts them, so two empty pops follow the tenth.
    for index in range(12):
        set_title(f"ring{index}")
        csi_t(22, 2)
    set_title("top")
    for index in range(12):
        csi_t(23, 2)
    checkpoint("ring-drained")
    csi_t(23, 2)
    checkpoint("ring-empty")
    results(query_silent=query(21) == b"" and query(20) == b"")


def phase_reports() -> None:
    checkpoint("external-labels")
    window = query(21) == report_of(b"l", "external-title")
    icon = query(20) == report_of(b"L", "external-icon")
    csi_t(22, 2)
    set_title("changed")
    csi_t(23, 2)
    after_pop = query(21) == report_of(b"l", "external-title")
    results(window=window, icon=icon, after_pop=after_pop)


def phase_toggle() -> None:
    set_title("toggled title")
    before = query(21) == b""
    checkpoint("toggle-on")
    enabled = query(21) == report_of(b"l", "toggled title")
    checkpoint("toggle-off")
    disabled = query(21) == b""
    results(before=before, enabled=enabled, disabled=disabled)


def phase_denied_stack() -> None:
    set_title("kept")
    csi_t(22, 2)
    set_title("changed")
    csi_t(23, 2)
    checkpoint("pop-denied")


PHASES = {
    "stack": phase_stack,
    "reports": phase_reports,
    "toggle": phase_toggle,
    "denied-stack": phase_denied_stack,
}

saved = termios.tcgetattr(0)
tty.setraw(0)
try:
    write_all(b"\x1b]2;title-start\x07")
    time.sleep(0.3)
    PHASES[phase]()
    write_all(b"\x1b]2;title-done\x07")
    time.sleep(0.3)
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, saved)
