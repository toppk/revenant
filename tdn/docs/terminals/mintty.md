# mintty

The terminal for Cygwin and MSYS2, also usable for WSL through `wsltty`.
Started as a fork of PuTTY's terminal core and now implements one of the
largest feature sets of any emulator.

## Identity

- `TERM`: `xterm` by default; `mintty` and `mintty-direct` terminfo entries
  exist and are recommended.
- `TERM_PROGRAM`: `mintty`; `TERM_PROGRAM_VERSION` carries the version.
- XTVERSION: `DCS > | mintty <version> ST`.

## Documentation

- [mintty(1)](https://mintty.github.io/mintty.1.html)
- [Control sequences wiki page](https://github.com/mintty/mintty/wiki/CtrlSeqs)
- [Changelog](https://github.com/mintty/mintty/blob/master/wiki/Changelog.md)

## Notable behavior

- Implements Sixel and iTerm2 inline images; does not implement the Kitty
  graphics protocol.
- Supports OSC 8, 52, 7, 9;4 progress, 777, underline styles, truecolor,
  and synchronized output (2026).
- Implements xterm `modifyOtherKeys` and its own extensions to it; Kitty
  keyboard protocol support is partial and version-dependent.
- Native Windows programs run through ConPTY inside mintty; Cygwin/MSYS2
  programs do not, so the same mintty can show two behaviors.

## Version notes

`3.x.y`; the Changelog lists sequences by release.

## Probe

```sh
mintty --version
tools/query '\033[>0q'
```
