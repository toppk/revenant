#!/usr/bin/env python3
"""Inventory emoji coverage across the pinned Unicode data, the staged fonts and
the test corpus.

Three questions are kept apart, because conflating them is how a font limitation
gets filed as a renderer defect:

- which Unicode cases an automated suite actually asserts something about;
- which cases at least one staged font supports but no suite asserts;
- which cases no staged font supports at all.

Coverage is read from `AUTOMATED_CASES` below: an explicit inventory naming the
suite, the case, the atom and what that case asserts. Scanning the suites' text
cannot answer this -- a codepoint can appear in a comment, in an unused variable,
or be assembled from shell variables the scanner never sees -- so the scan is kept
only as a separate, clearly labelled **source mentions** column, which is evidence
that a codepoint is written down somewhere and nothing more.

The inventory's scope is explicit: every sequence in its table and every base newer
than E15.0. Bases outside that scope are reported as **not inventoried**, never as
unasserted -- the suites assert a great deal about older bases, starting with the bare
U+1F6E0 this gate exists for; this table just does not enumerate them, so
asserted/unasserted totals are given for the audited scope alone.

`--check` is **case-reference validation**: every row must point at a case its suite
still declares, matched against the suite's own invocation forms and ignoring comments.
It cannot show that a case still makes the assertions the row claims; only reading the
case can.

Scalars and sequences are also kept apart. A cmap entry proves only that a base
has a glyph; whether a complete sequence can be drawn is a shaping question, so
sequences are shaped with `hb-shape` using **production's own buffer flags**:
`PRESERVE_DEFAULT_IGNORABLES` for a composition-requiring atom and
`REMOVE_DEFAULT_IGNORABLES` otherwise, matching `XtpShapeUtf8ForComposition` and
`XtpShapeUtf8` in src/glyph_shape.c. That distinction decides results: with
ignorables removed, an unsupported tag payload disappears and a face that cannot
draw the flag still reports one plausible glyph. Shaping to one glyph is also not
the whole of production's acceptance -- coverage, ink, presentation and the advance
rule are separate gates the audit does not model -- so the column is named
`shaping` rather than `renders`. Without `hb-shape` on PATH the rows report
`unmeasured` rather than guessing.

Every input is pinned: the emoji properties and their `E<version>` ages come from
the staged `data/emoji-data.txt`, the fonts from the staged tree. Nothing is
downloaded.
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

from fontTools.ttLib import TTFont

VS15 = 0xFE0E
VS16 = 0xFE0F
ZWJ = 0x200D
KEYCAP = 0x20E3
TAG_START = 0xE0020
TAG_END = 0xE007F
TAG_BASE = 0xE0000
BLACK_FLAG = 0x1F3F4

# Files scanned for the source-mentions column only. The probe scripts and the Go
# probe carry human-assessed samples, which are never automated evidence.
SOURCE_GLOBS = ("tests/xvfb-emoji-*.sh", "tests/xvfb-font-*.sh", "tests/xvfb-text-shaping.sh")
PROBE_GLOBS = ("tools/probe-emoji.py", "tools/_width_probe.py", "tools/probe/emoji_artwork.go")


def tag_sequence(code: str) -> tuple[int, ...]:
    return (BLACK_FLAG, *(TAG_BASE + ord(character) for character in code), TAG_END)


# Sequences whose shaping this audit measures. The RGI subdivision flags (UTS #51)
# are the only tag sequences in that set; `usca` is syntactically valid and
# deliberately outside it, which separates a missing ligature from a renderer
# defect.
SEQUENCES: dict[str, tuple[int, ...]] = {
    "keycap-1": (0x31, VS16, KEYCAP),
    "modifier-wave": (0x1F44B, 0x1F3FD),
    "zwj-technologist": (0x1F469, ZWJ, 0x1F4BB),
    "zwj-family-mwg": (0x1F468, ZWJ, 0x1F469, ZWJ, 0x1F467),
    "zwj-heart-on-fire": (0x2764, VS16, ZWJ, 0x1F525),
    "zwj-polar-bear": (0x1F43B, ZWJ, 0x2744, VS16),
    "flag-us": (0x1F1FA, 0x1F1F8),
    "vs16-information": (0x2139, VS16),
    "vs15-hammer": (0x1F6E0, VS15),
    "tag-flag-gbeng": tag_sequence("gbeng"),
    "tag-flag-gbsct": tag_sequence("gbsct"),
    "tag-flag-gbwls": tag_sequence("gbwls"),
    "tag-flag-usca": tag_sequence("usca"),
}
NON_RGI = frozenset({"tag-flag-usca"})

# What an automated case asserts about one atom.  These name the test's own
# assertions, read off the helper each case calls -- not what the renderer happens to
# do.  In particular `check_route` in the artwork suite requires that the role is not
# tofu and never inspects `glyphs=`, so those cases claim `non-tofu-role` and not
# `role` or `shaping`.
#
#   role               a specific serving role is required by name
#   non-tofu-role      only that the role is not tofu
#   presentation       the resolved presentation is required
#   effective-file     the serving font file, not a family preference
#   shaping            the route was required to report glyphs=1
#   ink                visible ink of the expected paint class
#   non-tofu           ink measured against the deterministic tofu box
#   distinct-artwork   two atoms of the row must not paint identical pixels
#   containment-right  the cell after the atom's span is blank
#   containment-below  the row below the span is blank
#   advance            an independently measured cursor position (CPR)
#   tofu-expected      a tofu route is required
#   tofu-ink           the pixels must equal the measured tofu box
ASSERTIONS = frozenset(
    {
        "role",
        "non-tofu-role",
        "presentation",
        "effective-file",
        "shaping",
        "ink",
        "non-tofu",
        "distinct-artwork",
        "containment-right",
        "containment-below",
        "advance",
        "tofu-expected",
        "tofu-ink",
    }
)

ROUTING = "tests/xvfb-emoji-routing.sh"
ARTWORK = "tests/xvfb-emoji-artwork.sh"

# How each suite declares a case, so a reference can be validated against the
# invocation rather than against any occurrence of the name.  `{name}` is filled in
# per case; comment lines are ignored.
DECLARATIONS: dict[str, tuple[str, ...]] = {
    ROUTING: (r"run_(?:unicode_)?case\s+\S+\s+{name}(?:\s|$)",),
    ARTWORK: (r"start_sample\s+{name}(?:\s|$)", r"negative_case\s+{name}(?:\s|$)"),
}

# tests/xvfb-emoji-routing.sh: run_case greps one literal route string, so whether a
# case asserts a role, a presentation or glyphs=1 depends on what that string spells
# out.  It also asserts the pixel class, the CPR column and a blank following cell,
# and -- only for the `seq-*` cases -- a blank band below the span.
ROUTE_BASE = ("role", "presentation", "ink", "containment-right", "advance")
ROUTE_SHAPED = ROUTE_BASE + ("shaping",)
# tests/xvfb-emoji-artwork.sh: check_route requires a non-tofu role, the presentation
# and the effective file; check_cell_artwork requires ink that is not the tofu box;
# the blanks and check_vertical_containment give containment; check_advance is CPR.
ARTWORK_BASE = (
    "non-tofu-role",
    "presentation",
    "effective-file",
    "ink",
    "non-tofu",
    "containment-right",
    "containment-below",
    "advance",
)
# check_every_route additionally requires glyphs=1 on every logged route for the base.
ARTWORK_SHAPED = ARTWORK_BASE + ("shaping",)

# The explicit coverage inventory, and the audited scope: every sequence in SEQUENCES
# and every base newer than E15.0.  Anything outside that scope is reported as *not
# inventoried* rather than as unasserted -- the suites asserted plenty about older
# bases (bare U+1F6E0 is the gate's own subject); this table simply does not enumerate
# them.
AUTOMATED_CASES: tuple[tuple[str, str, tuple[int, ...], tuple[str, ...]], ...] = (
    (ROUTING, "sequence-keycap", SEQUENCES["keycap-1"], ROUTE_SHAPED),
    (ROUTING, "sequence-tone", SEQUENCES["modifier-wave"], ROUTE_SHAPED),
    (ROUTING, "sequence-zwj", SEQUENCES["zwj-technologist"], ROUTE_SHAPED),
    (ROUTING, "sequence-family", SEQUENCES["zwj-family-mwg"], ROUTE_SHAPED),
    (ROUTING, "sequence-flag", SEQUENCES["flag-us"], ROUTE_SHAPED),
    (ROUTING, "sequence-tag-flag", SEQUENCES["tag-flag-gbsct"], ROUTE_SHAPED),
    (ROUTING, "sequence-tag-atomic-fallback", SEQUENCES["tag-flag-gbsct"], ROUTE_SHAPED),
    (ROUTING, "sequence-atomic-fallback", SEQUENCES["zwj-heart-on-fire"], ROUTE_SHAPED),
    # No glyphs= in this case's route string, so it asserts no shaping.
    (ROUTING, "info-vs16-unicode", SEQUENCES["vs16-information"], ROUTE_BASE),
    (ARTWORK, "tools-vs15", SEQUENCES["vs15-hammer"], ARTWORK_BASE),
    (ARTWORK, "tag-flags-row", SEQUENCES["tag-flag-gbeng"], ARTWORK_SHAPED + ("distinct-artwork",)),
    (ARTWORK, "tag-flags-row", SEQUENCES["tag-flag-gbwls"], ARTWORK_SHAPED + ("distinct-artwork",)),
    (ARTWORK, "tag-flags-row", SEQUENCES["tag-flag-usca"], ARTWORK_SHAPED),
    (ARTWORK, "tag-flags-legacy", SEQUENCES["tag-flag-gbeng"], ARTWORK_SHAPED),
    (ARTWORK, "tag-flags-legacy", SEQUENCES["tag-flag-gbwls"], ARTWORK_SHAPED),
    (ARTWORK, "tag-flags-legacy", SEQUENCES["tag-flag-usca"], ARTWORK_SHAPED),
    # Bases newer than E15.0.
    (ROUTING, "emoji-fallthrough", (0x1FAE8,), ROUTE_BASE),
    (ROUTING, "legacy-cbdt-fallthrough", (0x1FAE8,), ROUTE_BASE),
    (ARTWORK, "newer-bases-row", (0x1FA75,), ARTWORK_BASE),
    (ARTWORK, "newer-bases-row", (0x1FADF,), ARTWORK_BASE),
    (ARTWORK, "newer-bases-row", (0x1FA8A,), ARTWORK_BASE),
    (ARTWORK, "newer-bases-row", (0x1FACD,), ARTWORK_BASE),
    (ARTWORK, "newer-bases-text", (0x1FA75,), ARTWORK_BASE),
    (ARTWORK, "newer-bases-text", (0x1FADF,), ARTWORK_BASE),
    # The gap cases assert a tofu route, tofu pixels, the band below and the advance;
    # they do not assert a presentation.
    (
        ARTWORK,
        "newest-bases-text-gap",
        (0x1FA8A,),
        ("tofu-expected", "tofu-ink", "containment-below", "advance"),
    ),
    (
        ARTWORK,
        "newest-bases-text-gap",
        (0x1FACD,),
        ("tofu-expected", "tofu-ink", "containment-below", "advance"),
    ),
)


def emoji_modifier(codepoint: int) -> bool:
    return 0x1F3FB <= codepoint <= 0x1F3FF


def regional_indicator(codepoint: int) -> bool:
    return 0x1F1E6 <= codepoint <= 0x1F1FF


def keycap_base(codepoint: int) -> bool:
    return codepoint in {0x23, 0x2A} or 0x30 <= codepoint <= 0x39


def requires_composition(codepoints: tuple[int, ...]) -> bool:
    """Whether production shapes this atom with preserved default ignorables.

    A port of XtpEmojiResolveClusterStyle in src/emoji_presentation.c, which stays
    the authority; this exists so the audit shapes with the same buffer flags the
    renderer would use for the same atom.
    """
    trailing_zwj = False
    saw_tag = False
    terminated_tag = False
    keycap = False
    indicators = 0
    for index, codepoint in enumerate(codepoints):
        if codepoint == ZWJ:
            trailing_zwj = True
        elif trailing_zwj and codepoint not in {VS15, VS16}:
            return True
        if codepoint == KEYCAP:
            keycap = True
        if index != 0 and emoji_modifier(codepoint):
            return True
        if regional_indicator(codepoint):
            indicators += 1
        if TAG_START <= codepoint <= 0xE007E:
            saw_tag = True
        if codepoint == TAG_END:
            terminated_tag = True
    if keycap and keycap_base(codepoints[0]):
        return True
    if indicators >= 2:
        return True
    return codepoints[0] == BLACK_FLAG and saw_tag and terminated_tag


def emoji_properties(path: Path) -> tuple[dict[str, set[int]], dict[int, str], str | None]:
    """Binary properties, the E-version age per codepoint, and the file's version."""
    properties: dict[str, set[int]] = {}
    ages: dict[int, str] = {}
    version = None
    age_pattern = re.compile(r"#\s*E(\d+\.\d+)")
    for line in path.read_text(encoding="utf-8").splitlines():
        if version is None:
            header = re.match(r"^#\s*Version:\s*([0-9.]+)\s*$", line)
            if header is not None:
                version = header.group(1)
        payload, _, comment = line.partition("#")
        fields = [field.strip() for field in payload.split(";")]
        if len(fields) < 2 or not fields[0]:
            continue
        first, _, last = fields[0].partition("..")
        span = range(int(first, 16), int(last or first, 16) + 1)
        properties.setdefault(fields[1], set()).update(span)
        age = age_pattern.search("#" + comment)
        if age is not None:
            for codepoint in span:
                ages.setdefault(codepoint, age.group(1))
    return properties, ages, version


