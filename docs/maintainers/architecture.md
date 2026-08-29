# Architecture and source boundaries

Revenant combines an xterm-compatible X11 host with a modern terminal engine.
That sentence defines the principal boundary: Revenant owns presentation and
desktop integration, while `libghostty-vt` owns terminal semantics.

## Runtime layers

Input travels from X11 through the VT widget to the terminal encoder and then
to the PTY. Output returns from the PTY through the terminal engine and is
drawn by the X11 renderer:

```text
X11 keyboard / mouse / focus
             |
             v
     VT100 Xt widget  ---- local selection, menus, scrollbar
             |
             v
  backend-neutral terminal API
             |
             v
       libghostty-vt  <---->  child process through the PTY
             |
             v
   cells, styles, cursor, effects
             |
             v
   Xlib / Xft + Cairo renderer
```

The boundaries are deliberate:

- `src/main.c` owns the Xt application shell, PTY event loop, menus, geometry,
  and terminal effects such as bell and title changes.
- `src/vt_widget.c` owns the custom `VT100` widget class, resources, lifecycle,
  font slots, translations, callbacks, and public widget API.
- `src/vt_widgetP.h` contains the private widget record and interfaces shared
  only by the widget implementation.
- `src/vt_input.c` owns XIM, keyboard mapping and repeat tracking, and focus
  input.
- `src/vt_interaction.c` owns selection and paste, hyperlinks, mouse encoding,
  and local pointer and scroll actions. Encoded terminal input leaves the
  widget through `XtNinputCallback`.
- `src/vt_draw.c` owns frame caching, bitmap/Xft cell drawing, font-role
  routing, cursor painting, damage restoration, and redisplay.
- `src/glyph_cairo.c` owns the persistent Cairo Xlib surface, scaled-font
  cache, cell fitting, and color/outline glyph delegate. It receives the
  effective damage/cursor clip from `vt_draw.c`; it does not decide terminal
  geometry.
- `src/terminal.h` is the backend-neutral terminal boundary.
  `src/terminal.c` holds policy shared by both backends, currently output feed
  plus viewport anchoring. The `terminal_ghostty.c` and `terminal_stub.c`
  implementations are selected at link time.
- `src/pty.c` owns child creation, lifecycle, queued writes, reads, and kernel
  window-size updates.

## Ownership rules

`libghostty-vt` is the source of truth for parsing, terminal modes, grid state,
history, reflow, grapheme bytes, terminal selection primitives, input encoding,
and protocol effects. The X11 host must not infer terminal state from raw PTY
bytes or maintain a competing screen/history model.

Revenant is the source of truth for Xt resources and translations, Athena
widgets, X11 selections, rendering, window-manager interaction, local gestures,
and policy choices such as whether output returns a historical viewport to the
live bottom.

Ghostty-specific handles and types stop at the backend implementation. New
callers use `terminal.h`; they do not reach through it into Ghostty internals.
The stub backend must continue to compile so this separation remains tested.

The [rendering review](../compatibility/rendering-review.md) records the more
specific repaint, damage, cursor, and resize invariants. The
[roadmap](roadmap.md#architecture-guardrails) records the rules that should
remain true as new features are promoted.

## Relationship to xterm

xterm patch 410 is the current visible compatibility oracle and a source
reference. Revenant does not compile, link, or embed xterm's terminal engine.
Its application, PTY layer, widget, renderer, diagnostics, and backend adapter
are separate implementations; `libghostty-vt` replaces xterm's parser and
terminal-state engine.

The repository deliberately carries xterm-derived compatibility material:

- `data/app-defaults/XTerm`;
- resource, action, and app-default catalogs under `compat/`;
- menu and widget names;
- resource defaults and translation bindings;
- behavioral details reconstructed while consulting xterm's implementation.

Those portions are covered by
[`LICENSES/xterm.txt`](https://github.com/toppk/revenant/blob/master/LICENSES/xterm.txt).
Consult xterm to reproduce external behavior, not to transplant its terminal
engine. Keep the checked-in compatibility catalogs synchronized whenever the
oracle advances.

## Upstream dependencies and references

Ghostty is fetched into the ignored `upstream/` directory and built as a
separate dependency under its own license. Ghostling and xterm reference
checkouts also live there when present; they are research inputs rather than
vendored project source.

See [Upstream reference checkouts](upstream.md) for the exact roles, revision
policy, and update procedure. A Ghostty update should be isolated, recorded,
and tested across GCC, Clang, and the stub backend before unrelated feature
work continues.

## Documentation boundary

Durable user behavior, architecture decisions, compatibility status, and
maintenance procedures belong in the public `docs/` tree. Keep the repository
README as the entry point for users and maintainers, with links to the detailed
manual instead of parallel copies of it.

Temporary investigation notes and session-specific coordination are not an
architecture reference. Promote any conclusion that must survive the current
work into the appropriate public document, test, or compatibility catalog.
