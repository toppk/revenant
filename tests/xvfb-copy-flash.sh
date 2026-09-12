#!/bin/sh

set -eu

if test "$#" -ne 7
then
    echo "usage: $0 XVFB XTERM_PLUS WINDOW-ALPHA SEND-SELECTION READ-SELECTION OWN-SELECTION RESIZE" >&2
    exit 2
fi

xvfb=$1
terminal=$2
alpha=$3
sender=$4
reader=$5
owner=$6
resizer=$7
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
# libXt keeps one selection context per atom for the display's lifetime; the teardown case exits normally.
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS

# The child prints the sample, then acts on control files until done appears.
cat >"$test_dir/child.sh" <<'CHILD'
control=$1
printf '\033[?25l\033[2J\033[HCOPY alpha omega\r\n'
printf '\033]2;flash-ready\007'
while ! test -e "$control/done"
do
    if test -e "$control/sync-on"; then rm "$control/sync-on"; printf '\033[?2026hx'; fi
    if test -e "$control/sync-off"; then rm "$control/sync-off"; printf '\033[?2026l'; fi
    if test -e "$control/osc52"; then rm "$control/osc52"; printf '\033]52;p;b3NjNTI=\007'; fi
    sleep 0.02
done
CHILD

white=0xffffffff
black=0xff000000
red=0xffff0000

fail()
{
    echo "$case_name: $1" >&2
    shift
    printf '%s\n' "$@" >&2
    sed -n '1,300p' "$log" >&2
    exit 1
}

start_case()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    control=$test_dir/$case_name-control
    mkdir "$control"
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fn fixed -geometry 20x4 \
        -xrm 'xterm.vt100.internalBorder: 2' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        "$@" -e sh "$test_dir/child.sh" "$control" >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" flash-ready "$case_name scene"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cw=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    ch=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
}

