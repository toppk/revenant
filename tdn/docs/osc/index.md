# OSC: Operating System Command

OSC control strings carry free-form text rather than numeric parameters.
They are used for everything that talks to the environment around the
grid: window titles, hyperlinks, palette changes, clipboard access,
working-directory reports, shell-integration marks, and desktop
notifications.

## History and status

ECMA-48 defines the OSC framing (`OSC … ST`) and nothing else: the
standard reserves the command string for "operating system" use and
assigns no meaning to it. xterm gave the first byte a numeric selector,
assigned 0–2 to titles, 4 and 10–19 to colors, 50 to fonts, and 52 to the
clipboard, and every later emulator inherited that numbering. Selectors
above 100 are vendor choices with no registry; some collide.

"Supports OSC" is therefore not a compatibility claim. Each selector is
its own feature with its own status, and a terminal that ignores an
unknown one correctly must still parse it correctly.

## Start here

- [Anatomy and parsing](anatomy.md): framing, terminators, length limits,
  direction, and the security model that gates reads and writes.
- [Catalog](catalog.md): every selector in numeric order, its origin, and
  where it is documented.

## Feature pages

- [Window title](title.md): OSC 0, 1, 2 and the title stack.
- [Hyperlinks](hyperlinks.md): OSC 8.
- [Colors](colors.md): OSC 4, 5, 10–19, 104–119 and color-scheme reports.
- [Clipboard](clipboard.md): OSC 52.
- [Working directory](cwd.md): OSC 7, OSC 1337 `CurrentDir`, OSC 9;9.
- [Shell integration](shell-integration.md): OSC 133 and OSC 1337.
- [Notifications and progress](notifications.md): OSC 9, 777, 99, 9;4.
- [Miscellany](misc.md): OSC 3, 21, 22, 50, 66, and Apple document URLs.

## Utility and limitations

OSC is the only channel an application has to the world outside the grid,
which is also its problem:

- an OSC can reach the clipboard, the window system, and the desktop, so
  emulators gate the dangerous ones and the gates differ;
- payloads are unbounded in the grammar and bounded differently in every
  implementation;
- multiplexers interpret OSCs on their own terms and drop what they do
  not know; see [Multiplexers](../practices/multiplexers.md);
- replies to OSC queries arrive as input and must be parsed alongside
  keystrokes, exactly like CSI [queries](../csi/queries.md).

## Primary references

- [ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [XTerm Control Sequences, Operating System Commands](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
