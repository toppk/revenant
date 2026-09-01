#!/usr/bin/env python3
"""Replay the blessed patch-411 font deposition against Revenant.

The xterm oracle and this conformance runner are deliberately separate.  The
oracle records stock behavior; this runner projects that record onto durable
Revenant outcomes: resolved role file/index, command/resource precedence,
cell geometry, glyph-time role choice, and characterized diagnostics.

Cases whose inherited behavior is not implemented yet must be listed in
KNOWN_GAPS and must fail their relevant assertions.  An unexpected pass is a
failure so a completed feature cannot remain silently exempted.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time


EXPECTED_SCHEMA = 1
EXPECTED_HARNESS = 4
EXPECTED_PATCH = 411
EXPECTED_CASES = 32

# These are inherited-surface gaps, not intentional Revision 5 drift.  Each
# exemption is self-invalidating: once a case passes, the runner fails until
# the entry is removed.
KNOWN_GAPS = {
    "LM-04-limitFontHeight-default-decdhl": "libghostty does not expose DEC line-size state",
    "LM-05-limitFontHeight-cap-decdhl": "libghostty does not expose DEC line-size state",
}

PROBES = {
    "FD-01-doublesize-single": r"\u65e5\u672c",
    "FD-02-doublesize-list-prefixed": r"\u65e5\u672c",
    "FD-03-doublesize-bad-first": r"\u65e5\u672c",
    "FD-04-doublesize-x-first-xft-second": r"\u65e5\u672c",
    "FD-05-doublesize-x11-prefix": r"\u65e5\u672c",
    "FB-01-second-entry-glyph-fallback": r"\u65e5\u672c\u8a9e",
    "FB-02-bad-primary-valid-secondary-glyph-probe": r"\u65e5\u672c\u8a9e",
    "FB-03-third-entry-two-entry-limit": r"\u65e5\u672c\u8a9e",
    "FB-04-fa-supplied-list": r"\u65e5\u672c\u8a9e",
    "ST-01-bold-glyph-fallback": r"\033[1m\u65e5\u672c\u8a9e\033[0m",
    "ST-02-italic-glyph-fallback": r"\033[3m\u65e5\u672c\u8a9e\033[0m",
    "ST-03-bolditalic-glyph-fallback": r"\033[1;3m\u65e5\u672c\u8a9e\033[0m",
    "ST-04-boldFont-xft-entry": r"\033[1m\u65e5\u672c\u8a9e\033[0m",
    "ST-05-wideBoldFont-xft-entry": r"\033[1m\u65e5\u672c\u8a9e\033[0m",
    "WD-01-wide-miss-vs-normal-chain": r"\u65e5\u672c\u8a9e",
    "LM-01-limitFontsets-zero": r"\u65e5\u672c\u8a9e",
    "LM-02-limitFontsets-one": r"\u65e5\U0001f600",
    "LM-03-limitFontsets-two": r"\u65e5\U0001f600",
    "LM-04-limitFontHeight-default-decdhl": (
        r"\033[2J\033[H\033[?25l\033#3DEC\015\012\033#4DEC"
    ),
    "LM-05-limitFontHeight-cap-decdhl": (
        r"\033[2J\033[H\033[?25l\033#3DEC\015\012\033#4DEC"
    ),
}

# (base codepoint, fixture slot selector, Revenant role).  A None selector
# means the deposition says no fallback font may serve that probe.
ROUTES = {
    "FD-01-doublesize-single": [("65E5", "[wide]", "doublesize")],
    "FD-02-doublesize-list-prefixed": [("65E5", "[wide]", "doublesize")],
    "FD-03-doublesize-bad-first": [("65E5", "renderWideNorm", "doublesize-fallback")],
    "FD-04-doublesize-x-first-xft-second": [("65E5", "[wide]", "doublesize")],
    "FD-05-doublesize-x11-prefix": [("65E5", "renderWideNorm", "doublesize-fallback")],
    "FB-01-second-entry-glyph-fallback": [("65E5", "renderFontNorm", "fallback")],
    "FB-02-bad-primary-valid-secondary-glyph-probe": [
        ("65E5", "renderFontNorm", "fallback")
    ],
    "FB-03-third-entry-two-entry-limit": [("65E5", "renderFontNorm", "fallback")],
    "FB-04-fa-supplied-list": [("65E5", "renderFontNorm", "fallback")],
    "ST-01-bold-glyph-fallback": [("65E5", "renderFontBold", "fallback")],
    "ST-02-italic-glyph-fallback": [("65E5", "renderFontItal", "fallback")],
    # Intentional normal-canonical drift: stock's bold-italic chain selects the
    # CJK bold (roman) face.  Revenant keeps the already-selected normal role
    # because that family has no genuine bold-italic instance.
    "ST-03-bolditalic-glyph-fallback": [
        ("65E5", "revenant-normal-canonical", "fallback")
    ],
    "ST-04-boldFont-xft-entry": [("65E5", "renderFontBold", "fallback")],
    "ST-05-wideBoldFont-xft-entry": [("65E5", "renderWideBold", "doublesize-fallback")],
    "WD-01-wide-miss-vs-normal-chain": [
        ("65E5", "renderWideNorm", "doublesize-fallback")
    ],
    "LM-01-limitFontsets-zero": [("65E5", None, None)],
    "LM-02-limitFontsets-one": [
        ("65E5", "renderFontNorm", "fallback"),
        ("1F600", None, None),
    ],
    "LM-03-limitFontsets-two": [
        ("65E5", "renderFontNorm#2", "fallback"),
        ("1F600", "renderFontNorm#6", "fallback"),
    ],
}

LOAD_RE = re.compile(
    r"resolved Xft role=(\S+) slot=(\d+) style=(\S+) entry=(\d+) "
    r"request=(.*?) file=(.*?) index=(-?\d+)"
)
ROUTE_RE = re.compile(
    r"route base=U\+([0-9A-F]+).*? role=(\S+) glyphs=\d+ "
    r"file=(.*?) index=(-?\d+)"
)


def fail(message: str) -> None:
    raise RuntimeError(message)


def validate_fixture(document: dict) -> None:
    if (
        document.get("schema") != EXPECTED_SCHEMA
        or document.get("harness_version") != EXPECTED_HARNESS
        or document.get("oracle", {}).get("patch") != EXPECTED_PATCH
        or not document.get("oracle", {}).get("canonical")
        or len(document.get("cases", {})) != EXPECTED_CASES
        or document.get("environment", {}).get("xft_dpi") != 100
        or not document.get("environment", {})
        .get("probe_contract", {})
        .get("validated")
    ):
        fail("fixture is not the canonical patch-411 v4 32-case deposition")


def start_xvfb(executable: str, root: Path) -> tuple[subprocess.Popen, str]:
    read_fd, write_fd = os.pipe()
    process = subprocess.Popen(
        [
            executable,
            "-displayfd",
            str(write_fd),
            "-screen",
            "0",
            "1024x768x24",
            "-noreset",
            "-nolisten",
            "unix",
            "-listen",
            "tcp",
            "-ac",
        ],
        pass_fds=(write_fd,),
        stdout=(root / "xvfb.stdout").open("w"),
        stderr=(root / "xvfb.stderr").open("w"),
    )
    os.close(write_fd)
    deadline = time.monotonic() + 5
    display = b""
    while time.monotonic() < deadline and not display:
        if process.poll() is not None:
            fail("Xvfb exited before publishing a display")
        readable, _, _ = __import__("select").select([read_fd], [], [], 0.05)
        if readable:
            display = os.read(read_fd, 64).strip()
    os.close(read_fd)
    if not display:
        process.terminate()
        fail("Xvfb did not publish a display")
    return process, f"127.0.0.1:{display.decode()}"


def normalized_file(path: str) -> str:
    return os.path.basename(path)


def fixture_font(case: dict, selector: str) -> dict:
    if selector == "revenant-normal-canonical":
        stock = fixture_font(case, "renderFontBtal")
        result = dict(stock)
        result["file"] = result["file"].replace("-Bold.otf", "-Regular.otf")
        return result
    matches = [font for font in case["xft_fonts"] if selector in font["slot"]]
    if len(matches) != 1:
        fail(
            f"{case['id']}: selector {selector!r} matched {len(matches)} fixture fonts"
        )
    return matches[0]


def expected_loads(case: dict) -> list[tuple[str, str, str, int]]:
    result: list[tuple[str, str, str, int]] = []
    suffixes = {
        "[normal]": ("primary", "normal"),
        "[bold]": ("primary", "bold"),
        "[italic]": ("primary", "italic"),
        "[bold-italic]": ("primary", "bold-italic"),
        "[wide]": ("doublesize", "normal"),
        "[wide-bold]": ("doublesize", "bold"),
        "[wide-italic]": ("doublesize", "italic"),
        "[wide-bold-italic]": ("doublesize", "bold-italic"),
    }
    for font in case["xft_fonts"]:
        for suffix, (role, style) in suffixes.items():
            if font["slot"].endswith(suffix):
                result.append(
                    (role, style, normalized_file(font["file"]), int(font["index"]))
                )
                break
    return result


def parse_loads(log: str) -> dict[tuple[str, str], tuple[str, int]]:
    loads: dict[tuple[str, str], tuple[str, int]] = {}
    for match in LOAD_RE.finditer(log):
        role, slot, style, entry, _request, file, index = match.groups()
        if slot == "0" and entry == "1":
            loads[(role, style)] = (normalized_file(file), int(index))
    return loads


def parse_routes(log: str) -> dict[str, list[tuple[str, str, int]]]:
    routes: dict[str, list[tuple[str, str, int]]] = {}
    for match in ROUTE_RE.finditer(log):
        base, role, file, index = match.groups()
        value = (role, normalized_file(file), int(index))
        routes.setdefault(base, [])
        if value not in routes[base]:
            routes[base].append(value)
    return routes


def case_arguments(case: dict) -> list[str]:
    result: list[str] = []
    for argument in case["input"]["argv_font_args"]:
        result.append(argument.replace("T0Oracle*vt100.", "xterm.vt100."))
    return result


def window_hints(
    xprop: str, display: str, window: str, environment: dict
) -> tuple[list[int], list[int]]:
    completed = subprocess.run(
        [xprop, "-display", display, "-id", window, "WM_NORMAL_HINTS"],
        env=environment,
        check=False,
        capture_output=True,
        text=True,
    )
    increment = re.search(r"resize increment:\s+(\d+)\s+by\s+(\d+)", completed.stdout)
    base = re.search(r"base size:\s+(\d+)\s+by\s+(\d+)", completed.stdout)
    if increment is None or base is None:
        fail(f"cannot read WM_NORMAL_HINTS for window {window}: {completed.stdout!r}")
    return [int(value) for value in increment.groups()], [
        int(value) for value in base.groups()
    ]


def run_case(
    terminal: str,
    fixture_root: Path,
    xprop: str,
    display: str,
    root: Path,
    case: dict,
) -> list[str]:
    case_root = root / case["id"]
    case_root.mkdir()
    window_file = case_root / "window"
    done = case_root / "done"
    probe = bytes(PROBES.get(case["id"], ""), "ascii").decode("unicode_escape")
    child = (
        'printf "%s" "$1"; printf "\\033]2;t0-revenant-ready\\007"; '
        'printf "%s\\n" "$WINDOWID" >"$2"; '
        'while test ! -e "$3"; do sleep 0.05; done'
    )
    argv = [
        str(fixture_root / "run"),
        "cjk-emoji",
        terminal,
        "-log",
        "debug",
        "+sb",
        "-geometry",
        "80x24",
        "-xrm",
        "Xft.dpi: 100",
        "-xrm",
        "xterm.vt100.internalBorder: 2",
        "-xrm",
        "xterm.vt100.renderFont: true",
        "-xrm",
        "xterm.vt100.faceNameDoublesize:",
        "-xrm",
        "xterm.vt100.faceNameEmoji:",
        *case_arguments(case),
        "-e",
        "sh",
        "-c",
        child,
        "sh",
        probe,
        str(window_file),
        str(done),
    ]
    environment = dict(
        os.environ,
        DISPLAY=display,
        XENVIRONMENT="/dev/null",
        XFILESEARCHPATH=str(case_root / "app-defaults" / "%N"),
        XUSERFILESEARCHPATH=str(case_root / "user-app-defaults" / "%N"),
    )
    stdout_path = case_root / "stdout"
    stderr_path = case_root / "stderr"
    with stdout_path.open("w") as stdout, stderr_path.open("w") as stderr:
        process = subprocess.Popen(argv, env=environment, stdout=stdout, stderr=stderr)
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if window_file.exists() and window_file.stat().st_size:
            break
        if process.poll() is not None:
            break
        time.sleep(0.05)
    if not window_file.exists() or not window_file.stat().st_size:
        process.terminate()
        process.wait(timeout=3)
        fail(
            f"{case['id']}: Revenant did not publish a window\n{stderr_path.read_text()}"
        )
    ready = 'title changed bytes=17 preview="t0-revenant-ready"'
    ready_deadline = time.monotonic() + 8
    while time.monotonic() < ready_deadline:
        if ready in stderr_path.read_text():
            break
        if process.poll() is not None:
            break
        time.sleep(0.05)
    else:
        process.terminate()
        process.wait(timeout=3)
        fail(f"{case['id']}: Revenant did not become ready\n{stderr_path.read_text()}")
    if ready not in stderr_path.read_text():
        process.terminate()
        process.wait(timeout=3)
        fail(f"{case['id']}: Revenant exited before ready\n{stderr_path.read_text()}")
    log_before_exit = stderr_path.read_text()
    shell_windows = re.findall(
        r"shell: realized window=(0x[0-9a-fA-F]+)", log_before_exit
    )
    window = shell_windows[-1] if shell_windows else window_file.read_text().strip()
    increment, base = window_hints(xprop, display, window, environment)
    done.touch()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.send_signal(signal.SIGTERM)
        process.wait(timeout=3)
    log = stderr_path.read_text()
    mismatches: list[str] = []
    if increment != case["resize_inc"]:
        mismatches.append(f"resize increment {increment} != {case['resize_inc']}")
    if base != case["base_size"]:
        mismatches.append(f"base size {base} != {case['base_size']}")

    actual_loads = parse_loads(log)
    for role, style, expected_file, expected_index in expected_loads(case):
        actual = actual_loads.get((role, style))
        expected = (expected_file, expected_index)
        if actual != expected:
            mismatches.append(f"{role}/{style} load {actual!r} != {expected!r}")

    routes = parse_routes(log)
    for codepoint, selector, expected_role in ROUTES.get(case["id"], []):
        actual = routes.get(codepoint, [])
        if selector is None:
            if any(role.endswith("fallback") for role, _file, _index in actual):
                mismatches.append(
                    f"U+{codepoint} unexpectedly used fallback: {actual!r}"
                )
            continue
        expected_font = fixture_font(case, selector)
        expected = (
            expected_role,
            normalized_file(expected_font["file"]),
            int(expected_font["index"]),
        )
        if expected not in actual:
            mismatches.append(f"U+{codepoint} route lacks {expected!r}: {actual!r}")

    has_dhl = any("[DHL_TOP]" in font["slot"] for font in case["xft_fonts"])
    if has_dhl and not any(role == "doubleheight" for role, _style in actual_loads):
        mismatches.append("DEC double-height role was not loaded")

    if case["id"] == "FB-03-third-entry-two-entry-limit" and not re.search(
        r"faceName discarded 1 Xft list entries after 2", log
    ):
        mismatches.append("entry-3 discard warning was not emitted")
    if case["id"] == "LM-05-limitFontHeight-cap-decdhl" and not re.search(
        r"limit.*fontheight.*50.*51|fontheight.*51.*50", log, re.IGNORECASE
    ):
        mismatches.append("limitFontHeight cap-at-50 diagnostic was not emitted")
    return mismatches


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xprop", required=True)
    parser.add_argument("--xrdb", required=True)
    parser.add_argument("--revenant", required=True)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--fixture-root", required=True)
    parser.add_argument("--keep-artifacts", action="store_true")
    arguments = parser.parse_args()

    fixture_path = Path(arguments.fixture).resolve()
    fixture_root = Path(arguments.fixture_root).resolve()
    document = json.loads(fixture_path.read_text())
    try:
        validate_fixture(document)
    except RuntimeError as error:
        print(f"T0: {error}", file=sys.stderr)
        return 1
    if not (fixture_root / "run").is_file():
        print("SKIP: stage fixtures with tools/stage-font-fixtures first")
        return 77

    artifact_root = Path(tempfile.mkdtemp(prefix="revenant-t0-"))
    xvfb_process: subprocess.Popen | None = None
    unexpected: list[str] = []
    passed = 0
    expected_failures = 0
    try:
        xvfb_process, display = start_xvfb(arguments.xvfb, artifact_root)
        sterile_environment = dict(os.environ, DISPLAY=display)
        subprocess.run(
            [
                arguments.xrdb,
                "-display",
                display,
                "-nocpp",
                "-load",
            ],
            env=sterile_environment,
            input="Xft.dpi:\t100\nT0Sentinel.value:\ttrue\n",
            check=True,
            capture_output=True,
            text=True,
        )
        for case_id, case in document["cases"].items():
            try:
                mismatches = run_case(
                    arguments.revenant,
                    fixture_root,
                    arguments.xprop,
                    display,
                    artifact_root,
                    case,
                )
            except (RuntimeError, subprocess.SubprocessError) as error:
                mismatches = [str(error)]
            known = KNOWN_GAPS.get(case_id)
            if mismatches and known is not None:
                expected_failures += 1
                print(f"XFAIL {case_id}: {known}")
                for mismatch in mismatches:
                    print(f"  {mismatch}")
            elif mismatches:
                unexpected.append(case_id)
                print(f"FAIL  {case_id}")
                for mismatch in mismatches:
                    print(f"  {mismatch}")
            elif known is not None:
                unexpected.append(case_id)
                print(f"XPASS {case_id}: remove stale KNOWN_GAPS entry")
            else:
                passed += 1
                print(f"PASS  {case_id}")
    finally:
        if xvfb_process is not None and xvfb_process.poll() is None:
            xvfb_process.terminate()
            try:
                xvfb_process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                xvfb_process.kill()
                xvfb_process.wait()
        if not arguments.keep_artifacts and not unexpected:
            shutil.rmtree(artifact_root)

    print(
        f"T0 Revenant projection: {passed} passed, "
        f"{expected_failures} expected gaps, {len(unexpected)} unexpected"
    )
    if unexpected:
        print(f"artifacts retained at {artifact_root}", file=sys.stderr)
        return 1
    if arguments.keep_artifacts:
        print(f"artifacts retained at {artifact_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
