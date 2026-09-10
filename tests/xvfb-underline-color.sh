#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
window_alpha=$3
sender=$4
compositor_owner=$5

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# Row 0: A red single underline, B default single underline, C red underline
# under inverse video, D default underline under inverse video, then the
# marker title.  Green foreground on black keeps every color distinct.
cat >"$test_dir/scene.sh" <<'SCENE'
# No cursor or glyph ink can conceal the decoration samples on rows 1 and 2.
printf '\033[?25l\033]4;1;#ff0000\007'
printf '\033[4;58:2::255:0:0mA\033[59mB\033[7;58:2::255:0:0mC\033[59mD\033[0m'
printf '\033[2;1H'
for style in 1 2 3 4 5
do
    printf '\033[4:%s;58:2::255:0:0m \033[58:2::0:0:255m ' "$style"
done
printf '\033[0m\033[3;1H\033[4;58;5;1m \033[1m \033[0;4m \033[58:2::255:0:0m\033[0;4m '
printf '\033[2m \033[58:2::255:0:0m \033[22;9;53m \033[0m'
printf '\033]2;underline-ready\007'
while ! test -f "$XTP_UNDERLINE_STEP.1"; do sleep 0.02; done
# Rewrite identical text with only its underline color changed.
printf '\033[1;1H\033[4;58:2::0:0:255mA\033[0m\033]2;underline-updated\007'
while ! test -f "$XTP_UNDERLINE_STEP.2"; do sleep 0.02; done
# Existing indexed underline cells must follow palette changes, including bold.
printf '\033]4;1;#0000ff\007\033]2;underline-palette\007'
sleep 20
SCENE

start_case()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        XTP_UNDERLINE_STEP="$test_dir/$case_name-step" \
        "$terminal" -debug +sb -geometry 20x4 \
        -xrm 'xterm.vt100.internalBorder: 2' \
        -xrm 'xterm.vt100.foreground: #00ff00' \
        -xrm 'xterm.vt100.background: #000000' "$@" \
        -e sh "$test_dir/scene.sh" >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" underline-ready "$case_name scene"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
    # A single underline sits one pixel above the cell's bottom row.
    underline_y=$((2 + cell_height - 2))
}

stop_case()
{
    kill "$terminal_pid" 2>/dev/null || true
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

fail()
{
    echo "$1" >&2
    sed -n '1,120p' "$log" >&2
    exit 1
}

underline_pixel()
{
    "$window_alpha" "$window" --sample --argb $((2 + $1 * cell_width + cell_width / 2)) "$underline_y"
}

expect_underlines()
{
    label=$1
    a=$(underline_pixel 0)
    b=$(underline_pixel 1)
    c=$(underline_pixel 2)
    d=$(underline_pixel 3)
    test "$a" = 0xffff0000 || fail "$case_name $label: red underline painted $a"
    test "$b" = "$2" || fail "$case_name $label: default underline painted $b, expected $2"
    test "$c" = 0xffff0000 || fail "$case_name $label: inverse red underline painted $c"
    test "$d" = "$3" || fail "$case_name $label: inverse default underline painted $d, expected $3"
}

expect_pixel()
{
    actual=$("$window_alpha" "$window" --sample --argb "$1" "$2")
    test "$actual" = "$3" || fail "$case_name $4: painted $actual, expected $3"
}

expect_styles()
{
    style=1
    while test "$style" -le 5
    do
        # Sample the left endpoint; double underline lies one row above single.
        y=$((underline_y + cell_height))
        test "$style" -ne 2 || y=$((y - 1))
        x=$((2 + (style - 1) * 2 * cell_width))
        expect_pixel "$x" "$y" 0xffff0000 "style $style red"
        expect_pixel "$((x + cell_width))" "$y" 0xff0000ff "style $style blue"
        style=$((style + 1))
    done
    y=$((underline_y + 2 * cell_height))
    column=0
    for expected in 0xffff0000 0xffff0000 0xff00ff00 0xff00ff00 0xff00aa00 0xffff0000 0xffff0000
    do
        expect_pixel "$((2 + column * cell_width))" "$y" "$expected" "palette/reset/faint column $column"
        column=$((column + 1))
    done
    x=$((2 + 6 * cell_width))
    expect_pixel "$x" "$((2 + 2 * cell_height))" 0xff00ff00 overline
    expect_pixel "$x" "$((2 + 2 * cell_height + cell_height / 2))" 0xff00ff00 strikethrough
}

expect_updates()
{
    touch "$test_dir/$case_name-step.1"
    xtp_wait_for_title "$log" underline-updated "$case_name underline-only update"
    expect_pixel "$((2 + cell_width / 2))" "$underline_y" 0xff0000ff underline-only-update
    touch "$test_dir/$case_name-step.2"
    xtp_wait_for_title "$log" underline-palette "$case_name palette update"
    expect_pixel 2 "$((underline_y + 2 * cell_height))" 0xff0000ff palette-update
    expect_pixel "$((2 + cell_width))" "$((underline_y + 2 * cell_height))" 0xff0000ff bold-palette-update
}

# Bitmap and Xft renderers paint the same underline colors.
start_case bitmap -fn fixed
expect_underlines rendered 0xff00ff00 0xff000000
expect_styles
# Selecting the row swaps text colors, but an explicit underline color stays.
"$sender" "$window" $((2 + cell_width / 2)) $((2 + cell_height / 2)) \
    $((2 + 4 * cell_width + cell_width / 2)) $((2 + cell_height / 2)) >/dev/null
xtp_wait_for_log "$log" 'publish source=SELECT' 'bitmap selection'
expect_underlines selected 0xff000000 0xff00ff00
expect_updates
stop_case

start_case xft -fa monospace -xrm 'xterm.vt100.renderFont: true'
expect_underlines rendered 0xff00ff00 0xff000000
expect_styles
expect_updates
stop_case

# Under a compositor with a translucent background, underline ink stays opaque.
"$compositor_owner" >"$test_dir/compositor.out" 2>"$test_dir/compositor.log" &
aux_pid=$!
attempt=0
while ! grep -q '^ready$' "$test_dir/compositor.out" 2>/dev/null
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || { echo "compositor selection owner did not become ready" >&2; exit 1; }
    sleep 0.05
done
start_case translucent -fa monospace -xrm 'xterm.vt100.renderFont: true' \
    -xrm 'XTerm*backgroundOpacity: 50'
grep -q 'argb=true' "$log" || fail "translucent: no ARGB visual was selected"
expect_underlines rendered 0xff00ff00 0xff000000
expect_styles
expect_updates
stop_case
kill "$aux_pid" 2>/dev/null || true
wait "$aux_pid" 2>/dev/null || true
aux_pid=

echo "SGR 58 underline colors painted by both renderers, under inverse, selection, and opacity"
