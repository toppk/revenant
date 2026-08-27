#!/bin/sh

set -eu

if test "$#" -ne 4
then
    echo "usage: $0 XVFB XTERM-PLUS SEND-SELECTION READ-SELECTION" >&2
    exit 2
fi

xvfb=$1
terminal=$2
sender=$3
reader=$4
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
    policy=$1
    expected_selection=$2
    log=$test_dir/xterm-$policy.log

    "$terminal" -debug +sb \
        -xrm "XTerm*selectToClipboard:$policy" \
        -e sh -c 'printf "alpha beta\r\n"; sleep 20' \
        >"$test_dir/xterm-$policy.out" 2>"$log" &
    terminal_pid=$!

    attempt=0
    while ! grep -q 'shell: realized window=' "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ did not become ready for selectToClipboard=$policy" >&2
            sed -n '1,160p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell=$(sed -n 's/.*config: VT100 resolved renderer=.* cell=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p' "$log" | tail -1)
    cell_width=${cell%x*}
    cell_height=${cell#*x}
    start_x=$((2 + cell_width / 2))
    end_x=$((2 + 5 * cell_width + cell_width / 2))
    row_y=$((2 + cell_height / 2))
    "$sender" "$window" "$start_x" "$row_y" "$end_x" "$row_y" >/dev/null

    selected=$("$reader" "$expected_selection")
    cut_buffer=$("$reader" CUT_BUFFER0)
    if test "$selected" != alpha || test "$cut_buffer" != alpha
    then
        echo "selection mismatch policy=$policy selection=$selected cut=$cut_buffer" >&2
        sed -n '1,220p' "$log" >&2
        exit 1
    fi
    if ! grep -q "publish source=SELECT selection=$expected_selection .* owned=true" "$log"
    then
        echo "selection log did not confirm ownership of $expected_selection" >&2
        sed -n '1,220p' "$log" >&2
        exit 1
    fi

    kill "$terminal_pid"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

run_case false PRIMARY
run_case true CLIPBOARD
