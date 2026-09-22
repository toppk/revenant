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

Revenant retains libghostty's default scrollback-pull policy on resize. Growing
the grid can reveal history when the cursor is on the bottom row, and widening
the grid can reveal history when reflow needs fewer rows. The upstream
`GHOSTTY_TERMINAL_OPT_RESIZE_PULL_SCROLLBACK` option was reviewed at revision
`27e8b3fa85d9cf8c7cd5ae2ced348bcb0a4fba9c`: disabling it addresses hosts such as
Windows ConPTY that maintain a separate screen buffer without scrollback.
Revenant uses a Unix `forkpty` transport and has no such second screen buffer.
The maintainer decision is to keep the existing behavior, without a new resource
or selectable policy. Existing resize, reflow, selection and history regressions
remain applicable; this decision adds no new capability or TDN support claim.

Colored underlines are one such extension: SGR 58 (indexed or 24-bit) colors
every underline style and SGR 59 restores the text color, as in kitty and
Ghostty. xterm patch 411 has no per-cell underline color; its `colorUL`
resource recolors underlined text as a whole and remains unimplemented. An
explicit underline color is painted as opaque ink and is not affected by
inverse video, selection, or faint, while the default underline follows the
text color through all three. The faint behavior is an intentional difference
from Ghostty, whose renderer applies faint opacity to explicit underlines too.

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

### Mouse, Tcap and prepared Font Ops

The live **Allow Mouse Ops** switch defaults true. Turning it off suppresses
mouse and focus reporting and restores local selection, while retaining the
application's requested modes for re-enabling. This implements the master
gate with xterm's default deny-all outcome; `disallowedMouseOps` named
exceptions, Locator and alternate-scroll policy remain unimplemented. The
encoder's effective tracking state needs a public API before named exceptions
can be implemented reliably.

Wheel reports match XTerm(411) byte for byte in modes 1000, 1002 and 1003 and in
the X10, 1005, 1006 and 1015 encodings: one press per notch, no release, however
many lines a local notch scrolls. Three differences remain. SGR-pixel (1016)
coordinates count from 0 where xterm counts from 1, so the same point reports as
`21;19` rather than `22;20`. In mode 1003 Revenant reports every motion event,
where xterm reports only when the pointer enters a new cell. Alternate scroll
(mode 1007) is not implemented: on the alternate screen without tracking, the
wheel sends nothing, where xterm sends cursor Up or Down keys. An active drag
survives wheel scrolling and follows the content under the pointer. In the same
drag XTerm(411) selected the rows at the drag's screen positions after the scroll
(lines 0177–0182 rather than 0182 to 0184).
`xvfb-mouse-scroll` checks all of this with real (XTest) input.

**Allow Tcap Ops** defaults true and overrides `disallowedTcapOps` (default
`SetTcap,GetTcap`). Names, wildcards and tilde negation share the other Ops
list parser. GetTcap suppresses generated XTGETTCAP replies; it does not
change the capability database. `termName` (`-tn`) supplies both the child's
`TERM` and the `TN` reply, so they cannot disagree. XTSETTCAP remains
unimplemented. `allowSendEvents` does not override these
permissions; so far it affects only Color Ops.

`allowFontOps` (true) and `disallowedFontOps` (`SetFont,GetFont`) are accepted
preparation with no font-changing effect. The menu remains disabled until an
OSC 50 callback and behavior exist. The pinned unknown-sequence callback is
APC-only, so it does not close this gap. No claim of full Mouse/Tcap/Font Ops
compatibility follows from the prepared names or switches.

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
extension should be removed when one exists. Remaining differences: libghostty's
Kitty OSC 21 colors are not covered, and under DECSCNM libghostty addresses the
normal colors, so OSC 10 changes what is shown as the background where xterm
changes the visible foreground.

`allowSendEvents` (default false) interacts with Color Ops as it does in xterm:
- **Blanket permission:** it applies only while `allowColorOps` is true and
  `allowSendEvents` is false.
- **What is refused:** `disallowedColorOps` still selects each refusal. With an
  empty list every operation stays allowed; with `GetColor` only the dynamic-color
  queries go silent. Palette writes (OSC 4 sets, including inside a mixed list) are
  never gated.
- **Live changes:** the Allow Color Ops menu entry stays sensitive. The entry and
  the new `allow-color-ops(on|off|toggle)` action change the configured value, and
  the check mark shows it. While `allowSendEvents` is true the blanket permission
  stays blocked whatever that value is. Arguments and bells behave as for
  `allow-title-ops`.

