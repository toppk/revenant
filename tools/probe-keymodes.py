#!/usr/bin/env python3
"""
probe-keymodes.py - three stages of terminal keyboard input, each unlocking what
the previous one could not see.

  Stage 1  cooked mode      : line-buffered, kernel eats Ctrl-C/Z/S/Q, echo on
  Stage 2  raw mode         : every encoded byte reaches us; encoding still varies
  Stage 3  kitty protocol   : keys arrive as CSI code;mods[:event] u, unambiguous

Run in a terminal. Press Enter to move between stages, Ctrl-Q or 'q' to quit
a stage early. Use --kitty-only while iterating on protocol support. Requires
a terminal with kitty keyboard protocol support for stage 3 (kitty, foot,
WezTerm, Ghostty, Alacritty, iTerm2, rio, ...).
"""

import argparse
import os
import re
import select
import sys
import termios
import time
import tty

ESC = "\x1b"
CSI = ESC + "["

fd = sys.stdin.fileno()
out = sys.stdout


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def write(s):
    out.write(s)
    out.flush()


def banner(title):
    write(f"\r\n{CSI}1;7m {title} {CSI}0m\r\n")


def read_bytes(timeout=0.05):
    """Read whatever is pending on stdin within timeout. Returns bytes."""
    r, _, _ = select.select([fd], [], [], timeout)
    if not r:
        return b""
    data = os.read(fd, 1024)
    # gather any trailing bytes of a long escape sequence
    while True:
        r, _, _ = select.select([fd], [], [], 0.01)
        if not r:
            break
        more = os.read(fd, 1024)
        if not more:
            break
        data += more
    return data


def show(label, raw):
    write(f"  {label:<28} {repr(raw)}\r\n")


CTRL_NAMES = {
    0: "Ctrl-Space / Ctrl-@", 8: "Ctrl-H  (== Backspace)", 9: "Ctrl-I  (== Tab)",
    13: "Ctrl-M  (== Enter)", 27: "Ctrl-[  (== Escape)", 127: "Backspace (DEL)",
}


def describe_legacy(raw):
    """Best-effort interpretation of a legacy-encoded key press."""
    if len(raw) == 1:
        b = raw[0]
        if b in CTRL_NAMES:
            return CTRL_NAMES[b]
        if 1 <= b <= 26:
            return f"Ctrl-{chr(b + 64)}  (byte {b})"
        if 28 <= b <= 31:
            return f"Ctrl-{chr(b + 64)}  (byte {b})"
        if 32 <= b < 127:
            return f"'{chr(b)}'"
        return f"byte {b}"
    if raw == b"\x1b[A": return "Up"
    if raw == b"\x1b[B": return "Down"
    if raw[:1] == b"\x1b" and len(raw) == 2:
        return f"Alt-{chr(raw[1])}  (ESC + byte)"
    if raw.startswith(b"\x1b["):
        return "CSI sequence (arrow/function key with modifiers?)"
    return "multi-byte / UTF-8"


# --------------------------------------------------------------------------
# kitty protocol
# --------------------------------------------------------------------------

KITTY_QUERY = CSI + "?u"
# flags: 1 disambiguate, 2 report event types, 4 report alternate keys,
#        8 report all keys as escape codes, 16 report associated text
KITTY_REQUESTED_FLAGS = 1 | 2 | 4 | 8 | 16
KITTY_PUSH  = CSI + f">{KITTY_REQUESTED_FLAGS}u"
KITTY_POP   = CSI + "<u"

MOD_NAMES = ["Shift", "Alt", "Ctrl", "Super", "Hyper", "Meta", "CapsLock", "NumLock"]
LOCK_BITS = 64 | 128          # CapsLock, NumLock - ignore when matching hotkeys
EVENT_NAMES = {1: "press", 2: "repeat", 3: "release"}

FUNC_KEYS = {
    27: "Escape", 13: "Enter", 9: "Tab", 127: "Backspace", 32: "Space",
    57358: "CapsLock", 57359: "ScrollLock", 57360: "NumLock", 57361: "PrintScreen",
    57362: "Pause", 57363: "Menu",
    57441: "LeftShift", 57442: "LeftCtrl", 57443: "LeftAlt", 57444: "LeftSuper",
    57447: "RightShift", 57448: "RightCtrl", 57449: "RightAlt", 57450: "RightSuper",
}
TILDE_KEYS = {2: "Insert", 3: "Delete", 5: "PageUp", 6: "PageDown",
              15: "F5", 17: "F6", 18: "F7", 19: "F8", 20: "F9", 21: "F10",
              23: "F11", 24: "F12"}
