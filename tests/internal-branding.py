#!/usr/bin/env python3
"""Keep public X11 protocol exceptions narrower than general product branding."""

from pathlib import Path
import subprocess
import sys
import tempfile


checker = Path(__file__).resolve().parents[1] / "tools" / "check-internal-branding"
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    (root / "src").mkdir()
    fixture = root / "src" / "sample.c"
    prefix = "_REVE" + "NANT_"
    cases = [
        (
            '"_REVENANT_VERSION" "_REVENANT_CAPABILITIES" "_REVENANT_RELOAD_SIGHUP_V1"',
            True,
        ),
        ('"Reve' + 'nant"', False),
        ("void Reve" + "nantImplementation(void);", False),
        (f'"{prefix}VERSION_SUFFIX"', False),
        (f'"PREFIX{prefix}VERSION"', False),
        (f'"{prefix}RELOAD_SIGHUP_V10"', False),
    ]
    for contents, allowed in cases:
        fixture.write_text(contents + "\n")
        result = subprocess.run(
            [sys.executable, str(checker), str(root)], capture_output=True
        )
        assert (result.returncode == 0) == allowed, (contents, result.stderr.decode())
print(
    "internal branding: exact public X11 identifiers allowed; other branding rejected"
)
