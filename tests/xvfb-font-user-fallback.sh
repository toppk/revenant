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

log=$test_dir/font-user-fallback.log
done_dir=$test_dir/done
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 12x4 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.fallbackFace1: Noto Sans Mono CJK JP' \
    -xrm 'xterm.vt100.fallbackFace2: Noto Sans Mono CJK JP' \
    -xrm 'xterm.vt100.fallbackFace5: Noto Sans Devanagari' \
    -xrm 'xterm.vt100.limitFontsets: 2' \
    -xrm 'xterm.vt100.limitFontWidth: 50' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[?25l%s%s\033]2;font-user-fallback-ready\007" "$1" "$2"; while ! test -d "$3"; do sleep 0.05; done' \
    sh '日' 'क्ष' "$done_dir" >"$test_dir/stdout" 2>"$log" &
terminal_pid=$!

attempt=0
while ! grep -F -q -- 'title changed bytes=24 preview="font-user-fallback-ready"' "$log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for user fallback" >&2
        sed -n '1,360p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done

if test "$(grep -c -- 'FR-DUPROLE kept=fallbackFace1 dropped=fallbackFace2' "$log")" -ne 1 || \
   ! grep -E -q -- 'activated Xft fallback .*source=fallbackFace1 budget=1/2' "$log" || \
   ! grep -E -q -- 'activated Xft fallback .*source=fallbackFace5 budget=2/2' "$log" || \
   ! grep -E -q -- 'route base=U\+65E5 .*role=fallback .*NotoSansMonoCJKjp-Regular\.otf' "$log" || \
   ! grep -E -q -- 'route base=U\+0915 .*role=fallback .*NotoSansDevanagari-Regular\.ttf' "$log"
then
    echo "numbered user fallback ordering, gaps, duplicate handling, or budget failed" >&2
    sed -n '1,420p' "$log" >&2
    exit 1
fi

if ! awk '
    /normalized Xft/ {
        target = source = scale = result = 0
        for (field = 1; field <= NF; ++field) {
            if ($field ~ /^target-height=/) {
                split($field, value, "=")
                target = value[2] + 0
            } else if ($field ~ /^source-height=/) {
                split($field, value, "=")
                source = value[2] + 0
            } else if ($field ~ /^scale=/) {
                split($field, value, "=")
                scale = value[2] + 0
            } else if ($field ~ /^result-height=/) {
                split($field, value, "=")
                result = value[2] + 0
            }
        }
        expected = target / source
        difference = scale - expected
        if (difference < 0)
            difference = -difference
        if (target <= 0 || source <= 0 || result <= 0 || difference > 0.000000001)
            exit 1
        matches++
    }
    END { exit matches == 0 }
' "$log"
then
    echo "fallback metric normalization did not use the exact primary/face height ratio" >&2
    sed -n '1,420p' "$log" >&2
    exit 1
fi

mkdir "$done_dir"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
