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
XTP_KEY_EXPECTED=$test_dir/key.expected
export DISPLAY XTP_KEY_CAPTURE XTP_KEY_READY

printf '\033[97;;97u\033[97;1:2;97u\033[97;1:3u' >"$XTP_KEY_EXPECTED"
printf '\033[97:65;2;65u\033[97:65;2:2;65u\033[97:65;2:3u' >>"$XTP_KEY_EXPECTED"
printf '\033[57442;5u\033[57442;5:2u\033[57442;5:3u' >>"$XTP_KEY_EXPECTED"
printf '\033[1;1:1A\033[1;1:2A\033[1;1:3A' >>"$XTP_KEY_EXPECTED"
printf '\033[57376u\033[57376;1:2u\033[57376;1:3u' >>"$XTP_KEY_EXPECTED"
XTP_KEY_COUNT=$(wc -c <"$XTP_KEY_EXPECTED")
export XTP_KEY_COUNT

log=$test_dir/xterm-kitty-keyboard.log
"$terminal" -debug +sb -e sh -c '
    stty raw -echo
    printf "\033[?1h\033[>31u"
    sleep 0.1
    : >"$XTP_KEY_CAPTURE"
    : >"$XTP_KEY_READY"
    dd if=/dev/tty of="$XTP_KEY_CAPTURE" bs=1 count="$XTP_KEY_COUNT" 2>/dev/null
    sleep 20
' >"$test_dir/xterm-kitty-keyboard.out" 2>"$log" &
terminal_pid=$!

attempt=0
while ! test -f "$XTP_KEY_READY"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "xterm+ Kitty keyboard fixture did not become ready" >&2
        sed -n '1,220p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
"$sender" "$window" shift-insert-cycle >/dev/null
"$sender" "$window" a-cycle >/dev/null
"$sender" "$window" shift-a-cycle >/dev/null
"$sender" "$window" ctrl-cycle >/dev/null
"$sender" "$window" up-cycle >/dev/null
"$sender" "$window" f13-cycle >/dev/null

attempt=0
while test "$(wc -c <"$XTP_KEY_CAPTURE" 2>/dev/null || true)" -lt "$XTP_KEY_COUNT"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "xterm+ did not encode the complete Kitty keyboard fixture" >&2
        od -An -tx1 "$XTP_KEY_CAPTURE" >&2
        sed -n '1,300p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done
if ! cmp -s "$XTP_KEY_EXPECTED" "$XTP_KEY_CAPTURE"
then
    echo "xterm+ Kitty keyboard bytes did not match" >&2
    echo "expected:" >&2
    od -An -tx1 "$XTP_KEY_EXPECTED" >&2
    echo "actual:" >&2
    od -An -tx1 "$XTP_KEY_CAPTURE" >&2
    sed -n '1,320p' "$log" >&2
    exit 1
fi
