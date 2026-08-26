# Mouse and focus reporting

Status: xterm, with an urxvt extension and a later xterm pixel extension.

Mouse reporting is opt-in. An application sets a tracking mode to choose
which events it receives and an encoding mode to choose how coordinates are
written. Focus reporting uses the same machinery and is documented here
because it shares the input stream and the cleanup obligations.

## Tracking modes

<!-- markdownlint-disable MD013 -->

| Mode | Name | Reports |
| --- | --- | --- |
| `?9` | X10 | Button press only, no modifiers, no release |
| `?1000` | Normal | Press and release |
| `?1001` | Highlight | Press, release, and a highlight-tracking handshake; the terminal blocks until the application answers |
| `?1002` | Button-event | Normal plus motion while a button is held |
| `?1003` | Any-event | Button-event plus motion with no button held |

<!-- markdownlint-enable MD013 -->

Only one tracking mode is active at a time; setting another replaces it.
Mode `?1001` requires the application to reply with `CSI Ps ; Ps ; Ps ; Ps ; Ps T`
after each press or the terminal hangs, and most emulators refuse or ignore
it. Do not use it.

## Encodings

The default encoding is the original X10 form; an encoding mode changes it.

<!-- markdownlint-disable MD013 -->

| Mode | Name | Sequence | Limits |
| --- | --- | --- | --- |
| (none) | Legacy | `CSI M Cb Cx Cy`, each a single byte with 32 added | Coordinates above 223 overflow; bytes above 127 are invalid UTF-8 |
| `?1005` | UTF-8 | As legacy, but each value is UTF-8 encoded | Ambiguous with legacy for values ≤ 95; deprecated |
| `?1015` | urxvt | `CSI Cb ; Cx ; Cy M` with decimal values | Release is indistinguishable from press of button 3 |
| `?1006` | SGR | `CSI < Cb ; Cx ; Cy M` press, `CSI < Cb ; Cx ; Cy m` release | None practical; the recommended encoding |
| `?1016` | SGR-pixels | As SGR, but `Cx` and `Cy` are pixel offsets | Requires `?1006`-style parsing plus cell size knowledge |

<!-- markdownlint-enable MD013 -->

In the legacy and urxvt forms the release event sets the button field to
`3`. In the SGR form the final byte distinguishes press (`M`) from release
(`m`) and the button field keeps its value. Coordinates are 1-based.

## Button field

`Cb` is a bit mask; in the legacy encoding it is sent with 32 added.

| Bits | Meaning |
| --- | --- |
| `0`–`2` | `0` left, `1` middle, `2` right, `3` release (legacy and urxvt only) |
| `4` | Shift |
| `8` | Meta (Alt) |
| `16` | Control |
| `32` | Motion event |
| `64` | Wheel: `64` up, `65` down, `66` left, `67` right |
| `128` | Additional buttons: `128`–`131` for buttons 8–11 |

So `CSI < 0 ; 10 ; 5 M` is a left press at column 10 row 5, `CSI < 65 ; 10 ; 5 M`
is wheel-down there, and `CSI < 32 ; 11 ; 5 M` is motion with the left button
held.

## Alternate scroll

`?1007` makes wheel events on the alternate screen arrive as cursor Up and
Down keys instead of mouse reports when no tracking mode is active. Many
emulators enable it by default so that pagers scroll under the wheel.

## Focus reporting

```text
CSI ? 1004 h    enable
CSI I           terminal gained focus
CSI O           terminal lost focus
CSI ? 1004 l    disable
```

Reports are unconditional once enabled, including when the application did
not expect one, so a parser must accept `CSI I` and `CSI O` at any point.
Some emulators send an initial report when the mode is enabled; do not rely
on it either way.

## Mouse and selection

