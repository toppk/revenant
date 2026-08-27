# CSI: Control Sequence Introducer

CSI control sequences are compact, in-band commands carried in the same byte
stream as terminal text. Applications use them to move the cursor, erase
regions, select graphic rendition, change modes, and request information from
the terminal.

## History and status

The general CSI framework comes from the ANSI X3.64 and ECMA-48 family of
control-function standards. DEC terminals built a larger private VT protocol
on that framework. Xterm implemented DEC behavior and added extensions of its
own; later terminal emulators inherited overlapping subsets of all three.

That history explains why “supports CSI” is not a useful compatibility claim.
The introducer and byte grammar can be shared while individual final bytes,
private markers, parameters, defaults, and responses differ.

## Start here

- [Anatomy and parsing](anatomy.md) explains the wire grammar and how to read
  notation such as `CSI ? 1004 h` and `CSI 2 SP q`.
- [Catalog](catalog.md) lists every function by final byte with its origin
  and the page that documents it.
- [Cursor controls](cursor.md) covers style, blinking, and visibility, including
  the disputed meaning of DECSCUSR parameter 0.
- [Modes](modes.md) covers set/reset commands, DEC private modes, and common
  application modes.
- [Queries and reports](queries.md) covers terminal responses and why they must
  be treated as an asynchronous input protocol.

## Utility and limitations

CSI works well over a PTY because commands are short and require no side
channel. The same property creates limitations:

- terminal state is shared by every program using the PTY;
- most settings do not have a portable push/pop operation;
- applications may terminate without restoring state;
- queries and keyboard input share one response stream;
- byte sequences may be split across arbitrary reads;
- private extensions can collide or acquire incompatible defaults.

Terminfo helps applications choose known capabilities, but it is a capability
database rather than a complete semantic specification.

## Primary references

- [ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [DEC manuals archive](https://vt100.net/docs/)
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [terminfo](https://invisible-island.net/ncurses/man/terminfo.5.html)
