---
man: revenant-roadmap
section: 7
manual: maintainers
description: priorities and architecture guardrails
---

# Revenant roadmap

Revenant has two simultaneous product requirements:

1. Preserve xterm's visible X11/Xt/Athena experience and compatibility
   contract wherever practical.
2. Promote every advertised Ghostling capability through the real X11
   interface when the selected `libghostty-vt` supplies the underlying facility,
   then exceed that floor with the xterm daily-driver experience.

The first requirement defines how Revenant should look, configure, and behave at
its X11 boundary. The second is a functional floor. Pixel fidelity and obscure
xterm options must not indefinitely outrank basic modern terminal capability.

Ghostling is a deliberately minimal demo rather than a complete product. Its
single-file architecture and Raylib UI are not models for Revenant. Its use of
the libghostty C API is the comparison point. See the
[upstream reference guide](upstream.md) for checkout
roles and revision policy.

The [Ghostling feature-parity gate](../compatibility/ghostling-parity.md) is
an MVP gate, not an aspirational comparison. MVP requires every advertised
item to be Present at the user-visible boundary, including Kitty keyboard and
Kitty graphics, while retaining the xterm features that make Revenant a daily
driver rather than a differently skinned Ghostling.

## Capability baseline

This matrix compares Revenant with the Ghostling checkout reviewed on
2026-08-24. “Partial” means terminal state reaches Revenant or some behavior is
present, but the complete user-visible integration is not yet available.
The v0.5 inventory pass must refresh this dated comparison against the current
Ghostling checkout and selected libghostty commit, reconcile it with the
dedicated parity checklist, and record findings without turning the review into
an implementation gate.

<!-- markdownlint-disable MD013 -->

| Capability demonstrated by Ghostling | Revenant status | Remaining integration |
| --- | --- | --- |
| PTY-backed shell and terminal effects | Present | Retain ordered backpressure coverage for the shared write queue. XTWINOPS size and XTVERSION reports are integrated; review custom device-attribute identity in v0.6. |
| Resize with primary-screen reflow | Present | Retain geometry regression coverage; the [Readline 8.3 wrapped-prompt regression](../reference/bash-readline-resize.md) is fixed upstream and requires no terminal workaround. |
| 24-bit and 256-color terminal output | Present | Retain xterm `color0` through `color15` resource and OSC 4/104 reset coverage. |
| Bold, italic, inverse, and decorations | Present | Xft uses real clipped bold, italic, and bold-italic faces; bitmap bold remains synthetic as a separate xterm-fidelity task. |
| Unicode and multi-codepoint graphemes | Present | Unicode 17 emoji presentation, color formats, general fontconfig fallback, contextual shaping, and atomic role selection are covered. |
| Mode-aware keyboard input and modifiers | Present | Normal/application cursor and keypad modes, modifiers, editing/function keys, non-US UTF-8, XIM Compose, and Kitty event delivery have exact fixtures. |
| Default VT bindings | Partial | The patch-411 binding groups are audited; add Meta+Button-2 with the v0.5 clear-saved-lines slice, and defer Shift+Select, Alt+Return fullscreen, and Scroll Lock to v0.6. |
| Scrollback viewport | Present | Add xterm-compatible reset-and-clear-saved-lines behavior in v0.5 and retain deep-selection regression coverage. |
| Wheel behavior and draggable scrollbar | Present | Retain local-history versus application-reporting coverage and extend Xaw styling tests. |
| Mouse tracking and reporting formats | Present | Add Xvfb event-routing coverage and the remaining xterm mouse-policy resources. |
| Focus reporting | Present | Retain exact `CSI I`/`CSI O` encoding and X focus-transition coverage while DEC private mode 1004 is enabled. |
| OSC 8 hyperlinks | Present | Retain backend URI lookup and Xvfb Shift-hover/click coverage; keep non-HTTP schemes inert unless policy is deliberately revised. |
| Kitty graphics | Missing | Add terminal-boundary image placement data and an X11 rendering path, with resource limits and safe image decoding. |
| Dynamic title and bell | Present | Continue the xterm resource/permission audit for title, icon, visual bell, and urgency behavior. |

<!-- markdownlint-enable MD013 -->

Selection is a Revenant requirement even though the reviewed Ghostling feature
list does not make it a baseline item. The selected libghostty API already
provides selection gestures, history-safe grid references, row selection
ranges, and formatted selection text. Revenant now renders those ranges, owns
named X11 selections, supports xterm-style Button-3 extension, and sends
Button-2 paste through Ghostty's bracketed-paste encoder. Selection autoscroll
preserves tracked endpoints across deep history for both Button-1 drag and
Button-3 extension. `selectToClipboard`, ordered named action arguments, and
legacy cut buffers are implemented; the remaining xterm selection-policy
resources are still open.

