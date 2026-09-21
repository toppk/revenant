# Font fixture manifest contract

`manifest.json` describes the exact files accepted by the isolated font test
rig. Its `unicode_version` is also the required version for generated emoji
routing data and for the width tables that commit cells before rendering.
`font-fixture-info.py --check` reads the `# Version:` header from
`unicode_data`; additional width or routing inputs can be interlocked with
repeated `--unicode-data FILE` options.

Each entry also records `version`, the internal `head.fontRevision`. It is not
the release or package version: the monochrome face in the Noto Emoji 2.028
source archive reports `1.050`, and the Fedora `google-noto-emoji-fonts`
20250623-4 package file reports `3.003`. Quote both identities when reporting a
coverage result.

`text_default_bases` is the coverage census: how many emoji bases whose default
presentation is text, excluding the ASCII keycap bases, the font's best cmap
maps. The total comes from `unicode_data`, so `--check` needs that file to
recompute it. Scalar cmap coverage is the only claim here; it says nothing about
sequences, artwork quality, or whether the renderer can reach and fit the glyph.
The two monochrome Noto Emoji fixtures differ sharply under this census — 63 of
207 for `1.050` against 207 of 207 for `3.003` — which is why the older face is
retained as an explicitly incomplete negative fixture rather than replaced.

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

The two monochrome faces never share a universe, because both name the family
`Noto Emoji` and a family request could not then select between them. `mono`
and `routing` hold the incomplete `1.050` face; `mono-modern` and
`routing-modern` hold `3.003`. A positive text-emoji artwork expectation belongs
only in the modern universes; a tofu expectation in `mono` or `routing` records
genuinely absent coverage, not a rendering policy.

The `atomic-tag` universe pairs Twitter's SVGinOT face, which has a generic
black flag but no Scotland ligature, with current Noto Color Emoji. It proves
that preserved tag components reject the partial match and retry the complete
sequence in the next role.

`tools/emoji-coverage-audit.py` inventories these faces against the pinned
`data/emoji-data.txt`, within a declared scope: every sequence in its table and every
base newer than E15.0. Inside that scope it reports which atoms a named automated case
asserts something about, which a staged face supports but nothing asserts, and which no
face supports; older bases are reported as not inventoried rather than untested.
Coverage comes from an explicit case table, not from scanning the suites, and sequence
rows are shaped with `hb-shape` using production's own buffer flags, so neither a cmap
entry nor a vanished control character can pass for a renderable sequence.

For visual diagnosis outside the terminal, render a sequence directly from a
staged font:

```sh
hb-view --font-file=font-fixtures-stage/fonts/NotoColorEmoji-2.034.ttf \
  --output-file=family-2.034.png '👨‍👩‍👧‍👦'
```

See `docs/reference/diagnostics.md` for the corresponding `hb-shape` workflow
and the boundary between a useful standalone rendering and an Xvfb acceptance
test.
