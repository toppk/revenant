# DCS: Device Control String

Status: Standard (ECMA-48) framing; individual commands are DEC, xterm, or
extension.

DCS carries a command plus an opaque payload. Where CSI ends at its final
byte, DCS continues until a String Terminator, which is what lets it carry
Sixel images, terminfo values, and other data that cannot fit a numeric
parameter list.

## Syntax

```text
DCS parameter-bytes intermediate-bytes final-byte data ST
```

`DCS` is `ESC P` (or C1 `0x90`); `ST` is `ESC \` (or C1 `0x9c`). ECMA-48
defines only the framing; the header comes from DEC STD 070, which gives a
DCS the same parameter, intermediate, and final bytes as
[CSI](../csi/anatomy.md) and identifies the command by its intermediates
and final byte. The data that follows is command-specific and may contain
any bytes except the terminator. Most emulators also terminate a DCS on
`BEL`, `CAN`, `SUB`, or a bare `ESC` that does not begin `ST`, but only
`ST` is standard. The family as a whole, including APC, PM, and SOS, is
described in [Control strings](../control-strings.md).

DEC required the final byte to be private (`p`–`~`) and never used a
private marker in a DCS header. Both rules have since been broken: the
XTVERSION reply is `DCS > | text ST`, and the first synchronized-updates
draft used `DCS = 1 s ST` before
[mode 2026](../csi/modes.md#synchronized-output) replaced it. ECMA-48's own
mechanism for typing a DCS, IDCS (`CSI Pn SP O`) sent before the string,
was never implemented by anyone.

## Members

<!-- markdownlint-disable MD013 -->

| Sequence | Name | Direction | Page |
| --- | --- | --- | --- |
| `DCS $ q Pt ST` | DECRQSS | app → term | [DECRQSS](decrqss.md) |
| `DCS Ps $ r Pt ST` | DECRPSS | term → app | [DECRQSS](decrqss.md) |
| `DCS + q Pt ST` | XTGETTCAP | app → term | [XTGETTCAP](xtgettcap.md) |
| `DCS Ps + r Pt ST` | XTGETTCAP reply | term → app | [XTGETTCAP](xtgettcap.md) |
| `DCS > \| Pt ST` | XTVERSION reply | term → app | [XTVERSION](xtversion.md) |
| `DCS Pm q data ST` | Sixel | app → term | [Sixel](../graphics/sixel.md) |
| `DCS Pm p data ST` | ReGIS | app → term | [Graphics](../graphics/index.md) |
| `DCS Pfn ; Pcn \| keys ST` | DECUDK | app → term | [Other DCS](misc.md) |
| `DCS + p Pt ST` | XTSETTCAP | app → term | [Other DCS](misc.md) |
| `DCS tmux ; data ST` | tmux passthrough | app → tmux | below |
| `DCS 1000 p` | tmux control mode | tmux → client | [Other DCS](misc.md) |

<!-- markdownlint-enable MD013 -->

## Parsing requirements

The payload is opaque until `ST`. A parser must not interpret bytes inside
the string as CSI or text, and must not dispatch until the terminator
arrives or a size limit is hit. Because Sixel payloads can be megabytes, an
emulator either streams the data to the command handler as it arrives
(xterm, foot, and libghostty do this for Sixel) or imposes a hard limit and
discards the remainder.

Applications must never send a DCS without its terminator. A missing `ST`
leaves the terminal consuming all further output silently until something
happens to terminate the string; the usual symptom is a "hung" terminal
that recovers on `printf '\033\\'`.

Because the payload may contain `ESC`, the seven-bit `ESC \` terminator
creates an ambiguity for any payload that legitimately contains `ESC`. Real
DCS commands avoid it: Sixel and terminfo data are printable, and tmux's
passthrough doubles embedded escapes.

## tmux passthrough

```text
DCS tmux ; payload ST
```

When `allow-passthrough` is `on` (or `all`), tmux unwraps the payload and
writes it to the outer terminal without interpretation. Every `ESC` inside
the payload must be doubled (`ESC ESC`) so that tmux can find the real `ST`.
A shell example that sends XTVERSION through tmux:

```sh
printf '\033Ptmux;\033\033[>0q\033\\'
```

The outer terminal's reply arrives at the outer terminal's PTY input, which
tmux owns; tmux forwards recognized replies (DA, DECRQM) to the active pane
in recent versions, but an application cannot rely on receiving replies to
passthrough queries. See [tmux](../terminals/tmux.md).

## Sources

- [ECMA-48 §8.3.27 (DCS) and §8.3.143 (ST)](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [XTerm Control Sequences: Device-Control functions](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Device-Control-functions)
- [tmux(1): allow-passthrough](https://man.openbsd.org/tmux#allow-passthrough)
- [DEC STD 070 Video Systems Reference Manual §3.5.4.1](http://bitsavers.org/pdf/dec/standards/EL-SM070-00_DEC_STD_070_Video_Systems_Reference_Manual_Dec91.pdf)
- [terminal-wg N0001, Existing terminal sequence structures §4.1–4.2](https://gitlab.freedesktop.org/terminal-wg/terminal-parsing/-/blob/master/doc/n0001.md)
- [Control strings](../control-strings.md)

<!-- tdn:compatibility -->
