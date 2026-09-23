---
man: revenant-releasing
section: 7
manual: maintainers
description: release checklist and workflow
---

# Releasing

Use a release branch and package validation before tagging. Published releases
and any optional published candidates are immutable. Fix forward; never move a
published tag or replace published assets.
The Release workflow is dispatch-only and **never publishes automatically**.
Both its `validate` and `draft` modes use the same package-build, complete-test,
no-skips and install-check jobs.

## 0.8.0 retrospective and next-release changes

The 0.8.0 release avoided force pushes and tag replacement. Master advanced
normally; three branch fixes were consolidated into one follow-up commit while
preserving the already-pushed workflow and feature commits. The final tag was
created once, and the published release is immutable.

What worked:

- Untagged validation exposed real portability and dependency problems before
  publication: fortified `write` diagnostics, shell Unicode escapes under dash,
  a missing fontTools dependency on cache hits, and an incorrect Fedora package
  name. Fixing these was necessary; rerunning an unchanged failing job was not.
- Five native package jobs exercised builds, full tests, installation and version
  checks. The manifest and attestations bound the shipped assets to one source
  and workflow commit.
- Draft-first publication kept incomplete assets out of the public release.

What did not work:

- We started package validation before ordinary CI was green, then canceled it.
- Using an RC package version for build-only verification created a second
  version to rebuild without providing a UAT benefit.
- Release notes and history consolidation happened after the successful trial,
  changing the identity that still needed final validation.
