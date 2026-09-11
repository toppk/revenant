"""Keep Revenant's work references attached to registered TDN features."""

from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "hooks"))
from registry import Registry


ROOT = Path(__file__).resolve().parents[2]


def feature_refs(text):
    """Read explicit TDN paragraphs, including wrapped ID lists."""
    return [
        feature
        for paragraph in re.findall(r"(?m)^\s*TDN: ([^\n]*(?:\n[ \t]+`[^\n]*)*)", text)
        for feature in re.findall(r"`([^`]+)`", paragraph)
    ]


@unittest.skipUnless((ROOT / "HANDOFF.md").exists(), "Revenant checkout only")
class WorkReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.features = Registry(ROOT / "tdn").features
        cls.dispatch = (ROOT / "docs/maintainers/dispatch.md").read_text()

    def assert_registered(self, refs):
        self.assertTrue(refs, "Expected explicit feature references")
        self.assertEqual(set(refs) - self.features.keys(), set())

    def test_handoff_references_resolve(self):
        self.assert_registered(feature_refs((ROOT / "HANDOFF.md").read_text()))

    def test_dispatch_feature_links_resolve(self):
        refs = re.findall(
            r"https://toppk.github.io/revenant/tdn/features/([^/]+)/", self.dispatch
        )
        self.assert_registered(refs)

    @unittest.skipUnless((ROOT / "todo.md").exists(), "Local checklist is untracked")
    def test_pending_checklist_chunks_match_tracked_mapping(self):
        text = (ROOT / "todo.md").read_text()
        self.assert_registered(feature_refs(text))
        chunks = re.findall(
            r"(?ms)^- \[ \] \*\*([A-Z][0-9]+) — (.*?)(?=^- \[|^## |\Z)",
            text,
        )
        self.assertTrue(chunks)
        for chunk, body in chunks:
            with self.subTest(chunk=chunk):
                # F1 is followed by general backlog notes in the same section.
                body = re.split(r"\n\S", body, maxsplit=1)[0]
                refs = feature_refs(body)
                self.assert_registered(refs)
                row = re.search(r"(?m)^\| " + chunk + r" \| (.*)$", self.dispatch)
                self.assertIsNotNone(row, f"Missing tracked mapping for {chunk}")
                tracked = re.findall(r"/tdn/features/([^/]+)/", row.group(1))
                self.assertEqual(refs, tracked)


if __name__ == "__main__":
    unittest.main()
