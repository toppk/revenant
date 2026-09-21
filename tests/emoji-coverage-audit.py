#!/usr/bin/env python3
"""Tests for tools/emoji-coverage-audit.py.

Each of these properties was wrong in an earlier version of that tool: a codepoint
written in a comment or an unused variable must not be reported as covered; a case
reference must be validated against a real declaration rather than any occurrence of
its name; the assertion labels must match the helpers a case actually calls; a base
outside the inventory's scope must not be reported as unasserted; and a sequence a
font cannot draw must not be reported as complete merely because shaping dropped its
controls.
"""

import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "font-fixtures-stage"


def load():
    path = ROOT / "tools" / "emoji-coverage-audit.py"
    spec = importlib.util.spec_from_file_location("emoji_coverage_audit", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


audit = load()


class Accounting(unittest.TestCase):
    """A mention is a mention; only the explicit inventory is coverage."""

    def setUp(self):
        self.emoji = {0x1FAE9, 0x1F6E0}
        self.components = set()

    def test_comment_and_unused_literal_are_only_mentions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tests").mkdir()
            (root / "tests" / "xvfb-emoji-invented.sh").write_text(
                "# a comment about \U0001FAE9 which no case runs\n"
                'unused_literal="\\U0001FAE9"\n',
                encoding="utf-8",
            )
            atoms, scalars = audit.mentions(
                root, ("tests/xvfb-emoji-*.sh",), self.emoji, self.components
            )
        self.assertIn((0x1FAE9,), atoms, "the scan should see the written codepoint")
        self.assertIn(0x1FAE9, scalars)
        inventory = audit.asserted_atoms()
        self.assertNotIn(
            (0x1FAE9,),
            inventory,
            "a commented or unused codepoint must not appear as asserted coverage",
        )

    def test_asserted_atoms_carry_cases_and_known_assertions(self):
        inventory = audit.asserted_atoms()
        self.assertTrue(inventory)
        for atom, entry in inventory.items():
            self.assertTrue(entry["cases"], f"{atom!r} has no case")
            self.assertTrue(entry["assertions"] <= audit.ASSERTIONS)

    def test_inventory_names_real_cases(self):
        self.assertEqual(audit.check_inventory(ROOT), [])

    def test_a_deleted_declaration_is_reported_although_a_comment_keeps_the_name(self):
        """The name surviving in a comment must not pass for a declaration."""
        suite = "tests/xvfb-emoji-artwork.sh"
        body = (ROOT / suite).read_text(encoding="utf-8")
        self.assertEqual(audit.declarations(body, suite, "tag-flags-row"), 1)
        lines = [
            line
            for line in body.splitlines(keepends=True)
            if not line.lstrip().startswith("start_sample tag-flags-row ")
        ]
        self.assertEqual(len(lines), len(body.splitlines()) - 1, "the declaration was not removed")
        gutted = "".join(lines) + "# tag-flags-row was removed, but this comment names it\n"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tests").mkdir()
            (root / suite).write_text(gutted, encoding="utf-8")
            (root / "tests/xvfb-emoji-routing.sh").write_text(
                (ROOT / "tests/xvfb-emoji-routing.sh").read_text(encoding="utf-8"),
                encoding="utf-8",
            )
            errors = audit.check_inventory(root)
        self.assertEqual(audit.declarations(gutted, suite, "tag-flags-row"), 0)
        self.assertTrue(
            any("tag-flags-row" in error for error in errors),
            f"a removed declaration went unreported: {errors}",
        )

    def test_a_longer_case_name_is_not_a_declaration_of_a_shorter_one(self):
        body = "start_sample tag-flags-row-extra routing-modern x 16 unicode true true 1\n"
        suite = "tests/xvfb-emoji-artwork.sh"
        self.assertEqual(audit.declarations(body, suite, "tag-flags-row"), 0)
        self.assertEqual(audit.declarations(body, suite, "tag-flags-row-extra"), 1)

    def test_assertion_labels_match_the_helpers_each_case_calls(self):
        """Spot-check the metadata the reviewer found wrong: the artwork suite's
        check_route never inspects glyphs=, so only cases using check_every_route may
        claim shaping, and the tofu-gap case claims no presentation."""
        inventory = audit.asserted_atoms()
        for atom in ((0x1FA75,), (0x1FADF,), (0x1FA8A,)):
            entry = inventory[atom]
            self.assertIn("non-tofu-role", entry["assertions"])
            self.assertNotIn("role", entry["assertions"])
            self.assertNotIn("shaping", entry["assertions"])
        gap = [
            assertions
            for suite, case, atom, assertions in audit.AUTOMATED_CASES
            if case == "newest-bases-text-gap"
        ]
        self.assertTrue(gap)
        for assertions in gap:
            self.assertNotIn("presentation", assertions)
            self.assertNotIn("ink", assertions)
            self.assertIn("tofu-expected", assertions)
            self.assertIn("tofu-ink", assertions)
        shaped = {
            case
            for suite, case, atom, assertions in audit.AUTOMATED_CASES
            if suite == "tests/xvfb-emoji-artwork.sh" and "shaping" in assertions
        }
        self.assertEqual(shaped, {"tag-flags-row", "tag-flags-legacy"})

    def test_out_of_scope_bases_are_not_reported_as_unasserted(self):
        if not (FIXTURES / "data" / "emoji-data.txt").is_file():
            self.skipTest("staged fixtures are absent")
        inventory = audit.build(
            Namespace(fixture_root=FIXTURES, repo_root=ROOT, age="15.0", json=False, check=False)
        )
        hammer = next(row for row in inventory["scalars"] if row["codepoint"] == "U+1F6E0")
        self.assertFalse(hammer["in_scope"], "U+1F6E0 is older than the audited scope")
        self.assertTrue(
            all(row["in_scope"] for row in inventory["scalars"] if row["asserted"]),
            "the inventory should only claim coverage inside its own scope",
        )

    def test_a_renamed_case_is_reported(self):
        original = audit.AUTOMATED_CASES
        audit.AUTOMATED_CASES = original + (
            ("tests/xvfb-emoji-artwork.sh", "case-that-does-not-exist", (0x1F6E0,), ("ink",)),
        )
        try:
            errors = audit.check_inventory(ROOT)
        finally:
            audit.AUTOMATED_CASES = original
        self.assertTrue(any("case-that-does-not-exist" in error for error in errors))

    def test_reported_coverage_is_not_the_scan(self):
        """The two columns must be able to disagree, or one of them is redundant."""
        arguments = Namespace(
            fixture_root=FIXTURES, repo_root=ROOT, age="15.0", json=False, check=False
        )
        if not (FIXTURES / "data" / "emoji-data.txt").is_file():
            self.skipTest("staged fixtures are absent")
        inventory = audit.build(arguments)
        mentioned_only = [
            row
            for row in inventory["scalars"]
            if row["source_mention"] and not row["asserted"]
        ]
        self.assertTrue(
            mentioned_only,
            "every mentioned base is also asserted, so the scan cannot be distinguished",
        )


class Shaping(unittest.TestCase):
    """Production's buffer flags decide these verdicts, not hb-shape's defaults."""

    @classmethod
    def setUpClass(cls):
        if shutil.which("hb-shape") is None:
            raise unittest.SkipTest("hb-shape is unavailable")
        cls.twitter = FIXTURES / "fonts" / "TwitterColorEmoji-SVGinOT.ttf"
        cls.colrv1 = FIXTURES / "fonts" / "Noto-COLRv1.ttf"
        cls.mono = FIXTURES / "fonts" / "NotoEmoji-Regular-3.003.ttf"
        for path in (cls.twitter, cls.colrv1, cls.mono):
            if not path.is_file():
                raise unittest.SkipTest(f"{path.name} is absent")

    def cmap(self, path):
        from fontTools.ttLib import TTFont

        font = TTFont(path, lazy=True, fontNumber=0)
        try:
            return frozenset(font.getBestCmap() or {})
        finally:
            font.close()

    def test_tag_sequences_require_preserved_ignorables(self):
        self.assertTrue(audit.requires_composition(audit.SEQUENCES["tag-flag-gbsct"]))
        self.assertTrue(audit.requires_composition(audit.SEQUENCES["zwj-technologist"]))
        self.assertTrue(audit.requires_composition(audit.SEQUENCES["keycap-1"]))
        self.assertTrue(audit.requires_composition(audit.SEQUENCES["flag-us"]))
        self.assertTrue(audit.requires_composition(audit.SEQUENCES["modifier-wave"]))
        # A variation selector alone does not make an atom composition-requiring,
        # so production shapes it with ignorables removed.
        self.assertFalse(audit.requires_composition(audit.SEQUENCES["vs16-information"]))
        self.assertFalse(audit.requires_composition(audit.SEQUENCES["vs15-hammer"]))

    def test_vanished_controls_cannot_report_a_drawable_flag(self):
        """Twitter has the black flag and no subdivision ligature.

        With the controls removed it shapes to one plausible glyph; with production's
        flags the unsupported payload stays and the glyphs are missing, which is what
        the renderer sees and refuses.
        """
        sequence = audit.SEQUENCES["tag-flag-gbsct"]
        removed = subprocess.run(
            [
                "hb-shape",
                "--no-glyph-names",
                "--no-clusters",
                "--no-positions",
                "--bot",
                "--eot",
                "--cluster-level=1",
                "--remove-default-ignorables",
                str(self.twitter),
                "".join(chr(code) for code in sequence),
            ],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        self.assertEqual(
            len(removed.strip("[]").split("|")),
            1,
            "the false-positive shaping this test exists to exclude has changed",
        )
        state = audit.shaping_state(self.twitter, self.cmap(self.twitter), sequence)
        self.assertTrue(
            state.startswith("missing-glyph"),
            f"expected a missing-glyph verdict for an unsupported tag payload, got {state}",
        )

    def test_supported_sequences_are_still_complete(self):
        for path in (self.colrv1, self.mono):
            state = audit.shaping_state(
                path, self.cmap(path), audit.SEQUENCES["tag-flag-gbsct"]
            )
            self.assertEqual(state, "complete", f"{path.name}: {state}")
        # Reviewed separately: this valid non-RGI sequence does have a distinct
        # ligature in the COLRv1 face, and does not in the monochrome one.
        self.assertEqual(
            audit.shaping_state(self.colrv1, self.cmap(self.colrv1), audit.SEQUENCES["tag-flag-usca"]),
            "complete",
        )
        self.assertTrue(
            audit.shaping_state(
                self.mono, self.cmap(self.mono), audit.SEQUENCES["tag-flag-usca"]
            ).startswith("split"),
        )

    def test_shaping_is_not_presentation_or_ink(self):
        """A complete shaping verdict says nothing about ink or presentation.

        The monochrome face shapes the England flag to one glyph, which is exactly
        why the renderer suites -- not this audit -- decide whether it may serve an
        emoji-presentation atom.
        """
        self.assertEqual(
            audit.shaping_state(self.mono, self.cmap(self.mono), audit.SEQUENCES["tag-flag-gbeng"]),
            "complete",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2, argv=[sys.argv[0]])
