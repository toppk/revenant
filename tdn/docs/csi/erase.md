# CSI erase and edit

Status: Standard (ECMA-48) with DEC selective-erase variants and one xterm
extension.

Erase commands clear cells in place. Edit commands insert or delete cells or
rows and shift the rest. Both are bounded by the scrolling region and, when
enabled, the left/right margins.

## Erase

<!-- markdownlint-disable MD013 -->

| Name | Sequence | `Ps` | Effect |
| --- | --- | --- | --- |
| ED | `CSI Ps J` | 0 | From cursor to end of screen (default) |
| | | 1 | From start of screen to cursor, inclusive |
| | | 2 | Entire screen; cursor does not move |
| | | 3 | Scrollback only (xterm extension); the visible screen is untouched |
| EL | `CSI Ps K` | 0 | From cursor to end of line (default) |
| | | 1 | From start of line to cursor, inclusive |
| | | 2 | Entire line |
| ECH | `CSI Ps X` | 1 | Erase `Ps` cells from the cursor; no shifting |

<!-- markdownlint-enable MD013 -->

Erased cells receive the current background color in every modern emulator,
following xterm's "background color erase" (terminfo `bce`). Older behavior
erased to the default background. Applications that rely on `bce` to paint
regions should check the capability or paint spaces explicitly.

`ED 2` does not move the cursor and does not clear scrollback. `clear(1)`
sends `ED 2` then `ED 3` when the terminfo entry has `E3`; applications
should not assume `ED 3` exists. On the alternate screen `ED 3` is a no-op
almost everywhere because there is no scrollback.

### Selective erase

```text
CSI Ps " q     DECSCA: 1 = protect, 0 or 2 = unprotect subsequent characters
CSI ? Ps J     DECSED: like ED, but skips protected cells
CSI ? Ps K     DECSEL: like EL, but skips protected cells
```

DECSCA sets a per-cell protection attribute. Support is common in
DEC-faithful emulators (xterm, WezTerm, Ghostty, foot, Contour) and absent
elsewhere; consult the terminal page before use.

## Edit

<!-- markdownlint-disable MD013 -->

| Name | Sequence | Default | Effect |
| --- | --- | --- | --- |
| ICH | `CSI Ps @` | 1 | Insert `Ps` blank cells at the cursor; the rest of the row shifts right and falls off the right margin |
| DCH | `CSI Ps P` | 1 | Delete `Ps` cells at the cursor; the row shifts left and blanks enter from the right margin |
| IL | `CSI Ps L` | 1 | Insert `Ps` blank rows at the cursor row; rows below shift down and fall off the bottom margin |
| DL | `CSI Ps M` | 1 | Delete `Ps` rows at the cursor row; rows below shift up and blanks enter at the bottom margin |
| REP | `CSI Ps b` | 1 | Repeat the last printed character `Ps` times |

<!-- markdownlint-enable MD013 -->

IL and DL apply only when the cursor is inside the scrolling region; outside
it they are ignored. Both move the cursor to the left margin in DEC terminals
and in xterm; a few emulators leave the column unchanged, so applications
should follow IL/DL with an explicit CHA.

ICH and DCH operate on the current row only and respect left/right margins
when `?69` is set. Inserting into a row that contains a wide character at the
boundary splits it; emulators replace the orphaned half with a space.

Insert mode (`CSI 4 h`, IRM) makes ordinary printing behave like ICH before
each character. Almost nothing uses it, but full-screen libraries reset it
defensively.

## Wide characters and erase

Erasing one half of a wide character erases both halves in most emulators;
the wide cell cannot exist without its spacer. Whether the surviving
neighbor becomes a space or is left untouched varies. Applications that
erase columns precisely should erase whole grapheme runs.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ED/EL/ECH/ICH/DCH/IL/DL | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| ED 3 (scrollback) | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | Yes | Yes | Yes (`E3`) |
| `bce` | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| DECSCA/DECSED/DECSEL | Yes | ? | ? | ? | Yes | Yes | Yes | ? | ? | ? | ? | ? | Partial | ? |
| REP | Yes | Yes | ? | Yes | Yes | Yes | Yes | Yes | ? | Yes | ? | ? | Yes | Yes |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
printf 'abcdefgh\r\033[2@' ; sleep 1; printf '\n'      # ICH 2: expect "  abcdefgh" clipped
printf 'abcdefgh\r\033[3P' ; sleep 1; printf '\n'      # DCH 3: expect "defgh"
printf 'abcdefgh\r\033[4b\n'                            # REP after CR repeats "h"
printf '\033[3J'                                        # clear scrollback only
```

## Sources

- [ECMA-48 §8.3](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [VT510 DECSCA](https://vt100.net/docs/vt510-rm/DECSCA.html)
- [terminfo(5), `bce` and `E3`](https://invisible-island.net/ncurses/man/terminfo.5.html)
