#!/bin/sh
# hold: the window and its last output outlive the child until the user closes it.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
sender=$3
reader=$4
closer=$5
ink=$6
resizer=$7
keys=$8

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
# Rows are read through PRIMARY, and libXt keeps that selection context until exit.
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS
runs=0
child='printf "first line\nMARK-%s" "$0"; exit "$0"'
# Closes every PTY descriptor after its output but lives until a line arrives on the FIFO.
lingering='printf "first line\nMARK-P"; exec </dev/null >/dev/null 2>&1; read line <"$0"; exit 4'

fail()
{
    echo "$label: $*" >&2
    test -f "$work/log" && grep -E 'pty:|shell:|config: hold|Sanitizer|^ *#[0-9]|SUMMARY' "$work/log" | tail -60 >&2
    exit 1
}

start()
{
    runs=$((runs + 1))
    work=$test_dir/run$runs
    mkdir "$work"
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        XTP_HOLD_RUN="$work" "$terminal" -debug +sb -fn fixed "$@" >"$work/out" 2>"$work/log" &
    terminal_pid=$!
    xtp_wait_for_log "$work/log" 'shell: realized window=' 'terminal window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$work/log" | tail -1)
}

# The terminal's exit status, waiting up to five seconds.
finish()
{
    attempt=0
    while kill -0 "$terminal_pid" 2>/dev/null; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || fail "the terminal is still running"
        sleep 0.05
    done
    exit_status=0
    wait "$terminal_pid" || exit_status=$?
    terminal_pid=
}

survivors()
{
    grep -l -s -a -F "XTP_HOLD_RUN=$work" /proc/[0-9]*/environ | wc -l | tr -d ' ' || true
}

no_survivors()
{
    attempt=0
    while test "$(survivors)" != 0; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 40 || fail 'processes left behind'
        sleep 0.05
    done
}

# The text of screen row ROW, selected with a drag and read from PRIMARY.
row()
{
    y=$((2 + $1 * 13 + 6))
    "$sender" "$window" 5 "$y" 474 "$y" >/dev/null
    sleep 0.2
    "$reader" PRIMARY
}

# A click on a blank cell ends the selection and its highlight.
clear_selection()
{
    "$sender" "$window" 100 150 100 150 >/dev/null
    sleep 0.2
    test -z "$("$reader" PRIMARY 2>/dev/null)" || fail 'the selection was not cleared'
}

count()
{
    grep -c -F -- "$1" "$work/log" || true
}

# The PTY master reports the child's exit as EOF or, on Linux, EIO.
closes()
{
    grep -c -E 'pty: (EOF|read closed)' "$work/log" || true
}

# Ink hashes of "first line" and of MARK-N, leaving out the cursor cell after it.
text_pixels()
{
    first=$("$ink" "$window" "$1" 2 2 60 13 0xffffff) || fail 'cannot sample row 0'
    last=$("$ink" "$window" --sample 2 15 36 13 0xffffff) || fail 'cannot sample row 1'
    case $first in *' ink=0 '*) fail "no ink in row 0: $first" ;; esac
    case $last in *' ink=0 '*) fail "no ink in row 1: $last" ;; esac
    echo "${first#* hash=}" "${last#* hash=}"
}

ticks()
{
    awk '{ print $14 + $15 }' "/proc/$terminal_pid/stat"
}

for status in 0 3; do
    label="default, child exit $status"
    start -e sh -c "$child" "$status"
    finish
    test "$exit_status" -eq 0 || fail "terminal exited with $exit_status, not 0"
    test "$(closes)" = 1 || fail 'EOF was not handled once'
    test "$(survivors)" = 0 || fail 'processes left behind'
    echo "$label: terminal exited 0"
done

