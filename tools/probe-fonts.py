#!/usr/bin/env python3
"""Combining-mark, conjunct, and text-shaping width/ink probe.

Emoji presentation, modifiers, ZWJ sequences, and flags live in
probe-emoji.py. Width is measured mechanically with CPR; mark placement,
ligation, and other ink details remain visual diagnostics.
"""

from _width_probe import run_probe

SECTIONS = [
    (
        "1. Combining marks and conjuncts",
        [
            ("e + combining acute", "e\u0301", {1}, {1}, ""),
            (
                "Devanagari ksha",
                "\u0915\u094d\u0937",
                {2},
                {1, 2},
                "legacy: 1+0+1. Cluster width for Indic conjuncts is not yet "
                "settled by the 2027 spec ecosystem; 1 or 2 accepted, reported.",
            ),
            (
                "Vietnamese depth-2 (e+circumflex+acute)",
                "e\u0302\u0301",
                {1},
                {1},
                "everyday text needing mark-to-mark: acute must sit ON the "
                "circumflex, not on the e. Smudge here breaks Vietnamese.",
            ),
        ],
    ),
    (
        "2. Mark-placement lab (width is regime-neutral; ink differs)",
        [
            (
                "Tower: six marks above",
                "a\u0300\u0301\u0302\u0303\u0304\u0305",
                {1},
                {1},
                "shaping+capable font: marks climb; overstrike: one smudge.",
            ),
            (
                "Cellar: five marks below",
                "a\u0316\u0317\u0318\u0319\u031c",
                {1},
                {1},
                "same test downward; fonts with above-only anchors fail here.",
            ),
            (
                "Both directions at once",
                "o\u0300\u0301\u0302\u0316\u0317\u0318",
                {1},
                {1},
                "three up, three down; the base should stay on the baseline.",
            ),
            (
                "Enclosing circle (a + U+20DD)",
                "a\u20dd",
                {1},
                {1},
                "the mark surrounds the base; renderers that only offset "
                "upward draw a circle floating beside/above the a.",
            ),
            (
                "Marks on a WIDE base (CJK + acute)",
                "\u65e5\u0301",
                {2},
                {2},
                "2 cells both regimes: the mark must center over a "
                "double-width glyph - tests mark placement x, not just y.",
            ),
            (
                "Double diacritic SPANNING two bases (t+U+0361+s)",
                "t\u0361s",
                {2},
                {2},
                "one mark bridging two cells: U+0361 ties t and s. Legacy "
                "and cluster both 2 (two clusters); the tie must arc across "
                "the cell boundary - the hardest ink test in this file.",
            ),
            (
                "Zalgo word (three bases, mixed marks)",
                "h\u0335\u0328e\u0300\u0316\u0301\u0317l\u0334\u0303",
                {3},
                {3},
                "width 3 regardless of chaos: strikethrough overlay U+0335, "
                "ogonek-ish U+0328, overlay U+0334 - three mark CLASSES "
                "(above, below, overlay) on adjacent cells. Ink may legally "
                "escape the line box; cells must not.",
            ),
        ],
    ),
]


if __name__ == "__main__":
    run_probe(
        "Two-regime text shaping and font probe",
        "Measure text-grapheme widths while displaying font-shaping diagnostics.",
        SECTIONS,
    )
