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

## Immediate continuation order

1. Complete rendering acceptance using
   `docs/compatibility/rendering-review.md`. The correctness-first rewrite has
   removed PTY byte sniffing, content-diff scroll guessing, unsafe window
   copies, newline-based input scheduling, and broad
   clears ahead of row painting. Cursor-only frames and cached partial Expose
   repaint are implemented. Still exercise obscured/off-screen scrolling,
   DECSTBM/alternate-screen output, xtrace ordering, and scroll throughput
   before treating the renderer as settled.
2. Reach semantic parity with the pinned Ghostling skeleton, following the
   capability matrix in the [roadmap](docs/maintainers/roadmap.md). Add focused tests with each slice so
   parity does not depend on manual demonstrations alone.
3. Make xterm+ usable as the maintainer's daily terminal. `scrollKey` and
   `scrollTtyOutput` policies are implemented as resources, command-line
   options, and VT menu toggles. Historical mouse selection, visible
   highlighting, named X11 selections, Button-3 extension, and middle-button
   paste are also implemented, including edge-drag autoscroll through deep
   history. `selectToClipboard`, `set-select`, named action arguments, and cut
   buffers are wired.
   Application mouse reporting preserves Shift selection and the Ctrl+button
   popup menus, and focus reporting follows DEC private mode 1004. Next, add
   the remaining selection-retention and paste-control policies.

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
