# xterm+

xterm+ is an X11 terminal emulator which preserves xterm's Xt/Athena user
interface and resource contract while using `libghostty-vt` as its modern
terminal core.

The project has two product bars: xterm defines the visible X11 compatibility
contract, while the capabilities demonstrated by Ghostling define a minimum
functional baseline for libghostty integration. xterm fidelity should shape
how a feature is exposed; it should not leave xterm+ less capable than a
minimal libghostty example. The current comparison and implementation order
are maintained in [`ROADMAP.md`](ROADMAP.md).

The project is intentionally early. The current libghostty build runs a real
PTY-backed shell, parses its output, renders the visible libghostty grid, sends
mode-aware keyboard input and terminal query responses back to the child, and
handles resize, bell, and title effects. It also establishes the `XTerm`/xterm
resource identity, a `VT100`/`vt100` terminal widget, the real Xaw main/VT/font
popup menus and xterm-compatible Shift+keypad font sizing.
Menu entries not implemented yet remain visible but insensitive so the
compatibility scope stays honest.

Intentional behavioral and architectural differences from the current xterm
patch-410 reference are maintained in [`DRIFT.md`](DRIFT.md).
The roles and update policy for local Ghostty, Ghostling, xterm snapshot, and
xterm.dev checkouts under ignored `upstream/` are documented in
[`UPSTREAM.md`](UPSTREAM.md). Maintainer working agreements and architecture
are summarized in [`HANDOFF.md`](HANDOFF.md).

The published site has three parts, all built by `.github/workflows/docs.yml`:
a landing page from [`www/`](www/), the xterm+ documentation from
[`docs/`](docs/) at `/docs/`, and the
[Terminal Developers Network](tdn/README.md) from [`tdn/`](tdn/) at `/tdn/`.
Start with [`docs/configuration/xresources.md`](docs/configuration/xresources.md)
if X resources are new to you. Preview all three together with
`just serve`, or one site with live reload via `just serve-docs` or
`just serve-tdn` (uses `uvx`; see the `justfile`).

## Relationship to xterm source

xterm+ does not compile, link, or embed xterm's terminal implementation. Its
PTY/event loop, Xt widget, X11 renderer, diagnostics, and libghostty adapter are
new code. `libghostty-vt` replaces xterm's parser and terminal-state engine.

xterm patch 410 remains both the behavioral oracle and a source reference.
The repository deliberately carries xterm-derived compatibility material:
`data/app-defaults/XTerm`, the resource/action/app-default catalogs under
`compat/`, menu and widget names, resource defaults, translation bindings, and
behavioral details reconstructed while consulting xterm's implementation.
Those portions are covered by the permissive xterm license in
[`LICENSES/xterm.txt`](LICENSES/xterm.txt). The fetched Ghostty checkout lives
under ignored `upstream/` and is built as a separate dependency under its own
license.

## Build the UI scaffold

```sh
meson setup build -Dlibghostty=disabled
meson compile -C build
meson test -C build
./build/xterm+
```

The stub backend is useful for UI work and does not start a PTY or parse
terminal data.

## Formatting

C sources and headers use the living conventions in [`STYLE.md`](STYLE.md)
and the checked-in `.clang-format`. Format and verify them with:

```sh
clang-format -i src/*.[ch] tests/*.c
clang-format --dry-run --Werror src/*.[ch] tests/*.c
```

Include sorting is deliberately disabled because Xt private headers have a
dependency-sensitive order. Keep include groups ordered manually.

## Build with libghostty-vt

The helper keeps Ghostty source under ignored `upstream/` and checks out the
exact revision tested by this project:

```sh
tools/fetch-libghostty
meson setup build-ghostty -Dlibghostty=enabled
meson compile -C build-ghostty
meson test -C build-ghostty
./build-ghostty/xterm+
```

Run a specific command using xterm's conventional trailing `-e` form:

```sh
./build-ghostty/xterm+ -e sh -lc 'printf "hello from xterm+\\n"; exec "$SHELL"'
```

Without `-e`, xterm+ starts `$SHELL`, falling back to `/bin/sh`, and advertises
`TERM=xterm-256color` to the child.

An existing checkout can be used without copying it:

