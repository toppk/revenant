# Dispatchable feature TODO

Monstar (`upstream/monstar`) is a reference implementation, not a promise
that the pinned libghostty C API exposes every hook. Read `HANDOFF.md` and
`docs/maintainers/dispatch.md` before starting a chunk. This tracked checklist
contains only open work; durable implementation contracts belong in the handoff
and supporting docs.

Each ID is an open, bounded implementation/review unit. `TDN:` names the
feature slugs in `tdn/data/features.yaml`; chunk IDs are work units, not slugs.
`Probe:` is a runnable **Go** command: `just probe FEATURE-SLUG CASE-ID [OPTIONS]`.
Run it inside the terminal under test; `just probe` builds as needed. A feature
slug alone opens its case menu, and `--help` after a case shows its settings.

`Supporting fixture:` supplies inputs or a baseline, not full acceptance for
that chunk. `Probe gap:` means extend the Go probe in the implementation chunk;
do not invent an invocation or add a parallel Python/shell probe. An existing
fixture does not prove the terminal implements the feature. A registry slug
without a mapped case cannot yet be launched from the probe browser.

For each dispatch: include dependencies, acceptance and stop boundary below;
check the pinned API before implementation; extend the native case and its TDN
mapping where needed; test at 80×24, on normal exit and q/Escape cleanup. Record
terminal version, font/resources and Ops policy with observations. Run
`just test-probe` and `just check-tdn` when changing probe/registry code, plus
appropriate backend/self-tests and Xvfb acceptance for the terminal change.
Use the existing Ops family only where named. Complete review and commit a
finished chunk before starting the next dependent batch.

Legacy removal is a separate task after the maintainer's manual comparison:
[TDN migration worksheet](tdn/docs/probe-migration.md). Keep old tools/recipes
available meanwhile; new features use the Go runner now.

## Remaining dispatch order and dependencies

Existing IDs stay stable. K2 has three sequential review slices under its
existing ID; its parent is complete only after all three.

| Batch | Chunks | Dependency / decision before dispatch |
| --- | --- | --- |
| Rendering and selection | G2; U1; U2 → U3 | G2 reuses the landed G1 drawing module; U3 needs U2 search model. |
| Progress | N3 | Independent of desktop delivery; select the UI surface before coding. |
| API-dependent | V3, F1, M1 | Public OSC hooks for V3/F1; effective mouse tracking API for M1. |
| Graphics series | K1 → K2a limits → K2b temp files → K2c shared memory; K3/K4 after K1 | Keep resource limits, transport, placeholders and scheduling separate. |
| Optional | S4 after S2; N2 after N1 | Decide clear semantics / notification adapter before implementation. |

Shared-file collision rule: serialize edits to `main.c`, `vt_widget*`,
`terminal_ghostty*`, `meson.build`, and resource tables when dispatching work
concurrently. Independent feature boundaries do not make concurrent edits safe; serialize
conflicting ownership even within the same batch.

## V: Text and cursor presentation

- [ ] **V3 — OSC 22 pointer shape.** First obtain a public backend effect:
      pinned C API does not surface this OSC, and unknown APC does not help.
      Then resolve names through Xcursor, manage cursor lifetime, and compose
      application shape with hyperlink hover.
      Probe: `just probe osc-22-pointer-shape input-pointer`. This modern
      extension is not xterm's Mouse Ops reporting policy. Accept: valid and
      invalid names, default restore, hover precedence, resize and cleanup.
      Stop: no catch-all unknown-OSC observer or new permission family.
      TDN: `osc-22-pointer-shape`.
      Probe gap: the current case cycles known names only; add an invalid-name
      scenario and verify hyperlink-hover precedence externally.

## S: Shell integration

- [ ] **S4 — Optional clear-to-prompt.** Depends on S2. Define separately
      whether this means viewport movement or history deletion before coding.
      Supporting fixture: `just probe osc-133-prompt-marks shell-prompts` . Accept: explicit
      action, active-screen correctness,
      no accidental deletion of the current input. Stop: no reset semantics
      changes. If core cannot supply the chosen operation, record upstream ask.
      TDN: `ui-clear-to-prompt`.
      Probe gap: no case mapped to this slug yet. Add a clear-action scenario
      after defining the semantics; prompt markers alone do not verify clearing.

