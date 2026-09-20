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

The same family also carries one ZWJ sequence, U+1F468 ZWJ U+1F4BB, for styled
whole-sequence selection.  All three faces map both components and the joiner, so a
declined candidate cannot be explained by coverage; only Regular carries the
`liga` rule that joins them into a single glyph.  A bold or italic request therefore
has a real, covering candidate that still cannot shape the complete atom, which is
the branch a merely-unstyled fallback would satisfy by accident.

`XtpPartialSequence-Regular.ttf` is a separate family with the same components and no
ligature, to stand in front of that one as a preferred role face that covers every
component and cannot shape the sequence.
"""

import sys
from pathlib import Path

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

UPEM = 1000
FAMILY = "XTP Styled Emoji"
PARTIAL_FAMILY = "XTP Partial Sequence"
TOOLS = 0x1F6E0
VS15 = 0xFE0E
MAN = 0x1F468
LAPTOP = 0x1F4BB
BULB = 0x1F4A1
ZWJ = 0x200D
# Half an em, the same as Italic's own U+1F6E0 glyph: the sequence glyphs must not
# raise any face's maximum advance, because that is an input to the fitting decision
# the earlier styled cases grade.  It also stays well inside the two cells the
# sequence is committed to, so the advance rule is not what decides these cases.
SEQUENCE_ADVANCE = UPEM // 2
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


def build(path, style, weight, italic, advance, decompose, join=False, family=FAMILY):
    """One face.  FAMILY other than the styled one carries the sequence components
    only, so it cannot compete for the single-atom cases."""
    sequence_only = family != FAMILY
    glyph_order = [".notdef", "man", "zwj", "laptop"]
    if not sequence_only:
        glyph_order = [".notdef", "tools", "selector", "man", "zwj", "laptop"]
    glyphs = {
        ".notdef": TTGlyphPen(None).glyph(),
        "man": bars(1, SEQUENCE_ADVANCE),
        "zwj": TTGlyphPen(None).glyph(),
        "laptop": bars(2, SEQUENCE_ADVANCE),
    }
    metrics = {
        ".notdef": (advance, 0),
        "man": (SEQUENCE_ADVANCE, 0),
        "zwj": (0, 0),
        "laptop": (SEQUENCE_ADVANCE, 0),
    }
    character_map = {MAN: "man", ZWJ: "zwj", LAPTOP: "laptop"}
    if not sequence_only:
        # Only the styled family carries this one, so an atom using it is served on a
        # fallback rung rather than by the preferred role face.
        glyph_order += ["bulb"]
        glyphs["bulb"] = bars(2, SEQUENCE_ADVANCE)
        metrics["bulb"] = (SEQUENCE_ADVANCE, 0)
        character_map[BULB] = "bulb"
        glyphs["tools"] = bars(2 if weight < 700 else 3, advance)
        glyphs["selector"] = TTGlyphPen(None).glyph()
        metrics["tools"] = (advance, 0)
        metrics["selector"] = (0, 0)
        character_map.update({TOOLS: "tools", VS15: "selector"})
    features = []
    if decompose:
        glyph_order += ["toolsLeft", "toolsRight"]
        glyphs["toolsLeft"] = bars(1, advance)
        glyphs["toolsRight"] = bars(1, advance)
        metrics["toolsLeft"] = (advance, 0)
        metrics["toolsRight"] = (advance, 0)
        # `liga` is applied by default, so the base decomposes without the caller
        # requesting a feature.
        features.append("sub tools by toolsLeft toolsRight;")
    if join:
        glyph_order += ["joined"]
        # Three bars, so a joined atom is distinguishable on screen from the two
        # components drawn separately.
        glyphs["joined"] = bars(3, SEQUENCE_ADVANCE)
        metrics["joined"] = (SEQUENCE_ADVANCE, 0)
        features.append("sub man zwj laptop by joined;")

    builder = FontBuilder(UPEM, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap(character_map)
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable(
        {
            "familyName": family,
            "styleName": style,
            "uniqueFontIdentifier": f"{path.stem}",
            "fullName": f"{family} {style}",
            "psName": f"{path.stem}",
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
    if features:
        addOpenTypeFeaturesFromString(
            builder.font, "feature liga { %s } liga;" % " ".join(features)
        )
    builder.save(path)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: make-styled-emoji.py OUTPUT-DIRECTORY", file=sys.stderr)
        return 2
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    build(
        directory / "XtpStyledEmoji-Regular.ttf",
        "Regular",
        400,
        False,
        2 * UPEM,
        False,
        join=True,
    )
    build(directory / "XtpStyledEmoji-Bold.ttf", "Bold", 700, False, 2 * UPEM, False)
    build(directory / "XtpStyledEmoji-Italic.ttf", "Italic", 400, True, UPEM // 2, True)
    build(
        directory / "XtpPartialSequence-Regular.ttf",
        "Regular",
        400,
        False,
        SEQUENCE_ADVANCE,
        False,
        family=PARTIAL_FAMILY,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
