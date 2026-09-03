---
man: revenant-firstrun
section: 7
manual: revenant
description: menus, keys, and fonts on the first run
---

# First run

<!-- markdownlint-disable MD013 -->

Revenant starts with xterm's defaults on purpose: the `fixed` bitmap font, an
80x24 window, and 1024 lines of history. The traditional `fixed` bitmap font
is often too small on a modern high-density display, so configure Revenant
before judging it. Shipping useful modern defaults is a future goal; today a
few `~/.Xresources` lines are part of the first run.

## Check your setup

Run the read-only welcome assistant first. It tells you whether X resources
are loaded, whether a scalable font is configured, and which fonts and tools
are missing:

```sh
revenant -welcome
```

It reports the active renderer and cell size, app-default and live X resource
status, the resolved primary/emoji/CJK fonts, and whether `xrdb` is installed.
On Debian-family, Fedora-family, and Arch-family systems it gives appropriate
package suggestions for missing capabilities; other systems receive generic
capability names. It does not install packages, edit files, load resources, or
use the network.

When it detects that the unconfigured bitmap default may be difficult to read,
it prints a small `XTerm*` fragment for `~/.Xresources` that switches to Xft
at a readable size and enlarges history.
Apply a fragment only after reviewing it, then load it with
`xrdb -merge ~/.Xresources`. A visual Unicode sample is shown when the report
is itself running inside Revenant with the Xft renderer and emoji coverage;
elsewhere it avoids judging the host terminal. The final support block is
designed to be copied into a bug report without including home paths or the
complete resource database.

For colors, [terminal.love](https://terminal.love/) offers a scheme catalog
with a live demo and an Xresources export; review its `foreground`,
`background`, and `color0` through `color15` entries before adding them.

## Running it

Installed packages put `revenant` on your path. From a checkout, run the
binary in your build directory instead:

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
