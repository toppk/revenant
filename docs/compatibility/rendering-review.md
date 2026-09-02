---
man: revenant-rendering
section: 7
manual: compat
description: repaint, damage, cursor, and resize invariants
---

# X11 rendering review

Review of `src/vt_widget.c` drawing against upstream xterm
(`upstream/xterm-snapshots`) and, for contrast, ghostty/ghostling.
Goal: no flashing, blanking, or stale bands. xterm-level performance is
sufficient; frame batching is explicitly not wanted.

Revision 2 corrected overclaims about xterm found in review (xterm does
clear in some paths; Xft painting is ordered, not atomic; xterm renders
available input before returning to `select`, not per line) and adds the
cursor-only and Expose invariants.

Revision 3 records the correctness-first rewrite. All seven recommended
changes are implemented; the visual and performance acceptance items called
out below still require deliberate runtime coverage.

Revision 4 records two distinct resize failures found while repeatedly
reflowing a large scrollback. Grid dimensions changed while the last-painted
frame stayed valid, allowing Expose handling to paint an old-grid snapshot
with new-grid geometry. Resize now invalidates that snapshot before callbacks
or repaint. The separate wrapped Bash prompt failure is an identified Readline
8.3 regression. After widening, stale per-screen-line invisible-prompt
metadata moves the cursor left by the final invisible run's byte length. Bash
development commit `1e9f5e10b2` fixes it by refreshing expanded prompt
metadata after every `SIGWINCH`; the frame-cache fix and libghostty reflow are
not involved. See the
[source diagnosis](../reference/bash-readline-resize.md).

Revision 5 records a rapid-update TUI failure. One observed application refresh
larger than 4095 bytes was split by the Linux PTY, then painted once after the
erase-heavy prefix and again after the replacement content. The input callback
now drains a bounded burst of currently available data into terminal state and
paints once before returning to Xt.

## Summary

Revenant and xterm both draw directly to the window, immediately, as PTY
data arrives. The pre-rewrite Revenant renderer flashed and smeared because
it broke several disciplines xterm follows. The byte-sniffing scroll fast
path and content-diff scroll detector added to compensate were themselves
a source of wrong output and have been removed.

Recommendation: keep direct, immediate drawing (the xterm model), adopt
xterm's disciplines, and delete the heuristics. Do not move to a
pixmap/DBE frame buffer as the primary fix; that is the batched-frame
behaviour we want to avoid. Buffering remains the only way to get truly
atomic presentation, so the target is *ordered, low-flicker painting*,
not atomicity.

## What xterm does (with references)

1. **Clears only known background regions, or immediately repaints
   after a broader clear.** Row painting is background + glyphs per run:
   `XDrawImageString` (`util.c:4276`) on the core path — a single
   request — or `XftDrawRect` followed by the glyph draw
   (`util.c:4691`) on Xft, which is two requests but ordered and
   clipped to the cell box with `XftDrawSetClipRectangles`
   (`util.c:4048`). Whole-window clears do exist (`xtermRepaint`,
   `util.c:3167`; `xtermClear2` uses `XClearArea`, `util.c:2887`) but
   are followed at once by `ScrnRefresh`; separately, the no-copy scroll
   path clears only cells that are meant to stay blank (`ClearUndrawn`,
   `util.c:777`), which need no repaint.
2. **Scroll-copy is done safely.** GCs set `graphics_exposures = True`
   (`cachedGCs.c:352`); `copy_area` records the copied rectangle
   (`util.c:2663`); `CopyWait` consumes `GraphicsExpose`/`NoExpose`
   (`util.c:2573`); `HandleExposure` translates Expose events arriving
   mid-copy (`util.c:2790`).
3. **Scroll amounts come from the emulator** (`xtermScroll`,
   `util.c:940`; jump-scroll via `scroll_amt`/`FlushScroll`). No content
   diffing, no guessing from bytes.
4. **Expose repaints only the damaged rectangle**, from the screen
   buffer (`handle_translated_exposure`, `util.c:2922`).
5. Colours and GCs are cached (`getXftColor`, `cachedGCs.c`); no
   server round trips while drawing.
6. Timing: `in_put` (`charproc.c:6778`) reads whatever is available,
   parses all of it, paints, `XFlush`, then `select`. Rendering
   boundaries are available-input bursts, not newlines or individual
   kernel read fragments. No frame timer. DBE
   (`buffered`, 40 fps) exists but is off unless built with
   `--enable-double-buffer` (`main.h:184`).

