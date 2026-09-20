# Emoji artwork and fallback

Emoji support has several independent capability areas. A terminal can advance
its cursor correctly while drawing a missing-glyph box (tofu). A color smiley
does not establish that a text-style tool symbol can be drawn.

Three kinds of evidence appear around these capabilities and none substitutes for
another:

- **measured cursor advance** — what the width probes report from CPR. It is
  mechanical and repeatable, and it says nothing about what was drawn;
- **human-assessed artwork** — what the cases on this page produce. A case stays
  `unassessed` until somebody looks and records a judgment. A clean exit is not a
  pass;
- **automated renderer assertions** — what an emulator's own integration tests
  establish: a non-tofu route, the effective font file, visible ink, bounds and
  containment. Revenant has such a suite (`tests/xvfb-emoji-artwork.sh`). This
  evidence can support a versioned record for the terminal it tests, scoped to the
  behavior it actually asserts, and it is what Revenant's own
  `text-symbol-whitespace-expansion` record rests on. It cannot establish that a
  glyph is legible to a human, and it says nothing about any other terminal.

A record is never promoted because a width probe passed or a command exited
zero.

| Feature slug | What to inspect | Probe case |
| --- | --- | --- |
| `text-monochrome-emoji` | Recognizable text-style emoji, including bare U+1F6E0 and VS15 | `monochrome-emoji` |
| `text-emoji-presentation` | Bare defaults and explicit VS15/VS16 text/emoji choice | `emoji-presentation` |
| `text-color-emoji` | Actual color artwork with color enabled | `emoji-presentation` |
| `text-emoji-cell-fitting` | Legible artwork inside the cells the atom was committed, without clipping or overwriting neighbors | `emoji-cell-fitting` |
| `text-symbol-whitespace-expansion` | Optional: drawing a one-cell symbol larger into an adjacent blank cell | `symbol-whitespace-expansion` |
| `text-emoji-sequences` | Joined artwork for modifiers, ZWJ, flags and keycaps | `emoji-sequences` |

These are rendering capabilities, not new terminal escape protocols. Fitting is
an implementation concern: Unicode prescribes neither a scaling algorithm nor any
use of adjacent cells, so strict in-cell drawing and optional expansion are both
conformant and are recorded as separate capabilities here. See [Unicode emoji](https://www.unicode.org/reports/tr51/)
for presentation and sequence definitions, and [width](width.md) for the
separate cursor contract.

## Run and assess

```sh
just probe text-monochrome-emoji monochrome-emoji --assess
just probe text-emoji-presentation emoji-presentation --assess
just probe text-emoji-cell-fitting emoji-cell-fitting --assess
just probe text-symbol-whitespace-expansion symbol-whitespace-expansion --assess
just probe text-emoji-sequences emoji-sequences --assess
```

Each case labels its samples and expected artwork. Inspect **every** sample;
record the failing label if a row is blank, tofu, clipped, the wrong presentation,
or split into separate components. These cases remain unassessed without a
human observation. They do not identify the font used or automatically prove
that the glyph is recognizable. A successful command exit is not a pass.
Assessment applies to the case; annotate individual failures in the observation.

Record terminal revision, primary and fallback fonts (including versions),
font size, color settings, mode 2027, and relevant Fontconfig customizations.
Repeat under the default installed-font environment and an isolated known-font
environment. These samples are a regression smoke test, not an exhaustive
Unicode coverage claim. Run the existing width scenarios separately.

For fitting, compare spaced and adjacent symbols at multiple sizes. The expected
default is **strict**: recognizable ink inside the cells the terminal committed to
the atom, with nothing clipped and no neighboring cell touched. Inspect right-edge
placement and wrapping separately with the width boundary cases; those cases still
need visual inspection.

## Drawing into adjacent blank cells

`text-symbol-whitespace-expansion` is a separate, optional drawing behavior. No
Unicode or terminal specification describes it; its only reference is one
implementation, which is what the capability cites. In Ghostty, `constraintWidth`
in `src/renderer/cell.zig` allows a symbol-like glyph to be fitted to a two-cell
box when the next cell is blank and the previous glyph was not itself a symbol, so
a spaced row of symbols can look larger than an adjacent row of the same symbols.
What changes is the box the glyph is fitted to, not a fixed multiple of its size,
and the cursor advance is identical either way.

Assess it by comparing the two rows and reporting whether their artwork differs in
size, and roughly by how much. Do not assume a ratio: a wider box grants drawing
room rather than a fixed multiple of glyph size, so "twice as big" is a guess
rather than a contract. The first symbol of an adjacent run may be treated
differently from the ones after it, so compare whole rows rather than one pair.

Equal-sized rows are recorded as **no enlargement observed for these samples** —
not as proof of strict in-cell drawing. A terminal that implements expansion may
still show equal sizes for a particular font, symbol or size, so absence of the
effect in one sample set is not absence of the capability. Either way it is not a
defect.

Cursor movement is not part of this case. Each symbol's own advance is the same in
both rows; the rows differ in total advance only because the spaced row contains
spaces. Measure advance with the width probes.

An occupied neighbor must never be overdrawn in either case, which belongs to
`text-emoji-cell-fitting`.

In the reference implementation the eligible neighbor is an empty cell, a SPACE or
an EN SPACE; the right edge and a preceding symbol restrict expansion, with
exceptions for graphics elements. Expansion never changes a character's grid width
or cursor advance.

Assess it further by editing around a borrowed cell: insert and delete text next to
a symbol and watch whether the artwork resizes, repaints cleanly, and behaves under
selection and the cursor. Those interactions are the substance of the capability,
not just the initial size comparison.

Revenant's current policy is strict in-cell fitting: an oversized face is shrunk
into the committed cells and the ink is asserted to stay inside them, so its record
here is unsupported. That is today's default, not a decision against the feature;
adopting it would be opt-in and would need its own specification of eligibility
and of background, selection, cursor and damage behavior.

## Why a width suite can miss missing glyphs

The original emoji probe measured cursor position reports (CPR). A tofu box
with the correct advance could satisfy its assertions. Variation selectors,
ZWJ and flags in that corpus exercised width accounting; their presence did
not make font discovery, presentation, or glyph visibility tested properties.
The artwork cases above deliberately have separate feature IDs and assessments.
They do not upgrade existing terminal compatibility records to supported.

Renderer integration tests are how an emulator can hold itself to more than this:
assert a non-tofu route, the resolved font identity, visible ink and bounds, and
cursor advance independently. Use modern positive fonts and deliberately
incomplete negative fonts; neither monochrome pixels alone nor a font family name
proves successful rendering, since deterministic tofu is monochrome ink and a
family name is a preference rather than a file. Cite such a suite as evidence for
its own terminal's record, scoped to what it asserts; human legibility and any
other terminal's behavior still need the assessments above.

<!-- tdn:compatibility -->
