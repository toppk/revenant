# xterm.js

A terminal emulator written in TypeScript for the browser. It is the engine
behind VS Code's integrated terminal, Hyper, Tabby, ttyd, JupyterLab, Theia,
and many web consoles.

## Identity

- `TERM`, `COLORTERM`, and `TERM_PROGRAM` are set by the host that owns the
  PTY, not by xterm.js. VS Code sets `TERM_PROGRAM=vscode` and
  `TERM_PROGRAM_VERSION`; other hosts vary.
- DA1: `CSI ? 1 ; 2 c` `?` (configurable by the host through the API).
- DA2: `CSI > 0 ; 276 ; 0 c` `?`.
- XTVERSION: `?`

## Documentation

- [xtermjs.org](https://xtermjs.org/)
- [Supported sequences](https://xtermjs.org/docs/api/vtfeatures/)
- [API reference](https://xtermjs.org/docs/api/terminal/classes/terminal/)
- [Releases](https://github.com/xtermjs/xterm.js/releases)

## Addon model

The core handles parsing, the screen model, mouse and keyboard encoding, and
basic rendering. Optional behavior lives in addons that a host must load:

- `@xterm/addon-image`: Sixel and iTerm2 inline images.
- `@xterm/addon-clipboard`: OSC 52.
- `@xterm/addon-unicode11` (and later Unicode versions): character width
  tables newer than the built-in Unicode 6 table.
- `@xterm/addon-ligatures`, `@xterm/addon-webgl`, `@xterm/addon-canvas`:
  rendering only.
- `@xterm/addon-web-links`: URL detection in text, distinct from OSC 8.

Two hosts on the same core version therefore differ in graphics, clipboard,
and width behavior according to which addons they load.

## Notable behavior

- Supports truecolor, DECSCUSR, all xterm mouse modes, bracketed paste,
  alternate screen, OSC 8, underline styles, and synchronized output (2026).
- Does not implement the Kitty keyboard protocol or the Kitty graphics
  protocol in core.
- The host decides which keys reach the terminal at all; browser and editor
  keybindings frequently intercept chords before xterm.js sees them.

## Version notes

`5.x` releases; the package moved from `xterm` to `@xterm/xterm` in 5.4.
See the release notes for VT additions.

## Probe

```sh
echo "$TERM_PROGRAM $TERM_PROGRAM_VERSION"
tools/query '\033[>c'
```
