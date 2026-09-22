---
man: revenant-unicode-18-migration
section: 7
manual: maintainers
description: coordinated Ghostty and Unicode 18 migration, with measured behavior deltas
---

# Unicode 18 migration

**Recorded:** 2026-09-21. This page records what the coordinated backend and
Unicode migration changed, how each input is pinned, and which differences were
measured rather than inferred. Font coverage is reported separately from renderer
behavior throughout: a character a staged font lacks is a fixture limitation.

## Provenance

The backend pin moves to the maintainer-selected revision, and every Unicode input
moves with it. Ghostty takes its tables from the `uucode` Zig package, which vendors
the UCD files its build reads. Revenant's generators read their backend-side input
from the package that the **pinned** revision's `build.zig.zon` names — resolved with
`git show <pin>:build.zig.zon`, so neither the checkout's current HEAD nor other
packages in the Zig cache can substitute for it (`tools/uucode_pin.py`). That file is
resolved whatever the command line says: an explicit `--input` or `--width-input` is
accepted only as a byte-identical copy of it, which keeps overrides usable for a copy
elsewhere on disk and refuses anything else with both SHA-256 values named.

| Input | Before | After |
| --- | --- | --- |
| `tools/fetch-libghostty` pin | `0c2a290d3a3e2a599be3a43435d778a5896667ee` | `27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c` |
| Ghostty Unicode data | `uucode-0.2.0-ZZjBPlK5VADj7fdoq7G8LIHzD5o6FSkcBXXrRWr4jnrA` | `uucode-0.2.0-ZZjBPuuFVgC8YZ8eld4fOKsZANLIhTFMzULQxhkLi1C7` (Ghostty `9cdbf798d`) |
| `emoji-data.txt` | 17.0, `2cb2bb9455cda83e8481541ecf5b6dfda66a3bb89efa3fa7c5297eccf607b72b` | 18.0.0, `80d00f8e616a0ef27fd6b8de3b758c06383b5d917e2977709578e68baf733bf1` |
| `Scripts.txt` | 17.0, `9f5e50d3abaee7d6ce09480f325c706f485ae3240912527e651954d2d6b035bf` | 18.0.0, `0071fd81b6aeae25f6e8bce8efec3066a6476a91b49bdb2f52dc76e817862a6a` |
| `src/emoji_ranges.h`, `src/han_ranges.h` | generated, Unicode 17.0 | regenerated, Unicode 18.0 |
| Fixture manifest `unicode_version` | `17.0` | `18.0` |

Both new files were checked twice: the copies inside the pinned `uucode` package are
**byte-identical** to `https://www.unicode.org/Public/18.0.0/ucd/…`, so the staged
fixture data, Revenant's generated tables and libghostty's width and segmentation
tables all derive from the same bytes. The staging source is now named
`emoji-data-18.0.0.txt`, so a seed directory can hold several releases instead of
failing on a same-named file from the previous one.

Two generator changes were needed and nothing else. Both table generators take their
backend input from the pinned revision's own `uucode` dependency, as above: a Zig cache
keeps every package it has fetched, so after a migration both releases are present and
neither "the only one" nor "the one matching the manifest" identifies the backend
actually pinned. And version comparisons use the `major.minor` prefix, because 17.0's
header reads `17.0` while 18.0.0's reads `18.0.0`. `tests/unicode-table-provenance.py`
holds this. Its negative cases build a throwaway repository — a stub fetcher, a
one-commit checkout whose `build.zig.zon` names a package, and synthetic UCD files for
that package and an older one that sorts first — so they need no older commit or Zig
cache and run on a fresh checkout. They cover selection by pin rather than by cache or
directory order, a missing pinned package, a divergent manifest, older explicit inputs
for both generators (`--input` for Han; `--input` with and without `--width-input` for
emoji), and byte-identical copies being accepted. Separately, it checks the real pin:
the package `build.zig.zon` names, the staged fixture being an exact copy of it, and both
generators accepting it. Earlier versions of these generators wrote Unicode 17.0 tables
with exit status 0 in the divergence and explicit-input cases; both were reproduced
before the fix.

