#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
python=$3
driver=$4
window_alpha=$5
compositor_owner=$6
drag_slider=$7

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

start_case()
{
    phase=$1
    shift
    case_dir=$test_dir/$phase
    mkdir "$case_dir"
    log=$case_dir/log
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fa monospace -geometry 40x6 \
        -xrm 'xterm.vt100.renderFont: true' \
        -xrm 'xterm.vt100.internalBorder: 2' \
        -xrm 'xterm.vt100.foreground: #102030' \
        -xrm 'xterm.vt100.background: #304050' \
        -xrm 'xterm.vt100.cursorColor: #506070' \
        -xrm 'xterm.vt100.alwaysHighlight: true' \
        -xrm 'XTerm*color1: #cd0000' "$@" \
        -e "$python" "$driver" "$case_dir" "$phase" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" dynamic-start "$phase startup"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
}

finish_case()
{
    xtp_wait_for_title "$log" dynamic-done "$phase completion"
    if ! wait "$terminal_pid"
    then
        terminal_pid=
        echo "xterm+ failed during the $phase dynamic-color case" >&2
        sed -n '1,300p' "$log" >&2
        exit 1
    fi
    terminal_pid=
}

fail()
{
    echo "$1" >&2
    grep -n -E 'title changed|render: effective|terminal: (OSC|color)' "$log" >&2
    exit 1
}

