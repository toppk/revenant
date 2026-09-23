#!/bin/sh
# -geometry sizes the grid in characters and places the window in pixels, as in xterm.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
xwininfo=$3
xprop=$4
python=$5

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
runs=0

# The child reports its PTY size and the size reports, then waits or exits.
cat >"$test_dir/child.py" <<'EOF'
import fcntl, os, select, struct, sys, termios, time, tty
result, ending = sys.argv[1], sys.argv[2]
fd = os.open("/dev/tty", os.O_RDWR)
rows, columns, _, _ = struct.unpack("HHHH", fcntl.ioctl(fd, termios.TIOCGWINSZ, b"\0" * 8))
saved = termios.tcgetattr(fd)
tty.setraw(fd)
os.write(fd, b"\x1b[18t\x1b[14t\x1b[16t\x1b[5n")
data, deadline = b"", time.monotonic() + 5
while time.monotonic() < deadline and not data.endswith(b"\x1b[0n"):
    if select.select([fd], [], [], 0.1)[0]:
        data += os.read(fd, 256)
termios.tcsetattr(fd, termios.TCSANOW, saved)
replies = data.decode("ascii", "replace").replace("\x1b[", " ").split()
with open(result + ".tmp", "w") as out:
    out.write(f"winsize {columns} {rows}\n")
    for reply in replies:
        fields = reply.rstrip("tn").split(";")
        if reply.endswith("t") and len(fields) == 3:
            out.write(f"report{fields[0]} {fields[1]} {fields[2]}\n")
os.rename(result + ".tmp", result)
if ending == "wait":
    time.sleep(60)
os.write(fd, b"GEOMETRY-DONE")
EOF

fail()
{
    echo "$label: $*" >&2
    test -f "$work/log" && grep -E 'shell:|pty: spawned|WARNING' "$work/log" | tail -20 >&2
    test -f "$work/result" && cat "$work/result" >&2
    exit 1
}

