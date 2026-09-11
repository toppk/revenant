# Ops permission surfaces

These IDs separate policy behavior from the protocols it governs. The names
follow the xterm-compatible resource model; an ID does not claim that every
emulator uses that model or that Revenant has completed its implementation.

## Separately tracked behavior

### Mouse Ops named exceptions

Feature ID: `policy-mouse-ops-exceptions`. Per-operation disallowedMouseOps names, wildcards and negation based on effective tracking state.

### Font Ops policy

Feature ID: `policy-font-ops`. GetFont and SetFont permission, named exceptions and live menu state.

### Color Ops and synthetic events

Feature ID: `policy-color-ops-send-events`. Match allowSendEvents interaction with effective Color Ops permission.

### Kitty color-control permission

Feature ID: `policy-kitty-color-ops`. Define and enforce Color Ops coverage for libghostty Kitty OSC 21 colors.

<!-- tdn:compatibility -->
