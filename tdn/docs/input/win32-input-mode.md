# win32-input-mode

Status: Vendor-private, defined by Microsoft for ConPTY.

win32-input-mode is a keyboard encoding that carries a full Windows
`KEY_EVENT_RECORD` over the PTY. It exists because the Windows console API
gives applications more than a byte stream: virtual key codes, scan codes,
key-up events, and control-key state. When Windows Terminal or another
ConPTY host sits in front of a console application, the bytes it sends must
be turned back into those records without loss, and no VT encoding could
express them.

## Syntax

```text
CSI ? 9001 h     enable win32-input-mode
CSI ? 9001 l     disable win32-input-mode
CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
```

<!-- markdownlint-disable MD013 -->

| Field | Meaning |
| --- | --- |
| `Vk` | Virtual key code (`wVirtualKeyCode`) |
| `Sc` | Scan code (`wVirtualScanCode`) |
| `Uc` | Unicode character (`UnicodeChar`), as a decimal code unit |
| `Kd` | Key down flag: `1` press, `0` release |
| `Cs` | Control key state bit mask (`dwControlKeyState`) |
| `Rc` | Repeat count |

<!-- markdownlint-enable MD013 -->

Omitted fields default to `0`. The final byte `_` was chosen because it is
unused by other keyboard encodings.

## Behavior

Once enabled, every key event, including releases and modifier-only
presses, arrives as one `_` sequence, and the legacy encodings stop. The
receiver reconstructs a Windows input record and feeds it to the console
input buffer, so a console application sees exactly what it would see on a
local console.

The mode is intended for ConPTY-to-host communication. A Unix application
gains nothing from it and should not enable it; the ordinary VT encodings
remain available in ConPTY sessions without it.

## Relationship to the Kitty protocol

Both encodings report press and release and both can carry the produced
text, but they differ in intent. win32-input-mode is a transport for
platform key records, keyed by Windows virtual key and scan codes. The
[Kitty protocol](kitty-keyboard.md) is platform-neutral and keyed by Unicode
code points. An emulator on Windows may implement both: win32-input-mode to
talk to ConPTY, and the Kitty protocol for applications that request it.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Terminal | Support | Notes |
| --- | --- | --- |
| Windows Terminal | Yes | Origin; both as ConPTY host and consumer[^wt] |
| ConPTY (conhost) | Yes | Requests the mode from its host[^wt] |
| WezTerm | Yes | Implements it on Windows for ConPTY[^wezterm] |
| mintty | ? | |
| Alacritty | ? | |
| xterm, VTE, Konsole, kitty, Ghostty, foot, Contour, PuTTY, Apple Terminal, iTerm2, xterm.js, tmux | ? | Not a Unix protocol; not documented by any of them |

<!-- markdownlint-enable MD013 -->

[^wt]: [Improved keyboard handling in Conpty](https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Improved%20keyboard%20handling%20in%20Conpty.md).
[^wezterm]: [WezTerm `allow_win32_input_mode`](https://wezterm.org/config/lua/config/allow_win32_input_mode.html).

## Probe

Inside Windows Terminal or WezTerm on Windows:

```sh
printf '\033[?9001h'; cat -v; printf '\033[?9001l'
```

Each key press and release prints a `^[[...;...;...;1;...;1_` or `...;0;..._`
line. On other emulators nothing changes.

## Sources

- [Improved keyboard handling in Conpty (spec #4999)](https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Improved%20keyboard%20handling%20in%20Conpty.md)
- [KEY_EVENT_RECORD structure](https://learn.microsoft.com/en-us/windows/console/key-event-record-str)
