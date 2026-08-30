# Built in place from a git checkout: rpmbuild -bb --build-in-place --define "version X.Y.Z"
Name:           revenant
Version:        %{version}
Release:        1%{?dist}
Summary:        xterm-compatible X11 terminal emulator built on libghostty-vt
License:        MIT
URL:            https://github.com/toppk/revenant

# Debug subpackages do not work with --build-in-place.
%global debug_package %{nil}

BuildRequires:  gcc
BuildRequires:  meson >= 1.3
BuildRequires:  ninja-build
BuildRequires:  python3
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xt)
BuildRequires:  pkgconfig(xaw7)
BuildRequires:  pkgconfig(xmu)
BuildRequires:  pkgconfig(fontconfig)
BuildRequires:  pkgconfig(xft)
BuildRequires:  pkgconfig(xrender)
BuildRequires:  pkgconfig(cairo) >= 1.18
BuildRequires:  pkgconfig(cairo-ft) >= 1.18
BuildRequires:  pkgconfig(cairo-xlib) >= 1.18
BuildRequires:  pkgconfig(harfbuzz)
# zig >= 0.16 must be on PATH; Fedora's packaged zig is not used.

%description
Revenant is an X11 terminal emulator that keeps xterm's Xt/Athena user
interface and X resource contract while using libghostty-vt as its terminal
core. It installs as revenant, with an xterm+ compatibility symlink.

%build
%meson -Dlibghostty=enabled -Dxvfb-tests=enabled -Drelease-version=%{version}
%meson_build

%install
%meson_install

%check
%meson_test
tools/check-release-tests %{_vpath_builddir}

%files
%license LICENSE
%doc README.md
%{_bindir}/revenant
%{_bindir}/xterm+
%{_datadir}/applications/revenant.desktop
%{_datadir}/icons/hicolor/scalable/apps/revenant.svg
%{_datadir}/icons/hicolor/256x256/apps/revenant.png

%changelog
* Tue Aug 25 2026 Kenneth Topp <toppk@bllue.org> - %{version}-1
- Release built by the GitHub release workflow.
