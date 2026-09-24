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


def validate_chunk(chunk, body, features, dispatch):
    """Maintenance is explicit; unmarked work retains the feature contract."""
    kinds = re.findall(r"(?m)^\s*Kind: ([^\n]+)", body)
    if len(kinds) > 1 or (kinds and kinds[0] not in {"feature", "maintenance"}):
        raise ValueError(f"{chunk}: invalid work kind")
    kind = kinds[0] if kinds else "feature"
    refs = feature_refs(body)
    if set(refs) - set(features):
        raise ValueError(f"{chunk}: unregistered TDN reference")
    row = re.search(r"(?m)^\| " + re.escape(chunk) + r" \| (.*)$", dispatch)
    if kind == "maintenance":
        if row is not None:
            raise ValueError(
                f"{chunk}: maintenance conflicts with feature dispatch mapping"
            )
        return
    if not refs:
        raise ValueError(f"{chunk}: expected feature references")
    if row is None:
        raise ValueError(f"{chunk}: missing tracked mapping")
    tracked = re.findall(r"/tdn/features/([^/]+)/", row.group(1))
    if refs != tracked:
        raise ValueError(f"{chunk}: feature references differ from tracked mapping")


class WorkKindTests(unittest.TestCase):
    features = {"known", "other"}
    dispatch = "| F1 | [known](/tdn/features/known/) |"

    def test_maintenance_needs_no_feature_mapping(self):
        validate_chunk("R1", "Kind: maintenance", self.features, self.dispatch)
        validate_chunk(
            "R1", "Kind: maintenance\nTDN: `known`", self.features, self.dispatch
        )

    def test_maintenance_references_still_must_exist(self):
        with self.assertRaisesRegex(ValueError, "unregistered"):
            validate_chunk(
                "R1", "Kind: maintenance\nTDN: `typo`", self.features, self.dispatch
            )

    def test_unmarked_work_remains_feature_work(self):
        validate_chunk("F1", "TDN: `known`", self.features, self.dispatch)
        validate_chunk(
            "F1", "Kind: feature\nTDN: `known`", self.features, self.dispatch
        )
        with self.assertRaisesRegex(ValueError, "expected feature references"):
            validate_chunk("R1", "No declaration", self.features, self.dispatch)

    def test_feature_mapping_remains_required_and_exact(self):
        with self.assertRaisesRegex(ValueError, "missing tracked mapping"):
            validate_chunk("F2", "TDN: `known`", self.features, self.dispatch)
        with self.assertRaisesRegex(ValueError, "differ from tracked mapping"):
            validate_chunk("F1", "TDN: `other`", self.features, self.dispatch)
        with self.assertRaisesRegex(ValueError, "unregistered"):
            validate_chunk("F1", "TDN: `typo`", self.features, self.dispatch)

    def test_invalid_or_duplicate_kind_is_rejected(self):
        for body in ("Kind: maintenence", "Kind: maintenance\nKind: feature"):
            with (
                self.subTest(body=body),
                self.assertRaisesRegex(ValueError, "invalid work kind"),
            ):
                validate_chunk("R1", body, self.features, self.dispatch)

    def test_feature_cannot_silently_be_exempted(self):
        with self.assertRaisesRegex(ValueError, "conflicts with feature dispatch"):
            validate_chunk("F1", "Kind: maintenance", self.features, self.dispatch)


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
                validate_chunk(chunk, body, self.features, self.dispatch)


if __name__ == "__main__":
    unittest.main()
