# Sixel

Status: DEC. Defined for the VT240 and VT330/VT340; xterm is the reference
software implementation.

Sixel encodes a bitmap as printable ASCII, six vertical pixels per byte, with
a palette of color registers. It is the oldest image protocol in use and the
only one with a DEC manual behind it.

## Syntax

```text
DCS Pa ; Pb ; Ph q sixel-data ST
```

| Parameter | Meaning |
| --- | --- |
| `Pa` | Pixel aspect ratio selector: 0, 1, 5, 6 → 2:1; 2 → 5:1; 3, 4 → 3:1; 7, 8, 9 → 1:1. Emulators generally ignore it and use 1:1 |
| `Pb` | Background: 0 or 2 → pixels with no data take the background color; 1 → they are left unchanged (transparent) |
| `Ph` | Horizontal grid size; ignored by emulators |

### Data grammar

<!-- markdownlint-disable MD013 -->

| Element | Form | Meaning |
| --- | --- | --- |
| Sixel byte | `?`–`~` (`0x3f`–`0x7e`) | Subtract `0x3f`; each of the six bits is one pixel, LSB at the top |
| Repeat | `! Pn byte` | Emit the byte `Pn` times |
| Color select | `# Pc` | Use register `Pc` for following bytes |
| Color define | `# Pc ; Pu ; Px ; Py ; Pz` | Set register `Pc`; `Pu`=1 HLS (`Px` hue 0–360, `Py` lightness, `Pz` saturation), `Pu`=2 RGB with components 0–100 |
| Carriage return | `$` | Return to the left edge of the current six-pixel band |
| New line | `-` | Move to the next band |
| Raster attributes | `" Pan ; Pad ; Ph ; Pv` | Aspect numerator/denominator and image width/height in pixels; lets the terminal allocate the area first |

<!-- markdownlint-enable MD013 -->

Registers default to 16 on a VT340; emulators allow 256 or more and report
the number through XTSMGRAPHICS. Colors are percentages, so `#0;2;100;0;0`
is pure red.

## Related modes

### DECSDM, mode 80 — Disputed

The VT330/VT340 manual defines `CSI ? 80 h` as *Sixel Display Mode*: when
set, the image is displayed as on a graphics terminal without scrolling, at
the top left; when reset, sixel output scrolls with text and the cursor moves
below the image.

xterm implemented the mode with the opposite polarity until release 369,
treating set as "scrolling enabled". Since 369 xterm follows the manual, and
the change log records the correction[^xtermlog]. Emulators that copied the
earlier xterm behavior are still in the field; applications should not rely
on mode 80 and should instead leave it at the default and use `?8452` to
control cursor placement.

### Mode 8452

`CSI ? 8452 h`: after an image, leave the cursor on the same row to the right
of the image, instead of on the next row at column 1. xterm extension,
adopted by several emulators.

### Mode 1070

`CSI ? 1070 h` (default set in xterm): each sixel image uses a private
palette; reset shares one palette across images, as the VT340 did.

### XTSMGRAPHICS

```text
CSI ? Pi ; Pa ; Pv S
```

`Pi` = 1 for color registers, 2 for sixel geometry, 3 for ReGIS geometry.
`Pa` = 1 read, 2 reset to default, 3 set to `Pv`, 4 read maximum. The reply
is `CSI ? Pi ; Ps ; Pv S` with `Ps` = 0 success. Sending
`CSI ? 2 ; 1 S` returns the largest image size the terminal accepts.

## Behavior

The image is painted starting at the cursor position. Rows the image covers
are marked as containing graphics; text written later overprints or is
overprinted depending on the emulator. Scrolling the row scrolls the image
strip with it; erasing the cells removes that strip in most emulators.

