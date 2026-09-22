---
man: revenant-linear-light-study
section: 7
manual: maintainers
description: measured study of fractional text positioning and linear-light composition for the Xft path
---

# Fractional positioning and linear-light study

**Recorded:** 2026-09-22. This page compares Monstar's final text renderer with
Revenant's Xft, Cairo and XRender pipeline. It keeps pixel measurements separate
from readability judgements and ends with a recommendation for this release. No
default appearance changed and no production setting was added. The study used an
analysis tool and a probe sample.

## Monstar's final design (`d2fd5bb`)

The study is based on Monstar's final commit, not the earlier opt-in gamma 2.2
switch (`d835230`). That commit made four changes:

- **Fractional outlines:** outline sizes stay fractional (`FT_Set_Char_Size`), and
  HarfBuzz keeps 26.6 advances and offsets. Each cluster starts at its integer grid
  origin, and pen positions stay fractional inside the cluster.
- **Raster phases:** glyph caches store horizontal and vertical raster phases.
  Negative positions use floor division, and FreeType applies the phase once.
- **Hinting:** light hinting (`FT_LOAD_TARGET_LIGHT`) with native Adobe CFF stem
  darkening.
- **Composition:** always in linear light, using the exact piecewise sRGB curve.
  Channels decode to 16-bit linear values, blend, and encode back to 8-bit storage,
  with a coverage cache per colour pair. Monstar composes every pixel on the CPU;
  there is no server-side blending.

## Revenant's corresponding paths

**Shaping advances and offsets.** HarfBuzz shapes at the font's units per em
(`glyph_shape.c`, `hb_font_set_scale(upem)`), so positions keep full design
precision. They become pixels through Xft's fractional `x_scale`/`y_scale`.
`DrawXftGlyphRun` (`vt_draw.c`) rounds each glyph with `lround` to the integer
coordinates of an `XftGlyphFontSpec`. It rounds rather than truncates, so Monstar's
negative-offset bias does not occur. Each cell's run is centred on its advance
(`XtpFontCenteredOrigin`, `lround`), so glyphs never drift off the integer grid.

**Raster positions and hinting.** Xft rasterizes glyphs through FreeType. It caches
one image per glyph with no phase, and the glyph is placed at an integer pixel.
Revenant sets no hinting, antialiasing or LCD properties itself. They come from the
fontconfig configuration and `Xft.*` resources, as in xterm, and a `faceName`
pattern suffix (`:hintstyle=1`) overrides them. Sizes stay fractional (`FC_SIZE`
as a double).

**Colour transfer and alpha.** `XftDrawGlyphFontSpec` composites A8 glyph masks
with XRender `Over` inside the X server (pixman), in encoded sRGB values.
Backgrounds are `XRenderFillRectangle`. Colour fonts go through Cairo
(`glyph_cairo.c`), which also blends in encoded values. No path uses linear light.

**Text-emoji shrinking and cell containment.** `VtFontFittedForSpan`
(`vt_font.c`) opens a second Xft face scaled by `span / max_advance_width`, where
`max_advance_width` is Xft's integer advance. `CenteredGlyphRunX` then centres the
glyph on its advance, not on its ink.

## Environment

