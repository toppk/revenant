# Terminal Developers Network

A reference for terminal protocols in the style of a web-platform reference:
one page per feature, with syntax, behavior, history, a compatibility table
across emulators, and a reproducible probe.

- `docs/` — the site content, built with MkDocs (`mkdocs.yml`).
- `data/` — stable feature IDs and specification references.
- `terminals/` — one independently maintained compatibility YAML file per terminal.
- `hooks/` — validation and generated MkDocs views.
- `tools/` — shell probes (`query`, `sendcsi`, `sendosc`, `sgr-sampler`,
  `testfocus`) that produce the observations recorded in the tables.

TDN currently lives inside the Revenant repository and covers terminals in
general. Its data and site can move to a separate repository, but the current
probe-catalog integration cannot move unchanged; see the boundary below.
Read [the registry guide](docs/registry.md) before adding support claims.
The same data builds the comparison page, feature pages, and terminal profiles;
unknown or imported claims remain visibly distinct from reviewed evidence.

## Probe-catalog boundary

`hooks/probe_catalog.py` currently runs from the Revenant repository root. It
reads `tdn/data/features.yaml` for feature IDs, titles, specification references
and optional sequence text, and `tdn/data/probe-navigation.json` for the
independently curated breadcrumbs and feature ordering. It validates the
navigation against registered IDs and generates
`tools/probe/data/features.json`. Revenant's `tools/probe/features.go` embeds
that checked-in JSON into the Go binary; case IDs, handlers and their feature
mappings remain in `tools/probe/cases.go` and related Go files. The generated
catalog is navigation and reference metadata, not terminal support evidence.

From the Revenant root, `just update-probe-registry` owns regeneration and
requires Python with PyYAML. `just check-tdn` runs the registry tests, including
`tdn/tests/test_probe_catalog.py`'s snapshot comparison, and the strict TDN
site build. `just test-probe` checks Go formatting/tests and the PTY-driven
probe cases; `tests/probe-go.py` also compares the binary's feature listing
with the embedded JSON. The TDN MkDocs hook imports the same catalog to show
probe commands and breadcrumbs on feature pages. Ordinary `just probe` builds
the checked-in snapshot with Go; it does not regenerate TDN data.

If TDN moves out, choose which repository owns the curated navigation and
generator, then replace their assumed `tdn/` sibling and Revenant-root paths
in `hooks/probe_catalog.py` and the TDN MkDocs hook. Give Revenant a pinned or
otherwise reproducible input/snapshot update path for
`tools/probe/data/features.json`, and move or adapt the cross-repository
freshness checks, `just` recipes and CI gates. Update links and build paths in
both sites. Until that integration is designed and tested, moving the `tdn/`
tree alone would break catalog generation and validation.

Build locally:

```sh
pip install -r requirements.txt && mkdocs serve
```

## License

TDN content and compatibility data (`docs/`, `data/`, `terminals/`) is licensed under CC BY-SA 4.0
([`LICENSE-CC-BY-SA-4.0`](LICENSE-CC-BY-SA-4.0)); the probe tools and build code (`tools/`, `hooks/`, `tests/`)
are MIT licensed ([`LICENSE-MIT`](LICENSE-MIT)). By contributing you agree to
those terms.
