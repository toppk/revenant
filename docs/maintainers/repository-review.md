---
man: revenant-repository-review
section: 8
manual: maintainers
description: September 2026 maintenance review decisions
---

# Repository maintenance review — September 2026

The scoped R1–R10 review is accepted. This record preserves the design
rationale; future extractions require a new bounded assignment. The current
implementation queue remains in the repository's `todo.md`.

| Area | Accepted outcome and evidence |
| --- | --- |
| R1 | Xt-registered action lookup and source-derived default translations; `xvfb-report-config`. |
| R2 | Application-identity opacity lookup and isolated regressions; retain the two startup/widget stages. |
| R3 | `just check-all` covers local CI obligations; required real-backend prerequisites and no-skip gates. |
| R4 | Stable probe case IDs drive actions/options; renamed-path Go regressions. |
| R5 | Shared build declarations and `tests/meson.build`; retain separate compilation pending measurement. |
| R6 | Four Xvfb suites use the shared harness with isolated resources and cleanup checks. |
| R7 | Eight independent noninstalled URL cases; installed diagnostic retained. Further groups remain open. |
| R8 | General control-observer naming and per-consumer upstream requirements; behavior unchanged. |
| R9 | Widget-owned `VtSearchRuntime` and real/stub/ASan lifecycle coverage. |
| R10 | [Search contract](search.md), queue reconciliation and documented TDN generation boundary. |

T1b and login-shell startup were already committed in `712f7b7` and
`160c02d`; they are retired from the open queue. Version 0.8.0 was released
from `fb1745c`. Automated coverage does not supply human probe assessments.

## Design records

The following dated audits describe the pre-extraction state and the approved
boundaries. R7b, R8b and R9b have since been implemented and accepted; proposed
acceptance steps below are retained as rationale, not new dispatch instructions.

### R9a — search runtime-state ownership audit (2026-09-24)

**Recommendation: make one narrow widget-state extraction in R9b, if approved.**
Keep the already separate `vt_search.c` behavior and opaque backend search model;
group only the contiguous search runtime fields of `Vt100Part` into an embedded
`VtSearchRuntime`. This gives the timer, tracked marks, popup handles and paint
snapshot one explicit widget-owned lifecycle. It also keeps mutation in
`vt_search.c` and replaces direct reads in input and drawing code with the
existing search accessors. The benefit is a reviewable ownership boundary for
rebind/teardown and cached highlights, not a reduction in field count or
another source-file split.

The widget's search fields are `search`, `search_active`, `search_unavailable`,
the fixed query bytes/length, active and starting-viewport marks, the
at-bottom flag, timer, overlay and label handles, and the frame top/span/visible
match cache in `src/vt_widgetP.h`. No search field is an Xt resource. The
resource table in `src/vt_widget.c` uses `XtOffsetOf(Vt100Rec, vt.field)` for
resource fields before and after this block (for example `copyFlashColor` and
`scrollBar`). R9b must keep every resource's name, type and offset expression
valid and test that Xt still reads settings on both sides of the moved block;
it must not nest or replace resource storage as part of this extraction. The
current search fields rely on zero initial values; an aggregate initializer
must preserve that before a terminal is bound.

`vt_search.c` owns the lazily allocated `XtpTerminalSearch *` (created only for
an open search), the two terminal cell marks, the `search_visible` heap buffer,
the Xt timer registration and the top-level shell event-handler registration.
It creates an override-redirect popup and label, whose widget lifetimes belong
to Xt's parent hierarchy; the stored handles are references, not separately
freed objects. The bound `XtpTerminal *` inside the backend search is borrowed:
`XtpTerminalSearchFree` must precede freeing or replacing that terminal.
`terminal_ghostty_search.c` owns its compiled query, tracked scan cursor and
match endpoints, and its match/unit/pending/cluster buffers; its reset drops
query and tracked references while retaining allocated buffer capacity until
free. The visible spans are copied value snapshots, not backend-owned pointers.
Text returned by `XtpTerminalSpanText` is temporary owned memory; the
selection owner copies it before `CopyActive` frees it. The stub's search
constructor returns NULL, so opening search must remain a harmless unavailable
path.

