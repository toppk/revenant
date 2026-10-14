#!/usr/bin/env python3
"""Reserved user signals preserve the parent PTY and reset to defaults on exec."""

import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time
import tty


def wait_for(predicate):
    deadline = time.monotonic() + 10
    while not predicate():
        assert time.monotonic() < deadline, "timed out waiting for signal/PTY check"
        time.sleep(0.02)


def child(directory):
    tty.setraw(0)
    report = dict(
        pid=os.getpid(),
        defaults=all(
            signal.getsignal(s) == signal.SIG_DFL
            for s in (signal.SIGUSR1, signal.SIGUSR2, signal.SIGHUP)
        ),
    )
    step = 0
    while True:
        os.write(1, b"\x1b[5n")
        reply = b""
        deadline = time.monotonic() + 5
        while not reply.endswith(b"n"):
            remaining = deadline - time.monotonic()
            assert remaining > 0, "no PTY response after user signal"
            if select.select([0], [], [], remaining)[0]:
                reply += os.read(0, 32)
        assert reply == b"\x1b[0n", reply
        pending = directory / "report.tmp"
        pending.write_text(json.dumps(dict(report, step=step)))
        pending.replace(directory / "report.json")
        wait_for(lambda: (directory / str(step)).exists())
        step += 1


def main(terminal, work):
    directory = Path(work)
    env = dict(
        os.environ, HOME=work, XENVIRONMENT="/dev/null", XFILESEARCHPATH="/dev/null"
    )
    with subprocess.Popen(
        [terminal, "-e", sys.executable, __file__, "--child", work],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    ) as proc:
        try:
            wait_for(lambda: (directory / "report.json").exists())
            report = json.loads((directory / "report.json").read_text())
            assert report["defaults"], "child inherited ignored application signals"
            for step, number in enumerate(
                (signal.SIGUSR1, signal.SIGUSR2, signal.SIGUSR1), 1
            ):
                proc.send_signal(number)
                (directory / str(step - 1)).touch()
                wait_for(
                    lambda: (
                        json.loads((directory / "report.json").read_text())["step"]
                        == step
                    )
                )
                assert proc.poll() is None, "user signal killed the terminal"
            os.kill(report["pid"], signal.SIGUSR1)
            assert proc.wait(timeout=10) == 0
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
            if proc.returncode:
                print(proc.stderr.read().decode(errors="replace"), file=sys.stderr)
    print("USR1/USR2 preserve the parent and PTY; exec child has default dispositions")


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(Path(sys.argv[2]))
    else:
        main(*sys.argv[1:])
