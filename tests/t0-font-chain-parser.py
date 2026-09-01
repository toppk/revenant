#!/usr/bin/env python3
"""Replay the blessed T0 face-list inputs through Revenant's C parser."""

import json
import subprocess
import sys


FACE_RESOURCES = {
    "faceName",
    "faceNameDoublesize",
    "boldFont",
    "wideBoldFont",
}


def reference_parse(configured: str) -> tuple[list[str], int]:
    entries: list[str] = []
    discarded = 0
    for raw in configured.split(","):
        item = raw.strip()
        if not item or item.startswith("x:"):
            continue
        if item.startswith("xft:"):
            item = item[4:].strip()
            if not item:
                continue
        if len(entries) == 2:
            discarded += 1
        else:
            entries.append(item)
    return entries, discarded


def parser_result(tool: str, configured: str) -> tuple[list[str], int]:
    completed = subprocess.run(
        [tool, configured], check=True, capture_output=True, text=True
    )
    fields: dict[str, str] = {}
    for line in completed.stdout.splitlines():
        key, separator, value = line.partition("=")
        if not separator:
            raise RuntimeError(f"malformed parser output: {line!r}")
        fields[key] = value
    count = int(fields["count"])
    entries = [fields[f"entry{index}"] for index in range(1, count + 1)]
    return entries, int(fields["discarded"])


def inputs(document: dict) -> list[tuple[str, str, str]]:
    result: list[tuple[str, str, str]] = []
    for case_id, case in document["cases"].items():
        case_input = case["input"]
        for resource, value in case_input.get("resources", {}).items():
            if resource in FACE_RESOURCES:
                result.append((case_id, resource, value))
        arguments = case_input.get("argv_font_args", [])
        for index, argument in enumerate(arguments[:-1]):
            if argument == "-fa":
                result.append((case_id, "-fa", arguments[index + 1]))
    return result


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} PARSER FIXTURE", file=sys.stderr)
        return 2
    tool, fixture = sys.argv[1:]
    with open(fixture, encoding="utf-8") as stream:
        document = json.load(stream)
    if (
        document.get("schema") != 1
        or document.get("harness_version") != 4
        or document.get("oracle", {}).get("patch") != 411
        or len(document.get("cases", {})) != 32
    ):
        print("T0 fixture is not the blessed patch-411 v4 32-case record", file=sys.stderr)
        return 1

    checked = 0
    for case_id, source, configured in inputs(document):
        expected = reference_parse(configured)
        actual = parser_result(tool, configured)
        if actual != expected:
            print(
                f"{case_id} {source}: expected {expected!r}, got {actual!r}",
                file=sys.stderr,
            )
            return 1
        checked += 1
    if checked < 32:
        print(f"T0 parser replay was unexpectedly small: {checked}", file=sys.stderr)
        return 1
    print(f"T0 slot-chain parser: {checked} characterized inputs matched")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
