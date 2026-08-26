# iTerm2

The dominant third-party macOS emulator, with a large private extension
namespace under OSC 1337 and early adoption of most modern protocols.

## Identity

- `TERM`: `xterm-256color`.
- `TERM_PROGRAM`: `iTerm.app`; `TERM_PROGRAM_VERSION` carries the version.
- `ITERM_SESSION_ID` and `ITERM_PROFILE` are set.
- `LC_TERMINAL=iTerm2` and `LC_TERMINAL_VERSION` are forwarded over SSH when
  the shell integration is installed.
- XTVERSION: `?`

## Documentation

- [Proprietary escape codes](https://iterm2.com/documentation-escape-codes.html)
- [Shell integration](https://iterm2.com/documentation-shell-integration.html)
- [Inline images protocol](https://iterm2.com/documentation-images.html)
- [Changelog](https://iterm2.com/downloads.html)

## Notable behavior

- Originated OSC 1337 (inline images, `SetUserVar`, `CurrentDir`,
  `RemoteHost`, badges, annotations) and OSC 133 shell-integration marks
  (adopted from FinalTerm).
- Co-originated OSC 8 hyperlinks with VTE.
- Implements Sixel and the Kitty graphics protocol (3.5) in addition to its
  own image protocol.
- Implements the Kitty keyboard protocol (3.5) and `modifyOtherKeys`.
- Supports OSC 52 (governed by a preference), OSC 9 and 777 notifications,
  underline styles, truecolor, and synchronized output.
- The `it2*` utilities and the Python API are separate channels outside TDN
  scope.

## Version notes

`3.x.y`; the 3.5 series added the Kitty keyboard and graphics protocols.
See the changelog for specifics.

## Probe

```sh
echo "$TERM_PROGRAM_VERSION"
tools/query '\033[>0q'
tools/query '\033[?u'
```
