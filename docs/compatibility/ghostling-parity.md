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
| Kitty keyboard protocol | Present | Retain exact state-stack and X11 press/repeat/release fixtures; expand non-US XIM coverage as layouts become available in CI. |
| Kitty graphics protocol | Missing | Expose image placement/lifecycle through `terminal.h` and safely composite it in X11. Parser state alone is not promotion. |
| X10, normal, button-event, and any-event mouse tracking | Present | Retain backend and Xvfb routing coverage. |
| SGR, URxvt, UTF-8, and X10 mouse reports | Present | Retain exact encoding coverage; xterm+ also supports SGR-pixel reports. |
| Scroll wheel for history or application forwarding | Present | Retain local/application routing and Shift-override coverage. |
| Draggable scrollbar | Present | Retain Xaw thumb, placement, and deep-history coverage. |
| Focus reporting (`CSI I` and `CSI O`) | Present | Retain real focus-event coverage gated by DEC private mode 1004. |

<!-- markdownlint-enable MD013 -->

Current total: **8 Present, 3 Partial, 1 Missing**.

## Kitty keyboard acceptance backlog

The terminal core parses Kitty keyboard configuration and its encoder consults
current terminal state. xterm+ now also preserves X11 press, detectable
autorepeat, and release actions at that boundary. Promotion is backed by the
following maintained checks:

- [x] Query current flags with `CSI ? u`, including correct ordering with a
  following device-attributes query.
- [x] Set, augment, and clear flags with all three `CSI = flags ; mode u` modes.
- [x] Push and pop nested flag sets, restoring the outer application's state.
- [x] Preserve the legacy encoding when no flags are active.
- [x] Preserve libghostty's default fixterms exception to legacy encoding:
  Ctrl-I is `CSI 105;5 u` while Tab is `0x09`, even before Kitty flags are
  enabled. This intentional xterm incompatibility is recorded in the
  [xterm differences ledger](drift.md).
- [x] Flag `1`: disambiguate Escape, Ctrl/Alt combinations, Tab, Enter, and
  Backspace.
- [x] Flag `2`: distinguish press, autorepeat, and release. The X11 adapter
  uses XKB detectable autorepeat when available and the conventional paired
  release/press fallback on older servers.
- [x] Flag `4`: report shifted and base-layout alternatives.
- [x] Flag `8`: encode ordinary printable keys as escape sequences and verify its
  interaction with DECCKM and keypad modes.
- [x] Flag `16`: report associated text, including composed/XIM UTF-8 input.
- [x] Preserve Shift, Ctrl, Alt, Super, Caps Lock, and Num Lock modifiers in the
  combinations representable by X11.
- [x] Keep bracketed paste, mouse reports, xterm local bindings, popup menus, and
  the OSC 8 Shift gesture outside the keyboard protocol.
- [x] Turn real application failures reported by the maintainer into named,
  reproducible fixtures with exact expected PTY bytes.

`tests/xvfb-keyboard.sh` protects the zero-flags Ctrl-I/Tab split.
`tests/xvfb-kitty-keyboard.sh` drives real X key events and compares exact PTY
bytes for press, repeat, release, shifted alternatives, associated text, and a
bare modifier. The backend self-test protects query ordering, every set mode,
and nested stack restoration. Non-US layouts and composed XIM input remain
useful matrix expansion, but are no longer missing protocol plumbing.

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
