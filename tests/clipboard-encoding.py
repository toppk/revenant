#!/usr/bin/env python3
"""Clipboard steps for the encoding section of xvfb-clipboard.sh.

Runs inside the terminal. The driver writes WORK/go.N with one command and waits
for WORK/done.N; results land in WORK/res.N. Commands:

  query      send an OSC 52 read of CLIPBOARD and save the reply exactly
  query-paused:MS
             the same, but read nothing for MS milliseconds first; WORK/paused.N
             records the bytes waiting on the PTY at the start and end of the
             pause, which a stalled reader cannot make grow past the PTY buffer
  set:B64    OSC 52 write of CLIPBOARD
  paste      mark WORK/listening.N, then save whatever input arrives until quiet
  quit       stop

Files rather than titles keep each step's bytes exact and separate.
"""

import array
import fcntl
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


def collect(until_quiet: float, limit: float, stop=None) -> bytes:
    data = b""
    deadline = time.monotonic() + limit
    quiet = None
    while time.monotonic() < deadline:
        if quiet is not None and time.monotonic() > quiet:
            break
        ready, _, _ = select.select([fd], [], [], 0.05)
        if not ready:
            continue
        data += os.read(fd, 1 << 20)
        if stop is not None and stop(data):
            break
        quiet = time.monotonic() + until_quiet
    return data


def waiting() -> int:
    count = array.array("i", [0])
    fcntl.ioctl(fd, termios.FIONREAD, count)
    return count[0]


def is_reply_end(data: bytes) -> bool:
    return data.endswith(b"\x07") or data.endswith(b"\x1b\\")


try:
    step = 0
    while True:
        step += 1
        go = f"{work}/go.{step}"
        while not os.path.exists(go):
            time.sleep(0.02)
        command = open(go, encoding="ascii").read().strip()
        result = b""
        if command == "query":
            os.write(1, b"\x1b]52;c;?\x07")
            result = collect(5.0, 8.0, is_reply_end)
        elif command.startswith("query-paused:"):
            os.write(1, b"\x1b]52;c;?\x07")
            time.sleep(0.3)
            first = waiting()
            time.sleep(int(command.split(":", 1)[1]) / 1000)
            last = waiting()
            with open(f"{work}/paused.{step}", "w", encoding="ascii") as out:
                out.write(f"{first} {last}\n")
            result = collect(5.0, 20.0, is_reply_end)
        elif command.startswith("set:"):
            os.write(1, b"\x1b]52;c;" + command[4:].encode("ascii") + b"\x07")
        elif command == "paste":
            open(f"{work}/listening.{step}", "w").close()
            result = collect(0.4, 3.0)
        with open(f"{work}/res.{step}", "wb") as out:
            out.write(result)
        open(f"{work}/done.{step}", "w").close()
        if command == "quit":
            break
finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, saved)