Deliberately **not** migrated: the font fixtures. Their versions are pinned for the
behaviors they encode — Noto Emoji 1.05 is the coverage-miss negative, 2.034 is
Ghostty's historical CBDT, 3.003 is the modern monochrome positive — and none of them
is a Unicode data input. The xterm oracle stays `xterm-411`; the `xterm-411a`
development snapshot also carries Unicode 18 width tables, and comparing them is a
separate compatibility question, not part of this migration.

## What changed in the data

| Area | Delta |
| --- | --- |
| `Emoji` | +9, no removals: U+1F6D9, U+1FA8B, U+1FA8C, U+1FA8D, U+1FACC, U+1FADD, U+1FAEB, U+1FAF9, U+1FAFA (all E18.0) |
| `Emoji_Presentation` | the same 9; **no existing character changed its default presentation** |
| `Emoji_Modifier_Base` | +2 (U+1FAF9, U+1FAFA) |
| `Extended_Pictographic` | **−18** (U+1F1AE, U+1F7DA..U+1F7FF): assigned in 18.0 as ordinary symbols |
| `Script=Han` | +1 (U+2B81E); no script reassignments |
| East Asian Width | **no change for any existing code point**; 12319 newly assigned Wide/Fullwidth |
| Grapheme breaking | UAX #29 GB9c simplified: 4 of the 764 shared `GraphemeBreakTest` cases change, 87 cases added |

The text-default census that the fixture manifest records is unchanged at 207 bases,
because every Unicode 18 addition takes emoji presentation.

## Measured behavior, old against new

Both binaries were built from this tree; only the backend differs
(`build-u17-baseline` pins `0c2a290d3`, the candidate pins `27e8b3fa8`). The baseline
carries the new frontend tables, which isolates backend-owned width and segmentation.
Advance is the CPR column minus one; `graphemes` is the renderer's own frame count.

| Atom | Regime | Unicode 17 | Unicode 18 | What moved |
| --- | --- | --- | --- | --- |
| `?` + U+094D + U+0924 | 2027 | advance 2, graphemes 2 | advance 2, graphemes **1** | segmentation only |
| `?` + U+094D + U+0924 | legacy | advance 2, graphemes 2 | advance 2, graphemes 2 | nothing |
| U+094D + U+0915 | 2027 | advance 1, graphemes 1 | advance 1, graphemes 1 | nothing observable (see below) |
| U+0915 + U+094D + U+0937 (control) | 2027 | advance 2, graphemes 1 | advance 2, graphemes 1 | nothing |
| 😀 + ZWJ + U+1F7FF | 2027 | advance 2, graphemes 1 | advance **3**, graphemes **2** | segmentation **and** advance |
| 😀 + ZWJ + U+1F7FF | legacy | advance 3, graphemes 2 | advance 3, graphemes 2 | nothing |
| 😀 + ZWJ + 🔥 (control) | 2027 | advance 2, graphemes 1 | advance 2, graphemes 1 | nothing |
| U+1FADD (E18.0 base) | both | advance 1 | advance **2** | newly assigned width |
| U+2B81E (E18.0 Han) | both | advance 2 | advance 2 | script membership only |

U+094D + U+0915 is one of the four changed `GraphemeBreakTest` cases — UAX #29 splits
it in 17.0 and joins it in 18.0 — yet both backends report one atom. A line-initial
zero-width linker has no cell of its own to occupy, so the terminal attaches it either
way; the data change exists, and this position cannot show it.

The routing consequence is larger than the width consequence, and it is the reason
advance is asserted separately. For `? + virama + TA` with the staged `shaping`
fixtures:

```
Unicode 17: route base=U+003F width=1 presentation=none role=tofu glyphs=0 file=(unknown)
            route base=U+0924 width=1 presentation=none role=fallback … NotoSansDevanagari-Regular.ttf
Unicode 18: route base=U+003F width=2 presentation=none role=fallback glyphs=4 … NotoSansDevanagari-Regular.ttf
```

