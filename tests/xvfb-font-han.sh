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

attempt=0
while ! grep -F -q -- 'title changed bytes=14 preview="font-han-ready"' "$log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for Han routing" >&2
        sed -n '1,420p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done

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

attempt=0
while ! grep -F -q -- 'title changed bytes=24 preview="font-han-recapture-ready"' "$recapture_log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for Han miss recapture" >&2
        sed -n '1,420p' "$recapture_log" >&2
        exit 1
    fi
    sleep 0.05
done

if ! grep -E -q -- 'route base=U\+65E5 .*role=doublesize .*NotoSansMonoCJKjp-Regular\.otf' "$recapture_log"
then
    echo "a fully missed Han role did not recapture at the doublesize role" >&2
    sed -n '1,520p' "$recapture_log" >&2
    exit 1
fi

mkdir "$recapture_done"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
