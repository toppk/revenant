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

xtp_wait_for_title "$log" font-system-fallback-ready "system-fallback policy" 360

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
