#!/bin/sh

set -eu

if test "$#" -ne 5
then
    echo "usage: $0 XVFB XTERM_PLUS KEY-SENDER FIXTURE-ROOT REPORT-CHECKER" >&2
    exit 2
fi

xvfb=$1
terminal=$2
sender=$3
fixture_root=$4
checker=$5
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

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

    xtp_wait_for_title "$log" font-routing-report-ready "$mode routing report" 480
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
