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
mkdir "$test_dir/empty-home"

start_case()
{
    phase=$1
    shift
    case_dir=$test_dir/$phase
    mkdir "$case_dir"
    log=$case_dir/log
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 -n icon-label "$@" \
        -e "$python" "$driver" "$case_dir" "$phase" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" title-start "$phase startup"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
}

finish_case()
{
    xtp_wait_for_title "$log" title-done "$phase completion"
    if ! wait "$terminal_pid"
    then
        terminal_pid=
        echo "xterm+ failed during the $phase title case" >&2
        sed -n '1,300p' "$log" >&2
        exit 1
    fi
    terminal_pid=
}

fail()
{
    echo "$1" >&2
    grep -n -E 'title changed|shell: (title|XTWINOPS)|selection: allowWindowOps' "$log" >&2
    exit 1
}

# Block until the driver creates its marker file for NAME.
wait_for_marker()
{
    attempt=0
    while ! test -e "$case_dir/marker-$1"
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 200 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ did not reach the $phase checkpoint $1" >&2
            sed -n '1,300p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

acknowledge()
{
    touch "$case_dir/$1.done"
}

# Read the live WM_NAME or WM_ICON_NAME property as a plain string.
wm_label()
{
    "$xprop" -id "$window" "$1" |
        sed -n 's/^'"$1"'([A-Z0-9_]*) = "\(.*\)"$/\1/p'
}

expect_labels()
{
    marker=$1
    wanted_title=$2
    wanted_icon=$3
    wait_for_marker "$marker"
    actual_title=$(wm_label WM_NAME)
    actual_icon=$(wm_label WM_ICON_NAME)
    test "$actual_title" = "$wanted_title" ||
        fail "$phase: at $marker WM_NAME is '$actual_title', expected '$wanted_title'"
    test "$actual_icon" = "$wanted_icon" ||
        fail "$phase: at $marker WM_ICON_NAME is '$actual_icon', expected '$wanted_icon'"
    acknowledge "$marker"
}

# Change both labels from outside, as a window manager or script might.
set_external_labels()
{
    wait_for_marker external-labels
    "$xprop" -id "$window" -f WM_NAME 8s -set WM_NAME external-title
    "$xprop" -id "$window" -f WM_ICON_NAME 8s -set WM_ICON_NAME external-icon
    test "$(wm_label WM_NAME)" = external-title || fail "$phase: external title was not set"
    acknowledge external-labels
}

expect_result()
{
    grep -q "preview=\"result-$1-ok\"" "$log" || fail "$phase: result $1 was not ok"
}

# Default policy: the stack works on the live properties, reports stay silent.
start_case stack
set_external_labels
expect_labels beta-visible beta external-icon
expect_labels external-restored external-title external-icon
expect_labels gamma-restored gamma external-icon
expect_labels alpha-restored alpha external-icon
wait_for_marker empty-pop-ignored
grep -q 'title pop ignored: stack empty' "$log" || fail "stack: empty pop was not ignored"
expect_labels empty-pop-ignored alpha external-icon
expect_labels ring-drained ring2 external-icon
test "$(grep -c 'title pop consumed an empty entry' "$log")" -eq 2 ||
    fail "stack: the two overwritten ring entries were not consumed as empty"
wait_for_marker ring-empty
test "$(grep -c 'title pop ignored: stack empty' "$log")" -eq 2 ||
    fail "stack: drained ring did not report empty"
expect_labels ring-empty ring2 external-icon
finish_case
expect_result query_silent
grep -q 'XTWINOPS 21 denied by window-ops policy (GetWinTitle)' "$log" ||
    fail "stack: window title report was not denied"
grep -q 'XTWINOPS 20 denied by window-ops policy (GetIconTitle)' "$log" ||
    fail "stack: icon report was not denied"
grep -q 'title report code=' "$log" && fail "stack: a report was sent despite the policy"

# allowWindowOps=true: exact 7-bit replies carrying the live labels.
start_case reports -xrm 'XTerm*allowWindowOps: true'
set_external_labels
finish_case
expect_result window
expect_result icon
expect_result after_pop
grep -q 'title report code=l bytes=14' "$log" || fail "reports: window report was not logged"
grep -q 'title report code=L bytes=13' "$log" || fail "reports: icon report was not logged"

# Allow Window Ops toggled from the menu enables and then disables reports.
start_case toggle
wait_for_marker toggle-on
"$toggle" "$window" >>"$case_dir/out"
xtp_wait_for_log "$log" 'selection: allowWindowOps=true' "policy on"
acknowledge toggle-on
wait_for_marker toggle-off
"$toggle" "$window" >>"$case_dir/out"
xtp_wait_for_log "$log" 'selection: allowWindowOps=false' "policy off"
acknowledge toggle-off
finish_case
expect_result before
expect_result enabled
expect_result disabled

# Push/pop can be denied by name or by xterm's number; the title stays put.
start_case denied-stack -xrm 'XTerm*disallowedWindowOps: 22,PopTitle'
wait_for_marker pop-denied
grep -q 'XTWINOPS 22 denied by window-ops policy (PushTitle)' "$log" ||
    fail "denied-stack: push was not denied"
grep -q 'XTWINOPS 23 denied by window-ops policy (PopTitle)' "$log" ||
    fail "denied-stack: pop was not denied"
grep -q 'title pop restored' "$log" && fail "denied-stack: a title was restored"
expect_labels pop-denied changed icon-label
finish_case

echo "XTWINOPS title stack, reports, policy, and live toggle verified"
