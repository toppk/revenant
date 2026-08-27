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

### Page-granular `saveLines`

xterm treats `saveLines` as the exact number of historical rows to retain.
libghostty exposes a maximum-line constraint but allocates and removes history
in whole internal pages. Once history exceeds the configured maximum, removing
the oldest complete page can leave fewer than `saveLines` historical rows
until more output accumulates. The shortfall is bounded by one libghostty page
but varies with terminal width and page contents.

xterm+ accepts this page-granular result rather than maintaining a second
history store or depending on libghostty's private page layout. It does remove
libghostty's independent default byte cap whenever `saveLines` is positive;
without that correction, the byte cap can truncate history thousands of rows
before the requested line constraint. `saveLines: 0` still disables history.

Observed at 80×24 with `XTerm*saveLines: 16500` after `seq 1000000`, the oldest
visible sequence line was `983478` in xterm and `983721` in xterm+. The xterm
result is exact: 16,500 historical rows plus the 23 sequence rows still on the
live screen. The xterm+ result was 243 rows short because the final libghostty
prune removed a complete page. This is a deliberate compatibility tradeoff for
the current backend API, not evidence that the X resource was ignored.

### OSC 8 hyperlinks

Upstream xterm deliberately does not implement OSC 8 hyperlinks because the
visible label can differ from the target URI. xterm+ implements OSC 8 as a
modern terminal capability supplied by `libghostty-vt` and exposes it through
an explicit local gesture: Shift-hover underlines linked cells and
Shift+Button 1 activates a link.

Activation is intentionally narrower than many terminal emulators. xterm+
directly executes `xdg-open` with one URI argument only for `http://` and
`https://` targets. Other schemes remain visible on Shift-hover but are inert;
ordinary text is never promoted to a link heuristically. This is an
intentional extension to the patch-410 interaction contract, including when
Shift overrides application mouse reporting.

### Major default keyboard-input drift

This difference affects bytes sent to applications under the default terminal
configuration. It is therefore a larger compatibility departure than an
additional UI gesture or an internal backend substitution.

xterm's traditional keyboard encoding collapses Ctrl-I with Tab, Ctrl-M with
Enter, and Ctrl-[ with Escape before the bytes reach the pseudoterminal. Raw
TTY mode cannot recover those distinctions: it only prevents the kernel from
transforming bytes the terminal has already encoded.

xterm+ follows libghostty's fixterms behavior even when an application has not
explicitly enabled the Kitty keyboard protocol. It sends the ambiguous Ctrl
key combinations as CSI-u sequences; for example, Ctrl-I is
`CSI 105;5 u`, while Tab remains the single byte `0x09`. This is a substantial
intentional departure from xterm's default input contract. It lets modern
raw-mode applications distinguish the physical intentions, but an application
which assumes xterm's byte aliases may observe different input without first
negotiating a keyboard protocol.

These three split aliases are the named acceptance fixture, not the complete
compatibility boundary. Libghostty's legacy encoder also incorporates
fixterms and selected Kitty conventions for some Ctrl+Shift, digit,
punctuation, Alt+Ctrl, lock-state, and non-US-layout combinations. Those cases
require an explicit byte-for-byte matrix against xterm; they must not be
assumed compatible merely because ordinary Ctrl-letter input is compatible.

The X11 adapter must pass both the base key identity and printable logical
text to libghostty. It must not forward Xlib's already-collapsed C0 byte as the
text value or drop the event entirely.

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
