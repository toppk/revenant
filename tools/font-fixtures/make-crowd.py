#!/usr/bin/env python3
"""Generate filler faces that crowd the fallback candidate inventory.

Discovery stores a bounded prefix of the candidate sort.  Proving that a needed
face is still reachable past that bound needs candidates that sit ahead of it and
survive coverage trimming, which identical copies of one font do not: trimming
drops them as redundant.

Each generated face therefore carries the primary family name, so family scoring
puts it near the top of the sort, and one private-use codepoint nobody else maps,
so trimming keeps it.  None of them covers any emoji, which is what makes them
the right adversary: a presentation-aware pass that took a blind prefix would
fill up with these instead of the face the atom needs.
"""

import sys
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

FAMILY = "DejaVu Sans Mono"
FIRST_CODEPOINT = 0xE000


def box_glyph():
    """A filled box, so a filler face that is drawn by mistake is obvious."""
    pen = TTGlyphPen(None)
    pen.moveTo((100, 0))
    pen.lineTo((100, 700))
    pen.lineTo((600, 700))
    pen.lineTo((600, 0))
    pen.closePath()
    return pen.glyph()


def build(path: Path, index: int) -> None:
    glyph_order = [".notdef", "filler"]
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap({FIRST_CODEPOINT + index: "filler"})
    builder.setupGlyf({".notdef": TTGlyphPen(None).glyph(), "filler": box_glyph()})
    builder.setupHorizontalMetrics({name: (700, 100) for name in glyph_order})
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable(
        {
            "familyName": FAMILY,
            "styleName": f"XTP Filler {index:02d}",
            "uniqueFontIdentifier": f"XtpCrowdFiller-{index:02d}",
            "fullName": f"{FAMILY} XTP Filler {index:02d}",
            "psName": f"XtpCrowdFiller-{index:02d}",
            "version": "Version 1.000",
        }
    )
    builder.setupOS2(
        sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200
    )
    builder.setupPost()
    builder.setupMaxp()
    # Keep the generated binaries stable across runs, as make-sbix.py does.
    builder.setupHead(created=3577737600, modified=3577737600)
    builder.font.recalcTimestamp = False
    builder.save(path)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: make-crowd.py OUTPUT-DIRECTORY COUNT", file=sys.stderr)
        return 2
    directory = Path(sys.argv[1])
    count = int(sys.argv[2])
    directory.mkdir(parents=True, exist_ok=True)
    for index in range(count):
        build(directory / f"XtpCrowdFiller-{index:02d}.ttf", index)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
