# Environment and detection

Status: Convention. Nothing on this page is specified by a standard; all of
it is what programs actually do.

An application learns about its terminal from four sources, in decreasing
order of reliability: the terminfo database, direct queries, environment
variables, and guesswork.

## `TERM` and terminfo

`TERM` names a terminfo entry. `ncurses` reads the entry and exposes
capabilities such as `colors`, `cup`, `smkx`, `Tc`, `Smulx` through
`tput(1)`, `infocmp(1)`, and the curses API.

<!-- markdownlint-disable MD013 -->

| `TERM` | Set by | Notes |
| --- | --- | --- |
| `xterm-256color` | xterm, VTE, Konsole, WezTerm, iTerm2, Apple Terminal, xterm.js hosts, WSL | The lowest common denominator; describes xterm, not the emulator |
| `xterm` | PuTTY, mintty, many SSH defaults | 8 colors in the database, although the emulator does more |
| `xterm-kitty` | kitty | Ships with kitty; missing on remote hosts unless copied |
| `xterm-ghostty` | Ghostty | Same |
| `foot`, `foot-extra` | foot | Same |
| `alacritty` | Alacritty | Same |
| `wezterm` | WezTerm (optional) | Default remains `xterm-256color` |
| `contour` | Contour | Same |
| `rxvt-unicode-256color` | rxvt-unicode | |
| `st-256color` | st | |
| `tmux-256color`, `screen-256color` | tmux, screen | Describe the multiplexer, not the outer terminal |
| `linux` | Linux console | |
| `dumb` | pipes, some CI | No cursor addressing |

<!-- markdownlint-enable MD013 -->

The recurring failure is a terminfo entry that is absent on a remote host,
which produces `tput: unknown terminal` and broken `clear`, `less`, and
`vim`. Emulators with custom `TERM` values document how to copy the entry
(`infocmp -x | ssh host tic -x -`) or fall back to `xterm-256color`.

`TERM` cannot identify the emulator. `xterm-256color` is claimed by a dozen
programs with different feature sets.

## Environment variables

<!-- markdownlint-disable MD013 -->

| Variable | Set by | Values |
| --- | --- | --- |
| `COLORTERM` | Most emulators | `truecolor` or `24bit`; the only widely honored truecolor signal; not propagated by SSH by default |
| `TERM_PROGRAM` | Apple Terminal (`Apple_Terminal`), iTerm2 (`iTerm.app`), WezTerm, Ghostty, VS Code (`vscode`), mintty, tmux (`tmux`), Hyper, Tabby, Warp (`WarpTerminal`) | Emulator identity; unset by xterm, VTE, kitty, Alacritty, foot, Windows Terminal |
| `TERM_PROGRAM_VERSION` | Same | Free-form version string |
| `WT_SESSION`, `WT_PROFILE_ID` | Windows Terminal | Session GUID; the practical Windows Terminal detector |
| `KITTY_WINDOW_ID`, `KITTY_PID` | kitty | |
| `GHOSTTY_RESOURCES_DIR` | Ghostty | |
| `WEZTERM_PANE`, `WEZTERM_EXECUTABLE` | WezTerm | |
| `ALACRITTY_WINDOW_ID` | Alacritty | |
| `KONSOLE_VERSION`, `KONSOLE_DBUS_SESSION` | Konsole | |
| `VTE_VERSION` | VTE terminals | Decimal `MMmmpp`; note it identifies the library, not the front end |
| `TERM_SESSION_ID` | Apple Terminal, iTerm2 | |
| `ITERM_SESSION_ID`, `LC_TERMINAL` | iTerm2 | `LC_TERMINAL` survives SSH when the client forwards `LC_*` |
| `TMUX`, `TMUX_PANE` | tmux | Presence means a multiplexer sits between the application and the emulator |
| `STY` | screen | Same |
| `ZELLIJ` | zellij | Same |
| `SSH_TTY`, `SSH_CONNECTION` | sshd | The emulator is remote; environment variables describe the client only if forwarded |
| `NO_COLOR`, `FORCE_COLOR`, `CLICOLOR` | Users, CI | Application-level conventions; see no-color.org |
| `LANG`, `LC_ALL`, `LC_CTYPE` | Login | Determine whether the application encodes UTF-8; see [Encoding](text/encoding.md) |

<!-- markdownlint-enable MD013 -->

Variables are inherited, not live. They describe the terminal that started
the shell, which is wrong after `ssh`, `tmux attach` from a different
emulator, or `su`. Treat them as hints and confirm with queries.

## Queries

<!-- markdownlint-disable MD013 -->

| Question | Query | Page |
| --- | --- | --- |
| Is a terminal there at all? | DA1 `CSI c` | [Queries](csi/queries.md) |
| Which emulator and version? | XTVERSION `CSI > 0 q` | [XTVERSION](dcs/xtversion.md) |
| Is mode N supported / set? | DECRQM `CSI ? N $ p` | [Modes](csi/modes.md) |
| What does terminfo say, without a database? | XTGETTCAP `DCS + q … ST` | [XTGETTCAP](dcs/xtgettcap.md) |
| Kitty keyboard? | `CSI ? u` | [Kitty keyboard](input/kitty-keyboard.md) |
| Sixel? | DA1 feature `4`; XTSMGRAPHICS | [Sixel](graphics/sixel.md) |
| Kitty graphics? | `APC G i=31,s=1,v=1,a=q,t=d,f=24 ; AAAA ST` | [Kitty graphics](graphics/kitty.md) |
| Background dark or light? | `OSC 11 ; ? ST` | [Colors](osc/colors.md) |
| Cell size in pixels? | `CSI 16 t` | [Window operations](csi/window-ops.md) |
| Current cursor column (width test)? | `CSI 6 n` | [Width](text/width.md) |

<!-- markdownlint-enable MD013 -->

Send optional queries first and DA1 last in one write; DA1's reply marks the
end. Always use a timeout; never block. Under tmux the answers describe tmux.

## Guesswork

Applications that cannot query (non-interactive output, logging libraries)
fall back to heuristics: `COLORTERM` for truecolor, `TERM` containing
`256color`, `TERM_PROGRAM` for hyperlinks and images, `NO_COLOR` to disable
everything. Libraries such as `supports-color` and Python's `rich` encode
these heuristics; they are wrong in the same predictable ways (SSH loses
`COLORTERM`, tmux hides the outer terminal).

## Probe

```sh
env | grep -E '^(TERM|COLORTERM|TERM_PROGRAM|TERM_PROGRAM_VERSION|WT_SESSION|KITTY_|GHOSTTY|WEZTERM|VTE_VERSION|TMUX|STY|ZELLIJ|SSH_TTY)'
infocmp -x1 | grep -E '(colors|Tc|RGB|Smulx|Setulc|Sync|Ss|Se|E3|bce)'
tools/query version da1
```

## Sources

- [terminfo(5)](https://invisible-island.net/ncurses/man/terminfo.5.html)
- [ncurses terminfo source](https://invisible-island.net/ncurses/terminfo.src.html)
- [termstandard/colors: `COLORTERM`](https://github.com/termstandard/colors)
- [no-color.org](https://no-color.org/)
