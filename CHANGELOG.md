# Changelog

Newest release first. Development changes collect under the predicted next
version. A release commit dates that entry, links the commits it describes,
opens the next development entry, and advances the source version. Release
artifacts take their version from the tag, not from the development version in
`meson.build`.

## 0.7.0 — Unreleased

### Features

- Add the scrollback search model the upcoming search bar will use. It finds
  literal text across soft-wrapped lines in bounded steps, and its matches
  stay on their text through scrolling, eviction and resizing. There is no
  user-visible change yet.
- Flash text after copying it. The new `copyFlashDuration` resource, in
  milliseconds and 0 (off) by default, briefly marks the cells a selection
  gesture has just copied to PRIMARY or CLIPBOARD. `copyFlashColor` paints
  them on a chosen color; without it they show their unselected colors for
  the flash. The copied bytes and selection ownership are unchanged. A new
  selection, another client taking the selection, or an application
  replacing it ends the flash at once, a repeated copy starts a new one,
  application OSC 52 writes never flash, and a synchronized-output batch
  never shows a partial flash.
- Draw box-drawing and block-element characters from the cell geometry.
  U+2500 through U+259F are rasterized as exact rectangles inside their own
  cell, so lines, corners, tees, double lines, dashes, arcs, diagonals,
  half and eighth blocks, quadrants and shades join pixel-for-pixel with
  their neighbors at every font size, in bold, under inverse video and
  selection, and never spill into the next cell. With Xft the primary face
  keeps its own glyphs and the procedural drawing is used only for
  characters the face lacks; the bitmap path, which previously showed `?`,
  always draws them. The new `forceBoxChars` resource (`+fbx` on, `-fbx` off), the
  font menu's Line-Drawing Characters entry, and the `set-font-linedrawing()`
  action force the procedural drawing for the whole range, and switching
  it off returns to the font's glyphs immediately. Every key is now decided
  one event-loop tick after Xt dispatches it, so a key bound to any local
  action, not only the default prompt gestures, is kept from the
  application together with its release.
- Draw braille and Powerline separators from the cell geometry. Every
  braille pattern, U+2800 through U+28FF, is drawn as equal square dots in
  two columns of four, and U+E0B0 through U+E0BF, the Powerline arrows,
  half circles, slants and their thin outlines, span the full cell height
  so segments meet their neighbors' colors without a seam. They follow the
  box-drawing rules: Xft keeps the primary face's own glyphs, the bitmap
  path and `forceBoxChars` always draw them, and bold, inverse video,
  selection and the block cursor color them like text. Other Powerline and
  Nerd Font symbols, such as the branch icon U+E0A0 and the flame
  separators, stay with the font, as does braille in a cell too narrow for
  a visible dot.
- Pipe the last command's output to a helper. The new `pipeCommandOutput`
  resource names a shell command, unset by default, and the new
  `pipe-command-output()` action (Ctrl+Shift+G) finds the most recently
  completed prompt through its OSC 133 marks, extracts exactly that
  command's output with wrapped lines joined and Unicode intact, and writes
  it to the helper's standard input from the event loop without blocking.
  The helper runs in the directory the shell last reported with OSC 7 when
  one is known. Output is data only: nothing in it is ever executed. An
  unset resource, a command that wrote nothing, a helper that stops reading,
  and exiting while the helper still runs are all handled and logged.
- Navigate between shell prompts. Ctrl+Shift+Up and Ctrl+Shift+Down run the
  new `previous-prompt()` and `next-prompt()` translation actions, which move
  the viewport to the start of the previous or next prompt that a shell has
  marked with OSC 133, through scrollback, across wrapped and continuation
  prompt lines, and after resize reflow. A prompt inside the live area keeps
  the live view, a screen without history (the alternate screen) does nothing,
  and both actions accept a repeat count. Prompt positions come from an
  index of OSC 133 marks, so a deep history without prompts costs nothing to
  search. Binding the keys to xterm's `insert-seven-bit()` restores their
  delivery to applications. The backend also exposes the core's cell-exact
  command-output selection and its text for later output extraction.
