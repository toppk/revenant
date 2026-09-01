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

    attempt=0
    while ! grep -F -q -- 'title changed bytes=24 preview="font-wide-boundary-ready"' "$log" 2>/dev/null
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
