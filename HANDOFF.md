# xterm+ maintainer guide

This is the durable continuation brief for maintainers. Current feature
priorities and the Ghostling capability comparison live in the
[roadmap](docs/maintainers/roadmap.md); external checkout roles live in the
[upstream reference guide](docs/maintainers/upstream.md). Avoid recording an uncommitted
file list or a single transient commit as project state here.

## Mission

xterm+ is a faithful, sustainable X11 replacement for xterm's visible user
experience with a modern terminal engine underneath. It has two product bars:

- preserve the xterm/Xt/Athena skin and compatibility contract;
- promote every advertised Ghostling capability through the X11 boundary,
  then exceed that floor with the xterm daily-driver experience.

The second bar corrects an earlier imbalance. Pixel similarity, resources,
menus, and geometry remain essential, but they do not justify leaving
scrollback, mouse protocols, focus reporting, graphics, or other baseline
terminal capabilities unwired.

The [Ghostling parity checklist](docs/compatibility/ghostling-parity.md) is an
explicit MVP gate. Kitty keyboard is Present with progressive flags, stack,
legacy fallback, modifiers, composition, and real press/repeat/release events
covered end to end. Kitty graphics remains Missing until images render;
libghostty parser state alone does not count as promotion. Maintainer-reported
keyboard application failures should continue to become named regression
fixtures.

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
2. regressions found during real xterm+ use, especially input, resize,
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

xterm+ does not compile, link, or embed xterm's terminal implementation. The
PTY/event loop, Xt widget, X11 renderer, diagnostics, menu wiring, and
libghostty adapter are xterm+ code. `libghostty-vt` owns parsing, terminal
state, key and mouse encoding, query responses, resize reflow, history,
selection primitives, and graphics protocol state.

The patch-410 xterm tree in `/home/toppk/workspace/xterm` remains the visible
behavioral oracle. The checked-in compatibility material is:

- `data/app-defaults/XTerm`;
- `compat/xterm-410-resources.tsv`;
- `compat/xterm-410-app-defaults.txt`;
- `compat/xterm-410-actions.txt`;
- widget/menu names, defaults, translations, and reconstructed behavior.

Those portions are covered by `LICENSES/xterm.txt`. Consult xterm to reproduce
external behavior; do not copy its terminal engine wholesale.

See the [upstream reference guide](docs/maintainers/upstream.md) for the ignored Ghostty,
Ghostling, xterm snapshot, and
xterm.dev checkouts and their update rules. Ghostling is now the minimum
functional comparison, while xterm remains the UI and compatibility oracle.

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

The ignored Ghostty checkout is not a Meson input tree. The libghostty custom
target is therefore always considered stale and delegates incremental rebuild
decisions to Zig's cache. Removing that behavior can silently pair headers
from a newly fetched Ghostty revision with an archive from the old revision.

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

Do not conflate that fixed cache lifetime bug with the Readline 8.3
wrapped-prompt regression. A 45-column OSC 133-marked prompt resized
80→38→80 ends at column 37 because Readline drops the final eight-byte
nonprinting run from its cursor calculation; the SGR control lands at column
41 after dropping a four-byte run. Bash development commit `1e9f5e10b2`
unconditionally refreshes the affected prompt metadata after `SIGWINCH`.
xterm+ executes the resulting bytes correctly and must not add a terminal-side
workaround. Run `just reflow-prompt`, copy the logged top-level window ID, then
run `just reflow-resize WINDOW_ID`; one cycle is sufficient. The source
diagnosis, upstream links, version boundary, and fixed-build checks are in
`docs/reference/bash-readline-resize.md`.

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
- Full UTF-8 grapheme bytes at the renderer boundary; the Xft primary face can
  draw supported glyphs without fallback or shaping.
