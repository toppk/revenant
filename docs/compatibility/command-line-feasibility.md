# Command-line feasibility study

Every option in xterm patch-410's `-help` output (`xtermOptions[]` in
`main.c`, 120 lines including `#ifdef` duplicates): what xterm
does behind it, whether libghostty-vt or plain X11 supplies the mechanism,
and how approachable it is for Revenant. Companion to the
[popup-menu study](menu-feasibility.md); reviewed against `src/main.c` and
the then-selected libghostty headers on 2026-08-25.

Status key: **DONE** implemented; **XT** parsed and applied by the X Toolkit
without Revenant code; **ACCEPTED** parsed into a resource that the code does
not act on yet; **READY** can be wired now with no new subsystem;
**ROADMAP** waits on a [roadmap](../maintainers/roadmap.md) slice (number
given); **BLOCKED** needs a libghostty change; **SKIP** recommend not
implementing (record in the [xterm differences ledger](drift.md)).

Totals over 96 rows (grouped where xterm's help lists one family several
ways): 16 done, 5 Xt-handled, 1 accepted, 41 ready, 15 roadmap, 5 blocked,
13 skip.

## The big pattern

Almost every xterm option is a one-line alias for a resource in
`XrmOptionDescRec` form. The cost is never the parsing — it is whether the
resource it names is applied. So this table is really a resource-application
study viewed from the command line, and its statuses track the
`-report-config` classifications: an option is DONE only when the resource
behind it is *supported* there.

Two structural gaps come first, because they affect every row:

1. **Unknown options are dropped silently.** Xt leaves unrecognised
   arguments in `argv`, and Revenant ignores whatever remains. `-ls`, `-fb
   fixed`, or a typo start a normal window with no message. xterm prints
   `bad command line option` and its usage. Fix: after `XtOpenDisplay`,
   treat any leftover argument other than `-e` as an error. READY, and the
   prerequisite for calling the surface complete.
2. **`-help` and `-version` are missing** in xterm's single-dash form;
   Revenant has `--version` and `--self-test`. `-help` should print the option
   table with one-line descriptions. READY.

## Process and session

