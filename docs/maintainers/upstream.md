# Upstream reference checkouts

The ignored `upstream/` directory holds local checkouts used for building,
research, and behavioral comparison. These repositories are not vendored into
xterm+, and their presence is not required for a stub build.

Only the Ghostty revision selected by `tools/fetch-libghostty` is a reproducible
build input. The other checkouts are working references: inspect their current
revision before relying on a behavior or API, and record any resulting xterm+
compatibility decision in the
[xterm differences ledger](../compatibility/drift.md) or
[roadmap](roadmap.md).

## Checkouts

| Directory | Repository | Role in xterm+ |
| --- | --- | --- |
| `upstream/ghostty` | <https://github.com/ghostty-org/ghostty> | Source of the pinned `libghostty-vt` build. It owns VT parsing, terminal state, reflow, key and mouse encoding, history, selection primitives, and other terminal-core facilities. `tools/fetch-libghostty` checks out the exact supported revision. |
| `upstream/ghostling` | <https://github.com/ghostty-org/ghostling> | Minimal C consumer of `libghostty-vt` and the functional-baseline reference for xterm+. Its integrations show which terminal capabilities can already be exposed by a thin host application. It is a reference, not a linked dependency. |
| `upstream/xterm-snapshots` | <https://github.com/ThomasDickey/xterm-snapshots> | Current upstream xterm source snapshots. Use it to research newer xterm behavior and prepare an intentional baseline update. A checkout newer than patch 410 does not silently change the compatibility oracle. |
| `upstream/xterm.dev` | <https://github.com/xterm-x11/xterm.dev> | Source for the xterm project website and published documentation. It is useful for release notes and public documentation, but is not the source-code behavioral oracle. |

The neighboring `/home/toppk/workspace/xterm` repository is also important. It
contains the maintained patch-410 reference used to produce the checked-in
resource, app-default, and action catalogs under `compat/`. Until the baseline
is deliberately advanced, patch 410 remains the xterm compatibility oracle
even when `upstream/xterm-snapshots` contains a newer patch.

## Revision and update policy

- Do not edit a checkout under `upstream/` as part of an xterm+ change.
- Do not commit generated Ghostty build output or an upstream checkout.
- Update the Ghostty pin in `tools/fetch-libghostty` deliberately, then compile
  and test both the stub and libghostty configurations.
- When Ghostty changes its C API, keep `src/terminal.h` backend-neutral rather
  than leaking Ghostty handles into the Xt widget or application layer.
- When Ghostling adds a user-visible terminal capability, review the parity
  matrix in the [roadmap](roadmap.md).
- When advancing the xterm oracle, update all three `compat/xterm-410-*`
  catalogs (and rename them for the new patch), the
  [xterm differences ledger](../compatibility/drift.md), and documentation
  in one compatibility checkpoint.

Useful inspection commands:

```sh
git -C upstream/ghostty show -s --format='%H %cs %s' HEAD
git -C upstream/ghostling show -s --format='%H %cs %s' HEAD
git -C upstream/xterm-snapshots show -s --format='%H %cs %s' HEAD
git -C upstream/xterm.dev show -s --format='%H %cs %s' HEAD
```