Lifecycle: opening after realization lazily creates the backend object and
popup, records a viewport mark, installs the shell move handler, starts the
query and schedules scan or retry timers when needed, and switches input/output
policy. Query edits cancel the old timer, clear the active mark, reset the
backend query and repaint. A close
cancels the timer and backend scan, removes the handler, releases both marks,
pops down the overlay, and optionally restores the viewport; the backend
object, popup and visible buffer are reused. Resize only repositions the popup.
`VtRenderTerminal` refreshes visible spans before rendering, and drawing reads
their cached classification; changes invalidate/redraw unless synchronized
output defers it. `XtpVtSetTerminal` calls `VtSearchTerminalChanged` before
replacing the borrowed terminal, closing without viewport restoration and
freeing the backend search so the next open binds a new one. Widget destruction
cancels any timer, removes an active handler, frees marks, backend search and
visible buffer, then Xt tears down its child widgets. Application teardown
unbinds the widget before freeing the terminal.

Keep the side-effecting boundaries as functions: `XtpVtSearchActive` controls
the application's pinned-output feed, `VtSearchKey` owns keys and may scroll,
ring, copy PRIMARY or redraw, and `VtSearchPrepareFrame` may prune stale
backend matches. `VtSearchCellHighlight` is a paint-time read of that prepared
snapshot. `ApplyQuery`, timer stepping and close route through
`SearchRepaint`, which invalidates the frame and respects synchronized-output
deferral. None should become an `XtGetValues`/`XtSetValues` shortcut or a raw
field toggle. The backend's `XtpTerminalSearchMatches` and `Navigate` are also
not pure getters: they drop dead or changed matches. The existing search
behavioral contract is in the
[scrollback search contract](search.md), with the API in
`src/terminal.h`.

R9b acceptance should cover `xvfb-search` and the `search matching`/`scrollback
search` self-tests, plus a focused widget lifecycle case that opens a running
search, rebinds to a different terminal and to NULL, then frees the old core;
it should assert no stale timer/handler, mark or highlight and successful reopen
on the new core. Exercise Escape versus Enter viewport behavior, alternate-
screen retry, resize/repaint and synchronized-output deferral. Destroy with a
pending scan timer and active popup, and repeat open/close/rebind, under the
real-backend ASan configuration to catch use-after-free and leaks. In the stub
configuration, verify failed/unavailable open and destruction remain clean.
Add an Xt resource smoke check for fields adjacent to the aggregate. Do not
move backend search state, change parser or matching semantics, or run a full
compiler matrix for this extraction.

### R2b — startup/widget opacity ownership audit (2026-09-23)

**Recommendation: retain the two resolution stages; no opacity refactor is
justified.** Their shared parser defines the requested value, but they own
different lifecycles. `BackgroundOpacityResource` queries the display database
with the R2a application name/class and the `vt100` path before any shell or VT
widget exists. `ConfigureApplicationVisual` interprets that startup request to
choose the shell visual, depth and colormap. Its `App.background_alpha` is a
startup snapshot used by the realization log, not live rendering state.

The VT widget later obtains its own `backgroundOpacity` string through Xt.
`ResolveBackgroundOpacity` checks the already-selected parent visual for an
alpha format, parses that string with the same disabled/number parser, and sets
`Vt100Rec.background_alpha` to the **effective** 16-bit alpha (opaque when the
visual has no alpha). `alpha_visual` and `alpha_format` describe visual
capability; the resource string describes a request. The widget's effective
alpha drives its background pixel, frame background surfaces and progress
track. Explicit content/ink remains opaque. Thus the widget, not `App`, owns
painted alpha after creation.

The menu reads the widget's effective percent and visual capability to set the
slider value/sensitivity. A slider change calls
`XtpVtSetBackgroundOpacityPercent`, which updates widget effective alpha and
background surfaces, progress colors, window/scrollbar background and redraw.
It does not change the resource string or reselect a visual. A later Xt resource
change can re-resolve opacity inside the existing visual, but cannot make an
opaque shell ARGB. This is the necessary startup/live boundary, not duplicate
ownership of one mutable value.

