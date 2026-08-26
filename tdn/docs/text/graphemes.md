# Grapheme clusters

Status: Extension. Mode 2027 is specified by the terminal-wg working group
and originated in Contour.

A grapheme cluster is what a reader perceives as one character. Unicode Standard
Annex 29 (UAX #29) defines the boundaries: a base plus combining marks, an emoji ZWJ
sequence, a regional-indicator pair forming a flag, or a base plus skin-tone
modifier are each one cluster. A classic terminal ignores clusters and
measures code points one at a time.

## Mode 2027

```text
CSI ? 2027 h    treat each grapheme cluster as one unit
CSI ? 2027 l    measure code points individually (legacy)
CSI ? 2027 $ p  query; reply CSI ? 2027 ; Ps $ y
```

The DECRQM reply value tells an application what to expect: `1` set, `2`
reset, `3` permanently set, `4` permanently reset, `0` unrecognized. A reply
of `0` or no reply means the terminal does not know the mode and clusters are
not guaranteed.

## What changes when it is set

With the mode set the terminal:

- stores one cluster in one cell run, whose width is the width of the cluster
  as a whole, normally 2 for emoji sequences and 1 or 2 for scripts with
  combining marks;
- does not advance the cursor for a joiner, modifier, or variation selector
  that extends the current cluster;
- treats a regional-indicator pair as one width-2 unit rather than two;
- applies the same rule to the cursor-position report, so a probe sees the
  clustered width.

With the mode reset, behavior is whatever the emulator did before; the spec
does not require code-point-by-code-point measurement, only that the
terminal be honest through the query.

Some emulators cluster unconditionally and report the mode as permanently set
(`3`). That is conforming.

## Application behavior

1. Query `CSI ? 2027 $ p` with a timeout.
2. On `1` or `3`, measure strings with a UAX #29 segmenter and a cluster
   width function.
3. Otherwise, measure with `wcwidth` per code point and avoid ZWJ sequences
   where alignment matters.

Libraries that already do this include `unicode-width` plus
`unicode-segmentation` (Rust), `utf8proc`, and `libgrapheme`.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Emulator | Mode 2027 | Notes |
| --- | --- | --- |
| xterm | ? | Not listed in ctlseqs[^ctl] |
| VTE | ? | |
| Konsole | ? | |
| kitty | No[^kitty] | Clusters unconditionally; the author rejects the mode and relies on the [text sizing protocol](sizing.md) |
| WezTerm | Partial[^wez] | Clusters unconditionally; mode reported as `?` |
| Ghostty | Yes[^ghostty] | Listed in the VT reference |
| foot | Yes[^foot] | Since grapheme shaping was enabled by default |
| Alacritty | ? | Not implemented (`?` for issue status) |
| Contour | Yes[^contour] | Origin of the mode |
| mintty | ? | |
| PuTTY | ? | |
| Windows Terminal | ? | Issue tracked upstream |
| Apple Terminal | ? | |
| iTerm2 | ? | |
| xterm.js | ? | Per-code-point storage |
| tmux | ? | Does not pass through; own width tables |

<!-- markdownlint-enable MD013 -->

[^ctl]: [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).
[^kitty]: [kitty text sizing protocol](https://sw.kovidgoyal.net/kitty/text-sizing-protocol/), introduction.
[^wez]: [WezTerm escape sequences](https://wezterm.org/escape-sequences.html).
[^ghostty]: [Ghostty VT reference, mode 2027](https://ghostty.org/docs/vt/csi/decset).
[^foot]: [foot changelog](https://codeberg.org/dnkl/foot/src/branch/master/CHANGELOG.md).
[^contour]: [Contour, unicode-core mode](https://github.com/contour-terminal/contour/blob/master/docs/vt-extensions/index.md).

## Probe

```sh
stty -echo -icanon min 0 time 5
printf '\033[?2027$p'; dd bs=64 count=1 2>/dev/null | cat -v; echo
printf '\033[?2027h'
printf '\r\033[K👨‍👩‍👧\033[6n'; dd bs=64 count=1 2>/dev/null | cat -v; echo
stty sane
```

A clustering terminal reports column 3 after the family emoji; a legacy one
reports 5 or 7.

## Sources

- [terminal-wg, grapheme cluster specification](https://gitlab.freedesktop.org/terminal-wg/specifications)
- [Unicode Standard Annex #29, Text Segmentation](https://www.unicode.org/reports/tr29/)
- [Contour VT extensions](https://github.com/contour-terminal/contour/blob/master/docs/vt-extensions/index.md)
