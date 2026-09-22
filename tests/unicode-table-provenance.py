#!/usr/bin/env python3
"""The generated Unicode tables must come from the pinned backend's own data.

Selecting a cached uucode package by its Unicode version is not enough: a Zig cache
keeps every release it has fetched, so a frontend could agree with *a* backend
package without agreeing with the one `tools/fetch-libghostty` pins. Explicit inputs
must not bypass the pin either.

The negative cases build a controlled repository in a temporary directory -- a stub
fetcher, a one-commit Ghostty checkout whose `build.zig.zon` names a package, and
synthetic UCD files -- so they run on a fresh checkout with no older Zig cache. The
real pinned package is checked separately.
"""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import uucode_pin  # noqa: E402

EMOJI_GENERATOR = ROOT / "tools/generate-emoji-table"
HAN_GENERATOR = ROOT / "tools/generate-han-table"


def scripts_txt(version: str, extra: str = "") -> str:
    return (
        f"# Scripts-{version}.txt\n"
        "4E00..9FFF    ; Han # Lo [20992] CJK UNIFIED IDEOGRAPH-4E00..9FFF\n" + extra
    )


def emoji_data(version: str, extra: str = "") -> str:
    return (
        "# emoji-data.txt\n"
        f"# Version: {version}\n"
        "1F600         ; Emoji                # E1.0   [1] grinning face\n"
        "1F600         ; Emoji_Presentation   # E1.0   [1] grinning face\n" + extra
    )


CURRENT = "uucode-test-current"
# Sorts before CURRENT, so a choice by directory order would take it.
OLDER = "uucode-test-a-older"


class ControlledRepository:
    """A throwaway project root whose pin names CURRENT, with OLDER also cached."""

    def __init__(self, pinned_package: str = CURRENT):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name) / "repository"
        checkout = self.root / "upstream/ghostty"
        checkout.mkdir(parents=True)
        (checkout / "build.zig.zon").write_text(
            ".{ .dependencies = .{ .uucode = .{ .url = \"https://example.invalid/u.tar.gz\", "
            f".hash = \"{pinned_package}\", }}, }}, }}\n"
        )
        git = ["git", "-C", str(checkout)]
        subprocess.run([*git, "init", "-q"], check=True)
        subprocess.run([*git, "add", "build.zig.zon"], check=True)
        subprocess.run(
            [*git, "-c", "user.name=t", "-c", "user.email=t@example.invalid", "commit", "-qm", "pin"],
            check=True,
        )
        reference = subprocess.run(
            [*git, "rev-parse", "HEAD"], capture_output=True, text=True, check=True
        ).stdout.strip()
        fetcher = self.root / "tools/fetch-libghostty"
        fetcher.parent.mkdir(parents=True)
        fetcher.write_text(f"#!/bin/sh\necho {reference}\n")
        fetcher.chmod(0o755)
        self.packages = checkout / "zig-pkg"
        self.add_package(CURRENT, "18.0.0", "2B81E         ; Han # Lo  CJK UNIFIED IDEOGRAPH-2B81E\n",
                         "1FADD         ; Emoji                # E18.0  [1] pickle\n"
                         "1FADD         ; Emoji_Presentation   # E18.0  [1] pickle\n")
        self.add_package(OLDER, "17.0", "", "")

    def add_package(self, name, version, han_extra, emoji_extra):
        ucd = self.packages / name / "ucd"
        (ucd / "emoji").mkdir(parents=True)
        (ucd / "Scripts.txt").write_text(scripts_txt(version, han_extra))
        (ucd / "emoji/emoji-data.txt").write_text(emoji_data(version, emoji_extra))

    def ucd(self, package, relative):
        return self.packages / package / "ucd" / relative

    def manifest(self, version):
        path = self.root / f"manifest-{version}.json"
        path.write_text(json.dumps({"unicode_version": version}))
        return path

    def copy_elsewhere(self, source):
        target = Path(self.directory.name) / "elsewhere" / source.name
        target.parent.mkdir(exist_ok=True)
        shutil.copyfile(source, target)
        return target

    def output(self, name):
        return Path(self.directory.name) / name

    def run(self, generator, *arguments):
        return subprocess.run(
            [sys.executable, str(generator), "--project-root", str(self.root), *arguments],
            capture_output=True,
            text=True,
            check=False,
        )

    def close(self):
        self.directory.cleanup()


