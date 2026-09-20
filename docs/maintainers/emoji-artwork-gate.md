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

## A separate miss, since repaired

The older expected-tofu cases in `tests/xvfb-emoji-routing.sh` were **not** the
same defect as the width-1 advance rejection above. For `info-text`,
`policy-text-grin` and `policy-text-color-wide` in the `routing` universe, Noto
Emoji 1.05 does map the base, but the coverage-trimmed candidate sort dropped it
as redundant once the color face covered the same scalars: the log showed four
queued primary-slot candidates, the color face's empty outline being rejected,
and no monochrome candidate ever activated.

That was the discovery loss, and it is now repaired by the presentation-aware
second sort described in [font-resolution(7)](font-resolution.md). All three
cases render from `NotoEmoji-Regular.ttf` today, and their expectations in that
suite were updated from `role=tofu` to the serving role — a strictly stronger
assertion, since a role and a file are named where nothing was before.

`tests/xvfb-font-discovery.sh` is the suite for that repair: the reproduction
itself, clean defaults, a generic rule preferring color, a universe with no
monochrome face at all, an explicit `fallbackFace1` still outranking discovery,
`systemFallback: false`, `limitFontsets: 0`, and both CJK routes. Nine cases,
each checking route identity, containment and the cursor column.

One thing that repair did not change is worth keeping in view: a final
`role=tofu` still cannot be told apart from absent coverage without reading the
earlier log lines. Distinguishing lost candidates, advance rejection and genuine
absence at the route itself is the diagnostics work, still outstanding.

## The resources themselves

`tests/xvfb-emoji-text-role.sh` covers `faceNameEmojiText` and `fitEmojiText`,
which the gate above deliberately never sets: both chain entries, a hostile
generic-emoji pattern rule in the `alias-emoji` universe, an explicit
`color=true` in the user's own pattern being honored, the fitting opt-out at both
consult points, precedence against `faceName` entry 2 and `fallbackFace1`,
`limitFontsets: 0` suppressing both rescue entries, the width-one scope of the
advance rule, instance reuse, a second span reached through a font-slot switch,
and styled output. Twenty cases pass, with route, pixel and cursor assertions
as applicable; these are not all asserted in every case.

Three of them are the real styled faces: the regular face fitted and serving, SGR
bold selecting the **bold file** with that candidate fitted in its own right, and
an italic candidate that cannot satisfy the policy being declined with the regular
face retained. The two outcomes are distinguished rather than merged. The bold case
asserts a second fitted instance exists and that bold's pixels differ from
regular's; the italic case asserts the italic file never served, that the decline
was logged, and that the italic attribute survived the decline — the serving face is
the regular one, so nothing there claims the artwork looks slanted. Disabling the styled span
check makes the bold case fail on the fitted instance and the italic case fail on
the effective file, the serving face and the missing decline.

Paint clips to the atom's cells, so an unfitted oversized styled glyph is cropped
rather than allowed to bleed. The assertions these cases make — containment, and
bold's pixels differing from regular's — cannot reliably tell a cropped glyph from a
fitted one. Assertions that could, such as a reference render or a check on the
fixture's complete bar geometry, are not made here, which is why the bold case
asserts the fitted instance instead of leaning on pixels. The reported
AddressSanitizer run found no lifetime errors on the exercised paths. The
slot-switch case observes a second fitted instance; an earlier log line alone
does not establish that the first instance remains alive. Ownership is enforced
by retaining fitted fonts until universe destruction.

The fitted-face table's boundary is a unit test, `fitted-face table` in
`-self-test`: reuse returns the same entry, the span is part of the key, a full
table refuses a new entry instead of displacing one, every entry handed out
earlier stays findable after exhaustion, and clearing the count models the
universe replacement that a reload performs.

The styled-candidate branch was the outstanding gap and is now covered by a
generated family rather than by inspection.
`tools/font-fixtures/make-styled-emoji.py` produces **XTP Styled Emoji** in three
real styles: Fontconfig reads weight 200 for Bold and slant 100 for Italic, with no
`FC_EMBOLDEN` and no `FC_MATRIX` on either, so neither is a synthesised style.
Regular and Bold map U+1F6E0 to a two-em glyph — a measured advance of 52.187px in
a 13px cell at 16 point — so both must be fitted. Italic maps it to a half-em glyph
that a GSUB multiple substitution in `liga` decomposes into two, so the run
advances too far while the face's own maximum advance already fits one cell, which
is a shape fitting cannot repair. Regular draws two bars and Bold three, so which
face served shows in pixels as well as in the route log. The `styled-emoji`
universe carries the family from `fonts-styled/`, outside the manifest, for the
same reason the crowd fillers are outside it: an adversary for one code path, not a
rendering fixture.

One coverage limit is worth naming rather than papering over:
- Every real glyph in the staged monochrome face has the same 2600-unit advance,
  so no two atoms built from it can have different advances. Order independence
  is therefore established by construction — the atom's advance is not an input
  to the scale — and the suite asserts the observable consequence, that two
  atoms share one fitted instance.

## Reload and cache coverage

`tests/xtp-font-reload.c`, driven by `tests/xvfb-font-reload.sh` in the
`routing-modern` universe, carries the emoji transitions in **one running widget**
so that later steps inherit the state earlier ones produced. What it establishes,
in order:

