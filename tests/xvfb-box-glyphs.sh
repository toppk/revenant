#!/bin/sh

set -eu

if test "$#" -ne 7
then
    echo "usage: $0 XVFB XTERM_PLUS WINDOW-INK TOGGLE SENDER KEY-SENDER FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
window_ink=$3
toggle=$4
sender=$5
keys=$6
fixture_root=$7
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# Row 0: two light horizontals, then a full block, a blank and an upper half.
# Row 1: an inverse upper half, a blank, a bold light horizontal.
# Row 2: an upper half for selection, a blank, a double horizontal.
# Row 3: light, medium and dark shades.
cat >"$test_dir/scene.sh" <<'SCENE'
printf '\033[2J\033[H\033[?25l'
printf '\342\224\200\342\224\200\342\226\210 \342\226\200\r\n'
printf '\033[7m\342\226\200\033[27m \033[1m\342\224\200\033[22m\r\n'
printf '\342\226\200 \342\225\220\r\n'
printf '\342\226\221\342\226\222\342\226\223\r\n'
printf '\033]2;box-ready\007'
while ! test -d "$1"; do sleep 0.05; done
SCENE

fail()
{
    echo "$case_name: $1" >&2
    shift
    printf '%s\n' "$@" >&2
    sed -n '1,400p' "$log" >&2
    exit 1
}

start_case()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    done_dir=$test_dir/$case_name-done
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$fixture_root/run" base "$terminal" -debug +sb -geometry 8x5 \
        -xrm 'xterm.vt100.internalBorder: 4' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        -xrm 'xterm.vt100.cursorColor: #000000' \
        -xrm 'xterm.vt100.systemFallback: false' \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' \
        "$@" -e sh "$test_dir/scene.sh" "$done_dir" >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" box-ready "$case_name scene"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cw=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    ch=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
    t=$((ch / 16))
    test "$t" -le $((cw / 8)) || t=$((cw / 8))
    test "$t" -ge 1 || t=1
}

stop_case()
{
    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

# sample COLUMN ROW CELLS -> class=... ink=N ... bounds=x,y,w,h
sample()
{
    "$window_ink" "$window" --expose $((4 + $1 * cw)) $((4 + $2 * ch)) $(($3 * cw)) "$ch" 0x000000
}

ink_of()
{
    printf '%s\n' "$1" | sed -n 's/.*ink=\([0-9]*\).*/\1/p'
}

expect_ink()
{
    result=$(sample "$1" "$2" 1)
    test "$(ink_of "$result")" = "$3" || fail "$4 expected ink=$3" "$result"
}

expect_procedural()
{
    result=$(sample 0 0 2)
    case $result in
        *" bounds=0,$(((ch - t) / 2)),$((2 * cw)),$t") ;;
        *) fail "two light horizontals do not form one $t-pixel band across both cells" "$result" ;;
    esac
    test "$(ink_of "$result")" = $((2 * cw * t)) || fail "light horizontal band ink" "$result"
    expect_ink 2 0 $((cw * ch)) "full block"
    result=$(sample 3 0 1)
    case $result in
        class=blank*) ;;
        *) fail "the blank cell after the full block received ink" "$result" ;;
    esac
    expect_ink 4 0 $((cw * (ch / 2))) "upper half block"
    expect_ink 0 1 $((cw * (ch - ch / 2))) "inverse upper half block"
    expect_ink 2 1 $((cw * (t + 1))) "bold light horizontal"
    expect_ink 2 2 $((2 * cw * t)) "double horizontal"
    for shade in 1 2 3
    do
        result=$(sample $((shade - 1)) 3 1)
        ink=$(ink_of "$result")
        expected=$((cw * ch * shade / 4))
        if test $((ink + cw + ch)) -lt "$expected" || test "$ink" -gt $((expected + cw + ch))
        then
            fail "shade $shade/4 ink $ink is not near $expected" "$result"
        fi
    done
}

# Xft: the primary face owns box glyphs until the menu forces procedural drawing.
start_case xft -fa 'DejaVu Sans Mono:rgba=none' -fs 16 -xrm 'xterm.vt100.renderFont: true'
grep -q 'route base=U+2500 width=1 presentation=none role=primary' "$log" ||
    fail "the Xft path did not route U+2500 to the primary face by default"