for status in 0 3; do
    label="hold, child exit $status"
    start -hold -e sh -c "$child" "$status"
    xtp_wait_for_log "$work/log" "pty: held child exited status=$status" 'reaping the held child'
    kill -0 "$terminal_pid" || fail 'the terminal exited'
    baseline=$(text_pixels --sample)
    test "$(row 0)" = 'first line' || fail "row 0 is '$(row 0)'"
    test "$(row 1)" = "MARK-$status" || fail "row 1 is '$(row 1)'"
    clear_selection
    test "$(text_pixels --sample)" = "$baseline" || fail 'pixels differ once the selection is cleared'
    test "$(ps -o stat= --ppid "$terminal_pid" | grep -c . || true)" = 0 || fail 'a child remains'
    start_ticks=$(ticks)
    sleep 1
    used=$(($(ticks) - start_ticks))
    test "$used" -le 5 || fail "$used CPU ticks in an idle second"
    test "$(closes)" = 1 && test "$(count 'child finished; holding the window')" = 1 ||
        fail 'EOF handling repeated'
    test "$(text_pixels --expose)" = "$baseline" || fail 'an expose did not repaint the output'
    "$resizer" "$window" 1 150 >/dev/null
    sleep 0.3
    test "$(text_pixels --sample)" = "$baseline" || fail 'a resize did not repaint the output'
    test "$(row 1)" = "MARK-$status" || fail 'output lost after an expose and a resize'
    clear_selection
    "$keys" "$window" keysym a >/dev/null
    xtp_wait_for_log "$work/log" 'write dropped bytes=1: child finished' 'a dropped keypress'
    kill -0 "$terminal_pid" || fail 'a keypress closed the held window'
    test "$(count 'held child exited')" = 1 || fail 'the child was reported more than once'
    "$closer" "$window"
    finish
    test "$exit_status" -eq 0 || fail "closing exited with $exit_status"
    test "$(survivors)" = 0 || fail 'processes left behind'
    echo "$label: text and pixels kept, child reaped once, one EOF, idle; expose, resize and a key kept it; closed with 0"
done

for ending in exit close; do
    label="hold, child alive after closing the PTY, then $ending"
    fifo=$test_dir/release$runs
    mkfifo "$fifo"
    start -hold -e sh -c "$lingering" "$fifo"
    xtp_wait_for_log "$work/log" 'child finished; holding the window' 'the lingering child closing the PTY'
    child_pid=$(sed -n 's/.*pty: spawned pid=\([0-9]*\).*/\1/p' "$work/log")
    sleep 0.5
    kill -0 "$child_pid" || fail 'the child exited too early'
    test "$(count 'held child')" = 0 || fail 'a running child was reported'
    test "$(row 1)" = 'MARK-P' || fail "row 1 is '$(row 1)'"
    clear_selection
    test "$(closes)" = 1 && test "$(count 'child finished; holding the window')" = 1 ||
        fail 'EOF handling repeated'
    if test "$ending" = exit; then
        echo >"$fifo"
        xtp_wait_for_log "$work/log" 'pty: held child exited status=4' 'the late child exit'
        sleep 0.3
        test "$(count 'held child')" = 1 || fail 'the child was reported more than once'
        test "$(ps -o stat= --ppid "$terminal_pid" | grep -c . || true)" = 0 || fail 'a child remains'
        test "$(row 1)" = 'MARK-P' || fail 'output lost after the late exit'
        "$closer" "$window"
        finish
        grep -q 'pty: closing pid=-1 ' "$work/log" || fail 'the reaped child was not forgotten'
    else
        "$closer" "$window"
        finish
        grep -q "pty: closing pid=$child_pid " "$work/log" || fail 'reaping was not pending at close'
        test "$(count 'held child')" = 0 || fail 'a child was reported after close'
    fi
    test "$exit_status" -eq 0 || fail "closing exited with $exit_status"
    no_survivors
    echo "$label: held while the child ran, reported once or not at all, closed with 0, nothing left"
done

label='hold resource'
start -xrm 'XTerm*hold: true' -e sh -c "$child" 0
xtp_wait_for_log "$work/log" 'pty: held child exited status=0' 'resource hold'
"$closer" "$window"
finish
label='hold resource overridden by +hold'
start -xrm 'XTerm*hold: true' +hold -e sh -c "$child" 0
finish
test "$(count 'child finished; holding')" = 0 || fail '+hold still held the window'
echo 'hold resource: held; +hold on the command line wins'

label='hold, closed while the child runs'
start -hold -e sh -c 'sleep 30'
sleep 0.5
test "$(survivors)" -ge 1 || fail 'the child did not start'
"$closer" "$window"
finish
test "$exit_status" -eq 0 || fail "closing exited with $exit_status"
no_survivors
echo "$label: exited 0, child gone"

# xterm retries a failed exec through the shell, whose error stays on screen; the
# child here exits 127 without a message, and hold keeps an empty window.
label='hold, failed exec'
start -hold -e /nonexistent/xtp-hold-command
xtp_wait_for_log "$work/log" 'pty: held child exited status=127' 'failed exec'
test "$(row 0)" = '' || fail "row 0 is '$(row 0)'"
"$closer" "$window"
finish
test "$exit_status" -eq 0 || fail "closing exited with $exit_status"
echo "$label: held with status 127 and no message; closed with 0"
