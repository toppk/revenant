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

# Row 0: a right arrow, a blank, a full block joined to a right arrow, a blank, a left arrow.
# Row 1: an inverse right arrow, a blank, a bold thin arrow, a blank, a thin arrow.
# Row 2: full braille, dot 1, the left dot column and blank braille, each a cell apart.
# Row 3: U+E0A0 and U+E0C0, outside the drawn range.
# Row 4: a right arrow under the visible cursor.
cat >"$test_dir/scene2.sh" <<'SCENE'
printf '\033[2J\033[H'
printf '\356\202\260 \342\226\210\356\202\260 \356\202\262\r\n'
printf '\033[7m\356\202\260\033[27m \033[1m\356\202\261\033[22m \356\202\261\r\n'
printf '\342\243\277 \342\240\201 \342\241\207 \342\240\200\r\n'
printf '\356\202\240 \356\203\200\r\n'
printf '\356\202\260\033[5;1H\033[?25h'
printf '\033]2;box-ready\007'
while ! test -d "$1"; do sleep 0.05; done
SCENE

# Representatives from the standardized legacy-computing, supplement,
# geometric, DEC scan-line and extended Powerline ranges.
cat >"$test_dir/scene3.sh" <<'SCENE'
printf '\033[2J\033[H\033[?25l'
printf '\360\237\254\200\360\237\254\273\360\237\255\260\360\237\256\234\360\237\256\257\360\237\257\250\r\n'
printf '\360\234\264\200\360\234\267\245\360\234\260\241\360\234\260\257\360\234\260\265\360\234\271\221\r\n'
printf '\360\234\272\217\360\234\272\220\360\234\272\237\342\216\272\342\227\242\342\227\270\r\n'
printf '\356\203\222\356\203\224\r\n'
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
        "$@" -e sh "$test_dir/${scene:-scene}.sh" "$done_dir" >"$test_dir/$case_name.out" 2>"$log" &
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
xtp_wait_for_log "$log" 'procedural glyphs font-first -> forced' 'menu toggle on'
xtp_wait_for_log "$log" 'route base=U+2588 width=1 presentation=none role=box' 'procedural redraw'
expect_procedural
"$sender" "$window" $((4 + cw / 2)) $((4 + 2 * ch + ch / 2)) \
    $((4 + cw + cw / 2)) $((4 + 2 * ch + ch / 2)) >/dev/null
xtp_wait_for_log "$log" 'publish source=SELECT' 'selection'
expect_ink 0 2 $((cw * (ch - ch / 2))) "selected upper half block"
"$toggle" "$window" linedrawing >/dev/null
xtp_wait_for_log "$log" 'procedural glyphs forced -> font-first' 'menu toggle off'
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

# The extended standardized ranges share the same forcing and paint path.
scene=scene3
start_case extended-fallback -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.renderFont: true'
grep -q 'route base=U+1CC21 .*role=box' "$log" ||
    fail "a missing extended glyph did not use procedural fallback"
stop_case
start_case extended +fbx -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.renderFont: true'
for codepoint in 1FB00 1FB3B 1FB70 1FB9C 1FBAF 1FBE8 1CD00 1CDE5 \
    1CC21 1CC2F 1CC35 1CE51 1CE8F 1CE90 1CE9F 23BA 25E2 25F8 E0D2 E0D4
do
    grep -q "route base=U+$codepoint .*role=box" "$log" ||
        fail "U+$codepoint did not use procedural drawing"
done
row=0
while test "$row" -lt 4
do
    columns=6
    test "$row" -lt 3 || columns=2
    column=0
    while test "$column" -lt "$columns"
    do
        result=$(sample "$column" "$row" 1)
        test "$(ink_of "$result")" -gt 0 ||
            fail "extended glyph at $column,$row was blank" "$result"
        column=$((column + 1))
    done
    row=$((row + 1))
done
stop_case
scene=scene

# The bitmap path always draws these ranges procedurally.
start_case bitmap -fn fixed -xrm 'xterm.vt100.renderFont: false'
grep -q 'route base=U+2500 width=1 presentation=none role=box' "$log" ||
    fail "the bitmap path did not draw U+2500 procedurally"
expect_procedural
"$toggle" "$window" linedrawing >/dev/null
xtp_wait_for_log "$log" 'procedural glyphs font-first -> forced' 'bitmap menu toggle'
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
xtp_wait_for_log "$log" 'procedural glyphs font-first -> forced' 'key binding toggle on'
"$keys" "$window" f12 >/dev/null
xtp_wait_for_log "$log" 'procedural glyphs forced -> font-first' 'key binding toggle off'
sleep 0.7
test ! -s "$test_dir/keys" || fail "the bound key reached the application" "$(od -An -c "$test_dir/keys")"
owned=$(grep -c 'owned by local Xt action' "$log" || true)
test "$owned" = 4 || fail "expected both presses and both releases to be owned, saw $owned"
kill "$terminal_pid" 2>/dev/null || true
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=

