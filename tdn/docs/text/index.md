# Text

A terminal stores a grid of fixed-width cells. Unicode describes text as a
sequence of code points that combine, shape, and vary in width depending on
script, font, and context. Every text problem in a terminal is a place where
those two models disagree.

## Two models

The **cell model** is what applications assume: each row has a fixed number of
columns, each printable unit advances the cursor by a whole number of cells,
and a program can compute where the cursor will land after writing a string.

The **Unicode model** has no cells. Code points may be zero-width, combining,
context-dependent in width, or joined into a single visible glyph by a joiner
or a font feature. Width is not a property of a code point in Unicode; it is a
rendering outcome.

Terminals reconcile the two by assigning a cell width to every unit of text
before it is stored. The rules for that assignment are where implementations
differ, and where cursor drift, misaligned tables, and clipped emoji come from.

## Where the mismatch lives

| Problem | Page |
| --- | --- |
| Which bytes mean which characters | [Encoding](encoding.md) |
| How many cells a character occupies | [Width](width.md) |
| Which code points form one unit | [Grapheme clusters](graphemes.md) |
| Text larger or smaller than one cell | [Sizing](sizing.md) |
| Bold, italic, ligatures, fallback, bidi | [Rendering](rendering.md) |

## Practical rule

An application cannot know how a terminal will measure a string unless it
knows the terminal's width algorithm and Unicode version. The cursor position
report is the only portable way to find out; see the probe on the
[Width](width.md) page.

## Sources

- [Unicode Standard Annex #11, East Asian Width](https://www.unicode.org/reports/tr11/)
- [Unicode Standard Annex #29, Text Segmentation](https://www.unicode.org/reports/tr29/)
- [terminal-wg specifications](https://gitlab.freedesktop.org/terminal-wg/specifications)