```sh
meson setup build-ghostty \
  -Dlibghostty=enabled \
  -Dlibghostty-source=/path/to/ghostty
```

The build produces a private static `libghostty-vt` archive through Ghostty's
Zig build. Ghostty is not added to this Git repository.

Normal builds use Meson's `debugoptimized` build type by default. This retains
debug information while avoiding the severe terminal-throughput penalty of an
unoptimized `-O0` build. To update a build directory created before this became
the default:

```sh
meson configure build-ghostty -Dbuildtype=debugoptimized
```

## CPU flamegraphs

On Fedora, install the profiling tools if they are not already present:

```sh
sudo dnf install perf flamegraph
```

`tools/flamegraph` configures and compiles a separate `build-profile/` using
Meson's `debugoptimized` build type, disables link-time optimization, and keeps
C frame pointers. The pinned libghostty build also retains symbols and frame
pointers in this configuration. It then records xterm+ with `perf`'s DWARF
unwinder and writes the raw capture, collapsed stacks, a text report, and an
interactive SVG under ignored `profiles/`:

```sh
tools/flamegraph
```

The default workload runs `cat /tmp/lsasdf` ten times, repeating the long-output
case used while tuning progressive drawing so the capture has enough samples
to outweigh startup. Give a command after `--` to profile another case (or to
capture only one pass):

```sh
tools/flamegraph -- cat /tmp/lsasdf
tools/flamegraph -- sh -lc 'ls -ltR /usr; printf "done\\n"'
```

Open the reported `flamegraph.svg` in a browser. Wider frames consumed more
on-CPU samples; click a frame to zoom and use the SVG's search control to
highlight a function. `perf-report.txt` is useful when a flat function ranking
is clearer. These are CPU profiles, so time blocked on the PTY, X server, or
window manager does not appear as a wide frame.

To reuse an already compiled profile build, pass `--no-build`. The sampling
frequency, unwinder, build directory, and output parent can be overridden with
the `XTP_PROFILE_FREQUENCY`, `XTP_PROFILE_CALL_GRAPH`,
`XTP_PROFILE_BUILD_DIR`, and `XTP_PROFILE_OUTPUT_DIR` environment variables.
If `perf` reports a permissions error, check
`/proc/sys/kernel/perf_event_paranoid` and the local system policy for access to
performance counters.

## Current controls

- Ctrl+button 1: xterm main menu
- Ctrl+button 2: xterm VT menu
- Ctrl+button 3: xterm font menu
- Shift+Page Up: scroll back half a page
- Shift+Page Down: scroll forward half a page
- Shift+keypad plus: next larger configured xterm font
- Shift+keypad minus: next smaller configured xterm font
- Shift+Ctrl+keypad plus: next smaller configured xterm font, matching xterm

The main menu can toggle backarrow-key, NumLock-keypad, and Alt-escape modes.
The VT menu can toggle autowrap, reverse-wrap, automatic linefeed, application
cursor keys, and application keypad mode. Checkmarks are read back from the
terminal engine whenever a menu opens, so application-issued mode changes are
reflected in the UI.

Font changes retain the configured rows and columns, update cell dimensions,
resize the top-level window proportionally, and update its WM resize increments.
If the window manager cannot grant the requested pixel size, xterm+ accepts the
nearest size and derives the resulting rows and columns, matching xterm's Xt
geometry negotiation rather than repeatedly forcing the rejected size.
Window-manager resizes update both libghostty and the kernel PTY size; primary
screen contents therefore use libghostty's reflow behavior.

## Rendering boundary

libghostty supplies xterm+ with complete UTF-8 grapheme clusters, styles,
palette or true-color values, and cursor state through the backend-neutral
interface in `src/terminal.h`. The X11 compatibility renderer implements
colors, inverse, bold, underline, overline, and strikeout. It has two runtime
paths: Xlib bitmap fonts for pixel-accurate traditional xterm text, and
Xft/fontconfig for scalable TrueType text and UTF-8 glyphs present in the
selected primary face.

Cell damage, viewport movement, and cursor movement are independent rendering
signals. Cursor-only terminal updates therefore repaint the old and new cursor
cells even when libghostty reports no dirty grid rows.

