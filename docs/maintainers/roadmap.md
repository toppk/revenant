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

The [Ghostling feature-parity gate](../compatibility/ghostling-parity.md) is a
capability comparison rather than an automatic release gate. Every advertised
item must keep an honest Present, Partial, or Missing status at the user-visible
boundary. Kitty graphics remains Missing and is explicitly parked in the
current TODO drain; this exception must not be reported as parity.

## Next-release frontend compatibility batch

The selected batch **T1a, C1, T1c, W1 and P2a** is implemented and reviewed.
The [compatibility ledger](../compatibility/drift.md) records title actions and
`sameName`, effective Color/Title/Window Ops permissions under `allowSendEvents`,
exposed size-report gating, and hold-after-exit. Regression evidence lives in
`xvfb-title-ops`, `xvfb-color-ops`, `xvfb-window-ops`, the size-report policy
self-test and `xvfb-hold`. Native probes and their limitations are documented in
[the probe reference](../reference/probes.md).

T1b's UTF-8 title surface and other P2 options remain in `todo.md`. A separate
startup defect was identified: `-geometry 40x5` is treated as widget pixels
rather than a 40-column, 5-row request. Give it its own implementation and
external geometry regression; it was not repaired by W1 or P2a.
The completed batch does not replace the dependency release checkpoint below.

## Upstream integration follow-ups

The September 2026 Ghostty/Monstar review is complete. Implemented behavior and
regression evidence live in the [compatibility ledger](../compatibility/drift.md);
Unicode provenance lives in the [migration record](unicode-18-migration.md).
The temporary implementation queue has been retired. Remaining decisions are:

- **Ghostty 1.4.0 release checkpoint (maintainer-owned):** when the release is
  available, compare its exact commit against the adopted development pin
  `27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c`. Review intervening API and data changes,
  adopt an exact release commit, and repeat Unicode work only for an actual delta.
  Run the required supported-build/package matrix once at the final dependency
  checkpoint, including the no-skips release gate. Verify backend/data provenance,
  isolated and installed-font behavior, and representative shell/editor/multiplexer
  use. Keep xterm-411 as the compatibility oracle until a separate migration.
- **Alternate scroll (mode 1007):** a separate feature proposal, not part of the
  wheel-release or drag fixes. Decide the resource/default policy before adding
  cursor-key synthesis; respect application cursor mode, tracking precedence,
  configured scroll amounts and Mouse Ops policy. The existing mouse handoff
  probe exposes today's unsupported behavior.
- **Mouse differences:** pixel coordinates in mode 1016 are currently zero-based,
  unlike xterm's one-based reports; mode 1003 reports motion within the same cell.
  Keep these differences explicit in the ledger and review them as separate
  compatibility work rather than silently changing M3's accepted behavior.
- **Rendering study:** defer linear-light composition and fractional raster phases
  this release; preserve fontconfig hinting and explicit font precedence. The
  [study](linear-light-study.md) separates simulated contrast from human readability.
  Corrected captures show no fitted-emoji ink outside its cell, so no fitting fix
  is authorized by the withdrawn overflow claim. Whether hinting clips part of
  the fitted raster remains an unproven, nonblocking research question.
- **Evidence closeout:** refresh versioned TDN evidence when citing the landed
  synchronized-output and input changes. Keep visual readability and interactive
  probe assessments unassessed until someone actually records observations;
  automated byte/pixel checks support only the behavior they assert.

## Capability baseline

This matrix reflects the maintained status on 2026-09-14. “Partial” means
terminal state reaches Revenant or some behavior is present, but the complete
user-visible integration is not yet available. Refresh the comparison when
advancing the selected libghostty commit, and keep the dedicated parity
checklist aligned with the evidence.

<!-- markdownlint-disable MD013 -->

