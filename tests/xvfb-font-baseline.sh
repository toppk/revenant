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

wait_for_terminal()
{
    log=$1
    description=$2
    attempt=0
    while ! grep -F -q -- 'title changed bytes=19 preview="font-baseline-ready"' "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "revenant did not become ready for $description" >&2
            sed -n '1,260p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

run_case()
{
    universe=$1
    face=$2
    expected=$3
    case_name=$4
    doublesize=$5
    probe=$6
    log=$test_dir/$case_name.log
    cpr=$test_dir/$case_name.cpr
    done_dir=$test_dir/$case_name.done

    if test "$doublesize" = -
    then
        set -- -xrm 'xterm.vt100.faceNameDoublesize:' \
            -xrm 'xterm.vt100.faceNameEmoji:'
    else
        set -- -fd "$doublesize"
    fi

    # The single-quoted program is expanded by the child bash, not this shell.
    # shellcheck disable=SC2016
    "$fixture_root/run" "$universe" "$terminal" -debug +sb -geometry 8x4 \
        -fa "$face:rgba=none" -fs 16 "$@" \
        -xrm 'xterm.vt100.internalBorder: 4' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        -xrm 'xterm.vt100.cursorColor: #000000' \
        -xrm 'xterm.vt100.renderFont: true' \
        -e bash -c 'stty raw -echo; printf "\033[2J\033[H\033[?25l%s\033[6n" "$2"; IFS= read -r -d R reply; printf "%sR" "$reply" >"$1"; printf "\033]2;font-baseline-ready\007"; while ! test -d "$3"; do sleep 0.05; done' bash "$cpr" "$probe" "$done_dir" \
        >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    wait_for_terminal "$log" "$case_name"

    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
    result=$("$window_ink" "$window" --expose 4 4 $((2 * cell_width)) "$cell_height" 0x000000)
    class=$(printf '%s\n' "$result" | sed -n 's/^class=\([^ ]*\).*/\1/p')
    overflow=$("$window_ink" "$window" --expose $((4 + 2 * cell_width)) 4 "$cell_width" "$cell_height" 0x000000)
    overflow_class=$(printf '%s\n' "$overflow" | sed -n 's/^class=\([^ ]*\).*/\1/p')
    cpr_hex=$(od -An -tx1 -v "$cpr" | tr -d ' \n')

    printf '%-14s class=%-5s cpr=%s %s\n' "$case_name" "$class" "$cpr_hex" "$result"
    if test "$class" != "$expected" || test "$cpr_hex" != 1b5b313b3352 || \
       test "$overflow_class" != blank
    then
        echo "$case_name expected class=$expected, CPR row 1 column 3, and no third-cell ink" >&2
        sed -n '1,300p' "$log" >&2
        exit 1
    fi
    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

# This matrix isolates each rendering primitive by selecting the fixture as
# the primary Xft face.
run_case cbdt 'Noto Color Emoji' color cbdt - 😀
run_case cbdt-legacy 'Noto Color Emoji' color cbdt-legacy - 😀
run_case cbdt-lowres Twemoji color cbdt-lowres - 😀
run_case colrv0 OpenMoji color colrv0 - 😀
run_case colrv1 'Noto Color Emoji' color colrv1 - 😀
run_case mono 'Noto Emoji' mono mono - 😀
run_case svginot 'Twitter Color Emoji' color svginot - 😀
run_case sbix 'Revenant Synthetic sbix' color sbix - 😀
run_case cjk 'Noto Sans Mono CJK JP' mono cjk - 日

# Exercise the same formats through faceNameDoublesize with a primary face
# that lacks the probes. Every role must fit two cells without changing width.
run_case cbdt 'DejaVu Sans Mono' color fd-cbdt 'Noto Color Emoji' 😀
run_case cbdt-lowres 'DejaVu Sans Mono' color fd-cbdt-low Twemoji 😀
run_case colrv0 'DejaVu Sans Mono' color fd-colrv0 OpenMoji 😀
run_case colrv1 'DejaVu Sans Mono' color fd-colrv1 'Noto Color Emoji' 😀
run_case mono 'DejaVu Sans Mono' mono fd-mono 'Noto Emoji' 😀
run_case svginot 'DejaVu Sans Mono' color fd-svg 'Twitter Color Emoji' 😀
run_case sbix 'DejaVu Sans Mono' color fd-sbix 'Revenant Synthetic sbix' 😀
run_case cjk 'DejaVu Sans Mono' mono fd-cjk 'Noto Sans Mono CJK JP' 日

# Xterm gives an embedded size in the first faceName item precedence over the
# separate faceSize resource, then removes it before deriving other menu slots.
embedded_log=$test_dir/embedded-size.log
embedded_done=$test_dir/embedded-size.done
# The single-quoted program is expanded by the child bash, not this shell.
# shellcheck disable=SC2016
"$fixture_root/run" base "$terminal" -debug +sb -geometry 8x4 \
    -fa 'DejaVu Sans Mono:size=11:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.internalBorder: 4' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e bash -c 'printf "\033]2;font-baseline-ready\007"; while ! test -d "$1"; do sleep 0.05; done' \
    bash "$embedded_done" >"$test_dir/embedded-size.out" 2>"$embedded_log" &
terminal_pid=$!
wait_for_terminal "$embedded_log" embedded-size
if ! grep -F -q \
    'loaded Xft slot=0 face=DejaVu Sans Mono:rgba=none points=11.00' "$embedded_log"
then
    echo 'embedded-size expected faceName size=11 to override faceSize=16' >&2
    sed -n '1,300p' "$embedded_log" >&2
    exit 1
fi
mkdir "$embedded_done"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
