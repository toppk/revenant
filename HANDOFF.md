# Revenant maintainer guide

This is the durable continuation brief for maintainers. Current feature
priorities and the Ghostling capability comparison live in the
[roadmap](docs/maintainers/roadmap.md); external checkout roles live in the
[upstream reference guide](docs/maintainers/upstream.md). Avoid recording an uncommitted
file list or a single transient commit as project state here.

## Maintainer transition — 2026-09-02

The cursor-blink, ANSI-palette, and internal-naming rounds are complete. There
are no known open findings from their final reviews. The complete maintained
matrix passed at the transition point: GCC, Clang, and AddressSanitizer each
passed 26/26 tests, the stub backend passed 6/6, all four builds were
warning-free, and `git diff --check` was clean.

The important completed state is:

- Cursor blinking matches xterm's DECSCUSR/DEC mode 12 model, including the
  separate application operand, configurable XOR/OR composition, forced
  `always`/`never` policies, reset behavior, and query replies. The observer
  workaround and its parser tests are intentional; see Current architecture
  below before changing it.
- `color0` through `color15` are real supported resources with xterm's compiled
  defaults. They seed libghostty's default palette, survive OSC 4 current-color
  overrides, become the OSC 104 reset target, reach painted pixels, and leave
  indices 16 through 255 unchanged. Color names are parsed to RGB without
  allocating an X colormap entry.
- Product branding now has an enforced boundary. Project prose, Meson,
  packaging/release metadata, and concrete justfile launch paths may name the
  installed product. Internal C, tools, and tests use `XTP` or `xterm+`; the C
  program name comes from Meson's generated `XTP_PROGRAM_NAME`. The Meson
  `internal-branding` test enforces the internal trees. The release-note helper
  consequently lives at `packaging/release-notes`, and the synthetic sbix
  fixture is `XTP Synthetic sbix`/`XtpSyntheticSbix.ttf`.

Two review-method rules are now evidence-backed project practice. First, any
claim that “xterm does X” must be checked against the pinned
`upstream/xterm-snapshots` source and, when observable behavior is involved,
differentially exercised against xterm. Second, X resource tests must use an
isolated `HOME`; setting `XENVIRONMENT=/dev/null` does not suppress
`~/.Xdefaults`. A maintainer's loose `xterm*colorN` entries caused two false
palette findings and one implementation detour before a hermetic HOME and a
real-xterm differential exposed the contamination.

Resume from the v0.5 early-access plan and roadmap rather than reopening these
rounds without a concrete regression. Item 9 still names startup cursor-shape
resources, `clear-saved-lines`, and high-use key/action gaps after the completed
palette work. The honest command-line surface is now complete; session logging
and the option-driven process behaviors remain ahead.

## Mission

Revenant is a faithful, sustainable X11 replacement for xterm's visible user
experience with a modern terminal engine underneath. It installs as
`revenant`, retains `xterm+` as a compatibility symlink, and has two product
bars:

- preserve the xterm/Xt/Athena skin and compatibility contract;
- promote every advertised Ghostling capability through the X11 boundary,
  then exceed that floor with the xterm daily-driver experience.

The second bar corrects an earlier imbalance. Pixel similarity, resources,
menus, and geometry remain essential, but they do not justify leaving
scrollback, mouse protocols, focus reporting, graphics, or other baseline
terminal capabilities unwired.

The [Ghostling parity checklist](docs/compatibility/ghostling-parity.md) is the
explicit capability comparison and the default MVP gate, but the owner may
approve a clearly stated product exception rather than let one upstream demo
feature control a release indefinitely. Kitty graphics is the current
exception: it remains honestly Missing until images render, and libghostty
parser state alone does not count as promotion, but it is not a v0.5 or v0.6
release gate. Kitty keyboard is Present with progressive flags, stack, legacy
fallback, modifiers, composition, and real press/repeat/release events covered
end to end. Maintainer-reported keyboard application failures should continue
to become named regression fixtures.

Intentional architectural or behavioral differences from xterm belong in the
[xterm differences ledger](docs/compatibility/drift.md). Missing features belong
in the [roadmap](docs/maintainers/roadmap.md), not in the drift ledger.

## Working agreements

- Keep the branch named `master`.
- Do not commit unless the user explicitly asks.
- Do not configure, clean, or compile the user's `build/` directory. Use a
  private directory such as `build-agent-gcc` or `build-agent-clang`.
- Preserve unrelated and concurrent working-tree changes.
- `upstream/`, `build-*`, and `profiles/` are ignored deliberately.
- Keep compatibility changes separate from broad cleanup or formatting.
- Progressive, line-by-line-feeling output is observable behavior and must
  remain a performance concern.
- This remains an X11 program. Xt/Athena are part of the desired UI, not
  scaffolding scheduled for replacement.
- XCB may share the Xlib connection where appropriate, but event-queue
  ownership must stay coherent.

## Maintainer role and feedback arbitration

The coding agent is expected to act as an active maintainer and technical
arbiter, not as a transcription service. It should maintain an independent,
evidence-based view of the tree, test results, documented product bars, and
the user's observed daily-driver behavior. External reviews, automated
findings, and suggestions from other agents are valuable inputs, but none is
automatically authoritative.

For each material suggestion, the maintainer should inspect the affected code
and choose to accept, adapt, defer, or reject it. State the reason when the
choice is not obvious. Prefer work in this order:

1. correctness, security, data integrity, and lossless PTY behavior;
2. regressions found during real Revenant use, especially input, resize,
   selection, scrollback, and rendering failures;
3. the xterm-visible compatibility contract and the Ghostling promotion gate;
4. testability, clear module ownership, and sustainable architecture;
5. performance and cleanup that have measured or concrete maintenance value.

Do not create churn merely to satisfy a stylistic review, silently broaden a
request, or preserve an internal abstraction at the expense of user-visible
correctness. Conversely, do not dismiss feedback because xterm has similar
behavior or because a problem has existed for years. Reproduce it, determine
which layer owns it, and turn confirmed regressions into focused tests or
durable documentation whenever practical.

