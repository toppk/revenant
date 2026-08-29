#!/usr/bin/env python3
"""Emoji-only two-regime width and mode-2027 probe.

Non-emoji combining, conjunct, and mark-placement cases live in
probe-fonts.py. Width is measured mechanically with CPR; glyph artwork
is displayed but never graded. Legacy expectations use contemporary
glibc-style per-codepoint wcwidth arithmetic except where an accept set
explicitly allows table variation.
"""

from _width_probe import RightMarginSample, run_probe

SECTIONS = [
    (
        "1. Single-code-point emoji",
        [
            ("Grinning face U+1F600", "\U0001f600", {2}, {2}, ""),
            ("Rocket U+1F680", "\U0001f680", {2}, {2}, ""),
            ("Pizza U+1F355", "\U0001f355", {2}, {2}, ""),
            (
                "Snowman U+2603 (text-default)",
                "\u2603",
                {1},
                {1},
                "1 cell in BOTH regimes: no VS16, text presentation.",
            ),
        ],
    ),
    (
        "2. Variation selectors",
        [
            ("Heart U+2764 bare", "\u2764", {1}, {1}, ""),
            (
                "Heart + VS15",
                "\u2764\ufe0e",
                {1},
                {1},
                "Forced text presentation remains narrow in both regimes.",
            ),
            (
                "Heart + VS16",
                "\u2764\ufe0f",
                {1},
                {2},
                "THE regime discriminator: legacy 1+0=1, cluster 2. A terminal "
                "showing 2 with 2027 off widened unilaterally.",
            ),
            ("Warning U+26A0 bare", "\u26a0", {1}, {1}, ""),
            ("Warning + VS16", "\u26a0\ufe0f", {1}, {2}, ""),
            (
                "Emoji-default grinning face + VS15",
                "\U0001f600\ufe0e",
                {2},
                {2},
                "Verified libghostty behavior: VS15 changes presentation but "
                "does not narrow this emoji-default base in mode 2027.",
            ),
            (
                "Keycap 1+VS16+U+20E3",
                "1\ufe0f\u20e3",
                {1},
                {2},
                "legacy: 1+0+0 (20E3 is combining).",
            ),
            (
                "Keycap 1+U+20E3 (no VS16)",
                "1\u20e3",
                {1},
                {1},
                "Exercises optional-VS keycap syntax; rendering remains visual-only.",
            ),
            ("Keycap #+U+20E3", "#\u20e3", {1}, {1}, ""),
            ("Keycap *+U+20E3", "*\u20e3", {1}, {1}, ""),
        ],
    ),
    (
        "3. Skin-tone modifiers",
        [
            ("Waving hand", "\U0001f44b", {2}, {2}, ""),
            (
                "Wave + medium tone",
                "\U0001f44b\U0001f3fd",
                {4},
                {2},
                "legacy: 2+2 (modifiers are EAW Wide). Two glyphs at width 4 "
                "is CORRECT rendering with 2027 off.",
            ),
            ("Thumbs up + dark tone", "\U0001f44d\U0001f3ff", {4}, {2}, ""),
        ],
    ),
    (
        "4. ZWJ sequences",
        [
            (
                "Family M+W+G",
                "\U0001f468\u200d\U0001f469\u200d\U0001f467",
                {6},
                {2},
                "legacy: 2+0+2+0+2.",
            ),
            (
                "Family W+W+B+B",
                "\U0001f469\u200d\U0001f469\u200d\U0001f466\u200d\U0001f466",
                {8},
                {2},
                "",
            ),
            ("Woman technologist", "\U0001f469\u200d\U0001f4bb", {4}, {2}, ""),
            (
                "Firefighter + tone",
                "\U0001f9d1\U0001f3fe\u200d\U0001f692",
                {6},
                {2},
                "",
            ),
            (
                "Rainbow flag",
                "\U0001f3f3\ufe0f\u200d\U0001f308",
                {3},
                {2},
                "legacy: 1F3F3 is EAW Neutral (1) + VS(0) + ZWJ(0) + rainbow(2) "
                "= 3. Odd widths are legacy's honest arithmetic.",
            ),
            (
                "Pirate flag",
                "\U0001f3f4\u200d\u2620\ufe0f",
                {3},
                {2},
                "legacy: 2+0+1+0.",
            ),
            ("Polar bear", "\U0001f43b\u200d\u2744\ufe0f", {3}, {2}, ""),
            (
                "Heart on fire",
                "\u2764\ufe0f\u200d\U0001f525",
                {3},
                {2},
                "legacy: 1+0+0+2.",
            ),
        ],
    ),
    (
        "5. Flags",
        [
            (
                "US (RI pair)",
                "\U0001f1fa\U0001f1f8",
                {2, 4},
                {2},
                "UNDERSPECIFIED in legacy: wcwidth tables disagree on RI "
                "(1 or 2 each). Both 2 and 4 accepted; neither judged.",
            ),
            ("Japan", "\U0001f1ef\U0001f1f5", {2, 4}, {2}, ""),
            (
                "Scotland (tag sequence)",
                "\U0001f3f4\U000e0067\U000e0062\U000e0073\U000e0063\U000e0074"
                "\U000e007f",
                {2},
                {2},
                "legacy: 2 + six tag chars at 0. Same width in both regimes; "
                "only the RENDERING differs (and rendering is not graded).",
            ),
        ],
    ),
    (
        "6. Runs and boundaries (concatenated clusters)",
        [
            (
                "Bracketed run: [tech+family+flag]",
                "[\U0001f469\u200d\U0001f4bb\U0001f468\u200d\U0001f469\u200d"
                "\U0001f467\U0001f3f3\ufe0f\u200d\U0001f308]",
                {15},
                {8},
                "brackets are INSIDE the sample: ASCII hard against both "
                "cluster edges. legacy 1+4+6+3+1. Per-sample passes do not "
                "imply run passes; edge segmentation and error accumulation "
                "only show up here.",
            ),
            (
                "Adjacent flags (RI pairing parity)",
                "\U0001f1fa\U0001f1f8\U0001f1ef\U0001f1f5\U0001f1e7\U0001f1f7",
                {6, 12},
                {6},
                "six RIs = three flags (US JP BR). A mis-pairer shows the "
                "same width with the WRONG flags: verify by eye too.",
            ),
            (
                "Odd RI count (five indicators)",
                "\U0001f1fa\U0001f1f8\U0001f1ef\U0001f1f5\U0001f1e7",
                {5, 10},
                {6},
                "cluster: US + JP + lone B = 2+2+2. The trailing lone "
                "indicator is the parity edge case. Width 2 for the lone RI "
                "is libghostty's choice; another terminal may disagree.",
            ),
            (
                "ZWJ only WITHIN clusters, none between",
                "\U0001f468\u200d\U0001f469\u200d\U0001f467\U0001f469\u200d\U0001f4bb",
                {10},
                {4},
                "family then technologist, no joiner between: must stay two "
                "clusters. legacy 6+4.",
            ),
            (
                "Dangling ZWJ before plain text",
                "\U0001f468\u200dx",
                {3},
                {3},
                "UAX #29 breaks after ZWJ when the next char is not "
                "pictographic: both regimes agree on 3. Any other answer is "
                "a segmentation bug.",
            ),
            (
                "CJK-edged run: 日+technologist+日",
                "\u65e5\U0001f469\u200d\U0001f4bb\u65e5",
                {8},
                {6},
                "Wide neighbors expose cluster-boundary off-by-ones: legacy "
                "2+4+2, cluster 2+2+2.",
            ),
            RightMarginSample(
                "Right-margin wrap: grinning face",
                "\U0001f600",
                "The two-cell glyph must wrap as one unit, never split across rows.",
            ),
        ],
    ),
    (
        "7. Capacity and pathological clusters",
        [
            (
                "Four-person family, four tones (41 UTF-8 bytes)",
                "\U0001f468\U0001f3fd\u200d\U0001f469\U0001f3fe\u200d"
                "\U0001f467\U0001f3ff\u200d\U0001f466\U0001f3fb",
                {16},
                {2},
                "Control below the renderer's 64-byte cell buffer: legacy "
                "four people + four wide modifiers, cluster one family.",
            ),
            (
                "Heart + 25 VS16 selectors (78 UTF-8 bytes)",
                "\u2764" + "\ufe0f" * 25,
                {1},
                {2},
                "Exceeds XTP_VISUAL_TEXT_CAPACITY. Width and the following "
                "sample must remain sound; placeholder artwork is not graded.",
            ),
            (
                "Post-overflow sentinel",
                "X",
                {1},
                {1},
                "Must still align after the over-capacity grapheme.",
            ),
        ],
    ),
]


if __name__ == "__main__":
    run_probe(
        "Two-regime emoji width probe",
        "Measure emoji cursor advance under legacy and mode-2027 contracts.",
        SECTIONS,
    )
