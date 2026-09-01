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
test_dir=$(mktemp -d)
xvfb_pid=

cleanup()
{
    if test -n "$xvfb_pid"
    then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    rm -rf "$test_dir"
}
trap cleanup EXIT HUP INT TERM

if ! test -x "$fixture_root/run"
then
    echo "SKIP: stage fixtures with tools/stage-font-fixtures first"
    exit 77
fi

"$xvfb" -displayfd 3 -screen 0 1024x768x24 -nolisten unix -listen tcp -ac \
    3>"$test_dir/display" >"$test_dir/xvfb.log" 2>&1 &
xvfb_pid=$!

attempt=0
while ! test -s "$test_dir/display"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$xvfb_pid" 2>/dev/null
    then
        echo "Xvfb did not become ready" >&2
        sed -n '1,80p' "$test_dir/xvfb.log" >&2
        exit 1
    fi
    sleep 0.05
done
DISPLAY=127.0.0.1:$(sed -n '1p' "$test_dir/display")
export DISPLAY

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
snapshots = [record for record in records if record.get("type") == "snapshot"]
if len(snapshots) != 1 or snapshots[0].get("generation") != 2:
    raise SystemExit(f"unexpected font reload snapshot: {snapshots!r}")
PY