The maintainer owns technical diligence and honest status reporting. The user
remains the project owner and release authority: product-policy decisions,
public announcements, commits under the current working agreement, pushes,
tag changes, and destructive operations remain subject to the user's explicit
direction. When evidence is incomplete, distinguish a hypothesis from a
finding and preserve the user's concerns rather than declaring the project
ready.

## Code ownership and reference projects

Revenant does not compile, link, or embed xterm's terminal implementation. The
PTY/event loop, Xt widget, X11 renderer, diagnostics, menu wiring, and
libghostty adapter are Revenant code. `libghostty-vt` owns parsing, terminal
state, key and mouse encoding, query responses, resize reflow, history,
selection primitives, and graphics protocol state.

The exact `xterm-411` tag in `upstream/xterm-snapshots` is the visible
behavioral oracle. The neighboring `/home/toppk/workspace/xterm` checkout is a
historical patch-410 working reference. The checked-in compatibility material
is:

- `data/app-defaults/XTerm`;
- `compat/xterm-411-resources.tsv`;
- `compat/xterm-411-app-defaults.txt`;
- `compat/xterm-411-actions.txt`;
- `compat/xterm-411-face-name.json`;
- widget/menu names, defaults, translations, and reconstructed behavior.

Those portions are covered by `LICENSES/xterm.txt`. Consult xterm to reproduce
external behavior; do not copy its terminal engine wholesale.

See the [upstream reference guide](docs/maintainers/upstream.md) for the ignored
Ghostty, Ghostling, xterm snapshot, and xterm.dev checkouts and their update
rules. Ghostling is now the minimum functional comparison, while xterm remains
the UI and compatibility oracle.

## Current architecture

The durable module map and ownership rules live in
[the maintainer architecture guide](docs/maintainers/architecture.md). Keep
this ignored handoff focused on transient continuation notes and constraints
that should not become project documentation.

As new capability is added, prefer focused modules over continued growth of
`main.c` and `vt_widget.c`. Keep Ghostty-specific types behind `terminal.h`.
The private VT-widget boundary is now established. Keep keyboard/XIM ownership
and Xt translation exclusions in `vt_input.c`; keep pointer-driven selection,
paste, hyperlinks, mouse reporting, and local scroll actions in
`vt_interaction.c`. Encoded input delivery uses the widget's single
`XtNinputCallback` boundary.

Menu dispatch uses typed identifiers, and `main.c` delegates startup phases to
small helpers with one cleanup path for normal exit and partial initialization.

Keep the current pixel geometry in the terminal boundary until another caller
needs a different shape. At that point, prefer one geometry-setting operation
over continuing to add width, padding, and screen dimensions to individual
selection and mouse calls.

The ignored Ghostty checkout is not an ordinary Meson source tree.
`tools/fetch-libghostty` resolves and detaches it at the exact pinned commit;
Meson tracks the checkout's `.git/HEAD` plus the fetch and build scripts as
custom-target dependencies, while Zig's cache decides whether rebuilding the
archive has real work to do. Keep those dependencies synchronized. Dropping
one can silently pair headers from a newly fetched Ghostty revision with an
archive from the old revision.

The render-state dirty flag describes cell damage, not every visual-state
change. In particular, cursor movement can arrive with zero dirty rows. The
terminal adapter must still deliver begin/end callbacks and current cursor
state, while the VT widget treats its frame cache as the last pixels actually
painted. Do not infer cursor or viewport damage solely from dirty cells.

The frame cache is valid only for the widget grid that produced it. Invalidate
it before publishing new rows or columns; otherwise Expose can map an old-grid
snapshot onto new geometry and leave stale rows painted at the wrong cells.
Resize updates the kernel PTY first and libghostty terminal state second within
one Xt callback, matching Ghostty's ordering, so child `SIGWINCH` redraw bytes
cannot be fed between those operations.

A kernel PTY read boundary is not an application presentation boundary. Rich
Live-style refreshes erase the old display before writing the replacement, and
Linux was observed splitting one such write after 4095 bytes. `PtyReady`
therefore drains a bounded 256 KiB burst of currently available data into
libghostty and calls `XtpVtUpdate` once. Keep the budget for Xt fairness and
keep query/reply observation ordered per fragment. `xvfb-pty-burst` pins the
no-intermediate-render behavior.

The terminal adapter already exposes SGR 2 as `XtpRenderCell.faint`. The widget
must carry that into pixels: the default xterm-compatible policy scales each
foreground RGB component to two-thirds before inverse or selection swapping.
`xvfb-colors` samples an inverse faint cell to pin the exact result. The
`faintIsRelative` resource remains unsupported; add background-relative mixing
only when that resource is implemented.

Bold style and bold color are independent. The default `boldColors: true`
promotes foreground palette indices 0–7 to the live 8–15 entries while keeping
the bold font face; `+pc` disables only the promotion. Keep this policy in the
terminal boundary because its render snapshot owns the live OSC 4 palette.
Libghostty does not retain whether a stored palette index came from SGR 30–37
or SGR 38;5, so both are currently promoted; the narrower patch-411 distinction
is recorded as transitional drift. `xvfb-colors` pins both policy settings.

Do not conflate that fixed cache lifetime bug with the Readline 8.3
wrapped-prompt regression. A 45-column OSC 133-marked prompt resized
80→38→80 ends at column 37 because Readline drops the final eight-byte
nonprinting run from its cursor calculation; the SGR control lands at column
41 after dropping a four-byte run. Bash development commit `1e9f5e10b2`
unconditionally refreshes the affected prompt metadata after `SIGWINCH`.
Revenant executes the resulting bytes correctly and must not add a terminal-side
workaround. Run `just reflow-prompt`, copy the logged top-level window ID, then
run `just reflow-resize WINDOW_ID`; one cycle is sufficient. The source
diagnosis, upstream links, version boundary, and fixed-build checks are in
`docs/reference/bash-readline-resize.md`.

