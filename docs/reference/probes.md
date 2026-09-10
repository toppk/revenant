---
man: revenant-probes
section: 7
manual: revenant
description: interactive verification programs
---

# Interactive terminal probes

Human-run visual and interactive verification programs use the naming pattern
`tools/probe-<feature>.<ext>`. This keeps them discoverable separately from
build utilities and automated `tests/xtp-*` or `tests/xvfb-*` helpers.

Run the same probe inside Revenant, xterm, and Ghostty when comparing behavior.
Record the emulator version, font configuration, locale, and exact invocation
with any result.

| Probe | Purpose |
| --- | --- |
| `tools/probe-color.sh` | SGR attributes, underline styles, 16/256 colors, and semicolon/colon truecolor ramps. |
| `tools/probe-colors.py` | OSC 4 palette queries and resets, named palettes, and an `-xrm` spawn harness alongside expanded 16/256/truecolor displays. |
| `tools/probe-reverse-video.sh` | Enter-gated comparison of SGR 7, DECSCNM repaint/restoration, explicit backgrounds, and widget reverse video. |
| `tools/probe-osc8.sh` | Explicit OSC 8 labels and a detected plain URL for Shift-hover and safe HTTP(S)-only Shift-click activation. |
| `tools/probe-emoji.py` | CPR-measured legacy-versus-mode-2027 widths for emoji presentation, modifiers, ZWJ sequences, flags, cluster boundaries, right-margin wrapping, and over-capacity clusters. |
| `tools/probe-fonts.py` | CPR-measured widths plus visual diagnostics for combining marks, conjuncts, enclosing and spanning marks, and Zalgo-style stacking. |
| `tools/probe-keymodes.py` | Cooked input, raw bytes, fixterms drift, Kitty flags, associated text, event types, and flag-stack restoration. |
| `tools/probe-sync.py` | Slow redraw comparison with DEC mode 2026 off/on, with an optional mid-frame hold for timeout and resize checks. |
| `tools/probe-clipboard.py` | OSC 52 selection set, clear, invalid-payload, and query with decoded replies, for comparing `allowWindowOps` policy across emulators. |
| `tools/probe-titles.py` | Title/icon reports (XTWINOPS 20/21), nested push/pop (22/23), and visible title restoration. |
| `tools/probe-dynamic-colors.py` | OSC 10/11/12 sets and queries, OSC 110/111/112 resets, comparison of reported RGB values with visible default colors, and the `CSI ? 996 n` light/dark query. |
| `tools/probe-features.py` | Named dispatch fixtures for remaining features, plus live Mouse/Tcap Ops checks; use `--list`. |

Examples:

```sh
tools/probe-color.sh
python3 tools/probe-colors.py --query
python3 tools/probe-colors.py --spawn --query --program ./build/revenant
tools/probe-reverse-video.sh
tools/probe-osc8.sh
python3 tools/probe-emoji.py --no-pause
python3 tools/probe-emoji.py --regime legacy
python3 tools/probe-emoji.py --regime cluster
python3 tools/probe-fonts.py --no-pause
python3 tools/probe-keymodes.py --kitty-only
python3 tools/probe-sync.py
python3 tools/probe-sync.py --mode off
python3 tools/probe-sync.py --mode on
python3 tools/probe-sync.py --mode on --frames 3 --hold-ms 1500
python3 tools/probe-clipboard.py
python3 tools/probe-clipboard.py --target p --set "from OSC 52"
python3 tools/probe-clipboard.py --query --st
python3 tools/probe-titles.py
python3 tools/probe-titles.py --query
python3 tools/probe-titles.py --target title --delay 2
python3 tools/probe-dynamic-colors.py
python3 tools/probe-dynamic-colors.py --query
python3 tools/probe-dynamic-colors.py --scheme
python3 tools/probe-dynamic-colors.py --background '#142850' --foreground '#ffe080'
python3 tools/probe-dynamic-colors.py --reset all
```

The equivalent `just` recipes are `probe-color`, `probe-colors`, `probe-reverse-video`,
`probe-osc8`, `probe-emoji`, `probe-fonts`, `probe-keymodes`, `probe-sync`,
`probe-clipboard`, `probe-titles`, `probe-dynamic-colors`, and `probe-features`.

The synchronized-output probe draws alternating colored frames one row at a
time. By default it runs eight frames with synchronization off, then eight
with it on. Off should show a moving boundary between old and new rows; on
should show complete frames swapping together. The label reports the requested
mode, not detected support: terminals that ignore mode 2026 may show the sweep
in both phases. Run directly in the terminal when comparing emulators;
multiplexers can affect the result.

`--frame-ms` controls the total row-drawing delay (default 400 ms), and
`--pause-ms` controls the pause between frames (default 150 ms). Keep
`--frame-ms` plus `--hold-ms` comfortably below Revenant's one-second timeout
for the normal comparison. With `--mode on --hold-ms 1500`, the probe pauses
halfway through each frame: the timeout should reveal a partial frame before
the remaining rows arrive. Resize during a hold to observe the geometry
repaint exception; the next frame adapts to the new size. Ctrl+C exits, resets
mode 2026, restores cursor visibility, and leaves the alternate screen.

