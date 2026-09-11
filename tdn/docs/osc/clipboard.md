---
tags:
  - Window Ops
---

# Clipboard

Status: xterm.

OSC 52 lets an application write to, and in principle read from, the system
clipboard or selection through the PTY. It is the only clipboard mechanism
that works over SSH without a forwarded display, which makes it both widely
used and widely restricted.

**Window Ops:** xterm gates OSC 52 reads with `GetSelection` and writes
(including clears) with `SetSelection`. Both are denied by default. See the
[Window Ops policy](../policies/window-ops.md) for the permission rules.

## Syntax

```text
OSC 52 ; Pc ; Pd ST
```

`Pc` names one or more targets; `Pd` is base64-encoded data, or `?` to query.

| `Pc` | Target |
| --- | --- |
| `c` | Clipboard |
| `p` | Primary selection (X11, Wayland) |
| `q` | Secondary selection |
| `s` | Select (xterm's configurable selection, usually primary) |
| `0`–`7` | Cut buffers 0–7 (X11 only) |

Several letters may be combined: `cp` writes both clipboard and primary. An
empty `Pc` means `s0` in xterm. Non-X11 emulators treat everything except
`c` (and often `p`) as the clipboard.

```sh
printf '\033]52;c;%s\033\\' "$(printf 'hello' | base64)"
```

An empty `Pd` clears the selection. Invalid base64 is ignored.

## Behavior

### Writes

The write is applied by the emulator, not the application, so it works
through SSH and inside containers as long as every hop forwards the OSC.
Payload limits differ: xterm truncates at its OSC buffer, tmux at 100 KiB
unless raised, kitty and foot accept several megabytes. Emulators that reject
an oversized write generally do so silently.

### Reads

`OSC 52 ; c ; ? ST` asks the emulator to reply with
`OSC 52 ; c ; base64 ST`. The reply arrives as input, so any program that
can write to the terminal can read the user's clipboard, including passwords
in a password manager's clipboard. Most emulators therefore refuse reads by
default, and many refuse them without any configuration to enable them.

Applications should treat a read as best-effort with a timeout, and should
never depend on it for correctness.

### Multiplexers

tmux intercepts OSC 52 and, with `set-clipboard on`, both updates its own
paste buffer and forwards the write to the outer terminal using the
`Ms` terminfo capability. With `set-clipboard external` it forwards only;
with `off` it drops the sequence. A read query is answered by tmux from its
own buffer only when `set-clipboard on`. screen does not implement OSC 52;
use DCS passthrough.


## Probe

```sh
tools/sendosc copy 'TDN clipboard probe'
printf '\033]52;c;%s\033\\' "$(printf 'TDN clipboard probe' | base64)"
printf '\033]52;c;?\033\\'        # read; expect a reply, a prompt, or silence
```

Paste into another application after the first command. Use
`tools/query` to capture any reply to the third.

## Sources

- [XTerm Control Sequences, OSC 52](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
- [kitty: clipboard_control](https://sw.kovidgoyal.net/kitty/conf/#opt-kitty.clipboard_control)
- [Ghostty: clipboard-read, clipboard-write](https://ghostty.org/docs/config/reference#clipboard-read)
- [foot.ini(5)](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot.ini.5.scd)
- [WezTerm escape sequences, OSC 52](https://wezterm.org/escape-sequences.html#operating-system-command-sequences)
- [Windows Terminal 1.16 release notes](https://github.com/microsoft/terminal/releases/tag/v1.16.10261.0)
- [iTerm2 preferences, clipboard](https://iterm2.com/documentation-preferences-general.html)
- [@xterm/addon-clipboard](https://github.com/xtermjs/xterm.js/tree/master/addons/addon-clipboard)
- [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs)
- [tmux(1), set-clipboard](https://man.openbsd.org/tmux#set-clipboard)


## Separately tracked behavior

### OSC 52 multiple targets

Feature ID: `osc-52-multiple-targets`. Accept a list of selection letters rather than only one target.

### OSC 52 default targets

Feature ID: `osc-52-default-targets`. Apply xterm selection defaults when the target field is empty.

### OSC 52 cut-buffer targets

Feature ID: `osc-52-cut-buffers`. Honor the cut-buffer digits and target letters without folding them into CLIPBOARD.

### OSC 52 invalid-data handling

Feature ID: `osc-52-invalid-base64-clear`. Match xterm selection clearing for invalid base64 rather than silently ignoring it.

### OSC 52 reply target encoding

Feature ID: `osc-52-reply-target`. Match the requested target spelling in selection replies.

### Kitty clipboard protocol

Feature ID: `osc-5522-clipboard`. Separate clipboard extension, outside the current dispatch queue.

<!-- tdn:compatibility -->