libghostty exposes a resolved cursor-blink value, but xterm compatibility needs
the raw application request as a separate operand. `cursor_blink.c` therefore
observes the same PTY stream solely for DECSCUSR, DEC mode 12, their save/restore
forms, RIS, and DECSTR; libghostty remains authoritative for shape and all other
terminal state. This is deliberately a small second parser. Keep its streaming
state aligned with the pinned parser's anywhere transitions, cancellation,
empty-parameter, C1, OSC, and DCS-ignore behavior. Query replies must use state
at the query's byte position, not the final state of a coalesced PTY read. The
existing same-buffer, malformed-sequence, raw-C1, overflow, reply-rewrite, and
reset tests pin those requirements. Forced policies freeze application blink
tracking exactly where xterm's `SettableCursorBlink` does while shape remains
independently application-controlled.

## Deferred organization debt — 2026-09-02

The pre-multiplexing organization review completed the low-risk boundary work:
Ghostty selection policy is isolated from the adapter, failed render
transactions abort explicitly, font-universe types no longer live in the
widget-private header, repeated UTF-8 and widget invalidation idioms have names,
and the self-test runner and Meson source inventory are table-driven. The
remaining findings below are intentional deferrals, not release blockers by
themselves. Revisit them when work enters the named owner rather than performing
an unbounded cleanup pass.

- Before adding a second terminal handle, decide which state is per-terminal
  and which is per-window. Only then group the flat `Vt100Part` fields into
  cursor, selection, frame-cache, and input sub-structures. Grouping them first
  risks encoding the wrong lifetime. At the same boundary, replace the long
  selection geometry argument lists with one geometry/event value and decide
  whether terminal creation needs a configuration structure.
- During the next substantial font-routing change, finish removing widget
  ownership from the font stack: snapshot its resource inputs into
  `XtpFontUniverse`, pass the universe and display explicitly, return one route
  result instead of repeated out-parameters, and collapse the three parallel
  primary/wide/emoji/Han role representations. Also move the glyph-ink cache
  beside the Cairo scaled-font cache, establish one visible `FcPattern`
  ownership rule, and split consumer-specific helpers out of `font_role.c`.
- During configuration-report work, derive compiled defaults from the canonical
  Xt resource table instead of repeating string literals. Do not derive
  behavioral support merely from `XtGetResourceList`: a parsed resource is not
  necessarily implemented, so the support catalog remains an explicit claim.
- During the next option-driven process slice, add the accepted login-shell,
  terminal-name/mode, hold, map timing, message-permission, and session-logging
  behaviors from the command-line feasibility study. Review the runtime
  stub/backend PTY branch and the placement of `TERM` policy in that startup
  ownership pass. Hard-coded startup prose that duplicates defaults also
  remains cleanup debt. The pre-X scanner and font resource aliases are already
  centralized; do not reintroduce independent argv scans.
- When `vt_interaction.c` next receives material work, split X selection/paste,
  hyperlink launching, and mouse reporting into focused owners. Other local
  cleanup should follow its owning feature: table-drive the order-dependent
  default character classes, give `WarnRecord` kind-specific fields, name the
  remaining viewport/cell/frame-cache idioms, and shorten the large reporting,
  routing, drawing, and `SetValues` functions as they are changed.
- A logging-density pass remains worthwhile after behavior stabilizes. Prefer
  removing INFO narration and generated summaries over changing diagnostic
  coverage during feature work.

Two reviewed choices are deliberate. Keep `--self-test` in the installed binary
as a package smoke diagnostic; the table-driven runner addresses its structural
cost. Keep the three Ghostty callback-pointer shims typed: assignment to each
Ghostty callback typedef provides compile-time signature checking, while their
documented copy handles the option API's representation boundary. A generic
function-pointer helper would lose the useful type check.

## Implemented behavior

- Real PTY-backed shell with libghostty parsing, mode-aware basic keyboard
  encoding, query responses, bell, and title effects.
- Lossless nonblocking PTY writes: keys, paste, mouse/focus reports, and
  terminal responses share one ordered queue; `EAGAIN` arms an Xt writable
  input source until the queue drains.
- Backend-neutral terminal-mode get/set support with checked menu toggles for
  backarrow-key, NumLock-keypad, Alt-escape, autowrap, reverse-wrap,
  autolinefeed, application cursor keys, and application keypad mode.
- Progressive output rendering, a last-painted cell cache, and cursor-only
  repaint support when libghostty reports no cell damage.
- Primary-screen resize reflow, synchronized terminal/kernel PTY geometry,
  and pre-Expose invalidation of frames captured at the previous grid size.
- `saveLines` controls libghostty's line limit and clears its independent
  default byte cap when positive. Keep the large-history self-test: without
  that clearing step, `XTerm*saveLines: 16500` retains far fewer rows despite
  resolving and logging the correct X resource value. libghostty's remaining
  whole-page pruning granularity is documented rather than presented as exact.
- Xterm application and widget identities with real Athena popup menus.
- Xlib bitmap and Xft/fontconfig renderers with runtime `renderFont` toggling.
- Xft point sizes resolved with the active display and screen defaults,
  including Xft DPI; `-report-config` uses the identical font-matching path.
- Shift+keypad font selection, proportional window resizing, WM resize
  increments, and grid-preserving renderer switches.
- True color, palette terminal values, inverse, bold, underline, overline, and
  strikeout rendering, subject to the gaps recorded in the
  [roadmap](docs/maintainers/roadmap.md).
- xterm-compatible `color0` through `color15` resources with exact compiled
  defaults, all normal Xt/Xrm name/class and precedence behavior, safe
  name-to-RGB parsing independent of colormap capacity, and one atomic
  libghostty default-palette update. OSC 4 overrides the current palette and
  OSC 104 returns to the configured resource value. The self-test preserves a
  high palette index, while `xvfb-colors` covers every resource index,
  `-report-config`, RESOURCE_MANAGER and command-line forms, Xrm precedence,
  painted pixels, and constrained PseudoColor operation. `probe-colors.py`
  supplies the human query/spawn comparison.
