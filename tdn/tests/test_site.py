"""Exercise the actual MkDocs hook, generated links, and JSON publication."""

from html.parser import HTMLParser
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "hooks"))
from registry import Registry


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.targets = []

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            target = dict(attrs).get("href", "")
            if target:
                self.targets.append(target)


@unittest.skipUnless(
    shutil.which("mkdocs"), "install tdn/requirements.txt to test the site build"
)
class SiteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.site = Path(cls.temp.name) / "site"
        cls.root = Path(__file__).resolve().parents[1]
        result = subprocess.run(
            [
                "mkdocs",
                "build",
                "--strict",
                "-f",
                str(cls.root / "mkdocs.yml"),
                "-d",
                str(cls.site),
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        cls.registry = Registry(cls.root)

    def test_export_and_all_generated_feature_pages(self):
        exported = json.loads((self.site / "assets/compatibility.json").read_text())
        self.assertEqual(exported, self.registry.export())
        for fid in self.registry.features:
            with self.subTest(feature=fid):
                html = (self.site / "features" / fid / "index.html").read_text()
                self.assertIn(fid, html)
                self.assertIn("Terminal compatibility", html)

    def test_comparison_and_specification_share_records(self):
        comparison = (self.site / "comparison/index.html").read_text()
        specification = (self.site / "osc/hyperlinks/index.html").read_text()
        feature = (self.site / "features/osc-8-hyperlinks/index.html").read_text()
        profile = (self.site / "terminals/revenant/index.html").read_text()
        for html in (comparison, specification):
            self.assertIn('data-feature="osc-8-hyperlinks"', html)
            self.assertIn('data-terminal="revenant"', html)
        notes = self.registry.terminals["revenant"]["features"]["osc-8-hyperlinks"][
            "notes"
        ]
        self.assertIn(notes, feature)
        self.assertIn(notes, profile)
        self.assertIn("Imported · unverified", comparison)
        self.assertNotIn("<!-- tdn:comparison -->", comparison)

    def test_generated_local_links_resolve(self):
        # MkDocs cannot check links emitted as raw HTML, so inspect their output.
        paths = list((self.site / "features").rglob("index.html"))
        paths += [
            self.site / "comparison/index.html",
            self.site / "osc/hyperlinks/index.html",
        ]
        paths += list((self.site / "terminals").rglob("index.html"))
        for path in paths:
            parser = Links()
            parser.feed(path.read_text())
            for target in parser.targets:
                parts = urlsplit(target)
                if (
                    parts.scheme
                    or parts.netloc
                    or not parts.path
                    or parts.path.startswith("/")
                ):
                    continue
                resolved = (path.parent / unquote(parts.path)).resolve()
                with self.subTest(page=path.relative_to(self.site), link=target):
                    self.assertTrue(
                        resolved.exists(), f"missing generated link: {target}"
                    )