With these fixtures, Unicode 17 split the atom and the question-mark cluster found no
face that could shape it, so it drew a missing-glyph box; Unicode 18 hands the whole
cluster to one face. That artwork outcome belongs to this fixture set — another font
set could draw the split atoms differently — but the segmentation change does not. The
cursor advance is 2 in both, so a width-only test cannot see either.

## Regressions added

- `tests/xvfb-text-shaping.sh`, `indic-conjunct-18`: mode 2027, the `shaping`
  universe. Asserts the CPR column first and on its own, then that the atom routes as
  one width-2 cluster shaped by the Devanagari face with no tofu route for U+003F and
  no separate U+0924 route, then ink in its two cells and a blank cell after them. Run
  against the Unicode 17 baseline binary it fails with the documented old routes.
- `tests/xvfb-emoji-routing.sh`, `zwj-extpict-break-18`: the ZWJ/Extended_Pictographic
  boundary change, graded by CPR (3), pixel class and the first atom's route.
- `tests/xvfb-emoji-routing.sh`, `e18-base-width`: U+1FADD keeps the Unicode 18 width
  of 2 while routing to deliberate tofu, because **no staged face covers any E18.0
  base**. This is the font-coverage limitation stated as a test rather than left to
  inference.
- `-self-test` now pins `18.0` for both tables, asserts two Unicode 18 additions with
  their emoji presentation, checks that U+1F7FF is *not* an emoji base, and asserts
  the new Han ideograph U+2B81E.

## Fixture limitations

`tools/emoji-coverage-audit.py` reports, against the pinned 18.0 data: 1447 `Emoji`
bases, 43 in the audited scope (E15.0+), of which 5 are asserted by a named case, 29
are covered by a staged face and asserted by nothing, and **9 are covered by no staged
face at all** — exactly the E18.0 additions. Noto Color Emoji 2.051, OpenMoji 17.0 and
the monochrome Noto Emoji 3.003 all predate Unicode 18, so every E18.0 base renders as
deterministic tofu with a correct advance. Replacing a fixture with an E18-covering
release would turn `e18-base-width` into a positive case; no font was added here,
because none of the pinned releases covers these characters.

## Probe and TDN

- New width case `emoji-unicode18` (`just probe text-unicode-tables emoji-unicode18`,
  or `dec-mode-2027-grapheme-clusters emoji-unicode18`), carrying the five samples
  above with their measured contracts and notes that name what width cannot see.
  `text-unicode-tables` is now mapped in the probe's curated navigation group for
  emoji and Unicode widths; nothing else about navigation changed.
- `emoji-sequences` runs one pass per requested regime (`--regime both`, the default,
  gives a legacy pass then a cluster pass), requesting each through the width runner's
  mode handling, reporting the state the terminal actually answered, and restoring the
  original mode on exit. An unanswered or unrecognized query is shown as `unknown`,
  never as a confirmed legacy contract. Its samples are split into joined sequences,
  single bases and the Unicode 18 segmentation samples, and the last group gets
  per-sample, per-regime guidance: under the cluster contract the conjunct is one
  cluster and the ZWJ boundary is two; under legacy both stay separate in either
  release. None of it predicts artwork beyond segmentation, which depends on fonts.
- The other artwork cases do not select a regime; they report the state they found and
  leave mode 2027 alone. Visual outcomes stay unassessed until a human records one; see
  the [visual acceptance procedure](emoji-artwork-gate.md#visual-acceptance-procedure).

## Unresolved

Nothing known is out of step between the backend and the frontend: both derive from
Unicode 18.0.0. The guarantee is specific: the generators read the pinned revision's
`uucode` package, accept explicit inputs only as byte-identical copies of it, and
refuse a differing frontend version, which `unicode-table-provenance` tests. They do not re-verify libghostty's compiled tables
themselves, and they need the pinned revision present in `upstream/ghostty` with its
package in the Zig package directory. The open items are coverage rather than behavior
— no E18.0 font, and no human assessment of any artwork capability.
