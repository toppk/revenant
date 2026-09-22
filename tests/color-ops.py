#!/usr/bin/env python3
"""Exercise the live dynamic-color permission gate through real PTY replies.

color-ops.py DIR true|false   the scripted menu checks
color-ops.py DIR serve        run DIR/go.N commands (published by rename):
                              send:HEX  write the bytes, then require exactly the DSR ack
                              ask:HEX   the same, saving the bytes before the ack to res.N,
                                        so an empty result is silence, not a timeout
                              quit      stop, after checking that no input is pending
Any other input, a timeout or EOF writes "ERROR ..." to res.N.
"""
import os
from pathlib import Path
import select
import sys
import termios
import time
import tty

directory = Path(sys.argv[1])
initial = sys.argv[2] == 'true'
ST = b'\x1b\\'


def osc(code, value=None, end=ST):
    os.write(1, f'\x1b]{code}'.encode() + (b';'+value.encode() if value else b'') + end)


def query(code):
    osc(code, '?')
    result = b''
    deadline = time.monotonic()+.15
    while time.monotonic() < deadline and not result.endswith(ST):
        if select.select([0], [], [], max(0, deadline-time.monotonic()))[0]:
            result += os.read(0, 4096)
    return result


def expect(code, rgb):
    wanted = f'\x1b]{code};rgb:'.encode() + '/'.join(c*2 for c in rgb).encode()+ST
    actual = query(code)
    assert actual == wanted, (code, wanted, actual)


def checkpoint(name):
    (directory/(name+'.ready')).touch()
    deadline = time.monotonic()+8
    while not (directory/(name+'.done')).exists():
        if time.monotonic() > deadline:
            raise RuntimeError(name)
        time.sleep(.01)


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
            if command.startswith(("send:", "ask:")):
                os.write(1, bytes.fromhex(command.split(":", 1)[1]) + b"\x1b[5n")
                result = until_ack()
                if command.startswith("send:") and result:
                    raise UnexpectedInput(f"unexpected input {result!r}")
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
    osc(10, '#aa5500')
    if initial:
        expect(10, ['aa', '55', '00'])
    else:
        assert query(10) == b''
    checkpoint('initial')
    expect(10, ['aa', '55', '00'] if initial else ['10', '20', '30'])
    for code in [10, 11, 12]:
        osc(code, '#aabbcc')
        expect(code, ['aa', 'bb', 'cc'])
    checkpoint('disable')
    for code in [10, 11, 12]:
        # Every byte boundary, including the OSC header, crosses PTY writes.
        data = f'\x1b]{code};#123456'.encode()+ST
        for byte in data:
            os.write(1, bytes([byte])); time.sleep(.002)
        osc(code+100)
        osc(code, '#234567', b'\x07')
        osc(code+100, end=b'\x07')
        assert query(code) == b''
    checkpoint('enable')
    for code in [10, 11, 12]:
        expect(code, ['aa', 'bb', 'cc'])
        osc(code+100)
    for code, rgb in [(10, ['10','20','30']), (11, ['30','40','50']), (12, ['50','60','70'])]:
        expect(code, rgb)
    # A denied OSC must not swallow subsequent title/CSI traffic.
    osc(2, 'color-ops-done')
    (directory/'passed').touch()
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, saved)