Parity means equivalent semantic capability, not identical implementation.
Every integration must still fit the xterm resource model, menu surface,
translations, security expectations, and progressive rendering behavior.

## Priority plan

### 1. Reliability and test foundation

- Retain the lossless nonblocking PTY output queue across encoded keys, paste,
  mouse reports, focus reports, and terminal-generated responses, including
  forced `EAGAIN` coverage.
- Split the current smoke test into focused terminal-adapter and PTY tests.
- Test styles, cursor state, terminal effects, key modes, resize/reflow, and
  Unicode grapheme preservation against libghostty.
- Retain the Xvfb named-selection test for `selectToClipboard`, PRIMARY,
  CLIPBOARD, and `CUT_BUFFER0`, plus the OSC 8 test that permits HTTP and
  blocks a non-HTTP scheme; extend Xvfb coverage to widget identity,
  resources, menus, font switching, checkmarks, geometry, and scrollback.
- Test both stub and libghostty builds with strict GCC and Clang settings.
- Validate that menu sensitivity, registered actions, resource support
  classifications, and documentation describe the same capabilities.

This foundation is part of each feature rather than a long prerequisite: add
the first small harness, then grow it with every parity slice.

### 2. Scrollback and application-aware wheel input

- Retain coverage for backend-neutral history limits, `{total, offset,
  length}`, and relative and absolute viewport movement.
- Retain `saveLines`, `-sl`, `scrollBar`, `-sb`, `+sb`, `rightScrollBar`,
  `-rightbar`, `-leftbar`, and Athena child width/thickness resources.
- Extend the real Xaw scrollbar coverage for thumb state, dragging, left/right
  layout, and Athena styling resources.
- Extend `scroll-back`/`scroll-forw` parameter compatibility. For v0.5, add
  `clear-saved-lines` with xterm-compatible hard-reset behavior by using
  libghostty's public full-reset operation, which already clears retained
  history, and expose xterm's Meta+Button-2 binding. `scrollKey` and
  `scrollTtyOutput` are implemented as resources, command-line options, and VT
  menu toggles.
- When mouse tracking is active, encode wheel events for the application;
  otherwise scroll the libghostty viewport.
- Cover alternate-screen behavior, incoming output while scrolled back,
  resize/reflow, and large configurations such as 16,500 saved lines.
- Retain xterm's `scrollTtyOutput: false` invariant: incoming rows preserve the
  viewport's distance from the live bottom, so its absolute history offset
  advances rather than remaining pinned.

### 3. Mouse protocol and focus parity

- Retain libghostty mouse-encoder coverage for X11 press, release, motion,
  wheel, modifiers, and cell/pixel coordinates.
- Retain exact tests for X10, normal, button, any-event, SGR, URxvt, UTF-8,
  legacy X10, and SGR-pixel reporting selected by terminal state.
- Retain focus-in/focus-out protocol events only when the child enables DEC
  private mode 1004, with one report per real X focus transition.
- Preserve Ctrl+button menu grabs and selection gestures without leaking those
  UI events to terminal applications. Retain Shift as the local-selection and
  scrollback override while application tracking is active, including the
  Shift-hover and Shift+Button-1 OSC 8 gesture.

### 3a. Kitty keyboard promotion

The exact acceptance matrix lives in the
[Ghostling feature-parity gate](../compatibility/ghostling-parity.md). Treat
real maintainer application failures as high-value fixtures. Full promotion
requires query/set/push/pop state, every progressive-enhancement flag, legacy
fallback, modifiers, XIM text, and real press/repeat/release delivery through
the PTY. Parser support or press-only encoding does not satisfy this item.

### 4. Selection, copy, and paste

The [X11 copy/paste survey](../usage/copy-paste.md) records patch-411
semantics and describes the implemented named-selection path.

- Retain the libghostty gesture/grid-reference implementation across live and
  historical rows, including cell, word, line, drag, rectangular, whitespace,
  and Button-3 extension behavior.
- Retain selection autoscroll coverage and add the remaining xterm
  selection-policy resources.
- Retain `PRIMARY`/`CLIPBOARD`/`SECONDARY`, dynamic `SELECT`, ordered named
  action arguments, and `CUT_BUFFER0` through `CUT_BUFFER7`; add the remaining
  ICCCM text targets.
- Extend the implemented bracketed-paste/control-byte encoding with the
  remaining xterm paste controls.
- Define OSC clipboard policy explicitly; protocol requests must not bypass
  the X11/user permission model.

### 5. Renderer parity and Kitty graphics

