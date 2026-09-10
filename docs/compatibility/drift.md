---
man: revenant-drift
section: 7
manual: compat
description: recorded differences from xterm patch 411
---

# Recorded differences from xterm

Revenant is intended to be a faithful, drop-in xterm replacement at its visible
X11 boundary. It is not intended to preserve xterm's internal architecture.
This document records intentional differences so that compatibility work does
not quietly turn into unreviewed divergence.

The current behavioral reference is upstream xterm patch 411, represented by
the exact `xterm-411` tag in `upstream/xterm-snapshots`. The baseline advanced
from patch 410 on 2026-08-30. The compatibility-surface delta was one VT
resource (`brokenCopyArea`) and its `copy_area` menu entry; the registered
translation-action set did not change.

The initial 15-case patch-411 T0 font-name fixture was superseded before its
interpretation became an implementation contract. Harness version 3 added six
glyph-time fallback cases and re-recorded the complete 21-case deposition with
`Xft.dpi` pinned to 100. This corrected the baseline evidence: later applicable
Xft list entries are explicit glyph fallbacks ahead of fontconfig's system
candidates. It is a characterization correction, not an intentional Revenant
difference from xterm.

Harness version 4 superseded that fixture with a complete 32-case deposition.
It adds style-specific fallback, `boldFont`/`wideBoldFont` Xft lists,
wide-versus-normal slot ordering, `limitFontsets` budgets, and DEC
double-height/`limitFontHeight` cases. The harness now carries pinned PEP 723
`fonttools` and `wcwidth` dependencies and rejects a font universe whose probe
coverage or terminal widths do not make those questions discriminating. A
second independent patch-411 run matched all 32 records before the fixture was
blessed. `limitFontWidth` remains characterized from upstream source: the T0
font-load report does not expose its per-glyph draw-time decision, and an
experimental pixel probe did not distinguish its tested settings.

## Intentional differences

### Expanded font-resolution policy (accepted; rollout in progress)

The [font-resolution contract](../maintainers/font-resolution.md) adopts four
intentional differences from the now-characterized patch-411 renderer. They
become active only with the implementation stages and tests named there; this
entry records the accepted direction without pretending unfinished behavior is
already shipped.

1. Revenant keeps cell geometry fixed from the normal primary metrics rather
   than allowing bold or italic metrics to enlarge the grid.
2. Routing coverage is normal-canonical and style resources are same-family
   only. Stock maintains independent style fallback chains and can change the
   serving family under SGR (ST-01…05); Revenant instead degrades to the
   selected role's normal instance and reports `FR-STYLEFAMILY` for a
   different-family style request.
3. An all-role miss uses deterministic renderer-owned tofu, one box per
   committed cell, rather than a font-dependent `.notdef` result.
4. Fallback roles are normalized against the primary metrics instead of using
   stock's unnormalized fallback rendering.

The reasons are stable cell arithmetic, an atom-to-family decision independent
of SGR state, visible and reproducible failure, and consistent rendering inside
backend-committed cells. The inherited two-entry grammar, prefix behavior, and
normal-style fallback sources remain compatibility requirements, not drift.

### libghostty-vt terminal core

Revenant's VT parser, terminal state, key encoder, query responses, and
primary-screen resize reflow are provided by `libghostty-vt`. X11, Xt,
Athena widgets, xterm resources, menus, geometry, and the eventual font/render
skin remain Revenant responsibilities.

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

Revenant accepts this page-granular result rather than maintaining a second
history store or depending on libghostty's private page layout. It does remove
libghostty's independent default byte cap whenever `saveLines` is positive;
without that correction, the byte cap can truncate history thousands of rows
before the requested line constraint. `saveLines: 0` still disables history.

Observed at 80×24 with `XTerm*saveLines: 16500` after `seq 1000000`, the oldest
visible sequence line was `983478` in xterm and `983721` in Revenant. The xterm
result is exact: 16,500 historical rows plus the 23 sequence rows still on the
live screen. The Revenant result was 243 rows short because the final libghostty
prune removed a complete page. This is a deliberate compatibility tradeoff for
the current backend API, not evidence that the X resource was ignored.

### OSC 8 hyperlinks

