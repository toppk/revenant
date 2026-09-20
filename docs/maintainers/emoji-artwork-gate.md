---
man: revenant-emoji-artwork-gate
section: 7
manual: maintainers
description: text-emoji artwork acceptance gate and its known renderer gaps
---

# Text-emoji artwork acceptance gate

**Recorded:** 2026-09-19. This page documents fixtures and regression coverage
only. No font-routing, discovery, or fitting behavior is changed by the work it
describes; the failing cases below are the renderer repairs still owed.

The [font fallback review](font-fallback-review.md) traced one incident: the
application line

```rust
eprintln!("🛠 Installed {}-{}", name, version);
```

rendered tofu while the cursor advanced correctly. Three suites passed anyway.
`tests/xvfb-emoji-artwork.sh` is the gate that fails on it. The tracked emoji
and font-fallback priorities place it before any resolver change.

## What the gate asserts

Each positive case requires all of the following, and a case that satisfies
only some of them fails:

- a route whose role is not `tofu`;
- the expected effective font file, taken from the route log's `file=`, because
  a family name is a preference and not an identity;
- the expected presentation (`text`, `emoji`, or `none`);
- ink that is **not pixel-identical** to the deterministic tofu box measured in
  the same universe at the same cell geometry;
- ink above a low floor, with margins inside its own cells;
- every region **outside** those cells accounted for: a blank neighbor must be
  genuinely blank, an occupied neighbor must be pixel-identical to a control
  render of the same line with a space in place of the symbol, and the internal
  border above and the whole row below must carry no ink.

Containment is established by those outside-the-cell comparisons, not by bounds
measured inside an already cropped cell, which cannot see ink that left it. An
occupied neighbor is never accepted merely for being nonblank, since overwritten
text or a tofu box would satisfy that. Where two symbols are adjacent or spaced,
both are graded in full as atoms rather than one being treated as the other's
neighbor.

Cursor advance is asserted separately, from a CPR reply, and is never excused by
a known gap. That separation is the point: a monochrome pixel class alone proves
nothing, since deterministic tofu is monochrome ink, and a correct advance says
nothing about whether a glyph was drawn.

A listed gap must also fail in exactly the documented way. The suite requires the
width-1 advance deferral in the log and a tofu route, and permits only the three
failure categories that follow from that miss (`tofu-route`, `effective-file`,
`tofu-ink`). A wrong presentation, an unexpected effective font, a damaged
neighbor, or ink outside the assigned cells is an independent regression and
fails the suite even for a listed case. Fault injection confirms each path:
altering one gap case's expected presentation, its neighbor expectation, or
listing a case that does not fail by this mechanism each produce `FAIL` with the
offending category named.

Negative cases are retained rather than converted. `legacy-mono-absent` renders
bare U+1F6E0 in the `mono` universe, whose Noto Emoji 1.05 face genuinely lacks
that base, and `pua-absent` renders U+E000 with `systemFallback: false`. Both
must produce exactly the tofu box, which is what keeps the positive assertions
above from degenerating into "any ink passes".

## Fixtures and their identities

`tools/stage-font-fixtures` now stages two monochrome Noto Emoji faces. They
never share a universe, because both name the family `Noto Emoji`.

| File | Internal version | Source | Text-default bases | Role |
| --- | --- | --- | --- | --- |
| `NotoEmoji-Regular.ttf` | 1.050 | noto-emoji 2.028 source archive | 63 of 207 | Retained incomplete negative fixture |
| `NotoEmoji-Regular-3.003.ttf` | 3.003 | Fedora `google-noto-emoji-fonts` 20250623-4 | 207 of 207 | Modern positive fixture |

The new file is byte-identical to the installed package file the review
diagnosed, `/usr/share/fonts/google-noto-emoji-fonts/NotoEmoji-Regular.ttf`,
SHA-256 `b57ed895ae9d09ba7b4b19c343a75cf39aad57156c3450e339d9d11556d7edc1`. It is
a test input under SIL OFL 1.1, staged from a pinned archive hash like every
other fixture; nothing is bundled into a runtime artifact. Package release
versions and internal `head.fontRevision` values are different identities, so
`manifest.json` now records `version` alongside `sha256`, and
`text_default_bases` records the coverage census that separates these two faces.
The census counts emoji bases whose default presentation is text, excluding the
ASCII keycap bases, as a scalar cmap count. It makes no claim about sequences,
artwork quality, or reachability.

`mono-modern` holds the modern face with the base text font. `routing-modern`
adds the CJK face and Noto COLRv1, mirroring `routing`, so the color and
monochrome alternatives overlap exactly as they do on a real system.

## What repaired the reported failure

The gate reports **17 passed, 0 expected gaps, 0 unexpected**. The eleven
width-1 entries that used to be listed in `known_gap` were removed one at a time
as their complete assertion sets passed; no assertion was weakened, and the two
negative tofu cases still pass unchanged.

What was measured before the repair, in `routing-modern` and `mono-modern`:
system discovery did reach the modern monochrome face, and the width-1 advance
rule then deferred it.

```text
font: activated Xft fallback slot=0 style=0 entry=3 source=chain0 budget=1/50
font: deferred Xft fallback slot=0 entry=3 advance=28.207 cell=13 width=1 limit=10
font: route base=U+1F6E0 width=1 presentation=text role=tofu glyphs=0 file=(unknown)
```

