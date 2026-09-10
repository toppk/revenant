#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xvfb=$1
terminal=$2
python=$3
driver=$4
toggle=$5
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/home"

checkpoint()
{
    attempt=0
    while test ! -f "$case_dir/$1.ready"; do
        attempt=$((attempt+1))
        if test "$attempt" -gt 400 || ! kill -0 "$terminal_pid" 2>/dev/null; then
            cat "$case_dir/out" "$log" >&2
            exit 1
        fi
        sleep .02
    done
}

for initial in true false; do
    case_dir=$test_dir/$initial
    mkdir "$case_dir"
    log=$case_dir/log
    HOME="$test_dir/home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 \
        -fg '#102030' -bg '#304050' -cr '#506070' \
        -xrm "XTerm*allowColorOps: $initial" \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' \
        -e "$python" "$driver" "$case_dir" "$initial" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'color-policy window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    checkpoint initial
    if test "$initial" = false; then
        "$toggle" "$window" color
        xtp_wait_for_log "$log" 'terminal: allowColorOps=true' 'initial enable'
    fi
    touch "$case_dir/initial.done"
    checkpoint disable
    "$toggle" "$window" color
    xtp_wait_for_log "$log" 'terminal: allowColorOps=false' 'disable colors'
    touch "$case_dir/disable.done"
    checkpoint enable
    "$toggle" "$window" color
    touch "$case_dir/enable.done"
    xtp_wait_for_title "$log" color-ops-done 'color policy completion' 2000
    wait "$terminal_pid"
    terminal_pid=
    test -f "$case_dir/passed"
done
echo 'Dynamic-color permissions and live menu verified'
