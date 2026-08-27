# Interactive terminal probes

Human-run visual and interactive verification programs use the naming pattern
`tools/probe-<feature>.<ext>`. This keeps them discoverable separately from
build utilities and automated `tests/xtp-*` or `tests/xvfb-*` helpers.

Run the same probe inside xterm+, xterm, and Ghostty when comparing behavior.
Record the emulator version, font configuration, locale, and exact invocation
with any result.

| Probe | Purpose |
| --- | --- |
| `tools/probe-color.sh` | SGR attributes, underline styles, 16/256 colors, and semicolon/colon truecolor ramps. |
| `tools/probe-osc8.sh` | OSC 8 labels for Shift-hover and safe HTTP(S)-only Shift-click activation. |
| `tools/probe-emoji.py` | Emoji presentation, variation selectors, skin tones, ZWJ and flag sequences, combining text, width, and alignment. |
| `tools/probe-keymodes.py` | Cooked input, raw bytes, fixterms drift, Kitty flags, associated text, event types, and flag-stack restoration. |

Examples:

```sh
tools/probe-color.sh
tools/probe-osc8.sh
python3 tools/probe-emoji.py --no-pause
python3 tools/probe-keymodes.py --kitty-only
```

The equivalent `just` recipes are `probe-color`, `probe-osc8`, `probe-emoji`,
and `probe-keymodes`.

The probes deliberately do not decide pass or fail from screenshots or visual
output. Stable machine assertions belong in the automated test suite. The
keyboard probe is the exception for protocol parsing: its noninteractive
`--self-test` validates the decoder used to explain captured bytes.

Machine acceptance for keyboard delivery lives separately in
`tests/xvfb-keyboard.sh` and `tests/xvfb-kitty-keyboard.sh`. The latter checks
exact press, repeat, release, alternate-key, associated-text, and modifier
bytes through a real X server and PTY; it is not a replacement for testing a
human keyboard layout or input method with `probe-keymodes.py`.
