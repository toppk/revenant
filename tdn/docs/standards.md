# Standards and specifications

Every claim on TDN traces to one of a small number of documents. This page
catalogs them in the order the rest of the site follows: the framework
standards that define the byte grammar, the DEC manuals that define most of
the commands, xterm's document that defines most of the rest, and the
vendor and working-group documents that define modern extensions. Each
entry says what the document is authoritative for and which TDN section
derives from it.

## Framework standards

<!-- markdownlint-disable MD013 -->

| Document | Defines | TDN sections |
| --- | --- | --- |
| [ECMA-35](https://ecma-international.org/publications-and-standards/standards/ecma-35/) / ISO 2022 | Code structure: C0/C1 sets, G0–G3 designation and invocation, 7-bit vs 8-bit environments, the `ESC` intermediate/final byte grammar | [Control characters and ESC](escape.md), [Encoding](text/encoding.md) |
| [ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/) / ISO 6429 / ANSI X3.64 | Control functions: CSI parameter/intermediate/final grammar, the standard final bytes (CUP, ED, SGR, SM/RM…), the OSC/DCS/APC/PM/SOS string framing, `ST` | [CSI](csi/index.md), [OSC anatomy](osc/anatomy.md), [DCS](dcs/index.md) |
| [ECMA-6](https://ecma-international.org/publications-and-standards/standards/ecma-6/) / ISO 646 | The 7-bit graphic set that ASCII and the national variants are instances of | [Encoding](text/encoding.md) |
| [ISO/IEC 10646](https://www.iso.org/standard/76835.html) / [Unicode](https://www.unicode.org/versions/latest/) | The character repertoire; UTF-8 | [Text](text/index.md) |
| [UAX #11](https://www.unicode.org/reports/tr11/) East Asian Width | The `W`/`F`/`A` properties that terminal column width is derived from | [Character width](text/width.md) |
| [UAX #29](https://www.unicode.org/reports/tr29/) Text Segmentation | Extended grapheme cluster boundaries | [Grapheme clusters](text/graphemes.md) |
| [UTS #51](https://www.unicode.org/reports/tr51/) Emoji | Presentation selectors and ZWJ sequences that widen or join cells | [Character width](text/width.md), [Grapheme clusters](text/graphemes.md) |

<!-- markdownlint-enable MD013 -->

ECMA-48 defines a grammar and a set of functions, not a terminal. It does
not say what a screen is, has no notion of scrollback, private modes, or
mouse reporting, and reserves the `p`–`~` final bytes and the `<`, `=`, `>`,
`?` parameter prefixes for private use. Almost everything a modern
application relies on lives in that private space.

## DEC

<!-- markdownlint-disable MD013 -->

| Document | Defines | TDN sections |
| --- | --- | --- |
| [VT100 User Guide](https://vt100.net/docs/vt100-ug/) | The baseline: DECSC/DECRC, DECSTBM, the first private modes (`?1`–`?8`), DECALN, the original DA reply | [Cursor](csi/cursor.md), [Scrolling](csi/scrolling.md), [Modes](csi/modes.md) |
| [VT220 Programmer Reference](https://vt100.net/docs/vt220-rm/) | 8-bit controls and S7C1T/S8C1T, DECSCA and selective erase, DECUDK, secondary DA, the `~`-terminated function-key encoding | [Escape](escape.md), [Erase](csi/erase.md), [DCS](dcs/index.md), [Legacy keyboard](input/keyboard-legacy.md) |
| [VT320](https://vt100.net/docs/vt320-uu/), [VT420](https://vt100.net/docs/vt420-uu/) | DECRQM/DECRPM, DECRQSS/DECRPSS, DECSLRM and DECLRMM, DECSCUSR (VT520), tertiary DA | [Modes](csi/modes.md), [DECRQSS](dcs/decrqss.md), [Scrolling](csi/scrolling.md) |
| [VT330/VT340](https://vt100.net/docs/vt3xx-gp/) | Sixel and ReGIS | [Sixel](graphics/sixel.md), [ReGIS](graphics/legacy.md) |
| [VT510/VT520](https://vt100.net/docs/vt510-rm/) | The most complete single DEC reference; the manual TDN cites when a command's DEC definition is needed | throughout |

<!-- markdownlint-enable MD013 -->

The [vt100.net](https://vt100.net/docs/) archive hosts all of these. DEC
documents are the source for the **DEC** status on feature pages.

## xterm

[XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
(`ctlseqs`) is the single most-cited document on the site. It is
authoritative for xterm and, because every later emulator implemented some
subset of it, it is the de facto index of what "a terminal" understands. It
defines:

- the OSC selector numbering (0–2 titles, 4/5/10–19 colors, 50 fonts, 52
  clipboard, 104–119 resets) — [OSC catalog](osc/catalog.md);
- XTWINOPS (`CSI Ps t`) — [Window operations](csi/window-ops.md);
- the DEC private modes above `?1000`: mouse protocols, focus, bracketed
  paste, alternate screen — [Modes](csi/modes.md), [Mouse](input/mouse.md);
- modifyOtherKeys and XTQMODKEYS — [modifyOtherKeys](input/modify-other-keys.md);
- XTGETTCAP, XTVERSION, XTPUSHSGR/XTPOPSGR, XTPUSHCOLORS — [DCS](dcs/index.md), [SGR](csi/sgr.md).

Two companions matter:

- [terminfo(5)](https://invisible-island.net/ncurses/man/terminfo.5.html)
  and the ncurses terminfo database, which name capabilities and are how
  applications discover them — [Environment](environment.md);
- the [xterm FAQ](https://invisible-island.net/xterm/xterm.faq.html), which
  records the history behind several disputed behaviors.

xterm documents are the source for the **xterm** status.

## Cross-emulator specifications

<!-- markdownlint-disable MD013 -->

| Document | Defines | TDN sections |
| --- | --- | --- |
| [terminal-wg specifications](https://gitlab.freedesktop.org/terminal-wg/specifications) | The freedesktop working group's drafts; the venue where extensions become multi-vendor | referenced per feature |
| [Hyperlinks in terminal emulators](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda) | OSC 8 | [Hyperlinks](osc/hyperlinks.md) |
| [Kitty keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) | `CSI … u` progressive enhancement | [Kitty keyboard](input/kitty-keyboard.md) |
| [Kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) | APC `G` image transfer and placement | [Kitty graphics](graphics/kitty.md) |
| [Kitty text sizing](https://sw.kovidgoyal.net/kitty/text-sizing-protocol/) | OSC 66 | [Text sizing](text/sizing.md) |
| [Kitty desktop notifications](https://sw.kovidgoyal.net/kitty/desktop-notifications/) | OSC 99 | [Notifications](osc/notifications.md) |
| [Synchronized output](https://gist.github.com/christianparpart/d8a62cc1ab659194337d73e399004036) | DEC mode 2026 | [Modes](csi/modes.md) |
| [Color scheme reports](https://github.com/contour-terminal/contour/blob/master/docs/vt-extensions/color-palette-update-notifications.md) | DEC mode 2031, `CSI ? 996 n` | [Colors](osc/colors.md) |
| [FinalTerm / iTerm2 shell integration](https://iterm2.com/documentation-escape-codes.html) | OSC 133 and OSC 1337 | [Shell integration](osc/shell-integration.md) |
| [Sixel graphics](https://www.vt100.net/docs/vt3xx-gp/chapter14.html), [libsixel](https://github.com/saitoha/libsixel) | Sixel as implemented today | [Sixel](graphics/sixel.md) |
| [win32-input-mode](https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Improved%20keyboard%20handling%20in%20Conpty.md) | `CSI … _` key records | [win32-input-mode](input/win32-input-mode.md) |

<!-- markdownlint-enable MD013 -->

These are the source for the **Extension** status when at least one other
emulator has adopted them, and **Vendor-private** when not.

## Emulator documentation

Each emulator's own escape-sequence document is linked from its
[terminal page](terminals/index.md). Where an emulator documents a
behavior that no standard covers, the terminal page is the citation and the
feature page marks it **Vendor-private** or **Convention**.

## What is not written down

A great deal of terminal behavior has no document at all: what happens to
pending wrap after DECSC, which terminator to prefer, how to query without
blocking on a terminal that will never answer, how to survive tmux. TDN
records that material in [Practices](practices/index.md), with the status
**Convention**, and does not promote it to a standard however widely it is
shared.
