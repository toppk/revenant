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
is a shape fitting cannot repair. That half-em maximum advance is load-bearing:
anything added to these faces has to keep it, which is why the sequence glyphs
described below are half an em too. Regular draws two bars and Bold three, so which
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

## Unicode coverage audit

`tools/emoji-coverage-audit.py` is the reproducible inventory behind the coverage
claims here. Its inputs are pinned: the emoji properties and the `E<version>` age of
each base come from the staged `data/emoji-data.txt` (**Unicode 17.0**, whose version
header `font-fixture-info.py --check` pins against the manifest), and the faces come
from the staged tree. It downloads nothing. Run it with
`python3 tools/emoji-coverage-audit.py`, `--json` for the full table, or `--check` to
verify the inventory alone.

Two things it deliberately does not do, because the first version of it did both and
both produced unreliable numbers:

- **It does not infer coverage by scanning the suites.** A codepoint can be written in
  a comment, in an unused variable, or assembled from shell variables a scanner never
  sees, and a sequence's components are not the sequence. Coverage comes from
  `AUTOMATED_CASES`, an explicit table naming the suite, the case, the atom and what
  that case asserts. The labels are read off the helper each case calls, so they
  distinguish `role` (a named serving role is required) from `non-tofu-role` (only that
  it is not tofu), and `shaping` (the route was required to report `glyphs=1`) is
  claimed only where a case actually requires it — in the artwork suite that means the
  cases using `check_every_route`, because `check_route` never inspects `glyphs=`. The
  scan survives only as a separate **source mentions** column, which means a codepoint
  is written down somewhere and nothing more.
- **It does not claim coverage outside its own scope.** The scope is every sequence in
  the table and every base newer than E15.0. Older bases are reported as **not
  inventoried**: the suites assert a great deal about them — bare U+1F6E0 is this
  gate's own subject — and this table simply does not enumerate them, so the
  asserted/unasserted totals below are for the audited scope alone.
- **`--check` is case-reference validation, nothing more.** Each row must point at a
  case its suite still *declares*, matched against the suite's own invocation forms
  (`run_case` / `run_unicode_case` with the universe first; `start_sample` /
  `negative_case`) with comment lines ignored, so a deleted case whose name survives in
  a comment, or a longer case name that contains a shorter one, is not a declaration.
  It cannot show that a case still makes the assertions its row claims; only reading
  the case can.
- **It does not shape with `hb-shape`'s defaults.** Production shapes a
  composition-requiring atom with `HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES` and
  everything else with `REMOVE_DEFAULT_IGNORABLES` (`XtpShapeUtf8ForComposition` and
  `XtpShapeUtf8`, `src/glyph_shape.c`), and the audit now selects the same flag per
  atom through a port of the classifier in `src/emoji_presentation.c`, plus `--bot
  --eot --cluster-level=1`. This is not cosmetic: with ignorables removed, an
  unsupported tag payload disappears and `TwitterColorEmoji-SVGinOT.ttf` reports **one
  plausible glyph** for the Scotland flag it cannot draw. With production's flags the
  same face reports `missing-glyph:7`, which is what the renderer refuses. The column
  is named `shaping` because one glyph is not the whole of acceptance either: coverage,
  ink, presentation and the advance rule are separate gates the audit does not model.

`tests/emoji-coverage-audit.py` (meson test `emoji-coverage-audit`, 13 tests) holds
each of these properties, all of which an earlier version got wrong: a commented and an
unused literal are mentions and not coverage; the two columns are shown to disagree; a
declaration deleted while a comment keeps its name **is** reported, and a longer case
name is not a declaration of a shorter one; the assertion labels are checked against
the helpers the cases call; U+1F6E0 is shown to be out of scope rather than unasserted;
and the removed-ignorables false positive is reproduced and then shown to be rejected
by the audit's own verdict, while a complete shaping verdict is shown to say nothing
about presentation or ink.

Counts as of this slice — 21 staged faces, 1438 `Emoji` bases, of which **34 are in the
audited scope** (E15.0+):

