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
selected backend. Runtime logs cover the PTY lifecycle and byte counts,
terminal effects, key encoding, menus, font choices, scrollbar changes,
resizes, and rendered-frame summaries. PTY *contents* are never logged.

Keeping debug off also keeps synchronous per-key and per-frame logging out
of the rendering path, so leave it off unless you are reproducing something:

```sh
xterm+ -debug 2> xterm-plus.log
```

## Regression helpers

Three small X11 utilities are built alongside xterm+ but not installed. Give
them the top-level window ID reported by the `shell: realized` log line:

```sh
./build-ghostty/xtp-send-font-keys 0x4e00027        # two Shift+KP_Add presses
./build-ghostty/xtp-send-font-keys 0x4e00027 - 4    # four Shift+KP_Subtract
./build-ghostty/xtp-send-font-keys 0x4e00027 insert 1
./build-ghostty/xtp-send-font-keys 0x4e00027 page-up 1
./build-ghostty/xtp-send-wheel 0x4e00027 up 4       # four wheel ticks
./build-ghostty/xtp-send-selection 0x4e00027 10 10 200 60
```

Each press should produce one `action larger-vt-font` and one `font: select`
record; each wheel tick reports the requested delta and the resulting
viewport `{offset, length, total}`.

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
