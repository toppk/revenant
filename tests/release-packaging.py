#!/usr/bin/env python3
"""Fast release-contract tests: no package builds, network or publication."""

import base64
import copy
import hashlib
import importlib.machinery
import importlib.util
import json
import os
import re
import shutil
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

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
operations = load("release")
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


class ReleaseOperations(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.version = "0.8.0-rc.1"
        self.repo = "example/project"
        self.client = operations.Release(self.repo, self.root / "evidence")
        self.run = dict(
            id=123,
            run_attempt=1,
            repository={"full_name": self.repo},
            head_repository={"full_name": self.repo},
            head_sha=SHA,
            path=".github/workflows/release.yml",
            event="workflow_dispatch",
            status="completed",
            conclusion="success",
        )
        self.changelog = (
            f"## 0.8.0 — Unreleased\n\nSummary.\n\n### Features\n\n"
            f"- Feature. ([aaaaaaa](https://github.com/{self.repo}/commit/{SHA}))\n"
        )
        self.fixture = self.root / "fixture"
        self.fixture.mkdir()
        ReleaseContracts().make_artifacts(self.fixture)
        self.manifest = dict(
            schema=1,
            version=self.version,
            source_sha=SHA,
            workflow_sha=SHA,
            libghostty_sha="b" * 40,
            run_id="123",
            run_attempt="1",
            artifacts=manifest.inventory(self.fixture, self.version),
        )
        (self.fixture / "release-manifest.json").write_text(json.dumps(self.manifest))
        body = notes.render(
            self.version,
            str(self.fixture),
            source=SHA,
            changelog=self.changelog,
            repo=self.repo,
        )
        self.release = dict(
            id=42,
            tagName="v" + self.version,
            isDraft=True,
            isPrerelease=True,
            assets=[dict(name=p.name) for p in self.fixture.iterdir()],
            body=body,
            url="https://github.com/example/project/releases/tag/test",
        )
        self.calls = []
        self.client.api = Mock(side_effect=self.api)
        self.client.gh = Mock(side_effect=self.gh)

    def api(self, path, data=None, missing=False):
        if path == "actions/runs/123":
            return copy.deepcopy(self.run)
        if path.startswith("contents/"):
            content = (
                self.changelog
                if "CHANGELOG" in path
                else "readonly reference=" + "b" * 40
            )
            return dict(
                encoding="base64", content=base64.b64encode(content.encode()).decode()
            )
        if path.startswith("git/ref/tags/"):
            return dict(object=dict(type="commit", sha=SHA))
        if path == "immutable-releases" or path.startswith("releases/tags/"):
            return dict(enabled=True, immutable=True)
        raise AssertionError(path)

    def gh(self, *args):
        self.calls.append(args)
        if args[:2] == ("release", "view"):
            return json.dumps(self.release)
        if args[:2] == ("release", "download"):
            target = Path(args[args.index("--dir") + 1])
            for path in self.fixture.iterdir():
                shutil.copy2(path, target / path.name)
            return ""
        if args[:2] == ("attestation", "verify"):
            self.assertIn("--source-digest", args)
            self.assertEqual(args[args.index("--source-digest") + 1], SHA)
            self.assertEqual(args[args.index("--signer-digest") + 1], SHA)
            self.assertEqual(
                args[args.index("--signer-workflow") + 1],
                self.repo + "/.github/workflows/release.yml",
            )
            return '[{"verificationResult": "verified"}]'
        raise AssertionError(args)

    def test_run_rejects_foreign_failed_and_wrong_workflow(self):
        original = copy.deepcopy(self.run)
        for updates in (
            dict(repository={"full_name": "other/project"}),
            dict(head_repository={"full_name": "other/project"}),
            dict(path=".github/workflows/other.yml"),
            dict(event="pull_request"),
            dict(conclusion="failure"),
            dict(status="in_progress"),
            dict(head_sha="master"),
        ):
            with self.subTest(updates=updates):
                self.run = dict(original, **updates)
                with self.assertRaises(ValueError):
                    self.client.run(123)
        self.run = original
        self.assertEqual(self.client.run(123)["head_sha"], SHA)

    def test_verify_uses_source_notes_and_saves_all_proofs(self):
        record = self.client.verify(self.version, 123, "draft")
        self.assertEqual(record["source"], SHA)
        self.assertEqual(len(record["digests"]), 6)
        self.assertEqual(
            len(list((self.root / "evidence").rglob("provenance/*.json"))), 6
        )
        self.assertEqual(len(list((self.root / "evidence").rglob("report.json"))), 1)
        expected = hashlib.sha256(
            (self.fixture / "release-manifest.json").read_bytes()
        ).hexdigest()
        self.assertEqual(record["digests"]["release-manifest.json"], expected)

    def test_verify_rejects_wrong_notes_policy_or_asset_count(self):
        original = copy.deepcopy(self.release)
        for updates in (
            dict(body="different notes"),
            dict(isDraft=False),
            dict(isPrerelease=False),
            dict(assets=[]),
        ):
            self.release = dict(original, **updates)
            with self.subTest(updates=updates), self.assertRaises(ValueError):
                self.client.verify(self.version, 123, "draft")
        self.release = original

    def test_verify_rejects_tampered_package(self):
        path = next(self.fixture.glob("*.deb"))
        path.write_bytes(b"tampered")
        with self.assertRaises(ValueError):
            self.client.verify(self.version, 123, "draft")
        self.assertFalse(any(c[:2] == ("attestation", "verify") for c in self.calls))

    def test_verify_rejects_wrong_manifest_run_attempt_and_pin(self):
        for updates in (
            dict(run_id="456"),
            dict(run_attempt="2"),
            dict(libghostty_sha="c" * 40),
            dict(source_sha="c" * 40),
        ):
            (self.fixture / "release-manifest.json").write_text(
                json.dumps(dict(self.manifest, **updates))
            )
            with self.subTest(updates=updates), self.assertRaises(ValueError):
                self.client.verify(self.version, 123, "draft")

    def test_tag_mismatch_stops_before_download(self):
        self.client.tag_source = Mock(return_value="c" * 40)
        with self.assertRaises(ValueError):
            self.client.verify(self.version, 123, "draft")
        self.assertFalse(any(c[:2] == ("release", "download") for c in self.calls))

    def test_failed_attestation_prevents_publication(self):
        original = self.gh

        def gh(*args):
            if args[:2] == ("attestation", "verify"):
                raise ValueError("invalid signature")
            return original(*args)

        self.client.gh = Mock(side_effect=gh)
        with self.assertRaises(ValueError):
            self.client.publish(self.version, 123)
        self.assertFalse(any(c[:2] == ("release", "edit") for c in self.calls))

    def publication_record(self):
        return dict(
            source=SHA, release=self.release, digests={"release-manifest.json": "a"}
        )

    def test_publish_order_and_stable_or_candidate_latest_policy(self):
        for version, candidate in ((self.version, True), ("0.8.0", False)):
            events = []
            record = self.publication_record()
            self.client.verify = Mock(
                side_effect=lambda v, r, state: events.append(state) or record
            )
            self.client.release_info = Mock(return_value=self.release)
            self.client.tag_source = Mock(return_value=SHA)

            def gh(*args):
                events.append("publish")
                self.assertIn("--draft=false", args)
                self.assertIn(f"--prerelease={str(candidate).lower()}", args)
                self.assertIn(f"--latest={str(not candidate).lower()}", args)

            self.client.gh = Mock(side_effect=gh)
            self.client.publish(version, 123)
            self.assertEqual(events, ["draft", "publish", "published"])

    def test_changed_draft_or_tag_prevents_publish(self):
        record = self.publication_record()
        self.client.verify = Mock(return_value=record)
        for release, source in (
            (dict(self.release, body="changed"), SHA),
            (self.release, "c" * 40),
        ):
            self.client.release_info = Mock(return_value=release)
            self.client.tag_source = Mock(return_value=source)
            with self.assertRaises(ValueError):
                self.client.publish(self.version, 123)
        self.assertFalse(self.client.gh.called)

    def test_published_byte_change_is_reported(self):
        before = self.publication_record()
        after = dict(before, digests={"release-manifest.json": "different"})
        self.client.verify = Mock(side_effect=[before, after])
        self.client.release_info = Mock(return_value=self.release)
        self.client.tag_source = Mock(return_value=SHA)
        self.client.gh = Mock()
        with self.assertRaises(ValueError):
            self.client.publish(self.version, 123)

    def test_expired_validation_artifact_stops_draft_before_tag(self):
        self.client.gh = Mock(side_effect=ValueError("artifact expired"))
        with self.assertRaises(ValueError):
            self.client.draft(self.version, 123, False)
        self.assertFalse(
            any(
                call.args[0].startswith("git/")
                for call in self.client.api.call_args_list
            )
        )

    def test_existing_release_stops_draft_before_tag_or_dispatch(self):
        self.client.validation = Mock(return_value=(self.run, self.manifest))
        self.client.ordinary_ci = Mock()
        original = self.api
        self.client.api = Mock(
            side_effect=lambda path, **kw: (
                self.release
                if path.startswith("releases/tags/")
                else original(path, **kw)
            )
        )
        self.client.existing_release = Mock(return_value=self.release)
        self.client.dispatch = Mock()
        with self.assertRaises(ValueError):
            self.client.draft(self.version, 123, False)
        self.assertFalse(self.client.dispatch.called)
        self.assertFalse(
            any(
                call.args[0].startswith("git/")
                for call in self.client.api.call_args_list
            )
        )

    def test_invalid_dispatch_url_is_not_silently_retried(self):
        self.client.gh = Mock(
            return_value="https://github.com/other/project/actions/runs/123"
        )
        with self.assertRaises(ValueError):
            self.client.dispatch(
                "release/0.8", mode="validate", source=SHA, version=self.version
            )
        self.assertEqual(self.client.gh.call_count, 1)

    def test_validate_requires_exact_ci_and_notes_before_dispatch(self):
        self.client.api = Mock(return_value={"sha": SHA})
        self.client.ordinary_ci = Mock(side_effect=ValueError("CI not green"))
        self.client.dispatch = Mock()
        with self.assertRaises(ValueError):
            self.client.validate(self.version, "release/0.8", True)
        self.assertFalse(self.client.dispatch.called)

    def test_ordinary_ci_does_not_accept_success_on_other_commit(self):
        self.client.gh = Mock(
            return_value=json.dumps(
                [dict(headSha="c" * 40, status="completed", conclusion="success")]
            )
        )
        with self.assertRaises(ValueError):
            self.client.ordinary_ci(SHA)

    def test_published_verification_requires_immutability(self):
        self.release["isDraft"] = False
        record = self.client.verify(self.version, 123, "published")
        self.assertEqual(record["state"], "published")
        original = self.api
        self.client.api = Mock(
            side_effect=lambda path, **kw: (
                {"immutable": False}
                if path.startswith("releases/tags/")
                else original(path, **kw)
            )
        )
        with self.assertRaises(ValueError):
            self.client.verify(self.version, 123, "published")

    def test_validate_dispatches_checked_source_and_returns_without_publishing(self):
        self.client.api = Mock(return_value={"sha": SHA})
        self.client.ordinary_ci = Mock()
        self.client.content = Mock(return_value=self.changelog)
        self.client.dispatch = Mock(return_value="123")
        self.client.run = Mock(return_value=self.run)
        self.client.wait = Mock()
        self.client.validate(self.version, "release/0.8", True)
        self.client.dispatch.assert_called_once_with(
            "release/0.8", mode="validate", source=SHA, version=self.version
        )
        self.client.wait.assert_not_called()
        self.client.gh.assert_not_called()

    def test_invalid_notes_stop_validate_before_build(self):
        self.client.api = Mock(return_value={"sha": SHA})
        self.client.ordinary_ci = Mock()
        self.client.content = Mock(
            return_value="## 0.8.0 — Unreleased\n\n- Missing link.\n"
        )
        self.client.dispatch = Mock()
        with self.assertRaises(SystemExit):
            self.client.validate(self.version, "release/0.8", True)
        self.client.dispatch.assert_not_called()

    def test_draft_tag_and_dispatch_are_bound_to_validation_source(self):
        self.client.validation = Mock(return_value=(self.run, self.manifest))
        self.client.ordinary_ci = Mock()
        self.client.content = Mock(return_value=self.changelog)
        mutations = []

        def api(path, data=None, missing=False):
            if path.startswith("releases/tags/"):
                return None
            if path.startswith("git/matching-refs/"):
                return []
            mutations.append((path, data))
            if path == "git/tags":
                return {"sha": "d" * 40}
            if path == "git/refs":
                return {}
            raise AssertionError(path)

        self.client.api = Mock(side_effect=api)
        self.client.existing_release = Mock(return_value=None)
        self.client.dispatch = Mock(return_value="456")
        self.client.run = Mock(return_value=self.run)
        self.client.wait = Mock()
        self.client.draft(self.version, 123, True)
        self.assertEqual(mutations[0][1]["object"], SHA)
        self.assertEqual(
            mutations[1][1], {"ref": "refs/tags/v" + self.version, "sha": "d" * 40}
        )
        self.client.dispatch.assert_called_once_with(
            "v" + self.version, mode="draft", tag="v" + self.version
        )
        self.client.wait.assert_not_called()

    def test_existing_wrong_tag_is_never_replaced(self):
        self.client.validation = Mock(return_value=(self.run, self.manifest))
        self.client.ordinary_ci = Mock()
        self.client.content = Mock(return_value=self.changelog)
        self.client.api = Mock(
            side_effect=lambda path, **kw: (
                None
                if path.startswith("releases/tags/")
                else [{"ref": "refs/tags/v" + self.version}]
            )
        )
        self.client.tag_source = Mock(return_value="c" * 40)
        self.client.existing_release = Mock(return_value=None)
        self.client.dispatch = Mock()
        with self.assertRaises(ValueError):
            self.client.draft(self.version, 123, True)
        self.client.dispatch.assert_not_called()
        self.assertFalse(
            any(call.kwargs.get("data") for call in self.client.api.call_args_list)
        )

    def test_disabled_immutability_prevents_any_publish_verification(self):
        self.client.api = Mock(return_value={"enabled": False})
        self.client.verify = Mock()
        with self.assertRaises(ValueError):
            self.client.publish(self.version, 123)
        self.client.verify.assert_not_called()
        self.client.gh.assert_not_called()

    def test_existing_draft_is_detected_and_network_errors_are_not_absence(self):
        self.client.gh = Mock(
            return_value=json.dumps({"tagName": "v" + self.version, "isDraft": True})
        )
        self.assertTrue(self.client.existing_release("v" + self.version)["isDraft"])
        self.client.gh = Mock(
            side_effect=ValueError("gh release view: release not found")
        )
        self.assertIsNone(self.client.existing_release("v" + self.version))
        self.client.gh = Mock(side_effect=ValueError("network failure"))
        with self.assertRaises(ValueError):
            self.client.existing_release("v" + self.version)

    def test_download_counts_and_asset_order_do_not_invalidate_approval(self):
        before = self.publication_record()
        updated = copy.deepcopy(self.release)
        updated["assets"].reverse()
        for asset in updated["assets"]:
            asset["downloadCount"] = 100
        self.client.verify = Mock(return_value=before)
        self.client.release_info = Mock(return_value=updated)
        self.client.tag_source = Mock(return_value=SHA)
        self.client.gh = Mock()
        self.client.publish(self.version, 123)
        self.client.gh.assert_called_once()

    def test_asset_replacement_changes_approval_identity(self):
        modified = copy.deepcopy(self.release)
        modified["assets"][0]["id"] = "replacement"
        self.assertNotEqual(
            operations.release_identity(self.release),
            operations.release_identity(modified),
        )


if __name__ == "__main__":
    unittest.main()
