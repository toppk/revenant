"""Canonical application versions and distro-specific package versions."""

import re

PATTERN = re.compile(
    r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-rc\.([1-9][0-9]*))?"
)


def versions(value):
    match = PATTERN.fullmatch(value)
    if not match:
        raise ValueError(
            f"invalid release version {value!r}: expected X.Y.Z or X.Y.Z-rc.N"
        )
    base = ".".join(match.group(i) for i in (1, 2, 3))
    rc = match.group(4)
    return {
        "app": value,
        "base": base,
        "deb": f"{base}~rc.{rc}" if rc else base,
        "rpm": f"{base}~rc.{rc}" if rc else base,
        "arch": f"{base}rc{rc}" if rc else base,
        "prerelease": "true" if rc else "false",
    }
