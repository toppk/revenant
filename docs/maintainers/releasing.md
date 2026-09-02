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
   adds them. Soft-wrap long bullets normally: `packaging/release-notes` joins
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
    packaging/release-notes "$version"
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

- **Check tag** fails immediately if the tag does not exist on `origin`,
  derives the package version by stripping the leading `v` (`v0.3.0` →
  `0.3.0`), and renders the changelog entry with `packaging/release-notes` so a
  missing entry or an unlinked bullet fails in seconds rather than after the
  builds. Every packaging path passes that value through Meson's
  `release-version` option, overriding the next-development version in the
  tagged source. Installed-package checks require `revenant --version` and
  `xterm+ --version` to match the tag exactly.
- Every build job checks out the Revenant tag itself, so its packaging scripts
  come from that tagged commit. `tools/fetch-libghostty` pins one exact Ghostty
  commit; advance that pin as an ordinary reviewed change before a release.
  Every artifact in one release therefore builds the same libghostty source.
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
- Every build job installs Xvfb and the X bitmap fonts, pins fontTools, and
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
- **GitHub release** downloads every artifact, signs a build-provenance
  attestation for each with `actions/attest-build-provenance` (verify with
  `gh attestation verify FILE --repo toppk/revenant`), renders the release body
  with `packaging/release-notes` (highlights, changes with linked commits, an
  artifact table with SHA-256 sums, and the CPU floor), and publishes the
  release.
- The package artifacts that carry builds to the release job expire after
  one day; nothing deletes them earlier. The job fails if `CHANGELOG.md` has no
  entry for the version or any change bullet lacks a valid trailing commit
  link.

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
- No macOS or Windows builds; the X11 story there is unresolved.
