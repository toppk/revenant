# Apple Terminal (Terminal.app)

The emulator bundled with macOS. It is closed source, updated only with the
operating system, and documented for users rather than for protocol
implementers.

## Identity

- `TERM`: `xterm-256color`.
- `TERM_PROGRAM`: `Apple_Terminal`; `TERM_PROGRAM_VERSION` carries a build
  number (for example `453` on macOS 14), not the macOS version.
- `TERM_SESSION_ID`: a per-window identifier.
- DA1: `CSI ? 1 ; 2 c` `?`.
- XTVERSION: not supported.

## Documentation

- [Terminal User Guide](https://support.apple.com/guide/terminal/welcome/mac)
- No escape-sequence reference is published; `/usr/share/terminfo` and the
  bundled `/etc/bashrc_Apple_Terminal` (which emits OSC 7) are the closest
  primary sources.

## Notable behavior

- Originated OSC 7 (`file://` current working directory) and OSC 6
  (current document), both consumed to restore sessions and to set the
  proxy icon.
- 256 colors only; no 24-bit color support, so `COLORTERM` is not set and
  applications that assume truecolor from `TERM_PROGRAM` misrender.
- Bracketed paste, alternate screen, mouse reporting including SGR, and
  DECSCUSR cursor styles are supported.
- Does not implement OSC 8 hyperlinks, OSC 52, any graphics protocol,
  underline styles, the Kitty keyboard protocol, or synchronized output.
- Some behavior depends on the "Advanced ▸ Terminfo" declaration in the
  profile, which changes `TERM` without changing capabilities.

## Version notes

Versioned with macOS; observations must record both the macOS version and
`TERM_PROGRAM_VERSION`.

## Probe

```sh
echo "$TERM_PROGRAM $TERM_PROGRAM_VERSION"
tools/query '\033[c'
```
