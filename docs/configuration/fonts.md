# Fonts and font fallback

Revenant has two font renderers. The traditional Xlib renderer uses xterm's
bitmap-font resources. The Xft renderer uses fontconfig patterns, HarfBuzz
shaping, role-specific fonts for wide text and emoji, and bounded fallback.

This page is the user guide: it explains what to put in `.Xresources`, what
order fonts are tried in, and which parts of the expanded resolver are still
being implemented. The complete engineering contract is the
[font-resolution specification](../maintainers/font-resolution.md).

## A practical configuration

This is a good starting point for a system with the named fonts installed:

```xrdb
! These lines also affect stock xterm because Revenant currently shares its
! XTerm application class and vt100 widget identity.
XTerm*vt100.renderFont: true
XTerm*vt100.faceName: Iosevka Term:size=11,Noto Sans Mono CJK JP
XTerm*vt100.faceNameDoublesize: Noto Sans Mono CJK JP
XTerm*vt100.faceNameEmoji: Noto Color Emoji
XTerm*vt100.faceNameHan: Noto Sans Mono CJK JP
XTerm*vt100.fallbackFace1: Noto Sans Devanagari
XTerm*vt100.fallbackFace2: Noto Sans Hebrew
XTerm*vt100.limitFontWidth: 50
XTerm*vt100.emojiPresentation: unicode
XTerm*vt100.colorGlyphs: true
```

Load the file and start a new terminal:

```sh
xrdb -merge ~/.Xresources
revenant
```

Check fontconfig's interpretation independently:

```sh
fc-match 'Iosevka Term:size=11'
fc-match 'Noto Sans Mono CJK JP'
fc-match 'Noto Color Emoji'
```

## The grid has one authority

The first applicable entry in `faceName` defines the cell width, cell height,
ascent, and baseline. Wide, emoji, and fallback fonts contribute glyphs; they
do not resize the grid. The terminal backend commits every cluster's cell
width before font selection begins, and routing never changes that width.

That rule prevents a missing font, an emoji format, or a CJK fallback from
moving the cursor differently. It also means a fallback font with very
different metrics may be scaled, centred, or clipped to the cells already
committed for it.

## Xft and bitmap modes

Both renderers are supported. The expanded Revision 5 font resolver applies
only to the Xft path; it does not replace or deprecate the traditional bitmap
path. If an existing xterm bitmap-font configuration already does everything
you need, Revenant is intended to keep using it without requiring an Xft font
configuration.

```xrdb
XTerm*vt100.renderFont: true
```

enables the Xft/fontconfig renderer. These resources then apply:

| Resource | Purpose |
| --- | --- |
| `faceName` | Primary text and the sole cell-metrics authority |
| `faceNameDoublesize` | Wide text and the historical emoji-rescue role |
| `faceNameEmoji` | Emoji-presentation glyphs |
| `faceNameHan` | Text-presentation characters whose base has Script=Han |
| `fallbackFace1` … `fallbackFace16` | Ordered user fallback roles |
| `faceSize`, `faceSize1` … `faceSize7` | Point sizes for font-menu slots |
| `limitFontsets` | Inherited fallback-open budget; `0` disables all fallback |
| `limitFontHeight` | DEC double-height recognition tolerance |
| `limitFontWidth` | Fallback advance and DEC double-width tolerance |
| `systemFallback` | Permit unnamed fontconfig candidates after named fonts |
| `emojiPresentation` | `unicode`, `text`, or `emoji` presentation policy |
| `colorGlyphs` | Permit or decline color-glyph paint |
| `graphemeWidth` | Initial `legacy` or `unicode` mode-2027 policy |
| `reportFontRouting` | Collect bounded records for an on-demand routing snapshot |

```xrdb
XTerm*vt100.renderFont: false
```

keeps xterm's Xlib bitmap path. It uses the traditional `font`, `font1` …
`font7`, `wideFont`, `wideBoldFont`, and related bitmap resources. Xft role,
shaping, emoji, and fallback resources do not affect it.

