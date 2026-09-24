---
man: revenant-search
section: 8
manual: maintainers
description: scrollback search behavior and ownership contract
---

# Scrollback search contract

Revenant's `start-search()` action opens a literal, incremental search of the
primary screen and its scrollback. The widget owns the search UI and scheduling;
`XtpTerminalSearch` owns the backend scan and matches. The matcher in
`src/search_match.c` is backend-neutral, while
`src/terminal_ghostty_search.c` supplies the real terminal search model.

## Widget, input and paint

`start-search()` is bound by default to Ctrl+Shift+F and has its own
`LocalKeyAction` identity. `src/vt_search.c` lazily creates an override-redirect
`searchOverlay` popup with an Athena Label. It sits at the terminal's
bottom-left and moves on shell `ConfigureNotify` and widget resize; keyboard
focus stays on the terminal. `VtSearchRuntime` is the embedded widget-owned
state in `src/vt_widgetP.h`: it holds the backend search handle, query bytes,
timer, active and starting-viewport marks, popup references, and per-frame
visible-match snapshot. None of these fields is an Xt resource. Xt resource
storage and `XtOffsetOf` expressions remain outside the aggregate.

While `VtSearchRuntime.active` is set, `InputEvent` gives every key press to
`VtSearchKey`, clears Xt's continue-dispatch flag so no translation action
also runs, and records the keycode as owned. Neither the press nor its later
release reaches the child, even when Escape or Enter closes search first.
Typing appends UTF-8; Backspace removes one codepoint. An input-method commit
longer than the initial 128-byte lookup buffer is read again at its full size.
A commit that will not fit the 256-byte query is refused with a bell. Each
query change calls `XtpTerminalSearchSetQuery`, then scans in 2,048-row steps
from a zero-delay timer. A query refused on the alternate screen is retried
every 250 ms and applied when the primary screen returns.

The active match is a `XtpTerminalCellMark` on its start cell, resolved by
navigating backward from the following cell. It follows reflow and eviction;
navigation drops it if the text changed. The first active match is the nearest
one above the bottom of the viewport where search began. Because scanning runs
newest first, a candidate reached only by wrapping to a newer match is ignored
until scanning completes; previous/next navigation that would wrap during a
scan rings the bell and waits too. Moving to a match outside the view centres
it.

`VtRenderTerminal` calls `VtSearchPrepareFrame` before each render. It copies
visible spans into a widget-owned buffer sized for `XTP_SEARCH_MATCH_LIMIT`, so
every retained visible match can be painted, and resolves the active span. The
spans' starts and ends both ascend, so `VtSearchCellHighlight` uses a binary
search for each cell. `MakeVisualCell` paints other matches with selection
colors and the active one with cursor colors. Search changes invalidate and
redraw the frame, deferring under synchronized-output mode 2026. Visible-match
highlights are not re-verified on each frame: overwritten text can stay
highlighted until navigation or a query change drops its match.

Enter copies `XtpTerminalSpanText` for the active match through
`VtPublishSearchMatch`, which owns PRIMARY without selection highlighting and
logs source `SEARCH`; it then closes without moving the view. If publication
fails, Enter rings the bell and leaves search open. Escape closes and restores
the bottom or the marked top row of the starting viewport, falling back to the
oldest row if that mark was evicted. While search is open, the PTY loop calls
`XtpTerminalFeedOutputPinned`: it marks the viewport's top cell before feeding
output and scrolls back to it, instead of applying the ordinary xterm policy
that keeps a fixed distance from the live bottom.

## Backend matching and invalidation

`XtpTerminalSearch` in `src/terminal.h` searches for a literal UTF-8 query.
`src/search_match.c` converts it to codepoints with a Knuth-Morris-Pratt
failure table. `XtpSearchFindAll` reports every occurrence, including
overlaps, only if both ends fall on grapheme-cluster boundaries.
`XtpSearchPick` chooses the nearest start strictly after or before a screen
cell in a descending match array, wrapping to the first or last match. Search
does not fold case or normalize Unicode: precomposed é and e+U+0301 are
different queries.

The Ghostty adapter scans newest first. A tracked reference marks the bottom
row of the next logical line. The line extends upward through rows whose
`WRAP` flag is set, up to `XTP_SEARCH_LINE_ROW_LIMIT` rows; longer lines are
split, so a match across the split is missed. Empty cells become spaces,
spacer cells are skipped, and trailing spaces of the last row are trimmed.
Each match owns tracked start and end cells.

