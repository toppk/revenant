#!/usr/bin/env python3
"""Exercise OSC 52 selection writes and queries against the running terminal.

Run inside xterm+, xterm, or Ghostty.  xterm+ and xterm refuse OSC 52 by
default; enable Allow Window Ops in the Ctrl+right-click menu, start with
`-xrm 'XTerm*allowWindowOps: true'`, or trim `disallowedWindowOps` to compare
the permitted behavior. Queries need a raw
terminal to read the reply, so run the probe directly, not through a pager.
"""

import argparse
import base64
import os
import select
import sys
import termios
import time
import tty

BEL = b"\x07"
ST = b"\x1b\\"
TARGET_NAMES = {"c": "CLIPBOARD", "p": "PRIMARY", "s": "SELECT (selectToClipboard)"}


def write_all(data: bytes) -> None:
    offset = 0
    while offset < len(data):
        offset += os.write(1, data[offset:])


def osc52(target: str, payload: bytes, terminator: bytes) -> None:
    write_all(b"\x1b]52;" + target.encode() + b";" + payload + terminator)


def printable(data: bytes) -> str:
    return data.decode("latin-1").replace("\x1b", "ESC").replace("\x07", "BEL")


def do_set(target: str, text: str, terminator: bytes) -> None:
    osc52(target, base64.b64encode(text.encode()), terminator)
    print(f"requested {TARGET_NAMES.get(target, target)} = {text!r}; use --query or paste to verify")


def do_clear(target: str, terminator: bytes) -> None:
    osc52(target, b"", terminator)
    print(f"requested clear of {TARGET_NAMES.get(target, target)}")


def do_query(target: str, terminator: bytes, timeout: float) -> None:
    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        termios.tcflush(fd, termios.TCIFLUSH)
        osc52(target, b"?", terminator)
        reply = b""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline and not reply.endswith(terminator):
            ready, _, _ = select.select([fd], [], [], max(0.0, deadline - time.monotonic()))
            if ready:
                reply += os.read(fd, 256)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved)
    print(f"query {TARGET_NAMES.get(target, target)}:")
    if not reply:
        print("  no reply (policy denies GetSelection, or the terminal ignores OSC 52)")
        return
    print(f"  raw: {printable(reply)}")
    body = reply[len(b"\x1b]52;"):-len(terminator)]
    if b";" in body:
        echoed, payload = body.split(b";", 1)
        try:
            decoded = base64.b64decode(payload, validate=True)
        except ValueError:
            print(f"  malformed payload {payload!r}")
            return
        print(f"  target echoed as {echoed.decode('latin-1')!r}, {len(decoded)} bytes: {decoded!r}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--target", default="c", help="Pc character: c, p, or s (default c)")
    parser.add_argument("--st", action="store_true", help="terminate with ESC \\ instead of BEL")
    parser.add_argument("--timeout", type=float, default=2.0, help="seconds to wait for a reply")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--set", metavar="TEXT", help="write TEXT to the target")
    group.add_argument("--clear", action="store_true", help="clear the target")
    group.add_argument("--query", action="store_true", help="query the target and show the reply")
    group.add_argument("--invalid", action="store_true", help="send a payload that is not base64")
    args = parser.parse_args()
    terminator = ST if args.st else BEL

    if args.set is not None:
        do_set(args.target, args.set, terminator)
    elif args.clear:
        do_clear(args.target, terminator)
    elif args.query:
        do_query(args.target, terminator, args.timeout)
    elif args.invalid:
        osc52(args.target, b"!!!!", terminator)
        print("sent an invalid payload; xterm clears the selection, libghostty ignores it")
    else:
        do_set(args.target, "probe-clipboard", terminator)
        time.sleep(0.2)
        do_query(args.target, terminator, args.timeout)


if __name__ == "__main__":
    main()
