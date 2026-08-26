# XTVERSION: Report Terminal Name and Version

Status: xterm.

XTVERSION is a CSI request with a DCS reply. It returns the emulator's name
and version as free text, which makes it the most direct identification
probe available and also the least standardized: every terminal formats its
answer differently.

## Syntax

```text
CSI > Ps q          request; Ps is 0 or omitted
DCS > | Pt ST       reply; Pt is the name and version text
```

## Known reply forms

<!-- markdownlint-disable MD013 -->

| Terminal | Reply `Pt` | Source |
| --- | --- | --- |
| xterm | `XTerm(406)` | [ctlseqs](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html) |
| VTE | `VTE(7800)`-style, numeric version `?` | |
| Konsole | `?` | |
| kitty | `kitty(0.xx.y)` | [protocol extensions](https://sw.kovidgoyal.net/kitty/protocol-extensions/) |
| WezTerm | `WezTerm 20260820-005713-e95b3713` | [escape sequences](https://wezterm.org/escape-sequences.html) |
| Ghostty | `ghostty 1.x.y` | [VT reference](https://ghostty.org/docs/vt/csi/xtversion) |
| foot | `foot(1.x.y)` | [foot-ctlseqs](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd) |
| Alacritty | `?` | |
| Contour | `contour x.y.z` `?` | |
| mintty | `mintty 3.x.y` `?` | |
| PuTTY | no reply | |
| Windows Terminal | `?` | |
| Apple Terminal | no reply | |
| iTerm2 | `?` | |
| xterm.js | `?` | |
| tmux | `tmux 3.x` | [tmux CHANGES](https://raw.githubusercontent.com/tmux/tmux/master/CHANGES) |

<!-- markdownlint-enable MD013 -->

Do not parse the reply beyond "name, then version"; the separator, the
presence of parentheses, and the version format are all implementation
choices.

## Handling no reply

A terminal that does not implement XTVERSION ignores the request and sends
nothing. An application that blocks waiting for a DCS reply will hang. The
robust pattern is to send XTVERSION followed by DA1 (`CSI c`), which every
terminal answers, then read until the DA1 reply arrives: any DCS reply that
precedes it is the XTVERSION answer, and the DA1 reply doubles as the
"end of replies" marker. Apply a timeout as well, since a multiplexer or a
serial link can swallow either reply.

DA1 is the safer first probe because it is answered by every emulator in the
[landscape](../terminals/index.md), while XTVERSION is answered by roughly
half of them.

## Probe

```sh
tools/query '\033[>0q\033[c'
```

The reply shows the DCS answer (if any) followed by `CSI ? … c`.

## Sources

- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [CSI queries and reports](../csi/queries.md)
