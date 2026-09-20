#!/usr/bin/env python3
"""Generate one emoji family with real regular, bold and italic faces.

Styled fallback selection needs a family whose bold and italic members are real
faces rather than synthetic slants: the renderer refuses a candidate carrying
FC_EMBOLDEN, a non-identity FC_MATRIX, a weight below demibold for bold, or a
roman slant for italic.  No staged third-party family covers a text-default emoji
base in three real styles, so this generates one.

The metrics are the point of the fixture, not the artwork:

- Regular and Bold map U+1F6E0 to a glyph two ems wide.  Two ems is far wider
  than one cell, so the fallback advance rule refuses the face and the span
  fitting policy has to shrink it.  Each style draws a different number of bars,
  so which face served is visible on screen as well as in the route log.
- Italic maps U+1F6E0 to a half-em glyph and decomposes it into two half-em
  glyphs through a GSUB multiple substitution in `liga`, which HarfBuzz applies
  by default.  The run therefore advances one em, wider than the rule allows,
  while the face's own maximum advance is only half an em and already fits a
  cell.  Fitting cannot help a face that is not individually too wide, so this
  candidate is declined and the normal face has to be retained.  That is the
  branch no staged family could reach.

U+FE0E maps to an empty zero-advance glyph in every face, so a VS15 atom does not
change the measured advance.
"""

import sys
from pathlib import Path

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

UPEM = 1000
FAMILY = "XTP Styled Emoji"
TOOLS = 0x1F6E0
VS15 = 0xFE0E
# Deterministic timestamp, as the other generated fixtures use.
EPOCH = 3577737600


def bars(count, width):
    """A glyph of COUNT vertical bars, so each style is distinguishable."""
    pen = TTGlyphPen(None)
    step = width // (count * 2)
    for index in range(count):
        left = step + index * 2 * step
        pen.moveTo((left, 0))
        pen.lineTo((left, 700))
        pen.lineTo((left + step, 700))
        pen.lineTo((left + step, 0))
        pen.closePath()
    return pen.glyph()


def build(path, style, weight, italic, advance, decompose):
    glyph_order = [".notdef", "tools", "selector"]
    glyphs = {
        ".notdef": TTGlyphPen(None).glyph(),
        "tools": bars(2 if weight < 700 else 3, advance),
        "selector": TTGlyphPen(None).glyph(),
    }
    metrics = {".notdef": (advance, 0), "tools": (advance, 0), "selector": (0, 0)}
    if decompose:
        glyph_order += ["toolsLeft", "toolsRight"]
        glyphs["toolsLeft"] = bars(1, advance)
        glyphs["toolsRight"] = bars(1, advance)
        metrics["toolsLeft"] = (advance, 0)
        metrics["toolsRight"] = (advance, 0)

    builder = FontBuilder(UPEM, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap({TOOLS: "tools", VS15: "selector"})
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable(
        {
            "familyName": FAMILY,
            "styleName": style,
            "uniqueFontIdentifier": f"XtpStyledEmoji-{style}",
            "fullName": f"{FAMILY} {style}",
            "psName": f"XtpStyledEmoji-{style}",
            "version": "Version 1.000",
        }
    )
    builder.setupOS2(
        sTypoAscender=800,
        sTypoDescender=-200,
        usWinAscent=800,
        usWinDescent=200,
        usWeightClass=weight,
        # Bit 0 is ITALIC and bit 5 is BOLD; Fontconfig reads these for slant and
        # weight, which is what makes the faces real rather than synthesised.
        fsSelection=(
            1 if italic else (0x20 if weight >= 700 else 0x40)
        ),
    )
    builder.setupPost(italicAngle=-12.0 if italic else 0.0)
    builder.setupMaxp()
    builder.setupHead(created=EPOCH, modified=EPOCH)
    builder.font["head"].macStyle = (1 if weight >= 700 else 0) | (2 if italic else 0)
    builder.font.recalcTimestamp = False
    if decompose:
        # `liga` is applied by default, so the base decomposes without the caller
        # requesting a feature.
        addOpenTypeFeaturesFromString(
            builder.font,
            "feature liga { sub tools by toolsLeft toolsRight; } liga;",
        )
    builder.save(path)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: make-styled-emoji.py OUTPUT-DIRECTORY", file=sys.stderr)
        return 2
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    build(directory / "XtpStyledEmoji-Regular.ttf", "Regular", 400, False, 2 * UPEM, False)
    build(directory / "XtpStyledEmoji-Bold.ttf", "Bold", 700, False, 2 * UPEM, False)
    build(directory / "XtpStyledEmoji-Italic.ttf", "Italic", 400, True, UPEM // 2, True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
