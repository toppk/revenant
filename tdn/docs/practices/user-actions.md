# User actions and host integration

These IDs describe user-facing behavior and host integration named in Revenant’s
maintenance plans. They are not new escape-sequence standards. Optional and
undecided designs are identified explicitly; registration alone promises no
implementation or release date. Terminal support requires separate evidence.

## Separately tracked behavior

### Unknown APC diagnostics

Feature ID: `diagnostics-unknown-apc`. Bounded, non-executing diagnostics for unsupported APC strings; no protocol reply or extra OSC/CSI parser.

### Previous and next prompt navigation

Feature ID: `ui-prompt-navigation`. User actions move the viewport between semantic prompts, including wrapped and evicted history.

### Pipe the last command output

Feature ID: `ui-pipe-command-output`. An explicit user action sends the semantic output range to a configured process over stdin, using validated command cwd.

### Clear to prompt

Feature ID: `ui-clear-to-prompt`. Optional user action whose viewport-versus-history-deletion contract must be decided before implementation.

### Notification urgency on X11

Feature ID: `notification-x11-urgency`. Application notification requests set WM urgency while unfocused and clear it on focus without disturbing other hints.

### Desktop notification delivery

Feature ID: `notification-desktop-delivery`. Optional libnotify delivery of OSC 9/777 titles and bodies as escaped, bounded data while unfocused, with a rate limit, failure backoff, recovery and unchanged urgency.

### Progress indicator

Feature ID: `ui-progress-indicator`. Display normal, error, indeterminate, paused, and cleared application progress without corrupting title state.

### Procedural block-element drawing

Feature ID: `text-block-drawing`. Draw block elements from cell metrics with clipping and the same color/selection rules as text.

### Procedural braille drawing

Feature ID: `text-braille-drawing`. Rasterize braille dots consistently at odd and even cell sizes with an explicit fallback range.

### Procedural Powerline drawing

Feature ID: `text-powerline-drawing`. Rasterize supported Powerline separator glyphs and define when font fallback remains in use.

### Copy-highlight feedback

Feature ID: `selection-copy-feedback`. Timed visual feedback after successful user copy, without changing copied bytes or reacting to OSC 52 writes.

### Literal scrollback search

Feature ID: `search-scrollback-literal`. Bounded incremental Unicode search across logical lines, with wrapped ranges, eviction, and next/previous navigation.

### Interactive search overlay

Feature ID: `ui-search-overlay`. Keyboard-driven query UI with match highlighting and viewport/focus restoration on dismissal.

### Copy a search match

Feature ID: `search-copy-match`. Explicit acceptance of a search match copies it to PRIMARY without losing viewport or ownership semantics.

### Kitty graphics storage limit

Feature ID: `kitty-graphics-storage-limit`. Bound retained decoded image storage and define eviction or refusal behavior.

### Kitty graphics command byte limit

Feature ID: `kitty-graphics-command-limit`. Bound APC command bytes, including chunked transmissions and failed image decoding.

### Refresh hyperlink hover after output

Feature ID: `hyperlinks-hover-refresh`. Refresh an inferred-link underline after the painted text changes while the pointer is stationary.

### Terminal tabs

Feature ID: `ui-tabbed-terminals`. Open design question: multiple terminals in one Xt window, with explicit resource and focus ownership.

### Split terminal panes

Feature ID: `ui-split-terminals`. Open design question: independent terminal panes while preserving Xt and window-manager contracts.

### Configuration profiles

Feature ID: `configuration-profiles`. Open design question: named configurations compatible with X resources rather than an unrelated configuration system.

### Live configuration reload

Feature ID: `configuration-live-reload`. Open design question: reload resource-backed settings with explicit lifetimes, errors, and state preservation.

### Terminal state snapshots

Feature ID: `session-snapshots`. Future backend-owned snapshot interface; do not freeze an application-private wire format in advance.

### Raw PTY stream fanout

Feature ID: `session-pty-fanout`. Future delivery of the raw PTY stream to multiple clients without byte loss or reordering.

