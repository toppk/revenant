#!/usr/bin/env python3

import json
import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"font routing report: {message}")


if len(sys.argv) != 3 or sys.argv[2] not in {"enabled", "disabled"}:
    fail(f"usage: {sys.argv[0]} LOG enabled|disabled")

records = []
for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    if not line.startswith("{"):
        continue
    try:
        records.append(json.loads(line))
    except json.JSONDecodeError as error:
        fail(f"invalid NDJSON: {error}: {line}")

if not records:
    fail("no NDJSON records")
if any(record.get("schema") != 1 or not isinstance(record.get("type"), str) for record in records):
    fail("record missing schema=1 or string type")

snapshots = [record for record in records if record["type"] == "snapshot"]
if len(snapshots) != 1 or snapshots[0].get("collection") != sys.argv[2]:
    fail(f"unexpected snapshot records: {snapshots!r}")
if sys.argv[2] == "disabled":
    if records != snapshots or snapshots[0].get("records") != 0:
        fail("disabled snapshot emitted collected records")
    raise SystemExit(0)

allowed_types = {"load", "route", "warn", "bound", "snapshot"}
if {record["type"] for record in records} - allowed_types:
    fail("unknown record type")

loads = [record for record in records if record["type"] == "load"]
routes = [record for record in records if record["type"] == "route"]
warnings = [record for record in records if record["type"] == "warn"]
if not loads or not routes:
    fail("enabled snapshot lacks load or route records")
for record in loads:
    required = {"slot", "fontslot", "style", "entry", "configured", "effective", "status", "generation", "limits"}
    if not required <= record.keys() or record["entry"] not in {1, 2}:
        fail(f"invalid load record: {record!r}")
    effective = record["effective"]
    if effective is not None and not {"file", "index", "coords"} <= effective.keys():
        fail(f"invalid effective role: {effective!r}")

allowed_misses = {"cmap", "uvs", "shape", "ink", "budget", "truncated"}
allowed_rungs = {"entry1", "entry2", "system", "tofu"}
atoms = {}
for record in routes:
    required = {"atom", "presentation", "widthclass", "slot", "fontslot", "rung", "file", "index", "coords", "misses"}
    if not required <= record.keys():
        fail(f"invalid route record: {record!r}")
    rung = record["rung"]
    if rung not in allowed_rungs and re.fullmatch(r"fallbackFace(?:[1-9]|1[0-6])", rung) is None:
        fail(f"invalid rung: {rung!r}")
    if rung == "tofu":
        if any(record[field] is not None for field in ("file", "index", "coords")):
            fail("tofu role fields are not all null")
    elif record["file"] is None or record["index"] is None or record["coords"] is None:
        fail("served route has null role identity")
    for miss in record["misses"]:
        if miss.get("code") not in allowed_misses or "rung" not in miss:
            fail(f"invalid route miss: {miss!r}")
    if "missesTruncated" in record and record["missesTruncated"] is not True:
        fail(f"invalid route miss bound: {record!r}")
    atoms[record["atom"]] = record

if atoms.get("0041", {}).get("rung") != "entry1":
    fail("ASCII primary route missing")
if atoms.get("65E5", {}).get("rung") not in {"entry2", "system", "fallbackFace1"}:
    fail("CJK fallback route missing")
if atoms.get("10FFFF", {}).get("rung") != "tofu":
    fail("tofu route missing")
if atoms.get("0041", {}).get("styleFallback", {}).get("served") != "normal":
    fail("cache-hit styleFallback update missing from ASCII route")

for record in (record for record in records if record["type"] == "bound"):
    if record.get("code") not in {"FR-REPORTBOUND", "FR-LOADBOUND"}:
        fail(f"unknown report bound: {record!r}")

warning_codes = {record.get("code") for record in warnings}
if "FR-DUPROLE" not in warning_codes or "FR-STYLEFAMILY" not in warning_codes:
    fail(f"expected warnings missing: {warning_codes!r}")
