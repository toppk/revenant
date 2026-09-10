# Window Ops policy

**Window Ops** is xterm's permission category for controls spanning several
sequence families. **XTWINOPS** is the narrower `CSI … t` protocol for
window manipulation, geometry reports, and title stacks. OSC 52 clipboard
access belongs to the permission category even though it is not XTWINOPS.

The [Window Ops label](../tags.md) connects the relevant reference pages.
It identifies xterm policy membership; it does not mean a feature is disabled
by default or that every emulator uses xterm's permission model.

## Permission rules

With `allowWindowOps: false`, xterm denies the operations selected by
`disallowedWindowOps` and permits the others, subject to their other
requirements. With `allowWindowOps: true`, this permission check permits
all operations in the category; the deny list does not override it.

The default deny list in xterm patch 411 is:

```text
GetChecksum,GetIconTitle,GetSelection,GetWinTitle,SetSelection,SetXprop
```

Thus turning off Allow Window Ops restores the configured restrictions;
it does not necessarily disable all window manipulation or size reporting.
Names are case-insensitive. List entries support `*` and `?` wildcards,
with `~pattern` re-allowing matching names, in list order. For example,
`*,~SetSelection` allows only clipboard writes within this category while
`allowWindowOps` is false.

## Features in the category

<!-- markdownlint-disable MD013 -->

| Feature | Sequence or reference | Policy names |
| --- | --- | --- |
| Restore, minimize, move, resize, raise, lower, refresh, maximize, fullscreen | [XTWINOPS](../csi/window-ops.md), operations 1–10 | `RestoreWin`, `MinimizeWin`, `SetWinPosition`, `SetWinSizePixels`, `RaiseWin`, `LowerWin`, `RefreshWin`, `SetWinSizeChars`, `MaximizeWin`, `FullscreenWin` |
| Window state, position, and geometry reports | [XTWINOPS](../csi/window-ops.md), operations 11, 13–16, 18–19 | Per-operation checks; named entries include `GetWinState`, `GetWinPosition`, `GetWinSizePixels`, `GetWinSizeChars`, `GetScreenSizeChars` |
| Read icon label or title | [Title reports](../osc/title.md#title-reports-xtwinops-20-and-21), operations 20–21 | `GetIconTitle`, `GetWinTitle` |
| Save and restore titles | [Title stack](../osc/title.md#title-stack-xtwinops-22-and-23), operations 22–23 | `PushTitle`, `PopTitle` |
| Change line count | DECSLPP (`CSI Ps t`, `Ps ≥ 24`), DECSNLS (`CSI Ps * \|`) | `SetWinLines` |
| Switch 80/132-column mode | [DECCOLM and mode 40](../csi/modes.md#dec-private-modes) | `ColumnMode`; also subject to `c132` |
| Read or write selections | [OSC 52](../osc/clipboard.md) | `GetSelection`, `SetSelection` |
| Set or delete X11 properties | [OSC 3](../osc/misc.md#osc-3-x-property) | `SetXprop` |
| Read rectangular-area checksum | DECRQCRA (`CSI Pi ; Pg ; Pt ; Pl ; Pb ; Pr * y`) | `GetChecksum` |
| Select checksum algorithm extensions | XTCHECKSUM (`CSI Ps # y`) | `SetChecksum` |
| Select active status display or status-line type | DECSASD (`CSI Ps $ }`), DECSSDT (`CSI Ps $ ~`) | `StatusLine` |

<!-- markdownlint-enable MD013 -->

Some operations depend on xterm build options or the emulated VT level.
The policy names and checks above follow the repository's patch 411 source.

Setting titles with OSC 0/1/2 instead uses `allowTitleOps`. Font operations
have `allowFontOps`; color and terminal-capability operations have their
own policies. A control affecting a window does not automatically belong
to Window Ops.

## Sources

- [XTerm manual: disallowedWindowOps](https://invisible-island.net/xterm/manpage/xterm.html#VT100-Widget-Resources:disallowedWindowOps)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- Repository source: `upstream/xterm-snapshots/charproc.c`, `tblWindowOps`
  and its `AllowWindowOps` call sites; `ptyx.h`, `AllowWindowOps`.