Preserve the existing fallback contract: `disabled` selects the opaque default
visual at startup and leaves the slider unavailable; invalid values warn and
resolve to opaque alpha, while an ARGB visual may still be selected when a
compositor is present. A valid `1.0` may likewise start opaque on an ARGB visual
and be lowered by the slider. Without a compositor or usable ARGB visual,
startup chooses the default visual and the widget remains opaque with the
slider unavailable. Consolidating the stages into one stored alpha would blur
requested versus effective state and risk changing those cases. Moving widget
resource resolution ahead of visual selection would also invert the required
creation order. Keep R2a's identity-based lookup and the shared parser;
field/function count alone supplies no benefit.

### R5c — widget object reuse audit (2026-09-23)

**Recommendation: retain separate compilation for now.** The graph exposes a
paint-helper sharing candidate, but there is no measured clean or incremental
build-time cost to justify another archive and its link/rebuild contract. The
40 `widget_sources` files are compiled for the app, stub font-reload helper,
font-reload paint helper and synchronized-output paint helper: 160 C compile
edges per real or stub configuration, 120 more than one copy. The real build
also compiles its three `terminal_ghostty*` adapter files for the app and two
paint helpers (nine edges, six repeated), while font reload compiles one
`terminal_stub.c` separately. A stub build compiles `terminal_stub.c` four
times (three repeated). These are graph counts, not measured time savings.

The common widget files use the abstract terminal interface, not a direct
Ghostty header; the real and stub adapters remain distinct. GCC compile
commands for the two paint helpers have matching flags after removing their
target-local include directories, and Ninja records identical header sets for
all 43 common real sources or 41 common stub sources. A paint-only archive
could therefore share 40 widget objects plus the selected backend adapter,
eliminating at most 43 real or 41 stub duplicate compile edges per build.
In the stub configuration, all four targets have matching non-target compile
options and header sets for their 41 shared sources, but that does not by
itself prove an all-target archive is the right ownership boundary.

In the real configuration, the app has additional dependency-provided system
include paths, and the stub font-reload helper lacks `-DGHOSTTY_STATIC` and
the Ghostty include path used by the app and paint helpers. All four receive
`-DHAVE_LIBGHOSTTY=1`, even though font reload links the stub adapter. Link
requirements also differ: only the app needs its application-only libraries,
and only real-backend targets link `libghostty-vt.a`. The shared widget objects
currently include neither generated `version.h` nor `config.h`; the real
adapter includes `version.h` in the app and both paint helpers, while
`config.h` is consumed by the app's desktop-notification source. The ASan
build uses Clang with `-fsanitize=address` and frame pointers for each target;
its objects must remain inside that instrumented build, not be reused from
GCC or stub directories.

If measured builds justify a later change, prototype only the paint pair's
common sources as a per-configuration static library. Keep app and stub
font-reload compilation separate initially. Acceptance must compare Meson
source, compile-option and generated-header dependency inventories across
real, stub and ASan builds; preserve installed/test-only targets and every
test name, argument, environment, timeout and skip condition. Check the link
line keeps the widget archive before the Ghostty archive and required X/math
libraries in the linker group; use whole-archive linkage if normal archive
extraction drops objects currently linked directly. Verify no duplicate or
missing symbols, rebuilds after a shared source or generated `version.h`
change, and helper rebuilds before their tests. Run focused font-reload,
paint/synchronized-output and startup/self-test coverage in real, stub and
ASan configurations. Measure comparable clean and incremental build times
before claiming a speedup or extending sharing to the app.

### R7a — installed self-test versus noninstalled unit coverage (2026-09-24)

**Recommendation: keep `-self-test` as a display-free installed smoke diagnostic;
extract internal rule checks into noninstalled Meson tests, starting with URL
matching.** At the R7a audit, `XtpSelfTest` ran 21 named foundation cases,
35 named backend cases and two named PTY cases, stopping at the first failure.
Between those tables it also runs inline charClass, terminal feed/render, key,
scrollbar/scrollback, cursor, mode, selection, hyperlink and resize checks.
The final success line reports backend and menu-entry counts.