- Full UTF-8 grapheme bytes at the renderer boundary; HarfBuzz shapes primary
  and fallback faces, including compatible adjacent non-emoji cells whose
  context spans a backend grapheme boundary. Fontconfig supplies a bounded
  fallback set for each normal, bold, italic, and bold-italic slot.
- Unicode 17 emoji presentation and role routing through `faceNameEmoji` and
  `faceNameDoublesize`, including VS15/VS16, keycaps, modifiers, flags, tags,
  and ZWJ sequences. HarfBuzz shapes complete backend graphemes and rejects
  incomplete sequence composition atomically. The persistent Cairo delegate
  renders CBDT, COLRv0, COLRv1, SVGinOT, and sbix color glyphs or real outline
  fallbacks with cell fitting and effective damage/cursor clipping. The
  backend grid remains authoritative for width: xterm-compatible legacy width
  is the default, while applications may negotiate grapheme-cluster width with
  mode 2027. Reproducible font fixtures and Xvfb tests cover routing, shaping,
  ink paths, and format behavior. General text now shares the positioned-run
  and clipping path without changing backend-owned cell widths, and SGR italic
  selects a real italic or oblique face when one is available.
- Application-selected DECSCUSR block, underline, and bar cursor presentation,
  including cursor-shape-only repaint coverage. Blinking variants and DEC mode
  12 use an Xt timer with xterm's `cursorOnTime` and `cursorOffTime` defaults;
  `cursorBlink` supports configurable `false`/`true` operands and forced
  `always`/`never` policies. `cursorBlinkXOR` matches xterm's default XOR
  composition with the separate application state; OR remains available by
  setting it false. The compiled `false`/XOR defaults are initially steady but
  honor DECSCUSR and mode 12 blink requests. Focused blocks are filled and
  unfocused blocks are outlined.
- History-safe mouse selection with visible ranges, `multiClickTime`,
  whitespace double-click, unit-preserving Button-3 extension, named X11
  selection ownership, and Button-2 paste through Ghostty's bracketed-paste
  encoder. `SELECT` follows the `selectToClipboard` resource/menu/action
  policy; explicit atoms and `CUT_BUFFER0` through `CUT_BUFFER7` are honored in
  action order. Multi-click timing follows xterm's release-to-next-press
  interval.
- xterm-compatible Unicode character classes for word selection, including
  the `charClass` resource and `-cc` range syntax. This is implemented above
  Ghostty's binary word-boundary API so distinct punctuation classes remain
  distinct.
- xterm-style `CHARDRAWN`/past-end selection semantics: written blanks do not
  merge into the undrawn row suffix, double-clicking the suffix yields no word,
  and extending into an undrawn suffix or untouched row selects that region as
  a unit.
- Selection autoscroll for Button-1 drag and Button-3 extension. The terminal
  engine tracks the stationary endpoint while the viewport moves, and a
  multi-page regression requires exact, nonduplicated text through the oldest
  retained row.
- `scrollTtyOutput: false` preserves the viewport's distance from the live
  bottom as new history arrives instead of pinning an absolute history row;
  `true` continues to jump to the newest screen.
- Application mouse tracking with terminal-selected X10, normal, button, and
  any-event modes and X10, UTF-8, SGR, URxvt, and SGR-pixel formats. Press,
  release, motion, modifier, and wheel input is encoded through libghostty;
  Shift overrides reporting for local selection/scrollback and Ctrl+buttons
  retain the popup menus.
- Application focus reporting through libghostty when the child enables DEC
  private mode 1004. Real X focus transitions emit exactly one `CSI I` or
  `CSI O`; ordinary shells receive no focus bytes.
- OSC 8 hyperlink targets exposed through the backend-neutral terminal API.
  Shift-hover underlines linked cells and Shift+Button 1 directly launches
  only HTTP or HTTPS targets with `xdg-open`; other schemes are deliberately
  inert. Ordinary selection and the Ctrl+button menus keep their established
  gestures.
- Patch-411 default VT bindings are audited in
  `docs/compatibility/default-bindings.md`. Shift+Insert now owns the key event
  and pastes `SELECT` instead of also emitting modified Insert; paging and
  font-selection keys likewise remain local-only. Four action-level gaps are
  recorded explicitly.
- Consolidated `-report-config` inventory for resources, app-defaults,
  inherited Xt/Athena resources, translations, and actions.
- Structured diagnostics with a warning-and-above default, `-log` severity
  selection, xterm-compatible `-/+debug` aliases, and CPU flamegraph tooling.
- Compositor-backed default-background opacity with opaque ink, cursor,
  selection, decoration, and explicit-background policy.
- Independent widget `reverseVideo`, terminal-wide DECSCNM, and per-cell SGR 7
  composition, including forced full repaint on DECSCNM transitions and
  opacity-aware color provenance.

The authoritative missing-capability order is the
[roadmap](docs/maintainers/roadmap.md). Basic scrollback, its Xaw scrollbar,
historical and named selection, cut-buffer fallback, and middle-button paste
are wired, along with application mouse and focus reporting.
Selection-retention policy and Kitty graphics remain incomplete even though
libghostty exposes much of the required machinery.

## Project identity

The project and repository are named Revenant. The installed binary is
`revenant`; `xterm+` remains an installed compatibility symlink. The `XTerm`
application class, `xterm` instance, `vt100` widget name, resources, menus, and
translations remain intentional compatibility surfaces and must not be renamed
as cosmetic cleanup.

