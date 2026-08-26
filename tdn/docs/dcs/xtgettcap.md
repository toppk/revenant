# XTGETTCAP: Request Termcap/Terminfo String

Status: xterm.

XTGETTCAP asks the terminal for the value of a terminfo capability by name.
It lets an application learn what the terminal *thinks* its terminfo says
without a terminfo database on the local machine, which matters over SSH,
in containers, and for terminals whose entry is not installed.

## Syntax

```text
DCS + q hex-name [ ; hex-name ]… ST          request
DCS 1 + r hex-name = hex-value [ ; … ] ST     reply, valid
DCS 0 + r [ hex-name ] ST                     reply, invalid
```

Names and values are hex-encoded ASCII: `TN` is `544e`, `Co` is `436f`,
`RGB` is `524742`. Several names may be requested at once; xterm answers all
of them in one reply and reports `0` if any name is unknown. Other
terminals may answer each name separately or omit unknown names from a
`1` reply, so parse the reply by name rather than by position.

xterm gates this behind `allowTcapOps`, which is `true` by default but
frequently disabled by distributions.

## Useful names

<!-- markdownlint-disable MD013 -->

| Name | Meaning | Why ask |
| --- | --- | --- |
| `TN` | Terminal name | The terminfo entry the terminal wants; xterm answers with its `termName` |
| `Co` / `colors` | Number of colors | `256` versus `16777216` |
| `RGB` | Direct color capability | Terminfo's spelling of truecolor support |
| `Tc` | Truecolor (tmux convention) | The other spelling; ask for both |
| `Smulx` | Styled underline | `CSI 4 : Ps m` support |
| `Setulc` | Underline color | `CSI 58 ; … m` support |
| `Sync` | Synchronized output | Mode 2026 as a terminfo string |
| `Ss` / `Se` | Set/reset cursor style | DECSCUSR support |
| `kitty-query-*` | kitty-specific | Answered only by kitty; see its docs |

<!-- markdownlint-enable MD013 -->

Key-related names (`kf1`, `kcuu1`, …) are also useful: they tell an
application the encoding the terminal will actually send.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Terminal | XTGETTCAP | Notes |
| --- | --- | --- |
| xterm | Yes | [ctlseqs](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Device-Control-functions); `allowTcapOps` |
| VTE | `?` | |
| Konsole | `?` | |
| kitty | Yes | [protocol extensions](https://sw.kovidgoyal.net/kitty/protocol-extensions/) |
| WezTerm | Yes | [escape sequences](https://wezterm.org/escape-sequences.html) |
| Ghostty | Yes | [VT reference](https://ghostty.org/docs/vt/dcs/xtgettcap) |
| foot | Yes | [foot-ctlseqs](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd) |
| Alacritty | `?` | |
| Contour | Yes | [VT extensions](https://contour-terminal.org/vt-extensions/) |
| mintty | Yes | [CtrlSeqs](https://github.com/mintty/mintty/wiki/CtrlSeqs) |
| PuTTY | ? | |
| Windows Terminal | ? | not in the console VT list |
| Apple Terminal | ? | |
| iTerm2 | `?` | |
| xterm.js | ? | not in vtfeatures |
| tmux | `?` | |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
# TN ; Co ; RGB
tools/query '\033P+q544e;436f;524742\033\\'
```

Decode the reply with `xxd -r -p` on each `hex-name=hex-value` pair.

## Sources

- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Device-Control-functions)
- [terminfo(5)](https://invisible-island.net/ncurses/man/terminfo.5.html)
- [kitty: XTGETTCAP](https://sw.kovidgoyal.net/kitty/protocol-extensions/)
