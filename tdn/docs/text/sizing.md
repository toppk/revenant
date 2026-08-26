# Text sizing

Two mechanisms let text occupy something other than one cell per character:
the DEC double-width and double-height lines from the VT100, and kitty's
per-string text sizing protocol from 2025.

## DEC line attributes

Status: DEC.

```text
ESC # 3    DECDHL top half of a double-height, double-width line
ESC # 4    DECDHL bottom half
ESC # 5    DECSWL single-width, single-height (default)
ESC # 6    DECDWL double-width, single-height
```

The attribute applies to the whole current line. A double-width line holds
half the columns; text beyond that is discarded. Double height needs two lines
carrying the same text, one with `ESC # 3` and one with `ESC # 4`.

The VT100 rendered these with hardware; emulators must scale glyphs. xterm
does. Most modern emulators parse and ignore the sequences.

## kitty text sizing protocol

Status: Vendor-private. Adopters other than kitty: `?`.

```text
OSC 66 ; metadata ; text ST
```

`metadata` is a comma-separated list of `key=value` pairs:

| Key | Meaning |
| --- | --- |
| `s` | Scale: the text is rendered `s` cells tall, 1–7 |
| `w` | Width: the number of cells the text occupies, overriding measurement |
| `n`, `d` | Fractional scale numerator and denominator for subscript-style sizes |
| `v` | Vertical alignment within the scaled block: 0 top, 1 bottom, 2 centered |
| `h` | Horizontal alignment: 0 left, 1 right, 2 centered |

The text is at most a small number of code points and cannot contain
controls. `w` alone, with `s=1`, is the protocol's answer to width skew: the
application states how many cells the text should take, and the terminal
obeys instead of consulting its own tables.

Terminals that do not implement OSC 66 ignore the whole sequence, including
the text, so an application must query first. kitty documents detection via
its keyboard or graphics query paths and via `TERM_PROGRAM`; a generic probe
is to send OSC 66 with `w=1` and measure with `CSI 6 n`.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| DECDWL/DECDHL rendered | Yes[^ctl] | ? | ? | ? | ? | ? | ? | ? | ? | Yes[^mintty] | ? | ? | ? | ? | ? |
| OSC 66 | ? | ? | ? | Yes, 0.40[^tsp] | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

[^ctl]: [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).
[^mintty]: [mintty wiki, Control Sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs).
[^tsp]: [kitty text sizing protocol](https://sw.kovidgoyal.net/kitty/text-sizing-protocol/).

## Probe

```sh
printf '\033#6wide line\033#5\n'
printf '\033#3TALL\n\033#4TALL\n\033#5'
printf '\033]66;s=2;Big\033\\\n\n'
printf '\033]66;w=2;\xe2\x9d\xa4\033\\\033[6n'   # heart forced to width 2
```

## Sources

- [VT100 User Guide, line attributes](https://vt100.net/docs/vt100-ug/chapter3.html)
- [kitty text sizing protocol](https://sw.kovidgoyal.net/kitty/text-sizing-protocol/)
