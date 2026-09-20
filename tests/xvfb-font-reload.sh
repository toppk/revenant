#!/bin/sh

set -eu

if test "$#" -ne 4
then
    echo "usage: $0 XVFB RELOAD-TEST PAINT-TEST FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
reload_test=$2
paint_test=$3
fixture_root=$4
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

log=$test_dir/reload.log
# The helper asserts what a reload changes, so a developer's own ~/.Xdefaults
# naming a font resource would make a deliberate change a no-op.  Nothing outside
# test_dir is touched.
mkdir "$test_dir/empty-home"
if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$fixture_root/run" routing-modern "$reload_test" >"$test_dir/stdout" 2>"$log"
then
    sed -n '1,760p' "$log" >&2
    exit 1
fi

# The route-cache decisions are attributed to the phase that caused them: the
# first route must miss, the repeat must hit, and no phase may fall back through a
# stale entry.  These are the renderer's own log lines, not a test-only counter.
# The real-backend variant carries the painted checks; it asserts its own backend
# identity before sampling anything.
paint_log=$test_dir/paint.log
if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$fixture_root/run" routing-modern "$paint_test" >"$test_dir/paint-stdout" \
    2>"$paint_log"
then
    sed -n '1,200p' "$paint_log" >&2
    exit 1
fi
for phase in paint-automatic paint-after-reload paint-after-rescue paint-after-rollback
do
    if ! grep -F -q "PHASE $phase" "$paint_log"
    then
        echo "painted reload phase $phase never ran" >&2
        exit 1
    fi
done

python3 - "$log" <<'CACHE'
import re
import sys
from pathlib import Path

phases = {}
current = None
for line in Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines():
    marker = re.match(r"PHASE (\S+)", line)
    if marker is not None:
        current = marker.group(1)
        phases.setdefault(current, [])
        continue
    if current is not None and "route-cache" in line and "U+1F6E0" in line:
        phases[current].append(line.split("font: ")[-1])

def require(phase, needle):
    entries = phases.get(phase)
    if entries is None:
        raise SystemExit(f"font reload: phase {phase} never started")
    if not any(needle in entry for entry in entries):
        raise SystemExit(f"font reload: phase {phase} lacks {needle!r}: {entries!r}")

def forbid(phase, needle):
    if any(needle in entry for entry in phases.get(phase, [])):
        raise SystemExit(f"font reload: phase {phase} unexpectedly {needle!r}")

require("first-route", "route-cache miss")
forbid("first-route", "route-cache hit")
require("repeat-route", "route-cache hit")
forbid("repeat-route", "route-cache miss")
# A replaced universe starts with an empty cache, so its first route must miss
# again; a hit there would mean the previous universe's cache survived.
require("after-geometry-reload", "route-cache miss")
require("after-font-reload", "route-cache miss")
# The rejected reload keeps the effective universe, so its cache is still warm.
require("after-rejected-reload", "route-cache hit")
for phase, entries in phases.items():
    for entry in entries:
        if "route-cache stale" in entry:
            raise SystemExit(f"font reload: phase {phase} fell back through a stale entry: {entry}")
CACHE

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
# Three snapshots, taken after the first successful reload, after the two emoji
# reloads, and after the rejected one.  The generations are the evidence: each
# successful reload advances it, and the rejected reload leaves it alone.
generations = [record.get("generation") for record in snapshots]
if generations != [2, 4, 4]:
    raise SystemExit(f"unexpected font reload snapshot generations: {snapshots!r}")
PY