Keep product branding at the outer product boundary: documentation and website
prose, Meson build/install metadata, desktop and release/package assets, and
justfile recipes that must name the concrete `build*/revenant` output. Internal
C source, tools, test programs, fixtures, diagnostics about implementation
roles, temporary names, and variable names use `XTP` or `xterm+`. Meson owns
`program_name = 'revenant'`, exports it through the generated
`XTP_PROGRAM_NAME`, names the executable and install symlink target from that
setting, and reports it in the configuration summary. Do not reintroduce a
literal product name into `src/` to print `--version` or for another runtime
purpose. `tools/check-internal-branding` is a normal Meson test and makes this
boundary executable rather than conventional.

The justfile is the intentional exception among internal-looking files: a
recipe that launches a build artifact must know that `./build*/revenant` is the
concrete path. Generic tools receive that path from the justfile or discover
the sole installed Meson executable; they must not hardcode it. Keep
release-specific helpers under `packaging/`, not under `tools/`, when their
purpose requires product and artifact names.

A useful product lens is that xterm is unusually broad in historical terminal
protocols but deliberately narrow as a modern terminal application. Patch 411
implements a deep DEC and xterm-specific tail, including selectable VT levels,
DECDHL double-size lines, DECUDK, printer controls, rectangle and locator
operations, Tektronix 4014, and xterm query and keyboard extensions; configured
builds also provide sixel and ReGIS. That does not make xterm a superset of
modern terminals. Its gaps extend beyond window tabs and native transparency:
there is no resize reflow, OSC 8 hyperlink model, Kitty keyboard or graphics
protocol, general shaping and color-font fallback pipeline, built-in scrollback
search, split or profile interface, live configuration reload, or native
Wayland frontend. Window-manager opacity can affect an xterm window, but that
is not an xterm background/transparency feature.

This split helps define possible Revenant work without turning it into a promise.
Revenant already promotes resize reflow, OSC 8 links, current mouse and focus
reporting, and the Kitty keyboard protocol through an X11/Athena frontend. It
still needs the missing and Partial items in the Ghostling parity checklist,
and it does not yet reproduce all of xterm's historical protocol tail. Future
daily-driver ideas include scrollback search, richer font fallback and shaping,
Kitty graphics, and possibly tabs, splits, or profiles if they can be added
without discarding the X11 resource, translation, menu, and window-manager
contract.
Native Wayland and a wholesale GPU-shell redesign are not implied by this idea;
the current project remains intentionally X11.

Opacity is implemented as compositor-backed 32-bit ARGB rendering with the
straightforward `XTerm*backgroundOpacity` resource. Alpha applies to the
default terminal background while explicit cell backgrounds, selections,
cursor presentation, and menus remain legible; startup falls back cleanly to
an opaque visual when compositing is unavailable. Do not add urxvt-style
root-pixmap pseudo-transparency, `inheritPixmap`, desktop wallpaper copying,
tinting, or shading. The project is a time capsule carried forward, not a
recreation of obsolete X11 rendering hacks.

### Reverse video and opacity invariants

Keep the three sources of reversed presentation distinct even though their
visible color result has parity/XOR behavior:

- the `-rv`/`reverseVideo` resource and `Enable Reverse Video` menu action are
  widget-level swaps of the configured default foreground and background;
- DECSCNM (`CSI ? 5 h`/`CSI ? 5 l`) is a screen-wide terminal rendering mode;
- SGR 7 is per-cell inverse styling.

Do not implement DECSCNM by permanently rewriting stored cell colors or by
folding it into SGR 7. SGR 7 while DECSCNM is enabled must render with normal
color polarity because the two rendering inversions cancel. The order and
provenance still matter for opacity: SGR 7 by itself moves the default
foreground into the cell background and that background is opaque ink, as the
existing Xvfb opacity test asserts. DECSCNM by itself changes the screen's
effective default-background color, but that screen background remains
translucent. An SGR 7 cell under DECSCNM is double-inverted and its restored
default background is translucent. This opaque-to-translucent change for an
SGR 7 cell across `?5h`/`?5l` is intentional and needs a test comment so it is
not later "fixed" from a screenshot alone. The widget-level `-rv` swap changes
which concrete color the default background names; it must not detach opacity
from the background surface or require special slider behavior.

The implementation now carries `GHOSTTY_MODE_REVERSE_COLORS` as explicit
frame-level state while preserving `GhosttyStyle.inverse` as the independent
SGR 7 bit. A DECSCNM transition forces a full repaint, so cells the application
does not rewrite still change. The X11 renderer resolves only default colors
through DECSCNM; explicit RGB and palette colors remain concrete. It determines
the final background source before applying alpha, retaining an opaque form for
cursor and selection presentation.

The RGB-aware Xvfb opacity scenario pins the complete composition: untouched
cell repaint, premultiplied reversed background, SGR 7 cancellation under
DECSCNM, explicit orange preservation, complete `?5l` restoration, and
64-percent opacity under widget `-rv`. The backend self-test separately checks
frame state and full-repaint transitions. `tools/probe-reverse-video.sh` is the
Enter-gated human comparison for SGR 7, DECSCNM, and the widget/menu toggle.

Preserve the established rendering rules: derive translucent pixels from the
retained opaque background, use `PictOpSrc` for background fills, keep Xft
glyph compositing as `Over`, and keep explicit SGR backgrounds, cursor,
selection, decorations, and other ink opaque. Do not reopen this composition
from screenshot intuition alone; change it only with corresponding pixel-level
coverage.

## v0.5.0 early-access plan

The v0.5 feature milestone is the first build intended for people beyond the
maintainer; release-pipeline-only checkpoints do not lower that product bar.
It is a reconnaissance and early-access release, not the announcement release.
Its job is to touch the important compatibility and onboarding surfaces, fix
the cheap or immediately harmful gaps, and leave an evidence-backed inventory
for v0.6. Crashes, lost or reordered PTY bytes, grid-width drift, corrupt
history or selection, and silent compatibility breaks remain release blockers.

The release thesis is: **make Revenant useful and understandable to its first
outside users while discovering, rather than prematurely completing, the work
needed for an announcement.** This is a smaller and more exploratory scope than
the v0.6 hard gates below.

### Release scope

