#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
xwininfo=$3
xprop=$4

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
mkdir "$test_dir/app-defaults"

resource_file=$test_dir/app-defaults/CustomTerm
printf '%s\n' \
    'custom*foreground: #040506' \
    'CustomTerm*cursorColor: #070809' >"$resource_file"

HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH="$test_dir/app-defaults/%N" \
    "$terminal" -name custom -class CustomTerm -bg '#010203' -fa monospace +pc \
    -report-config >"$test_dir/report" 2>"$test_dir/report-log"

grep -E -B 1 '^XTerm\*background:[[:space:]]+#010203$' "$test_dir/report" | \
    grep -q '\[command line\] \[supported\]'
grep -E -B 1 '^XTerm\*faceName:[[:space:]]+monospace$' "$test_dir/report" | \
    grep -q '\[command line\] \[supported'
grep -E -B 1 '^XTerm\*foreground:[[:space:]]+#040506$' "$test_dir/report" | \
    grep -q '\[X resources\] \[supported\]'
grep -E -B 1 '^XTerm\*cursorColor:[[:space:]]+#070809$' "$test_dir/report" | \
    grep -q '\[X resources\] \[supported\]'
grep -E -B 1 '^XTerm\*boldColors:[[:space:]]+false$' "$test_dir/report" | \
    grep -q '\[command line\] \[supported\]'
grep -F -q "! resolved CustomTerm app-defaults: $resource_file" "$test_dir/report"

HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH="$test_dir/app-defaults/%N" \
    "$terminal" -nam custom -clas CustomTerm -geo 80x24 -e sh -c \
    'test "$1" = "-bogus" && test "$2" = "-help" && exec sleep 10' \
    sh -bogus -help \
    >"$test_dir/out" 2>"$test_dir/log" &
terminal_pid=$!

window=
attempt=0
while test -z "$window"
do
    window=$($xwininfo -root -tree 2>/dev/null | awk '/"sh"/ { print $1; exit }')
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "xterm+ did not create the custom command-line identity" >&2
        sed -n '1,120p' "$test_dir/log" >&2
        exit 1
    fi
    test -n "$window" || sleep 0.05
done

$xprop -id "$window" WM_CLASS WM_NAME WM_ICON_NAME >"$test_dir/properties"
grep -q '^WM_CLASS(STRING) = "custom", "CustomTerm"$' "$test_dir/properties"
grep -q '^WM_NAME(STRING) = "sh"$' "$test_dir/properties"
grep -q '^WM_ICON_NAME(STRING) = "sh"$' "$test_dir/properties"

echo "command-line resources, application identity, and child title/icon passed"
