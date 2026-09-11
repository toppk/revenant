# Graphics

Terminals display images through three live protocols and two historical
ones. None is a standard; each was defined by one vendor and adopted by
others to varying degrees.

## The protocols

<!-- markdownlint-disable MD013 -->

| Protocol | Status | Transport | Pixel formats | Placement model | Page |
| --- | --- | --- | --- | --- | --- |
| Sixel | DEC | `DCS q … ST` | Indexed palette, encoded six pixels per byte | Painted at the cursor, scrolls with text | [Sixel](sixel.md) |
| iTerm2 inline images | Extension | `OSC 1337 ; File=… BEL` | Any file format the emulator decodes | Occupies a cell rectangle at the cursor | [iTerm2](iterm2.md) |
| Kitty graphics | Extension | `APC G … ST` | RGB, RGBA, PNG; zlib; file and shm | Stored images with independent placements, z-order, and Unicode placeholders | [Kitty](kitty.md) |
| ReGIS | DEC | `DCS p … ST` | Vector commands | Separate graphics plane | [Legacy](legacy.md) |
| Tektronix 4014 | xterm | Mode switch | Vector | Separate window | [Legacy](legacy.md) |

<!-- markdownlint-enable MD013 -->

## Lifecycle model

The questions an application must ask of every protocol:

- **Does the image scroll?** Sixel and iTerm2 images are attached to the
  rows they were drawn on and scroll into scrollback with them. Kitty
  placements are attached to cells by default and can be pinned with virtual
  placements.
- **What happens on the alternate screen?** Images drawn on the alternate
  screen are discarded when it is left, in all three protocols.
- **Can the application delete it?** Only the Kitty protocol has a delete
  command. Sixel and iTerm2 images are removed by erasing the cells under them
  (`ED`, `EL`) or scrolling them away; emulators differ on whether partial
  erasure clips the image.
- **What happens on resize?** Cell-anchored images keep their cell rectangle
  and are rescaled or clipped; pixel-anchored images may not move. Behavior
  is emulator-specific in every protocol.
- **Where is the cursor afterward?** Sixel uses mode `?8452`; iTerm2 places
  the cursor after the image unless `doNotMoveCursor=1`; Kitty places it after
  the image unless `C=1`.

## Detection

- Sixel: send `CSI c` and look for parameter `4` in the DA1 reply.
- Kitty: send a query `APC G i=31,s=1,v=1,a=q,t=d,f=24 ; AAAA ST` and wait
  for `APC G i=31;OK ST`.
- iTerm2: `TERM_PROGRAM=iTerm.app` or `TERM_PROGRAM=WezTerm`; there is no
  in-band query.

Always pair detection with a timeout; see [Queries](../csi/queries.md).

## Multiplexers

tmux passes Sixel through to the outer terminal when built with
`--enable-sixel` (3.4)[^tmux] and does not understand Kitty or iTerm2 images.
The Kitty protocol's Unicode placeholders were designed for this case: the
image is transmitted once through a passthrough sequence, then referenced with
ordinary text cells that tmux can move and redraw.


[^tmux]: [tmux CHANGES](https://github.com/tmux/tmux/blob/master/CHANGES), 3.4.


## Probe

```sh
stty -echo -icanon min 0 time 5
printf '\033[c'; dd bs=64 count=1 2>/dev/null | cat -v; echo   # look for ;4;
printf '\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\'
dd bs=64 count=1 2>/dev/null | cat -v; echo                    # expect i=31;OK
stty sane
```

## Sources

- [VT330/VT340 Programmer Reference, chapter 14](https://vt100.net/docs/vt3xx-gp/)
- [kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/)
- [iTerm2 inline images](https://iterm2.com/documentation-images.html)

<!-- tdn:compatibility -->
