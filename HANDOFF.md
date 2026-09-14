# Revenant maintainer guide

This is the durable continuation brief for maintainers. Current feature
priorities and the Ghostling capability comparison live in the
[roadmap](docs/maintainers/roadmap.md); external checkout roles live in the
[upstream reference guide](docs/maintainers/upstream.md). Avoid recording an uncommitted
file list or a single transient commit as project state here.

## Dispatch plumbing and remaining features

Use the [dispatch guide](docs/maintainers/dispatch.md) and the local untracked
`todo.md` IDs for bounded assignments. The tracked guide maps every pending
chunk to its stable TDN feature IDs; chunk IDs are work units, not feature IDs.
`tools/probe-features.py --list` lists 18 manual fixtures. These fixtures do not imply that the corresponding
features are implemented. Extend the graphics fixture as media, placeholders
and animation land; it currently covers only static inline RGBA placement.

**Allow Mouse Ops** and **Allow Tcap Ops** are live Ctrl+right-click menu
controls, both defaulting true. Mouse Ops suppresses encoded mouse/focus
reports without clearing requested modes; `disallowedMouseOps` named
exceptions remain open. Obtain the encoder's effective tracking mode before
adding those exceptions: simultaneous requested mode bits are not a reliable
substitute. Tcap Ops honors `disallowedTcapOps` (`SetTcap,GetTcap`) through the
shared wildcard/negation parser and gates complete backend-generated
XTGETTCAP replies. Unrelated replies must survive mixed feeds. Configured TN and
child TERM integration are implemented separately by A2; XTSETTCAP remains open.
TDN: `policy-mouse-ops-exceptions`, `dcs-xtsettcap`.

Font Ops has Xt resources (`allowFontOps`, `disallowedFontOps`), SetFont/GetFont
policy helpers and checked menu identity, but its menu remains insensitive and
resources have no operational effect until OSC 50 is implemented. The pinned
unknown-sequence callback is APC-only; it cannot supply OSC 50, OSC 22 or
arbitrary CSI passthrough. Obtain a public hook rather than another escape
parser. ENQ, underline attributes, startup cursor style, shell integration,
notification urgency and the output pipe have no new xterm Ops family. Full Window Ops and Title Ops completion
checklists below still apply independently.
TDN: `osc-50-font-set`, `osc-50-font-query`, `policy-font-ops`, `osc-22-pointer-shape`,
     `diagnostics-unknown-apc`.

`probe-features` checks the runner through a fake PTY, including interruption
cleanup; `xvfb-request-ops` checks startup/live mouse and capability policy,
GetTcap exceptions, and preservation of unrelated replies.

## Unified Go manual probe

The probe is part of TDN; use `just probe` for new feature work. It builds the
standalone Linux Go runner independently of the C terminal build. Add native
cases with stable TDN IDs, expected behavior, policy and cleanup; register their
navigation in `tdn/data/probe-navigation.json` and refresh metadata with
`just update-probe-registry`. Keep measurements quiet, reads bounded, and cleanup
active on exits, signals and panics. `just test-probe` covers Go/PTY behavior;
`just check-tdn` also checks the embedded snapshot against the registry.

The [probe guide](tools/probe/README.md) is the source for controls, commands,
80×24 behavior, evidence and maintenance instructions. Retain legacy scripts
and recipes until the maintainer completes the [migration worksheet](tdn/docs/probe-migration.md)
and resolves its color-tour/spawn gaps. Probe observations do not automatically
become terminal support claims.

## TDN feature registry and compatibility tracking

The user brought the registry work forward while the local `todo.md` queue
is still being drained. TDN now has stable feature IDs in
`tdn/data/features.yaml`, specification reference IDs in
`tdn/data/specifications.yaml`, and an independent support mapping in each
`tdn/terminals/<terminal-id>.yaml`. Add a terminal by adding its file;
there is no central terminal list. `as-of` names the assessed version or
revision, never an inferred first-supported release. Older assessments can
be retained in `history`.

MkDocs generates a filterable comparison page, one permanent page per
feature ID, compatibility tables at the bottom of related specification
pages, terminal profile tables, and a JSON export from those same records.
The data validator runs during every build; `tdn/tests` covers its contract.
See `tdn/docs/registry.md` for contributor instructions.

The initial migration preserves old table claims as imported/unverified,
with links to the exact previous source revision. Conflicting claims stay
unknown. Legacy aggregate rows are marked as groups and do not confer
support on individual operations. Revenant is assessed independently of
xterm and Ghostty; its initial entries identify reviewed development
revisions and do not assert released-version support.

All named pending features in the local checklist and this handoff have TDN
IDs, including policy gaps and host/user actions without escape sequences.
The latter use descriptive IDs and documented design scopes, not invented
protocol numbers. Optional ideas, open questions and upstream blockers retain
their existing priority; registration does not promote them into the active
queue. Broad legacy inventories remain explicitly grouped until their exact
subfeatures are defined. Refactoring, test-matrix expansion, release audits
and product anti-goals are not new compatibility features.

Continue implementation work from `todo.md`. Record new support under the
appropriate feature ID with version, policy/configuration, limitations and
evidence. After draining the queue, audit the imported claims, resolve
conflicts, fill missing emulator assessments, and split remaining aggregate
records into independently tested behavior. Preserve published IDs when
moving or rewriting pages. Do not turn unknown into unsupported merely
because a query is denied or a probe has not been run.

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
- The libghostty adapter now supplies current cell/grid geometry for XTWINOPS
  `CSI 14 t`, `CSI 16 t`, and `CSI 18 t` queries and identifies itself to
  XTVERSION as `revenant(<version>)`, following xterm's `XTerm(411)` display
  convention while retaining honest product identity. Backend self-tests pin
  the initial and post-resize replies. Device-attribute replies are now
  Revenant-owned through libghostty's device-attributes callback (see the
  A3 entry below and the drift ledger); the earlier "deferred to v0.6"
  wording described an unreviewed default, not a completed audit.

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
resources and `clear-saved-lines` as bounded v0.5 work after the completed
palette round. The larger high-use binding gaps are classified for v0.6. The
honest command-line surface is now complete; session logging is explicitly
deferred to v0.6, while the other option-driven process behaviors remain
inventory items.

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
TDN: `resource-faint-is-relative`.

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
  centralized; do not reintroduce independent argv scans. Terminal-name
  integration is now complete (A2); the other process slices remain deferred.
  TDN: `startup-login-shell`, `resource-terminal-modes`, `startup-hold-after-exit`,
       `startup-wait-for-map`, `pty-message-permission`, `logging-session-transcript`.
- When `vt_interaction.c` next receives material work, split X selection/paste,
  hyperlink launching, and mouse reporting into focused owners. Other local
  cleanup should follow its owning feature: table-drive the order-dependent
  default character classes, give `WarnRecord` kind-specific fields, name the
  remaining viewport/cell/frame-cache idioms, and shorten the large reporting,
  routing, drawing, and `SetValues` functions as they are changed.
  Inferred-link hover is derived from the last completed frame. If terminal
  output replaces that text while the pointer remains stationary with Shift
  held, the old span can remain underlined until the next pointer or Shift
  event; activation re-resolves the target and cannot open the replaced URL.
  Fix the cosmetic lag with a non-reentrant post-render hover refresh when the
  hyperlink interaction owner is split out.
  TDN: `hyperlinks-hover-refresh`.
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
- XTWINOPS title stack and reports: libghostty parses `CSI 20-23 t` but
  exposes no hook, so the cursor-blink control observer also reports those
  four operations (flushing preceding output first so titles are current);
  `main.c` applies the Window Ops policy per operation, answers 20/21 with
  xterm's 7-bit `OSC L`/`OSC l` replies read from the shell's WM properties,
  and drives `title_stack.c`, a copy of xterm's ten-entry ring with direct
  slot access, empty-entry quirks, and unclamped `used` counting. Pop applies
  the saved labels to the shell and to libghostty's title when `allowTitleOps`
  is enabled. The **Allow Title Ops** menu toggles that resource live (default
  true), gating application title changes and applying saved labels while
  leaving reports and stack permissions under Window Ops. A permitted pop
  consumes its entry even when title changes are disabled. `xvfb-title-ops`
  checks startup policy, actual menu clicks, live changes, and visible labels.
- OSC 52 selection access: libghostty decodes the request and calls the
  clipboard write/read effects; `main.c` applies `allowWindowOps` and
  `disallowedWindowOps` (parsed by `window_ops.c`) plus `maxStringParse`, and
  the widget owns or clears the named X selection with a private per-atom text
  copy, or answers a query from an owned entry or a synchronous X round trip
  against a private request window. The read effect is installed only when
  `GetSelection` is allowed so a denied query stays silent like xterm. The
  **Allow Window Ops** menu toggle updates that effect and the write policy
  at runtime; `xvfb-window-ops` covers enable/disable and restoration of a
  configured set-only policy.
  This is partial Window Ops support: `GetSelection`, `SetSelection`, and the
  four title operations consult the policy. Existing XTWINOPS size reports
  are not yet gated by it. See the completion plan below before claiming full support.
  libghostty's parser accepts one selection letter; `q`, cut-buffer digits,
  lists, and the `s0` default are recorded in the drift ledger.