# start LABEL ENDING ARGS...: launches the terminal and reads the child's report.
start()
{
    label=$1
    ending=$2
    shift 2
    runs=$((runs + 1))
    work=$test_dir/run$runs
    mkdir "$work"
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -xrm 'XTerm*allowWindowOps: true' "$@" \
        -e "$python" "$test_dir/child.py" "$work/result" "$ending" >"$work/out" 2>"$work/log" &
    terminal_pid=$!
    attempt=0
    until test -s "$work/result"; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || fail 'the child did not report'
        kill -0 "$terminal_pid" 2>/dev/null || fail 'the terminal exited'
        sleep 0.05
    done
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$work/log")
    test -n "$window" || fail 'no window'
    set -- $($xwininfo -id "$window" | awk '/Absolute upper-left X/ { x = $4 }
        /Absolute upper-left Y/ { y = $4 } /Width:/ { w = $2 } /Height:/ { h = $2 }
        /Border width/ { b = $3 } END { print w, h, x, y, b }')
    width=$1 height=$2 x=$3 y=$4 border=$5
    set -- $(awk '$1 == "report6" { print $2, $3 }' "$work/result")
    cell_height=${1:-0} cell_width=${2:-0}
}

stop()
{
    kill "$terminal_pid" 2>/dev/null || true
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

# expect COLUMNS ROWS X Y: the grid, PTY, reports and window agree with the request.
expect()
{
    columns=$1 rows=$2
    test "$cell_width" -gt 0 && test "$cell_height" -gt 0 || fail 'no cell-size report'
    grep -qx "winsize $columns $rows" "$work/result" || fail "PTY size is not ${columns}x$rows"
    grep -qx "report8 $rows $columns" "$work/result" || fail "CSI 18 t is not $rows;$columns"
    grep -qx "report4 $((rows * cell_height)) $((columns * cell_width))" "$work/result" ||
        fail 'CSI 14 t does not match the grid'
    grep -q "pty: spawned .* size=${columns}x$rows cell=${cell_width}x$cell_height" "$work/log" ||
        fail 'the child was not started at that size'
    test "$width" = $((columns * cell_width + 4)) && test "$height" = $((rows * cell_height + 4)) ||
        fail "window ${width}x$height does not hold ${columns}x$rows cells of ${cell_width}x$cell_height"
    test "$x" = "$3" && test "$y" = "$4" || fail "window at $x,$y, not $3,$4"
}

hints()
{
    $xprop -id "$window" WM_NORMAL_HINTS
}

start default wait -fn fixed
expect 80 24 0 0
hints | grep -q 'user specified' && fail 'hints claim a user geometry'
stop
echo "default: 80x24 at 0,0"

start size-only wait -fn fixed -geometry 40x5
expect 40 5 0 0
hints | grep -q "user specified size: $width by $height" || fail 'no user-specified size'
hints | grep -q 'user specified location' && fail 'a location was claimed'
stop
echo "size only: 40x5, user-specified size"

start size-position wait -fn fixed -geometry 40x5+100+50
expect 40 5 100 50
hints | grep -q 'user specified location: 100, 50' || fail 'no user-specified location'
stop
echo "size and position: 40x5 at 100,50"

start negative-position wait -fn fixed -geometry 40x5-10-20
expect 40 5 $((1024 - 10 - width - 2 * border)) $((768 - 20 - height - 2 * border))
hints | grep -q 'window gravity: SouthEast' || fail 'gravity is not SouthEast'
stop
echo "negative offsets: 40x5 from the bottom-right corner, SouthEast gravity"

start position-only wait -fn fixed -geometry +100+50
expect 80 24 100 50
stop
echo "position only: 80x24 at 100,50"

# A lone x offset keeps it; y stays at xterm's initial 1.
start single-offset wait -fn fixed -geometry 40x5+100
expect 40 5 100 1
hints | grep -q 'user specified location: 100, 1' || fail 'no user-specified location'
hints | grep -q 'window gravity: NorthWest' || fail 'gravity is not NorthWest'
stop
start single-offset-only wait -fn fixed -geometry +100
expect 80 24 100 1
stop
start single-negative-offset wait -fn fixed -geometry 40x5-10
expect 40 5 $((1024 - 10 - width - 2 * border)) 1
hints | grep -q "user specified location: $x, 1" || fail 'no user-specified location'
hints | grep -q 'window gravity: NorthEast' || fail 'gravity is not NorthEast'
stop
start single-negative-offset-only wait -fn fixed -geometry -10
expect 80 24 $((1024 - 10 - width - 2 * border)) 1
stop
echo "single offsets: +100 and -10, with and without a size, keep x; y is 1"

start zero wait -fn fixed -geometry 0x0
expect 1 1 0 0
stop
echo "zero size: 1x1"

start vt100-resource wait -fn fixed -xrm 'XTerm*vt100.geometry: 30x4+20+30'
expect 30 4 20 30
stop
start application-resource wait -fn fixed -xrm 'XTerm.geometry: 30x4+20+30'
expect 30 4 20 30
stop
start over-columns wait -fn fixed -geometry 40x5 -xrm 'XTerm*vt100.columns: 90' \
    -xrm 'XTerm*vt100.rows: 30'
expect 40 5 0 0
stop
echo "resources: vt100 and application geometry in characters; geometry wins over columns and rows"

start font wait -fa monospace -fs 15 -geometry 40x5+10+10
test "${cell_width}x$cell_height" != 6x13 || fail 'the font did not change the cell'
expect 40 5 10 10
stop
echo "font-derived cells: 40x5 of ${cell_width}x$cell_height"

start hold exit -fn fixed -hold -geometry 30x4
expect 30 4 0 0
xtp_wait_for_log "$work/log" 'pty: held child exited status=0' 'the held child exit'
set -- $($xwininfo -id "$window" | awk '/Width:/ { w = $2 } /Height:/ { h = $2 } END { print w, h }')
test "$1x$2" = "${width}x$height" || fail "the held window is $1x$2"
stop
echo "hold: 30x4 window kept after the child exits"
