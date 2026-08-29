#!/usr/bin/env python3
"""Generate Revenant's tiny, deterministic sbix fixture font."""

import binascii
import struct
import sys
import zlib
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables.sbixGlyph import Glyph
from fontTools.ttLib.tables.sbixStrike import Strike


def png_chunk(kind: bytes, data: bytes) -> bytes:
    payload = kind + data
    return (
        struct.pack(">I", len(data))
        + payload
        + struct.pack(">I", binascii.crc32(payload) & 0xFFFFFFFF)
    )


def make_png() -> bytes:
    """Return a 16x16 RGBA checker that is visually unmistakable."""
    rows = []
    colors = ((0xF4, 0x43, 0x36, 0xFF), (0xFF, 0xD5, 0x4F, 0xFF))
    for y in range(16):
        row = bytearray([0])
        for x in range(16):
            row.extend(colors[(x // 4 + y // 4) % 2])
        rows.append(bytes(row))
    header = struct.pack(">IIBBBBB", 16, 16, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
        + png_chunk(b"IEND", b"")
    )


def empty_glyph():
    pen = TTGlyphPen(None)
    return pen.glyph()


def build(path: Path) -> None:
    glyph_order = [".notdef", "syntheticEmoji"]
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap({0x1F600: "syntheticEmoji"})
    builder.setupGlyf({name: empty_glyph() for name in glyph_order})
    builder.setupHorizontalMetrics({name: (1000, 0) for name in glyph_order})
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable(
        {
            "familyName": "Revenant Synthetic sbix",
            "styleName": "Regular",
            "uniqueFontIdentifier": "RevenantSyntheticSbix-Regular-1",
            "fullName": "Revenant Synthetic sbix Regular",
            "psName": "RevenantSyntheticSbix-Regular",
            "version": "Version 1.000",
        }
    )
    builder.setupOS2(
        sTypoAscender=800,
        sTypoDescender=-200,
        usWinAscent=800,
        usWinDescent=200,
    )
    builder.setupPost()
    builder.setupMaxp()
    # Keep the generated binary stable across runs. OpenType timestamps count
    # seconds from 1904; this is 2017-05-17 00:00:00 UTC.
    builder.setupHead(created=3577737600, modified=3577737600)
    builder.font.recalcTimestamp = False

    sbix = newTable("sbix")
    strike = Strike(ppem=16, resolution=72)
    strike.glyphs = {
        "syntheticEmoji": Glyph(
            glyphName="syntheticEmoji",
            originOffsetX=0,
            originOffsetY=0,
            graphicType="png ",
            imageData=make_png(),
        )
    }
    sbix.strikes = {strike.ppem: strike}
    builder.font["sbix"] = sbix
    builder.save(path)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: make-sbix.py OUTPUT.ttf", file=sys.stderr)
        return 2
    build(Path(sys.argv[1]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
