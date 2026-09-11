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

<!-- tdn:compatibility -->
