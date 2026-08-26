# CSI scrolling and margins

Status: Standard (SU/SD), DEC (margins), xterm (scrollback controls).

Scrolling moves rows within the *scrolling region*, which defaults to the
whole screen. Only text scrolled off the top of the primary screen's region
enters scrollback, and only when the region is the full screen in most
emulators.

## Scroll commands

<!-- markdownlint-disable MD013 -->

| Name | Sequence | Default | Effect |
| --- | --- | --- | --- |
| SU | `CSI Ps S` | 1 | Scroll region up `Ps` rows; blank rows enter at the bottom |
| SD | `CSI Ps T` | 1 | Scroll region down `Ps` rows; blank rows enter at the top |
| IND | `ESC D` | | Cursor down; scrolls at the bottom margin |
| RI | `ESC M` | | Cursor up; scrolls at the top margin |
| NEL | `ESC E` | | Like CR LF |
| LF, VT, FF | `0x0a`, `0x0b`, `0x0c` | | Cursor down; scroll at the bottom margin; CR too if LNM (`CSI 20 h`) |

<!-- markdownlint-enable MD013 -->

`CSI Ps T` with five parameters is xterm's mouse highlight-tracking
initiator (`XTHIMOUSE`), and `CSI > Ps T` resets title-mode features.
Parsers must dispatch on the parameter count and private marker.

## Vertical margins: DECSTBM

```text
CSI Pt ; Pb r      set top and bottom margins (1-based, inclusive)
CSI r              reset to the full screen
```

Setting margins moves the cursor to the home position (respecting origin
mode). A region of fewer than two rows is rejected. Applications use DECSTBM
to keep a status line fixed while output scrolls above it; `DECRQSS` with
`r` reads it back.

## Horizontal margins: DECSLRM

Status: DEC (VT420), opt-in.

```text
CSI ? 69 h         DECLRMM: enable left/right margin mode
CSI Pl ; Pr s      DECSLRM: set left and right margins
CSI ? 69 l         disable; `CSI s` reverts to SCOSC
```

Support exists in xterm, WezTerm, Ghostty, foot, Contour, and Windows
Terminal; VTE, Alacritty, and xterm.js do not implement it (xterm.js
documents it as unsupported). Because `CSI s` changes meaning, applications
must check `?69` support with DECRQM before enabling it.

## Scrollback

Scrollback is outside every standard. Emulator conventions:

<!-- markdownlint-disable MD013 -->

| Control | Meaning | Status |
| --- | --- | --- |
| `CSI 3 J` | Clear scrollback | xterm |
| `?1049` | Alternate screen, which has no scrollback | xterm |
| `?1007` | Alternate scroll: wheel sends arrow keys on the alternate screen | xterm |
| `?1010` | Scroll to bottom on output | xterm |
| `?1011` | Scroll to bottom on key press | xterm |
| `CSI ? Ps ; … t`, `Ps` = 14 | Reserved by xterm for scrollback size reports; not implemented | |

<!-- markdownlint-enable MD013 -->

When the scrolling region is smaller than the screen, rows scrolled off its
top are discarded, not archived. Terminals differ when the region starts at
row 1 but ends above the bottom; xterm archives, several others do not.
Applications that want output preserved should leave the region at the
full screen.

## Reverse wraparound

`?45` (xterm `reverseWrap`) lets BS at column 1 move to the end of the
previous row. Off by default; used by shells that redraw long prompts.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SU/SD | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| DECSTBM | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| DECSLRM / `?69` | Yes | ? | ? | ? | Yes | Yes | Yes | ? | Yes | ? | Yes | ? | Yes | No[^xjs] | ? |
| `?1007` alternate scroll | Yes | Yes | ? | ? | Yes | Yes | Yes | Yes | ? | ? | Yes | ? | Yes | Yes | ? |
| `?45` reverse wrap | Yes | ? | ? | ? | Yes | Yes | ? | ? | Yes | ? | ? | ? | ? | Yes | ? |

<!-- markdownlint-enable MD013 -->

[^xjs]: [xterm.js supported terminal sequences](https://xtermjs.org/docs/api/vtfeatures/) lists DECSLRM as unsupported.

## Probe

```sh
printf '\033[5;10r'; seq 1 30; printf '\033[r'      # only rows 5-10 scroll
tools/query decrqss r                      # DECRQSS: expect ^[P1$r5;10r^[\ before the reset
tools/query mode 69                        # DECRQM: 1/2 supported, 0 unknown
```

## Sources

- [ECMA-48 §8.3.147, §8.3.113](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [VT510 DECSTBM](https://vt100.net/docs/vt510-rm/DECSTBM.html), [DECSLRM](https://vt100.net/docs/vt510-rm/DECSLRM.html)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [xterm.js supported sequences](https://xtermjs.org/docs/api/vtfeatures/)
