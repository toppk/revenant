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

## Manual fixtures

Run inside the terminal under test:

```sh
python3 tools/probe-features.py --list
python3 tools/probe-features.py underline
python3 tools/probe-features.py tcap --cap TN --cap Co
just probe-features mouse --seconds 60
```

The runner emits protocol requests or synthetic data. It does not implement
features or turn an unanswered request into a pass. Use the same invocation
in the comparison emulator and record versions/resources. The
[probe reference](../reference/probes.md#dispatch-fixtures) explains the
commands, cleanup and limitations.

| Work group | Probe subcommands | Boundary |
| --- | --- | --- |
| A: answerback and identity | `answerback`, `tcap`, `identity`, `unknown` | ENQ, terminal name and DA claims are separate tasks; unknown callback is APC-only. |
| V: text/cursor presentation | `underline`, `cursor`, `pointer` | SGR underline color, startup text cursor, and OSC 22 pointer each have separate state. |
| S: shell integration | `cwd`, `prompts`, `pipe` | Retain cwd, navigate semantic prompts, then pipe a semantic output range on a user action. |
| N: notifications | `notify`, `progress` | Urgency first; optional notification adapter and progress UI are separate. |
| G: procedural glyphs | `glyphs` | Box/block rendering first, braille/Powerline later. |
| U: copy/search | `copy`, `search` | Copy flash; search model; search UI are independent review units. |
| K: graphics | `graphics` | Initial fixture covers static inline RGBA only. Extend it for media, placeholders and animation as those chunks land. |
| M/F: permission completion | `mouse`, `font` | Mouse named exceptions and OSC 50 remain feature work. |

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
| Allow Tcap Ops | Live override of `disallowedTcapOps`; GetTcap gates generated XTGETTCAP replies. Default true. | Configured TN/child TERM integration; XTSETTCAP is absent. |
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
