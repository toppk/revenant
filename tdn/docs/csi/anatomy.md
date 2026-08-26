# CSI anatomy and parsing

Status: Standard (ECMA-48 §5.4).

## Syntax

```text
ESC [ parameter-bytes intermediate-bytes final-byte
```

`ESC` is `0x1b` and `[` is `0x5b`. An eight-bit environment may use the single
C1 byte `0x9b`, but the two-byte form is the interoperable choice in UTF-8
sessions, where `0x9b` is a valid continuation byte.

<!-- markdownlint-disable MD013 -->

| Part | Byte range | Purpose |
| --- | --- | --- |
| Parameter bytes | `0x30`–`0x3f` | Digits `0`–`9`, separators `;` and `:`, private markers `<`, `=`, `>`, `?` |
| Intermediate bytes | `0x20`–`0x2f` | Select a subfamily of the final command; `SP` means literal `0x20` |
| Final byte | `0x40`–`0x7e` | Terminates and identifies the control function |

<!-- markdownlint-enable MD013 -->

A private marker, if present, must be the first parameter byte. ECMA-48
reserves final bytes `0x70`–`0x7e` (`p`–`~`) for private use, which is why so
many DEC and xterm commands end in `p`, `q`, `t`, or `~`.

## Parameters and defaults

`Ps` is one numeric parameter, `Pm` several separated by `;`, `Pt` text. An
omitted or empty parameter is not universally equivalent to zero; the command
definition decides. `CSI H` moves to row 1, column 1 because the default is 1;
`CSI J` erases to the end of the screen because the default is 0.

Parameters are decimal, non-negative, and unbounded in the grammar. Parsers
impose limits: xterm caps the count at 30 and values at 65535; other
emulators use 16 or 32 parameters. Values past the limit are discarded, not
wrapped.

## Sub-parameters

ECMA-48 allows `:` to separate sub-parameters within one parameter. Only SGR
uses this in practice:

```text
CSI 4 : 3 m                curly underline
CSI 58 : 2 : : 255 : 0 : 0 m   underline color, truecolor
CSI 38 : 2 : : r : g : b m     foreground, with empty color-space id
```

The colon form is unambiguous when parameters are combined, because the
parser knows how many sub-parameters belong to `38`. The semicolon form
`38;2;r;g;b` is far more widely supported but cannot be skipped by a terminal
that does not understand it. See [SGR](sgr.md#color-parameter-forms).

Terminals that predate sub-parameters may treat `:` as a parameter
terminator or ignore the whole sequence. Applications should consult the
terminfo `Smulx`/`Setulc` capabilities or probe before relying on colons.

## Reading examples

```text
CSI 31 m         SGR foreground color 1
CSI ? 25 l       reset DEC private mode 25 (hide cursor)
CSI 2 SP q       DECSCUSR steady block
CSI > 4 ; 2 m    xterm modifyOtherKeys level 2
CSI 1 ; 5 A      cursor up, modifier 5 (Ctrl) — this is a key report, not a command
```

The last example shows why direction matters: the same grammar carries key
reports from the emulator to the application.

## Parser requirements

A PTY read is not a protocol message. One sequence may be split across reads
and one read may contain text plus many sequences. A parser therefore keeps
state between writes and dispatches only on the final byte.

The de facto reference is Paul Williams' VT500-series state machine, which
most emulators implement directly. Its properties worth knowing:

- `ESC` inside a sequence aborts it and starts a new escape;
- `CAN` (`0x18`) and `SUB` (`0x1a`) abort the sequence;
- C0 controls other than those two are executed *inside* a CSI sequence
  without aborting it (so `CSI 3` `LF` `1 m` is legal, if unwise);
- DEL (`0x7f`) is ignored;
- an unrecognized final byte dispatches to nothing; the bytes are consumed
  silently and never printed.

Malformed or unbounded parameter strings must not consume unlimited memory.
Emulators typically stop accumulating after a fixed number of parameter
bytes and drop the sequence.

## Sending probes

```sh
tools/sendcsi list
tools/sendcsi steady-block
printf '\033[4:3m curly \033[0m\n'
```

The helpers write only the requested bytes, so `tools/sendcsi
blink-bar | od -c` shows exactly what a parser receives.

## Sources

- [ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [A parser for DEC's ANSI-compatible video terminals](https://vt100.net/emu/dec_ansi_parser)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
