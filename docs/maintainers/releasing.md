# Releasing

Releases are built by `.github/workflows/release.yml`. It is intentionally
dispatch-only: pushing a tag makes that revision eligible for release, but
does **not** start a build. A maintainer pushes the release commit and tag,
then dispatches the workflow against that tag.

## Release checklist

1. Keep the predicted next release at the top of `CHANGELOG.md`, for example
   `## 0.5.0 — Unreleased`, while `meson.build` reports `0.5.0-dev`. Add
   development bullets under `### Features`, `### Bug fixes`, or `### Other`.
   Bullets added during development may omit commit links; the release commit
   adds them. Soft-wrap long bullets normally: `tools/release-notes` joins
   continuation lines when it renders the GitHub release body.

2. Freeze the libghostty input before preparing the release. During development
   `tools/fetch-libghostty` may follow a moving branch such as `main` to test an
   unreleased Ghostty version. A Revenant release must replace that branch with
   an immutable upstream release tag or the exact tested commit. When the next
   Ghostty release has no tag yet, as with early 1.4 development, pinning the
   full commit is the correct fallback:

    ```sh
    rg '^readonly reference=' tools/fetch-libghostty
    tools/fetch-libghostty
    ```

   Land the pin as an ordinary change before the release commit and include it
   in the changelog's commit reconciliation. Do not release while the reference
   is a moving branch.

3. Prepare one bookkeeping commit containing only `CHANGELOG.md` and the
   project-version line in `meson.build`:

   - Review the whole release entry for concise, user-facing language.
   - Replace `Unreleased` with the release date and add a short summary.
   - Trace every bullet to the commits that implement it. Each bullet must end
     with one or more links in the form
     `([abc1234](https://github.com/toppk/revenant/commit/<full-hash>))`;
     a bullet without a commit link is not releasable. Reconcile in both
     directions against `git log --oneline <previous-tag>..HEAD`: every bullet
     gets its commits, and every user-visible commit gets a bullet. Pure
     refactors, test-only changes, and CI work may be folded into one
     `### Other` bullet, but the commits must still be linked. One commit may
     support several bullets, and one bullet may cite several commits.
   - Open the next predicted release above it.
   - Advance the project version to that next release's `-dev` version.

   Render the notes before committing, then verify the commit's file boundary:

    ```sh
    version=0.4.0
    previous_tag=v0.3.0
    git log --oneline "$previous_tag"..HEAD
    tools/release-notes "$version"
    git add CHANGELOG.md meson.build
    git commit -m "release: $version"
    git diff-tree --no-commit-id --name-only -r HEAD
    ```

   The final command must list only `CHANGELOG.md` and `meson.build`.

   The prediction is intentionally cheap: if the next release becomes 0.4.1
   rather than 0.5.0, correct the heading and development version in that
   later release commit.

4. Tag the release commit and push the branch and tag:

    ```sh
    version=0.4.0
    tag="v$version"
    git tag "$tag"
    git push origin master "$tag"
    ```

5. Explicitly dispatch the workflow with the pushed tag, from the CLI or the
   Actions tab. Tag creation and tag push do not perform this step:

    ```sh
    tag=v0.4.0
    gh workflow run release.yml -f tag="$tag"
    gh run watch
    ```

6. When the run finishes, verify the published release rather than treating a
   green workflow as the finish line:

    ```sh
    version=0.4.0
    tag="v$version"
    verify_dir=$(mktemp -d)
    gh release download "$tag" --dir "$verify_dir"
    gh attestation verify "$verify_dir"/* --repo toppk/revenant
    tar -xzf "$verify_dir/revenant-$version-linux-$(uname -m).tar.gz" \
      -C "$verify_dir"
    test "$("$verify_dir/revenant-$version-linux-$(uname -m)/revenant" \
      --version)" = "revenant $version"
    gh release view "$tag"
    ```

   Confirm that all four assets are present, every attestation verifies, the
   binary version matches the tag, and the release body contains Highlights,
   Changes, and Artifacts rendered from the intended changelog entry.

## What the workflow does