stop_case()
{
    : >"$control/done"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

# Select the first word, COPY, by dragging across its four cells.
drag_copy()
{
    "$sender" "$window" $((2 + 1)) $((2 + ch / 2)) $((2 + 4 * cw - 1)) $((2 + ch / 2)) >/dev/null
}

# The top-right pixel of the second cell is glyph-free in the fixed font.
cell_pixel()
{
    "$alpha" "$window" --sample --argb $((2 + 2 * cw - 1)) 2
}

wait_pixel()
{
    attempt=0
    while test "$(cell_pixel)" != "$1"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 40 || fail "$2 expected $1" "pixel=$(cell_pixel)"
        sleep 0.05
    done
}

count_of()
{
    grep -c -F -- "$1" "$log" || true
}

wait_count()
{
    attempt=0
    while test "$(count_of "$1")" -lt "$2"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || fail "did not log '$1' $2 times"
        sleep 0.05
    done
}

expect_selection()
{
    value=$("$reader" "$1")
    test "$value" = "$2" || fail "$1 holds the wrong bytes" "$(printf '%s' "$value" | od -c)"
}

# PRIMARY with copyFlashColor: copied cells turn red, then return to the selection colors.
start_case primary -xrm 'xterm.vt100.copyFlashDuration: 1500' -xrm 'xterm.vt100.copyFlashColor: #FF0000'
drag_copy
xtp_wait_for_log "$log" 'copy flash start duration=1500 ms color=#FF0000' 'flash start'
wait_pixel "$red" "flashing copied cell"
expect_selection PRIMARY COPY
xtp_wait_for_log "$log" 'copy flash expired' 'flash expiry'
wait_pixel "$white" "selected cell after expiry"
# A new gesture replaces a running flash, so only the last copy's timer fires.
drag_copy
wait_count 'copy flash start' 2
sleep 0.8
drag_copy
wait_count 'copy flash start' 3
wait_count 'copy flash cancelled reason=new-selection' 1
sleep 0.9
test "$(count_of 'copy flash expired')" = 1 || fail "the replaced flash timer still fired"
wait_count 'copy flash expired' 2
wait_pixel "$white" "selected cell after the replacing flash expired"
expect_selection PRIMARY COPY
# Another client taking PRIMARY ends the flash and the highlight at once.
drag_copy
wait_count 'copy flash start' 4
wait_pixel "$red" "flash before selection loss"
"$owner" PRIMARY stolen >/dev/null &
aux_pid=$!
xtp_wait_for_log "$log" 'copy flash cancelled reason=selection-lost' 'selection loss'
wait_pixel "$black" "cell after selection loss"
sleep 1.6
test "$(count_of 'copy flash expired')" = 2 || fail "a cancelled flash timer fired"
kill "$aux_pid" 2>/dev/null || true
wait "$aux_pid" 2>/dev/null || true
aux_pid=
stop_case

# CLIPBOARD through selectToClipboard, no color: copied cells show their unselected colors.
start_case clipboard -xrm 'xterm.vt100.selectToClipboard: true' -xrm 'xterm.vt100.copyFlashDuration: 1000'
drag_copy
xtp_wait_for_log "$log" 'copy flash start duration=1000 ms color=unselected' 'clipboard flash start'
wait_pixel "$black" "unselected flash"
expect_selection CLIPBOARD COPY
xtp_wait_for_log "$log" 'copy flash expired' 'clipboard flash expiry'
wait_pixel "$white" "clipboard selection after expiry"
stop_case

# The default duration of zero disables the flash without changing the copy.
start_case disabled -xrm 'xterm.vt100.copyFlashColor: #FF0000'
drag_copy
xtp_wait_for_log "$log" 'copy flash disabled' 'disabled flash'
wait_pixel "$white" "selection with the flash disabled"
expect_selection PRIMARY COPY
grep -q 'copy flash start' "$log" && fail "a zero duration started a flash"
stop_case

# An application's OSC 52 write never flashes, but replacing the user's PRIMARY ends a flash.
start_case osc52 -xrm 'xterm.vt100.copyFlashDuration: 3000' -xrm 'xterm.vt100.copyFlashColor: #FF0000' \
    -xrm 'xterm.vt100.allowWindowOps: true'
: >"$control/osc52"
wait_count 'result=success' 1
expect_selection PRIMARY osc52
sleep 0.3
grep -q 'copy flash' "$log" && fail "an OSC 52 write touched the copy flash"
drag_copy
xtp_wait_for_log "$log" 'copy flash start' 'user copy after OSC 52'
wait_pixel "$red" "flash before the OSC 52 replacement"
: >"$control/osc52"
xtp_wait_for_log "$log" 'copy flash cancelled reason=selection-replaced' 'OSC 52 replacement'
wait_pixel "$black" "cell after the OSC 52 replacement"
expect_selection PRIMARY osc52
stop_case

# A resize during the flash redraws it at the new size; expiry then clears it.
start_case resize -xrm 'xterm.vt100.copyFlashDuration: 2500' -xrm 'xterm.vt100.copyFlashColor: #FF0000'
drag_copy
xtp_wait_for_log "$log" 'copy flash start' 'flash before resize'
wait_pixel "$red" "flash before resize"
"$resizer" "$window" --grid 20 30 4 100 >/dev/null
xtp_wait_for_log "$log" 'VT100 grid changed' 'resize'
wait_pixel "$red" "flash after resize"
xtp_wait_for_log "$log" 'copy flash expired' 'expiry after resize'
wait_pixel "$white" "selected cell after expiry at the new size"
expect_selection PRIMARY COPY
stop_case

# During a synchronized-output hold the flash is never painted; release shows the final state.
start_case sync -xrm 'xterm.vt100.copyFlashDuration: 250' -xrm 'xterm.vt100.copyFlashColor: #FF0000'
: >"$control/sync-on"
xtp_wait_for_log "$log" 'synchronized output hold updates=1' 'sync hold'
drag_copy
xtp_wait_for_log "$log" 'copy flash start' 'flash during hold'
test "$(cell_pixel)" = "$black" || fail "a held frame showed the selection or flash" "pixel=$(cell_pixel)"
xtp_wait_for_log "$log" 'copy flash expired' 'expiry during hold'
test "$(cell_pixel)" = "$black" || fail "a held frame showed the selection or flash" "pixel=$(cell_pixel)"
grep -q 'synchronized output timeout' "$log" &&
    fail "the hold timed out before the flash expired; the case ran too slowly"
: >"$control/sync-off"
xtp_wait_for_log "$log" 'synchronized output released' 'sync release'
wait_pixel "$white" "released frame with the expired flash"
expect_selection PRIMARY COPY
stop_case

# Teardown during a flash removes the timer and exits cleanly.
start_case teardown -xrm 'xterm.vt100.copyFlashDuration: 5000' -xrm 'xterm.vt100.copyFlashColor: #FF0000'
drag_copy
xtp_wait_for_log "$log" 'copy flash start' 'flash before teardown'
: >"$control/done"
status=0
wait "$terminal_pid" || status=$?
terminal_pid=
test "$status" = 0 || fail "the terminal exited with status $status during a flash"
grep -q 'copy flash cancelled reason=teardown' "$log" || fail "teardown did not cancel the flash"
grep -q 'copy flash expired' "$log" && fail "the flash timer outlived the widget"

echo "copy flash follows PRIMARY and CLIPBOARD copies, expiry, replacement, loss, OSC 52, resize, synchronized output and teardown"
