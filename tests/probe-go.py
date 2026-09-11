#!/usr/bin/env python3
"""Go probe acceptance: real PTYs, simulated terminal replies, no X11 required."""

import base64
import importlib.util
import json
import os
from pathlib import Path
import pty
import re
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time
import unittest
import fcntl

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(
    sys.argv.pop(1) if len(sys.argv) > 1 else ROOT / "build-probe/probe"
).resolve()


def run_probe(args, respond=None, interrupt=None, sig=None, interaction=None):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
    saved = termios.tcgetattr(slave)
    child = subprocess.Popen(
        [str(BINARY), *args], stdin=slave, stdout=slave, stderr=slave
    )
    output = bytearray()
    stopped = False
    pending_replies = []
    deadline = time.monotonic() + 30
    try:
        while child.poll() is None:
            while pending_replies and pending_replies[0][0] <= time.monotonic():
                os.write(master, pending_replies.pop(0)[1])
            if time.monotonic() > deadline:
                raise AssertionError(("probe hung", args, bytes(output[-2000:])))
            if select.select([master], [], [], 0.02)[0]:
                data = os.read(master, 65536)
                output.extend(data)
                if respond:
                    reply = respond(data)
                    if isinstance(reply, list):
                        pending_replies.extend(
                            (time.monotonic() + i * 0.03, part)
                            for i, part in enumerate(reply)
                        )
                    elif reply:
                        os.write(master, reply)
                if interaction:
                    reply = interaction(
                        re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]", b"", bytes(output))
                    )
                    if isinstance(reply, tuple):
                        _, columns, rows = reply
                        fcntl.ioctl(
                            slave,
                            termios.TIOCSWINSZ,
                            struct.pack("HHHH", rows, columns, 0, 0),
                        )
                        child.send_signal(signal.SIGWINCH)
                    elif reply:
                        os.write(master, reply)
            if interrupt and interrupt in output and not stopped:
                if sig:
                    child.send_signal(sig)
                else:
                    os.write(master, b"\x03")
                stopped = True
        while select.select([master], [], [], 0.01)[0]:
            output.extend(os.read(master, 65536))
        return child.returncode, bytes(output), saved == termios.tcgetattr(slave)
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()
        os.close(master)
        os.close(slave)


class Emulator:
    def __init__(self):
        self.pending = b""
        self.modes = {25: True}
        self.pattern = re.compile(rb"\x1b\[\?(\d+)(h|l|\$p)|\x1b\[6n|\x1bP\$q q\x1b\\")

    def __call__(self, data):
        self.pending += data
        out = b""
        end = 0
        for match in self.pattern.finditer(self.pending):
            end = match.end()
            if match[0] == b"\x1b[6n":
                out += b"\x1b[1;5R"
            elif match[0].startswith(b"\x1bP"):
                out += b"\x1bP1$r2 q\x1b\\"
            else:
                mode = int(match[1])
                if match[2] == b"$p":
                    out += f"\x1b[?{mode};{1 if self.modes.get(mode) else 2}$y".encode()
                else:
                    self.modes[mode] = match[2] == b"h"
        self.pending = self.pending[end:][-100:]
        return out