| Item | Value |
|---|---|
| Binary | `build-u18-gcc/revenant`, 0.8.0-dev at `8a2eec9` plus the staged tree (sha256 `d2365b113ef73cd9…`), renderer `xft` |
| Server | Xvfb 21.1.24, 1024×768×24, 100 dpi; pixel size = points × 100 / 72 (derived) |
| Libraries | libXft 2.3.8, FreeType 2.14.3, fontconfig 2.17.0, cairo 1.18.4, pixman 0.46.2, HarfBuzz 14.1.0 (`hb-shape` 14.4.0 for the position measurement) |
| Fontconfig | a single font directory with no `conf.d` rules, so Xft's built-in defaults apply. The default is pixel-identical to `:hintstyle=3` (full). Grayscale; every captured pixel had R = G = B |
| Fonts | DejaVu Sans Mono 2.37 Book `b4a6c3e4…` and Oblique `74209784…` (staged fixtures); JetBrains Mono 2.304 Regular `621dcc09…` and Italic `b6b6b6c9…` (Fedora `jetbrains-mono-fonts-2.304-10.fc44`, CFF, a real italic); Noto Emoji 3.003 `b57ed895…` (staged; 1.050 lacks U+1F6E0) |
| Sizes | 7, 7.5, 8.25 and 9 pt (9.72, 10.42, 11.46 and 12.50 px) |
| Themes | `#000000` on `#ffffff` and `#ffffff` on `#000000` |
| Variants | default (full hinting), `:hintstyle=1`, `:hinting=false`, `:hintstyle=3` |
| Grid | measured from background-only control cells: origin (3, 3) in the captured shell window, cell size as logged, in all 64 captures |

The corpus is ordinary text (`Hamburgefonstiv 0Oo1lI|{}[]`), combining marks
(`é ñ ä q̣̇ ệ`), the italic face (SGR 3), and U+1F6E0 🛠 in text presentation. That
🛠 is not an emoji-presentation character: it falls back to monochrome Noto Emoji
and is fitted into one cell. DejaVu's "italic" is an oblique, and JetBrains Mono
supplies the real italic.

Cell geometry is not always constant, and the tables name it. DejaVu 7 and 7.5 pt
share a 6×13 cell. JetBrains Mono 7 and 7.5 pt share the cell width but not the
height (6×13 and 6×15).

## Measurements

**Grid origin.** The analysis does not assume where the grid starts. The capture
prints a control row with red-background cells in columns 0 and 2. Background
fills cover whole cells, so their edges give the grid origin and cell size
independently of any glyph ink. In all 64 captures the control cells matched the
logged cell size, and the origin was (3, 3) in the captured shell window. That is
its one-pixel X border, a child at offset (0, 0), and the two-pixel internal
border, all recorded from `xwininfo` for each capture. The first version of this
study hardcoded (2, 2), which put every sampling rectangle one pixel off. Every
figure below was recomputed from the measured origin.

**What is measured and what is calculated.** Pixel values are measured. XRender
composes in encoded values, so each pixel inverts exactly to its coverage
α = (p − bg)/(fg − bg), and coverage is also a measurement. The *linear-light
contrast per coverage* is calculated from those pixels: the pixel's difference from
the background in linear-light units, divided by its coverage. It is a contrast
metric derived from the encoded values. It is not a measurement of physical or
perceived stroke weight. The *simulated* images blend the recovered coverage in
linear light with the piecewise sRGB curve. They show one effect of Monstar's
composition applied to Revenant's glyph masks. They do not reproduce Monstar's
appearance, which also depends on its hinting, stem darkening and raster phases,
and they say nothing about readability.

### Encoded composition makes the contrast metric depend on the theme

Ordinary text row, default hinting. The metric is 1.0 by construction when the same
coverage is blended in linear light.

| Face | Size (pt) | Pixel size | Cell | Dark on light | Light on dark | Ratio | Mean change on partial pixels in the simulation |
|---|---|---|---|---|---|---|---|
| DejaVu Sans Mono | 7 | 9.72 | 6×13 | 1.322 | 0.679 | 1.95 | 48/255 |
| DejaVu Sans Mono | 7.5 | 10.42 | 6×13 | 1.322 | 0.679 | 1.95 | 48/255 |
| DejaVu Sans Mono | 8.25 | 11.46 | 7×14 | 1.287 | 0.711 | 1.81 | 46/255 |
| DejaVu Sans Mono | 9 | 12.50 | 8×15 | 1.282 | 0.716 | 1.79 | 47/255 |
| JetBrains Mono | 7 | 9.72 | 6×13 | 1.374 | 0.625 | 2.20 | 44/255 |
| JetBrains Mono | 7.5 | 10.42 | 6×15 | 1.393 | 0.611 | 2.28 | 46/255 |
| JetBrains Mono | 8.25 | 11.46 | 7×16 | 1.354 | 0.648 | 2.09 | 46/255 |
| JetBrains Mono | 9 | 12.50 | 8×17 | 1.331 | 0.668 | 1.99 | 46/255 |