| Capability demonstrated by Ghostling | Revenant status | Remaining integration |
| --- | --- | --- |
| PTY-backed shell and terminal effects | Present | Retain ordered backpressure coverage for the shared write queue. XTWINOPS size, XTVERSION, and audited DA1/DA2/DA3 replies are integrated; the drift ledger records the capability evidence. |
| Resize with primary-screen reflow | Present | Retain geometry regression coverage; the [Readline 8.3 wrapped-prompt regression](../reference/bash-readline-resize.md) is fixed upstream and requires no terminal workaround. |
| 24-bit and 256-color terminal output | Present | Retain xterm `color0` through `color15` resource and OSC 4/104 reset coverage. |
| Bold, italic, inverse, and decorations | Present | Xft uses real clipped bold, italic, and bold-italic faces; bitmap bold remains synthetic as a separate xterm-fidelity task. |
| Unicode and multi-codepoint graphemes | Present | Unicode 17 emoji presentation, color formats, general fontconfig fallback, contextual shaping, and atomic role selection are covered. |
| Mode-aware keyboard input and modifiers | Present | Normal/application cursor and keypad modes, modifiers, editing/function keys, non-US UTF-8, XIM Compose, and Kitty event delivery have exact fixtures. |
| Default VT bindings | Partial | The patch-411 binding groups are audited; Shift+Select, Alt+Return fullscreen, Scroll Lock, and Meta+Button-2 clear-saved-lines remain open. |
| Scrollback viewport | Present | Retain deep-selection regression coverage; clear-saved-lines remains an independent xterm compatibility gap. |
| Wheel behavior and draggable scrollbar | Present | Retain local-history versus application-reporting coverage and extend Xaw styling tests. |
| Mouse tracking and reporting formats | Present | Add Xvfb event-routing coverage and the remaining xterm mouse-policy resources. |
| Focus reporting | Present | Retain exact `CSI I`/`CSI O` encoding and X focus-transition coverage while DEC private mode 1004 is enabled. |
| Hyperlinks | Present | Retain explicit OSC 8 precedence, bounded soft-wrapped HTTP(S) detection, and Xvfb Shift-hover/click coverage; keep non-HTTP schemes inert unless policy is deliberately revised. |
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
- Extend `scroll-back`/`scroll-forw` parameter compatibility, and add
  xterm-compatible `clear-saved-lines` with its Meta+Button-2 binding.
  `scrollKey` and
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
- OSC 52 follows xterm's `allowWindowOps`/`disallowedWindowOps` policy and
  reuses the X11 selection machinery. Remaining asks are upstream parser
  limits recorded in the drift ledger (selection lists, `SECONDARY`, cut
  buffers, invalid-payload clearing). The `allow-window-ops` menu toggle
  applies the policy at runtime.

### 5. Renderer parity and Kitty graphics

- Apply the remaining xterm pointer and specialized color resources.
- Retain real clipped bold/italic Xft faces, Unicode emoji/CJK role routing,
  bounded fontconfig fallback, color formats, contextual HarfBuzz shaping, and
  fixed cell placement while preserving display-aware Xft point sizing.
- Retain application-selected block, underline, and bar cursor and blink
  coverage, the four-value `cursorBlink` policy, and xterm's `cursorBlinkXOR`
  composition. `cursorUnderLine`/`-uc` and `cursorBar`/`-barc` now set libghostty's
  default cursor style, with underline taking precedence over bar as it does
  in xterm; leave the remaining menu policy for
  its owning slice.
- Expose Kitty graphics placement and image lifecycle through a
  backend-neutral renderer interface, then implement safe X11 composition.
- Keep image-protocol work isolated from the traditional xterm appearance;
  capability does not require redesigning the UI.

### 6. Broader xterm compatibility and packaging

- Complete xterm's full Window Ops category. OSC 52, the title stack, the
  title reports, and the live menu toggle are implemented; `GetSelection`,
  `SetSelection`, `GetIconTitle`, `GetWinTitle`, `PushTitle`, `PopTitle`, and
  exposed CSI 14/16/18 size reports consult the permission policy. Finish the
  remaining XTWINOPS manipulation and geometry reports, then column/line resizing,
  checksums, X properties, and status-line controls;
  preserve libghostty parser ownership and track missing public hooks upstream.
  Require per-operation policy tests, live toggle coverage, and differential
  X11/window-manager checks before marking the category complete.
- Continue converting insensitive menu entries and unsupported actions into
  tested implementations.
