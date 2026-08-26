# Encoding

Status: Standard (ISO 2022 character-set switching; UTF-8 per RFC 3629).

The byte stream on a PTY has no declared encoding. The emulator decodes it
using a mode selected by configuration, locale, or an escape sequence, and the
application must agree without being told.

## UTF-8 as the practical default

Every emulator in the compatibility tables defaults to UTF-8 when the locale
says so, and most do so unconditionally. `LANG` or `LC_CTYPE` ending in
`.UTF-8` is the signal applications and libraries such as ncurses use; the
emulator often sets nothing and simply decodes UTF-8.

The explicit switches are:

```text
ESC % G    select UTF-8
ESC % @    return to ISO 2022 / Latin-1 handling
```

xterm honors both. Several modern emulators ignore them and stay in UTF-8;
they are safe to send but cannot be relied on to change anything.

## Legacy character sets

ISO 2022 lets a terminal hold up to four designated sets, G0 to G3, and shift
between them:

<!-- markdownlint-disable MD013 -->

| Sequence | Meaning |
| --- | --- |
| `ESC ( B` | Designate US-ASCII into G0 |
| `ESC ( 0` | Designate DEC Special Graphics (line drawing) into G0 |
| `ESC ) B`, `ESC ) 0` | Same, into G1 |
| `ESC * …`, `ESC + …` | Same, into G2 and G3 |
| `SO` (`0x0e`) | Lock shift: use G1 |
| `SI` (`0x0f`) | Lock shift: use G0 |
| `ESC N`, `ESC O` | Single shift: next character from G2 or G3 |

<!-- markdownlint-enable MD013 -->

DEC Special Graphics is the one legacy set still in daily use. Terminfo's
`acsc` capability maps ASCII letters to line-drawing glyphs through it, and
ncurses emits `ESC ( 0` followed by `j`, `k`, `l`, `m`, `q`, `x` for corners
and lines. Most UTF-8 emulators still translate this set even when they
implement nothing else from ISO 2022.

`luit` is xterm's companion filter that converts legacy locales to UTF-8 on
the fly. It remains the reference for how a UTF-8 emulator can serve a
non-UTF-8 application.

## Invalid input

A decoder must not stall on malformed bytes. The common choices are to emit
U+FFFD for each invalid byte or for each maximal invalid subsequence, as
described in the Unicode Standard chapter 3. Emulators differ in how many
replacement characters one bad sequence produces, which changes the cursor
position; applications should never write bytes they do not know to be valid.

## C1 controls

Bytes `0x80`–`0x9f` are C1 control codes in ISO 6429 but continuation bytes in
UTF-8. In a UTF-8 session an emulator must treat them as text, or a valid
character such as U+2013 would be parsed as `CSI`. xterm documents that it
disables 8-bit controls in UTF-8 mode; modern emulators do not recognize them
at all. Always send the two-byte `ESC [` forms.

## Byte-order mark

U+FEFF at the start of a stream is not meaningful on a PTY. Emulators treat it
as a zero-width character; applications should strip it from files before
printing.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Windows Terminal | iTerm2 | xterm.js |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `ESC % G` / `ESC % @` | Yes[^ctl] | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| DEC Special Graphics | Yes[^ctl] | Yes[^vte] | ? | Yes[^kitty] | Yes[^wez] | Yes[^ghostty] | Yes[^foot] | Yes[^ala] | Yes[^wt] | ? | Yes[^xjs] |
| 8-bit C1 in UTF-8 | No[^ctl] | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| Non-UTF-8 locales | Yes[^ctl] | ? | ? | No[^kitty] | ? | ? | ? | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

[^ctl]: [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html), "Controls beginning with ESC".
[^vte]: [VTE source](https://gitlab.gnome.org/GNOME/vte), `src/parser-charset-tables.hh`.
[^kitty]: kitty documents UTF-8-only operation; see [kitty FAQ](https://sw.kovidgoyal.net/kitty/faq/).
[^wez]: [WezTerm escape sequences](https://wezterm.org/escape-sequences.html).
[^ghostty]: [Ghostty VT reference](https://ghostty.org/docs/vt).
[^foot]: [foot](https://codeberg.org/dnkl/foot), README "Features".
[^ala]: [Alacritty](https://github.com/alacritty/alacritty), `alacritty_terminal` charset handling.
[^wt]: [Windows Terminal](https://github.com/microsoft/terminal), `adaptDispatch` charset support.
[^xjs]: [xterm.js](https://xtermjs.org/), `src/common/data/Charsets.ts`.

## Probe

```sh
printf '\033(0lqqk\033(B\n'      # expect a box corner: ┌──┐
printf '\xe2\x80\x93\n'          # en dash; must print, not swallow the line
printf '\xff\n'                  # one invalid byte; count the U+FFFD glyphs
printf '\033%%G'                 # harmless in UTF-8; no visible change
```

## Sources

- [ISO/IEC 2022](https://www.iso.org/standard/22747.html)
- [RFC 3629, UTF-8](https://www.rfc-editor.org/rfc/rfc3629)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [luit](https://invisible-island.net/luit/)
- [Unicode Standard, chapter 3, conformance](https://www.unicode.org/versions/latest/)
