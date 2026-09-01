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
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

log=$test_dir/font-route-cache.log
done_dir=$test_dir/done
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 12x7 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[?25l日\r\n\033[1m日\033[0m\r\n本\r\n\364\217\277\277\r\n\033[3m\364\217\277\277\033[0m\033]2;font-route-cache-ready\007"; while ! test -d "$1"; do sleep 0.05; done' \
    sh "$done_dir" >"$test_dir/stdout" 2>"$log" &
terminal_pid=$!

xtp_wait_for_title "$log" font-route-cache-ready "route-cache test"

if test "$(grep -c -- 'route-cache miss base=U+65E5 ' "$log")" -ne 1 || \
   ! grep -q -- 'route-cache hit base=U+65E5 .*role=fallback' "$log" || \
   ! grep -q -- 'route-cache miss base=U+672C ' "$log" || \
   test "$(grep -c -- 'route-cache miss base=U+10FFFF ' "$log")" -ne 1 || \
   ! grep -q -- 'route-cache hit base=U+10FFFF .*role=tofu' "$log" || \
   ! grep -q -- 'route base=U+65E5 .*role=fallback .*bold=false' "$log" || \
   ! grep -q -- 'route base=U+65E5 .*role=fallback .*bold=true' "$log"
then
    echo "font routing cache did not reuse style-neutral family decisions or discriminate keys" >&2
    sed -n '1,520p' "$log" >&2
    exit 1
fi

mkdir "$done_dir"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
