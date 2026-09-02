#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
fixture_root=$3

xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

report=$test_dir/report
log=$test_dir/log
XENVIRONMENT=/dev/null "$fixture_root/run" base "$terminal" -report-config >"$report" 2>"$log"

for resource in limitFontsets limitFontHeight limitFontWidth cursorBlink cursorBlinkXOR
do
    block=$(grep -F -B 1 -- "XTerm*$resource:" "$report")
    if ! printf '%s\n' "$block" | grep -F -q '[supported]'
    then
        echo "$resource is not classified as supported" >&2
        printf '%s\n' "$block" >&2
        exit 1
    fi
done

override_report=$test_dir/override-report
XENVIRONMENT=/dev/null "$fixture_root/run" base "$terminal" \
    -xrm 'XTerm*cursorBlink: always' \
    -xrm 'XTerm*cursorBlinkXOR: false' \
    -report-config >"$override_report" 2>>"$log"

grep -E -q '^XTerm\*cursorBlink:[[:space:]]+always$' "$override_report"
grep -E -q '^XTerm\*cursorBlinkXOR:[[:space:]]+false$' "$override_report"

echo "report-config resource classifications and cursor-blink overrides passed"
