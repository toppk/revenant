---
man: revenant-signals
section: 7
manual: maintainers
description: user-signal reservation and xterm signal-handling comparison
---

# Signal handling

The patch-411 xterm reference has no USR1/USR2 handler registrations in its C
sources. They retain their default terminating action. In
`upstream/xterm-snapshots/main.c`, `posix_signal` installs the child reaper;
`spawnXTerm` configures the parent dispositions after creating the child.

The final non-System-V branch installs `Exit` for SIGINT, SIGQUIT,
SIGTERM and SIGPIPE. Historical System V branches vary between exit and ignore
based on platform and process group. SIGHUP likewise depends on the build's
System V handling. These are explicit process/session policies, not a general
scheme for ignoring unknown signals.

Released Revenant through 0.8.1 has no HUP/USR1/USR2 handlers. The development
startup policy installs caught no-op handlers for all three before opening the
display or creating a child. USR1 and USR2 remain reserved no-ops. HUP becomes
the configuration reload signal once the event-loop state is ready, using Xt
signal notification; teardown restores its no-op before removing that state.
`SA_RESTART` prevents ordinary interrupted I/O from becoming a new PTY failure
path. HUP received before reload setup or during teardown is a no-op.

Unlike xterm's modern-platform parent policy of ignoring HUP, Revenant uses it
for safe configuration reload. The child shell retains normal HUP behavior,
including Revenant sending HUP when closing a live PTY child.

A caught no-op is intentional: caught handlers reset to SIG_DFL when a child
execs. SIG_IGN persists across exec and would change shell/application behavior
unless every execution path restored it. This includes PTY commands and helper
processes. SIGPIPE retains Revenant's separate EPIPE handling and explicit child
reset.

SIGCHLD uses a caught handler that only calls `XtNoticeSignal`. Its Xt callback
reaps the specific PTY child, drains a bounded nonblocking output burst, and
closes or holds the window independently of PTY EOF. Background descendants
retaining the slave PTY do not extend the window's lifetime. Pipe helpers keep
their own reaping; the callback does not use `waitpid(-1)`. The handler is
installed after spawning, with an immediate reap notification to cover a child
that already exited, and is removed before destroying application state.
`xvfb-hold` checks this path with a background process retaining stdin.

Do not treat termination, crash or job-control signals as spare no-op slots.
Define their cleanup or scheduling behavior separately when implementing it.
Reserving the user signals cannot protect an already-running older executable;
reload callers must know the target supports the operation. Target a window
advertising the capability and verify its local process identity before signaling.

`xvfb-user-signals` checks parent survival and PTY replies across USR1/USR2,
checks the executed child's default dispositions, and confirms that a direct
USR1 still terminates that child normally. `xvfb-theme-reload` covers the active
HUP reload path. Run those focused tests when changing this policy.

## Discovering running reload support

New windows publish `_REVENANT_VERSION` as UTF8_STRING. After HUP reload is
successfully installed, `_REVENANT_CAPABILITIES` (ATOM list) includes
`_REVENANT_RELOAD_SIGHUP_V1`. The capability is removed before reload teardown.
These three public X11 identifiers are exact-token exceptions to the internal
branding check; implementation names remain subject to that check.

The version alone is not a capability test: development builds can share a
version number while differing in signal support.

```sh
xprop -id "$WINDOWID" _REVENANT_VERSION _REVENANT_CAPABILITIES _NET_WM_PID WM_CLIENT_MACHINE
```

A reload sender should enumerate window clients, require the capability,
and associate each local window with its live process using `_NET_WM_PID` and
`WM_CLIENT_MACHINE`, deduplicating windows for a shared PID. Do not infer support
from WM_CLASS (Revenant intentionally uses xterm/XTerm by default), the currently
installed executable's version, or a process name. Older running windows lack the
capability even after upgrading the executable on disk. A Linux pidfd can pin
the process before rechecking window identity/capability and delivering HUP.

Unchanged HUP reloads compare resolved RGB values with the configured widget
colors before applying an update. Equivalent color spellings are no-ops too.
This avoids resetting terminal colors, invalidating rendering, and repainting
for an unchanged configuration.

Theme changes use the widget's dedicated batch color update and publish the full
replacement through its backing pixmap. They bypass XtSetValues' clear-before-
expose path; frame color changes also avoid clearing the visible window before
painting. The sync-output paint test samples new text/background before allowing
any Xt expose dispatch, as well as after normal event processing.
