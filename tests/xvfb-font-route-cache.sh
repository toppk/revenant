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

attempt=0
while ! grep -F -q -- 'title changed bytes=22 preview="font-route-cache-ready"' "$log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for route-cache test" >&2
        sed -n '1,420p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done

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
