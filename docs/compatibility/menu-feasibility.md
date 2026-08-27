# Popup-menu feasibility study

Every entry from xterm patch-410's `mainMenu` (Ctrl-Button1), `vtMenu`
(Ctrl-Button2) and `fontMenu` (Ctrl-Button3): what xterm does behind it,
how much of that lives in libghostty-vt, and how approachable it is for
xterm+. Reviewed against the pinned libghostty headers on 2026-08-25.

Status key: **DONE** implemented; **READY** can be wired now with no new
subsystem; **ROADMAP** waits on a planned
[roadmap](../maintainers/roadmap.md) slice; **BLOCKED** needs a libghostty
change; **SKIP** recommend not planning (record in the
[xterm differences ledger](drift.md)).

Totals over 85 entries: 23 done, 21 ready, 17 roadmap, 13 blocked, 11 skip.

## The big pattern

About a third of the VT and main menus are DEC private modes. libghostty
exposes all of them through `ghostty_terminal_set(GHOSTTY_TERMINAL_OPT_MODE)`
and `ghostty_terminal_get(GHOSTTY_TERMINAL_DATA_MODE)`, and the key encoder
re-reads them via `ghostty_key_encoder_setopt_from_terminal`. The generic
`XtpTerminalSetMode`/`XtpTerminalGetMode` pair in `src/terminal.h` now backs
autowrap, reversewrap, autolinefeed, appcursor, appkeypad, backarrow key,
num-lock and alt-esc, with checkmark state read back from `DATA_MODE`.
Meta-esc still needs a distinct X Meta/Alt policy and allow132 needs grid-size
polling after terminal input. Application cursor blinking and its presentation
timer and four-value `cursorBlink` resource policy are implemented; the menu
toggle still needs defined transitions between configured and forced policy.

## mainMenu

<!-- markdownlint-disable MD013 -->

| Item | What xterm does | libghostty involvement | Approachability | Status |
| --- | --- | --- | --- | --- |
| toolbar | Xaw toolbar (`OPT_TOOLBAR`, off in most builds) | None | Not worth reproducing | SKIP |
| fullscreen | Toggle `_NET_WM_STATE_FULLSCREEN` | None | One EWMH client message | READY |
| securekbd | `XGrabKeyboard`+`XGrabServer`, reverse-video flash | None | Pure Xlib, low value | READY |
| allowsends | Accept `send_event` X events | None | Check `xany.send_event` in handlers | READY |
| redraw | Full repaint | — | — | DONE |
| logging | Tee PTY output to `logFile`, honour `logInhibit` | None (before `vt_write`) | Tee in `pty.c` read path | READY |
| print-immediate | Dump screen text (+SGR) to `printerCommand` | Screen read: `ghostty_row_get`/`cell_get` or `select_all`+`selection_format_alloc` | Needs a "walk screen as styled text" helper | ROADMAP |
| print-on-error | Same, from the X I/O error handler | Same | Same walker | ROADMAP |
| print | Buffered screen dump | Same | Same walker | ROADMAP |
| print-redir | Printer controller mode (`CSI 5 i`/`4 i`) | Ghostty has no media copy; `OPT_UNKNOWN_SEQUENCE` is APC-only | Needs MC callback or CSI passthrough | BLOCKED |
| dump-html | Screen with colours as HTML | Screen read + `DATA_COLOR_PALETTE` | Reuse render walker | ROADMAP |
| dump-svg | Same as SVG | Same | Same | ROADMAP |
| 8-bit control | S8C1T single-byte C1 output | No encoder/response option | Ask for DRIFT entry instead | BLOCKED |
| backarrow key | DECBKM | `GHOSTTY_MODE_BACKARROW_KEY_MODE` (?67) | Mode flip | DONE |
| num-lock | Ignore keypad app mode with NumLock | `GHOSTTY_MODE_NUMLOCK_KEYPAD` (?1035) | Mode flip | DONE |
| alt-esc | `altSendsEscape` | `GHOSTTY_MODE_ALT_SENDS_ESC` (?1039) | Mode flip | DONE |
| meta-esc | `metaSendsEscape` | `GHOSTTY_MODE_ALT_ESC_PREFIX` (?1036); Ghostty has one "alt" | Mode flip + decide X Meta/Alt mapping (DRIFT note) | READY |
| delete-is-del | Delete sends `0x7F` | None | Special-case `XTP_KEY_DELETE` before encoder | READY |
| oldFunctionKeys | Legacy F-key table | Encoder has one table | Post-mapping table; low value | SKIP |
| tcapFunctionKeys | Keys from terminfo | None | Needs terminfo layer | SKIP |
| hpFunctionKeys | HP table | None | Static table | SKIP |
| scoFunctionKeys | SCO table | None | Static table | SKIP |
| sunFunctionKeys | Sun table | None | Static table | SKIP |
| sunKeyboard | VT220 keypad mapping | None | Same family | SKIP |
| suspend/continue/interrupt/hangup/terminate/kill | `kill(-pgrp, SIG*)` | None | Six one-liners in `pty.c` | READY |
| quit | Exit | — | — | DONE |

