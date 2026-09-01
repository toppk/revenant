#!/bin/sh

xtp_xvfb_test_cleanup()
{
    if test -n "${terminal_pid:-}"
    then
        kill "$terminal_pid" 2>/dev/null || true
        wait "$terminal_pid" 2>/dev/null || true
    fi
    if test -n "${xvfb_pid:-}"
    then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    if test -n "${test_dir:-}"
    then
        rm -rf "$test_dir"
    fi
}

xtp_xvfb_test_init()
{
    test_dir=$(mktemp -d)
    xvfb_pid=
    terminal_pid=
    trap xtp_xvfb_test_cleanup EXIT HUP INT TERM
}

xtp_require_font_fixtures()
{
    fixture_root=$1
    if ! test -x "$fixture_root/run"
    then
        echo "SKIP: stage fixtures with tools/stage-font-fixtures first"
        exit 77
    fi
}

xtp_start_xvfb()
{
    xvfb=$1
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
}

xtp_wait_for_title()
{
    log=$1
    title=$2
    description=$3
    lines=${4:-420}
    bytes=${#title}
    attempt=0
    while ! grep -F -q -- "title changed bytes=$bytes preview=\"$title\"" "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "revenant did not become ready for $description" >&2
            sed -n "1,${lines}p" "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

xtp_wait_for_log()
{
    log=$1
    pattern=$2
    description=$3
    lines=${4:-420}
    attempt=0
    while ! grep -F -q -- "$pattern" "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "revenant did not log $description" >&2
            sed -n "1,${lines}p" "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}
