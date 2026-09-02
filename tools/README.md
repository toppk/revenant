# Maintainer tools

Human-run terminal verification programs are named
`probe-<feature>.<extension>`:

- `probe-color.sh`
- `probe-colors.py`
- `probe-osc8.sh`
- `probe-emoji.py`
- `probe-fonts.py`
- `probe-keymodes.py`

See [`docs/reference/probes.md`](../docs/reference/probes.md) for their scope
and usage. Other files in this directory are build, import, release, or profiling
utilities (`release-notes` renders a `CHANGELOG.md` entry into the GitHub
release body; see `docs/maintainers/releasing.md`) rather than interactive terminal probes. Automated fixtures remain
under `tests/`.

`probe-color.sh` is the compact SGR sampler. `probe-colors.py` adds OSC 4
palette queries, named palette installation, and an `-xrm` spawn harness for
comparing resource-driven palettes across terminals.
