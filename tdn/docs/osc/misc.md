# Miscellaneous OSCs

Selectors that do not fit another page. Each is documented by one emulator
family; treat every one as non-portable.

## OSC 3: X property

Status: xterm.

```text
OSC 3 ; prop=value ST    set an X11 window property
OSC 3 ; prop ST          delete it
```

Sets an arbitrary property on the xterm window. Disabled unless
`allowWindowOps` is true. No non-X11 emulator implements it.

## OSC 50: font

Status: xterm.

```text
OSC 50 ; fontspec ST    change the font
OSC 50 ; ? ST           query the current font
```

`fontspec` is an X logical font description or `#n` to select the nth font
from `xterm`'s font menu; `#+1` and `#-1` step. Gated by `allowFontOps`.
kitty reuses OSC 50 for nothing; mintty accepts it with its own syntax.

## OSC 22: pointer shape

Status: xterm.

```text
OSC 22 ; shape ST
```

Sets the mouse pointer shape by X cursor name (`xterm`, `arrow`,
`crosshair`, `hand2`). kitty adopted it with a push/pop extension:

```text
OSC 22 ; >shape ST    push
OSC 22 ; <  ST        pop
OSC 22 ; ?__current__ ST   query
```

Supported by xterm, kitty, and foot; others ignore it.

## OSC 21: kitty color control

Status: Vendor-private (kitty).

```text
OSC 21 ; key=value ; key=value ST
OSC 21 ; key=? ST
```

Sets or queries named colors (`foreground`, `background`, `cursor`,
`selection_background`, `0`–`255`, `visual_bell`, mark colors) in one
string, with `reset` values. Overlaps OSC 4/10/11 but lets one string reset
or query many at once. See [Colors](colors.md) for the portable forms.

kitty also defines a color stack:

```text
CSI # P    push colors
CSI # Q    pop colors
CSI # R    report stack depth
```

The same `CSI # P` and `CSI # Q` are xterm's XTPUSHCOLORS and XTPOPCOLORS.

## OSC 66: text sizing

Status: Vendor-private (kitty 0.40).

```text
OSC 66 ; s=scale : w=width : n=numerator : d=denominator ; text ST
```

Renders `text` at a multiple of the cell size or as a fraction of a cell,
for large headings and subscripts. See [Text](../text/index.md) for width
implications.

## OSC 1337: iTerm2 family

Status: Vendor-private (iTerm2) with partial adoption.

`OSC 1337` multiplexes many commands by keyword. The ones covered elsewhere:

- `File=…` inline images: [Graphics](../graphics/index.md).
- `CurrentDir=`, `RemoteHost=`: [Working directory](cwd.md).
- `ShellIntegrationVersion=`, `SetUserVar=`, `SetMark`:
  [Shell integration](shell-integration.md).

Others: `CursorShape=0|1|2` (block, bar, underline), `StealFocus`,
`ClearScrollback`, `RequestAttention=yes|no|fireworks`, `CopyToClipboard=`
… `EndCopy`, `SetBadgeFormat=base64`, `ReportCellSize`,
`SetColors=name=color`, `HighlightCursorLine=yes|no`,
`UnicodeVersion=8|9`. WezTerm implements `File=`, `SetUserVar=`, and a few
others; mintty implements `File=`.

## OSC 6 and 7: Apple document and directory

Status: Convention (Apple Terminal).

```text
OSC 6 ; file://host/path ST    current document
OSC 7 ; file://host/path ST    current working directory
```

Apple Terminal uses OSC 6 to show a proxy icon for the file being edited.
OSC 7 became the portable directory report; see
[Working directory](cwd.md). Nothing else implements OSC 6.

## OSC 133

See [Shell integration](shell-integration.md).

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OSC 3 | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| OSC 50 font | Yes | ? | ? | ? | ? | ? | ? | ? | ? | Yes | ? | ? | ? | ? | ? | ? |
| OSC 22 pointer | Yes | ? | ? | Yes | ? | ? | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| OSC 21 | ? | ? | ? | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| `CSI # P` / `# Q` | Yes | ? | ? | Yes | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| OSC 66 | ? | ? | ? | Yes (0.40) | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| OSC 1337 (any) | ? | ? | ? | ? | Partial | Partial | ? | ? | ? | Partial | ? | ? | ? | Yes | ? | ? |
| OSC 6 | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | Yes | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
printf '\033]22;hand2\033\\'; sleep 2; printf '\033]22;xterm\033\\'
tools/query '\033]50;?\033\\'    # xterm: font query
printf '\033]1337;RequestAttention=fireworks\033\\'
```

## Sources

- [XTerm Control Sequences, OSC](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Operating-System-Commands)
- [kitty: pointer shapes](https://sw.kovidgoyal.net/kitty/pointer-shapes/)
- [kitty: color control](https://sw.kovidgoyal.net/kitty/color-stack/)
- [kitty: text sizing protocol](https://sw.kovidgoyal.net/kitty/text-sizing-protocol/)
- [iTerm2 proprietary escape codes](https://iterm2.com/documentation-escape-codes.html)
- [WezTerm escape sequences](https://wezterm.org/escape-sequences.html)
- [mintty control sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs)
- [foot: escape sequences](https://codeberg.org/dnkl/foot/src/branch/master/doc/foot-ctlseqs.7.scd)
