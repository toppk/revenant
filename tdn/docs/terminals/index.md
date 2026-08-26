# Terminals

This section is the TDN analogue of a browser list. It names the emulators
that compatibility tables refer to, records how each identifies itself, and
explains which of them share an engine and therefore share behavior.

Column order in compatibility tables across TDN follows
[the conventions page](../conventions.md). The table below lists more
terminals than that fixed set; the extras appear in tables only when a page
has something specific to say about them.

## Landscape

<!-- markdownlint-disable MD013 -->

| Terminal | Maintainer | Platforms | Engine | Default `TERM` | `TERM_PROGRAM` | Version | Docs | Source |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| [xterm](xterm.md) | Thomas Dickey | X11 | own | `xterm-256color` (build-dependent) | — | `xterm -v`; XTVERSION | [ctlseqs](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html) | [invisible-island.net/xterm](https://invisible-island.net/xterm/) |
| [VTE](vte.md): GNOME Terminal, Ptyxis, Tilix, xfce4-terminal, Terminator, Black Box | GNOME | Linux, BSD | libvte | `xterm-256color` | — | host app; XTVERSION | [VTE docs](https://gnome.pages.gitlab.gnome.org/vte/) | [gitlab.gnome.org/GNOME/vte](https://gitlab.gnome.org/GNOME/vte) |
| [Konsole](konsole.md) | KDE | Linux, BSD | own | `xterm-256color` | — | `konsole --version`; XTVERSION `?` | [docs.kde.org](https://docs.kde.org/stable5/en/konsole/konsole/) | [invent.kde.org/utilities/konsole](https://invent.kde.org/utilities/konsole) |
| [kitty](kitty.md) | Kovid Goyal | Linux, macOS, BSD | own | `xterm-kitty` | — | `kitty --version`; XTVERSION | [sw.kovidgoyal.net/kitty](https://sw.kovidgoyal.net/kitty/) | [github.com/kovidgoyal/kitty](https://github.com/kovidgoyal/kitty) |
| [WezTerm](wezterm.md) | Wez Furlong | Linux, macOS, Windows, BSD | own (wezterm-term) | `xterm-256color` | `WezTerm` | `wezterm --version`; XTVERSION | [wezterm.org](https://wezterm.org/escape-sequences.html) | [github.com/wezterm/wezterm](https://github.com/wezterm/wezterm) |
| [Ghostty](ghostty.md) | Mitchell Hashimoto | Linux, macOS | libghostty | `xterm-ghostty` | `ghostty` | `ghostty --version`; XTVERSION | [ghostty.org/docs](https://ghostty.org/docs) | [github.com/ghostty-org/ghostty](https://github.com/ghostty-org/ghostty) |
| [foot](foot.md) | Daniel Eklöf | Wayland | own | `foot` | — | `foot --version`; XTVERSION | [foot-ctlseqs(7)](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd) | [codeberg.org/dnkl/foot](https://codeberg.org/dnkl/foot) |
| [Alacritty](alacritty.md) | Alacritty project | Linux, macOS, Windows, BSD | own (alacritty_terminal) | `alacritty` | — | `alacritty --version`; XTVERSION `?` | [alacritty.org](https://alacritty.org/config-alacritty.html) | [github.com/alacritty/alacritty](https://github.com/alacritty/alacritty) |
| [Contour](contour.md) | Christian Parpart | Linux, macOS, Windows, BSD | libvtbackend | `contour` | `contour` | `contour version`; XTVERSION | [contour-terminal.org](https://contour-terminal.org/) | [github.com/contour-terminal/contour](https://github.com/contour-terminal/contour) |
| [rxvt-unicode](urxvt.md) | Marc Lehmann | X11 | own (rxvt lineage) | `rxvt-unicode-256color` | — | `urxvt -help` header | [urxvt(7)](http://pod.tst.eu/http://cvs.schmorp.de/rxvt-unicode/doc/rxvt.7.pod) | [software.schmorp.de/pkg/rxvt-unicode](http://software.schmorp.de/pkg/rxvt-unicode.html) |
| [st](st.md) | suckless | X11 | own | `st-256color` | — | `st -v` | [st.suckless.org](https://st.suckless.org/) | [git.suckless.org/st](https://git.suckless.org/st/) |
| [mintty](mintty.md) | Thomas Wolff | Windows (Cygwin, MSYS2, WSL) | own (PuTTY lineage) | `xterm` | `mintty` | `mintty --version`; XTVERSION | [mintty.github.io](https://mintty.github.io/mintty.1.html) | [github.com/mintty/mintty](https://github.com/mintty/mintty) |
| [PuTTY](putty.md) | Simon Tatham et al. | Windows, Unix | own | `xterm` (configurable) | — | About dialog; `putty --version` `?` | [PuTTY manual](https://the.earth.li/~sgtatham/putty/latest/htmldoc/) | [git.tartarus.org/simon/putty.git](https://git.tartarus.org/?p=simon/putty.git) |
| [Windows Terminal](windows-terminal.md) (and conhost/ConPTY) | Microsoft | Windows | ConPTY + own renderer | none set by the terminal | — | `wt -v`; About | [learn.microsoft.com](https://learn.microsoft.com/windows/terminal/) | [github.com/microsoft/terminal](https://github.com/microsoft/terminal) |
| [Apple Terminal](apple-terminal.md) | Apple | macOS | own | `xterm-256color` | `Apple_Terminal` | `TERM_PROGRAM_VERSION` | [Terminal User Guide](https://support.apple.com/guide/terminal/welcome/mac) | closed |
| [iTerm2](iterm2.md) | George Nachman | macOS | own | `xterm-256color` | `iTerm.app` | `TERM_PROGRAM_VERSION`; XTVERSION `?` | [iterm2.com/documentation](https://iterm2.com/documentation.html) | [github.com/gnachman/iTerm2](https://github.com/gnachman/iTerm2) |
| [xterm.js](xtermjs.md): VS Code, Hyper, Tabby, ttyd, JupyterLab, Theia | xterm.js project | browser, Electron | xterm.js core | host decides | host decides (`vscode` sets `TERM_PROGRAM=vscode`) | package version | [xtermjs.org](https://xtermjs.org/) | [github.com/xtermjs/xterm.js](https://github.com/xtermjs/xterm.js) |
| Rio | Raphael Amorim | Linux, macOS, Windows | own (Rust) | `rio` | — | `rio --version` | [raphamorim.io/rio](https://raphamorim.io/rio/) | [github.com/raphamorim/rio](https://github.com/raphamorim/rio) |
| Warp | Warp | macOS, Linux, Windows | own | `xterm-256color` | `WarpTerminal` | About | [docs.warp.dev](https://docs.warp.dev/) | closed core |
| Terminology | Enlightenment | Linux, BSD | own (EFL) | `xterm-256color` | — | `terminology --version` | [enlightenment.org](https://www.enlightenment.org/about-terminology) | [git.enlightenment.org](https://git.enlightenment.org/enlightenment/terminology) |
| Termux | Termux | Android | own (Java) | `xterm-256color` | — | app version | [wiki.termux.com](https://wiki.termux.com/) | [github.com/termux/termux-app](https://github.com/termux/termux-app) |
| mlterm | Araki Ken | X11, Wayland, others | own | `xterm` | — | `mlterm --version` | [mlterm.sourceforge.net](https://mlterm.sourceforge.net/) | [github.com/arakiken/mlterm](https://github.com/arakiken/mlterm) |
| Emacs vterm / eat | community | Emacs | libvterm / own (Elisp) | `xterm-256color` / `eat-truecolor` | — | package version | [emacs-libvterm](https://github.com/akermu/emacs-libvterm), [eat](https://codeberg.org/akib/emacs-eat) | same |
| [tmux](tmux.md) | Nicholas Marriott | Unix | own (multiplexer) | `tmux-256color` or `screen-256color` | — | `tmux -V` | [tmux(1)](https://man.openbsd.org/tmux) | [github.com/tmux/tmux](https://github.com/tmux/tmux) |
| GNU screen | GNU | Unix | own (multiplexer) | `screen` / `screen-256color` | — | `screen -v` | [gnu.org/software/screen](https://www.gnu.org/software/screen/manual/) | [git.savannah.gnu.org/screen](https://git.savannah.gnu.org/cgit/screen.git) |
| zellij | zellij project | Unix | own (multiplexer) | inherits the outer `TERM` | — | `zellij --version` | [zellij.dev](https://zellij.dev/documentation/) | [github.com/zellij-org/zellij](https://github.com/zellij-org/zellij) |

<!-- markdownlint-enable MD013 -->

"Default `TERM`" is what a fresh session sees before shell startup files run.
Distributions, hosts, and users change it; treat the column as a hint about
which terminfo entry the emulator's authors expect, not as a detection method.

## Families and shared engines

**VTE.** GNOME Terminal, Ptyxis, Tilix, xfce4-terminal, Terminator, Black
Box, and many smaller programs embed libvte. The escape-sequence parser,
mode table, and reports all live in the library, so a compatibility claim
about "GNOME Terminal 3.52" is really a claim about the VTE version it links
against. Host applications differ in menus, profiles, and which security
knobs they expose, not in wire behavior. Use `vte-2.91` package versions or
XTVERSION to identify the engine.

**xterm.js.** VS Code's integrated terminal, Hyper, Tabby, ttyd, JupyterLab,
and Theia render through xterm.js. The core parser and screen model are
shared; graphics, clipboard, and Unicode-width behavior arrive as addons that
each host chooses to load or not. A host also owns the PTY and therefore
`TERM`, `COLORTERM`, and `TERM_PROGRAM`. Two hosts on the same xterm.js
version can differ in every one of those.

**ConPTY.** On Windows every terminal that runs console programs talks to
them through the console subsystem. ConPTY is a pseudo-console that sits
between the terminal and the application: the application's console API
calls and VT output are turned into a rendered screen state, and ConPTY
re-emits that state as a *new* VT stream to the terminal. Sequences the
console host does not model are dropped or rewritten, cursor and attribute
state may be re-serialized differently from what the application wrote, and
input is re-encoded on the way in. Windows Terminal, VS Code on Windows,
mintty when hosting native programs, and WezTerm on Windows all inherit these
transformations. See [Windows Terminal](windows-terminal.md).

**PuTTY lineage.** mintty began as a fork of PuTTY's terminal core and has
since diverged extensively; the two should be treated as separate terminals.

**Multiplexers.** tmux, screen, and zellij are emulators toward the programs
inside them and applications toward the terminal outside. Each keeps its own
screen model, so a feature must be supported *by the multiplexer* to reach an
inner program, and *by the outer terminal* for the multiplexer to render it.
Passthrough mechanisms exist for opaque payloads but not for state the
multiplexer must track, which is why graphics, keyboard protocols, and
synchronized output are the usual casualties. See [tmux](tmux.md).

## Choosing baseline behavior

The practical lowest common denominator is the behavior implied by the
`xterm-256color` terminfo entry: 7-bit controls, ECMA-48 cursor and erase
commands, SGR with 16 and 256 colors, DEC private modes 1, 7, 25, 47/1049,
and the xterm mouse and bracketed-paste modes. Every terminal in the table
above implements that set, including Apple Terminal, PuTTY, and ConPTY-hosted
terminals, with the caveats noted on each page.

Beyond that baseline, ask rather than assume. These probes are safe on every
listed terminal because every one of them either answers or ignores the query
without visible side effects:

- **DA1** (`CSI c`): universally answered; the reply identifies a DEC model
  class and some feature flags. See [CSI queries](../csi/queries.md).
- **DECRQM** (`CSI ? Ps $ p`): the correct way to learn whether a mode is
  supported and its current value; unsupported queries are silently ignored
  by older terminals, so use a timeout.
- **XTVERSION** (`CSI > 0 q`): identifies emulator and version when answered;
  see [XTVERSION](../dcs/xtversion.md).
- **XTGETTCAP** (`DCS + q … ST`): asks for terminfo capabilities from the
  terminal itself; see [XTGETTCAP](../dcs/xtgettcap.md).
- **Kitty keyboard query** (`CSI ? u`): answered only by terminals that
  implement the protocol; see [Keyboard](../input/keyboard-legacy.md).

Sequence a probe run so that a terminal answering only DA1 still yields a
usable result: send the optional queries first and DA1 last, then read until
the DA1 reply arrives or a timeout elapses. Everything before the DA1 reply is
attributable to the earlier queries.

These are *not* safe to assume from `TERM` alone: truecolor, underline
styles, OSC 8, OSC 52, any graphics protocol, the Kitty keyboard protocol,
synchronized output, and grapheme clustering. Each has a page with the
detection method that works.
