# Legacy keyboard encoding

Status: DEC and xterm, with terminfo as the application-facing catalogue.

The legacy encoding is what every terminal emits when no other keyboard
protocol has been requested. It is a union of the VT100/VT220 key encodings,
xterm's additions, and behaviors that terminfo describes but no standard
defines. It cannot represent every key and modifier, and its ambiguities are
the reason later protocols exist.

## Printable keys and control characters

A printable key sends its character encoded in the terminal's charset,
normally UTF-8. Holding Ctrl with a letter or a few punctuation keys sends the
C0 control byte obtained by clearing bits 6 and 5 of the uppercase ASCII
code:

<!-- markdownlint-disable MD013 -->

| Keys | Byte | Also produced by |
| --- | --- | --- |
| Ctrl-@, Ctrl-Space | `0x00` NUL | |
| Ctrl-A … Ctrl-Z | `0x01` … `0x1a` | |
| Ctrl-H | `0x08` BS | Backspace on some terminals |
| Ctrl-I | `0x09` HT | Tab |
| Ctrl-J | `0x0a` LF | Enter with LNM set in some configurations |
| Ctrl-M | `0x0d` CR | Enter |
| Ctrl-[ | `0x1b` ESC | Escape, and the prefix of every escape sequence |
| Ctrl-\ | `0x1c` FS | |
| Ctrl-] | `0x1d` GS | |
| Ctrl-^ | `0x1e` RS | |
| Ctrl-_ | `0x1f` US | Ctrl-/ on many terminals |
| Ctrl-? | `0x7f` DEL | Backspace on most terminals |

<!-- markdownlint-enable MD013 -->