<!-- markdownlint-disable MD013 -->

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-e command args…` | Run command in the PTY | `pty.c` | — | DONE |
| `-help` | Print usage | None | Print table; add descriptions to the option list | READY |
| `-version` | Print version | None | Alias of `--version` | READY |
| `-display name` | Xt standard | Xt | — | XT |
| `-class string` | Override application class before Xt init | Pre-scan argv, pass to `XtOpenDisplay` | Small; class is currently hard-coded `XTerm` | READY |
| `-name string` | Override instance name (`xterm.` resources, icon, title) | Same pre-scan; currently `"xterm"` is passed explicitly so Xt never sees `-name` | Same change as `-class`; Roadmap #6 names it explicitly | READY |
| `-/+ls` | `loginShell`: prefix `argv[0]` with `-` | `pty.c` exec | One line at exec time | READY |
| `-tn name` | `termName`: `TERM` for the child | `pty.c` environment; libghostty `OPT_TERMINFO_NAME` should match for XTGETTCAP | Set env; also feed the name to libghostty | READY |
| `-ti termid` | `decTerminalID`: DA responses (vt100/220/…) | libghostty answers DA itself; no terminal-ID option in the reviewed API | Needs an ID option or DA callback | BLOCKED |
| `-tm string` | `ttyModes`: stty-style keywords for the PTY | `pty.c` termios | Parse the small keyword language, apply with `tcsetattr` | READY |
| `-/+ie` | `ptyInitialErase`: take erase char from the PTY | `pty.c` termios | Read `VERASE`, feed backarrow mode | READY |
| `-/+im` | `useInsertMode`: termcap capability tweak | Only affects xterm's `TERMCAP` export | No TERMCAP export in Revenant | SKIP |
| `-baudrate rate` | Set PTY line speed | `pty.c` termios | `cfsetspeed`; low value but trivial | READY |
| `-/+hold` | Keep window after child exits | Event loop | Stop tearing down on `SIGCHLD`; keep rendering | READY |
| `-/+wf` | `waitForMap`: delay exec until mapped | Event loop | Defer `pty` spawn to first `MapNotify` | READY |
| `-/+ut` | utmp/wtmp entries | None; xterm uses `utempter` | Link `libutempter`; policy and packaging decision | ROADMAP #6 |
| `-/+mesg` | `messages`: `mesg y/n` on the PTY | `pty.c` | `chmod` slave | READY |
| `-Sccn` | Slave mode on an existing tty fd | `pty.c` | Rare; used by old session tools | SKIP |
| `-C` | Intercept console messages (`TIOCCONS`) | `pty.c` | Root-only, distro-disabled | SKIP |
| `-into windowId` | Reparent into a foreign window | `XReparentWindow` after realize | Small Xlib change; used by tabbed | READY |
| `-/+sm` | Session management | `XtSessionConnect` | Xt SM support; modest, low demand | ROADMAP #6 |
| `-/+samename` | Skip redundant title/icon updates | Title callback | Compare before `XtSetValues` | READY |
| `-ziconbeep percent` | Beep and mark icon on hidden output | Bell + `XUrgencyHint` while unmapped | Pair with `bellIsUrgent` work | READY |

## Window, geometry, and appearance

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-geometry geom` | Size in characters | — | — | DONE |
| `#geom` | `iconGeometry` | Xt shell resource | Sticky-arg entry in option table; resource already supported | READY |
| `%geom` | Tek window geometry | None | Tek not planned | SKIP |
| `-title string` / `-T string` | Window title | Xt shell | `-T` needs a table entry aliasing `.title` | XT / READY |
| `-n string` | `iconName` | Xt shell | Table entry aliasing `.iconName` | READY |
| `-iconic` | Start iconified | Xt | — | XT |
| `-/+maximized` | `_NET_WM_STATE_MAXIMIZED_*` at startup | EWMH client message | Same helper as the fullscreen menu entry | READY |
| `-/+fullscreen` | `_NET_WM_STATE_FULLSCREEN` at startup | EWMH client message | Same helper | READY |
| `-/+nomap` | Do not map the window; useful with `-into` | `mappedWhenManaged` | Shell already sets it False then maps; make it conditional | READY |
| `-b number` | `internalBorder` | — | — | DONE |
| `-bw number` / `-bd color` | Shell border width and colour | Xt | — | XT |
| `-/+rv` | `reverseVideo` at startup | Swap fg/bg before realize | Resource, command-line forms, and menu toggle share the widget color swap | DONE |
| `-/+t` | Start in Tek mode | None | Tek not planned | SKIP |
| `-/+tb` | Toolbar | None | Toolbar not planned | SKIP |
| `-/+ai` / `-fi fontname` | Active icon and its font | None | Modern WMs ignore | SKIP |
| `-/+rca` | Cursor adjustment on resize | libghostty reflow already keeps the cursor | Likely moot; verify and record | SKIP |

