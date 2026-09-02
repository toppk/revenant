---
man: revenant-diagnostics
section: 7
manual: revenant
description: the configuration report, the log, and profiling
---

# Diagnostics

Revenant ships three tools for answering "what is it actually doing":
the configuration report, the structured log, and the CPU profiling helper.

## `-report-config`

```sh
revenant -report-config
revenant -fa Hack -fs 12 -report-config
revenant -report-config > revenant.resources.txt
```

The report resolves the same Xt resource database startup would use, prints
the effective value of every setting in `.Xresources` syntax, and exits
without opening a window or starting a shell. On a terminal, values are
coloured by origin — command line, X resources, compiled default, unset —
and each is annotated with a one-line explanation and its support status.
Redirected output is plain text; `NO_COLOR` also disables colour.

It is deliberately exhaustive. After the focused summary it prints:

- every Xt input channel it consulted: the resolved `XTerm` app-defaults
  file, the server `RESOURCE_MANAGER` property, `XENVIRONMENT`,
  `XFILESEARCHPATH`, `XUSERFILESEARCHPATH`, `XAPPLRESDIR`, command-line
  resources, and the compiled fallbacks;
- all 331 resources in xterm patch 411's application, VT100, Tek4014, and
  VT-font tables, plus 17 behind disabled compile options, each marked
  *supported*, *accepted but ignored*, or *unsupported*;
- all 131 patterns from patch 411's `XTerm.ad`, so widget paths such as
  `mainMenu*redraw*Label` are checked too;
- inherited resources of the Xt shell, the VT100 widget, and the Athena
  menu, scrollbar, and toolbar components;
- your `VT100.translations`, re-printed in reusable syntax, with each action
  checked against the 114 actions xterm registers;
- the font section: the ten font-menu slots, actual size order, derived
  `faceSize` values, and the fontconfig match for `faceName`.

Xt does not retain which *file* a merged value came from, so values from
app-defaults and `xrdb` are labelled `X resources` together; command-line
values are identified separately.

## The log

Diagnostics go to standard error as

```text
hh:mm:ss subsystem: message
```

The default threshold is `warning`, so a healthy ordinary launch writes
nothing. Choose `debug`, `info`, `warning`, or `error` with `-log LEVEL` or the
`logLevel` X resource; the chosen level and everything more severe are
enabled. The xterm-compatible `-debug` and `+debug` options remain aliases for
`-log debug` and `-log warning`. The legacy Boolean `debug` resource is used
only when `logLevel` is unset. On a terminal the timestamp is cyan and the
message coloured by severity; redirected output is plain, and `NO_COLOR` is
honoured.

With debug on, startup logs the full command line, compiled defaults, the
relevant `RESOURCE_MANAGER` entries, merged values after precedence, and the
selected backend. Runtime logs cover the PTY lifecycle and byte previews,
terminal effects, key encoding, menus, font choices, scrollbar changes,
resizes, and rendered-frame summaries. Each PTY read previews at most 256
input bytes. Control and non-ASCII bytes use backslash escapes (`\\e` for
Escape and `\\xNN` otherwise), and a truncated preview reports the number of
omitted bytes explicitly.

`FR-STYLEFAMILY` means an explicit Xft `boldFont` or `wideBoldFont` entry
resolved outside the already-selected role family. Revenant keeps the family
decision stable and renders that atom with the role's normal instance; the
warning names the slot and both families.

PTY previews can contain application data. Review or redact a debug log before
sharing it if the terminal displayed sensitive output.

The command-line level is applied before the display is opened; an X resource
takes effect once Xt has resolved the display's resource database. Keeping the
default warning threshold also keeps synchronous per-key and per-frame logging
out of the rendering path, so enable debug only while reproducing something:

```sh
revenant -log debug 2> revenant.log
```

## Font-routing snapshots

The font-routing report answers which configured or automatic font actually
served each distinct terminal atom. Collection is disabled by default. Enable
it, bind the snapshot action, and keep standard error separate from the PTY:

```sh
revenant -report-font-routing \
  -xrm 'XTerm*vt100.translations: #override <Key>F12: report-font-routing()' \
  2>font-routing.ndjson
```

After the relevant text has appeared, press F12. Each output line is an
independent JSON object with `"schema": 1`. The snapshot contains:

- `load` records for the configured slot chain entries and their effective
  file, collection index, and variation coordinates; `fontslot` distinguishes
  the eight Xft font-menu sizes. These describe the latest universe-build
  attempt, including retained old roles after a failed reload;
- `warn` records with stable codes such as `FR-BADPATTERN`, `FR-DUPROLE`,
  `FR-STYLEFAMILY`, and `FR-UVSMISS`;
- bounded, first-use `route` records showing the atom, committed width class,
  capturing semantic slot, active font-menu slot, winning rung, role identity,
  routing misses, and any style-to-normal degradation;
