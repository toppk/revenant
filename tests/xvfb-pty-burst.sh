#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
python=$3
burst=$4

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug -fn fixed -geometry 80x24 -e "$python" "$burst" \
    >"$test_dir/out" 2>"$test_dir/log" &
terminal_pid=$!

if ! wait "$terminal_pid"
then
    terminal_pid=
    echo "xterm+ failed while processing the split PTY burst" >&2
    sed -n '1,160p' "$test_dir/log" >&2
    exit 1
fi
terminal_pid=

grep -E -q 'pty: drained reads=([2-9]|[1-9][0-9]+) bytes=' "$test_dir/log"
updates=$(grep -c 'render: dirty update requested' "$test_dir/log")
if test "$updates" -ne 2
then
    echo "split PTY burst produced $updates renders instead of 2 complete updates" >&2
    grep -E 'pty: (read bytes|drained)|render: (dirty update|frame mode)' \
        "$test_dir/log" >&2
    exit 1
fi

echo "split PTY burst drained before rendering"