LETTER_KEYS = {"A": "Up", "B": "Down", "C": "Right", "D": "Left", "H": "Home",
               "F": "End", "P": "F1", "Q": "F2", "S": "F4", "E": "KP_Begin"}


def is_q(code, mods, _event="press"):
    """Plain 'q' event (lock bits ignored)."""
    return code == 113 and (mods & ~LOCK_BITS) == 0


def kitty_flags():
    """Send CSI ?u; return the flags integer from CSI ?<flags>u, or None if no reply."""
    write(KITTY_QUERY + CSI + "c")            # follow with DA1 as a sentinel
    reply = read_bytes(timeout=0.5)
    match = re.search(rb"\x1b\[\?(\d*)u", reply)
    if match is None:
        return None
    digits = match.group(1)
    return int(digits) if digits.isdigit() else 0


def parse_kitty(raw, want_tuple=False):
    """
    Parse one kitty-protocol key event. Format:
        CSI unicode-key-code[:shifted-key[:base-layout-key]] ; modifiers[:event-type] [; text-as-codepoints] u
    Also handles the legacy-compatible forms CSI 1;mods:event X  and  CSI num;mods:event ~
    Returns a human-readable string or None if not a key event.
    """
    if not raw.startswith(b"\x1b["):
        return None
    body = raw[2:].decode("utf-8", "replace")
    if not body:
        return None
    final = body[-1]
    params = body[:-1].split(";")

    def split_mods(p):
        mods, _, ev = p.partition(":")
        mods = int(mods or "1") - 1
        ev = int(ev or "1")
        names = [n for i, n in enumerate(MOD_NAMES) if mods & (1 << i)]
        return names, EVENT_NAMES.get(ev, "?")

    if final == "u":
        keyparts = params[0].split(":")
        code = int(keyparts[0])
        mods, ev = split_mods(params[1] if len(params) > 1 else "1")
        modbits = int((params[1] if len(params) > 1 else "1").partition(":")[0] or "1") - 1
        if want_tuple:
            return code, modbits, ev
        text = ""
        if len(params) > 2 and params[2]:
            text = "".join(chr(int(c)) for c in params[2].split(":") if c)
        if code in FUNC_KEYS:
            name = FUNC_KEYS[code]
        elif 32 < code < 0x110000 and not (57344 <= code <= 63743):
            name = f"'{chr(code)}'"
        else:
            name = f"key#{code}"
        alt = ""
        if len(keyparts) > 1 and keyparts[1]:
            alt = f"  shifted='{chr(int(keyparts[1]))}'"
        return f"{ev:<7} {'+'.join(mods + [name]) if mods else name}{alt}" + (f"  text={text!r}" if text else "")

    if final == "~":
        num = int(params[0])
        if num == 27 and len(params) >= 3:       # xterm modifyOtherKeys: CSI 27;mod;key ~
            modbits = int(params[1]) - 1
            code = int(params[2])
            if want_tuple:
                return code, modbits, "press"
            mods = [n for i, n in enumerate(MOD_NAMES) if modbits & (1 << i)]
            name = FUNC_KEYS.get(code, f"'{chr(code)}'" if 32 < code < 127 else f"key#{code}")
            return f"press   {'+'.join(mods + [name]) if mods else name}   [modifyOtherKeys]"
        mod_param = params[1] if len(params) > 1 else "1"
        mods, ev = split_mods(mod_param)
        if want_tuple:
            modbits = int(mod_param.partition(":")[0] or "1") - 1
            return -1, modbits, ev
        name = TILDE_KEYS.get(num, f"~key#{num}")
        return f"{ev:<7} {'+'.join(mods + [name]) if mods else name}"

    if final in LETTER_KEYS:
        mod_param = params[1] if len(params) > 1 else "1"
        mods, ev = split_mods(mod_param)
        if want_tuple:
            modbits = int(mod_param.partition(":")[0] or "1") - 1
            return -1, modbits, ev
        name = LETTER_KEYS[final]
        return f"{ev:<7} {'+'.join(mods + [name]) if mods else name}"

    return None


