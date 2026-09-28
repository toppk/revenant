---
man: revenant-libghostty-c-api-review
section: 7
manual: maintainers
description: libghostty C API gaps and potential upstream contributions
---

# libghostty C API review

Source review, 2026-09-23. These are proposed contribution scopes, not approved
implementation assignments or promises of upstream acceptance.

Revenant 0.8.0 pins `27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c`.
The development pin advanced to `b1d2b7ef1f1cf5cc0118298bad849870bc678298` on 2026-09-28.
Ghostty PR #14371 is merged: its public mouse-shape getter now drives
Revenant's Xcursor pointer handling. Other API gaps in this review remain
independent of that getter; source links below retain the reviewed 0.8.0 baseline.
At the original review, fetched upstream main was
`7fb75b3c508ce8dfccfe796d9bec3cba75843d84`, with no terminal API diff from
the 0.8.0 pin. No runtime behavior was newly measured for that source review: findings below come from source, existing tests and Revenant's
recorded compatibility work.

The relevant distinction is between a **missing C accessor**, a **missing effect
in the embeddable Zig handler**, and **missing protocol implementation**. The
native Ghostty application has its own stream handler; a feature there does not
necessarily exist in `stream_terminal.Handler`, which backs the C terminal.
The standalone C OSC parser also recognizes some commands that the integrated
terminal cannot expose. Feeding it a second copy of PTY output is not the
proposed integration strategy.

## Try the behavior in Ghostty

Run these commands **inside the terminal being examined**, in a disposable window.
The probes send terminal protocols to their own controlling terminal; they do not
launch Ghostty. Record the terminal version/configuration and what you observed.
Run the same commands in Revenant to compare. Use Space/Enter to advance and q/Esc
to stop. Do not use `--no-pause` for visual assessment.

| Proposed API | Command | What to examine and how to explain the benefit |
| --- | --- | --- |
| Effective mouse state | `just probe dec-mode-1000-mouse mouse-mode-order --seconds 30` | Compare no-button motion and button dragging after mode 1000, 1002, 1003 and competing orders. DECRQM shows requested bits; actual reports show behavior. “The host needs the encoder's chosen mode to apply its mouse policy correctly.” |
| Pointer shape | `just probe osc-22-pointer-shape input-pointer` | Move over ordinary text and the link, without clicking. Compare each shape, an invalid name after crosshair, and returning from hyperlink hover. “Applications can request a meaningful pointer; a C frontend needs the shape already stored by the core.” |
| Size-report policy | `just probe csi-14-t-pixel-size-report window-observe` | Compare pixel, cell and grid reports before/after manual resize. “The host must distinguish report requests to mediate them without breaking unsolicited resize updates.” This case demonstrates reports, not that policy hook or mode 2048. |
| Semantic events | `just probe osc-133-prompt-marks shell-events` | Navigate the three synthetic prompts with your terminal's bindings. Inputs include success, failure and unspecified completion status. “A host needs accepted command boundaries for prompt navigation and last-command output, without watching raw bytes.” No visible status indicator or callback is assumed. |
| Parsed color policy | `just probe osc-10-default-foreground-color colors-dynamic-demo` | Compare default-colored text and explicit RGB text during changes/resets. “The core should parse colors while the host decides which operations it permits.” This demonstrates color operations, not an xterm-style permission menu in Ghostty. |

All five use existing TDN capability slugs; a library API proposal is not a new
terminal protocol capability. No support records are promoted by these runs.
For optional JSON evidence, append `--output /tmp/ghostty-pointer.json` (use a
separate name for each run), `--terminal ghostty --terminal-version <version>`
and `--configuration <description>`. A clean exit remains `unassessed`; record
what you actually saw, not an assumed pass.

The native Ghostty action names for prompt movement are `jump_to_prompt:-1` and
`jump_to_prompt:1`; use your configured bindings. The probe does not install key
bindings or change Ghostty configuration. Prompt marks remain in scrollback.