- Synchronized output (DEC private mode 2026): the widget holds dirty updates
  while the mode is set and paints once on release. A one-second timeout
  releases a stuck batch and resets the mode so DECRQM reports the reset. An
  ordinary expose repaints the cached frame and keeps holding; a resize paints
  the current state at the new grid, and the backend restores the mode that
  libghostty clears on resize. Hover repaints during a hold are deferred to a
  full repaint at release. `xvfb-sync-output` covers release, timeout, resize
  with DECRQM confirmation, and hover.
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
- Working directory: libghostty's `OPT_PWD_CHANGED` callback delivers the raw
  OSC 7 (or OSC 9/1337) value through the backend-neutral
  `working_directory_changed` effect. `working_directory.c` decodes `file://`
  URIs (empty, `localhost`, or this host by short or full name; percent
  escapes; query and fragment dropped) and bare absolute paths, rejecting
  other schemes, relative paths, malformed escapes, and control bytes. The
  authority must be a plain registered name; userinfo, ports, escapes, or
  delimiters in it are rejected before any hostname comparison. The
  application keeps one heap copy in `App.working_directory` after a `stat`
  confirms a local directory; every report it cannot validate clears the
  copy so consumers never see a stale path, and an empty report clears it
  too. The terminal process's cwd is never changed. No Ops family gates the
  report. libghostty drops any OSC 7 longer than its fixed 2048-byte capture
  (`osc.zig` `Parser.MAX_BUF`) without a callback, so the feed observer
  flushes earlier output at each OSC 7 header (so a preceding OSC 1337 or
  OSC 7 in the same feed delivers its own callback first), dispatches the
  report to the core at its terminator, and fires the
  `working_directory_dropped` effect when no pwd callback followed; the
  application clears its copy on that effect as well.
  `xvfb-working-directory` checks the logged event sequence for spaces and
  UTF-8, the local hostname, a remote host, a missing directory, the
  bare-path form, a malformed escape, a clear, and an oversized report
  between two valid ones, and reads `/proc/<pid>/cwd` before and after to
  prove the terminal stayed put.
- Terminal name: the `termName` application resource (`-tn`) is resolved
  once, exported as the child's `TERM` by the PTY spawner, and pushed to
  libghostty's terminfo-name option so XTGETTCAP `TN` reports the same
  string; the reply passes through the Tcap Ops filter like any XTGETTCAP
  reply. Names the core rejects (over 128 bytes) still set `TERM` but clear
  any earlier core name so `TN` stays unanswered rather than stale, with a
  warning. `-report-config` prints `termName` with its compiled default and
  adds an `effective termName` line when the configured value is empty.
  `xvfb-term-name` compares `TERM` and the hex-encoded `TN` reply for the
  default, option, and resource forms, and checks the silent reply under Tcap
  Ops denial; `xvfb-report-config` covers the default, empty, and custom
  report lines.
- Device attributes: libghostty's device-attributes callback fills the
  identity declared in `src/device_attributes.h`: DA1 `CSI ? 62 ; 6 ; 21 ;
  22 c`, DA2 `CSI > 1 ; Pv ; 0 c` with `Pv` computed from `XTP_VERSION`
  (`major * 10000 + minor * 100 + patch`, clamped to 16 bits), and DA3
  `DCS ! | 00000000 ST`; XTVERSION is unchanged. Every DA1 code is backed
  by the `device attributes evidence` self-test, which checks rendered cells
  for DECSCA/DECSED/DECSEL, DECLRMM/DECSLRM with DECRQM, and palette/RGB
  foregrounds; the `device attributes` self-test pins the exact bytes, the
  zero-parameter forms, neighboring DSR/XTVERSION/DECRQM replies in one
  feed, byte-by-byte split feeds, the core's answer to a nonzero parameter,
  and silence for other intermediates. Adding a code means adding rendered
  evidence there and updating the drift ledger's omission list, which says
  which codes were rejected and why. `xvfb-device-attributes`
  reads the exact reply bytes through the real PTY for one write and for
  fragmented writes, with the expected DA2 number computed independently
  from Meson's version string.
- Pipe command output: the application resource `pipeCommandOutput`
  (unset by default) names a shell command; the widget's
  `pipe-command-output()` action, bound to `Ctrl Shift <KeyPress> g` and
  decided through the same deferred key path as the prompt keys, only
  fires the `XtNpipeOutputCallback`. Completion comes from OSC 133 D: the
  feed observer accounts each OSC 133 across feed boundaries the way the
  OSC 7 path does, counting only bytes at or above 0x20 because the core
  drops nonterminating C0 bytes without storing them, and accepts it only
  when its first item is exactly `D`,
  it ended with BEL or ST rather than CAN or SUB, and its payload after
  `133;` fit the core's 2048-byte capture (2048 accepted, 2049 dropped),
  so libghostty acted on it; acceptance alone sets the "D seen" state,
  then it flushes the sequence through the core and `RecordCommandEnd`
  keeps a tracked reference to the newest prompt's row as the command
  that finished,
  exposed as `XtpTerminalLastCompletedPrompt` (resolved through the same
  continuation rule; -1 before any D, after that prompt is pruned or reset
  away, or on the alternate screen) alongside `XtpTerminalCommandEndSeen`.
  The application uses that row; when it is unavailable but a D was seen
  before, it reports that the completed command is no longer in history
  and stops, and only a shell that has never sent D gets the prompt before
  the newest one, with a log line saying so; then it calls
  `XtpTerminalCommandOutput` and `XtpTerminalSpanText` for the text. `src/pipe_command.c` makes the write
  end non-blocking and close-on-exec before forking and gives up cleanly
  if that fails, forks `/bin/sh -c` in its own process group (verified with
  `setpgid` or `getpgid`; without one only the direct child can be
  signalled and that is logged) with the text on a pipe, runs it in the validated OSC 7 directory when one is retained
  (a failed `chdir` exits 126 instead of running elsewhere), writes from
  `XtAppAddInput` writability callbacks, reports EPIPE or a helper that
  exited with text pending as "output pipe closed early", and reaps by
  polling `waitpid` on a backing-off timer that retries EINTR, so the PTY
  child's handling is untouched. The parent ignores SIGPIPE; the PTY child,
  the pipe child, and the hyperlink launcher all restore the default before
  exec. Jobs live in a list on the App and unlink themselves through a done
  hook, but a job stays alive after its shell exits for as long as the
  process group still has members (probed with `kill(-group, 0)`, safe
  because a group id is reserved while any member lives), so background
  descendants remain reachable; `DestroyApplication` abandons any job
  still open by closing the pipe, sending SIGTERM to the group while it
  has members, waiting up to a quarter second, then SIGKILL, and reaping
  the leader, so neither the helper nor a descendant that ignores SIGTERM
  outlives the terminal. A helper that must survive the terminal has to
  leave the group with `setsid`. Without a group of its own only the
  unreaped direct child is signalled, never a reaped pid. The captured text is never interpreted; the only command
  that runs is the configured one. `xvfb-pipe-output` captures exactly
  COMMAND-3-BEGIN through COMMAND-3-END from a three-command fixture with
  Unicode, a 110-character wrapped line, and shell-looking text, first
  before the shell prints its next prompt (so completion must come from D)
  and again with the live prompt on screen, checks the helper's working
  directory against the OSC 7 report, confirms the unset resource only
  logs, drives a helper that exits without reading a 145 KB output to the
  closed-early path, and exits after a helper shell has already exited
  leaving a SIGTERM-ignoring `sleep` in its group, then confirms that
  child is gone.
  The self-test pins the D contract: none seen, D before any next prompt,
  a running later command not moving it, CAN, SUB, `Dgarbage`, an oversized
  report, the exact 2048/2049 payload boundary, NUL bytes inside the D item
  whole and split across feeds, NULs interleaved at that boundary, a D
  split across feeds inside its terminator, BEL and ST terminators, and
  the alternate screen.
