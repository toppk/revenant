---
man: revenant-dispatch
section: 8
manual: maintainers
description: dispatch boundaries, manual fixtures, and permission plumbing
---

# Dispatching feature work

The tracked `todo.md` groups remaining work into bounded IDs with dependencies,
acceptance criteria and stopping points. This
page records the shared interfaces so an implementation task can use existing
plumbing without silently expanding into another feature.

Start a task with its ID, expected behavior, dependencies, test oracle and
file ownership. Implement one reviewable feature at a time. Shared startup,
backend, widget and build files require coordination even when features are
independent. Read the handoff's parser, rendering, selection and reset
invariants before editing those paths.

## Feature identifiers for pending chunks

Work IDs such as S2 name dispatch units; TDN IDs name stable, independently
assessed behavior. Keep these associations when splitting or reordering the
local checklist. Dependencies, acceptance criteria and optional/upstream-blocked
status remain in that checklist; a registry entry does not schedule the feature
or assert support. This table captures the pending queue at registration time;
remove a row when its chunk is completed, preserving the feature IDs and evidence.

| Chunk | Scope | TDN feature IDs |
| --- | --- | --- |
| T1 | Frontend-owned Title Ops parity | [action-allow-title-ops](https://toppk.github.io/revenant/tdn/features/action-allow-title-ops/), [resource-same-name](https://toppk.github.io/revenant/tdn/features/resource-same-name/), [resource-utf8-title](https://toppk.github.io/revenant/tdn/features/resource-utf8-title/), [x11-utf8-title-properties](https://toppk.github.io/revenant/tdn/features/x11-utf8-title-properties/), [policy-title-ops-send-events](https://toppk.github.io/revenant/tdn/features/policy-title-ops-send-events/) |
| T2 | Parser-dependent Title Ops | [osc-0-title-icon](https://toppk.github.io/revenant/tdn/features/osc-0-title-icon/), [osc-1-icon-name](https://toppk.github.io/revenant/tdn/features/osc-1-icon-name/), [osc-2-title](https://toppk.github.io/revenant/tdn/features/osc-2-title/), [resource-title-modes](https://toppk.github.io/revenant/tdn/features/resource-title-modes/), [csi-xtsmtitle](https://toppk.github.io/revenant/tdn/features/csi-xtsmtitle/), [csi-xtrmtitle](https://toppk.github.io/revenant/tdn/features/csi-xtrmtitle/), [title-modes-hex-input](https://toppk.github.io/revenant/tdn/features/title-modes-hex-input/), [title-modes-hex-reports](https://toppk.github.io/revenant/tdn/features/title-modes-hex-reports/), [title-modes-utf8-input](https://toppk.github.io/revenant/tdn/features/title-modes-utf8-input/), [title-modes-utf8-reports](https://toppk.github.io/revenant/tdn/features/title-modes-utf8-reports/), [title-input-normalization](https://toppk.github.io/revenant/tdn/features/title-input-normalization/), [title-input-byte-limit](https://toppk.github.io/revenant/tdn/features/title-input-byte-limit/) |
| W1 | Exposed Window Ops audit and reports | [policy-window-ops](https://toppk.github.io/revenant/tdn/features/policy-window-ops/), [csi-14-t-pixel-size-report](https://toppk.github.io/revenant/tdn/features/csi-14-t-pixel-size-report/), [csi-16-t-report-cell-size-in-pixels](https://toppk.github.io/revenant/tdn/features/csi-16-t-report-cell-size-in-pixels/), [csi-18-t-text-size](https://toppk.github.io/revenant/tdn/features/csi-18-t-text-size/) |
| W2 | Remaining Window Ops and related controls | [csi-1-t-de-iconify](https://toppk.github.io/revenant/tdn/features/csi-1-t-de-iconify/), [csi-2-t-iconify](https://toppk.github.io/revenant/tdn/features/csi-2-t-iconify/), [csi-3-t-move-window](https://toppk.github.io/revenant/tdn/features/csi-3-t-move-window/), [csi-4-t-resize-window-in-pixels](https://toppk.github.io/revenant/tdn/features/csi-4-t-resize-window-in-pixels/), [csi-5-t-raise](https://toppk.github.io/revenant/tdn/features/csi-5-t-raise/), [csi-6-t-lower](https://toppk.github.io/revenant/tdn/features/csi-6-t-lower/), [csi-7-t-refresh](https://toppk.github.io/revenant/tdn/features/csi-7-t-refresh/), [csi-8-t-resize-text-area-in-cells](https://toppk.github.io/revenant/tdn/features/csi-8-t-resize-text-area-in-cells/), [csi-9-t-maximize](https://toppk.github.io/revenant/tdn/features/csi-9-t-maximize/), [csi-10-t-fullscreen](https://toppk.github.io/revenant/tdn/features/csi-10-t-fullscreen/), [csi-11-t-report-window-state](https://toppk.github.io/revenant/tdn/features/csi-11-t-report-window-state/), [csi-13-t-position-report](https://toppk.github.io/revenant/tdn/features/csi-13-t-position-report/), [csi-15-t-report-screen-size-in-pixels](https://toppk.github.io/revenant/tdn/features/csi-15-t-report-screen-size-in-pixels/), [csi-19-t-report-screen-size-in-cells](https://toppk.github.io/revenant/tdn/features/csi-19-t-report-screen-size-in-cells/), [dec-mode-3-deccolm](https://toppk.github.io/revenant/tdn/features/dec-mode-3-deccolm/), [dec-mode-40-allow-3-to-resize](https://toppk.github.io/revenant/tdn/features/dec-mode-40-allow-3-to-resize/), [csi-decslpp](https://toppk.github.io/revenant/tdn/features/csi-decslpp/), [csi-decsnls](https://toppk.github.io/revenant/tdn/features/csi-decsnls/), [csi-decrqcra](https://toppk.github.io/revenant/tdn/features/csi-decrqcra/), [csi-xtchecksum](https://toppk.github.io/revenant/tdn/features/csi-xtchecksum/), [osc-3-x-property](https://toppk.github.io/revenant/tdn/features/osc-3-x-property/), [csi-decsasd](https://toppk.github.io/revenant/tdn/features/csi-decsasd/), [csi-decssdt](https://toppk.github.io/revenant/tdn/features/csi-decssdt/), [osc-52-multiple-targets](https://toppk.github.io/revenant/tdn/features/osc-52-multiple-targets/), [osc-52-default-targets](https://toppk.github.io/revenant/tdn/features/osc-52-default-targets/), [osc-52-cut-buffers](https://toppk.github.io/revenant/tdn/features/osc-52-cut-buffers/), [osc-52-invalid-base64-clear](https://toppk.github.io/revenant/tdn/features/osc-52-invalid-base64-clear/), [osc-52-reply-target](https://toppk.github.io/revenant/tdn/features/osc-52-reply-target/) |
| C1 | Color Ops and `allowSendEvents` | [policy-color-ops-send-events](https://toppk.github.io/revenant/tdn/features/policy-color-ops-send-events/) |
| P1 | Session transcript logging | [logging-session-transcript](https://toppk.github.io/revenant/tdn/features/logging-session-transcript/) |
| P2 | Option-driven process behavior | [startup-login-shell](https://toppk.github.io/revenant/tdn/features/startup-login-shell/), [startup-hold-after-exit](https://toppk.github.io/revenant/tdn/features/startup-hold-after-exit/), [startup-wait-for-map](https://toppk.github.io/revenant/tdn/features/startup-wait-for-map/), [resource-terminal-modes](https://toppk.github.io/revenant/tdn/features/resource-terminal-modes/), [pty-message-permission](https://toppk.github.io/revenant/tdn/features/pty-message-permission/) |
| V3 | OSC 22 pointer shape | [osc-22-pointer-shape](https://toppk.github.io/revenant/tdn/features/osc-22-pointer-shape/) |
| S4 | Optional clear-to-prompt | [ui-clear-to-prompt](https://toppk.github.io/revenant/tdn/features/ui-clear-to-prompt/) |
| K1 | Static inline image rendering | [apc-kitty-static-images](https://toppk.github.io/revenant/tdn/features/apc-kitty-static-images/) |
| K2 | Media and byte limits | [kitty-graphics-storage-limit](https://toppk.github.io/revenant/tdn/features/kitty-graphics-storage-limit/), [kitty-graphics-command-limit](https://toppk.github.io/revenant/tdn/features/kitty-graphics-command-limit/), [apc-kitty-temp-file-transfer](https://toppk.github.io/revenant/tdn/features/apc-kitty-temp-file-transfer/), [apc-kitty-shared-memory](https://toppk.github.io/revenant/tdn/features/apc-kitty-shared-memory/) |
| K3 | Unicode placeholders | [apc-kitty-unicode-placeholders](https://toppk.github.io/revenant/tdn/features/apc-kitty-unicode-placeholders/) |
| K4 | Animation scheduling | [apc-kitty-animation](https://toppk.github.io/revenant/tdn/features/apc-kitty-animation/) |
| M1 | Full Mouse Ops exceptions | [policy-mouse-ops-exceptions](https://toppk.github.io/revenant/tdn/features/policy-mouse-ops-exceptions/) |
| F1 | OSC 50 Font Ops | [osc-50-font-set](https://toppk.github.io/revenant/tdn/features/osc-50-font-set/), [osc-50-font-query](https://toppk.github.io/revenant/tdn/features/osc-50-font-query/), [policy-font-ops](https://toppk.github.io/revenant/tdn/features/policy-font-ops/) |

The handoff records durable Window/Title/Color Ops contracts and longer-term
designs. Internal refactors and release verification are maintenance tasks,
not terminal compatibility features.

## Manual fixtures

The primary manual entry point is `just probe`, which builds the native Go
runner and opens a keyboard/mouse browser. Its cases have the same handlers in menu
and command mode, with TDN IDs and optional JSON evidence. For example:

```sh
just probe dcs-xtgettcap identity-tcap --cap TN --cap Co
just probe dec-mode-1000-mouse input-mouse --seconds 60
just probe sgr-58-underline-color text-underline
```

New feature work adds or extends native cases in `tools/probe/cases.go` and
their Go handlers. Give each case stable TDN feature IDs, a curated browser
location, expected behavior, policy and cleanup. Refresh embedded metadata with
`just update-probe-registry` when changing the registry/navigation, and run
`just test-probe` and `just check-tdn`. Check completion and early exit at 80×24.

Keep the legacy Python/shell probes and their recipes for comparison until the
maintainer finishes the [TDN migration checklist](https://toppk.github.io/revenant/tdn/probe-migration/)
and resolves its gaps. New features do not need a duplicate legacy probe.

The runner emits protocol requests or synthetic data. It does not implement
features or turn an unanswered request into a pass. Use the same invocation
in the comparison emulator and record versions/resources. The
[probe reference](../reference/probes.md#dispatch-fixtures) explains the
commands, cleanup and limitations.

| Work group | Go case IDs (append to the relevant TDN slug) | Boundary |
| --- | --- | --- |
| A: answerback and identity | `identity-answerback`, `identity-tcap`, `identity-reports`, `diagnostics-unknown` | ENQ, terminal name and DA claims are separate tasks; unknown callback is APC-only. |
| V: text/cursor presentation | `text-underline`, `text-cursor`, `input-pointer` | SGR underline color, startup text cursor, and OSC 22 pointer each have separate state. |
| S: shell integration | `shell-cwd`, `shell-prompts`, `shell-pipe` | Retain cwd, navigate semantic prompts, then pipe a semantic output range on a user action. |
| N: notifications | `notifications-urgency`, `notifications-progress` | Urgency first; optional notification adapter and progress UI are separate. |
| U: search | `selection-search` | Build the interactive search UI over the landed search model. |
| K: graphics | `graphics-static` | Initial fixture covers static inline RGBA only. Extend it for media, placeholders and animation as those chunks land. |
| M/F: permission completion | `input-mouse`, `font-query`, `font-set` | Mouse named exceptions and OSC 50 remain feature work. |

A fixture must be extended when a task introduces behavior it cannot yet
exercise. In particular, the graphics fixture is not evidence for file or
shared-memory transport, storage limits, Unicode placeholders or animation.

## Permission interfaces already prepared

Ctrl+right-click opens the font/options menu containing the Ops controls.
Use the actual family associated with a protocol; do not invent a permission
for every TODO item. An Ops switch is an application permission, not a
renderer feature switch.

| Control | Current effect | Remaining work |
| --- | --- | --- |
| Allow Mouse Ops | Live master gate on encoded mouse and focus reports; false restores local selection. Default true. | `disallowedMouseOps` named exceptions and unsupported tracking families. |
| Allow Tcap Ops | Live override of `disallowedTcapOps`; GetTcap gates generated XTGETTCAP replies. Default true. | XTSETTCAP is absent. Configured TN/child TERM integration is implemented separately (A2). |
| Allow Font Ops | Prepared checked state, resource and SetFont/GetFont parser; menu remains insensitive. | Public OSC 50 callback and font query/set behavior. |
| Allow Color Ops | Existing dynamic-color policy. | Remaining parity differences are in the drift ledger. |
| Allow Title Ops | Existing title-change/restoration permission. | Remaining title semantics are in the handoff. |
| Allow Window Ops | Existing selection and title-stack/report permissions. | Full XTWINOPS completion remains in the handoff. |

`src/request_ops.h` exposes `XtpFontOps` / `XtpTcapOps`, parse helpers and
operation predicates. Lists use the shared `ops_list` wildcard/negation
semantics. The default lists are `SetFont,GetFont` and `SetTcap,GetTcap`;
a true master permission overrides the list. Font policy is stored and
queryable via `XtpVtFontOps`, with `XtpVtAllowFontOps` for the master value.
The future OSC 50 handler must consult both and only then enable the menu.

The mouse gate lives in `XtpTerminalSetAllowMouseOps`; it suppresses mouse
tracking, mouse encoding and focus encoding without clearing the application's
requested modes. The widget clears remembered reported buttons when the
permission changes. Read requested mode state separately from permitted
reporting. Do not infer the encoder's effective tracking family from a
priority ordering of mode bits when implementing named exceptions.

`XtpTerminalSetTcapOpsPolicy` updates the capability reply gate. It recognizes
libghostty's complete generated `DCS 0/1 + r` replies in the PTY write effect;
it does not parse application input or alter capabilities. Preserve unrelated
replies in mixed feeds and revisit this boundary if upstream changes callback
batching. The current API emits a complete callback per capability reply.

SGR underline color, startup cursor shape, OSC 7/133, answerback and DA are
not covered by these xterm Ops families. OSC 22 is a modern pointer-shape
extension, not xterm's mouse-reporting permission. Search/copy actions belong
to local UI behavior. Notifications and graphics need explicit design choices
rather than an unrelated Ops checkbox.

## Evidence required when a feature lands

Add focused backend tests and external Xvfb checks where behavior is visible.
For an application-controlled feature, cover startup policy and changing its
menu permission live. For a query, check exact bytes, denied silence, split
feeds and unrelated replies before/after it. Preserve the final fixes in the
color-policy tests; do not reintroduce after-the-fact rollback.

The `probe-features` test uses a fake PTY to check every runner command,
reply decoding, interruption cleanup and termios restoration. It proves
fixture behavior, not terminal feature support. `xvfb-request-ops` exercises
the real Mouse/Tcap menu entries, startup denial, a GetTcap exception and
restoration after toggling. Extend these tests when extending their policy.

Keep resource support reports, menu feasibility, the drift ledger and the
handoff aligned with implemented behavior. A parser accepting a future
operation name does not make that operation supported. Missing hooks become
explicit upstream asks; the APC-only unknown callback cannot carry OSC 22,
OSC 50 or arbitrary CSI controls.
