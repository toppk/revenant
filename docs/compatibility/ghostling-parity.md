# Ghostling feature-parity gate

Ghostling is xterm+'s minimum modern-terminal capability floor. The goal is
not merely to use the same parser: each advertised feature must cross the
backend boundary and work through the real X11 interface.

MVP requires every item in this checklist to be **Present**, plus the daily
xterm-driver advantages that make xterm+ more than a Ghostling port: the
Xt/Athena interface, xterm resources and bindings, saved-history navigation,
and X11 selection and paste behavior.

“Partial” means libghostty already supplies state or encoding, but xterm+ has
not finished the user-visible integration or acceptance coverage.

<!-- markdownlint-disable MD013 -->

| Advertised Ghostling capability | Status | Promotion still required |
| --- | --- | --- |
| Resize with text reflow | Present | Retain live Readline, large-history, and stale-frame regression coverage. |
| Full 24-bit color and 256-color palette | Present | The terminal output works; applying xterm `color0` through `color15` resources is a separate xterm-fidelity task. |
| Bold, italic, and inverse styles | Partial | Add an italic Xft face/path and complete style-combination coverage. |
| Unicode and multi-codepoint graphemes, without shaping or layout | Partial | Full grapheme bytes reach Xft; add fallback faces and acceptance coverage for unsupported glyphs, combining text, and emoji. |
| Keyboard input with Shift, Ctrl, Alt, and Super | Partial | Complete the X11 key map and exact modifier/application-mode regression matrix. |
| Kitty keyboard protocol | Partial | Complete the acceptance backlog below, especially repeat and release events. |
| Kitty graphics protocol | Missing | Expose image placement/lifecycle through `terminal.h` and safely composite it in X11. Parser state alone is not promotion. |
| X10, normal, button-event, and any-event mouse tracking | Present | Retain backend and Xvfb routing coverage. |
| SGR, URxvt, UTF-8, and X10 mouse reports | Present | Retain exact encoding coverage; xterm+ also supports SGR-pixel reports. |
| Scroll wheel for history or application forwarding | Present | Retain local/application routing and Shift-override coverage. |
| Draggable scrollbar | Present | Retain Xaw thumb, placement, and deep-history coverage. |
| Focus reporting (`CSI I` and `CSI O`) | Present | Retain real focus-event coverage gated by DEC private mode 1004. |

<!-- markdownlint-enable MD013 -->

Current total: **7 Present, 4 Partial, 1 Missing**.

## Kitty keyboard acceptance backlog

The terminal core already parses Kitty keyboard configuration and its encoder
consults current terminal state. That is not sufficient for parity. Promotion
requires real X key events to exercise all of the following through the PTY:

- Query current flags with `CSI ? u`, including correct ordering with a
  following device-attributes query.
- Set, augment, and clear flags with all three `CSI = flags ; mode u` modes.
- Push and pop nested flag sets, restoring the outer application's state.
- Preserve the legacy encoding when no flags are active.
- Preserve libghostty's default fixterms exception to legacy encoding:
  Ctrl-I is `CSI 105;5 u` while Tab is `0x09`, even before Kitty flags are
  enabled. This intentional xterm incompatibility is recorded in `DRIFT.md`.
- Flag `1`: disambiguate Escape, Ctrl/Alt combinations, Tab, Enter, and
  Backspace.
- Flag `2`: distinguish press, autorepeat, and release. xterm+ currently
  forwards only the press-side event and therefore does not satisfy this
  flag yet.
- Flag `4`: report shifted and base-layout alternatives.
- Flag `8`: encode ordinary printable keys as escape sequences and verify its
  interaction with DECCKM and keypad modes.
- Flag `16`: report associated text, including composed/XIM UTF-8 input.
- Preserve Shift, Ctrl, Alt, Super, Caps Lock, and Num Lock modifiers in the
  combinations representable by X11.
- Keep bracketed paste, mouse reports, xterm local bindings, popup menus, and
  the OSC 8 Shift gesture outside the keyboard protocol.
- Turn real application failures reported by the maintainer into named,
  reproducible fixtures with exact expected PTY bytes.

The wire format and manual probes are documented in the
[TDN Kitty keyboard reference](https://toppk.github.io/xterm-plus/tdn/input/kitty-keyboard/).
Run `python3 tools/probe-keymodes.py --kitty-only` from a checkout for the maintained
interactive acceptance probe; it preserves exact bytes, decodes event types,
and verifies flag-stack restoration.

## Definition of “better than Ghostling”

Passing this checklist establishes the modern capability floor. xterm+ exceeds
it by preserving the xterm daily-driver contract and by shipping tested local
features such as named X11 selections, reflow-safe historical selection,
middle-button paste, OSC 8 interaction, popup menus, font switching, cursor
policy, and X resource configuration. Neither half substitutes for the other.
