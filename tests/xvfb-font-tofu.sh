#!/bin/sh

set -eu

if test "$#" -ne 4
then
    echo "usage: $0 XVFB REVENANT WINDOW-INK FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
window_ink=$3
fixture_root=$4
test_dir=$(mktemp -d)
xvfb_pid=
terminal_pid=

cleanup()
{
    if test -n "$terminal_pid"
    then
        kill "$terminal_pid" 2>/dev/null || true
        wait "$terminal_pid" 2>/dev/null || true
    fi
    if test -n "$xvfb_pid"
    then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    rm -rf "$test_dir"
}
trap cleanup EXIT HUP INT TERM

if ! test -x "$fixture_root/run"
then
    echo "SKIP: stage fixtures with tools/stage-font-fixtures first"
    exit 77
fi

"$xvfb" -displayfd 3 -screen 0 1024x768x24 -nolisten unix -listen tcp -ac \
    3>"$test_dir/display" >"$test_dir/xvfb.log" 2>&1 &
xvfb_pid=$!

attempt=0
while ! test -s "$test_dir/display"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$xvfb_pid" 2>/dev/null
    then
        echo "Xvfb did not become ready" >&2
        sed -n '1,80p' "$test_dir/xvfb.log" >&2
        exit 1
    fi
    sleep 0.05
done
DISPLAY=127.0.0.1:$(sed -n '1p' "$test_dir/display")
export DISPLAY

log=$test_dir/font-tofu.log
done_dir=$test_dir/done
"$fixture_root/run" base "$terminal" -debug +sb -geometry 8x5 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.faceNameHan:' \
    -xrm 'xterm.vt100.systemFallback: false' \
    -xrm 'xterm.vt100.internalBorder: 4' \
    -xrm 'xterm.vt100.background: #000000' \
    -xrm 'xterm.vt100.foreground: #FFFFFF' \
    -xrm 'xterm.vt100.cursorColor: #000000' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[?25l%s\r\n%s\r\n \033]2;font-tofu-ready\007" "$1" "$2"; while ! test -d "$3"; do sleep 0.05; done' \
    sh '' '日' "$done_dir" >"$test_dir/stdout" 2>"$log" &
terminal_pid=$!

attempt=0
while ! grep -F -q -- 'title changed bytes=15 preview="font-tofu-ready"' "$log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for deterministic tofu" >&2
        sed -n '1,360p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done

window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
width1=$($window_ink "$window" --expose 4 4 "$cell_width" "$cell_height" 0x000000)
wide_first=$($window_ink "$window" --expose 4 $((4 + cell_height)) "$cell_width" "$cell_height" 0x000000)
wide_second=$($window_ink "$window" --expose $((4 + cell_width)) $((4 + cell_height)) "$cell_width" "$cell_height" 0x000000)
blank=$($window_ink "$window" --expose 4 $((4 + 2 * cell_height)) "$cell_width" "$cell_height" 0x000000)

if ! grep -E -q -- 'route base=U\+E000 width=1 .*role=tofu' "$log" || \
   ! grep -E -q -- 'route base=U\+65E5 width=2 .*role=tofu' "$log" || \
   ! printf '%s\n' "$width1" | grep -q '^class=mono ' || \
   ! printf '%s\n' "$wide_first" | grep -q '^class=mono ' || \
   ! printf '%s\n' "$wide_second" | grep -q '^class=mono ' || \
   ! printf '%s\n' "$blank" | grep -q '^class=blank '
then
    echo "deterministic width-1/width-2 tofu or blank-cell handling failed" >&2
    printf 'width1: %s\nwide-first: %s\nwide-second: %s\nblank: %s\n' \
        "$width1" "$wide_first" "$wide_second" "$blank" >&2
    sed -n '1,460p' "$log" >&2
    exit 1
fi

mkdir "$done_dir"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