- Box and block glyphs: `src/box_glyphs.c` turns one U+2500–U+259F code
  point plus a cell size and the bold flag into a list of rectangles and a
  shade level, with no X dependency. Line thickness is `height / 16`, capped
  at `width / 8` for tall narrow cells, at least one pixel and one more when
  bold; heavy is twice light; double
  lines are two light strips a strip apart. Every arm is a band centered
  with `(size - thickness) / 2`, so a band depends only on the cell size
  and thickness and joins the neighbor's band exactly; arms run from the
  cell edge through the junction strip of the perpendicular arms, and the
  double-line rules give the outer strip the far stop and the inner strip
  the near stop so double corners, tees and crosses meet cleanly. Dashes
  leave a gap at each cell edge, arcs and diagonals are rasterized per
  row, and shades are 2×2 stipples (one, two or three pixels of four)
  drawn with `FillStippled` from lazily created bitmaps that `Destroy`
  frees. `vt_draw.c` decides per cell in `ProceduralBoxGlyph`: procedural
  when `forceBoxChars` is set, always on the bitmap path (whose
  `MakeVisualCell` now keeps these code points instead of `?`), and on the
  Xft path when the primary face for the cell's bold/italic style lacks the
  character (`XftCharExists`), so a fallback face never draws a box glyph
  with foreign metrics. A box cell is never grouped for shaping. The cell
  is painted as background fill plus `XFillRectangles` on the widget GC
  under the same clip as text, using the `VisualCell` colors, so inverse,
  selection, faint and default-background opacity behave exactly as for
  text; the block cursor draws the glyph in the cursor text color over
  the fill, and decorations (underline, strikethrough) are unchanged. The
  route log line says `role=box file=(procedural)`. `XtpVtSetForceBoxChars`
  invalidates the frame and redraws, logging `box glyphs font-first ->
  forced` (or back); the font menu's `font-linedrawing` entry is an active
  checked item, `set-font-linedrawing(on|off|toggle)` is an action with
  its own `LocalKeyAction` identity so a bound key and its release never
  reach the child, and `+fbx`/`-fbx` set the resource with xterm's
  polarity (plus turns it on). Plans allocate their rectangle list on the
  heap, so arcs and diagonals stay complete at any cell size; if an
  allocation fails the cell falls back to the font glyph with a warning.
  The same module plans braille, U+2800–U+28FF, with Ghostty's dot layout
  (dot size `min(width / 4, height / 8)`, leftover pixels spent on margins,
  then spacing, then dot size); it declines when a dot would vanish, so a
  one-pixel-wide cell uses the font, and bold does not change the dots. It
  plans Powerline U+E0B0–U+E0BF only. Arrows, half circles and slants are
  per-row runs from the flat side, at least one pixel on every row so a
  segment's background meets the glyph, merged into taller rectangles when
  consecutive rows match; the thin variants keep a run of the line
  thickness inside the solid outline, overlapping the neighbor rows and
  reaching the flat side on the first and last rows; the slash separators
  reuse the U+2571/U+2572 diagonals. The draw decision, colors, cursor,
  route log and `forceBoxChars` switch are shared with box drawing, and the
  rest of the private-use area (U+E0A0–U+E0AF, U+E0C0 onward) stays with
  the font. `XtpBoxGlyphSetAllocator` lets the self-test fail allocations.
  The `box-glyphs` self-test checks every code
  point at six cell sizes for staying inside the cell, the light band
  geometry and column/row identity across cells, cross = union of the two
  lines, corner and tee arms reaching the correct edges, heavy containing
  light, bold thickness, double-line strip count and corner continuity,
  a single line crossing a double pair without a gap, block partitions
  (upper/lower, left/right, quadrant complements), monotone eighths,
  shade densities, dash gaps, arc endpoints and diagonal corners.
  `xvfb-box-glyphs` samples pixels under Xft (menu toggle on, then a
  selection, then toggle off), under `+fbx` at 24 points, and under the
  bitmap path: one continuous band across two cells with the expected
  bounds, a full block, an empty neighbor, half blocks in normal, inverse
  and selected colors, bold and double thickness, and shade densities.
  `xtp-toggle-window-ops WINDOW linedrawing` picks the entry by counting
  rows and separators from the bottom of the font menu. A final case binds
  F12 to `set-font-linedrawing(toggle)` with a raw child reporting Kitty
  releases and checks two toggles, four owned events and no bytes.
  The `braille and Powerline glyphs` self-test checks golden dot layouts at
  6×13 and 9×19; equal square dots in their columns and rows, with gaps
  and margins, at nine cell sizes up to 1030×1100; the dots of every
  braille pattern; one run per row from the flat side, vertical symmetry,
  containment and reflections for each Powerline shape; bold; the diagonal
  separators; and plans that decline and free everything after zero or one
  successful allocation. A second `xvfb-box-glyphs` scene checks, under Xft
  at 16 and 9 points and on the bitmap path, exact arrow ink and bounds, a
  full block joined to an arrow, inverse, selected and cursor-covered
  arrows, a heavier bold thin arrow, braille dot squares and counts, and
  that U+E0A0 and U+E0C0 are not drawn procedurally.
