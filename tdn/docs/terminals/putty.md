# PuTTY

An SSH, Telnet, and serial client for Windows and Unix with a built-in
terminal emulator. Its emulator predates most modern extensions and is
configured per session rather than by a single global file.

## Identity

- `TERM`: `xterm` by default; the "Terminal-type string" setting in
  Connection ▸ Data changes it.
- `TERM_PROGRAM`: not set.
- Answerback (`ENQ`): configurable, default `PuTTY`.
- DA1: `CSI ? 6 c` (VT102 class) `?`.
- XTVERSION: not supported.

## Documentation

- [PuTTY manual](https://the.earth.li/~sgtatham/putty/latest/htmldoc/)
- [Terminal configuration chapter](https://the.earth.li/~sgtatham/putty/latest/htmldoc/Chapter4.html#config-terminal)
- [Changes](https://www.chiark.greenend.org.uk/~sgtatham/putty/changes.html)

## Notable behavior

- Supports 256 colors and, since 0.71, 24-bit color.
- Bracketed paste, the alternate screen (configurable), and xterm mouse
  reporting including the SGR encoding are supported.
- Several classic behaviors are configuration options: "Disable switching to
  alternate terminal screen", "Disable remote-controlled window title
  changing", "Disable remote-controlled character set configuration",
  "Response to remote title query" (which defaults to an empty string as a
  security measure).
- Does not implement OSC 8 hyperlinks, OSC 52, graphics, or the Kitty
  keyboard protocol.
- Function-key and keypad encodings follow a "Function keys and keypad"
  setting with several modes (ESC[n~, Linux, Xterm R6, VT400, VT100+, SCO);
  the default is `ESC[n~`, which matches xterm for F1–F4 only in the
  "Xterm R6" mode.

## Version notes

`0.xx` numbering; the Changes page lists terminal-related changes.

## Probe

```sh
tools/query '\033[c'
tools/query '\005'       # ENQ answerback
```
