---
man: revenant-releasing
section: 7
manual: maintainers
description: release checklist and workflow
---

# Releasing

Use `packaging/release` for release operations. It requires Python 3.11+ and an
authenticated GitHub CLI (`gh` 2.97+). Run it from the repository; it retains
downloads, verification reports and attestations under `.cache/releases/`.

Releases in this owner-maintained repository do **not** require a pull request.
Use ordinary commits and a release branch when isolation is useful. The project
owner chooses the scope and authorizes publication. Creating or verifying a
draft does not authorize publishing it.

The current workflow is **validation build → draft build → publish that draft**.
The first two stages each build and test the five packages. Publication performs
no build. Removing the duplicate build remains a separate workflow improvement;
the commands below automate the existing procedure without weakening its checks.

## Prepare the source

1. Choose the release scope and exact backend pin. Do not wait for unrelated
   features or drain `todo.md` before a bug-fix release.
2. Commit the selected changes and finish the version/changelog bookkeeping.
   Date a final release entry and link each change to its implementing commit.
   Leave the next development entry open. The package version is passed through
   Meson's `release-version` override; an RC is useful only for actual UAT.
3. Push the source branch. Ordinary Test and Documentation CI must pass for the
   exact source SHA before package validation. Successful hosted compiler,
   sanitizer and stub results count; do not repeat that full matrix locally by
   default. Iterate with focused checks, and test the intended user workflow.

A patch release may start from its published base in a separate worktree, such
as `release/0.8` from `v0.8.0`. Preserve pending master work. The release helper
uses the pushed source's changelog and backend pin, so unrelated local edits do
not affect release verification. It never commits source changes or pushes
master automatically.

## Three release commands

For a prepared and pushed `release/0.9` branch:

```sh
packaging/release validate 0.9.0 --ref release/0.9
```

This checks ordinary CI and release notes, dispatches package validation at the
selected source, monitors that run, and verifies its downloaded manifest and
five packages. It prints the exact run ID and the next command.

```sh
packaging/release draft 0.9.0 --run VALIDATION_RUN_ID
```

This verifies that specific successful validation run, its version, source,
workflow, backend pin, attempt and assets. It checks notes and CI before creating
the annotated tag, then starts the current draft-build workflow. It waits and
verifies the completed unpublished draft: all six files, exact source/tag,
notes, hashes and provenance. Existing tags must identify the same commit;
an existing release is refused. Nothing is published.

Review the draft and obtain the project owner's explicit publication approval.
Then run the command printed by the draft step:

```sh
packaging/release publish 0.9.0 --run DRAFT_RUN_ID
```

This verifies the draft again, checks that the tag and draft have not changed,
publishes it, downloads the public assets, verifies their provenance and compares
all six files against the verified draft. Stable releases become latest; RCs
remain prereleases and are not latest. Publication does not rebuild anything.

Use explicit run IDs. Never select an artifact set by “latest run.” The helper
rejects failed, incomplete, foreign-workflow or foreign-repository runs, missing
or expired downloads, incorrect versions, mismatched identities and changed
assets. It stops on an error; it does not automatically redispatch or publish.

`validate` and `draft` accept `--no-wait` to dispatch and return. This skips their
waiting and post-build verification. After validation finishes, `draft` verifies
its artifacts. After a draft finishes, use `verify --state draft` before review.
The next stage still requires a successful run. Interrupted monitoring does not
cancel the GitHub run. Keep its printed URL/ID and inspect or resume using the
commands below instead of starting another build.

## Verification and recovery

Verify an existing draft or published release with one command:

```sh
packaging/release verify 0.9.0 --run DRAFT_RUN_ID --state draft
packaging/release verify 0.8.1 --run 38361193910 --state published
```

The run must be the **draft build**, whose manifest and attestations identify the
attached release assets. An earlier validation run identifies a different build.
Each verification retains a fresh download, a `report.json` and the six verified
provenance results under `.cache/releases/`. Override the location with the
global `--state-dir DIRECTORY` option. `--repo OWNER/REPO` selects the repository;
the helper binds every API lookup and attestation to it.

- For a genuine build/test failure, inspect the retained job logs, commit a
  focused correction and validate the new SHA. Source or packaging input changes
  require a new candidate build.
- For a transient runner or download failure with unchanged source, rerun only
  the failed jobs. Do not change a dependency hash to accept an incomplete file.
  Do not rerun successful unrelated suites. A new successful run attempt needs
  a manifest from that same attempt. If assembly already completed, use a fresh
  validation run instead of reusing its old manifest.
- For an interrupted command, inspect the printed run URL before retrying.
  `gh run watch RUN_ID --exit-status` resumes monitoring without redispatching.
