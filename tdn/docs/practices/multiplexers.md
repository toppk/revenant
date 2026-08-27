# Multiplexers

Status: Convention.

tmux and screen own the PTY that the application sees. They parse every
sequence themselves, act on the ones they understand, re-emit what they
choose to the outer terminal, and drop the rest. An application that
works in a bare emulator and fails under tmux has usually hit one of
three cases: a sequence the multiplexer does not know, a query the
multiplexer answers on the outer terminal's behalf, or a terminator the
multiplexer's passthrough grammar cannot carry.

## What a multiplexer does natively

tmux understands and forwards, without any wrapping:

- window titles (`OSC 0`/`2`, subject to `set-titles`);
- `OSC 52` clipboard writes (subject to `set-clipboard`);
- `OSC 8` hyperlinks (tmux 3.4+);
- SGR including truecolor and underline styles when the outer `TERM`
  advertises them (`terminal-features`, `terminal-overrides`);
- the DEC private modes it tracks per pane (alternate screen, cursor
  visibility, mouse, bracketed paste, focus events);
- DECSCUSR (cursor style), `CSI ? 2026` synchronized output (tmux 3.4+).

Queries are answered by tmux, not the outer terminal: DA1, DA2, DSR,
DECRQM, and XTVERSION all report tmux's own identity and its per-pane
mode state. An application cannot see through tmux to the real emulator
unless it uses passthrough and parses the reply itself.

screen is more conservative: titles, alternate screen, and basic modes
are handled; most modern OSCs are dropped.

## Passthrough

Anything the multiplexer does not understand is dropped unless it is
wrapped in a DCS the multiplexer recognises as "send this on verbatim":

```text
DCS tmux ; <payload with every ESC doubled> ST     tmux
DCS <payload> ST                                   screen
```

```sh
# OSC 8 hyperlink through tmux
printf '\033Ptmux;\033\033]8;;https://example.com\033\033\\\033\\'
```

Rules that follow from the grammar:

- every `ESC` in the payload is doubled for tmux, so the multiplexer's
  own DCS parser does not see the payload's `ST` early;
- the payload's own string must end in `ST`, never `BEL`, because `BEL`
  cannot terminate a DCS; an [OSC](../osc/anatomy.md#terminators) sent
  this way must use `ESC \`;
- tmux forwards the payload only when `allow-passthrough` is `on` or
  `all` (tmux 3.3+; earlier versions always forwarded). `on` forwards from
  visible panes; `all` also from invisible ones;
- screen has a payload length limit (historically 768 bytes) and splits
  are not reassembled, so large payloads (images, OSC 52) need chunking
  by the protocol itself;
- a reply to a query sent through passthrough comes back from the outer
  terminal as raw input to the pane, so the application must be reading
  for it.

Nested multiplexers require nested wrapping: doubling the ESCs of an
already-wrapped payload and adding another `DCS tmux ;` layer per hop.
Detecting the depth is guesswork; `TMUX` and `STY` in the environment
show at most one level.

## Detecting a multiplexer

- `TERM` is `screen`, `screen-256color`, `tmux`, or `tmux-256color`;
- `TMUX` is set (tmux, current session only) or `STY` (screen);
- DA2 reports tmux's identity (`CSI > 84 ; 0 ; 0 c`, Pp=84 meaning `T`);
- XTVERSION replies `tmux <version>`.

None of these reveal the outer terminal. `tmux display -p
'#{client_termname}'` does, from inside a tmux session, and is the only
reliable way.

## Sources

- [tmux wiki: FAQ, passthrough](https://github.com/tmux/tmux/wiki/FAQ#what-is-the-passthrough-escape-sequence-and-how-do-i-use-it)
- [tmux(1), allow-passthrough](https://man.openbsd.org/tmux#allow-passthrough)
- [screen(1), Control sequences](https://www.gnu.org/software/screen/manual/screen.html#Control-Sequences)
- [tmux page](../terminals/tmux.md)
