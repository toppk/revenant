# CSI cursor controls

Status: Standard (movement), DEC (save/restore, style, visibility).

The cursor has a position, a saved position, and an appearance with three
independent dimensions: visibility, shape, and blinking. Applications should
not assume that changing one dimension changes another.

## Movement

<!-- markdownlint-disable MD013 -->

| Name | Sequence | Default | Effect |
| --- | --- | --- | --- |
| CUU | `CSI Ps A` | 1 | Up `Ps` rows; stops at the top margin |
| CUD | `CSI Ps B` | 1 | Down `Ps` rows; stops at the bottom margin |
| CUF | `CSI Ps C` | 1 | Forward `Ps` columns; stops at the right margin |
| CUB | `CSI Ps D` | 1 | Back `Ps` columns; stops at the left margin |
| CNL | `CSI Ps E` | 1 | Down `Ps` rows, to column 1 |
| CPL | `CSI Ps F` | 1 | Up `Ps` rows, to column 1 |
| CHA | `CSI Ps G` | 1 | To column `Ps` in the current row |
| CUP | `CSI Ps ; Ps H` | 1;1 | To row, column |
| HVP | `CSI Ps ; Ps f` | 1;1 | Same as CUP |
| VPA | `CSI Ps d` | 1 | To row `Ps` in the current column |
| HPA | `CSI Ps \`` | 1 | To column `Ps` (rarely used; CHA is equivalent) |
| CHT | `CSI Ps I` | 1 | Forward `Ps` tab stops |
| CBT | `CSI Ps Z` | 1 | Back `Ps` tab stops |

<!-- markdownlint-enable MD013 -->

A parameter of 0 is treated as 1. Movement never scrolls; `CUD` at the
bottom margin stays there, unlike `LF` or `IND`.

Coordinates are 1-based and, under DEC origin mode (`?6`, DECOM), relative to
the scrolling region. With origin mode reset, the cursor may be placed outside
the region with `CUP`, but relative motion still clamps to margins.

`CUP` with a row or column beyond the screen clamps to the edge. Emulators
do not report an error.

### Pending wrap

After writing a character in the last column, DEC terminals and every serious
emulator enter a *pending wrap* state: the cursor is still drawn in the last
column and the next printable character wraps first. Any explicit cursor
movement, including `CSI 0 C`, clears the pending state without moving.
Applications that write exactly one row of text and then position the cursor
depend on this; applications that assume the cursor is already on the next
row do not work.

## Save and restore

```text
ESC 7      DECSC   save cursor position, attributes, charset, origin mode, pending wrap
ESC 8      DECRC   restore them
CSI s      SCOSC   save position only (ANSI.SYS); conflicts with DECSLRM when ?69 is set
CSI u      SCORC   restore position only
```

`DECSC`/`DECRC` are the portable pair. `CSI s` is ambiguous: with left/right
margin mode enabled, xterm interprets `CSI Pl ; Pr s` as DECSLRM. Alternate
screen mode `?1049` performs its own save and restore; see [Modes](modes.md).

## Style: DECSCUSR

Status: DEC (VT520), with emulator extensions for parameter 0.

```text
CSI Ps SP q
```

`SP` is a literal space byte, not notation.

<!-- markdownlint-disable MD013 -->

| `Ps` | Description |
| --- | --- |
| `0` | Default cursor style; the meaning of "default" differs between emulators |
| `1` | Blinking block |
| `2` | Steady block |
| `3` | Blinking underline |
| `4` | Steady underline |
| `5` | Blinking bar (xterm extension, now universal) |
| `6` | Steady bar (xterm extension, now universal) |

<!-- markdownlint-enable MD013 -->

The VT520 manual defines 0 as "blinking block" and 1 the same. xterm and most
emulators treat 0 as "return to the configured default", which may be any
shape and either blink state. Parameter 0 is therefore suitable only when an
application wants the emulator's reset behavior; it cannot restore an unknown
previous application state. Programs that need to restore should query with
DECRQSS (`DCS $ q SP q ST`) first; see [DECRQSS](../dcs/decrqss.md).

## Visibility: DECTCEM

```text
CSI ? 25 h    show the cursor
CSI ? 25 l    hide the cursor
```

Visibility is independent of style. A forced blink policy must still honor a
hidden cursor. Full-screen programs hide the cursor while painting and show it
after; a crash leaves it hidden, and `reset(1)` or `tput cnorm` recovers.

## Blinking: DEC mode 12

```text
CSI ? 12 h    start blinking
CSI ? 12 l    stop blinking
```

Mode 12 (`att610` in xterm's naming) changes blinking without selecting a
shape. Several emulators let configuration override or ignore it.

## Observed compatibility

Observed on 2026-08-25 with `tools/sendcsi blink-block`, then
`tools/sendcsi default`.

<!-- markdownlint-disable MD013 -->

| Emulator | Version | Initial cursor | After `blink-block` | After `default` |
| --- | --- | --- | --- | --- |
| xterm | 406 | Steady | Blinking | Blinking |
| WezTerm | `20260820_005713_e95b3713` | Steady | Blinking | Steady |
| Ghostty | 1.3.1, default configuration | Blinking | Blinking | Blinking |

<!-- markdownlint-enable MD013 -->

This table describes outcomes, not motives. Configuration changes every row;
an implementation's documentation remains authoritative for its intent.

## Documented compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| DECSCUSR 1–6 | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Yes (`Ss`/`Se`) |
| DECSCUSR 0 = configured default | Yes | Yes | ? | Yes | Yes | Yes | Yes | ? | ? | ? | ? | ? | ? | pass-through |
| DECTCEM `?25` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Mode `?12` | Yes | Yes | ? | ? | Yes | Yes | ? | ? | ? | Yes | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

Sources for the Yes cells are the escape-sequence references listed under
each terminal in [Terminals](../terminals/index.md). Cells marked `?` are
contribution targets.

## Probe

```sh
tools/sendcsi steady-block
tools/sendcsi blink-block
tools/sendcsi blink-steady
tools/sendcsi default
tools/query cursor                # CPR: expect ^[[row;colR
tools/query decrqss ' q'         # DECRQSS for DECSCUSR; reply carries the style
printf '%*s' "$(tput cols)" x; tools/query cursor   # pending wrap: column stays at the edge
```

## Sources

- [DEC VT520 programmer reference, DECSCUSR](https://vt100.net/docs/vt510-rm/DECSCUSR.html)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [XTerm manual](https://invisible-island.net/xterm/manpage/xterm.html)
- [WezTerm escape-sequence reference](https://wezterm.org/escape-sequences.html)
- [Ghostty configuration reference](https://ghostty.org/docs/config/reference#cursor-style-blink)
- [xterm.js supported sequences](https://xtermjs.org/docs/api/vtfeatures/)
