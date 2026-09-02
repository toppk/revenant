#!/bin/sh

set -eu

if test "$#" -ne 3
then
    echo "usage: $0 XVFB XTERM_PLUS FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
fixture_root=$3
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

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

xtp_wait_for_title "$log" font-user-fallback-ready "user fallback" 360

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

blank_log=$test_dir/blank-budget.log
blank_done=$test_dir/blank-budget.done
"$fixture_root/run" routing "$terminal" -debug +sb -geometry 12x4 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.fallbackFace1: Noto Sans Mono CJK JP' \
    -xrm 'xterm.vt100.fallbackFace2: Noto Emoji' \
    -xrm 'xterm.vt100.limitFontsets: 1' \
    -xrm 'xterm.vt100.systemFallback: false' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[?25l%s%s\033]2;font-blank-budget-ready\007" "$1" "$2"; while ! test -d "$3"; do sleep 0.05; done' \
    sh '　' '😀' "$blank_done" >"$test_dir/blank-budget.out" 2>"$blank_log" &
terminal_pid=$!

xtp_wait_for_title "$blank_log" font-blank-budget-ready "blank fallback budget" 360

if grep -E -q -- 'activated Xft fallback .*source=fallbackFace1' "$blank_log" || \
   ! grep -E -q -- 'activated Xft fallback .*source=fallbackFace2 budget=1/1' "$blank_log" || \
   ! grep -E -q -- 'route base=U\+1F600 .*role=fallback .*NotoEmoji-Regular\.ttf' "$blank_log"
then
    echo "blank cluster consumed fallback budget or blocked the following visible glyph" >&2
    sed -n '1,420p' "$blank_log" >&2
    exit 1
fi

mkdir "$blank_done"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
