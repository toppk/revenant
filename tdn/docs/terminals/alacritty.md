# Alacritty

A GPU-rendered emulator with a deliberately small feature set; window
management, tabs, and graphics are left to other programs.

## Identity

- `TERM`: `alacritty` (ships its own terminfo).
- `TERM_PROGRAM`: not set.
- `ALACRITTY_WINDOW_ID`, `ALACRITTY_SOCKET`, and `ALACRITTY_LOG` are set.
- XTVERSION: `?`

## Documentation

- [Configuration](https://alacritty.org/config-alacritty.html)
- [Repository](https://github.com/alacritty/alacritty)
- [CHANGELOG](https://github.com/alacritty/alacritty/blob/master/CHANGELOG.md)

## Notable behavior

- No graphics protocols by design.
- Implements the Kitty keyboard protocol (0.13) and underline styles.
- Supports OSC 8, OSC 52 (writes; reads are off by default), and
  synchronized output (2026).
- Does not implement mode 2027.
- `alacritty_terminal` is published as a library and is used by other
  projects, so Alacritty's parser behavior can appear in unexpected places.

## Version notes

`0.x.y` numbering; the CHANGELOG is precise about which release added each
sequence.

## Probe

```sh
alacritty --version
tools/query '\033[c'
```