Work in this order. Review each surface only deeply enough to establish its
real state. Fix a finding in v0.5 when it is small, blocks ordinary use, makes
the early-access experience misleading, or is prerequisite to another scoped
item. Otherwise record it under v0.6 and keep moving.

1. **Protect the daily-driver path.** Continue using Revenant for ordinary
   shells, editors, tmux, SSH, selection/paste, hyperlinks, resize, alternate
   screen, and deep scrollback. Close reproducible crashes, byte loss, input
   errors, stale painting, reflow failures, and history or selection corruption
   before feature work. Finish the obscured-window, DECSTBM, alternate-screen,
   X request ordering, and throughput checks in
   `docs/compatibility/rendering-review.md`.
2. **Perform a cursory Ghostling parity re-audit.** Review the current Ghostling
   checkout against the libghostty commit selected for v0.5 rather than carrying
   the 2026-08-24 comparison forward by assumption. Exercise every advertised
   capability at the visible X11 boundary, confirm the existing claims with
   representative evidence, and update both parity matrices. Keep actual status
   (`Present`, `Partial`, or `Missing`) separate from release scope. Kitty
   graphics remains Missing and is deliberately deferred from v0.5, so this
   release must not claim complete Ghostling parity or the existing MVP gate.
   The review should also decide whether Ghostling has added or removed a
   capability since the original inventory.
3. **General shaping/fallback and italic—not emoji-only rendering. Completed.**
   See the user guide at `docs/configuration/fonts.md` and the implementation
   contract at `docs/maintainers/font-resolution.md`. The durable invariants
   are: libghostty commits cell width before font lookup; entry 1 of the primary
   Xft role alone defines fixed cell geometry; atom routing is normal-canonical;
   compatible adjacent atoms shape together; and styles never change the
   serving family. Primary, doublesize, emoji, and Han slots use xterm's
   characterized two-entry chains, followed by numbered user fallbacks and
   optionally unnamed system candidates under the inherited governors.

   Non-primary faces normalize to primary metrics. Exact Han IVS misses and
   exhausted ink-bearing clusters produce deterministic per-cell tofu. The
   width-keyed LRU cache, schema-1 NDJSON routing report, and transactional
   font-universe reload are implemented and covered by focused tests. The
   patch-411 T0 runner replays the blessed 32-case deposition; changes to slot
   grammar, governor behavior, or intentional style drift must update the
   compat evidence rather than relying on memory.

   Preserve the Xlib bitmap/BDF world when `renderFont: false`. The remaining
   LM-04/05 gap is DEC double-height/double-width row recognition: the current
   libghostty stream exposes no row-size state. Add that state to the backend
   API before implementing the renderer behavior; never reconstruct it by
   rescanning PTY bytes in the UI. Keep shaped-run caches separate from family
   routing, preserve variable-font coordinates in every engine, and retain the
   existing emoji routing/width, atomicity, color, clipping, and fixture gates.
4. **Review the command line and fix its obvious dishonesty.** Use
   `docs/compatibility/command-line-feasibility.md` as the inventory and compare
   behavior with the patch-411 xterm oracle. Reject an unknown option such as
   `-asdfzxcv` with the invoked program name, xterm-style `bad command line
   option` text, usage, and a failing status instead of silently opening a
   window. Add single-dash `-help` and `-version`, preserve `-e` as the boundary
   after which arguments belong to the child, and test output streams and exit
   statuses. Add aliases for behavior that is already genuinely supported.
   Triage the remaining inventory into small v0.5 work and the v0.6 hard gate;
   Xt parsing alone is not support.
5. **Implement xterm session logging.** Add `-/+l`, `-lf`, the `logFile` and
   `logInhibit` resources, and the `logging` main-menu action as one PTY-output
   tee with shared state and error handling. Compare start, stop, append or
   truncate, default filename, permissions, and failure behavior with xterm.
   Logging must be off by default, create files safely, never reorder or block
   terminal delivery, and report write failures. Keep this user-requested
   session transcript distinct from Revenant's structured diagnostic `-log`
   severity; neither option may accidentally enable the other.
6. **Add `revenant -welcome`.** Make it a local, self-contained tutorial and
   installation audit, not a web page and not a substitute for the automated
   test suite. It should identify the running version and resource identity,
   explain the inherited xterm controls and X resource model, demonstrate
   styles, wide text, shaping, emoji, links, selection, scrollback, font-menu
   changes, and width negotiation, and point to `-report-config`, logs, probes,
   documentation, and the issue-report path. Clearly label automatic facts,
   visual checks, and user actions. It must not mutate persistent
   configuration, leak sensitive environment data, or report a visual sample
   as mechanically verified when it was merely printed.
7. **Write the name/class transition plan. Completed for v0.5.** Preserve the
   seamless transition invariant: application class `XTerm`, instance `xterm`,
   and widget `vt100` remain the defaults for both `revenant` and `xterm+`.
   Explicit `-name` and `-class` now override the application and shell
   identity before `XtOpenDisplay`; Xvfb pins WM_CLASS, custom instance/class
   resource lookup and report provenance, and xterm's `-e` child-basename
   default for WM_NAME and WM_ICON_NAME. Invocation through another symlink
   does not silently change resource identity. An `argv[0]`-derived application
   identity remains a possible future migration only after existing `xterm*`
   resources have a documented path.
8. **Remaining keyboard/XIM compatibility matrix. Completed.** The exact-byte
   Xvfb matrix covers ordinary and application cursor/keypad modes,
   Shift/Ctrl/Alt/Super combinations, function and editing keys, XIM Compose,
   a remapped non-US character, and no-XIM Unicode fallback. F13
   press/repeat/release is covered under Kitty reporting with a synthetic XKB
   mapping. Keep libghostty's intentional fixterms default visible in the
   drift ledger, and turn future application failures into named fixtures.
