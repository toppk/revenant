# Dispatchable feature TODO

Monstar (`upstream/monstar`) is a reference implementation, not a promise
that the pinned libghostty C API exposes every hook. Read `HANDOFF.md` and
`docs/maintainers/dispatch.md` before starting a chunk. This tracked checklist
contains only open work; durable implementation contracts belong in the handoff
and supporting docs.

The September 2026 Ghostty/Monstar review is complete. Remaining release and
feature decisions live in the [roadmap](docs/maintainers/roadmap.md#upstream-integration-follow-ups);
they do not replace this file's compatibility IDs or expand their scope.

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

Existing IDs stay stable. A chunk with sequential review slices is complete
only after every slice lands.

| Batch | Chunks | Dependency / decision before dispatch |
| --- | --- | --- |
| Active drain | T1 → W1 → C1 → P1 → P2 | Prefer frontend-owned work that can land against the current backend. |
| Decision needed | S4 | Choose viewport movement or history deletion before implementation. |
| API-dependent | V3, F1, M1, T2, W2 | Obtain the named libghostty effects/state; do not add another parser. |
| Parked | K1 → K2a → K2b → K2c; K3/K4 after K1 | Kitty graphics is explicitly bunted during this drain. |

Shared-file collision rule: serialize edits to `main.c`, `vt_widget*`,
`terminal_ghostty*`, `meson.build`, and resource tables when dispatching work
concurrently. Independent feature boundaries do not make concurrent edits safe; serialize
conflicting ownership even within the same batch.

## T: Title and window compatibility

- [ ] **T1 — Frontend-owned Title Ops parity (three review slices).** These
      pieces need no new escape callback and should land in order. Preserve the
      tested separation between Title Ops setting permission and Window Ops
      reports/stack operations.
      Probe: extend the title family with cases mapped to each slice; retain
      `just probe csi-21-t-title-report titles-query --target both` for report
      regression.
      Accept: actual ICCCM/EWMH properties, menu/action state, resource reports,
      isolated HOME, relevant locales, and differential xterm 411 behavior.

      1. **T1a — Action and redundant-update policy.** Register
         `allow-title-ops(on|off|toggle)` against the existing live state and
         implement `sameName` without changing stack consumption.
         TDN: `action-allow-title-ops`, `resource-same-name`.
      2. **T1b — UTF-8 title surface.** Implement `utf8Title`, the
         `utf8-title` menu/action, and synchronized ICCCM `WM_NAME` /
         `WM_ICON_NAME` plus EWMH `_NET_WM_NAME` / `_NET_WM_ICON_NAME`,
         including stale-property deletion.
         TDN: `resource-utf8-title`, `x11-utf8-title-properties`.
      3. **T1c — Send-event interaction.** Make `allowSendEvents` disable the
         effective Title Ops permission and its toggle exactly as xterm does.
         TDN: `policy-title-ops-send-events`.

- [ ] **T2 — Parser-dependent Title Ops completion.** Obtain selector/raw-input
      effects for independent OSC 0/1/2 labels, title encoding modes, and the
      1000-byte normalization boundary. Then implement XTSMTITLE/XTRMTITLE and
      hex/UTF-8 input/report modes. Stop: no catch-all OSC/CSI observer.
      Supporting fixtures: `just probe csi-21-t-title-report titles-query`
      and `just probe csi-22-t-push-title titles-stack`; they exercise reports
      and stack behavior. Probe gap: add mapped cases for independent OSC
      0/1/2 input, title modes, malformed input and exact boundaries.
      TDN: `osc-0-title-icon`, `osc-1-icon-name`, `osc-2-title`,
           `resource-title-modes`, `csi-xtsmtitle`, `csi-xtrmtitle`,
           `title-modes-hex-input`, `title-modes-hex-reports`,
           `title-modes-utf8-input`, `title-modes-utf8-reports`,
           `title-input-normalization`, `title-input-byte-limit`.

- [ ] **W1 — Window Ops audit and exposed reports.** Re-audit patch 411's
      `tblWindowOps` against the pinned libghostty revision, then gate every
      operation already exposed, including CSI 14/16/18 t, through the existing
      live policy. Add exact permitted/denied reply tests and numeric/name/
      wildcard/negation coverage. Do not claim the category complete for
      operations that still lack callbacks.
      Probe: extend the window-report family for 14/16/18 and live policy.
      TDN: `policy-window-ops`, `csi-14-t-pixel-size-report`,
           `csi-16-t-report-cell-size-in-pixels`, `csi-18-t-text-size`.

- [ ] **W2 — Remaining Window Ops effects.** After W1 identifies the concrete
      API gaps, obtain callbacks for window manipulation/reports and the
      cross-family column, line, checksum, X-property and status-line controls.
      Use a window manager for stacking, minimize, maximize and fullscreen.
      Stop: preserve libghostty parser ownership and keep OSC 0/1/2 under Title
      Ops.
      TDN: `csi-1-t-de-iconify`, `csi-2-t-iconify`,
           `csi-3-t-move-window`, `csi-4-t-resize-window-in-pixels`,
           `csi-5-t-raise`, `csi-6-t-lower`, `csi-7-t-refresh`,
           `csi-8-t-resize-text-area-in-cells`, `csi-9-t-maximize`,
           `csi-10-t-fullscreen`, `csi-11-t-report-window-state`,
           `csi-13-t-position-report`,
           `csi-15-t-report-screen-size-in-pixels`,
           `csi-19-t-report-screen-size-in-cells`, `dec-mode-3-deccolm`,
           `dec-mode-40-allow-3-to-resize`, `csi-decslpp`, `csi-decsnls`,
           `csi-decrqcra`, `csi-xtchecksum`, `osc-3-x-property`,
           `csi-decsasd`, `csi-decssdt`, `osc-52-multiple-targets`,
           `osc-52-default-targets`, `osc-52-cut-buffers`,
           `osc-52-invalid-base64-clear`, `osc-52-reply-target`.

## C: Remaining Color Ops policy

- [ ] **C1 — Color Ops and `allowSendEvents`.** Apply xterm's effective
      permission interaction to the existing live Color Ops gate and menu,
      without changing per-item filtering or palette writes. Add startup,
      live-toggle and mixed-list regressions.
      Probe: extend the dynamic-color policy case for `allowSendEvents`.
      TDN: `policy-color-ops-send-events`.

      Keep `policy-kitty-color-ops` and
      `osc-dynamic-colors-reverse-video` recorded as core/API differences; do
      not fold them into this frontend-owned slice.

## P: Process and session behavior

- [ ] **P1 — Session transcript logging.** Implement `-/+l`, `-lf`,
      `logFile`, `logInhibit`, and the `logging` menu action as one safe,
      nonblocking PTY-output tee. Keep it separate from diagnostic `-log`,
      define file ownership/permissions and failure behavior, and cover live
      enable/disable plus teardown.
      TDN: `logging-session-transcript`.
      Probe gap: add a native session-log case and verify the file externally.

- [ ] **P2 — Remaining option-driven process behavior.** Implement and review
      login-shell invocation, hold-after-exit, wait-for-map startup, terminal
      mode resources, and PTY message permission as independent slices. Do not
      disturb the completed `termName`/TERM contract or pre-X option scanner.
      TDN: `startup-login-shell`, `startup-hold-after-exit`,
           `startup-wait-for-map`, `resource-terminal-modes`,
           `pty-message-permission`.
      Probe gap: add one native case per slice that has observable child state.

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

## K: Kitty graphics (parked)

These chunks remain honest Missing/Partial registry work, but are not part of
the current implementation drain. Reopen K1 explicitly before dispatching any
of K2–K4.

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
