#!/usr/bin/env python3
"""Check live Allow Title Ops separately from the Window Ops permissions.

title-ops.py DIR true|false   the scripted menu checks
title-ops.py DIR serve        run DIR/go.N commands (published by rename): send:HEX writes
                              the bytes and waits until the terminal has parsed them;
                              report saves the CSI 21 t reply to DIR/res.N; quit stops
"""

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


class UnexpectedInput(Exception):
    pass


ACK = b"\x1b[0n"


def ack_state(data):
    if data == ACK:
        return "complete"
    return "pending" if ACK.startswith(data) else "unexpected"


def report_state(data):
    """One OSC l report terminated by ST, with no escape inside the label."""
    if len(data) < 3:
        return "pending" if b"\x1b]l".startswith(data) else "unexpected"
    if not data.startswith(b"\x1b]l"):
        return "unexpected"
    body = data[3:]
    end = body.find(b"\x1b")
    if end < 0:
        return "pending"
    if body[end:] == b"\x1b\\":
        return "complete"
    return "pending" if body[end:] == b"\x1b" else "unexpected"


def read_reply(state, seconds=2):
    """Read exactly one reply; other input, a timeout or EOF is an error."""
    result = b""
    deadline = time.monotonic() + seconds
    while True:
        current = state(result)
        if current == "complete":
            return result
        if current == "unexpected":
            raise UnexpectedInput(f"unexpected input {result!r}")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise UnexpectedInput(f"timed out after {result!r}")
        if select.select([0], [], [], remaining)[0]:
            chunk = os.read(0, 1)
            if not chunk:
                raise UnexpectedInput(f"EOF after {result!r}")
            result += chunk


def drain(seconds=0.3):
    """Input that arrives with no request outstanding is a leak."""
    if select.select([0], [], [], seconds)[0]:
        data = os.read(0, 1024)
        if data:
            raise UnexpectedInput(f"unexpected input {data!r}")


def serve():
    step = 0
    while True:
        step += 1
        go = directory / f"go.{step}"
        while not go.exists():
            time.sleep(0.01)
        command = go.read_text().strip()
        result = b""
        try:
            drain(0)
            if command.startswith("send:"):
                send(bytes.fromhex(command[5:]) + b"\x1b[5n")
                read_reply(ack_state)
            elif command == "report":
                send(b"\x1b[21t")
                result = read_reply(report_state)
            elif command == "quit":
                drain()
        except UnexpectedInput as error:
            result = f"ERROR {error}".encode()
        (directory / f"res.{step}").write_bytes(result)
        (directory / f"done.{step}").touch()
        if command == "quit":
            return


saved = termios.tcgetattr(0)
tty.setraw(0)
try:
    if sys.argv[2] == "serve":
        serve()
        sys.exit(0)
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
