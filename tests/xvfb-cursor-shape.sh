#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
window_alpha=$3

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# Each case leaves the cursor on the empty cell at column 0, row 0, after any
# DECSCUSR traffic, then raises a marker title.  alwaysHighlight fills the
# block cursor without focus.
# Shapes are sampled with blinking forced off so the cursor is never mid-blink.
blink_policy=never

start_case()
{
    case_name=$1
    sequence=$2
    shift 2
    log=$test_dir/$case_name.log
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fn fixed -geometry 20x4 \
        -xrm 'xterm.vt100.internalBorder: 2' \
        -xrm 'xterm.vt100.foreground: #00ff00' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.cursorColor: #ff0000' \
        -xrm 'xterm.vt100.alwaysHighlight: true' \
        -xrm "xterm.vt100.cursorBlink: $blink_policy" "$@" \
        -e sh -c "printf '$sequence\033]2;cursor-ready\007'; sleep 20" \
        >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" cursor-ready "$case_name scene"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
}

stop_case()
{
    kill "$terminal_pid" 2>/dev/null || true
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

fail()
{
    echo "$1" >&2
    grep -n -E 'default cursor shape|frame mode' "$log" | tail -6 >&2
    exit 1
}

pixel()
{
    "$window_alpha" "$window" --sample --argb "$1" "$2"
}

# The block fills the cell; the underline occupies the bottom rows; the bar
# occupies the left columns.  Sample the center, the bottom row, and the
# left edge of the cursor cell.
expect_shape()
{
    wanted=$1
    center=$(pixel $((2 + cell_width / 2)) $((2 + cell_height / 2)))
    bottom=$(pixel $((2 + cell_width / 2)) $((2 + cell_height - 1)))
    left=$(pixel 2 $((2 + cell_height / 2)))
    case $wanted in
    block)
        test "$center" = 0xffff0000 || fail "$case_name: block center painted $center"
        ;;
    underline)
        test "$center" = 0xff000000 || fail "$case_name: underline center painted $center"
        test "$bottom" = 0xffff0000 || fail "$case_name: underline bottom painted $bottom"
        test "$left" = 0xff000000 || fail "$case_name: underline left edge painted $left"
        ;;
    bar)
        test "$center" = 0xff000000 || fail "$case_name: bar center painted $center"
        test "$left" = 0xffff0000 || fail "$case_name: bar left edge painted $left"
        test "$bottom" = 0xff000000 || fail "$case_name: bar bottom painted $bottom"
        ;;
    esac
}

start_case default ''
expect_shape block
stop_case

start_case underline '' -uc
grep -q 'default cursor shape=underline' "$log" || fail "underline: default shape not applied"
expect_shape underline
stop_case

start_case bar '' -barc
grep -q 'default cursor shape=bar' "$log" || fail "bar: default shape not applied"
expect_shape bar
stop_case

start_case precedence '' -uc -barc
grep -q 'default cursor shape=underline' "$log" || fail "precedence: underline did not win"
expect_shape underline
stop_case

start_case resource '' -xrm 'XTerm*cursorBar: true'
expect_shape bar
stop_case

# Applications override the startup shape, and DECSCUSR 0 restores it.
start_case override '\033[5 q' -uc
expect_shape bar
stop_case

start_case restore '\033[5 q\033[0 q' -uc
expect_shape underline
stop_case

start_case reset '\033[6 q\033c' -uc
expect_shape underline
stop_case

# A blink request must not change the shape, and vice versa: with the normal
# blink policy the frame reports the bar with the blink request recorded.
blink_policy=false
start_case blink '\033[?12h' -barc
xtp_wait_for_log "$log" 'blink-requested=true' 'blink request'
grep -q 'cursor=visible@0,0 shape=2 blink-requested=true' "$log" ||
    fail "blink: the bar shape did not survive the blink request"
stop_case

echo "startup cursor shapes, precedence, application override, and restoration verified"
