#!/usr/bin/env python3
"""Exercise live resource reloads against the real PTY and OSC color replies."""

import json
import importlib.util
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time
import tty


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location(
    "theme_setter", ROOT / "tools/theme-setter-utility.py"
)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def wait_for(predicate):
    deadline = time.monotonic() + 5
    while not predicate():
        if time.monotonic() >= deadline:
            raise AssertionError("timed out waiting for reload/PTY reply")
        time.sleep(0.02)


def child(directory):
    tty.setraw(0)
    os.write(1, b"Theme reload must preserve this running child.\r\n")
    (directory / "ready").write_text(os.environ["WINDOWID"])
    step = 0
    while True:
        request = directory / f"query.{step}"
        wait_for(lambda: request.exists() or (directory / "stop").exists())
        if (directory / "stop").exists():
            return
        replies = []
        for command in ["10", "11", "12"] + [f"4;{i}" for i in range(16)]:
            os.write(1, f"\033]{command};?\033\\".encode())
            reply = b""
            while not reply.endswith(b"\033\\"):
                assert select.select([0], [], [], 3)[0], "OSC query timed out"
                reply += os.read(0, 1)
            replies.append(reply.decode())
        os.write(1, b"\033[6n")
        position = b""
        while not position.endswith(b"R"):
            assert select.select([0], [], [], 3)[0], "cursor query timed out"
            position += os.read(0, 1)
        assert position == b"\033[2;1R", "reload moved or reset the cursor"
        (directory / f"reply.{step}").write_text(json.dumps(replies))
        step += 1


def run(terminal, xrdb, directory, reverse=False):
    directory.mkdir()
    home = directory / "home"
    home.mkdir()
    log_path = directory / "log"

    def resources(colors):
        names = ["foreground", "background", "cursorColor"] + [
            f"color{i}" for i in range(16)
        ]
        subprocess.run(
            [xrdb, "-nocpp", "-load"],
            input="".join(
                f"XTerm*{name}: {value}\n" for name, value in zip(names, colors)
            ),
            text=True,
            check=True,
        )

    def query(step):
        (directory / f"query.{step}").touch()
        path = directory / f"reply.{step}"
        wait_for(lambda: path.exists() and path.stat().st_size > 0)
        return json.loads(path.read_text())

    def expected(colors):
        colors = list(colors)
        if reverse:
            colors[0], colors[1] = colors[1], colors[0]
        commands = ["10", "11", "12"] + [f"4;{i}" for i in range(16)]
        return [
            f"\033]{command};rgb:"
            + "/".join(value[i : i + 2] * 2 for i in (1, 3, 5))
            + "\033\\"
            for command, value in zip(commands, colors)
        ]

    def reload(message):
        before = log_path.read_text().count(message)
        tool.ReloadTargets(os.environ["DISPLAY"]).hup(target)
        wait_for(lambda: log_path.read_text().count(message) > before)

    initial = [f"#{i + 16:02x}2030" for i in range(19)]
    resources(initial)
    with log_path.open("w") as log:
        process = subprocess.Popen(
            [
                terminal,
                "-debug",
                "-rv" if reverse else "+rv",
                "-fg",
                "#abcdef",
                "-xrm",
                "XTerm*color3: #112233",
                "-e",
                sys.executable,
                __file__,
                "--child",
                str(directory),
            ],
            env=dict(os.environ, HOME=str(home), XENVIRONMENT="/dev/null"),
            stdout=log,
            stderr=log,
        )
        try:
            wait_for(
                lambda: (
                    (directory / "ready").exists()
                    and (directory / "ready").stat().st_size
                )
            )
            props = subprocess.check_output(
                [
                    "xprop",
                    "-id",
                    (directory / "ready").read_text(),
                    "_REVENANT_VERSION",
                    "_REVENANT_CAPABILITIES",
                    "_NET_WM_PID",
                ],
                text=True,
            )
            assert "_REVENANT_VERSION(UTF8_STRING)" in props, props
            assert (
                "_REVENANT_CAPABILITIES(ATOM) = _REVENANT_RELOAD_SIGHUP_V1" in props
            ), props
            assert str(process.pid) in props, props
            target = tool.ReloadTargets(os.environ["DISPLAY"]).inspect(
                int((directory / "ready").read_text())
            )
            assert target is not None and target["pid"] == process.pid
            initial[0], initial[6] = "#abcdef", "#112233"
            assert query(0) == expected(initial), "startup resource precedence"
            changed = [f"#4050{i + 32:02x}" for i in range(19)]
            resources(changed)
            # User signals must leave colors unchanged even with new XRDB values.
            for step, number in enumerate((signal.SIGUSR1, signal.SIGUSR2), 1):
                process.send_signal(number)
                assert query(step) == expected(initial), "user signal reloaded colors"
            reload("SIGHUP reloaded")
            changed[0], changed[6] = "#abcdef", "#112233"
            assert query(3) == expected(changed), "live colors or command precedence"
            # Unchanged reloads must skip widget/backend color reset work entirely.
            before_defaults = log_path.read_text().count("default colors foreground=")
            before_palette = log_path.read_text().count("configured ANSI palette ")
            for attempt in range(3):
                reload("SIGHUP reloaded")
                assert "colors=0" in log_path.read_text().split("SIGHUP reloaded")[-1].splitlines()[0].split()
            assert (
                log_path.read_text().count("default colors foreground=")
                == before_defaults
            )
            assert (
                log_path.read_text().count("configured ANSI palette ") == before_palette
            )
            # Equivalent spellings of the same RGB values are also no-ops.
            resources([value.upper() for value in changed])
            reload("SIGHUP reloaded")
            assert "colors=0" in log_path.read_text().split("SIGHUP reloaded")[-1].splitlines()[0].split()
            invalid = ["#010203"] * 19
            invalid[18] = "no-such-color"
            resources(invalid)
            reload("SIGHUP reload rejected")
            assert query(4) == expected(changed), "invalid reload was not atomic"
            resources(["#fefdfc", "#030201"])
            reload("SIGHUP reloaded")
            changed[1] = "#030201"
            assert query(5) == expected(changed), "omitted colors were not retained"
            for step in range(6, 11):
                colors = [f"#{step:02x}aabb"] * 19
                resources(colors)
                reload("SIGHUP reloaded")
                colors[0], colors[6] = "#abcdef", "#112233"
                assert query(step) == expected(colors), "repeated reload"
            subprocess.run([xrdb, "-remove"], check=True)
            reload("SIGHUP reload skipped")
            assert query(11) == expected(colors), "missing server resources"
            assert process.poll() is None, "reload terminated terminal or child"
            (directory / "stop").touch()
            assert process.wait(timeout=5) == 0, "terminal teardown failed"
        except BaseException:
            print(log_path.read_text(), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
    print("live theme colors, precedence, atomic rejection and repeated reloads passed")


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(Path(sys.argv[2]))
    else:
        run(sys.argv[1], sys.argv[2], Path(sys.argv[3]))
        run(sys.argv[1], sys.argv[2], Path(sys.argv[3] + "-reverse"), reverse=True)