- `FR-REPORTBOUND` if more than 4096 distinct keys were observed;
- `FR-LOADBOUND` only if allocation prevented retaining every load record from
  the latest build;
- one final `snapshot` record with the generation, effective DPI, collection
  state, and route-record count.

`rung` is `entry1`, `entry2`, a literal numbered name such as
`fallbackFace7`, `system`, or `tofu`. Tofu records keep `file`, `index`, and
`coords` present with null values. Routing miss codes are `cmap`, `uvs`,
`shape`, `ink`, `budget`, and `truncated`; style fallback is a separate
informational object because it never changes the routed family.
If a route exceeds its 64 recorded misses, it carries
`"missesTruncated": true`; routing itself continues normally.

The report is emitted only by the explicit action. There is no guaranteed
exit-time dump. Invoking the action while collection is off writes exactly one
disabled `snapshot` record, making a missing `-report-font-routing` visible.
Collection is intended for diagnosis rather than permanent use: it retains up
to 4096 routing records and makes the normally batched one-byte path observable
one atom at a time.

## Inspecting one font with HarfBuzz

When a sequence looks surprising, inspect the font independently of terminal
routing, cell sizing, and clipping. `hb-shape` reports the glyph IDs, clusters,
and positions selected by HarfBuzz; `hb-view` shapes the same text and writes
the font's rasterized result to a PNG:

```sh
hb-shape font-fixtures-stage/fonts/NotoColorEmoji.ttf '👨‍👩‍👧‍👦'
hb-view --font-file=font-fixtures-stage/fonts/NotoColorEmoji.ttf \
  --output-file=family-current.png '👨‍👩‍👧‍👦'
hb-view --font-file=font-fixtures-stage/fonts/NotoColorEmoji-2.034.ttf \
  --output-file=family-2.034.png '👨‍👩‍👧‍👦'
```

