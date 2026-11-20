#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
sender=$3
manager=$4
xprop=$5

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

fail()
{
    echo "$case_name: $1" >&2
    sed -n '1,400p' "$log" >&2
    test ! -e "$test_dir/manager.out" || sed -n '1,40p' "$test_dir/manager.out" >&2
    exit 1
}

start_terminal()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    rm -f "$test_dir/step-done"
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fn fixed -geometry 80x24 "$@" -e sh -c \
        "printf '\\033]2;fullscreen-ready\\007'; while ! test -e '$test_dir/step-done'; do sleep 0.05; done" \
        >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" fullscreen-ready "$case_name"
    xtp_wait_for_log "$log" "shell: mapped" "$case_name map"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
}

finish_terminal()
{
    touch "$test_dir/step-done"
    wait "$terminal_pid" || { terminal_pid=; fail "terminal failed while exiting"; }
    terminal_pid=
}

# The Nth "fullscreen=STATE" report, so a repeated state is not satisfied by an old line.
wait_for_state()
{
    attempt=0
    while test "$(grep -c -F -- "shell: fullscreen=$1" "$log" || true)" -lt "$2"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || fail "$3: no fullscreen=$1 report number $2"
        sleep 0.05
    done
}

wait_for_size()
{
    attempt=0
    while ! grep -q -- "shell: configure .* pixels=$1 " "$log"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || fail "$2: the window never became $1"
        sleep 0.05
    done
}

expect_increment()
{
    "$xprop" -id "$window" WM_NORMAL_HINTS >"$test_dir/hints"
    grep -q -F "resize increment: $1" "$test_dir/hints" ||
        fail "$2: expected resize increment $1, got: $(grep increment "$test_dir/hints" || true)"
}

# Without a window manager that advertises fullscreen, a request is refused.
start_terminal unsupported
"$sender" "$window" keysym Return alt >/dev/null
xtp_wait_for_log "$log" "fullscreen request refused target=true current=false" "unsupported request"
grep -q -F "shell: fullscreen=true" "$log" && fail "entered fullscreen with no window manager"
finish_terminal

"$manager" >"$test_dir/manager.out" 2>&1 &
aux_pid=$!
attempt=0
while ! grep -q '^ready root=1024x768$' "$test_dir/manager.out" 2>/dev/null
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || { echo "the fullscreen manager did not start" >&2; exit 1; }
    sleep 0.05
done

# xterm's default Alt+Return toggles, and the cell increments are lifted meanwhile.
start_terminal toggle
expect_increment "6 by 13" "before fullscreen"
"$sender" "$window" keysym Return alt >/dev/null
wait_for_state true 1 "Alt+Return"
wait_for_size 1024x768 "Alt+Return"
expect_increment "1 by 1" "while fullscreen"
"$sender" "$window" keysym Return alt >/dev/null
wait_for_state false 1 "second Alt+Return"
expect_increment "6 by 13" "after fullscreen"
grep -q "^add window=$window fullscreen=true\$" "$test_dir/manager.out" || fail "no add request"
grep -q "^remove window=$window fullscreen=false\$" "$test_dir/manager.out" || fail "no remove request"

# The window manager may leave fullscreen on its own; the terminal follows it.
"$sender" "$window" keysym Return alt >/dev/null
wait_for_state true 2 "third Alt+Return"
"$xprop" -id "$window" -remove _NET_WM_STATE
wait_for_state false 2 "window manager left fullscreen"
expect_increment "6 by 13" "after the window manager left fullscreen"
finish_terminal

# A user translation binds F11, and the explicit parameters refuse a no-op.
start_terminal f11 -xrm '*VT100.translations: #override <Key>F11: fullscreen()\n <Key>F12: fullscreen(on)'
"$sender" "$window" keysym F11 >/dev/null
wait_for_state true 1 "F11"
"$sender" "$window" keysym F12 >/dev/null
xtp_wait_for_log "$log" "fullscreen request refused target=true current=true" "fullscreen(on) twice"
"$sender" "$window" keysym F11 >/dev/null
wait_for_state false 1 "second F11"
finish_terminal

# -fullscreen starts in the layout through the initial window state.
: >"$test_dir/manager.out"
start_terminal startup -fullscreen
wait_for_state true 1 "-fullscreen"
wait_for_size 1024x768 "-fullscreen"
grep -q "^initial window=$window fullscreen=true\$" "$test_dir/manager.out" ||
    fail "the window was not mapped with the fullscreen state"
finish_terminal

# never refuses every request, silently as in xterm.
start_terminal never -xrm '*fullscreen: never'
"$sender" "$window" keysym Return alt >/dev/null
xtp_wait_for_log "$log" "fullscreen request ignored: fullscreen=never" "never"
grep -q -F "shell: fullscreen=true" "$log" && fail "fullscreen=never entered fullscreen"
finish_terminal

echo "fullscreen follows the action, the resource and the window manager"