| bucket | bases |
| --- | --- |
| in scope, asserted by a named automated case | 5 |
| in scope, covered by a staged face, asserted by nothing | 29 |
| in scope, covered by no staged face | 0 |
| not inventoried (older than E15.0) | 1404 |
| *(written in a suite's source, coverage unproven)* | *35* |
| *(human-assessed probe samples, never automated evidence)* | *49* |

Probe samples include the Go artwork probe (`tools/probe/emoji_artwork.go`) as well as
the width probes; U+1FAE9 is a probe sample and nothing else, which the report shows.

Per-face coverage of the 34 bases newer than E15.0, cmap only:

| face | E15.0+ bases |
| --- | --- |
| Noto-COLRv1, NotoColorEmoji (2.051), OpenMoji-COLRv0 | 34/34 |
| NotoEmoji-Regular-3.003 | 27/34 — missing U+1F6D8, U+1FA8A, U+1FA8E, U+1FAC8, U+1FACD, U+1FAEA, U+1FAEF |
| TwitterColorEmoji-SVGinOT | 20/34 |
| NotoColorEmoji-2.034, NotoEmoji-Regular (1.05), Twemoji | 0/34 |

**The audit found no renderer defect.** That conclusion rests on the live renderer
cases below, not on the audit's shaping: every newer base sampled routes, shapes to one
glyph and paints. The one gap exposed is a font gap, and it is recorded as one. The
monochrome fixture predates E17.0, so forced text presentation has nothing to serve
those seven bases with and the route is deliberate tofu — with the cursor advance still
correct, which is the renderer's own contract and is asserted separately.

Representative cases, added to `tests/xvfb-emoji-artwork.sh` as rows rather than one
launch per codepoint:

| case | what it grades |
| --- | --- |
| `newer-bases-row` | U+1FA75 (E15.0), U+1FADF (E16.0), U+1FA8A and U+1FACD (E17.0) in mode 2027: each routed `presentation=emoji glyphs=1` from `Noto-COLRv1.ttf`, color ink that is not the tofu box, margins inside its own two cells, blank following cell, blank row below and border above, and one CPR of column 9 for the row |
| `newer-bases-text` | the same E15.0 and E16.0 bases under forced text presentation: served from `NotoEmoji-Regular-3.003.ttf`, monochrome ink, same containment, CPR column 5 |
| `newest-bases-text-gap` | the two E17.0 bases under forced text: deliberate tofu, pixel-identical to the measured width-2 tofu box, advance still column 5. A face covering E17.0 should turn this into a positive case |
| `tag-flags-row` | RGI England and Wales plus the valid non-RGI `usca` sequence in mode 2027: every logged U+1F3F4 route shaped one glyph from the color face, each atom contained, and England's artwork differs from Wales's, so the tag payload demonstrably reached the font |
| `tag-flags-legacy` | the same row in the legacy width regime, which was **measured** to supply the same complete atoms here — the identical advance (column 7) and route evidence, not an assumption that legacy joins anything |

Tag sequences, shaped with production's flags: the three RGI subdivision flags
(`gbeng`, `gbsct`, `gbwls`) are complete in six faces including the monochrome one.
The non-RGI `usca` is complete in `Noto-COLRv1.ttf` — a real, distinct ligature, which
is why the painted case above is legitimate — and `split:6` in the monochrome face,
which is a font limitation and not a defect: splitting a sequence the font cannot join
is the correct outcome.

Remaining limits:

- 29 in-scope bases are covered by a staged face and asserted by nothing, and 1404
  older bases are not inventoried at all. That is by design: this is a routing and
  artwork gate, not a Unicode conformance matrix, and the inventory is a scoped
  instrument for picking the next sample rather than a census of what the suites assert.
- The inventory's assertion labels are maintained by hand against the suites' helpers.
  `--check` catches a case that no longer exists; it cannot catch a case whose
  assertions changed while its name stayed, so the labels are only as good as the last
  reading of those cases.
- `zwj-polar-bear` is asserted by no automated case; it is a probe sample only.
- The audit measures scalar coverage from cmaps and sequence shaping from `hb-shape`.
  Neither says a glyph is legible, and nothing here promotes a visual-legibility
  record; the TDN capability records are unchanged by this slice.
- Modifier, keycap and regional-indicator sequences are sampled by one entry each,
  already exercised by the routing suite; the new cases add none.

## Styled whole-sequence selection

Style selection runs **after** a route is chosen, so a bold or italic request can
only replace the serving font, never the routing decision. The question that leaves
open is whether it can replace a *complete* sequence with an incomplete drawing of
its parts. Production accepts a styled candidate only if it shapes the whole cluster
to one glyph — `requires_composition` in `FontHasCluster`, `src/font_router.c` —
and the same condition governs the two places a styled candidate comes from: a
configured role (`VtFontRoleStyle`) and the same-family candidate list on a fallback
rung (`FallbackStyleRangeWithCluster`).

`tests/xvfb-emoji-routing.sh` grades that with eleven cases in the `styled-emoji`
universe, all in mode 2027, because the complete grapheme is the atom under test. The
same generated family now also carries one ZWJ sequence, U+1F468 ZWJ U+1F4BB: every
face maps both components and the joiner, and only Regular carries the `liga` rule
that joins them, verified with `hb-shape` — Regular shapes one glyph, Bold, Italic
and the partial family shape two, since HarfBuzz drops the default-ignorable joiner.
A second generated family, **XTP Partial Sequence**, covers the same components with
no ligature and nothing else, so it can stand in front as a preferred role face. One
more codepoint, U+1F4A1, is carried by the styled family alone, which is how an atom
is steered onto a fallback rung rather than to the preferred role face.

The sequence glyphs are half an em, the same as Italic's own U+1F6E0 glyph, so that
no face's maximum advance changes: that value is an input to the fitting decisions the
styled cases above grade, and a wider component glyph would have raised Italic's
`advanceWidthMax` from 500 to 700 and quietly moved that mechanism. The three faces'
`advanceWidthMax`, ascent, descent, weight class, `fsSelection`, italic angle,
`macStyle` and U+1F6E0 metrics are identical to the pre-change faces, and every
pre-existing glyph's outline and metrics compare byte-identical; only cmap entries
were added. Unchanged shaping alone would not have established that.

What the cases establish:

- the complete sequence is served by Regular with `glyphs=1`, and a bold or italic
  request keeps that effective file — the real, covering styled candidate is declined;
- the same request style on **one component** of that sequence is served by the bold
  or italic file itself, which is what makes the declines above attributable to
  complete-sequence acceptance rather than to coverage or to a synthesised style;
- a preferred role face that covers both components and cannot shape the sequence is
  refused whole: the atom falls through to the later candidate under regular, bold
  and italic requests, and that same face does serve one component;
- the decline happens on a fallback rung as well as in a configured role, reached by
  removing the wide role so the atom is served at `emoji-fallback`;
- that rung has its own control: under the same configuration, U+1F4A1 is served by
  the **bold file** at `emoji-fallback`, so retaining Regular for the sequence is not
  merely the absence of a usable bold candidate on that rung;
- every case keeps non-tofu ink, a blank following cell, a blank band below the whole
  span, and a CPR-measured advance of two columns, independent of which face served.

Disabling the acceptance check (dropping `requires_composition && run->count != 1U`)
makes both decisive cases fail. `seq-styled-bold` and `seq-fallback-bold` then accept
the Bold face for the incomplete atom: the route loses `glyphs=1` and the painted ink
goes from 171 pixels in `bounds=6,2,11,19` to 285 in `bounds=0,2,23,19` — the two
components drawn side by side across the span instead of the joined glyph. The source
was restored afterwards and the diff is empty.

Two limits, stated rather than implied:

- The route line reports the **requested** bold and italic attributes, not the served
  face's, so the effective file is the discriminator; `slant=` does follow the served
  face and distinguishes the italic cases.
- The fixture's component glyph is the same shape in all three faces, so pixels do
  not distinguish bold from italic here, and nothing in these cases claims the
  artwork looks bold or slanted. Human legibility is still not asserted anywhere.
- Two further cases use real Noto faces for a keycap and a regional flag under a
  style request. Neither family ships a real bold or italic, so they reach only the
  "no styled candidate exists" path: they grade that a style request leaves a composed
  atom whole, with its width and containment intact, and nothing more.

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