Measured under Xvfb against XTerm(411), with the same requests, the default
exceptions, an empty list, `GetColor` only and `SetColor` only, both
implementations matched across all sixteen startup combinations. The comparison
covered:
- exact replies and silences;
- pixel-verified background and palette writes;
- mixed lists. The OSC 10 write inside an `OSC 10;#aabbcc;?` request is applied
  exactly when SetColor is allowed, whatever GetColor allows. xterm was checked by
  querying after re-enabling the permission, because its reverse-video
  default-colored cells keep the previous foreground.

The live sequences also matched: every `allow-color-ops` argument form (none,
`toggle`, `on`, `off`, `ON`, `OFF`, `true`, a bogus word, two arguments) and the
menu toggle.

`allowSendEvents` is read only at startup: the Allow SendEvents menu entry stays
inert. Unlike xterm, Revenant accepts synthetic key and button events whatever
its value. Title Ops and Window Ops also consult it (see the title stack and window
report sections below). Font, Mouse and Tcap Ops do not.

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

Text crosses the X11 selection by the type its owner declares, never by what the
bytes look like; paste and OSC 52 queries follow the same rules. A `UTF8_STRING`
reply is passed on as sent — malformed UTF-8 included, as xterm-411 does. `STRING`
is Latin-1, so an owner's `c3 a9` is the two characters `Ã©`, not `é`.
`COMPOUND_TEXT` is decoded through Xlib, as xterm does; a reply of any type Revenant
cannot read is refused and `STRING` is asked for instead, or the query is
unavailable. As an owner, Revenant serves `STRING` as Latin-1 and replaces a
character Latin-1 cannot hold with `?`, byte for byte what xterm-411 serves;
`UTF8_STRING` carries the text unchanged. One difference: xterm answers a `TEXT`
request with `STRING`, or `COMPOUND_TEXT` when the text needs it, and Revenant
answers it with `UTF8_STRING`, which ICCCM permits since the reply names its type.

Replies are not cut short. A direct reply is read whole (tested through 8 MiB)
and queued to the PTY in one piece. In a test where the reader stopped reading
for 1.5 s during an 8 MiB reply, the terminal held the undelivered bytes, and the
complete reply followed when reading resumed. The queue has no size cap, but it
is memory: if an allocation fails, the session ends, so delivery is not
guaranteed then. An `INCR` transfer is not supported and is refused with the same
empty reply as an unavailable selection; the next request is unaffected. Taking
ownership uses the time of the last event Revenant processed, as xterm does, so a
selection another client changed more recently than that is not taken back.

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
- `allowSendEvents: true` blocks the Window Ops blanket permission, as in xterm, so
  `disallowedWindowOps` alone decides reports and the stack (see the window report
  section below).

`allowTitleOps` defaults to true and the **Allow Title Ops** menu entry
changes it immediately. False blocks the displayed title changes exposed by
libghostty and applying saved labels on pop. A permitted pop still consumes
its stack entry, matching xterm. Reports and pushes remain governed by Window
Ops, independently of this setting. The OSC 1/OSC 0 icon limitation above
also applies when Title Ops is enabled.

The `allow-title-ops` action changes the same live state that the menu entry
shows. Its arguments behave as XTerm(411)'s, measured with the same key bindings:

- **Change the state:** no argument or `toggle` flips it; `on` and `off` set it
  and ignore case.
- **Bell, no change:** `on` while on, `off` while off, any other word (including
  `true`), or more than one argument.

xterm's error bell also marks the window urgent under `bellIsUrgent`. Revenant's
is a plain `XBell`, like its other actions' errors.

`sameName` (default true; `-samename`/`+samename`) skips a title or icon-name
write that repeats the label last requested, as xterm does. Under Xvfb, counting
`WM_NAME` and `WM_ICON_NAME` notifications externally, Revenant matched
XTerm(411) exactly:

- **Repeated labels:** titles A, A, B, B cause two notifications with sameName on
  and four with it off.
- **Comparison with the last request:** the comparison is with the last request,
  not the live property. After another client changes `WM_NAME`, resending the
  previous title leaves the other client's value, as in xterm.
- **Title stack:** a pop whose label equals the current one writes nothing. It
  still consumes its entry, so the next pop restores the entry beneath it.

