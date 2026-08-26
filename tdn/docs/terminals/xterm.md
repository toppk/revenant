# xterm

The reference implementation for most of what other emulators call "xterm
behavior". Maintained by Thomas Dickey since 1996; its *XTerm Control
Sequences* document is the closest thing the field has to a shared
specification for non-DEC extensions.

## Identity

- `TERM`: `xterm-256color` on most distributions; the build default is
  `xterm` and the `termName` resource overrides it.
- `TERM_PROGRAM`: not set.
- `XTERM_VERSION`: set in the environment, e.g. `XTerm(406)`.
- DA1: `CSI ? 64 ; … c` by default (VT420 class); the model class follows the
  `decTerminalID` resource.
- DA2: `CSI > 41 ; Pv ; 0 c` where `Pv` is the patch level.
- XTVERSION: `DCS > | XTerm(406) ST`.

## Documentation

- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [xterm manual](https://invisible-island.net/xterm/manpage/xterm.html)
- [Changelog](https://invisible-island.net/xterm/xterm.log.html)
- [FAQ](https://invisible-island.net/xterm/xterm.faq.html)

## Notable behavior

- Originated the mouse tracking modes (1000–1006), bracketed paste (2004),
  the alternate screen with cursor save (1049), `modifyOtherKeys`, XTVERSION,
  XTGETTCAP, XTWINOPS, and OSC 4/10/11/12 color control.
- Implements Sixel and ReGIS graphics when built with them enabled; DA1
  advertises `4` when Sixel is available.
- Many reporting features are disabled by default through the
  `allowWindowOps`, `allowTitleOps`, `allowFontOps`, `allowTcapOps`, and
  `disallowedWindowOps` resources; a probe that "fails" in xterm is often
  refused rather than unsupported.
- Does not implement the Kitty keyboard protocol, OSC 8 hyperlinks, or the
  Kitty graphics protocol.
- Cursor blink interacts with resources (`cursorBlink`, `cursorBlinkXOR`) in
  ways other emulators do not copy; see [Cursor controls](../csi/cursor.md).

## Version notes

Patch numbers, not semantic versions. Additions are recorded per patch in
the changelog; the ctlseqs document notes the patch introducing each
extension where relevant.

## Probe

```sh
xterm -v
tools/query '\033[>0q'   # XTVERSION
tools/query '\033[c'     # DA1
```
