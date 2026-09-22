#!/usr/bin/env python3
"""Launcher and child for xvfb-pty-closed-stdio.sh.

launch MASK WORK TERMINAL ARGS...
    close the standard descriptors named by MASK (bit 0 = fd 0, ...), then exec
    TERMINAL; an exec failure is reported on fd 9, never on 0, 1 or 2
child WORK
    run inside the terminal: check stdio, the controlling terminal and the PTY
    connection, write WORK/child, and exit 3
"""

import fcntl
import os
import select
import sys
import termios
import time
import tty

REPORT_FD = 9
CHILD_EXIT = 3


def launch(mask: int, work: str, command: list[str]) -> None:
    report = os.open(f"{work}/launch", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    os.dup2(report, REPORT_FD)
    os.close(report)
    fcntl.fcntl(REPORT_FD, fcntl.F_SETFD, fcntl.FD_CLOEXEC)
    for fd in range(3):
        if mask & (1 << fd):
            try:
                os.close(fd)
            except OSError:
                pass
    try:
        os.execv(command[0], command)
    except OSError as error:
        os.write(REPORT_FD, f"exec failed: {error}\n".encode())
        os._exit(126)


def describe(path: str) -> str:
    try:
        target = os.readlink(path)
    except OSError:
        return "closed"
    if target.startswith("socket:"):
        return "socket"
    if target.startswith("pipe:"):
        return "pipe"
    if target == "/dev/ptmx" or target == "/dev/pts/ptmx":
        return "ptmx"
    return target


def tty_index(pid: int, fd: str) -> str:
    try:
        with open(f"/proc/{pid}/fdinfo/{fd}", encoding="ascii") as info:
            for line in info:
                if line.startswith("tty-index:"):
                    return line.split()[1]
    except OSError:
        pass
    return ""


def ask(fd: int, query: bytes, end: bytes) -> str:
    os.write(fd, query)
    reply = b""
    deadline = time.monotonic() + 5
    while not reply.endswith(end):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return reply.hex() + ":timeout"
        ready, _, _ = select.select([0], [], [], remaining)
        if not ready:
            continue
        chunk = os.read(0, 64)
        if not chunk:
            return reply.hex() + ":eof"
        reply += chunk
    return reply.hex()


def child(work: str) -> None:
    lines = []
    ttys = [os.isatty(fd) for fd in range(3)]
    lines.append("stdio-tty=" + ",".join("yes" if t else "no" for t in ttys))
    devices = {os.fstat(fd).st_rdev for fd in range(3)}
    lines.append(f"stdio-same-terminal={'yes' if len(devices) == 1 else 'no'}")
    slave = os.fstat(0).st_rdev
    with open("/proc/self/stat", encoding="ascii") as proc:
        tty_nr = int(proc.read().rsplit(")", 1)[1].split()[4])
    try:
        os.close(os.open("/dev/tty", os.O_RDWR | os.O_NOCTTY))
        controlling = "stdio" if tty_nr == slave else "other"
    except OSError:
        controlling = "none"
    lines.append(f"controlling-terminal={controlling}")
    lines.append(
        f"session-leader={'parent' if os.getsid(0) == os.getppid() else 'other'}"
    )
    foreground = os.tcgetpgrp(0) == os.getpgrp()
    lines.append(f"foreground={'yes' if foreground else 'no'}")
    # The listing's own descriptor is closed again by the time each entry is checked.
    listed = sorted(int(name) for name in os.listdir("/proc/self/fd"))
    extra = [fd for fd in listed if fd > 2 and fd_open(fd)]
    lines.append("inherited-fds=" + (",".join(map(str, extra)) or "none"))

    terminal = parent_of(os.getppid())
    index = str(os.minor(slave)) if os.major(slave) >= 136 else "?"
    masters = [
        fd
        for fd in os.listdir(f"/proc/{terminal}/fd")
        if describe(f"/proc/{terminal}/fd/{fd}") == "ptmx"
        and tty_index(terminal, fd) == index
    ]
    lines.append(f"terminal-holds-master={'yes' if masters else 'no'}")
    lines.append(
        "terminal-stdio="
        + ",".join(describe(f"/proc/{terminal}/fd/{fd}") for fd in range(3))
    )

    saved = termios.tcgetattr(0)
    tty.setraw(0)
    try:
        lines.append("stdout-status=" + ask(1, b"\x1b[5n", b"n"))
        os.write(1, b"\x1b[2;1Habc")
        lines.append("stderr-cursor=" + ask(2, b"\x1b[6n", b"R"))
    finally:
        termios.tcsetattr(0, termios.TCSADRAIN, saved)
    lines.append(f"pids={os.getppid()} {os.getpid()}")
    with open(f"{work}/child.tmp", "w", encoding="ascii") as out:
        out.write("\n".join(lines) + "\n")
    os.rename(f"{work}/child.tmp", f"{work}/child")
    sys.exit(CHILD_EXIT)


def fd_open(fd: int) -> bool:
    try:
        fcntl.fcntl(fd, fcntl.F_GETFD)
    except OSError:
        return False
    return True


def parent_of(pid: int) -> int:
    with open(f"/proc/{pid}/stat", encoding="ascii") as proc:
        return int(proc.read().rsplit(")", 1)[1].split()[1])


if __name__ == "__main__":
    if sys.argv[1] == "launch":
        launch(int(sys.argv[2]), sys.argv[3], sys.argv[4:])
    else:
        child(sys.argv[2])
