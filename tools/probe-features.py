#!/usr/bin/env python3
"""Manual fixtures for dispatchable TODO chunks; run inside the terminal under test.

Use --list to find a probe, then SUBCOMMAND --help. A request or fixture is not
proof of feature support. No child commands are executed by these probes.
"""
import argparse
import base64
import contextlib
import math
import os
from pathlib import Path
import re
import select
import shutil
import signal
import sys
import termios
import time
import tty
from urllib.parse import quote

ST = b"\x1b\\"
PROBES = {
    "answerback": "ENQ reply (answerbackString); no Ops category",
    "tcap": "XTGETTCAP names and values; Allow Tcap Ops",
    "mouse": "Mouse/focus report bytes; Allow Mouse Ops",
    "font": "OSC 50 query or explicit set; Allow Font Ops (currently disabled)",
    "underline": "SGR 58/59 truecolor and indexed underline samples",
    "pointer": "OSC 22 named pointer shapes; inspect the pointer over the grid",
    "cursor": "Startup cursor appearance and DECRQSS; optional style cycle",
    "identity": "DA1, DA2 and XTVERSION replies",
    "unknown": "Unsupported APC callback fixture, not arbitrary passthrough",
    "cwd": "OSC 7 URI for an existing local directory",
    "prompts": "Synthetic OSC 133 prompt boundaries for navigation",
    "pipe": "Synthetic last-command output for a future pipe action",
    "notify": "OSC 9 and OSC 777 notification fixtures",
    "progress": "OSC 9;4 progress states, reset on exit",
    "glyphs": "Box, block, braille and powerline cell-joining samples",
    "copy": "Selection text for the copy-highlight timer/overlay",
    "search": "Scrollback search fixture with repeated, wrapped and Unicode matches",
    "graphics": "Small inline Kitty RGBA placement; deletes only its own image",
}


def send(data):
    while data:
        n = os.write(1, data)
        if n <= 0:
            raise OSError("terminal write made no progress")
        data = data[n:]


def say(text):
    send(text.encode() + b"\r\n")


def osc(selector, value):
    send(f"\x1b]{selector};{value}".encode() + ST)


def receive(timeout):
    if not select.select([0], [], [], max(0, timeout))[0]:
        return b""
    data = os.read(0, 8192)
    if not data:
        raise EOFError("terminal input closed")
    if b"\x03" in data:
        raise KeyboardInterrupt
    return data


def collect(timeout, terminator=None):
    result = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and len(result) < 65536:
        result += receive(deadline - time.monotonic())
        if terminator and result.endswith(terminator):
            break
    return result


def ask(sequence, timeout, terminator=None):
    # Flush before sending, never after: replies may arrive immediately.
    termios.tcflush(0, termios.TCIFLUSH)
    send(sequence)
    return collect(timeout, terminator)


def show_reply(label, sequence, args, terminator=None):
    result = ask(sequence, args.timeout, terminator)
    say(f"{label}: {result!r}" if result else f"{label}: no reply (unsupported, denied, or empty)")
    return result


def pause(args):
    if args.delay is not None:
        collect(args.delay)
        return
    say("Inspect the result, then press Enter; Ctrl+C exits.")
    while True:
        data = receive(3600)
        if b"\r" in data or b"\n" in data:
            return


def tcap_query(name):
    return b"\x1bP+q" + name.encode().hex().encode() + ST


def decode_tcap(data):
    results = []
    for match in re.finditer(rb"\x1bP([01])\+r([^\x1b]*)\x1b\\", data):
        for field in match[2].split(b";"):
            key, separator, value = field.partition(b"=")
            try:
                results.append((match[1] == b"1", bytes.fromhex(key.decode()),
                                bytes.fromhex(value.decode()) if separator else None))
            except (ValueError, UnicodeError):
                results.append((False, b"malformed hex", field))
    return results


def mode_sequence(mode, enabled):
    return f"\x1b[?{mode}{'h' if enabled else 'l'}".encode()


def mode_snapshot(modes, args):
    result = {}
    for mode in modes:
        data = ask(f"\x1b[?{mode}$p".encode(), args.timeout, b"y")
        match = re.search(rb"\x1b\[\?" + str(mode).encode() + rb";([1-4])\$y", data)
        result[mode] = match is not None and match[1] in (b"1", b"3")
    return result


