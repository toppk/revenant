#!/bin/sh

set -eu

if test "$#" -ne 3
then
    echo "usage: $0 XVFB RELOAD-TEST FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
reload_test=$2
fixture_root=$3
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

log=$test_dir/reload.log
if ! "$fixture_root/run" routing "$reload_test" >"$test_dir/stdout" 2>"$log"
then
    sed -n '1,760p' "$log" >&2
    exit 1
fi

python3 - "$log" <<'PY'
import json
import sys
from pathlib import Path

records = [
    json.loads(line)
    for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    if line.startswith("{")
]
if not any(record.get("type") == "warn" and record.get("code") == "FR-RELOADFAIL" for record in records):
    raise SystemExit("font reload report lacks FR-RELOADFAIL")
retained = [record for record in records if record.get("type") == "load" and record.get("status") == "retained"]
if not retained or any(record.get("configured") != "" for record in retained):
    raise SystemExit("font reload report lacks configured/effective retained records")
if any(record.get("limits", {}).get("systemfallback") is not True for record in retained):
    raise SystemExit("failed reload changed the effective systemFallback policy")
active_slot = [
    record
    for record in records
    if record.get("type") == "load"
    and record.get("status") == "active"
    and record.get("slot") == "primary"
    and record.get("fontslot") == 1
    and record.get("generation") == 2
]
if len(active_slot) != 4:
    raise SystemExit(f"reloaded nonzero slot records were lost: {active_slot!r}")
snapshots = [record for record in records if record.get("type") == "snapshot"]
if len(snapshots) != 2 or any(record.get("generation") != 2 for record in snapshots):
    raise SystemExit(f"unexpected font reload snapshot: {snapshots!r}")
PY
