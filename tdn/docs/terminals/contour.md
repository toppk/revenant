# Contour

A modern emulator whose author originated several extensions later adopted
elsewhere, notably synchronized output (mode 2026) and grapheme clustering
(mode 2027).

## Identity

- `TERM`: `contour` (ships its own terminfo).
- `TERM_PROGRAM`: `contour`; `TERM_PROGRAM_VERSION` carries the version.
- XTVERSION: `DCS > | contour <version> ST`.

## Documentation

- [VT extensions](https://contour-terminal.org/vt-extensions/)
- [Configuration](https://contour-terminal.org/configuration/)
- [Repository](https://github.com/contour-terminal/contour)

## Notable behavior

- Originated modes 2026 and 2027 and mode 2031 (color-scheme change
  notifications); each has a specification page on the Contour site.
- Implements Sixel; implements its own "Good Image Protocol" rather than the
  Kitty graphics protocol.
- Supports OSC 8, 52, 7, and 133.
- Provides a separate `libvtbackend` library for embedding.

## Version notes

`0.x.y` numbering; see the release notes on GitHub.

## Probe

```sh
contour version
tools/query '\033[>0q'
```