When a tracking mode is active the emulator gives mouse events to the
application, so the user can no longer select text. The near-universal
convention is that holding Shift bypasses tracking and restores selection;
some emulators use a different modifier or make it configurable. This is a
convention, not a protocol, and the application never sees the shifted click.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Terminal | `?1000` | `?1002` | `?1003` | `?1006` | `?1015` | `?1016` | `?1004` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| xterm | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^xterm] |
| VTE | Yes | Yes | Yes | Yes | Yes | ? | Yes[^vte] |
| Konsole | Yes | Yes | Yes | Yes | Yes | ? | Yes[^konsole] |
| kitty | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^kitty] |
| WezTerm | Yes | Yes | Yes | Yes | ? | Yes | Yes[^wezterm] |
| Ghostty | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^ghostty] |
| foot | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^foot] |
| Alacritty | Yes | Yes | Yes | Yes | Yes | ? | Yes[^alacritty] |
| Contour | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^contour] |
| mintty | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^mintty] |
| PuTTY | Yes | Yes | ? | Yes | ? | ? | ?[^putty] |
| Windows Terminal | Yes | Yes | Yes | Yes | Yes | ? | Yes[^wt] |
| Apple Terminal | ? | ? | ? | ? | ? | ? | ? |
| iTerm2 | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^iterm2] |
| xterm.js | Yes | Yes | Yes | Yes | Yes | ? | Yes[^xtermjs] |
| tmux | Yes | Yes | Yes | Yes | Yes | ? | Yes[^tmux] |

<!-- markdownlint-enable MD013 -->

[^xterm]: [XTerm Control Sequences, Mouse Tracking](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking).
[^vte]: VTE has no protocol reference; support is visible in `src/vte.cc` of [gnome/vte](https://gitlab.gnome.org/GNOME/vte).
[^konsole]: [Konsole source, `Vt102Emulation.cpp`](https://invent.kde.org/utilities/konsole).
[^kitty]: [kitty protocol extensions](https://sw.kovidgoyal.net/kitty/protocol-extensions/).
[^wezterm]: [WezTerm escape sequences](https://wezterm.org/escape-sequences.html).
[^ghostty]: [Ghostty VT reference](https://ghostty.org/docs/vt).
[^foot]: [foot README](https://codeberg.org/dnkl/foot).
[^alacritty]: [Alacritty escape sequence support](https://github.com/alacritty/alacritty/blob/master/docs/escape_support.md).
[^contour]: [Contour VT extensions](https://contour-terminal.org/vt-extensions/).
[^mintty]: [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs).
[^putty]: [PuTTY documentation, Terminal panel](https://the.earth.li/~sgtatham/putty/latest/htmldoc/Chapter4.html).
[^wt]: [microsoft/terminal VT support](https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Improved%20keyboard%20handling%20in%20Conpty.md) and [Windows Terminal 1.x release notes](https://github.com/microsoft/terminal/releases).
[^iterm2]: [iTerm2 escape codes](https://iterm2.com/documentation-escape-codes.html).
[^xtermjs]: [xterm.js supported sequences](https://xtermjs.org/docs/api/vtfeatures/).
[^tmux]: [tmux(1), `mouse` and `terminal-features`](https://man.openbsd.org/tmux.1).

## Pitfalls

- Enable `?1006` before or together with the tracking mode; otherwise a
  large terminal produces legacy bytes that corrupt UTF-8 decoding.
- A legacy report begins `CSI M`, so an input parser must read three more
  bytes after that prefix rather than dispatching on the final byte alone.
- Reset every mouse and focus mode on exit; a shell with `?1003` still active
  receives motion reports as garbage.

## Probe

```sh
printf '\033[?1000h\033[?1006h'; cat -v; printf '\033[?1006l\033[?1000l'
```

Click; a press prints `^[[<0;x;yM` and the release `^[[<0;x;ym`. Repeat with
`?1003` to see motion, and with `?1004` and a window switch to see `^[[I` and
`^[[O`.

## Sources

- [XTerm Control Sequences, Mouse Tracking](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking)
- [rxvt-unicode(7), mouse reporting](http://pod.tst.eu/http://cvs.schmorp.de/rxvt-unicode/doc/rxvt.7.pod)
