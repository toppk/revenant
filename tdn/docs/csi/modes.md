# CSI modes

Status: Standard (ANSI modes), DEC (private modes), xterm and Extension for
the numbers above 1000.

Modes are persistent terminal switches. They stay in effect until reset,
until a terminal reset (`RIS`, `DECSTR`) changes them, or until the PTY
session ends.

## Syntax

```text
CSI Pm h        SM: set one or more ANSI modes
CSI Pm l        RM: reset one or more ANSI modes
CSI ? Pm h      DECSET: set one or more DEC private modes
CSI ? Pm l      DECRST: reset one or more DEC private modes
CSI Ps $ p      DECRQM: query an ANSI mode
CSI ? Ps $ p    DECRQM: query a private mode
```

The `?` selects a separate namespace; ANSI mode 4 (insert) and private mode
`?4` (smooth scroll) are unrelated. Multiple parameters are set in order:

```text
CSI ? 1000 ; 1006 h
```

Applications should tolerate terminals that honor only part of a combined
request, and should never combine a mode whose failure is harmful with
others.

### DECRQM replies

```text
CSI ? Ps ; Pm $ y
```

<!-- markdownlint-disable MD013 -->

| `Pm` | Meaning |
| --- | --- |
| `0` | Mode not recognized |
| `1` | Set |
| `2` | Reset |
| `3` | Permanently set |
| `4` | Permanently reset |

<!-- markdownlint-enable MD013 -->

DECRQM is the safest feature-detection tool for modes: a terminal that
does not implement DECRQM at all sends nothing, so a timeout is still
required. See [Queries](queries.md).

## ANSI modes

<!-- markdownlint-disable MD013 -->

| Mode | Name | Effect |
| --- | --- | --- |
| `2` | KAM | Keyboard action: lock the keyboard; rarely implemented |
| `4` | IRM | Insert mode; printing shifts the rest of the row right |
| `12` | SRM | Send/receive: *reset* means local echo; almost never implemented |
| `20` | LNM | Line feed/new line: LF also performs CR |

<!-- markdownlint-enable MD013 -->

## DEC private modes

### Screen and cursor

<!-- markdownlint-disable MD013 -->

| Mode | Name | Effect | Status |
| --- | --- | --- | --- |
| `?1` | DECCKM | Application cursor keys: arrows send `SS3 A` instead of `CSI A` | DEC |
| `?2` | DECANM | VT52 mode when reset; do not use | DEC |
| `?3` | DECCOLM | 132 columns when set; also clears the screen unless `?40` is reset | DEC |
| `?4` | DECSCLM | Smooth scroll; ignored by most emulators | DEC |
| `?5` | DECSCNM | Reverse video for the whole screen | DEC |
| `?6` | DECOM | Origin mode: cursor addressing is relative to the scrolling region | DEC |
| `?7` | DECAWM | Autowrap; reset means text overwrites the last column | DEC |
| `?8` | DECARM | Autorepeat | DEC |
| `?12` | | Cursor blink | xterm (att610) |
| `?25` | DECTCEM | Cursor visible | DEC |
| `?40` | | Allow `?3` to resize | xterm |
| `?45` | | Reverse wraparound | xterm |
| `?47` | | Alternate screen (no cursor save) | xterm |
| `?66` | DECNKM | Application keypad | DEC |
| `?67` | DECBKM | Backspace sends BS rather than DEL | DEC |
| `?69` | DECLRMM | Enable left/right margins | DEC |
| `?80` | DECSDM | Sixel display mode; polarity is disputed, see [Sixel](../graphics/sixel.md) | Disputed |
| `?1047` | | Alternate screen; clears it on return | xterm |
| `?1048` | | Save/restore cursor as DECSC/DECRC | xterm |
| `?1049` | | Save cursor, switch to alternate screen, clear it; reverse on reset | xterm |

<!-- markdownlint-enable MD013 -->

`?1049` is what full-screen programs use. The alternate screen has no
scrollback, and emulators that enable `?1007` translate wheel events to
arrow keys there.

### Input

<!-- markdownlint-disable MD013 -->

