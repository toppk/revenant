# SGR: Select Graphic Rendition

Status: Standard (ECMA-48) for the core attributes; xterm, Extension, and
Convention for colors beyond 8, underline styles, and underline color.

```text
CSI Pm m
```

SGR sets the attributes applied to characters printed afterwards. Parameters
accumulate: `CSI 1 ; 31 m` is bold and red. `CSI m` and `CSI 0 m` reset every
attribute, including colors.

## Attributes

<!-- markdownlint-disable MD013 -->

| `Ps` | Effect | Reset by | Notes |
| --- | --- | --- | --- |
| `0` | Reset all | | Also resets foreground, background, underline color |
| `1` | Bold | `22` | Historically "bright"; see [bold-as-bright](#bold-and-bright) |
| `2` | Faint | `22` | Rendered as dimmer color or lighter weight |
| `3` | Italic | `23` | ECMA-48 says "italicized"; some terminals render inverse instead |
| `4` | Underline | `24` | `4:n` selects a style; see below |
| `5` | Slow blink | `25` | Often disabled by configuration |
| `6` | Rapid blink | `25` | Almost universally treated as `5` |
| `7` | Inverse | `27` | Swaps foreground and background at render time |
| `8` | Conceal | `28` | Rendered as spaces; text remains in the grid and can be copied |
| `9` | Strikethrough | `29` | |
| `21` | Double underline | `24` | ECMA-48; xterm used to treat as "bold off", now follows ECMA |
| `53` | Overline | `55` | |
| `58` | Underline color | `59` | Extension (kitty); same syntax as `38` |

<!-- markdownlint-enable MD013 -->

Parameters 10–20 (font selection), 26 (proportional), 51–52 (framed,
encircled), and 60–65 (ideogram attributes) are defined by ECMA-48 and
implemented almost nowhere; mintty is the notable exception.

### Underline styles

Status: Extension, originated by kitty.

```text
CSI 4 : 0 m    no underline
CSI 4 : 1 m    straight
CSI 4 : 2 m    double
CSI 4 : 3 m    curly
CSI 4 : 4 m    dotted
CSI 4 : 5 m    dashed
```

`CSI 4 m` equals `4:1` and `CSI 24 m` clears all styles. Terminfo announces
support with `Smulx` (styled underline) and `Setulc` (underline color); tmux
relays them when its outer terminal has them.

Terminals that do not understand sub-parameters typically apply plain
underline for `4:3` (they stop at the colon) or ignore the whole SGR. Neither
outcome is harmful, which is why applications ship curly underlines without
detection.

## Colors

<!-- markdownlint-disable MD013 -->

| `Ps` | Effect |
| --- | --- |
| `30`–`37` | Foreground from the 8-color palette |
| `40`–`47` | Background from the 8-color palette |
| `90`–`97` | Foreground from the bright 8 (aixterm; universal) |
| `100`–`107` | Background from the bright 8 |
| `38 ; 5 ; n` | Foreground from the 256-color palette |
| `48 ; 5 ; n` | Background from the 256-color palette |
| `38 ; 2 ; r ; g ; b` | Foreground truecolor |
| `48 ; 2 ; r ; g ; b` | Background truecolor |
| `39` / `49` | Default foreground / background |
| `58 ; 2 ; r ; g ; b`, `58 ; 5 ; n` | Underline color; `59` resets |

<!-- markdownlint-enable MD013 -->

The 256-color palette is 16 named colors, a 6×6×6 cube at 16–231 with levels
`0, 95, 135, 175, 215, 255`, and a 24-step gray ramp at 232–255. Entries are
redefinable with [OSC 4](../osc/colors.md); applications must not assume
index 1 is red.

### Color parameter forms

ISO 8613-6 defines the colon form with a color-space identifier:

```text
CSI 38 : 2 : id : r : g : b m     id is usually empty
CSI 38 : 5 : n m
```

xterm adopted the semicolon form `38;2;r;g;b` before sub-parameters were
common, and it became the de facto interchange form. Both are now widely
accepted; the differences:

- `38;2;r;g;b` is parsed by everything that supports truecolor, including
  terminals that reject colons;
- `38:2::r:g:b` survives being combined with other parameters in one
  sequence when the receiver does not implement it, because the receiver
  can skip one parameter;
- some terminals accept `38:2:r:g:b` (no id) as well; xterm documents both.

Terminfo reports truecolor as `RGB` (ncurses 6.1+) or the older tmux-style
`Tc` user capability; `COLORTERM=truecolor` is the shell-level convention.
See [Environment](../environment.md).

### Bold and bright

Classic terminals rendered SGR 1 by switching to the bright palette entry.
Modern emulators render a bold weight and, by configuration, may also
brighten the color. Applications wanting bright colors should use `90`–`97`
explicitly and treat `1` as weight only. xterm's `boldColors` resource,
kitty's `bold_is_bright`, and VTE's `bold-is-bright` control this.

### Faint

There is no bright counterpart. Emulators blend toward the background or use
a lighter font weight; the result differs enough that faint text should
never carry information that regular text does not.

## Push and pop

Status: xterm.

```text
CSI # {          XTPUSHSGR: push current attributes
CSI # }          XTPOPSGR: pop
CSI Ps ; … # {   push only the listed attribute classes
```

Implemented by xterm (opt-in build), Contour, and mintty; elsewhere `?`.
Applications cannot rely on it and should track attributes themselves.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 256 colors | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Truecolor `38;2` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes (0.71+) | Yes | ? | Yes | Yes | Yes (`Tc`/`RGB` feature) |
| Colon form `38:2` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Yes |
| Italic `3` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | Yes | Yes | Yes | pass |
| Strikethrough `9` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | pass |
| Underline styles `4:n` | Yes | Yes (0.52+) | Yes | Yes | Yes | Yes | Yes | Yes (0.12+) | Yes | Yes | ? | Yes | ? | Yes | Yes | Yes (`Smulx`) |
| Underline color `58` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Yes (`Setulc`) |
| Overline `53` | Yes | Yes | ? | ? | Yes | Yes | ? | ? | Yes | Yes | ? | ? | ? | ? | Yes | pass |
| Blink `5` | Yes | Yes (config) | Yes | ? | Yes | Yes | Yes | ? | Yes | Yes | Yes | Yes | Yes | Yes | Yes | pass |
| XTPUSHSGR | Yes (opt) | ? | ? | ? | ? | ? | ? | ? | Yes | Yes | ? | ? | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

"pass" means tmux relays the attribute to the outer terminal when that
terminal's terminfo advertises it.

## Probe

```sh
tools/sgr-sampler
tools/query tcap RGB       # hex reply carries the terminfo value
tools/query tcap Smulx
tools/query decrqss m      # DECRQSS: current SGR, e.g. ^[P1$r0;1;31m^[\
```

## Sources

- [ECMA-48 §8.3.117](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [ISO/IEC 8613-6 (ITU T.416) §13.1.8](https://www.itu.int/rec/T-REC-T.416/en)
- [XTerm Control Sequences, SGR](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Functions-using-CSI-_-ordered-by-the-final-character_s_)
- [kitty underline styles](https://sw.kovidgoyal.net/kitty/underlines/)
- [Gist: True Colour support in terminals](https://github.com/termstandard/colors)
- [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs)
