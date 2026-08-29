# Font fixture manifest contract

`manifest.json` describes the exact files accepted by the isolated font test
rig. Its `unicode_version` is also the required version for generated emoji
routing data and for the width tables that commit cells before rendering.
`font-fixture-info.py --check` reads the `# Version:` header from
`unicode_data`; additional width or routing inputs can be interlocked with
repeated `--unicode-data FILE` options.

The top-level `outline` value records which outline table technology a font
contains (`glyf`, `CFF`, or `CFF2`). It does **not** claim that a particular
mapped glyph has visible contours. Each probe value records actual ink paths:

- `missing`: the best cmap has no mapping.
- `covered-no-ink`: a cmap mapping exists but none of the inspected paths has
  ink.
- `outline`, `bitmap`, `color`, or `svg`: that path has ink.
- A `+`-joined value such as `outline+color` means multiple usable paths exist.

Outline ink is established by drawing the mapped glyph through a bounds pen;
a mere `glyf` entry is insufficient. Thus Noto COLRv1 probes read `color`
despite the font's top-level `outline: glyf`, OpenMoji probes read
`outline+color`, and the synthetic sbix probe reads `bitmap`. Those distinctions
make the empty-base mechanism behind blank cells manifest-visible.
