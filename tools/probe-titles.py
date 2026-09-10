#!/usr/bin/env python3
"""Exercise title/icon reports and nested XTWINOPS title stacks.

Run directly inside the terminal being tested. The default demo pauses for
Enter at each title change. Use --query to read without changing titles, or
--delay SECONDS for an automatic demo. Ctrl+C requests stack restoration.
"""

import argparse
import math
import os
import select
import signal
import sys
import termios
import time
import tty


ST = b"\x1b\\"
TARGETS = {"icon": (1, 20, b"L"), "title": (2, 21, b"l")}
MAX_REPLY = 65536


def send(data):
    while data:
        written = os.write(sys.stdout.fileno(), data)
        if written == 0:
            raise OSError("terminal write made no progress")
        data = data[written:]


def say(text=""):
    send(text.encode("utf-8") + b"\r\n")


def receive(timeout):
    if not select.select([sys.stdin.fileno()], [], [], max(0, timeout))[0]:
        return b""
    data = os.read(sys.stdin.fileno(), 4096)
    if not data:
        raise EOFError("terminal input closed")
    if b"\x03" in data:
        raise KeyboardInterrupt
    return data


def report(target, timeout):
    _, operation, marker = TARGETS[target]
    send(f"\x1b[{operation}t".encode())
    deadline = time.monotonic() + timeout
    captured = b""
    while time.monotonic() < deadline:
        captured += receive(deadline - time.monotonic())
        # Replies are OSC L/l text ST (BEL is accepted for comparison).
        # Retain fragmented input and distinguish icon from title replies.
        for prefix in (b"\x1b]" + marker, b"\x9d" + marker):
            start = captured.find(prefix)
            if start < 0:
                continue
            body_start = start + len(prefix)
            endings = [(captured.find(end, body_start), end)
                       for end in (ST, b"\x07", b"\x9c")]
            endings = [(pos, end) for pos, end in endings if pos >= 0]
            if endings:
                pos, end = min(endings)
                value = captured[body_start:pos]
                # repr prevents reported title text from executing controls.
                say(f"  {target}: {value.decode('utf-8', errors='backslashreplace')!r}")
                say(f"    reply: {captured[start:pos + len(end)]!r}")
                return
        if len(captured) > MAX_REPLY:
            say(f"  {target}: reply exceeded {MAX_REPLY} bytes; stopped reading")
            return
    if captured:
        say(f"  {target}: no complete matching report; captured {captured!r}")
    else:
        say(f"  {target}: no reply (denied by policy or unsupported)")


def pause(delay):
    if delay is not None:
        deadline = time.monotonic() + delay
        while time.monotonic() < deadline:
            receive(deadline - time.monotonic())
    else:
        say("  Inspect the title bar; press Enter to continue, Ctrl+C to stop.")
        while True:
            data = receive(3600)
            if b"\r" in data or b"\n" in data:
                break


def run(args):
    targets = list(TARGETS) if args.target == "both" else [args.target]
    if args.query:
        say("Reading current labels without changing them:")
        for target in targets:
            report(target, args.timeout)
        return

    operand = 0 if args.target == "both" else TARGETS[args.target][0]
    depth = 0

    def push():
        nonlocal depth
        send(f"\x1b[22;{operand}t".encode())
        depth += 1

    def pop():
        nonlocal depth
        send(f"\x1b[23;{operand}t".encode())
        depth -= 1

    def show(label):
        say(label)
        for target in targets:
            report(target, args.timeout)
        pause(args.delay)

    def set_labels(stage):
        for target in targets:
            osc = TARGETS[target][0]
            send(f"\x1b]{osc};probe-titles {stage} {target}".encode() + ST)

    say("Title stack demo: original -> A -> B -> A -> original.")
    say("Requests have no acknowledgement; inspect the window and reports.")
    say("Icon labels may only be visible in the window manager's task list.")
    say("Cleanup requests matching pops; restoration needs working, permitted stacks.")
    try:
        push()
        set_labels("A")
        show("1. Requested A labels; saved original labels with CSI 22.")
        push()
        set_labels("B")
        show("2. Requested B labels; saved A on the nested stack.")
        pop()
        show("3. Requested pop with CSI 23; labels should return to A.")
        pop()
        show("4. Requested final pop; labels should return to their original values.")
    finally:
        while depth:
            pop()


def seconds(value):
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise argparse.ArgumentTypeError("must be a finite number greater than zero")
    return result


def interrupt(signum, frame):
    raise KeyboardInterrupt


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--query", action="store_true", help="only query the current labels")
    parser.add_argument("--target", choices=["both", "title", "icon"], default="both",
                        help="labels to exercise (default: both)")
    parser.add_argument("--timeout", type=seconds, default=1.0,
                        help="seconds to wait per report (default: 1)")
    parser.add_argument("--delay", type=seconds,
                        help="seconds to display each stage instead of waiting for Enter")
    args = parser.parse_args()
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        parser.error("run directly in a terminal with both stdin and stdout attached")
    saved = termios.tcgetattr(sys.stdin.fileno())
    previous = signal.signal(signal.SIGTERM, interrupt)
    status = 0
    try:
        tty.setraw(sys.stdin.fileno())
        run(args)
    except KeyboardInterrupt:
        say("Stopped; requested any outstanding stack pops.")
        status = 130
    except (EOFError, OSError) as error:
        print(f"probe-titles: {error}", file=sys.stderr)
        status = 1
    finally:
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, saved)
        signal.signal(signal.SIGTERM, previous)
    return status


if __name__ == "__main__":
    sys.exit(main())