- **Check tag** fails immediately if the tag does not exist on `origin`, and
  derives the package version by stripping the leading `v` (`v0.3.0` →
  `0.3.0`). Every packaging path passes that value through Meson's
  `release-version` option, overriding the next-development version in the
  tagged source. Installed-package checks require `revenant --version` and
  `xterm+ --version` to match the tag exactly.
- Every build job checks out the Revenant tag itself, so its packaging scripts
  come from that tagged commit. `tools/fetch-libghostty` pins one exact Ghostty
  commit; advance that pin as an ordinary reviewed change before a release.
  Every artifact in one release therefore builds the same libghostty source.
- **tar.gz** builds on x86_64 and aarch64 runners. This job also installs
  Xvfb and stages the pinned font fixtures with `tools/stage-font-fixtures`,
  so `meson test` there runs all eight tests, including
  `xvfb-font-baseline` and `xvfb-emoji-routing`. The archive is
  `revenant-<version>-linux-<arch>.tar.gz` containing a stripped `revenant`,
  an `xterm+` symlink, `README.md`, and `LICENSES/`.
- **deb** builds on `ubuntu-latest` with `debian/rules binary` from
  `packaging/debian/` (not `dpkg-buildpackage`: its `.buildinfo` generation
  scans the whole runner package database and is discarded anyway). It runs
  the six tests that do not require staged font fixtures, then installs the
  result and checks `revenant --version`, `xterm+ --version`, and the desktop
  files.
- **rpm** builds in a `fedora:latest` container with
  `rpmbuild --build-in-place` from `packaging/revenant.spec`. With no Xvfb in
  that container it runs the parser and self-test only, then installs the
  result and runs the same binary and desktop-integration checks.
- **GitHub release** downloads every artifact, signs a build-provenance
  attestation for each with `actions/attest-build-provenance` (verify with
  `gh attestation verify FILE --repo toppk/revenant`), renders the release body
  with `tools/release-notes` (highlights, changes with linked commits, and
  an artifact table with SHA-256 sums), publishes the release, and then
  deletes the run's build artifacts. The job fails if `CHANGELOG.md` has no
  entry for the version or any change bullet lacks a valid trailing commit
  link. Artifacts are also uploaded with a 3-day expiry, so a failed run leaves
  them around briefly for debugging.

The synthetic sbix fixture is byte-exact only for the fontTools release
pinned in the workflow's `FONTTOOLS_VERSION`; regenerate
`tools/font-fixtures/manifest.json` and move the pin together.

Zig is installed by `packaging/install-zig`, which downloads the version
pinned in the workflow's `ZIG_VERSION` from ziglang.org and verifies the
checksum against `https://ziglang.org/download/index.json`. Bump
`ZIG_VERSION` when `tools/fetch-libghostty` moves to a Ghostty revision that
needs a newer Zig.

## Building packages locally

Each job calls a script under `packaging/` that works outside CI too.
All of them need the build dependencies from [Install](../getting-started/install.md),
Zig on `PATH`, and `tools/fetch-libghostty` run first; output lands in
`dist/`. The version argument names the package and is compiled into the
binary through Meson's `release-version` override.

```sh
version=0.4.0
packaging/build-tarball "$version"
packaging/build-deb "$version"      # Debian/Ubuntu: also needs debhelper
packaging/build-rpm "$version"      # Fedora: also needs rpm-build
```

## Recovering from a failed release

If a published release is wrong, remove the release and remote tag, fix
forward, prepare a new release commit, recreate the tag, and dispatch again.
Do not amend an asset or silently move a published tag while leaving the old
release in place.

```sh
tag=v0.4.0
gh release delete "$tag" --yes --cleanup-tag
git tag -d "$tag"
# Land the fix, then repeat the release checklist.
```

## Known gaps

- The packages have no dependency on xterm. Once Revenant reads
  `/usr/share/X11/app-defaults/XTerm` and `XTerm-color` from the
  distribution's xterm package instead of carrying its own copy, the deb and
  rpm need a `Depends`/`Requires` on `xterm` (or on whichever subpackage
  owns those files).
- The rpm release container does not install Xvfb or stage the font fixtures,
  so its package build runs only two tests. The tarball jobs provide the full
  eight-test release gate today.
- No macOS or Windows builds; the X11 story there is unresolved.
