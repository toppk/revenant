# XTWINOPS: window operations

Status: xterm, derived from dtterm; most reports are widely implemented,
most manipulations are not.

```text
CSI Ps ; Ps ; Ps t
```

XTWINOPS covers window manipulation, size reports, and the title stack. It
is the most frequently disabled sequence family because moving, resizing,
and reading titles are attack surface; xterm's `allowWindowOps` resource
defaults to false and gates everything except the report of size in
characters and pixels.

## Operations

<!-- markdownlint-disable MD013 -->

| `Ps` | Operation | Report | Typical policy |
| --- | --- | --- | --- |
| `1` | De-iconify | | Gated |
| `2` | Iconify | | Gated |
| `3 ; x ; y` | Move window | | Gated |
| `4 ; h ; w` | Resize window in pixels | | Gated |
| `5` | Raise | | Gated |
| `6` | Lower | | Gated |
| `7` | Refresh | | Gated |
| `8 ; rows ; cols` | Resize text area in cells | | Gated |
| `9 ; 0/1/2/3` | Restore / maximize / vertical / horizontal | | Gated |
| `10 ; 0/1/2` | Fullscreen off / on / toggle | | Gated |
| `11` | Report window state | `CSI 1 t` open, `CSI 2 t` iconified | Allowed |
| `13` / `13 ; 2` | Report position (window / text area) | `CSI 3 ; x ; y t` | Gated |
| `14` / `14 ; 2` | Report text-area / window size in pixels | `CSI 4 ; h ; w t` | Allowed |
| `15` | Report screen size in pixels | `CSI 5 ; h ; w t` | Allowed |
| `16` | Report cell size in pixels | `CSI 6 ; h ; w t` | Allowed |
| `18` | Report text-area size in cells | `CSI 8 ; rows ; cols t` | Allowed |
| `19` | Report screen size in cells | `CSI 9 ; rows ; cols t` | Allowed |
| `20` | Report icon label | `OSC L label ST` | Disabled by default |
| `21` | Report title | `OSC l title ST` | Disabled by default |
| `22 ; 0/1/2` | Push icon and title / icon / title | | Allowed |
| `23 ; 0/1/2` | Pop | | Allowed |
| `≥ 24` | Resize to `Ps` rows (DECSLPP) | | Gated |

<!-- markdownlint-enable MD013 -->

## Size reports

`CSI 14 t`, `CSI 16 t`, and `CSI 18 t` are the sanctioned way for an
application to learn pixel geometry when `TIOCGWINSZ` returns zero
`ws_xpixel`/`ws_ypixel`, which is common over SSH and under multiplexers.
Image protocols depend on this; see [Graphics](../graphics/index.md).

Reports are asynchronous input; see [Queries](queries.md).

In-band resize notification (DEC mode `?2048`) is a newer extension that
sends `CSI 48 ; rows ; cols ; height ; width t` whenever the size changes,
replacing `SIGWINCH` polling under multiplexers. Ghostty, kitty, foot,
Contour, and WezTerm document it; see [Modes](modes.md).

## Title stack

`22`/`23` were added so applications could set a title and restore whatever
was there without reading it, which `21` allows only when the user has
opted in. The stack depth is small (xterm: 10). See
[OSC titles](../osc/title.md).

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `18` size in cells | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | Yes | Yes |
| `14`/`16` pixel sizes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Yes (3.4+ relays) |
| `22`/`23` title stack | Yes | Yes | ? | ? | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | ? |
| `21` title report | Gated | ? | ? | ? | ? | ? | ? | ? | ? | Gated | ? | ? | ? | ? | ? |
| Move/resize (`3`,`4`,`8`) | Gated | ? | ? | ? | Partial | ? | ? | ? | ? | Gated | ? | ? | Yes | ? | ? |
| `?2048` in-band resize | ? | ? | ? | Yes | Yes | Yes | Yes | ? | Yes | ? | ? | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
tools/query winsize      # CSI 18 t
tools/query cellsize     # CSI 16 t
tools/query pixsize      # CSI 14 t
tools/sendosc push-title; tools/sendosc title probe; sleep 2; tools/sendosc pop-title
```

## Sources

- [XTerm Control Sequences, XTWINOPS](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [XTerm manual, `allowWindowOps`](https://invisible-island.net/xterm/manpage/xterm.html)
- [In-band window resize notifications](https://gist.github.com/rockorager/e695fb2924d36b2bcf1fff4a3704bd83)
- [kitty: in-band resize](https://sw.kovidgoyal.net/kitty/changelog/)
