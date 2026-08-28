# xterm+

xterm+ is an X11 terminal emulator that preserves xterm's Xt/Athena interface
and X resource contract while using `libghostty-vt` as its terminal core.

For an xterm user, the goal is a familiar window: the `XTerm` resource class,
the `vt100` widget, Ctrl+mouse-button menus, configured bitmap and Xft font
slots, and the same `.Xresources` vocabulary. Under that interface, xterm+
adds a modern terminal engine with resize reflow, 24-bit color, current mouse
and focus protocols, robust grapheme state, and the Kitty keyboard protocol.

> **Project status:** xterm+ is early and not yet a complete xterm replacement.
> Unsupported menu entries remain visible but insensitive, and accepted but
> unwired resources are reported honestly. See the
> [roadmap](docs/maintainers/roadmap.md) and
> [Ghostling parity gate](docs/compatibility/ghostling-parity.md) for the
> current boundary.

## Why xterm+

xterm+ is deliberately judged against two product bars:

- **xterm compatibility:** xterm patch 410 defines the visible X11 contract —
  resources, menus, translations, geometry, and font behavior.
- **Modern terminal capability:** Ghostling is the minimum integration floor
  for features already supplied by `libghostty-vt`.

xterm compatibility determines how features are presented; it should not make
xterm+ less capable than a thin libghostty host. Intentional differences are
recorded in the [xterm differences ledger](docs/compatibility/drift.md).

The largest default difference today is keyboard encoding. xterm+ follows
libghostty's fixterms behavior, distinguishing Ctrl+I from Tab, Ctrl+M from
Enter, and Ctrl+[ from Escape without application opt-in. Read the
[keyboard compatibility warning](docs/compatibility/keyboard-input.md) before
treating xterm+ as a transparent replacement.

## Build and run

Releases provide Linux archives and Debian and RPM packages. The
[installation guide](docs/getting-started/install.md) lists dependencies and
packaging details. To build the full terminal from a checkout:

```sh
tools/fetch-libghostty
meson setup build-ghostty -Dlibghostty=enabled
meson compile -C build-ghostty
meson test -C build-ghostty
./build-ghostty/xterm+
```

Without arguments, xterm+ starts `$SHELL` (falling back to `/bin/sh`) and sets
`TERM=xterm-256color`. Use xterm's trailing `-e` form to run another command:

```sh
./build-ghostty/xterm+ -e sh -lc 'printf "hello from xterm+\n"; exec "$SHELL"'
```

For UI-only work, a stub build avoids Zig and does not start a PTY:

```sh
meson setup build -Dlibghostty=disabled
meson compile -C build
meson test -C build
./build/xterm+
```

## First things to know

- Ctrl+button 1, 2, and 3 open the Main Options, VT Options, and VT Fonts
  menus.
- Shift+Page Up and Shift+Page Down navigate saved history.
- Mouse selection owns X11 `PRIMARY` by default; button 2 and Shift+Insert
  paste it. X11 also has a separate `CLIPBOARD` selection.
- Hold Shift over an OSC 8 link to underline it, then Shift+button 1 to open an
  HTTP or HTTPS target.
- Existing xterm resources can apply directly because the application class,
  instance, and terminal widget identity are compatible.

The [first-run guide](docs/getting-started/first-run.md) covers menus, fonts,
and basic controls. Continue with:

- [X resources, explained](docs/configuration/xresources.md), for readers new
  to X11 configuration;
- [configuring xterm+](docs/configuration/xterm-plus.md), for supported fonts,
  colors, scrollback, selection, and cursor policies;
- [copy and paste on X11](docs/usage/copy-paste.md), for `PRIMARY`,
  `CLIPBOARD`, and cut buffers;
- [OSC 8 hyperlinks](docs/usage/hyperlinks.md), for interaction and launch
  policy;
- [diagnostics](docs/reference/diagnostics.md), for `-report-config`, debug
  logs, regression helpers, and profiling;
- [interactive probes](docs/reference/probes.md), for comparable color,
  keyboard, emoji, and hyperlink fixtures.

The published documentation also includes the independently licensed
[Terminal Developers Network](tdn/README.md), a protocol reference with
reproducible terminal probes.

## Maintaining xterm+

The normal maintainer loop is:

```sh
tools/fetch-libghostty     # first checkout, or deliberate upstream refresh
just check-all             # formatting, GCC, Clang, stub, Xvfb, and docs
```

Focused commands are available when iterating:

```sh
just format               # rewrite C sources and test helpers
just format-check         # verify formatting only
just test                 # GCC + Clang libghostty builds and the stub build
just check                # strict-build both documentation sites
just serve                # preview landing page, manual, and TDN together
```

Human-run fixtures use the discoverable `tools/probe-<feature>.<ext>` naming
convention and are cataloged in the
[probe reference](docs/reference/probes.md).

Durable maintainer documentation lives under `docs/maintainers/`:

- [architecture and source boundaries](docs/maintainers/architecture.md);
- [roadmap and architecture guardrails](docs/maintainers/roadmap.md);
- [upstream checkout and update policy](docs/maintainers/upstream.md);
- [C style and review conventions](docs/maintainers/style.md);
- [release and packaging procedure](docs/maintainers/releasing.md).

The repository's main areas are intentionally unsurprising:

| Path | Purpose |
| --- | --- |
| `src/` | Xt/X11 application, VT widget, PTY layer, and terminal backends |
| `tests/` | Backend self-tests and Xvfb integration coverage |
| `tools/` | Fetch, profiling, maintenance, and interactive probe utilities |
| `docs/` | xterm+ user and maintainer manual |
| `compat/` | Machine-readable xterm patch-410 compatibility catalogs |
| `data/` | Reference app-default material |
| `tdn/` | Independently licensed terminal-protocol documentation |
| `packaging/` | Release archives, Debian packages, and RPM support |

## Source and licenses

xterm+ does not compile or embed xterm's terminal implementation. Its
PTY/event loop, Xt widget, X11 renderer, diagnostics, and libghostty adapter
are project code; `libghostty-vt` supplies parsing and terminal state.

The repository does carry xterm-derived compatibility data, app-defaults,
names, defaults, and bindings. Those portions are covered by the permissive
xterm license in [`LICENSES/xterm.txt`](LICENSES/xterm.txt). Ghostty is fetched
under ignored `upstream/`, built as a separate dependency, and remains under
its own license. The [architecture guide](docs/maintainers/architecture.md)
describes these boundaries in more detail.