The installed command should retain a small check of the *installed build*:
terminal/backend construction, a basic feed/render and key-encoding path,
scrollbar/resize smoke, the PTY lifecycle and exported environment/version,
and the backend/menu-count success line. Keep its display-free behavior,
`-self-test` spelling, `--self-test` alias, no-extra-options rule, zero/nonzero
success/failure statuses and diagnostic output contract. Keep both real and
stub paths; the stub's reduced render assertions must stay explicit. Move the
large fixture and edge-case checks into noninstalled targets: foundation
parsers/policies (including log levels, OS release, URL, Unicode, fonts,
opacity, glyphs, search and notification), protocol/backend state and render
fixtures (the 35 backend cases and detailed inline cursor/selection/hyperlink
checks), and the PTY queue stress case. Retain a minimal PTY spawn check in the
installed command, but report queue behavior as a separate build test.
This is a proposed ownership split, not a request to remove all those checks
in R7b; later batches need their own dependency and coverage review.

Actual CLI dispatch is `src/main.c` -> `XtpSelfTest`. Direct callers are the
Meson `self-test` registration and `tests/command-line.sh`, which runs both
spellings with `DISPLAY=` and compares their stdout; help output is asserted
there too. CI invokes it indirectly through complete Meson suites for GCC,
Clang, stub and ASan. `just test-gcc`, `test-clang`, `test-stub`, `test-asan` and
their aggregate likewise run those suites. Release package gates run the
Meson suite through `packaging/build-tarball`, Debian `dh_auto_test`, RPM
`%meson_test` and Arch `check()`. `tools/check-release-tests` requires every
registered case to pass without a skip for real release builds; the release
workflow uses those packaging paths rather than a separate CLI call. The man
page and `docs/configuration/command-line.md` advertise the installed
display-free command and alias; `docs/compatibility/drift.md` and
`docs/reference/probes.md` mention its diagnostic/case coverage. Historical
`HANDOFF.md` and maintainer notes refer to individual assertions. The
`--self-test` switches in `tools/probe-keymodes.py` and `tools/probe-colors.py`
belong to those separate Python probes, not Revenant's CLI.

R7b scope is only `SelfTestUrlMatch`: put its fixtures in one noninstalled
`xtp-url-match` test executable that compiles the same `src/url_match.c` used
by the application, through `src/url_match.h`; do not copy the matching
algorithm or add a widget/backend link dependency. Register separate Meson
cases for ordinary and case-insensitive URLs, balanced/unmatched punctuation,
missing hosts, length limits and invalid inputs. Each selected case should
return nonzero with a named failure and let Meson report it independently.
Register this target unconditionally so real and stub builds run identical
cases; ASan inherits its configured instrumentation. Keep the existing Meson
`self-test` and `command-line` registrations, and verify their CLI output and
failure behavior still work after the URL fixture moves. The release gate
must see the new cases and still reject failures/skips; neither target nor
fixtures should be installed. Separate compilation of the small pure module
is acceptable here; do not assume object reuse or binary-size savings. The
R7b is accepted.

### R8a — stream-observer ownership audit (2026-09-24)

**Recommendation: retain the observer until its six consumers have scoped
upstream replacements; R8b may rename it without changing behavior.** The
consumer-by-consumer timing and C API inventory is in the
[libghostty C API review](libghostty-c-api-review.md#r8a-downstream-stream-observer-ownership-and-ordering).
The observer sees cursor requests/resets, solicited size-report identity,
title/icon Window Ops, color-operation item boundaries, OSC 133 prompt A/D,
and dropped OSC 7 reports. Prefixes are flushed before pre-parser decisions;
prompt and dropped-report paths flush through the terminator before inspecting
the core's result. A raw sequence match is not itself proof of acceptance.

The pinned backend already offers resolved cursor state and mode setters, an
identity-free size provider, title/PWD effects, color getters, semantic row
state and a render-hold callback. It lacks the original blink request/reset
event, a request-aware size policy effect, equivalent title/icon operations,
per-item color allow/deny, an accepted prompt event and a discarded-report
diagnostic. The proposed pointer-shape getter is outside the pin and addresses
none of these requirements. R8b renames the module to
`terminal_control_observer.[ch]` and the flush callback to
`FlushObservedInputPrefix`; the pure `XtpCursorBlinkEffective`
policy helper remains available to widget and drawing code. Do not remove observation
on the strength of any one getter or rename.