## vtMenu

| Item | What xterm does | libghostty involvement | Approachability | Status |
| --- | --- | --- | --- | --- |
| scrollbar | Show/hide Xaw scrollbar | `DATA_SCROLLBAR`, `scroll_viewport` (already wrapped) | Xaw widget, layout, thumb and navigation | DONE |
| jumpscroll | Batch lines per repaint | None | Render-scheduling knob; subtle drift | READY |
| reversevideo | Swap fg/bg | — | — | DONE |
| autowrap | DECAWM | `GHOSTTY_MODE_WRAPAROUND` (?7) | Mode flip | DONE |
| reversewrap | Reverse wrap | `GHOSTTY_MODE_REVERSE_WRAP` (?45) | Mode flip | DONE |
| autolinefeed | LNM | `GHOSTTY_MODE_LINEFEED` (20) | Mode flip | DONE |
| appcursor | DECCKM | `GHOSTTY_MODE_DECCKM` (?1) | Mode flip | DONE |
| appkeypad | DECKPAM | `GHOSTTY_MODE_KEYPAD_KEYS` (?66) | Mode flip | DONE |
| scrollkey | Keypress snaps to bottom | `scroll_viewport` | Resource, CLI, and menu policy | DONE |
| scrollttyoutput | Output snaps to bottom | Same | Resource, CLI, and menu policy | DONE |
| allow132 | Allow DECCOLM resize | `GHOSTTY_MODE_ENABLE_MODE_3` (?40); no mode-change callback, poll `DATA_COLS` after feed | Mode flip + small poll | READY |
| keepSelection | Keep highlight after loss | None | Roadmap #4 | ROADMAP |
| keepClipboard | Don't clear CLIPBOARD | None | Roadmap #4 | ROADMAP |
| selectToClipboard | Selection owns CLIPBOARD | Xt named-selection ownership | Resource, `set-select`, and menu policy | DONE |
| visualbell | Flash | Bell callback exists | Invert + timer | READY |
| bellIsUrgent | WM urgency hint | Bell callback | `XUrgencyHint` | READY |
| poponbell | Raise window | Bell callback | `XRaiseWindow` | READY |
| cursorblink | Blink cursor | `OPT_DEFAULT_CURSOR_BLINK`, `DATA_CURSOR_STYLE` | Timer and resource policy exist; define menu transitions among default and forced states | READY |
| titeInhibit | Ignore alt-screen switches | No way to disable a mode handler | Needs "permanently reset mode" option | BLOCKED |
| activeicon | Terminal in icon window | None | Modern WMs ignore | SKIP |
| softreset | DECSTR | Pinned Ghostty does not handle `CSI ! p` | Needs DECSTR support or a soft-reset API | BLOCKED |
| hardreset | RIS, keep saved lines | `ghostty_terminal_reset()` clears scrollback | Needs a reset-preserving-history API or a complete local RIS implementation | BLOCKED |
| clearsavedlines | Reset + drop scrollback | No clear API; `CSI 3 J` supported | Reset + `ESC [ 3 J`; test with `DATA_SCROLLBACK_ROWS` | ROADMAP |
| tekshow/tekmode/vthide | Tek 4014 window | None, ever | See Tek section | SKIP |
| altscreen | Show alternate screen | `DATA_ACTIVE_SCREEN`; `OPT_MODE` ?1047 should switch — verify the set path runs the full handler | Probably one call | ROADMAP |
| sixelScrolling | DECSDM | No sixel in Ghostty | Blocked upstream | BLOCKED |
| privateColorRegisters | Sixel colour registers | Sixel-only | Blocked | BLOCKED |

## fontMenu

