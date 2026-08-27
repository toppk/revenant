#!/usr/bin/env python3
"""
probe-emoji.py - interactive terminal emoji and grapheme capability probe

Run this in each terminal under comparison. Each section prints a
sample and an alignment ruler so you can see whether the terminal placed
the cursor / following text in the right column.

The alignment test works by printing an emoji followed by a '|' and a
separately printed '|' at the column where it *should* land. If the two
bars line up, the terminal handled the width correctly.

Usage: python3 tools/probe-emoji.py [--no-pause]
"""

import os
import sys
import time

PAUSE = "--no-pause" not in sys.argv

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

BOLD = "\033[1m"
DIM = "\033[2m"
RESET = "\033[0m"
CYAN = "\033[36m"
YELLOW = "\033[33m"
GREEN = "\033[32m"
RED = "\033[31m"


def header(title):
    print()
    print(f"{BOLD}{CYAN}== {title} =={RESET}")
    print()


def codepoints(s):
    """Show the code points that make up a string."""
    return " ".join(f"U+{ord(c):04X}" for c in s)


def wait():
    if PAUSE:
        try:
            input(f"{DIM}  [Enter to continue]{RESET}")
        except EOFError:
            pass


def show(label, sample, expect_cells=2, note=""):
    """
    Print a sample with an alignment check.

    We print:  <sample>|    (the terminal decides where | ends up)
    then move the cursor to column expect_cells+1 and print another |.
    If both bars overlap, the terminal counted the width correctly.
    """
    print(f"  {BOLD}{label}{RESET}")
    print(f"    {DIM}{codepoints(sample)}{RESET}")
    # Alignment line: print sample, a bar, then use absolute cursor
    # positioning to drop a second bar where it *should* be.
    # Column 5 is the start of the sample (4 spaces of indent + 1).
    target_col = 5 + expect_cells
    sys.stdout.write(f"    {sample}|")
    sys.stdout.write(f"\033[{target_col}G")  # move to absolute column
    sys.stdout.write(f"{GREEN}|{RESET}")
    sys.stdout.write("\n")
    print(f"    {DIM}(expect {expect_cells} cells; the two bars should overlap){RESET}")
    if note:
        print(f"    {YELLOW}{note}{RESET}")
    print()


def box_test(samples, width=24):
    """Draw a box whose right edge only stays straight if widths are right."""
    top = "    ┌" + "─" * width + "┐"
    bot = "    └" + "─" * width + "┘"
    print(top)
    for text, cells in samples:
        pad = width - cells
        print(f"    │{text}{' ' * pad}│")
    print(bot)
    print()


# ---------------------------------------------------------------------------
# sections
# ---------------------------------------------------------------------------

def section_env():
    header("Environment")
    print(f"  TERM          = {os.environ.get('TERM', '?')}")
    print(f"  TERM_PROGRAM  = {os.environ.get('TERM_PROGRAM', '?')}")
    print(f"  GHOSTTY       = {'yes' if 'GHOSTTY_RESOURCES_DIR' in os.environ else 'no'}")
    print(f"  XTERM_VERSION = {os.environ.get('XTERM_VERSION', '-')}")
    print(f"  LANG          = {os.environ.get('LANG', '?')}")
    print()
    print("  Note: this script tests DISPLAY, not data. Every terminal will")
    print("  receive these bytes fine; the question is what you see.")
    wait()


def section_basic():
    header("1. Basic single-code-point emoji (BMP and astral plane)")
    print("  xterm: monochrome outline at best, often a box or blank.")
    print("  Ghostty: full-color glyph from the system emoji font.")
    print()
    show("Smiley (astral plane, U+1F600)", "😀")
    show("Rocket", "🚀")
    show("Pizza", "🍕")
    show("Snowman (BMP, U+2603, no color by default)", "☃", expect_cells=1)
    wait()


def section_variation():
    header("2. Variation selectors (text vs emoji presentation)")
    print("  Same base character, U+FE0F requests the emoji glyph.")
    print("  xterm: renders base char only, ignores/misplaces FE0F.")
    print("  Ghostty: switches to the color emoji form and widens to 2 cells.")
    print()
    show("Heart, text style", "❤", expect_cells=1)
    show("Heart + VS16 (emoji style)", "❤️", expect_cells=2)
    show("Warning sign, text style", "⚠", expect_cells=1)
    show("Warning sign + VS16", "⚠️", expect_cells=2)
    show("Digit + VS16 + keycap (3 code points, one glyph)", "1️⃣", expect_cells=2)
    wait()


def section_skin_tone():
    header("3. Skin tone modifiers (base + Fitzpatrick modifier)")
    print("  xterm: draws the hand and the modifier as two separate glyphs")
    print("         (the modifier often shows as a colored square).")
    print("  Ghostty: merges into a single recolored glyph.")
    print()
    show("Waving hand", "👋")
    show("Waving hand + medium skin tone", "👋🏽")
    show("Thumbs up + dark skin tone", "👍🏿")
    print("  All tones in a row (should be 5 evenly spaced hands):")
    print("    👋🏻 👋🏼 👋🏽 👋🏾 👋🏿")
    print()
    wait()


