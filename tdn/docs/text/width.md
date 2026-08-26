# Width

Status: Convention. No standard defines terminal cell width; `wcwidth` and
Unicode Annex #11 are the shared references.

Width is the number of cells a character advances the cursor. Terminals,
libc, and application libraries each compute it independently. When they
disagree, the application believes the cursor is somewhere it is not.

## The `wcwidth` rule

POSIX `wcwidth(3)` returns 0, 1, 2, or -1 for a code point. The conventional
mapping, from Markus Kuhn's reference implementation, is:

| Class | Width |
| --- | --- |
| Control characters | -1 (undefined) |
| Combining marks (`Mn`, `Me`), most format characters (`Cf`) | 0 |
| East Asian Wide (`W`) and Fullwidth (`F`) | 2 |
| Everything else | 1 |

The East Asian Width property comes from Unicode Annex #11. Categories `N`,
`Na`, and `H` are one cell. Category `A`, ambiguous, is one cell by default
and two cells in CJK locales; emulators expose this as a setting:

<!-- markdownlint-disable MD013 -->

| Emulator | Setting |
| --- | --- |
| xterm | `cjkWidth` resource, `-cjk_width` option[^xterm] |
| VTE | `VTE_CJK_WIDTH` environment variable, terminal "ambiguous width" option[^vte] |
| kitty | Follows the wide list; no ambiguous toggle documented |
| WezTerm | `treat_east_asian_ambiguous_width_as_wide`[^wez] |
| foot | `tweak.ambiguous-width`? (`?`) |
| Windows Terminal | Follows the locale (`?`) |

<!-- markdownlint-enable MD013 -->

## Zero-width characters

Combining marks attach to the preceding cell. Zero Width Joiner (U+200D),
Zero Width Non-Joiner (U+200C), and variation selectors are format characters
with width 0 in `wcwidth` terms, yet each may change the width of the unit
they belong to. That is the seam between width and
[grapheme clusters](graphemes.md).

## Emoji and variation selectors

Emoji presentation is the largest source of drift. The rules that most
emulators converge on:

- Code points with `Emoji_Presentation=Yes` are width 2.
- Text-default emoji such as U+2764 HEART are width 1, but U+2764 U+FE0F is
  width 2 in emulators that honor VS16.
- U+FE0E (VS15) requests text presentation; a width-2 emoji followed by VS15
  may become width 1.
- Skin-tone modifiers (U+1F3FB–U+1F3FF) and ZWJ sequences form one width-2
  unit only when the emulator clusters; otherwise each part is measured alone.

`wcwidth` implementations that predate a given Unicode version return 1 for
emoji they do not know, so the same string measures differently on an old
libc and a new emulator.

## Unicode version skew

Three components carry their own Unicode tables:

1. the emulator, compiled against a Unicode version;
2. libc, whose `wcwidth` the shell and many C programs use;
3. the application's own library, such as `wcwidth` for Python,
   `unicode-width` for Rust, or `utf8proc`.

Any mismatch produces drift after the first affected character. There is no
in-band negotiation of Unicode version; the [text sizing](sizing.md) and
[grapheme](graphemes.md) protocols exist partly to reduce dependence on it.

Some emulators ship generated tables from Unicode's `EastAsianWidth.txt` and
`emoji-data.txt` (kitty, foot, Ghostty, WezTerm); others call libc or a
bundled `utf8proc`. The difference is visible with recently added emoji.

## Box drawing and symbols

U+2500–U+257F box drawing, U+2580–U+259F block elements, and Powerline
private-use glyphs are width 1 by `wcwidth`. Emulators often synthesize these
glyphs instead of using the font so the lines meet; see
[Rendering](rendering.md). U+2000–U+200A spaces are width 1 except the
zero-width ones.

## Wide character at the last column

If a width-2 character is written when only one cell remains, the terminal
must either wrap it to the next line, leaving a blank spacer cell, or clip it.
DEC terminals did not have wide characters; xterm wraps and leaves the last
column blank, and other emulators follow. An application computing wrap
positions must reproduce this rule.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Behavior | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Windows Terminal | Apple Terminal | iTerm2 | xterm.js |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Ambiguous-width option | Yes[^xterm] | Yes[^vte] | ? | ? | Yes[^wez] | ? | ? | ? | ? | ? | Yes[^iterm] | ? |
| VS16 widens | Partial[^xterm] | Yes[^vte] | ? | Yes[^kitty] | Yes[^wez] | Yes[^ghostty] | Yes[^foot] | ? | ? | ? | ? | ? |
| Bundled Unicode tables | Yes | Partial | ? | Yes | Yes | Yes | Yes | Yes | ? | ? | ? | Yes |

<!-- markdownlint-enable MD013 -->

[^xterm]: [xterm manual](https://invisible-island.net/xterm/manpage/xterm.html), `cjkWidth`, `mkWidth`.
[^vte]: [VTE](https://gitlab.gnome.org/GNOME/vte), `VTE_CJK_WIDTH` and `src/vte.cc`.
[^wez]: [WezTerm config reference](https://wezterm.org/config/lua/config/treat_east_asian_ambiguous_width_as_wide.html).
[^kitty]: [kitty](https://github.com/kovidgoyal/kitty), `gen/wcwidth.py`.
[^ghostty]: [Ghostty](https://github.com/ghostty-org/ghostty), `src/unicode`.
[^foot]: [foot](https://codeberg.org/dnkl/foot), README "Unicode".
[^iterm]: [iTerm2 preferences, Profiles > Text, "Treat ambiguous-width characters as double width"](https://iterm2.com/documentation-preferences-profiles-text.html).

## Probe

Print a character, ask for the cursor position, and compare the column. This
measures the emulator's opinion directly and is the only portable method.

```sh
stty -echo -icanon min 0 time 5
for s in 'A' '中' '❤' '❤️' '👍🏽' '🇺🇸' '👨‍👩‍👧'; do
  printf '\r\033[K%s\033[6n' "$s"
  reply=$(dd bs=64 count=1 2>/dev/null | tr -d '\033')
  col=${reply##*;}; col=${col%R}
  printf ' width=%s\n' "$((col - 1))"
done
stty sane
```

Compare with `python3 -c 'import unicodedata;print(unicodedata.east_asian_width("中"))'`
and with the application's own width library.

## Sources

- [Unicode Standard Annex #11, East Asian Width](https://www.unicode.org/reports/tr11/)
- [Markus Kuhn's wcwidth.c](https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c)
- [Unicode Technical Standard #51, Emoji](https://www.unicode.org/reports/tr51/)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [utf8proc](https://github.com/JuliaStrings/utf8proc)
