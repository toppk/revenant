#!/usr/bin/env python3
"""Check live Allow Title Ops separately from the Window Ops permissions."""

import os
from pathlib import Path
import select
import sys
import termios
import time
import tty

directory = Path(sys.argv[1])
initially_allowed = sys.argv[2] == "true"
ST = b"\x1b\\"


def send(data):
    os.write(1, data)


def title(text, osc=2):
    send(f"\x1b]{osc};{text}".encode() + ST)


def checkpoint(name):
    (directory / (name + '.ready')).touch()
    deadline = time.monotonic() + 8
    while not (directory / (name + '.done')).exists():
        if time.monotonic() > deadline:
            raise RuntimeError(f"checkpoint timed out: {name}")
        time.sleep(.01)


def query():
    send(b"\x1b[21t")
    result = b""
    deadline = time.monotonic() + 1
    while not result.endswith(ST) and time.monotonic() < deadline:
        if select.select([0], [], [], max(0, deadline-time.monotonic()))[0]:
            result += os.read(0, 1024)
    return result


def expect(text):
    actual = query()
    assert actual == b"\x1b]l" + text.encode() + ST, (text, actual)


saved = termios.tcgetattr(0)
tty.setraw(0)
try:
    title("first")
    expect("first" if initially_allowed else "startup")
    checkpoint("initial")
    # The harness enables Title Ops if it started disabled.
    title("saved")
    send(b"\x1b[22;0t")
    title("current")
    expect("current")
    checkpoint("disable")
    title("blocked-osc2")
    title("blocked-osc0", 0)
    expect("current")  # Window Ops reports still work.
    send(b"\x1b[23;0t")
    expect("current")  # Pop consumes the entry but cannot change labels.
    send(b"\x1b[22;0t")
    checkpoint("enable")
    title("blocked-osc2")  # Repeating a denied title must work after enabling.
    expect("blocked-osc2")
    send(b"\x1b[23;0t")
    expect("current")  # Push remained allowed while Title Ops was disabled.
    checkpoint("restored")
    send(b"\x1b[23;0t")  # The earlier denied restoration consumed its entry.
    expect("current")
    (directory / 'passed').touch()
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, saved)
