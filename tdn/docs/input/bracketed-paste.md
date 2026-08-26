# Bracketed paste

Status: xterm.

Bracketed paste marks the beginning and end of pasted text so that an
application can tell it apart from typing. Without it, a pasted newline
executes a command, a pasted `ESC` starts an escape sequence, and an editor
auto-indents pasted code. It is the single most widely implemented xterm
extension after mouse tracking.

## Syntax

```text
CSI ? 2004 h    enable
CSI 200 ~       start of paste
CSI 201 ~       end of paste
CSI ? 2004 l    disable
```

The pasted bytes arrive between the two markers. Nothing else changes: keys
pressed after the paste continue in whatever encoding was active.

## Behavior

The emulator is responsible for making the markers unforgeable. A robust
emulator removes from the paste anything that could terminate the bracket
or inject a control:

- the end marker `CSI 201 ~` itself;
- `ESC` and the C1 controls, or the whole escape sequence they begin;
- C0 controls other than `HT`, `LF`, and `CR`;
- optionally, `LF`/`CR` normalization to the application's preferred line
  ending.

What is stripped is emulator policy. An application should still treat the
contents as untrusted text: a terminal that strips nothing can be made to
end the bracket early with a crafted clipboard.

Applications enable the mode at startup and disable it on exit. Line editors
use it to insert the paste as a single unit without executing or
auto-completing; editors use it to suspend auto-indent and key bindings.

## Line editor defaults

| Editor | Default |
| --- | --- |
| GNU Readline (bash 5.1+) | `enable-bracketed-paste on` |
| zsh (5.1+) | `bracketed-paste-magic` widget bound by default |
| fish | Enabled |
| libedit | Depends on the build |

A shell with bracketed paste on shows the pasted text and waits for Enter
rather than executing each line.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Terminal | Support | Notes |
| --- | --- | --- |
| xterm | Yes | Origin[^xterm] |
| VTE | Yes | Strips C0 controls and `CSI 201 ~`[^vte] |
| Konsole | Yes | [^konsole] |
| kitty | Yes | Sanitizes controls[^kitty] |
| WezTerm | Yes | [^wezterm] |
| Ghostty | Yes | Configurable sanitization, `clipboard-paste-protection`[^ghostty] |
| foot | Yes | Strips C0 controls[^foot] |
| Alacritty | Yes | [^alacritty] |
| Contour | Yes | [^contour] |
| mintty | Yes | [^mintty] |
| PuTTY | Yes | Enabled by default since 0.63[^putty] |
| Windows Terminal | Yes | [^wt] |
| Apple Terminal | ? | No public protocol document |
| iTerm2 | Yes | [^iterm2] |
| xterm.js | Yes | [^xtermjs] |
| tmux | Yes | Requests it from the outer terminal, forwards markers to the pane[^tmux] |

<!-- markdownlint-enable MD013 -->

[^xterm]: [XTerm Control Sequences, `?2004`](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character_s_).
[^vte]: [gnome/vte](https://gitlab.gnome.org/GNOME/vte), paste handling in `src/vte.cc`.
[^konsole]: [Konsole source](https://invent.kde.org/utilities/konsole).
[^kitty]: [kitty, `paste_actions`](https://sw.kovidgoyal.net/kitty/conf/#opt-kitty.paste_actions).
[^wezterm]: [WezTerm escape sequences](https://wezterm.org/escape-sequences.html).
[^ghostty]: [Ghostty configuration reference](https://ghostty.org/docs/config/reference#clipboard-paste-protection).
[^foot]: [foot README](https://codeberg.org/dnkl/foot).
[^alacritty]: [Alacritty escape sequence support](https://github.com/alacritty/alacritty/blob/master/docs/escape_support.md).
[^contour]: [Contour VT extensions](https://contour-terminal.org/vt-extensions/).
[^mintty]: [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs).
[^putty]: [PuTTY changes, 0.63](https://www.chiark.greenend.org.uk/~sgtatham/putty/changes.html).
[^wt]: [Windows Terminal 1.x release notes](https://github.com/microsoft/terminal/releases).
[^iterm2]: [iTerm2 escape codes](https://iterm2.com/documentation-escape-codes.html).
[^xtermjs]: [xterm.js supported sequences](https://xtermjs.org/docs/api/vtfeatures/).
[^tmux]: [tmux(1)](https://man.openbsd.org/tmux.1).

## Pitfalls

- The end marker can be split across reads like any other sequence; buffer
  until it arrives, but cap the buffer.
- Text pasted by a multiplexer's own paste command may or may not be
  bracketed depending on its `-p` flag.
- Some applications leave `?2004` set after a crash; the shell then sees
  `^[[200~` around every paste until something resets it.

## Probe

```sh
printf '\033[?2004h'; cat -v; printf '\033[?2004l'
```

Paste any text: it appears wrapped in `^[[200~` and `^[[201~`. Paste text
containing an escape character to see what the emulator strips.

## Sources

- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [GNU Readline, `enable-bracketed-paste`](https://tiswww.case.edu/php/chet/readline/readline.html)
