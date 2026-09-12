---
man: revenant-dispatch
section: 8
manual: maintainers
description: dispatch boundaries, manual fixtures, and permission plumbing
---

# Dispatching feature work

The local `todo.md` groups remaining work into bounded IDs with dependencies,
acceptance criteria and stopping points. Keep that checklist untracked. This
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
| V3 | OSC 22 pointer shape | [osc-22-pointer-shape](https://toppk.github.io/revenant/tdn/features/osc-22-pointer-shape/) |
| S4 | Optional clear-to-prompt | [ui-clear-to-prompt](https://toppk.github.io/revenant/tdn/features/ui-clear-to-prompt/) |
| N2 | Optional desktop notification adapter | [notification-desktop-delivery](https://toppk.github.io/revenant/tdn/features/notification-desktop-delivery/) |
| N3 | Progress display | [osc-9-4-progress](https://toppk.github.io/revenant/tdn/features/osc-9-4-progress/), [ui-progress-indicator](https://toppk.github.io/revenant/tdn/features/ui-progress-indicator/) |
| G1 | Box and block glyphs | [text-box-drawing](https://toppk.github.io/revenant/tdn/features/text-box-drawing/), [text-block-drawing](https://toppk.github.io/revenant/tdn/features/text-block-drawing/), [resource-force-box-chars](https://toppk.github.io/revenant/tdn/features/resource-force-box-chars/) |
| G2 | Braille and Powerline | [text-braille-drawing](https://toppk.github.io/revenant/tdn/features/text-braille-drawing/), [text-powerline-drawing](https://toppk.github.io/revenant/tdn/features/text-powerline-drawing/) |
| U1 | Copy-highlight flash | [selection-copy-feedback](https://toppk.github.io/revenant/tdn/features/selection-copy-feedback/) |
| U2 | Search model and navigation | [search-scrollback-literal](https://toppk.github.io/revenant/tdn/features/search-scrollback-literal/) |
| U3 | Search UI and bindings | [ui-search-overlay](https://toppk.github.io/revenant/tdn/features/ui-search-overlay/), [search-copy-match](https://toppk.github.io/revenant/tdn/features/search-copy-match/) |
| K1 | Static inline image rendering | [apc-kitty-static-images](https://toppk.github.io/revenant/tdn/features/apc-kitty-static-images/) |
| K2 | Media and byte limits | [kitty-graphics-storage-limit](https://toppk.github.io/revenant/tdn/features/kitty-graphics-storage-limit/), [kitty-graphics-command-limit](https://toppk.github.io/revenant/tdn/features/kitty-graphics-command-limit/), [apc-kitty-temp-file-transfer](https://toppk.github.io/revenant/tdn/features/apc-kitty-temp-file-transfer/), [apc-kitty-shared-memory](https://toppk.github.io/revenant/tdn/features/apc-kitty-shared-memory/) |
| K3 | Unicode placeholders | [apc-kitty-unicode-placeholders](https://toppk.github.io/revenant/tdn/features/apc-kitty-unicode-placeholders/) |
| K4 | Animation scheduling | [apc-kitty-animation](https://toppk.github.io/revenant/tdn/features/apc-kitty-animation/) |
| M1 | Full Mouse Ops exceptions | [policy-mouse-ops-exceptions](https://toppk.github.io/revenant/tdn/features/policy-mouse-ops-exceptions/) |
| F1 | OSC 50 Font Ops | [osc-50-font-set](https://toppk.github.io/revenant/tdn/features/osc-50-font-set/), [osc-50-font-query](https://toppk.github.io/revenant/tdn/features/osc-50-font-query/), [policy-font-ops](https://toppk.github.io/revenant/tdn/features/policy-font-ops/) |

The handoff separately maps remaining Window/Title/Color Ops parity, optional
interaction work and longer-term designs. Internal refactors and release
verification are maintenance tasks, not terminal compatibility features.

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
| G: procedural glyphs | `text-glyphs` | Box/block rendering first, braille/Powerline later. |
| U: copy/search | `selection-copy`, `selection-search` | Copy flash; search model; search UI are independent review units. |
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