For a sequence that differs between terminals, render it directly from the
suspected font with `hb-shape` and `hb-view`; the
[diagnostics guide](diagnostics.md#inspecting-one-font-with-harfbuzz) explains
how this separates font artwork and shaping from terminal routing and clipping.

The reverse-video probe pauses before each state change so the original cells
can be inspected after DECSCNM repaints them. Run it once normally and once in
a terminal started with `revenant -rv` when comparing the startup resource;
use `tools/probe-reverse-video.sh --no-pause` only for a quick replay.

The probes deliberately do not decide pass or fail from screenshots or visual
output. Stable machine assertions belong in the automated test suite. The
keyboard probe is the exception for protocol parsing: its noninteractive
`--self-test` validates the decoder used to explain captured bytes.

The emoji and font probes share the same protocol-aware measurement engine. On
a tty they use cursor-position reports to measure advance mechanically, first
request mode 2027 reset and then set, query DECRQM once after each request, and
grade against the contract the terminal reports as active. A terminal that
does not recognize 2027 remains a correct legacy terminal when it preserves
legacy widths; lack of cluster support is reported as a capability gap. Both
probes report the startup state and restore it when DECRQM recognizes the mode,
because terminals may configure either initial state. Appearance and font
artwork are displayed but never graded.

The emoji probe's legacy accept sets use contemporary glibc-style,
per-codepoint `wcwidth` arithmetic. Regional-indicator cases explicitly accept
the common table variants. A normal sample that crosses a row boundary is
reported as `wrapped`, rather than producing a misleading negative width; the
dedicated right-margin case separately verifies that a two-cell cluster wraps
atomically.

Machine acceptance for keyboard delivery lives separately in
`tests/xvfb-keyboard.sh` and `tests/xvfb-kitty-keyboard.sh`. The latter checks
exact press, repeat, release, alternate-key, associated-text, and modifier
bytes through a real X server and PTY; it is not a replacement for testing a
human keyboard layout or input method with `probe-keymodes.py`.

The clipboard probe writes, clears, or queries one OSC 52 target. Revenant and
xterm refuse both directions by default. Enable **Allow Window Ops** in the
Ctrl+right-click menu, or start the terminal with
`-xrm 'XTerm*allowWindowOps: true'`, to compare the permitted behavior; a query
that receives no reply within the timeout reports the denial. `--invalid`
sends a payload that is not base64 to show whether the emulator clears the
selection, as xterm does, or ignores the request.

The title probe runs an Enter-paced **original → A → B → A → original**
demo, using two nested pushes and matching pops. It requests distinct title
and icon labels and prints decoded reports at each stage; `--target title`
or `--target icon` isolates one label. `--query` only reads current labels.
`--delay 2` displays each stage for two seconds after its queries instead of
waiting for Enter. Ctrl+C requests any outstanding pops and restores terminal
input settings. Restoring the original labels depends on the terminal
supporting and permitting the stack operations; requests are not proof of
support. Shell prompt hooks may replace the title when the probe exits.

In xterm, compare `--query` with **Allow Window Ops** unchecked and checked:
the default deny list blocks `GetIconTitle` and `GetWinTitle`, while permitting
`PushTitle` and `PopTitle`. OSC 0/1/2 title setting uses the separate
`allowTitleOps` permission, controlled by **Allow Title Ops** in the same
Ctrl+right-click menu. Disable it before starting the demo to confirm that
title changes and restoration are blocked while permitted reports still work.
Older Revenant builds that apply Window Ops only
to clipboard access do not enable title reports when the toggle is checked.
The probe distinguishes an empty reported title from no reply, so it can
compare those builds with implementations of title reporting.
Keep permissions unchanged during a stack demo, then
rerun to compare configurations. Run directly outside tmux/screen to observe
the emulator's own behavior.

The dynamic-color probe queries the current foreground, background, and cursor
colors, then demonstrates yellow text on a dark blue background with a pink
cursor. Press Enter through each stage, or use `--delay 2`. It resets the
three colors individually to their configured defaults. An explicit RGB sample
provides a reference that should stay unchanged while default-colored text,
including text already printed, changes. Query replies alone do not prove that
the window repainted correctly.

Normal exit and Ctrl+C request restoration of the original queried colors.
This restores RGB values by installing overrides; it cannot recover whether an
original value was itself an override. If a query was refused or unsupported,
cleanup requests the configured default for that target instead. Keep color
permissions unchanged through cleanup, then rerun with another policy.
`--foreground`, `--background`, and `--cursor` send persistent set requests;
`--reset foreground|background|cursor|all` sends persistent reset requests.
`--query` changes nothing and distinguishes a decoded RGB reply from silence.
`--bel` tests BEL termination instead of the default ST.

Use **Ctrl+right-click → Allow Color Ops** to compare the dynamic-color probe
with permission checked and unchecked. It defaults to checked; no `-xrm`
option is needed to change it. Start with
`python3 tools/probe-dynamic-colors.py --query`, then run without arguments
for the Enter-paced visual demo.
Keep the permission unchanged during a demo so cleanup can restore colors;
toggle it between runs. With permission off, queries should time out and
sets/resets should leave dynamic colors unchanged. The current menu gates
OSC 10–19 and 110–119; palette-query permissions and `disallowedColorOps`
exceptions remain open. Changed RGB replies with unchanged painted colors
expose the pending runtime-color rendering work.

## Dispatch fixtures

`tools/probe-features.py` supplies one named fixture per remaining work area.
Most of those features are still pending; a sent request or an unanswered
query is not evidence of implementation. Start with:

```sh
python3 tools/probe-features.py --list
python3 tools/probe-features.py mouse --help
python3 tools/probe-features.py tcap --cap Co
python3 tools/probe-features.py mouse --seconds 60
```

Run in a newly built Revenant window. Ctrl+right-click opens the menu: switch
**Allow Tcap Ops** off and rerun the `tcap --cap Co` query; it should become
silent under the default restrictions. Switch it on and it should reply
again. During `mouse`, uncheck **Allow Mouse Ops** to stop mouse/focus bytes
and restore ordinary selection, then check it to resume. Both permissions
start true. A configured GetTcap exception can keep queries allowed while
the Tcap master override is unchecked:

```sh
revenant -xrm 'XTerm*allowTcapOps: false' \
  -xrm 'XTerm*disallowedTcapOps: *,~GetTcap'
```

**Allow Font Ops** remains disabled: its policy/resource plumbing is prepared,
but OSC 50 has no connected callback. The font probe cannot make font changes
work. `TN` may also be unanswered until the configured terminal-name task is
implemented; use `Co` for the existing Tcap permission check.

| Subcommand | Manual check |
| --- | --- |
| `answerback` | Send ENQ and show raw answerback bytes. A default empty answer is silent. |
| `tcap` | Decode XTGETTCAP replies; repeat `--cap NAME` to select capabilities. |
| `mouse` | Observe mouse/focus bytes; `--mode 9/1000/1002/1003`, `--seconds N`; q exits. |
| `font` | Query OSC 50; `--font NAME` explicitly requests a persistent font change. |
| `underline` | Compare colored underline styles, indexed/RGB colors, and SGR 59 reset. |
| `cursor` | Inspect startup cursor before a DECSCUSR request; `--styles` also cycles application styles. |
| `pointer` | Move over the grid to compare requested OSC 22 pointer shapes. |
| `identity` | Show DA1, DA2 and XTVERSION reply bytes. |
| `unknown` | Send an unsupported APC for future diagnostic logging; no reply is expected. |
| `cwd --cwd /tmp` | Report an OSC 7 directory for a future consumer/debug check. |
| `prompts` | Emit synthetic OSC 133 prompts for future previous/next actions. |
| `pipe` | Emit three synthetic command outputs; a future pipe action should capture only COMMAND-3-BEGIN through COMMAND-3-END. |
| `notify` | After three seconds to change focus, send OSC 9 and OSC 777 notifications. |
| `progress` | Cycle OSC 9;4 normal, error, indeterminate, paused and cleared states. |
| `glyphs` | Inspect joined boxes/blocks, braille and Powerline at different fonts/sizes. |
| `copy` | Select sample text and check a future copy-highlight flash and paste contents. |
| `search` | Seed scrollback with repeated, Unicode, missing and wrapped search cases. |
| `graphics` | Place red/green over blue/yellow using an inline Kitty RGBA image; inspect resize/scroll. |

Visual stages wait for Enter; `--delay SECONDS` advances them automatically.
`--timeout SECONDS` bounds each reply wait. These options follow the
subcommand. `just probe-features SUBCOMMAND ...` invokes the same runner.
Shell fixtures display commands and shell-looking text as data; they never
execute those commands. The graphics fixture covers static inline placement
only; later graphics tasks must add their own media/animation cases.

Ctrl+C restores termios and requests cleanup for temporary state. Mouse
modes are queried before the probe and known settings restored; modes whose
state cannot be queried are reset. Underline styling is reset; pointer shape
returns to default; progress is cleared; only the probe's image is deleted.
`cursor --styles` restores the configured default, not an earlier application
cursor override. An explicit `font --font` request is persistent: restore it
through the font menu. Shell fixtures restore the probe process's cwd URI and
close synthetic command markers; they cannot reconstruct a shell's prior
semantic state. Run in a disposable test window for comparisons requiring
pristine state.

See the [dispatch guide](../maintainers/dispatch.md) for the existing policy
APIs, feature boundaries and test expectations.