- Raise the X11 urgency hint for application notifications. An OSC 9 or
  OSC 777 `notify` request that arrives while the window is unfocused sets
  `XUrgencyHint` in WM_HINTS, leaving every other hint field alone, and the
  next focus-in clears it; a request while focused only logs. Title and body
  are recorded in the `-debug` log. No desktop notification is delivered,
  no permission setting applies, and no D-Bus or helper process is involved.
- Log unsupported Application Program Commands. Every completed APC that
  libghostty does not implement (anything other than Kitty graphics and the
  glyph protocol) is recorded under `-debug` as `unknown APC ignored` with
  up to 256 payload bytes and a truncation flag; nothing is answered and
  surrounding output is unaffected. Unsupported OSC and CSI controls are
  still not visible, which remains an upstream libghostty limitation.
- Answer device-attribute queries with an audited identity instead of
  libghostty's default. DA1 reports `CSI ? 62 ; 6 ; 21 ; 22 c` (VT220 level
  with selective erase, left/right margins, and ANSI color, each backed by a
  rendered-cell test), DA2 reports `CSI > 1 ; Pv ; 0 c` with `Pv` derived
  from the Revenant version, and DA3 keeps the all-zero unit ID. Sixel,
  ReGIS, 132-column, locator, and rectangular-editing codes are not claimed;
  XTVERSION still names the product. The manual probe decodes the replies.
- Track the shell's working directory from OSC 7 reports. The `file://` URI
  is percent-decoded and kept only when it names an existing local directory
  on this host; reports for other hosts, other URI schemes, malformed escapes,
  or missing directories clear the retained path instead, as do reports
  longer than libghostty's 2048-byte OSC limit. Nothing acts on the path
  yet, and the terminal's own working directory never changes.
- Add the `termName` resource and `-tn` option. The configured name (default
  `xterm-256color`) becomes the child's `TERM` and the answer to an XTGETTCAP
  `TN` query, so the two can no longer disagree; the reply still obeys Tcap
  Ops, and `-report-config` shows the effective name.
- Answer ENQ with xterm's `answerbackString` resource. The string is sent
  verbatim for every ENQ (0x05) the application writes; the default is empty
  and sends nothing.
- Honor the startup cursor shape resources. `cursorUnderLine` (`-uc`) starts
  with an underline cursor and `cursorBar` (`-barc`) with a bar, underline
  winning when both are set. An application's DECSCUSR request overrides the
  startup shape, and `CSI 0 SP q` or a full reset returns to it; the blink
  policy is untouched.
- Activate **Allow Mouse Ops** and **Allow Tcap Ops** in the Ctrl+right-click
  menu. Mouse Ops gates mouse/focus reporting; Tcap Ops gates XTGETTCAP
  replies with `disallowedTcapOps` exceptions. Prepare Font Ops resources and
  policy helpers while keeping its menu entry disabled until OSC 50 exists.
  Add `probe-features.py` with 18 manual fixtures and a maintainer dispatch
  guide for the remaining feature work.
- Paint colored underlines. SGR 58 selects an indexed or 24-bit underline
  color for every underline style, SGR 59 returns to the text color, and the
  color survives inverse video, selection, and translucent backgrounds in
  both the bitmap and Xft renderers.
- Render dynamic colors. OSC 10, 11, and 12 now repaint the default
  foreground, background, and cursor immediately, OSC 110, 111, and 112
  restore the configured X resources, and queries report the displayed
  colors. Explicit SGR colors, reverse video, and opacity keep their rules.
- Report the color scheme: `CSI ? 996 n` answers light or dark from the
  displayed background, and mode 2031 sends `CSI ? 997 ; Ps n` whenever a
  color change flips the scheme.
- Activate **Allow Color Ops** in the Ctrl+right-click menu, with
  `allowColorOps` defaulting to true, and honor xterm's `disallowedColorOps`
  list when it is off: `SetColor` gates OSC 10-19 sets and 110-119 resets,
  `GetColor` their queries, and `GetAnsiColor` OSC 4/5 queries, while
  ordinary palette writes stay ungated. Add `tools/probe-dynamic-colors.py`
  for manual foreground, background, cursor, and scheme checks.

- Activate **Allow Title Ops** in the Ctrl+right-click menu, with the
  `allowTitleOps` resource defaulting to true. Turning it off blocks
  application title changes and applying saved labels on pop; title reports
  and stack permissions remain controlled separately by Window Ops.
