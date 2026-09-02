---
man: revenant-hyperlinks
section: 7
manual: revenant
description: OSC 8 hyperlinks and how to open them
---

# OSC 8 hyperlinks

Terminal applications can attach a URI to arbitrary visible text with the
OSC 8 control sequence. Revenant recognizes those links; it does not guess URLs
from ordinary terminal text.

Try it with:

```sh
printf '\e]8;;http://example.com\e\\This is a link\e]8;;\e\\\n'
```

## Opening a link

Hold Shift while the pointer is over linked text. Revenant underlines all
visible cells carrying that target. Shift+Button 1 opens an `http://` or
`https://` target with `xdg-open` when the button is released over the same
target.

The modifier keeps hyperlink activation separate from the established mouse
bindings:

- Button 1 without Shift starts a selection.
- Button 2 pastes the selected X11 data.
- Ctrl+buttons 1, 2, and 3 open the xterm popup menus.
- When an application has enabled mouse reporting, Shift retains the local
  terminal gesture instead of sending it to the application.

OSC 8 targets using `file:`, `mailto:`, `ftp:`, or any other scheme still
underline on Shift-hover, but Shift-click deliberately does nothing. This
lets Revenant expose the terminal's actual hyperlink state without handing
arbitrary URI schemes to a desktop launcher.

## Security model

An OSC 8 label and its target can be different: text which says
`example.com` can point somewhere else. The underline confirms that the cells
carry a link, not that the visible label describes its destination.

Revenant passes an allowed target to `xdg-open` as one argument without invoking
a shell. It rejects embedded NUL bytes and permits only the case-insensitive
`http://` and `https://` prefixes. The browser or desktop handler remains
responsible for displaying and applying its own policy to the destination.

For protocol syntax, application guidance, and compatibility across terminal
emulators, see the
[TDN OSC 8 reference](https://toppk.github.io/revenant/tdn/osc/hyperlinks/).
