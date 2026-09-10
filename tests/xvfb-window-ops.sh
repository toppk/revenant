#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xvfb=$1
terminal=$2
python=$3
driver=$4
toggle=$5
reader=$6
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS
for policy in default setonly
do
    case_dir=$test_dir/$policy
    mkdir "$case_dir"
    log=$case_dir/log
    disallowed='GetSelection,SetSelection'
    expected=enabled
    if test "$policy" = setonly
    then
        disallowed=GetSelection
        expected=after
    fi
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed \
        -xrm "XTerm*disallowedWindowOps: $disallowed" \
        -e "$python" "$driver" "$case_dir" "$policy" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    for number in 1 2
    do
        xtp_wait_for_title "$log" "window-ops-toggle-$number" "toggle $number"
        window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
        "$toggle" "$window"
        state=true
        test "$number" = 1 || state=false
        xtp_wait_for_log "$log" "selection: allowWindowOps=$state" "runtime policy $state"
        touch "$case_dir/toggled-$number"
    done
    xtp_wait_for_title "$log" window-ops-check-selection "final selection"
    actual=$("$reader" CLIPBOARD)
    test "$actual" = "$expected" || { echo "clipboard: expected $expected, got $actual" >&2; exit 1; }
    touch "$case_dir/checked"
    xtp_wait_for_title "$log" window-ops-done "driver completion"
    if ! wait "$terminal_pid"
    then
        terminal_pid=
        cat "$log" >&2
        exit 1
    fi
    terminal_pid=
done
echo "Allow Window Ops toggles live reads/writes and restores configured restrictions"