- Application-selected DECSCUSR block, underline, and bar cursor presentation,
  including cursor-shape-only repaint coverage. Blinking variants and DEC mode
  12 use an Xt timer with xterm's `cursorOnTime` and `cursorOffTime` defaults;
  `cursorBlink` supports configurable `false`/`true` defaults and forced
  `always`/`never` policies. The compiled `false` default is steady but honors
  explicit application blink requests. Focused blocks are filled and unfocused
  blocks are outlined.
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
- Patch-410 default VT bindings are audited in
  `docs/compatibility/default-bindings.md`. Shift+Insert now owns the key event
  and pastes `SELECT` instead of also emitting modified Insert; paging and
  font-selection keys likewise remain local-only. Four action-level gaps are
  recorded explicitly.
- Consolidated `-report-config` inventory for resources, app-defaults,
  inherited Xt/Athena resources, translations, and actions.
- Structured diagnostics and CPU flamegraph tooling.

The authoritative missing-capability order is the [roadmap](docs/maintainers/roadmap.md). Basic scrollback,
its Xaw scrollbar, historical and named selection, cut-buffer fallback, and
middle-button paste are wired, along with application mouse and focus
reporting. Selection-retention policy and Kitty graphics remain incomplete
even though libghostty exposes much of the required machinery.

## Ideas and possible future identity

`Revenant` is a possible future project name. It fits the idea of bringing the
xterm form and interaction model back with a modern engine, but it is only an
idea, not an approved rename. Before adopting it, check name and package
collisions, decide whether continuity with the `xterm+` compatibility promise
is more valuable, and treat the rename as a separate product decision rather
than feature work.

A useful product lens is that xterm is unusually broad in historical terminal
protocols but deliberately narrow as a modern terminal application. Patch 410
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

This split helps define possible xterm+ work without turning it into a promise.
xterm+ already promotes resize reflow, OSC 8 links, current mouse and focus
reporting, and the Kitty keyboard protocol through an X11/Athena frontend. It
still needs the missing and Partial items in the Ghostling parity checklist,
and it does not yet reproduce all of xterm's historical protocol tail. Future
daily-driver ideas include scrollback search, richer font fallback and shaping,
Kitty graphics, and possibly tabs or opacity if they can be added without
discarding the X11 resource, translation, menu, and window-manager contract.
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

## Current phase and immediate continuation order

The project is in private stabilization and daily-driver evaluation, not in
announcement preparation. The `v0.2.0` build is a useful packaging milestone;
a successful workflow is not evidence by itself that xterm+ is ready to be
recommended publicly. The maintainer intends to use xterm+ for several days
and still has concerns to surface and characterize. Treat those observations
as the highest-value input during this phase.

The current modern-capability count is 8 Present, 3 Partial, and 1 Missing in
the [Ghostling parity checklist](docs/compatibility/ghostling-parity.md).
Italic rendering, font fallback/emoji acceptance, and the complete X11 key
matrix remain Partial; Kitty graphics remains Missing. The default fixterms
keyboard behavior is also a major, intentional drift from xterm and must be
prominent wherever replacement compatibility is discussed. Do not announce
xterm+ as MVP, fully Ghostling-equivalent, or an unqualified drop-in upgrade
while those statements are false.

Continue in this order:

1. **Dogfood the terminal.** Use xterm+ for normal shell, editor,
   multiplexer, remote-session, selection/paste, hyperlink, resize, and
   scrollback work. Record each concrete concern with reproduction steps and
   diagnostics, determine the owning layer, and prioritize crashes, byte
   loss, input errors, stale rendering, and history/selection corruption.
2. **Finish the unsettled renderer checks.** Follow
   `docs/compatibility/rendering-review.md` for obscured or off-screen
   scrolling, DECSTBM and alternate-screen output, X request ordering, and
   scroll throughput. The correctness-first renderer and cache fixes are in
   place, but those manual acceptance cases remain relevant before broad use.
3. **Exercise the v0.2 artifacts without treating them as a launch.** Let the
   workflow finish, install the tarball, Debian package, and RPM in clean
   environments where available, and verify startup, shell exit, resources,
   fonts, menus, and `--version`. Resolve the top-level license for xterm+'s
   own code before a public release; the packaging metadata currently calls
   its MIT declaration a placeholder. Pin the Ghostty 1.4 tag when it exists
   so release jobs no longer resolve moving `main` revisions independently.
