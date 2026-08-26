# Input: keyboard, mouse, and paste

Input is the other direction of the PTY. The emulator encodes keys, mouse
events, focus changes, and pasted text as bytes and writes them to the PTY
input, where they arrive interleaved with the terminal's own
[reports](../csi/queries.md). An application therefore reads one stream that
mixes what the user did with what the terminal answered, and must parse both
with the same tolerance for split reads.

## Generations of keyboard encoding

Keyboard encoding has grown in layers, each preserving the previous one:

<!-- markdownlint-disable MD013 -->

| Generation | Defined by | Distinguishes | Cost |
| --- | --- | --- | --- |
| [Legacy VT](keyboard-legacy.md) | DEC, xterm, terminfo | Printable keys, a few C0 controls, cursor and function keys | Ctrl-I is Tab, Ctrl-M is Enter, Esc is ambiguous with Alt |
| [modifyOtherKeys](modify-other-keys.md) | xterm | Modifier combinations on most keys | Two output formats; opt-in per application |
| [Kitty keyboard protocol](kitty-keyboard.md) | kitty | Every key, modifier, and event type, with a push/pop stack | Requires a stack-aware application and terminal |
| [win32-input-mode](win32-input-mode.md) | Microsoft ConPTY | Raw Windows key records | Windows-specific; not a general protocol |

<!-- markdownlint-enable MD013 -->

An application that needs unambiguous keys should query for the Kitty
protocol first, fall back to modifyOtherKeys, and treat the legacy encoding
as the floor. Each page explains how to detect support.

## Other input protocols

- [Mouse](mouse.md): tracking modes, coordinate encodings, and focus events.
- [Bracketed paste](bracketed-paste.md): marking pasted text so it is not
  executed as typed commands.

## Shared rules

- Input bytes and report bytes share one stream; an ESC byte alone is
  ambiguous until more bytes arrive or a timeout expires.
- Every mode an application enables for input must be reset on exit,
  including on error paths; see [Modes](../csi/modes.md).
- Encodings are chosen by the emulator, not negotiated; an application can
  only request a mode and observe what arrives.