A complete minimal bitmap configuration can remain as simple as:

```xrdb
XTerm*vt100.renderFont: false
XTerm*vt100.font: fixed
XTerm*vt100.font5: 9x15
```

XLFDs and aliases from `xlsfonts` work as they do in xterm. Font-menu size
selection and the runtime **TrueType Fonts** toggle continue to work in both
directions. Switching back to bitmap mode restores the configured bitmap font
slot; it does not translate an Xft face into a bitmap font.

## A face resource is a two-entry chain

Patch-411 xterm does not treat `faceName` as one opaque fontconfig pattern.
It uses a comma-separated, prefix-aware list with at most two applicable Xft
entries. Revenant reproduces that grammar for `faceName`,
`faceNameDoublesize`, `faceNameEmoji`, and `faceNameHan`:

```xrdb
XTerm*vt100.faceName: Primary Mono:size=11,Secondary Symbols
```

- Empty entries and surrounding whitespace are ignored.
- `xft:` explicitly marks an Xft entry and is removed before fontconfig sees
  the pattern.
- `x:` or `xlfd:` marks a core X font and is skipped by the Xft renderer.
- `x11:` is **not** another spelling of `x:`. It remains literal request text.
- Entry 1 is the role's primary request. For `faceName`, it also defines cell
  geometry.
- Entry 2 is explicit glyph fallback and is tried before unnamed fontconfig
  candidates.
- Entry 3 and later are discarded with a warning.

Consequently, this is not a general comma-separated fallback list. To add one
explicit fallback inside a role, use entry 2. For additional fonts, use the
numbered resources. Each value is one complete fontconfig pattern, so commas
inside that pattern are not treated as a fallback-list separator:

```xrdb
XTerm*vt100.fallbackFace1: Noto Sans Devanagari
XTerm*vt100.fallbackFace2: Noto Sans Hebrew
XTerm*vt100.fallbackFace4: Symbola
```

The numbers set the order; gaps are harmless. The first resolved role wins
when two entries resolve to the same normal/bold/italic/bold-italic family
role. These named fallbacks are tried after a captured role's entry 2 and
before unnamed fontconfig candidates. A named font only spends the inherited
`limitFontsets` budget after it successfully supplies a cluster; a font that
does not cover the cluster is free.

`limitFontWidth` defaults to 10 percent. A fallback glyph whose advance is
wider than a one-cell atom by that tolerance is deferred in favor of another
candidate, because the terminal will not widen the grid to accommodate it.
The candidate still consumes `limitFontsets` once it has supplied the glyph.
If an intentionally selected proportional script face is being rejected, the
largest compatible setting is:

```xrdb
XTerm*vt100.limitFontWidth: 50
```

Revenant vertically normalizes each non-primary role and fallback from its
ascent-plus-descent ratio against the active primary face. This does not alter
cell geometry; horizontal ink remains centered or clipped inside the committed
span.

To use only fonts you named while retaining entry 2 and the numbered chain:

```xrdb
XTerm*vt100.systemFallback: false
```

This is intentionally different from `limitFontsets: 0`, which disables the
whole fallback sequence, including entry 2 and every `fallbackFaceN`.

Do not put a fontconfig pattern that needs an internal comma in one of the
inherited two-entry role resources; xterm's list grammar will split it. Use a
numbered `fallbackFaceN` for such a pattern.

The `-fa`, `-fd`, and `-fe` command-line forms accept the same values:

```sh
revenant -fa 'Iosevka Term:size=11,Noto Sans Mono CJK JP' \
  -fd 'Noto Sans Mono CJK JP' \
  -fe 'Noto Color Emoji'
```

An embedded `size=` in entry 1 of `faceName` takes precedence over
`faceSize`. Prefer setting the size in one place.

## Which role serves a cluster

Font routing operates on one backend-provided cluster at a time: a base,
marks, selectors, ZWJ members, and tag characters stay together. Adjacent
clusters that select compatible faces are then shaped together so contextual
scripts are not reduced to per-cell shaping.

