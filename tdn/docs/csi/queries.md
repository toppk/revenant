# CSI queries and reports

Status: Standard (DSR, DA), DEC (DECRQM, DA2, DA3), xterm (XTVERSION and
window reports).

Some controls produce bytes in the reverse direction. The application writes
a query to terminal output; the terminal writes its report to PTY input,
where it is interleaved with keyboard and mouse events.

## Common exchanges

<!-- markdownlint-disable MD013 -->

| Request | Report | Purpose |
| --- | --- | --- |
| `CSI 5 n` | `CSI 0 n` | Device status: "OK" |
| `CSI 6 n` | `CSI row ; col R` | Cursor position (CPR); relative to origin under DECOM |
| `CSI ? 6 n` | `CSI ? row ; col R` | Extended CPR (DECXCPR); unambiguous against `CSI 1;1 R` F3 reports |
| `CSI c` / `CSI 0 c` | `CSI ? Pm c` | Primary device attributes (DA1): conformance level and feature list |
| `CSI > c` | `CSI > Pp ; Pv ; Pc c` | Secondary DA: terminal type, version, ROM cartridge |
| `CSI = c` | `DCS ! \| hex ST` | Tertiary DA: unit id; mostly zeros |
| `CSI > 0 q` | `DCS > \| name ( version ) ST` | XTVERSION; see [XTVERSION](../dcs/xtversion.md) |
| `CSI ? Ps $ p` | `CSI ? Ps ; Pm $ y` | DECRQM; see [Modes](modes.md) |
| `CSI ? u` | `CSI ? flags u` | Kitty keyboard flags; see [Input](../input/kitty-keyboard.md) |
| `CSI Ps t` (14, 16, 18) | `CSI Ps ; h ; w t` | Window sizes; see [Window operations](window-ops.md) |
| `CSI ? Pi ; Pa ; Pv S` | `CSI ? Pi ; Ps ; Pv S` | XTSMGRAPHICS: Sixel registers and geometry |
| `DCS $ q Pt ST` | `DCS 1 $ r … ST` | DECRQSS; see [DCS](../dcs/decrqss.md) |
| `DCS + q hex ST` | `DCS 1 + r … ST` | XTGETTCAP; see [DCS](../dcs/xtgettcap.md) |
| `OSC 10 ; ? ST` | `OSC 10 ; rgb:… ST` | Color queries; see [OSC colors](../osc/colors.md) |

<!-- markdownlint-enable MD013 -->

## Device attributes

DA1 is the oldest and most reliable "is anyone there" probe. Every emulator
answers, and the answer identifies a DEC conformance level and features:

<!-- markdownlint-disable MD013 -->

| Reply | Claimed identity | Feature codes seen |
| --- | --- | --- |
| `CSI ? 1 ; 2 c` | VT100 with AVO | |
| `CSI ? 6 c` | VT102 | Common in minimal emulators |
| `CSI ? 62 ; … c` | VT220 | `1` 132 columns, `2` printer, `4` Sixel, `6` selective erase, `9` NRCS, `15` technical charset, `16` locator, `17` terminal state, `18` windowing, `21` horizontal scrolling, `22` color, `28` rectangular editing, `29` ANSI text locator |
| `CSI ? 63 ; … c`, `64`, `65` | VT320, VT420, VT520 | Same list |

<!-- markdownlint-enable MD013 -->

Feature `4` is the only sanctioned Sixel detection. xterm's default reply
depends on `decTerminalID`; kitty answers `CSI ? 62 ; c`; Windows Terminal
answers as a VT220 with `1;4;6;7;14;21;22;23;24;28;32;42` (feature `4`
present since 1.22); Alacritty `CSI ? 6 c`.

DA2 replies `CSI > Pp ; Pv ; Pc c` where `Pp` is a terminal type (`0` VT100,
`1` VT220, `41` VT420, `61` VT510, `64` VT520) and `Pv` a version. xterm
reports its patch level as `Pv`; many emulators copy `CSI > 1 ; 10 ; 0 c`
or `CSI > 0 ; 95 ; 0 c` verbatim and the number means nothing. Use
XTVERSION for identification, DA2 only as a fallback.

## Application requirements

Reports are asynchronous input. A robust application must:

- put the terminal in raw or cbreak mode before writing the query;
- distinguish reports from user input and from enabled mouse and focus
  protocols, which share final bytes (`R` from CPR and from F3 with
  modifiers under some encodings);
- accept reports split across reads or adjacent to other input;
- impose a timeout without assuming unsupported terminals reply;
- restore input modes even if the query fails;
- never pass reply bytes to a shell, a format string, or a title.

A reliable pattern sends the uncertain query followed by DA1 in one write.
DA1 always answers, so if DA1's reply arrives without the other, the other
is unsupported and no timeout is needed:

```sh
printf '\033[?u\033[c'      # kitty keyboard flags, then DA1
```

Sending a query and immediately performing a blocking read is unsafe when
another input consumer can run concurrently; multiplexers, editors with
job control, and shells with asynchronous plugins all do this.

## Emulator requirements

An emulator should generate each report as one logical write and enqueue it
through the same lossless PTY-input path used for encoded keys, mouse
reports, and focus reports. A short write must not truncate the response.

Answers should reflect the terminal state associated with the query, not
stale renderer state; CPR after a sequence of cursor moves must report the
final position even if nothing has been drawn.

Unsupported or disallowed queries should follow the protocol: DECRQM
answers `0`, DECRQSS answers invalid, XTGETTCAP answers `DCS 0 + r ST`,
XTVERSION and OSC color queries answer nothing. Inventing plausible
responses is worse than silence because it defeats the DA1 fallback.

## Multiplexers

tmux answers DA, DSR, and DECRQM itself from its own model of the pane and
does not relay most queries to the outer terminal; XTVERSION reports
`tmux 3.x`. Applications under tmux therefore see tmux's capabilities, which
is the intended behavior and the reason tmux has `terminal-features`.

## Probe

```sh
tools/query da1 da2 version
tools/query cursor
tools/query '\033[?6n'          # DECXCPR
tools/query -r da1 | od -c      # raw bytes
```

## Sources

- [ECMA-48 §8.3.35 DSR, §8.3.24 DA](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [VT510 DA1](https://vt100.net/docs/vt510-rm/DA1.html), [DA2](https://vt100.net/docs/vt510-rm/DA2.html), [DECXCPR](https://vt100.net/docs/vt510-rm/DECXCPR.html)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [Windows Terminal: sequences supported](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences)
- [kitty: keyboard protocol detection](https://sw.kovidgoyal.net/kitty/keyboard-protocol/#detection-of-support-for-this-protocol)
