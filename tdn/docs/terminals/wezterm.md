# WezTerm

A cross-platform GPU-rendered emulator with a Lua configuration API and its
own built-in multiplexer.

## Identity

- `TERM`: `xterm-256color`; a `wezterm` terminfo is available for
  installation.
- `TERM_PROGRAM`: `WezTerm`; `TERM_PROGRAM_VERSION` carries the version.
- `WEZTERM_EXECUTABLE`, `WEZTERM_PANE`, and `WEZTERM_UNIX_SOCKET` are set.
- XTVERSION: `DCS > | WezTerm <version> ST`.

## Documentation

- [Escape sequences](https://wezterm.org/escape-sequences.html)
- [Configuration](https://wezterm.org/config/files.html)
- [Changelog](https://wezterm.org/changelog.html)

## Notable behavior

- Implements Sixel, the Kitty graphics protocol, and iTerm2 inline images.
- Implements the Kitty keyboard protocol and xterm `modifyOtherKeys`.
- Supports OSC 8, 52, 7, 133, 1337 (a subset), and 9;4 progress.
- Synchronized output (mode 2026) is supported.
- Versions are date-stamped (`YYYYMMDD-HHMMSS-hash`); nightly builds are
  common in the wild, so record the full string in observations.

## Version notes

See the changelog; each entry notes escape-sequence additions.

## Probe

```sh
wezterm --version
tools/query '\033[>0q'
```