The italic and combining rows follow the same pattern. At 7.5 pt the italic row is
1.327/0.672 for DejaVu and 1.412/0.589 for JetBrains; the combining row is
1.286/0.717 and 1.411/0.596.

With the same glyph mask, the metric for dark text on a light background is 1.8–2.3
times the metric for light text on a dark background. Blending in linear light
removes that asymmetry by construction. In the simulation, the metric falls by
22–28 % for dark on light and rises by 40–64 % for light on dark. How that
relates to perceived weight or legibility was not measured. Monstar pairs linear
light with stem darkening, which only affects CFF faces such as JetBrains Mono, not
TrueType faces such as DejaVu.

### Full hinting discards fractional TrueType sizes

Coverage of the ordinary row, dark on light:

| Variant | DejaVu 7 pt | DejaVu 7.5 pt | JetBrains 7 pt | JetBrains 7.5 pt |
|---|---|---|---|---|
| default (full) | 317.9 (6×13) | 317.9 (6×13) | 291.2 (6×13) | 335.3 (6×15) |
| `:hintstyle=1` | 320.0 | 342.4 | 291.2 | 335.3 |
| `:hinting=false` | 290.6 | 333.4 | 290.7 | 335.3 |

Under full hinting, the text rows of DejaVu 7 pt and 7.5 pt are pixel-identical
in the same cell, because the TrueType interpreter works at a whole pixels-per-em
value. Only the fitted 🛠 row differs, since its scale comes from a different
maximum advance. Slight hinting or no hinting keeps the fractional size. JetBrains
Mono, a CFF face, is barely affected by the hint style. All 16 default captures are
pixel-identical to `:hintstyle=3`. Fedora's fontconfig ships
`10-hinting-slight.conf`, so on an ordinary Fedora desktop the collapse happens
only when a user or `faceName` asks for full hinting.

### Positions: rounding moves glyphs by at most half a pixel

For each glyph in the combining clusters, `hb-shape` compared the exact pixel
position with its rounded position. The largest difference is 0.02–0.50 px,
depending on font and size, and the mean is at most 0.09 px. The fonts attach most
marks through their outlines rather than with large offsets. The monospace advance
(5.83–7.53 px) leaves 0–0.5 px of slack in each integer cell. This bounds how far
rounding displaces a glyph. It does not bound the readability benefit of raster
phases, which change the rasterized image itself and were not measured.

### Fitted 🛠: no ink leaves its cell

Coverage in the blank cells on each side of a fitted 🛠 was 0.00 in all 64
captures. Every size, face, theme and hinting variant keeps its ink inside the
cell. The first version of this study reported a right-hand overhang. That was the
cell's own last pixel column, read one pixel off, and the finding is withdrawn.

Three separate questions were checked:

- **Does the advance exceed the span?** Yes, at three of four sizes. The fit scale
  is computed from Xft's integer `max_advance_width` (15 at 9 pt, against an exact
  15.87 px), so the fitted advance is 0.11–0.46 px wider than the span. For
  example, it is 8.46 px against 8 px at 9 pt. The advance includes sidebearings,
  so this alone does not put ink outside the cell.
- **Is the glyph clipped inside the cell?** Not established. Revenant clips drawing
  to the cell, so clipped pixels cannot appear in a capture. At every size the
  unhinted outline's ink lies inside the cell once centred (0.57–7.89 px in 8 px at
  9 pt). An offline FreeType raster at the 9 pt fitted size spans 9 columns,
  though, so the hinted raster could be a column wider than the span, and the clip
  would then cut it. The measured coverage in the cell's first and last columns
  (0.69–0.72 and 0.14–1.08 under default hinting) is consistent with either
  reading. Settling it needs a render without the clip, which this study did not
  make.
