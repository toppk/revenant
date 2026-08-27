# Kitty keyboard protocol

Status: Extension, originated by kitty and adopted by most current emulators.

The Kitty keyboard protocol replaces the [legacy encoding](keyboard-legacy.md)
with one that can represent every key, every modifier, key release and
repeat events, and the text a key would produce. It is opt-in per
application, layered on a stack so that nested programs do not disturb one
another, and progressive: an application enables only the flags it needs.

## Syntax

```text
CSI > flags u        push a new set of flags on the stack
CSI < n u            pop n entries (default 1)
CSI = flags ; mode u set flags in the current entry without pushing
CSI ? u              query current flags
CSI ? flags u        reply to the query
```

`flags` is a bit mask:

<!-- markdownlint-disable MD013 -->

| Bit | Name | Effect |
| --- | --- | --- |
| `1` | Disambiguate escape codes | Keys that were ambiguous (Esc, Ctrl-letter, Alt-letter, Tab, Enter, Backspace) become `CSI … u` sequences |
| `2` | Report event types | Add press/repeat/release as a sub-parameter |
| `4` | Report alternate keys | Add the shifted key and base-layout key |
| `8` | Report all keys as escape codes | Even plain printable keys become `CSI … u` |
| `16` | Report associated text | Append the code points the key would have inserted |

<!-- markdownlint-enable MD013 -->

For `CSI = flags ; mode u`, `mode` is `1` to set the given flags, `2` to set
the given flags without clearing others, and `3` to clear them.

## Key encoding

```text
CSI unicode-key-code : shifted-key : base-layout-key ; modifiers : event-type ; text-as-codepoints u
```

Sub-parameters are separated by `:` and may be omitted from the right; a
key with no modifier and no event data is `CSI 97 u` for `a`.

The modifier value is one plus a mask:

| Bit | Modifier |
| --- | --- |
| `1` | Shift |
| `2` | Alt |
| `4` | Ctrl |
| `8` | Super |
| `16` | Hyper |
| `32` | Meta |
| `64` | Caps Lock |
| `128` | Num Lock |

Event types are `1` press (default, may be omitted), `2` repeat, and `3`
release; they are only sent when flag `2` is set.

## Functional keys

Keys that already had a legacy encoding keep it, with the modifier and event
sub-parameters attached in the same positions as the xterm form:

<!-- markdownlint-disable MD013 -->

| Key | Encoding | Note |
| --- | --- | --- |
| Escape | `CSI 27 u` | Only with disambiguate; the bare `ESC` byte is no longer sent |
| Enter | `CSI 13 u` | With disambiguate when modified; plain Enter still sends `CR` unless flag `8` is set |
| Tab | `CSI 9 u` | As Enter |
| Backspace | `CSI 127 u` | As Enter |
| Insert | `CSI 2 ; mod ~` | Legacy tilde form retained |
| Delete | `CSI 3 ; mod ~` | |
| Up, Down, Right, Left | `CSI 1 ; mod A` … `D` | `CSI A` when unmodified |
| Home, End | `CSI 1 ; mod H`, `CSI 1 ; mod F` | |
| F1–F4 | `CSI 1 ; mod P` … `S` | `SS3 P` unmodified |
| F5–F12 | `CSI 15 ; mod ~` … `CSI 24 ; mod ~` | Same numbers as xterm |
| Keypad, media, modifier keys | `CSI code ; mod u` with `code` ≥ 57344 | Private Use Area code points assigned by the protocol |

<!-- markdownlint-enable MD013 -->

The Private Use Area assignments start at `U+E000` (57344) and cover Caps
Lock, Scroll Lock, Num Lock, Print Screen, Pause, Menu, F13–F35, the numeric
keypad, media keys, and left/right modifier keys. The full table is in the
protocol document.

## The stack

Each `CSI > flags u` pushes; each `CSI < u` pops. A program that pushes must
pop before exiting, including on signal-driven exit, or the next program on
the same PTY inherits an encoding it did not request. The terminal caps the
stack depth and discards the oldest entry when exceeded, and resets the
stack when the alternate screen is entered or left only in emulators that
document doing so; do not rely on that.

The push/pop model exists so that a shell can enable disambiguation for its
own line editor, run an editor that pushes its own flags, and get its flags
back when the editor pops.