`XtpFontFallbackAdvanceFits` in `src/font_metrics.c` returns `true` before
computing anything unless `committed_width == 1U`, so the rule is a one-cell
rule and the defect was one-cell advance rejection of a usable face with no
fitting policy to answer it. That is what the span fitting policy in
[font-resolution(7)](font-resolution.md) now supplies. `limitFontWidth` is
unchanged and still governs every other atom class; the fitted instance has to
satisfy it like anything else.

The same line now routes as:

```text
font: fitted Xft fallback slot=0 span=13 advance=28.207 fitted-advance=12.902
font: route base=U+1F6E0 width=1 presentation=text role=fallback \
      file=.../NotoEmoji-Regular-3.003.ttf
```

With `faceNameEmojiText` configured the same atom routes `role=emoji-text` from
the same file, which is the explicit lever for the same outcome.

Measured artwork at 16 point in a 13x27 cell: 90 inked pixels with bounds
`1,12,12,11`, so the glyph occupies roughly a 12x11 box inside its cell, with
margins on every side and no ink in the border above, the row below, or any
neighboring cell. It is small. Legibility remains a human judgment and belongs
to the TDN artwork probe; this suite establishes that a recognizable-sized,
correctly routed, contained glyph is drawn where tofu used to be.

## A separate, differently caused miss

The older expected-tofu cases in `tests/xvfb-emoji-routing.sh` are **not** the
same defect, and the audit annotations in that file now say so. For `info-text`,
`policy-text-grin`, and `policy-text-color-wide` in the `routing` universe, Noto
Emoji 1.05 does map the base, but the log shows only four queued primary-slot
candidates and the color face's empty outline being rejected: no monochrome
candidate is ever activated. That is the discovery and charset-trimming loss,
recorded separately.

Those expectations are left unchanged. They accurately describe today's
behavior and they still prove that forced text presentation never leaks color
and never alters a committed width. What they cannot do is show that a glyph
appeared, which is why the positive requirement lives in the new suite instead of
being bolted onto them.

### The rest of the audit

Every other expected-tofu assertion in the suite was checked against the
fixture manifest and found to be a genuine absence, so none of them needed a
change:

| Assertion | Why tofu is correct |
| --- | --- |
| `xvfb-font-tofu.sh`, both cases | The whole point of the suite: all roles cleared and `systemFallback: false` |
| `xvfb-emoji-routing.sh` `no-color-*` | Color declined for bases whose only ink is color or bitmap; already annotated upstream of this work |
| `xvfb-font-wide-boundary.sh` `configured-wide-miss` | The wide face is Noto Emoji, whose manifest probe records `cjk_sentinel: missing` |
| `xvfb-font-han.sh` U+65E5 with an unsupported IVS | Exact IVS matching must reject it |
| `xvfb-font-route-cache.sh` and `check-font-routing-report.py` U+10FFFF | A noncharacter, unassigned in every face |

That two different mechanisms produce an indistinguishable final `role=tofu` is
itself a finding: the route log cannot separate lost candidates from advance
rejection without reading earlier lines. The diagnostics work in the priorities
should make that distinction visible at the route.

## The resources themselves

`tests/xvfb-emoji-text-role.sh` covers `faceNameEmojiText` and `fitEmojiText`,
which the gate above deliberately never sets: both chain entries, a hostile
generic-emoji pattern rule in the `alias-emoji` universe, an explicit
`color=true` in the user's own pattern being honored, the fitting opt-out at both
consult points, precedence against `faceName` entry 2 and `fallbackFace1`,
`limitFontsets: 0` suppressing both rescue entries, the width-one scope of the
advance rule, instance reuse, a second span reached through a font-slot switch,
and styled output. Seventeen cases pass, with route, pixel and cursor assertions
as applicable; these are not all asserted in every case. The reported
AddressSanitizer run found no lifetime errors on the exercised paths. The
slot-switch case observes a second fitted instance; an earlier log line alone
does not establish that the first instance remains alive. Ownership is enforced
by retaining fitted fonts until universe destruction.

The fitted-face table's boundary is a unit test, `fitted-face table` in
`-self-test`: reuse returns the same entry, the span is part of the key, a full
table refuses a new entry instead of displacing one, every entry handed out
earlier stays findable after exhaustion, and clearing the count models the
universe replacement that a reload performs.

Two coverage limits are worth naming rather than papering over:

- No staged fixture has a monochrome emoji family with a real bold or italic
  face, so the styled cases prove SGR does not widen an atom past its cells but
  do **not** exercise the branch that declines an oversized *styled candidate*.
  That branch is covered by inspection only; a bold-capable monochrome emoji
  fixture would close it.
- Every real glyph in the staged monochrome face has the same 2600-unit advance,
  so no two atoms built from it can have different advances. Order independence
  is therefore established by construction — the atom's advance is not an input
  to the scale — and the suite asserts the observable consequence, that two
  atoms share one fitted instance.

## Running it

```sh
# The staging tree is never modified in place; restage and replace it.
tools/stage-font-fixtures --seed font-fixtures-stage/sources font-fixtures-next
tools/font-fixture-info.py --check font-fixtures-next/manifest.json
mv font-fixtures-stage font-fixtures-old && mv font-fixtures-next font-fixtures-stage
meson test -C build-agent-gcc xvfb-emoji-artwork --print-errorlogs
```

A repaired renderer will turn a gap into `XPASS`, which fails the suite until
its `known_gap` entry is removed. Remove entries one at a time, with the
observation that justifies each removal; do not lower an expectation to clear
one.