## N: Notifications

- [ ] **N2 — Optional desktop notification adapter.** Depends on N1 effect.
      Choose configured executable or optional libnotify Meson feature; keep
      the default build dependency-free.
      Supporting fixture: `just probe osc-777-notification notifications-urgency`
      plus a fake sink.
      Accept: title/body passed as data, unavailable adapter, bounded/rate-
      limited delivery and child cleanup. Stop: no hand-written D-Bus client.
      TDN: `notification-desktop-delivery`.
      Probe gap: no case mapped to this slug yet. Add delivery/fake-sink coverage;
      the existing urgency fixture cannot establish adapter behavior or limits.

- [ ] **N3 — Progress display.** Wire `OPT_PROGRESS_REPORT` for OSC 9;4;
      choose an indicator consistent with the Athena UI (scrollbar or title).
      Probe: `just probe ui-progress-indicator notifications-progress` . No Ops family. Accept:
      normal/error/indeterminate/
      paused/clear states, bounds, reset/exit, and no corruption of title stack
      or title reports. Stop: no launcher protocol or notification dependency.
      TDN: `osc-9-4-progress`, `ui-progress-indicator`.

## G: Procedural glyphs

- [ ] **G2 — Braille and Powerline.** Extend the `src/box_glyphs.c` planner
      and `XtpBoxGlyphCodepoint` range that G1 landed.
      Probe: `just probe text-braille-drawing text-glyphs`; repeat via
      `just probe text-powerline-drawing text-glyphs` . Add deterministic dot/triangle pixel
      tests at odd cell sizes.
      Define supported codepoint ranges and per-family fallback explicitly.
      Stop: no wholesale substitution of symbol fonts.
      TDN: `text-braille-drawing`, `text-powerline-drawing`.

## U: Selection polish and search

- [ ] **U1 — Copy-highlight flash.** Add duration/color resources, timer and
      overlay triggered by successful user copy. Owner: selection/drawing.
      Probe: `just probe selection-copy-feedback selection-copy` . No Ops family. Accept: exact
      paste contents unchanged,
      expiry, repeated copy, selection loss, redraw/resize, zero duration,
      synchronized-output hold and widget teardown. Stop: no clipboard policy
      changes or flash triggered by an application's OSC 52 write.
      TDN: `selection-copy-feedback`.

- [ ] **U2 — Search model and navigation.** Add bounded incremental search
      across logical scrollback lines, match ranges and next/previous APIs.
      Owner: backend/history helper with focused tests.
      Supporting fixture: `just probe search-scrollback-literal selection-search`
      supplies input data; no interactive search exists until U3. Accept: Unicode,
      soft wraps, eviction, cancellation and responsiveness under PTY output.
      Stop: no regex engine or Athena overlay in this chunk.
      TDN: `search-scrollback-literal`.
      Probe gap: fixture only until U3 provides the interactive UI. Prove U2
      through model tests; do not count printed search text as acceptance.

- [ ] **U3 — Search UI and bindings.** Depends on U2. Add Athena overlay,
      incremental query, next/previous, Enter copies match to PRIMARY, Escape
      restores viewport.
      Probe: `just probe ui-search-overlay selection-search`. No Ops family. Accept: keyboard
      focus, empty/missing matches, wrapped highlight, input while output
      arrives, resize, selection ownership and viewport restoration.
      TDN: `ui-search-overlay`, `search-copy-match`.

## K: Kitty graphics (separate series)

- [ ] **K1 — Static inline image rendering.** Establish placement iteration,
      image upload/cache ownership, clipping, scroll/resize/repaint, and a
      bounded storage resource. Owner: separate renderer module and backend
      placement adapter.
      Probe: `just probe apc-kitty-static-images graphics-static` (inline RGBA only). No existing
      xterm Ops family. Accept: 2x2 quadrant colors, placement deletion,
      scrolling, resize, reset, alternate screen, opacity and memory limits.
      Stop: no file/shared-memory mediums, placeholders or animation.
      TDN: `apc-kitty-static-images`.

