#!/usr/bin/env python3
"""Match the child's WINDOWID, window owner PID, and PTY master owner."""

import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time


def child(report: Path) -> None:
    pending = report.with_suffix(".tmp")
    pending.write_text(
        json.dumps(
            {
                "window": os.environ["WINDOWID"],
                "parent": os.getppid(),
                "tty": os.minor(os.fstat(0).st_rdev),
            }
        )
    )
    pending.replace(report)
    while not report.with_suffix(".stop").exists():
        time.sleep(0.02)


def main() -> None:
    terminal, xprop, work = sys.argv[1:]
    report = Path(work) / "identity.json"
    env = dict(
        os.environ,
        WINDOWID="123",
        XENVIRONMENT="/dev/null",
        XFILESEARCHPATH="/dev/null",
        HOME=work,
    )
    with subprocess.Popen(
        [terminal, "-e", sys.executable, __file__, "--child", str(report)],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    ) as proc:
        try:
            deadline = time.monotonic() + 10
            while not report.exists():
                assert proc.poll() is None, "terminal exited before child reported"
                assert time.monotonic() < deadline, "child did not report"
                time.sleep(0.02)
            data = json.loads(report.read_text())
            assert data["window"].isdecimal() and int(data["window"]) != 123, data
            assert data["parent"] == proc.pid, data
            props = subprocess.check_output(
                [
                    xprop,
                    "-id",
                    data["window"],
                    "_NET_WM_PID",
                    "WM_CLIENT_MACHINE",
                    "WM_CLASS",
                ],
                text=True,
            )
            assert re.search(rf"_NET_WM_PID\(CARDINAL\) = {proc.pid}\b", props), props
            assert socket.gethostname() in props, props
            assert "WM_CLASS(STRING)" in props, props
            fdinfo = Path(f"/proc/{proc.pid}/fdinfo")
            assert any(
                re.search(rf"^tty-index:\s+{data['tty']}$", entry.read_text(), re.M)
                for entry in fdinfo.iterdir()
            ), "window owner does not hold PTY master"
            report.with_suffix(".stop").touch()
            assert proc.wait(timeout=10) == 0
            print(
                "WINDOWID identifies shell window; CARDINAL PID identifies PTY master owner"
            )
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
            if proc.returncode != 0:
                print(proc.stderr.read().decode(errors="replace"), file=sys.stderr)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(Path(sys.argv[2]))
    else:
        main()
