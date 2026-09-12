# Control strings: DCS, OSC, APC, PM, SOS

Status: Standard (ECMA-48 framing); DEC (internal structure); xterm (OSC
selector numbering, `BEL`).

A control string is an introducer, an opaque payload, and a String
Terminator. Where an ESC or CSI sequence ends at its final byte, a control
string continues until `ST`, which is what lets it carry titles, terminfo
values, images, and other data that cannot fit a numeric parameter list.
ECMA-48 defines five introducers, the terminator, and which bytes may
appear between them, but no internal field structure. This page covers the family; [DCS](dcs/index.md)
and [OSC anatomy](osc/anatomy.md) cover the two members in daily use.

## Syntax

```text
command-string:    (DCS | OSC | PM | APC) data ST
character-string:   SOS                    data ST
```

<!-- markdownlint-disable MD013 -->

| Introducer | 7-bit | C1 | Name | Used for |
| --- | --- | --- | --- | --- |
| DCS | `ESC P` | `0x90` | Device Control String | DECRQSS, XTGETTCAP, Sixel, passthrough; see [DCS](dcs/index.md) |
| OSC | `ESC ]` | `0x9d` | Operating System Command | Titles, colors, clipboard, hyperlinks; see [OSC](osc/index.md) |
| APC | `ESC _` | `0x9f` | Application Program Command | [Kitty graphics](graphics/kitty.md); otherwise ignored |
| PM | `ESC ^` | `0x9e` | Privacy Message | Nothing in current emulators; consumed and ignored |
| SOS | `ESC X` | `0x98` | Start of String | Nothing; consumed and ignored where recognized |
| ST | `ESC \` | `0x9c` | String Terminator | Ends all of the above |

<!-- markdownlint-enable MD013 -->

ECMA-48 restricts the payload of a *command string* (DCS, OSC, PM, APC) to
the bytes `0x08`–`0x0d` and `0x20`–`0x7e`: printable ASCII plus BS, HT, LF,
VT, FF, and CR. A *character string* (SOS) may contain anything except SOS
and ST. Neither rule anticipated UTF-8 payloads, which emulators accept in
practice for OSC titles and hyperlinks without any standard saying so; see
[OSC anatomy](osc/anatomy.md#parser-requirements) for what emulators
actually do with control bytes and invalid UTF-8, and the per-feature
compatibility tables for which emulators implement each OSC at all.

`BEL` as a terminator is an xterm convention for OSC only. It is not part
of ECMA-48, does not terminate DCS in the reference parser, and cannot be
carried through [multiplexer passthrough](practices/multiplexers.md#passthrough).

## Internal structure

ECMA-48 leaves the payload opaque and offers IDCS (`CSI Pn SP O`) to
announce the type of the DCS that follows. Nobody implemented IDCS. The
structure emulators actually parse comes from DEC STD 070, which gives
each command string a CSI-like header:

```text
DCS P* I* Ft data ST      parameters, intermediates, final byte, then data
OSC    I* Ft data ST      intermediates and final byte only
APC    I* Ft data ST
PM     I* Ft data ST
```

DEC used only the DCS form, and required the final byte to be in the
private range `p`–`~`. Several other vendors' terminals used standard-range
finals, and so does the one modern APC in use: kitty's graphics protocol is
`APC G … ST`. Private markers (`<`, `=`, `>`, `?`) never appeared in a DCS
header until xterm's XTVERSION reply (`DCS > | text ST`) and the original
synchronized-updates draft (`DCS = 1 s ST`, superseded by
[mode 2026](csi/modes.md#synchronized-output)).

OSC took a different path. xterm ignored the DEC form and numbered its
commands instead:

```text
OSC Ps ; Pt ST
OSC Ps ; Pt BEL
```

The two forms coexist without ambiguity, because the DEC form has no
digits or `;` before its final byte. Sun's shelltool used the DEC form
(`OSC l`, `OSC I`, `OSC L`); everything since uses xterm's, and
[OSC anatomy](osc/anatomy.md) documents that grammar and its history.

tmux's passthrough, `DCS tmux ; payload ST`, is a DCS with the DEC header
read literally: `t` is the final byte and `mux;` begins the payload. It is
a tmux-specific DCS form, not a new introducer, and
[DCS](dcs/index.md#tmux-passthrough) documents it.

PM and SOS have no structure in practice. jexer sends plain-text PM
strings for its own use; no emulator listed on TDN acts on either. A
parser still has to recognize both introducers so that the payload is
swallowed rather than printed.

## Non-standard introducers

ECMA-48 says its opening delimiters are the ones "defined in this
Standard", which leaves room for others. One is in daily use:

```text
ESC k name ST
```

GNU screen's window-name string. `ESC k` is otherwise an unassigned
single-function escape, and screen gives it a payload and a terminator.
tmux accepts it too, subject to `allow-rename`; no emulator outside a
multiplexer acts on it, and a parser that does not know it sees `ESC k`
followed by printable text. See [Multiplexers](practices/multiplexers.md).

Historical terminals also used `PU1`, `PU2`, and `SS3 P` as introducers;
none survive in current emulators.

## Parser requirements

The VT500 reference parser treats every command string the same way once
the introducer is seen: collect until `ST`, or until something aborts.

- `ESC` followed by `\` is `ST`. `ESC` followed by anything else aborts
  the string and starts a new escape; the collected payload is discarded.
- `CAN` and `SUB` abort.
- `BEL` terminates OSC as an xterm convention that emulators generally
  follow; see [OSC terminators](osc/anatomy.md#terminators). Some also
  accept it for DCS and APC, but the reference parser does not, and
  applications must not rely on it outside OSC.
- The C1 introducers and `0x9c` are valid UTF-8 continuation bytes.
  Emulators in UTF-8 mode either disable them or accept them only when
  8-bit controls are explicitly enabled; see
  [C1 controls](escape.md#c1-controls).
- Payload length is unbounded in the grammar, so every emulator imposes a
  limit and either truncates or drops the string; see
  [OSC length limits](osc/anatomy.md#length-limits).
- A missing `ST` swallows all following printable output until a
  terminator or an aborting `ESC`, `CAN`, or `SUB` arrives. The usual
  recovery is `printf '\033\\'`.

An emulator that does not implement a family (PM, SOS, APC without kitty
graphics) still has to consume the string. Printing the payload is the
common bug in minimal parsers, and the reason applications should assume
nothing about a terminal they have not identified.

## Probe

```sh
printf '\033_hidden\033\\visible\n'     # APC: only "visible" should print
printf '\033^hidden\033\\visible\n'     # PM
printf '\033Xhidden\033\\visible\n'     # SOS: some emulators print "hidden"
printf '\033kname\033\\'                # screen/tmux window name; ignored elsewhere
```

## Sources

- [ECMA-48 §5.6 control strings; §8.3.2 APC, §8.3.27 DCS, §8.3.89 OSC, §8.3.94 PM, §8.3.128 SOS, §8.3.143 ST](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [DEC STD 070 Video Systems Reference Manual §3.5.4, control string formats](http://bitsavers.org/pdf/dec/standards/EL-SM070-00_DEC_STD_070_Video_Systems_Reference_Manual_Dec91.pdf)
- [terminal-wg N0001, Existing terminal sequence structures §4](https://gitlab.freedesktop.org/terminal-wg/terminal-parsing/-/blob/master/doc/n0001.md)
- [A parser for DEC's ANSI-compatible video terminals](https://vt100.net/emu/dec_ansi_parser)
- [Kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/)
- [iTerm2 synchronized updates draft](https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec)
- [screen(1), Naming windows](https://www.gnu.org/software/screen/manual/screen.html#Naming-Windows)
- [tmux(1), allow-rename](https://man.openbsd.org/tmux#allow-rename)
