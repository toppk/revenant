# Maintainer tools

Human-run terminal verification programs are named
`probe-<feature>.<extension>`:

- `probe-color.sh`
- `probe-osc8.sh`
- `probe-emoji.py`
- `probe-fonts.py`
- `probe-keymodes.py`

See [`docs/reference/probes.md`](../docs/reference/probes.md) for their scope
and usage. Other files in this directory are build, import, or profiling
utilities rather than interactive terminal probes. Automated fixtures remain
under `tests/`.