The icon name is covered only where Revenant sets it today, on pop.

`allowSendEvents: true` blocks Title Ops completely. Measured against XTerm(411):
- **No exceptions:** no title or icon label changes with `allowTitleOps` true or
  false.
- **Action:** `allow-title-ops` still changes the configured value, with the same
  arguments and bells. The effective permission stays denied.
- **Menu:** unlike Allow Color Ops, the Allow Title Ops menu entry is insensitive,
  so a click changes nothing. Its check mark shows the configured value.
- **Stack pops** still consume their entries while restoration is refused. In
  xterm, two pushes and a blocked pop, followed by `allow-send-events(off)`, left
  one entry, which the next pop restored.

Revenant cannot lift `allowSendEvents` while running, so that recovery is not
exercised; the stack consumption is checked through its own log instead. Title
reports and pushes stay under Window Ops.

### XTWINOPS window reports and the Window Ops inventory

**What the pinned libghostty exposes.** Compared with xterm's `tblWindowOps`:
- **CSI 14, 16 and 18 t** (text area in pixels, cell in pixels, text area in
  characters) reach Revenant only through the `SIZE` callback. libghostty answers
  them only in the single-parameter form, so xterm's `CSI 14 ; 2 t` (outer window
  size) goes unanswered. The callback does not say which report asked. Revenant's
  existing cursor-blink control observer, which already reports XTWINOPS 20-23,
  now also notes CSI 14/16/18 t just before libghostty sees the final byte, so each
  report is gated separately. `SIZE` calls without such a note (mode 2048 in-band
  reports) are not Window Ops and stay ungated.
- **Titles and the stack** (20-23 t) go through the same observer.
- **OSC 52 selection access** goes through the clipboard callbacks.
- **No public hook**, and left for W2: every window manipulation (1-10: restore,
  minimize, move, resize, raise, lower, refresh, maximize, fullscreen), the other
  reports (11 state, 13 position, 15 and 19 screen size), 24+ (`SetWinLines`),
  `DECCOLM` (`ColumnMode`; libghostty implements mode 3 behind mode 40 without a
  hook), `DECRQCRA` (`GetChecksum`/`SetChecksum`), OSC 3 (`SetXprop`) and the
  status line.

**Measured against XTerm(411)** with the same requests, each followed by a status
request:
- **Replies:** `4;height;width` for the text area in pixels, `6;height;width` for
  the cell, and `8;rows;columns`. The text-area reply matches the window's X size
  minus the internal border on each side.
- **Names and numbers:** `GetWinSizePixels` (14) and `GetWinSizeChars` (18) gate
  their reports. CSI 16 t is gated by `GetScreenSizeChars` (19) in xterm, so a
  list naming `16` changes nothing. Revenant reproduces both, along with
  wildcards, `~` negation and the `allowWindowOps` blanket permission.
- **allowSendEvents** blocks that blanket permission for every Window Ops user:
  reports, the title stack and OSC 52. The list still decides, as with Color Ops,
  and the Allow Window Ops menu entry is insensitive, as with Title Ops. For OSC 52,
  with `allowWindowOps: true` and an external owner holding CLIPBOARD, both
  terminals matched in each case:
  - `*` refused the write and silenced the query;
  - `GetSelection` allowed the write and silenced the query;
  - `SetSelection` refused the write and answered the query with the owner's text.

  xterm delivers that query reply asynchronously around a following status
  request.
- **Mode 2048** in-band reports use the same libghostty callback. Backend
  self-tests confirm that a denied CSI 14/16/18 t never suppresses them and that
  they never open the gate for an explicit request. That holds for consecutive and
  split requests, and an ignored extra-parameter form leaves no pending state.
- **Denials are silent,** split requests are handled, and replies to neighbouring
  requests arrive in order either way.

xterm's `allow-window-ops` action, which changes the configured value while the
entry is insensitive, is not registered in Revenant.

### Synchronized output (DEC private mode 2026)

Stock xterm patch 411 ignores DEC private mode 2026. Revenant honors it:
while an application has the mode set, output still reaches the terminal core
but the window shows the frame the application completed before the hold
began, and the batch is painted once when the application resets the mode,
unless a resize or the timeout below intervenes. This removes the tearing that
full-screen programs such as editors and TUI dashboards otherwise show while
they redraw.

