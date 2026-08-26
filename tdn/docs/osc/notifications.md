# Notifications and progress

Status: Extension, several incompatible origins.

An application that finishes a long job wants to tell the user even when the
window is not focused. Terminals expose this through desktop notifications,
urgency hints, and, more recently, a progress indicator in the tab or
taskbar. No two origins agree on the selector.

## Syntax

### OSC 9: iTerm2 notification

```text
OSC 9 ; text ST
```

Shows `text` as a desktop notification. Introduced by iTerm2.

### OSC 9 collisions: ConEmu

ConEmu independently chose selector 9 for a family of sub-commands:

```text
OSC 9 ; 1 ; ms ST                  sleep
OSC 9 ; 2 ; text ST                message box
OSC 9 ; 4 ; state ; progress ST    taskbar progress
OSC 9 ; 9 ; path ST                current directory
OSC 9 ; 10 ST                      ANSI processing toggle
```

An emulator receiving `OSC 9 ; 4 ; 1 ; 50 ST` must decide whether it is a
notification whose text is `4;1;50` or a 50% progress bar. Windows Terminal
and mintty follow ConEmu; iTerm2, kitty, foot, and WezTerm treat OSC 9 as a
notification. Applications cannot send one form that is correct everywhere.

### OSC 9;4 progress

```text
OSC 9 ; 4 ; 0 ST          clear
OSC 9 ; 4 ; 1 ; N ST      normal, N percent (0–100)
OSC 9 ; 4 ; 2 ; N ST      error state
OSC 9 ; 4 ; 3 ST          indeterminate
OSC 9 ; 4 ; 4 ; N ST      paused/warning state
```

Rendered as a taskbar progress overlay (Windows) or a tab indicator.

### OSC 777: rxvt-unicode notify

```text
OSC 777 ; notify ; title ; body ST
```

Originated in the urxvt `notify` Perl extension and adopted as the most
common Linux form. Fields are plain text; `;` inside the body is not
escapable.

### OSC 99: kitty desktop notifications

```text
OSC 99 ; key=value : key=value ; payload ST
```

A structured protocol with identifiers, chunked payloads, icons, urgency,
buttons, and a close-and-report mechanism. Common keys:

| Key | Meaning |
| --- | --- |
| `i=ID` | Notification identifier for updates, closing, and activation reports |
| `d=0` | More chunks follow (default `d=1`, done) |
| `p=title` / `p=body` / `p=icon` / `p=buttons` | What the payload is |
| `e=1` | Payload is base64-encoded |
| `u=0..2` | Urgency: low, normal, critical |
| `a=focus,report` | Action on activation |
| `o=always` / `unfocused` / `invisible` | When to show |

On activation the emulator sends `OSC 99 ; i=ID ; ST` back as input when
`a=report` was requested. Query support with `OSC 99 ; i=ID : p=? ST`.

### BEL and urgency

`BEL` (`0x07`) remains the lowest common denominator. Emulators map it to an
audible bell, a visual flash, an X11 urgency hint, a Wayland activation
request, or a tab badge, and most let the user choose. It cannot carry text.

## Behavior

- Whether a notification appears while the window is focused is
  emulator-defined; kitty and foot default to unfocused only.
- Text is shown by the desktop's notification daemon, which may apply its
  own markup rules; emulators generally strip control bytes but not markup.
- Progress states persist until cleared. A program that exits without
  `OSC 9 ; 4 ; 0` leaves a stale bar.
- Multiplexers drop all of these without passthrough.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OSC 9 notify | ? | ? | ? | Yes | Yes | Yes | ? | ? | ? | No[^mt] | ? | No[^wt] | ? | Yes | ? | ? |
| OSC 9;4 progress | ? | ? | ? | ? | ? | Yes (1.2) | ? | ? | ? | Yes | ? | Yes (1.6) | ? | ? | ? | ? |
| OSC 777 notify | ? | Partial[^vte] | ? | ? | Yes | Yes | Yes | ? | ? | Yes | ? | ? | ? | ? | ? | ? |
| OSC 99 | ? | ? | ? | Yes | ? | Yes | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| BEL urgency/visual | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Yes |

<!-- markdownlint-enable MD013 -->

[^mt]: mintty interprets OSC 9 sub-commands in the ConEmu style.
[^wt]: Windows Terminal interprets OSC 9 sub-commands in the ConEmu style.
[^vte]: Some VTE-based terminals (e.g. Tilix, Terminator) handle OSC 777 in
    the host application; VTE itself does not emit a notification.

## Probe

```sh
tools/sendosc notify 'TDN' 'notification probe'
printf '\033]9;TDN OSC 9 probe\033\\'
printf '\033]777;notify;TDN;OSC 777 probe\033\\'
printf '\033]99;i=1:d=0:p=title;TDN\033\\'; printf '\033]99;i=1:p=body;OSC 99 probe\033\\'
printf '\033]9;4;1;50\033\\'; sleep 2; printf '\033]9;4;0\033\\'
```

Unfocus the window before running if the emulator hides focused
notifications.

## Sources

- [iTerm2 proprietary escape codes, OSC 9](https://iterm2.com/documentation-escape-codes.html)
- [ConEmu ANSI escape codes](https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC)
- [Windows Terminal 1.6 release notes, progress](https://github.com/microsoft/terminal/releases/tag/v1.6.10272.0)
- [kitty: desktop notifications](https://sw.kovidgoyal.net/kitty/desktop-notifications/)
- [rxvt-unicode urxvt-perl(1), notify](https://man.archlinux.org/man/urxvtperl.3)
- [foot: escape sequences](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd)
- [Ghostty VT reference](https://ghostty.org/docs/vt)
- [WezTerm escape sequences](https://wezterm.org/escape-sequences.html)
- [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs)
