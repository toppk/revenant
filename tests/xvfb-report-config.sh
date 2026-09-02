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
mkdir "$test_dir/empty-home"

report=$test_dir/report
log=$test_dir/log
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$fixture_root/run" base "$terminal" -report-config >"$report" 2>"$log"

for resource in limitFontsets limitFontHeight limitFontWidth cursorBlink cursorBlinkXOR color0 color15
do
    if ! awk -v needle="XTerm*$resource:" '
        index($0, needle) {
            found = 1
            if (previous !~ /\[supported\]/)
                inconsistent = 1
        }
        { previous = $0 }
        END { exit !(found && !inconsistent) }
    ' "$report"
    then
        block=$(grep -F -B 1 -- "XTerm*$resource:" "$report")
        echo "$resource is not classified consistently as supported" >&2
        printf '%s\n' "$block" >&2
        exit 1
    fi
done

grep -F -q 'patch-411 VT100 resource (class Color0).' "$report"
grep -F -q 'patch-411 VT100 resource (class Color15).' "$report"
if grep -E -q '^! XTerm\*color(0|15):[[:space:]]+<unset>$' "$report"
then
    echo "supported ANSI palette resource reported as unset" >&2
    exit 1
fi
grep -E -B 1 '^XTerm\*color0:[[:space:]]+black$' "$report" | \
    grep -q '\[compiled default\] \[supported\]'
grep -E -B 1 '^XTerm\*color15:[[:space:]]+white$' "$report" | \
    grep -q '\[compiled default\] \[supported\]'

override_report=$test_dir/override-report
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$fixture_root/run" base "$terminal" \
    -xrm 'XTerm*cursorBlink: always' \
    -xrm 'XTerm*cursorBlinkXOR: false' \
    -xrm 'XTerm*color0: #123456' \
    -report-config >"$override_report" 2>>"$log"

grep -E -q '^XTerm\*cursorBlink:[[:space:]]+always$' "$override_report"
grep -E -q '^XTerm\*cursorBlinkXOR:[[:space:]]+false$' "$override_report"
grep -B 1 -E '^XTerm\*color0:[[:space:]]+#123456$' "$override_report" | \
    grep -q '\[command line\] \[supported\]'

alias_report=$test_dir/alias-report
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$fixture_root/run" base "$terminal" \
    -fa monospace -fd serif -fe emoji -fb bold -fwb wide-bold \
    -bg '#010203' -fg '#fefefe' -cr red -T cli-title -n cli-icon '#+10+20' \
    -report-config >"$alias_report" 2>>"$log"

for expected in \
    'XTerm\*faceName:[[:space:]]+monospace' \
    'XTerm\*faceNameDoublesize:[[:space:]]+serif' \
    'XTerm\*faceNameEmoji:[[:space:]]+emoji' \
    'XTerm\*boldFont:[[:space:]]+bold' \
    'XTerm\*wideBoldFont:[[:space:]]+wide-bold' \
    'XTerm\*background:[[:space:]]+#010203' \
    'XTerm\*foreground:[[:space:]]+#fefefe' \
    'XTerm\*cursorColor:[[:space:]]+red' \
    'XTerm\*title:[[:space:]]+cli-title' \
    'XTerm\*iconName:[[:space:]]+cli-icon' \
    'XTerm\*iconGeometry:[[:space:]]+\+10\+20'
do
    grep -E -q "^$expected$" "$alias_report"
done

echo "report-config resource classifications, overrides, and aliases passed"
