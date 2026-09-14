#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
local_dir="$test_dir/cwd dir/été"
mkdir -p "$local_dir"
missing_dir="$test_dir/missing"
# Three 240-byte components: the percent-encoded URI exceeds libghostty's
# 2048-byte OSC capture, so the core drops the report without a callback.
component=$(printf 'd%.0s' $(seq 1 240))
long_dir="$test_dir/$component/$component/$component"
mkdir -p "$long_dir"

# Percent-encode every byte except '/' so spaces and UTF-8 exercise decoding.
uri_encode()
{
    printf '%s' "$1" | od -An -v -tx1 | tr -s ' \n' '\n' | sed '/^$/d' | while read -r hex
    do
        case $hex in
            2f) printf '/' ;;
            *) printf '%%%s' "$hex" ;;
        esac
    done
}

local_uri=$(uri_encode "$local_dir")
missing_uri=$(uri_encode "$missing_dir")
long_uri=$(uri_encode "$long_dir")
if test "${#long_uri}" -le 2048
then
    echo "the oversized fixture is only ${#long_uri} bytes" >&2
    exit 1
fi
host=$(uname -n)

# The child records the terminal's cwd before and after the reports so the
# test can prove the terminal process never follows the shell.
script=$test_dir/child.sh
cat >"$script" <<EOF
readlink /proc/\$PPID/cwd >"$test_dir/before"
report()
{
    printf '\\033]7;%s\\033\\\\' "\$1"
}
report 'file://localhost$local_uri'
report 'file://$host$local_uri'
report 'file://elsewhere.invalid/tmp'
report 'file://localhost$missing_uri'
report '$local_dir'
report 'file:///tmp/%zz'
report ''
report 'file://localhost/tmp'
report 'file://localhost$long_uri'
report 'file://localhost/tmp'
# One write: an OSC 1337 CurrentDir report followed by the oversized OSC 7.
printf '\\033]1337;CurrentDir=/tmp\\033\\\\\\033]7;%s\\033\\\\' 'file://localhost$long_uri'
sleep 0.5
readlink /proc/\$PPID/cwd >"$test_dir/after"
EOF

log=$test_dir/log
if ! (cd "$test_dir/empty-home" && HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
    XFILESEARCHPATH=/dev/null "$terminal" -debug -e sh "$script" >"$test_dir/out" 2>"$log")
then
    echo "xterm+ failed during the working-directory case" >&2
    sed -n '1,160p' "$log" >&2
    exit 1
fi

expect_order()
{
    if ! grep -F -- 'working directory' "$log" | sed 's/^.*working directory //' >"$test_dir/events"
    then
        echo "no working-directory events were logged" >&2
        sed -n '1,160p' "$log" >&2
        exit 1
    fi
    if ! diff -u "$test_dir/expected" "$test_dir/events" >&2
    then
        echo "working-directory events differ from the expected sequence" >&2
        sed -n '1,160p' "$log" >&2
        exit 1
    fi
}

preview_remote=file://elsewhere.invalid/tmp
preview_invalid='file:///tmp/%zz'
cat >"$test_dir/expected" <<EOF
effect bytes=$((${#local_uri} + 16))
set path=$local_dir
effect bytes=$((${#local_uri} + 7 + ${#host}))
set path=$local_dir
effect bytes=${#preview_remote}
rejected reason=remote host bytes=${#preview_remote} preview="$preview_remote"
effect bytes=$((${#missing_uri} + 16))
rejected reason=not a local directory path=$missing_dir
effect bytes=$(printf '%s' "$local_dir" | wc -c)
set path=$local_dir
effect bytes=${#preview_invalid}
rejected reason=invalid report bytes=${#preview_invalid} preview="$preview_invalid"
effect bytes=0
cleared
effect bytes=20
set path=/tmp
report dropped by the core bytes=$((${#long_uri} + 16)) limit=2048
rejected reason=report dropped bytes=$((${#long_uri} + 16))
effect bytes=20
set path=/tmp
effect bytes=4
set path=/tmp
report dropped by the core bytes=$((${#long_uri} + 16)) limit=2048
rejected reason=report dropped bytes=$((${#long_uri} + 16))
EOF
expect_order

before=$(cat "$test_dir/before")
after=$(cat "$test_dir/after")
if test "$before" != "$test_dir/empty-home" || test "$after" != "$test_dir/empty-home"
then
    echo "the terminal's own working directory changed: before=$before after=$after" >&2
    exit 1
fi

echo "working-directory reports decoded, validated, and left the terminal cwd alone"
