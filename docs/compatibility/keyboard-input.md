# Keyboard input differences from xterm

!!! warning "Major default-input compatibility difference"

    Revenant does not reproduce xterm's complete default keyboard byte stream.
    It uses libghostty's legacy-plus-fixterms encoder, including before an
    application enables the Kitty keyboard protocol. Raw TTY mode exposes
    this difference directly; it does not restore xterm encoding.

## The confirmed aliases that split

xterm traditionally collapses several different key intentions into the same
control byte. Revenant deliberately preserves the distinction:

<!-- markdownlint-disable MD013 -->

| Keys | xterm default | Revenant default |
| --- | --- | --- |
| Tab | `0x09` | `0x09` |
| Ctrl-I | `0x09` | `CSI 105 ; 5 u` |
| Enter | `0x0d` | `0x0d` |
| Ctrl-M | `0x0d` | `CSI 109 ; 5 u` |
| Escape | `0x1b` | `0x1b` |
| Ctrl-[ | `0x1b` | `CSI 91 ; 5 u` |

<!-- markdownlint-enable MD013 -->

For example, the literal bytes for Ctrl-I are `1b 5b 31 30 35 3b 35 75` in
Revenant, while Tab remains the single byte `09`.

## Why raw mode does not make them equal

The terminal emulator chooses a key's byte sequence before writing it to the
pseudoterminal. The kernel's cooked mode may then interpret, transform, or
withhold some bytes. Raw mode disables that second layer; it does not ask the
emulator to switch keyboard encoders.

Consequently, a raw-mode program receives the fixterms sequence from Revenant
and the aliased control byte from xterm. This behavior does not imply that the
program negotiated Kitty keyboard flags. `CSI ? u` can still report a flag
value of zero.

## The broader surface

The three pairs above are the clearest and currently acceptance-tested
departure. Libghostty's default encoder also combines historical terminal
rules, xterm `modifyOtherKeys`, fixterms, and selected Kitty conventions.
Additional Ctrl+Shift, digit, punctuation, Alt+Ctrl, Caps Lock, and
non-US-layout combinations may therefore differ from xterm's default byte
stream. Treat xterm byte-for-byte compatibility as a matrix to test, not as an inference
from the Ctrl-I result.

Applications that parse modern CSI-u input benefit from the extra distinction.
Programs that assume xterm's aliases without negotiating a protocol can behave
differently under Revenant. This is an intentional current product decision and
is recorded in the [xterm differences ledger](drift.md), not an implementation
gap.

The protocol mechanics and the zero-flags exception are covered in the
[TDN Kitty keyboard reference](https://toppk.github.io/revenant/tdn/input/kitty-keyboard/).

## Negotiated Kitty keyboard input

Revenant implements the five Kitty keyboard flags through its real X11 input
path. Applications can query, set, augment, clear, push, and pop flag sets.
When event reporting is active, a held key is distinguished as an initial
press, one or more repeats, and a release. Shifted/base-layout alternatives,
all-keys-as-escapes, associated UTF-8 text, bare modifier keys, and the X11
Shift, Ctrl, Alt, Super, Caps Lock, and Num Lock states are passed to
libghostty's encoder.

XKB detectable autorepeat is enabled when the X server supports it. On an
older server, Revenant recognizes the traditional adjacent release/press pair
with the same keycode and timestamp. Losing focus clears the pressed-key set
so a missing release cannot turn a later press into a false repeat.

The maintained Xvfb matrix covers normal and application cursor/keypad modes,
Shift/Ctrl/Alt/Super arrow modifiers, function and editing keys, NumLock-keypad
policy, a remapped non-US character, and a built-in XIM Compose sequence with
exact PTY bytes. The no-XIM fallback independently converts legacy Latin-1 and
Unicode keysyms to UTF-8 instead of forwarding locale-ambiguous bytes. Kitty
coverage includes keys absent from Xvfb's default map by installing an isolated
mapping and checking F13 press, repeat, and release events.

Xt-owned gestures remain local. In particular, Shift+Insert paste,
Shift+PageUp/PageDown history navigation, popup menus, mouse reporting, and
OSC 8 Shift-click do not leak partial keyboard-protocol events to the child.

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
the value observed before the probe. It warns if a terminal grants event flag
`2` but sends no release events, making it useful for comparisons with older
Revenant builds and other terminals.
