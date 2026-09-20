---
man: revenant-font-resolution
section: 7
manual: maintainers
description: font-resolution architecture and contract
---

# Font-resolution architecture and contract

**Status:** implementation in progress. This document is the maintained
technical form of Font Resolution Specification Revision 5 plus Erratum 1.
It is normative for new resolver work; the implementation-status section
distinguishes landed behavior from requirements that remain to be built.

The [2026-09-19 fallback review](font-fallback-review.md) documents gaps in
default text-presentation emoji coverage, candidate discovery, and fitting.
The text-emoji rescue and fitting amendment is implemented below; candidate
discovery gaps remain. Passing resolver gates does not establish glyph
availability in every installed-font environment.

The companion contracts are:

- the [font-format baseline](../compatibility/font-format-baseline.md), which
  owns glyph technologies, emoji presentation, and fixture behavior;
- the width-vs-output contract, implemented at the backend boundary, which
  owns committed cell width and mode 2027;
- this document, which owns which font role serves an atom.

The shared invariant is: **width is committed before fonts are consulted and
is never revised afterward**.

## Evidence and authority

Inherited behavior is defined by patch-411 xterm as deposed or source-cited,
not by recollection:

<!-- markdownlint-disable MD013 -->

- [`compat/xterm-411-face-name.json`](https://github.com/toppk/revenant/blob/master/compat/xterm-411-face-name.json)
  is the blessed 32-case T0 v4 deposition at `Xft.dpi: 100`.
- [`tools/t0-facename-oracle.py`](https://github.com/toppk/revenant/blob/master/tools/t0-facename-oracle.py) records or
  checks that deposition without running Revenant.
- the exact `xterm-411` snapshot under `upstream/xterm-snapshots` is the source
  oracle where the runtime report cannot observe a decision.
- intentional differences belong in the
  [drift ledger](../compatibility/drift.md).

<!-- markdownlint-enable MD013 -->

The deposition owns list grammar, glyph-time entry-2 fallback, per-style stock
chains, wide-slot ordering, `limitFontsets` observations, and DEC double-height
loads. `limitFontWidth` is explicitly source-characterized because the
deposition could not distinguish it.

## Principles

1. **Shared names retain characterized semantics.** Stock resource grammar is
   inherited even when it is inconvenient. New behavior uses new names.
2. **One metrics authority.** Entry 1 of `faceName` alone defines cell
   geometry. Every other face contributes glyphs normalized to that grid.
3. **Width before ink.** A miss, unsupported glyph format, or failed render
   never changes committed width.
4. **No new default fallback source.** With new resources unset, Revenant does
   not consult a fallback source absent from stock xterm. Intentional default
   differences are enumerated rather than hidden by that narrower claim.
5. **Every automatic decision has a lever and observable output.**
6. **Atoms route; runs shape.** Family selection is atomic per backend cluster;
   adjacent compatible atoms shape as one run.

## Text model

- **Atom:** one cluster supplied by `libghostty-vt`, including its base,
  combining marks, variation selectors, ZWJ members, tag characters, and
  committed width. It is the family-decision unit.
- **Family decision:** one atom selects one role. Every scalar in the atom is
  rendered from that role; marks never switch families independently.
- **Shaping run:** a maximal adjacent sequence compatible in resolved role,
  selected style, script, direction, language, features, and paint policy.
  HarfBuzz receives one buffer per run.

Routing is greedy and per atom. Role boundaries are shaping boundaries.
Neighbor-dependent reconciliation is deferred because it would make routing
contextual, invalidate the atom-keyed cache/report model, and risk demoting a
user-pinned choice.

## Inherited slot-chain grammar

`faceName`, `faceNameDoublesize`, and `-fa` are comma-separated,
prefix-aware chains with at most two applicable Xft entries:

- trim whitespace and ignore empty entries (FN-07/08);
- skip `x:` entries in the Xft path (FN-02/03, FD-04);
- strip `xft:` before fontconfig matching;
- do not recognize `x11:` as a core-font prefix; retain it literally
  (FN-04, FD-05);
- retain entry 1 as the primary request even when fontconfig rescues an
  unresolvable family (FN-06);
- consult entry 2 as explicit glyph fallback before system candidates
  (FB-01/02);
- discard entry 3 and later with a warning (FB-03);
- give `-fa` full list semantics and precedence over X resources independent
  of argument order (FB-04, PREC-01/02).

New semantic slots (`faceNameEmoji` and `faceNameHan`) use this one grammar.
New numbered user-fallback resources deliberately do not: each is one complete
fontconfig pattern.

Entry 1 of the primary chain is the sole metrics authority. Fontconfig's
load-time rescue of an invalid entry 1 and glyph-time consultation of entry 2
are independent mechanisms.

## Resources

### Inherited

<!-- markdownlint-disable MD013 -->

| Resource | Contract |
| --- | --- |
| `renderFont` | Select Xft versus untouched bitmap mode |
| `faceName` | Primary two-entry chain and metrics authority |
| `faceNameDoublesize` | Wide chain and historical emoji rescue |
| `boldFont`, `wideBoldFont` | Stock style-chain inputs; constrained by the intentional same-family rule below |
| `limitFontsets` | Fallback-open budget, default 50 |
| `limitFontHeight` | DEC double-height recognition tolerance, default 10 |
| `limitFontWidth` | Per-glyph advance and DEC double-width tolerance, default 10 |
| `forceXftHeight` | Participates in primary cell-height calculation |

### New

| Resource | Default | Contract |
| --- | --- | --- |
| `faceNameEmoji` | unset | Emoji-presentation slot chain |
| `faceNameEmojiText` | unset | Monochrome rescue chain for text-presentation emoji atoms |
| `faceNameHan` | unset | Han glyph-form slot chain |
| `fallbackFace1` … `fallbackFace16` | unset | Ordered single-pattern user fallback roles |
| `systemFallback` | true | Permit unnamed slot-seeded fontconfig candidates |
| `colorGlyphs` | true | Permit color paint at any role/rung |
| `emojiPresentation` | `unicode` | Unicode, forced-text, or forced-emoji policy |
| `fitEmojiText` | true | Permit span fitting for text-presentation emoji atoms |
| `reportFontRouting` | false | Collect bounded routing records |

<!-- markdownlint-enable MD013 -->

There is no flat `fallbackFaces` value and no `-fb`. Comma ambiguity in an
inherited resource is compatibility debt, not precedent for new resources.

## Governor semantics

### `limitFontsets`

The limit is a per-(slot, style) budget of fallback fonts opened after a glyph
is found. Missing-glyph candidates are free; width-deferred glyph candidates
consume budget. LM-01/02/03 show zero, one, and two fallback loads.

Revenant applies the same glyph-bearing-open rule across the remaining
sequence after policy truncation, including `fallbackFaceN`. A candidate that
does not cover the atom does not let a long user list starve later fallback.
`limitFontsets: 0` exits before entry 2, numbered fallbacks, and system
candidates.

### `limitFontHeight`

This resource governs DEC double-height recognition, not ordinary fallback
face rejection. Values above 50 are capped at 50 with a diagnostic. LM-04 is
the default case; LM-05 records the cap diagnostic.

### `limitFontWidth`

This resource has two source-defined jobs:

1. `fontutils.c:4433`: one-sided per-glyph advance tolerance against committed
   cell width during fallback selection;
2. `util.c:4005`: DEC double-width font recognition.

`charproc.c:11576` caps values above 50 with a diagnostic. These rules are
source-characterized, not T0-deposed; T8b is a Revenant-level unit test rather
than an oracle gate.

## Han and ideographic variation sequences

An atom is captured by `faceNameHan` only when effective presentation is text
and its base has `Script=Han` in the pinned Unicode version. Characters merely
listing Han in `Script_Extensions` are excluded; shared CJK punctuation stays
under doublesize typography. Kana and Hangul are not captured. For coherent
Japanese or Korean typography, documentation should recommend assigning the
Han and doublesize roles to the same family.

An IVS remains attached to its Han base. Support requires the exact
base-selector pair in cmap format 14; default and non-default UVS mappings both
qualify. Shaping that drops the selector and produces the unvaried base is a
miss. If every role misses, deterministic tofu renders at committed width;
silently substituting the unvaried base is forbidden.

The Script=Han table is generated from and version-locked with the project's
pinned Unicode data, though the width table remains backend-owned.

## Capture, then resolve

### Phase 1: capture

Choose the first configured and applicable slot:

1. `faceNameHan` for the Han predicate;
2. `faceNameEmoji` for effective emoji presentation;
3. `faceNameDoublesize` for wide text, and for emoji when the emoji slot is
   unset;
4. `faceName` otherwise.

Three new-resource-gated cascades are permitted:

- a configured emoji slot that misses re-captures at doublesize, preserving
  the historical emoji rescue;
- a configured Han slot that misses re-captures at the slot that would have
  captured the atom without `faceNameHan`;
- a configured `faceNameEmojiText` is consulted for a text-emoji atom in the
  primary or wide resolve stage, as specified below.

A configured wide-text slot does not re-capture at primary after a miss
(WD-01). This boundary is implemented together with deterministic tofu so a
missing glyph cannot become either an accidental blank or a cross-slot rescue.

### Phase 2: resolve within the captured slot

The order is:

1. entry 1;
2. entry 2;
3. `fallbackFace1` … `fallbackFace16`;
4. `faceNameEmojiText` entry 1 then entry 2, for a text-emoji atom only;
5. slot-seeded `FcFontSort` candidates when `systemFallback` is true and the
   glyph-bearing-open budget remains;
6. deterministic tofu.

`systemFallback: false` truncates only before unnamed system candidates.
Named entry 2 and numbered fallbacks remain. This is intentionally different
from `limitFontsets: 0`, which permits nothing beyond entry 1.

The stock-equality claim applies to the normal style only. Bold, italic, and
bold-italic coverage is normal-canonical by intentional drift.

## Text-presentation emoji

A **text-emoji atom** is an atom that requires ink, whose effective presentation
is text rather than emoji, and whose base carries the Unicode `Emoji` property
while not being a bare ASCII keycap base (U+0023, U+002A, U+0030…U+0039 outside
an actual keycap sequence). Nothing else qualifies: ordinary text, Han, kana,
Hangul, shared punctuation, and every emoji-presentation atom are untouched by
everything in this section.

Bare U+1F6E0 in ordinary application output is the motivating case. It is
text-default, so the emoji slot never captures it, and it reaches the primary or
wide slot where nothing is obliged to supply a pictograph.

### `faceNameEmojiText` precedence

The resource is a two-entry slot chain under the inherited grammar, unset by
default. It is a **resolve-stage rescue, never a capture slot**: it cannot take
over primary text, and an atom it fails to serve continues down the same order
it would have taken if the resource were unset.

For a text-emoji atom, it is consulted at position 4 above, which is:

- **after** every existing explicit text choice for the resolving slot — entry 1,
  entry 2, and `fallbackFace1` … `fallbackFace16` — so a user's existing chain
  keeps precedence and a face that already serves the atom continues to;
- **before** unnamed slot-seeded system candidates, matching the rule that an
  explicit choice outranks automatic discovery everywhere else in this document;
- **before** that slot's deterministic tofu.

It is consulted in the primary and wide resolve stages, which are the two that
serve text presentation, and never in the emoji or Han stages. Consulting it in
the wide stage is a third new-resource-gated cascade, listed with the other two;
it does not weaken WD-01, which forbids a configured wide slot from re-capturing
at primary.

The role has no numbered fallbacks and no system seeding of its own, so it adds
no second budget. `limitFontsets: 0` suppresses **both** of its entries: unlike
`faceNameEmoji` and `faceNameHan`, this role does not capture atoms, it rescues
them after the capturing slot's own choices are exhausted, so the value that
permits nothing beyond entry 1 of the capturing slot must permit nothing here
either. Entry 2 is an ordinary candidate and consumes the glyph-bearing-open
budget when it is opened.

### Presentation-aware matching

The monochrome requirement is communicated **before** Fontconfig substitution:
`FC_COLOR` is added as false to the request unless the configured pattern already
constrains color, so a system or user rule that appends `color=true` to a
generic `emoji` request cannot silently redirect the role to a color face. The
user's pattern wins when it is explicit; Revenant never rewrites global
Fontconfig policy.

A family name remains a preference, not an identity. Acceptance is unchanged:
required cmap coverage, exact UVS, shaping without dropped selectors or
mid-cluster `.notdef`, and actual ink under the current paint policy. Because a
text-emoji atom paints with color disabled, a color-only glyph in a face that
also carries outlines is rejected here exactly as it is anywhere else. The
effective file and index are reported, so a family that resolved elsewhere is
visible rather than assumed.

### Presentation-aware discovery

`FcFontSort` is asked twice for a slot's system candidates, and the two results
are used differently.

The first sort is unchanged: the slot-seeded request, coverage-trimmed, stored in
order until the inventory bound is reached. The second states the monochrome
preference — `FC_COLOR` deleted and re-added as false — and is **retained rather
than drained**. Nothing is stored from it up front.

The second sort exists because trimming answers a coverage question, not a
presentation one. When a color face covers the same scalars, the trimmed sort
drops the monochrome face as redundant and it never reaches the inventory at all,
so a text-presentation atom has nothing to route to however large the activation
budget is. Measured in the `routing` fixture universe: the trimmed sort omits
`Noto Emoji` entirely, the untrimmed sort has it seventh, and the
monochrome-preferring sort has it fifth, ahead of the color face. Shared
character coverage does not imply interchangeable presentation.

It is retained rather than drained because a prefix of it would only exchange one
blind cutoff for another. Unrelated monochrome faces sort first in plenty of
environments, and they would fill a reserve before the needed face was reached.
Instead, when every stored candidate has missed a particular atom, the sort is
scanned for the first non-color face whose charset covers **that atom's** required
scalars; it is appended and tried, and if it also misses, the scan resumes where
it stopped.

The scan position belongs to **one atom's search**. Each unresolved atom starts at
the beginning of the retained sort and carries its own cursor across its own
retries. A shared cursor would be a correctness bug, not an optimization: a face
that does not cover the atom being searched for would be stepped over
permanently, and every later atom that face does cover would be denied it.

Repeating the scan per atom is bounded work. A face that covers an atom is
appended once and found by the ordinary walk afterwards, so a full rescan only
happens for an atom nothing covers — and that atom's tofu route is cached, so it
does not rescan on repaint either.

Three bounds, deliberately distinct:

- `XTP_XFT_FALLBACK_CAPACITY`, 32, bounds **enumeration** of ordinary candidates
  per (slot, style).
- `XTP_XFT_PRESENTATION_RESERVE`, 8, bounds how many candidates the per-atom scan
  may append, counted in `presentation_counts` and enforced, not merely implied
  by the array size. An atom nothing covers appends nothing, so it cannot spend
  the reserve on behalf of later atoms. They are stored beyond the ordinary bound, so recovering an
  alternative never costs an ordinary candidate its place.
- `limitFontsets`, default 50, budgets **activation**: how many glyph-bearing
  faces may be opened. Raising it cannot recover a candidate enumeration
  excluded, which is why the inventory needed its own answer.

Nothing else moves. The scan runs only where system candidates already ran, so
`systemFallback: false` still suppresses it, `limitFontsets: 0` still exits before
it, explicit entry 1, entry 2 and numbered fallbacks keep their precedence, and
the Han and wide routes are untouched: a discovered candidate still has to satisfy
coverage, presentation and ink for the atom in front of it. A candidate the scan
supplied is reported as `source=monochrome` when it activates, and the scan logs
how far it went, against both the sort length and the reserve.

Discovery remains bounded work over a Fontconfig-ordered list. It is not a
guarantee that an installed face will always be found; `faceNameEmojiText` is the
lever that does not depend on discovery.

### Automatic color-emoji discovery

A role with no configured face name has no system-candidate seed, because the
seed is built from that name. With `faceNameEmoji` unset, the emoji branch
therefore had nothing to discover and an emoji-presentation atom could reach tofu
with a usable color face installed. `faceNameDoublesize` masked this whenever it
happened to be set: the wide role's seed served the emoji branch on its behalf.

The emoji role's normal-style request is now seeded even when the role is unnamed:

- The seed is built from **entry 1 of the primary chain**, the one face guaranteed
  to exist, with `FC_COLOR` stated as true so the sort prefers color faces. It is
  the mirror of the monochrome sort above, and for the same reason: presentation
  has to be requested, not hoped for.
- It is prepared only when `faceNameEmoji` is unset. A named emoji role keeps its
  own seed, built from its own name, exactly as before.
- It is prepared only when `systemFallback` is true and `limitFontsets` is
  nonzero, so both controls still suppress it.
- Routing coverage is decided through the normal instance, so only that style is
  seeded.

**Where the seeded candidates are consulted matters, and it is not where a
configured role's are.** A configured emoji role keeps its system candidates
inside the emoji branch, as before. The unnamed role's seeded candidates are
automatic, so they are consulted in the primary slot's tail instead: after entry
1, entry 2 and `fallbackFace1` … `fallbackFace16`, after the `faceNameEmojiText`
rescue, and before the presentation-neutral primary sort. Consulting them any
earlier would let a discovered color font override a usable explicit choice,
which is the one thing automatic discovery must never do.

Before the primary sort rather than after it, because that sort states no
presentation and would answer a color-emoji atom with the first covering face —
in the fixtures, a monochrome one.

The seed earns its place by ordering, not by reachability. With neither slot
configured the atom already fell through to the primary slot's candidates, but
that sort states no presentation, so it answered a color-emoji atom with the
first covering face — in the fixtures, a monochrome one. A color-preferring seed
is what makes the emoji slot answer with color artwork.

**The budget is shared, not duplicated.** Each fallback set counts activations
separately, so a set of its own would have handed the default configuration a
second allowance of `limitFontsets`. The unnamed emoji role's activations
therefore count against the primary role's counter through
`shared_activations`. A configured emoji role keeps its own budget, exactly as
before. T0's `LM-02` pins this: with `limitFontsets: 1`, the single allowance is
spent on the first atom and the emoji atom gets no fallback at all.

This is deliberately **not** extended to the wide role. An unset
`faceNameDoublesize` still seeds nothing, so wide-text and CJK discovery are
untouched: emoji presentation is the demonstrated requirement, and widening
unnamed discovery to the wide role would change Han and kana routing without one.

One inherited rule had to be scoped for this to work at all. The fallback advance
rule refuses a candidate whose glyphs are wider than the committed cells, and
every color emoji is about two cells wide by design, so a width-one color atom
was refused however it was discovered. The rule now follows one of three policies
per atom: `STRICT` is the inherited behavior; `FIT` adds span fitting for
text-emoji atoms; and `EMOJI` exempts a candidate that will paint as **color
artwork**, because the paint path already scales that into the committed cells,
exactly as it does for a configured emoji role. A monochrome candidate under
`EMOJI` is still refused, since an outline glyph is drawn at its normalized size
and would overflow. `limitFontWidth` itself is unchanged.

T0's `LM-03` records the one projected outcome that moves: the same request
resolves the same file within the same budget, but the serving role is
`emoji-fallback` rather than the primary slot's `fallback`. The oracle-deposed
facts — file, index, activation count — are unchanged; only Revenant's own role
label differs, because an emoji-presentation atom is now served by the emoji
slot. The xterm deposition in `compat/xterm-411-face-name.json` is untouched.

Acceptance is unchanged, which is what keeps `colorGlyphs: false` honest. A
discovered candidate must still cover the atom and produce ink under the current
paint policy, so declining color still refuses a color-only glyph and still ends
in deterministic tofu rather than leaking color.

### What presentation does and does not decide

Presentation orders candidates and governs paint. It does **not** make a
monochrome face ineligible for an emoji-presentation atom, and no blanket
rejection is implemented. The observable policy, with no color face installed at
all, follows from the advance rule rather than from presentation:

| Atom | Outcome |
| --- | --- |
| One cell (bare VS16 under the legacy width regime) | The monochrome face's glyphs are about two cells wide, the advance rule refuses them, and the atom is deterministic tofu |
| Two cells (an emoji-default base, or VS16 under mode 2027) | The advance rule does not apply, so the monochrome face covers the atom, produces ink, and serves it |

So a wide emoji atom can render in monochrome when nothing colored is installed.
That is a coverage outcome, not a presentation swap: the color-preferring seed
had nothing color to offer, and the ordinary acceptance rules then applied. Both
rows are asserted in `tests/xvfb-font-discovery.sh`; neither is a general claim
about presentation.

### Span fitting

`fitEmojiText` defaults to true and governs one narrow behavior. For a
text-emoji atom only, when a face has already satisfied coverage, shaping, and
ink, and would be refused **solely** because its advance exceeds the atom's
committed span, a fitted instance of that same face is opened once at a size
scaled uniformly by `span / advance`, and the atom is validated again in it. If
the fitted instance satisfies the ordinary advance rule it serves the atom;
otherwise the face is refused exactly as before.

- The scale is uniform on both axes, so artwork is not distorted, and it only
  ever shrinks.
- It applies at both consult points that can serve a text-emoji atom with an
  oversized face: the `faceNameEmojiText` role and a fallback candidate that
  would otherwise be advance-deferred. Stating both is deliberate: a role face
  is not advance-checked at all today, so an explicit monochrome choice would
  otherwise overflow its cell.
- `limitFontWidth` is **not** relaxed, reinterpreted, or bypassed. It still
  governs every other atom class, and the fitted instance must satisfy it.
- Ink stays inside the atom's assigned cells. Borrowing a neighboring cell's
  whitespace is not part of this policy.
- Committed width, the primary metrics authority, cell geometry, and every other
  role's faces are untouched. Only the instance serving this atom is scaled.
- The scale is taken from the face's **maximum advance**, not from the advance of
  whichever atom asked first, so `(source face, span)` is a complete key: two
  atoms with different advances in one face share one instance, and what a later
  atom gets does not depend on the order atoms arrived in. The atom's advance is
  not an input to the scale at all, which is why order dependence is excluded by
  construction rather than by a test. A face whose widest glyph is far wider than
  the atom's is therefore shrunk more than strictly necessary; the atom's own
  advance is still validated in the fitted instance, so this costs size, never
  correctness.
- Because the advance rule is a one-cell rule, only one-cell atoms are ever
  fitted. A width-two atom bypasses the rule and is served unfitted. A second
  span therefore appears only when the cell size itself changes, as it does on a
  font-slot switch.
- Fitted instances are **never evicted or closed while the universe lives**,
  because the route cache and the glyph-ink cache hold these pointers. The table
  is bounded instead; exhaustion refuses to fit further atoms, warns once, and is
  cleared only by the transactional reload that replaces the whole universe.
- Style selection cannot undo fitting. A bold or italic candidate of the same
  family must satisfy the same span, being fitted itself if necessary; when it
  cannot, it is declined and the fitted normal face is retained, so SGR never
  widens an atom past its cells.
- With `fitEmojiText: false` a fallback candidate is refused on advance exactly
  as it is today, which keeps the pre-existing behavior available and testable.
  A `faceNameEmojiText` face is refused on the same terms, because no role may
  paint outside its atom's cells: an explicit choice cannot buy a neighbor's
  cell, and the refusal is logged as a deferral with the measured advance.

The routing report schema is unchanged. This role is not a capture slot, so a
route it serves reports the capturing slot that owned the atom, with rung
`entry1` or `entry2`; no rung, miss, or slot enum gains a value.

`tests/xvfb-emoji-artwork.sh` is the acceptance gate for all of the above.

## Support validation

Fontconfig charset matching is a prefilter, not acceptance. Split the atom into:

- coverage-required scalars: bases, marks, and ordinary components, which must
  map through ordinary cmap coverage;
- sequence controls: variation selectors, ZWJ, tags, and relevant default
  ignorables, which are validated through cmap 14 or whole-sequence shaping
  rather than required as ordinary cmap entries.

A role supports an atom only when required cmap mappings exist, exact UVS
requirements hold, shaping produces usable glyphs without dropped selectors or
mid-cluster `.notdef`, and the selected renderer can produce ink under the
current color/presentation policy. Coverage alone is insufficient.

## Roles and styles

A role is a family-level object with up to four real instances: normal, bold,
italic, and bold-italic. Each instance identity includes file, collection
index, and variation coordinates. Named variable-font instances are distinct
roles when those signatures differ.

Routing coverage is decided exclusively through the role's normal instance.
After routing, discover a requested real style inside the same family. A real
style must have no synthetic `FC_EMBOLDEN`, no non-identity `FC_MATRIX`, weight
at least `FC_WEIGHT_DEMIBOLD` for bold, and non-roman slant for italic. Missing
styles render with the normal instance; no synthetic emboldening or slant is
allowed in the Xft path.

If `boldFont`, `wideBoldFont`, or a future style resource resolves to a
different family, it is unavailable for that role. Render normal and emit
`FR-STYLEFAMILY` with slot, requested style, selected role family, and resolved
style family. ST-04/05 document stock's different-family behavior; Revenant's
same-family rule is intentional drift because SGR must not change the serving
family.

## Cache

Use a fixed-capacity LRU of at least 8192 entries. Eviction is silent normal
operation. The key is:

```text
(ordered atom codepoints,
 committed width class,
 effective presentation,
 colorGlyphs,
 capturing slot,
 systemFallback,
 slot generation)
```

The full signature distinguishes marks, VS15/VS16, ZWJ/tag sequences, and IVS.
A width-class term is necessary because a mode-2027 change can alter wide-slot
applicability without changing fonts. Generation advances when a complete font
universe is installed initially or by a successful relevant SetValues
transaction, invalidating entries from the previous universe. Font-menu
selection does not advance generation because the active size slot is already
part of the key. Revenant does not watch fontconfig configuration changes;
those reliably take effect after a process restart.

## Metrics and rendering

For non-primary instance `F` and active primary-normal instance `P`:

```text
scale(F) = (ascent(P) + descent(P)) / (ascent(F) + descent(F))
```

Compute this per `(instance, active primary slot, DPI, generation)`. Apply
variation coordinates consistently in Xft, HarfBuzz, and Cairo. Align `F` to
the primary baseline at `ascent(P)`. Cell geometry comes only from `P`, using
xterm's characterized primary calculation including `forceXftHeight`.

Accumulate shaped pen positions in double precision and round each draw
position half-away-from-zero; never round each advance independently. Centre
using shaped advance bounds, not ink bounds.

Run itemization uses resolved role/style, UAX #24 script (with
Common/Inherited resolution), backend bidi direction when present otherwise
HarfBuzz inference, process-locale language, default features, and paint
policy. An unconditional LTR fallback is forbidden. Runs never cross a visual
row, including soft wrap. Shape the complete run, centre it in the combined
committed span, clip ink to that span, and draw cell backgrounds/decorations
separately.

Ink escape is deferred until repaint ordering, cursor restoration, selection
redraw, and partial-Expose neighbor damage form a complete contract.

## Tofu

Tofu is renderer-owned and deterministic, never a font's `.notdef`: draw one
box per committed cell. A two-cell miss therefore shows two boxes. The
blank-is-never-an-outcome rule applies to ink-bearing atoms; spaces, controls,
and intentionally ignorable-only sequences may be blank.

## Transactional reload

If no usable entry-1 primary exists, disable Xft and retain the bitmap path;
without a metrics authority there is no valid TrueType universe.

SetValues builds a complete replacement universe, including roles,
normalization, and caches. Swap and increment generation only when the new
primary succeeds. On failure, keep the old effective universe, retain the new
configured resource value, and emit `FR-RELOADFAIL`. Reporting must expose
that configured/effective divergence.

## Routing report schema

The report is stderr-only NDJSON. Every object contains integer `schema: 1`
and string `type`. Collection is bounded to at least 4096 first-use route
records. Cache eviction and report overflow are independent.

### `load`

Required fields: `slot`, integer `fontslot`, `style`, `entry`, `configured`, `effective`,
`status`, and integer `generation`. `effective` is a nullable role object with
`file`, integer `index`, and `coords` (axis-to-double object). `status` is
`active`, `retained`, or `failed`. After a failed reload, a retained record
contains the new configured text and old effective role. Load records describe
the most recent universe-build attempt rather than accumulating every prior
generation; storage grows as needed instead of saturating at one initial
universe.

### `route`

Required fields: uppercase space-separated codepoints in `atom`,
`presentation` (`text` or `emoji`), integer `widthclass`, capturing `slot`,
integer active `fontslot`, `rung`,
nullable-but-present `file`/`index`/`coords`, and `misses`.

`rung` is `entry1`, `entry2`, literal `fallbackFaceN` (for example
`fallbackFace7`), `system`, or `tofu`. Tofu carries null role fields.
Routing-only miss codes are `cmap`, `uvs`, `shape`, `ink`, `budget`, and
`truncated`. If more than 64 misses occur while resolving one atom, the first
64 remain in `misses` and the route also carries `missesTruncated: true`.

When a requested real style is unavailable, add:

```json
"styleFallback": {"requested": "italic", "served": "normal"}
```

This is informational, not a routing miss.

### Other record types

- `warn`: stable code plus code-specific subjects. Codes include
  `FR-BADPATTERN`, `FR-DUPROLE`, `FR-UVSMISS`, `FR-RELOADFAIL`, and
  `FR-STYLEFAMILY`.
- `bound`: integer `records`, with `code: "FR-REPORTBOUND"` for the fixed route
  ceiling or `code: "FR-LOADBOUND"` if allocation prevents retaining every
  load record from the latest build.
- `snapshot`: integer `generation`, double `dpi`, `collection` as `enabled` or
  `disabled`, and integer `records`. Invoking the action while collection is
  disabled emits exactly one disabled snapshot record.

The explicit `report-font-routing()` Xt action writes a snapshot to stderr.
CI parses types, codes, and fields, never presentation prose.

## Intentional differences from xterm

Each item requires a reasoned drift-ledger entry:

1. fixed cell geometry rather than bold/italic cell enlargement;
2. normal-canonical family routing and same-family style resources rather than
   stock style-specific coverage fallback (ST-01…05);
3. deterministic renderer-owned tofu;
4. normalized fallback metrics rather than stock unnormalized fallback paint.

## Test gates

The complete resolver is gated by T0 through T22 from Revision 5 + Erratum 1.
The essential groups are:

- T0 replay against all 32 patch-411 cases;
- rung order, emoji rescue, interlock, metrics authority, atom atomicity, Han,
  and exact IVS behavior;
- `limitFontsets`, `limitFontHeight`, and source-only `limitFontWidth` tests;
- numbered-resource grammar, role-signature duplicate handling, cache keys and
  invalidation, run shaping/Expose behavior, and bounded performance;
- same-family style behavior and `FR-STYLEFAMILY`;
- wide-stays-in-slot plus new-resource-only recapture;
- `systemFallback: false` versus `limitFontsets: 0`;
- golden parsing of every NDJSON type, null tofu role, `styleFallback`,
  retained reload, and every miss/warn code.

Prefer routing identity, file/index/coordinates, glyph counts, metrics, and
pixel class/bounds over byte-identical raster goldens.

## Implementation status

Landed in the current worktree:

- reusable two-entry parser with the characterized prefix/whitespace rules;
- parser-layer replay of 46 list inputs from the blessed 32-case fixture;
- live Xvfb replay of all 32 deposition cases, comparing resolved file/index,
  primary and wide role/style loads, command/resource precedence, glyph-time
  routing, warnings, and WM cell/base geometry;
- 29 cases remain stock-identical, ST-03 exercises the accepted
  normal-canonical runtime drift, and the two remaining inherited gaps are
  self-invalidating expected failures for DEC double-height handling (LM-04
  and LM-05);
- patch-411 `-fa`/`-fd`/`-fe` precedence over competing `-xrm` bindings and
  default packed printable cell-width metrics;
- entry 2 before system candidates for primary, doublesize, and emoji roles;
- separate explicit/system boundaries and separate fallback sets per role;
- lazy fontconfig candidates with the inherited per-(role, style)
  `limitFontsets` budget: missing candidates remain free, activated candidates
  remain reusable, and LM-01/02/03 now conform;
- partial frame changes are collected and painted in logical row/column order,
  so cursor or damage repaint order cannot decide which glyph consumes a
  fallback budget;
- inherited `limitFontHeight` and `limitFontWidth` defaults and cap-at-50
  diagnostics; the source-characterized one-sided fallback advance check is
  implemented, including glyph-bearing width deferrals consuming
  `limitFontsets`; DEC line-size recognition itself remains blocked on backend
  state described below;
- exact ascent-plus-descent normalization is applied per non-primary instance,
  including semantic roles, real styles, and lazily opened explicit, numbered,
  and system fallbacks; the isolated fixture asserts the applied ratio;
- ordered `fallbackFace1` … `fallbackFace16` roles between entry 2 and system
  candidates, with gaps, complete four-style file/index/variation-coordinate
  identity, keep-first duplicate warnings, lazy opening, and shared
  `limitFontsets` accounting; the isolated integration test proves user order,
  duplicate handling, gap handling, routing identity, and budget consumption;
- `systemFallback` defaults true and, when false, truncates each role/style
  sequence after entry 2 and numbered user fallbacks without changing the
  inherited `limitFontsets` accounting for those named fonts;
- generated Unicode 17.0 Script=Han membership, `faceNameHan` two-entry role
  capture ahead of emoji/wide/primary routing, Script_Extensions exclusion,
  Han-miss recapture, and exact cmap-14 IVS validation; supported and
  unsupported IVS fixture branches prove that selectors are never silently
  dropped, with renderer-owned per-cell tofu on the exact-variant miss;
- deterministic renderer-owned tofu for every ink-bearing all-role miss, one
  outlined box per committed cell; the pixel test covers width-1 and width-2
  misses independently and proves that a space cell remains blank;
- normal-canonical runtime coverage for primary and fallback roles: coverage
  and `limitFontsets` activation use the normal chain, then a requested style
  is accepted only when it is genuine, same-family, and covers the already
  selected atom; the shaping test proves real DejaVu italic and normal
  degradation for a CJK bold-italic request;
- strict configured-wide-slot ownership: a coverage-thin doublesize role ends
  at tofu even when primary could draw the atom, while an unset doublesize
  resource leaves the same atom available to primary capture;
- inherited `boldFont` and `wideBoldFont` Xft chains as post-routing style
  sources: ST-04/05 prove same-family explicit-entry selection, while the
  focused style fixture proves real bold rendering and different-family
  degradation with `FR-STYLEFAMILY`;
- a fixed 8192-entry, globally LRU atom-routing cache keyed by the complete
  UTF-8 atom signature, committed width, effective presentation and policy,
  color policy, active and capturing slots, `systemFallback`, and font-universe
  generation. Cached values are normal-canonical family decisions (including
  tofu), so SGR style remains outside the key and is resolved within the cached
  family on every draw. Unit coverage exercises every discriminator and true
  LRU eviction; the focused Xvfb test proves reuse across normal/bold, distinct
  atom keys, and cached tofu;
- existing font-format, emoji, shaping, italic, and partial-Expose suites remain
  green;
- the routing-report ceiling is exercised directly at 4096 first-use records;
  the next route marks the report bounded without affecting service.

Still required:

- close the two named T0 DEC gaps and remove their expected-failure entries;
  the runner treats an unexpected pass as a failure so exemptions cannot
  survive their implementation;
- DEC double-width recognition, alongside the double-height backend contract.

The bounded schema-1 NDJSON report/action and transactional font-universe
reload are implemented. `xvfb-font-routing-report` covers the report taxonomy
and action isolation; `xvfb-font-reload` covers a successful atomic swap,
single generation advance, failed-primary retention, configured/effective
policy separation, `FR-RELOADFAIL`, and `status: "retained"` records.

LM-04/05 require a backend contract change, not an Xft-only patch. The pinned
libghostty stream currently treats `ESC # 3` and `ESC # 4` as unsupported, its
terminal rows retain no DEC double-height attribute, and the C render API has
no row line-size query. Close that gap by implementing and preserving DECDHL
state in libghostty and exposing it through `GhosttyRenderStateRowData`; do not
infer it from glyph width or rescan PTY bytes in the UI adapter.

Do not update user documentation to claim an unfinished resource works merely
because this document specifies it.