| Item | What xterm does | libghostty involvement | Approachability | Status |
| --- | --- | --- | --- | --- |
| fontdefault, font1–font7 | Font slot | — | — | DONE |
| fontescape | Font from OSC 50 | OSC 50 ignored, not forwarded | Needs unknown-OSC passthrough | BLOCKED |
| fontsel | Font from PRIMARY | None | Small after Roadmap #4 | ROADMAP |
| allow-bold-fonts | Real bold faces | None; bold flag in `XtpRenderCell` | Renderer toggle | READY |
| font-linedrawing | Internal box glyphs | None | Box-glyph rasteriser; medium | ROADMAP |
| font-packed | Min glyph width for bitmap fonts | None | Cell-metric choice | ROADMAP |
| font-doublesize | DECDHL/DECDWL | Not in Ghostty | Blocked upstream | BLOCKED |
| font-loadable | DECDLD soft fonts | Not in Ghostty | Blocked, unused | BLOCKED |
| render-font | Xft vs bitmap | — | — | DONE |
| utf8-mode | UTF-8 vs Latin-1 | Ghostty is UTF-8 only | iconv possible; recommend permanently on (DRIFT) | BLOCKED |
| utf8-fonts | `utf8Fonts` slot set | None | Only meaningful with utf8-mode | ROADMAP |
| utf8-title | UTF-8 `_NET_WM_NAME` | Title callback | Set EWMH property | READY |
| allow-color-ops | Gate OSC 4/10–19 | Ghostty applies internally; `DATA_COLOR_*_DEFAULT` available | Render from defaults when denied; queries still drift | ROADMAP |
| allow-font-ops | Gate OSC 50 | Not routed | Moot until fontescape | BLOCKED |
| allow-mouse-ops | Gate mouse reports | Mouse encoder (Roadmap #3) | Skip emitting | ROADMAP |
| allow-tcap-ops | Gate XTGETTCAP | Ghostty answers itself (`OPT_TERMINFO_NAME`) | Needs option | BLOCKED |
| allow-title-ops | Gate OSC 0/1/2 + reports | Title callback, `OPT_TITLE_REPORT` | Trivial | READY |
| allow-window-ops | Gate XTWINOPS | `OPT_SIZE` covers 14/16/18 t only | Gate reports now; other `t` ops need a callback | ROADMAP |

<!-- markdownlint-enable MD013 -->

## Tektronix 4014

In xterm the Tek items drive a second top-level widget (`TekWidget`,
`Tekproc.c` ≈ 2,100 lines plus `TekPrsTbl.c` state tables). Nothing about it
is a mode of the VT100 engine:

- Separate parser. In Tek mode the PTY byte stream bypasses the VT parser.
  Switching is by `CSI ? 38 h` (DECTEK), `ESC ^X`, `ESC ETX`, or the menu.
  libghostty would treat ?38 as unknown and keep consuming bytes as text.
- Vector renderer: four text sizes, vector/point/incremental plot modes,
  1024×780 virtual coordinates, persistent display list for redraw.
- GIN mode: crosshair cursor; a keypress sends key plus encoded coordinates.
- Copy: writes the display list as raw Tek escapes to `COPY` files.

xterm+ would have to intercept bytes before `XtpTerminalFeed`, own a Tek
state machine and display list, and add a widget with its own `tekMenu`.
libghostty contributes nothing. xterm's own `--disable-tek4014` builds leave
the three items insensitive, which is what xterm+ does today. Recommendation:
record "Tek 4014 not planned" in the [xterm differences ledger](drift.md).

## Suggested order

1. Slice A: the generic mode helper and eight mode-backed items are complete.
   Continue with signals ×6; allowsends; visualbell/bellIsUrgent/
   poponbell; utf8-title; allow-title-ops; delete-is-del; and logging.
2. Slice B, ride the roadmap: scrollbar family with #2; selection policies
   and fontsel with #4; allow-mouse-ops with #3; allow-color-ops,
   allow-bold-fonts, font-linedrawing, font-packed with #5.
3. Slice C, screen-dump family: one styled-text walker unlocks print,
   print-immediate, print-on-error, dump-html, dump-svg.
4. Upstream asks, by value: unknown OSC/CSI passthrough (fontescape,
   print-redir, full XTWINOPS); permanently-reset mode (titeInhibit);
   XTGETTCAP toggle. Not worth asking: DECDHL, sixel, DECDLD, S8C1T.
5. Drift-ledger entries: Tek 4014, toolbar, activeicon, legacy keyboard
   tables/sunKeyboard, UTF-8-only, 8-bit controls, soft fonts.
