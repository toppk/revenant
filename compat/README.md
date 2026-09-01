# Compatibility inventory

This directory will track xterm's externally visible UI contract separately
from its implementation. Every imported resource, Xt action, command-line
option, menu entry, translation, and geometry behavior should be classified as:

- exact;
- mapped to a libghostty or xterm+ implementation;
- accepted but obsolete; or
- unsupported with an explicit diagnostic.

The behavioral oracle is the `xterm-411` tag in `upstream/xterm-snapshots`.
The implementation preserves all three VT popup-menu entry
names, but deliberately disables entries which have not been implemented yet.
Intentional departures from that oracle are recorded in the
[xterm differences ledger](../docs/compatibility/drift.md);
this inventory tracks the compatibility surface and implementation status.
Compatibility is one of two product bars: the
[project roadmap](../docs/maintainers/roadmap.md)
tracks the parallel requirement that xterm+ expose at least the functional
terminal baseline demonstrated by Ghostling.

## Machine-readable patch-411 baseline

[`xterm-411-resources.tsv`](xterm-411-resources.tsv) records all 331 entries in
the active patch-411 application, VT100, Tek4014, and named VT-font subresource
tables plus 17 resources present in the source behind disabled compile-time
options, including resource class and compiled default. This is generated into
the xterm+ binary and emitted by `-report-config` with the current support
decision and resolved value. The report supplements it at runtime with the
merged inherited Xt and Athena class resources; that catches resources such as
`VT100.translations` and the SimpleMenu/SmeBSB/Scrollbar styling surface that
do not appear in xterm's private tables.

[`xterm-411-app-defaults.txt`](xterm-411-app-defaults.txt) records all 131
active resource records from patch 411's `XTerm.ad`. This preserves the widget
instance hierarchy—menu and menu-entry names, Tek paths, font subresource
names, and optional toolbar paths—which cannot be reconstructed from resource
class definitions alone. `-report-config` emits every record with its current
support decision and separately identifies the app-defaults file selected by
the running Xt installation.

[`xterm-411-actions.txt`](xterm-411-actions.txt) records all 114 unique actions
registered by the active patch-411 VT100 and Tek4014 widgets. A translation is
not considered compatible merely because Xt parses it: each action it invokes
must also be implemented and is reported separately.

[`xterm-411-face-name.json`](xterm-411-face-name.json) is the reviewed T0
characterization of stock patch 411's `faceName` and `faceNameDoublesize`
grammar and glyph-time list fallback. Its 32 cases were recorded with harness
version 4 of `tools/t0-facename-oracle.py` against the isolated `cjk-emoji` fixture
universe, with `Xft.dpi` pinned to 100. Run the harness with `uv run`; its PEP
723 header pins the `fonttools` and `wcwidth` dependencies used to prove that
the fixture's Japanese and emoji probes really distinguish the selected font
families before xterm is asked the questions.

Record a fresh candidate without overwriting the blessed fixture:

```sh
DISPLAY=:99 uv run tools/t0-facename-oracle.py \
  --xterm /usr/bin/xterm \
  --fontconfig font-fixtures-stage/universes/cjk-emoji/fontconfig.conf \
  --text-family 'DejaVu Sans Mono' \
  --wide-family 'Noto Sans Mono CJK JP' \
  --third-family 'Noto Color Emoji' \
  --record
```

The harness writes under `/tmp` and prints the candidate path. Human review
and an independent `--check` run are required before replacing the checked-in
record.

The supplemental FB cases establish that the second applicable Xft list entry
is consulted before fontconfig's system candidates, including when fontconfig
rescues an unresolvable primary; they also capture the two-entry limit and the
same behavior through `-fa`. The ST cases characterize xterm's independent
normal, bold, italic, and bold-italic fallback lists, including Xft entries
supplied through `boldFont` and `wideBoldFont`. WD-01 establishes that a wide
slot miss stays within the wide slot rather than consulting the normal slot's
chain. LM-01 through LM-03 characterize the per-style `limitFontsets` budget;
LM-04 and LM-05 record DEC double-height loading and the 50-percent
`limitFontHeight` cap. `limitFontWidth` is a draw-time per-glyph decision not
observable through this load report and remains source-characterized rather
than being overstated as deposition evidence. The JSON records requested
resources, resolved files and indices, styles, font sizes, and cell geometry;
it describes xterm's behavior rather than asserting what that behavior ought
to be.

The baseline is intentionally tied to the active patch-411 build configuration.
Changing the xterm reference or its compile-time feature set requires updating
the catalogs and deposition and recording the new oracle in
`docs/compatibility/drift.md`.

## Current classifications

- Exact: `XTerm`/`xterm` and `VT100`/`vt100` resource identities, Athena popup
  menu names, bitmap-font cell geometry, Shift+keypad font resizing, WM resize
  increments, normal startup activation,
  and focused-block/unfocused-outline cursor presentation including
  `cursorColor` and `alwaysHighlight`.
- Mapped: PTY parsing, key encoding, query responses, screen state, and
  primary-screen resize reflow are owned by libghostty rather than xterm's
  terminal core.
- Supported: saved-history and scrollbar resources and command-line options,
  including the functional VT-menu entry.
- Transitional: the Xlib path draws traditional bitmap fonts. The
  Xft/fontconfig path supports configured primary, wide, and emoji roles,
  general fallback, contextual shaping, real italic styles, and color emoji;
  broader user-configured fallback chains and the remaining font-resolution
  policy are still open.
- Unsupported entries remain present but insensitive in the menus. They become
  sensitive only when the corresponding behavior has an implementation and a
  compatibility test.
