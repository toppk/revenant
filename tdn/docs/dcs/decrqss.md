# DECRQSS: Request Status String

Status: DEC, with a **Disputed** reply code.

DECRQSS asks the terminal to report the current setting of one control
function, encoded as the parameters that would reproduce it. It is the only
general way to read back values such as the current SGR attributes or the
scrolling margins.

## Syntax

```text
DCS $ q Pt ST                 request
DCS Ps $ r Pt ST              reply (DECRPSS)
```

`Pt` in the request is the intermediate and final bytes of the control to
query, for example `m` for SGR or `SP q` for DECSCUSR. `Pt` in the reply is
the same suffix preceded by the current parameters, so that `CSI` followed
by the reply's `Pt` would set the reported value.

## The disputed status code

The VT510 reference manual defines the reply `Ps` as `0` for a valid
request and `1` for an invalid one. *XTerm Control Sequences* defines the
opposite: `DCS 1 $ r … ST` for a valid request and `DCS 0 $ r … ST` for an
invalid one. xterm has used its reading for so long that nearly every
emulator implementing DECRQSS follows xterm, and terminfo-aware software
expects `1` to mean valid.

An application should accept a reply as valid when `Ps` is `1` and the
payload is non-empty, treat `Ps` `0` with an empty payload as "invalid", and
distrust any terminal claiming DEC semantics until confirmed by a probe.
Emulator authors should follow xterm and document that they do.

## Common requests

<!-- markdownlint-disable MD013 -->

| `Pt` | Control | Reply payload |
| --- | --- | --- |
| `m` | SGR | Current attributes, e.g. `0;1;38:2::255;0;0m` |
| `r` | DECSTBM | `top;bottomr` scrolling margins |
| `s` | DECSLRM | `left;rights` margins (only meaningful with mode 69) |
| `SP q` | DECSCUSR | `Ps SP q` current cursor style |
| `" q` | DECSCA | `Ps " q` character protection attribute |
| `" p` | DECSCL | `Pl;Pc " p` conformance level |
| `t` | DECSLPP | `Ps t` lines per page |
| `$ \|` | DECSCPP | `Ps $ \|` columns per page |
| `* \|` | DECSNLS | `Ps * \|` number of lines |

<!-- markdownlint-enable MD013 -->

The SGR reply is the one most applications care about and the one with the
most variation: terminals differ in whether they emit colon-separated
extended colors, whether they include the leading `0`, and whether they
report underline style.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Terminal | DECRQSS | Notes |
| --- | --- | --- |
| xterm | Yes | [ctlseqs](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Device-Control-functions); reply `1` = valid |
| VTE | Yes | `?` which `Pt` values |
| Konsole | `?` | |
| kitty | Yes | `SP q`, `m`, `r` documented in source `?` |
| WezTerm | Yes | [escape sequences](https://wezterm.org/escape-sequences.html) |
| Ghostty | Yes | [VT reference](https://ghostty.org/docs/vt/dcs/decrqss) |
| foot | Yes | [foot-ctlseqs](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd) |
| Alacritty | `?` | |
| Contour | Yes | [VT extensions](https://contour-terminal.org/vt-extensions/) |
| mintty | Yes | [CtrlSeqs](https://github.com/mintty/mintty/wiki/CtrlSeqs) |
| PuTTY | ? | not listed in the manual |
| Windows Terminal | Partial | [console VT sequences](https://learn.microsoft.com/windows/console/console-virtual-terminal-sequences) list `m` and `r` `?` |
| Apple Terminal | `?` | |
| iTerm2 | Yes | `?` which `Pt` values |
| xterm.js | Yes | [vtfeatures](https://xtermjs.org/docs/api/vtfeatures/): `m`, `r`, `SP q` |
| tmux | Yes | answers for the pane's state `?` |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
tools/query '\033P$qm\033\\'      # current SGR
tools/query '\033P$q q\033\\'     # cursor style
tools/query '\033P$qr\033\\'      # scrolling margins
```

## Sources

- [VT510 Video Terminal Programmer Information: DECRQSS](https://vt100.net/docs/vt510-rm/DECRQSS.html)
- [VT510: DECRPSS](https://vt100.net/docs/vt510-rm/DECRPSS.html)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Device-Control-functions)