The resolver first captures a role and then resolves only inside that role:

1. Han override, when configured and applicable.
2. Emoji role for effective emoji presentation.
3. Doublesize role for wide text, and as the historical emoji rescue.
4. Primary role otherwise.

Inside the captured role, the order is entry 1, entry 2, numbered
user fallbacks, system fallback, then deterministic tofu. A wide atom captured
by a configured doublesize role does not silently switch to the primary role
when that role misses.

`faceNameHan` uses the base character's Unicode `Script=Han` value—not
Script_Extensions. Kana, Hangul, shared CJK punctuation, and fullwidth forms
therefore remain in the doublesize role. For visually coherent Japanese or
Korean text, normally point `faceNameHan` and `faceNameDoublesize` at the same
family. Ideographic variation selectors remain attached to the Han atom and
must have an exact cmap-14 mapping; Revenant draws visible per-cell tofu rather
than silently substituting the unvaried ideograph when every role misses.

### Current rollout status

The resolver described in this section is an expansion of the Xft path. Bitmap
mode is already supported and is deliberately outside this rollout: its font
resources and renderer remain governed by the traditional xterm contract.

For the Xft resolver, today:

- the characterized two-entry grammar is implemented and replayed against the
  blessed patch-411 fixture;
- entry 2 precedes system candidates for primary, doublesize, and emoji roles;
- inherited `limitFontsets` budgets are enforced lazily per role/style: `0`
  disables all fallback, while a positive value is consumed only when a
  candidate actually serves a glyph;
- `limitFontWidth` enforces xterm's source-characterized, one-sided fallback
  advance tolerance; width-deferred glyph candidates consume the same budget;
- semantic roles, styles, explicit/user fallbacks, and system fallbacks use
  the exact primary-height/face-height normalization ratio;
- emoji rescue, general HarfBuzz shaping, color glyphs, and system fallback
  remain covered by the existing fixture matrix;
- `fallbackFace1` … `fallbackFace16` are implemented as ordered, single-pattern
  user fallbacks after entry 2 and before unnamed system candidates; gaps are
  allowed, duplicate resolved roles keep the first entry, and their successful
  glyph-bearing opens consume the inherited `limitFontsets` budget;
- `systemFallback` is implemented as the unnamed-candidate switch; entry 2 and
  numbered fallbacks remain active when it is false;
- `faceNameHan` is implemented from the pinned Unicode 17.0 Script table,
  including kana/shared-punctuation exclusion, miss recapture, and exact IVS
  validation;
- deterministic tofu is implemented for every ink-bearing all-role miss: one
  renderer-owned box per committed cell, while spaces, controls, and
  default-ignorable-only atoms remain blank;
- routing coverage is normal-canonical for primary, semantic, explicit,
  numbered, and system roles: SGR cannot change the selected family, and a
  requested bold/italic instance is used only when it is a genuine same-family
  style that covers the already-routed atom;
- explicit `xft:` entries in inherited `boldFont` and `wideBoldFont` chains
  can supply bold instances after routing; a different-family result degrades
  to normal and reports `FR-STYLEFAMILY` rather than changing the family;
- wide text captured by a configured doublesize role now stays in that role
  through its final miss and renders deterministic tofu; clearing the resource
  leaves the wide atom available for primary capture;
- atom-family decisions, including tofu, use a bounded 8192-entry LRU cache;
  requested style remains outside the key and cannot change the family;
- `reportFontRouting`, `-report-font-routing`, and the
  `report-font-routing()` action implement bounded schema-1 NDJSON snapshots,
  including a tested 4096-route `FR-REPORTBOUND` ceiling;
- font-related SetValues updates are transactional: Revenant builds a complete
  replacement universe, swaps it and advances generation only after the new
  primary succeeds, and otherwise retains the prior effective fonts and policy
  while reporting the new configured values with `FR-RELOADFAIL`.