wait_for_marker()
{
    attempt=0
    while ! test -e "$case_dir/marker-$1"
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 200 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ did not reach the $phase checkpoint $1" >&2
            sed -n '1,300p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

# Sample the center of cell COLUMN on ROW as 0xAARRGGBB, without forcing an
# expose, so a stale surface is caught rather than repainted.
cell_pixel()
{
    "$window_alpha" "$window" --sample --argb \
        $((2 + $1 * cell_width + cell_width / 2)) $((2 + $2 * cell_height + cell_height / 2))
}

# The internal border is painted from the window background attribute only.
border_pixel()
{
    "$window_alpha" "$window" --sample --argb 1 1
}

# Assert the background cell, the border, the inverse (foreground) cell, and
# the cursor cell at MARKER.
expect_colors()
{
    marker=$1
    wait_for_marker "$marker"
    background=$(cell_pixel 0 0)
    border=$(border_pixel)
    foreground=$(cell_pixel 2 0)
    cursor=$(cell_pixel 4 0)
    test "$background" = "$2" || fail "$phase: at $marker background painted $background, expected $2"
    test "$border" = "$2" || fail "$phase: at $marker border painted $border, expected $2"
    test "$foreground" = "$3" || fail "$phase: at $marker foreground painted $foreground, expected $3"
    test "$cursor" = "$4" || fail "$phase: at $marker cursor painted $cursor, expected $4"
    touch "$case_dir/$marker.done"
}

expect_palette_cell()
{
    marker=$1
    wait_for_marker "$marker"
    painted=$(cell_pixel 0 1)
    test "$painted" = "$2" || fail "$phase: at $marker SGR 41 painted $painted, expected $2"
    touch "$case_dir/$marker.done"
}

expect_result()
{
    grep -q "preview=\"result-$1-ok\"" "$log" || fail "$phase: result $1 was not ok"
}

# Default policy: sets repaint immediately, resets restore the resources,
# queries match the display, and scheme reports follow the background.
start_case colors
expect_colors initial 0xff304050 0xff102030 0xff506070
expect_colors set 0xff112233 0xffaa5500 0xff445566
grep -q 'effective colors applied foreground=#aa5500 background=#112233 cursor=#445566' "$log" ||
    fail "colors: effective colors were not applied together"
expect_colors reset 0xff304050 0xff102030 0xff506070
expect_colors final 0xff304050 0xff102030 0xff506070
finish_case
expect_result set_queries
expect_result reset_queries
expect_result scheme
expect_result unsolicited
grep -q 'color scheme report sent scheme=dark' "$log" || fail "colors: no unsolicited dark report"
grep -q 'color scheme report sent scheme=light' "$log" || fail "colors: no unsolicited light report"

# allowColorOps false with only the queries denied: sets still repaint.
start_case setonly -xrm 'XTerm*allowColorOps: false' \
    -xrm 'XTerm*disallowedColorOps: GetColor,GetAnsiColor'
expect_colors set 0xff112233 0xff102030 0xff506070
expect_palette_cell palette 0xffff0000
finish_case
expect_result silent
grep -q 'OSC 11 denied by Color Ops policy (GetColor)' "$log" || fail "setonly: query was not denied"
grep -q 'OSC 4 denied by Color Ops policy (GetAnsiColor)' "$log" || fail "setonly: palette query was not denied"

# allowColorOps false with only SetColor denied: sets and resets are ignored, queries answer.
start_case getonly -xrm 'XTerm*allowColorOps: false' -xrm 'XTerm*disallowedColorOps: SetColor'
expect_colors unchanged 0xff304050 0xff102030 0xff506070
finish_case
expect_result answered
grep -q 'OSC 11 denied by Color Ops policy (SetColor)' "$log" || fail "getonly: set was not denied"
grep -q 'OSC 111 denied by Color Ops policy (SetColor)' "$log" || fail "getonly: reset was not denied"
grep -q 'effective colors changed' "$log" && fail "getonly: a denied set changed the display"

# allowColorOps false with xterm's default list: everything dynamic is refused,
# while ordinary palette writes still apply.
start_case denyall -xrm 'XTerm*allowColorOps: false'
expect_colors unchanged 0xff304050 0xff102030 0xff506070
expect_palette_cell palette 0xffff0000
finish_case
expect_result silent

# With a compositor present, changing opacity keeps the runtime background on
# every surface: cells, border, and the translucent alpha.
"$compositor_owner" >"$test_dir/compositor.out" 2>"$test_dir/compositor.log" &
aux_pid=$!
attempt=0
while ! grep -q '^ready$' "$test_dir/compositor.out" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$aux_pid" 2>/dev/null
    then
        echo "compositor selection owner did not become ready" >&2
        exit 1
    fi
    sleep 0.05
done
start_case opacity -xrm 'XTerm*backgroundOpacity: 100'
grep -q 'argb=true' "$log" || fail "opacity: no ARGB visual was selected"
expect_colors set 0xff112233 0xff102030 0xff506070
wait_for_marker opacity
"$drag_slider" open "$window" >/dev/null
xtp_wait_for_log "$log" 'opacity slider geometry' "opacity slider geometry"
slider_x=$(sed -n 's/.*opacity slider geometry x=\([0-9]*\) .*/\1/p' "$log" | tail -1)
slider_y=$(sed -n 's/.*opacity slider geometry .* y=\([0-9]*\) .*/\1/p' "$log" | tail -1)
slider_w=$(sed -n 's/.*opacity slider geometry .* width=\([0-9]*\) .*/\1/p' "$log" | tail -1)
slider_h=$(sed -n 's/.*opacity slider geometry .* height=\([0-9]*\).*/\1/p' "$log" | tail -1)
"$drag_slider" drag "$window" $((slider_x + slider_w / 2)) $((slider_y + slider_h / 2)) >/dev/null
xtp_wait_for_log "$log" 'background opacity changed percent=' "opacity change"
percent=$(sed -n 's/.*background opacity changed percent=\([0-9][0-9]*\).*/\1/p' "$log" | tail -1)
test "$percent" -lt 100 || fail "opacity: the slider did not reduce opacity"
# ARGB pixels are premultiplied: expect #112233 scaled by the sampled alpha.
background=$(cell_pixel 0 0)
border=$(border_pixel)
alpha=$((0x$(printf '%s' "$background" | cut -c3-4)))
test "$alpha" -lt 255 || fail "opacity: the background cell kept full alpha ($background)"
expected=$(printf '0x%02x%02x%02x%02x' "$alpha" $(((0x11 * alpha + 127) / 255)) \
    $(((0x22 * alpha + 127) / 255)) $(((0x33 * alpha + 127) / 255)))
test "$background" = "$expected" ||
    fail "opacity: after the slider the background cell is $background, expected $expected"
test "$border" = "$expected" ||
    fail "opacity: after the slider the border is $border, expected $expected"
touch "$case_dir/opacity.done"
finish_case
kill "$aux_pid" 2>/dev/null || true
wait "$aux_pid" 2>/dev/null || true
aux_pid=

echo "dynamic colors repaint, reset, report, and obey the Color Ops policy"