## Defects found in the pre-rewrite renderer

| Symptom | Cause | Revision 3 status |
|---|---|---|
| Whole-window flash on full repaint | Whole-window clear, then rows painted in two passes | Removed; rows paint background and glyphs per run |
| Stale/garbage bands when partly obscured or off-screen | Unsafe window→window copy without exposure repair | All window copies removed |
| Wrong colours/attributes and DECSTBM behavior | Raw PTY-byte scroll path bypassed the emulator | Raw-byte path deleted |
| Spurious scrolling and O(rows²·cols) work | Content-diff scroll detector | Detector deleted |
| Piecemeal, staggered painting | `XQueryColor` on every text draw | Xft colors cached with pixels |
| Pixel crumbs, especially bold | Synthetic Xft bold with no cell clip | Real Xft bold faces and run clips |
| Double flash on font change/redraw | Clear-generated Expose followed by another clear | Redraw calls the cache/full repaint directly |
| Full repaint on every Expose | `Redisplay` ignored the damage region | Cached rows/columns intersecting damage are repainted |
| No-newline output delayed | Incomplete lines held on an 8 ms timer | Every PTY read is fed and rendered immediately |
| Rapid-update TUI flashes between erased and replaced content | One logical refresh crossed a PTY read boundary and each fragment was painted | Drain currently available input, up to a fairness budget, and paint once |
| Dim text has normal intensity | The adapter preserved SGR 2 `faint`, but the widget discarded it | Scale foreground RGB to two-thirds, matching xterm's default `faintIsRelative: false` policy |
| Bold ANSI headings retain muted base colors | Bold font selection did not apply xterm's default `boldColors` palette rule | Promote foreground palette colors 0–7 to 8–15; expose `-/+pc` and `boldColors` to control it |
| Stale rows painted with old geometry after resize | Grid changed while the old `frame_cells` snapshot remained valid | Resize invalidates the frame before callbacks and Expose |
| Wrapped Bash prompt leaves cursor inside the prompt after shrink/grow | Readline 8.3 retains stale `local_prompt_invis_chars[]` metadata when widening | Fixed in Bash development commit `1e9f5e10b2`; keep the fixture as external-component coverage and do not work around it in Revenant |

The unused `XtpVtUpdateScrolled`, raw-byte helpers, content detector, and scroll
branch of `RenderBegin` were deleted together.

## Invariants the rewrite must keep

- **Cursor-only frames.** libghostty can report no cell damage while
  the cursor moves. `XtpTerminalRender` already calls begin/end in that
  case and `RenderEnd` performs the
  cursor-only repaint (`RenderEnd`). Cursor, scrollbar thumb,
  viewport offset, and focus state must be refreshed independently of
  dirty rows. The self-test covers a cursor move with zero damaged cells.
- **Expose is not terminal damage.** X damage is repainted from the
  widget's last-painted `frame_cells` snapshot, clipped to the exposed
  rectangle — never by asking libghostty for dirty rows. Cursor
  intersection and wide/grapheme cells straddling the exposed edge must
  be handled (repaint the whole cell group).
- **Frame geometry must match widget geometry.** A cached frame is repaintable
  only while its rows, columns, and cell geometry describe the current widget.
  `ResizeWidget` invalidates `frame_cells` before publishing new grid
  dimensions or invoking resize callbacks. The next terminal render or Expose
  rebuilds the snapshot from libghostty's reflowed state; it must never map an
  old snapshot onto a new grid.
- **Resize is one ordered transaction.** Update the kernel PTY size first, then
  resize/reflow terminal state before returning to the Xt event loop. Child
  `SIGWINCH` output cannot be read and fed between those operations. This
  matches Ghostty's `Termio.resize` ordering and ensures any subsequent shell
  redraw is parsed against the new grid.
- **Emulator is the only source of truth for content.** Nothing is
  painted from raw PTY bytes.
- **Kernel read fragments are not presentation frames.** Drain currently
  available PTY data into the emulator and paint once before returning to Xt.
  Keep the drain bounded so a continuous producer cannot starve other events.

## Applied changes

