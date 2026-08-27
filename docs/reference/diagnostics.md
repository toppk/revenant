# Diagnostics

xterm+ ships three tools for answering "what is it actually doing":
the configuration report, the structured log, and the CPU profiling helper.

## `-report-config`

```sh
xterm+ -report-config
xterm+ -fa Hack -fs 12 -report-config
xterm+ -report-config > xterm-plus.resources.txt
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
- all 330 resources in xterm patch 410's application, VT100, Tek4014, and
  VT-font tables, plus 17 behind disabled compile options, each marked
  *supported*, *accepted but ignored*, or *unsupported*;
- all 130 patterns from patch 410's `XTerm.ad`, so widget paths such as
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

Info, warning, and error records are always on. Debug records are off by
default (xterm's `+debug` state) and enabled with `-debug` or the `debug`
resource. On a terminal the timestamp is cyan and the message coloured by
severity; redirected output is plain, and `NO_COLOR` is honoured.

With debug on, startup logs the full command line, compiled defaults, the
relevant `RESOURCE_MANAGER` entries, merged values after precedence, and the
selected backend. Runtime logs cover the PTY lifecycle and byte previews,
terminal effects, key encoding, menus, font choices, scrollbar changes,
resizes, and rendered-frame summaries. Each PTY read previews at most 256
input bytes. Control and non-ASCII bytes use backslash escapes (`\\e` for
Escape and `\\xNN` otherwise), and a truncated preview reports the number of
omitted bytes explicitly.

PTY previews can contain application data. Review or redact a debug log before
sharing it if the terminal displayed sensitive output.

Keeping debug off also keeps synchronous per-key and per-frame logging out
of the rendering path, so leave it off unless you are reproducing something:

```sh
xterm+ -debug 2> xterm-plus.log
```

## Regression helpers

Four small X11 utilities are built alongside xterm+ but not installed. Give
them the top-level window ID reported by the `shell: realized` log line:

```sh
./build-ghostty/xtp-send-font-keys 0x4e00027        # two Shift+KP_Add presses
./build-ghostty/xtp-send-font-keys 0x4e00027 - 4    # four Shift+KP_Subtract
./build-ghostty/xtp-send-font-keys 0x4e00027 insert 1
./build-ghostty/xtp-send-font-keys 0x4e00027 page-up 1
./build-ghostty/xtp-send-wheel 0x4e00027 up 4       # four wheel ticks
./build-ghostty/xtp-send-selection 0x4e00027 10 10 200 60
./build-ghostty/xtp-resize-loop 0x4e00027            # four narrow/wide cycles
```

Each press should produce one `action larger-vt-font` and one `font: select`
record; each wheel tick reports the requested delta and the resulting
viewport `{offset, length, total}`.

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
`PS1` from a plain prompt to one containing OSC 133 marks. Controlled xterm+
tests now show that a plain long Bash prompt recovers correctly, while both
OSC-marked and ordinary SGR-styled prompts can finish with the cursor inside
the visible prompt. An equivalent native-Wayland manual run did not reproduce,
but the upstream terminal-independent PTY fixture needs only one
`TIOCSWINSZ`; recheck whether that manual prompt actually wrapped before it
was widened.

See the complete
[Readline 8.3 wrapped-prompt diagnosis](bash-readline-resize.md) for the
terminal/PTY background, exact controls, cursor-offset arithmetic, upstream
fix, and local-build validation. Do not add a libghostty or xterm+ workaround.

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