- For an incomplete unpublished draft, inspect it before deleting or rebuilding.
  The helper refuses to overwrite a release. Keep the tag at the same commit;
  a source change needs the next candidate/version.
- For a published defect, fix forward with another RC or patch version. Never
  move a published tag, replace assets or delete/recreate the release.

Repository setup is a one-time prerequisite: enable release immutability and
protect `v*` tags against updates and deletion. The publish command requires
immutability to be enabled, and public verification requires an immutable
release. Attach all assets before publication.

After publication, carry the shipped changes and release notes onto master
through ordinary commits/merges, preserving unrelated working changes. Check
that the published tag is in master’s ancestry and that the shipped behavior
remains present. A temporary release branch/worktree may then be removed after
checking for unreleased work. Keep a maintenance branch when it has an explicit
ongoing purpose. No PR is required for reconciliation.

## What the workflow builds and checks

The dispatch-only Release workflow builds five native packages: Linux x86_64 and
aarch64 tarballs, Debian/Ubuntu `.deb`, Fedora `.rpm`, and Arch `.pkg.tar.zst`.
Each package path runs the complete real-backend suite and the no-skips gate,
then checks the extracted or installed binaries and desktop integration.

`packaging/release-manifest` requires exactly those five package files and records
their hashes, version, source/workflow SHA, backend pin and original run/attempt.
Draft mode attests these packages and the manifest, generates linked notes, and
attaches all six files before creating the unpublished release. Both modes keep
package artifacts, manifests and diagnostic logs for 30 days; logs are not assets.

The workflow shares pinned font fixtures and architecture-specific Zig caches.
`tools/fetch-libghostty` selects an exact commit. `packaging/install-zig` checks
the pinned compiler against Zig's published checksum. The synthetic sbix fixture
is tied to `FONTTOOLS_VERSION`; update its manifest and the version together.
The x86_64 CPU floor is `x86_64-v3` (AVX2); aarch64 targets baseline ARMv8.

The low-level `packaging/release-notes`, `release-manifest` and `release-version`
tools remain available to CI and for debugging. Routine releases use the single
`packaging/release` entry point rather than temporary scripts or manual hash loops.

## Build once, promote by run ID (future work)

The current scripts automate both existing builds; they do not implement artifact
promotion. To remove the duplicate build, validation must attest all six assets,
then a promotion workflow must verify the selected successful run, source,
workflow, version, exact assets, hashes and attestations before tagging and
attaching those same bytes. Keep the original build run/attempt in the manifest.
Test expired/missing artifacts, wrong identities and modified bytes before
adopting promotion. Never weaken verification merely to save a rebuild.

## 0.8.1 retrospective

0.8.1 shipped the PID/`WINDOWID` fix from `v0.8.0`, retaining its backend pin.
The user passed local SessionLaunch acceptance testing. Ordinary CI and
[package validation](https://github.com/toppk/revenant/actions/runs/38359210944)
passed; the [draft build](https://github.com/toppk/revenant/actions/runs/38361193910)
produced the published assets. Draft and public downloads were verified,
including byte-for-byte equality and attestations.

An unnecessary PR duplicated CI. Its ASan job received a truncated FontTools
wheel; the bad hash matched a prefix of the valid wheel, and the failed job
passed on retry. Avoid PRs for routine owner releases. The repeated manual
verification commands and `/tmp/verify-*.py` scripts motivated the repository
helper above. Package builds still run twice until promotion is implemented.

## 0.8.0 retrospective and next-release changes

0.8.0 established validation before tagging, full native package/install checks,
immutable tags/assets, manifests, attestations and draft-first publication.
Validation exposed portability and dependency defects before publication.

The final validation took 14m44s
([run 35826352413](https://github.com/toppk/revenant/actions/runs/35826352413));
draft preparation rebuilt the same source for another 12m19s
([run 35863346353](https://github.com/toppk/revenant/actions/runs/35863346353)).
Finishing notes and source identity before validation, using the intended final
version, and reusing retained artifacts through explicit promotion remain the
lessons. The duplicate-build problem persisted through 0.8.1.

## Building packages locally

The individual packaging scripts work outside CI with the pinned backend,
fixtures, Zig, and native build/test dependencies installed:

```sh
packaging/build-tarball 0.9.0
packaging/build-deb 0.9.0
packaging/build-rpm 0.9.0
packaging/build-arch 0.9.0
python3 tests/release-packaging.py
```

`packaging/release-version` maps canonical `0.9.0-rc.1` to Debian/RPM
`0.9.0~rc.1` and Arch `0.9.0rc1`; the final version is `0.9.0`. Binaries always
display the canonical application version.

## Known gaps

- Build-once promotion is not implemented.
- Packages do not depend on xterm. Revisit dependencies if app-defaults are read
  from the distributor's xterm package instead of Revenant's supplied data.
- No macOS or Windows packages.
