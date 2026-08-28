# Built in place from a git checkout: rpmbuild -bb --build-in-place --define "version X.Y.Z"
Name:           revenant
Version:        %{version}
Release:        1%{?dist}
Summary:        xterm-compatible X11 terminal emulator built on libghostty-vt
License:        MIT
URL:            https://github.com/toppk/xterm-plus

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
# zig >= 0.16 must be on PATH; Fedora's packaged zig is not used.

%description
Revenant (installed as revenant, with an xterm+ compatibility symlink) preserves xterm's Xt/Athena user interface and X resource contract
while using libghostty-vt as its terminal core.

%build
%meson -Dlibghostty=enabled
%meson_build

%install
%meson_install

%check
%meson_test

%files
%license LICENSES/xterm.txt
%doc README.md
%{_bindir}/revenant
%{_bindir}/xterm+

%changelog
* Tue Aug 25 2026 Kenneth Topp <toppk@bllue.org> - %{version}-1
- Release built by the GitHub release workflow.
