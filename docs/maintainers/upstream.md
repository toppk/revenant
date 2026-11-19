---
man: revenant-upstream
section: 7
manual: maintainers
description: reference checkouts and update procedure
---

# Upstream reference checkouts

The ignored `upstream/` directory holds local checkouts used for building,
research, and behavioral comparison. These repositories are not vendored into
Revenant, and their presence is not required for a stub build.

## libghostty-vt releases

Ghostty the application and `libghostty-vt` the library live in one repository
but are released separately. The application uses `vX.Y.Z` tags (1.4.0 is
`v1.4.0`); the library has its own semantic version and `vt-X.Y.Z` tags, and a
Ghostty release does not contain a library release. `PACKAGING-LIBGHOSTTY.md` in
the checkout is upstream's packaging contract for the library. The 1.x C API
and ABI are stable: minor and patch releases only add to them.

`tools/fetch-libghostty` pins an exact commit and leaves the checkout detached at
that revision. Use `tools/fetch-libghostty --print-reference` to read the
configured pin. A local checkout may have been advanced independently; inspect
the actual build inputs and binary provenance rather than assuming the checkout
and configured pin agree.

The current pin is `108cacb17ed8990840cd14abfce43d8ed8503ca1`, the commit the
signed `vt-1.0.1` tag names (libghostty-vt 1.0.1), adopted on 2026-11-18. It
follows Ghostty 1.4.0 (`v1.4.0`, `83fb95624b0439ee660c2679b43b38c8b63769cd`) and
replaces the development pin `b115e456749e2820a14d3942a63159ff8d46d925`. Keep the
full-hash contract: record the tag beside the hash, but never substitute a tag or
branch name for it.

Revenant builds the tagged commit from the git checkout with `-Demit-lib-vt`,
which upstream permits, rather than from the signed release tarballs at
`release.files.ghostty.org/libghostty-vt/`. The provenance tooling
(`tools/uucode_pin.py`, the release manifest, the CI cache key) reads the pinned
commit; moving to the tarball and its `--system` dependency bundle is a separate
decision.

Three build facts changed with this pin, all handled in `tools/build-libghostty`:

- libghostty-vt 1.0.x requires Zig 0.17 (any 0.17.x; 0.16 and 0.18 are refused).
  `ZIG_VERSION` in both workflows is 0.17.0.
- Zig 0.17 takes its cache directories from `ZIG_LOCAL_CACHE_DIR` and
  `ZIG_GLOBAL_CACHE_DIR` instead of flags. The caches are named per Zig version
  so a directory populated by an older Zig is never reused.
- Ghostty's build asks git for version tags in the *working directory*, which
  is Revenant's build directory, and aborts on a Revenant release tag such as
  `v0.9.0`. The builder passes `-Dversion-string` and `-Dlib-version-string`
  (read from `build.zig.zon` and `include/ghostty/vt/version.h`) so git is never
  consulted.

The 1.0 API pass was source-incompatible with the development pin. The adapter
port was mechanical except for two behavior changes worth remembering:

- `GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES` with a NULL value now restores the
  10 kB default instead of removing the limit; pass `SIZE_MAX` for unlimited.
- The OSC 7501 support reply follows protocol revision 0.4 and lists the accepted
  states and kinds instead of echoing a bare `?`.

Otherwise: fixed-layout structs (`GhosttyGridRef`, `GhosttySelection`,
`GhosttyStyle`, effect reports) lost their `size` field, style attributes became
`GhosttyStyleFlags` bits, one-shot selection formatting moved to `GhosttyFormatter`
with `GHOSTTY_FORMATTER_OPT_SELECTION`, encoders write `uint8_t` buffers and
`setopt` became `set`, key events borrow their UTF-8 text rather than copying it,
and several mode and cursor-style constants were renamed.

The November 19 checkpoint rebuilt the normal binary with Zig 0.17 and passed
the full GCC and AddressSanitizer suites (82 tests per build, all passing, no
skips), plus the stub suite (21 passing). The first run against the new pin
failed three tests, all from the scrollback change above.

The Unicode data is unchanged: the pin names a newer `uucode` package
(`uucode-0.2.0-ZZjBPm6FVgBcY-79AHV8ckQj86z0JwOyVyj89VeZrEiv`), but its `ucd/`
directory is byte-identical to the previous one, so the Unicode 18
[provenance and behavior record](unicode-18-migration.md) still holds and no
table regeneration was needed.

The maintainer selects dependency revisions. Adopt later 1.x releases by the
exact commit of their `vt-X.Y.Z` tag, and repeat the Unicode migration only when
a release introduces a relevant data delta. The other checkouts are also working
references: record compatibility decisions in the
[xterm differences ledger](../compatibility/drift.md) or [roadmap](roadmap.md).

## C API contribution review

The [libghostty C API review](libghostty-c-api-review.md) separates missing
accessors and effects from protocol work, with proposed PR scopes and concrete
Revenant consumers. It is a decision record, not an implementation assignment.

The review also tracks [OSC 7501 program status](libghostty-c-api-review.md#program-status-osc-7501-ghostty-pr-14560)
from Ghostty PR #14560. Its C callback is sufficient; native Ghostty ignores the
reports. The dependency pin includes the public C effect. Downstream status
storage and presentation are tracked separately.

## Checkouts

| Directory | Repository | Role in Revenant |
| --- | --- | --- |
| `upstream/ghostty` | <https://github.com/ghostty-org/ghostty> | Source of the selected `libghostty-vt` build. It owns VT parsing, terminal state, reflow, key and mouse encoding, history, selection primitives, and other terminal-core facilities. `tools/fetch-libghostty` selects the exact commit of a `vt-X.Y.Z` release tag, currently libghostty-vt 1.0.1. |
| `upstream/ghostling` | <https://github.com/ghostty-org/ghostling> | Minimal C consumer of `libghostty-vt` and the functional-baseline reference for Revenant. Its integrations show which terminal capabilities can already be exposed by a thin host application. It is a reference, not a linked dependency. |
| `upstream/xterm-snapshots` | <https://github.com/ThomasDickey/xterm-snapshots> | The xterm source-code behavioral oracle is the exact `xterm-411` tag. The working checkout may be ahead; inspect the pinned tag for baseline comparisons. Advance the oracle only as part of an intentional compatibility-baseline migration. |
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
- Check `minimum_zig_version` in the new revision's `build.zig.zon` and move
  `ZIG_VERSION` in both workflows with the pin when it changes.
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
