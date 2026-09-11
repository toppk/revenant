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

TDN currently lives inside the Revenant repository, but it is about terminals
in general and is laid out so it can move to its own repository unchanged.
Read [the registry guide](docs/registry.md) before adding support claims.
The same data builds the comparison page, feature pages, and terminal profiles;
unknown or imported claims remain visibly distinct from reviewed evidence.

Build locally:

```sh
pip install -r requirements.txt && mkdocs serve
```

## License

TDN content and compatibility data (`docs/`, `data/`, `terminals/`) is licensed under CC BY-SA 4.0
([`LICENSE-CC-BY-SA-4.0`](LICENSE-CC-BY-SA-4.0)); the probe tools and build code (`tools/`, `hooks/`, `tests/`)
are MIT licensed ([`LICENSE-MIT`](LICENSE-MIT)). By contributing you agree to
those terms.
