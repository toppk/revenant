# Other DCS commands

Less common device-control strings, most of which exist for a single
terminal or a historical DEC feature.

## DECUDK: User-Defined Keys

Status: DEC.

```text
DCS Pc ; Pl | key / hex-string ; key / hex-string … ST
```

Programs function keys to send arbitrary strings. `Pc` selects clear-all
(`0`) or clear-only-listed (`1`); `Pl` selects locked (`0`) or unlocked
(`1`). Each definition is a key number, `/`, and a hex-encoded string.

Most modern emulators do not implement DECUDK, and those that do (xterm)
typically disable it because a remote program could rebind keys to inject
commands. Treat it as unavailable.

Source: [VT510: DECUDK](https://vt100.net/docs/vt510-rm/DECUDK.html).

## DECRSPS: Restore Presentation State

Status: DEC.

```text
DCS Ps $ t data ST
```

Restores state previously reported by DECRQPSR (`CSI Ps $ w`): cursor
information (`Ps = 1`) or tab stops (`Ps = 2`). xterm implements both
directions; other emulators generally do not. Ghostty and foot document
tab-stop restore only `?`.

Source: [VT510: DECRSPS](https://vt100.net/docs/vt510-rm/DECRSPS.html).

## XTSETTCAP: Set Termcap/Terminfo Data

Status: xterm.

```text
DCS + p Pt ST
```

The write counterpart of [XTGETTCAP](xtgettcap.md): xterm accepts a
terminfo-style name (`Pt` is the hex-encoded entry name) and switches its
key-encoding behavior to match. Gated by `allowTcapOps`. No other emulator
implements it.

Source: [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Device-Control-functions).

## Sixel and ReGIS

```text
DCS Pa ; Pb ; Ph q data ST      Sixel
DCS Pm p data ST                ReGIS
```

Both are DCS commands with large payloads; see [Sixel](../graphics/sixel.md)
and the [Graphics overview](../graphics/index.md). ReGIS is implemented by
xterm (when built with it) and essentially nothing else.

## tmux control mode

Status: Vendor-private (tmux).

```text
DCS 1000 p                       begins control mode output
```

When tmux is started with `-C` or `-CC`, it emits `DCS 1000 p` and then a
line-oriented text protocol (`%begin`, `%output`, `%end`, …) instead of a
rendered screen. iTerm2 uses `-CC` to present tmux windows as native tabs.
The stream ends with `ST`. This is a client protocol rather than a terminal
control sequence and is documented in tmux's manual under "Control mode".

Source: [tmux(1): Control mode](https://man.openbsd.org/tmux#CONTROL_MODE).

## Sources

- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [VT510 Programmer Information](https://vt100.net/docs/vt510-rm/)
