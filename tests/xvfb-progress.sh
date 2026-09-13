#!/bin/sh

set -eu

if test "$#" -ne 6
then
    echo "usage: $0 XVFB XTERM_PLUS WINDOW-ALPHA RESIZE XPROP TOGGLE" >&2
    exit 2
fi

xvfb=$1
terminal=$2
alpha=$3
resizer=$4
xprop=$5
toggle=$6
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home" "$test_dir/steps"
steps=$test_dir/steps

# The child sends each request, then waits for the test to inspect it before the next step.
cat >"$test_dir/child.sh" <<'CHILD'
dir=$1
stty raw -echo
step()
{
    : >"$dir/ready-$1"
    while ! test -e "$dir/go-$1"
    do
        sleep 0.05
    done
}
osc()
{
    printf '\033]9;4;%s\033\\' "$1"
}
printf '\033]2;saved\033\\\033[22;0t\033]2;current\033\\'
step start
osc '2'
step fresh-error
osc '1;0'
step zero
osc '2'
step zero-error
osc '1;42'
step set
printf '\033]10;rgb:0000/ffff/0000\033\\\033]11;rgb:0000/0000/8080\033\\'
step colors
printf '\033]110\033\\\033]111\033\\'
step colors-reset
step reverse
osc '2'
step error
osc '4;80'
step pause
osc '3'
step indeterminate
osc '1;150'
step clamp
printf '\033[?2026h'
osc '1;10'
step sync
printf '\033[?2026l'
step resize
osc '0'
step clear
printf '\033[21t'
IFS= read -r -t 2 -d '\' answer || true
printf '%s' "$answer" >"$dir/reply"
printf '\033[23;0t'
step pop
osc '1;50'
step before-reset
printf '\033c'
step reset
osc '1;70'
step exit
CHILD

log=$test_dir/log
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug +sb -fn fixed -T startup \
    -xrm 'xterm.vt100.internalBorder: 2' \
    -xrm 'xterm.vt100.background: #000000' \
    -xrm 'xterm.vt100.foreground: #FFFFFF' \
    -xrm 'XTerm*allowWindowOps: true' \
    -xrm 'XTerm*disallowedColorOps: GetColor,GetAnsiColor' \
    -xrm 'XTerm*vtMenu*font: fixed' -xrm 'XTerm*vtMenu*vertSpace: 0' \
    -e bash "$test_dir/child.sh" "$steps" >"$test_dir/out" 2>"$log" &
terminal_pid=$!
xtp_wait_for_log "$log" 'shell: realized window=' 'terminal window'
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)

white=0xffffffff
black=0xff000000
red=0xffe01b24
amber=0xfff5c211
green=0xff00ff00
navy=0xff000080

fail()
{
    echo "$1" >&2
    shift
    printf '%s\n' "$@" >&2
    grep -E 'progress:|title|synchronized|grid changed' "$log" | tail -40 >&2
    exit 1
}

count_of()
{
    grep -c -F -- "$1" "$log" || true
}

wait_count()
{
    attempt=0
    while test "$(count_of "$1")" -lt "$2"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || fail "did not log '$1' $2 times"
        sleep 0.05
    done
}

ready()
{
    attempt=0
    while ! test -e "$steps/ready-$1"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || fail "the child did not reach step $1"
        sleep 0.05
    done
}

go()
{
    : >"$steps/go-$1"
}

# The newest placement gives the indicator's outer box; its border is one pixel.
place()
{
    line=$(grep 'progress: placed ' "$log" | tail -1)
    px=$(printf '%s\n' "$line" | sed -n 's/.* x=\([0-9]*\) .*/\1/p')
    py=$(printf '%s\n' "$line" | sed -n 's/.* y=\([0-9]*\) .*/\1/p')
    pw=$(printf '%s\n' "$line" | sed -n 's/.* width=\([0-9]*\) .*/\1/p')
    ph=$(printf '%s\n' "$line" | sed -n 's/.* height=\([0-9]*\)$/\1/p')
}

start_pixel()
{
    "$alpha" "$window" --sample --argb $((px + 3)) $((py + 1 + ph / 2))
}

border_pixel()
{
    "$alpha" "$window" --sample --argb "$px" $((py + 1 + ph / 2))
}

end_pixel()
{
    "$alpha" "$window" --sample --argb $((px + pw - 1)) $((py + 1 + ph / 2))
}

expect_bar()
{
    attempt=0
    while test "$(start_pixel)" != "$1" || test "$(end_pixel)" != "$2"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 40 || fail "$3: expected $1 then $2" "start=$(start_pixel) end=$(end_pixel)"
        sleep 0.05
    done
}

title_is()
{
    value=$("$xprop" -id "$window" WM_NAME)
    test "$value" = "WM_NAME(STRING) = \"$1\"" || fail "the window title changed" "$value"
}

ready start
title_is current
go start

# An error before any percentage was reported fills the bar.
ready fresh-error
xtp_wait_for_log "$log" 'progress: state=error percent=100 shown=true' 'error before any percentage'
place
expect_bar "$red" "$red" "error before any percentage"
go fresh-error

# A reported 0% stays 0% when an error arrives without a value.
ready zero
xtp_wait_for_log "$log" 'progress: state=set percent=0 shown=true' 'zero progress'
expect_bar "$black" "$black" "0 percent"
go zero
ready zero-error
xtp_wait_for_log "$log" 'progress: state=error percent=0 shown=true' 'error after zero progress'
expect_bar "$black" "$black" "error after 0 percent"
go zero-error

