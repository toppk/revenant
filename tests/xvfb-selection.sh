#!/bin/sh

set -eu

if test "$#" -ne 5
then
    echo "usage: $0 XVFB XTERM-PLUS SEND-SELECTION READ-SELECTION SEND-SHIFT-CLICK" >&2
    exit 2
fi

xvfb=$1
terminal=$2
sender=$3
reader=$4
shift_click=$5
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

wait_for_output()
{
    wait_log=$1
    wait_what=$2
    attempt=0
    while ! awk '/terminal: feed bytes=/{fed=1} fed && /render: frame/{found=1} END{exit !found}' "$wait_log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ did not render shell output for $wait_what" >&2
            sed -n '1,220p' "$wait_log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

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
    wait_for_output "$log" "selectToClipboard=$policy"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell=$(sed -n 's/.*config: VT100 resolved renderer=.* cell=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p' "$log" | tail -1)
    cell_width=${cell%x*}
    cell_height=${cell#*x}
    start_x=$((2 + cell_width / 2))
    end_x=$((2 + 5 * cell_width + cell_width / 2))
    row_y=$((2 + cell_height / 2))
    "$sender" "$window" "$start_x" "$row_y" "$end_x" "$row_y" >/dev/null

    attempt=0
    while ! grep -q "publish source=SELECT selection=$expected_selection .* owned=true" "$log"
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ never took ownership of $expected_selection" >&2
            sed -n '1,220p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
    selected=$("$reader" "$expected_selection" || echo "<read failed>")
    cut_buffer=$("$reader" CUT_BUFFER0 || echo "<read failed>")
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

mkdir "$test_dir/bin"
cat >"$test_dir/bin/xdg-open" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >>"$XTP_XDG_OPEN_LOG"
EOF
chmod +x "$test_dir/bin/xdg-open"
XTP_XDG_OPEN_LOG=$test_dir/xdg-open.log
PATH=$test_dir/bin:$PATH
export XTP_XDG_OPEN_LOG PATH

hyperlink_log=$test_dir/xterm-hyperlink.log
"$terminal" -debug +sb -xrm 'XTerm*columns: 30' -xrm 'XTerm*rows: 6' \
    -e sh -c 'printf "\033]8;;http://example.com\033\\HTTP\033]8;;\033\\ \033]8;;file:///tmp/inert\033\\https://masked.test\033]8;;\033\\\r\nplain https://wrapped.example/a_(b).\r\n"; sleep 20' \
    >"$test_dir/xterm-hyperlink.out" 2>"$hyperlink_log" &
terminal_pid=$!
attempt=0
while ! grep -q 'shell: realized window=' "$hyperlink_log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "xterm+ did not become ready for hyperlink test" >&2
        sed -n '1,180p' "$hyperlink_log" >&2
        exit 1
    fi
    sleep 0.05
done
wait_for_output "$hyperlink_log" "hyperlink test"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$hyperlink_log" | tail -1)
cell=$(sed -n 's/.*config: VT100 resolved renderer=.* cell=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p' "$hyperlink_log" | tail -1)
cell_width=${cell%x*}
cell_height=${cell#*x}
http_x=$((2 + cell_width / 2))
file_x=$((2 + 10 * cell_width + cell_width / 2))
row_y=$((2 + cell_height / 2))
inferred_x=$((2 + 2 * cell_width + cell_width / 2))
period_x=$((2 + 5 * cell_width + cell_width / 2))
inferred_y=$((2 + 2 * cell_height + cell_height / 2))
"$shift_click" "$window" "$http_x" "$row_y" >/dev/null
attempt=0
while ! test -s "$XTP_XDG_OPEN_LOG"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100
    then
        echo "HTTP OSC 8 link did not invoke xdg-open" >&2
        sed -n '1,240p' "$hyperlink_log" >&2
        exit 1
    fi
    sleep 0.05
done
if test "$(sed -n '1p' "$XTP_XDG_OPEN_LOG")" != http://example.com
then
    echo "xdg-open received the wrong HTTP URI" >&2
    exit 1
fi
"$shift_click" "$window" "$file_x" "$row_y" >/dev/null
sleep 0.1
if test "$(wc -l <"$XTP_XDG_OPEN_LOG")" -ne 1
then
    echo "non-HTTP OSC 8 link unexpectedly invoked xdg-open" >&2
    exit 1
fi
"$shift_click" "$window" "$inferred_x" "$inferred_y" >/dev/null
attempt=0
while test "$(wc -l <"$XTP_XDG_OPEN_LOG")" -lt 2
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100
    then
        echo "wrapped plain-text URL did not invoke xdg-open" >&2
        sed -n '1,280p' "$hyperlink_log" >&2
        exit 1
    fi
    sleep 0.05
done
if test "$(sed -n '2p' "$XTP_XDG_OPEN_LOG")" != 'https://wrapped.example/a_(b)'
then
    echo "plain-text URL was not unwrapped or trimmed correctly" >&2
    cat "$XTP_XDG_OPEN_LOG" >&2
    exit 1
fi
"$shift_click" "$window" "$period_x" "$inferred_y" >/dev/null
sleep 0.1
if test "$(wc -l <"$XTP_XDG_OPEN_LOG")" -ne 2
then
    echo "trailing URL punctuation was unexpectedly activated" >&2
    exit 1
fi
if ! grep -q 'hyperlink: hover bytes=.*http://example.com' "$hyperlink_log" || \
   ! grep -q 'hyperlink: blocked bytes=.*file:///tmp/inert' "$hyperlink_log" || \
   ! grep -q 'hyperlink: hover inferred bytes=.*https://wrapped.example/a_(b)' "$hyperlink_log"
then
    echo "explicit or inferred hyperlink diagnostics missing" >&2
    sed -n '1,260p' "$hyperlink_log" >&2
    exit 1
fi
