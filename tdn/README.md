# Terminal Developers Network

A reference for terminal protocols in the style of a web-platform reference:
one page per feature, with syntax, behavior, history, a compatibility table
across emulators, and a reproducible probe.

- `docs/` — the site content, built with MkDocs (`mkdocs.yml`).
- `tools/` — shell probes (`query`, `sendcsi`, `sendosc`, `sgr-sampler`,
  `testfocus`) that produce the observations recorded in the tables.

TDN currently lives inside the xterm+ repository, but it is about terminals
in general and is laid out so it can move to its own repository unchanged.
Contributions that replace a `?` in a compatibility table with a sourced
**Yes**/**No** or a versioned observation are the most useful kind.

Build locally:

```sh
pip install -r requirements.txt && mkdocs serve
```

## License

TDN content (`docs/`) is licensed under CC BY-SA 4.0
([`LICENSE-CC-BY-SA-4.0`](LICENSE-CC-BY-SA-4.0)); the probe tools (`tools/`)
are MIT licensed ([`LICENSE-MIT`](LICENSE-MIT)). By contributing you agree to
those terms.
