# Releasing

Releases are built by `.github/workflows/release.yml`. It never runs on its
own: nothing happens on push or when a tag appears. A maintainer tags a
commit, then dispatches the workflow against that tag.

## Steps

1. Tag the commit and push the tag:

    ```sh
    git tag v0.2.0
    git push origin master v0.2.0
    ```

2. Dispatch the workflow with the tag, from the CLI or the Actions tab:

    ```sh
    gh workflow run release.yml -f tag=v0.2.0
    gh run watch
    ```

3. When the run finishes, the release at
   `https://github.com/toppk/xterm-plus/releases/tag/v0.2.0` is published
   with its assets attached.

## What the workflow does

- **Check tag** fails immediately if the tag does not exist on `origin`, and
  derives the package version by stripping the leading `v` (`v0.2.0` →
  `0.2.0`).
- Every build job checks out the Revenant tag itself, so its packaging scripts
  come from that tagged commit. While `tools/fetch-libghostty` temporarily
  follows Ghostty `main`, however, separate jobs can resolve different Ghostty
  commits. Replace `main` with the Ghostty 1.4 release tag before publishing an
  Revenant release.
- **tar.gz** builds on x86_64 and aarch64 runners:
  `revenant-<version>-linux-<arch>.tar.gz` containing a stripped `revenant`,
  an `xterm+` symlink,
  `README.md`, and `LICENSES/`.
- **deb** builds on `ubuntu-latest` with `dpkg-buildpackage` from
  `packaging/debian/`, then installs the result and runs `revenant --version`
  and `xterm+ --version`.
- **rpm** builds in a `fedora:latest` container with
  `rpmbuild --build-in-place` from `packaging/revenant.spec`, then installs
  the result and runs `revenant --version` and `xterm+ --version`.
- **GitHub release** downloads every artifact, publishes the release, and
  then deletes the run's build artifacts. Artifacts are also uploaded with
  a 3-day expiry, so a failed run leaves them around briefly for debugging.

Zig is installed by `packaging/install-zig`, which downloads the version
pinned in the workflow's `ZIG_VERSION` from ziglang.org and verifies the
checksum against `https://ziglang.org/download/index.json`. Bump
`ZIG_VERSION` when `tools/fetch-libghostty` moves to a Ghostty revision that
needs a newer Zig.

## Building packages locally

Each job calls a script under `packaging/` that works outside CI too.
All of them need the build dependencies from [Install](../getting-started/install.md),
Zig on `PATH`, and `tools/fetch-libghostty` run first; output lands in
`dist/`.

```sh
packaging/build-tarball 0.2.0
packaging/build-deb 0.2.0      # Debian/Ubuntu: also needs debhelper
packaging/build-rpm 0.2.0      # Fedora: also needs rpm-build
```

## Undoing a test release

```sh
gh release delete v0.2.0 --yes
git push origin :v0.2.0
git tag -d v0.2.0
```

## Known gaps

- The version baked into the binary (`revenant --version`, `meson.build`) is
  not derived from the tag yet. Update the project version in `meson.build`
  before tagging; Meson generates the C version definition from it.
- The packages have no dependency on xterm. Once Revenant reads
  `/usr/share/X11/app-defaults/XTerm` and `XTerm-color` from the
  distribution's xterm package instead of carrying its own copy, the deb and
  rpm need a `Depends`/`Requires` on `xterm` (or on whichever subpackage
  owns those files).
- No macOS or Windows builds; the X11 story there is unresolved.
- The `MIT` license declared in the spec and `debian/copyright` is a
  placeholder until the repository carries a top-level license for Revenant's
  own code.
