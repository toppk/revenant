# Rendering

Status: Convention. This page collects rendering behavior that has no wire
protocol of its own but changes how the same bytes look.

## Bold, faint, italic

SGR 1 historically meant "bright" on eight-color hardware; the eight bright
colors 90–97 were later given their own codes. Emulators now offer a choice:
render bold with a heavier font, brighten the color, or both. xterm's
`boldColors`, VTE's "allow bold" and bright-bold settings, kitty's
`bold_is_bright`, and Windows Terminal's `intenseTextStyle` are the switches.
An application that wants bright must use 90–97; one that wants a bold face
must use SGR 1 and accept that some terminals will also brighten.

SGR 2 (faint) is usually rendered by dimming the foreground; SGR 3 (italic)
requires an italic face and is ignored by terminals without one. Underline
styles are on the [SGR page](../csi/sgr.md).

## Shaping and ligatures

Most emulators shape one cell at a time. Each cell holds a code point (or a
cluster), the glyph is looked up in the font, and the result is placed at the
cell origin. This is why ligatures, contextual forms in Arabic, and
conjuncts in Indic scripts historically did not appear.

Emulators that run a shaper (HarfBuzz) over a run of cells and then split the
result back into cells: kitty, WezTerm (`harfbuzz_features`), foot (with
`tweak.grapheme-shaping` and `tweak.font-shaping`), Contour, Ghostty (`?`),
Alacritty No. The shaped run is still constrained to the cell grid; a
ligature is drawn across its original cells but does not change cursor
movement, so selecting or deleting inside it behaves as if the characters
were separate.

## Font fallback

A single font rarely covers Latin, CJK, symbols, and emoji. Emulators build a
fallback chain from configuration or fontconfig and pick the first font that
has the glyph. Two consequences matter to applications:

- width was already decided from tables, so a fallback glyph that is naturally
  wider or narrower is scaled or clipped to fit;
- a glyph from a fallback font with different metrics can sit visibly higher
  or lower than neighbors.

## Synthesized glyphs

Box drawing (U+2500–U+257F), block elements (U+2580–U+259F), Braille
(U+2800–U+28FF), Powerline (U+E0B0–U+E0BF), and some legacy computing symbols
(U+1FB00–U+1FBFF) are drawn by the emulator rather than the font in kitty,
WezTerm, foot, Ghostty, Alacritty (`builtin_box_drawing`), Windows Terminal,
and VTE, so that lines connect exactly. xterm draws line-drawing characters
itself when the font lacks them. Nerd Font private-use glyphs beyond the
Powerline range come from the font.

## Bidirectional text

Status: Disputed.

ECMA TR/53 and the terminal-wg bidi specification define explicit modes:

```text
CSI ? 2500 h    bidi: terminal performs implicit reordering (default on/off differs)
CSI ? 2501 h    bidi: box mirroring / explicit direction, per spec
```

The terminal-wg draft uses these numbers; adoption is limited. mlterm and
Konsole implement bidi reordering by their own rules; VTE implements the
terminal-wg draft (0.58+)[^vtebidi]; foot, kitty, WezTerm, Ghostty, Alacritty,
xterm do not reorder. Applications targeting Arabic or Hebrew users must
assume either logical-order display or visual reordering with no way to tell
except by asking the user, unless the emulator answers DECRQM for 2500.

## Color emoji

Color emoji come from bitmap (CBDT, sbix) or vector (COLR) fonts. Emulators
that render them: kitty, WezTerm, foot, Ghostty, Alacritty (via crossfont),
Windows Terminal, iTerm2, Apple Terminal, VTE (through Pango), Konsole. xterm
renders monochrome only. Color glyphs ignore the foreground SGR color.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | Windows Terminal | Apple Terminal | iTerm2 | xterm.js |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bold-is-bright option | Yes | Yes | ? | Yes[^kitty] | Yes | Yes | Yes[^foot] | Yes | ? | Yes[^wt] | Yes | Yes | Partial |
| Italic face | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Ligatures / shaping | ? | ? | ? | Yes[^kitty] | Yes[^wez] | ? | Yes[^foot] | ? | Yes | Yes | Yes | Yes | Partial (canvas renderer) |
| Synthesized box drawing | Yes | Yes | ? | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | ? | Yes |
| Bidi reordering | ? | Yes[^vtebidi] | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| Color emoji | ? | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |

<!-- markdownlint-enable MD013 -->

[^kitty]: [kitty configuration](https://sw.kovidgoyal.net/kitty/conf/), `bold_is_bright`, `disable_ligatures`.
[^foot]: [foot.ini(5)](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot.ini.5.scd).
[^wez]: [WezTerm, `harfbuzz_features`](https://wezterm.org/config/lua/config/harfbuzz_features.html).
[^wt]: [Windows Terminal profile settings, `intenseTextStyle`](https://learn.microsoft.com/windows/terminal/customize-settings/profile-appearance).
[^vtebidi]: [VTE bidi documentation](https://gitlab.gnome.org/GNOME/vte/-/blob/master/doc/bidi.md).

## Probe

```sh
printf '\033[1mbold\033[0m \033[91mbright red\033[0m \033[1;31mbold red\033[0m\n'
printf '\033[2mfaint\033[0m \033[3mitalic\033[0m\n'
printf 'fi ffl -> => != \n'          # ligature check
printf '┌─┬─┐\n│ │ │\n└─┴─┘\n'      # box drawing joins
printf 'שלום עולם\n'                 # bidi: logical or visual order?
printf '😀🎉\n'                       # color emoji
```

## Sources

- [ECMA TR/53, Handling of Bi-Directional Texts](https://ecma-international.org/publications-and-standards/technical-reports/ecma-tr-53/)
- [terminal-wg bidi specification](https://gitlab.freedesktop.org/terminal-wg/specifications)
- [HarfBuzz](https://harfbuzz.github.io/)
- [xterm manual](https://invisible-island.net/xterm/manpage/xterm.html)
