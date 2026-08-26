# OSC: Operating System Command

OSC control strings carry free-form text rather than numeric parameters. They
are used for everything that talks to the environment around the grid: window
titles, hyperlinks, palette changes, clipboard access, working-directory
reports, shell-integration marks, and desktop notifications.

## Syntax

```text
OSC Ps ; Pt ST
OSC Ps ; Pt BEL
```

`Ps` is a numeric selector, `Pt` is text, and the string ends with either
`ST` (`ESC \`) or `BEL` (`0x07`). Some selectors take several `;`-separated
fields inside `Pt`; each subpage documents its own field grammar.

## Terminators

`ST` is the ECMA-48 terminator. `BEL` is an xterm convention that most
emulators also accept. Both are common, and both have costs:

- `BEL` cannot terminate a DCS string, so an OSC wrapped inside a DCS
  passthrough (see below) must end in `ST`. Emulators that only accept `BEL`
  in that position are rare, but scripts that only emit `BEL` break the moment
  they run under a multiplexer.
- The single-byte C1 form of `ST` (`0x9c`) is a valid UTF-8 continuation byte.
  Emulators in UTF-8 mode generally do not recognise it as C1, so the
  seven-bit `ESC \` is the interoperable choice.
- Text before the terminator is opaque. An emulator that does not know `Ps`
  must still consume everything up to `ST` or `BEL` without printing it.

## Length limits

OSC strings have no length in the framing, so an emulator must impose one.
Limits differ by orders of magnitude: xterm caps at a compile-time buffer,
VTE and kitty allow multi-megabyte payloads for clipboard and images, and
multiplexers may truncate silently. Applications sending large payloads
(OSC 52, OSC 1337 `File=`) should expect a truncated string to be ignored
rather than partially applied.

## Security model

An OSC can read from or write to resources outside the PTY. Three classes of
selector are gated by most emulators:

- **Reads** (`OSC 52 ; c ; ?`, `OSC 10 ; ?`, title reports via XTWINOPS 21):
  a hostile program can turn a report into typed input, so emulators either
  refuse or require explicit configuration.
- **Clipboard writes** (`OSC 52`): allowed by default in some emulators,
  opt-in in others, because pasting attacker-controlled text into another
  application is a real injection path.
- **Environment mutation** (`OSC 3` X properties, `OSC 50` fonts, `OSC 7`
  directory, `OSC 133` marks): usually allowed because the effect stays inside
  the terminal, but tmux and screen may strip them.

An OSC whose `Pt` comes from untrusted data (file names, remote output)
should be validated by the application. The terminator bytes themselves are
the injection point: an untrusted string containing `ESC \` ends the OSC
early and the remainder is interpreted as fresh input.

## Multiplexer passthrough

tmux and screen own the PTY that the application sees, and interpret OSCs on
their own terms. Anything they do not understand is dropped unless it is
wrapped:

```text
DCS tmux ; <payload with every ESC doubled> ST
```

```sh
printf '\033Ptmux;\033\033]8;;https://example.com\033\033\\\033\\'
```

tmux passes the payload to the outer terminal only when `allow-passthrough`
is `on` or `all` (tmux 3.3+). screen uses `DCS <payload> ST` without the
`tmux;` prefix. Because the payload itself must end in `ST`, not `BEL`, a
passthrough OSC cannot use the `BEL` terminator.

tmux natively understands some OSCs and needs no passthrough for them: titles
(`set-titles`), OSC 52 (`set-clipboard`), and OSC 8 hyperlinks (tmux 3.4+).

## Pages

- [Window title](title.md): OSC 0, 1, 2 and the title stack.
- [Hyperlinks](hyperlinks.md): OSC 8.
- [Colors](colors.md): OSC 4, 5, 10–19, 104–119 and color-scheme reports.
- [Clipboard](clipboard.md): OSC 52.
- [Working directory](cwd.md): OSC 7, OSC 1337 `CurrentDir`, OSC 9;9.
- [Shell integration](shell-integration.md): OSC 133 and OSC 1337.
- [Notifications and progress](notifications.md): OSC 9, 777, 99, 9;4.
- [Miscellany](misc.md): OSC 3, 21, 22, 50, 66, and Apple document URLs.

## Sources

- [ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [XTerm Control Sequences, Operating System Commands](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
- [tmux wiki: FAQ, passthrough](https://github.com/tmux/tmux/wiki/FAQ#what-is-the-passthrough-escape-sequence-and-how-do-i-use-it)
- [tmux(1), allow-passthrough](https://man.openbsd.org/tmux#allow-passthrough)
