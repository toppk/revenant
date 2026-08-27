# Hyperlinks

Status: Extension (VTE 0.50, 2017).

OSC 8 attaches a URI to a run of printed text. The text renders normally; the
emulator makes it clickable and may underline it on hover. Unlike heuristic
URL detection, the visible text and the target can differ.

## Syntax

```text
OSC 8 ; params ; URI ST   start a link
text
OSC 8 ; ; ST              end the link
```

`params` is a `:`-separated list of `key=value` pairs; only `id` is defined.
An empty URI ends the current link. The URI is not percent-encoded by the
terminal; the application must produce a valid URI.

```sh
printf '\033]8;;https://example.com\033\\Example\033]8;;\033\\\n'
printf '\033]8;id=a1;https://example.com\033\\two\033]8;;\033\\ '
printf '\033]8;id=a1;https://example.com\033\\cells\033]8;;\033\\\n'
```

## Behavior

- A link is an attribute of each cell, like a color. It survives scrolling
  into scrollback and is cleared by erase operations that reset attributes.
- Cells with the same URI and the same non-empty `id` form one logical link
  even when separated by other cells or wrapped across lines. Without an
  `id`, adjacent cells with the same URI are usually treated as one link;
  cells on different lines with the same URI but no `id` are separate links.
- Emulators apply a scheme allowlist. `http`, `https`, `file`, `mailto`, and
  `ftp` are near-universal; VTE additionally accepts a fixed set, kitty and
  foot let configuration decide, and some emulators pass everything to the
  OS URL opener. A scheme outside the allowlist is dropped without an error.
- Rendering is not specified. VTE shows nothing until hover; kitty, foot, and
  WezTerm underline on hover by default and can underline always; iTerm2
  underlines on modifier-hover. Applications must not depend on visual
  feedback to indicate a link.
- URI length is capped (VTE: 2083 bytes, matching Internet Explorer's old
  limit); longer links are ignored.

Applications should emit the link end sequence explicitly rather than
relying on line breaks or resets, and should not open an OSC 8 that spans a
scroll region change.

xterm does not implement OSC 8; its maintainer has stated that the design
cannot be made safe for the visible-text/target mismatch. Programs that gate
on `TERM=xterm*` to decide whether to emit links will therefore be wrong in
both directions.

### xterm+ interaction policy

xterm+ is distinct from upstream xterm and does implement OSC 8 through its
`libghostty-vt` terminal core. Hold Shift while hovering to underline linked
cells, then use Shift+Button 1 to activate a target. The press and release must
occur on the same target while Shift remains held.

Only `http://` and `https://` targets are passed to `xdg-open`. Other schemes
still receive hover feedback but are inert. xterm+ invokes the opener directly
with the URI as one argument, without a shell, and does not recognize plain
URL-looking text as a link.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OSC 8 | No[^xt] | Yes (0.50) | Yes | Yes | Yes | Yes | Yes | Yes (0.11) | Yes | Yes | ? | Yes (1.4) | ? | Yes | Yes (5.0) | Yes (3.4)[^tm] |
| `id=` grouping | — | Yes | ? | Yes | Yes | ? | Yes | ? | ? | ? | — | ? | — | ? | ? | Yes |

<!-- markdownlint-enable MD013 -->

[^xt]: Deliberately unimplemented; see the xterm FAQ.
[^tm]: Requires the outer terminal to support hyperlinks; tmux forwards the
    attribute rather than rendering it.

## Probe

```sh
tools/sendosc link https://example.com 'Example link'
printf '\033]8;;mailto:test@example.com\033\\mail\033]8;;\033\\\n'
printf '\033]8;;javascript:alert(1)\033\\should not open\033]8;;\033\\\n'
```

Hover or click the output. The third line tests the scheme allowlist.
In xterm+, hold Shift while hovering or clicking; only the first link opens.

## Sources

- [Hyperlinks (a.k.a. HTML-like anchors) in terminal emulators](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda)
- [VTE 0.50 release notes](https://gitlab.gnome.org/GNOME/vte/-/blob/master/NEWS)
- [xterm FAQ, OSC 8](https://invisible-island.net/xterm/xterm.faq.html#hyperlinks)
- [kitty: hyperlinks](https://sw.kovidgoyal.net/kitty/open_actions/)
- [foot(1), OSC 8](https://codeberg.org/dnkl/foot)
- [Windows Terminal 1.4 release notes](https://github.com/microsoft/terminal/releases/tag/v1.4.3141.0)
- [xterm.js hyperlinks](https://xtermjs.org/docs/api/terminal/interfaces/ilinkprovider/)
- [tmux 3.4 CHANGES](https://raw.githubusercontent.com/tmux/tmux/master/CHANGES)
