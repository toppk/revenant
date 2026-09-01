#!/bin/sh

set -eu

if test "$#" -ne 3
then
    echo "usage: $0 XVFB REVENANT FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
fixture_root=$3
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
    name=$1
    bold_face=$2
    expected_file=$3
    expected_slant=$4
    warning=$5
    log=$test_dir/$name.log
    done_dir=$test_dir/$name-done

    "$fixture_root/run" routing "$terminal" -debug +sb -geometry 8x3 \
        -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
        -xrm "xterm.vt100.boldFont: xft:$bold_face" \
        -xrm 'xterm.vt100.faceNameDoublesize:' \
        -xrm 'xterm.vt100.faceNameEmoji:' \
        -xrm 'xterm.vt100.faceNameHan:' \
        -xrm 'xterm.vt100.systemFallback: false' \
        -xrm 'xterm.vt100.renderFont: true' \
        -e sh -c 'printf "\033[2J\033[H\033[?25l\033[1m%s\033[0m\033]2;font-style-family-ready\007" "$1"; while ! test -d "$2"; do sleep 0.05; done' \
        sh 'A' "$done_dir" >"$test_dir/$name.stdout" 2>"$log" &
    terminal_pid=$!

    attempt=0
    while ! grep -F -q -- 'route base=U+0041' "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "revenant did not become ready for $name" >&2
            sed -n '1,420p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done

    if ! grep -E -q -- "route base=U\+0041 .*role=primary .*${expected_file}.*bold=true italic=false slant=${expected_slant}" "$log" || \
       { test "$warning" = yes && ! grep -F -q -- 'FR-STYLEFAMILY slot=primary style=bold roleFamily=DejaVu Sans Mono resolvedFamily=Noto Emoji' "$log"; } || \
       { test "$warning" = no && grep -F -q -- 'FR-STYLEFAMILY' "$log"; }
    then
        echo "$name did not enforce same-family real-style selection" >&2
        sed -n '1,520p' "$log" >&2
        exit 1
    fi

    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

run_case same-family 'DejaVu Sans Mono' 'DejaVuSansMono-Bold.ttf' roman no
run_case different-family 'Noto Emoji' 'DejaVuSansMono.ttf' roman yes
