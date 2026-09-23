#!/usr/bin/env python3
"""Fast release-contract tests: no package builds, network or publication."""

import importlib.machinery
import importlib.util
import os
import re
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "packaging"))
from release_version import versions  # noqa: E402 - repository-local helper


def load(name):
    loader = importlib.machinery.SourceFileLoader(
        name.replace("-", "_"), str(ROOT / "packaging" / name)
    )
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


inputs = load("check-release-inputs")
manifest = load("release-manifest")
notes = load("release-notes")
SHA = "a" * 40
PROGRAM = re.search(
    r"program_name = '([^']+)'", (ROOT / "meson.build").read_text()
).group(1)


class ReleaseContracts(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(
            versions("0.8.0-rc.12"),
            {
                "app": "0.8.0-rc.12",
                "base": "0.8.0",
                "deb": "0.8.0~rc.12",
                "rpm": "0.8.0~rc.12",
                "arch": "0.8.0rc12",
                "prerelease": "true",
            },
        )
        self.assertEqual(versions("0.8.0")["prerelease"], "false")

    def test_invalid_versions(self):
        for value in (
            "v0.8.0",
            "0.8",
            "0.8.0-dev",
            "0.8.0-rc.0",
            "0.8.0-rc.01",
            "0.8.0\n",
            "0.8.0;echo bad",
            "01.8.0",
        ):
            with self.subTest(value=value), self.assertRaises(ValueError):
                versions(value)

    def test_validate_identity(self):
        self.assertEqual(
            inputs.check("validate", SHA, "", "0.8.0-rc.1", SHA, SHA)["sha"], SHA
        )
        for source, head, workflow in (
            ("master", SHA, SHA),
            (SHA, "b" * 40, SHA),
            (SHA, SHA, "b" * 40),
        ):
            with self.assertRaises(ValueError):
                inputs.check("validate", source, "", "0.8.0-rc.1", head, workflow)

    def test_draft_identity(self):
        self.assertEqual(
            inputs.check("draft", "", "v0.8.0-rc.1", "", SHA, SHA)["prerelease"], "true"
        )
        for mode, source, tag, version in (
            ("publish", "", "v0.8.0", ""),
            ("draft", SHA, "v0.8.0", ""),
            ("draft", "", "v0.8.0", "0.9.0"),
            ("validate", SHA, "v0.8.0", "0.8.0"),
        ):
            with self.assertRaises(ValueError):
                inputs.check(mode, source, tag, version, SHA, SHA)

    def test_notes_candidate_and_final(self):
        link = f"([aaaaaaa](https://github.com/example/project/commit/{SHA}))"
        with (
            tempfile.TemporaryDirectory() as directory,
            patch.object(notes, "ROOT", directory),
        ):
            path = Path(directory) / "CHANGELOG.md"
            path.write_text(
                f"## 0.8.0 — Unreleased\n\nSummary.\n\n### Features\n\n- Feature. {link}\n"
            )
            with (
                patch.dict(os.environ, {"RELEASE_SOURCE_SHA": SHA}),
                patch("builtins.print") as output,
            ):
                notes.main(["release-notes", "0.8.0-rc.1", "/nonexistent"])
                rendered = output.call_args.args[0]
                self.assertIn("Release candidate 0.8.0-rc.1", rendered)
                self.assertIn(f"/blob/{SHA}/CHANGELOG.md", rendered)
                self.assertNotIn("Released Unreleased", rendered)
                with self.assertRaises(SystemExit):
                    notes.main(["release-notes", "0.8.0", "/nonexistent"])
                path.write_text(path.read_text().replace("Unreleased", "2026-09-22"))
                notes.main(["release-notes", "0.8.0", "/nonexistent"])
                self.assertIn("Released 2026-09-22", output.call_args.args[0])

    def test_notes_still_require_commit_links(self):
        with (
            tempfile.TemporaryDirectory() as directory,
            patch.object(notes, "ROOT", directory),
        ):
            (Path(directory) / "CHANGELOG.md").write_text(
                "## 0.8.0 — Unreleased\n\n- Missing link.\n"
            )
            with self.assertRaises(SystemExit):
                notes.main(["release-notes", "0.8.0-rc.1", "/nonexistent"])

    def make_artifacts(self, directory):
        names = [
            f"{PROGRAM}-0.8.0-rc.1-linux-x86_64.tar.gz",
            f"{PROGRAM}-0.8.0-rc.1-linux-aarch64.tar.gz",
            f"{PROGRAM}_0.8.0~rc.1-1_amd64.deb",
            f"{PROGRAM}-0.8.0~rc.1-1.fc44.x86_64.rpm",
            f"{PROGRAM}-0.8.0rc1-1-x86_64.pkg.tar.zst",
        ]
        for name in names:
            (directory / name).write_bytes(name.encode())
        return names

    def test_manifest_and_tampering(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = self.make_artifacts(root)
            with (
                patch.dict(
                    os.environ,
                    {
                        "VERSION": "0.8.0-rc.1",
                        "SOURCE_SHA": SHA,
                        "WORKFLOW_SHA": SHA,
                        "RUN_ID": "123",
                        "RUN_ATTEMPT": "1",
                    },
                ),
                patch.object(
                    manifest.subprocess, "check_output", return_value="b" * 40
                ),
            ):
                manifest.main([directory])
            manifest.main(["--check", directory, "0.8.0-rc.1", SHA])
            with self.assertRaises(ValueError):
                manifest.main(["--check", directory, "0.8.0-rc.1", "c" * 40])
            (root / names[0]).write_bytes(b"replaced")
            with self.assertRaises(ValueError):
                manifest.main(["--check", directory, "0.8.0-rc.1", SHA])

    def test_inventory_requires_every_package_and_no_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = self.make_artifacts(root)
            self.assertEqual(len(manifest.inventory(root, "0.8.0-rc.1")), 5)
            (root / "testlog.txt").write_text("diagnostics must not be shipped")
            with self.assertRaises(ValueError):
                manifest.inventory(root, "0.8.0-rc.1")
            (root / "testlog.txt").unlink()
            (root / names[0]).unlink()
            with self.assertRaises(ValueError):
                manifest.inventory(root, "0.8.0-rc.1")


if __name__ == "__main__":
    unittest.main()
