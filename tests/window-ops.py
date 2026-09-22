#!/usr/bin/env python3
"""Check actual OSC 52 replies across two clicks on Allow Window Ops.

window-ops.py DIR default|setonly   the scripted OSC 52 checks
window-ops.py DIR serve             run DIR/go.N commands (published by rename):
    ask:HEX        write the bytes, then a status request; res.N gets the bytes that
                   arrived before the status reply, so empty means silence
    split:HEX:HEX  the same, with the request written in two separate writes
    quit           stop, after checking that no input is pending
Any other input, a timeout or EOF writes "ERROR ..." to res.N.
"""

import base64
import os
from pathlib import Path
import select
import sys
import termios
import time
import tty


def title(value):
    os.write(1, b'\x1b]2;' + value.encode() + b'\x07')


def set_text(text):
    os.write(1, b'\x1b]52;c;' + base64.b64encode(text) + b'\x07')


def query():
    os.write(1, b'\x1b]52;c;?\x07')
    reply = b''
    deadline = time.monotonic() + 0.4
    while time.monotonic() < deadline and not reply.endswith(b'\x07'):
        if select.select([0], [], [], max(0, deadline - time.monotonic()))[0]:
            reply += os.read(0, 1024)
    return reply


def wait_for_toggle(number):
    title(f'window-ops-toggle-{number}')
    deadline = time.monotonic() + 8
    while not (Path(sys.argv[1]) / f'toggled-{number}').exists():
        if time.monotonic() >= deadline:
            raise RuntimeError('menu toggle did not arrive')
        time.sleep(0.01)


ACK = b"\x1b[0n"


class UnexpectedInput(Exception):
    pass


def until_ack(seconds=2):
    data = b""
    deadline = time.monotonic() + seconds
    while not data.endswith(ACK):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise UnexpectedInput(f"timed out after {data!r}")
        if select.select([0], [], [], remaining)[0]:
            chunk = os.read(0, 4096)
            if not chunk:
                raise UnexpectedInput(f"EOF after {data!r}")
            data += chunk
    return data[: -len(ACK)]


def pending():
    if select.select([0], [], [], 0)[0]:
        data = os.read(0, 4096)
        if data:
            raise UnexpectedInput(f"unexpected input {data!r}")


def serve():
    directory = Path(sys.argv[1])
    step = 0
    while True:
        step += 1
        go = directory / f"go.{step}"
        while not go.exists():
            time.sleep(0.01)
        command = go.read_text().strip()
        result = b""
        try:
            pending()
            if command.startswith("ask:"):
                os.write(1, bytes.fromhex(command[4:]) + b"\x1b[5n")
                result = until_ack()
            elif command.startswith("split:"):
                first, second = command[6:].split(":")
                os.write(1, bytes.fromhex(first))
                time.sleep(0.05)
                os.write(1, bytes.fromhex(second) + b"\x1b[5n")
                result = until_ack()
            elif command == "quit":
                time.sleep(0.3)
                pending()
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
    # Start from the resource-controlled policy: deny-all or set-only.
    set_text(b'before')
    assert query() == b'', 'initial query was not silent'
    wait_for_toggle(1)
    wanted = b'before' if sys.argv[2] == 'setonly' else b''
    assert query() == b'\x1b]52;c;' + base64.b64encode(wanted) + b'\x07'
    set_text(b'enabled')
    assert query() == b'\x1b]52;c;' + base64.b64encode(b'enabled') + b'\x07'
    wait_for_toggle(2)
    set_text(b'after')
    assert query() == b'', 'disabling left the read callback installed'
    title('window-ops-check-selection')
    deadline = time.monotonic() + 8
    while not (Path(sys.argv[1]) / 'checked').exists():
        if time.monotonic() >= deadline:
            raise RuntimeError('external selection check did not arrive')
        time.sleep(0.01)
    title('window-ops-done')
    time.sleep(0.1)
finally:
    termios.tcsetattr(0, termios.TCSANOW, saved)