def relevant(codepoint: int, emoji: set[int], components: set[int]) -> bool:
    return (
        codepoint in emoji
        or codepoint in components
        or codepoint in {VS15, VS16, ZWJ, KEYCAP}
        or TAG_BASE <= codepoint <= TAG_END
    )


def atoms_in_text(text: str, emoji: set[int], components: set[int]) -> set[tuple[int, ...]]:
    """Maximal runs of emoji-relevant codepoints, after unescaping \\U/\\u forms.

    This is the source-mentions scan. It cannot tell a comment from a probe string
    and cannot follow shell concatenation, so its result is never coverage.
    """
    expanded = re.sub(
        r"\\U([0-9a-fA-F]{8})|\\u([0-9a-fA-F]{4})",
        lambda match: chr(int(match.group(1) or match.group(2), 16)),
        text,
    )
    atoms: set[tuple[int, ...]] = set()
    run: list[int] = []
    for character in expanded:
        codepoint = ord(character)
        if relevant(codepoint, emoji, components):
            run.append(codepoint)
            continue
        if run:
            atoms.add(tuple(run))
            run = []
    if run:
        atoms.add(tuple(run))
    return atoms


def mentions(root: Path, globs: tuple[str, ...], emoji: set[int], components: set[int]):
    """The atoms and bare emoji scalars written anywhere in one set of files."""
    atoms: set[tuple[int, ...]] = set()
    for pattern in globs:
        for path in sorted(root.glob(pattern)):
            atoms |= atoms_in_text(
                path.read_text(encoding="utf-8", errors="replace"), emoji, components
            )
    scalars = {code for atom in atoms for code in atom}
    return atoms, scalars & emoji


