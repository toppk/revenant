# Kitty graphics protocol

Status: Extension. Defined by kitty; adopted by WezTerm, Ghostty, Konsole,
and others.

The protocol separates *images* from *placements*. An image is transmitted
once and given an id; any number of placements show it at cell positions with
z-order, offsets, and cropping. Placements can be deleted individually, and
images can be referenced from ordinary text cells so that multiplexers pass
them through.

## Syntax

```text
APC G control-data ; payload ST
```

That is `ESC _ G key=value,key=value ; base64 ESC \`. The payload is base64
and must be split into chunks of at most 4096 bytes; every chunk except the
last carries `m=1`. Only the first chunk carries the full control data;
continuation chunks need `m=` and optionally `q=`.

## Control keys

<!-- markdownlint-disable MD013 -->

| Key | Values | Meaning |
| --- | --- | --- |
| `a` | `t` transmit, `T` transmit and display, `p` put (display an existing image), `d` delete, `q` query, `f` frame, `a` animate, `c` compose | Action; default `t` |
| `f` | `24` RGB, `32` RGBA, `100` PNG | Pixel format; default 32 |
| `t` | `d` direct in payload, `f` file path, `t` temporary file (terminal deletes it), `s` POSIX shared memory | Transmission medium; default `d` |
| `s`, `v` | pixels | Width and height; required for `f=24` and `f=32` |
| `S`, `O` | bytes | Size and offset within a file or shm object |
| `m` | `0` or `1` | More chunks follow |
| `o` | `z` | Payload is zlib-compressed |
| `i` | 1–4294967295 | Image id, chosen by the client |
| `I` | number | Image number; terminal assigns the id and reports it |
| `p` | number | Placement id |
| `q` | `0`, `1`, `2` | Quiet: 0 respond always, 1 suppress OK, 2 suppress everything |
| `x`, `y`, `w`, `h` | pixels | Source rectangle to display |
| `X`, `Y` | pixels | Offset within the first cell |
| `c`, `r` | cells | Columns and rows to scale into |
| `z` | integer | Z-index; negative values draw under text |
| `C` | `1` | Do not move the cursor after display |
| `U` | `1` | Virtual placement, referenced by Unicode placeholders |
| `d` | letters | Delete target; see below |

<!-- markdownlint-enable MD013 -->

## Responses

```text
APC G i=31 ; OK ST
APC G i=31 ; ENOENT:image not found ST
```

Responses echo `i` (and `I`, `p` when used) and carry `OK` or an error code
with a message. Documented codes include `EINVAL`, `ENOENT`, `EBADF`,
`ENODATA`, `EFBIG`, `ENOSPC`, `EIO`, `EUNSUPPORTED`. The `q=2` setting turns
responses off for fire-and-forget use; keep responses on while querying
support.

## Delete

`a=d` with `d=` selects what to remove. A lowercase letter deletes
placements; the uppercase letter also frees the image data.

| `d` | Target |
| --- | --- |
| `a`/`A` | All placements on the visible screen |
| `i`/`I` | Image with id `i` (optionally placement `p`) |
| `n`/`N` | Image with number `I` |
| `c`/`C` | Placements intersecting the cursor cell |
| `p`/`P` | Placements at cell `x`,`y` |
| `q`/`Q` | Placements at cell `x`,`y` with z-index `z` |
| `x`/`X`, `y`/`Y`, `z`/`Z` | Placements in column, row, or z-index |
| `f`/`F` | Animation frames |

## Unicode placeholders

With `U=1` the terminal creates a *virtual* placement that draws nothing by
itself. The client then writes the placeholder character U+10EEEE into the
cells the image should cover. The image id is carried in the cell's
foreground color (the low 24 bits, with an optional high byte in the
underline color), and the row and column within the image are encoded with
combining diacritics from a documented table (U+0305, U+030D, …). Any program
that moves text cells intact — tmux, an editor, a pager — moves the image with
them without understanding the protocol. This is the intended path through
multiplexers, combined with tmux's passthrough (`allow-passthrough on` and
`DCS tmux ; … ST` wrapping) to deliver the transmission.

## Animation and composition

`a=f` adds frames to an image, `a=a` controls playback (`s=` state, `r=`
current frame, `z=` delay gap), and `a=c` composes one frame onto another.
Support outside kitty is partial; treat these as kitty-only unless the
emulator documents them.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Emulator | Support | Notes |
| --- | --- | --- |
| xterm | ? | |
| VTE | ? | |
| Konsole | Yes, 22.04+[^konsole] | Subset; `?` for placeholders and animation |
| kitty | Yes[^kitty] | Reference implementation |
| WezTerm | Yes[^wez] | Placeholders `?`; animation No |
| Ghostty | Yes[^ghostty] | Placeholders Yes; animation No |
| foot | ? | |
| Alacritty | No[^alac] | Declined by the maintainers |
| Contour | Partial[^contour] | Own "good image protocol" plus partial kitty |
| mintty | ? | |
| PuTTY | ? | |
| Windows Terminal | ? | |
| Apple Terminal | ? | |
| iTerm2 | ? | |
| xterm.js | ? | |
| tmux | Partial[^tmux] | Passthrough with `allow-passthrough`; placeholders render as cells |
| wayst | Yes[^wayst] | |
| st | Partial | Community patch only |

<!-- markdownlint-enable MD013 -->

[^kitty]: [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).
[^konsole]: [Konsole 22.04 announcement](https://kde.org/announcements/gear/22.04.0/).
[^wez]: [WezTerm imgcat](https://wezterm.org/imgcat.html).
[^ghostty]: [Ghostty VT reference](https://ghostty.org/docs/vt).
[^contour]: [Contour VT extensions](https://github.com/contour-terminal/contour/blob/master/docs/vt-extensions/index.md).
[^tmux]: [tmux manual, `allow-passthrough`](https://man.openbsd.org/tmux#allow-passthrough).
[^wayst]: [wayst](https://github.com/91861/wayst).

[^alac]: [Alacritty issue #910, "Sixel/graphics support"](https://github.com/alacritty/alacritty/issues/910), closed as out of scope.

## Probe

Query support, then display a 2×2 red RGB image scaled to 4 columns:

```sh
stty -echo -icanon min 0 time 5
printf '\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\'
dd bs=64 count=1 2>/dev/null | cat -v; echo
stty sane
python3 -c '
import base64,sys
px=bytes([255,0,0])*4
sys.stdout.write("\033_Ga=T,f=24,s=2,v=2,c=4,r=2,i=1;%s\033\\\\\n"
                 % base64.b64encode(px).decode())'
printf '\033_Ga=d,d=I,i=1\033\\'      # delete it and free the data
```

## Sources

- [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/)
- [tmux manual](https://man.openbsd.org/tmux)