class ControlledPin(unittest.TestCase):
    def setUp(self):
        self.repository = ControlledRepository()
        self.addCleanup(self.repository.close)

    def test_selection_follows_the_pin_not_the_cache(self):
        scripts = uucode_pin.pinned_ucd_file(self.repository.root, "ucd/Scripts.txt")
        self.assertEqual(scripts.parent.parent.name, CURRENT)

    def test_missing_pinned_package_is_an_error(self):
        absent = ControlledRepository(pinned_package="uucode-test-absent")
        self.addCleanup(absent.close)
        with self.assertRaises(uucode_pin.PinError):
            uucode_pin.pinned_ucd_file(absent.root, "ucd/Scripts.txt")
        result = absent.run(HAN_GENERATOR, "--manifest", str(absent.manifest("18.0")),
                            "--output", str(absent.output("han.h")))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("is not in the Zig package directory", result.stderr)


class HanGenerator(unittest.TestCase):
    def setUp(self):
        self.repository = ControlledRepository()
        self.addCleanup(self.repository.close)

    def test_explicit_older_input_is_refused(self):
        output = self.repository.output("han.h")
        result = self.repository.run(
            HAN_GENERATOR,
            "--input", str(self.repository.ucd(OLDER, "Scripts.txt")),
            "--manifest", str(self.repository.manifest("17.0")),
            "--output", str(output),
        )
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("is not the pinned libghostty data", result.stderr)
        self.assertFalse(output.exists())

    def test_divergent_manifest_is_refused(self):
        output = self.repository.output("han.h")
        result = self.repository.run(
            HAN_GENERATOR,
            "--manifest", str(self.repository.manifest("17.0")),
            "--output", str(output),
        )
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Unicode version mismatch", result.stderr)
        self.assertFalse(output.exists())

    def test_matching_copy_is_accepted(self):
        copy = self.repository.copy_elsewhere(self.repository.ucd(CURRENT, "Scripts.txt"))
        output = self.repository.output("han.h")
        result = self.repository.run(
            HAN_GENERATOR,
            "--input", str(copy),
            "--manifest", str(self.repository.manifest("18.0")),
            "--output", str(output),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("2B81E", output.read_text().upper())


class EmojiGenerator(unittest.TestCase):
    def setUp(self):
        self.repository = ControlledRepository()
        self.addCleanup(self.repository.close)
        self.older = self.repository.ucd(OLDER, "emoji/emoji-data.txt")

    def test_explicit_older_inputs_are_refused(self):
        output = self.repository.output("emoji.h")
        result = self.repository.run(
            EMOJI_GENERATOR,
            "--input", str(self.older),
            "--width-input", str(self.older),
            "--manifest", str(self.repository.manifest("17.0")),
            "--output", str(output),
        )
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("--width-input", result.stderr)
        self.assertIn("is not the pinned libghostty data", result.stderr)
        self.assertFalse(output.exists())

    def test_explicit_older_routing_input_is_refused(self):
        output = self.repository.output("emoji.h")
        result = self.repository.run(
            EMOJI_GENERATOR,
            "--input", str(self.older),
            "--manifest", str(self.repository.manifest("17.0")),
            "--output", str(output),
        )
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("--input", result.stderr)
        self.assertFalse(output.exists())

    def test_matching_copies_are_accepted(self):
        current = self.repository.ucd(CURRENT, "emoji/emoji-data.txt")
        copy = self.repository.copy_elsewhere(current)
        output = self.repository.output("emoji.h")
        result = self.repository.run(
            EMOJI_GENERATOR,
            "--input", str(copy),
            "--width-input", str(copy),
            "--manifest", str(self.repository.manifest("18.0")),
            "--output", str(output),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1FADD", output.read_text().upper())


class RealPin(unittest.TestCase):
    """The repository's own pin, checked against its actual Zig package directory."""

    def setUp(self):
        try:
            self.scripts = uucode_pin.pinned_ucd_file(ROOT, "ucd/Scripts.txt")
            self.emoji = uucode_pin.pinned_ucd_file(ROOT, "ucd/emoji/emoji-data.txt")
        except uucode_pin.PinError as error:
            self.skipTest(f"the pinned libghostty package is not available: {error}")

    def test_pinned_package_is_the_one_build_zig_zon_names(self):
        name = uucode_pin.pinned_package_name(ROOT)
        self.assertEqual(self.scripts.parent.parent.name, name)
        self.assertEqual(self.emoji.parent.parent.parent.name, name)

    def test_staged_fixture_copy_is_the_pinned_data(self):
        staged = ROOT / "font-fixtures-stage/data/emoji-data.txt"
        if not staged.is_file():
            self.skipTest("font fixtures are not staged")
        uucode_pin.require_pinned_copy(staged, self.emoji, "staged fixture")

    def test_generators_accept_the_pinned_data(self):
        for generator in (EMOJI_GENERATOR, HAN_GENERATOR):
            result = subprocess.run(
                [sys.executable, str(generator), "--check"],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
