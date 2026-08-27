# OSC anatomy and parsing

Status: Standard (ECMA-48 §8.3.89 framing); xterm (selector numbering, `BEL`).

## Syntax

```text
OSC Ps ; Pt ST
OSC Ps ; Pt BEL
```

`OSC` is `ESC ]` (`0x1b 0x5d`) or the C1 byte `0x9d`. `Ps` is a decimal
selector, `Pt` is text, and the string ends with either `ST` (`ESC \`) or
`BEL` (`0x07`). ECMA-48 defines only the framing: everything between the
introducer and the terminator is opaque command string, and the meaning of
`Ps` comes from xterm and the vendors that followed it. The
[catalog](catalog.md) lists every selector TDN knows about.

Unlike CSI, OSC has no parameter/intermediate/final structure. Some
selectors take several `;`-separated fields inside `Pt`, some take
`key=value` pairs, some take base64; each feature page documents its own
field grammar. A parser must not split on `;` before it knows the selector,
because titles, URIs, and file paths legitimately contain semicolons.

## Terminators

`ST` is the ECMA-48 terminator. `BEL` is an xterm convention that nearly
every emulator also accepts. Both are common, and both have costs:

- `BEL` cannot terminate a DCS string, so an OSC wrapped inside a DCS
  passthrough must end in `ST`. Scripts that only emit `BEL` break the
  moment they run under a [multiplexer](../practices/multiplexers.md).
- The single-byte C1 form of `ST` (`0x9c`) is a valid UTF-8 continuation
  byte. Emulators in UTF-8 mode generally do not recognise it as C1, so the
  seven-bit `ESC \` is the interoperable choice.
- A reply to a query (`OSC 10 ; ?`, `OSC 52 ; c ; ?`) uses the same
  terminator the request used in xterm; other emulators always reply with
  `ST` or always with `BEL`. Applications parsing replies must accept both.

Text before the terminator is opaque. An emulator that does not know `Ps`
must still consume everything up to `ST` or `BEL` without printing it.

## Parser requirements

The VT500 parser's `osc_string` state collects bytes until `ST`, `BEL`
(xterm extension), or an abort:

- `ESC` followed by anything other than `\` aborts the OSC and starts a new
  escape; the collected text is discarded;
- `CAN` and `SUB` abort;
- other C0 controls inside the string are ignored by the reference parser,
  but some emulators keep them in the payload; applications must not rely
  on either;
- the payload is bytes, not characters. Emulators that validate it as UTF-8
  (VTE, kitty) drop or replace invalid sequences; xterm passes them through.

As with CSI, a PTY read is not a message boundary. An OSC may arrive split
across reads and a parser keeps state until the terminator.

## Length limits

OSC strings have no length in the framing, so an emulator must impose one.
Limits differ by orders of magnitude: xterm caps at a compile-time buffer,
VTE and kitty allow multi-megabyte payloads for clipboard and images, and
multiplexers may truncate silently. Applications sending large payloads
(OSC 52, OSC 1337 `File=`) should expect a truncated string to be ignored
rather than partially applied, and should prefer the chunked forms where a
protocol offers one.

## Direction

Most OSCs are commands from the application to the emulator. A few travel
the other way as replies to queries (`OSC 4 ; n ; ?`, `OSC 10 ; ?`,
`OSC 52 ; c ; ?`, `OSC 22 ; ?__current__`), and those replies arrive on the
same input stream as keystrokes. Everything in
[Queries and reports](../csi/queries.md#application-requirements) about
treating responses as asynchronous input applies to OSC replies too.

## Security model

An OSC can read from or write to resources outside the PTY. Three classes
of selector are gated by most emulators:

- **Reads** (`OSC 52 ; c ; ?`, `OSC 10 ; ?`, title reports via XTWINOPS
  21): a hostile program can turn a report into typed input, so emulators
  either refuse or require explicit configuration.
- **Clipboard writes** (`OSC 52`): allowed by default in some emulators,
  opt-in in others, because pasting attacker-controlled text into another
  application is a real injection path.
- **Environment mutation** (`OSC 3` X properties, `OSC 50` fonts, `OSC 7`
  directory, `OSC 133` marks): usually allowed because the effect stays
  inside the terminal, but tmux and screen may strip them.

An OSC whose `Pt` comes from untrusted data (file names, remote output)
should be validated by the application. The terminator bytes themselves are
the injection point: an untrusted string containing `ESC \` or `BEL` ends
the OSC early and the remainder is interpreted as fresh input. Strip both
from anything interpolated into `Pt`.

## Sending probes

```sh
tools/sendosc list
tools/sendosc title "hello"
printf '\033]8;;https://example.com\033\\link\033]8;;\033\\\n'
```

## Sources

- [ECMA-48 §8.3.89 OSC, §8.3.143 ST](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [XTerm Control Sequences, Operating System Commands](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
- [A parser for DEC's ANSI-compatible video terminals](https://vt100.net/emu/dec_ansi_parser)
