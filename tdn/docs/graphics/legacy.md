# Legacy graphics

Two vector protocols predate every bitmap protocol. They are documented here
because xterm still implements them and because their design explains why
Sixel scrolls the way it does.

## ReGIS

Status: DEC. Remote Graphics Instruction Set, VT125, VT240, VT330/VT340.

```text
DCS Pn p command-string ST
```

`Pn` 0 or 1 resumes or starts a fresh command set. Commands are single
letters with bracketed arguments: `S` screen control, `W` writing control,
`P` position, `V` vector, `C` curve, `T` text, `L` load character set, `F`
fill, `R` report. Example, a line from the origin to (100,100):

```text
DCS p S(E) P[0,0] V[100,100] ST
```

ReGIS draws on a graphics plane the size of the screen, independent of text
cells; scrolling text does not move it. xterm implements it only when built
with `--enable-regis-graphics` and the terminal ID is 240 or above.

## Tektronix 4014

Status: xterm.

```text
CSI ? 38 h    switch to Tektronix mode (xterm opens a separate window)
ESC ^C        return to VT mode from Tektronix
```

xterm emulates a Tektronix 4014 storage-tube display in a separate window,
driven by the 4014's own byte protocol (GS for vector mode, FS point plot, US
alpha mode). It exists for legacy plotting software such as old `gnuplot`
drivers and has no modern users.

## Why not use these

- No modern emulator other than xterm implements ReGIS; none implements
  Tektronix.
- Vector protocols have no palette or raster model, so photographs are
  impossible.
- Modern terminals are UTF-8 and cell-oriented; a separate graphics plane
  does not survive scrollback, resizing, or multiplexing.

For new work use [Sixel](sixel.md) for breadth, the
[Kitty protocol](kitty.md) for lifecycle control, or the
[iTerm2 protocol](iterm2.md) for macOS-centric tools.

## Compatibility

| Emulator | ReGIS | Tektronix |
| --- | --- | --- |
| xterm | Yes, build option[^ctl] | Yes[^ctl] |
| All others in the standard column set | ? | ? |
| mlterm | Yes[^mlterm] | ? |

[^ctl]: [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html); [xterm manual](https://invisible-island.net/xterm/manpage/xterm.html), `-t` and Tek window.
[^mlterm]: [mlterm](https://github.com/arakiken/mlterm).

## Probe

```sh
printf '\033Pp S(E) P[0,0] V[200,100] \033\\'    # ReGIS line, xterm -ti vt340
printf '\033[?38h'                               # xterm: opens the Tek window
```

## Sources

- [VT330/VT340 Programmer Reference, ReGIS chapters](https://vt100.net/docs/vt3xx-gp/)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [Xterm Tektronix emulation, xterm manual](https://invisible-island.net/xterm/manpage/xterm.html)