def section_zwj():
    header("4. ZWJ sequences (multiple emoji glued with U+200D)")
    print("  These are the hardest case. A family emoji can be 7 code points.")
    print("  xterm: shows each person separately -> 3-4 glyphs, wrong width.")
    print("  Ghostty: grapheme segmentation -> one glyph, 2 cells.")
    print()
    show("Family (man, woman, girl)", "👨‍👩‍👧")
    show("Family (woman, woman, boy, boy)", "👩‍👩‍👦‍👦")
    show("Woman technologist (woman + ZWJ + laptop)", "👩‍💻")
    show("Firefighter with skin tone", "🧑🏾‍🚒")
    show("Rainbow flag (white flag + VS16 + ZWJ + rainbow)", "🏳️‍🌈")
    show("Pirate flag (black flag + VS16 + ZWJ + skull)", "🏴‍☠️")
    show("Polar bear (bear + ZWJ + snowflake + VS16)", "🐻‍❄️")
    show("Heart on fire", "❤️‍🔥")
    wait()


def section_flags():
    header("5. Country flags (pairs of regional indicator symbols)")
    print("  Each flag is two code points, e.g. U+1F1FA U+1F1F8 = US.")
    print("  xterm: shows the letters 'U' 'S' in boxes, 4 cells wide.")
    print("  Ghostty: one flag, 2 cells.")
    print()
    show("United States", "🇺🇸")
    show("Japan", "🇯🇵")
    show("Brazil", "🇧🇷")
    print("  Adjacent flags must NOT merge across boundaries:")
    print("    🇺🇸🇯🇵🇧🇷  <- should be exactly three flags")
    print()
    show("Subdivision flag: Scotland (black flag + 6 tag chars + cancel tag)",
         "🏴󠁧󠁢󠁳󠁣󠁴󠁿",
         note="8 code points; xterm has no chance here.")
    wait()


def section_combining():
    header("6. Combining marks / stacked characters (non-emoji graphemes)")
    print("  Not emoji, but the same grapheme-cluster machinery.")
    print()
    show("e + combining acute (2 code points, 1 cell)", "e\u0301", expect_cells=1)
    show("Devanagari conjunct (क्ष)", "क्ष", expect_cells=2,
         note="Ghostty shapes this into one ligature; xterm shows pieces.")
    show("Zalgo-style stacking", "a\u0300\u0301\u0302\u0303\u0304\u0305",
         expect_cells=1)
    wait()


def section_alignment():
    header("7. Layout stress test: box with mixed content")
    print("  If the terminal computes every width correctly the right border")
    print("  is a straight vertical line. Ragged edge = width bugs.")
    print()
    box_test([
        ("plain ascii text", 16),
        ("🚀 rocket", 9),
        ("👨‍👩‍👧 family", 9),
        ("🇺🇸🇯🇵 flags", 10),
        ("👋🏽 wave tone", 12),
        ("🏳️‍🌈 pride", 8),
        ("❤️ heart", 8),
        ("🏴󠁧󠁢󠁳󠁣󠁴󠁿 scotland", 11),
    ])
    wait()


def section_cursor():
    header("8. Interactive cursor test")
    print("  After the line below, the cursor should sit directly after the")
    print("  closing ']'. In xterm it usually lands too far right (or left)")
    print("  because the ZWJ sequence was counted as several characters.")
    print()
    sys.stdout.write("    [👩‍💻👨‍👩‍👧🏳️‍🌈]")
    sys.stdout.flush()
    time.sleep(1.5)
    print()
    print()
    wait()


def section_mode2027():
    header("9. Mode 2027 (grapheme cluster width negotiation)")
    print("  Ghostty implements DECSET 2027, which lets applications ask the")
    print("  terminal to treat grapheme clusters as single cells. We enable")
    print("  it and re-run one hard case. xterm ignores the sequence entirely.")
    print()
    sys.stdout.write("\033[?2027h")  # enable mode 2027
    show("Family with mode 2027 on", "👨‍👩‍👧‍👦")
    sys.stdout.write("\033[?2027l")  # disable again
    sys.stdout.flush()
    wait()


def section_summary():
    header("Summary: what to look for")
    rows = [
        ("Color glyphs", "no (mono or boxes)", "yes"),
        ("Variation selector U+FE0F", "ignored", "honored"),
        ("Skin-tone modifiers", "two glyphs", "one glyph"),
        ("ZWJ sequences (family, flags)", "split apart", "single glyph"),
        ("Regional-indicator flags", "letters in boxes", "flag"),
        ("Tag-sequence flags (Scotland)", "garbage", "flag"),
        ("Cursor column after emoji", "often wrong", "correct"),
        ("Mode 2027", "unsupported", "supported"),
        ("Automatic font fallback", "manual (-fd)", "automatic"),
    ]
    print(f"  {'Feature':<32}{'xterm':<22}{'Ghostty'}")
    print(f"  {'-'*32}{'-'*22}{'-'*10}")
    for feat, x, g in rows:
        print(f"  {feat:<32}{RED}{x:<22}{RESET}{GREEN}{g}{RESET}")
    print()


# ---------------------------------------------------------------------------

def main():
    if sys.stdout.encoding is None or "utf" not in sys.stdout.encoding.lower():
        print(
            "Your stdout is not UTF-8. Try: "
            "PYTHONIOENCODING=utf-8 python3 tools/probe-emoji.py"
        )
        sys.exit(1)

    print(f"{BOLD}Terminal emoji and grapheme capability probe{RESET}")
    print("Run in xterm+, xterm, and Ghostty and compare what you see.")

    section_env()
    section_basic()
    section_variation()
    section_skin_tone()
    section_zwj()
    section_flags()
    section_combining()
    section_alignment()
    section_cursor()
    section_mode2027()
    section_summary()


if __name__ == "__main__":
    main()
