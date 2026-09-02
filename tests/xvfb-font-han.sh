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

log=$test_dir/font-han.log
done_dir=$test_dir/done
unsupported_ivs=$(printf '\346\227\245\363\240\207\257')
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 12x7 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize: DejaVu Sans Mono' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.faceNameHan: Noto Sans Mono CJK JP' \
    -xrm 'xterm.vt100.systemFallback: false' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[?25l%s\r\n%s\r\n%s\r\n%s\r\n%s\033]2;font-han-ready\007" "$1" "$2" "$3" "$4" "$5"; while ! test -d "$6"; do sleep 0.05; done' \
    sh '日' 'あ' '、' '侮︀' "$unsupported_ivs" "$done_dir" >"$test_dir/stdout" 2>"$log" &
terminal_pid=$!

xtp_wait_for_title "$log" font-han-ready "Han routing"

if ! grep -E -q -- 'route base=U\+65E5 .*role=han .*NotoSansMonoCJKjp-Regular\.otf' "$log" || \
   ! grep -E -q -- 'route base=U\+4FAE .*role=han .*NotoSansMonoCJKjp-Regular\.otf' "$log" || \
   grep -E -q -- 'route base=U\+3042 .*role=han' "$log" || \
   grep -E -q -- 'route base=U\+3001 .*role=han' "$log" || \
   ! grep -E -q -- 'route base=U\+65E5 .*role=tofu .*file=\(unknown\)' "$log"
then
    echo "Han capture, Script_Extensions exclusion, or exact-IVS handling failed" >&2
    sed -n '1,520p' "$log" >&2
    exit 1
fi

mkdir "$done_dir"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=

recapture_log=$test_dir/font-han-recapture.log
recapture_done=$test_dir/recapture-done
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 12x3 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize: Noto Sans Mono CJK JP' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.faceNameHan: DejaVu Sans Mono' \
    -xrm 'xterm.vt100.systemFallback: false' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[?25l%s\033]2;font-han-recapture-ready\007" "$1"; while ! test -d "$2"; do sleep 0.05; done' \
    sh '日' "$recapture_done" >"$test_dir/recapture.stdout" 2>"$recapture_log" &
terminal_pid=$!

xtp_wait_for_title "$recapture_log" font-han-recapture-ready "Han miss recapture"

if ! grep -E -q -- 'route base=U\+65E5 .*role=doublesize .*NotoSansMonoCJKjp-Regular\.otf' "$recapture_log"
then
    echo "a fully missed Han role did not recapture at the doublesize role" >&2
    sed -n '1,520p' "$recapture_log" >&2
    exit 1
fi

mkdir "$recapture_done"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