Because the mapping is a bit operation on the byte, Ctrl-I and Tab are the
same byte and cannot be told apart, nor can Ctrl-M and Enter, Ctrl-[ and
Escape, or Ctrl-H and Backspace when Backspace is configured to send `0x08`.
Ctrl with digits or most punctuation sends nothing distinct in the legacy
encoding.

## Backspace: BS or DEL

Two conventions exist for the Backspace key: `0x08` BS and `0x7f` DEL. Modern
Linux and macOS terminals default to DEL, matching the `kbs` capability in
their terminfo entries; xterm's `backarrowKey` resource and DECBKM
(`CSI ? 67 h`) switch between them. An application should read `kbs` from
terminfo or accept both.

## Alt and Meta

There are two encodings for a key pressed with Alt or Meta:

- **ESC prefix**: send `ESC` followed by the key's normal bytes.
- **Eighth bit**: set bit 7 of the key's byte, which is unusable in UTF-8
  sessions because it produces invalid or misinterpreted bytes.

xterm controls the choice with resources and DEC private modes:

<!-- markdownlint-disable MD013 -->

| Mode | Resource | Effect when set |
| --- | --- | --- |
| `?1034` | `eightBitInput` | Meta sets the eighth bit |
| `?1036` | `metaSendsEscape` | Meta sends an `ESC` prefix instead |
| `?1039` | `altSendsEscape` | Alt sends an `ESC` prefix instead |

<!-- markdownlint-enable MD013 -->

The ESC prefix makes Alt-x indistinguishable from Escape followed quickly by
x. Applications resolve this with a timeout (`ESCDELAY` in ncurses, `ttimeoutlen`
in Vim), which is why Escape can feel slow in some programs.

## Cursor keys

Arrow keys send `CSI` plus a final byte in normal mode and `SS3` plus the same
byte when DECCKM (`CSI ? 1 h`) is set:

| Key | Normal | Application (`?1`) |
| --- | --- | --- |
| Up | `CSI A` | `SS3 A` |
| Down | `CSI B` | `SS3 B` |
| Right | `CSI C` | `SS3 C` |
| Left | `CSI D` | `SS3 D` |

`SS3` is `ESC O`. With a modifier, xterm emits `CSI 1 ; mod A`, dropping the
`SS3` form regardless of DECCKM.

## Keypad

DECKPAM (`ESC =`) selects application keypad mode, in which keypad keys send
`SS3` sequences (`SS3 M` for Enter, `SS3 j` … `SS3 y` for operators and
digits). DECKPNM (`ESC >`) selects numeric mode, in which they send the same
digits as the main keyboard. Terminfo describes the two modes with `smkx` and
`rmkx`; many terminfo entries fold DECCKM into the same strings, so a full-screen
application that sends `smkx` gets both.

## Home, End, and the tilde keys

Editing keys use a numbered form terminated by `~`:

<!-- markdownlint-disable MD013 -->

| Key | xterm | rxvt | VT220 |
| --- | --- | --- | --- |
| Home | `CSI H` (`SS3 H` in `?1`) | `CSI 7 ~` | `CSI 1 ~` (Find) |
| Insert | `CSI 2 ~` | `CSI 2 ~` | `CSI 2 ~` |
| Delete | `CSI 3 ~` | `CSI 3 ~` | `CSI 3 ~` (Remove) |
| End | `CSI F` (`SS3 F` in `?1`) | `CSI 8 ~` | `CSI 4 ~` (Select) |
| Page Up | `CSI 5 ~` | `CSI 5 ~` | `CSI 5 ~` |
| Page Down | `CSI 6 ~` | `CSI 6 ~` | `CSI 6 ~` |

<!-- markdownlint-enable MD013 -->

xterm accepts `CSI 1 ~` and `CSI 4 ~` as Home and End for compatibility. An
application should read `khome` and `kend` from terminfo rather than assume
one form.

## Function keys

F1–F4 follow the VT100 PF keys and use `SS3`; F5 onward follow the VT220 and
use numbered tilde sequences with a gap left by history:

<!-- markdownlint-disable MD013 -->

| Key | Unmodified | With modifier |
| --- | --- | --- |
| F1 | `SS3 P` | `CSI 1 ; mod P` |
| F2 | `SS3 Q` | `CSI 1 ; mod Q` |
| F3 | `SS3 R` | `CSI 1 ; mod R` |
| F4 | `SS3 S` | `CSI 1 ; mod S` |
| F5 | `CSI 15 ~` | `CSI 15 ; mod ~` |
| F6 | `CSI 17 ~` | `CSI 17 ; mod ~` |
| F7 | `CSI 18 ~` | `CSI 18 ; mod ~` |
| F8 | `CSI 19 ~` | `CSI 19 ; mod ~` |
| F9 | `CSI 20 ~` | `CSI 20 ; mod ~` |
| F10 | `CSI 21 ~` | `CSI 21 ; mod ~` |
| F11 | `CSI 23 ~` | `CSI 23 ; mod ~` |
| F12 | `CSI 24 ~` | `CSI 24 ; mod ~` |

<!-- markdownlint-enable MD013 -->

Numbers 16 and 22 are unused. Older xterm and rxvt encode Shift-F1 … as F11
… F20 (`CSI 23 ~` upward), so `CSI 23 ~` can mean F11 or Shift-F1 depending
on the emulator; terminfo `kf11` … `kf63` record each emulator's choice.
rxvt also sends `CSI 11 ~` … `CSI 14 ~` for F1–F4.

## The xterm modifier parameter

When a modifier is held, xterm adds a second parameter whose value is one plus
a bit mask:

| Bit | Modifier |
| --- | --- |
| 1 | Shift |
| 2 | Alt |
| 4 | Ctrl |
| 8 | Meta |

So `CSI 1 ; 5 C` is Ctrl-Right and `CSI 3 ; 2 ~` is Alt-Delete. The `1 ;`
placeholder is required for keys whose base form has no parameter. The same
parameter is reused by [modifyOtherKeys](modify-other-keys.md) and, with more
bits, by the [Kitty keyboard protocol](kitty-keyboard.md).

## Terminfo names

Applications should not hard-code the sequences above. Terminfo provides one
capability per key, and ncurses maps them to `KEY_*` constants:

| Capability | Key |
| --- | --- |
| `kcuu1` `kcud1` `kcuf1` `kcub1` | Up, Down, Right, Left |
| `khome` `kend` | Home, End |
| `kich1` `kdch1` | Insert, Delete |
| `kpp` `knp` | Page Up, Page Down |
| `kf1` … `kf63` | Function keys, including modified variants |
| `kbs` | Backspace |
| `kcbt` | Shift-Tab (`CSI Z`) |
| `smkx` `rmkx` | Enter and leave keypad-transmit mode |

Terminfo describes at most one sequence per capability, so it cannot describe
an emulator that accepts several forms, and it has no vocabulary for
modifier combinations beyond xterm's extended `kUP5`-style names.

## Known ambiguities

<!-- markdownlint-disable MD013 -->

| Bytes | Possible meanings | Resolution |
| --- | --- | --- |
| `0x09` | Tab, Ctrl-I | Not resolvable; use a later protocol |
| `0x0d` | Enter, Ctrl-M | Not resolvable |
| `0x08` / `0x7f` | Backspace, Ctrl-H / Ctrl-? | Read `kbs` from terminfo |
| `ESC` alone | Escape key, start of a sequence, Alt prefix | Timeout |
| `ESC x` | Alt-x, Escape then x | Timeout |
| `CSI 23 ~` | F11, Shift-F1 | Consult terminfo for the emulator |
| `CSI 1 ~` | Home (VT220), Find | Consult terminfo |
| `SS3 A` vs `CSI A` | Same key, different DECCKM state | Accept both |
| Shift-arrow | Sometimes `CSI 1 ; 2 A`, sometimes `CSI a` (rxvt) | Consult terminfo or accept both |

<!-- markdownlint-enable MD013 -->

## Probe

Print raw bytes for each key pressed; `cat -v` shows `ESC` as `^[`:

```sh
cat -v
```

On Linux consoles and X11 sessions `showkey -a` gives the same view with
decimal and hex values. To capture with exact byte values:

```sh
stty raw -echo; dd bs=1 count=8 2>/dev/null | od -c; stty sane
```

Check what terminfo believes the current terminal sends:

```sh
infocmp -1 | grep -E '^\s*(kbs|khome|kend|kf5|kcuu1)='
```

## Sources

- [XTerm Control Sequences, PC-Style Function Keys](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-PC-Style-Function-Keys)
- [XTerm manual, resources `backarrowKey`, `metaSendsEscape`, `altSendsEscape`](https://invisible-island.net/xterm/manpage/xterm.html)
- [terminfo(5)](https://invisible-island.net/ncurses/man/terminfo.5.html)
- [VT220 Programmer Reference, keyboard chapter](https://vt100.net/docs/vt220-rm/chapter3.html)