- Support the XTWINOPS title stack and title reports. `CSI 22 ; Ps t` saves
  and `CSI 23 ; Ps t` restores the window title and icon name through xterm's
  ten-entry ring, including direct slot access; `CSI 20 t` and `CSI 21 t`
  report the icon name and title as `OSC L` / `OSC l` replies. All four
  consult the Window Ops policy (`PushTitle`, `PopTitle`, `GetIconTitle`,
  `GetWinTitle`, or xterm's numbers 20-23) and the live **Allow Window Ops**
  toggle; xterm's defaults leave the stack available and the reports silent.
- Support OSC 52 selection access with xterm's permission model. Applications
  can set or clear `CLIPBOARD`, `PRIMARY`, or the `SELECT` name, and query them,
  through the existing X11 selection machinery. The new `allowWindowOps` and
  `disallowedWindowOps` resources gate `SetSelection` and `GetSelection` with
  xterm's defaults, which leave OSC 52 disabled; `maxStringParse` bounds the
  encoded payload. A denied query stays unanswered, as in xterm. The
  Ctrl+right-click **Allow Window Ops** toggle changes selection permissions
  immediately and restores the configured restrictions when turned off.
- Honor synchronized output (DEC private mode 2026): dirty updates are held
  while an application batches a redraw and painted once it releases the mode,
  so batched redraws no longer tear. A one-second timeout releases and resets
  a batch the application never closes. An expose during a hold repaints the
  last complete frame; a resize repaints the current state at the new grid
  while keeping the mode set and the pending update intact.

### Tools

- Add a standalone Go probe with 53 command/TUI cases, arrow-key and mouse
  navigation, named settings, scrollable help, TDN feature references,
  JSON evidence, shared terminal cleanup, and migrated font/emoji samples and
  palettes. `just probe` builds it as needed; `just test-probe` checks decoders
  and real PTY behavior. Assessment prompts are opt-in with `--assess`; emoji
  tests have individual sections, a mode-2027 comparison and Run all. Keep the
  original scripts for comparison. TDN feature slugs now open scenario menus,
  with stable case IDs, curated breadcrumbs, and entry commands on TDN pages.
  Page dense samples for 80x24, remove redundant completion pauses, and add
  clickable ancestors plus selectable test locations for reporting problems.
  Standardize inspection on Space/Enter to continue and q/Escape to exit a test
  with cleanup; add F2 selection mode for copying menu text directly;
  document the input-capture and text-editor exceptions.

### Documentation

- Give TDN features stable identifiers and specification references. Store
  compatibility in independently maintained terminal YAML files, and generate
  feature pages, comparison filters, profile tables, and a JSON export from
  the same data. Preserve imported claims as unverified and retain evidence.

### Bug fixes

- Map `-geometry` to the application shell only, as the X Toolkit does, and
  keep popup menus from inheriting a loose `*geometry` value; a terminal
  started with `-geometry` no longer shows tiny, unusable menus.

## 0.6.1 — 2026-09-06

A focused patch release: opacity can now be adjusted from Revenant's default
opaque start whenever the X server and compositor support transparency.

### Bug fixes

- Select a compositor-backed ARGB visual independently of the initial opacity,
  so the Main Options slider remains usable at 100%; visibly grey it out only
  when transparency is unavailable or explicitly disabled with
  `backgroundOpacity: disabled` or `-opacity disabled`.
  ([c4218a3](https://github.com/toppk/revenant/commit/c4218a31f4472826c0212c2c1cee7762d0cddc62))

## 0.6.0 — 2026-09-06

A smaller quality-of-life release: visible HTTP(S) text is now actionable, and
Shift-hover makes terminal hyperlinks easier to find and distinguish.

### Features

- Detect visible HTTP and HTTPS URLs across soft-wrapped rows and open them
  through the existing Shift+Button-1 hyperlink gesture, excluding trailing
  sentence punctuation and preserving explicit OSC 8 precedence.
  ([3e472b0](https://github.com/toppk/revenant/commit/3e472b048148dd02c33941e83bb02b2d1bba8161))
- Give Shift-hovered OSC 8 and detected links a hand pointer and a visible
  underline transition: unstyled links become single-underlined, while
  application single underlines become double-underlined.
  ([5f9255e](https://github.com/toppk/revenant/commit/5f9255ecda52ec75369b9c0004350364668fc1b7))

### Bug fixes

- Keep Shift-hover feedback on the individual OSC 8 label when multiple
  visible labels point to the same destination.
  ([5f9255e](https://github.com/toppk/revenant/commit/5f9255ecda52ec75369b9c0004350364668fc1b7))

## 0.5.0 — 2026-09-02

Fonts resolve through xterm's complete fallback chain, the cursor and ANSI
palette follow xterm's resources, the command line parses like xterm's, and
release builds become reproducible with Arch packaging and an offline setup
audit for a readable first run.

### Features

- Resolve fonts through shaped primary, semantic, explicit, user, and system
  fallback roles with real same-family styles, Han and IVS routing,
  deterministic tofu, metric normalization, route caching and reporting, and
  transactional reloads.
  ([2b4d1ba](https://github.com/toppk/revenant/commit/2b4d1ba8e776723d256592864be91db5456d3c31),
  [67077fb](https://github.com/toppk/revenant/commit/67077fbb3198c0c238efa79a150baeb85191769d))
- Follow xterm's cursor-blink policy: the four-value `cursorBlink` resource,
  `cursorBlinkXOR`, `-/+bc`, the VT Options toggle, and application blink
  requests through DECSCUSR.
  ([8d214bc](https://github.com/toppk/revenant/commit/8d214bcfbe8ebf779528d3c08244420ac280d5cf))
- Configure the ANSI palette from `color0` through `color15` with xterm's
  compiled defaults, and accept OSC 4 and OSC 104 palette operations by
  default.
  ([af03695](https://github.com/toppk/revenant/commit/af03695ba49d465d5d554d57dbda7d6a753ea813))
- Parse the command line like xterm: unambiguous option prefixes, rejection of
  unknown options before the display opens, xterm's `-help` and `-version`
  wording, plus GNU-style `--help` and `--version`.
  ([156f3e5](https://github.com/toppk/revenant/commit/156f3e5558a2bf45c1da4126e970f97023072429),
  [bc27724](https://github.com/toppk/revenant/commit/bc277249656b98051ca6630b85e4433e0400fc62))
- Install a `revenant(1)` manual page and present the documentation site as
  an xman-style manual that shares the same action and resource reference.
  ([d73e90f](https://github.com/toppk/revenant/commit/d73e90f43d89667bc500da62fe7b562d4bb16b86),
  [a253f0c](https://github.com/toppk/revenant/commit/a253f0c862a52e946e4f49bf26cc2d766b4c1c88),
  [b5455b9](https://github.com/toppk/revenant/commit/b5455b9c024af6a71aafce8076849936c0dc75b6),
  [6a01a36](https://github.com/toppk/revenant/commit/6a01a36267ca34c8c4feec24928cb9362d893c4d))
- Publish an Arch Linux package alongside the tarballs, Debian package, and
  Fedora RPM.
  ([3a57611](https://github.com/toppk/revenant/commit/3a576118d5c499d58a508c6330765d82e2db113a))
- Add `-welcome`, a read-only audit of X resources, display readability,
  installed fonts and tools, with distribution-aware setup suggestions and a
  redacted support summary.
  ([dd462b4](https://github.com/toppk/revenant/commit/dd462b4b6720fee1d9bdc4ffe75cb38c43ee58c8))
- Set `TERM_PROGRAM` and `TERM_PROGRAM_VERSION` in child sessions so diagnostics
  can identify when Revenant is the host terminal.
  ([dd462b4](https://github.com/toppk/revenant/commit/dd462b4b6720fee1d9bdc4ffe75cb38c43ee58c8))

### Bug fixes

- Drain a bounded burst of available PTY output and paint once, so a
  rapid-update refresh split by the kernel no longer flashes between erased
  and replaced content.
  ([3ea6f39](https://github.com/toppk/revenant/commit/3ea6f398352817365720418dbaa8e2bce3668ca2),
  [670b3a6](https://github.com/toppk/revenant/commit/670b3a66afb5a36c598f5a1eb2857f1e921bdb99))
- Render faint text at two-thirds intensity and promote bold foreground colors
  0 through 7 to 8 through 15 as xterm does; `boldColors` and `-/+pc` control
  the promotion.
  ([93852d9](https://github.com/toppk/revenant/commit/93852d998c98b06d84f72c665405850c50982ce3))
- Keep a client-created ARGB colormap alive while display shutdown flushes Xt's
  cached color converters, avoiding `BadColor` when a report exits without
  realizing a window.
  ([dd462b4](https://github.com/toppk/revenant/commit/dd462b4b6720fee1d9bdc4ffe75cb38c43ee58c8))
- Build libghostty for explicit `x86_64-linux-gnu`/`x86_64-v3` and
  `aarch64-linux-gnu`/`baseline` targets. Earlier releases could contain
  instructions unsupported on older CPUs; the install guide and every release
  body now state the supported CPU floor.
  ([3a57611](https://github.com/toppk/revenant/commit/3a576118d5c499d58a508c6330765d82e2db113a))

### Other

- Pin Ghostty to one exact commit, rebuild it only when its inputs change, and
  share a Zig cache keyed by target, CPU, Zig version, Ghostty commit, build
  script, and runner CPU model across packages of the same architecture.
  ([3a57611](https://github.com/toppk/revenant/commit/3a576118d5c499d58a508c6330765d82e2db113a))
- Require every tarball, Debian, RPM, and Arch build to run the complete Xvfb
  and font-fixture suite without skips.
  ([3a57611](https://github.com/toppk/revenant/commit/3a576118d5c499d58a508c6330765d82e2db113a),
  [62f6d06](https://github.com/toppk/revenant/commit/62f6d066397014bc5585ab051bda35011709726d))
- Stage and cache the pinned font fixtures once, require every builder to
  restore that exact cache entry, and remove the staging toolchain from package
  builders.
  ([3a57611](https://github.com/toppk/revenant/commit/3a576118d5c499d58a508c6330765d82e2db113a))
- Validate changelog release notes before starting builds and let intermediate
  package artifacts expire after one day.
  ([3a57611](https://github.com/toppk/revenant/commit/3a576118d5c499d58a508c6330765d82e2db113a))
- Enforce the internal `xterm+` naming boundary across sources, tests, tools,
  and packaging with a branding check.
  ([c4d9150](https://github.com/toppk/revenant/commit/c4d9150f664bf53e71990c747aa3e62e26c331e8))
- Isolate font-universe types, extract the font routing lifecycle, centralize
  role policy and Unicode routing helpers, and move Ghostty selection policy
  behind a private backend header to prepare the terminal boundary for
  multiplexing.
  ([3b150c8](https://github.com/toppk/revenant/commit/3b150c8ccd3bad7c54c426b894b83b86c97cd243),
  [e0e0b22](https://github.com/toppk/revenant/commit/e0e0b22f195df94a7566fac173ea047f68345691),
  [379c725](https://github.com/toppk/revenant/commit/379c725ddb8fb5c0e3ac7bf916554aeeaaf79ef8),
  [955f3c7](https://github.com/toppk/revenant/commit/955f3c72dce6e6adcb05c07cfa23e5e5af6ea659),
  [df3de1a](https://github.com/toppk/revenant/commit/df3de1a150c7e9480fce73580a921e9b7eabb7bb),
  [1a3f4f8](https://github.com/toppk/revenant/commit/1a3f4f83d0c7d2223af4ebe0294b7b2cbc69cda4),
  [40d4e56](https://github.com/toppk/revenant/commit/40d4e56b6b1e495c639455c1c478946c7fea8a72))
- Share Xvfb harness setup across the font and keyboard suites, guard font
  routing diagnostics, and add a pixel-level bitmap regression for
  `renderFont: false`.
  ([9cc6a6d](https://github.com/toppk/revenant/commit/9cc6a6d9929592cc713b2f22755ce93391d15260),
  [ed4e40b](https://github.com/toppk/revenant/commit/ed4e40b10558bb4ca456a266c7736aec217c80a3),
  [379c725](https://github.com/toppk/revenant/commit/379c725ddb8fb5c0e3ac7bf916554aeeaaf79ef8))
- Document installation, CPU requirements, release provenance, and local
  package construction, and lead every entry page with the xterm-defaults
  caveat and `revenant -welcome`.
  ([3a57611](https://github.com/toppk/revenant/commit/3a576118d5c499d58a508c6330765d82e2db113a),
  [172d1e8](https://github.com/toppk/revenant/commit/172d1e8e07cf509acd2d458eff9365a37a3caa34))

## 0.4.0 — 2026-08-29

Emoji gain dedicated color-font routing and HarfBuzz shaping, with reproducible
fixtures guarding every supported font format.

### Features

- Route emoji through a dedicated face and render color glyphs from CBDT,
  COLRv0, COLRv1, SVGinOT, and sbix fonts.
  ([49a6ea0](https://github.com/toppk/revenant/commit/49a6ea0e198b820a4e7c75e2f21e56ac713a6be8))
- Shape complete grapheme clusters with HarfBuzz and fall through atomically
  when a face cannot compose a modifier, keycap, flag, or ZWJ sequence.
  ([1aca6ff](https://github.com/toppk/revenant/commit/1aca6ffcb06e2d5b550240dca9466090d25aac9a))
- Add negotiated Unicode grapheme widths through DEC private mode 2027 while
  retaining xterm-compatible legacy widths by default.
  ([1aca6ff](https://github.com/toppk/revenant/commit/1aca6ffcb06e2d5b550240dca9466090d25aac9a))
- Default logging to warnings; `-log` selects the severity and xterm's
  `-/+debug` aliases remain available.
  ([3e5eade](https://github.com/toppk/revenant/commit/3e5eadebd402e5bb5066a4b6468f035a07473c13))

### Bug fixes

- Honor an embedded `faceName:size=` value with the same precedence and font
  menu behavior as xterm.
  ([0ba409d](https://github.com/toppk/revenant/commit/0ba409d06f1902cbedd13f58150e3be03f0a01b2))

### Other

- Add an isolated, reproducible font-format matrix and real-ink Xvfb tests for
  routing, shaping, clipping, and fallback.
  ([662d6e8](https://github.com/toppk/revenant/commit/662d6e8950e0e22b56fc8fd68263dccc5bc73a62))
- Add changelog-driven release notes with commit-traceability checks, artifact
  sizes, and SHA-256 hashes.
  ([d0b43fd](https://github.com/toppk/revenant/commit/d0b43fde89ada3b1736ab6bd2df440415aedf309))
- Run the font fixture and Xvfb suites in release builds, pin their tooling,
  freeze libghostty to one tested commit, and make the synthetic sbix fixture
  independent of the host zlib.
  ([d0b43fd](https://github.com/toppk/revenant/commit/d0b43fde89ada3b1736ab6bd2df440415aedf309))
- Sign every release asset with a build-provenance attestation and streamline
  Debian packaging by omitting unused `.buildinfo` generation.
  ([d0b43fd](https://github.com/toppk/revenant/commit/d0b43fde89ada3b1736ab6bd2df440415aedf309))
- Document the Revenant identity, release installation, and provenance
  verification.
  ([2ef00e0](https://github.com/toppk/revenant/commit/2ef00e0d90b6f54126d0cbe0e45d869451f51f21),
  [d0b43fd](https://github.com/toppk/revenant/commit/d0b43fde89ada3b1736ab6bd2df440415aedf309))

## 0.3.0 — 2026-08-28

The project becomes Revenant and adds translucent backgrounds, desktop
integration, and complete reverse-video rendering.

### Features

- Add compositor-backed background opacity while keeping text, cursors,
  selections, and decorations opaque.
  ([3920d64](https://github.com/toppk/revenant/commit/3920d64ef7ef6a279822a644bd227006eddce0c9))
- Implement widget reverse video, terminal-wide DECSCNM, and per-cell SGR 7.
  ([2a9801b](https://github.com/toppk/revenant/commit/2a9801b43c952c3ca7a6d81bce30dbf5d0e11e6d))
- Install the desktop launcher and scalable and 256-pixel application icons.
  ([8d8ea18](https://github.com/toppk/revenant/commit/8d8ea18be30616af152d6f2f88adbd5fb99a1ae0))

### Bug fixes

- Premultiply translucent background pixels for correct compositing.
  ([a2a0d07](https://github.com/toppk/revenant/commit/a2a0d07f9aa4d6e065f6237fd078e872b56bdf01))

### Other

- Rename the project and binary to Revenant; retain `xterm+` as a compatibility
  symlink.
  ([db9b913](https://github.com/toppk/revenant/commit/db9b91304a421242a0c170cd6dd0464fde2ddbbe))
- Stabilize the opacity test and extract backend-neutral `charClass` handling.
  ([855e2f4](https://github.com/toppk/revenant/commit/855e2f49ff19234aa4439efdf06d5885eaab63fd),
  [408cc44](https://github.com/toppk/revenant/commit/408cc44a4f0af51a1ba7803bdee3051c5d161053))

## 0.2.0 — 2026-08-27

Keyboard, links, selections, resize behavior, and source ownership mature around
the libghostty-backed terminal core.

### Features

- Promote the Kitty keyboard protocol through X11, including progressive
  flags, stack operations, modifiers, composition, and event types.
  ([cb43327](https://github.com/toppk/revenant/commit/cb433272c97bb8e0d8b87c1ba414344e430803fe))
- Add safe OSC 8 hyperlink hover and activation.
  ([27542ac](https://github.com/toppk/revenant/commit/27542ac1d8f70090d12d82d66cce0ebef9579e66))
- Implement named X11 selections.
  ([dfcf2a3](https://github.com/toppk/revenant/commit/dfcf2a3e07f6dac0fb12f8f2593533fee5b83668))

### Bug fixes

- Correct modern Ctrl-key encoding and add keyboard protocol probes.
  ([dff1053](https://github.com/toppk/revenant/commit/dff105343da7f0cdc044dcd55a01ca41b927ded3))
- Fix resize-frame invalidation and add reflow diagnostics.
  ([93394d4](https://github.com/toppk/revenant/commit/93394d481eb3eb9e3016ff0c0df3b6e1d80fa2a5))
- Keep the outer `DESTDIR` out of the libghostty build.
  ([d1a1123](https://github.com/toppk/revenant/commit/d1a11231848b3a4b52cbf0b4e562c46d78c343f9))

### Other

- Split the VT widget into owned modules for input, interaction, and rendering.
  ([fcf0870](https://github.com/toppk/revenant/commit/fcf0870a03910f114dcf041aa1e607269e32e292),
  [b411a84](https://github.com/toppk/revenant/commit/b411a846b61158fed2615f9bb997f687e60df835),
  [23d36a6](https://github.com/toppk/revenant/commit/23d36a6fc38efd503bcaf7d320b231f22cc889a8))
- Simplify application setup, menu dispatch, and backend plumbing.
  ([9c67b7c](https://github.com/toppk/revenant/commit/9c67b7c63b15820e05ccef39746ab78e72ee833a),
  [9e5bb98](https://github.com/toppk/revenant/commit/9e5bb9848d0ef044426f27d4c6f65181c736c918))
- Reorganize project documentation and build artifacts around the source
  architecture.
  ([6a24822](https://github.com/toppk/revenant/commit/6a2482265cdb9fba1d4e6e479cc47ce1ed286aca),
  [2c0f560](https://github.com/toppk/revenant/commit/2c0f56007d41cbca0ff9848aeeb7a7ce0cb72670))
- Define Ghostling parity as the MVP gate and track Ghostty `main` for the 1.4
  transition.
  ([aeb9ad7](https://github.com/toppk/revenant/commit/aeb9ad726ff1eed4bca9787450488c07e77043b4),
  [9285601](https://github.com/toppk/revenant/commit/9285601743bfa1f4b4c0f648f3aaf70e58a186b7))
- Stabilize Xvfb selection tests.
  ([c8b5264](https://github.com/toppk/revenant/commit/c8b5264ea363738fb22ff4843d2f4e285590b21c))

## 0.1.0 — 2026-08-26

First tagged build of the xterm-skinned, libghostty-backed X11 terminal.

### Other

- Initial commit.
  ([0e7e0f0](https://github.com/toppk/revenant/commit/0e7e0f098e5cd44a55a0a688a658d43e4144c2fb))
