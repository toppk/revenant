#!/bin/sh

set -eu

if test "$#" -ne 4
then
    echo "usage: $0 XVFB REVENANT WINDOW-INK FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
window_ink=$3
fixture_root=$4
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

log=$test_dir/text-shaping.log
done_dir=$test_dir/done
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 12x8 \
    -fa 'DejaVu Sans Mono:rgba=none,Noto Sans Mono CJK JP' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.limitFontWidth: 50' \
    -xrm 'xterm.vt100.internalBorder: 4' \
    -xrm 'xterm.vt100.background: #000000' \
    -xrm 'xterm.vt100.foreground: #FFFFFF' \
    -xrm 'xterm.vt100.cursorColor: #000000' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e bash -c 'printf "\033[2J\033[H\033[?25l%s\r\n\033[3m%s\033[23m\r\n%s\r\n%s\r\n\033[1;3m%s\033[0m\r\n%s\033]2;text-shaping-ready\007" "$1" "$2" "$3" "$4" "$5" "$6"; while ! test -d "$7"; do sleep 0.05; done' \
    bash 'ế' 'é' 'क्ष' 'A日é' '日' 'h̵̨è̖̗́l̴̃' "$done_dir" \
    >"$test_dir/text-shaping.out" 2>"$log" &
terminal_pid=$!

attempt=0
while ! grep -F -q -- 'title changed bytes=18 preview="text-shaping-ready"' "$log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for text shaping" >&2
        sed -n '1,360p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done

window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
ink=$("$window_ink" "$window" --expose 4 4 $((4 * cell_width)) $((6 * cell_height)) 0x000000)
conjunct_routes_before=$(grep -c -- 'route base=U+0915 width=2' "$log")
conjunct_tail=$("$window_ink" "$window" --expose $((4 + cell_width)) \
    $((4 + 2 * cell_height)) "$cell_width" "$cell_height" 0x000000)
conjunct_routes_after=$(grep -c -- 'route base=U+0915 width=2' "$log")

if ! printf '%s\n' "$ink" | grep -q '^class=mono ' || \
   test "$conjunct_routes_after" -le "$conjunct_routes_before" || \
   ! grep -E -q -- 'route base=U\+0065 .*role=primary .*italic=false .*positioned=true' "$log" || \
   ! grep -E -q -- 'route base=U\+0065 .*role=primary .*italic=true slant=(italic|oblique)' "$log" || \
   ! grep -F -q -- 'queued Xft explicit fallback role=primary slot=0 style=0 face=Noto Sans Mono CJK JP' "$log" || \
   ! grep -E -q -- 'route base=U\+0915 width=2 .*role=fallback glyphs=1 .*clusters=2' "$log" || \
   ! grep -E -q -- 'route base=U\+65E5 width=3 .*role=fallback .*clusters=2' "$log" || \
   ! grep -E -q -- 'route base=U\+65E5 width=2 .*role=fallback .*NotoSansMonoCJKjp-Regular.otf.*bold=true italic=true slant=roman' "$log" || \
   ! grep -E -q -- 'route base=U\+0068 width=3 .*role=primary .*positioned=true clusters=3' "$log"
then
    echo "general shaping, real italic, or fallback routing did not match" >&2
    printf '%s\n' "$ink" >&2
    printf '%s\n' "$conjunct_tail" >&2
    sed -n '1,420p' "$log" >&2
    exit 1
fi

mkdir "$done_dir"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=

context_log=$test_dir/context-group.log
context_update=$test_dir/context-update
context_done=$test_dir/context-done
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 8x4 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.internalBorder: 4' \
    -xrm 'xterm.vt100.background: #000000' \
    -xrm 'xterm.vt100.foreground: #FFFFFF' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e bash -c 'printf "\033[2J\033[H\033[?25l%s\033]2;context-group-ready\007" "$1"; while ! test -d "$2"; do sleep 0.05; done; printf "\r\033[CX\033]2;context-group-updated\007"; while ! test -d "$3"; do sleep 0.05; done' \
    bash 'بت' "$context_update" "$context_done" \
    >"$test_dir/context-group.out" 2>"$context_log" &
terminal_pid=$!

