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
| `tools/probe-reverse-video.sh` | Enter-gated comparison of SGR 7, DECSCNM repaint/restoration, explicit backgrounds, and widget reverse video. |
| `tools/probe-osc8.sh` | OSC 8 labels for Shift-hover and safe HTTP(S)-only Shift-click activation. |
| `tools/probe-emoji.py` | CPR-measured legacy-versus-mode-2027 widths for emoji presentation, modifiers, ZWJ sequences, flags, cluster boundaries, right-margin wrapping, and over-capacity clusters. |
| `tools/probe-fonts.py` | CPR-measured widths plus visual diagnostics for combining marks, conjuncts, enclosing and spanning marks, and Zalgo-style stacking. |
| `tools/probe-keymodes.py` | Cooked input, raw bytes, fixterms drift, Kitty flags, associated text, event types, and flag-stack restoration. |

Examples:

```sh
tools/probe-color.sh
tools/probe-reverse-video.sh
tools/probe-osc8.sh
python3 tools/probe-emoji.py --no-pause
python3 tools/probe-emoji.py --regime legacy
python3 tools/probe-emoji.py --regime cluster
python3 tools/probe-fonts.py --no-pause
python3 tools/probe-keymodes.py --kitty-only
```

The equivalent `just` recipes are `probe-color`, `probe-reverse-video`,
`probe-osc8`, `probe-emoji`, `probe-fonts`, and `probe-keymodes`.

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