PTY reads are fed to libghostty immediately, without newline splitting or a
frame timer. Full snapshots repaint cached rows in place without window copies
or a preceding whole-grid clear. X Expose damage is restored from the
last-painted cell cache rather than consuming terminal dirty state. The Xft
path caches resolved colors, uses a real bold face, and clips each ordered
background/glyph run to its terminal cells.

The bitmap path substitutes `?` for non-ASCII graphemes. The Xft path retains
and draws the UTF-8 bytes, but does not yet shape text, search fallback faces,
or render color emoji; unavailable glyphs therefore appear as the primary
font's missing-glyph box. This is a renderer limitation, not lost terminal
state: CJK, combining sequences, and emoji remain intact in libghostty and at
the renderer boundary.

## Consolidated configuration report

Run `-report-config` to resolve the same Xt resource database used at startup,
print the effective values in an `.Xresources`-shaped report, and exit without
opening a terminal or spawning a child:

```sh
./build/xterm+ -report-config
./build/xterm+ -fa 'Hack' -fs 12 -report-config
./build/xterm+ -report-config >xterm-plus.resources.txt
```

Terminal output colors command-line values, X resources, compiled defaults,
and unset resources differently. Redirected output is plain text, and
`NO_COLOR` disables color explicitly. Each setting includes a short explanation
and its origin.

This is also the compatibility ledger, not merely a short settings dashboard.
After the focused summary it prints every resource in patch 410's active
application, VT100, Tek4014, and VT-font subresource tables (330 active entries)
plus 17 resources hidden behind disabled patch-410 compile options, then all
130 active patterns from patch 410's `XTerm.ad`, and finally the merged
inherited resource lists for the Xt ApplicationShell and VT100 classes and the
Athena menu, scrollbar, and conditional toolbar components (including Form
constraint resources).
Every entry is marked supported, accepted but ignored, or unsupported. The
checked baseline is [`compat/xterm-410-resources.tsv`](compat/xterm-410-resources.tsv);
Meson compiles it into the executable so reporting does not depend on a nearby
xterm checkout.
The app-default instance/value baseline is
[`compat/xterm-410-app-defaults.txt`](compat/xterm-410-app-defaults.txt); it is
kept separate because widget paths such as `mainMenu*redraw*Label` are part of
the compatibility contract even though `Label` belongs to an Athena class.

The report begins by identifying every Xt input channel: the resolved `XTerm`
app-defaults file, server `RESOURCE_MANAGER`, `XENVIRONMENT`,
`XFILESEARCHPATH`, `XUSERFILESEARCHPATH`, `XAPPLRESDIR`, command-line resources,
and the compiled fallback set. Xt does not retain file-level provenance after
merging, so values from those layers are conservatively labeled `X resources`;
command-line values are identified separately and server presence is reported.

The `VT100.translations` value is printed in reusable Xt syntax and each action
it invokes is checked. The report also lists all 114 unique actions registered
by the patch-410 VT100 and Tek4014 widgets, using
[`compat/xterm-410-actions.txt`](compat/xterm-410-actions.txt) as its baseline.
For example, a traditional wheel override using `scroll-back(5,line)` and
`scroll-forw(5,line)` is resolved and reported as supported.

The font section explains xterm's ten font-menu choices: eight configured
slots (`font`, then `font1` through `font7`) and the two runtime-only Escape
Sequence and Selection slots. It reports the active renderer's cell
dimensions, actual size order, xterm-style derived `faceSize` values, and the
fontconfig match for `faceName`. With `renderFont: true`, xterm+ uses Xft and
fontconfig; `renderFont: false` retains the Xlib bitmap path. The right-button
font menu's `TrueType Fonts` entry switches between those renderers at runtime
and displays xterm's checkmark for the active Xft state. The equivalent Xt
action is `set-render-font(on)`, `set-render-font(off)`, or
`set-render-font(toggle)`.

## Diagnostic log

xterm+ writes diagnostics to standard error in this form:

```text
hh:mm:ss subsystem: message
```

The level is carried internally but does not add another field to the line.
On a terminal, the timestamp is bright cyan; debug, info, warning, and error
text is bright blue, green, yellow, and red respectively. Redirected stderr is
plain text, and setting `NO_COLOR` also disables ANSI color.

