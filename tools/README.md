# Maintainer tools

Human-run terminal verification programs are named
`probe-<feature>.<extension>`:

- `probe-color.sh`
- `probe-colors.py`
- `probe-osc8.sh`
- `probe-emoji.py`
- `probe-fonts.py`
- `probe-keymodes.py`
- `probe-sync.py`
- `probe-clipboard.py`
- `probe-titles.py`
- `probe-dynamic-colors.py`
- `probe-features.py` (`--list` lists 18 dispatch fixtures)

See [`docs/reference/probes.md`](../docs/reference/probes.md) for their scope
and usage. Other files in this directory are build, import, or profiling
utilities rather than interactive terminal probes. Release helpers live under
`packaging/`; `packaging/release-notes` renders a `CHANGELOG.md` entry into the
GitHub release body (see `docs/maintainers/releasing.md`). Automated fixtures
remain under `tests/`.

`probe-color.sh` is the compact SGR sampler. `probe-colors.py` adds OSC 4
palette queries, named palette installation, and an `-xrm` spawn harness for
comparing resource-driven palettes across terminals.

`demo.sh` is a compact screenshot-oriented scene showing emoji, colored
underline styles, OSC 8 labels, and an auto-detected URL. It is illustrative,
not a probe or automated test.

For pending feature work, run `python3 tools/probe-features.py --list`, then
`python3 tools/probe-features.py SUBCOMMAND --help`. For example,
`just probe-features tcap --cap Co` checks capability replies and
`just probe-features mouse --seconds 60` captures mouse/focus events while
you toggle their Ops entries with Ctrl+right-click. Font Ops is prepared but
insensitive until OSC 50 is implemented. The other fixtures seed or request
pending behavior; their presence does not claim feature support. See the
[dispatch guide](../docs/maintainers/dispatch.md) for implementation boundaries.