| Mode | Effect | Status |
| --- | --- | --- |
| `?9` | X10 mouse: button press only | xterm |
| `?1000` | Normal mouse: press and release | xterm |
| `?1001` | Highlight tracking; can hang the terminal if the application does not reply | xterm |
| `?1002` | Button-event: motion while a button is held | xterm |
| `?1003` | Any-event: all motion | xterm |
| `?1004` | Focus reporting: `CSI I` / `CSI O` | xterm |
| `?1005` | UTF-8 mouse coordinates (obsolete) | xterm |
| `?1006` | SGR mouse encoding | xterm |
| `?1007` | Alternate scroll | xterm |
| `?1015` | urxvt mouse encoding | Extension (rxvt-unicode) |
| `?1016` | SGR-pixels mouse encoding | xterm |
| `?1034` | Eight-bit Meta | xterm |
| `?1035` | Num Lock modifier handling | xterm |
| `?1036` | Meta sends ESC | xterm |
| `?1039` | Alt sends ESC | xterm |
| `?2004` | Bracketed paste | xterm |
| `?9001` | win32-input-mode | Vendor-private (ConPTY) |

<!-- markdownlint-enable MD013 -->

Details are in [Input](../input/index.md).

### Rendering and synchronization

<!-- markdownlint-disable MD013 -->

| Mode | Effect | Status |
| --- | --- | --- |
| `?1010` | Scroll to bottom on output | xterm |
| `?1011` | Scroll to bottom on key press | xterm |
| `?1070` | Sixel uses a private palette | xterm |
| `?2026` | Synchronized output: buffer rendering between set and reset | Extension (Contour) |
| `?2027` | Grapheme-cluster cell width | Extension (Contour) |
| `?2031` | Color-scheme change notifications | Extension (Contour) |
| `?2048` | In-band window-resize notifications | Extension |
| `?8452` | Sixel: cursor to the right of the image rather than below | xterm |

<!-- markdownlint-enable MD013 -->

#### Synchronized output

```text
CSI ? 2026 h    begin: the terminal stops presenting frames
… draw …
CSI ? 2026 l    end: present everything at once
```

Terminals impose a timeout (typically 1 s) after which they render anyway,
so a crashed application cannot freeze the display. Terminfo announces it
with the `Sync` user capability. Implemented by Contour, kitty, WezTerm,
Ghostty, foot, Alacritty, iTerm2, VTE, Windows Terminal, xterm.js, and
relayed by tmux 3.4+; not by xterm, PuTTY, or Apple Terminal.

## Ownership and cleanup

A mode belongs to the terminal session, not to a process. The terminal
cannot tell which nested program enabled it, so full-screen applications
must reset the modes they enable, including on error paths and signal
handlers.

Cleanup is not restoration. Resetting a mode chooses its reset state; it
does not recover an unknown value that another application selected
earlier. `DECRQM` before enabling, and restoring the answered state after,
is the only correct sequence; few programs do it.

`XTSAVE` / `XTRESTORE` (`CSI ? Pm s`, `CSI ? Pm r`) save and restore
private modes in xterm and several others, but with `?69` set `CSI Pm s`
means DECSLRM. The Kitty keyboard protocol solved the same problem with an
explicit stack; see [Kitty keyboard](../input/kitty-keyboard.md).

## Reset

```text
ESC c          RIS: hard reset; clears the screen and every mode
CSI ! p        DECSTR: soft reset; modes to defaults, screen preserved
```

`reset(1)` sends RIS plus terminfo `rs1`–`rs3`. Emulators differ on whether
RIS clears scrollback, the title stack, and private colors.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Mode | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| DECRQM | Yes | Yes | ? | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Partial |
| `?1049` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| `?2004` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| `?2026` | ? | Yes | ? | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Yes (3.4+) |
| `?2027` | ? | ? | ? | ? | ? | Yes | Yes | ? | Yes | ? | ? | ? | ? | ? | ? |
| `?2031` | ? | ? | ? | Yes | ? | ? | ? | ? | Yes | ? | ? | ? | ? | ? | ? |
| `?2048` | ? | ? | ? | Yes | Yes | Yes | Yes | ? | Yes | ? | ? | ? | ? | ? | ? |
| DECSTR | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | ? |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
tools/query mode 2026     # ^[[?2026;2$y = supported and reset; ;0 = unknown
tools/query mode 2027
tools/query mode 1049
tools/testfocus           # click away and back when prompted
```

## Sources

- [ECMA-48 §7.1](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [VT510 DECRQM](https://vt100.net/docs/vt510-rm/DECRQM.html)
- [XTerm Control Sequences, DECSET](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Functions-using-CSI-_-ordered-by-the-final-character_s_)
- [Synchronized output specification](https://gist.github.com/christianparpart/d8a62cc1ab659194337d73e399004036)
- [Grapheme cluster mode 2027](https://github.com/contour-terminal/terminal-unicode-core)
- [Color-scheme notifications (mode 2031)](https://contour-terminal.org/vt-extensions/color-palette-update-notifications/)
