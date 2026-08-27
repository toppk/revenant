# OSC catalog

Every OSC selector TDN knows about, in numeric order, with its origin and
the page that documents it. The selector space has no registry; xterm's
`ctlseqs` is the closest thing to one, and the numbers above 100 that are
not resets were chosen by individual vendors, sometimes colliding.

Status uses the [conventions](../conventions.md#status-vocabulary)
vocabulary. "Origin" names the document that first defined the selector,
which is not always the emulator most associated with it today.

<!-- markdownlint-disable MD013 -->

| Ps | Name | Origin | Status | Page |
| --- | --- | --- | --- | --- |
| 0 | Set icon name and window title | xterm | xterm | [Titles](title.md) |
| 1 | Set icon name | xterm | xterm | [Titles](title.md) |
| 2 | Set window title | xterm | xterm | [Titles](title.md) |
| 3 | Set X11 window property | xterm | Vendor-private | [Miscellany](misc.md#osc-3-x-property) |
| 4 | Set or query palette color `index` | xterm | xterm | [Colors](colors.md#palette-osc-4-and-osc-5) |
| 5 | Set or query special color (bold, underline, blink, reverse) | xterm | xterm | [Colors](colors.md#palette-osc-4-and-osc-5) |
| 6 | Current document (`file://` URL) | Apple Terminal | Vendor-private | [Miscellany](misc.md#osc-6-and-7-apple-document-and-directory) |
| 6 | Enable/disable special color | xterm | Disputed | [Colors](colors.md) |
| 7 | Current working directory (`file://` URL) | Apple Terminal | Extension | [Working directory](cwd.md) |
| 8 | Hyperlink | VTE / iTerm2 | Extension | [Hyperlinks](hyperlinks.md) |
| 9 | Desktop notification | iTerm2 | Extension | [Notifications](notifications.md#osc-9-iterm2-notification) |
| 9 ; 1–10 | ConEmu commands (sleep, message box, progress, directory) | ConEmu | Extension | [Notifications](notifications.md#osc-9-collisions-conemu) |
| 9 ; 4 | Taskbar / progress indicator | ConEmu | Extension | [Notifications](notifications.md#osc-94-progress) |
| 9 ; 9 | Current working directory (Windows path) | ConEmu | Extension | [Working directory](cwd.md#osc-99) |
| 10 | Default foreground color | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 11 | Default background color | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 12 | Cursor color | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 13 | Pointer foreground | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 14 | Pointer background | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 15 | Tektronix foreground | xterm | Vendor-private | [Colors](colors.md#dynamic-colors-osc-1019) |
| 16 | Tektronix background | xterm | Vendor-private | [Colors](colors.md#dynamic-colors-osc-1019) |
| 17 | Highlight (selection) background | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 18 | Tektronix cursor | xterm | Vendor-private | [Colors](colors.md#dynamic-colors-osc-1019) |
| 19 | Highlight (selection) foreground | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 21 | Color control (`key=value` set/query) | kitty | Vendor-private | [Miscellany](misc.md#osc-21-kitty-color-control) |
| 22 | Pointer (mouse cursor) shape | xterm | Extension | [Miscellany](misc.md#osc-22-pointer-shape) |
| 46 | Log file | xterm | Vendor-private | not documented; disabled at compile time in most builds |
| 50 | Font | xterm | xterm | [Miscellany](misc.md#osc-50-font) |
| 51 | Reserved (Emacs shell) | xterm | Vendor-private | not documented |
| 52 | Clipboard and selection access | xterm | xterm | [Clipboard](clipboard.md) |
| 60–61 | Query allowed/disallowed features | xterm | Vendor-private | not documented |
| 66 | Text sizing | kitty | Extension | [Text sizing](../text/sizing.md) |
| 99 | Desktop notification (structured) | kitty | Extension | [Notifications](notifications.md#osc-99-kitty-desktop-notifications) |
| 104 | Reset palette color(s) | xterm | xterm | [Colors](colors.md#palette-osc-4-and-osc-5) |
| 105 | Reset special color(s) | xterm | xterm | [Colors](colors.md#palette-osc-4-and-osc-5) |
| 106 | Enable/disable special color | xterm | xterm | [Colors](colors.md) |
| 110–119 | Reset the dynamic color set by 10–19 | xterm | xterm | [Colors](colors.md#dynamic-colors-osc-1019) |
| 133 | Semantic prompt marks (`A`/`B`/`C`/`D`) | FinalTerm | Extension | [Shell integration](shell-integration.md) |
| 440 | Audio | mintty | Vendor-private | not documented |
| 701 | Locale | rxvt-unicode | Vendor-private | not documented |
| 777 | Notification (`notify`) and other urxvt extensions | rxvt-unicode | Extension | [Notifications](notifications.md#osc-777-rxvt-unicode-notify) |
| 1337 | iTerm2 family: `File=`, `CurrentDir=`, `SetUserVar=`, `SetMark`, shell integration | iTerm2 | Extension | [Miscellany](misc.md#osc-1337-iterm2-family), [Inline images](../graphics/iterm2.md), [Shell integration](shell-integration.md#osc-1337-iterm2-integration) |
| 7704 | Special color (WezTerm) | WezTerm | Vendor-private | not documented |
| 7770 | Font size | mintty | Vendor-private | not documented |
| 7771 | Glyph check | mintty | Vendor-private | not documented |
| 7777 | Window size and position | mintty | Vendor-private | not documented |
| 9001 | Kitty remote control | kitty | Vendor-private | not documented |

<!-- markdownlint-enable MD013 -->

## Collisions

- **OSC 6** means "current document" to Apple Terminal and
  "enable special color" to xterm. Neither reads the other's syntax
  successfully; emulators pick one.
- **OSC 9** is an iTerm2 notification unless the first field is a small
  integer followed by `;`, which is ConEmu syntax. Windows Terminal and
  WezTerm implement the ConEmu subset; kitty and iTerm2 the notification.
- **OSC 22** in xterm is the pointer shape; older mintty releases used 22
  for something else and now follow xterm.
- Every selector from a single vendor above 1000 (1337, 7704, 777x, 9001)
  was chosen to avoid collision by being unlikely; there is no coordination.

## Not documented

Rows marked "not documented" are selectors an emulator implements that TDN
has no page for. They are listed so a parser author knows the number is
taken. A page is warranted when a second emulator adopts one.

## Sources

- [XTerm Control Sequences, Operating System Commands](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
- [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs)
- [rxvt-unicode(7)](https://man.archlinux.org/man/urxvt.7)
- [iTerm2 proprietary escape codes](https://iterm2.com/documentation-escape-codes.html)
- [kitty protocol extensions](https://sw.kovidgoyal.net/kitty/protocol-extensions/)
- [WezTerm escape sequences](https://wezfurlong.org/wezterm/escape-sequences.html)
- [ConEmu ANSI escape codes](https://conemu.github.io/en/AnsiEscapeCodes.html)