- Copy feedback: `PublishSelection` in `vt_interaction.c` calls
  `VtStartCopyFlash` when a selection gesture owned at least one atom; OSC
  52 writes own atoms through `OwnSelectionText` directly and never reach
  it. `VtStartCopyFlash` (`vt_widget.c`) sets `copy_flash_active` and arms,
  or re-arms, one Xt timer for `copyFlashDuration` milliseconds; a zero
  duration logs `copy flash disabled`. While the flash is active
  `MakeVisualCell` draws selected cells on `copyFlashColor`, parsed once to
  RGB and allocated through the color cache, or skips the selection swap
  when no color is set. Start, expiry and cancellation repaint through
  `RepaintCopyFlash`, which defers to `VtDeferSynchronizedRedraw` under mode
  2026 and otherwise invalidates the frame for a full redraw, because a
  flash changes colors on rows libghostty does not mark dirty.
  `VtCancelCopyFlash` runs when a select-start or start-extend gesture
  begins, when `RemoveOwnedSelection` drops the last highlighted atom
  (another client's SelectionClear, an OSC 52 clear), and when an OSC 52
  write replaces the highlighted atom; `Destroy` removes the timer.
  `xvfb-copy-flash` samples a copied cell's background under PRIMARY with a
  color (flash, exact bytes, expiry, a replacing gesture whose earlier timer
  never fires, another client taking PRIMARY), CLIPBOARD without a color, a
  zero duration, OSC 52 writes and replacement, a resize during the flash, a
  synchronized-output hold that never shows it, and teardown mid-flash.
- Desktop notifications: `notification_policy.c` (widget sources, so the
  self-test and helpers link it without libnotify) holds the pure rules: a
  gate allowing `XTP_NOTIFY_RATE_BURST` (5) attempts in any
  `XTP_NOTIFY_RATE_WINDOW_MS` (10,000 ms) sliding window, counted when an
  attempt is allowed, with `XTP_NOTIFY_FAILURE_BACKOFF_MS` (5,000 ms) after a
  failure and a first-denial flag so rate limiting and backoff each log once
  per period; and text preparation that decodes UTF-8, replaces invalid
  bytes, NUL, C0, DEL and C1 with U+FFFD, turns newline and tab into a space
  in titles, cuts at `XTP_NOTIFY_TITLE_LIMIT` (256) or
  `XTP_NOTIFY_BODY_LIMIT` (1,024) bytes on a codepoint boundary, and escapes
  `& < > " '` when asked. `desktop_notification.c` (program sources only)
  includes `config.h`. Without `HAVE_LIBNOTIFY` it logs once that desktop
  notifications are not compiled in. With it, `TerminalNotification` in
  `main.c` applies urgency first and then calls `XtpDesktopNotify`, which
  logs and drops focused requests, copies the borrowed spans into prepared
  text (logging truncation and replacement by size only), and queues at most
  four requests for a delivery thread started on the first request, dropping
  a request that finds the queue full. The thread checks the gate as it takes
  each request, immediately before calling libnotify, so requests queued
  before a failure fall under its backoff and a stalled call cannot bunch
  attempts past the burst. That thread is the only caller of libnotify
  while the terminal runs, because `notify_notification_show` is synchronous
  and a hung daemon must not stall the PTY: it initializes libnotify lazily,
  asks the server's capabilities once per successful period and escapes the
  body only for `body-markup` servers (summaries are never markup), uses the
  program name as icon and as the summary of an untitled request, shows and
  unrefs each notification, and reports success or failure to the gate
  (failure logged once when a failing period begins, recovery logged once).
  Any failure calls `notify_uninit`, because libnotify's cached proxy only
  follows a restarted daemon through signals that this loopless thread never
  dispatches; a `ServiceUnknown` or `NameHasNoOwner` error retries once at
  once with a fresh proxy.
  `DestroyApplication` stops the thread, joins it when idle so it can call
  `notify_uninit`, and otherwise leaves a busy thread and its state alone
  rather than waiting out a D-Bus timeout at exit. `-report-config` ends with
  a section saying whether libnotify is compiled in and, if so, the running
  daemon's name and version or that none answers. `xvfb-desktop-notification`
  runs in compiled mode under `dbus-run-session` against
  `xtp-fake-notifications`, a GIO `org.freedesktop.Notifications` that
  records each request with its arrival time as hex fields, can reject, and
  can hold calls unanswered until SIGUSR1: exact Unicode, escaped markup,
  OSC 9 and OSC 777, empty title and body, focus suppression, control and
  invalid bytes, both limits, a rate-limited burst reported once, rejection,
  backoff, recovery, a missing daemon, requests queued behind a failing call
  dropped by its backoff, a call stalled past the window with no more than
  five calls arriving in any ten seconds, urgency throughout, no title or
  progress changes, the report lines (including a one-second limit against a
  daemon that never answers) and a clean stop. Without
  libnotify the same script checks that three requests log, set urgency and
  report unavailable once. The `notification policy` self-test covers the
  gate and text rules.
- Progress indicator: libghostty's `GHOSTTY_TERMINAL_OPT_PROGRESS_REPORT`
  callback becomes the `progress` effect (`XtpProgressState`, percent 0-100
  or -1 when omitted, clamped) and `XtpVtSetProgress` in `vt_progress.c`.
  libghostty itself reports a removal on RIS, so no reset hook is needed;
  the PTY loop removes progress when the child's output ends. The indicator
  is an Athena Simple child of the VT widget, created on the first report,
  managed while progress is shown and unmanaged on removal, so it takes no
  keyboard input. `VtLayoutProgress` runs from `LayoutScrollbar` (every
  resize and scrollbar change) and places it in the top-right corner inside
  the internal border, left of a right-hand scrollbar, a fifth of the widget
  width clamped to 40-160 pixels and 6 pixels high with a 1-pixel border. It
  draws in its own window with its own GC: the effective background pixel
  (translucent under `backgroundOpacity`), then a
  fill of `percent`% in the foreground color (set), red #E01B24 (error) or
  amber #F5C211 (paused); error and paused without a value keep the last
  reported percentage, 0 included, or fill when none was ever reported
  (tracked separately from the value and forgotten on removal; libghostty
  reports a bare `9;4;1` as 0%, so only the effect's -1 means none); the error and paused colors
  are allocated once with the indicator and freed in `VtProgressDestroy`,
  and a failed allocation draws in the current foreground instead; indeterminate moves a quarter-width
  block every 120 ms. Because the indicator is a separate window it updates
  while synchronized output holds the terminal frame, and the frame's own
  drawing is clipped around it. New effective colors from OSC 10/11 or their
  resets reach it only through `VtProgressColorsChanged`, which
  `ApplyFrameColors` calls to update its background and border resources and
  redraw the bar; the live opacity setter calls it too, and so does
  `SetValues`, whose color reset can make the next frame match and skip
  `ApplyFrameColors`. Nothing reaches the title path.
  `xvfb-progress` steps a raw bash child through an error before any
  percentage, 0% then error without a value, set, a foreground/background change and its reset checked on the
  fill, the empty part and the border, a live reverse-video toggle each way,
  error, paused,
  indeterminate (animation and its stop), 150 clamped to 100, a value sent
  inside a mode 2026 hold, a resize, clear, a title report and title-stack
  pop checked against WM_NAME, RIS and exit, sampling the bar's first and
  last inner pixels. `xvfb-opacity` adds a progress case under the fake
  compositor: the track is the translucent background before and after a
  live slider change while the fill and border stay opaque.
- Search overlay: `vt_search.c` drives the model from the widget.
  `start-search()` (Ctrl+Shift+F, with its own `LocalKeyAction` identity)
  creates an override-redirect `searchOverlay` popup holding an Athena
  Label, placed over the bottom-left of the terminal and moved on shell
  ConfigureNotify and widget resize; keyboard focus never leaves the
  terminal. While `search_active` is set, `InputEvent` hands every key press
  to `VtSearchKey`, clears Xt's continue-dispatch flag so no translation
  action also runs, and records the keycode as owned, so neither the press
  nor its later release reaches the child even when Escape or Enter closes
  the search first. Typing appends UTF-8, Backspace removes one codepoint,
  and each change calls `XtpTerminalSearchSetQuery`, then scans in
  2,048-row steps from a zero-delay timer (every 250 ms while the alternate
  screen suspends it). The active match is a `XtpTerminalCellMark` on its
  start cell, resolved by navigating backward from the next cell, so it
  survives reflow and eviction and disappears if its text changes; the
  first one is the nearest match above the bottom of the viewport where the
  search began; a candidate that only wrapped to a newer match is ignored
  until the scan completes, because the newest-first scan may still find an
  older one. A query the alternate screen refuses is retried every 250 ms and
  applied once the primary screen returns. Previous or next navigation that
  would wrap while the scan runs rings the bell and waits, like the first
  pick. Moving to a match outside the view centres it. If publishing PRIMARY
  fails, Enter rings the bell and the search stays open. An input method
  commit longer than the 128-byte buffer is looked up again at its full size;
  one that does not fit the query is refused with a bell.
  `VtRenderTerminal` calls `VtSearchPrepareFrame` before each render to
  collect the visible spans (`XtpTerminalSearchVisible` into a buffer sized
  for `XTP_SEARCH_MATCH_LIMIT`, so every retained match can be drawn; a cell
  bisects them, since their starts and ends both ascend) and the active span; `MakeVisualCell` draws
  other matches in selection colors and the active one on the cursor color.
  Every search change invalidates the frame and redraws, deferring under
  mode 2026 like the copy flash. Enter copies `XtpTerminalSpanText` of the
  active match through `VtPublishSearchMatch`, which owns PRIMARY without a
  highlight (logged as source SEARCH), then closes without moving the view.
  Escape closes and returns to the bottom, or to a cell mark on the starting
  viewport's top row (the oldest row if that was evicted). While a search is
  open the PTY loop uses `XtpTerminalFeedOutputPinned`, which marks the
  viewport's top cell before feeding and scrolls back to it, instead of the
  xterm policy that keeps the distance from the bottom. Visible-match
  highlights are not re-verified per frame, so an overwritten match stays
  highlighted until navigation or a query change drops it. `xvfb-search`
  uses a 40x10 grid set through `columns`/`rows` (`-geometry` did not set
  the grid in this harness) and samples cell backgrounds for a wrapped
  highlight, other matches, navigation with wrapping and scrolling, an
  empty and a missing query, output that must not move the view, a resize,
  Escape restoring the bottom, Enter copying the wrapped match to PRIMARY,
  a screen of 360 single-character matches highlighted to the last row,
  a raw Kitty-release child that receives nothing during the search, and a
  key that reaches it afterwards. `xtp-send-key` gained `keysym NAME
  [ctrl-shift]` and `text STRING` modes.
- Scrollback search model: `XtpTerminalSearch` in
  `terminal.h` searches the primary screen for a literal UTF-8 query.
  `search_match.c` is backend-neutral: the query becomes codepoints with a
  Knuth-Morris-Pratt failure table, `XtpSearchFindAll` reports every
  occurrence, overlaps included, only when both ends fall on grapheme-cluster
  boundaries, and `XtpSearchPick` chooses the nearest start strictly after or
  before a cell in a descending array, wrapping to the first or last match.
  There is no case folding or Unicode normalization, so precomposed é and
  e+U+0301 are different queries. `terminal_ghostty_search.c` scans newest
  first. A tracked reference marks the bottom row of the next logical line;
  the line extends upward through rows whose WRAP flag is set, up to
  `XTP_SEARCH_LINE_ROW_LIMIT` rows (a longer line is split, and a match
  across the split is missed). Cells become units: empty cells read as
  spaces, spacer cells are skipped, and trailing spaces of the last row are
  trimmed. Each match is stored as tracked start and end cells.
  `XtpTerminalSearchSetQuery` reads the active screen at once, at most its
  height plus the rest of one wrapped line, because only those rows can
  still be written; history rows cannot, so the results describe the text
  present when the query was set. Later output stays out: new rows arrive
  below the fixed boundary, and an overwrite of a row already read is caught
  by navigation's re-check. The one gap is a resize that pulls unread history
  back into the active screen before a step reaches it.
  If that immediate scan fails, the setter returns -1 and the search stays
  in `XTP_SEARCH_ERROR`; `XtpSearchSetAllocator` lets the self-test fail
  the scan's first allocation.
  `XtpTerminalSearchStep` then examines at most its row budget plus the rest
  of one wrapped line, and a zero budget does nothing, so a caller can
  interleave PTY reads.
  Tracked references follow scrolling, eviction and reflow: a dead cursor
  completes the scan, `XtpTerminalSearchMatches` drops dead matches, and
  `XtpTerminalSearchNavigate` re-reads the chosen match and drops it when its
  text no longer spells the query, as after an overwrite. Matches are capped
  at `XTP_SEARCH_MATCH_LIMIT`, keeping the newest and setting the truncated
  flag. The alternate screen has no history: setting a query there fails,
  and a running search reports `XTP_SEARCH_UNAVAILABLE`, keeping its state,
  until the primary screen returns. The stub backend returns no search
  object. A reflow in the middle of a scan can make a split long line report
  a match twice; only lines longer than the row limit are affected. The
  `search matching` self-test covers the matcher and navigation order;
  `scrollback search` covers text written after the query is set, an
  overwrite after it, an allocation failure in the immediate scan, a zero budget, empty and invalid queries, wide characters,
  combining clusters, overlaps, matches across a soft wrap including a wide
  character pushed to the next row, reflow narrower and wider, an
  overwritten match, the alternate screen mid-scan, eviction of finished
  matches, a 20,000-row history scanned in 256-row steps with output fed
  between steps, cancellation, the match limit, and eviction during a scan.
- Prompt navigation: the adapter keeps an index of OSC 133 prompt starts as
  libghostty tracked grid references. The feed observer already delimits
  OSC selectors and payload items for OSC 7 and the color queries; for
  selector 133 it looks at the first payload byte only, and when that byte
  is `A` it flushes the OSC through the core at its terminator and records a
  tracked reference at the cursor row (`RecordPromptMark`). Marks are kept
  in screen-row order (a mark placed above the newest one is inserted by
  bisection), one per row so a redraw of any prompt row, newest or older,
  adds nothing. Pruning takes rows from the top and a reset takes them all,
  so dead marks form a prefix that `CompactPromptMarks` drops on every
  insert (`XtpTerminalPromptMarks` reports the raw stored count and never
  compacts, so the regression measures the insert-time bound); the index is
  therefore bounded by the rows the core retains even when nothing ever
  searches it, with no fixed cap and no fallback scan. Marks are skipped when
  the alternate screen is active, dropped lazily once the core reports they
  have no value (pruned scrollback, reset) or their row no longer carries a
  mark, and freed with the terminal. `XtpTerminalFindPrompt` bisects the
  index for `from`, then walks marks in one direction resolving each to its
  prompt start (`PromptStartOf`: a continuation row walks up through the
  run to its primary row or to the run's top when that row is gone), so a
  search costs a handful of point conversions rather than a row scan; a
  history without marks costs nothing. `XtpTerminalSemanticRow` still
  resolves one row from the top of the screen and is for tests and
  spot checks, not loops. Upward orphan handling is a Revenant
  normalization, not exact core behavior: libghostty's upward iterator
  returns the continuation row where it entered an orphan run that starts
  at screen row zero, while its downward iterator returns row zero;
  Revenant returns the run's top row in both directions. The core marks a
  soft-wrapped prompt's tail rows as continuations and can leave a reflowed
  prompt row marked as a continuation; both resolve through the same rule.
  `XtpTerminalCommandOutput` wraps the core's own `select_output`: it finds
  any output-content cell in the prompt's block and lets libghostty derive
  the highlight from the prompt (first written output cell to the last, a
  written space counting as output, bounded by the row before the next
  prompt; no-value when nothing was written, so blank-only output is a span
  whose text formats to nothing), then confirms with `OutputBelongsToPrompt`
  that no other prompt row lies between the prompt's own rows and the
  selection, which keeps a prompt missing from the index from handing back
  a later command's output; the result is a cell-exact `XtpSemanticSpan`;
  `XtpTerminalSpanText` formats a span through the core's plain formatter
  with soft wraps joined and trailing blanks trimmed. Output that begins on
  the prompt row after the input is included; a prompt that starts mid-row
  is moved to a fresh line by the core. All of these decline on the
  alternate screen. The widget's `VtScrollToPrompt` refuses when there is
  no history, reports "already at the live view" when the next prompt is
  inside the live area and the viewport is already there, logs each move as
  `prompt navigation direction=… from=… to=…`, and scrolls with the usual
  render scheduling. `previous-prompt()`/`next-prompt()` take an optional
  count and `Ctrl Shift <KeyPress> Up/Down` are the default translations.
  Whether a key is translation-owned is decided after Xt has dispatched
  the event: the raw key path appends every press, repeat and release it
  does not already own to an unbounded pending list, and a zero-delay
  timeout either drops it or encodes it normally (G1 generalized this from
  the Ctrl+Shift+Up/Down/G set to all keys, so any action that calls
  `VtAcceptLocalKeyAction` keeps its key from the child under any
  binding). Bursts queue without limit (Xt drains queued X
  events before due timers, so a burst arrives whole), and an allocation
  failure drops the key rather than delivering a bound gesture. A local
  action marks its pending press owned directly (`VtMarkPendingKeyOwned`)
  as well as recording it in the key-action ring, and release ownership
  is tracked per keycode in `owned_keycodes`, so releasing a modifier
  before the arrow still suppresses the arrow's release; a focus-out clears
  both the keycode set and the action ring so a key released elsewhere
  cannot eat a later press's release. An override such
  as `Ctrl Shift <KeyPress> Up: insert-seven-bit()` restores delivery;
  `insert-seven-bit()`/`insert-eight-bit()` are registered as no-op
  actions for that purpose and `-report-config` knows all four. The
  `prompt navigation` self-test pins row states, both search directions,
  the continuation and orphan rules, cell-exact output spans and text for
  plain, continuation, wrapped and orphan prompts plus a same-row layout,
  blank-leading and blank-only output, no output for the live prompt, scrolling to a history row versus the
  live area, the alternate screen, reflow at a narrower width, no markers,
  and eviction. `xvfb-prompt-navigation` presses the bindings against a
  six-prompt fixture and checks every logged move plus the top row's ink
  from outside the process (a prompt row is inverse video, an output row is
  blank), resizes through X to reflow a 150-character prompt, enters and
  leaves the alternate screen, runs a thirty-block fixture against `-sl 40`
  for page eviction, checks with Kitty event reporting on in a raw child
  that the `insert-seven-bit()` override delivers the press and release
  while the default binding delivers nothing, that a twelve-pair burst in
  one flush runs twelve navigations with every press and release owned and
  nothing delivered, that releasing Ctrl before the arrow still suppresses
  the arrow's release so only a sentinel key reaches the child, that a
  focus change while Ctrl+Shift+Up is held leaves a later plain Up's press
  and release intact, and runs a fixture without markers. The self-test
  also indexes six thousand prompts and walks them all, and redraws an
  older prompt row without growing the walk, and writes three thousand
  prompts into a 40-line scrollback without searching to confirm the live
  mark count never exceeds the retained rows.
- Notification urgency: libghostty's desktop-notification callback (OSC 9
  iTerm2 form with an empty title, OSC 777 `notify;title;body`) reaches the
  application through the backend-neutral `notification` effect. The
  application logs `notification received count=<n> focus=<in|out>` plus
  byte previews of the title and body, and when the terminal widget is
  unfocused it sets `XUrgencyHint` through `XtpUrgencyApply`
  (`src/urgency.c`), which reads WM_HINTS, flips only that bit, and writes
  it back so Xt's input, state, and group hints survive. Focus is tracked by
  a second `FocusChangeMask` handler on the VT widget, mirroring the
  widget's own notion; the next focus-in clears the hint. Repeated
  notifications log but do not rewrite the hint; a notification before
  realization is kept as pending state and applied right after
  `XtRealizeWidget`; desired and applied urgency are tracked separately so
  a failed WM_HINTS update is retried on the next notification or focus
  change rather than latched; the Xvfb test seeds every WM_HINTS field
  with distinctive values first and requires all of them back unchanged; after `DestroyApplication` the shell pointer is NULL
  and the effect cannot fire because the terminal is freed first. Valid
  ConEmu OSC 9 forms, including complete `9;4;<state>` progress reports,
  are separate protocols in the core; libghostty deliberately treats an
  incomplete or invalid ConEmu shape (`9;4`, `9;4;`, `9;4;5`) as iTerm2
  notification text, so those bodies do arrive here and can set urgency.
  They are not suppressed because that would also drop legitimate iTerm2
  text; the self-test pins both sides of that line. The `notification effect` self-test pins OSC 9 and
  OSC 777 decoding with both terminators, byte-by-byte delivery, neighbors
  with a CPR reply, UTF-8 and empty bodies, valid ConEmu forms staying
  silent while incomplete ones notify, malformed OSC 777, PTY silence, and
  the null-effect path. `xvfb-notification-urgency` drives a
  child through four notifications while `xtp-wm-urgency` moves X focus and
  reads WM_HINTS externally: unfocused sets the hint with every other field
  equal to the baseline, a repeat leaves it set once, focus-in clears it,
  a focused request never sets it, unfocus-then-notify sets it again with
  a CPR reply intact, and exiting with the hint set is clean. Desktop
  delivery stays a separate optional adapter (N2).
- Unknown APC diagnostics: the adapter sets libghostty's unknown-sequence
  callback with `XTP_UNKNOWN_APC_CAPTURE_LIMIT` (256) retained bytes and
  forwards APC-tagged reports through the backend-neutral `unknown_apc`
  effect; the application logs each one with `XtpLogBytePreview` under the
  `terminal` subsystem at info level as `unknown APC ignored
  truncated=<yes|no> bytes=<n> preview="..."`. No reply is written and no
  Ops family applies. The `unknown APC` self-test pins whole, 8-bit-ST,
  byte-by-byte, and ESC-split delivery, surrounding text and a CPR reply,
  embedded C0 bytes including NUL, ESC-terminated and CAN/SUB-aborted forms, empty and
  identifier-prefix drops, truncation at and beyond the limit, the Kitty
  graphics reply and glyph-protocol silence, parser health afterwards, and
  silence without an effect. `xvfb-unknown-apc` sends whole, fragmented,
  C0-laden, and oversized APCs around a Kitty query and DSR through the
  real PTY, requires exactly those two replies, and diffs the logged event
  lines. The callback is APC-only; OSC/CSI visibility stays an upstream ask.
- Answerback: `answerbackString` is copied into the backend and, on
  libghostty's ENQ effect, written straight through the host PTY effect with
  an empty result returned to the core, so the reply filters (mode-12
  rewrite, Color Ops, Tcap Ops) never touch it and its length is not bounded
  by libghostty's reply buffer; an empty or unset value sends nothing.
  `xvfb-answerback` reads the exact bytes for two ENQs through the real PTY
  and confirms the silent default.
- Startup cursor shape: `cursorUnderLine`/`-uc` and `cursorBar`/`-barc`
  resolve through `XtpTerminalStartupCursorShape` (underline beats bar) into
  libghostty's default cursor style, which DECSCUSR 0, an omitted parameter,
  and a full reset return to; application DECSCUSR shapes override it and the
  blink observer is untouched. `xvfb-cursor-shape` samples the cursor cell for
  each shape and for override and restoration.
- SGR 58/59 underline colors: the render cell carries libghostty's underline
  color, the visual cell resolves an explicit color to opaque ink of its own
  while the default follows the text color through faint, inverse, and
  selection, and both renderers draw every underline style with it;
  `xvfb-underline-color` samples every style under both renderers, inverse
  video, selection, and a translucent background; it also checks faint/reset
  behavior, independent overline/strikethrough colors, color-only redraw and
  palette changes to existing underlines. Explicit ink staying opaque under
  faint is an intentional difference from Ghostty.
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
  selects a real italic or oblique face when one is available. Box-drawing,
  block-element, braille and Powerline separator characters are rasterized
  from the cell geometry when the primary face lacks them, on the bitmap
  path, or under `forceBoxChars`.
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
- OSC 8 hyperlink targets exposed through the backend-neutral terminal API,
  plus bounded frontend detection of visible HTTP and HTTPS URLs across
  soft-wrapped rows. Explicit OSC 8 state wins; inferred URLs trim sentence
  punctuation and unmatched closing delimiters without changing terminal or
  copied text. Shift-hover underlines the selected target occurrence and
  Shift+Button 1 launches HTTP(S) with `xdg-open`; other explicit schemes are
  deliberately inert. Ordinary selection and the Ctrl+button menus keep their
  established gestures.
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
translations are the pinned v0.5 compatibility behavior and must not be renamed
as cosmetic cleanup. They are not necessarily the permanent identity model.
The leading long-term candidate is instance `revenant`, class `XTerm`, but any
transition must first account for instance-specific X resources, WM_CLASS-based
window rules and grouping, `-name`, compatibility invocation, app-default
lookup, desktop integration, diagnostics, and generated configuration. Class-
based `XTerm*` resources would continue to match that candidate; `xterm.*`
instance resources would not.

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
2. **Refresh the Ghostling inventory.** Review the current Ghostling checkout
   against the libghostty commit selected for v0.5 rather than carrying the
   2026-08-24 comparison forward by assumption. Exercise each advertised
   capability at the visible X11 boundary, confirm the existing claims with
   representative evidence, reconcile the two parity matrices, and record any
   capability Ghostling has added or removed. This is an inventory exercise for
   v0.5, not a parity-implementation gate: fix only release-blocking regressions
   or a clearly small dishonest classification discovered by the review. Keep
   actual status (`Present`, `Partial`, or `Missing`) separate from release
   scope. Kitty graphics remains Missing and is deliberately deferred from
   v0.5, so this release must not claim complete Ghostling parity or the
   existing MVP gate.

   The 2026-09-02 source inventory compared Ghostling `63842bf8e5e4` (also the
   current remote head) with Revenant's selected libghostty
   `5aeb693b7727`; Ghostling itself pins `f64f4aca2c29`. Its advertised list is
   unchanged, but its code now has a real, deliberately simple Kitty PNG and
   placement renderer. Revenant remains 11 Present and 1 Missing against that
   advertised list, with Kitty graphics the only missing item. The roadmap's
   stale `Partial` resize label was corrected to `Present`.

   Upstream triage on 2026-09-03 included Ghostty Discussions, which are its
   pre-work request queue. Do not open a duplicate animation request:
   [discussion 13379](https://github.com/ghostty-org/ghostty/discussions/13379)
   is the active request for Kitty animation frames and links the older issue
   and discussion history. No current Discussion was found for the other
   concrete external-renderer gap: resolving Unicode-placeholder/virtual
   placements into drawable placement geometry through libghostty's C API.
   The closest contribution precedent is
   [discussion 12347](https://github.com/ghostty-org/ghostty/discussions/12347),
   where libghostty Kitty-graphics inspection work was welcomed. Implement
   ordinary static placements first; if that confirms the boundary, take the
   resolved-virtual-placement request to one narrowly scoped new Discussion
   and add any required external-renderer animation-tick detail to 13379.
   Ghostty's submission form requires the author to write in their own voice,
   so preserve technical facts and reproductions here rather than preparing
   text to paste as a submission.

   The broader source comparison found two small terminal-effect differences.
   XTWINOPS size reports and a product-owned XTVERSION reply are now integrated
   and covered before and after resize. Full Ghostty formats XTVERSION as
   `ghostty <version>`, Ghostling reports `ghostling`, and xterm 411 reports
   `XTerm(411)`; Revenant deliberately reports `revenant(<version>)`. DA
   replies are configured through the device-attributes callback rather
   than libghostty's `CSI ? 62 ; 22 c` default: the advertised feature set,
   clipboard code, DA2 version value, and DA3 form were each audited against
   implemented behavior instead of copied from Ghostling; the drift ledger
   records the evidence. The Clang build and 30/30 maintained
   tests, formatting check, and `git diff --check` passed after the terminal
   report implementation. A full visible-boundary re-exercise of every matrix
   row remains part of the release inventory if stronger acceptance evidence is
   desired.

   Welcome currently has small duplicated X resource/font-probe logic beside
   `config_report.c`, and its compositor-ready loop duplicates the opacity
   harness. Consolidate those only when touching the owning report and Xvfb
   helper modules; the v0.5 review found no remaining behavioral dependency on
   that cleanup. Keep `-version` concise and xterm-like. The detailed
   `-report-config` and `-welcome` support block, rather than `-version`, carry
   the selected backend revision.
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
   TDN: `esc-double-size-lines`.
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
5. **Defer xterm session logging to v0.6.** Keep `-/+l`, `-lf`, the `logFile`
   and `logInhibit` resources, and the `logging` main-menu action classified as
   one coherent v0.6 PTY-output-tee slice. Do not expose a partial v0.5 surface.
   This user-requested session transcript remains distinct from Revenant's
   structured diagnostic `-log` severity; neither option may accidentally
   enable the other.
   TDN: `logging-session-transcript`.
6. **Add a bounded `revenant -welcome` setup assistant. Completed.** The shipped
   path initializes the real Xt widget and resource database, prints its report,
   and exits before terminal-backend creation or PTY spawn. It distinguishes
   app-default availability, the server `RESOURCE_MANAGER`, and relevant live
   instance/class settings; reports renderer, cell geometry, display DPI, and
   actual fontconfig matches; conservatively flags an unconfigured small bitmap
   font or bitmap use on a high-density display; and emits a review-before-use
   scalable-font `XTerm*` starter fragment.

   `/etc/os-release` is parsed as data with a whitelist and size bound. Tested
   Debian-, Fedora-, and Arch-family mappings provide install commands for
   missing `xrdb`, scalable monospace, emoji, and CJK capabilities, while an
   unknown family receives generic advice. No network, package installation,
   resource edit, or `xrdb` mutation occurs. Children now receive generated
   `TERM_PROGRAM` and `TERM_PROGRAM_VERSION`; matching host identity and a
   terminal stdout gate the advanced sample. The resolved widget's Xft and
   emoji state is diagnostic data, not evidence about the terminal displaying
   stdout. The bounded support block
   includes OS/version/architecture, generated program and reviewed backend
   identity, renderer, instance/class, font matches, app-default status, and
   host identity without home paths or arbitrary environment content. Focused
   self-tests and Xvfb cases cover parsing/family fallback, conventional and
   simulated high-density policy, bare/configured resources, tool absence,
   host gating, redaction, and unrealized ARGB-widget teardown. The last case
   also fixed a pre-existing `BadColor` exit race: display close flushes Xt's
   cached color converters, so the client colormap must remain alive until that
   flush completes rather than being freed prematurely.
   The report's optional appearance guidance points to `terminal.love` for its
   scheme catalog, live demo, and default Xresources export, but performs no
   network request or automatic import.

   The retained design contract is that this remains a local,
   read-only diagnosis for someone who may have neither an xterm configuration
   nor suitable terminal fonts, not merely a decorative sample and not an
   installer. Reuse `-report-config` and the renderer's own font-resolution
   paths to summarize the running version and identity, the resolved
   app-defaults file, live `RESOURCE_MANAGER` state and relevant merged
   resources, configured bitmap/Xft faces and fallbacks, and representative
   emoji/CJK coverage. Turn findings into short, actionable recommendations: a
   starter `XTerm*` resource fragment, the preferred `~/.Xresources` plus
   `xrdb -merge` workflow, the limited `~/.Xdefaults` fallback, and fonts or
   optional tools that are missing. Point to `revenant(1)`, `-report-config`,
   the detailed documentation, and the issue-report path.

   Define “healthy” in terms of the resolved experience rather than merely the
   presence of an xterm resource file. A stock tiny bitmap `fixed` face on a
   high-density display deserves a readability recommendation even when an
   `XTerm.ad` exists; a deliberately configured bitmap setup does not. Base the
   decision on resource provenance, active renderer, resolved cell pixel size,
   Xft DPI/display characteristics, and actual font matches. Characterize the
   threshold on conventional and 4K displays before freezing it, keep the
   recommendation conservative, and show the exact resource fragment rather
   than silently changing the font. The default suggestion should use scalable
   fontconfig aliases and a readable size, then add emoji or CJK packages only
   when representative coverage is missing.

   Parse `/etc/os-release` as data, never by sourcing it, and use only a
   whitelisted `ID`, `ID_LIKE`, `VERSION_ID`, and display name to select
   maintained package suggestions for the supported distribution families.
   Pair that with `uname` architecture. Unknown distributions get generic
   capability names and documentation rather than a guessed command. Package
   mappings require fixtures and periodic verification; missing metadata must
   degrade to an honest generic recommendation.

   Revenant child processes should receive `TERM_PROGRAM` and
   `TERM_PROGRAM_VERSION` set from generated build identity, rather than
   inheriting a misleading outer-terminal value. When `-welcome` sees its output
   is hosted by the matching program/version, it may show an advanced rendering
   sample covering color emoji, ZWJ families, modifiers, flags, variation
   selectors, combining text, and wide CJK. In another terminal it should keep
   the setup report useful, label the host honestly, and ask the user to run the
   visual sample inside Revenant instead of grading another emulator.

   End with a stable, plain-text, copyable support block containing at minimum
   OS distribution/version, architecture, Revenant version, selected terminal
   backend/reviewed libghostty identity, renderer, application instance/class,
   resolved primary and emoji fonts, app-default status, and whether the host
   was identified as Revenant. Do not include usernames, home paths, arbitrary
   environment values, or the complete resource database. The detailed
   `-report-config` remains the opt-in attachment when more evidence is needed;
   the same small collector may later back a dedicated support-report option.

   Do not make xterm, `xrdb`, `fc-match`, or a particular font package a hard
   dependency merely to support this assistant. Fontconfig is already a runtime
   dependency and can answer font questions directly; Xlib exposes the server
   resource database without invoking `xrdb`. Detect optional tools and explain
   how to obtain them using verified per-distribution documentation, but never
   install packages or edit/load resources automatically. Separately evaluate
   whether packages should recommend xterm for its installed `XTerm.ad`, should
   recommend the distribution's `xrdb` package, or should instead expand
   Revenant's compiled fallback resources from the maintained app-defaults
   reference. A hard dependency needs evidence that the terminal cannot provide
   a sound standalone first run. Keep the assistant offline, avoid sensitive
   environment output, and never claim that visual coverage was mechanically
   verified merely because a sample was printed.
7. **Pin the current name/class behavior and keep the transition open.** For
   v0.5, application class `XTerm`, instance `xterm`, and widget `vt100` remain
   the defaults for both `revenant` and `xterm+`. Explicit `-name` and `-class`
   override application and shell identity before `XtOpenDisplay`; Xvfb pins
   WM_CLASS, custom instance/class resource lookup and report provenance, and
   xterm's `-e` child-basename default for WM_NAME and WM_ICON_NAME. Invocation
   through another symlink does not silently change resource identity. This
   describes how v0.5 works; it does not close the long-term decision.

   Evaluate `revenant`/`XTerm` as the leading successor along with invoked-name
   identity, dual-resource migration, and other viable models. The comparison
   must cover `xterm.*` versus `XTerm*` resources, installed `XTerm.ad`, WM_CLASS
   consumers, desktop files, `-name`/`-class`, compatibility symlinks,
   configuration/report output, upgrade warnings, and rollback. Do not change
   the default until existing users have a documented, tested migration path.
8. **Remaining keyboard/XIM compatibility matrix. Completed.** The exact-byte
   Xvfb matrix covers ordinary and application cursor/keypad modes,
   Shift/Ctrl/Alt/Super combinations, function and editing keys, XIM Compose,
   a remapped non-US character, and no-XIM Unicode fallback. F13
   press/repeat/release is covered under Kitty reporting with a synthetic XKB
   mapping. Keep libghostty's intentional fixterms default visible in the
   drift ledger, and turn future application failures into named fixtures.
9. **Finish the small, visible xterm-compatibility set. In progress.** `color0`
   through `color15` are implemented. For v0.5, wire `cursorUnderLine`/`-uc`
   and `cursorBar`/`-barc` to libghostty's existing default-cursor-style option,
   preserving xterm's underline-over-bar precedence. Add
   `clear-saved-lines()` using libghostty's public full-reset operation, whose
   RIS behavior already clears screen contents and scrollback, and add xterm's
   Meta+Button-2 binding with backend and Xvfb coverage. Compare both slices
   against patch 411. Defer Shift+Select keyboard selection and Scroll Lock to
   v0.6 because each needs new policy and widget state. Keep Alt+Return
   fullscreen with the coherent fullscreen resource/menu/EWMH slice in v0.6;
   the client message is small, but policy and useful window-manager testing are
   not part of the v0.5 quick fixes. Do not turn v0.5 into an exhaustive
   resource-catalog exercise. Startup cursor shape is now complete (V2);
   the action gaps retain the identifiers below.
   TDN: `ui-clear-saved-lines`, `selection-keyboard-extension`, `input-scroll-lock`,
        `ui-fullscreen-toggle`.
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
- The refreshed Ghostling inventory and command-line triage are recorded, and
  the read-only `-welcome` setup audit passes without network access or system
  mutation. Resource/app-default/font findings and recommendations are covered
  on configured, intentionally bare, and simulated high-density systems.
  `/etc/os-release` family fixtures, unknown-distribution fallback, missing-tool
  and missing-font cases, host-terminal detection, advanced-sample gating, and
  redaction of the copyable support block are tested. The
  checklist and xterm differences ledger agree with shipped behavior and
  explicitly identify deferred Kitty graphics and session logging; v0.5 is not
  advertised as full Ghostling parity.
- Unknown options fail visibly, `-help` is useful without an X display, and
  `-e` preserves arbitrary child arguments. The name/class record pins v0.5
  behavior, identifies `revenant`/`XTerm` as a leading future candidate, and
  leaves the final transition decision explicitly open pending the impact
  audit.
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
  TDN: `selection-keep-selection`, `selection-keep-clipboard`, `selection-icccm-targets`,
       `paste-control-filtering`, `bell-visual`, `notification-x11-urgency`,
       `ui-scrollbar-styles`, `ui-menu-action-parity`.
- Automatic package installation, resource-file editing/loading, and an
  interactive multi-step welcome UI are out of scope; v0.5 requires the
  read-only setup analysis and recommendations described above.
- Additional shaping scripts, XIM layouts, and color-font versions are valuable
  matrix expansion after each underlying path has one adversarial acceptance
  fixture.

### Runtime dynamic colors and Color Ops — mostly closed

OSC 10/11/12 now reach pixels: `XtpTerminalRender` reads libghostty's
effective default colors into the frame and forces a full repaint when they
change (libghostty sets no dirty flag for them), and the widget keeps separate
effective pixels that `ApplyFrameColors` updates before painting, including
the window and scrollbar background. The configured resource pixels remain
the defaults pushed to libghostty, so OSC 110/111/112 restore them, and the
widget-level `-rv` swap still re-pushes swapped defaults. DECSCNM continues to
swap only default colors at paint time. `CSI ? 996 n` answers from the
displayed background's perceived luminance and mode 2031 sends unsolicited
reports when a render observes the scheme flipping.

The Color Ops policy follows xterm: `allowColorOps` overrides the list;
`disallowedColorOps` (default `SetColor,GetColor,GetAnsiColor`) applies when
it is false. Enforcement happens before libghostty parses a denied request:
a denied reset is spoiled at its selector, a denied set item in an OSC 10-19
list is withheld and forwarded as a query so successive selectors stay
aligned, and the PTY write effect drops replies to denied queries and to those
substitutes by the selector and palette index each reply carries, one decision
per query occurrence in order, with the index read as libghostty reads it
(ignored C0 bytes, a leading plus, leading zeros) and any unreadable denied
index blocking the whole list's palette replies; permitted items in the same
list still apply and no denied color is ever applied, painted, or reported. The filter forwards kept replies one at a time without
allocating, and drops unrecognized output while a denial is pending, so it
cannot fail open. The observer reports the selector, each item's start and end, and
the first terminator byte, where libghostty dispatches the command; never
roll a color back after the fact, since that races the render and disturbs
the parser state of whatever control follows. The observer remains
a temporary seam; seek a public libghostty permission callback and remove
the extension when available. `xvfb-dynamic-colors` samples pixels for the
three colors and the border across set, reset, an opacity change, and each
policy shape, checks exact replies and scheme reports; `xvfb-color-ops`
covers the live toggle.

Remaining: `allowSendEvents` interaction, policy for libghostty's Kitty
OSC 21 colors, and xterm's DECSCNM-relative OSC 10/11 addressing (libghostty
addresses the normal colors, so under DECSCNM OSC 10 changes what is shown as
the background).
TDN: `policy-color-ops-send-events`, `policy-kitty-color-ops`,
     `osc-dynamic-colors-reverse-video`.

### Complete xterm Window Ops support — open

Complete the full patch-411 Window Ops permission category, not only OSC 52
or the `CSI ... t` XTWINOPS family. The live menu toggle and clipboard checks
are the starting point, not completion of this work. The
[roadmap](docs/maintainers/roadmap.md#6-broader-xterm-compatibility-and-packaging)
tracks the feature; TDN's [policy inventory](tdn/docs/policies/window-ops.md)
maps the controls across sequence families.

Deliver this in reviewable slices:

1. Inventory every operation in xterm's `tblWindowOps` and every
   `AllowWindowOps` call site against the pinned source. Record whether the
   selected libghostty exposes its effect or query, whether Revenant already
   implements it, and which policy check is missing. Preserve parser ownership
   in libghostty; missing public hooks are upstream API asks, not a reason to
   add a second escape parser.
   TDN: `policy-window-ops`.
2. Complete XTWINOPS window manipulation and reports: restore/minimize,
   move/resize in pixels or cells, raise/lower/refresh, maximize/fullscreen,
   window state and position, window/screen/cell geometry, title/icon reports,
   and title push/pop. Apply policy to the existing `CSI 14 t`, `CSI 16 t`,
   and `CSI 18 t` replies as well as newly implemented operations.
   TDN: `csi-1-t-de-iconify`, `csi-2-t-iconify`, `csi-3-t-move-window`,
        `csi-4-t-resize-window-in-pixels`, `csi-5-t-raise`, `csi-6-t-lower`,
        `csi-7-t-refresh`, `csi-8-t-resize-text-area-in-cells`, `csi-9-t-maximize`,
        `csi-10-t-fullscreen`, `csi-11-t-report-window-state`, `csi-13-t-position-report`,
        `csi-14-t-pixel-size-report`, `csi-15-t-report-screen-size-in-pixels`,
        `csi-16-t-report-cell-size-in-pixels`, `csi-18-t-text-size`,
        `csi-19-t-report-screen-size-in-cells`, `csi-20-t-report-icon-label`,
        `csi-21-t-title-report`, `csi-22-t-push-title`, `csi-23-t-pop`.
3. Complete the cross-family controls: `ColumnMode`, `SetWinLines`,
   `GetChecksum`/`SetChecksum`, `SetXprop`, and `StatusLine`. Retain OSC 52
   regression coverage and resolve or explicitly track its upstream parser
   differences. Keep OSC 0/1/2 title setting under its separate `allowTitleOps`
   policy rather than treating every window-related control as Window Ops.
   TDN: `dec-mode-3-deccolm`, `dec-mode-40-allow-3-to-resize`, `csi-decslpp`,
        `csi-decsnls`, `csi-decrqcra`, `csi-xtchecksum`, `osc-3-x-property`,
        `csi-decsasd`, `csi-decssdt`.

   OSC 52 parser parity has separate IDs for each observable difference.
   TDN: `osc-52-multiple-targets`, `osc-52-default-targets`, `osc-52-cut-buffers`,
        `osc-52-invalid-base64-clear`, `osc-52-reply-target`.
4. Make resources, the live menu, actions, configuration reporting, and support
   classifications agree. Preserve xterm's rule that `allowWindowOps: true`
   overrides the deny list, while false applies per-operation restrictions;
   cover names, numeric aliases where supported by xterm, wildcards, negation,
   and restoration of the configured restrictions when toggled off.
   TDN: `policy-window-ops`.

Acceptance requires exact request/reply and denied-operation tests, observable
X11 effects, live enable/disable coverage beyond the clipboard, and
differential checks against patch 411 with an isolated HOME. Use a window
manager for stacking, minimize, maximize, and fullscreen checks; bare Xvfb
cannot establish those behaviors alone. Keep unsupported operations and
upstream blockers explicitly partial until they work end to end, and run the
maintained compiler/backend, sanitizer, formatting, and documentation matrix.

### Complete xterm Title Ops compatibility — open

The live **Allow Title Ops** toggle and `allowTitleOps` resource are
implemented, defaulting to true. They gate the displayed title changes
exposed by libghostty and applying saved labels on pop. Title reports,
pushes, and pops separately consult Window Ops; a permitted normal pop
consumes its entry even when Title Ops blocks applying the labels. Preserve
that tested separation. TDN's [Title Ops inventory](tdn/docs/policies/title-ops.md)
records the policy scope and its overlap with Window Ops.

The patch-411 source audit leaves these gaps:

- **Independent icon-name changes.** Surface OSC 1 and the icon half of OSC 0
  through a backend effect, so OSC 0 updates both labels and OSC 1 changes
  only the icon. Both must obey the live Title Ops permission. Preserve OSC 2
  as a window-title-only update. Seek a libghostty callback carrying the
  selector rather than adding another OSC parser.
  TDN: `osc-0-title-icon`, `osc-1-icon-name`, `osc-2-title`.
- **Title encoding modes.** Implement the `titleModes` resource and
  XTSMTITLE/XTRMTITLE (`CSI > Pm t` / `CSI > Pm T`), including hexadecimal
  input/output, UTF-8 input/output, and default/reset semantics. Input decoding
  belongs to the Title Ops path; report encoding is companion work under the
  Window Ops report permission, not another Title Ops authorization check.
  TDN: `resource-title-modes`, `csi-xtsmtitle`, `csi-xtrmtitle`, `title-modes-hex-input`,
       `title-modes-hex-reports`, `title-modes-utf8-input`, `title-modes-utf8-reports`.
- **UTF-8 title resources and properties.** Implement `utf8Title` and the
  `utf8-title` menu/action, with the patch-411 locale and `allowC1Printable`
  interactions. Synchronize ICCCM `WM_NAME`/`WM_ICON_NAME` and EWMH
  `_NET_WM_NAME`/`_NET_WM_ICON_NAME`, including deleting stale EWMH labels
  when the encoding policy requires it. The current setter only calls Xt's
  title/icon resources; reading UTF-8 WM properties does not complete this.
  TDN: `resource-utf8-title`, `x11-utf8-title-properties`.
- **Title normalization and limits.** Match `ChangeGroup`'s title-path
  control-character normalization, hex validation, and 1000-byte rejection
  before decoding. The pop path has the 1000-byte check, but ordinary OSC
  title changes currently use libghostty's 1024-byte truncation and no matching
  frontend rejection. Cover non-ASCII labels, invalid input, and boundary
  lengths rather than claiming parity from ASCII examples.
  TDN: `title-input-normalization`, `title-input-byte-limit`.
- **Resource/action parity.** Register `allow-title-ops(on/off/toggle)` in the
  translation action table, using the same live state and checkmark as the
  menu. Honor xterm's `allowSendEvents` interaction: it disables effective
  Title Ops and makes the permission toggle insensitive. Audit and implement
  the `sameName` resource's suppression of redundant title/icon property
  updates. These remain missing despite the working menu toggle.
  TDN: `action-allow-title-ops`, `policy-title-ops-send-events`, `resource-same-name`.

Use `misc.c` (`ChangeGroup` and label setters), `charproc.c` (OSC dispatch,
title modes, and reports), `ptyx.h` (`AllowTitleOps`/`AllowXtermOps`), and
`menu.c` in the pinned xterm checkout as the source oracle. Acceptance must
compare actual ICCCM/EWMH properties and exact report bytes against xterm,
with an isolated HOME and relevant locales, and cover menu and translation
actions plus permitted/denied operations. Preserve the completed external-WM
property and stack regressions. Keep unresolved backend hooks and deliberate
differences explicit; neither a working toggle nor title reports alone means
full Title Ops compatibility.

### Preserve for future releases

The broader ideas remain project direction rather than v0.5 promises:

- Kitty graphics and scrollback search are expressly punted from v0.5, and
  neither is a v0.6 announcement gate. They remain possible later features,
  not work to squeeze in after scope freeze. Their bounded chunks are K1–K4
  and U2–U3 in the dispatch guide.
  TDN: `apc-kitty-static-images`, `apc-kitty-temp-file-transfer`,
       `apc-kitty-shared-memory`, `apc-kitty-unicode-placeholders`, `apc-kitty-animation`,
       `search-scrollback-literal`, `ui-search-overlay`, `search-copy-match`.
- Tabs, splits, profiles, and live configuration reload remain open questions.
  They are acceptable only if they respect Xt/X11 resources and the
  window-manager contract rather than turning Revenant into an unrelated shell.
  TDN: `ui-tabbed-terminals`, `ui-split-terminals`, `configuration-profiles`,
       `configuration-live-reload`.
- The deeper DEC/xterm tail, sixel, ReGIS, Tektronix 4014, printer controls,
  locator operations, and exhaustive command-line/resource compatibility. The
  broad inventory remains grouped; give newly identified DEC operations their
  own IDs when the scope is audited.
  TDN: `dcs-sixel`, `dcs-regis`, `tek-4014-graphics`, `csi-mc`, `csi-dec-media-copy`,
       `csi-decelr`, `csi-decefr`, `csi-decsle`, `csi-decrqlp`,
       `xterm-resource-compatibility`.
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
3. **The name/class transition decision is evidence-backed.** Invocation names,
   `-name`, `-class`, WM_CLASS, app-default lookup, existing `xterm*` and
   `XTerm*` resources, compatibility symlinks, desktop integration, and storage
   of generated customization have a documented and tested model. The decision
   may retain `xterm`/`XTerm` for another release or move toward
   `revenant`/`XTerm`; it must not describe the current default as permanent
   without completing the migration analysis.
   TDN: `x11-resource-identity`.
4. **The newcomer path is release quality.** Installation, `-welcome`, the
   manual and website, configuration examples, migration guidance, known
   limitations, diagnostics, package removal, and issue reporting agree and
   have been followed successfully from clean systems.
5. **Daily-driver confidence supports the announcement.** The maintained
   shell, editor, multiplexer, SSH, input, resize, rendering, selection,
   scrollback, font, and packaging matrices pass, and known serious defects are
   resolved or explicitly judged incompatible with announcing.
6. **xterm session logging is complete and safe.** `-/+l`, `-lf`, `logFile`,
   `logInhibit`, and the `logging` menu action share one tested PTY-output tee.
   Start/stop behavior, file creation and permissions, append/truncate policy,
   errors, and nonblocking terminal delivery are compared with xterm, and this
   transcript facility remains independent of diagnostic `-log` severity.
   TDN: `logging-session-transcript`.

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
cut buffers, and OSC 8 launch policy; legacy/fixterms keyboard delivery;
Kitty keyboard press, repeat, and release; synchronized-output hold,
timeout, and resize behavior; OSC 52 policy, selection ownership, and exact
query replies; and XTWINOPS title stack restoration, exact title reports,
and the live Allow Window Ops toggle. Release package configurations use
`-Dxvfb-tests=enabled`, which makes missing Xvfb or libghostty an immediate
configuration error, and `tools/check-release-tests` rejects skipped suites.
The live xterm font/geometry oracle remains an explicit side test. Split the
remaining harness into focused tests and grow Xvfb coverage; do not treat any
one suite alone as evidence of full UI compatibility.

The normal full matrix currently contains 50 tests for each libghostty build
and 8 for the stub build. One of those is `internal-branding`, which scans
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
TDN: `session-snapshots`, `session-pty-fanout`, `scrollback-asynchronous-history`,
     `ui-independent-viewports`, `session-snapshot-resync`.
