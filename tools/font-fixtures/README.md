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

The font set includes both the current pinned Noto Color Emoji CBDT build and
Noto Emoji 2.034. The latter is byte-identical to the copy embedded by Ghostty
tags 1.2.1, 1.3.0, and 1.3.1, retains the earlier colorful family artwork, and
lacks U+1FAE8. It therefore captures Noto before the Emoji 15.1 family
redesign. The `cbdt-legacy` universe isolates its direct rendering; the
`legacy-routing` universe pairs it with OpenMoji so a genuine color-face
coverage miss exercises role fall-through.

The `atomic-tag` universe pairs Twitter's SVGinOT face, which has a generic
black flag but no Scotland ligature, with current Noto Color Emoji. It proves
that preserved tag components reject the partial match and retry the complete
sequence in the next role.

For visual diagnosis outside the terminal, render a sequence directly from a
staged font:

```sh
hb-view --font-file=font-fixtures-stage/fonts/NotoColorEmoji-2.034.ttf \
  --output-file=family-2.034.png '👨‍👩‍👧‍👦'
```

See `docs/reference/diagnostics.md` for the corresponding `hb-shape` workflow
and the boundary between a useful standalone rendering and an Xvfb acceptance
test.
