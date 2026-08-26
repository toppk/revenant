# Control characters and ESC sequences

Status: Standard (C0, C1), DEC (most `ESC x` commands).

Before CSI, OSC, and DCS there are single control bytes and two-byte escape
commands. Every emulator implements the C0 set; the rest is uneven.

## C0 controls

<!-- markdownlint-disable MD013 -->

| Byte | Name | Behavior |
| --- | --- | --- |
| `0x00` | NUL | Ignored |
| `0x05` | ENQ | Answerback; xterm sends the `answerbackString` (empty by default); PuTTY sends "PuTTY" |
| `0x07` | BEL | Bell: audible, visual, urgency hint, or nothing; also terminates OSC |
| `0x08` | BS | Cursor left one column; no erase; stops at the left margin unless `?45` |
| `0x09` | HT | Next tab stop; stops default every 8; `HTS`/`TBC` change them |
| `0x0a` | LF | Cursor down; scrolls at the bottom margin; CR too under LNM |
| `0x0b` | VT | Same as LF |
| `0x0c` | FF | Same as LF |
| `0x0d` | CR | Column 1 (or the left margin) |
| `0x0e` | SO | Shift out: use the G1 charset (line drawing when `ESC ) 0`) |
| `0x0f` | SI | Shift in: use G0 |
| `0x18`, `0x1a` | CAN, SUB | Abort a sequence in progress; SUB may print a reverse `?` |
| `0x1b` | ESC | Start of an escape sequence |
| `0x7f` | DEL | Ignored on output; the Backspace key sends it |

<!-- markdownlint-enable MD013 -->

## Two-byte ESC commands

<!-- markdownlint-disable MD013 -->

| Sequence | Name | Effect |
| --- | --- | --- |
| `ESC 7` | DECSC | Save cursor, attributes, charset, origin mode, pending wrap |
| `ESC 8` | DECRC | Restore them |
| `ESC =` | DECKPAM | Application keypad: keypad sends `SS3 x` |
| `ESC >` | DECKPNM | Normal keypad |
| `ESC c` | RIS | Full reset |
| `ESC D` | IND | Index: cursor down, scroll at the margin |
| `ESC E` | NEL | Next line: CR plus IND |
| `ESC H` | HTS | Set a tab stop at the cursor column |
| `ESC M` | RI | Reverse index: cursor up, scroll down at the top margin |
| `ESC N`, `ESC O` | SS2, SS3 | Single shift; SS3 is how application-mode keys are encoded |
| `ESC P` | DCS | Start of a device control string |
| `ESC [` | CSI | Control sequence introducer |
| `ESC \` | ST | String terminator |
| `ESC ]` | OSC | Operating system command |
| `ESC ^`, `ESC _` | PM, APC | Privacy message, application program command (APC carries Kitty graphics) |
| `ESC # 8` | DECALN | Fill the screen with `E`; the classic alignment test |
| `ESC # 3`–`# 6` | DECDHL, DECSWL, DECDWL | Double-height and double-width lines; see [Text sizing](text/sizing.md) |
| `ESC ( C`, `ESC ) C` | SCS | Designate charset `C` to G0/G1: `B` ASCII, `0` DEC Special Graphics, `A` UK |
| `ESC % G`, `ESC % @` | | Select UTF-8 / return to ISO 2022; see [Encoding](text/encoding.md) |
| `ESC SP F`, `ESC SP G` | S7C1T, S8C1T | Emit C1 controls as 7-bit or 8-bit |
| `ESC l`, `ESC m` | | xterm: memory lock / unlock; obsolete |

<!-- markdownlint-enable MD013 -->

## C1 controls

Each `ESC x` for `x` in `0x40`–`0x5f` has an eight-bit alias at
`0x80`–`0x9f`. In a UTF-8 session those bytes are continuation bytes, so
emulators either disable eight-bit C1 on input (kitty, foot, Ghostty, VTE)
or accept them only when `ESC SP G` or a non-UTF-8 locale is active
(xterm). Applications must send the seven-bit form.

Reports can also be emitted in eight-bit form when S8C1T is set; every
serious application assumes seven-bit and sets S7C1T defensively during
initialization.

## Tab stops

`HTS` sets, `CSI g` (TBC) clears the current stop, `CSI 3 g` clears all,
and `CSI Ps I`/`Ps Z` move by stops. `DECST8C` (`CSI ? 5 W`) resets stops
to every eighth column in xterm and DEC-faithful emulators. Tab stops are
one of the least-tested state items; `reset(1)` restores them.

## Probe

```sh
printf '\033#8'; sleep 1; printf '\033c'       # DECALN then reset
printf '\033(0lqqqk\033(B\n'                    # line drawing: ┌───┐
printf '\005'; tools/query ''          # ENQ answerback, usually silent
```

## Sources

- [ECMA-48 §8.2, §8.3](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [ECMA-35 (ISO 2022) code extension](https://ecma-international.org/publications-and-standards/standards/ecma-35/)
- [VT510 programmer reference](https://vt100.net/docs/vt510-rm/)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