## Fonts

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-fn fontname` | Bitmap Default slot | — | — | DONE |
| `-fa pattern` | Xft face | — | — | DONE |
| `-fs size` | Xft size | — | — | DONE |
| `-fd pattern` | Double-width Xft face | Routed for wide text and emoji fallback | — | DONE |
| `-fb fontname` | `boldFont` for the bitmap path | Renderer: bitmap bold is synthetic today | Load a second `XFontStruct`; medium | ROADMAP #5 |
| `-fw` / `-fwb fontname` | Bitmap wide and wide-bold fonts | Renderer | Needs wide-cell bitmap drawing | ROADMAP #5 |
| `-fc fontmenu` | Start on a named font-menu slot | Font slot code exists | Look up slot by name at startup | READY |
| `-/+fbb` | `freeBoldBox`: skip bold/normal metric comparison | Renderer | Only meaningful once `-fb` exists | ROADMAP #5 |
| `-/+fbx` | `forceBoxChars`: internal line-drawing glyphs | Renderer | Same box-glyph rasteriser as `font-linedrawing` menu entry | ROADMAP #5 |
| `-fx fontname` | XIM fontset | Input method | Pass fontset to the XIM preedit style | READY |
| `-sh number` | `scaleHeight` | Renderer cell metrics | Multiply ascent+descent; simple | READY |
| `-/+bdc` | `colorBDMode` | Renderer palette | Roadmap #5 palette work | ROADMAP #5 |
| `-/+ulc` / `-/+itc` / `-/+rvc` | Underline/italic/reverse rendered as colour | Renderer palette | Same | ROADMAP #5 |
| `-/+ulit` | Underline drawn as italic | Renderer | Needs italic face | ROADMAP #5 |
| `-/+nul` | `underLine`: suppress underlining | Renderer | Flag check in the decoration path | READY |
| `-/+pc` | `boldColors`: PC-style bright bold | Renderer palette | Roadmap #5 | ROADMAP #5 |
| `-/+cm` | `colorMode`: disable ANSI colour | libghostty always parses SGR; renderer would render monochrome | Renderer flag; libghostty state unaffected | READY |
| `-/+dc` | `dynamicColors`: allow OSC 10–19 | libghostty applies OSC colours internally; `DATA_COLOR_*_DEFAULT` exposes defaults | Render from defaults when denied; queries still drift (see `allow-color-ops`) | ROADMAP #5 |

## Colours and pointer

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-bg color` / `-fg color` | Background / foreground | Xt → `vt100` core and `foreground` | — | XT |
| `-cr color` | `cursorColor` | Resource supported | Table entry aliasing `*cursorColor` | READY |
| `-ms color` | `pointerColor` | Resource merged, not applied | `XRecolorCursor`; pairs with `pointerShape` | READY |
| `-pf fontname` | `pointerFont` | None | Cursor font glyphs; trivial with `XCreateGlyphCursor` | READY |
| `-selbg` / `-selfg color`, `-/+hm` | Selection colours, `highlightColorMode` | Selection renderer | Roadmap #4 | ROADMAP #4 |

## Scrolling

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-sl number` | `saveLines` | — | — | DONE |
| `-/+sb` | `scrollBar` | — | — | DONE |
| `-rightbar` / `-leftbar` | `rightScrollBar` | — | — | DONE |
| `-/+si` | `scrollTtyOutput` inhibit | — | — | DONE |
| `-/+sk` | `scrollKey` | — | — | DONE |
| `-/+j` | `jumpScroll` | Render scheduling | Same knob as the `jumpscroll` menu entry | READY |
| `-/+s` | `multiScroll`: asynchronous scrolling | Render scheduling | Largely meaningless with direct painting; accept and ignore | SKIP |
| `-/+mb` / `-nb number` | Margin bell and its column | libghostty exposes cursor column | Ring bell when cursor crosses column on input | READY |

## Cursor

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-/+ah` | `alwaysHighlight` | — | — | DONE |
| `-/+bc`, `-bcf`, `-bcn ms` | Cursor blink and its on/off durations | Xt timer and `cursorBlink`/`cursorOnTime`/`cursorOffTime` resources | Four-value policy; aliases select `true`/`false` and the timing resources | DONE |
| `-/+uc` | `cursorUnderLine` | Renderer cursor shape; `OPT_DEFAULT_CURSOR_STYLE` | Alternate-shape drawing exists; wire the startup default | READY |

## Keyboard and input

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-kt keyboardtype` | Select legacy keyboard table (`sun`, `hp`, `sco`, `vt220`, `tcap`) | None; libghostty has one encoder table | Same family as the SKIP menu entries | SKIP |
| `-/+hf` / `-/+sf` / `-/+sp` | HP / Sun / Sun-PC function-key tables | Same | Same | SKIP |
| `-/+k8` | `allowC1Printable` | libghostty parser treats C1 as controls | No parser option | BLOCKED |
| `-/+132` | `c132`: allow DECCOLM | `GHOSTTY_MODE_ENABLE_MODE_3` | Mode flip plus grid poll, as `allow132` menu | READY |
| `-/+aw` | `autoWrap` | `GHOSTTY_MODE_WRAPAROUND` | Apply resource at startup; menu toggle exists | READY |
| `-/+rw` | `reverseWrap` | `GHOSTTY_MODE_REVERSE_WRAP` | Same | READY |
| `-/+cu` | `curses`: workaround for old curses `more` | Screen-manipulation quirk | Obsolete | SKIP |
| `-mc milliseconds` | `multiClickTime` | Selection gestures | Resource-backed click timing | DONE |
| `-cc classrange` | `charClass` for word selection | xterm-compatible class table in the terminal adapter | Supports `low[-high][:class]` ranges and X resource configuration | DONE |
| `-/+cb` / `-/+cn` | `cutToBeginningOfLine`, `cutNewline` | Selection formatting | Roadmap #4 | ROADMAP #4 |

## Encoding and locale

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-/+u8` / `-/+wc` | UTF-8 and wide-character mode | libghostty is UTF-8 only | Accept `-u8`/`-wc`; `+u8`/`+wc` would need iconv; recommend permanently on (DRIFT) | BLOCKED |
| `-/+lc` / `-lcc path` | Run the child through `luit` | Exec wrapper in `pty.c` | Spawn `luit` in front of the command | READY |
| `-/+mk_width` / `-/+cjk_width` / `-/+emoji_width` | Width conventions | libghostty owns width tables; no option in the reviewed API | Needs width-policy option | BLOCKED |

