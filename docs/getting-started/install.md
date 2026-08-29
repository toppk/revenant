# Install

Revenant releases provide Linux tarballs plus Debian and RPM packages. You can
also build it directly from the repository.

The binary is `revenant`; every package and tarball also installs an `xterm+`
symlink pointing to it, so either name starts the same program.

System installation with Meson, the Debian package, or the RPM also installs a
Revenant launcher and application icons in the standard freedesktop locations.
The launcher identifies the window as XTerm so desktop environments associate
it with Revenant's existing X11 application class.

## Prerequisites

You need a C toolchain, Meson and Ninja, the X11 development libraries xterm
itself uses (Xlib, Xt, Xaw, Xft, Xrender, and fontconfig), and Cairo 1.18 with
its FreeType and Xlib backends. Building the terminal core additionally needs
Zig, because `libghostty-vt` is built from Ghostty's source.

=== "Fedora"

    ```sh
    sudo dnf install gcc meson ninja-build zig \
      libX11-devel libXt-devel libXaw-devel libXft-devel libXrender-devel \
      fontconfig-devel cairo-devel libxcb-devel
    ```

=== "Debian / Ubuntu"

    ```sh
    sudo apt install build-essential meson ninja-build zig \
      libx11-dev libxt-dev libxaw7-dev libxft-dev libxrender-dev \
      libfontconfig1-dev libcairo2-dev libxcb1-dev
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
