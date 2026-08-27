# Shell integration

Status: Extension (FinalTerm origin for OSC 133; iTerm2 for OSC 1337).

Shell integration is a set of marks the shell emits around each prompt,
command, and output block. With them the emulator can jump between prompts,
select a command's output, redraw the prompt on resize, and show exit status
in the scrollbar. Two families exist and most shells emit both.

## Syntax

### OSC 133 semantic prompts

```text
OSC 133 ; A ST            prompt start (before PS1)
OSC 133 ; B ST            command start (after PS1, user input follows)
OSC 133 ; C ST            command executed (output follows)
OSC 133 ; D ; exit ST     command finished with exit status
```

FinalTerm defined `A`–`D`; the `D` exit parameter is optional. Later
implementations added `;`-separated `key=value` parameters after the letter:

| Parameter | On | Meaning |
| --- | --- | --- |
| `aid=ID` | A, B, C, D | Application identifier linking marks from one shell across nested shells |
| `cl=line` | A | Which prompt line follows: `line` (default), `m` first of many, `n` continuation |
| `k=kind` | A | Prompt kind: `i` initial, `r` right, `c` continuation, `s` secondary |
| `redraw=0` | A | The shell will not redraw its prompt; the emulator should not trigger one |
| `redraw=1` | A | The shell will redraw its prompt after resize; the emulator may erase the marked prompt first |
| `err=text` | D | Human-readable error description |

Parameter support varies; VTE's specification (the "semantic prompt"
document maintained in the terminal-wg repository) is the most complete
statement. Emulators ignore unknown parameters.

A `P` command for prompt-line hints (`OSC 133 ; P ; k=kind ST`) appears in
some shells and is documented by iTerm2 and WezTerm.

### OSC 1337 iTerm2 integration

```text
OSC 1337 ; ShellIntegrationVersion=N ; shell=bash ST
OSC 1337 ; RemoteHost=user@host ST
OSC 1337 ; CurrentDir=/path ST
OSC 1337 ; SetUserVar=name=base64value ST
OSC 1337 ; ClearScrollback ST
OSC 1337 ; SetMark ST
```

`SetUserVar` exposes arbitrary key-value data to the emulator's status bar
or scripting API. `RemoteHost` and `CurrentDir` feed the same
directory-tracking as [OSC 7](cwd.md).

## Behavior

Emulators use the marks for:

- **Prompt navigation**: jump to the previous or next `A` mark.
- **Output selection**: select from `C` to the next `A`.
- **Prompt redraw**: on resize, erase from the last `A` and let the shell
  redraw it, instead of reflowing a stale prompt (kitty, foot, Ghostty).
- **Command status**: color the scrollbar or a gutter by the `D` exit code
  (VTE-based terminals, Windows Terminal, iTerm2, WezTerm).
- **Click to move cursor**: with `B`–`C` known, a click on the command line
  can be translated into cursor-movement keys (kitty, Ghostty, foot).
- **Bell on completion** and unread-output indicators.

The marks are cell attributes or line metadata. They survive scrollback but
not `ED 3`. A shell inside tmux still emits them; tmux forwards nothing by
default and has no native support, so the outer terminal sees no marks.

### How shells get the marks

Emulators either ship shell scripts to source or inject them at startup:

| Emulator | Mechanism |
| --- | --- |
| kitty | `shell_integration` option; injected by setting `ENV`/`ZDOTDIR`/`XDG_DATA_DIRS` at launch; also emits OSC 7 |
| Ghostty | `shell-integration` option with the same injection strategy; scripts under `shell-integration/` in the resources dir |
| WezTerm | `wezterm.sh` sourced by the user; emits OSC 133, OSC 7, OSC 1337 |
| foot | Scripts documented in the README, sourced by the user |
| iTerm2 | Downloaded `iterm2_shell_integration.<shell>`, sourced by the user; emits OSC 133 and OSC 1337 |
| VTE | The distribution's `vte.sh` / `vte-profile` emits OSC 7 and, in newer VTE, OSC 133 |
| Windows Terminal | User adds `PROMPT_COMMAND` snippets from the documentation; `experimental.autoMarkPrompts` |
| Konsole | User adds snippets from the Konsole handbook |
| fish | Emits OSC 133 natively since 3.x when it detects a supporting terminal |

Nested shells, `ssh`, and `sudo -s` generally lose integration unless the
inner shell is set up too.

### Ghostty application and embedding defaults

Prompt redraw is partly an integration policy, not just parser support.
Ghostty's native terminal state defaults to assuming a marked prompt can be
redrawn. The `libghostty-vt` C constructor deliberately changes that default
to false because an embedding application may not install Ghostty's shell
integration. A marked prompt can opt the embedded session back in with
`OSC 133 ; A ; redraw=1 ST`; `redraw=0` opts out.

Consequently, a terminal embedding libghostty-vt can behave differently from
the Ghostty application during resize even when both use the same terminal
parser and reflow implementation. This is a Ghostty implementation detail,
not a requirement of OSC 133.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OSC 133 A–D | ? | Yes | Yes (22.04) | Yes | Yes | Yes | Yes | ? | ? | ? | ? | Yes (1.18) | ? | Yes | ? | ? |
| `aid=` / `cl=` | ? | Yes | ? | Partial | ? | Partial | Partial | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| Prompt redraw on resize | ? | ? | ? | Yes | ? | Yes | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| OSC 1337 SetUserVar | ? | ? | ? | ? | Yes | ? | ? | ? | ? | ? | ? | ? | ? | Yes | ? | ? |
| Ships its own shell scripts | ? | Yes | ? | Yes | Yes | Yes | Yes | ? | ? | ? | ? | ? | ? | Yes | ? | ? |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
printf '\033]133;A\033\\'; printf 'probe$ '; printf '\033]133;B\033\\'
printf 'true\n'; printf '\033]133;C\033\\'
printf '\033]133;D;0\033\\'
printf '\033]1337;SetUserVar=%s=%s\033\\' tdn "$(printf probe | base64)"
```

Then use the emulator's "previous prompt" binding (kitty `ctrl+shift+z`,
Ghostty and WezTerm via configurable actions) to confirm the mark landed.

## Sources

- [FinalTerm semantic prompts, as documented by iTerm2](https://iterm2.com/documentation-shell-integration.html)
- [Semantic prompt specification (terminal-wg / VTE)](https://gitlab.freedesktop.org/Per_Bothner/specifications/blob/master/proposals/semantic-prompts.md)
- [iTerm2 proprietary escape codes](https://iterm2.com/documentation-escape-codes.html)
- [kitty: shell integration](https://sw.kovidgoyal.net/kitty/shell-integration/)
- [Ghostty: shell integration](https://ghostty.org/docs/features/shell-integration)
- [Ghostty terminal prompt-redraw default](https://github.com/ghostty-org/ghostty/blob/f64f4aca2c29b554d111b36c3d946a9bddd159ff/src/terminal/Terminal.zig#L99)
- [libghostty-vt embedding default](https://github.com/ghostty-org/ghostty/blob/f64f4aca2c29b554d111b36c3d946a9bddd159ff/src/terminal/c/terminal.zig#L673)
- [WezTerm: shell integration](https://wezterm.org/shell-integration.html)
- [foot README: shell integration](https://codeberg.org/dnkl/foot#shell-integration)
- [Windows Terminal: shell integration](https://learn.microsoft.com/en-us/windows/terminal/tutorials/shell-integration)
- [Konsole handbook: semantic integration](https://docs.kde.org/stable5/en/konsole/konsole/)
- [fish shell: prompt marks](https://fishshell.com/docs/current/index.html)