This is the quickest way to distinguish font artwork from a Revenant shaping
or rendering bug. If the standalone PNG has the same design, the terminal is
faithfully painting that font. If its glyph selection or image differs, inspect
Revenant's `font: route` debug record and clipping next. The historical 2.034
fixture is useful here because it has the earlier colorful family artwork,
while the current pinned Noto release has the achromatic family design
introduced by [Google's Emoji 15.1 update](https://blog.emojipedia.org/googles-emoji-15-1-support-in-noto-color-emoji/).

At debug level, `font: route-cache miss` identifies the first family decision
for a key and `font: route-cache hit` identifies reuse. These records include
the base codepoint, active font-menu slot, committed width, and resolved role.
They are performance diagnostics, not the specified NDJSON routing report, and
their prose is not a stable machine-readable interface.

Use `tools/font-fixture-info.py FONT...` alongside these commands to inspect
format tables, strikes, coverage, and per-probe ink paths. `hb-view` is a
diagnostic, not an acceptance oracle: terminal tests must still assert committed
cell width, clipping, fallback, and cursor behavior under Xvfb.

## Regression helpers

### Patch-411 font-name deposition

<!-- markdownlint-disable MD013 -->

[`tools/t0-facename-oracle.py`](https://github.com/toppk/revenant/blob/master/tools/t0-facename-oracle.py) is a
deposition harness, not a Revenant test. It starts stock patch-411 xterm in a
sterile resource session and isolated fontconfig universe, asks the maintained
face-list, style, slot-order, and governor questions, and records xterm's
answers. A separate Revenant conformance test consumes the reviewed fixture.

The script has a PEP 723 header, so use `uv run` rather than manually managing
`fonttools` and `wcwidth`. The complete record/check procedure and command are
in [`compat/README.md`](https://github.com/toppk/revenant/blob/master/compat/README.md).
`--record` writes only under
`/tmp`; it never overwrites the blessed fixture.

<!-- markdownlint-enable MD013 -->

### Live xterm font compatibility

Use the optional live comparison when font loading, point-size handling, or
font-menu sizing changes:

```sh
just xterm-font-compat
just xterm-font-compat build-agent-gcc
```

It opens disposable xterm and Revenant windows on the current `$DISPLAY`, so
it works directly inside an existing VNC desktop. The helper supplies
`tools/xterm-font-compat.Xresources` to both programs with command-line `-xrm`
entries; it neither reads a dotfile nor changes the server's resource database.
The profile deliberately combines `faceName: DejaVu Sans Mono:size=11` with a
conflicting `faceSize: 16`, then checks exact window size, requested size,
resize increments, and base size at the default, largest, and smallest
font-menu positions.

This is an external compatibility oracle, not part of `just test`, Meson, or
normal CI. It depends on the installed xterm, the live X server and window
manager, `wmctrl`, and DejaVu Sans Mono. Missing dependencies produce a focused
error; a mismatch leaves both program logs in a named temporary directory.

Five small X11 utilities are built alongside Revenant but not installed. Give
them the top-level window ID reported by the `shell: realized` log line:

```sh
./build-ghostty/xtp-send-font-keys 0x4e00027        # two Shift+KP_Add presses
./build-ghostty/xtp-send-font-keys 0x4e00027 - 4    # four Shift+KP_Subtract
./build-ghostty/xtp-send-font-keys 0x4e00027 insert 1
./build-ghostty/xtp-send-font-keys 0x4e00027 page-up 1
./build-ghostty/xtp-send-wheel 0x4e00027 up 4       # four wheel ticks
./build-ghostty/xtp-send-selection 0x4e00027 10 10 200 60
./build-ghostty/xtp-send-shift-click 0x4e00027 15 15
./build-ghostty/xtp-resize-loop 0x4e00027            # four narrow/wide cycles
```

Each press should produce one `action larger-vt-font` and one `font: select`
record; each wheel tick reports the requested delta and the resulting
viewport `{offset, length, total}`.

The Shift-click helper sends a Shift-modified motion, Button-1 press, and
Button-1 release at one pixel coordinate. It is useful for exercising OSC 8
hover and activation policy without a physical pointer.

The resize helper defaults to four `400x300` to `1000x700` cycles separated by
100 ms. Override the cycle count and delay, or provide all four dimensions:

```sh
./build-ghostty/xtp-resize-loop WINDOW-ID CYCLES DELAY-MS
./build-ghostty/xtp-resize-loop WINDOW-ID 10 20 320 240 1200 800
```

It can also resize in terminal cells. It reads the window's base size and cell
increments from `WM_NORMAL_HINTS`, so this form is independent of the selected
font and scrollbar width:

```sh
./build-ghostty/xtp-resize-loop WINDOW-ID --grid 38 80 24 250
```

### Readline 8.3 wrapped-prompt reproducer

The known Readline 8.3 regression needs only one resize across a prompt's wrap
boundary; repeated dragging is not required. In one terminal, start the fixed
45-column OSC 133-marked prompt:

```sh
just reflow-prompt
```

Copy the top-level window ID from the `shell: realized window=...` debug line.
In another terminal, perform one 80-to-38-to-80-column cycle:

```sh
just reflow-resize 0x4e00027
```

Before the resize the cursor is at column 45. Released Readline 8.3 leaves it
at column 37 (over the `u` in `plus`) after the window returns to 80 columns:
it has dropped the final eight-byte invisible OSC run from its cursor
calculation. Bash development commit `1e9f5e10b2` should restore column 45.
The fixture supplies its own Bash startup file and therefore does not depend
on the user's Bash files.

An early Ghostty 1.3.1 X11 reproducer disabled Ghostty's automatic shell
integration but used `--noprofile` without `--norc`:

```sh
PS1="someitnh rellayl logn so you can see" \
  GDK_BACKEND=x11 ghostty --shell-integration=none -e bash --noprofile
```

That observation is tracked as Ghostty
[discussion #14026](https://github.com/ghostty-org/ghostty/discussions/14026).
The user's `.bashrc` still loaded in that command and changed the exported
`PS1` from a plain prompt to one containing OSC 133 marks. Controlled Revenant
tests now show that a plain long Bash prompt recovers correctly, while both
OSC-marked and ordinary SGR-styled prompts can finish with the cursor inside
the visible prompt. An equivalent native-Wayland manual run did not reproduce,
but the upstream terminal-independent PTY fixture needs only one
`TIOCSWINSZ`; recheck whether that manual prompt actually wrapped before it
was widened.

See the complete
[Readline 8.3 wrapped-prompt diagnosis](bash-readline-resize.md) for the
terminal/PTY background, exact controls, cursor-offset arithmetic, upstream
fix, and local-build validation. Do not add a libghostty or Revenant workaround.

## CPU flamegraphs

```sh
sudo dnf install perf flamegraph      # Fedora
tools/flamegraph                      # default long-output workload
tools/flamegraph -- cat /tmp/big.txt  # your own workload
tools/flamegraph --no-build           # reuse the compiled profile build
```

`tools/flamegraph` builds a separate `build-profile/` (debugoptimized, no
LTO, frame pointers kept, matching symbols in libghostty), records with
`perf`'s DWARF unwinder, and writes the raw capture, collapsed stacks, a text
report, and `flamegraph.svg` under the ignored `profiles/` directory. Wider
frames consumed more CPU; these are on-CPU profiles, so time blocked on the
PTY or X server does not appear.

Sampling frequency, unwinder, build directory, and output location are
overridable with `XTP_PROFILE_FREQUENCY`, `XTP_PROFILE_CALL_GRAPH`,
`XTP_PROFILE_BUILD_DIR`, and `XTP_PROFILE_OUTPUT_DIR`. A permissions error
from `perf` usually means `/proc/sys/kernel/perf_event_paranoid` is too
strict.