Debug logging defaults off, matching xterm's `+debug` command-line state. Use
`-debug` to enable it and `+debug` to disable it explicitly; the `debug` X
resource provides the same control. Info, warning, and error records are always
enabled. Keeping debug disabled also keeps synchronous per-key, PTY, and frame
diagnostics out of the interactive rendering path.

With debug enabled, startup logs include the complete command line, compiled
defaults, relevant entries from the server `RESOURCE_MANAGER` property (the
database managed by `xrdb`), merged Xt values after
command-line/app-default precedence, resolved VT widget values, and the
selected terminal backend. Runtime logs cover shell mapping and configuration,
PTY byte counts and lifecycle, terminal effects, key encoding, menu popup and
selection, font choices, scrollbar changes, resizes, redraw requests, and
rendered-frame summaries.

PTY contents are not copied into the log: byte counts, terminal effects, and
configuration changes are recorded without duplicating potentially sensitive
shell output. Redirect a session log when reproducing a visual problem:

```sh
./build-ghostty/xterm+ -debug 2>xterm-plus.log
```

Meson also builds the non-installed `xtp-send-font-keys` X11 regression
utility. Give it the top-level window ID from the `shell: realized` log to send
two distinct Shift+KP_Add presses. Pass `-` to exercise Shift+KP_Subtract,
`insert` to exercise PRIMARY paste, `page-up` or `page-down` to exercise
scrollback, and optionally pass an explicit count:

```sh
./build-ghostty/xtp-send-font-keys 0x4e00027
./build-ghostty/xtp-send-font-keys 0x4e00027 + 4
./build-ghostty/xtp-send-font-keys 0x4e00027 - 4
./build-ghostty/xtp-send-font-keys 0x4e00027 insert 1
./build-ghostty/xtp-send-font-keys 0x4e00027 page-up 1
```

The log should contain one `action larger-vt-font` and one `font: select` for
each requested press. Replayed copies of the same X event are diagnosed once
and ignored for every local key action, including paste and paging.

The companion `xtp-send-wheel` utility exercises scrollback without relying on
a physical pointing device. Pass the top-level window ID, a direction, and an
optional tick count:

```sh
./build-ghostty/xtp-send-wheel 0x4e00027 up 4
./build-ghostty/xtp-send-wheel 0x4e00027 down 4
```

With `-debug`, each tick reports the requested five-line delta and Ghostty's
resulting `{offset, length, total}` viewport state.

`xtp-resize-loop` repeatedly resizes the top-level window to exercise reflow
and expose handling. It defaults to four `400x300` to `1000x700` cycles with a
100 ms delay; the cycle count, delay, and both sizes are configurable:

```sh
./build-ghostty/xtp-resize-loop 0x4e00027
./build-ghostty/xtp-resize-loop 0x4e00027 10 20 320 240 1200 800
./build-ghostty/xtp-resize-loop 0x4e00027 --grid 38 80 24 250
```