attempt=0
while ! grep -F -q -- 'title changed bytes=19 preview="context-group-ready"' "$context_log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for contextual group repaint" >&2
        sed -n '1,360p' "$context_log" >&2
        exit 1
    fi
    sleep 0.05
done

context_window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$context_log" | tail -1)
context_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$context_log" | tail -1)
context_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$context_log" | tail -1)
mkdir "$context_update"

attempt=0
while ! grep -F -q -- 'title changed bytes=21 preview="context-group-updated"' "$context_log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not update the contextual group" >&2
        sed -n '1,420p' "$context_log" >&2
        exit 1
    fi
    sleep 0.05
done

context_partial=$("$window_ink" "$context_window" --sample 4 4 \
    $((2 * context_width)) "$context_height" 0x000000)
context_full=$("$window_ink" "$context_window" --expose 4 4 \
    $((2 * context_width)) "$context_height" 0x000000)
context_partial_hash=$(printf '%s\n' "$context_partial" | sed -n 's/.* hash=\([0-9a-f]*\) .*/\1/p')
context_full_hash=$(printf '%s\n' "$context_full" | sed -n 's/.* hash=\([0-9a-f]*\) .*/\1/p')
if test -z "$context_partial_hash" || test "$context_partial_hash" != "$context_full_hash"
then
    echo "partial contextual repaint differed from a full Expose" >&2
    printf 'partial: %s\nfull: %s\n' "$context_partial" "$context_full" >&2
    sed -n '1,420p' "$context_log" >&2
    exit 1
fi

mkdir "$context_done"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=

cursor_log=$test_dir/cursor-group.log
cursor_update=$test_dir/cursor-update
cursor_done=$test_dir/cursor-done
"$fixture_root/run" shaping "$terminal" -debug +sb -geometry 8x4 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.faceNameDoublesize:' \
    -xrm 'xterm.vt100.faceNameEmoji:' \
    -xrm 'xterm.vt100.internalBorder: 4' \
    -xrm 'xterm.vt100.background: #000000' \
    -xrm 'xterm.vt100.foreground: #FFFFFF' \
    -xrm 'xterm.vt100.cursorColor: #FF0000' \
    -xrm 'xterm.vt100.alwaysHighlight: true' \
    -xrm 'xterm.vt100.cursorBlink: false' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e bash -c 'printf "\033[2J\033[H%s%s\033[1D\033]2;cursor-group-ready\007" "$1" "$1"; while ! test -d "$2"; do sleep 0.05; done; printf "\r%s\033]2;cursor-group-updated\007" "$3"; while ! test -d "$4"; do sleep 0.05; done' \
    bash 'é' "$cursor_update" 'á' "$cursor_done" \
    >"$test_dir/cursor-group.out" 2>"$cursor_log" &
terminal_pid=$!

attempt=0
while ! grep -F -q -- 'title changed bytes=18 preview="cursor-group-ready"' "$cursor_log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not become ready for cursor group repaint" >&2
        sed -n '1,360p' "$cursor_log" >&2
        exit 1
    fi
    sleep 0.05
done

cursor_window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$cursor_log" | tail -1)
cursor_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$cursor_log" | tail -1)
cursor_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$cursor_log" | tail -1)
mkdir "$cursor_update"

attempt=0
while ! grep -F -q -- 'title changed bytes=20 preview="cursor-group-updated"' "$cursor_log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not update the cursor group" >&2
        sed -n '1,420p' "$cursor_log" >&2
        exit 1
    fi
    sleep 0.05
done

attempt=0
cursor_ink=
while test "$attempt" -lt 100
do
    cursor_ink=$("$window_ink" "$cursor_window" --sample $((4 + cursor_width)) 4 \
        "$cursor_width" "$cursor_height" 0x000000)
    if printf '%s\n' "$cursor_ink" | grep -q '^class=color '
    then
        break
    fi
    attempt=$((attempt + 1))
    sleep 0.05
done
if ! printf '%s\n' "$cursor_ink" | grep -q '^class=color '
then
    echo "complex-text repaint erased the cursor" >&2
    printf '%s\n' "$cursor_ink" >&2
    sed -n '1,420p' "$cursor_log" >&2
    exit 1
fi

mkdir "$cursor_done"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
