# kitty

A GPU-rendered emulator whose author has published several protocols now
implemented by other terminals: the Kitty keyboard protocol, the Kitty
graphics protocol, desktop notifications (OSC 99), and the text sizing
protocol (OSC 66).

## Identity

- `TERM`: `xterm-kitty` (ships its own terminfo).
- `TERM_PROGRAM`: not set; `KITTY_WINDOW_ID`, `KITTY_PID`, and
  `KITTY_PUBLIC_KEY` are set.
- DA1: `CSI ? 62 ; c` `?`
- XTVERSION: `DCS > | kitty(0.xx.y) ST`.

## Documentation

- [Protocol extensions](https://sw.kovidgoyal.net/kitty/protocol-extensions/)
- [Keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/)
- [Graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/)
- [Configuration](https://sw.kovidgoyal.net/kitty/conf/)
- [Changelog](https://sw.kovidgoyal.net/kitty/changelog/)

## Notable behavior

- Refuses to implement Sixel; the graphics protocol is the alternative.
- Refuses DEC private mode 2027 (grapheme clustering); kitty segments
  graphemes unconditionally and offers the text sizing protocol for width
  control.
- Ships terminfo with custom capabilities (`Smulx`, `Setulc`, `Sync`,
  `kitty-query-*`) and answers XTGETTCAP.
- OSC 52 reads and writes are governed by `clipboard_control`.
- Provides a remote-control protocol over a socket in addition to
  escape-sequence extensions; that channel is out of TDN scope.
- Shell integration is injected automatically for supported shells and
  emits OSC 133 marks.

## Version notes

Kitty uses `0.x.y` numbering with frequent minor releases; each protocol
page states the version in which it was introduced or revised.

## Probe

```sh
kitty --version
tools/query '\033[>0q'   # XTVERSION
tools/query '\033[?u'    # keyboard protocol flags
```
