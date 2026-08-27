# xterm+

xterm+ is an X11 terminal emulator that looks, configures, and behaves like
xterm, with `libghostty-vt` as its terminal core.

If you already run xterm, xterm+ is meant to be a drop-in replacement: the
same `XTerm` resource class, the same `vt100` widget, the same Ctrl+button
popup menus, the same `~/.Xresources` lines. If you have never run xterm,
this documentation starts from zero — including the X11 configuration
mechanism that xterm inherits and that most modern terminals never had.

## Two product bars

xterm+ has two requirements that pull in different directions, and the
project keeps both honest:

1. **xterm fidelity.** xterm patch 410 defines the visible X11 contract:
   resources, menus, translations, geometry, fonts. Anything xterm shows the
   user, xterm+ tries to show the same way.
2. **Modern terminal capability.** `libghostty-vt` supplies parsing,
   terminal state, reflow, history, key encoding, and query responses. xterm
   compatibility shapes *how* a feature is exposed; it never justifies
   leaving a basic capability unwired.

Where the two conflict, the difference is written down rather than hidden:
intentional departures live in `DRIFT.md`, unfinished work in `ROADMAP.md`,
and every menu entry and command-line option is classified in the
[compatibility](compatibility/command-line-feasibility.md) section.

## Where to go

<div class="grid cards" markdown>

- **New to xterm and X11?**
  Start with [X resources, explained](configuration/xresources.md). It is
  written for people who have never seen a `.Xresources` file.

- **Copying and pasting**
  Learn why X11 has [PRIMARY, CLIPBOARD, and cut
  buffers](usage/copy-paste.md), which one mouse selection uses, and what
  Shift+Insert actually pastes.

- **Opening terminal hyperlinks**
  Learn how [OSC 8 links](usage/hyperlinks.md) use Shift-hover and
  Shift+Button 1, and why only HTTP and HTTPS targets launch.

- **Coming from xterm?**
  Go straight to [Configuring xterm+](configuration/xterm-plus.md) and
  the [command-line feasibility](compatibility/command-line-feasibility.md)
  table to see which of your options already work. Also read the
  [major default keyboard-input difference](compatibility/keyboard-input.md):
  xterm+ deliberately distinguishes Ctrl-I from Tab, Ctrl-M from Enter, and
  Ctrl-[ from Escape without application opt-in.

- **Building it**
  [Install](getting-started/install.md) covers the stub build for UI work
  and the full `libghostty-vt` build.

- **Something looks wrong**
  [Diagnostics](reference/diagnostics.md) explains `-report-config`, the
  structured log, and the CPU profiling helper. The
  [interactive probe suite](reference/probes.md) provides named visual and
  keyboard fixtures for comparing xterm+, xterm, and Ghostty.

- **Writing a terminal application or emulator**
  The [Terminal Developers Network](https://toppk.github.io/xterm-plus/tdn/) explains control protocols,
  their history, compatibility differences, and reproducible probes.

</div>

## Status

xterm+ is early. Today it runs a real PTY-backed shell, renders the
libghostty grid through Xlib bitmap fonts or Xft, handles resize with reflow,
scrollback with the real Athena scrollbar, font switching from the font menu
or Shift+keypad, [named X11 selection with middle-button
paste](usage/copy-paste.md), [OSC 8 hyperlinks](usage/hyperlinks.md),
application mouse and focus reporting, and the mode toggles in the main and VT
menus. Startup cursor-shape resources, palette resources, and Kitty graphics
are on the roadmap. Application-selected block, underline, and bar
cursors and blink requests work now. Menu entries and options that are not
implemented yet stay visible but insensitive, so the UI never claims more than
the code delivers.
