#!/usr/bin/env python3
"""Classify font fixtures and verify their maintained manifest."""

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

from fontTools.pens.boundsPen import BoundsPen
from fontTools.ttLib import TTFont

PROBES = {
    0x1F600: "grinning_face",
    0x2764: "heart_text_default",
    0x1FAE8: "shaking_face_emoji_15",
    0x1F1FA: "regional_indicator_u",
    0x65E5: "cjk_sentinel",
    0x2139: "information_text_default",
}

INK_KINDS = {"outline", "bitmap", "color", "svg"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def family_name(font: TTFont) -> str:
    for record in font["name"].names:
        if record.nameID == 1:
            try:
                return record.toUnicode()
            except UnicodeDecodeError:
                pass
    return "?"


def outline_kind(font: TTFont) -> str:
    if "glyf" in font:
        return "glyf"
    if "CFF " in font:
        return "CFF"
    if "CFF2" in font:
        return "CFF2"
    return "none"


def outline_ink(font: TTFont, glyph_name: str | None) -> bool:
    """Return whether the mapped outline glyph draws nonempty contours."""
    if glyph_name is None or outline_kind(font) == "none":
        return False
    glyphs = font.getGlyphSet()
    pen = BoundsPen(glyphs)
    try:
        glyphs[glyph_name].draw(pen)
    except (KeyError, TypeError):
        return False
    return pen.bounds is not None


def cbdt_glyphs(font: TTFont) -> set[str]:
    if "CBDT" not in font:
        return set()
    return {name for strike in font["CBDT"].strikeData for name in strike}


def sbix_glyphs(font: TTFont) -> set[str]:
    if "sbix" not in font:
        return set()
    result = set()
    for strike in font["sbix"].strikes.values():
        result.update(name for name, glyph in strike.glyphs.items() if glyph.imageData)
    return result


def colr_glyphs(font: TTFont) -> set[str]:
    if "COLR" not in font:
        return set()
    table = font["COLR"]
    if table.version == 0:
        return set(table.ColorLayers)
    result = set()
    base_list = getattr(table.table, "BaseGlyphList", None)
    if base_list is not None:
        result.update(record.BaseGlyph for record in base_list.BaseGlyphPaintRecord)
    base_array = getattr(table.table, "BaseGlyphRecordArray", None)
    if base_array is not None:
        result.update(record.BaseGlyph for record in base_array.BaseGlyphRecord)
    return result


def svg_glyph_ids(font: TTFont) -> set[int]:
    if "SVG " not in font:
        return set()
    result = set()
    for document in font["SVG "].docList:
        result.update(range(document.startGlyphID, document.endGlyphID + 1))
    return result


def technologies(font: TTFont) -> list[str]:
    result = []
    if "CBDT" in font and "CBLC" in font:
        result.append("CBDT")
    if "COLR" in font:
        result.append(f"COLRv{font['COLR'].version}")
    if "SVG " in font:
        result.append("SVGinOT")
    if "sbix" in font:
        result.append("sbix")
    return result or ["monochrome"]


def strike_sizes(font: TTFont) -> dict[str, list[int]]:
    result = {}
    if "CBLC" in font:
        result["CBDT"] = sorted(
            strike.bitmapSizeTable.ppemX for strike in font["CBLC"].strikes
        )
    if "sbix" in font:
        result["sbix"] = sorted(font["sbix"].strikes)
    return result


def axes(font: TTFont) -> list[dict[str, float | str]]:
    if "fvar" not in font:
        return []
    return [
        {
            "tag": axis.axisTag,
            "min": axis.minValue,
            "default": axis.defaultValue,
            "max": axis.maxValue,
        }
        for axis in font["fvar"].axes
    ]


def inspect_font(path: Path) -> dict:
    font = TTFont(path, lazy=False, fontNumber=0)
    cmap = font.getBestCmap() or {}
    glyph_order = font.getGlyphOrder()
    glyph_ids = {name: index for index, name in enumerate(glyph_order)}
    cbdt = cbdt_glyphs(font)
    sbix = sbix_glyphs(font)
    colr = colr_glyphs(font)
    svg = svg_glyph_ids(font)
    probes = {}
    for codepoint, label in PROBES.items():
        glyph_name = cmap.get(codepoint)
        glyph_id = glyph_ids.get(glyph_name) if glyph_name is not None else None
        if glyph_name is None:
            probes[label] = "missing"
            continue
        ink = []
        if outline_ink(font, glyph_name):
            ink.append("outline")
        if glyph_name in cbdt or glyph_name in sbix:
            ink.append("bitmap")
        if glyph_name in colr:
            ink.append("color")
        if glyph_id in svg:
            ink.append("svg")
        probes[label] = "+".join(ink) if ink else "covered-no-ink"
    return {
        "sha256": sha256(path),
        "family": family_name(font),
        "technologies": technologies(font),
        "outline": outline_kind(font),
        "strikes": strike_sizes(font),
        "axes": axes(font),
        "glyphs": font["maxp"].numGlyphs,
        "codepoints": len(cmap),
        "probes": probes,
    }


def inspect_directory(directory: Path) -> dict[str, dict]:
    suffixes = {".ttf", ".otf", ".ttc"}
    paths = sorted(
        path for path in directory.iterdir() if path.suffix.lower() in suffixes
    )
    return {path.name: inspect_font(path) for path in paths}


def report(fonts: dict[str, dict]) -> None:
    for filename, info in fonts.items():
        print(f"\n{filename}   family: {info['family']}")
        print(f"  technology: {', '.join(info['technologies'])}")
        print(f"  outline:    {info['outline']}")
        if info["strikes"]:
            values = ", ".join(
                f"{kind}={sizes}" for kind, sizes in info["strikes"].items()
            )
            print(f"  strikes:    {values}")
        if info["axes"]:
            print(f"  axes:       {info['axes']}")
        print(
            f"  size:       {info['glyphs']} glyphs, "
            f"{info['codepoints']} codepoints"
        )
        for label, state in info["probes"].items():
            print(f"  probe:      {label}={state}")


def compare(expected, actual, location: str, errors: list[str]) -> None:
    if isinstance(expected, dict):
        if not isinstance(actual, dict):
            errors.append(f"{location}: expected object, got {actual!r}")
            return
        for key, value in expected.items():
            if key not in actual:
                errors.append(f"{location}.{key}: missing")
            else:
                compare(value, actual[key], f"{location}.{key}", errors)
    elif expected != actual:
        errors.append(f"{location}: expected {expected!r}, got {actual!r}")


def unicode_version(path: Path) -> str | None:
    pattern = re.compile(r"^#\s*Version:\s*([0-9]+(?:\.[0-9]+)*)\s*$")
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            match = pattern.match(line.rstrip("\n"))
            if match is not None:
                return match.group(1)
    return None


def check_probe_paths(fonts: dict[str, dict], errors: list[str]) -> None:
    for filename, info in fonts.items():
        for probe, path in info.get("probes", {}).items():
            if path in {"missing", "covered-no-ink"}:
                continue
            kinds = path.split("+")
            if (
                not kinds
                or len(kinds) != len(set(kinds))
                or not set(kinds) <= INK_KINDS
            ):
                errors.append(f"{filename}.probes.{probe}: invalid ink path {path!r}")


def check_unicode_sources(expected: str, paths: list[Path], errors: list[str]) -> None:
    for path in paths:
        if not path.is_file():
            errors.append(f"unicode data: missing {path}")
            continue
        actual = unicode_version(path)
        if actual is None:
            errors.append(f"unicode data: no '# Version:' header in {path}")
        elif actual != expected:
            errors.append(
                f"unicode data: {path} is version {actual!r}, expected {expected!r}"
            )


def check_manifest(
    manifest_path: Path, directory: Path | None, unicode_data: list[Path]
) -> int:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1:
        print("unsupported fixture manifest schema", file=sys.stderr)
        return 1
    if directory is None:
        directory = manifest_path.parent / manifest.get("font_directory", "fonts")
    actual = inspect_directory(directory)
    expected = manifest["fonts"]
    errors = []
    check_probe_paths(expected, errors)
    expected_unicode = manifest.get("unicode_version")
    if not isinstance(expected_unicode, str):
        errors.append("unicode_version: expected string")
    else:
        if not unicode_data and "unicode_data" in manifest:
            unicode_data = [manifest_path.parent / manifest["unicode_data"]]
        check_unicode_sources(expected_unicode, unicode_data, errors)
    if set(actual) != set(expected):
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        if missing:
            errors.append(f"fonts: missing {missing}")
        if extra:
            errors.append(f"fonts: unexpected {extra}")
    for filename in sorted(set(actual) & set(expected)):
        compare(expected[filename], actual[filename], filename, errors)
    if errors:
        print("font fixture manifest check failed:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        return 1
    print(f"font fixture manifest matches {len(actual)} fonts")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--dir", type=Path)
    parser.add_argument("--check", type=Path, metavar="MANIFEST")
    parser.add_argument(
        "--unicode-data",
        action="append",
        default=[],
        type=Path,
        metavar="FILE",
        help="also require a Unicode data file's Version header to match the manifest",
    )
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    if args.check is not None:
        if args.paths:
            parser.error("font paths cannot be combined with --check")
        return check_manifest(args.check, args.dir, args.unicode_data)
    if args.unicode_data:
        parser.error("--unicode-data requires --check")
    if args.dir is not None:
        if args.paths:
            parser.error("font paths cannot be combined with --dir")
        fonts = inspect_directory(args.dir)
    elif args.paths:
        fonts = {path.name: inspect_font(path) for path in args.paths}
    else:
        parser.error("provide font paths, --dir, or --check")
    if args.json:
        json.dump(fonts, sys.stdout, indent=2, sort_keys=True)
        print()
    else:
        report(fonts)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
