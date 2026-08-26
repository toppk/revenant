# st (suckless simple terminal)

A minimal X11 emulator with a small, readable code base and a culture of
user-applied patches; the unpatched program is the reference.

## Identity

- `TERM`: `st-256color`.
- `TERM_PROGRAM`: not set.
- DA1: `CSI ? 6 c` (VT102 class).
- XTVERSION: not supported.

## Documentation

- [st.suckless.org](https://st.suckless.org/)
- [Source](https://git.suckless.org/st/)
- [Patches](https://st.suckless.org/patches/)

## Notable behavior

- Truecolor, 256 colors, DECSCUSR, bracketed paste, mouse modes including
  SGR, alternate screen, and OSC 52 clipboard writes are supported.
- No scrollback, no OSC 8, no graphics, no Kitty keyboard protocol in the
  stock build; each exists as a community patch.
- Because patched builds are common, record whether a build is stock.

## Version notes

`0.9.x`; see the site's changelog.

## Probe

```sh
st -v
tools/query '\033[c'
```
