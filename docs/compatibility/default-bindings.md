---
man: revenant-bindings
section: 7
manual: revenant
description: default VT translations audited against xterm
---

# Default VT bindings

This audit tracks the default VT translation groups assembled by
`VTInitTranslations()` in upstream xterm patch 411. Revenant should not grow a
separate, accidental keyboard-and-mouse vocabulary: an xterm default is either
implemented, provided by an equivalent input path, or recorded here as a gap.

The source of truth is `src/charproc.c` in the tracked xterm repository. The
table covers the active patch-411 defaults and the bindings guarded by build
options in that function; it does not include user overrides from X resources.

<!-- markdownlint-disable MD013 -->

| xterm group | Default gesture or event | Revenant status | Notes |
| --- | --- | --- | --- |
| select | Shift+Select | Missing | Deferred to v0.6: keyboard-driven selection needs `select-cursor-start` and `select-cursor-end` plus explicit cursor/selection state. |
| select | Shift+Insert | Done | Uses `insert-selection(SELECT, CUT_BUFFER0)` and the bracketed-paste encoder; `SELECT` follows `selectToClipboard`, and named arguments are tried in order. |
| fullscreen | Alt+Return | Missing | Deferred to the coherent v0.6 fullscreen resource/menu/EWMH slice; the client message is small, but policy and useful window-manager testing are not. |
| scroll-lock | Scroll Lock release | Missing | Deferred to v0.6: output anchoring, focus/LED policy, and widget state make this more than a binding alias. |
| shift-fonts | Shift/Ctrl+keypad plus and Shift+keypad minus | Done | Uses xterm's historical larger/smaller direction, including Shift+Ctrl+keypad plus. |
| paging | Shift+Page Up/Down | Done | Scrolls one half-page locally and is not also sent to the PTY. |
| keypress | Ordinary and Meta keypresses | Equivalent path | XIM lookup and libghostty key encoding run in the raw key event handler rather than `insert-seven-bit`/`insert-eight-bit` actions. |
| popup-menu | Exact Ctrl+buttons 1, 2, and 3, including Lock/NumLock variants | Done | Opens `mainMenu`, `vtMenu`, and `fontMenu`. |
| reset | Meta+button 2 | Planned for v0.5 | Add with `clear-saved-lines`; libghostty's public full-reset operation matches xterm's RIS-plus-history-clear behavior. |
| select | Buttons 1, 2, and 3, motion, and release | Done | Cell/word/line selection, Button-3 extension, Button-2 paste, autoscroll, and application mouse routing share these bindings. |
| block-select | Meta+button 1 | Done | Starts rectangular selection. |
| wheel-mouse | Wheel, plus Ctrl half-page variants | Done | The default wheel distance is five lines, matching xterm; application tracking and Shift override remain effective. |
| pointer | Generic button and motion events | Done | Routes press, release, drag, hover, modifiers, and wheel events to an application when it enables mouse tracking. |
| default | Otherwise unmatched button release | Done | The generic release action is harmless when no selection or application report owns it. |

<!-- markdownlint-enable MD013 -->

## Regression rule

When a supported local key binding is added, the separate raw key handler must
also classify that gesture as translation-owned. Otherwise both paths run and
the terminal receives an unwanted escape sequence after performing the local
action. Shift+Insert originally exposed this failure by sending modified Insert
(`CSI 2 ; 2 ~`) instead of pasting.

Xt can also dispatch the same physical key event through more than one
translation route. Local key actions share an event-identity guard so paste,
paging, and font changes run once per X event without suppressing genuine
auto-repeat.

Shift+Button 1 on an explicit or detected hyperlink is an intentional Revenant
extension, not one of patch 411's default bindings. Shift-hover underlines the
target; pressing and releasing on the same target launches only HTTP or HTTPS
links. On a cell without an explicit OSC 8 target or detected URL, the same
gesture falls through to ordinary selection. The extension and its scheme
policy are documented in [Hyperlinks](../usage/hyperlinks.md) and the
[xterm differences ledger](drift.md).

Changes to the upstream baseline require re-reading `VTInitTranslations()` and
updating this table. Missing bindings are compatibility work, not intentional
drift.

See [Copy and paste on X11](../usage/copy-paste.md) for the distinction between
xterm's `SELECT` token, the X11 `PRIMARY` and `CLIPBOARD` selections, and its
legacy `CUT_BUFFER0` fallback.
