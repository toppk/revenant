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

log=$test_dir/font-system-fallback.log
done_dir=$test_dir/done
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 12x4 \
    -fa 'DejaVu Sans Mono:rgba=none,Noto Sans Mono CJK JP' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.fallbackFace1: Noto Sans Devanagari' \
    -xrm 'xterm.vt100.systemFallback: false' \
    -xrm 'xterm.vt100.limitFontsets: 2' \
    -xrm 'xterm.vt100.limitFontWidth: 50' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[?25l%s%s\033]2;font-system-fallback-ready\007" "$1" "$2"; while ! test -d "$3"; do sleep 0.05; done' \
    sh '日' 'क्ष' "$done_dir" >"$test_dir/stdout" 2>"$log" &
terminal_pid=$!

attempt=0
while ! grep -F -q -- 'title changed bytes=26 preview="font-system-fallback-ready"' "$log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for system-fallback policy" >&2
        sed -n '1,360p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done

if ! grep -E -q -- 'activated Xft fallback .*source=chain0 budget=1/2' "$log" || \
   ! grep -E -q -- 'activated Xft fallback .*source=fallbackFace1 budget=2/2' "$log" || \
   ! grep -E -q -- 'route base=U\+65E5 .*role=fallback .*NotoSansMonoCJKjp-Regular\.otf' "$log" || \
   ! grep -E -q -- 'route base=U\+0915 .*role=fallback .*NotoSansDevanagari-Regular\.ttf' "$log" || \
   grep -F -q -- 'queued Xft fallback role=' "$log"
then
    echo "systemFallback=false did not preserve named fonts while truncating system candidates" >&2
    sed -n '1,420p' "$log" >&2
    exit 1
fi

mkdir "$done_dir"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