That frame is captured where the hold begins in the parser, not when the window
next paints. libghostty reports the start of a hold before it parses anything
after it, and the backend captures the terminal's render state at that moment
(`XtpTerminalSetRenderHold`). So output written just before a hold in the same
write is shown, and a frame completed between a release and a new hold is shown
even when the release, the frame and the new hold all arrive before the next
paint — in one write or across several reads. Before this, the window kept
whatever it had last painted, so both frames could be lost and a program that
redraws continuously could look frozen.

The capture covers the whole visible frame, not only its cells. Screen reverse
video, the default foreground, background and cursor colors, and the cursor's
position, visibility, shape and blink request are taken at the same parser
position, so a `DECSCNM`, OSC 10/11/12, `DECTCEM` or `DECSCUSR` that arrives after
the hold began stays hidden until release, and one that arrived just before it is
shown. A captured frame is painted once when its cells or any of that metadata
differ from what is on screen; when the hold begins with nothing new since the
last paint, nothing extra is drawn.

A one-second timeout guards against an application that never resets the
mode. When it fires, Revenant paints the pending output and resets mode 2026
itself, so a later `DECRQM` query reports the mode as reset. Setting the mode
directly bypasses libghostty's hold report, so the backend ends the hold itself
on that path, and captures a new one when it re-arms the mode after a host
resize.

An ordinary expose during a hold repaints the cached last complete frame and
keeps holding. Events that invalidate that cache, such as a window resize or a
font change, paint the current terminal state at the new geometry instead,
since a stale frame at the old geometry is worse than a partial one; the mode
stays set across a host resize and the pending update is still painted when
the application releases the batch. Hyperlink hover feedback requested during
a hold is deferred and applied by a full repaint at release.

### OSC 7 working-directory reports

Stock xterm ignores OSC 7. Revenant accepts the `file://` URI that shells
emit on each prompt, percent-decodes it, and retains the path when the URI
names this host (an empty host, `localhost`, or the local hostname) and the
directory exists here. Reports for other hosts, other schemes, malformed
escapes, or directories that do not exist locally clear the retained path
rather than leaving an earlier one in place, and an empty OSC 7 clears it as
the shell intends. The URI's authority must be a plain host name; a user
name, port, or other delimiter in it is rejected. libghostty discards any
OSC 7 longer than 2048 bytes without reporting it; Revenant detects the
missing report and clears the retained path, so a very long directory name
is unknown rather than stale. The path is recorded for later features such as opening a
new window in the same directory; nothing acts on it yet, no permission
setting gates it, and the terminal process's own working directory never
follows the shell. The `-debug` log shows each decision as a `working
directory` event.

### Device attributes (DA1, DA2, DA3)

xterm 411 with its default `decTerminalID` of 420 answers DA1 with
`CSI ? 64 ; 1 ; 2 ; 6 ; 9 ; 15 ; 17 ; 18 ; 21 ; 22 ; 28 c` (observed from
the Fedora build under Xvfb; `charproc.c` adds 3 and 4 when ReGIS/Sixel are
compiled in and enabled, 8 for a VT220 keyboard type, and 16 and 29 with the
DEC locator), DA2 with `CSI > 41 ; 411 ; 0 c`, and DA3 with
`DCS ! | 00000000 ST`. Those lists describe xterm's own feature set and its
`decTerminalID` resource, which Revenant does not implement. Revenant instead
advertises only what it demonstrably does:

| Query | Revenant reply | Basis |
| --- | --- | --- |
| DA1 | `CSI ? 62 ; 6 ; 21 ; 22 c` | VT220 level; selective erase, left/right margins, ANSI color |
| DA2 | `CSI > 1 ; Pv ; 0 c` | VT220 type; `Pv` is `major * 10000 + minor * 100 + patch` of the Revenant version |
| DA3 | `DCS ! | 00000000 ST` | No unit ID; same all-zero form as xterm |
| XTVERSION | `DCS > | revenant(<version>) ST` | Unchanged product identity |