For the deterministic wrapped-prompt case, run `just reflow-prompt`, copy the
window ID from its debug output, then run `just reflow-resize WINDOW-ID` in a
second terminal. This crosses a fixed 45-column Bash prompt from 80 to 38 and
back to 80 columns once; hand-resizing and repeated cycles are unnecessary.
The same logical failure reproduces in Ghostty 1.3.1's X11 backend with shell
integration disabled and is tracked upstream in Ghostty
[discussion #14026](https://github.com/ghostty-org/ghostty/discussions/14026).

## X resources

xterm+ deliberately uses application class `XTerm`, instance `xterm`, and
terminal widget `vt100` of class `VT100`. Existing class- and instance-based
xterm resources can therefore apply directly. The `-name` compatibility option
will be added before the command-line surface is declared complete.

Saved-history navigation uses libghostty's viewport and history state.
`saveLines`/`-sl` set its line limit; `scrollBar`, `-sb`, and `+sb` control the
real Athena scrollbar; and `rightScrollBar`, `-rightbar`, and `-leftbar` select
its side. The wheel scrolls five rows per tick when the child has not enabled
mouse tracking, and the classic Xaw thumb supports both dragging and xterm's
button-based scrolling. Shift+Page Up and Shift+Page Down use xterm's default
half-page navigation. The complete patch-410 gesture inventory is in the
[default VT bindings audit](docs/compatibility/default-bindings.md).
`scrollbar.width` and `scrollbar.thickness` remain ordinary Athena child
resources. Attempts to scroll past either history boundary do not schedule
redundant terminal renders, and wheel bursts within one display frame are
coalesced to their final viewport.

`scrollKey` (default false) controls whether an encoded keypress returns the
viewport to the active screen; use `-sk` to enable it and `+sk` to disable it.
`scrollTtyOutput` (default true) controls whether new PTY output returns the
viewport to the active screen; `-si` inhibits that behavior and `+si` restores
it. Both policies can also be toggled from the VT Options menu.

When a child enables terminal mouse tracking, xterm+ sends button press,
release, drag, hover, modifier, and wheel events in the requested X10, UTF-8,
SGR, URxvt, or SGR-pixel format. Hold Shift to override application tracking
for local selection or scrollback. Ctrl+button 1, 2, and 3 remain reserved for
the xterm popup menus.

When a child enables DEC private mode 1004, real X focus transitions produce
the standard `CSI I` focus-in and `CSI O` focus-out reports. No focus sequences
are sent unless the child requests them. Run `tdn/tools/testfocus`, focus
another window, and return to verify both reports and their ordering.

Mouse selection uses libghostty's history-safe selection gestures. Button 1
selects cells, words, and lines by single, double, and triple click; dragging
works in the visible scrollback viewport, and Button 3 extends the nearer end
of an existing selection using that selection's cell, word, or line unit.
Crossing the opposite endpoint switches the extension side. Dragging either a
new selection or a Button-3 extension beyond the top or bottom edge scrolls
through saved history while keeping the stationary endpoint attached to its
original terminal row. Selected cells are highlighted and exported as X11
`PRIMARY`; Button 2 pastes `PRIMARY` through Ghostty's control-byte filtering
and bracketed-paste encoder. Shift+Insert uses the same paste path.
`multiClickTime` defaults to 250 milliseconds and can be set with `-mc
milliseconds`; as in xterm, it measures from the previous button release to
the next press.
Word selection uses xterm's character classes rather than a whitespace-only
split: punctuation is separate by default, and `charClass` or `-cc` accepts
xterm's `low[-high][:class]` override syntax. Written spaces remain selectable
characters, while the undrawn suffix after a row's last written cell is a
separate past-end region: double-click does not select it, character/word
extension consumes it as a whole, and line selection works on untouched rows.

New windows use normal Xt/window-manager map-time activation, matching xterm.
Applications can select block, underline, and bar cursors with DECSCUSR. The
blinking DECSCUSR variants and DEC private mode 12 drive the cursor blink timer;
`cursorOnTime` and `cursorOffTime` default to xterm's 600 and 300 milliseconds.
`cursorBlink` is `false` by default, so the initial cursor and DECSCUSR 0 are
steady while explicit application blink requests are honored. Set it to `true`
for a blinking default, or to `always`/`never` to ignore application blink
requests. `-bc` and `+bc` select the `true` and `false` policies; `-bcn` and
`-bcf` set the on/off times. A focused block is filled and an unfocused block
is an outline; `cursorColor`, `alwaysHighlight`, `-ah`, and `+ah` follow the
corresponding xterm interfaces. See the
[TDN cursor-controls reference](tdn/docs/csi/cursor.md) for protocol details.

The `tdn/tools/sendcsi` capability probe makes cursor behavior easy to
exercise without remembering the byte sequences:

```sh
tdn/tools/sendcsi list
tdn/tools/sendcsi steady-block
tdn/tools/sendcsi blink-bar show
tdn/tools/sendcsi default
```

Like upstream xterm, xterm+ creates its Athena popup menus under the
`menuLocale` resource, whose default is `C`, then restores the process locale.
This permits xterm's bitmap menu-font resources to work on UTF-8 desktops
without a misleading `Missing charsets` warning.

The repository carries xterm's app-defaults as a reference, but does not
install it yet: overwriting `/usr/share/X11/app-defaults/XTerm` would conflict
with an installed upstream xterm package. When upstream xterm is installed,
Xt naturally loads that existing app-defaults file for xterm+.
