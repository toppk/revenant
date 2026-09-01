#!/bin/sh

set -eu

if test "$#" -ne 5
then
    echo "usage: $0 XVFB REVENANT KEY-SENDER FIXTURE-ROOT REPORT-CHECKER" >&2
    exit 2
fi

xvfb=$1
terminal=$2
sender=$3
fixture_root=$4
checker=$5
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

run_case()
{
    mode=$1
    report_option=$2
    text=$3
    log=$test_dir/$mode.log
    done_dir=$test_dir/$mode-done

    # shellcheck disable=SC2086
    "$fixture_root/run" routing "$terminal" -debug +sb -geometry 12x6 \
        -fa 'DejaVu Sans Mono:rgba=none' -fs 16 $report_option \
        -xrm 'xterm.vt100.translations: #override <Key>F12: report-font-routing()' \
        -xrm 'xterm.vt100.faceNameDoublesize:' \
        -xrm 'xterm.vt100.faceNameEmoji:' \
        -xrm 'xterm.vt100.faceNameHan:' \
        -xrm 'xterm.vt100.boldFont: xft:Noto Emoji' \
        -xrm 'xterm.vt100.fallbackFace1: Noto Sans Mono CJK JP' \
        -xrm 'xterm.vt100.fallbackFace2: Noto Sans Mono CJK JP' \
        -xrm 'xterm.vt100.renderFont: true' \
        -e sh -c 'printf "\033[2J\033[H\033[?25l%b\033]2;font-routing-report-ready\007" "$1"; while ! test -d "$2"; do sleep 0.05; done' \
        sh "$text" "$done_dir" >"$test_dir/$mode.stdout" 2>"$log" &
    terminal_pid=$!

    attempt=0
    while ! grep -F -q -- 'title changed bytes=25 preview="font-routing-report-ready"' "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "revenant did not become ready for $mode routing report" >&2
            sed -n '1,480p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    "$sender" "$window" f12 >/dev/null

    attempt=0
    while ! grep -q -- '^{"schema":1,"type":"snapshot"' "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "routing report snapshot action did not emit for $mode" >&2
            sed -n '1,620p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
    if ! python3 "$checker" "$log" "$mode"
    then
        sed -n '1,760p' "$log" >&2
        exit 1
    fi
    if grep -F -q -- 'bytes=5 preview="\e[24~"' "$log"
    then
        echo "report-font-routing translation leaked F12 to the PTY" >&2
        sed -n '1,760p' "$log" >&2
        exit 1
    fi

    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

run_case disabled '' 'A'
run_case enabled '-report-font-routing' 'A\r\n\033[1mA日\033[0m\r\n\364\217\277\277'