Upstream xterm deliberately does not implement OSC 8 hyperlinks because the
visible label can differ from the target URI. Revenant implements OSC 8 as a
modern terminal capability supplied by `libghostty-vt` and exposes it through
an explicit local gesture: Shift-hover underlines linked cells and
Shift+Button 1 activates a link.

Activation is intentionally narrower than many terminal emulators. Revenant
directly executes `xdg-open` with one URI argument only for `http://` and
`https://` targets. Other explicit schemes remain visible on Shift-hover but
are inert. Visible HTTP and HTTPS URLs in ordinary text are detected across
soft-wrapped rows; trailing sentence punctuation and unmatched closing
delimiters are excluded. Explicit OSC 8 state takes precedence. This is an
intentional extension to the patch-411 interaction contract, including when
Shift overrides application mouse reporting.

### Color Ops policy and dynamic colors

`allowColorOps` defaults to true, as in xterm patch 411, so dynamic-color
requests are permitted out of the box. The live **Allow Color Ops** entry in
the Ctrl+right-click menu turns the master permission off and on. When it is
off, `disallowedColorOps` (default `SetColor,GetColor,GetAnsiColor`) selects
what is refused, with xterm's names, wildcards, and `~` negation: `SetColor`
covers OSC 10-19 sets and OSC 110-119 resets, `GetColor` their queries, and
`GetAnsiColor` OSC 4/5 palette queries. Ordinary OSC 4 palette writes and
OSC 104 resets are never gated, as in xterm. Denied queries stay silent and
denied sets leave the display unchanged.

OSC 10/11/12 sets repaint immediately, resets restore the configured
`foreground`, `background`, and `cursorColor` resources, and queries report
the displayed colors. `CSI ? 996 n` and mode 2031 report light or dark from
the displayed background, a modern extension xterm does not implement.

Permission is sampled as each request item begins, so toggling the menu
cannot revoke an item already accepted. The backend enforces the policy before
libghostty sees a denied request: a denied reset is spoiled at its selector, a
denied set in an OSC 10-19 list is withheld and forwarded as a query so the
list's successive selectors stay aligned, and replies to denied queries and to
those substitutes are dropped, so permitted items in the same list still take
effect as in xterm and nothing denied is ever painted or reported. A public
libghostty color-policy hook is an upstream API ask, and the observer
extension should be removed when one exists. Remaining differences: `allowSendEvents` does not
override the permission, libghostty's Kitty OSC 21 colors are not covered,
and under DECSCNM libghostty addresses the normal colors, so OSC 10 changes
what is shown as the background where xterm changes the visible foreground.

### Major default keyboard-input drift

This difference affects bytes sent to applications under the default terminal
configuration. It is therefore a larger compatibility departure than an
additional UI gesture or an internal backend substitution.