- [ ] **K2 — Media and byte limits (three review slices).** Depends on K1;
      preserve its safe default storage bound throughout. Finish limits before
      enabling transport. Keep temp-file/shared-memory mediums opt-in and
      disabled by default until validated. Do not mark K2 complete after K2a.
      Supporting fixture: `just probe apc-kitty-static-images graphics-static`.
      Probe gap: that case exercises inline RGBA only; none of the K2 slugs has
      a dedicated case yet. Add the matching Go cases in the slices below.
      TDN: `kitty-graphics-storage-limit`, `kitty-graphics-command-limit`,
           `apc-kitty-temp-file-transfer`, `apc-kitty-shared-memory`.

      1. **K2a — Limits and failure paths.** Wire
         `OPT_KITTY_IMAGE_STORAGE_LIMIT` and `OPT_APC_MAX_BYTES_KITTY` to the
         intended resources; test over-budget/chunked input, failed decode,
         eviction and cleanup using inline data first. Add explicit size-limit
         cases for `kitty-graphics-storage-limit` and `kitty-graphics-command-limit`.
         Stop: no new transport.
      2. **K2b — Temporary-file transport.** After K2a, enable the opt-in
         temporary-file path, preserving limits and validating input ownership,
         missing files, failure and deletion/lifetime rules. Add a temp-file
         case for `apc-kitty-temp-file-transfer`. Stop: no shared memory.
      3. **K2c — Shared-memory transport.** After K2a/K2b, validate opt-in
         shared-memory ownership, sizes, missing inputs and cleanup under the
         same limits. Add a case for `apc-kitty-shared-memory`.
         Stop: no placeholders or animation.

- [ ] **K3 — Unicode placeholders.** Depends on K1. Map core placeholders to
      placements while preserving cell attributes and history. Extend the Go graphics family
      with a dedicated placeholder case. Accept: combining encodings, wrapping,
      erasure, clipping, selection and scrollback. No animation dependency.
      TDN: `apc-kitty-unicode-placeholders`.
      Supporting fixture: `just probe apc-kitty-static-images graphics-static`.
      Probe gap: no placeholder case exists; add one mapped to the K3 slug.

- [ ] **K4 — Animation scheduling.** Depends on K1. Dedicated image timer
      independent of PTY activity. Extend the Go graphics family with a dedicated animation case.
      Accept: idle progression, pause/delete/reset, deadlines, hidden windows,
      synchronized-output interaction and timer cleanup. Do not busy-poll.
      TDN: `apc-kitty-animation`.
      Supporting fixture: `just probe apc-kitty-static-images graphics-static`.
      Probe gap: no animation case exists; add one mapped to the K4 slug.

## Permission completion and upstream seams

- [ ] **M1 — Full Mouse Ops exceptions.** Reuse live allowMouseOps gate;
      implement xterm's disallowedMouseOps names/wildcards/negation after
      exposing the effective tracking mode from the encoder.
      Supporting fixture: `just probe dec-mode-1000-mouse input-mouse --mode 1000 --seconds 60`
      (repeat with `--mode 9` , `--mode 1002` , `--mode 1003` , plus focus); extend for
      supported encodings. Accept:
      requested mode bits versus effective tracking, named exceptions, live
      transition during drag and Shift selection override. Do not guess the
      active tracking mode by prioritizing several simultaneously set bits.
      Locator/unsupported modes must remain honestly classified.
      TDN: `policy-mouse-ops-exceptions`.
      Probe gap: existing input capture supports selected modes, not a complete
      exception/encoding matrix. Extend it and map this policy slug for M1.

- [ ] **F1 — OSC 50 Font Ops.** Obtain public OSC 50 callback first, then
      connect prepared `XtpFontOps` policy, resource values and disabled menu
      entry.
      Probe: `just probe osc-50-font-query font-query`;
      `just probe osc-50-font-set font-set --font fixed` (persistent; restore font manually).
      Accept: GetFont/SetFont
      separately and together; live toggle; deny-list exceptions; invalid
      names; query bytes; font reload/geometry lifetime. Stop: no new escape
      parser or implementation via the APC-only unknown callback.
      TDN: `osc-50-font-set`, `osc-50-font-query`, `policy-font-ops`.