class ProbeAcceptance(unittest.TestCase):
    def test_command_catalog_and_tdn_references(self):
        catalog = json.loads(subprocess.check_output([str(BINARY), "list", "--json"]))
        ids = set(
            re.findall(
                r"^  ([a-z0-9-]+):$",
                (ROOT / "tdn/data/features.yaml").read_text(),
                re.M,
            )
        )
        self.assertGreater(len(catalog), 30)
        for case in catalog:
            self.assertLessEqual(set(case["feature_ids"]), ids, case["path"])
            help_result = subprocess.run(
                [str(BINARY), *case["path"].split(), "--help"], capture_output=True
            )
            self.assertEqual(help_result.returncode, 0, help_result.stderr)
            self.assertIn(b"Cleanup:", help_result.stdout)

    def test_feature_entry_points_and_breadcrumb_navigation(self):
        catalog = json.loads(
            subprocess.check_output([str(BINARY), "features", "--json"])
        )
        self.assertEqual(
            catalog, json.loads((ROOT / "tools/probe/data/features.json").read_text())
        )
        for feature in catalog:
            result = subprocess.run(
                [str(BINARY), feature["id"], "--help"], capture_output=True
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(feature["id"].encode(), result.stdout)
            self.assertIn(" / ".join(feature["breadcrumb"]).encode(), result.stdout)
        result = subprocess.run(
            [str(BINARY), "osc-52-read", "clipboard-clear"], capture_output=True
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn(b"does not exercise feature", result.stderr)
        steps = [
            (b"Home", b"2\r"),
            (b"Home / Window and desktop", b"\r"),
            (b"Home / Window and desktop / OSC 52 clipboard", b"2\r"),
            (b"Home / Window and desktop / OSC 52 clipboard / OSC 52 write", b"2\r"),
            (b"clipboard-clear", b"\x1b[D"),
            (
                b"Home / Window and desktop / OSC 52 clipboard / OSC 52 write",
                b"\x1b[D",
            ),
            (b"Home / Window and desktop / OSC 52 clipboard", b"\x1b[D"),
            (b"Home / Window and desktop", b"\x1b[D"),
            (b"Home", b"q"),
        ]
        offset = 0

        def interact(output):
            nonlocal offset
            if steps and steps[0][0] in output[offset:]:
                marker, reply = steps.pop(0)
                # Consume this complete frame so a detail's case ID cannot satisfy a later screen.
                offset = len(output)
                return reply
            return b""

        code, output, restored = run_probe([], respond=Emulator(), interaction=interact)
        self.assertEqual(code, 0, output[-2000:])
        self.assertFalse(steps, steps)
        self.assertTrue(restored)
        self.assertNotIn(b"\x1b]52;", output)  # browsing a feature must not mutate it
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.json"
            code, output, restored = run_probe(
                [
                    "osc-52-write",
                    "clipboard-set",
                    "--target",
                    "primary",
                    "--text",
                    "slug fixture",
                    "--no-pause",
                    "--output",
                    str(path),
                ],
                respond=Emulator(),
            )
            self.assertEqual(code, 0, output[-2000:])
            record = json.loads(path.read_text())[0]
            self.assertEqual(record["case_id"], "clipboard-set")
            self.assertEqual(record["entry_feature_id"], "osc-52-write")
            self.assertEqual(record["arguments"][:2], ["osc-52-write", "clipboard-set"])
            self.assertIn(b"\x1b]52;p;" + base64.b64encode(b"slug fixture"), output)
            self.assertTrue(restored)

    def test_emoji_topic_shortcut(self):
        result = subprocess.run([str(BINARY), "emoji", "--help"], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"text emoji flags", result.stdout)
        self.assertIn(b"text emoji all", result.stdout)
        code, output, restored = run_probe(
            ["emoji", "flags", "--no-pause", "--timeout", ".01"], respond=Emulator()
        )
        self.assertEqual(code, 0, output[-2000:])
        self.assertIn(b"Test: emoji-flags", output)
        self.assertIn(b"5. Flags", output)
        self.assertNotIn(b"4. ZWJ sequences", output)
        self.assertTrue(restored)

    def test_native_families_run_and_restore_tty(self):
        catalog = json.loads(subprocess.check_output([str(BINARY), "list", "--json"]))
        for case in catalog:
            name = case["path"]
            if name == "colors palette spawn":
                continue  # Explicit external process, tested through argv tests.
            with self.subTest(case=name):
                args = case["command"].split() + ["--timeout", ".005", "--no-pause"]
                if name.startswith("input keyboard") or name == "input mouse":
                    args += ["--seconds", ".06"]
                if name == "rendering sync":
                    args += ["--frames", "1", "--frame-ms", "0", "--pause-ms", "0"]
                if name == "font set":
                    args += ["--font", "fixed"]
                code, output, restored = run_probe(args, respond=Emulator())
                self.assertEqual(code, 0, output[-3000:])
                self.assertTrue(restored)
                self.assertNotIn(b"panic:", output)

    def test_focused_emoji_sections_and_mode_restoration(self):
        for section, count in [("flags", 6), ("mode-2027", 4), ("all", 76)]:
            with (
                self.subTest(section=section),
                tempfile.TemporaryDirectory() as directory,
            ):
                path = Path(directory) / "emoji.json"
                emulator = Emulator()
                emulator.modes[2027] = True
                code, output, restored = run_probe(
                    [
                        "text",
                        "emoji",
                        section,
                        "--no-pause",
                        "--timeout",
                        ".01",
                        "--output",
                        str(path),
                    ],
                    respond=emulator,
                )
                self.assertEqual(code, 0, output[-2000:])
                self.assertTrue(restored)
                self.assertTrue(emulator.modes[2027])
                findings = json.loads(path.read_text())[0]["findings"]
                self.assertEqual(len(findings), count)
                self.assertIn(b"Requested legacy", output)
                self.assertIn(b"Requested cluster", output)
                if section == "flags":
                    self.assertIn(b"5. Flags", output)
                    self.assertNotIn(b"4. ZWJ sequences", output)
                    self.assertNotIn(b"7. Capacity", output)

    def test_interrupts_cleanup_and_restore_tty(self):
        for args, marker, reset in [
            (
                ["notifications", "progress"],
                b"Requested progress state",
                b"\x1b]9;4;0\x1b\\",
            ),
            (
                ["input", "pointer"],
                b"Requested pointer shape",
                b"\x1b]22;default\x1b\\",
            ),
            (["titles", "stack"], b"Requested A", b"\x1b[23;0t"),
            (
                ["rendering", "sync", "--mode", "on", "--hold-ms", "1500"],
                b"\x1b[?2026h",
                b"\x1b[?2026l",
            ),
        ]:
            for sig in [None, signal.SIGTERM, signal.SIGHUP]:
                with self.subTest(args=args, signal=sig):
                    code, output, restored = run_probe(
                        args + ["--timeout", ".01"],
                        respond=Emulator(),
                        interrupt=marker,
                        sig=sig,
                    )
                    self.assertEqual(code, 130, output[-2000:])
                    self.assertIn(reset, output[output.index(marker) :])
                    self.assertTrue(restored)

    def test_reply_bytes_and_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.json"
            sent = False

            def respond(data):
                nonlocal sent
                if b"\x1b]52;p;?\x1b\\" in data and not sent:
                    sent = True
                    return (
                        b"\x1b]52;p;" + base64.b64encode("héllo Ü".encode()) + b"\x1b\\"
                    )
                return b""

            code, output, restored = run_probe(
                [
                    "clipboard",
                    "query",
                    "--target",
                    "primary",
                    "--output",
                    str(path),
                    "--terminal",
                    "fixture",
                    "--terminal-version",
                    "test-1",
                ],
                respond=respond,
            )
            self.assertEqual(code, 0, output)
            self.assertTrue(restored)
            self.assertIn("héllo Ü".encode(), output)
            result = json.loads(path.read_text())[0]
            self.assertEqual(result["outcome"], "unassessed")
            self.assertEqual(result["as-of"], "test-1")
            self.assertEqual(result["feature_ids"], ["osc-52-read"])
            self.assertEqual(
                base64.b64decode(result["events"][0]["bytes_base64"]),
                b"\x1b]52;p;?\x1b\\",
            )

    def test_answerback_preserves_control_bytes(self):
        payload = b"q \r\n\x00\x03\x1b[?12;1$y\x1b"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "answerback.json"

            def respond(data):
                return payload if b"\x05" in data else b""

            code, output, restored = run_probe(
                [
                    "c0-enq-answerback",
                    "identity-answerback",
                    "--timeout",
                    ".1",
                    "--output",
                    str(path),
                ],
                respond=respond,
            )
            self.assertEqual(code, 0, output)
            self.assertTrue(restored)
            self.assertIn(f"Answerback ({len(payload)} bytes)".encode(), output)
            result = json.loads(path.read_text())[0]
            self.assertEqual(result["outcome"], "unassessed")
            received = b"".join(
                base64.b64decode(event["bytes_base64"] or "")
                for event in result["events"]
                if event["direction"] == "received"
            )
            self.assertEqual(received, payload)

    def test_identity_decodes_device_attributes(self):
        replies = {
            b"\x1b[c": b"\x1b[?62;6;21;22c",
            b"\x1b[>c": b"\x1b[>1;700;0c",
            b"\x1b[=c": b"\x1bP!|00000000\x1b\\",
            b"\x1b[>q": b"\x1bP>|xterm+(test)\x1b\\",
        }

        def respond(data):
            return b"".join(reply for request, reply in replies.items() if request in data)

        code, output, restored = run_probe(
            ["csi-da1", "identity-reports", "--timeout", ".2"], respond=respond
        )
        self.assertEqual(code, 0, output)
        self.assertTrue(restored)
        self.assertIn(
            b"DA1 claims level 62 (VT220 class); features: 6 selective erase, "
            b"21 horizontal scrolling, 22 ANSI color",
            output,
        )
        self.assertIn(b"DA2 claims type 1 (VT220), firmware 700, cartridge 0", output)
        self.assertIn(b"xterm+(test)", output)

    def test_slow_escape_prefix_remains_query_data(self):
        sent = False

        def respond(data):
            nonlocal sent
            if b"\x1b[21t" in data and not sent:
                sent = True
                return [b"\x1b", *([b""] * 7), b"]lquiet title\x1b\\"]
            return b""

        code, output, restored = run_probe(
            [
                "csi-21-t-title-report",
                "titles-query",
                "--target",
                "title",
                "--timeout",
                ".5",
            ],
            respond=respond,
        )
        self.assertEqual(code, 0, output)
        self.assertTrue(restored)
        self.assertIn(b"quiet title", output)

    def test_ctrl_c_exits_browser_case(self):
        phase, offset = 0, 0

        def interact(output):
            nonlocal phase, offset
            fresh = output[offset:]
            markers = [
                b"notifications-progress",
                b"Run probe",
                b"Space/Enter: continue",
            ]
            if phase < len(markers) and markers[phase] in fresh:
                phase, offset = phase + 1, len(output)
                return b"\x03" if phase == 3 else b" "
            return b""

        code, output, restored = run_probe(
            ["osc-9-4-progress"], respond=Emulator(), interaction=interact
        )
        self.assertEqual(phase, 3)
        self.assertEqual(code, 130, output)
        self.assertTrue(restored)
        self.assertIn(b"\x1b]9;4;0\x1b\\", output)
        self.assertNotIn(b"Last run:", output)

    def test_fragmented_utf8_title_and_quiet_cpr(self):
        # C1-looking UTF-8 continuation bytes must stay inside the title.
        replies = [b"\x1b]l", "Ü fragmented title".encode(), b"\x1b", b"\\"]

        def title(data):
            if b"\x1b[21t" in data:
                return replies
            return b""

        code, output, restored = run_probe(
            ["titles", "query", "--target", "title"], respond=title
        )
        self.assertEqual(code, 0, output)
        self.assertIn("Ü fragmented title".encode(), output)
        self.assertTrue(restored)
        code, output, restored = run_probe(
            ["text", "fonts", "--regime", "legacy", "--no-pause", "--timeout", ".01"],
            respond=Emulator(),
        )
        self.assertEqual(code, 0, output[-2000:])
        self.assertNotIn(b"CPR:", output)
        self.assertTrue(restored)

    def test_nested_menu_and_case_settings(self):
        for assess in [False, True]:
            with self.subTest(assess=assess):
                self.check_nested_settings(assess)

    def check_nested_settings(self, assess):
        steps = [
            (b"Home", b"2\r"),
            (b"Home / clipboard", b"\x1b[B\x1b[C"),
            (b"Home / clipboard set", b"\x1b[B\r"),
            (b"Home / clipboard set / Settings", b"\r"),
            (b"Home / clipboard set / Target", b"\x1b[B\r"),
            (b"Home / clipboard set / Settings", b"\x1b[B\r"),
            (
                b"Home / clipboard set / Text",
                b"\x7f" * 15 + "words café with spaces\r".encode(),
            ),
            (b"Home / clipboard set / Settings", b"3\r"),
            (b"Home / clipboard set / Settings", b"7\r"),
            (b"Home / clipboard set", b"\r"),
            (b"Probe complete.", b" "),
        ]
        if assess:
            steps.append((b"Home / Assessment / clipboard set", b"\x1b[B\r"))
        steps += [
            (b"Last run:", b"\x1b[D"),
            (b"Home / clipboard", b"\x1b[D"),
            (b"Home", b"q"),
        ]
        offset = 0

        def interact(output):
            nonlocal offset
            if steps and steps[0][0] in output[offset:]:
                marker, response = steps.pop(0)
                offset = len(output)
                return response
            return b""

        code, output, restored = run_probe(
            ["cases", "--assess"] if assess else ["cases"],
            respond=Emulator(),
            interaction=interact,
        )
        self.assertEqual(code, 0, output[-3000:])
        if not assess:
            self.assertNotIn(b"Home / Assessment", output)
            self.assertIn(b"Last run: completed", output)
        self.assertFalse(steps, steps)
        payload = (
            b"\x1b]52;p;"
            + base64.b64encode("words café with spaces".encode())
            + b"\x07"
        )
        self.assertIn(payload, output)
        self.assertTrue(restored)
        # The browser's mouse capture and alternate screen are relinquished
        # before the case gets terminal ownership.
        launch = output.index(payload)
        before = output[:launch]
        self.assertGreater(before.rfind(b"\x1b[?1000l"), before.rfind(b"\x1b[?1000h"))
        self.assertGreater(before.rfind(b"\x1b[?1006l"), before.rfind(b"\x1b[?1006h"))
        self.assertGreater(before.rfind(b"\x1b[?1049l"), before.rfind(b"\x1b[?1049h"))

    def test_mouse_navigation_and_menu_signal_cleanup(self):
        # Click clipboard (row 6), then set (row 6), then go back with Left.
        steps = [
            (b"Home", b"\x1b[<0;8;6M\x1b[<0;8;6m"),
            (b"Home / clipboard", b"\x1b[<0;8;6M\x1b[<0;8;6m"),
            (b"Home / clipboard set", b"\x1b[D"),
            (b"Home / clipboard", b"\x1b[D"),
            (b"Home", b"q"),
        ]
        offset = 0

        def interact(output):
            nonlocal offset
            if steps and steps[0][0] in output[offset:]:
                marker, response = steps.pop(0)
                offset = len(output)
                return response
            return b""

        code, output, restored = run_probe(
            ["cases"], respond=Emulator(), interaction=interact
        )
        self.assertEqual(code, 0, output[-2000:])
        self.assertFalse(steps)
        self.assertTrue(restored)
        self.assertNotIn(b"\x1b]52;", output)
        for sig in [None, signal.SIGTERM, signal.SIGHUP]:
            with self.subTest(signal=sig):
                emulator = Emulator()
                code, output, restored = run_probe(
                    [], respond=emulator, interrupt=b"Home", sig=sig
                )
                self.assertEqual(code, 130, output[-2000:])
                self.assertTrue(restored)
                ending = output[output.index(b"Home") :]
                self.assertIn(b"\x1b[?1000l", ending)
                self.assertIn(b"\x1b[?1006l", ending)
                self.assertIn(b"\x1b[?25h", ending)
                self.assertIn(b"\x1b[?1049l", ending)

    def test_browser_resize_and_wheel(self):
        steps = [
            (b"Home", ("resize", 20, 8)),
            (b"Resize to at least", ("resize", 100, 30)),
            (b"Home", b"\x1b[<65;8;6M"),
            (b"4 / 6", b"\x1b[Hq"),
        ]
        offset = 0

        def interact(output):
            nonlocal offset
            if steps and steps[0][0] in output[offset:]:
                marker, reply = steps.pop(0)
                offset = len(output)
                return reply
            return b""

        code, output, restored = run_probe([], respond=Emulator(), interaction=interact)
        self.assertEqual(code, 0, output[-2000:])
        self.assertFalse(steps, steps)
        self.assertTrue(restored)

    def test_colors_pages_location_and_breadcrumb_jumps_at_80x24(self):
        phase = 0
        offset = 0
        pages = []

        def interact(output):
            nonlocal phase, offset
            fresh = output[offset:]
            if phase == 0 and b"colors-samples" in fresh:
                phase, offset = 1, len(output)
                return b"\r"
            if phase == 1 and b"Run probe" in fresh:
                phase, offset = 2, len(output)
                return b"\r"
            if phase == 2 and b"Space/Enter: continue | q/Esc: exit test" in fresh:
                page = fresh[fresh.rfind(b"Test: colors-samples") :]
                pages.append(page)
                offset = len(output)
                if len(pages) == 4:
                    phase = 3
                return b"\r" if len(pages) % 2 == 0 else b" "
            if phase == 3 and b"Last run: completed" in fresh:
                phase, offset = 4, len(output)
                return b"4\r"  # Test location
            if phase == 4 and b"Space/Enter: return to test | q/Esc: back" in fresh:
                self.assertIn(b"Test: colors-samples", fresh)
                self.assertIn(b"Location: Text, emoji and rendering", fresh)
                self.assertIn(b"Command: probe csi-sgr colors-samples", fresh)
                phase, offset = 5, len(output)
                return b" "
            if phase == 5 and b"Run probe" in fresh:
                phase, offset = 6, len(output)
                return b"\x1b[<0;12;2M"  # Text, emoji and rendering ancestor, several levels up
            if phase == 6 and b"Emoji and Unicode widths" in fresh:
                phase, offset = 7, len(output)
                return b"\x1b[<0;4;2M"  # Home
            if phase == 7 and b"Window and desktop" in fresh:
                phase = 8
                return b"q"
            return b""

        code, output, restored = run_probe(
            ["csi-sgr"], respond=Emulator(), interaction=interact
        )
        self.assertEqual(code, 0, output[-2000:])
        self.assertEqual(phase, 8)
        self.assertTrue(restored)
        self.assertNotIn(b"Probe complete.", output)
        self.assertNotIn(b"Inspect the result", output)
        self.assertEqual(len(pages), 4)
        for page, title in zip(
            pages,
            [
                b"SGR styles",
                b"16 colors",
                b"256 indexed colors",
                b"Truecolor gradients",
            ],
        ):
            self.assertIn(title, page)
            lines = page.splitlines()
            self.assertLessEqual(len(lines), 24, (title, lines))
            self.assertTrue(all(len(line) < 80 for line in lines), (title, lines))
        # Location text can be selected: menu mouse capture is off at that point.
        pos = output.index(b"Location: Text, emoji and rendering")
        before = output[:pos]
        self.assertGreater(before.rfind(b"\x1b[?1000l"), before.rfind(b"\x1b[?1000h"))

    def test_q_exits_probes_and_runs_cleanup(self):
        cases = [
            (
                ["notifications", "progress"],
                b"Requested progress state",
                b"\x1b]9;4;0\x1b\\",
                b"q",
            ),
            (
                ["input", "pointer"],
                b"Requested pointer shape",
                b"\x1b]22;default\x1b\\",
                b"q",
            ),
            (["titles", "stack"], b"Requested A", b"\x1b[23;0t", b"q"),
            (
                ["rendering", "sync", "--mode", "on", "--hold-ms", "5000"],
                b"Synchronized output requested",
                b"\x1b[?2026l",
                b"q",
            ),
            (
                ["input", "mouse"],
                b"captured as input",
                b"\x1b[?1000l",
                b"\x1b[<0;4;4Mq",
            ),
            (["input", "keyboard", "raw"], b"captured as input", None, b" q"),
            (
                ["input", "keyboard", "cooked"],
                b"captured as input",
                None,
                b"q\n",
            ),
            (
                ["input", "keyboard", "kitty"],
                b"Kitty flags active:",
                b"\x1b[<u",
                b"\x1b[32;1u\x1b[113;1u",
            ),
        ]
        for args, marker, cleanup, key in cases:
            with self.subTest(case=args), tempfile.TemporaryDirectory() as directory:
                sent = False

                def interact(output):
                    nonlocal sent
                    if not sent and marker in output:
                        sent = True
                        return key
                    return b""

                path = Path(directory) / "result.json"
                code, output, restored = run_probe(
                    args + ["--timeout", ".01", "--output", str(path)],
                    respond=Emulator(),
                    interaction=interact,
                )
                self.assertEqual(code, 0, output[-2000:])
                self.assertTrue(sent)
                self.assertTrue(restored)
                self.assertEqual(json.loads(path.read_text())[0]["outcome"], "stopped")
                if cleanup:
                    self.assertIn(cleanup, output)
                if args == ["input", "keyboard", "raw"]:
                    self.assertIn(b'Input bytes: " "', output)

    def test_q_returns_to_case_menu_without_assessment(self):
        phase, offset = 0, 0

        def interact(output):
            nonlocal phase, offset
            fresh = output[offset:]
            if phase == 0 and b"Run probe" in fresh:
                phase, offset = 1, len(output)
                return b" "
            if phase == 1 and b"Space/Enter: continue | q/Esc: exit test" in fresh:
                phase, offset = 2, len(output)
                return b"q"
            if phase == 2 and b"Last run: stopped" in fresh:
                phase, offset = 3, len(output)
                return b"q"
            if phase == 3 and b"progress" in fresh:
                phase = 4
                return b"q"
            return b""

        # Start in the feature's scenario menu and use Space consistently.
        phase = -1

        def feature_interact(output):
            nonlocal phase, offset
            if phase == -1 and b"notifications-progress" in output:
                phase, offset = 0, len(output)
                return b" "
            return interact(output)

        code, output, restored = run_probe(
            ["osc-9-4-progress", "--assess"],
            respond=Emulator(),
            interaction=feature_interact,
        )
        self.assertEqual(code, 0, output[-2000:])
        self.assertTrue(restored)
        self.assertEqual(phase, 4)
        self.assertNotIn(b"Tester reports", output)
        self.assertNotIn(b"Probe complete.", output)

    def test_menu_text_selection_mode(self):
        for activate, resume in [
            (b"\x1b[12~", b"\x1b"),
            (b"\x1b[<0;5;22M", b"\r"),
            (b"\x1bOQ", b"\x1bOQ"),
        ]:
            with self.subTest(activate=activate, resume=resume):
                phase, offset = 0, 0

                def interact(output):
                    nonlocal phase, offset
                    fresh = output[offset:]
                    if phase == 0 and b"clipboard-set" in fresh:
                        phase, offset = 1, len(output)
                        return b" "
                    if phase == 1 and b"Run probe" in fresh:
                        phase, offset = 2, len(output)
                        return activate
                    if phase == 2 and b"Select text: drag/copy" in fresh:
                        phase, offset = 3, len(output)
                        return b"z" + resume
                    if phase == 3 and b"Run probe" in fresh:
                        phase, offset = 4, len(output)
                        return b"q"
                    if phase == 4 and b"clipboard-set" in fresh:
                        phase = 5
                        return b"q"
                    return b""

                code, output, restored = run_probe(
                    ["osc-52-write"], respond=Emulator(), interaction=interact
                )
                self.assertEqual(code, 0, output[-2000:])
                self.assertTrue(restored)
                self.assertEqual(phase, 5)
                hint = output.index(b"Select text: drag/copy")
                before = output[:hint]
                self.assertGreater(
                    before.rfind(b"\x1b[?1000l"), before.rfind(b"\x1b[?1000h")
                )
                self.assertGreater(
                    before.rfind(b"\x1b[?1006l"), before.rfind(b"\x1b[?1006h")
                )
                # No clear or alternate-screen switch when entering selection.
                release = before.rfind(b"\x1b[?1000l")
                self.assertNotIn(b"\x1b[2J", output[release:hint])
                self.assertNotIn(b"\x1b[?1049l", output[release:hint])
                self.assertIn(b"\x1b[?1000h", output[hint:])
                self.assertNotIn(b"\x1b]52;", output)

    def test_escape_exits_inspection_and_timed_wait(self):
        for args, marker in [
            (["notifications", "progress"], b"Space/Enter: continue"),
            (
                ["rendering", "sync", "--mode", "on", "--hold-ms", "5000"],
                b"Synchronized output requested",
            ),
            (["input", "keyboard", "raw"], b"captured as input"),
        ]:
            with self.subTest(args=args), tempfile.TemporaryDirectory() as directory:
                sent = False

                def interact(output):
                    nonlocal sent
                    if not sent and marker in output:
                        sent = True
                        return b"\x1b"
                    return b""

                path = Path(directory) / "result.json"
                code, output, restored = run_probe(
                    args + ["--output", str(path)],
                    respond=Emulator(),
                    interaction=interact,
                )
                self.assertEqual(code, 0, output[-2000:])
                self.assertTrue(restored)
                self.assertEqual(json.loads(path.read_text())[0]["outcome"], "stopped")

    def test_migrated_samples_and_palettes_match(self):
        sys.path.insert(0, str(ROOT / "tools"))
        from _width_probe import RightMarginSample

        for name in ["emoji", "fonts", "colors"]:
            spec = importlib.util.spec_from_file_location(
                "legacy_" + name, ROOT / f"tools/probe-{name}.py"
            )
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            if name == "colors":
                expected = module.PALETTES
            else:
                expected = []
                for title, samples in module.SECTIONS:
                    entries = []
                    for sample in samples:
                        if isinstance(sample, RightMarginSample):
                            entries.append(
                                dict(
                                    label=sample.label,
                                    text=sample.text,
                                    note=sample.note,
                                    right_margin=True,
                                )
                            )
                        else:
                            entries.append(
                                dict(
                                    label=sample[0],
                                    text=sample[1],
                                    legacy=sorted(sample[2]),
                                    cluster=sorted(sample[3]),
                                    note=sample[4],
                                )
                            )
                    expected.append(dict(title=title, samples=entries))
            actual = json.loads(
                (
                    ROOT
                    / f"tools/probe/data/{'palettes' if name == 'colors' else name}.json"
                ).read_text()
            )
            if name == "emoji":
                actual = [
                    {k: v for k, v in section.items() if k != "id"}
                    for section in actual
                ]
            self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