### Asynchronous scrollback history

Feature ID: `scrollback-asynchronous-history`. Future history access decoupled from the immediate renderer and parser lifetime.

### Independent client viewports

Feature ID: `ui-independent-viewports`. Future per-client viewport and selection ownership for shared terminal state.

### Snapshot resynchronization

Feature ID: `session-snapshot-resync`. Future recovery of a client by taking a fresh terminal snapshot and resuming ordered stream delivery.

### Touchpad inertial scrolling

Feature ID: `input-touchpad-inertia`. Low-priority smooth-scroll/inertia behavior requiring an XInput2 input path.

### Force procedural box characters

Feature ID: `resource-force-box-chars`. `forceBoxChars` and the
**Procedural Glyphs** (`font-linedrawing`) menu item select procedural drawing
versus font glyphs for every supported range.

### Procedural terminal glyph families

Feature IDs: `text-dec-scanline-drawing`,
`text-geometric-terminal-drawing`, `text-legacy-computing-drawing`, and
`text-legacy-computing-supplement-drawing`. These distinguish DEC scan lines,
terminal-oriented geometric pieces, Symbols for Legacy Computing, and its
Unicode 16 supplement so terminals can report each family independently.

### Retain selection ownership

Feature ID: `selection-keep-selection`. Selection retention across changes to the screen and mouse selection lifetime.

### Retain clipboard ownership

Feature ID: `selection-keep-clipboard`. Clipboard retention policy and ownership lifetime independent of PRIMARY.

### ICCCM text selection targets

Feature ID: `selection-icccm-targets`. Advertise and convert supported X11 text targets with explicit fallback and ownership rules.

### Paste control filtering

Feature ID: `paste-control-filtering`. Configured handling of control bytes in user paste, distinct from clipboard protocol permission.

### Visual bell

Feature ID: `bell-visual`. User-configured visible bell indication with repaint and timer lifetime semantics.

### Scrollbar presentation styles

Feature ID: `ui-scrollbar-styles`. Supported scrollbar appearance and interaction choices.

### Menu and translation-action parity

Feature ID: `ui-menu-action-parity`. Aggregate inventory: each menu/action is implemented, disabled, or explicitly classified.

### Login-shell startup

Feature ID: `startup-login-shell`. Accepted login-shell option changes the child invocation as specified by xterm.

### Hold the window after child exit

Feature ID: `startup-hold-after-exit`. Configured window retention after the PTY child exits.

### Wait for mapping before startup

Feature ID: `startup-wait-for-map`. Configured ordering of window mapping and child startup.

### PTY message permissions

Feature ID: `pty-message-permission`. Configured message-permission behavior for the child terminal device.

### Initial terminal mode settings

Feature ID: `resource-terminal-modes`. Apply configured terminal/tty mode settings during PTY startup.

### PTY session transcript logging

Feature ID: `logging-session-transcript`. -/+l, -lf, logFile, logInhibit and the logging action control one PTY-output tee; separate from diagnostic log severity.

### X11 resource and application identity

Feature ID: `x11-resource-identity`. Class/instance/WM_CLASS, invocation names, app-default lookup and migration compatibility.

### Keyboard selection extension

Feature ID: `selection-keyboard-extension`. Shift+Select-style keyboard extension with explicit anchor, focus and ownership rules.

### Scroll Lock behavior

Feature ID: `input-scroll-lock`. Scroll Lock state, input handling and scrolling policy with xterm-compatible semantics.

### User fullscreen toggle

Feature ID: `ui-fullscreen-toggle`. Coherent fullscreen resource/menu/Alt+Return action and EWMH behavior under a real window manager.

### Clear saved lines action

Feature ID: `ui-clear-saved-lines`. The xterm clear-saved-lines action and its binding; reset scope must match the maintained implementation contract.

### xterm resource compatibility

Feature ID: `xterm-resource-compatibility`. Aggregate classification of command-line and X resource surfaces; parsed is not implemented.

<!-- tdn:compatibility -->