xterm's traditional keyboard encoding collapses Ctrl-I with Tab, Ctrl-M with
Enter, and Ctrl-[ with Escape before the bytes reach the pseudoterminal. Raw
TTY mode cannot recover those distinctions: it only prevents the kernel from
transforming bytes the terminal has already encoded.

Revenant follows libghostty's fixterms behavior even when an application has not
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

Revenant also supports application-negotiated Kitty keyboard flags, including
press/repeat/release events, shifted and base-layout alternatives, bare
modifier keys, all-keys-as-escape encoding, associated UTF-8 text, and nested
flag stacks. This is a major capability extension beyond xterm's keyboard
protocol surface. Except for the default fixterms distinctions described
above, those extra event forms are emitted only after an application requests
the corresponding Kitty flags.

### OSC 52 selection access follows xterm's default deny

Revenant implements OSC 52 on top of libghostty's clipboard callbacks and
gates it with xterm's resources: `allowWindowOps` (default `false`) and
`disallowedWindowOps`, whose patch-411 default lists both `SetSelection` and
`GetSelection`. Out of the box an OSC 52 set is refused and an OSC 52 query
receives no reply at all, exactly as in stock xterm. Setting
`XTerm*allowWindowOps: true`, or removing the two names from
`disallowedWindowOps`, enables each direction independently; the list accepts
xterm's case-insensitive names, `*` and `?` wildcards, and `~` negation. `maxStringParse`
(default `600000`) refuses a set whose encoded control string would exceed the
limit; libghostty separately caps every OSC at 8 MiB.

Permitted operations use the same X11 selection machinery as the mouse: `c`
owns `CLIPBOARD`, `p` owns `PRIMARY`, and `s` resolves through
`selectToClipboard` like the `SELECT` action name. A set never disturbs other
selections Revenant owns, an empty payload clears only the named selection,
and a query answers from Revenant's own copy when it owns the selection or
from a synchronous `UTF8_STRING`/`STRING` conversion request otherwise. Query
replies mirror the request's `BEL` or `ST` terminator and carry padded
base64.

Known differences from xterm patch 411, all rooted in libghostty's parser or
reply formatter and recorded as upstream asks:

- Only one selection letter is accepted. xterm accepts a list such as `cp`
  and sets or tries each; libghostty discards such requests.
- An empty selection parameter means `CLIPBOARD`. xterm treats it as `s0`,
  the `SELECT` name plus `CUT_BUFFER0`.
- `q` (`SECONDARY`) and the cut-buffer digits `0` through `7` are folded into
  `CLIPBOARD` instead of reaching their xterm targets.
- A payload that is not valid base64 is ignored. xterm clears the selection.
- The query reply echoes the letter derived from the target, not the request
  text, and the clipboard text is not filtered through `allowPasteControls`.
- A query that Revenant cannot serve within one second, or whose owner uses an
  `INCR` transfer, is answered with an empty payload rather than waiting.
- Denied queries are silent because the read callback is withheld. The
  **Allow Window Ops** toggle in the Ctrl+right-click menu updates the policy
  immediately; turning it off restores the configured `disallowedWindowOps`
  restrictions.

### XTWINOPS title stack and title reports

Revenant implements xterm's title stack (`CSI 22 ; Ps t` saves, `CSI 23 ; Ps t`
restores) and title reports (`CSI 20 t` icon name, `CSI 21 t` window title),
under the same Window Ops policy as OSC 52. With xterm's defaults the stack is
available and both reports are refused silently; `allowWindowOps: true`, the
**Allow Window Ops** menu toggle, or removing `GetIconTitle`/`GetWinTitle`
from `disallowedWindowOps` enables them. `PushTitle`, `PopTitle`, and xterm's
numeric codes 20-23 are accepted in the list.

