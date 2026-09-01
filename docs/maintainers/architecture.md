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
  translations, callbacks, font-selection actions, and public widget API.
- `src/vt_widgetP.h` contains the private widget record and interfaces shared
  only by the widget implementation.
- `src/vt_input.c` owns XIM, keyboard mapping and repeat tracking, and focus
  input.
- `src/vt_interaction.c` owns selection and paste, hyperlinks, mouse encoding,
  and local pointer and scroll actions. Encoded terminal input leaves the
  widget through `XtNinputCallback`.
- `src/vt_draw.c` owns frame caching, bitmap/Xft cell drawing, cursor painting,
  damage restoration, and redisplay.
- `src/vt_font.c` owns the heap-allocated Xft font universe: slot and style
  loading, metric normalization, named and system fallback candidates, lazy
  non-default size slots, and transactional universe replacement.
- `src/font_router.c` owns atom-to-role selection, fallback traversal, route
  caching, style degradation, and deterministic missing-glyph decisions. It
  returns a selected font and shaped run to `vt_draw.c`; it never paints.
- `src/glyph_cairo.c` owns the persistent Cairo Xlib surface, scaled-font
  cache, cell fitting, and color/outline glyph delegate. It receives the
  effective damage/cursor clip from `vt_draw.c`; it does not decide terminal
  geometry.
- `src/glyph_shape.c` owns HarfBuzz state and the per-Xft-font shaping cache.
  It shapes complete backend graphemes and compatible adjacent non-emoji cell
  runs, returning positioned glyph IDs without changing the backend's
  committed cell width. `font_router.c` chooses primary, explicit
  wide/emoji/Han, and bounded fontconfig fallback faces atomically for each
  atom; `vt_draw.c` itemizes compatible runs and clips ink to their combined
  committed cells.
  Font ownership is intentionally isolated: Xft owns its live FreeType face,
  Cairo creates an independent cairo-ft face, and HarfBuzz maps a separate face
  from the font file. Sharing Xft's live face lets one consumer change another
  consumer's size state and is forbidden. The HarfBuzz reconstruction currently
  uses the file and face index but not `FC_FONT_VARIATIONS` or named-instance
  coordinates, so configured variable-font axes can differ in multi-glyph run
  positioning. Runs are shaped per repaint without a text-run cache; profile
  redraw before adding one. Atomic emoji-sequence probes preserve HarfBuzz
  default-ignorables while checking for one composed glyph; ordinary drawing
  removes unresolved controls only after that support decision is made.
- `src/font_chain.c` owns the characterized two-entry, prefix-aware Xft list
  grammar shared by primary, doublesize, and emoji roles. The complete
  capture, fallback, style, cache, reload, and reporting rules live in the
  [font-resolution contract](font-resolution.md); keep implementation status
  there distinct from normative end-state behavior.
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

xterm patch 411 is the current visible compatibility oracle and a source
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