9. **Triage a small, visible xterm-compatibility set. In progress.** `color0` through
   `color15` are implemented; examine startup cursor-shape resources,
   `clear-saved-lines`, and the remaining high-use key/action gaps. Implement
   the small pieces and move the rest into the v0.6 compatibility gate. Do not
   turn v0.5 into an exhaustive resource-catalog exercise.
10. **Review the documentation as a new user.** Start from a clean supported
    system and follow install, first launch, configuration, fonts, copy/paste,
    keyboard, troubleshooting, and removal without maintainer knowledge.
    Separate a short successful path from reference inventories, add an honest
    early-access limitations page, make xterm migration explicit, and ensure
    every command and resource example still works. `-welcome`, the manual,
    website, package metadata, and `-report-config` should use the same names
    and explanations.
11. **Make early access supportable.** Keep default startup quiet and make
   `--version`, `-report-config`, severity-selected logs, probes, and known
   limitations sufficient for a useful bug report. Verify installation,
   upgrade, shell exit, desktop entry, resources, fonts, menus, and uninstall
   from the tarball, Debian package, RPM, and Arch Linux package. The release
   notes must call the release early access and distinguish missing capability
   from known defects.

### Checkpoint gates

Before publishing the v0.5 early-access feature release:

- `just check-all` passes with strict GCC, strict Clang, AddressSanitizer, the
  stub backend, Xvfb, and the reproducible font fixtures; every package job
  runs the same relevant integration suites.
- The selected Ghostty source is one reviewed, exact commit;
  `tools/fetch-libghostty` rejects moving references. While the project targets
  early 1.4 work before an upstream tag exists, keep the reviewed commit pin
  and identify in the release notes that it came from an unreleased branch.
- The live xterm geometry/font-menu comparison passes under VNC with curated
  resources. Keep this an explicit side test because it depends on a separately
  installed xterm oracle; do not pretend it is a hermetic normal test.
- The emoji follow-through acceptance passes: ordinary complex and combining
  text uses the generalized shaping/fallback path, the bitmap/BDF renderer is
  unchanged, and the complete emoji routing, format, atomicity, clipping,
  fitting, and two-regime width matrix remains green.
- The remaining keyboard/XIM compatibility matrix passes its exact PTY-byte
  fixtures, including application modes, modifiers, composition, and the
  maintained non-US layout case.
- The fresh Ghostling review and command-line triage are recorded; the
  PTY-session logging tests and `-welcome` smoke/audit checks pass. The
  checklist and xterm differences ledger agree with shipped behavior and
  explicitly identify deferred Kitty graphics; v0.5 is not advertised as full
  Ghostling parity.
- Unknown options fail visibly, `-help` is useful without an X display, and
  `-e` preserves arbitrary child arguments. The accepted name/class plan pins
  the v0.5 behavior and its future migration criteria.
- A newcomer can install one published package, complete the welcome path,
  apply a minimal X resource configuration, and produce the documented
  diagnostic information without consulting maintainer notes.
- There are no open release-blocking daily-driver regressions or sanitizer
  findings in exercised code. Package smoke tests report the tag-derived
  version, and the published assets and provenance are verified using the
  maintained release procedure.

Kitty graphics remains Missing, so v0.5 is an explicitly limited early-access
release rather than the MVP promised by the current Ghostling gate. Do not
weaken the checklist or imply that parser support renders images. A cursory
review is complete when it produces trustworthy status and bounded follow-up;
it does not require resolving every finding before v0.5 ships.

### Optional work, not release blockers

- Remaining selection-retention, ICCCM text-target, paste-control, visual-bell,
  urgency, scrollbar-style, and insensitive-menu work should be driven by
  actual use or a small compatibility slice.
- Additional shaping scripts, XIM layouts, and color-font versions are valuable
  matrix expansion after each underlying path has one adversarial acceptance
  fixture.

### Preserve for future releases

The broader ideas remain project direction rather than v0.5 promises:

- Kitty graphics and scrollback search are expressly punted from v0.5, and
  neither is a v0.6 announcement gate. They remain possible later features,
  not work to squeeze in after scope freeze.
- Tabs, splits, profiles, and live configuration reload remain open questions.
  They are acceptable only if they respect Xt/X11 resources and the
  window-manager contract rather than turning Revenant into an unrelated shell.
- The deeper DEC/xterm tail, sixel, ReGIS, Tektronix 4014, printer controls,
  locator operations, and exhaustive command-line/resource compatibility;
- The snapshot/raw-byte multiplexer design described below, after the intended
  upstream snapshot interface exists.

Native Wayland support and a wholesale GPU renderer are anti-goals for
Revenant. They would erase the project's intentional X11/Xt/Athena identity and
should be pursued, if desired, as a different frontend or project rather than
used to redirect this one. Normal use of existing X11 acceleration APIs does
not violate this rule.

## v0.6 announcement-release sketch

v0.6 is the capture point for work that proves too large for the v0.5 survey.
Unlike v0.5, it is intended to support a deliberate public announcement. Its
scope should be refined from actual v0.5 findings, but the following are hard
gates rather than aspirations:

1. **The Ghostling comparison is current and the in-scope parity work is
   complete.** Every advertised capability has maintained evidence and an
   honest status at the visible X11 boundary. All non-excepted announcement
   items are Present. Kitty graphics may remain the explicit Missing exception;
   parser state alone is insufficient, and announcement language must say
   "Ghostling parity except Kitty graphics" rather than claim full parity.
2. **Menus and command line are honest and compatibility-reviewed.** Every
   patch-411 menu entry and command-line option is implemented and tested,
   deliberately insensitive/rejected, or explicitly documented as an
   intentional difference. Supported entries match the xterm oracle, unknown
   options fail, and no accepted-but-inert surface is advertised as working.
3. **The name/class policy is implemented.** Invocation names, `-name`,
   `-class`, WM_CLASS, app-default lookup, existing `xterm*` resources, and
   storage of generated customization have one documented and tested
   transition model.
