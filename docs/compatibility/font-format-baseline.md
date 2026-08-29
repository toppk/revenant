# Font format baseline

This is the pre-emoji-routing characterization of Revenant's Xft renderer. It
separates two questions that otherwise look identical on screen:

1. What happens when a fixture is selected directly as `faceName`?
2. Does the currently accepted `faceNameDoublesize` resource route a wide
   character to that fixture?

The executable contract is `tests/xvfb-font-baseline.sh`. It runs under Xvfb
with isolated Fontconfig universes, a black background, and white foreground.
For each wide probe it asserts a cursor-position report at row 1, column 3,
classifies every pixel in the committed two-cell rectangle, and proves the
third cell contains no ink.

## Direct Xft format behavior

These results were recorded before renderer changes, using Xft 2.3.8 and the
fonts pinned by `tools/font-fixtures/manifest.json`:

| Fixture path | U+1F600 result |
|---|---|
| CBDT at 109 ppem | color |
| CBDT at 61 ppem | color |
| COLRv0 + genuine outline base | color |
| COLRv1 + empty outline base | blank |
| Monochrome outline | foreground monochrome |
| SVGinOT + genuine outline base | blank |
| sbix at 16 ppem + empty outline base | color |

Noto Sans Mono CJK JP renders U+65E5 in foreground monochrome with the same
two-cell geometry assertion.

Two rows differ from the initial design assumptions. Current Xft renders the
OpenMoji COLRv0 layers in color. Conversely, merely having real fallback
contours does not make TwitterColorEmoji's SVGinOT glyph render: the current
Xft request is blank rather than falling back to those contours. The manifest
and baseline intentionally preserve both sides of that distinction.

## `faceNameDoublesize` boundary

Revenant currently accepts and reports `faceNameDoublesize` but documents it
as unsupported. Holding `faceName` at DejaVu Sans Mono while varying the
doublesize request across every fixture produces the exact same missing-glyph
pixel summary. U+65E5 with the CJK doublesize request does too. Width remains
two cells because libghostty commits grid width independently of font output.

This is a known compatibility gap, not the desired old-way behavior. Emoji
routing work may deliberately change these cells. It must not obscure the
direct primitive baseline above; in particular, the direct COLRv1 blank is the
passing characterization that the interlock and COLRv1 renderer are intended
to break on purpose.

## Unicode version interlock

The fixture routing input and the selected libghostty `uucode` width input both
declare Unicode 17.0. `font-fixture-info.py --check` accepts repeated
`--unicode-data` inputs and rejects any source whose `# Version:` header does
not match the manifest. Generated routing data must retain that assertion
rather than copying the version as an unchecked label.