Mouse cleanup restores queryable requested mode bits, not unknowable historical
precedence; use a disposable window. Pointer cleanup requests `default`, since
the original shape is not queryable. Color cleanup requests saved RGB values or
configured defaults if queries failed; denied restores can leave a changed
color. Geometry probes do not undo your manual window resize.

These observations can justify the user-facing feature, but cannot prove a C
API is missing or a proposed callback is correct. The source audit establishes
the missing boundary. Getter/effect acceptance tests remain part of each PR.
For host-specific policy comparison, keep the separate existing
`just probe policy-window-ops window-reports` and
`just probe policy-color-ops-send-events colors-dynamic-policy` cases; their
menu/action instructions are for Revenant/xterm and should not be imposed on
Ghostty.

## Candidate order

| Candidate | What already exists | Missing boundary | Revenant benefit | Recommendation |
| --- | --- | --- | --- | --- |
| Effective mouse tracking and encoding | `Terminal.flags.mouse_event` / `mouse_format`; C encoder copies both | Read access to the actual effective enums | Correct named Mouse Ops exceptions without reconstructing mode precedence | First small PR |
| OSC 22 pointer shape | OSC parser, typed stream action, `Terminal.mouse_shape`; native app applies it | C terminal getter for shape, optionally change notification | Application-selected Xcursor shape, composed with hover policy | Merged as PR #14371; implemented downstream |
| Size-report request identity/policy | Typed `.size_report` action and reply encoder | Request-aware policy effect; existing size provider has no request identity | Remove the 14/16/18 observer and keep mode 2048 independent | Next focused effect PR |
| Accepted semantic-prompt events | OSC 133 parser/options and terminal state updates | Post-acceptance event in Zig handler and C wrapper | Remove A/D observation and expose command completion/exit status | High-value follow-up |
| Parsed color-operation policy | Parsed per-item set/query/reset operations | Before-operation allow/deny effect in handler and C wrapper | Remove dynamic-color permission observation | Material, but design first |
| Cursor request/reset events | Typed style/mode/reset actions and resolved cursor state | Original request notification or explicit policy operand | Replace the blink portion of the observer without losing xterm policy; other consumers need their own hooks | Design after event scopes agree |
| Title/icon operations | Title mutation; parsed icon command; partial title-stack actions | Information preservation plus handler effects and C API | Independent labels, title-stack ownership, less observation | Split into a separate design |
| DECCOLM host coordination | Core 80/132-column resize behind mode 40 | Host notification/mediation and ordering contract | Keep X window, PTY and terminal grid synchronized | Separate design, not a getter-only PR |

### R8a: downstream stream-observer ownership and ordering

`src/terminal_control_observer.[ch]` observes the stream. `XtpTerminalFeed` first scans the
whole incoming chunk with `XtpTerminalControlObserverFeed`, but
`FlushObservedInputPrefix` feeds each required prefix to libghostty before a
consumer observes or changes state. A callback's byte offset is therefore an
ordering boundary, **not proof that libghostty accepted the sequence**. CSI
callbacks flush up to, but not including, the final byte; OSC completion paths
flush through the terminator (`offset + 1`) when they need the core's result or
reply. The unwritten suffix is fed once at the end. This also keeps Ghostty's
render-hold effect, already present in the pinned API, aligned with the state
used by Revenant's presentation snapshot. Removing the observer requires
replacing each consumer's information *and* its before/after ordering.

