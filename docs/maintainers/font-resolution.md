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
| `faceNameHan` | unset | Han glyph-form slot chain |
| `fallbackFace1` … `fallbackFace16` | unset | Ordered single-pattern user fallback roles |
| `systemFallback` | true | Permit unnamed slot-seeded fontconfig candidates |
| `colorGlyphs` | true | Permit color paint at any role/rung |
| `emojiPresentation` | `unicode` | Unicode, forced-text, or forced-emoji policy |
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

Two new-resource-gated cascades are permitted:

- a configured emoji slot that misses re-captures at doublesize, preserving
  the historical emoji rescue;
- a configured Han slot that misses re-captures at the slot that would have
  captured the atom without `faceNameHan`.

A configured wide-text slot does not re-capture at primary after a miss
(WD-01). This boundary is implemented together with deterministic tofu so a
missing glyph cannot become either an accidental blank or a cross-slot rescue.

### Phase 2: resolve within the captured slot

The order is:

1. entry 1;
2. entry 2;
3. `fallbackFace1` … `fallbackFace16`;
4. slot-seeded `FcFontSort` candidates when `systemFallback` is true and the
   glyph-bearing-open budget remains;
5. deterministic tofu.

`systemFallback: false` truncates only before unnamed system candidates.
Named entry 2 and numbered fallbacks remain. This is intentionally different
from `limitFontsets: 0`, which permits nothing beyond entry 1.

The stock-equality claim applies to the normal style only. Bold, italic, and
bold-italic coverage is normal-canonical by intentional drift.

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
