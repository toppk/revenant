#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xvfb=$1
terminal=$2
python=$3
driver=$4
xprop=$5
toggle=$6
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/home"

checkpoint()
{
    name=$1
    wanted=$2
    attempt=0
    while test ! -f "$case_dir/$name.ready"; do
        attempt=$((attempt + 1))
        if test "$attempt" -gt 400; then
            cat "$case_dir/out" "$log" >&2
            exit 1
        fi
        sleep 0.02
    done
    "$xprop" -id "$window" WM_NAME > "$case_dir/property"
    grep -Fx "WM_NAME(STRING) = \"$wanted\"" "$case_dir/property"
}

for initial in true false; do
    case_dir=$test_dir/$initial
    mkdir "$case_dir"
    log=$case_dir/log
    HOME="$test_dir/home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 -T startup \
        -xrm 'XTerm*allowWindowOps: true' \
        -xrm "XTerm*allowTitleOps: $initial" \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' \
        -e "$python" "$driver" "$case_dir" "$initial" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'terminal window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    if test "$initial" = true; then
        checkpoint initial first
    else
        checkpoint initial startup
        "$toggle" "$window" title
        xtp_wait_for_log "$log" 'shell: allowTitleOps=true' 'enable initially denied titles'
    fi
    touch "$case_dir/initial.done"
    checkpoint disable current
    "$toggle" "$window" title
    xtp_wait_for_log "$log" 'shell: allowTitleOps=false' 'disable titles'
    touch "$case_dir/disable.done"
    checkpoint enable current
    "$toggle" "$window" title
    touch "$case_dir/enable.done"
    checkpoint restored current
    touch "$case_dir/restored.done"
    wait "$terminal_pid"
    terminal_pid=
    test -f "$case_dir/passed"
done
echo 'Allow Title Ops resources, live menu, reports, and pop policy verified'