| Consumer in `src/terminal_ghostty.c` | Current observation and timing | Pinned API versus replacement needed |
| --- | --- | --- |
| Cursor requests, reset and reports | `CSI ? 12 h/l/s/r`, `CSI Ps SP q`, RIS and DECSTR change the raw requested operand. A prefix flush precedes each change; the request/reset is observed before the final byte reaches the core. `SyncCursorBlinkMode` reconciles the core after the feed. Rendering/hold metadata reads the raw operand, and `WritePtyEffect` rewrites DECRQM/DECRQSS replies to report it. Reset also calls the host to restore its resource policy. | The pin has mode/default-style setters and resolved cursor state, but no C event for original request or reset. A typed request/reset notification or explicit raw operand with reset semantics must preserve same-feed order, host resource policy and reply behavior. A resolved-blink getter alone cannot do that. |
| CSI 14/16/18 size reports | `ObservedWindowOp` records the report kind after flushing the prefix and before Ghostty sees the final `t`; `SizeEffect` then allows/denies the core's reply. Mode 2048 uses the same size provider without a pending solicited report. | `GHOSTTY_TERMINAL_OPT_SIZE` is present but carries no request identity. A request-aware policy effect before a solicited reply must keep unsolicited mode-2048 geometry separate. See [request-aware size-report policy](#3-request-aware-size-report-policy). |
| CSI 20–23 title/icon reports and stack | The same CSI observer invokes the host's `TerminalTitleOp` after the prefix flush and before Ghostty sees the final byte. It applies Window Ops permission, reads live X labels, and owns push/pop state. Observation of bytes is not a post-acceptance event. | The pinned title-change effect/getter concerns core title mutation; icon labels and title-stack host operations are not exposed by an equivalent C effect. Preserve selector, target, slot, order and policy in a typed host operation; the existing title getter cannot supply them. |
| OSC 4/5/10–19/110–119 color policy | Selector, item starts, query markers and item ends are observed before core application. The host can spoil a denied reset or replace a denied set item with a query, then filters denied replies in the PTY write effect while the list's decisions are still active. `ColorOscEnd` flushes the terminator before clearing that state. | Color getters and parsed Zig operations exist, but the pinned C handler has no per-item pre-operation allow/deny hook. A typed policy effect must handle mixed set/query/reset lists and reply suppression without copying payload parsing. See [larger contributions](#larger-or-unsuitable-first-contributions). |
| OSC 133 A/D prompt boundaries | The observer recognizes the first item, then flushes through the terminator before `RecordPromptMark` or `RecordCommandEnd`. A checks the core's marked row; D infers acceptance from terminator, item length and capture limit because the pin supplies no command-end event. The tracked row must be captured before later bytes in the same feed move it. | Semantic row and tracked-grid APIs are already available. A post-acceptance semantic event with action, optional status and event-time location is missing; it must not report aborted or discarded OSCs. See [accepted shell-integration events](#4-accepted-shell-integration-events). |
| OSC 7 dropped working-directory report | The header snapshots the count of `PWD_CHANGED` callbacks after flushing earlier controls. The completion path flushes through the terminator and, if no callback arrived, emits `working_directory_dropped` so the host clears a stale directory. This is a discarded-report diagnostic, not a successful PWD change. | The pin has `GHOSTTY_TERMINAL_OPT_PWD_CHANGED` and a PWD getter for accepted reports, but no typed discarded/overflow event. Such an event needs reason and byte count after the parser rejects the report; an accepted-command callback cannot replace it. |

`XtpCursorBlinkEffective` is a separate host policy calculation used by
`vt_widget.c` for resource/live policy and `vt_draw.c` for the painted cursor;
`selftest.c` covers its truth table. It also serves the stub build and does not
need a stream hook. The pinned C API's
render-hold callback and accepted PWD callback remain useful. The
mouse-shape getter from Ghostty PR #14371 is **in the development pin**
and exposes accepted pointer state only; it replaces none of
the six consumers above. The review's candidate entries are independent API
asks, not one proposed generic escape callback.

R8b renamed the module/type to `terminal_control_observer.[ch]` and
`XtpTerminalControlObserver`, with `FlushObservedInputPrefix` for the prefix-flush callback.
Those names describe CSI/OSC observation and the prefix-ordering duty. Keep the pure blink-policy
helper available to its current widget/draw callers. Existing self-tests and
focused Xvfb window, title, color, prompt, working-directory and synchronized-
output suites are the downstream acceptance evidence for any later hook swap.

## 1. Effective mouse state

The C encoder's `setopt_from_terminal` copies the core's effective event and
format fields. Its public C enums already describe both. By contrast,
`GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING` is only a boolean OR of requested mode
bits. It is not an accessor for the encoder's selected mode. The handler updates
one effective enum as mode commands arrive; requested flags can therefore fail
to describe what the encoder will do. Reading the individual flags and inventing
a priority order in Revenant would duplicate and potentially contradict the core.

