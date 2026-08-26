# modifyOtherKeys

Status: xterm.

modifyOtherKeys is xterm's mechanism for reporting modifier combinations that
the [legacy encoding](keyboard-legacy.md) cannot express, such as Ctrl-1,
Ctrl-Shift-A, or Alt-Enter. It predates the
[Kitty keyboard protocol](kitty-keyboard.md) and is implemented by several
other emulators, but the two output formats and the interaction with other
xterm resources make it harder to consume than its successor.

## Syntax

```text
CSI > 4 ; Pv m      set modifyOtherKeys to Pv
CSI > 4 m           reset modifyOtherKeys to the resource default
CSI ? 4 m           XTQMODKEYS: query the current value
CSI > 4 ; Pv m      reply to XTQMODKEYS
```

| `Pv` | Behavior |
| --- | --- |
| `0` | Disabled; legacy encoding only |
| `1` | Report modified keys except those with a well-established encoding |
| `2` | Report all modified keys, including Ctrl-letter combinations |

Level 1 leaves Ctrl-C, Ctrl-letter, and similar bytes alone so that shells
and job control keep working. Level 2 is for full-screen applications that
want every combination.

`CSI > 4 m` with no value resets to the `modifyOtherKeys` resource, which is
how xterm's own reset works; sending `CSI > 4 ; 0 m` forces level 0 instead.

## Output formats

The `formatOtherKeys` resource chooses how xterm encodes a modified key:

<!-- markdownlint-disable MD013 -->

| `formatOtherKeys` | Sequence | Example: Ctrl-Shift-A |
| --- | --- | --- |
| `0` (default) | `CSI 27 ; mod ; code ~` | `CSI 27 ; 6 ; 65 ~` |
| `1` | `CSI code ; mod u` | `CSI 65 ; 6 u` |

<!-- markdownlint-enable MD013 -->

`code` is the Unicode code point of the key and `mod` is the xterm modifier
parameter (one plus shift 1, alt 2, ctrl 4, meta 8). Format 1 is the form
the Kitty protocol later adopted, which is why an application that parses
`CSI code ; mod u` covers both. The `27 ~` form was chosen first because
`27` is otherwise unused in the tilde key space.

An application cannot select the format from the wire; it must accept both.

## Related resources

`modifyCursorKeys` and `modifyFunctionKeys` (`CSI > 1 ; Pv m` and
`CSI > 2 ; Pv m`) control how modifiers attach to cursor and function keys.
Their levels change whether the modifier goes in the second parameter
(`CSI 1 ; 5 A`) or in a private prefix. Level 2, the default, produces the
`CSI 1 ; mod X` form documented under
[legacy encoding](keyboard-legacy.md#the-xterm-modifier-parameter).

`modifyKeyboard` (`CSI > 0 ; Pv m`) affects the numeric keypad and is rarely
used.

## Multiplexers

tmux's `extended-keys` option (`on`, `off`, or `always`) controls whether
tmux requests modifyOtherKeys from the outer terminal and forwards the
resulting sequences to the inner application in the format the inner
application asked for. With `extended-keys-format` tmux can choose the
`27 ~` or `u` form. Without it, tmux collapses modified keys back to the
legacy encoding.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Terminal | Support | Notes |
| --- | --- | --- |
| xterm | Yes | Origin; both formats, `XTQMODKEYS` since patch 372[^xterm] |
| VTE | ? | |
| Konsole | ? | |
| kitty | Partial | Accepts `CSI > 4 ; 2 m` as a request for its own protocol's disambiguate flag[^kitty] |
| WezTerm | Yes | `enable_csi_u_key_encoding` and modifyOtherKeys handling[^wezterm] |
| Ghostty | Yes | Documented in its VT reference[^ghostty] |
| foot | Yes | Both levels; format 1 not selectable[^foot] |
| Alacritty | ? | |
| Contour | ? | |
| mintty | Yes | `modifyOtherKeys` documented in the mintty manual[^mintty] |
| PuTTY | ? | |
| Windows Terminal | ? | |
| Apple Terminal | ? | |
| iTerm2 | Yes | Preference "Report modifiers using CSI u"[^iterm2] |
| xterm.js | ? | |
| tmux | Yes | `extended-keys` option[^tmux] |

<!-- markdownlint-enable MD013 -->

[^xterm]: [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html) and the [xterm manual](https://invisible-island.net/xterm/manpage/xterm.html), resources `modifyOtherKeys` and `formatOtherKeys`.
[^kitty]: [kitty keyboard protocol, legacy compatibility](https://sw.kovidgoyal.net/kitty/keyboard-protocol/#legacy-key-event-encoding).
[^wezterm]: [WezTerm configuration reference](https://wezterm.org/config/lua/config/enable_csi_u_key_encoding.html).
[^ghostty]: [Ghostty VT reference](https://ghostty.org/docs/vt).
[^foot]: [foot README, keyboard section](https://codeberg.org/dnkl/foot#keyboard).
[^mintty]: [mintty manual](https://mintty.github.io/mintty.1.html).
[^iterm2]: [iTerm2 documentation](https://iterm2.com/documentation-preferences-profiles-keys.html).
[^tmux]: [tmux(1), `extended-keys`](https://man.openbsd.org/tmux.1#extended-keys).

## Pitfalls

- Level 2 makes Ctrl-C arrive as `CSI 27 ; 5 ; 99 ~` rather than `0x03`, so
  the kernel's `ISIG` handling no longer sees it; the application must handle
  interrupt itself.
- The two formats mean a parser needs both a `~` rule with leading `27` and a
  `u` rule.
- Some emulators answer `CSI > 4 ; 2 m` by enabling a different protocol
  entirely; detect with `XTQMODKEYS` or a
  [Kitty protocol query](kitty-keyboard.md#detection) rather than by assuming.

## Probe

```sh
tools/query '\033[?4m'          # XTQMODKEYS
printf '\033[>4;2m'; cat -v; printf '\033[>4;0m'
```

Press Ctrl-1 or Ctrl-Shift-A inside `cat -v` and look for `^[[27;` or
`^[[49;5u`-style output.

## Sources

- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [XTerm manual](https://invisible-island.net/xterm/manpage/xterm.html)
- [tmux(1)](https://man.openbsd.org/tmux.1)
