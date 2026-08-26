# Terminal Developers Network

The Terminal Developers Network (TDN) is a reference for people who write
terminal applications, terminal emulators, and compatibility tests. It
documents terminal protocols the way a web-platform reference documents the
web: one page per feature, with syntax, behavior, history, a compatibility
table across emulators, and a probe that can be run against a real terminal.

TDN is not a standards body and does not invent escape sequences. It keeps
three things separate that are often mixed together:

1. a published standard or vendor specification;
2. behavior documented by an emulator;
3. behavior observed in a named emulator version.

An observation is not promoted to a standard because several terminals share
it. An old specification does not erase useful modern behavior. Where
sources disagree, the page says so and marks the feature **Disputed**.

## Sections

<!-- markdownlint-disable MD013 -->

| Section | What it covers |
| --- | --- |
| [Conventions](conventions.md) | Notation, status vocabulary, the compatibility legend, the page template |
| [Terminals](terminals/index.md) | The emulators, multiplexers, and engines that appear in every compatibility table, with identity and documentation links |
| [Environment and detection](environment.md) | `TERM`, terminfo, environment variables, and how to ask the terminal what it can do |
| [Control characters and ESC](escape.md) | C0, C1, and two-byte escape commands |
| [CSI](csi/index.md) | Cursor, erase, SGR, scrolling, modes, queries, window operations |
| [OSC](osc/index.md) | Titles, hyperlinks, colors, clipboard, working directory, shell integration, notifications |
| [DCS](dcs/index.md) | DECRQSS, XTGETTCAP, XTVERSION, and other device-control strings |
| [Input](input/index.md) | Keyboard encodings from VT100 to the Kitty protocol, mouse, focus, bracketed paste |
| [Text](text/index.md) | UTF-8, character width, grapheme clusters, text sizing, rendering |
| [Graphics](graphics/index.md) | Sixel, Kitty graphics, iTerm2 inline images, ReGIS |
| [Glossary](glossary.md) | Terms used across the site |

<!-- markdownlint-enable MD013 -->

## Which terminals

Compatibility tables cover, in a fixed order, xterm, VTE (GNOME Terminal and
relatives), Konsole, kitty, WezTerm, Ghostty, foot, Alacritty, Contour,
mintty, PuTTY, Windows Terminal, Apple Terminal, iTerm2, xterm.js (VS Code
and other hosts), and tmux. The [Terminals](terminals/index.md) section
explains why those, what they share, and how to identify each one.

Every table cell is a claim with a source: **Yes**, **Partial**, or **No**
with a reference, **Observed** with a version and a probe, or `?` meaning
nobody has checked. A `?` is an invitation, not a guess.

## Probes

Reproducible probes live under `tools/` in this repository:

```sh
tools/query version da1      # who is this terminal?
tools/query mode 2026        # is a mode supported?
tools/sgr-sampler            # colors and underline styles
tools/sendosc link https://example.com example
tools/sendcsi blink-bar
```

Each feature page ends with the probe that produced its observations, so a
reader can rerun it in a new emulator version and update the table.

## Source map

- [ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
  for the control-function framework;
- [DEC VT manuals](https://vt100.net/docs/) for DEC behavior;
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
  for xterm and most of what descends from it;
- [terminfo(5)](https://invisible-island.net/ncurses/man/terminfo.5.html)
  for capability names;
- [terminal-wg specifications](https://gitlab.freedesktop.org/terminal-wg/specifications)
  for cross-emulator extensions;
- each emulator's own escape-sequence documentation, linked from its
  [terminal page](terminals/index.md);
- versioned probes for everything else.

## Relationship to xterm+

TDN started inside the [xterm+](https://toppk.github.io/xterm-plus/) project
because xterm+ needed it: a compatibility target has to be written down
before it can be met. The content is about terminals in general, not about
xterm+, and it is meant to become a community effort. xterm+-specific
decisions stay in the [xterm+ documentation](https://toppk.github.io/xterm-plus/docs/).