Propose additive terminal data keys returning the existing C tracking/format
enums; preserve the existing boolean's ABI and behavior. A getter on the encoder
could be useful too, but the terminal keys directly expose authoritative state
without requiring a temporary encoder. Exact names are for upstream agreement.

Tests must compare the getter with actual encoded output after competing mode
sets/resets in both orders, repeated toggles, reset, and split input. Cover mode 9,
1000/1002/1003 and format 1005/1006/1015/1016. Distinguish ordinary tracking from
focus reporting, and leave unsupported locator semantics out of the claim.

Revenant consumer: `todo.md` M1, `policy-mouse-ops-exceptions`; extend
`just probe dec-mode-1000-mouse input-mouse` with policy scenarios and reuse
`tests/xvfb-mouse-scroll.sh`. The binding alone does not implement exceptions.

Evidence: [C encoder][mouse-c], [C terminal getter][terminal-c],
[effective-mode handling][handler], [mouse enums][mouse].

## 2. Pointer shape

OSC 22 already becomes a validated `mouse.Shape` through `Stream` and is stored
in `Terminal.mouse_shape`. The native application's handler updates that state
and sends a UI message. The C terminal now exposes accepted shape state through
`GHOSTTY_TERMINAL_DATA_MOUSE_SHAPE`; there is no change callback.