Each DA1 feature code is pinned by a backend self-test that checks rendered
cells rather than parser acceptance: `6` covers DECSCA with DECSED/DECSEL
(and plain ED ignoring DEC protection), `21` covers DECLRMM mode 69 with
DECSLRM bounding line insertion and DECRQM reporting the mode, and `22`
covers palette and 24-bit foregrounds reaching cells. The level and DA2 type
stay at VT220 for the reason Ghostty gives: the core has no DECRQSS-class
DCS replies or rectangular editing, so a VT420 claim would be hollow even
though two VT420-family controls are present. The codes xterm lists but
Revenant omits are each unsupported here: `1` (DECCOLM is ignored because
mode 40 is off and the window never resizes to 132 columns), `2` (no
printer), `3`/`4` (no ReGIS or Sixel rendering), `8` (no DECUDK), `9`/`15`
(only the UK and DEC special sets are mapped, not the national replacement
or technical sets), `16`/`29` (no DEC locator), `17` (no DECRQTSR/DECRQSS),
`18` (no DECRQDE windowing), and `28` (no DECCRA/DECFRA/DECERA). Ghostty's
non-standard `52` clipboard code is also omitted: OSC 52 obeys the Window
Ops policy, which denies it by default, and no application keys off the
code. `Pv` is the only DA2 field with content because DA2 defines it as a
firmware number; XTVERSION remains the identification path.

Two small parser-level differences come with the core. libghostty answers
`CSI 1 c` and other nonzero DA parameters, where xterm stays silent, and it
answers DA3 at every level, where xterm requires VT420 or higher. Both are
pinned in the self-test so a core change reopens this entry.

### Unknown APC diagnostics

xterm 411 consumes an Application Program Command silently: `CASE_APC`
begins a string that the string-mode dispatch treats as `/* ignored */`.
libghostty likewise implements only the Kitty graphics (`G`) and glyph
(`25a1;`) APC protocols and discards the rest. Revenant registers the core's
unknown-sequence callback so each other completed APC is written to the
diagnostic log as `unknown APC ignored truncated=<yes|no> bytes=<n>
preview="..."` at info level (visible with `-debug` or `-log info`). The
backend retains at most 256 payload bytes per APC; longer payloads are
reported with `truncated=yes` and the byte count of the retained prefix,
so the log cannot grow with the application's output. Nothing is answered
on the PTY, no permission setting applies, and the core's parser state and
surrounding text or controls are unaffected. Recognized APCs keep their own
paths and are never reported as unknown.

Behavior that follows from the core, pinned by the `unknown APC` self-test:
an APC ends at the ESC of its `ESC \` terminator (or at the 8-bit ST), so an
ESC followed by any other byte still reports the payload and then processes
that escape normally; CAN and SUB abort the APC without a report; an empty
payload, or one that is only a prefix of the glyph identifier (`2`, `25`,
`25a`), is discarded without a report; other, non-aborting C0 bytes inside
the payload, NUL included, are data.

This callback is APC-only. Unsupported OSC and CSI controls remain invisible
to Revenant, which is an upstream API limitation rather than a place for a
second escape parser; OSC 22 and OSC 50 stay on that upstream ask.

### Prompt navigation (OSC 133)

xterm 411 has no notion of shell prompts: it ignores OSC 133 and binds
nothing to Ctrl+Shift+Up or Ctrl+Shift+Down, so those keys reach the
application as modified arrows. Revenant adds `previous-prompt()` and
`next-prompt()` translation actions on those default bindings, matching
Ghostty's `jump_to_prompt` keys, and treats the gesture as translation-owned
so it is not also sent to the PTY. The actions move the viewport to the
start of the previous or next prompt that a shell marked with OSC 133,
following libghostty's prompt iterator: continuation lines (`k=s`) and
soft-wrapped prompt rows belong to the prompt above them, a continuation
run whose primary row was pruned counts as a prompt, a prompt inside the
live area keeps the live view, and the alternate screen has no history to
move through. One normalization differs from the core: for an orphan
continuation run at the very top of the screen, libghostty's upward search
stops at the row it entered while its downward search returns the top;
Revenant returns the run's top row in both directions. To restore xterm's
delivery of the keys, bind them to xterm's own keypress action, which
Revenant accepts for this purpose:

```xrdb
XTerm*vt100.translations: #override \n\
    Ctrl Shift <KeyPress> Up:   insert-seven-bit() \n\
    Ctrl Shift <KeyPress> Down: insert-seven-bit()
