# Ghostty

A native emulator for macOS and Linux built on libghostty, the same core
that xterm+ uses. Its documentation is unusual in listing every escape
sequence with a statement of intended behavior.

## Identity

- `TERM`: `xterm-ghostty` (ships its own terminfo).
- `TERM_PROGRAM`: `ghostty`; `TERM_PROGRAM_VERSION` carries the version.
- `GHOSTTY_RESOURCES_DIR` and `GHOSTTY_BIN_DIR` are set.
- XTVERSION: `DCS > | ghostty <version> ST`.

## Documentation

- [VT reference](https://ghostty.org/docs/vt)
- [Configuration reference](https://ghostty.org/docs/config/reference)
- [Releases](https://github.com/ghostty-org/ghostty/releases)

## Notable behavior

- Implements the Kitty graphics and keyboard protocols; does not implement
  Sixel.
- Implements DEC private mode 2027 (grapheme clustering) and mode 2048
  (in-band resize notifications).
- Supports OSC 8, 52, 7, 133, 9, 777, and synchronized output (2026).
- Ships shell integration that is injected automatically for supported
  shells.
- Documents each sequence with a status page; treat those pages as the
  authoritative intent for libghostty-based terminals, including xterm+.

## Version notes

`1.x.y` numbering; see the release notes.

## Probe

```sh
ghostty --version
tools/query '\033[>0q'
```