## Logging and printing

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| `-/+l` / `-lf filename` | Tee PTY output to a log file | `pty.c` read path | Same tee as the `logging` menu entry | READY |
| `-report-charclass` | Dump `charClass` table | Selection classes | With `-cc` | ROADMAP #4 |
| `-report-colors` | Log colour allocations | Renderer | Debug log already records colour cache; alias | READY |
| `-report-fonts` | Log loaded fonts | `-report-config` font section covers it | Alias to a subset of `-report-config` | READY |
| `-report-icons` | Log title/icon updates | Title callback; debug log already prints them | Alias | READY |
| `-report-xres` | Dump VT100 resources | `-report-config` is a superset | Alias | READY |
| `-report-config` | Revenant resolved-configuration report | — | Revenant extension, not an xterm option | DONE |
| `-log level` | Revenant severity threshold | Structured diagnostic logger | Revenant extension; debug, info, warning, or error | DONE |
| `-/+debug` | Debug log | `logLevel` debug / warning | Compatibility aliases around the threshold | DONE |

## Sixel and Tek

| Option | What xterm does | Mechanism | Approachability | Status |
| --- | --- | --- | --- | --- |
| Sixel-related (`sixelScrolling`, `privateColorRegisters`) | Sixel graphics | Not in libghostty | Blocked upstream; Kitty graphics is the roadmap path | BLOCKED |
| `-/+t`, `%geom` | Tek 4014 | None | Not planned | SKIP |

<!-- markdownlint-enable MD013 -->

## Suggested order

1. **Slice A — make the surface honest.** Reject unknown options, add
   `-help`/`-version`, and add every pure-alias entry for resources that are
   already supported: `-T`, `-n`, `#geom`, `-cr`. This is an afternoon and
   removes the silent-ignore trap.
2. **Slice B — `-name`/`-class`** by pre-scanning `argv` before
   `XtOpenDisplay`; Roadmap #6 calls this out as the last blocker before the
   command line is declared complete.
3. **Slice C — process options** in `pty.c`: `-ls`, `-tn`, `-tm`, `-ie`,
   `-hold`, `-wf`, `-mesg`, `-baudrate`, `-l`/`-lf`, `-lc`/`-lcc`.
4. **Slice D — startup application of existing menu modes**: `-rv`, `-aw`,
   `-rw`, `-132`, `-j`, `-mb`/`-nb`, `-fc`, `-sh`,
   `-nul`, `-cm`, `-into`, `-nomap`, `-maximized`/`-fullscreen`,
   `-samename`, `-ziconbeep`, `-ms`, `-pf`, `-fx`.
5. **Ride the roadmap**: selection options with #4; bold/wide/palette/cursor
   options with #5; `-ut` and `-sm` with #6 packaging.
6. **Upstream asks**: terminal-ID/DA option (`-ti`), C1-printable parser
   option (`-k8`), width-convention options. Not worth asking: Latin-1 mode
   and sixel.
7. **Drift-ledger entries**: legacy keyboard tables, Tek, toolbar, active icon,
   `-S`, `-C`, `-im`, `-cu`, `-s`, `-rca`, UTF-8-only.