- Apply the remaining xterm pointer and specialized color resources.
- Retain real clipped bold/italic Xft faces, Unicode emoji/CJK role routing,
  bounded fontconfig fallback, color formats, contextual HarfBuzz shaping, and
  fixed cell placement while preserving display-aware Xft point sizing.
- Retain application-selected block, underline, and bar cursor and blink
  coverage, the four-value `cursorBlink` policy, and xterm's `cursorBlinkXOR`
  composition. In v0.5, wire `cursorUnderLine`/`-uc` and `cursorBar`/`-barc`
  through the existing default cursor-style option, with underline taking
  precedence over bar as it does in xterm; leave the remaining menu policy for
  its owning slice.
- Expose Kitty graphics placement and image lifecycle through a
  backend-neutral renderer interface, then implement safe X11 composition.
- Keep image-protocol work isolated from the traditional xterm appearance;
  capability does not require redesigning the UI.

### 6. Broader xterm compatibility and packaging

- Continue converting insensitive menu entries and unsupported actions into
  tested implementations.
- Extend the now-honest command-line parser with the remaining process and
  resource semantics classified in the feasibility study. Unknown options,
  `-help`/`-version`, `-e`, resource aliases, and `-name`/`-class` are covered.
- Implement xterm session transcript logging in v0.6 as one coherent slice:
  `-/+l`, `-lf`, `logFile`, `logInhibit`, and the `logging` menu action must
  share a safe, nonblocking PTY-output tee and remain separate from diagnostic
  `-log` severity.
- For v0.5, `-welcome` is implemented as a read-only setup assistant over the existing
  configuration-report and font-resolution paths. It should diagnose a bare
  installation as well as an established xterm setup: resolved app-defaults,
  live server resources, likely resource-file workflow, configured bitmap/Xft
  fonts and fallbacks, and representative emoji/CJK availability. Emit a small
  starter resource fragment and actionable documentation/package suggestions;
  do not install packages, modify user files, or load the X resource database.
- Treat readability as an outcome, not as “an `XTerm.ad` was found.” Use
  resource provenance, renderer, resolved cell geometry, effective Xft/display
  density, and font matches to conservatively flag the tiny stock bitmap-font
  experience on high-density displays. Characterize the threshold before
  freezing it and recommend an explicit scalable-font resource fragment.
- Parse a strict whitelist from `/etc/os-release` plus `uname` architecture to
  select maintained package suggestions for missing `xrdb`, scalable monospace,
  emoji, and CJK capabilities. Use tested distribution-family fixtures and a
  generic fallback; never source the file, guess an unsupported package manager,
  or run an installer.
- Set `TERM_PROGRAM` and `TERM_PROGRAM_VERSION` for Revenant children so the
  welcome report can distinguish a Revenant host from inherited outer-terminal
  identity. Gate the advanced emoji/shaping sample on that signal and a terminal
  stdout; the renderer and font probes describe the newly resolved widget, not
  the terminal displaying the report. Finish with a redacted, stable support
  block containing OS/version, architecture, Revenant and backend identity,
  renderer, application identity, key font matches, app-default status, and
  host-terminal status; reserve the full resource database for the opt-in
  `-report-config` attachment.
- Keep xterm and `xrdb` optional while measuring the bare-install experience.
  Revenant already carries the reviewed `XTerm.ad` records and uses fontconfig
  directly. Compare soft package recommendations with expanding the compiled
  fallback resources; do not create a hard dependency solely to obtain another
  terminal's app-defaults file or a command-line frontend to X resources.
- Treat `xterm`/`XTerm` as pinned current behavior, not a closed identity
  decision. Evaluate the leading `revenant`/`XTerm` candidate and alternatives
  against instance-specific resources, class resources, app-defaults, WM_CLASS
  consumers, `-name`/`-class`, compatibility invocation, desktop integration,
  diagnostics, generated configuration, and rollback before changing it.
- Retain the installed man page and CI, then add installable icons and an
  app-default packaging strategy that does not overwrite a distributor's
  upstream `XTerm` file.
- Advance beyond patch 411 only as an explicit compatibility migration.

## Architecture guardrails

- `src/terminal.h` owns backend-neutral terminal capability. Xt and X11 code
  must not reach directly into Ghostty handles.
- libghostty owns terminal history, reflow, protocol modes, selection
  semantics, and image protocol state. Do not build parallel copies in the UI.
- Revenant owns the PTY/event loop, X resources, Xt actions/translations, Xaw
  widgets, X11 selections, renderer, window-manager behavior, and policy.
- Extract focused modules as capabilities grow; avoid making `main.c` and
  `vt_widget.c` the permanent home of every integration.
- Keep `-report-config`, menu sensitivity, tests, the
  [xterm differences ledger](../compatibility/drift.md), and this roadmap
  honest whenever a capability changes state.
