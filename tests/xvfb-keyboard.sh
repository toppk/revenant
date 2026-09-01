#!/bin/sh

set -eu

if test "$#" -ne 3
then
    echo "usage: $0 XVFB XTERM-PLUS SEND-KEY" >&2
    exit 2
fi

xvfb=$1
terminal=$2
sender=$3
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
XTP_KEY_CAPTURE=$test_dir/key.capture
XTP_KEY_READY=$test_dir/key.ready
XMODIFIERS=@im=revenant-missing
export DISPLAY XMODIFIERS XTP_KEY_CAPTURE XTP_KEY_READY

log=$test_dir/xterm-keyboard.log
"$terminal" -debug +sb -e sh -c '
    stty raw -echo
    : >"$XTP_KEY_CAPTURE"
    : >"$XTP_KEY_READY"
    dd if=/dev/tty of="$XTP_KEY_CAPTURE" bs=1 count=11 2>/dev/null
    sleep 20
' >"$test_dir/xterm-keyboard.out" 2>"$log" &
terminal_pid=$!

attempt=0
while ! test -f "$XTP_KEY_READY"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "xterm+ raw-mode key fixture did not become ready" >&2
        sed -n '1,220p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
"$sender" "$window" ctrl-i >/dev/null
"$sender" "$window" tab >/dev/null
"$sender" "$window" adiaeresis >/dev/null

attempt=0
while test "$(wc -c <"$XTP_KEY_CAPTURE" 2>/dev/null || true)" -lt 11
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "xterm+ did not encode raw-mode Ctrl-I and Tab" >&2
        sed -n '1,260p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done
printf '\033[105;5u\t\303\244' >"$test_dir/key.expected"
if ! cmp -s "$test_dir/key.expected" "$XTP_KEY_CAPTURE"
then
    echo "xterm+ Ctrl-I, Tab, or no-XIM UTF-8 bytes did not match" >&2
    od -An -tx1 "$XTP_KEY_CAPTURE" >&2
    sed -n '1,260p' "$log" >&2
    exit 1
fi
if ! grep -F -q -- 'input-method=unavailable input-context=unavailable' "$log"
then
    echo "xterm+ did not exercise the no-XIM UTF-8 fallback" >&2
    sed -n '1,260p' "$log" >&2
    exit 1
fi
