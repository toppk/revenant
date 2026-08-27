# CSI catalog

Every CSI control function TDN knows about, ordered by final byte and then
by private marker and intermediate, with its origin and the page that
documents it. ECMA-48 assigns the final bytes `@`–`o`; `p`–`~` are private
and carry most DEC and xterm commands. A private marker (`?`, `>`, `=`,
`<`) or an intermediate (`SP`, `!`, `"`, `#`, `$`, `'`) selects a different
function under the same final byte.

Notation follows the [conventions](../conventions.md#notation). A row that
says "report" is emulator-to-application; everything else is a command.

<!-- markdownlint-disable MD013 -->

| Sequence | Name | Origin | Page |
| --- | --- | --- | --- |
| `CSI Ps @` | ICH insert characters | ECMA-48 | [Erase and edit](erase.md#edit) |
| `CSI Ps SP @` | SL shift left | ECMA-48 | [Scrolling](scrolling.md) |
| `CSI Ps A` | CUU cursor up | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps SP A` | SR shift right | ECMA-48 | [Scrolling](scrolling.md) |
| `CSI Ps B` | CUD cursor down | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps C` | CUF cursor forward | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps D` | CUB cursor back | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps E` | CNL cursor next line | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps F` | CPL cursor previous line | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps G` | CHA cursor horizontal absolute | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps ; Ps H` | CUP cursor position | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps I` | CHT cursor forward tabulation | ECMA-48 | [Escape](../escape.md#tab-stops) |
| `CSI Ps J` | ED erase in display | ECMA-48 | [Erase and edit](erase.md#erase) |
| `CSI ? Ps J` | DECSED selective erase in display | DEC | [Erase and edit](erase.md#selective-erase) |
| `CSI Ps K` | EL erase in line | ECMA-48 | [Erase and edit](erase.md#erase) |
| `CSI ? Ps K` | DECSEL selective erase in line | DEC | [Erase and edit](erase.md#selective-erase) |
| `CSI Ps L` | IL insert lines | ECMA-48 | [Erase and edit](erase.md#edit) |
| `CSI Ps M` | DL delete lines | ECMA-48 | [Erase and edit](erase.md#edit) |
| `CSI Ps P` | DCH delete characters | ECMA-48 | [Erase and edit](erase.md#edit) |
| `CSI # P` | XTPUSHCOLORS push palette | xterm | [Miscellany](../osc/misc.md#osc-21-kitty-color-control) |
| `CSI # Q` | XTPOPCOLORS pop palette | xterm | [Miscellany](../osc/misc.md#osc-21-kitty-color-control) |
| `CSI # R` | XTREPORTCOLORS report palette stack | xterm | [Miscellany](../osc/misc.md#osc-21-kitty-color-control) |
| `CSI Ps S` | SU scroll up | ECMA-48 | [Scrolling](scrolling.md#scroll-commands) |
| `CSI ? Pi ; Pa ; Pv S` | XTSMGRAPHICS graphics attributes | xterm | [Sixel](../graphics/sixel.md) |
| `CSI Ps T` | SD scroll down | ECMA-48 | [Scrolling](scrolling.md#scroll-commands) |
| `CSI > Pm T` | XTRMTITLE reset title modes | xterm | [Titles](../osc/title.md) |
| `CSI Ps X` | ECH erase characters | ECMA-48 | [Erase and edit](erase.md#erase) |
| `CSI Ps Z` | CBT cursor backward tabulation | ECMA-48 | [Escape](../escape.md#tab-stops) |
| `CSI Ps ^` | SD scroll down (alternate) | ECMA-48 | [Scrolling](scrolling.md#scroll-commands) |
| `CSI Ps `` ` `` | HPA horizontal position absolute | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps a` | HPR horizontal position relative | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps b` | REP repeat preceding character | ECMA-48 | [Erase and edit](erase.md) |
| `CSI Ps c` | DA1 primary device attributes | ECMA-48 / DEC | [Queries](queries.md#device-attributes) |
| `CSI > Ps c` | DA2 secondary device attributes | DEC | [Queries](queries.md#device-attributes) |
| `CSI = Ps c` | DA3 tertiary device attributes | DEC | [Queries](queries.md#device-attributes) |
| `CSI Ps d` | VPA vertical position absolute | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps e` | VPR vertical position relative | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps ; Ps f` | HVP horizontal and vertical position | ECMA-48 | [Cursor](cursor.md#movement) |
| `CSI Ps g` | TBC tab clear | ECMA-48 | [Escape](../escape.md#tab-stops) |
| `CSI Pm h` | SM set mode | ECMA-48 | [Modes](modes.md#ansi-modes) |
| `CSI ? Pm h` | DECSET set private mode | DEC / xterm | [Modes](modes.md#dec-private-modes) |
| `CSI Ps i` | MC media copy | ECMA-48 | not documented |
| `CSI ? Ps i` | DEC media copy | DEC | not documented |
| `CSI Pm l` | RM reset mode | ECMA-48 | [Modes](modes.md#ansi-modes) |
| `CSI ? Pm l` | DECRST reset private mode | DEC / xterm | [Modes](modes.md#dec-private-modes) |
| `CSI Pm m` | SGR select graphic rendition | ECMA-48 | [SGR](sgr.md) |
| `CSI > Pp ; Pv m` | XTMODKEYS set key modifier options | xterm | [modifyOtherKeys](../input/modify-other-keys.md) |
| `CSI ? Pp m` | XTQMODKEYS query key modifier options | xterm | [modifyOtherKeys](../input/modify-other-keys.md) |
| `CSI Ps n` | DSR device status report; `6 n` requests CPR | ECMA-48 | [Queries](queries.md#common-exchanges) |
| `CSI ? Ps n` | DEC DSR; `? 996 n` theme query | DEC / Contour | [Queries](queries.md), [Colors](../osc/colors.md#color-scheme-reports-dec-mode-2031) |
| `CSI > Ps n` | Disable key modifier options | xterm | [modifyOtherKeys](../input/modify-other-keys.md) |
| `CSI ! p` | DECSTR soft reset | DEC | [Modes](modes.md#reset) |
| `CSI Ps $ p` | DECRQM request ANSI mode | DEC | [Modes](modes.md#decrqm-replies) |
| `CSI ? Ps $ p` | DECRQM request private mode | DEC | [Modes](modes.md#decrqm-replies) |
| `CSI > Ps p` | XTSMPOINTER pointer mode | xterm | not documented |
| `CSI Ps ; Ps " p` | DECSCL conformance level | DEC | not documented |
| `CSI Ps q` | DECLL load LEDs | DEC | not documented |
| `CSI Ps SP q` | DECSCUSR cursor style | DEC | [Cursor](cursor.md#style-decscusr) |
| `CSI Ps " q` | DECSCA character protection | DEC | [Erase and edit](erase.md#selective-erase) |
| `CSI > Ps q` | XTVERSION request | xterm | [XTVERSION](../dcs/xtversion.md) |
| `CSI Pt ; Pb r` | DECSTBM top and bottom margins | DEC | [Scrolling](scrolling.md#vertical-margins-decstbm) |
| `CSI ? Pm r` | XTRESTORE restore private modes | xterm | [Modes](modes.md) |
| `CSI Pt ; Pl ; Pb ; Pr ; Pm $ r` | DECCARA change attributes in area | DEC | not documented |
| `CSI s` | SCOSC save cursor (ANSI.SYS) | Convention | [Cursor](cursor.md#save-and-restore) |
| `CSI Pl ; Pr s` | DECSLRM left and right margins | DEC | [Scrolling](scrolling.md#horizontal-margins-decslrm) |
| `CSI ? Pm s` | XTSAVE save private modes | xterm | [Modes](modes.md) |
| `CSI Ps ; Ps ; Ps t` | XTWINOPS window operations | xterm | [Window operations](window-ops.md) |
| `CSI > Pm t` | XTSMTITLE set title modes | xterm | [Titles](../osc/title.md) |
| `CSI Ps SP t` | DECSWBV warning bell volume | DEC | not documented |
| `CSI u` | SCORC restore cursor (ANSI.SYS) | Convention | [Cursor](cursor.md#save-and-restore) |
| `CSI Pk ; Pm ; Pe ; Pt u` | Kitty key report | kitty | [Kitty keyboard](../input/kitty-keyboard.md) |
| `CSI ? u` | Kitty keyboard query | kitty | [Kitty keyboard](../input/kitty-keyboard.md) |
| `CSI > Pf u` | Kitty keyboard push flags | kitty | [Kitty keyboard](../input/kitty-keyboard.md) |
| `CSI < Ps u` | Kitty keyboard pop flags | kitty | [Kitty keyboard](../input/kitty-keyboard.md) |
| `CSI = Pf ; Pm u` | Kitty keyboard set flags | kitty | [Kitty keyboard](../input/kitty-keyboard.md) |
| `CSI Ps SP u` | DECSMBV margin bell volume | DEC | not documented |
| `CSI … $ v` | DECCRA copy rectangular area | DEC | not documented |
| `CSI Ps $ w` | DECRQPSR request presentation state | DEC | [DCS](../dcs/misc.md#decrsps-restore-presentation-state) |
| `CSI Pt ; Pl ; Pb ; Pr ' w` | DECEFR enable filter rectangle | DEC | not documented |
| `CSI Ps x` | DECREQTPARM request terminal parameters | DEC | not documented |
| `CSI Ps * x` | DECSACE select attribute change extent | DEC | not documented |
| `CSI … $ x` | DECFRA fill rectangular area | DEC | not documented |
| `CSI Ps ; Pu ' z` | DECELR enable locator reporting | DEC | not documented |
| `CSI … $ z` | DECERA erase rectangular area | DEC | not documented |
| `CSI Pm ' {` | DECSLE select locator events | DEC | not documented |
| `CSI # {` | XTPUSHSGR push graphic rendition | xterm | [SGR](sgr.md#push-and-pop) |
| `CSI … $ {` | DECSERA selective erase rectangular area | DEC | not documented |
| `CSI Ps ' \|` | DECRQLP request locator position | DEC | not documented |
| `CSI # }` | XTPOPSGR pop graphic rendition | xterm | [SGR](sgr.md#push-and-pop) |
| `CSI Ps ' }` | DECIC insert columns | DEC | [Erase and edit](erase.md#edit) |
| `CSI Ps ' ~` | DECDC delete columns | DEC | [Erase and edit](erase.md#edit) |
| `CSI Ps ~` | Function-key report (`CSI 15 ~` = F5); `200 ~`/`201 ~` bracket a paste | DEC / xterm | [Legacy keyboard](../input/keyboard-legacy.md), [Bracketed paste](../input/bracketed-paste.md) |

<!-- markdownlint-enable MD013 -->

## Reports that share the grammar

Key and mouse reports from the emulator use CSI final bytes too, and a
parser on the application side must not confuse them with commands:

<!-- markdownlint-disable MD013 -->

| Sequence | Meaning | Page |
| --- | --- | --- |
| `CSI Ps ; Ps R` | CPR cursor position report (reply to `CSI 6 n`) | [Queries](queries.md#common-exchanges) |
| `CSI ? Ps ; Ps R` | DECXCPR extended cursor position report | [Queries](queries.md) |
| `CSI ? Ps ; Pm $ y` | DECRPM mode report (reply to DECRQM) | [Modes](modes.md#decrqm-replies) |
| `CSI ? 62 ; … c` | DA1 reply | [Queries](queries.md#device-attributes) |
| `CSI > Pp ; Pv ; Pc c` | DA2 reply | [Queries](queries.md#device-attributes) |
| `CSI 1 ; Pm A`–`D`, `H`, `F`, `P`–`S` | Modified cursor and function keys | [Legacy keyboard](../input/keyboard-legacy.md) |
| `CSI < Pb ; Px ; Py M` / `m` | SGR mouse press / release | [Mouse](../input/mouse.md) |
| `CSI M Cb Cx Cy` | X10/normal mouse report | [Mouse](../input/mouse.md) |
| `CSI I`, `CSI O` | Focus in, focus out | [Mouse and focus](../input/mouse.md) |
| `CSI ? 997 ; Ps n` | Color-scheme report | [Colors](../osc/colors.md#color-scheme-reports-dec-mode-2031) |
| `CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _` | win32-input-mode key record | [win32-input-mode](../input/win32-input-mode.md) |

<!-- markdownlint-enable MD013 -->

## Not documented

Rows marked "not documented" name a function TDN has no page for: DEC
rectangular-area operations, locator reporting, media copy, and a handful
of xterm resource switches. They are listed so a parser author knows the
final byte and intermediate are taken.

## Sources

- [ECMA-48 §8.3](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [XTerm Control Sequences, CSI](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Functions-using-CSI-_-ordered-by-the-final-character_s_)
- [VT510 Video Terminal Programmer Information](https://vt100.net/docs/vt510-rm/)
