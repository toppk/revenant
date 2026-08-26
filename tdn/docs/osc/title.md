# Window title

Status: xterm.

OSC 0, 1, and 2 set the text an emulator shows in its window title bar, tab,
or icon. They are the oldest OSCs in wide use and the one most shells emit by
default.

## Syntax

```text
OSC 0 ; Pt ST    set icon name and window title
OSC 1 ; Pt ST    set icon name
OSC 2 ; Pt ST    set window title
```

`Pt` is arbitrary text. Emulators differ on encoding: xterm interprets the
string according to `utf8Title`, while most modern emulators assume UTF-8
unconditionally. Control bytes inside `Pt` are stripped or terminate the
string; do not rely on either.

## Behavior

- The "icon name" is an X11 concept. On other platforms OSC 1 is usually
  ignored or aliased to the tab title.
- Titles are per emulator window or tab, not per PTY. A title set by a
  program inside tmux is applied by tmux to the outer terminal only when
  `set-titles` is `on`, and tmux then formats it with `set-titles-string`.
- Nothing restores the old title when a program exits. Shells that set the
  title on every prompt (bash `PROMPT_COMMAND`, zsh `precmd`) mask this;
  editors that set a title and are killed do not.

### Title stack: XTWINOPS 22 and 23

xterm added a push/pop stack so an application can save and restore the
title around its own changes:

```text
CSI 22 ; 0 t    push icon name and title
CSI 22 ; 1 t    push icon name
CSI 22 ; 2 t    push title
CSI 23 ; 0 t    pop icon name and title
CSI 23 ; 1 t    pop icon name
CSI 23 ; 2 t    pop title
```

The stack is bounded (10 entries in xterm). See
[Window operations](../csi/window-ops.md) for the rest of XTWINOPS.

### Title reports: XTWINOPS 20 and 21

`CSI 20 t` and `CSI 21 t` ask the terminal to report the icon name and title
as `OSC L Pt ST` and `OSC l Pt ST`. xterm disables both by default
(`allowWindowOps`) because the reply arrives as input: a program that first
sets the title to a shell command and then requests it back has typed that
command. Most emulators either do not implement the reports or answer with an
empty title. Do not depend on them.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OSC 0/2 set | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes[^tmux] |
| OSC 1 icon | Yes | Partial[^icon] | ? | Partial[^icon] | ? | ? | Partial[^icon] | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| 22/23 stack | Yes | Yes | ? | ? | Yes | ? | ? | ? | ? | Yes | ? | ? | ? | ? | ? | ? |
| 20/21 report | Partial[^xtreport] | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

[^tmux]: Applied to the outer terminal only with `set-titles on`; otherwise
    stored as the pane title visible in the status line.
[^icon]: Accepted and parsed; no distinct icon name exists on the platform,
    so it is ignored or aliased to the title.
[^xtreport]: Off unless `allowWindowOps` is true; `disallowedWindowOps`
    can keep individual operations disabled.

## Probe

```sh
printf '\033]2;TDN title probe\033\\'
tools/sendosc title 'TDN title probe'
printf '\033[22;2t'; printf '\033]2;pushed\033\\'; sleep 2; printf '\033[23;2t'
```

The third line should end with the title that was current before `pushed`.

## Sources

- [XTerm Control Sequences, OSC](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
- [XTerm Control Sequences, XTWINOPS](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen)
- [xterm manual, allowWindowOps](https://invisible-island.net/xterm/manpage/xterm.html)
- [tmux(1), set-titles](https://man.openbsd.org/tmux#set-titles)
- [WezTerm escape sequences](https://wezterm.org/escape-sequences.html)
- [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs)