- **Does ink escape the cell?** No, as measured above.

### Screenshots

Each image shows the capture above the *simulation* described above, at 3× with
nearest-neighbour scaling. The lower half is recovered coverage blended in linear
light, not a Monstar rendering:

- [JetBrains Mono 7.5 pt, dark on light](linear-light-study/jetbrains-7.5-dark-on-light-vs-linear.png)
- [JetBrains Mono 7.5 pt, light on dark](linear-light-study/jetbrains-7.5-light-on-dark-vs-linear.png)
- [DejaVu Sans Mono 7.5 pt, dark on light, default (full) hinting](linear-light-study/dejavu-7.5-dark-on-light-vs-linear.png)
- [DejaVu Sans Mono 7.5 pt, dark on light, `:hintstyle=1`](linear-light-study/dejavu-7.5-dark-on-light-hintstyle1-vs-linear.png)

### Readability judgements (not measured)

These are the implementer's impressions from the screenshots, not assessments;
the visual outcomes remain unassessed. In the simulation, dark-on-light text looks
thinner and greyer, and light-on-dark text bolder, than in the capture. The fitted
🛠 is 5–7 pixels wide and hard to recognise. A maintainer should judge readability
from the probe sample on real hardware. Neither the images nor the metric
establish it.

## Recommendation

**Defer linear-light composition and fractional raster phases this release.**
Nothing in the corrected evidence changes that.

- **Composition:**
  - *Benefit:* the contrast metric would no longer depend on the theme. That is a
    real property of encoded blending, shared by every Xft and XRender client,
    xterm included, and not a Revenant regression. Whether it improves readability
    is unmeasured.
  - *Cost:* high. Composition happens in the X server, so Revenant would need
    either client-side CPU composition with image upload, which is a compositor
    rewrite, or glyph masks rewritten per colour pair, which defeats XRender glyph
    sets.
  - *Risk:* high. The simulation lowers the dark-on-light metric by about a quarter
    by default. It would diverge from xterm and every other Xft application on the
    same desktop. It would need stem darkening, which does not help TrueType faces.
- **Fractional phases:** rounding displaces glyphs by at most 0.5 px. Any
  readability benefit is unmeasured, and Xft cannot cache phases. Leave them.
- **Hinting:** no change. Revenant follows fontconfig and `faceName` as xterm
  does, and Fedora's defaults already keep fractional sizes. Document the
  full-hinting collapse for users. Explicit font precedence is untouched.
- **Fitted emoji:** no defect demonstrated and no fix proposed. One question stays
  open: whether hinting widens the fitted raster past the span, so that the cell
  clip cuts a column. Answering it needs a clip-free render of the fitted face.

## Reproduction

The study needs the staged fixtures (`tools/stage-font-fixtures`), Fedora's
`jetbrains-mono-fonts`, `xwd`, ImageMagick and Python Pillow.

```sh
Xvfb :90 -screen 0 1024x768x24 -nolisten unix -listen tcp -ac &
export DISPLAY=127.0.0.1:90
tools/text-contrast-study.py capture /tmp/m4 --terminal build/revenant \
    --variant '' --variant ':hintstyle=1' --variant ':hinting=false' --variant ':hintstyle=3'
tools/text-contrast-study.py analyze /tmp/m4
```

`capture` writes a screenshot, the log and the geometry (including `xwininfo`
border and child offsets) for each face, size, theme and variant, plus `fonts.json`
with the file hashes. `analyze` writes `summary.json` and a `-vs-linear.png` per
capture, and stops if the control cells disagree with the logged cell size.

For human assessment in any terminal, run the same corpus through the probe at
several sizes:

```sh
revenant -fa 'JetBrains Mono' -fs 7.5 -e just probe text-shaping text-contrast
```