The stack copies xterm patch 411: ten entries in a ring whose oldest entry is
dropped on overflow, `Ps` 0/1/2 for both labels, icon name, or title with an
omitted or out-of-range value behaving like xterm (an out-of-range push saves
an empty entry), a third parameter addressing a slot directly, a missing label
taken from the nearest older entry, and a pop of an empty stack ignored.
Reports are always 7-bit `ESC ] l text ESC \` and `ESC ] L text ESC \` with
the label taken from the shell's current title and icon name.

libghostty parses these sequences but discards them without a callback, and
does not implement `CSI 20 t` at all. Rather than a second escape parser,
Revenant's existing cursor-blink control observer reports the four operations
and flushes preceding output to libghostty first so the saved title is the one
the application just set. This is recorded as an upstream API ask alongside
the observer's own removal note. Remaining differences from xterm:

- OSC 1 icon-name changes are not surfaced by libghostty, so the icon name
  stays the `iconName` resource value and an icon report or icon push sees
  that value; OSC 0 updates only the title.
- `titleModes` (hex-encoded reports, `CSI > Ps t`) is not implemented.
- `allowSendEvents` does not disable `allowWindowOps` or `allowTitleOps`.

`allowTitleOps` defaults to true and the **Allow Title Ops** menu entry
changes it immediately. False blocks the displayed title changes exposed by
libghostty and applying saved labels on pop. A permitted pop still consumes
its stack entry, matching xterm. Reports and pushes remain governed by Window
Ops, independently of this setting. The OSC 1/OSC 0 icon limitation above
also applies when Title Ops is enabled.

### Synchronized output (DEC private mode 2026)

Stock xterm patch 411 ignores DEC private mode 2026. Revenant honors it:
while an application has the mode set, output still reaches the terminal core
but the window keeps its last complete frame, and the batch is painted once
when the application resets the mode, unless a resize or the timeout below
intervenes. This removes the tearing that full-screen
programs such as editors and TUI dashboards otherwise show while they redraw.

A one-second timeout guards against an application that never resets the
mode. When it fires, Revenant paints the pending output and resets mode 2026
itself, so a later `DECRQM` query reports the mode as reset.

An ordinary expose during a hold repaints the cached last complete frame and
keeps holding. Events that invalidate that cache, such as a window resize or a
font change, paint the current terminal state at the new geometry instead,
since a stale frame at the old geometry is worse than a partial one; the mode
stays set across a host resize and the pending update is still painted when
the application releases the batch. Hyperlink hover feedback requested during
a hold is deferred and applied by a full repaint at release.

### Compositor-backed background opacity

Revenant adds a `backgroundOpacity` resource, expressed as a number from `0.0`
through `1.0`. A non-opaque value selects a 32-bit ARGB visual when the X
server provides one and a compositor owns `_NET_WM_CM_Sn`. The alpha channel
belongs only to the default terminal background and scrollbar trough;
foreground text, explicit cell backgrounds, selections, cursors, scrollbar
thumbs, and Athena menus remain opaque.

This is an intentional modern extension rather than an emulation of older
pseudo-transparency schemes. Revenant never reads or copies the root pixmap and
does not implement urxvt's `transparent`, `inheritPixmap`, tint, or shade
resources. When compositing or an ARGB visual is unavailable, it uses the
ordinary opaque visual rather than approximating transparency.

### Structured diagnostic logging

Revenant emits `hh:mm:ss subsystem: message` diagnostics on standard error.
Internally each record has debug, info, warning, or error severity. On a
terminal the timestamp is bright cyan and the message is colored by severity;
redirected output is plain text.

The compiled logging threshold is `warning`, so ordinary healthy operation is
silent. Revenant's `-log LEVEL` option and `logLevel` X resource select
`debug`, `info`, `warning`, or `error`, enabling that severity and everything
above it. The xterm-style `-debug` and `+debug` options remain aliases for
`-log debug` and `-log warning`; the legacy `debug` resource is honored when
`logLevel` is unset. This logging surface and its exact output are Revenant
facilities, not an xterm compatibility promise.

### `brokenCopyArea` rendering workaround

Patch 411 added the `brokenCopyArea` resource and an **Enable XCopyArea** VT
menu item. They control xterm's scroll-copy optimization and its workaround
for servers where that operation is broken. Revenant does not use xterm's
`XCopyArea` scroll path, so applying this implementation-specific switch would
not change its rendering. The resource remains classified unsupported and the
new menu entry is present but insensitive.

### Cursor-blink policy

Revenant follows xterm's cursor-blink policy. `false` and `true` provide the
configured blink operand; `cursorBlinkXOR` selects XOR (the default) or OR when
combining it with the separate application blink state. `always` and `never`
force blinking or a steady cursor and bypass that expression. Cursor shape and
visibility remain under application control in every case.

Like xterm, Revenant treats DECSCUSR 0 as a blinking application request rather
than as the configured operand itself. Mode-12 save/restore preserves the
application state; RIS and DECSTR clear it and restore the startup
`cursorBlink` policy. Under `always` or `never`, incoming blink controls leave
that stored state unchanged, matching xterm's
`SettableCursorBlink` gate. This distinction is retained outside libghostty's
resolved cursor state so split control sequences and the xterm resource policy
compose correctly.
The [TDN cursor-controls
reference](https://toppk.github.io/revenant/tdn/csi/cursor/) documents the
wire controls, the Revenant policy, the xterm policy reference, and versioned
observations from other terminals.

Revenant supports DEC private mode 2027 for negotiated Unicode grapheme-cluster
widths. Unlike Ghostty, it defaults that mode off (`graphemeWidth: legacy`) to
preserve xterm and wcwidth application arithmetic. `graphemeWidth: unicode`
changes the initial and reset default; applications may still select or reset
the mode explicitly.

### Command-line diagnostics and extensions

Revenant accepts only options whose effect it implements or whose behavior is
owned by Xt. Unlike xterm's `-help`, which also advertises unavailable
compile-time features, Revenant's help output is an executable inventory of its
accepted surface. Unknown options and missing values are rejected before an X
display is opened. This is intentionally stricter than letting Xt discover a
missing resource argument after display startup; the error wording and wrapped
usage otherwise follow patch 411. `-e` ends application parsing and preserves
all remaining arguments for the child.

Like xterm/Xrm, Revenant accepts an unambiguous prefix of any single-dash
option: for example, `-geo`, `-clas`, `-h`, and `-v`. Exact options take
precedence, while ambiguous prefixes such as `-fo` and `-bo` are rejected.
The xterm-compatible informational forms are `-help` and `-version`. Revenant
additionally accepts GNU-style `--help` and `--version`; double-dash options do
not abbreviate. Its version line identifies the installed product and project
version rather than using xterm's `XTerm(411)` form.

Revenant-only command options are `--self-test`, `-welcome`, `-report-config`,
`-report-font-routing`, `-log`, and `-fe`. The first is an installed package
diagnostic; `-welcome` is an offline setup audit; the report and logging options
expose Revenant's structured configuration/diagnostic facilities; `-fe`
selects the explicit emoji face.
The complete accepted and deferred option inventory is maintained in the
[command-line feasibility study](command-line-feasibility.md).

## Transitional gaps, not intended differences

The following are incomplete compatibility work and should not be treated as
design decisions:

- `renderFont`, the primary `faceName`, and `faceSize`/`faceSize1` through
  `faceSize7` select the Xft/fontconfig renderer. The Xlib bitmap path remains
  available when `renderFont` is false. `faceNameDoublesize`, `faceNameEmoji`,
  Unicode/VS emoji presentation, color emoji, HarfBuzz grapheme shaping, and
  atomic empty-ink fallback are implemented. General fontconfig fallback,
  contextual adjacent-cell shaping, and real italic/bold-italic Xft faces are
  also implemented. The characterized two-entry slot chain is implemented;
  numbered user fallback resources and the remaining expanded-resolution
  policy are still transitional.
- The xterm `color0` through `color15` resources configure the ANSI palette.
  `boldColors` and `-/+pc` select PC-style bright bold colors. Libghostty's
  rendered cell style retains the palette index but not whether it came from a
  legacy SGR 30–37 color or an indexed SGR 38;5 form, so Revenant currently
  promotes either origin when its index is 0–7. Patch-411 xterm excludes the
  indexed form. This is a transitional backend-API gap, not an intentional
  difference. Pointer colors and the remaining specialized color resources
  are merged by Xt but are not yet applied by the drawer.
- `-report-config` is a Revenant diagnostic which presents resolved resources,
  provenance, font-menu ordering, fontconfig matches, all 331 resources in the
  active patch-411 xterm tables plus 17 compile-conditional resources,
  inherited Xt/Athena component resources and constraints, all 131 active
  patch-411 `XTerm.ad` patterns, and all 114 registered patch-411 translation
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
- Command-line parsing itself is complete and honest, but much of xterm's
  option-driven behavior is not implemented yet. Current process/session gaps
  include `-/+ls`, `-baudrate`, `-tm`, `-/+ie`, `-/+hold`, `-/+wf`,
  `-/+mesg`, `-into`, `-/+sm`, `-/+ut`, `-/+l`/`-lf`, and
  `-/+lc`/`-lcc`. Window/render gaps include `-/+132`, `-/+aw`, `-/+rw`,
  `-/+j`, margin bell, startup cursor shape, pointer configuration,
  selection colors, fullscreen/maximize/nomap, and the remaining specialized
  font/color switches. `-fb` and `-fwb` currently affect the Xft fallback
  model but do not supply xterm's complete bitmap bold/wide behavior. Terminal
  ID, C1-printable, and width-policy switches remain blocked on backend
  support. Legacy keyboard tables, Tek mode, active icon, toolbar, console and
  slave modes, TERMCAP insert-mode, and multiscroll are intentionally omitted
  candidates rather than silently accepted options. Patch 411 also permits one
  bare explicit shell path; Revenant requires `-e` for an explicit command.

Detailed compatibility classifications live in the repository's
[`compat/README.md`](https://github.com/toppk/revenant/blob/master/compat/README.md).
