"""Locate the Unicode data the pinned libghostty backend is built from.

libghostty takes its width and segmentation tables from the `uucode` Zig package
named in Ghostty's `build.zig.zon`. The generated frontend tables must come from that
same package, so the package is resolved from the commit `tools/fetch-libghostty`
pins -- read from git, not from whatever the checkout happens to have checked out --
and then looked up in the checkout's Zig package directory. A cache holding other
uucode releases, or a checkout advanced past the pin, cannot change the answer.
"""

import hashlib
import re
import subprocess
from pathlib import Path

HASH_RE = re.compile(r"\.uucode\s*=\s*\.\{[^}]*?\.hash\s*=\s*\"([^\"]+)\"", re.S)


class PinError(Exception):
    """The pinned backend's Unicode data cannot be identified or is missing."""


def pinned_reference(root: Path) -> str:
    result = subprocess.run(
        [str(root / "tools/fetch-libghostty"), "--print-reference"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout.strip():
        raise PinError("cannot read the libghostty pin from tools/fetch-libghostty")
    return result.stdout.strip()


def pinned_package_name(root: Path, reference: str | None = None) -> str:
    """The uucode package hash that the pinned Ghostty commit depends on."""
    reference = reference or pinned_reference(root)
    checkout = root / "upstream/ghostty"
    result = subprocess.run(
        ["git", "-C", str(checkout), "show", f"{reference}:build.zig.zon"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise PinError(
            f"pinned libghostty {reference} is not available in {checkout}; "
            "run tools/fetch-libghostty"
        )
    match = HASH_RE.search(result.stdout)
    if match is None:
        raise PinError(f"no uucode dependency in build.zig.zon at {reference}")
    return match.group(1)


def pinned_ucd_file(root: Path, relative: str, package: str | None = None) -> Path:
    """RELATIVE (for example `ucd/Scripts.txt`) inside the pinned uucode package."""
    package = package or pinned_package_name(root)
    path = root / "upstream/ghostty/zig-pkg" / package / relative
    if not path.is_file():
        raise PinError(
            f"{relative} of the pinned uucode package {package} is not in the Zig "
            "package directory; build libghostty at the pinned revision first"
        )
    return path


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require_pinned_copy(supplied: Path, pinned: Path, option: str) -> None:
    """An explicit input may stand in for the pinned file only as a byte-identical copy.

    Overrides stay useful for a copy elsewhere on disk (a staged fixture, an unpacked
    package), but a file with different contents -- an older release, a hand-edited
    table -- would otherwise let the frontend drift from the backend the pin builds.
    """
    if not supplied.is_file():
        raise PinError(f"{option} {supplied} does not exist")
    supplied_hash, pinned_hash = sha256(supplied), sha256(pinned)
    if supplied_hash != pinned_hash:
        raise PinError(
            f"{option} {supplied} (SHA-256 {supplied_hash}) is not the pinned libghostty "
            f"data {pinned} (SHA-256 {pinned_hash})"
        )
