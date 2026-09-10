#!/usr/bin/env python3
"""Probe OSC 10/11/12 foreground, background, and cursor colors.

Run inside the terminal under test. With no action flags, an Enter-paced demo
sets all three colors, resets them individually, and requests restoration of
the original queried values on exit. --query never changes colors. Explicit
--foreground/--background/--cursor and --reset requests remain in effect.
"""

import argparse
import math
import os
import re
import select
import signal
import sys
import termios
import time
import tty


TARGETS = {"foreground": 10, "background": 11, "cursor": 12}
ST = b"\x1b\\"


def send(data):
    while data:
        count = os.write(1, data)
        if count == 0:
            raise OSError("terminal write made no progress")
        data = data[count:]


def say(text):
    send(text.encode() + b"\r\n")


def osc(code, value, terminator):
    payload = f";{value}" if value is not None else ""
    send(f"\x1b]{code}{payload}".encode() + terminator)


def receive(timeout):
    if not select.select([0], [], [], max(0, timeout))[0]:
        return b""
    data = os.read(0, 4096)
    if not data:
        raise EOFError("terminal input closed")
    if b"\x03" in data:
        raise KeyboardInterrupt
    return data


def query(target, args):
    code = TARGETS[target]
    osc(code, "?", args.terminator)
    pattern = re.compile(rb"(?:\x1b\]|\x9d)" + str(code).encode()
                         + rb";([^\x07\x1b\x9c]*)(?:\x07|\x1b\\|\x9c)")
    data = b""
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline and len(data) <= 65536:
        data += receive(deadline - time.monotonic())
        match = pattern.search(data)
        if match:
            payload = match[1]
            rgb = re.fullmatch(rb"rgb:([0-9a-fA-F]{1,4})/([0-9a-fA-F]{1,4})/([0-9a-fA-F]{1,4})", payload)
            if rgb is None:
                say(f"  {target}: unexpected reply {match[0]!r}")
                return None
            components = [round(int(c, 16) * 255 / (16 ** len(c) - 1)) for c in rgb.groups()]
            color = "#" + "".join(f"{c:02x}" for c in components)
            say(f"  {target}: {payload.decode()} (approximately {color})")
            return payload.decode()
    say(f"  {target}: no complete RGB reply (policy denial or unsupported query)")
    return None


def pause(args):
    if args.delay is not None:
        deadline = time.monotonic() + args.delay
        while time.monotonic() < deadline:
            receive(deadline-time.monotonic())
    else:
        say("Inspect colors; Enter continues, Ctrl+C exits.")
        while True:
            data = receive(3600)
            if b"\r" in data or b"\n" in data:
                return


def sample():
    say("Default foreground on default background: AaBb 0123456789")
    send(b"\x1b[38;2;255;255;255;48;2;24;24;24m Explicit RGB reference \x1b[0m\r\n")
    say("The cursor on the next line should use the reported cursor color.")


def query_scheme(args):
    send(b"\x1b[?996n")
    data = b""
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        data += receive(deadline - time.monotonic())
        match = re.search(rb"\x1b\[\?997;([12])n", data)
        if match:
            say("Color scheme: " + ("dark" if match[1] == b"1" else "light") + ".")
            return
    say("No color scheme report; the terminal does not answer CSI ? 996 n.")


def run(args):
    requested = {name: getattr(args, name) for name in TARGETS if getattr(args, name) is not None}
    if args.scheme:
        query_scheme(args)
        return
    if args.query:
        for target in TARGETS:
            query(target, args)
        return
    if args.reset is not None:
        targets = TARGETS if args.reset == "all" else [args.reset]
        for target in targets:
            osc(TARGETS[target] + 100, None, args.terminator)
            say(f"Requested reset of {target} to the configured default.")
        return
    if requested:
        for target, value in requested.items():
            osc(TARGETS[target], value, args.terminator)
            say(f"Requested {target} = {value!r}; use --query and inspect the window.")
        sample()
        return

    say("Reading current colors for cleanup before the demo:")
    originals = {target: query(target, args) for target in TARGETS}
    if any(value is None for value in originals.values()):
        say("Unreadable original colors will be reset to configured defaults on exit.")
    say("Keep Allow Color Ops enabled through cleanup; rerun separately with it off.")
    changed = False
    try:
        sample()
        pause(args)
        changed = True
        for target, value in {"foreground": "#ffe080", "background": "#142850", "cursor": "#ff60c0"}.items():
            osc(TARGETS[target], value, args.terminator)
        say("Requested yellow text, dark blue background, and pink cursor.")
        say("Previously printed default-colored text should change too; the RGB reference stays fixed.")
        for target in TARGETS:
            query(target, args)
        sample()
        pause(args)
        for target in TARGETS:
            osc(TARGETS[target] + 100, None, args.terminator)
            say(f"Requested OSC {TARGETS[target] + 100}: reset {target} only.")
            query(target, args)
            sample()
            pause(args)
    finally:
        if changed:
            for target, value in originals.items():
                osc(TARGETS[target] if value is not None else TARGETS[target] + 100,
                    value, args.terminator)
            say("Requested original RGB values (or defaults where unreadable); verify visually.")


def seconds(value):
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise argparse.ArgumentTypeError("must be finite and greater than zero")
    return result


def color(value):
    if not value or any(ord(char) < 32 or 127 <= ord(char) <= 159 or char == ";" for char in value):
        raise argparse.ArgumentTypeError("use a color name, #RRGGBB, or rgb:R/G/B without controls or semicolons")
    return value


def interrupt(signum, frame):
    raise KeyboardInterrupt


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--query", action="store_true", help="only read current colors")
    parser.add_argument("--scheme", action="store_true", help="only ask CSI ? 996 n for light or dark")
    parser.add_argument("--reset", choices=["all", *TARGETS], help="request configured defaults")
    for name in TARGETS:
        parser.add_argument("--" + name, type=color, metavar="COLOR", help=f"request a {name} color")
    parser.add_argument("--delay", type=seconds, help="seconds per demo stage instead of Enter")
    parser.add_argument("--timeout", type=seconds, default=1.0, help="seconds per query (default 1)")
    parser.add_argument("--bel", action="store_true", help="use BEL instead of ST as the OSC terminator")
    args = parser.parse_args()
    if sum([args.query, args.scheme, args.reset is not None,
            any(getattr(args, name) is not None for name in TARGETS)]) > 1:
        parser.error("choose query, reset, or color-setting options")
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        parser.error("run directly in a terminal with stdin and stdout attached")
    args.terminator = b"\x07" if args.bel else ST
    saved = termios.tcgetattr(0)
    previous = signal.signal(signal.SIGTERM, interrupt)
    status = 0
    try:
        tty.setraw(0)
        run(args)
    except KeyboardInterrupt:
        say("Stopped.")
        status = 130
    except (EOFError, OSError) as error:
        print(f"probe-dynamic-colors: {error}", file=sys.stderr)
        status = 1
    finally:
        termios.tcsetattr(0, termios.TCSADRAIN, saved)
        signal.signal(signal.SIGTERM, previous)
    return status


if __name__ == "__main__":
    sys.exit(main())
