# Keyboard input differences from xterm

!!! warning "Major default-input compatibility difference"

    xterm+ does not reproduce xterm's complete default keyboard byte stream.
    It uses libghostty's legacy-plus-fixterms encoder, including before an
    application enables the Kitty keyboard protocol. Raw TTY mode exposes
    this difference directly; it does not restore xterm encoding.

## The confirmed aliases that split

xterm traditionally collapses several different key intentions into the same
control byte. xterm+ deliberately preserves the distinction:

<!-- markdownlint-disable MD013 -->

| Keys | xterm default | xterm+ default |
| --- | --- | --- |
| Tab | `0x09` | `0x09` |
| Ctrl-I | `0x09` | `CSI 105 ; 5 u` |
| Enter | `0x0d` | `0x0d` |
| Ctrl-M | `0x0d` | `CSI 109 ; 5 u` |
| Escape | `0x1b` | `0x1b` |
| Ctrl-[ | `0x1b` | `CSI 91 ; 5 u` |

<!-- markdownlint-enable MD013 -->

For example, the literal bytes for Ctrl-I are `1b 5b 31 30 35 3b 35 75` in
xterm+, while Tab remains the single byte `09`.

## Why raw mode does not make them equal

The terminal emulator chooses a key's byte sequence before writing it to the
pseudoterminal. The kernel's cooked mode may then interpret, transform, or
withhold some bytes. Raw mode disables that second layer; it does not ask the
emulator to switch keyboard encoders.

Consequently, a raw-mode program receives the fixterms sequence from xterm+
and the aliased control byte from xterm. This behavior does not imply that the
program negotiated Kitty keyboard flags. `CSI ? u` can still report a flag
value of zero.

## The broader surface

The three pairs above are the clearest and currently acceptance-tested
departure. Libghostty's default encoder also combines historical terminal
rules, xterm `modifyOtherKeys`, fixterms, and selected Kitty conventions.
Additional Ctrl+Shift, digit, punctuation, Alt+Ctrl, Caps Lock, and
non-US-layout combinations may therefore differ from xterm's default byte
stream. Treat keyboard compatibility as a matrix to test, not as an inference
from the Ctrl-I result.

Applications that parse modern CSI-u input benefit from the extra distinction.
Programs that assume xterm's aliases without negotiating a protocol can behave
differently under xterm+. This is an intentional current product decision and
is recorded in the repository's `DRIFT.md`, not an implementation gap.

The protocol mechanics and the zero-flags exception are covered in the
[TDN Kitty keyboard reference](https://toppk.github.io/xterm-plus/tdn/input/kitty-keyboard/).

## Interactive acceptance probe

The repository includes `tools/probe-keymodes.py` for comparing cooked input, raw
bytes, and negotiated Kitty input in the terminal being tested:

```sh
python3 tools/probe-keymodes.py
```

During Kitty keyboard development, skip directly to the protocol stage:

```sh
python3 tools/probe-keymodes.py --kitty-only
# or: just probe-keymodes --kitty-only
```

The Kitty stage requests flags `1|2|4|8|16`, prints every exact input byte
sequence alongside its decoded key/modifier/event fields, counts press,
repeat, and release events, and verifies that popping the flag stack restores
the value observed before the probe. A terminal which grants event flag `2`
but sends no release events receives an explicit warning.
