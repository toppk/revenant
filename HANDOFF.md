# xterm+ maintainer guide

This is the durable continuation brief for maintainers. Current feature
priorities and the Ghostling capability comparison live in `ROADMAP.md`;
external checkout roles live in `UPSTREAM.md`. Avoid recording an uncommitted
file list or a single transient commit as project state here.

## Mission

xterm+ is a faithful, sustainable X11 replacement for xterm's visible user
experience with a modern terminal engine underneath. It has two product bars:

- preserve the xterm/Xt/Athena skin and compatibility contract;
- expose at least the terminal functionality demonstrated by Ghostling when
  the pinned libghostty C API already supplies the core facility.

The second bar corrects an earlier imbalance. Pixel similarity, resources,
menus, and geometry remain essential, but they do not justify leaving
scrollback, mouse protocols, focus reporting, graphics, or other baseline
terminal capabilities unwired.

Intentional architectural or behavioral differences from xterm belong in
`DRIFT.md`. Missing features belong in `ROADMAP.md`, not in the drift ledger.

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

See `UPSTREAM.md` for the ignored Ghostty, Ghostling, xterm snapshot, and
xterm.dev checkouts and their update rules. Ghostling is now the minimum
functional comparison, while xterm remains the UI and compatibility oracle.

## Current architecture

- `src/main.c`: Xt application shell, Xlib/XCB connection, PTY event loop,
  input method, key dispatch, menus, geometry, and terminal effects.
- `src/vt_widget.c`: custom `VT100` composite widget, bitmap/Xft drawing,
  cursor, frame caching, progressive drawing, font slots, resources,
  translations, and callbacks.
- `src/terminal.h`: backend-neutral terminal state, rendering, input encoding,
  resize, and effects boundary.
- `src/terminal_ghostty.c`: `libghostty-vt` adapter.
- `src/terminal_stub.c`: UI-only backend when libghostty is disabled.
- `src/pty.c`: PTY creation, child lifecycle, lossless queued I/O, and kernel
  resize.
- `src/menus.c`: complete patch-410 popup-menu inventory; unimplemented
  entries stay visible but insensitive.
- `src/config_report.c`: resource provenance and compatibility report.
- `compat/`: machine-readable patch-410 resource, app-default, and action
  catalogs.
- `docs/`, `tdn/`, and `www/`: the xterm+ manual, the independently licensed
  Terminal Developers Network reference, and the published landing page.
- `justfile`: isolated GCC, Clang, stub, formatting, documentation, and
  combined maintainer checks.

As new capability is added, prefer focused modules over continued growth of
`main.c` and `vt_widget.c`. Keep Ghostty-specific types behind `terminal.h`.

The render-state dirty flag describes cell damage, not every visual-state
change. In particular, cursor movement can arrive with zero dirty rows. The
terminal adapter must still deliver begin/end callbacks and current cursor
state, while the VT widget treats its frame cache as the last pixels actually
painted. Do not infer cursor or viewport damage solely from dirty cells.

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
- Primary-screen resize reflow and synchronized terminal/kernel PTY geometry.
- Xterm application and widget identities with real Athena popup menus.
- Xlib bitmap and Xft/fontconfig renderers with runtime `renderFont` toggling.
- Xft point sizes resolved with the active display and screen defaults,
  including Xft DPI; `-report-config` uses the identical font-matching path.
- Shift+keypad font selection, proportional window resizing, WM resize
  increments, and grid-preserving renderer switches.
- True color, palette terminal values, inverse, bold, underline, overline, and
  strikeout rendering, subject to the gaps recorded in `ROADMAP.md`.
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
  whitespace double-click, unit-preserving Button-3 extension, X11 `PRIMARY`,
  and Button-2 paste through Ghostty's bracketed-paste encoder. Multi-click
  timing follows xterm's release-to-next-press interval.
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
- Patch-410 default VT bindings are audited in
  `docs/compatibility/default-bindings.md`. Shift+Insert now owns the key event
  and pastes `PRIMARY` instead of also emitting modified Insert; paging and
  font-selection keys likewise remain local-only. Four action-level gaps are
  recorded explicitly.
- Consolidated `-report-config` inventory for resources, app-defaults,
  inherited Xt/Athena resources, translations, and actions.
- Structured diagnostics and CPU flamegraph tooling.

The authoritative missing-capability order is `ROADMAP.md`. Basic scrollback,
its Xaw scrollbar, historical selection, `PRIMARY`, and middle-button paste
are wired, along with application mouse and focus reporting. Selection
clipboard policy and Kitty graphics remain incomplete even though libghostty
exposes much of the required machinery.

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
   capability matrix in `ROADMAP.md`. Add focused tests with each slice so
   parity does not depend on manual demonstrations alone.
3. Make xterm+ usable as the maintainer's daily terminal. `scrollKey` and
   `scrollTtyOutput` policies are implemented as resources, command-line
   options, and VT menu toggles. Historical mouse selection, visible
   highlighting, X11 `PRIMARY`, Button-3 extension, and middle-button paste are
   also implemented, including edge-drag autoscroll through deep history.
   Application mouse reporting preserves Shift selection and the Ctrl+button
   popup menus, and focus reporting follows DEC private mode 1004. Next, add
   the remaining selection/clipboard policies.

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
  `DRIFT.md`, and `ROADMAP.md` synchronized.
- When moving beyond patch 410, update all three compatibility catalogs and
  record the new oracle together.
- Do not install this repository's `XTerm` app-default over a distributor's
  xterm package without a conflict-free packaging strategy.

## Style

`STYLE.md` is the adopted classic-X style. Use the checked-in `.clang-format`
for C sources and headers. Functions use mixed case, public interfaces retain
the `Xtp` prefix, important types are capitalized, and eight-column indentation
gives the code its traditional vertical rhythm. Keep formatting-only changes
separate from behavioral changes when practical.

## Verification

The `justfile` creates or reconfigures private build directories and never
touches `build/`. Run the complete pre-push suite with:

```sh
tools/fetch-libghostty              # once, or whenever the pin changes
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
```

The current self-test has focused backend checks for rendering, cursor state,
modes, selection and deep scrollback, tty-output viewport anchoring, mouse and
focus encoding, resize, PTY setup, and write backpressure without byte loss,
but it remains one in-process harness. Split it into focused tests and add Xvfb
integration coverage. Do not treat a passing self-test as evidence of full UI
compatibility.

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