def shell_fixture(args):
    directory = args.cwd.resolve()
    if not directory.is_dir():
        raise ValueError(f"not a directory: {directory}")
    osc(7, "file://localhost" + quote(str(directory), safe="/"))
    say(f"Reported cwd: {directory}")
    if args.probe == "cwd":
        say("Inspect backend diagnostics or the future new-window/pipe action's cwd.")
        pause(args)
        return
    say("Synthetic shell markers follow; displayed commands are not executed.")
    for n in range(1, 4):
        osc(133, "A")
        send(f"probe-{n}$ ".encode())
        osc(133, "B")
        say(f"synthetic-command-{n}")
        osc(133, "C")
        say(f"COMMAND-{n}-BEGIN")
        say("output with Unicode: café 界; literal shell text: $(do-not-execute)")
        say(f"COMMAND-{n}-END")
        osc(133, "D;0")
    osc(133, "A")
    send(b"probe-ready$ ")
    osc(133, "B")
    say("[waiting for manual navigation or pipe action]")
    say("Pipe must contain only COMMAND-3-BEGIN through COMMAND-3-END, not prompts.")
    pause(args)
    osc(133, "C")
    osc(133, "D;0")


def run(args, cleanup):
    name = args.probe
    if name == "answerback":
        show_reply("ENQ", b"\x05", args)
    elif name == "tcap":
        for capability in args.cap or ["TN", "Co", "RGB", "not-a-capability"]:
            data = show_reply(capability, tcap_query(capability), args, ST)
            for valid, key, value in decode_tcap(data):
                say(f"  supported={valid} name={key!r} value={value!r}")
    elif name == "font":
        if args.font is not None:
            osc(50, args.font)
            say("Sent an explicit persistent font request; restore via the font menu.")
        show_reply("OSC 50", b"\x1b]50;?" + ST, args, ST)
    elif name == "identity":
        for label, request, end in [("DA1", b"\x1b[c", b"c"),
                                    ("DA2", b"\x1b[>c", b"c"),
                                    ("XTVERSION", b"\x1b[>q", ST)]:
            show_reply(label, request, args, end)
    elif name == "mouse":
        modes = [9, 1000, 1002, 1003, 1004, 1006]
        saved = mode_snapshot(modes, args)
        def restore():
            for mode in modes:
                send(mode_sequence(mode, False))
            for mode, enabled in saved.items():
                if enabled:
                    send(mode_sequence(mode, True))
        cleanup.callback(restore)
        for mode in modes:
            send(mode_sequence(mode, False))
        for mode in [args.mode, 1004, 1006]:
            send(mode_sequence(mode, True))
        say("Move, click, scroll and change focus. Ctrl+right-click toggles Allow Mouse Ops.")
        say("Unchecked: reports stop and ordinary selection works. q or Ctrl+C exits.")
        end = time.monotonic() + args.seconds
        while time.monotonic() < end:
            data = receive(min(.25, end-time.monotonic()))
            if data == b"q":
                break
            if data:
                say(f"input: {data!r}")
    elif name == "underline":
        cleanup.callback(send, b"\x1b[0m")
        for style in range(1, 6):
            for color in ["58;2;255;40;40", "58;2;40;120;255", "58;5;46"]:
                say(f"\x1b[4:{style};{color}mUnderline style {style}: AaBb gjpq 界\x1b[59m default color\x1b[0m")
        pause(args)
    elif name == "pointer":
        cleanup.callback(osc, 22, "default")
        for shape in ["text", "pointer", "crosshair", "wait", "default"]:
            osc(22, shape)
            say(f"Requested pointer shape: {shape}. Move over the text grid.")
            pause(args)
    elif name == "cursor":
        say("Inspect the startup cursor before any DECSCUSR request.")
        show_reply("Cursor style", b"\x1bP$q q" + ST, args, ST)
        pause(args)
        if args.styles:
            cleanup.callback(send, b"\x1b[0 q")
            for style in range(1, 7):
                send(f"\x1b[{style} q".encode())
                say(f"Requested DECSCUSR style {style}")
                pause(args)
    elif name == "unknown":
        send(b"\x1b_unknown-probe-dispatch" + ST)
        say("Sent an unsupported APC. Check -debug diagnostics; no reply is required.")
        say("The current callback cannot establish unknown OSC/CSI support.")
    elif name in ("cwd", "prompts", "pipe"):
        cleanup.callback(osc, 7, "file://localhost" + quote(str(Path.cwd()), safe="/"))
        if name != "cwd":
            cleanup.callback(osc, 133, "D;0")
            cleanup.callback(osc, 133, "C")
        shell_fixture(args)
    elif name == "notify":
        say("Switch focus away now; notification requests will be sent after three seconds.")
        collect(3)
        osc(9, "xterm+ manual OSC 9 notification")
        osc(777, "notify;xterm+ probe;Manual OSC 777 notification")
        say("Requests sent. Inspect urgency/notification delivery and focus behavior.")
        pause(args)
    elif name == "progress":
        cleanup.callback(osc, 9, "4;0")
        for state in ["1;10", "1;60", "2;60", "3", "4;60", "0"]:
            osc(9, "4;" + state)
            say(f"Requested progress state {state}; inspect title/indicator.")
            pause(args)
    elif name == "glyphs":
        for line in ["┌────────┬────────┐  ╔════════╦════════╗", "│ normal │ boxes  │  ║ double ║ boxes  ║",
                     "├────────┼────────┤  ╠════════╬════════╣", "└────────┴────────┘  ╚════════╩════════╝",
                     "▁▂▃▄▅▆▇█ ▉▊▋▌▍▎▏ ▀▄▌▐░▒▓█", "⠁⠃⠇⠏⠟⠿⡿⣿ ⣿⣿⣿⣿⣿⣿⣿⣿", "  branch  status "]:
            say(line)
        say("Check joined edges at multiple font sizes, bitmap/Xft and bold; compare forceBoxChars.")
        pause(args)
    elif name == "copy":
        say("COPY-PROBE alpha café 界 omega COPY-END")
        say("Select the line, copy, and inspect highlight duration, final selection and paste contents.")
        pause(args)
    elif name == "search":
        width = shutil.get_terminal_size().columns
        for n in range(120):
            say(f"row {n:03d} " + ("FIND-ME café 界 e\u0301" if n % 17 == 0 else "ordinary scrollback text"))
        say("x" * max(1, width-4) + "WRAPPED-NEEDLE")
        say("Search FIND-ME, café, a missing string, then WRAPPED-NEEDLE; verify next/prev and Escape.")
        pause(args)
    elif name == "graphics":
        image_id = 100000 + os.getpid() % 100000
        cleanup.callback(send, f"\x1b_Ga=d,d=I,i={image_id}".encode() + ST)
        pixels = bytes([255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255])
        command = f"\x1b_Ga=T,f=32,s=2,v=2,c=12,r=6,i={image_id};".encode()
        show_reply("Kitty placement", command + base64.b64encode(pixels) + ST, args, ST)
        say("Expect red/green above blue/yellow. This tests inline static placement only.")
        say("Scroll and resize; animation, shared memory and placeholders need later probes.")
        pause(args)


