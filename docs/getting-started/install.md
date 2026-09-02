---
man: revenant-install
section: 7
manual: revenant
description: packages, dependencies, and building from source
---

# Install

Revenant releases provide Linux tarballs plus Debian and RPM packages. You can
also build it directly from the repository.

The binary is `revenant`; every package and tarball also installs an `xterm+`
symlink pointing to it, so either name starts the same program.

System installation with Meson, the Debian package, or the RPM also installs a
Revenant launcher and application icons in the standard freedesktop locations.
The launcher identifies the window as XTerm so desktop environments associate
it with Revenant's existing X11 application class.

## Install a release

Each [release](https://github.com/toppk/revenant/releases) provides
`revenant-<version>-linux-<arch>.tar.gz` for x86_64 and aarch64, a Debian
package, a Fedora RPM, and an Arch Linux package. The release page lists a
SHA-256 for every file.

The x86_64 builds target the `x86_64-v3` level (AVX2: Intel Haswell 2013 and
later, AMD Zen 2019 and later); aarch64 builds target generic ARMv8. On an
older CPU the terminal exits with an illegal-instruction error — build from
source instead, or open an issue so the floor can be reconsidered.

=== "Fedora"

    ```sh
    sudo dnf install ./revenant-<version>-1.fc*.x86_64.rpm
    ```

=== "Debian / Ubuntu"

    ```sh
    sudo apt install ./revenant_<version>-1_amd64.deb
    ```

=== "Arch Linux"

    ```sh
    sudo pacman -U ./revenant-<version>-1-x86_64.pkg.tar.zst
    ```

=== "Tarball"

    ```sh
    tar xzf revenant-<version>-linux-x86_64.tar.gz
    ./revenant-<version>-linux-x86_64/revenant
    ```

    The archive contains a dynamically linked `revenant`, the `xterm+`
    symlink, the README, and licenses. It needs the runtime X11, Xaw, Xft,
    fontconfig, Cairo, and HarfBuzz libraries but no development packages,
    and it does not install the launcher or icons.

### Verify a download

Every release file carries a build-provenance attestation signed through
GitHub's Sigstore instance. It proves the file was produced by the Revenant
release workflow from a specific commit, rather than uploaded by hand. With
the [GitHub CLI](https://cli.github.com/) installed:

```sh
gh attestation verify revenant_<version>-1_amd64.deb --repo toppk/revenant
```

A successful check prints the workflow, commit, and signer identity. Add
`--format json` to see the full provenance statement, including the exact
build inputs. The attestations are also listed at
<https://github.com/toppk/revenant/attestations>.

## Prerequisites

You need a C toolchain, Meson and Ninja, the X11 development libraries xterm
itself uses (Xlib, Xt, Xaw, Xft, Xrender, and fontconfig), HarfBuzz, and Cairo
1.18 with its FreeType and Xlib backends. Building the terminal core
additionally needs Zig, because `libghostty-vt` is built from Ghostty's source.

=== "Fedora"

    ```sh
    sudo dnf install gcc meson ninja-build zig \
      libX11-devel libXt-devel libXaw-devel libXft-devel libXrender-devel \
      fontconfig-devel cairo-devel harfbuzz-devel libxcb-devel
    ```

=== "Debian / Ubuntu"

    ```sh
    sudo apt install build-essential meson ninja-build zig \
      libx11-dev libxt-dev libxaw7-dev libxft-dev libxrender-dev \
      libfontconfig1-dev libcairo2-dev libharfbuzz-dev libxcb1-dev
    ```

!!! note
    Package names are the usual ones; if your distribution's Zig is older
    than the version Ghostty requires, install Zig from
    [ziglang.org](https://ziglang.org/download/) instead.

## Full build with libghostty-vt

The helper checks out the exact Ghostty revision this project is tested
against, under the git-ignored `upstream/` directory:

```sh
tools/fetch-libghostty
meson setup build-ghostty -Dlibghostty=enabled
meson compile -C build-ghostty
meson test -C build-ghostty
./build-ghostty/revenant
```

The Ghostty checkout is built into a private static `libghostty-vt` archive.
Nothing from Ghostty is added to the Revenant repository.

If you already have a Ghostty checkout, point the build at it instead of
fetching another:

```sh
meson setup build-ghostty -Dlibghostty=enabled \
  -Dlibghostty-source=/path/to/ghostty
```

## UI-only stub build

For work on menus, fonts, resources, or geometry, a build without the
terminal core starts faster and needs no Zig. It does not start a PTY or
parse terminal data:

```sh
meson setup build -Dlibghostty=disabled
meson compile -C build
meson test -C build
./build/revenant
```

## Build type

Builds default to Meson's `debugoptimized` type: debug information is kept,
but the severe terminal-throughput penalty of `-O0` is avoided. A build
directory created before that became the default can be updated with:

```sh
meson configure build-ghostty -Dbuildtype=debugoptimized
```

## Installing alongside xterm

Revenant carries xterm's `XTerm` app-defaults file as a reference but does not
install it: overwriting `/usr/share/X11/app-defaults/XTerm` would conflict
with a distribution's xterm package. When upstream xterm is installed, Xt
loads that existing app-defaults file for Revenant automatically, which is
exactly what you want — see [X resources, explained](../configuration/xresources.md#where-resources-come-from).