grep -q 'route base=U+2500 .*role=box' "$log" && fail "box glyphs were procedural before the toggle"
"$toggle" "$window" linedrawing >/dev/null
xtp_wait_for_log "$log" 'box glyphs font-first -> forced' 'menu toggle on'
xtp_wait_for_log "$log" 'route base=U+2588 width=1 presentation=none role=box' 'procedural redraw'
expect_procedural
"$sender" "$window" $((4 + cw / 2)) $((4 + 2 * ch + ch / 2)) \
    $((4 + cw + cw / 2)) $((4 + 2 * ch + ch / 2)) >/dev/null
xtp_wait_for_log "$log" 'publish source=SELECT' 'selection'
expect_ink 0 2 $((cw * (ch - ch / 2))) "selected upper half block"
"$toggle" "$window" linedrawing >/dev/null
xtp_wait_for_log "$log" 'box glyphs forced -> font-first' 'menu toggle off'
before=$(grep -c 'route base=U+2588 width=1 presentation=none role=primary' "$log" || true)
sample 2 0 1 >/dev/null
after=$(grep -c 'route base=U+2588 width=1 presentation=none role=primary' "$log" || true)
test "$after" -gt "$before" || fail "the font glyph did not return after the toggle" "$before $after"
stop_case

# Xft with forceBoxChars from the command line at a larger size.
start_case xft-forced +fbx -fa 'DejaVu Sans Mono:rgba=none' -fs 24 -xrm 'xterm.vt100.renderFont: true'
grep -q 'route base=U+2500 width=1 presentation=none role=box' "$log" ||
    fail "+fbx did not force procedural box glyphs"
test "$t" -ge 2 || fail "the 24 point cell is too small to scale line thickness" "cell=${cw}x$ch"
expect_procedural
stop_case

# The bitmap path always draws these ranges procedurally.
start_case bitmap -fn fixed -xrm 'xterm.vt100.renderFont: false'
grep -q 'route base=U+2500 width=1 presentation=none role=box' "$log" ||
    fail "the bitmap path did not draw U+2500 procedurally"
expect_procedural
"$toggle" "$window" linedrawing >/dev/null
xtp_wait_for_log "$log" 'box glyphs font-first -> forced' 'bitmap menu toggle'
expect_procedural
stop_case

# A key bound to set-font-linedrawing() is owned locally: the raw child, with
# Kitty release reports on, receives neither the press nor the release.
case_name=keybinding
log=$test_dir/keybinding.log
rm -f "$test_dir/keys" "$test_dir/keys-ready"
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$fixture_root/run" base "$terminal" -debug +sb -geometry 8x5 \
    -fa 'DejaVu Sans Mono:rgba=none' -fs 16 -xrm 'xterm.vt100.renderFont: true' \
    -xrm 'xterm.vt100.translations: #override <Key>F12: set-font-linedrawing(toggle)' \
    -e sh -c "stty raw -echo; printf '\033[>2u'; : >'$test_dir/keys-ready'; dd if=/dev/tty of='$test_dir/keys' bs=1 count=64 2>/dev/null; sleep 20" \
    >"$test_dir/keybinding.out" 2>"$log" &
terminal_pid=$!
attempt=0
while ! test -f "$test_dir/keys-ready"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || fail "raw child did not start"
    sleep 0.05
done
sleep 0.2
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
"$keys" "$window" f12 >/dev/null
xtp_wait_for_log "$log" 'box glyphs font-first -> forced' 'key binding toggle on'
"$keys" "$window" f12 >/dev/null
xtp_wait_for_log "$log" 'box glyphs forced -> font-first' 'key binding toggle off'
sleep 0.7
test ! -s "$test_dir/keys" || fail "the bound key reached the application" "$(od -An -c "$test_dir/keys")"
owned=$(grep -c 'owned by local Xt action' "$log" || true)
test "$owned" = 4 || fail "expected both presses and both releases to be owned, saw $owned"
kill "$terminal_pid" 2>/dev/null || true
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=

echo "box and block glyphs join across cells under Xft, forced Xft and bitmap rendering"
