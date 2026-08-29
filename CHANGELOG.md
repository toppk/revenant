# Changelog

Newest release first. Development changes collect under the predicted next
version. A release commit dates that entry, links the commits it describes,
opens the next development entry, and advances the source version. Release
artifacts take their version from the tag, not from the development version in
`meson.build`.

## 0.5.0 — Unreleased

### Other

- Document release installation and provenance verification.

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