## Detection

Send the query followed by a request that always answers, then read until the
second reply arrives:

```text
CSI ? u   CSI c
```

A terminal that supports the protocol replies `CSI ? flags u` before its
[primary device attributes](../csi/queries.md) report. A terminal that does
not support it replies only to `CSI c`. This is the pattern kitty's own
documentation recommends.

## Interaction with legacy modes

- Flag `1` alone leaves printable keys as plain text and keeps
  [DECCKM](keyboard-legacy.md#cursor-keys) and keypad modes effective.
- Flag `8` overrides DECCKM: every key becomes `CSI … u` or the tilde form.
- The protocol ignores `modifyOtherKeys` while any flags are active; kitty
  maps `CSI > 4 ; 2 m` to flag `1` for applications written for xterm.
- Terminals that implement the protocol must still deliver bracketed paste
  and mouse events in their own encodings; the keyboard flags do not change
  them.

Ghostty, and therefore xterm+, has one important exception to the opt-in
boundary: its default fixterms encoder reports ambiguous Ctrl-I, Ctrl-M, and
Ctrl-[ combinations using CSI-u even while the Kitty flag value is zero. For
example, Ctrl-I sends `CSI 105 ; 5 u`, while Tab remains `0x09`. Seeing one of
these sequences does not mean that an application enabled Kitty flags; query
`CSI ? u` to determine the active protocol state. Traditional xterm collapses
these pairs instead. xterm+ records this substantial default-input difference
in `docs/compatibility/drift.md`.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Terminal | Support | Notes |
| --- | --- | --- |
| xterm | ? | Not implemented[^xterm] |
| VTE | ? | |
| Konsole | ? | |
| kitty | Yes | Origin, all flags[^kitty] |
| WezTerm | Yes | `enable_kitty_keyboard`; default off[^wezterm] |
| Ghostty | Yes | All flags[^ghostty] |
| foot | Yes | All flags[^foot] |
| Alacritty | Yes | Since 0.13[^alacritty] |
| Contour | Yes | Listed in its VT extensions[^contour] |
| mintty | ? | |
| PuTTY | ? | |
| Windows Terminal | ? | Open feature request[^wt] |
| Apple Terminal | ? | |
| iTerm2 | Yes | Since 3.5[^iterm2] |
| xterm.js | ? | Open feature request[^xtermjs] |
| tmux | Partial | Forwards with `extended-keys`; does not implement the stack itself[^tmux] |

<!-- markdownlint-enable MD013 -->

[^xterm]: Not present in [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).
[^kitty]: [Comprehensive keyboard handling in terminals](https://sw.kovidgoyal.net/kitty/keyboard-protocol/).
[^wezterm]: [WezTerm `enable_kitty_keyboard`](https://wezterm.org/config/lua/config/enable_kitty_keyboard.html).
[^ghostty]: [Ghostty VT reference](https://ghostty.org/docs/vt).
[^foot]: [foot README, keyboard section](https://codeberg.org/dnkl/foot#keyboard).
[^alacritty]: [Alacritty 0.13 changelog](https://github.com/alacritty/alacritty/blob/master/CHANGELOG.md).
[^contour]: [Contour VT extensions](https://contour-terminal.org/vt-extensions/).
[^wt]: [microsoft/terminal issue on kitty keyboard protocol](https://github.com/microsoft/terminal/issues/16155).
[^iterm2]: [iTerm2 3.5 release notes](https://iterm2.com/downloads.html).
[^xtermjs]: [xtermjs/xterm.js issue tracker](https://github.com/xtermjs/xterm.js/issues).
[^tmux]: [tmux(1), `extended-keys`](https://man.openbsd.org/tmux.1#extended-keys).

## Probe

```sh
tools/query '\033[?u\033[c'
```

A reply beginning `^[[?0u` or `^[[?1u` means the protocol is available. To
watch encoded keys:

```sh
printf '\033[>1u'; cat -v; printf '\033[<u'
```

Press Escape; with the protocol active it prints `^[[27u` rather than `^[`.

## Sources

- [Comprehensive keyboard handling in terminals](https://sw.kovidgoyal.net/kitty/keyboard-protocol/)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
