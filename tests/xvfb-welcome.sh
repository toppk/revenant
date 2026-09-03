#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
fixture_root=$3
xrdb=$4
compositor_owner=$5
program=$(basename "$terminal")

xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home" "$test_dir/app-defaults" "$test_dir/no-tools"
mkdir "$test_dir/no-tools/xrdb"

"$compositor_owner" >"$test_dir/compositor.out" 2>"$test_dir/compositor.log" &
aux_pid=$!
attempt=0
while ! grep -q '^ready$' "$test_dir/compositor.out" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$aux_pid" 2>/dev/null
    then
        echo "compositor selection owner did not become ready" >&2
        sed -n '1,80p' "$test_dir/compositor.log" >&2
        exit 1
    fi
    sleep 0.05
done

bare_report=$test_dir/bare-report
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH="$test_dir/app-defaults/%N" \
    TERM_PROGRAM=outside TERM_PROGRAM_VERSION=1 \
    "$fixture_root/run" base /usr/bin/env PATH="$test_dir/no-tools" \
    "$terminal" -xrm 'XTerm*backgroundOpacity: 0.64' -welcome \
    >"$bare_report" 2>"$test_dir/bare-log"

grep -q "^$program welcome$" "$bare_report"
grep -q '^Setup health$' "$bare_report"
grep -q 'XTerm app-defaults: not found; built-in fallbacks active' "$bare_report"
grep -q '\[recommend\] xrdb command: not found' "$bare_report"
grep -q '^Readable starter resources$' "$bare_report"
grep -q '^    XTerm\*renderFont: true$' "$bare_report"
grep -q '^    XTerm\*faceName: monospace$' "$bare_report"
grep -q '^    XTerm\*faceSize: 13$' "$bare_report"
grep -q '^Color palettes$' "$bare_report"
grep -q 'https://terminal.love/' "$bare_report"
grep -q 'ready for Xresources by default' "$bare_report"
grep -q 'Advanced rendering sample withheld: output=redirected, host=outside' "$bare_report"
grep -q '^Support$' "$bare_report"
grep -q '^  architecture: ' "$bare_report"
grep -q "^  $program: " "$bare_report"
grep -q '^  backend: ' "$bare_report"
grep -E -q '^  backend-revision: [0-9a-f]{40}(-dirty)?$' "$bare_report"
grep -q '^  identity: xterm / XTerm$' "$bare_report"
grep -q '^  host-terminal: outside 1$' "$bare_report"
if grep -F -q "$test_dir" "$bare_report"
then
    echo "welcome report leaked its temporary home" >&2
    exit 1
fi

printf 'XTerm*saveLines: 1234\n' | "$xrdb" -merge

configured_report=$test_dir/configured-report
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH="$test_dir/app-defaults/%N" \
    TERM_PROGRAM="$program" TERM_PROGRAM_VERSION=test-host \
    "$fixture_root/run" cjk-emoji "$terminal" -fa 'x:fixed, xft:DejaVu Sans Mono' \
    -fe 'Noto Color Emoji' -fs 13 -welcome \
    >"$configured_report" 2>"$test_dir/configured-log"

if ! grep -q '\[ok\] Renderer: xft' "$configured_report"
then
    echo "configured welcome report did not select Xft" >&2
    sed -n '1,160p' "$configured_report" >&2
    exit 1
fi
grep -q '^  \[ok\] Primary scalable font: DejaVu Sans Mono (faceSize applies)$' \
    "$configured_report"
grep -q '\[ok\] Color emoji coverage: Noto Color Emoji' "$configured_report"
grep -q '\[ok\] Relevant xterm/XTerm server settings: found' "$configured_report"
grep -q 'Advanced rendering sample withheld: output=redirected' "$configured_report"
grep -q "^  host-terminal: $program test-host$" "$configured_report"
if grep -q '^Readable starter resources$' "$configured_report"
then
    echo "welcome report treated an explicit scalable font as unconfigured" >&2
    exit 1
fi

default_size_report=$test_dir/default-size-report
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH="$test_dir/app-defaults/%N" \
    "$fixture_root/run" base "$terminal" -fa 'DejaVu Sans Mono' -welcome \
    >"$default_size_report" 2>"$test_dir/default-size-log"
grep -q '^  \[ok\] Primary scalable font: DejaVu Sans Mono$' "$default_size_report"

custom_class_report=$test_dir/custom-class-report
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH="$test_dir/app-defaults/%N" \
    "$fixture_root/run" base /usr/bin/env PATH="$test_dir/no-tools" \
    "$terminal" -class CustomTerm -welcome \
    >"$custom_class_report" 2>"$test_dir/custom-class-log"
grep -q '^    CustomTerm\*renderFont: true$' "$custom_class_report"
grep -q 'CustomTerm app-defaults: not found; built-in fallbacks active' "$custom_class_report"

resource_conflict_log=$test_dir/resource-conflict-log
if HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH="$test_dir/app-defaults/%N" \
    "$fixture_root/run" base "$terminal" -xrm 'XTerm*reportConfig: true' -welcome \
    >"$test_dir/resource-conflict-report" 2>"$resource_conflict_log"
then
    echo "welcome accepted a reportConfig resource conflict" >&2
    exit 1
fi
grep -q -- '-welcome and reportConfig cannot be combined$' "$resource_conflict_log"

if grep -q 'BadColor' "$test_dir/bare-log"
then
    echo "unrealized ARGB welcome teardown triggered BadColor" >&2
    exit 1
fi

for report in "$bare_report" "$configured_report" "$default_size_report" "$custom_class_report"
do
    if LC_ALL=C grep -q "$(printf '\033')" "$report"
    then
        echo "redirected welcome report contained terminal escapes: $report" >&2
        exit 1
    fi
done

echo "welcome bare/configured diagnosis, host gating, and support summary passed"
