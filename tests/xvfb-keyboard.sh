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
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xtp_xvfb_test_init

mkdir "$test_dir/empty-home"
HOME=$test_dir/empty-home
XENVIRONMENT=/dev/null
XFILESEARCHPATH=/dev/null
XUSERFILESEARCHPATH=/dev/null
export HOME XENVIRONMENT XFILESEARCHPATH XUSERFILESEARCHPATH

xtp_start_xvfb "$xvfb"
XTP_KEY_CAPTURE=$test_dir/key.capture
XTP_KEY_READY=$test_dir/key.ready
XMODIFIERS=@im=xtp-missing
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