def asserted_atoms() -> dict[tuple[int, ...], dict]:
    """The inventory, keyed by atom, with the union of what the cases assert."""
    result: dict[tuple[int, ...], dict] = {}
    for suite, case, atom, assertions in AUTOMATED_CASES:
        unknown = set(assertions) - ASSERTIONS
        if unknown:
            raise SystemExit(f"{case}: unknown assertion names {sorted(unknown)}")
        entry = result.setdefault(atom, {"cases": [], "assertions": set()})
        entry["cases"].append(f"{Path(suite).name}:{case}")
        entry["assertions"].update(assertions)
    return result


def declarations(body: str, suite: str, case: str) -> int:
    """How many times SUITE declares CASE, by its own invocation forms.

    Comment lines are skipped and the name has to sit where the invocation puts it, so
    a deleted case whose name survives in a comment -- or another case's name that
    merely contains this one -- is not a declaration.
    """
    patterns = [
        re.compile(r"^\s*" + pattern.replace("{name}", re.escape(case)))
        for pattern in DECLARATIONS[suite]
    ]
    count = 0
    for line in body.splitlines():
        if line.lstrip().startswith("#"):
            continue
        if any(pattern.search(line) is not None for pattern in patterns):
            count += 1
    return count


def check_inventory(root: Path) -> list[str]:
    """Case-reference validation.

    Each inventory row must point at a case its suite still declares.  This keeps the
    table from naming cases that no longer exist; it says nothing about whether a case
    still makes the assertions the row claims, which only reading the case can show.
    """
    errors = []
    bodies: dict[str, str] = {}
    for suite, case, _, _ in AUTOMATED_CASES:
        if suite not in bodies:
            path = root / suite
            bodies[suite] = (
                path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""
            )
            if not bodies[suite]:
                errors.append(f"{suite}: missing")
        if not bodies[suite]:
            continue
        if declarations(bodies[suite], suite, case) == 0:
            errors.append(f"{suite}: case {case!r} is not declared")
    return sorted(set(errors))