# Ink of U+E0B0, mirroring the planner's per-row extent.
triangle_ink()
{
    total=0
    row=0
    while test "$row" -lt "$ch"
    do
        edge=$((2 * row + 1))
        test $((2 * ch - 2 * row - 1)) -ge "$edge" || edge=$((2 * ch - 2 * row - 1))
        extent=$(((cw * edge + ch - 1) / ch))
        test "$extent" -ge 1 || extent=1
        test "$extent" -le "$cw" || extent=$cw
        total=$((total + extent))
        row=$((row + 1))
    done
    echo "$total"
}

expect_exact()
{
    result=$(sample "$1" "$2" "$3")
    case $result in
        *" ink=$4 "*" bounds=$5") ;;
        *) fail "$6 expected ink=$4 bounds=$5" "$result" ;;
    esac
}

expect_braille_powerline()
{
    for base in E0B0 E0B1 E0B2 28FF 2801 2847 2800
    do
        grep -q "route base=U+$base width=1 presentation=none role=box" "$log" ||
            fail "U+$base was not drawn procedurally"
    done
    for base in E0A0 E0C0
    do
        grep -q "route base=U+$base .*role=box" "$log" && fail "U+$base is outside the drawn range"
    done
    tri=$(triangle_ink)
    expect_exact 0 0 1 "$tri" "0,0,$cw,$ch" "right arrow"
    result=$(sample 1 0 1)
    case $result in
        class=blank*) ;;
        *) fail "the blank cell after the right arrow received ink" "$result" ;;
    esac
    expect_exact 2 0 2 $((cw * ch + tri)) "0,0,$((2 * cw)),$ch" "full block joined to the right arrow"
    expect_exact 5 0 1 "$tri" "0,0,$cw,$ch" "left arrow"
    expect_ink 0 1 $((cw * ch - tri)) "inverse right arrow"
    thin=$(ink_of "$(sample 4 1 1)")
    bold=$(ink_of "$(sample 2 1 1)")
    test "$thin" -gt 0 && test "$thin" -lt "$tri" || fail "thin arrow ink $thin is not inside the solid arrow $tri"
    test "$bold" -gt "$thin" || fail "bold thin arrow ink $bold is not heavier than $thin"
    result=$(sample 2 2 1)
    dot_w=$(printf '%s\n' "$result" | sed -n 's/.*bounds=[0-9]*,[0-9]*,\([0-9]*\),[0-9]*$/\1/p')
    dot_h=$(printf '%s\n' "$result" | sed -n 's/.*bounds=[0-9]*,[0-9]*,[0-9]*,\([0-9]*\)$/\1/p')
    test -n "$dot_w" && test "$dot_w" = "$dot_h" && test "$(ink_of "$result")" = $((dot_w * dot_h)) ||
        fail "braille dot 1 is not a filled square" "$result"
    expect_ink 0 2 $((8 * dot_w * dot_w)) "full braille cell"
    expect_ink 4 2 $((4 * dot_w * dot_w)) "left braille column"
    expect_ink 0 4 $((cw * ch - tri)) "block cursor over the right arrow"
}

# Braille and Powerline: the fixture face has neither, so Xft draws them procedurally by default.
scene=scene2
cursor_args="-xrm xterm.vt100.cursorColor:#FF0000 -xrm xterm.vt100.alwaysHighlight:true -xrm xterm.vt100.cursorBlink:false"
# shellcheck disable=SC2086
start_case braille-powerline-xft -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
    -xrm 'xterm.vt100.renderFont: true' $cursor_args
expect_braille_powerline
"$sender" "$window" $((4 + 5 * cw + cw / 2)) $((4 + ch / 2)) $((4 + 6 * cw + cw / 2)) $((4 + ch / 2)) >/dev/null
xtp_wait_for_log "$log" 'publish source=SELECT' 'powerline selection'
expect_ink 5 0 $((cw * ch - tri)) "selected left arrow"
stop_case

# shellcheck disable=SC2086
start_case braille-powerline-small -fa 'DejaVu Sans Mono:rgba=none' -fs 9 \
    -xrm 'xterm.vt100.renderFont: true' $cursor_args
expect_braille_powerline
stop_case

# shellcheck disable=SC2086
start_case braille-powerline-bitmap -fn fixed -xrm 'xterm.vt100.renderFont: false' $cursor_args
expect_braille_powerline
stop_case

echo "box, block, braille and Powerline glyphs join across cells under Xft, forced Xft and bitmap rendering"