- Finish Title Ops compatibility beyond the working live toggle: independent
  OSC 1/OSC 0 icon updates, title encoding modes, UTF-8 title resources and
  ICCCM/EWMH properties and title normalization/length rules. The translation
  action, `allowSendEvents` interaction and `sameName` are complete. Keep title-setting authorization
  separate from Window Ops report and stack permissions. The maintainer
  handoff contains the source audit and acceptance scope.
- Extend the now-honest command-line parser with the remaining process and
  resource semantics classified in the feasibility study. Unknown options,
  `-help`/`-version`, `-e`, resource aliases, and `-name`/`-class` are covered.
- Implement xterm session transcript logging as TODO P1, one coherent slice:
  `-/+l`, `-lf`, `logFile`, `logInhibit`, and the `logging` menu action must
  share a safe, nonblocking PTY-output tee and remain separate from diagnostic
  `-log` severity.
- `-welcome` is implemented as a read-only setup assistant over the existing
  configuration-report and font-resolution paths. It should diagnose a bare
  installation as well as an established xterm setup: resolved app-defaults,
  live server resources, likely resource-file workflow, configured bitmap/Xft
  fonts and fallbacks, and representative emoji/CJK availability. Emit a small
  starter resource fragment and actionable documentation/package suggestions;
  do not install packages, modify user files, or load the X resource database.
- The [emoji artwork gate](emoji-artwork-gate.md) is the acceptance suite for this
  work. It passes on the reported line and carries no known gaps; a future gap must
  be entered there with the mechanism that produces it.
- Done: the [emoji fallback review](font-fallback-review.md) is closed using
  installed fonts. Presentation-aware matching and discovery, relevant candidate
  selection, and span fitting within backend-owned cells are specified in
  [font-resolution](font-resolution.md); `-welcome` shows requested versus
  effective files, names the color and text emoji roles, and explains inherited
  Fontconfig preferences without treating them as errors or rewriting them. One of
  the two coverage gaps this entry listed is closed: the generated `styled-emoji`
  family supplies real bold and italic monochrome faces. Styled whole-sequence
  selection, reload/rollback painting and a scoped Unicode coverage audit are covered
  in the [artwork gate](emoji-artwork-gate.md), which records the cases, evidence
  and limits without duplicating the implementation contract.
- Still open, and unchanged by the text-presentation work: with no color face
  installed, a **one-cell emoji-presentation** atom — bare VS16 under the legacy
  width regime — has no monochrome substitute. Span fitting rescues
  *text*-presentation atoms; an emoji-presentation atom whose only supply is an
  oversized monochrome face is still refused by the advance rule and renders
  deterministic tofu, as
  [font-resolution](font-resolution.md#what-presentation-does-and-does-not-decide)
  specifies and `tests/xvfb-font-discovery.sh` asserts. The two-cell row of that
  table does serve monochrome; conflating the two is what this entry guards against.
- Open, and needing a person rather than code: no artwork capability carries a
  Revenant support record, because legibility has not been assessed. Follow the
  [visual acceptance procedure](emoji-artwork-gate.md#visual-acceptance-procedure)
  at several sizes and record the observation in `tdn/terminals/revenant.yaml`.
  Also open by design: the pinned monochrome fixture predates E17.0, so seven newer
  bases have no monochrome supply — a font gap the gate records with the advance
  still asserted — and behavior outside the audit's inventoried scope carries no
  coverage claim in either direction.
- Future, and separate from each other: (1) evaluate vendored color and monochrome
  emoji fonts as optional defaults, with pinned provenance, licenses, coverage tests,
  and installed artifact checks — users must be able to replace both faces with their
  own fonts or disable the vendored source, preserving explicit choices, CJK capture,
  budgets and fixed widths; (2) decide whether to adopt optional symbol enlargement
  into adjacent whitespace (`text-symbol-whitespace-expansion`), which is recorded as
  unsupported today and would be an opt-in with its own eligibility, background,
  selection, cursor and damage rules. Neither is a prerequisite for emoji
  correctness, and installed-font routing works without them.
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
