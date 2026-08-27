# Compatibility inventory

This directory will track xterm's externally visible UI contract separately
from its implementation. Every imported resource, Xt action, command-line
option, menu entry, translation, and geometry behavior should be classified as:

- exact;
- mapped to a libghostty or xterm+ implementation;
- accepted but obsolete; or
- unsupported with an explicit diagnostic.

The behavioral oracle is the patch-410 xterm tree in the neighboring `xterm`
repository. The first implementation preserves all three VT popup-menu entry
names, but deliberately disables entries which have not been implemented yet.
Intentional departures from that oracle are recorded in the
[xterm differences ledger](../docs/compatibility/drift.md);
this inventory tracks the compatibility surface and implementation status.
Compatibility is one of two product bars: the
[project roadmap](../docs/maintainers/roadmap.md)
tracks the parallel requirement that xterm+ expose at least the functional
terminal baseline demonstrated by Ghostling.

## Machine-readable patch-410 baseline

[`xterm-410-resources.tsv`](xterm-410-resources.tsv) records all 330 entries in
the active patch-410 application, VT100, Tek4014, and named VT-font subresource
tables plus 17 resources present in the source behind disabled compile-time
options, including resource class and compiled default. This is generated into
the xterm+ binary and emitted by `-report-config` with the current support
decision and resolved value. The report supplements it at runtime with the
merged inherited Xt and Athena class resources; that catches resources such as
`VT100.translations` and the SimpleMenu/SmeBSB/Scrollbar styling surface that
do not appear in xterm's private tables.

[`xterm-410-app-defaults.txt`](xterm-410-app-defaults.txt) records all 130
active resource records from patch 410's `XTerm.ad`. This preserves the widget
instance hierarchy—menu and menu-entry names, Tek paths, font subresource
names, and optional toolbar paths—which cannot be reconstructed from resource
class definitions alone. `-report-config` emits every record with its current
support decision and separately identifies the app-defaults file selected by
the running Xt installation.

[`xterm-410-actions.txt`](xterm-410-actions.txt) records all 114 unique actions
registered by the active patch-410 VT100 and Tek4014 widgets. A translation is
not considered compatible merely because Xt parses it: each action it invokes
must also be implemented and is reported separately.

The baseline is intentionally tied to the active patch-410 build configuration.
Changing the xterm reference or its compile-time feature set requires updating
all three catalogs and recording the new oracle in
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
- Transitional: the X11 drawer consumes complete grapheme clusters. Its Xlib
  path draws traditional bitmap fonts, while its Xft/fontconfig path draws
  UTF-8 glyphs from the configured primary face. Shaping, fallback, override
  faces, and color emoji remain pending and are not parser limitations.
- Unsupported entries remain present but insensitive in the menus. They become
  sensitive only when the corresponding behavior has an implementation and a
  compatibility test.