4. **The newcomer path is release quality.** Installation, `-welcome`, the
   manual and website, configuration examples, migration guidance, known
   limitations, diagnostics, package removal, and issue reporting agree and
   have been followed successfully from clean systems.
5. **Daily-driver confidence supports the announcement.** The maintained
   shell, editor, multiplexer, SSH, input, resize, rendering, selection,
   scrollback, font, and packaging matrices pass, and known serious defects are
   resolved or explicitly judged incompatible with announcing.

These gates demand complete classification and honest behavior, not wholesale
implementation of xterm's historical tail. v0.5 findings should flow into this
section as bounded tasks. If a v0.5 item starts expanding, moving its completion
here is the normal scope valve, not a failure of the earlier release.

Do not interpret the Ghostling comparison as the finish line or as authority
over explicit product exceptions. It is the modern engine-integration floor
for capabilities kept in scope; xterm remains the behavioral oracle for the
daily-use interaction contract. When ordering similarly sized work, prefer
changes that let multiple people run Revenant for real work and produce useful
evidence for the next decision.

## Compatibility discipline

- xterm compatibility is judged at the visible X11 boundary, not by internal
  similarity.
- A resource is supported only when its behavior exists; successful Xt parsing
  alone is insufficient.
- Menu items remain insensitive until behavior and compatibility tests exist.
- Keep `-report-config`, the catalogs, menu sensitivity, translations,
  the [xterm differences ledger](docs/compatibility/drift.md), and the
  [roadmap](docs/maintainers/roadmap.md) synchronized.
- When moving beyond patch 411, update all four compatibility artifacts and
  record the new oracle together.
- Do not install this repository's `XTerm` app-default over a distributor's
  xterm package without a conflict-free packaging strategy.

## Style

The [C style guide](docs/maintainers/style.md) is the adopted classic-X style.
Use the checked-in `.clang-format` for C sources and headers. Functions use
mixed case, public interfaces retain the `Xtp` prefix, important types are
capitalized, and eight-column indentation gives the code its traditional
vertical rhythm. Keep formatting-only changes separate from behavioral changes
when practical.

## Verification

The `justfile` creates or reconfigures private build directories and never
touches `build/`. Run the complete pre-push suite with:

```sh
tools/fetch-libghostty              # once, or whenever the tracked reference advances
tools/stage-font-fixtures           # once, or whenever fixture inputs advance
just check-all
```

The compiler/backend checks can also be run independently:

```sh
just test-gcc
just test-clang
just test-asan
just test-stub
just check                         # strict Revenant and TDN documentation
```

AddressSanitizer is a maintained gate rather than an ad-hoc build directory.
`just test-asan` configures the ignored `build-agent-asan/` directory with
Clang, `-Db_sanitize=address`, `-Db_lundef=false`, the libghostty backend, and
required Xvfb tests. It runs the complete Meson suite and then
`tools/check-release-tests`, so missing font fixtures or skipped integration
tests fail the target. Run it outside ptrace- or tracing-based sandboxes;
LeakSanitizer cannot operate correctly under those environments.

`.github/workflows/test.yml` runs the same AddressSanitizer gate plus warning-free
GCC, Clang, and stub-backend jobs for pushes to `master`, pull requests, and
manual dispatch. It selects the pinned libghostty commit, restores the shared
Zig and font-fixture caches, requires all registered tests to pass without
skips, and is intentionally separate from the manual package-release workflow.

Useful runtime checks:

```sh
./build-agent-gcc/revenant -log debug 2>revenant.log
./build-agent-gcc/revenant -report-config
./build-agent-gcc/xtp-send-font-keys WINDOW_ID + 4
./build-agent-gcc/xtp-send-font-keys WINDOW_ID - 4
just probe-reverse-video
just probe-emoji --no-pause
just probe-fonts --no-pause
just probe-keymodes --kitty-only
just xterm-font-compat build-agent-gcc
just resize-loop WINDOW_ID 8 20
just reflow-prompt
just reflow-resize WINDOW_ID
```

The current self-test has focused backend checks for rendering, cursor state,
modes, selection and deep scrollback, tty-output viewport anchoring, mouse and
focus encoding, resize, PTY setup, log-level parsing, and write backpressure
without byte loss, but it remains one in-process harness. When Xvfb is
available, Meson also runs integration suites for the reproducible font
baseline; emoji routing, shaping, width regimes, and color-font formats;
opacity/reverse-video pixel policy and logging thresholds; named selections,
cut buffers, and OSC 8 launch policy; legacy/fixterms keyboard delivery; and
Kitty keyboard press, repeat, and release. Release package configurations use
`-Dxvfb-tests=enabled`, which makes missing Xvfb or libghostty an immediate
configuration error, and `tools/check-release-tests` rejects skipped suites.
The live xterm font/geometry oracle remains an explicit side test. Split the
remaining harness into focused tests and grow Xvfb coverage; do not treat any
one suite alone as evidence of full UI compatibility.

The normal full matrix currently contains 29 tests for each libghostty build
and 7 for the stub build. One of those is `internal-branding`, which scans
`src/`, `tools/`, and `tests/`; a count drop or a newly skipped check is a
failure to investigate rather than an expected consequence of changing build
options. The generated font fixture staging tree now contains
`XtpSyntheticSbix.ttf`; rerun `tools/stage-font-fixtures` after changing its
generator or manifest rather than retaining the old product-branded fixture.

## Performance and longer-term direction

Keep debug logging off for throughput measurements. Compare xterm, Ghostty,
Ghostling where useful, and Revenant under equivalent optimized conditions.
Separate parser, PTY, X drawing, X synchronization, and renderer costs. Use
`tools/flamegraph` for representative output workloads.

The longer-term multiplexer direction is compatible libghostty snapshots plus
raw PTY-byte fanout, asynchronous history, independent client viewports, and
resynchronization by fresh snapshot. Do not freeze a private protocol before
the intended upstream design is available. Keeping terminal state and
viewport/selection ownership behind `terminal.h` is the useful preparation.
