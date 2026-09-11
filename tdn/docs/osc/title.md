---
tags:
  - Window Ops
  - Title Ops
---

# Window title

Status: xterm.

OSC 0, 1, and 2 set the text an emulator shows in its window title bar, tab,
or icon. They are the oldest OSCs in wide use and the one most shells emit by
default.

**Title Ops:** OSC 0/1/2 label changes and applying saved labels on pop use
[xterm's Title Ops policy](../policies/title-ops.md).
**Window Ops:** reports and stack operations use the separate
[Window Ops policy](../policies/window-ops.md). A pop involves both checks.

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

`PushTitle` and `PopTitle` govern the stack operations. Applying labels after
a permitted pop also requires Title Ops: with `allowTitleOps` false, a normal
pop consumes its entry but leaves the visible labels unchanged.

### Title reports: XTWINOPS 20 and 21

`CSI 20 t` and `CSI 21 t` ask the terminal to report the icon name and title
as `OSC L Pt ST` and `OSC l Pt ST`. xterm disables both by default
(`allowWindowOps`) because the reply arrives as input: a program that first
sets the title to a shell command and then requests it back has typed that
command. Most emulators either do not implement the reports or answer with an
empty title. Do not depend on them.

### Title encoding modes

```text
CSI > Pm t    XTSMTITLE: enable title encoding features
CSI > Pm T    XTRMTITLE: reset title encoding features
```

Each parameter selects one feature. The `titleModes` resource supplies a
bitmask, with bit `n` corresponding to parameter `n` below.

| Parameter | Feature |
| --- | --- |
| `0` | Set window/icon labels using hexadecimal |
| `1` | Report window/icon labels using hexadecimal |
| `2` | Set window/icon labels using UTF-8 |
| `3` | Report window/icon labels using UTF-8 |

Without parameters, both sequences restore the compiled-in title-mode defaults.
These settings select encoding; they do not grant permission to change or
report labels. `utf8Title` also enables UTF-8 title input and EWMH label
properties. See [Title Ops policy](../policies/title-ops.md) for the distinction.


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


## Separately tracked behavior

### Hexadecimal title input

Feature ID: `title-modes-hex-input`. titleModes/XTSMTITLE hexadecimal input decoding.

### Hexadecimal title reports

Feature ID: `title-modes-hex-reports`. titleModes hexadecimal report encoding under the report permission.

### UTF-8 title input mode

Feature ID: `title-modes-utf8-input`. titleModes UTF-8 input decoding and reset semantics.

### UTF-8 title report mode

Feature ID: `title-modes-utf8-reports`. titleModes UTF-8 report encoding under the report permission.

### Title encoding configuration

Feature ID: `resource-title-modes`. titleModes resource default and XTSMTITLE/XTRMTITLE state changes.

### UTF-8 Titles setting

Feature ID: `resource-utf8-title`. utf8Title resource and utf8-title menu/action with locale and allowC1Printable interaction.

### ICCCM and EWMH title properties

Feature ID: `x11-utf8-title-properties`. Synchronize WM_NAME/WM_ICON_NAME and UTF-8 EWMH labels; delete stale properties when encoding changes.

### Title input normalization

Feature ID: `title-input-normalization`. Normalize title control characters and validate hex input like xterm ChangeGroup.

### Title input length limits

Feature ID: `title-input-byte-limit`. Reject overlong title input before decoding, rather than truncating it.

### Suppress redundant title updates

Feature ID: `resource-same-name`. sameName avoids redundant title/icon property changes.

<!-- tdn:compatibility -->
