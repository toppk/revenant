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
(U+2800–U+28FF), Powerline, and standardized legacy-computing symbols are
drawn by the emulator rather than the font in kitty,
WezTerm, foot, Ghostty, Alacritty (`builtin_box_drawing`), Windows Terminal,
and VTE, so that lines connect exactly. xterm draws line-drawing characters
itself when the font lacks them. Nerd Font private-use glyphs beyond the
Powerline range come from the font.

TDN tracks the implementation technique as `text-procedural-glyphs`. Each
terminal record names its exact procedural ranges, because terminals divide
DEC scan lines, geometric pieces, Symbols for Legacy Computing, its Unicode 16
supplement, and private-use glyphs differently.

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

See [emoji artwork and fallback](emoji-rendering.md) for separate monochrome,
presentation, fitting and sequence capability probes.

## Color emoji

Color emoji come from bitmap (CBDT, sbix) or vector (COLR) fonts. Emulators
that render them: kitty, WezTerm, foot, Ghostty, Alacritty (via crossfont),
Windows Terminal, iTerm2, Apple Terminal, VTE (through Pango), Konsole. xterm
renders monochrome only. Color glyphs ignore the foreground SGR color.


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
- [Unicode Character Database](https://www.unicode.org/ucd/)

## Background-relative faint text

Feature ID: `resource-faint-is-relative`. The xterm `faintIsRelative` resource
selects background-relative mixing for faint text. This is independent of
recognizing SGR 2 or rendering faint text with the default foreground scaling.

<!-- tdn:compatibility -->
