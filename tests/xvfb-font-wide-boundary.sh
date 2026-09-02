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
    wide_face=$2
    expected_role=$3
    log=$test_dir/$name.log
    done_dir=$test_dir/$name-done

    "$fixture_root/run" routing "$terminal" -debug +sb -geometry 8x3 \
        -fa 'Noto Sans Mono CJK JP' -fs 16 \
        -xrm "xterm.vt100.faceNameDoublesize: $wide_face" \
        -xrm 'xterm.vt100.faceNameEmoji:' \
        -xrm 'xterm.vt100.faceNameHan:' \
        -xrm 'xterm.vt100.systemFallback: false' \
        -xrm 'xterm.vt100.renderFont: true' \
        -e sh -c 'printf "\033[2J\033[H\033[?25l%s\033]2;font-wide-boundary-ready\007" "$1"; while ! test -d "$2"; do sleep 0.05; done' \
        sh '日' "$done_dir" >"$test_dir/$name.stdout" 2>"$log" &
    terminal_pid=$!

    xtp_wait_for_title "$log" font-wide-boundary-ready "$name"

    if ! grep -E -q -- "route base=U\+65E5 width=2 .*role=$expected_role" "$log"
    then
        echo "$name did not preserve the configured wide-slot boundary" >&2
        sed -n '1,520p' "$log" >&2
        exit 1
    fi

    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

# A configured but coverage-thin wide slot owns the miss.  The primary CJK
# role must not rescue it across the slot boundary.
run_case configured-wide-miss 'Noto Emoji' tofu

# An unset wide slot does not capture the atom, so the same primary role serves.
run_case unset-wide-slot '' primary
