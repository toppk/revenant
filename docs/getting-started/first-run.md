---
man: revenant-firstrun
section: 7
manual: revenant
description: menus, keys, and fonts on the first run
---

# First run

<!-- markdownlint-disable MD013 -->

Run the binary from your build directory:

```sh
./build-ghostty/revenant
```

Without arguments Revenant starts `$SHELL` (falling back to `/bin/sh`) with
`TERM=xterm-256color`. To run a specific command instead, use xterm's
trailing `-e` form; everything after `-e` is the command and its arguments:

```sh
./build-ghostty/revenant -e sh -lc 'printf "hello from Revenant\n"; exec "$SHELL"'
```

## The three menus

Like xterm, there is no menu bar. Menus pop up when you hold ++ctrl++ and
press a mouse button over the terminal:

| Gesture | Menu | What it holds |
| --- | --- | --- |
| Ctrl + button 1 | **Main Options** | Window and process controls, key modes, the live background-opacity slider, signals, and quit |
| Ctrl + button 2 | **VT Options** | Terminal behaviour: scrollbar, reverse video, autowrap, application cursor/keypad, scroll-on-key/output |
| Ctrl + button 3 | **VT Fonts** | Font slots Default through Huge, the TrueType (Xft) toggle |

Checkmarks are read back from the terminal engine each time a menu opens, so
if a program switches a mode with an escape sequence the menu reflects it.
Entries that are not implemented yet are shown greyed out; the complete list
with reasons is in the [popup-menu feasibility](../compatibility/menu-feasibility.md)
study.

The Main Options opacity control sits immediately below **SVG Screen Dump**. It
is a single continuous Athena slider, not a set of presets. It is available for
windows started with compositor-backed background opacity and changes only the
terminal's default background alpha.

## Keyboard controls

| Keys | Action |
| --- | --- |
| Shift + Page Up / Page Down | Scroll back / forward half a page |
| Shift + Insert | Paste the X11 primary selection |
| Shift + keypad + | Next larger configured font |
| Shift + keypad − | Next smaller configured font |
| Shift + Ctrl + keypad + | Next smaller font (xterm's historical binding) |

Mouse wheel scrolls five rows per tick while no program has enabled mouse
tracking. When a program does enable tracking, hold Shift to select text or
scroll locally. The Athena scrollbar on the left supports thumb dragging and
xterm's button-based scrolling.

## Fonts

Revenant has two renderers and you can switch between them at runtime from the
VT Fonts menu (`TrueType Fonts`):

- **Bitmap (Xlib)** — pixel-identical to traditional xterm, using X core
  fonts such as `fixed` or `-misc-fixed-*`.
- **Xft / fontconfig** — scalable TrueType fonts, `-fa 'Hack' -fs 12`, with
  UTF-8 glyphs from the selected face.

Changing font keeps your rows and columns and resizes the window to fit.

## Next

Almost everything else — colours, fonts, scrollback size, scrollbar side,
window title — is set through X resources. If that phrase means nothing to
you yet, read [X resources, explained](../configuration/xresources.md) next.
If it does, jump to [Configuring Revenant](../configuration/revenant.md).

<!-- markdownlint-enable MD013 -->