Revenant reads the getter after output, maps all 34 enum values to W3C cursor
names, and loads the effective shape through Xcursor. Missing theme shapes fall
back to core-font cursors. Shift-hover links override the application shape;
leaving restores the latest accepted shape. Updates also apply during synchronized
output holds. Initial `text` uses an I-beam; explicit `default` uses an arrow.
Invalid names and split OSC handling follow the backend. Backend resets preserve
the accepted shape at this pin, matching xterm 411. Invalid-name behavior differs:
xterm falls back to an I-beam. Selected xterm aliases such as `xterm` and `left_ptr`
are accepted; the full cursor-font vocabulary is not supported. See the
[pointer drift record](../compatibility/drift.md#pointer-shape-osc-22).

`XtpPointerShape` keeps backend types out of the widget; `XtpPointerShapeName`
provides the canonical name table. The widget caches cursors lazily by enum value
and frees every cached cursor at destruction. Refreshing is tied to
`XtpVtUpdate`; paths feeding the core must also update the widget.

The `pointer shape` self-test covers all enum mappings, selected aliases and split/invalid input.
`xvfb-pointer-shape` reads native cursor images to check hover restoration,
changes during hover, explicit default, holds, resize, reset, and detachment.
The existing `xvfb-hyperlink-hover` test remains a separate regression check.
Manual probe: `just probe osc-22-pointer-shape input-pointer`.

Evidence: [stream dispatch][stream], [terminal state][terminal],
[native application handler][native], [shape enum][mouse].

### Prepared patch and related upstream work

The signed commit [78494bad8][pointer-patch] is pushed to
`toppk/ghostty:vt-mouse-shape`, based on upstream `7fb75b3c5`. It adds
`GHOSTTY_TERMINAL_DATA_MOUSE_SHAPE` to the existing getter, a public
`GhosttyMouseShape` enum, ABI metadata and tests. It exposes accepted state;
it does not add a callback or change OSC parsing. Validation passed: 78 focused
tests, the library build, ABI schema validation and a C consumer smoke test.
The patch merged on 2026-09-28; Revenant's development pin and pointer handling
now include it.

The upstream search on 2026-09-23 found two relevant precedents:

- [Discussion #12477: OSC dispatch effect for the embedder API][osc-dispatch]
  proposes a general callback for parsed OSC commands, explicitly including
  mouse shapes. Its author embeds libghostty in the Emacs terminal ghostel and
  wants to avoid a second OSC parser. This is a broader alternative design:
  it delivers commands before state mutation, while our getter reads the final
  accepted shape after a feed. The getter suits pointer updates; individual
  events and ordering matter for other features. The designs could coexist.
  The discussion had no comments when checked; it is not evidence of maintainer
  approval for either design.
- [PR #14128: restore application mouse shape after modifier overrides][pointer-restore],
  merged 2026-09-03, fixes native Ghostty restoring a hardcoded text or default
  pointer instead of the application-requested shape. This supplies a concrete
  reason for a frontend to retain access to that state. It changes native UI
  behavior, not the C bindings, and does not endorse a particular accessor API.

No earlier dedicated request for this getter was found. The maintainer's
[discussion #14365][bindings-introduction] introduces the C binding work and
links the branch. It is an introductory contribution discussion; choosing how
to pursue a PR remains the maintainer's decision. Keep #12477 as design context
for a future PR rather than expanding that introduction into an API debate.
No PR has been opened, and no further submission action is planned pending that
decision.

## 3. Request-aware size-report policy

The parser already distinguishes CSI 14/16/18. The embeddable handler knows the
report kind when it encodes the reply, but calls the same identity-free `size`
effect for those queries and mode 2048. Revenant currently observes CSI bytes
and maintains a pending report identifier to gate replies.

Prefer an additive typed permission effect for solicited reports, separate from
the existing geometry provider. A null effect must retain existing behavior.
This avoids changing the signature of the public size callback or accidentally
blocking unsolicited mode 2048 updates. Upstream may prefer a versioned request
callback; agree the API before implementation. Include only already-supported
reports initially, and decide explicitly whether CSI 21 belongs in this hook.

Tests: per-kind allow/deny, split and adjacent requests, same-feed mode 2048,
unsolicited resize reports, unsupported extra parameters, no effect installed,
and preservation of neighboring replies. Existing Revenant `size-report policy`
self-tests and `xvfb-window-ops` supply the consumer acceptance cases.
TDN/probe: `policy-window-ops`, `just probe policy-window-ops window-reports`.
This does not implement the rest of W2.

Evidence: [CSI dispatch][stream], [reportSize/reportMode2048][handler],
[existing callback ABI][terminal-header], `src/terminal_control_observer.h`.

## 4. Accepted shell-integration events

The parser already represents OSC 133 actions and can extract an optional exit
code. The embeddable handler updates terminal state but has no semantic-prompt
effect. Revenant observes A/D boundaries, reproduces acceptance details and
capture limits, and records tracked row references for prompt navigation and
last-command output. This is substantial maintenance coupling.

Propose a post-success semantic event with an action enum and explicitly present
optional fields. Do not conflate missing exit status with success (zero). Define
whether coordinates describe before or after the operation; Revenant needs to
capture the accepted prompt location at the event, not after a later command
in the same feed. Do not return unstable internal pointers or build a command
history model into the first patch.

Test malformed/aborted/oversized OSCs, optional exit status, A/B/C/D ordering,
multiple commands in one feed, alternate-screen handling, and failure to apply a
semantic update. Initially expose only validated options with a clear contract;
the Zig representation contains lazily read options, not a fully validated C
record ready to copy.

Revenant consumer: `RecordPromptMark`, `RecordCommandEnd` and the OSC observer
in `src/terminal_ghostty.c`; `xvfb-prompt-navigation`, `xvfb-pipe-output`, and
`just probe osc-133-prompt-marks shell-prompts`. Existing navigation/row APIs
remain useful and should not be replaced.

Evidence: [semantic parser][semantic], [semanticPrompt][terminal],
[handler dispatch][handler].

## Larger or unsuitable first contributions

- **Color policy:** the existing handler iterates typed operations and applies
  them before C can mediate. A per-item pre-operation decision could replace
  Revenant's OSC boundary observer, including mixed write/query requests. It
  needs semantics for resets and separate decisions about Kitty colors. Do not
  move xterm's permission names or its palette-write exception into libghostty.
  Use the existing color-policy matrix as downstream evidence.
- **Cursor blink:** the stream has original actions, but the terminal folds
  default/explicit choices into resolved state. A getter of resolved blink
  cannot recover Revenant's requested operand. Scoped request/reset events may
  help; this is a new effect contract, not an existing hidden getter.
- **Titles:** OSC 0 and 2 collapse to the same command, OSC 1 is parsed but
  ignored by the stream, and supported title-stack actions are no-ops in the
  embeddable handler. Full title selector/raw-byte semantics require changes
  before the C boundary. Native-handler support is also incomplete. Split
  selector preservation, icon delivery and host-owned stack effects; do not
  promise all of T2 from a callback alone.
- **DECCOLM:** core behavior exists, including mode-40 gating and grid resizing.
  Host coordination must define permission denial, X/PTY synchronization and
  reentrancy; the current resize contract must not be broken by calling back
  into the parser while it is resizing.
- **OSC 50, most W2 window operations, title encoding modes:** the inspected
  integrated parser/handler lacks the necessary operations. These require new
  protocol behavior, not C wrappers for a finished Zig implementation.
- **OSC 52 target lists/cut buffers:** these exceed current single-target parsing;
  a C signature alone cannot recover discarded or rejected selectors.
- **Dropped OSC diagnostics:** an accepted-command callback cannot report an
  overflow the parser discarded. A narrow typed diagnostic for discarded reports
  may eventually replace Revenant's OSC 7 workaround, but belongs to parser error
  reporting rather than a successful-PWD accessor.
- **Kitty drag/drop:** the Zig handler has a drag/drop effect and state machine
  without a corresponding C integration surface. Real binding opportunity, but
  Revenant has no reviewed native drop workflow; lower priority than current
  user-visible blockers.
- **Graphics, selection, search, snapshots:** public C surfaces already exist.
  Their mere presence in the backlog is not evidence of a missing binding.
  Kitty rendering still requires substantial host renderer work.

Evidence: [handler][handler], [OSC parser][osc], [stream][stream],
[C public API][api].

## Contribution shape and decision gate

Recommend agreeing on the two accessor PRs first, followed by request-aware size
policy and semantic events. Keep each contribution independently testable. Do
not combine full xterm compatibility, generic escape passthrough, or a second
parser with these additions. Color and title policy need an upstream design
conversation before a larger implementation.

For each approved patch: add a small C consumer example/test, Zig wrapper tests,
header documentation of lifetime and callback restrictions, and a Revenant
consumer experiment proving the claimed benefit. Use additive ABI changes,
existing enum sentinels and sized structs where needed; update wrapper exports,
headers and schema checks together. Define null-callback defaults. Getter tests
must fail with the accessor removed; effects tests must fail when delivery or
policy enforcement is disabled. Use targeted `test-lib-vt` tests first.

The initial open-PR and issue searches found no direct overlap. The subsequent
discussion search found the broader OSC dispatch proposal #12477, recorded
above with the pointer patch. These searches are not proof that no duplicate
exists; recheck the exact chosen scope before starting a PR.

Upstream contribution requirements apply at submission: its
[contribution guide][contributing] requires first-time contributors to be
vouched, and its [AI policy][ai-policy] requires disclosure and human
understanding/review. `toppk` was not present in the fetched vouch list. The
vouch request must be in the contributor's own voice. No discussion, issue or PR
was created by this review. Local development can be planned separately from
that submission prerequisite.

[mouse-c]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/c/mouse_encode.zig
[terminal-c]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/c/terminal.zig
[handler]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/stream_terminal.zig
[mouse]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/mouse.zig
[stream]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/stream.zig
[terminal]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/Terminal.zig
[native]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/termio/stream_handler.zig
[terminal-header]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/include/ghostty/vt/terminal.h
[semantic]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/osc/parsers/semantic_prompt.zig
[osc]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/src/terminal/osc.zig
[api]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/include/ghostty/vt.h
[contributing]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/CONTRIBUTING.md
[ai-policy]: https://github.com/ghostty-org/ghostty/blob/27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c/AI_POLICY.md
[pointer-patch]: https://github.com/toppk/ghostty/commit/78494bad86c51ae78a66bfea3e42b8bd8c0b7cc2
[osc-dispatch]: https://github.com/ghostty-org/ghostty/discussions/12477
[pointer-restore]: https://github.com/ghostty-org/ghostty/pull/14128
[bindings-introduction]: https://github.com/ghostty-org/ghostty/discussions/14365
