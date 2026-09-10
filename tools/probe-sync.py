#!/usr/bin/env python3
"""Visually compare slow redraws with DEC synchronized output off and on."""

import argparse
import os
import signal
import sys
import time


SYNC_ON = "\x1b[?2026h"
SYNC_OFF = "\x1b[?2026l"


def emit(text: str) -> None:
    sys.stdout.write(text)
    sys.stdout.flush()


def nonnegative(value: str) -> int:
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must be nonnegative")
    return number


def positive(value: str) -> int:
    number = nonnegative(value)
    if number == 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def dimensions() -> tuple[int, int]:
    columns, rows = os.get_terminal_size(sys.stdout.fileno())
    if columns < 24 or rows < 8:
        raise ValueError("use a terminal at least 24 columns by 8 rows")
    # Leave the last column and row unused to avoid wrapping and scrolling.
    return columns - 1, rows - 1


def line(row: int, text: str, width: int, style: str = "") -> None:
    emit(f"\x1b[{row};1H\x1b[0m\x1b[2K{style}{text[:width]}\x1b[0m")


def draw(mode: str, frame: int, args: argparse.Namespace) -> None:
    width, bottom = dimensions()
    body_rows = bottom - 4
    delay = args.frame_ms / 1000 / body_rows
    label = f"frame {frame:03d} "
    track_width = max(1, width - len(label))
    position = (frame * 4) % track_width
    track = "." * position + "|" + "." * (track_width - position - 1)
    style = "\x1b[30;46m" if frame % 2 else "\x1b[30;43m"
    if mode == "on":
        emit(SYNC_ON)
    for offset in range(body_rows):
        line(offset + 5, label + track, width, style)
        # Separate writes and sleeps make intermediate frames observable.
        time.sleep(delay)
        if offset == body_rows // 2:
            time.sleep(args.hold_ms / 1000)
    if mode == "on":
        emit(SYNC_OFF)
    time.sleep(args.pause_ms / 1000)


def phase(mode: str, args: argparse.Namespace) -> None:
    width, _ = dimensions()
    emit(SYNC_OFF + "\x1b[0m\x1b[2J\x1b[H")
    line(1, f"Synchronized output requested: {mode.upper()}", width)
    expected = (
        "Expect whole-frame swaps; all rows match."
        if mode == "on"
        else "Expect a sweep: old and new rows mix."
    )
    line(2, expected, width)
    line(3, f"Draw: {args.frame_ms} ms; mid-frame hold: {args.hold_ms} ms", width)
    line(4, "Ctrl+C exits. Resize to exercise held repaint.", width)
    time.sleep(0.8)
    for frame in range(1, args.frames + 1):
        draw(mode, frame, args)
    time.sleep(0.7)


def stop(signum: int, _frame: object) -> None:
    raise SystemExit(128 + signum)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog=(
            "Default: compare OFF then ON. ON requests mode 2026; it does not "
            "detect support. Keep frame-ms + hold-ms below 1000 for the normal "
            "comparison. Try --mode on --hold-ms 1500 to expose a terminal's "
            "one-second safety timeout."
        ),
    )
    parser.add_argument("--mode", choices=("compare", "off", "on"), default="compare")
    parser.add_argument("--frames", type=positive, default=8, help="frames per mode (default: 8)")
    parser.add_argument(
        "--frame-ms", type=nonnegative, default=400,
        help="time spent drawing each frame, excluding hold (default: 400 ms)",
    )
    parser.add_argument(
        "--hold-ms", type=nonnegative, default=0,
        help="extra pause halfway through each frame (default: 0 ms)",
    )
    parser.add_argument(
        "--pause-ms", type=nonnegative, default=150,
        help="pause after each completed frame (default: 150 ms)",
    )
    args = parser.parse_args()
    if not sys.stdout.isatty():
        parser.error("stdout must be a terminal; run this inside the terminal being evaluated")
    try:
        dimensions()
    except ValueError as error:
        parser.error(str(error))
    for signum in (signal.SIGTERM, signal.SIGHUP):
        signal.signal(signum, stop)
    try:
        emit(SYNC_OFF + "\x1b[?1049h\x1b[?25l")
        modes = ("off", "on") if args.mode == "compare" else (args.mode,)
        for mode in modes:
            phase(mode, args)
    except KeyboardInterrupt:
        return 130
    except ValueError as error:
        print(f"probe-sync: {error}", file=sys.stderr)
        return 1
    finally:
        emit(SYNC_OFF + "\x1b[0m\x1b[?25h\x1b[?1049l")
    return 0


if __name__ == "__main__":
    sys.exit(main())
