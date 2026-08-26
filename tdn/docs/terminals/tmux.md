# tmux

A terminal multiplexer. Toward the programs it runs, tmux is an emulator
with its own screen model; toward the terminal it runs in, tmux is an
application. Features must be supported on both sides to work end to end.

## Identity

- `TERM` inside: `tmux-256color` when that terminfo entry exists, otherwise
  `screen-256color`; set by `default-terminal`.
- `TMUX` and `TMUX_PANE` are set inside sessions.
- DA1: `CSI ? 1 ; 2 c` `?`.
- DA2: `CSI > 84 ; 0 ; 0 c` (84 is ASCII `T`).
- XTVERSION: `DCS > | tmux <version> ST`.

## Documentation

- [tmux(1)](https://man.openbsd.org/tmux)
- [Wiki: FAQ](https://github.com/tmux/tmux/wiki/FAQ)
- [CHANGES](https://raw.githubusercontent.com/tmux/tmux/master/CHANGES)

## Configuration that changes wire behavior

- `terminal-features` and `terminal-overrides`: declare what the *outer*
  terminal can do (`RGB`, `hyperlinks`, `sixel`, `sync`, `usstyle`,
  `extkeys`, `focus`, `clipboard`, `mouse`, `title`). tmux only forwards a
  feature to the outer terminal when it believes the terminal supports it.
- `allow-passthrough`: permits `DCS tmux ; … ST` wrapped payloads to reach
  the outer terminal unmodified (see [DCS](../dcs/index.md)).
- `set-clipboard`: whether OSC 52 from inner programs is forwarded (`on`) or
  only used to fill tmux's own buffers (`external`/`off`).
- `extended-keys` and `extended-keys-format`: whether tmux requests and
  forwards `modifyOtherKeys`/CSI u style key encodings.
- `focus-events`: whether mode 1004 reports are forwarded.
- `allow-rename`, `set-titles`: title handling.

## Notable behavior

- Supports truecolor, underline styles, OSC 8 (3.4), OSC 52, mouse modes
  including SGR, bracketed paste, and synchronized output toward the outer
  terminal.
- Sixel is supported when built with `--enable-sixel` (3.4).
- Does not implement the Kitty graphics or keyboard protocols; passthrough
  can carry graphics payloads but tmux cannot track placements.
- Reports its own DA/XTVERSION identity, so inner programs cannot see the
  outer terminal without passthrough tricks.

## Version notes

`3.x`; CHANGES lists feature additions per release.

## Probe

```sh
tmux -V
tmux display -p '#{client_termfeatures}'
tools/query '\033[>0q'
```