def positive(value):
    value = float(value)
    if not math.isfinite(value) or value <= 0:
        raise argparse.ArgumentTypeError("must be positive and finite")
    return value


def safe_text(value):
    if any(ord(c) < 32 or 127 <= ord(c) <= 159 for c in value):
        raise argparse.ArgumentTypeError("control characters are not accepted")
    return value


def interrupt(*_):
    raise KeyboardInterrupt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="list probes and their purpose")
    sub = parser.add_subparsers(dest="probe")
    for name, description in PROBES.items():
        child = sub.add_parser(name, help=description, description=description)
        child.add_argument("--timeout", type=positive, default=.5, help="seconds to wait for each reply")
        child.add_argument("--delay", type=positive, help="seconds per visual stage instead of Enter")
        if name == "tcap":
            child.add_argument("--cap", action="append", type=safe_text, help="query a capability; repeatable")
        if name == "font":
            child.add_argument("--font", type=safe_text, help="explicit persistent OSC 50 font request")
        if name == "mouse":
            child.add_argument("--mode", type=int, choices=[9, 1000, 1002, 1003], default=1000)
            child.add_argument("--seconds", type=positive, default=20, help="report capture duration")
        if name == "cursor":
            child.add_argument("--styles", action="store_true", help="also cycle DECSCUSR, then reset to default")
        if name in ("cwd", "prompts", "pipe"):
            child.add_argument("--cwd", type=Path, default=Path.cwd())
    args = parser.parse_args()
    if args.list:
        for name, description in PROBES.items():
            print(f"{name:12} {description}")
        return 0
    if not args.probe:
        parser.print_help()
        return 0
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        parser.error("run directly inside the terminal under test")
    saved = termios.tcgetattr(0)
    old_signal = signal.signal(signal.SIGTERM, interrupt)
    try:
        tty.setraw(0)
        with contextlib.ExitStack() as cleanup:
            run(args, cleanup)
    except KeyboardInterrupt:
        say("Stopped; restoration requested.")
        return 130
    except (OSError, EOFError, ValueError) as error:
        print(f"probe-features: {error}", file=sys.stderr)
        return 1
    finally:
        termios.tcsetattr(0, termios.TCSADRAIN, saved)
        signal.signal(signal.SIGTERM, old_signal)
    return 0


if __name__ == "__main__":
    sys.exit(main())