# --------------------------------------------------------------------------
# stages
# --------------------------------------------------------------------------

def stage_cooked():
    banner("Stage 1: cooked (canonical) mode")
    write("  The kernel line-buffers input and interprets control characters.\r\n"
          "  Try: type a few keys, then Enter.  Try Ctrl-C (we catch it - watch what happens).\r\n"
          "  Note that Ctrl-U erases the line, arrow keys echo as junk, and nothing\r\n"
          "  reaches us until Enter. Type 'q' + Enter to move on.\r\n\r\n")
    while True:
        try:
            line = sys.stdin.readline()
        except KeyboardInterrupt:
            write("\r\n  -> SIGINT arrived: the kernel turned Ctrl-C into a signal,\r\n"
                  "     we never saw the byte 0x03.\r\n")
            continue
        if line == "":
            return
        show("readline() got:", line)
        if line.strip() == "q":
            return


def stage_raw():
    banner("Stage 2: raw mode")
    write("  Every byte now reaches us: Ctrl-C is byte 3, arrows are ESC sequences.\r\n"
          "  Raw mode does not choose the bytes: the terminal emulator does. Traditional\r\n"
          "  xterm encoding makes these pairs identical, while modern terminals may use\r\n"
          "  CSI-u/fixterms for ambiguous Ctrl keys such as Ctrl-I, Ctrl-M, and Ctrl-[.\r\n"
          "  Compare each pair and inspect what your terminal actually sends:\r\n"
          "     Tab  vs Ctrl-I        Enter vs Ctrl-M       Esc vs Ctrl-[\r\n"
          "     Ctrl-A vs Ctrl-Shift-A          Enter vs Shift-Enter\r\n"
          "     Ctrl-1 vs 1 (nothing)           a vs A vs Shift-a (only case)\r\n"
          "  Press Escape and note the delay before it's reported. Hold a key: no\r\n"
          "  release event. Press 'q' to move on.\r\n\r\n")
    while True:
        raw = read_bytes(timeout=1.0)
        if not raw:
            continue
        show(describe_legacy(raw), raw)
        if raw == b"q":
            return


def stage_kitty():
    banner("Stage 3: kitty keyboard protocol")
    initial_flags = kitty_flags()
    if initial_flags is None:
        write("  This terminal did not answer CSI ? u - kitty protocol not supported.\r\n")
        return stage_modify_other_keys()
    write(KITTY_PUSH)
    granted = None
    stats = None
    try:
        granted = kitty_flags()               # re-query: which flags actually took?
        names = {
            1: "disambiguate",
            2: "event-types",
            4: "alternate-keys",
            8: "all-keys-as-escapes",
            16: "associated-text",
        }
        got = [n for bit, n in names.items() if granted is not None and granted & bit]
        write(
            f"  Initial flags={initial_flags}; requested flags=1|2|4|8|16; "
            f"terminal reports flags={granted}\r\n"
            f"  Active features: {', '.join(got) or 'none'}\r\n"
        )
        if granted is not None and not granted & 2:
            write("  WARNING: event-types was not granted; repeat/release cannot appear.\r\n")
        write("  Now try the same pairs - they should be DIFFERENT. Also try:\r\n"
              "     Ctrl-Shift-A, Shift-Enter, Ctrl-Enter, Ctrl-Tab, Ctrl-1, Ctrl-Q, Super-x\r\n"
              "     Escape (instant, no timeout)   hold a key (expect repeat + release)\r\n"
              "     press a bare Shift or Ctrl key by itself   type composed/Unicode text\r\n"
              "  Every line includes the exact bytes received. CapsLock/NumLock bits are\r\n"
              "  dimmed and ignored for the plain-q exit gesture. Press 'q' to finish.\r\n\r\n")
        stats = _loop_kitty_like(expect_event_types=bool(granted and granted & 2))
    finally:
        write(KITTY_POP)
        restored = kitty_flags()
        if restored == initial_flags:
            write(f"  Kitty flag stack restored to {restored}.\r\n")
        else:
            write(
                f"  WARNING: Kitty flag stack restore mismatch: "
                f"initial={initial_flags}, after-pop={restored}.\r\n"
            )
    if stats is not None and granted is not None and granted & 2:
        write(
            "  Event totals: "
            + ", ".join(f"{name}={stats[name]}" for name in EVENT_NAMES.values())
            + ".\r\n"
        )
        if stats["release"] == 0:
            write("  WARNING: flag 2 was active but no release event was observed.\r\n")


