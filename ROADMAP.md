# xterm+ roadmap

xterm+ has two simultaneous product requirements:

1. Preserve xterm's visible X11/Xt/Athena experience and compatibility
   contract wherever practical.
2. Expose at least the terminal capability demonstrated by Ghostling when the
   pinned `libghostty-vt` already provides the underlying facility.

The first requirement defines how xterm+ should look, configure, and behave at
its X11 boundary. The second is a functional floor. Pixel fidelity and obscure
xterm options must not indefinitely outrank basic modern terminal capability.

Ghostling is a deliberately minimal demo rather than a complete product. Its
single-file architecture and Raylib UI are not models for xterm+. Its use of
the libghostty C API is the comparison point. See `UPSTREAM.md` for checkout
roles and revision policy.

## Capability baseline

This matrix compares xterm+ with the Ghostling checkout reviewed on
2026-08-24. “Partial” means terminal state reaches xterm+ or some behavior is
present, but the complete user-visible integration is not yet available.

<!-- markdownlint-disable MD013 -->

| Capability demonstrated by Ghostling | xterm+ status | Remaining integration |
| --- | --- | --- |
| PTY-backed shell and terminal effects | Present | Retain ordered backpressure coverage for the shared write queue and expand the effect surface. |
| Resize with primary-screen reflow | Partial | Retain geometry regression coverage; track the wrapped-prompt failure reported in Ghostty [discussion #14026](https://github.com/ghostty-org/ghostty/discussions/14026). |
| 24-bit and 256-color terminal output | Partial | Terminal colors render, but xterm `color0` through `color15` resource overrides are not all applied. |
| Bold, italic, inverse, and decorations | Partial | Xft uses a real clipped bold face; bitmap bold remains synthetic, italic is incomplete, and style/color combinations need compatibility tests. |
| Unicode and multi-codepoint graphemes | Partial | State and UTF-8 cross the backend boundary; Xft still lacks shaping, fallback faces, and color emoji. |
| Mode-aware keyboard input and modifiers | Partial | Basic mapping and libghostty encoding exist. Expand key coverage and test application modes and Kitty keyboard behavior. |
| Default VT bindings | Partial | The patch-410 binding groups are audited; add Shift+Select, Alt+Return fullscreen, Scroll Lock, and clear-saved-lines as their actions become available. |
| Scrollback viewport | Present | Add saved-line clearing and retain deep-selection regression coverage. |
| Wheel behavior and draggable scrollbar | Present | Retain local-history versus application-reporting coverage and extend Xaw styling tests. |
| Mouse tracking and reporting formats | Present | Add Xvfb event-routing coverage and the remaining xterm mouse-policy resources. |
| Focus reporting | Present | Retain exact `CSI I`/`CSI O` encoding and X focus-transition coverage while DEC private mode 1004 is enabled. |
| Kitty graphics | Missing | Add terminal-boundary image placement data and an X11 rendering path, with resource limits and safe image decoding. |
| Dynamic title and bell | Present | Continue the xterm resource/permission audit for title, icon, visual bell, and urgency behavior. |

<!-- markdownlint-enable MD013 -->

Selection is an xterm+ requirement even though the reviewed Ghostling feature
list does not make it a baseline item. The pinned libghostty API already
provides selection gestures, history-safe grid references, row selection
ranges, and formatted selection text. xterm+ now renders those ranges, owns
X11 `PRIMARY`, supports xterm-style Button-3 extension, and sends Button-2
paste through Ghostty's bracketed-paste encoder. Selection autoscroll preserves
tracked endpoints across deep history for both Button-1 drag and Button-3
extension. `CLIPBOARD` and the remaining xterm selection-policy resources are
still open.

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
- Add Xvfb integration tests for widget identity, resources, menus, font
  switching, checkmarks, geometry, and later scrollback and selection.
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
- Extend `scroll-back`/`scroll-forw` parameter compatibility and implement
  `clear-saved-lines` with xterm-compatible policy. `scrollKey` and
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
  scrollback override while application tracking is active.

### 4. Selection, copy, and paste

- Retain the libghostty gesture/grid-reference implementation across live and
  historical rows, including cell, word, line, drag, rectangular, whitespace,
  and Button-3 extension behavior.
- Retain selection autoscroll coverage and add the remaining xterm
  selection-policy resources.
- Extend the implemented highlighting, X11 `PRIMARY`, and Button-2 paste path
  with `CLIPBOARD` policy and additional ICCCM targets.
- Extend the implemented bracketed-paste/control-byte encoding with the
  remaining xterm paste controls.
- Define OSC clipboard policy explicitly; protocol requests must not bypass
  the X11/user permission model.

### 5. Renderer parity and Kitty graphics

- Apply xterm palette and pointer resources.
- Retain real clipped bold Xft faces; add italic faces, font fallback, shaping,
  and color emoji while retaining fixed terminal-cell placement and
  display-aware Xft point sizing.
- Retain application-selected block, underline, and bar cursor and blink
  coverage and the four-value `cursorBlink` startup policy; wire the remaining
  startup shape resources and menu toggle.
- Expose Kitty graphics placement and image lifecycle through a
  backend-neutral renderer interface, then implement safe X11 composition.
- Keep image-protocol work isolated from the traditional xterm appearance;
  capability does not require redesigning the UI.

### 6. Broader xterm compatibility and packaging

- Continue converting insensitive menu entries and unsupported actions into
  tested implementations.
- Complete command-line parsing, including `-name`.
- Add a man page, CI, installable icons, and an app-default packaging strategy
  that does not overwrite a distributor's upstream `XTerm` file.
- Advance beyond patch 410 only as an explicit compatibility migration.

## Architecture guardrails

- `src/terminal.h` owns backend-neutral terminal capability. Xt and X11 code
  must not reach directly into Ghostty handles.
- libghostty owns terminal history, reflow, protocol modes, selection
  semantics, and image protocol state. Do not build parallel copies in the UI.
- xterm+ owns the PTY/event loop, X resources, Xt actions/translations, Xaw
  widgets, X11 selections, renderer, window-manager behavior, and policy.
- Extract focused modules as capabilities grow; avoid making `main.c` and
  `vt_widget.c` the permanent home of every integration.
- Keep `-report-config`, menu sensitivity, tests, `DRIFT.md`, and this roadmap
  honest whenever a capability changes state.