`revenant -report-config` is the authority for whether a resource in your
installed build is supported, accepted but ignored, or unsupported.

## Emoji presentation and color

VS15 requests text presentation and VS16 requests emoji presentation. Without
a selector, `emojiPresentation` uses Unicode defaults or forces text/emoji:

```xrdb
XTerm*vt100.emojiPresentation: unicode
XTerm*vt100.colorGlyphs: true
```

Complete emoji sequences are routed atomically. A partially covered keycap,
modifier sequence, flag, tag sequence, or ZWJ sequence is retried as a whole;
marks and controls are not split across faces.

With `colorGlyphs: false`, CBDT, sbix, COLR, and SVG paint is declined. A real
outline can still render in the foreground; an empty or degenerate outline is
a miss and routing continues. Cairo 1.18 supplies color formats Xft cannot
paint, including COLRv1 and SVGinOT.

## Bold and italic

Revenant loads real bold, italic/oblique, and bold-italic instances when the
selected family provides them. It does not synthesize slant in the Xft path.
The resolver's family choice is normal-canonical: applying SGR bold or
italic will not change which family serves a cluster. If that family lacks the
requested real style, its normal instance serves.

To provide inherited Xft bold chains explicitly, mark their entries with the
`xft:` prefix:

```xrdb
XTerm*vt100.boldFont: xft:Primary Mono,xft:Secondary Symbols
XTerm*vt100.wideBoldFont: xft:CJK Mono
```

Only an entry matching the already-routed family can render. A
different-family result produces `FR-STYLEFAMILY` and normal serves instead.
Unprefixed values are not reinterpreted as Xft; `renderFont: false` keeps the
existing bitmap renderer behavior.

## Width and shaping

`graphemeWidth: legacy` is the default and leaves DEC private mode 2027 off,
preserving wcwidth-oriented application arithmetic. `graphemeWidth: unicode`
enables cluster-aware width initially and after reset. Applications may still
negotiate the mode explicitly.

HarfBuzz shapes complete clusters and compatible adjacent text runs. The
renderer centres shaped advance bounds in the combined committed cells and
clips ink to that combined span. Shaping never changes cursor arithmetic,
history, selection, or backend-owned width.

The fallback-advance part of `limitFontWidth` is active. Its historical DEC
double-width role, and `limitFontHeight`'s DEC double-height role, still await
line-size state from the terminal backend; ordinary fixed-grid text does not
depend on those two pending DEC paths.

## Diagnosing selection

Start with the static configuration report and human-readable debug log:

```sh
revenant -report-config
revenant -log debug 2>revenant-font.log
```

The configuration report shows merged resources, point sizes, menu-slot order,
and fontconfig's primary match. Debug logs show loaded roles, explicit fallback
loads, and routing decisions.

For a stable machine-readable snapshot, enable collection and bind the Xt
action to a key:

```sh
revenant -report-font-routing \
  -xrm 'XTerm*vt100.translations: #override <Key>F12: report-font-routing()' \
  2>revenant-font-routing.ndjson
```

Press F12 after displaying the text you want to inspect. The action writes a
schema-1 NDJSON snapshot to standard error: resolved slot entries, stable
warnings, one first-use record per distinct routed atom, an overflow marker if
the 4096-record collection bound was reached, and a final snapshot record.
Collection is opt-in because it retains diagnostic state and routes the
normally batched one-byte text path individually so its decisions are
observable. It does not alter cell widths or font priority.

The report is a snapshot, not an exit hook; signal termination cannot be
trusted to flush diagnostics. Invoking `report-font-routing()` without
enabling collection emits one `collection: "disabled"` snapshot record, which
helps diagnose a forgotten option or resource. See
[Diagnostics](../reference/diagnostics.md#font-routing-snapshots) for the
record fields and warnings.

For standalone shaping and font-table inspection, see
[Diagnostics](../reference/diagnostics.md#inspecting-one-font-with-harfbuzz)
and the [font format baseline](../compatibility/font-format-baseline.md).