1. A one-cell text emoji atom is served by automatic monochrome discovery with role
   `fallback` from `NotoEmoji-Regular-3.003.ttf`, and the serving font **is** the
   fitted table's entry for the current span. Routing it again returns the same
   instance and does not fit the face a second time.
2. A successful geometry reload of the active slot advances the generation exactly
   once and changes the cell width. The replaced universe starts with an empty
   fitted table; re-routing the atom fits the face to the **new** span and no entry
   for the previous span survives.
3. A successful font reload that sets `faceNameEmojiText` advances the generation
   once and changes the serving **role** from `fallback` to `emoji-text`. The role
   is the assertion, because the same file reached through automatic discovery would
   prove nothing about the rescue taking over.
4. A rejected reload leaves the generation and the effective universe unchanged, and
   the atom then still routes `emoji-text`, from the same file, returning the same
   fitted instance, which is still the table's entry for the current span.

Two kinds of reuse are asserted separately, because the same font pointer comes
back either way: the **route cache** gains exactly one entry for the atom and none
when it is routed again, and the **fitted table** keeps exactly one instance. Both
counters reset when the universe is replaced.

A count that does not grow is not a hit, since re-storing the same key would also
leave it unchanged, so the hit itself is read from the router's own debug logging
(`src/font_router.c` logs `route-cache hit`, `miss` and `stale`). The helper raises
the log level and prints a `PHASE` marker before each transition; the suite then
attributes the lines to phases and requires a `miss` for the atom's first route, a
`hit` for the repeat, and **no** `route-cache stale` line anywhere, since a stale
entry surviving a reload is exactly the defect this area is about.

Evidence is observable state — generation counters, role names, effective files and
the fitted table's spans. Nothing compares against a font pointer from a destroyed
universe: an allocator may hand the same address back, so identity there would
prove nothing about replacement. The
suite's snapshot check pins the generation sequence `2, 4, 4`: advanced by each
successful reload, held by the rejected one. The whole suite also runs under
AddressSanitizer, since this is the path where a universe is destroyed while fitted
faces and route-cache entries point into it.

What these transitions do **not** cover:

- Nothing about **pixels**: routing is in-process here. Painting across the same
  transitions is covered by the separate helper below, which is why the routing
  helper keeps the stub backend.
- Only the monochrome text path. The color emoji role and the styled bold and italic
  faces are not exercised across a reload.
- The presentation reserve's interaction with reload is untested: a reload clears the
  reserve by replacing the universe, which no case asserts.

### Painting across the same transitions

`tests/xtp-font-reload-paint.c` is the painted variant, run by the same suite. The
routing helper links `src/terminal_stub.c` (`meson.build`), and the stub ignores
input, emits no cells and reports column zero, so it can never paint: the painted
variant links the real backend instead and asserts that with
`XtpTerminalBackendIsStub` **before** it looks at any pixel. It keeps one widget
across every transition, as the routing helper does.

It is deliberately narrow. It paints through `XtpTerminalFeedOutput` and
`XtpVtUpdate` — the dirty-update path the application uses; `XtpVtRedraw` repaints
the cached frame, so every sample would have shown the first screen — and reads back
rectangles of the window with `XGetImage`. Two harness facts are established before
any emoji claim: plain ASCII paints ink in its own cell and advances one column, and
the reference character U+E000 **routes to tofu** — asserted, not assumed from a
nonblank drawing — and paints the box whose checksum is what "not tofu" then means at
that geometry. The reference is re-measured before every phase, since a reload
changes the cell.

Four phases follow in one widget: automatic fallback, the same after a geometry
reload, the explicit `faceNameEmojiText` rescue, and the rejected reload that has to
keep painting from the retained universe. Each asserts, for bare U+1F6E0: the
**serving role** expected of that phase (`fallback`, then `emoji-text`) and an
effective file of `NotoEmoji-Regular-3.003.ttf` for the very route the paint draws
from; then ink present, a checksum different from that geometry's tofu box, and
containment. The rescue phase is where role and file diverge in usefulness: the file
is the same one automatic discovery already found, so only the role shows the rescue
took over — and only the paint shows it still draws.

Containment is measured **outside** the atom: the cell to its right and the full
two-cell-wide band in the row below must be blank. A bound computed inside the
sampled cell, as an earlier version had, cannot fail, so it is gone rather than
labelled. The containment sample is taken with the cursor hidden, since the cursor
block inks the cell after the atom; the advance is read from a second paint with it
visible, since the core reports a cursor column only for a visible cursor.

The widget is created with `internalBorder` 0 and white-on-black through
`XtVaTypedArg`: those resources are a Dimension and Pixels, so a plain string
argument is stored as its pointer value, which put the text origin off-window and
left the window a uniform garbage color — every cell then sampled identically and
the emoji looked "pixel-identical to tofu".

Each of these checks was shown to reject a wrong expectation: naming the
`emoji-text` role in the automatic phase, expecting `DejaVuSansMono.ttf` as the
effective file, using a served character as the tofu reference, and painting ink into
the cell beside the atom or the row below it each produce the corresponding failure.

What it does not cover: only the monochrome text path and this one atom, no color or
styled faces, and no assertion that the artwork is recognizable — it distinguishes
drawings, it does not read them.

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