- Final validation of `fb1745c` took 14m44s
  ([run 35826352413](https://github.com/toppk/revenant/actions/runs/35826352413)).
  Draft creation rebuilt the same source and version for another 12m19s
  ([run 35863346353](https://github.com/toppk/revenant/actions/runs/35863346353)).
  Rebuilding did not preserve the already-verified artifacts; promotion should.

The next workflow change should implement **build once, promote by run ID**.
This is a proposed replacement, not a capability of the current `draft` mode:

1. Finish feature selection, history consolidation, release notes and version
   bookkeeping before the final candidate build. Use the intended final package
   version even in untagged validation; use RC versions only for actual UAT.
2. Require successful ordinary CI for the chosen source. Hosted compiler,
   sanitizer and stub results count; do not repeat the full matrix locally by
   default. During iteration, select checks according to the changed inputs.
   A packaging-only dependency correction does not require another unrelated
   renderer matrix before retrying packaging.
3. Build and test all five packages once. Generate the manifest and attest all
   six assets in this successful validation run, before any release tag exists.
4. Promote that explicit run ID without rebuilding. A promotion job must verify
   the trusted repository and workflow, successful run, source/workflow SHA,
   intended version, exact asset set, hashes and attestations. Missing, expired,
   partial or mismatched artifacts must fail closed. Never select "latest run".
5. Create the tag only after promotion checks pass, pointing at the verified
   source. Attach those exact bytes to a draft, verify the draft downloads, then
   publish on maintainer instruction. Verify the published state and bytes.

A new source, version or packaging input requires a new validated artifact set.
A retry of draft creation or publication does not require rebuilding valid,
retained artifacts. Retain the manifest's original build run ID during promotion.
Test rejection paths before adopting this workflow, especially wrong SHA/version,
foreign workflow, failed run, missing artifact and modified bytes. Do not weaken
hash/provenance verification to save time; remove duplicate compilation instead.

## 0.8 release procedure (current workflow)

For 0.8, freeze features at the accepted UTF-8 title and login-shell work. Ship
against the exact tested libghostty development commit in
`tools/fetch-libghostty`; do not wait for Ghostty 1.4.0. Its eventual release is
an independent maintainer checkpoint. Keep xterm-411 as the comparison oracle.

1. Land the CI/packaging preparation as ordinary commits. The workflow must be
   present on the default branch before GitHub will offer manual dispatch.
2. Create `release/0.8`, open a PR to `master`, and restrict it to release
   blockers, packaging/CI fixes and release documentation. Ordinary test and
   documentation workflows also run on pushes to `release/**`. Only `master`
   deploys the documentation site.
3. Validate exact candidate commits without creating tags or releases. Keep CI
   fixes in separate commits from changelog/version bookkeeping as they are made.
4. For 0.8, use validation artifacts for build/test verification; no published
   RC or RC tag is required. The validation version `0.8.0-rc.1` may be reused
   across attempts: the source SHA and run ID identify each artifact set.
5. Bring the tested changes back to `master` without rewriting published
   history. Untagged branch fixes may be squashed into a coherent commit; keep
   feature commits intact. Keep the validation branch until publication checks
   finish, then delete it as described below. Revalidate the resulting SHA.
   Do not squash/rebase a tagged candidate.
6. Prepare final notes/version bookkeeping, validate that exact final commit
   with the final version, then tag, build its draft, verify and publish.
   Binary versions are embedded, so RC artifacts cannot be renamed or promoted
   byte-for-byte into final-version artifacts. Final packages must be rebuilt
   and checked.

This is a bounded release preparation, not a requirement to drain `todo.md`.
Do not introduce additional features while fixing packaging failures.

## Validation before tags

From the frozen release branch, push the exact commit and require the complete
compiler/sanitizer/stub matrix before release. Successful hosted Test results
count; do not duplicate that matrix locally by default. Real-backend GCC,
Clang and ASan use the no-skips gate.
Then dispatch the same package pipeline used for release drafts:

```sh
source_sha=$(git rev-parse HEAD)
git push origin release/0.8
gh workflow run release.yml --ref release/0.8 \
  -f mode=validate -f source="$source_sha" -f version=0.8.0-rc.1
```

The workflow's dispatch commit must equal `source`; a branch advancing between
selection and dispatch causes a fail-fast identity mismatch, not a mixed build.
Use the Actions run URL/ID to follow this particular run. Do not infer its
identity from whichever run happens to be newest.

Download the `package-*` artifacts and `candidate-manifest` from that run into
one empty directory (each package artifact contains one file), then check:

```sh
packaging/release-manifest --check "$candidate_dir" 0.8.0-rc.1 "$source_sha"
```

Validation mode does not require a completed changelog and creates neither a
tag nor a release. Package results, the manifest and failure diagnostics remain
available as Actions artifacts for 30 days. Inspect successful package install
checks and do representative interactive shell/editor/multiplexer testing.
A code fix creates a new candidate commit; repeat only the affected local checks
while iterating, then the complete candidate gates before publication.

## Candidate and final drafts

Prepare the `0.8.0` changelog entry with user-facing bullets linked to their
implementing commits. RC notes use that base entry (which may stay `Unreleased`)
and link to the exact candidate source; do not add a new changelog heading or
advance the development version for every RC. Draft preparation rejects missing
entries and unlinked bullets before building.

For the final release only, date the entry, reconcile its bullets against
`git log <previous-tag>..HEAD`, open the next development entry, and advance
Meson's project version in a separate bookkeeping commit containing only
`CHANGELOG.md` and `meson.build`. Validate this final commit with `version=0.8.0`.

Once the selected commit is validated, tag that commit and dispatch at the tag:

```sh
version=0.8.0-rc.1                  # use 0.8.0 for the final draft
tag="v$version"
git tag -a "$tag" "$source_sha" -m "Revenant $version"
git push origin "$tag"
gh workflow run release.yml --ref "$tag" -f mode=draft -f tag="$tag"
```

Draft mode rebuilds and checks all packages; it does not promote unverified
Actions downloads. It writes an attestation for each of the five packages and
for `release-manifest.json`, and creates a draft with all six files attached.
RC drafts are explicitly prereleases and not latest. Final drafts also remain
not latest until publication. The workflow refuses an existing release rather
than replacing its assets. The source and workflow SHAs are the same, and every
build job checks out that SHA instead of repeatedly resolving a movable branch.

## Verify, then publish explicitly

Before publishing, enable **release immutability** in the repository's release
settings and restrict updates/deletion of release tags with a tag ruleset. These
are repository settings, not declarations that YAML can apply. Enable them after
the draft-first workflow is installed. Existing releases are not retroactively
made immutable. GitHub's
[immutable-release procedure](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases)
requires assets to be attached before publishing the draft.

Inspect the workflow result and its exact commit, release notes, and downloaded
assets. A green build alone is not the publication decision:

```sh
verify_dir=$(mktemp -d)
gh release download "$tag" --dir "$verify_dir"
packaging/release-manifest --check "$verify_dir" "$version" "$source_sha"
for asset in "$verify_dir"/*; do
  gh attestation verify "$asset" --repo toppk/revenant \
    --signer-workflow toppk/revenant/.github/workflows/release.yml \
    --source-digest "$source_sha" --signer-digest "$source_sha"
done
# Confirm that the remote tag has not changed since validation.
git fetch --no-tags origin "refs/tags/$tag"
test "$(git rev-parse 'FETCH_HEAD^{commit}')" = "$source_sha"
```

Require six assets, matching identities/hashes and verified provenance. Inspect
installed package/binary versions and desktop integration, and run representative
interactive use before the explicit publication step. The workflow already tests
tarballs after extraction, and installs the deb, rpm and Arch packages.

```sh
# Candidate: explicitly not latest.
gh release edit "$tag" --verify-tag --draft=false --prerelease --latest=false
# Final instead: explicitly stable and latest.
# gh release edit "$tag" --verify-tag --draft=false --prerelease=false --latest
```

After publishing, check the release's state and download/verify the published
assets again. If a published final release has a defect, make a patch release;
if an RC has a defect, publish the next RC. Never delete and recreate a public
release/tag as a repair strategy.

## Clean up the release branch

After publication and verification of the public downloads, delete the temporary
release branch on the remote and locally. First confirm that its intended changes
are present on `master`, including any squashed fixes, and that no unreleased work
remains on it. The published tag, manifest and attestations identify what shipped;
a permanent release branch is not required for that purpose.

For example, after releasing 0.8.0:

```sh
git switch master
git push origin --delete release/0.8
git branch -d release/0.8
```

If the branch was squash-merged, Git may refuse `-d` because its original commits
are not ancestors of master. After verifying the consolidated changes, use
`git branch -D release/0.8` to remove only that local branch reference. Do not
remove or move the release tag. Keep a release branch only when it has an explicit
ongoing maintenance purpose.

## What the workflow does

- **Resolve candidate identity** validates either a full SHA plus application version
  (`validate`) or an existing version tag (`draft`). The workflow's dispatch ref
  must resolve to the same commit. Every downstream checkout uses the resolved
  SHA, and libghostty must be pinned to a full commit. Draft mode also checks the
  remote tag and linked changelog notes before spending time on builds.
- **tar.gz** builds on x86_64 and aarch64 runners. The archive is
  `revenant-<version>-linux-<arch>.tar.gz` containing a stripped `revenant`,
  an `xterm+` symlink, `README.md`, and `LICENSES/`.
- **deb** builds on `ubuntu-latest` with `debian/rules binary` from
  `packaging/debian/` (not `dpkg-buildpackage`: its `.buildinfo` generation
  scans the whole runner package database and is discarded anyway), then
  installs the result and checks `revenant --version`, `xterm+ --version`,
  and the desktop files.
- **rpm** builds in a `fedora:latest` container with
  `rpmbuild --build-in-place` from `packaging/revenant.spec`, then installs
  the result and runs the same binary and desktop-integration checks.
- **pkg.tar.zst** builds in an `archlinux:latest` container with `makepkg`
  from `packaging/PKGBUILD` via `packaging/build-arch` (which drops to an
  unprivileged `builder` user, since `makepkg` refuses root), then installs
  the result and runs the same checks. The container needs
  `xorg-mkfontscale` and an explicit `mkfontdir` so Xvfb can serve the misc
  bitmap fonts.
- Every build job requires libnotify with `-Dlibnotify=enabled`, installs its
  development package and D-Bus (the desktop-notification test runs under
  `dbus-run-session`), installs Xvfb and the X bitmap fonts, pins fontTools, and
  stages the font fixtures with `tools/stage-font-fixtures`. Every packaging
  configuration passes `-Dxvfb-tests=enabled`, so Meson fails immediately if
  Xvfb or the libghostty backend is unavailable. After `meson test`, each path
  runs `tools/check-release-tests`, which fails unless every test Meson lists
  ran and passed with no skips. The only checks outside this gate are the
  interactive probes and `tools/check-xterm-font-compat`, which need a live
  xterm and display.
- **Font fixtures** runs before the builders. It restores the staged
  fixture tree from `actions/cache` (keyed on `tools/stage-font-fixtures`,
  `tools/font-fixtures/`, and `FONTTOOLS_VERSION`), stages it with
  `tools/stage-font-fixtures` only on a miss, and saves it. Each builder
  then restores the same key with `actions/cache/restore` and
  `fail-on-cache-miss`, which is several times faster than passing a run
  artifact around. If the cache were evicted mid-run the builder fails
  loudly and a re-dispatch repopulates it. The upstream font sources are
  therefore fetched once per pin change rather than five times per release,
  and the builder images need none of the staging toolchain (fontTools,
  cpio, rpm2cpio, unzip).
- Every build job restores a Zig cache with `actions/cache`. The key is
  self-describing:

      libghostty-<runner os>-<zig target>-<zig cpu>-zig<ZIG_VERSION>-ghostty<commit>-<hash of tools/build-libghostty>-<runner cpu model>

  `tools/build-libghostty --print-target` and `tools/fetch-libghostty
  --print-reference` supply the target tuple and pinned commit, so the key
  changes exactly when the libghostty inputs do. The distro is deliberately
  absent: zig uses its own compiler and libc headers for an explicit target,
  so one cache serves the tarball, deb, rpm, and Arch jobs of an
  architecture. The runner CPU model is present because Ghostty compiles its
  build-time generators for the native host CPU and zig has no override
  ([ziglang/zig#22663](https://github.com/ziglang/zig/issues/22663)); a
  `restore-keys` prefix restores the newest same-architecture cache on a
  model mismatch and the union saved afterward converges to a cache that
  hits on every model. A warm cache turns the libghostty build into a few
  seconds.
- `build-libghostty` passes an explicit `-Dtarget` and `-Dcpu`
  (`x86_64-linux-gnu`/`x86_64_v3`, `aarch64-linux-gnu`/`baseline`) so the
  released library never depends on the build host. The x86_64-v3 floor is
  stated in the install guide and in every release body; drop to
  `x86_64_v2` or `baseline` only in response to user reports.
- **Artifact assembly** requires exactly five expected package files and records
  their hashes, version, source/workflow SHA, libghostty SHA and run identity in
  `release-manifest.json`. Package artifacts, this manifest and diagnostic logs
  are retained for 30 days. Logs are uploaded even on package-job failure and
  are never included in release assets.
- **Draft mode only** attests the packages and manifest, renders linked release
  notes, checks the remote tag again, and creates a new unpublished GitHub
  release with every asset attached. It never publishes. An existing release
  is refused rather than updated. Validation mode creates no GitHub release.

The synthetic sbix fixture is byte-exact only for the fontTools release
pinned in the workflow's `FONTTOOLS_VERSION`; regenerate
`tools/font-fixtures/manifest.json` and move the pin together.

Zig is installed by `packaging/install-zig`, which downloads the version
pinned in the workflow's `ZIG_VERSION` from ziglang.org and verifies the
checksum against `https://ziglang.org/download/index.json`. Bump
`ZIG_VERSION` when `tools/fetch-libghostty` moves to a Ghostty revision that
needs a newer Zig.

## Building packages locally

Every job calls scripts under `packaging/` that work outside CI. They require
the build/test dependencies, Zig on PATH, and the pinned Ghostty/fixture inputs.
The application version is passed through Meson's `release-version` override;
package metadata uses native ordering so an RC upgrades to the final release:

| Surface | Example candidate | Final |
| --- | --- | --- |
| Tag | `v0.8.0-rc.1` | `v0.8.0` |
| Binary and tarball | `0.8.0-rc.1` | `0.8.0` |
| Debian/RPM version | `0.8.0~rc.1` | `0.8.0` |
| Arch pkgver | `0.8.0rc1` | `0.8.0` |

`packaging/release-version` owns this mapping. Each native-package CI job uses
its package manager to check `rc.1 < rc.2 < rc.10 < final` before building.
Package file names reflect the native version; `revenant --version` and
`xterm+ --version` always show the canonical application version.

```sh
packaging/build-tarball 0.8.0-rc.1
packaging/build-deb 0.8.0-rc.1
packaging/build-rpm 0.8.0-rc.1
packaging/build-arch 0.8.0-rc.1
python3 tests/release-packaging.py
```

## Recovering from failures

- **Validation failure:** inspect retained logs, commit the fix normally, and
  validate the new SHA. No release or tag needs repairing.
- **Transient infrastructure failure with unchanged source:** rerun the failed
  jobs at the same commit. Do not move a tag to retry a download or runner outage.
- **Failed or partial unpublished draft:** inspect it before doing anything.
  An unpublished incomplete draft may be deleted and rebuilt at the **same tag
  and commit**, without deleting or moving the tag. The workflow will not overwrite
  an existing draft. If source changes, allocate the next candidate tag instead.
- **Published RC/final defect:** fix forward with a new RC/patch version. Keep
  the published tag, assets and history intact.

## Known gaps

- The packages have no dependency on xterm. Once Revenant reads
  `/usr/share/X11/app-defaults/XTerm` and `XTerm-color` from the
  distribution's xterm package instead of carrying its own copy, the deb and
  rpm need a `Depends`/`Requires` on `xterm` (or on whichever subpackage
  owns those files).
- No macOS or Windows builds; the X11 story there is unresolved.
