"""Check the embedded probe metadata against TDN's source of truth."""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "hooks"))
from probe_catalog import ROOT, catalog, rendered


class ProbeCatalogTests(unittest.TestCase):
    def test_snapshot_matches_registry_and_navigation(self):
        self.assertEqual(
            (ROOT / "tools/probe/data/features.json").read_text(), rendered()
        )
        self.assertGreater(len(catalog()), 0)
