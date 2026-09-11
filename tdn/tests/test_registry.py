"""Data contract regressions; run with python -m unittest discover -s tdn/tests."""

from pathlib import Path
import sys
import tempfile
import unittest

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "hooks"))
from registry import Registry, RegistryError


class RegistryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "data").mkdir()
        (self.root / "docs").mkdir()
        (self.root / "terminals").mkdir()
        (self.root / "docs/spec.md").write_text(
            "# Spec\n\n<!-- tdn:compatibility -->\n"
        )
        self.write(
            "data/specifications.yaml",
            {
                "schema_version": 1,
                "specifications": {
                    "example-spec": {
                        "title": "Example",
                        "url": "https://example.org/spec",
                    }
                },
            },
        )
        self.write(
            "data/features.yaml",
            {
                "schema_version": 1,
                "features": {
                    "osc-8-links": {
                        "title": "Links",
                        "kind": "feature",
                        "category": "osc",
                        "pages": ["spec.md"],
                        "specifications": ["example-spec"],
                        "labels": [],
                    }
                },
            },
        )
        self.record = {
            "status": "supported",
            "as-of": "1.2",
            "review": "reviewed",
            "checked": "2026-09-10",
            "notes": "Default profile",
            "evidence": [{"kind": "test", "url": "https://example.org/tests/1.2"}],
        }
        self.terminal = {
            "schema_version": 1,
            "terminal": "example",
            "name": "Example",
            "features": {"osc-8-links": self.record},
        }
        self.save_terminal()

    def write(self, path, data):
        (self.root / path).write_text(yaml.safe_dump(data, sort_keys=False))

    def save_terminal(self):
        self.write("terminals/example.yaml", self.terminal)

    def test_terminal_added_without_central_catalog(self):
        self.write(
            "terminals/new-terminal.yaml",
            {
                "schema_version": 1,
                "terminal": "new-terminal",
                "name": "New terminal",
                "features": {},
            },
        )
        registry = Registry(self.root)
        self.assertEqual(set(registry.terminals), {"example", "new-terminal"})
        self.assertEqual(registry.terminals["new-terminal"]["features"], {})

    def test_duplicate_keys_fail_instead_of_overwriting_claim(self):
        with (self.root / "terminals/example.yaml").open("a") as stream:
            stream.write("features: {}\n")
        with self.assertRaisesRegex(RegistryError, "duplicate key"):
            Registry(self.root)

    def test_unknown_feature_rejected(self):
        self.terminal["features"]["typo"] = self.terminal["features"].pop("osc-8-links")
        self.save_terminal()
        with self.assertRaisesRegex(RegistryError, "unknown feature"):
            Registry(self.root)

    def test_versions_must_be_quoted_strings(self):
        self.record["as-of"] = 1.2
        self.save_terminal()
        with self.assertRaisesRegex(RegistryError, "quote the as-of"):
            Registry(self.root)

    def test_import_is_not_promoted_to_reviewed(self):
        self.record["evidence"][0]["kind"] = "import"
        self.save_terminal()
        with self.assertRaisesRegex(RegistryError, "import alone"):
            Registry(self.root)

    def test_unversioned_import_and_history_survive_export(self):
        self.record["history"] = [{**self.record, "as-of": None, "review": "imported"}]
        self.save_terminal()
        data = Registry(self.root).export()
        record = data["terminals"]["example"]["features"]["osc-8-links"]
        self.assertEqual(record["as-of"], "1.2")
        self.assertIsNone(record["history"][0]["as-of"])

    def test_review_requires_version_and_date(self):
        for key in ("as-of", "checked"):
            with self.subTest(key=key):
                saved = self.record.pop(key)
                self.save_terminal()
                with self.assertRaises(RegistryError):
                    Registry(self.root)
                self.record[key] = saved

    def test_conflicting_cannot_claim_support(self):
        self.record["review"] = "conflicting"
        self.save_terminal()
        with self.assertRaisesRegex(
            RegistryError, "conflicting claims must be unknown"
        ):
            Registry(self.root)

    def test_unknown_fields_and_unsafe_urls_fail(self):
        self.record["typo"] = True
        self.save_terminal()
        with self.assertRaisesRegex(RegistryError, "unknown fields"):
            Registry(self.root)
        del self.record["typo"]
        self.record["evidence"][0]["url"] = "javascript:alert(1)"
        self.save_terminal()
        with self.assertRaisesRegex(RegistryError, "HTTP"):
            Registry(self.root)

    def test_missing_specification_and_page_fail(self):
        path = self.root / "data/features.yaml"
        data = yaml.safe_load(path.read_text())
        data["features"]["osc-8-links"]["specifications"] = ["missing"]
        self.write("data/features.yaml", data)
        with self.assertRaisesRegex(RegistryError, "unknown specification"):
            Registry(self.root)
        data["features"]["osc-8-links"]["specifications"] = ["example-spec"]
        data["features"]["osc-8-links"]["pages"] = ["../outside.md"]
        self.write("data/features.yaml", data)
        with self.assertRaisesRegex(RegistryError, "missing/invalid page"):
            Registry(self.root)

    def test_reload_reads_changed_files(self):
        self.assertEqual(
            Registry(self.root).terminals["example"]["features"]["osc-8-links"][
                "status"
            ],
            "supported",
        )
        self.record["status"] = "partial"
        self.save_terminal()
        self.assertEqual(
            Registry(self.root).terminals["example"]["features"]["osc-8-links"][
                "status"
            ],
            "partial",
        )

    def test_repository_data(self):
        registry = Registry(Path(__file__).resolve().parents[1])
        self.assertIn("osc-8-hyperlinks", registry.features)
        self.assertIn("text-color-emoji", registry.features)
        self.assertIn("revenant", registry.terminals)
        self.assertIn("xterm", registry.terminals)
        self.assertEqual(
            registry.terminals["revenant"]["features"]["osc-7-working-directory"][
                "review"
            ],
            "reviewed",
        )


if __name__ == "__main__":
    unittest.main()