Sixel data for a large image is long. An emulator must parse it
incrementally, and applications on slow links should size images with the
raster attribute so the terminal can reserve space before data arrives.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Emulator | Sixel | Notes |
| --- | --- | --- |
| xterm | Yes[^ctl] | Build with `--enable-sixel-graphics`; `-ti vt340` or `decTerminalID: 340`; `sixelScrolling` resource |
| VTE | Yes, 0.70+[^vte] | Build option and `VTE_SIXEL` / `enable-sixel` property; distributions vary |
| Konsole | Yes, 22.04+[^konsole] | |
| kitty | No[^kitty] | Refused by design |
| WezTerm | Yes[^wez] | |
| Ghostty | No[^ghostty] | Not implemented; `?` for roadmap |
| foot | Yes[^foot] | |
| Alacritty | No[^alac] | Declined by the maintainers |
| Contour | Yes[^contour] | |
| mintty | Yes[^mintty] | |
| PuTTY | ? | |
| Windows Terminal | Yes, 1.22[^wt] | |
| Apple Terminal | ? | |
| iTerm2 | Yes[^iterm] | |
| xterm.js | Yes, addon[^xjs] | `@xterm/addon-image` |
| tmux | Yes, 3.4[^tmux] | `--enable-sixel` build option |
| mlterm | Yes[^mlterm] | |
| DomTerm | Yes[^domterm] | |
| RLogin | Yes[^rlogin] | |

<!-- markdownlint-enable MD013 -->

[^ctl]: [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html), "Sixel Graphics".
[^xtermlog]: [xterm change log](https://invisible-island.net/xterm/xterm.log.html), patch 369.
[^vte]: [VTE NEWS](https://gitlab.gnome.org/GNOME/vte/-/blob/master/NEWS).
[^konsole]: [Konsole 22.04 announcement](https://kde.org/announcements/gear/22.04.0/).
[^kitty]: [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).
[^wez]: [WezTerm imgcat](https://wezterm.org/imgcat.html).
[^ghostty]: [Ghostty VT reference](https://ghostty.org/docs/vt).
[^foot]: [foot README](https://codeberg.org/dnkl/foot).
[^contour]: [Contour](https://github.com/contour-terminal/contour).
[^mintty]: [mintty wiki, Control Sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs).
[^wt]: [Windows Terminal releases](https://github.com/microsoft/terminal/releases), 1.22.
[^iterm]: [iTerm2 images](https://iterm2.com/documentation-images.html).
[^xjs]: [@xterm/addon-image](https://github.com/jerch/xterm-addon-image).
[^tmux]: [tmux CHANGES](https://github.com/tmux/tmux/blob/master/CHANGES).
[^mlterm]: [mlterm](https://github.com/arakiken/mlterm).
[^domterm]: [DomTerm](https://domterm.org/).
[^rlogin]: [RLogin](https://kmiya-culti.github.io/RLogin/).

[^alac]: [Alacritty issue #910, "Sixel/graphics support"](https://github.com/alacritty/alacritty/issues/910), closed as out of scope.

## Probe

A 6×6 red square with a blue diagonal band, no external tools:

```sh
printf '\033P0;1;0q"1;1;6;6#0;2;100;0;0#1;2;0;0;100#0!6~-#0!6~\033\\\n'
```

Query the maximum geometry and register count:

```sh
stty -echo -icanon min 0 time 5
printf '\033[?2;4S\033[?1;1S'; dd bs=64 count=1 2>/dev/null | cat -v; echo
stty sane
```

Convert a real image with libsixel: `img2sixel photo.png`. Or:

```sh
python3 -c '
import sys
w,h=12,12
out="\033P0;1;0q\"1;1;%d;%d#0;2;0;80;0"%(w,h)
for band in range(0,h,6):
    out+="#0"+"~"*w+("-" if band+6<h else "")
sys.stdout.write(out+"\033\\\\\n")'
```

## Sources

- [VT330/VT340 Programmer Reference Manual, chapter 14, Sixel Graphics](https://vt100.net/docs/vt3xx-gp/chapter14.html)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [xterm change log](https://invisible-island.net/xterm/xterm.log.html)
- [libsixel](https://github.com/saitoha/libsixel)
- [lsix](https://github.com/hackerb9/lsix) and its notes on emulator quirks
