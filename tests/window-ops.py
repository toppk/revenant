#!/usr/bin/env python3
"""Check actual OSC 52 replies across two clicks on Allow Window Ops."""

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


saved = termios.tcgetattr(0)
tty.setraw(0)
try:
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