`XtpTerminalSearchSetQuery` immediately reads the active screen, at most its
height plus the rest of one wrapped line. Those rows can still be written;
history rows cannot. Results therefore describe text present when the query
was set. New output arrives below that fixed boundary and stays out of the
search; navigation re-checks a previously read row after overwrite. A resize
that pulls unread history into the active screen before the scan reaches it is
a known gap. An immediate-scan failure returns -1 and leaves
`XTP_SEARCH_ERROR`; `XtpSearchSetAllocator` lets the self-test fail its first
allocation. `XtpTerminalSearchStep` then examines at most its row budget plus
the rest of one wrapped line. A zero budget does nothing, allowing PTY reads
between steps.

Tracked references follow scrolling, eviction and reflow. A dead scan cursor
completes the scan; `XtpTerminalSearchMatches` drops dead matches, and
`XtpTerminalSearchNavigate` re-reads a chosen match and drops it if it no
longer spells the query. The newest `XTP_SEARCH_MATCH_LIMIT` matches are kept;
additional matches set the truncated flag. The alternate screen has no
history: setting a query there fails, while an existing scan reports
`XTP_SEARCH_UNAVAILABLE` and retains its state until the primary screen
returns. The stub backend returns no search object. Reflow during a scan can
report a split long-line match twice; this affects only lines longer than the
row limit.

## Ownership and lifecycle

`src/vt_search.c` is the sole writer of `VtSearchRuntime`. It owns the lazily
allocated `XtpTerminalSearch *`, active and starting-viewport cell marks,
visible-span buffer, Xt timer registration, and shell move-handler
registration. The popup and label are Xt children: their stored widget handles
are references, and Xt tears them down with the VT widget. The backend search
borrows its `XtpTerminal *`, so it must be freed before that terminal is
replaced or freed. In the backend, the compiled query, tracked scan cursor and
match endpoints, and match/unit/pending/cluster buffers belong to
`XtpTerminalSearch`. Cancellation/reset drops the query and tracked references
but retains buffer capacity until the search object is freed. Visible spans
in the widget are copied values, not borrowed backend pointers. Text returned
by `XtpTerminalSpanText` is temporary owned memory: the selection owner copies
it before the caller frees it.

Search opens only after realization, creating the backend object and popup as
needed, recording the starting viewport, installing the shell handler, and
scheduling a scan or retry timer when needed. Query changes cancel the old
timer, clear the active mark, reset the backend query and repaint. Closing
cancels the timer and backend scan, removes the shell handler, releases marks,
pops down the overlay and optionally restores the viewport; the backend
object, popup and visible buffer remain reusable. Resize repositions the
popup. Rebinding through `XtpVtSetTerminal` closes without restoring the
viewport and frees the search object before replacing its borrowed terminal;
the next open binds a new one. Widget destruction cancels any pending timer,
removes the active shell handler and frees marks, backend search and visible
buffer before Xt destroys its child widgets. Application teardown unbinds the
widget before freeing the terminal.

Keep these boundaries as functions. `XtpVtSearchActive` controls the
application's pinned-output policy; `VtSearchKey` can scroll, ring, copy
PRIMARY or redraw; `VtSearchPrepareFrame` may prune stale backend matches;
`VtSearchCellHighlight` reads the prepared paint snapshot. Query changes,
timer stepping and close invalidate and repaint, respecting synchronized
output. `XtpTerminalSearchMatches` and `XtpTerminalSearchNavigate` are not
pure getters: they can discard dead or changed matches. Raw field toggles or
`XtGetValues`/`XtSetValues` would bypass these effects.

## Verification

xvfb-search uses a 40×10 grid configured through the columns and rows resources.
It samples backgrounds for a wrapped
highlight, other matches, navigation with wrapping and scrolling, empty and
missing queries, output that must not move the view, resize, Escape restoring
the bottom, Enter copying a wrapped match to PRIMARY, and 360
single-character matches highlighted to the last row. A raw Kitty-release
child receives no keys during search and receives the next key afterward.
`xtp-send-key` supplies `keysym NAME [ctrl-shift]` and `text STRING` modes.

The `search matching` self-test covers matcher and navigation order. The
`scrollback search` self-test covers text added after a query, overwrite,
allocation failure in the immediate scan, zero budget, empty and invalid
queries, wide characters, combining clusters, overlaps, soft-wrap matches
including a pushed wide character, narrower and wider reflow, the alternate
screen mid-scan, eviction of finished matches, a 20,000-row history scanned
in 256-row steps with output interleaved, cancellation, the match cap and
eviction during a scan. `xvfb-search-lifecycle` covers active rebind to another
terminal and to NULL, freeing old terminals, reopening, stale highlights,
adjacent Xt resources and destruction with a pending timer while the parent
shell survives; its ASan negative control detects missing timer cancellation.
