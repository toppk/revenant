---
man: revenant-font-baseline
section: 7
manual: compat
description: font format baseline and emoji renderer contract
---

# Font format baseline and emoji renderer contract

The original baseline commit captured Revenant's pre-routing Xft behavior. The
same executable contract now separates two questions that otherwise look
identical on screen:

1. What happens when a fixture is selected directly as `faceName`?
2. Does `faceNameDoublesize` route a wide character to that fixture while
   preserving the terminal engine's committed width?

The executable contract is `tests/xvfb-font-baseline.sh`. It runs under Xvfb
with isolated Fontconfig universes, a black background, and white foreground.
For each wide probe it asserts a cursor-position report at row 1, column 3,
classifies every pixel in the committed two-cell rectangle, and proves the
third cell contains no ink.

## Current direct format behavior

These results use Xft 2.3.8, Cairo 1.18, and the fonts pinned by
`tools/font-fixtures/manifest.json`:

| Fixture path | U+1F600 result |
|---|---|
| CBDT at 109 ppem | color |
| Historical Noto 2.034 CBDT at 109 ppem | color |
| CBDT at 61 ppem | color |
| COLRv0 + genuine outline base | color |
| COLRv1 + empty outline base | color |
| Monochrome outline | foreground monochrome |
| SVGinOT + genuine outline base | color |
| sbix at 16 ppem + empty outline base | color |

Noto Sans Mono CJK JP renders U+65E5 in foreground monochrome with the same
two-cell geometry assertion.

Cairo 1.18 renders the selected color-glyph formats while Xft continues to
render monochrome outlines. The format matrix preserves the already-working
CBDT, COLRv0, and sbix results and deliberately breaks the pre-feature
baseline's passing blank COLRv1 and SVG cells.

The Cairo delegate is persistent rather than constructed per cell. Its
scaled-font cache centres each selected face from its own ascent and descent,
and its draw call receives the effective intersection of the cell, damage,
and cursor clips. The routing test guards partial-damage restoration and a
width-one COLRv1 glyph under a block cursor against neighbouring-cell paint.

The ink taxonomy requires nonzero outline area, not just a `glyf` entry or
contour bounds. OpenMoji's mapped U+1F600 base has a two-point degenerate
contour and therefore records `color`, not `outline+color`; TwitterColorEmoji
has a genuine outline and records `outline+svg`.

## `faceNameDoublesize` boundary

Holding `faceName` at DejaVu Sans Mono while varying `faceNameDoublesize`
exercises every color technology, monochrome outlines, and CJK CFF outlines.
All fixtures now render through the role, remain inside two cells, report the
cursor at column 3, and leave the third cell blank. Fixed strikes are fitted in
both directions: CBDT 61/109 ppem downscale and sbix 16 ppem upscale.

`tests/xvfb-emoji-routing.sh` adds Unicode defaults, VS15/VS16, policy
overrides, emoji-to-doublesize fall-through, CJK isolation, one-cell width
preservation, genuine outline fallback, and empty/degenerate-base rejection.
It also requires single-run shaping for keycap, skin-tone, ZWJ, family,
regional-indicator, subdivision-tag, adjacent-flag, and combining sequences,
plus atomic whole-grapheme role fallback. Current Noto paints the tested family
sequence through COLRv1 with an achromatic gray palette, so the pixel probe
correctly reports `mono` even though routing and rendering still use the color
font path.

The historical Noto Emoji 2.034 fixture is byte-identical to the font embedded
by Ghostty tags 1.2.1, 1.3.0, and 1.3.1. It preserves the earlier colorful
family artwork from before Google's Emoji 15.1 redesign and, because it
predates U+1FAE8, supplies a realistic color-face miss for the emoji
fall-through test. It complements rather than replaces the current pinned Noto
CBDT fixture.

The combining case then redraws ordinary primary-face text and requires normal
vertical cell margins. This guards the ownership boundary between HarfBuzz and
Xft: shaping uses an independent OpenType face and must not change Xft's live
FreeType face state.
Its policy adversaries put a color emoji font in the doublesize role under
`emojiPresentation: text`, and put an outline-less CBDT font in the primary
role under `colorGlyphs: false`; neither case may leak color.

## General shaping and fallback

`tests/xvfb-text-shaping.sh` extends the isolated fixture contract beyond
emoji. DejaVu Sans Mono supplies regular, bold, oblique, and bold-oblique
faces; Noto Sans Devanagari and Noto Sans Mono CJK JP are available only
through fontconfig fallback. The test requires positioned Vietnamese and
Zalgo runs, a real oblique SGR-italic face, a two-backend-grapheme Devanagari
conjunct shaped into one glyph across its unchanged two-cell span, and a mixed
CJK/combining-text fallback run. A partial Expose beginning in the conjunct's
second cell must reconstruct the same complete shaped run.

## Unicode version interlock

The fixture routing input and selected libghostty `uucode` width input both
declare Unicode 17.0. `tools/generate-emoji-table --check` rejects a mismatch
between either input, the fixture manifest, and the generated C table.
`font-fixture-info.py --check` independently checks maintained fixture data.
