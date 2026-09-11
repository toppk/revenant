# Title Ops policy

**Title Ops** is xterm's permission for changing the displayed window title
or icon label. The `allowTitleOps` resource defaults to true; the
**Allow Title Ops** entry in the Ctrl+right-click font menu changes it at
runtime. The translation action is `allow-title-ops(on/off/toggle)`.
There is no separate `disallowedTitleOps` list.

This label describes xterm policy membership, independently of support in
other emulators. See the [feature label index](../tags.md).

## Controls and overlapping permissions

<!-- markdownlint-disable MD013 -->

| Control | Title Ops check | Window Ops check |
| --- | --- | --- |
| [OSC 0](../osc/title.md#syntax): set window title and icon label | Applies to both label changes | None |
| [OSC 1](../osc/title.md#syntax): set icon label | Applies to the icon change | None |
| [OSC 2](../osc/title.md#syntax): set window title | Applies to the title change | None |
| [XTWINOPS 23](../osc/title.md#title-stack-xtwinops-22-and-23): pop/retrieve saved labels | Applies when updating the displayed labels | `PopTitle` permits the stack operation |
| XTWINOPS 22: push/save labels | None | `PushTitle` |
| XTWINOPS 20/21: report icon/window labels | None | `GetIconTitle` / `GetWinTitle` |

<!-- markdownlint-enable MD013 -->

If Window Ops permits a normal pop but Title Ops is disabled, xterm consumes
the stack entry without changing the labels. A direct-slot read leaves the
stack depth unchanged, as usual. Enabling Allow Window Ops does not override
Title Ops. Disabling Title Ops does not itself disable reports or pushes.

In patch 411, `allowSendEvents: true` also makes the effective Title Ops
permission false and makes the corresponding menu toggle insensitive.
Setting startup labels with `-T`, `-n`, or shell resources is separate from
these application control sequences.

## Related title behavior

The [title encoding modes](../osc/title.md#title-encoding-modes) select how
labels are decoded on input and encoded in reports. They are related
configuration, rather than additional permissions. `utf8Title` and xterm's
UTF-8 Titles menu also affect encoding and the EWMH `_NET_WM_NAME` and
`_NET_WM_ICON_NAME` properties. Report authorization still comes from
[Window Ops](window-ops.md).

The title-changing path also normalizes control characters, rejects labels
longer than 1000 bytes before hex decoding, and supports `sameName` to avoid
redundant property updates. These behaviors apply alongside permission checks;
allowing a title operation does not guarantee that arbitrary bytes will be
stored unchanged.

## Sources

- [XTerm manual: allowTitleOps](https://invisible-island.net/xterm/manpage/xterm.html#VT100-Widget-Resources:allowTitleOps)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- Repository patch-411 source: `misc.c` (`ChangeGroup`, `ChangeTitle`,
  `ChangeIconName`); `charproc.c` (OSC dispatch and `window_ops`); `ptyx.h`
  (`AllowTitleOps`, `AllowXtermOps`); `menu.c` (`HandleAllowTitleOps`,
  `enable_allow_xxx_ops`) under `upstream/xterm-snapshots/`.

## Separately tracked behavior

### Title Ops and synthetic events

Feature ID: `policy-title-ops-send-events`. allowSendEvents disables effective Title Ops and makes the toggle insensitive.

### Title Ops translation action

Feature ID: `action-allow-title-ops`. allow-title-ops(on/off/toggle) shares live state and menu checkmark.

<!-- tdn:compatibility -->