4. **Reassess announcement scope after dogfooding.** Before announcing, write
   concise release notes and an honest known-limitations list, rerun
   `just check-all`, verify the published manual, and make an explicit owner
   decision about whether the message is an early preview or an MVP claim. A
   preview may document unfinished capability; the existing MVP claim cannot.
5. **Close the declared MVP gate.** Promote italic rendering, Unicode/font
   fallback acceptance, the remaining X11 keyboard matrix, and Kitty graphics
   with focused backend and Xvfb coverage. Continue the remaining
   selection-retention and paste-control policies as daily use exposes their
   value; do not let low-impact compatibility inventory displace observed
   correctness problems.

Do not interpret Ghostling parity as the finish line. Ghostling is the minimum
engine-integration floor; xterm remains the behavioral oracle for the daily-use
interaction contract. When ordering similarly sized work, prefer changes that
let the maintainer run xterm+ for real work and expose the next correctness
problem quickly.

## Compatibility discipline

- xterm compatibility is judged at the visible X11 boundary, not by internal
  similarity.
- A resource is supported only when its behavior exists; successful Xt parsing
  alone is insufficient.
- Menu items remain insensitive until behavior and compatibility tests exist.
- Keep `-report-config`, the catalogs, menu sensitivity, translations,
  the [xterm differences ledger](docs/compatibility/drift.md), and the
  [roadmap](docs/maintainers/roadmap.md) synchronized.
- When moving beyond patch 410, update all three compatibility catalogs and
  record the new oracle together.
- Do not install this repository's `XTerm` app-default over a distributor's
  xterm package without a conflict-free packaging strategy.

## Style

The [C style guide](docs/maintainers/style.md) is the adopted classic-X style. Use the
checked-in `.clang-format`
for C sources and headers. Functions use mixed case, public interfaces retain
the `Xtp` prefix, important types are capitalized, and eight-column indentation
gives the code its traditional vertical rhythm. Keep formatting-only changes
separate from behavioral changes when practical.

## Verification

The `justfile` creates or reconfigures private build directories and never
touches `build/`. Run the complete pre-push suite with:

```sh
tools/fetch-libghostty              # once, or whenever the tracked reference advances
just check-all
```

The compiler/backend checks can also be run independently:

```sh
just test-gcc
just test-clang
just test-stub
just check                         # strict xterm+ and TDN documentation
```

Useful runtime checks:

```sh
./build-agent-gcc/xterm+ -debug 2>xterm-plus.log
./build-agent-gcc/xterm+ -report-config
./build-agent-gcc/xtp-send-font-keys WINDOW_ID + 4
./build-agent-gcc/xtp-send-font-keys WINDOW_ID - 4
just resize-loop WINDOW_ID 8 20
just reflow-prompt
just reflow-resize WINDOW_ID
```

The current self-test has focused backend checks for rendering, cursor state,
modes, selection and deep scrollback, tty-output viewport anchoring, mouse and
focus encoding, resize, PTY setup, and write backpressure without byte loss,
but it remains one in-process harness. When Xvfb is available, Meson also runs
an X11 integration test which drags across real terminal cells, verifies the
selected bytes through a separate X client, checks PRIMARY versus CLIPBOARD
resolution for both `selectToClipboard` policies, checks `CUT_BUFFER0`, and
uses a fake `xdg-open` to prove that Shift-click launches an HTTP OSC 8 target
while leaving a `file:` target inert.
Split the remaining harness into focused tests and grow Xvfb coverage; do not
treat either test alone as evidence of full UI compatibility.

## Performance and longer-term direction

Keep debug logging off for throughput measurements. Compare xterm, Ghostty,
Ghostling where useful, and xterm+ under equivalent optimized conditions.
Separate parser, PTY, X drawing, X synchronization, and renderer costs. Use
`tools/flamegraph` for representative output workloads.

The longer-term multiplexer direction is compatible libghostty snapshots plus
raw PTY-byte fanout, asynchronous history, independent client viewports, and
resynchronization by fresh snapshot. Do not freeze a private protocol before
the intended upstream design is available. Keeping terminal state and
viewport/selection ownership behind `terminal.h` is the useful preparation.
