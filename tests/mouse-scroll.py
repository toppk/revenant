#!/usr/bin/env python3
"""Application side of xvfb-mouse-scroll.sh.

Runs inside the terminal in raw mode. The driver publishes WORK/go.N (by rename)
with one command and waits for WORK/done.N; results land in WORK/res.N. Commands:

  lines:N     print "line 0001" .. "line N", one per row
  send:HEX    write the decoded bytes, such as mode changes
  collect     save the input received since the last collect, once quiet
  quit        stop
"""

import os
import select
import sys
import termios
import time
import tty

work = sys.argv[1]
fd = sys.stdin.fileno()
saved = termios.tcgetattr(fd)
tty.setraw(fd)
received = b""


def drain(quiet: float) -> None:
    global received
    deadline = time.monotonic() + quiet
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return
        ready, _, _ = select.select([fd], [], [], remaining)
        if ready:
            chunk = os.read(fd, 4096)
            if not chunk:
                return
            received += chunk
            deadline = time.monotonic() + quiet


def settle() -> None:
    """Wait until the terminal has parsed everything written so far."""
    global received
    os.write(1, b"\x1b[5n")
    deadline = time.monotonic() + 5
    while b"\x1b[0n" not in received and time.monotonic() < deadline:
        drain(0.05)
    received = received.replace(b"\x1b[0n", b"", 1)


try:
    step = 0
    while True:
        step += 1
        go = f"{work}/go.{step}"
        while not os.path.exists(go):
            drain(0.02)
        command = open(go, encoding="ascii").read().strip()
        result = b""
        if command.startswith("lines:"):
            count = int(command[6:])
            text = "\r\n".join(f"line {n:04d}" for n in range(1, count + 1))
            os.write(1, text.encode("ascii"))
            settle()
        elif command.startswith("send:"):
            os.write(1, bytes.fromhex(command[5:]))
            settle()
        elif command == "collect":
            drain(0.3)
            result, received = received, b""
        with open(f"{work}/res.{step}", "wb") as out:
            out.write(result)
        open(f"{work}/done.{step}", "w").close()
        if command == "quit":
            break
finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, saved)