1. **Done:** paint rows in place. Each run draws its own background
   (`XDrawImageString` on the bitmap path; `XftDrawRect` immediately
   followed by the clipped glyph draw on Xft). Clear only regions known
   to be undrawn (e.g. beyond the grid after a resize), with the repaint
   ordered directly behind the clear. Remove the whole-window
   `XClearWindow` from the render path; full repaint = walk rows.
2. **Done:** drop window→window `XCopyArea` (xterm `no_copy_area` mode) as the
   correctness-first step. If scroll-copy is wanted later, port
   `CopyWait` and the Expose translation together with
   `graphics_exposures = True`.
3. **Done:** delete `XtpVtPredictSimpleScroll`, `XtpVtDrawSimpleScrolledLine`,
   `DetectVerticalScroll`, and the dead scroll path.
4. **Done:** feed each PTY fragment to libghostty immediately, drain up to
   256 KiB of currently available input, and render once at the end of that
   burst. This removes both newline splitting and the 8 ms incomplete-line
   hold without exposing an erase-heavy prefix as an intermediate frame.
   Dirty state decides what to paint, not the byte stream. The budget preserves
   fairness for other Xt events under a continuous producer.
5. **Done:** cache `XftColor` alongside `Pixel` in the colour cache.
6. **Done for Xft:** load a real bold face (`FC_WEIGHT`) instead of
   double-striking; clip
   each run to its cells.
7. **Done:** `Redisplay` repaints only rows/columns intersecting the exposed
   region from `frame_cells`. The extra `XClearArea` calls are removed from
   redraw and font switching.
8. **Partial:** grid resize invalidates the last-painted frame before the grid
   is changed, and the resize callback updates PTY geometry before terminal
   reflow. `xtp-resize-loop` provides repeatable narrow/wide X11 coverage.
   The wrapped-prompt fixture also distinguishes released Readline 8.3 from
   the upstream-fixed build while confirming cached-frame geometry remains
   correct.
9. **Done:** preserve SGR 2 through visual-cell conversion by reducing each
   foreground RGB component to two-thirds. This matches xterm's default
   absolute faint calculation; relative-to-background faint remains tied to
   the unsupported `faintIsRelative` resource.
10. **Done:** implement `boldColors` with xterm's default value of true.
    Bold foreground palette colors 0–7 use configured colors 8–15; `+pc` or
    `boldColors: false` retains the base color while still selecting the bold
    font face.

The bitmap path retains its existing clipped synthetic-bold fallback until a
separate xterm `boldFont` resource is wired.

## Acceptance criteria

- During `DIRTY_FULL` repaints, no request clears the whole grid or a
  multi-row band ahead of its repaint; every background fill is
  immediately followed by the glyph request for the same run (verify
  request ordering with xtrace — a screen recording cannot prove the
  absence of intermediate frames).
- Partially obscured / off-screen window scrolls without stale bands.
- Persistent SGR, DECSTBM, and alt-screen output render identically to
  a full repaint (regression tests for each).
- Cursor-only movement repaints without cell damage.
- Expose of a sub-rectangle repaints only that rectangle, correctly,
  including wide cells at its edge.
- Prompt and cursor-control output without a trailing newline appears
  without the 8 ms hold.
- An erase-then-replace refresh split across multiple PTY reads produces one
  replacement render, never a visible intermediate erased frame.
- SGR 2 text is visibly dimmer than normal text, while SGR 1 continues to use
  the selected bold face independently.
- With default `boldColors`, bold ANSI colors 0–7 paint through their bright
  counterparts 8–15. With `+pc`, the same cells retain their base colors and
  only their font weight changes.
- Repeated narrow/wide resize of large scrollback never paints prompt
  fragments or stale rows from a previous grid. After returning to the live
  bottom, editable input and its cursor remain intact. Exercise this with
  `xtp-resize-loop` and a live Readline buffer.

## Cost caveat

libghostty-vt exposes a snapshot with per-row dirty bits and a FULL
flag, not an edit stream, and marks FULL on every scroll. After change
1 each scrolled line costs an in-place repaint of every row. This is
not equivalent to xterm's no-copy mode, which knows the scroll region
and clears/repaints only undrawn cells; it is strictly more work.
Measure after implementing changes 1–5 (cells painted and X requests
per scrolled line under `yes`/`cat` of a large file) before deciding
whether scroll-copy with proper exposure handling needs to come back.
