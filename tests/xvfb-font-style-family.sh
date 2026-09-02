#!/bin/sh

set -eu

if test "$#" -ne 3
then
    echo "usage: $0 XVFB XTERM_PLUS FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
fixture_root=$3
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

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
            echo "xterm+ did not become ready for $name" >&2
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
