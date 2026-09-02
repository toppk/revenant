---
man: revenant-options
section: 7
manual: revenant
description: command-line options and the resources they set
---

# Command-line options

xterm's command-line options are almost all shorthand for resources: `-sl
5000` and `-xrm 'XTerm*saveLines: 5000'` do the same thing, and a value
given on the command line beats any resource file. This page lists the
options Revenant implements today. The status of every option xterm defines is
in the [command-line feasibility](../compatibility/command-line-feasibility.md) study.

xterm's convention: `-opt` turns a boolean on and `+opt` turns it off. A few
options (`-si`, `-fbb`) are named for the *inhibit*, so `-si` disables the
behaviour; the table gives the resource value each form sets.

Unknown options and missing values fail before Revenant opens an X display,
using xterm-style error and usage output. Run `revenant -help` for the accepted
inventory; an option not shown there is not silently accepted.

<!-- markdownlint-disable MD013 -->

## Running a command

| Option | Meaning |
| --- | --- |
| `-e command [args…]` | Run `command` instead of `$SHELL`. Must be last; everything after it is the command |

## Fonts

| Option | Resource | Meaning |
| --- | --- | --- |
| `-fn font` | `vt100.font` | Bitmap font for the Default slot |
| `-fb font` | `vt100.boldFont` | Bold fallback for the Xft path; bitmap bold remains incomplete |
| `-fwb font` | `vt100.wideBoldFont` | Wide-bold fallback for the Xft path; bitmap wide-bold remains incomplete |
| `-fa pattern` | `vt100.faceName` | Xft face (fontconfig pattern) |
| `-fs size` | `vt100.faceSize` | Xft point size |
| `-fd pattern` | `vt100.faceNameDoublesize` | Xft face for wide text and emoji fallback |
| `-fe pattern` | `vt100.faceNameEmoji` | Preferred Xft face for emoji presentation |

## Colours, window, geometry

| Option | Resource | Meaning |
| --- | --- | --- |
| `-bg color` | `*background` | Background |
| `-fg color` | `*foreground` | Foreground |
| `-bd color` | `.borderColor` | Shell border color |
| `-bw pixels` / `-w pixels` | `.borderWidth` | Shell border width |
| `-rv` / `+rv`, `-r` / `+r` | `vt100.reverseVideo` true / false | Swap the configured default foreground and background |
| `-geometry WxH+X+Y` | `.geometry` | Size in characters, position in pixels |
| `-b pixels` | `vt100.internalBorder` | Padding inside the window |
| `-cr color` | `vt100.cursorColor` | Text cursor color |
| `-title string` / `-T string` | `.title` | Window title |
| `-n string` | `.iconName` | Icon name |
| `#geom` | `.iconGeometry` | Icon geometry in xterm's sticky form |
| `-iconic` | `.iconic` | Start iconified |
| `-name string` | — | Override the application instance; with `-e`, default title/icon still use the child basename |
| `-class string` | — | Override the application class |
| `-display name` | — | X server to connect to |
| `-xrm 'spec: value'` | any | Any resource, repeatable |

`-bg`, `-fg`, `-geometry`, `-title`, `-iconic`, `-display`, and `-xrm` are
parsed by the X Toolkit itself and work in every Xt program.

## Scrollback and scrollbar

| Option | Resource | Meaning |
| --- | --- | --- |
| `-sl lines` | `vt100.saveLines` | History size |
| `-sb` / `+sb` | `vt100.scrollBar` true / false | Show the scrollbar |
| `-rightbar` / `-leftbar` | `vt100.rightScrollBar` true / false | Which side |
| `-si` / `+si` | `vt100.scrollTtyOutput` **false** / true | Inhibit scroll-to-bottom on output |
| `-sk` / `+sk` | `vt100.scrollKey` true / false | Scroll to bottom on keypress |
| `-mc milliseconds` | `vt100.multiClickTime` | Multi-click selection interval |
| `-cc classrange` | `vt100.charClass` | Override double-click character classes |

## Cursor

| Option | Resource | Meaning |
| --- | --- | --- |
| `-ah` / `+ah` | `vt100.alwaysHighlight` true / false | Filled cursor even when unfocused |
| `-bc` / `+bc` | `vt100.cursorBlink` true / false | Configured blink operand; combined with application state by `cursorBlinkXOR` |
| `-bcn milliseconds` | `vt100.cursorOnTime` | Time the blinking cursor remains visible |
| `-bcf milliseconds` | `vt100.cursorOffTime` | Time the blinking cursor remains hidden |

Use `-xrm 'XTerm*cursorBlink: always'` or `never` for a forced blink policy
that ignores application blink requests.

## Diagnostics

| Option | Resource | Meaning |
| --- | --- | --- |
| `-log level` | `logLevel` | Minimum stderr severity: `debug`, `info`, `warning` (default), or `error` |
| `-debug` / `+debug` | `logLevel` debug / warning | xterm-compatible aliases for verbose/default logging |
| `-report-config` | `reportConfig` | Print the resolved configuration report and exit |
| `-report-font-routing` | `vt100.reportFontRouting` | Collect bounded font-routing records for `report-font-routing()` snapshots |
| `-help` / `--help` | — | Print accepted options and exit without opening a display |
| `-version` / `--version` | — | Print the version and exit; the double-dash form is Revenant specific |
| `--self-test` | — | Run the built-in self test and exit (Revenant specific) |

!!! note "Unsupported means rejected"
    For example, `revenant -ls` currently exits with a bad-option diagnostic;
    it does not start an ordinary non-login shell. The
    [feasibility table](../compatibility/command-line-feasibility.md) classifies
    every patch-411 option that is not yet accepted.

Single-dash options may use an unambiguous prefix, as in xterm: `-geo` means
`-geometry` and `-clas` means `-class`. Exact spellings take precedence;
ambiguous prefixes such as `-fo` are rejected. Revenant's double-dash
extensions require their full spelling.

<!-- markdownlint-enable MD013 -->