```

Nothing is selected, executed, or cleared; command-output extraction and
clear-to-prompt remain separate work, although the backend already exposes
the core's cell-exact command-output selection for them.

### Pipe the last command output

xterm 411 has no such action; Ctrl+Shift+G reaches the application as a
modified `g`. Revenant binds it to `pipe-command-output()`, which sends the
output of the most recently completed OSC 133 command to the command named
by the `pipeCommandOutput` resource over standard input, running it in the
directory the shell last reported with OSC 7 when one is retained. The
resource is unset by default, so the binding only logs until it is set;
the output is data on a pipe and is never executed; the helper is started
only by the explicit action, never by OSC 133 or OSC 7 arriving, and is
terminated with the terminal if it is still running at exit; and the key
can be returned to applications with the same `insert-seven-bit()`
override as the prompt keys.

### Procedural terminal glyphs

xterm draws its own line-drawing characters only for the VT100 special
graphics set (and a few DEC technical characters), when `forceBoxChars`
is set or the font lacks the glyph, with lines `fontHeight / 16` pixels
thick (`/ 12` when bold); every other Unicode box or block character comes
from the font, and its bitmap path shows only what the bitmap font has.
Revenant draws all of U+2500–U+257F and U+2580–U+259F itself: with Xft
only when the primary face lacks the character or `forceBoxChars` is set,
and always on the bitmap path, which previously showed `?` for them. The
thickness rule is xterm's for light lines (capped at an eighth of the cell
width), bold adds one pixel, heavy is
twice light, and every stroke is a rectangle placed from the cell size
alone so adjacent cells join without gaps. Shades are 2×2 stipples. The
`font-linedrawing` menu entry, `+fbx`/`-fbx` (`+fbx` turns it on, as in
xterm), and `set-font-linedrawing()` have xterm's names and meaning. A key
bound to it, like every locally bound key, is decided one tick after Xt
dispatches it and never reaches the application.

xterm has no braille, Powerline, or legacy-computing drawing; those come from
the font. Revenant applies the same routing and `forceBoxChars` switch to
Braille (U+2800–U+28FF), Powerline U+E0B0–U+E0BF plus the flame separators
U+E0D2/U+E0D4, DEC scan lines U+23BA–U+23BD, the terminal-oriented geometric
triangles U+25E2–U+25E5 and U+25F8–U+25FA/U+25FF, Symbols for Legacy Computing
U+1FB00–U+1FB92, U+1FB94–U+1FB9B, U+1FB9C–U+1FBAF,
U+1FBBD–U+1FBBF, and U+1FBCE–U+1FBEF, and selected standardized ranges from
the Unicode 16 supplement: U+1CC1B–U+1CC1E, U+1CC21–U+1CC3F,
U+1CD00–U+1CDE5, U+1CE00–U+1CE01, U+1CE0B–U+1CE0C,
U+1CE16–U+1CE19, U+1CE51–U+1CE8F, and U+1CE90–U+1CEAF. U+1FB93 is
unassigned and remains font-owned. Ordinary geometric symbols such as U+25B0,
other Powerline and Nerd Font symbols, and Ghostty's private U+F5D0–U+F60D
range also remain font-owned. The visible menu label is **Procedural Glyphs**;
the xterm-compatible `font-linedrawing` item name, action, resource, and
`+fbx`/`-fbx` options are unchanged.

### Scrollback search

xterm has no search. Revenant adds `start-search()`, bound by default to
Ctrl+Shift+F like Ghostty and Kitty, with a literal incremental search bar;
while it is open it owns every key, and Enter copies the active match to
PRIMARY. There is no regular-expression search.

### Copy feedback

xterm has no copy feedback. Revenant adds `copyFlashDuration` and
`copyFlashColor`; the duration defaults to 0, so the xterm appearance is
unchanged unless it is set. The flash follows only user selection gestures,
never an application's OSC 52 write.

### Progress reports (OSC 9;4)

xterm 411 ignores OSC 9, including ConEmu's `9;4` progress form. Revenant
draws the reported progress as a small bar in the top-right corner of the
terminal for normal, error, paused and indeterminate states and removes it
when the application clears it, on a full reset, or when the program exits.
The indicator never touches the window title, the title stack or title
reports, and needs no Window Ops permission.

### Desktop notification delivery (OSC 9, OSC 777)

xterm has no desktop notifications. When Revenant is built with libnotify,
as its release packages are, an OSC 9 or OSC 777 request that arrives while
the terminal is unfocused is also shown through the desktop notification
service, with the title and body treated as escaped, bounded text and a
limit of five notifications in ten seconds. Requests made while focused are
logged only. Builds without libnotify keep the urgency behavior below and
nothing else.

### Notification urgency (OSC 9, OSC 777)

xterm 411 does not implement OSC 9 or OSC 777; both are discarded as
unknown OSCs. Its only urgency mechanism is `bellIsUrgent`, which sets
`XUrgencyHint` on BEL while the window lacks focus (`setXUrgency` in
`misc.c`); Revenant's menu still lists that entry as inert. Revenant instead
applies the same hint for application notifications: an OSC 9 (iTerm2 form,
empty title) or OSC 777 `notify;title;body` request that arrives while the
terminal widget is unfocused sets `XUrgencyHint` in the shell's WM_HINTS,
reading and writing back the existing hints so input, state, and group
fields are untouched, and the next focus-in clears the bit. A request while
focused is logged only. Title and body appear in the `-debug` log; nothing
is displayed, no permission setting gates it, and no D-Bus, libnotify, or
helper process is involved. Valid ConEmu OSC 9 forms such as `9;4;0`
progress are separate protocols in the core; an incomplete or invalid
ConEmu shape (`9;4`, `9;4;`, `9;4;5`) is treated by libghostty as iTerm2
notification text and does reach this path, since suppressing it would
also suppress legitimate notifications. Delivering the text as a desktop
notification is a separate optional adapter.

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

If Revenant is started with standard input, output or error closed, it first opens
`/dev/null` on each closed one, and exits with status 1 before connecting to X if it
cannot. Otherwise the X connection or the PTY master takes
the free number, and anything written to standard error lands in it. XTerm(411),
started with only fd 2 closed and given a font it cannot load, writes its warning
into the X connection and hangs before starting the shell; with fds 1 and 2 closed
its PTY master becomes fd 2, so a later warning would reach the shell as input. The
shell's own stdio is unaffected in both terminals: `forkpty` closes the master
before installing the slave on fds 0–2. `xvfb-pty-closed-stdio` checks all eight
combinations.

### `brokenCopyArea` rendering workaround

Patch 411 added the `brokenCopyArea` resource and an **Enable XCopyArea** VT
menu item. They control xterm's scroll-copy optimization and its workaround
for servers where that operation is broken. Revenant does not use xterm's
`XCopyArea` scroll path, so applying this implementation-specific switch would
not change its rendering. The resource remains classified unsupported and the
new menu entry is present but insensitive.

### Startup cursor shape and DECSCUSR 0

`cursorUnderLine` and `cursorBar` select the startup cursor shape with
xterm's precedence (underline over bar). In xterm patch 411, `CSI 0 SP q` and
`CSI SP q` both select a blinking block regardless of the resources, and the
xterm-specific `CSI 7 SP q` returns to the configured initial cursor. Revenant
follows libghostty here: `CSI 0 SP q` and `CSI SP q` return to the configured
startup shape, as does a full reset, and `CSI 7 SP q` is not a recognized
style and leaves the shape unchanged. Blink handling is unchanged; the shape
resources never set or clear the application blink state.

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

### Hold after the child exits

`-hold`, `+hold` and the `hold` resource follow xterm patch 411, measured with
isolated resources. By default the terminal exits when its child does. With hold
set, the window stays after the child exits with any status, keeps its last
output, including a final line without a newline, and reaps the child. It
redraws on expose and resize and still allows selection. Key input is discarded.
Closing the window exits the terminal with status 0, as closing it while the
child still runs does. Neither terminal passes the child's exit status on, so the
terminal exits with status 0 in every case above.

One difference remains when the command cannot be run. xterm-411 retries a failed
`-e` command through `$SHELL -c`, so a held window shows the shell's
"No such file or directory" message. Revenant's child exits with status 127
without printing anything, so the held window is empty. Revenant does not retry
through the shell.

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
As in xterm, `-h` and `-v` always mean `-help` and `-version`, although `-hold`
shares the `-h` prefix; `-ho` selects `-hold`.
The xterm-compatible informational forms are `-help` and `-version`. Revenant
additionally accepts GNU-style `--help` and `--version`; double-dash options do
not abbreviate. Its version line identifies the installed product and project
version rather than using xterm's `XTerm(411)` form.

Revenant-only command options are `-self-test`, `-welcome`, `-report-config`,
`-report-font-routing`, `-log`, and `-fe`. The first is an installed package
diagnostic and keeps `--self-test` as a compatibility alias; `-welcome` is an offline setup audit; the report and logging options
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