# Normal progress fills the bar from the left in the foreground color.
ready set
xtp_wait_for_log "$log" 'progress: state=set percent=42 shown=true' 'normal progress'
expect_bar "$white" "$black" "42 percent"
title_is current
go set

# New foreground and background colors reach the fill, the empty part and the border, and back.
ready colors
xtp_wait_for_log "$log" 'effective colors applied foreground=#00ff00 background=#000080' 'color change'
expect_bar "$green" "$navy" "42 percent after a color change"
test "$(border_pixel)" = "$green" || fail "the border kept its old color" "border=$(border_pixel)"
go colors
ready colors-reset
xtp_wait_for_log "$log" 'effective colors applied foreground=#ffffff background=#000000' 'color reset'
expect_bar "$white" "$black" "42 percent after the colors reset"
test "$(border_pixel)" = "$white" || fail "the border kept the changed color" "border=$(border_pixel)"
go colors-reset

# A live reverse-video toggle recolors the fill, the track and the border, both ways.
ready reverse
"$toggle" "$window" reverse >/dev/null
xtp_wait_for_log "$log" 'menu: reverse-video enabled=true' 'reverse video on'
expect_bar "$black" "$white" "42 percent in reverse video"
attempt=0
while test "$(border_pixel)" != "$black"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 40 || fail "the border kept its color after reverse video" "border=$(border_pixel)"
    sleep 0.05
done
"$toggle" "$window" reverse >/dev/null
xtp_wait_for_log "$log" 'menu: reverse-video enabled=false' 'reverse video off'
expect_bar "$white" "$black" "42 percent after reverse video"
attempt=0
while test "$(border_pixel)" != "$white"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 40 || fail "the border kept the reversed color" "border=$(border_pixel)"
    sleep 0.05
done
go reverse

# Error without a value keeps the percentage in red; paused shows its own value in amber.
ready error
xtp_wait_for_log "$log" 'progress: state=error percent=42 shown=true' 'error progress'
expect_bar "$red" "$black" "error"
go error
ready pause
xtp_wait_for_log "$log" 'progress: state=pause percent=80 shown=true' 'paused progress'
expect_bar "$amber" "$black" "paused"
title_is current
go pause

# Indeterminate progress animates.
ready indeterminate
xtp_wait_for_log "$log" 'progress: state=indeterminate' 'indeterminate progress'
phases=$(count_of 'progress: indeterminate phase=')
wait_count 'progress: indeterminate phase=' $((phases + 3))
title_is current
go indeterminate

# An out-of-range value is a full bar, and animation stops.
ready clamp
xtp_wait_for_log "$log" 'progress: state=set percent=100 shown=true' 'clamped progress'
expect_bar "$white" "$white" "full bar"
phases=$(count_of 'progress: indeterminate phase=')
sleep 0.4
test "$(count_of 'progress: indeterminate phase=')" = "$phases" || fail "the animation kept running"
go clamp

# The indicator updates while a synchronized-output batch holds the terminal frame.
ready sync
xtp_wait_for_log "$log" 'progress: state=set percent=10 shown=true' 'progress during a hold'
xtp_wait_for_log "$log" 'synchronized output hold' 'synchronized output hold'
expect_bar "$white" "$black" "10 percent during a synchronized-output hold"
go sync

# A resize moves the indicator to the new top-right corner.
ready resize
old_x=$px
placed=$(count_of 'progress: placed ')
"$resizer" "$window" --grid 60 100 24 100 >/dev/null
xtp_wait_for_log "$log" 'VT100 grid changed' 'resize'
wait_count 'progress: placed ' $((placed + 1))
sleep 0.3
place
test "$px" != "$old_x" || fail "the indicator did not move with the resize" "x=$px"
expect_bar "$white" "$black" "10 percent after the resize"
go resize

# Clearing hides the indicator; the title report and the title stack are untouched.
ready clear
xtp_wait_for_log "$log" 'progress: state=remove percent=10 shown=false' 'cleared progress'
attempt=0
while test "$(start_pixel)" != "$black"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 40 || fail "the indicator stayed visible after clearing" "start=$(start_pixel)"
    sleep 0.05
done
go clear
ready pop
test "$(cat "$steps/reply")" = "$(printf '\033]lcurrent\033')" ||
    fail "the title report changed" "$(od -An -c "$steps/reply")"
title_is saved
go pop

# A full reset removes progress.
ready before-reset
xtp_wait_for_log "$log" 'progress: state=set percent=50 shown=true' 'progress before reset'
go before-reset
ready reset
wait_count 'progress: state=remove ' 2
go reset

# Progress left behind when the process exits is cleared.
ready exit
xtp_wait_for_log "$log" 'progress: state=set percent=70 shown=true' 'progress before exit'
go exit
status=0
wait "$terminal_pid" || status=$?
terminal_pid=
test "$status" = 0 || fail "the terminal exited with status $status"
grep -q 'progress: cleared reason=exit' "$log" || fail "exit did not clear progress"
test "$(count_of 'progress: state=remove ')" = 3 || fail "exit did not remove the indicator"

echo "progress indicator shows every OSC 9;4 state, clamps, follows resize and holds, and leaves titles alone"