def stage_modify_other_keys():
    write("  Falling back to xterm modifyOtherKeys=2 (CSI > 4;2 m).\r\n"
          "  Ctrl-I vs Tab etc. become distinguishable as CSI 27;mod;key ~ ;\r\n"
          "  no release events or bare modifiers. Press 'q' to finish.\r\n\r\n")
    write(CSI + ">4;2m")
    try:
        _loop_kitty_like()
    finally:
        write(CSI + ">4m")                    # reset modifyOtherKeys to default


def _loop_kitty_like(expect_event_types=False):
    stats = {name: 0 for name in EVENT_NAMES.values()}
    pending_q_deadline = None
    while True:
        timeout = 0.1 if pending_q_deadline is not None else 1.0
        raw = read_bytes(timeout=timeout)
        if not raw:
            if pending_q_deadline is not None and time.monotonic() >= pending_q_deadline:
                return stats
            continue
        parts = [b"\x1b" + p for p in raw.split(b"\x1b") if p]
        if raw == b"\x1b":
            parts = [raw]
        if not raw.startswith(b"\x1b"):
            parts = [raw]
        for ev in parts:
            desc = parse_kitty(ev)
            if desc:
                # dim the lock-bit noise
                desc = desc.replace("CapsLock+", CSI + "2mCapsLock+" + CSI + "0m") \
                           .replace("NumLock+", CSI + "2mNumLock+" + CSI + "0m")
            show(desc or describe_legacy(ev), ev)
            t = parse_kitty(ev, want_tuple=True)
            if t:
                _code, _mods, event = t
                if event in stats:
                    stats[event] += 1
                if is_q(*t):
                    if event == "release" or not expect_event_types:
                        return stats
                    pending_q_deadline = time.monotonic() + 0.5
            elif ev == b"q":
                return stats


# --------------------------------------------------------------------------

def self_test():
    cases = [
        (b"\x1b[105;5u", (105, 4, "press"), "Ctrl+'i'"),
        (b"\x1b[97;1:2u", (97, 0, "repeat"), "repeat"),
        (b"\x1b[97;1:3u", (97, 0, "release"), "release"),
        (b"\x1b[97:65:97;2:1;65u", (97, 1, "press"), "text='A'"),
        (b"\x1b[3;2:3~", (-1, 1, "release"), "Shift+Delete"),
        (b"\x1b[1;2:3A", (-1, 1, "release"), "Shift+Up"),
    ]
    for raw, expected_tuple, expected_text in cases:
        actual_tuple = parse_kitty(raw, want_tuple=True)
        actual_text = parse_kitty(raw)
        if actual_tuple != expected_tuple or actual_text is None or expected_text not in actual_text:
            raise AssertionError(
                f"parse mismatch for {raw!r}: tuple={actual_tuple!r}, text={actual_text!r}"
            )
    if not is_q(113, 0, "press") or not is_q(113, 0, "release") or is_q(113, 1, "press"):
        raise AssertionError("plain-q event matching failed")
    print(f"probe-keymodes parser self-test: {len(cases)} sequences passed")


# --------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    stages = parser.add_mutually_exclusive_group()
    stages.add_argument("--raw-only", action="store_true", help="run only the raw byte probe")
    stages.add_argument(
        "--kitty-only", action="store_true", help="run only the Kitty protocol probe"
    )
    stages.add_argument(
        "--self-test", action="store_true", help="test the protocol decoder without a TTY"
    )
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not os.isatty(fd):
        sys.exit("stdin is not a tty")
    saved = termios.tcgetattr(fd)
    try:
        if args.raw_only:
            tty.setraw(fd)
            stage_raw()
        elif args.kitty_only:
            tty.setraw(fd)
            stage_kitty()
        else:
            stage_cooked()
            tty.setraw(fd)
            stage_raw()
            stage_kitty()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved)
        write("\r\n" + CSI + "0m" + "restored terminal.\r\n")


if __name__ == "__main__":
    main()
