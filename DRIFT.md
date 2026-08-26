# Recorded differences from xterm

xterm+ is intended to be a faithful, drop-in xterm replacement at its visible
X11 boundary. It is not intended to preserve xterm's internal architecture.
This document records intentional differences so that compatibility work does
not quietly turn into unreviewed divergence.

The current behavioral reference is upstream xterm patch 410, as represented
by the neighboring `xterm` repository. A newer reference must be recorded here
when that baseline changes.

## Intentional differences

### libghostty-vt terminal core

xterm+'s VT parser, terminal state, key encoder, query responses, and
primary-screen resize reflow are provided by `libghostty-vt`. X11, Xt,
Athena widgets, xterm resources, menus, geometry, and the eventual font/render
skin remain xterm+ responsibilities.

This replaces xterm's terminal core rather than porting it. Compatibility is
judged by externally observable behavior. Improvements supplied by the modern
core, including preservation and reflow of text across width changes and full
UTF-8 grapheme state, are intentional even where historical xterm behaves
differently.

### Structured diagnostic logging

xterm+ emits `hh:mm:ss subsystem: message` diagnostics on standard error.
Internally each record has debug, info, warning, or error severity. On a
terminal the timestamp is bright cyan and the message is colored by severity;
redirected output is plain text.

High-volume debug diagnostics are controlled by xterm-style `-debug` and
`+debug` options and the `debug` X resource. The compiled default is `+debug`
(debug disabled). Info, warning, and error records remain enabled. This logging
surface and its exact output are xterm+ facilities, not an xterm compatibility
promise.

### Cursor-blink policy

xterm+ treats `cursorBlink` as a four-value policy. `false` and `true` select
the steady or blinking default to which `CSI 0 SP q` returns, while still
honoring application blink requests. `always` and `never` force blinking or a
steady cursor and ignore application blink requests; cursor shape and
visibility remain under application control in every case.

This behavior intentionally differs from xterm's cursor-blink policy.
`cursorBlinkXOR` is accepted for resource-file compatibility but has no effect.
The [TDN cursor-controls reference](tdn/docs/csi/cursor.md) documents the wire
controls, the xterm+ policy, the xterm policy reference, and versioned
observations from other terminals.

## Transitional gaps, not intended differences

The following are incomplete compatibility work and should not be treated as
design decisions:

- `renderFont`, the primary `faceName`, and `faceSize`/`faceSize1` through
  `faceSize7` select the Xft/fontconfig renderer. The Xlib bitmap path remains
  available when `renderFont` is false. Font fallback, comma-separated
  override faces, color emoji, and `faceNameDoublesize` are not implemented.
- The xterm color palette and pointer resources are merged by Xt but are not
  all applied by the drawer.
- `-report-config` is an xterm+ diagnostic which presents resolved resources,
  provenance, font-menu ordering, fontconfig matches, all 330 resources in the
  active patch-410 xterm tables plus 17 compile-conditional resources,
  inherited Xt/Athena component resources and constraints, all 130 active
  patch-410 `XTerm.ad` patterns, and all 114 registered patch-410 translation
  actions in an annotated
  `.Xresources` form. Upstream xterm instead has lower-level `-report-xres` and
  `-report-fonts` reports.
- Saved history, wheel and Shift+Page Up/Down navigation,
  `scroll-back`/`scroll-forw`, the Athena scrollbar, selection across history,
  and `scrollKey`/`scrollTtyOutput` use libghostty's viewport state. Clearing
  saved lines remains incomplete.
- Focused and unfocused block-cursor presentation, application-selected
  underline and bar shapes, application-requested blinking, `cursorColor`,
  `cursorBlink`, `cursorOnTime`, `cursorOffTime`, and `alwaysHighlight` are
  implemented. The `cursorUnderLine` and `cursorBar` startup shape resources
  are not wired yet.
- Unimplemented xterm menu commands remain visible but insensitive.
- The xterm command-line implementation remains incomplete. The patch-410
  resource and translation-action names are now exhaustively inventoried, but
  most are explicitly classified unsupported and still need implementations
  and compatibility tests.

Detailed compatibility classifications live in [`compat/README.md`](compat/README.md).
