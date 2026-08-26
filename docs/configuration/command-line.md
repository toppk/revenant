# Command-line options

xterm's command-line options are almost all shorthand for resources: `-sl
5000` and `-xrm 'XTerm*saveLines: 5000'` do the same thing, and a value
given on the command line beats any resource file. This page lists the
options xterm+ implements today. The status of every option xterm defines is
in the [command-line feasibility](../compatibility/command-line-feasibility.md) study.

xterm's convention: `-opt` turns a boolean on and `+opt` turns it off. A few
options (`-si`, `-fbb`) are named for the *inhibit*, so `-si` disables the
behaviour; the table gives the resource value each form sets.

<!-- markdownlint-disable MD013 -->

## Running a command

| Option | Meaning |
| --- | --- |
| `-e command [args…]` | Run `command` instead of `$SHELL`. Must be last; everything after it is the command |

## Fonts

| Option | Resource | Meaning |
| --- | --- | --- |
| `-fn font` | `vt100.font` | Bitmap font for the Default slot |
| `-fa pattern` | `vt100.faceName` | Xft face (fontconfig pattern) |
| `-fs size` | `vt100.faceSize` | Xft point size |
| `-fd pattern` | `vt100.faceNameDoublesize` | Accepted; not applied yet |

## Colours, window, geometry

| Option | Resource | Meaning |
| --- | --- | --- |
| `-bg color` | `*background` | Background |
| `-fg color` | `*foreground` | Foreground |
| `-geometry WxH+X+Y` | `.geometry` | Size in characters, position in pixels |
| `-b pixels` | `vt100.internalBorder` | Padding inside the window |
| `-title string` | `.title` | Window title |
| `-iconic` | `.iconic` | Start iconified |
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
| `-bc` / `+bc` | `vt100.cursorBlink` true / false | Blinking or steady configured default; application requests remain effective |
| `-bcn milliseconds` | `vt100.cursorOnTime` | Time the blinking cursor remains visible |
| `-bcf milliseconds` | `vt100.cursorOffTime` | Time the blinking cursor remains hidden |

Use `-xrm 'XTerm*cursorBlink: always'` or `never` for a forced blink policy
that ignores application blink requests.

## Diagnostics

| Option | Resource | Meaning |
| --- | --- | --- |
| `-debug` / `+debug` | `debug` | Verbose stderr log (default off) |
| `-report-config` | `reportConfig` | Print the resolved configuration report and exit |
| `--version` | — | Print the version and exit (xterm+ specific; xterm uses `-version`) |
| `--self-test` | — | Run the built-in self test and exit (xterm+ specific) |

!!! warning "Unknown options are ignored silently"
    Options xterm+ does not know are left in `argv` and dropped. `xterm+ -ls`
    starts without a login shell and without complaint. Until the option
    table is complete, check the [feasibility table](../compatibility/command-line-feasibility.md)
    when an option seems to have no effect.

<!-- markdownlint-enable MD013 -->