def font_coverage(directories: list[Path]) -> dict[str, tuple[Path, frozenset[int]]]:
    result = {}
    for directory in directories:
        if not directory.is_dir():
            continue
        for path in sorted(directory.iterdir()):
            if path.suffix.lower() not in {".ttf", ".otf", ".ttc"}:
                continue
            font = TTFont(path, lazy=True, fontNumber=0)
            result[path.name] = (path, frozenset(font.getBestCmap() or {}))
            font.close()
    return result


def shape(path: Path, codepoints: tuple[int, ...]) -> tuple[int, bool] | None:
    """(glyph count, any missing glyph), shaped the way production would.

    Returns None when hb-shape is unavailable.
    """
    if shutil.which("hb-shape") is None:
        return None
    ignorables = (
        "--preserve-default-ignorables"
        if requires_composition(codepoints)
        else "--remove-default-ignorables"
    )
    text = "".join(chr(code) for code in codepoints)
    try:
        output = subprocess.run(
            [
                "hb-shape",
                "--no-glyph-names",
                "--no-clusters",
                "--no-positions",
                "--bot",
                "--eot",
                "--cluster-level=1",
                ignorables,
                str(path),
                text,
            ],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except subprocess.CalledProcessError:
        return None
    glyphs = [entry for entry in output.strip("[]").split("|") if entry]
    return len(glyphs), any(entry == "0" for entry in glyphs)


def shaping_state(path: Path, cmap: frozenset[int], codepoints: tuple[int, ...]) -> str:
    """One font's shaping verdict for one atom, in production's terms."""
    if not all(
        code in cmap or code in {VS15, VS16, ZWJ} or TAG_BASE <= code <= TAG_END
        for code in codepoints
    ):
        return "uncovered"
    measurement = shape(path, codepoints)
    if measurement is None:
        return "unmeasured"
    count, missing = measurement
    if missing:
        return f"missing-glyph:{count}"
    return "complete" if count == 1 else f"split:{count}"


def build(arguments) -> dict:
    data = arguments.fixture_root / "data" / "emoji-data.txt"
    properties, ages, version = emoji_properties(data)
    emoji = properties.get("Emoji", set())
    presentation = properties.get("Emoji_Presentation", set())
    components = properties.get("Emoji_Component", set())
    fonts = font_coverage(
        [arguments.fixture_root / "fonts", arguments.fixture_root / "fonts-styled"]
    )
    inventory = asserted_atoms()
    asserted_scalars = {atom[0] for atom in inventory if len(atom) == 1}
    source_atoms, source_scalars = mentions(arguments.repo_root, SOURCE_GLOBS, emoji, components)
    probe_atoms, probe_scalars = mentions(arguments.repo_root, PROBE_GLOBS, emoji, components)

    def age_of(codepoint: int) -> str:
        return ages.get(codepoint, "0.0")

    def newer(codepoint: int) -> bool:
        return tuple(int(part) for part in age_of(codepoint).split(".")) >= tuple(
            int(part) for part in arguments.age.split(".")
        )

    scalars = [
        {
            "codepoint": f"U+{code:04X}",
            "in_scope": newer(code),
            "age": age_of(code),
            "presentation": "emoji" if code in presentation else "text",
            "fonts": sorted(name for name, (_, cmap) in fonts.items() if code in cmap),
            "asserted": code in asserted_scalars,
            "cases": inventory.get((code,), {}).get("cases", []),
            "assertions": sorted(inventory.get((code,), {}).get("assertions", ())),
            "source_mention": code in source_scalars,
            "probe_sample": code in probe_scalars,
        }
        for code in sorted(emoji)
    ]
    sequences = [
        {
            "name": name,
            "codepoints": [f"U+{code:04X}" for code in codepoints],
            "rgi": name not in NON_RGI,
            "composition": requires_composition(codepoints),
            "shaping": {
                filename: shaping_state(path, cmap, codepoints)
                for filename, (path, cmap) in fonts.items()
            },
            "asserted": codepoints in inventory,
            "cases": inventory.get(codepoints, {}).get("cases", []),
            "assertions": sorted(inventory.get(codepoints, {}).get("assertions", ())),
            "source_mention": codepoints in source_atoms,
            "probe_sample": codepoints in probe_atoms,
        }
        for name, codepoints in SEQUENCES.items()
    ]
    for row in sequences:
        row["complete_in"] = sorted(k for k, v in row["shaping"].items() if v == "complete")
    return {
        "unicode_version": version,
        "fixture_root": str(arguments.fixture_root),
        "fonts": sorted(fonts),
        "inventory_errors": check_inventory(arguments.repo_root),
        "scalars": scalars,
        "sequences": sequences,
        "newer_than": arguments.age,
    }


def report(inventory: dict) -> None:
    age = inventory["newer_than"]

    def newer(row) -> bool:
        return tuple(int(part) for part in row["age"].split(".")) >= tuple(
            int(part) for part in age.split(".")
        )

    print(
        f"emoji-data.txt version {inventory['unicode_version']}; "
        f"{len(inventory['fonts'])} staged faces"
    )
    buckets = {"asserted": [], "font-only": [], "unsupported": []}
    outside = [row for row in inventory["scalars"] if not row["in_scope"]]
    for row in inventory["scalars"]:
        if not row["in_scope"]:
            continue
        key = "asserted" if row["asserted"] else "font-only" if row["fonts"] else "unsupported"
        buckets[key].append(row)
    total = len(inventory["scalars"])
    in_scope = total - len(outside)
    print(
        f"\nscalars: {total} Emoji bases, of which {in_scope} are in the audited scope"
        f" (E{age}+); asserted = named in the explicit inventory"
    )
    for key, rows in buckets.items():
        print(f"  {key:16} {len(rows):5}")
    print(f"  {'not inventoried':16} {len(outside):5}   older bases this table does not enumerate;")
    print(f"  {'':16} {'':5}   many are asserted by the suites, which is not read off here")
    mentioned = sum(1 for row in inventory["scalars"] if row["source_mention"])
    sampled = sum(1 for row in inventory["scalars"] if row["probe_sample"])
    print(f"  {'(mentioned)':16} {mentioned:5}   written in a suite's source, coverage unproven")
    print(f"  {'(probed)':16} {sampled:5}   human-assessed probe samples, never automated evidence")
    print(f"\nnewer bases (E{age}+) no staged face covers")
    for row in buckets["unsupported"]:
        if newer(row):
            print(f"  {row['codepoint']} E{row['age']} {row['presentation']}")
    print(f"\nin-scope bases (E{age}+) a staged face covers but no case asserts")
    for row in buckets["font-only"]:
        if newer(row):
            state = "probe-sample" if row["probe_sample"] else "unasserted"
            print(f"  {row['codepoint']} E{row['age']} {row['presentation']:5} {state:12} {', '.join(row['fonts'])}")
    print(f"\nper-face coverage of E{age}+ bases (cmap only, not shaping)")
    recent = [row for row in inventory["scalars"] if newer(row)]
    for filename in inventory["fonts"]:
        have = [row for row in recent if filename in row["fonts"]]
        missing = [row["codepoint"] for row in recent if filename not in row["fonts"]]
        note = "  missing: " + " ".join(missing) if missing and len(missing) <= 8 else ""
        print(f"  {filename:32} {len(have):3}/{len(recent)}{note}")
    print("\nsequences (shaped with production's buffer flags; complete = one glyph, none missing)")
    for row in inventory["sequences"]:
        print(
            f"  {row['name']:20} rgi={str(row['rgi']):5} composition={str(row['composition']):5} "
            f"asserted={str(row['asserted']):5} probe={str(row['probe_sample']):5}"
        )
        print(f"    complete-in: {', '.join(row['complete_in']) or '-'}")
        if row["cases"]:
            print(f"    cases: {', '.join(row['cases'])}")
    if inventory["inventory_errors"]:
        print("\ninventory errors")
        for error in inventory["inventory_errors"]:
            print(f"  {error}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-root", default="font-fixtures-stage", type=Path)
    parser.add_argument("--repo-root", default=".", type=Path)
    parser.add_argument("--json", action="store_true", help="emit the inventory as JSON")
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify the explicit inventory still matches the suites, and exit",
    )
    parser.add_argument(
        "--age",
        default="15.0",
        help="report scalars introduced in this emoji version or later (default 15.0)",
    )
    arguments = parser.parse_args()
    if arguments.check:
        errors = check_inventory(arguments.repo_root)
        for error in errors:
            print(error, file=sys.stderr)
        return 1 if errors else 0
    inventory = build(arguments)
    if arguments.json:
        json.dump(inventory, sys.stdout, indent=2, default=sorted)
        print()
    else:
        report(inventory)
    return 1 if inventory["inventory_errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
