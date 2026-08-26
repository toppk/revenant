# Glossary

**Alternate screen**
: A second screen buffer without scrollback, entered with `?1049`, used by
  full-screen programs so the shell's history survives.

**APC**
: Application Program Command, `ESC _ … ST`. Carries the Kitty graphics
  protocol; otherwise ignored.

**Application**
: The program writing to the PTY: a shell, an editor, a TUI.

**bce**
: Background color erase. Erased cells take the current background color.

**C0, C1**
: The control-character sets at `0x00`–`0x1f` and `0x80`–`0x9f`. C1 has
  seven-bit aliases `ESC @`–`ESC _`.

**Cell**
: One column of one row in the grid. Wide characters occupy two.

**ConPTY**
: The Windows pseudoconsole. Sits between every Windows terminal and its
  applications and re-encodes the byte stream.

**CPR**
: Cursor Position Report, `CSI row ; col R`.

**CSI**
: Control Sequence Introducer, `ESC [`.

**DA1, DA2, DA3**
: Device Attributes. Identity queries answered by every terminal.

**DCS**
: Device Control String, `ESC P … ST`. Carries Sixel, DECRQSS, XTGETTCAP.

**DEC private mode**
: A mode set with `CSI ? Pm h`. The namespace DEC reserved and xterm
  extended.

**DECRQM**
: Request Mode; the query that reports whether a mode is set or known.

**DECSCUSR**
: Set Cursor Style.

**Emulator**
: The program that owns the PTY master, parses the stream, and renders
  cells.

**Grapheme cluster**
: The user-perceived character defined by UAX #29; may be many code points.

**Intermediate byte**
: A byte in `0x20`–`0x2f` between parameters and the final byte of a CSI.

**Kitty keyboard protocol**
: The progressive-enhancement keyboard encoding from kitty, adopted widely.

**Mode**
: A persistent switch set with `h`, reset with `l`.

**Multiplexer**
: tmux, screen, zellij. An emulator on one side, an application on the
  other.

**OSC**
: Operating System Command, `ESC ] … ST`. Titles, colors, hyperlinks,
  clipboard.

**Passthrough**
: Wrapping a sequence so a multiplexer forwards it to the outer terminal
  unchanged.

**Pending wrap**
: The state after printing in the last column: the cursor is drawn there and
  the next character wraps first.

**PTY**
: Pseudo-terminal. The kernel object with a master (emulator) and slave
  (application) end.

**Report**
: Bytes the emulator writes to PTY input in reply to a query.

**Scrolling region**
: The rows between the DECSTBM margins; the only rows that scroll.

**SGR**
: Select Graphic Rendition, `CSI Pm m`.

**Sixel**
: DEC's bitmap graphics format carried in a DCS.

**ST**
: String Terminator, `ESC \`.

**Sub-parameter**
: A `:`-separated part of one CSI parameter; used by SGR.

**terminfo**
: The capability database keyed by `TERM`.

**Truecolor**
: 24-bit color selected with SGR `38;2;r;g;b`.

**wcwidth**
: The C function, and by extension the algorithm, that maps a code point to
  a cell width of −1, 0, 1, or 2.

**XTVERSION**
: xterm's `CSI > 0 q` query returning the emulator name and version.
