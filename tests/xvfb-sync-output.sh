#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
python=$3
driver=$4
resize_tool=$5
hover_tool=$6
paint_tool=$7

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
log=$test_dir/log

HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug -fn fixed -geometry 80x24 -e "$python" "$driver" \
    >"$test_dir/out" 2>"$log" &
terminal_pid=$!

# Print the log from one phase title up to the next.
phase()
{
    sed -n "/title changed bytes=${#1} preview=\"$1\"/,/title changed bytes=${#2} preview=\"$2\"/p" \
        "$log"
}

# Wait until PATTERN appears after the PHASE title, ignoring earlier phases.
wait_in_phase()
{
    attempt=0
    while ! phase "$1" sync-done | grep -F -q -- "$2"
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ did not log $3" >&2
            sed -n '1,300p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

xtp_wait_for_title "$log" sync-start "synchronized output startup"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
cell=$(sed -n 's/.*config: VT100 resolved renderer=.* cell=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p' "$log" | tail -1)
if test -z "$window" || test -z "$cell"
then
    echo "cannot find the realized shell window or cell size" >&2
    sed -n '1,160p' "$log" >&2
    exit 1
fi
cell_width=${cell%x*}
cell_height=${cell#*x}
link_x=$((2 + cell_width / 2))
link_y=$((2 + cell_height / 2))

wait_in_phase sync-phase-c "synchronized output hold updates=1" "phase C hold"
"$resize_tool" "$window" --grid 70 80 24 100 >>"$test_dir/out"

wait_in_phase sync-phase-d "synchronized output hold updates=1" "phase D hold"
"$hover_tool" "$window" "$link_x" "$link_y" shift
wait_in_phase sync-phase-d 'hyperlink: hover bytes=20 preview="https://sync.example"' "phase D hover"
"$hover_tool" "$window" "$link_x" "$link_y" plain
wait_in_phase sync-phase-d "hyperlink: hover cleared" "phase D hover clear"

xtp_wait_for_title "$log" sync-done "driver completion"
if ! wait "$terminal_pid"
then
    terminal_pid=
    echo "xterm+ failed while driving synchronized output" >&2
    sed -n '1,300p' "$log" >&2
    exit 1
fi
terminal_pid=

fail()
{
    echo "$1" >&2
    grep -n -E 'title changed|render: (dirty update|synchronized|expose|frame mode)|resize: VT100 grid|hyperlink: hover' \
        "$log" >&2
    exit 1
}

# Count painted frames in a slice of the log.
frames()
{
    grep -c 'render: frame mode=' || true
}

# Phase A: two held PTY drains paint nothing; the release paints exactly one frame.
a=$(phase sync-phase-a sync-phase-b)
echo "$a" | grep -q 'synchronized output hold updates=2' ||
    fail "phase A did not hold two dirty updates"
held=$(echo "$a" | sed -n '/synchronized output hold updates=1/,/synchronized output released/p' | frames)
test "$held" -eq 0 || fail "phase A painted $held frames during the hold"
released=$(echo "$a" | sed -n '/synchronized output released updates=2 full-redraw=false/,/pty: read/p' | frames)
test "$released" -eq 1 || fail "phase A painted $released frames at release instead of 1"
echo "$a" | grep -q 'synchronized output timeout' &&
    fail "phase A hit the timeout instead of a normal release"

# Phase B: the application never releases; the timeout paints, resets the mode, and DECRQM agrees.
b=$(phase sync-phase-b sync-phase-c)
echo "$b" | grep -q 'synchronized output timeout after 1000 ms; releasing held updates=1' ||
    fail "phase B did not time out the stuck batch"
timed_out=$(echo "$b" | sed -n '/synchronized output timeout/,/pty: read/p' | frames)
test "$timed_out" -eq 1 || fail "phase B painted $timed_out frames at the timeout instead of 1"
echo "$b" | grep -q 'preview="sync-phase-b-mode-reset"' ||
    fail "phase B DECRQM did not report mode 2026 reset after the timeout"
after=$(echo "$b" | sed -n '/sync-phase-b-mode-reset/,$p' | grep -c 'synchronized output hold' || true)
test "$after" -eq 0 || fail "phase B kept holding after the timeout reset the mode"

# Phase C: a host resize during the hold repaints the new grid, keeps the mode, and keeps holding.
c=$(phase sync-phase-c sync-phase-d)
echo "$c" | sed -n '/synchronized output hold updates=1/,/synchronized output released/p' |
    grep -q 'resize: VT100 grid changed 80x24 -> 70x24' || fail "phase C resize did not land during the hold"
echo "$c" | sed -n '/resize: VT100 grid changed 80x24 -> 70x24/,/resize: VT100 grid changed 70x24/p' |
    grep -q 'render: frame mode=full grid=70x24' || fail "phase C resize did not repaint the new grid"
echo "$c" | sed -n '/resize: VT100 grid changed 70x24 -> 80x24/,$p' |
    grep -q 'synchronized output hold updates=2' || fail "phase C output after the resize was not held"
echo "$c" | grep -q 'preview="sync-phase-c-mode-set"' ||
    fail "phase C DECRQM did not report mode 2026 still set after the resize"
restored=$(echo "$c" | sed -n '/resize: VT100 grid changed 70x24 -> 80x24/,/synchronized output released/p' | frames)
test "$restored" -eq 1 || fail "phase C painted $restored frames from the restored grid to the release instead of 1"
released=$(echo "$c" | sed -n '/synchronized output released updates=/,/pty: read/p' | frames)
test "$released" -eq 1 || fail "phase C painted $released frames at release instead of 1"
echo "$c" | grep -q 'synchronized output timeout' &&
    fail "phase C hit the timeout instead of a normal release"

# Phase D: Shift-hovering a link during the hold defers the repaint; the release repaints fully.
d=$(phase sync-phase-d sync-done)
echo "$d" | grep -q 'hyperlink: hover bytes=20 preview="https://sync.example"' ||
    fail "phase D did not detect the hovered link"
echo "$d" | grep -q 'synchronized output deferred full redraw' ||
    fail "phase D hover did not defer its repaint"
hovered=$(echo "$d" | sed -n '/synchronized output hold updates=1/,/synchronized output released/p' | frames)
test "$hovered" -eq 0 || fail "phase D painted $hovered frames during the hold"
echo "$d" | grep -q 'preview="sync-phase-d-mode-set"' ||
    fail "phase D DECRQM did not report mode 2026 set during the hover"
echo "$d" | grep -q 'synchronized output released updates=.* full-redraw=true' ||
    fail "phase D release did not schedule a full redraw"
echo "$d" | sed -n '/synchronized output released/,$p' | grep -q 'render: frame mode=full' ||
    fail "phase D release did not paint a full frame"
echo "$d" | grep -q 'synchronized output timeout' &&
    fail "phase D hit the timeout instead of a normal release"

# Transitions inside one parser batch, judged on painted pixels. A PTY write can be
# split or merged by the reader, so the helper feeds the backend directly: each of its
# batches is exactly one parser call and it paints only where the scenario says. The
# scenarios cover visible output then a hold in one batch, a release, a completed frame
# and a new hold in one batch, and the same transitions spread over consecutive batches
# with no paint between them.
if HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$paint_tool" >"$test_dir/paint.out" 2>"$test_dir/paint.log"
then
    paint_status=0
else
    paint_status=$?
fi
if test "$paint_status" -ne 0
then
    # Unfiltered: a missing or crashing helper says why only on stderr.
    echo "painted scenarios failed: $paint_tool exited $paint_status" >&2
    cat "$test_dir/paint.out" >&2
    tail -n 200 "$test_dir/paint.log" >&2
    exit 1
fi
cat "$test_dir/paint.out"
for scenario in same-write-output-then-hold same-write-release-frame-rehold \
    consecutive-batches-release-frame-rehold
do
    grep -q "^$scenario held " "$test_dir/paint.out" ||
        fail "painted scenario $scenario never ran"
done

echo "synchronized output held, timed out, survived a resize, deferred a hover repaint, and kept frame boundaries within a batch"
