# rxvt-unicode (urxvt)

A long-lived fork of rxvt with Unicode support and a Perl extension system.
Its escape-sequence lineage is rxvt, not xterm, so several numbers differ.

## Identity

- `TERM`: `rxvt-unicode-256color` (or `rxvt-unicode`).
- `TERM_PROGRAM`: not set.
- `COLORTERM`: `rxvt-xpm` or `rxvt` in older builds.
- DA1: `CSI ? 1 ; 2 c`.
- XTVERSION: not supported.

## Documentation

- [urxvt(7) escape-sequence reference](http://pod.tst.eu/http://cvs.schmorp.de/rxvt-unicode/doc/rxvt.7.pod)
- [urxvt(1)](http://pod.tst.eu/http://cvs.schmorp.de/rxvt-unicode/doc/rxvt.1.pod)
- [Changes](http://cvs.schmorp.de/rxvt-unicode/Changes)

## Notable behavior

- Originated mouse mode 1015 (the "urxvt" mouse encoding) and OSC 777
  notifications (through the `notify` extension).
- No truecolor in the stock build; distributions carry a 24-bit patch, so
  observations must say which build was used.
- Does not implement DECSCUSR cursor styles, OSC 8, or synchronized output.
- Bracketed paste and the alternate screen are supported.
- Keys use rxvt-style encodings (`ESC [ 7 ~` for Home, `ESC O a` for
  Ctrl-Up) that differ from xterm; terminfo handles the mapping.

## Version notes

Releases are infrequent (9.x); check `Changes` for the specific release.

## Probe

```sh
urxvt -help 2>&1 | head -1
tools/query '\033[c'
```
