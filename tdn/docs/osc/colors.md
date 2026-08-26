# Colors

Status: xterm, with an Extension for scheme reports.

OSC 4 and OSC 10–19 change and query the colors an emulator uses for the
palette, default foreground and background, cursor, and selection. They are
the only portable way for an application to discover whether it is running
on a dark or light background.

## Syntax

### Palette: OSC 4 and OSC 5

```text
OSC 4 ; index ; spec ST         set palette entry
OSC 4 ; index ; ? ST            query palette entry
OSC 5 ; index ; spec ST         set special color (bold, underline, blink, reverse)
OSC 104 ; index ST              reset palette entry
OSC 104 ST                      reset entire palette
OSC 105 ; index ST              reset special color
```

`index` ranges over 0–255 (or 0–15 in emulators that lock the extended
cube). Several `index ; spec` pairs may appear in one string. xterm's OSC 5
indices are 0 bold, 1 underline, 2 blink, 3 reverse, 4 italic; few emulators
implement them.

### Dynamic colors: OSC 10–19

```text
OSC 10 ; spec ST    default foreground
OSC 11 ; spec ST    default background
OSC 12 ; spec ST    cursor color
OSC 13 ; spec ST    pointer foreground
OSC 14 ; spec ST    pointer background
OSC 15 ; spec ST    Tektronix foreground
OSC 16 ; spec ST    Tektronix background
OSC 17 ; spec ST    highlight (selection) background
OSC 18 ; spec ST    Tektronix cursor
OSC 19 ; spec ST    highlight (selection) foreground
OSC 110–119 ST      reset the corresponding color
```

A `spec` of `?` queries instead of sets. Successive values in one string
apply to successive selectors: `OSC 10 ; fg ; bg ST` sets 10 and 11.

### Color specifications

`spec` is parsed like XParseColor:

```text
rgb:RR/GG/BB          1–4 hex digits per channel, scaled to the digit count
rgb:RRRR/GGGG/BBBB
#RGB #RRGGBB #RRRRGGGGBBBB
rgbi:R/G/B            floating point 0.0–1.0
name                  X11 color names such as red, DarkSlateGray
```

Replies use the 16-bit `rgb:RRRR/GGGG/BBBB` form regardless of how the
color was set. Emulators without the X11 color database reject names.

### Query and reply

```text
Application:  OSC 11 ; ? ST
Terminal:     OSC 11 ; rgb:1e1e/1e1e/1e1e ST
```

The reply uses the same terminator the query used in xterm; other emulators
always reply with `ST` or always with `BEL`. Parse both.

## Behavior

- Palette changes affect cells already on screen: the palette is indexed at
  render time. Truecolor (`SGR 38;2`) cells are unaffected.
- Reset (`104`, `110`–`119`) restores the configured value, not the value
  before the last OSC. Applications cannot restore an unknown prior state;
  see [Modes: ownership and cleanup](../csi/modes.md#ownership-and-cleanup).
- Queries are reports and share every hazard described in
  [Queries and reports](../csi/queries.md). Under tmux, OSC 10/11 queries are
  answered by tmux from its own idea of the outer terminal, which may be
  stale or empty.
- Some emulators (Konsole, VTE) treat OSC 4 as changing the current profile
  only until reset; others (kitty) can be told to persist.

### Color-scheme reports: DEC mode 2031

Status: Extension (Contour).

Querying OSC 11 gives one color; it does not say whether the user intends a
dark or light theme. Contour introduced a mode that reports the theme as a
category and notifies on change:

```text
CSI ? 2031 h          enable unsolicited theme reports
CSI ? 2031 l          disable
CSI ? 996 n           query the current theme
CSI ? 997 ; 1 n       report: dark
CSI ? 997 ; 2 n       report: light
```

The report is a DSR-style CSI, not an OSC, and is emitted whenever the
emulator's theme changes while the mode is set. Applications should treat it
as a hint and still honor explicit `NO_COLOR` or their own configuration.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OSC 4 set | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Partial[^tm] |
| OSC 4 query | Yes | Yes | ? | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | ? | ? | Yes | Yes | Partial[^tm] |
| OSC 10/11 set | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Partial[^tm] |
| OSC 10/11 query | Yes | Yes | ? | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | ? | ? | Yes | Yes | Partial[^tm] |
| OSC 12 cursor | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | ? | ? | Yes | Yes | ? |
| OSC 17/19 selection | Yes | Yes | ? | ? | ? | ? | ? | ? | ? | Yes | ? | ? | ? | ? | ? | ? |
| 104/110/111 reset | Yes | Yes | ? | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | ? | ? | Yes | Yes | ? |
| Mode 2031 | ? | ? | ? | Yes (0.35)? | ? | Yes | Yes | ? | Yes | ? | ? | ? | ? | Yes | ? | ? |
| X11 color names | Yes | Yes | ? | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

[^tm]: tmux forwards sets to the outer terminal and answers queries itself;
    the answer may not match the outer terminal until it has been refreshed.

## Probe

```sh
tools/query bg                # OSC 11 ; ? — reply is ^[]11;rgb:rrrr/gggg/bbbb^[\
tools/query fg
printf '\033]4;1;rgb:ff/00/ff\033\\'; printf '\033[31mred is now magenta\033[0m\n'
printf '\033]104;1\033\\'
printf '\033[?2031h'; printf '\033[?996n'   # expect CSI ? 997 ; 1 n or ; 2 n
```

## Sources

- [XTerm Control Sequences, OSC 4/5/10–19](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
- [XParseColor(3) color formats](https://www.x.org/releases/current/doc/man/man3/XParseColor.3.xhtml)
- [Contour: dark and light mode detection](https://contour-terminal.org/vt-extensions/color-palette-update-notifications/)
- [kitty: color control](https://sw.kovidgoyal.net/kitty/color-stack/)
- [Ghostty VT reference](https://ghostty.org/docs/vt)
- [foot: escape sequences](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd)
- [WezTerm escape sequences](https://wezterm.org/escape-sequences.html)
- [iTerm2 proprietary escape codes](https://iterm2.com/documentation-escape-codes.html)
- [tmux(1)](https://man.openbsd.org/tmux)
