# Upstream reference checkouts

The ignored `upstream/` directory holds local checkouts used for building,
research, and behavioral comparison. These repositories are not vendored into
Revenant, and their presence is not required for a stub build.

`tools/fetch-libghostty` currently follows Ghostty's moving `main` branch in
preparation for 1.4. It fetches that reference and leaves the checkout detached
at the exact resolved commit. Builds made after separate fetches are therefore
not reproducible until this temporary reference is replaced with the Ghostty
1.4 release tag. The other checkouts are also working references: inspect their
current revision before relying on a behavior or API, and record any resulting
Revenant compatibility decision in the
[xterm differences ledger](../compatibility/drift.md) or
[roadmap](roadmap.md).

## Checkouts

| Directory | Repository | Role in Revenant |
| --- | --- | --- |
| `upstream/ghostty` | <https://github.com/ghostty-org/ghostty> | Source of the selected `libghostty-vt` build. It owns VT parsing, terminal state, reflow, key and mouse encoding, history, selection primitives, and other terminal-core facilities. `tools/fetch-libghostty` currently resolves `main` to an exact detached commit; switch the configured reference to the 1.4 release tag when available. |
| `upstream/ghostling` | <https://github.com/ghostty-org/ghostling> | Minimal C consumer of `libghostty-vt` and the functional-baseline reference for Revenant. Its integrations show which terminal capabilities can already be exposed by a thin host application. It is a reference, not a linked dependency. |
| `upstream/xterm-snapshots` | <https://github.com/ThomasDickey/xterm-snapshots> | The xterm source-code behavioral oracle, detached at the exact `xterm-411` tag. Advance it only as part of an intentional compatibility-baseline migration. |
| `upstream/xterm.dev` | <https://github.com/xterm-x11/xterm.dev> | Source for the xterm project website and published documentation. It is useful for release notes and public documentation, but is not the source-code behavioral oracle. |

The neighboring `/home/toppk/workspace/xterm` repository remains useful as a
historical patch-410 working reference. It no longer defines the compatibility
baseline; `upstream/xterm-snapshots` at `xterm-411` is the current oracle used
for the checked-in resource, app-default, and action catalogs under `compat/`.

## Review evidence policy

Before accepting a review finding, compatibility decision, or documentation
statement of the form “xterm does X,” check the claim against
`upstream/xterm-snapshots` at the pinned oracle revision. Record the relevant
source file and symbol (and line when useful) in the review or change rationale.
Memory, manual-page interpretation, and observations from a live xterm are
useful supporting evidence, but they do not replace checking the available
source. If source and observed behavior appear to disagree, record both as an
open discrepancy and investigate rather than promoting either recollection to
a finding.

## Revision and update policy

- Do not edit a checkout under `upstream/` as part of a Revenant change.
- Do not commit generated Ghostty build output or an upstream checkout.
- Update the Ghostty reference in `tools/fetch-libghostty` deliberately, then compile
  and test both the stub and libghostty configurations.
- When Ghostty changes its C API, keep `src/terminal.h` backend-neutral rather
  than leaking Ghostty handles into the Xt widget or application layer.
- When Ghostling adds a user-visible terminal capability, review the parity
  matrix in the [roadmap](roadmap.md).
- When advancing the xterm oracle, update all four `compat/xterm-411-*`
  artifacts (and rename them for the new patch), the
  [xterm differences ledger](../compatibility/drift.md), and documentation
  in one compatibility checkpoint.

Useful inspection commands:

```sh
git -C upstream/ghostty show -s --format='%H %cs %s' HEAD
git -C upstream/ghostling show -s --format='%H %cs %s' HEAD
git -C upstream/xterm-snapshots show -s --format='%H %cs %s' HEAD
git -C upstream/xterm.dev show -s --format='%H %cs %s' HEAD
```
