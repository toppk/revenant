# foot

A fast, minimal Wayland-native emulator with thorough escape-sequence
documentation in its `foot-ctlseqs(7)` manual page.

## Identity

- `TERM`: `foot` (ships its own terminfo; `foot-extra` is also provided).
- `TERM_PROGRAM`: not set.
- XTVERSION: `DCS > | foot(1.x.y) ST`.

## Documentation

- [foot-ctlseqs(7)](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd)
- [foot.ini(5)](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot.ini.5.scd)
- [CHANGELOG](https://codeberg.org/dnkl/foot/src/branch/master/CHANGELOG.md)

## Notable behavior

- Implements Sixel; does not implement the Kitty graphics protocol.
- Implements the Kitty keyboard protocol and DEC private mode 2027.
- Supports OSC 8, 52, 7, 133, 777, and synchronized output (2026).
- Implements mode 2048 in-band resize notifications.
- Documents which OSC/CSI sequences are supported in one place, making it a
  convenient second opinion when reading the xterm document.

## Version notes

`1.x.y` numbering; the CHANGELOG lists each escape-sequence addition under
"Added".

## Probe

```sh
foot --version
tools/query '\033[>0q'
```
