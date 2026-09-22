#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
python=$3
child=$4
sender=$5
reader=$6

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
# Every scenario needs real input; without XTest the test is skipped, not failed.
xtest=0
"$sender" --xtest || xtest=$?
if test "$xtest" -eq 77
then
    echo "SKIP: XTest is unavailable"
    exit 77
fi
test "$xtest" -eq 0 || exit "$xtest"
mkdir "$test_dir/empty-home"

# Fixed 6x13 cells inside a 2-pixel border; the window sits below the screen top so
# the pointer can leave it upwards during autoscroll.
cx()
{
    echo $((2 + $1 * 6 + 3))
}
ry()
{
    echo $((2 + $1 * 13 + 6))
}

fail()
{
    echo "$*" >&2
    test -n "${work:-}" && test -f "$work/log" && sed -n '1,120p' "$work/log" >&2
    exit 1
}

start_terminal()
{
    work="$test_dir/run$runs"
    runs=$((runs + 1))
    mkdir "$work"
    step=0
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fn fixed -geometry 80x24+0+100 "$@" \
        -e "$python" "$child" "$work" >"$work/out" 2>"$work/log" &
    terminal_pid=$!
    xtp_wait_for_log "$work/log" "shell: realized window=" "window realization"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$work/log" | tail -1)
    grep -q 'resolved renderer=.* cell=6x13 ' "$work/log" || fail "the fixed font is not 6x13"
}

stop_terminal()
{
    child_command quit
    wait "$terminal_pid" || true
    terminal_pid=
}

# One command to the child, published whole by rename.
child_command()
{
    step=$((step + 1))
    printf '%s\n' "$1" >"$work/go.$step.tmp"
    mv "$work/go.$step.tmp" "$work/go.$step"
    attempt=0
    while ! test -e "$work/done.$step"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 400 || fail "the child did not finish: $1"
        sleep 0.025
    done
}

send()
{
    child_command "send:$(printf '%b' "$1" | od -An -tx1 | tr -d ' \n')"
}

events()
{
    injected=0
    "$sender" "$window" --steps "$@" || injected=$?
    test "$injected" -ne 77 || exit 77
    test "$injected" -eq 0 || fail "cannot inject: $*"
    sleep 0.15
}

# The application's input since the last check, in hex.
expect_input()
{
    child_command collect
    actual=$(od -An -tx1 "$work/res.$step" | tr -d ' \n')
    expected=$(printf '%b' "$2" | od -An -tx1 | tr -d ' \n')
    printf 'input %-44s %s\n' "$1" "${actual:-none}"
    test "$actual" = "$expected" || fail "$1: expected ${expected:-none}"
}

# The PRIMARY text a finished drag left, exactly.
expect_selection()
{
    sleep 0.3
    actual=$("$reader" PRIMARY | od -An -c | tr -s ' ' | tr -d '\n')
    expected=$(printf '%b' "$2" | od -An -c | tr -s ' ' | tr -d '\n')
    printf 'selection %-40s %s\n' "$1" "$(printf '%b' "$2" | head -1)..$(printf '%b' "$2" | tail -1)"
    test "$actual" = "$expected" || fail "$1: selected [$actual], expected [$expected]"
}

# The line at the top of the viewport, read by selecting row 0 (Shift overrides
# application tracking; the pause keeps the click from joining the previous one).
expect_top()
{
    events pause:400 ${2:+shift:on} press:1:"$(cx 0)":"$(ry 0)" motion:"$(cx 9)":"$(ry 0)" \
        release:1:"$(cx 9)":"$(ry 0)" ${2:+shift:off}
    sleep 0.2
    actual=$("$reader" PRIMARY)
    printf 'viewport %-41s %s\n' "$3" "$actual"
    test "$actual" = "line $1" || fail "$3: viewport top is '$actual', expected 'line $1'"
}

runs=0

# Reports under a configured multiplier: three lines per tick locally, one report
# per tick for the application, in every tracking mode and encoding.
start_terminal -xrm 'XTerm*VT100.translations: #override <Btn4Down>: scroll-back(3,line,m)\n<Btn5Down>: scroll-forw(3,line,m)'
child_command lines:200
expect_top 0177 "" "at the bottom"
events wheel:up:"$(cx 3)":"$(ry 5)"
expect_top 0174 "" "one local tick up (x3)"
events wheel:down:"$(cx 3)":"$(ry 5)"
expect_top 0177 "" "one local tick down (x3)"
events motion:"$(cx 3)":"$(ry 1)"
for mode in 1000 1002 1003
do
    for encoding in x10 1005 1006 1015 1016
    do
        on="\033[?${mode}h"
        off="\033[?${mode}l"
        if test "$encoding" != x10
        then
            on="$on\033[?${encoding}h"
            off="$off\033[?${encoding}l"
        fi
        case $encoding in
        x10 | 1005) reports='\033[M`$"\033[Ma$"' ;;
        1006) reports='\033[<64;4;2M\033[<65;4;2M' ;;
        1015) reports='\033[96;4;2M\033[97;4;2M' ;;
        1016) reports='\033[<64;21;19M\033[<65;21;19M' ;;
        esac
        send "$on"
        events wheel:up wheel:down
        expect_input "mode $mode $encoding, up then down" "$reports"
        send "$off"
    done
done
send '\033[?9h'
events wheel:up
expect_input "mode 9 (X10 reports no wheel)" ""
send '\033[?9l\033[?1000h\033[?1006h'
events shift:on wheel:up shift:off
expect_input "Shift overrides tracking" ""
expect_top 0174 shift "Shift tick scrolled locally"
stop_terminal

# Handoffs between reporting and local scrolling; nothing carries across.
start_terminal
child_command lines:200
send '\033[?1000h\033[?1006h'
events wheel:up:"$(cx 3)":"$(ry 1)" wheel:up:"$(cx 3)":"$(ry 1)"
expect_input "tracking on, two ticks up" '\033[<64;4;2M\033[<64;4;2M'
expect_top 0177 shift "reported ticks did not scroll"
send '\033[?1000l\033[?1006l'
events wheel:up:"$(cx 3)":"$(ry 1)"
expect_input "tracking off, one tick up" ""
expect_top 0172 "" "exactly one local tick (5 lines)"
send '\033[?1000h\033[?1006h'
events wheel:down:"$(cx 3)":"$(ry 1)"
expect_input "tracking on again, one tick down" '\033[<65;4;2M'
expect_top 0177 shift "output returned to the bottom; no local tick"
send '\033[?1000l\033[?1006l\033[?1049h\033[?1007h'
events wheel:up:"$(cx 3)":"$(ry 1)"
expect_input "alternate screen, 1007 set, no tracking" ""
send '\033[?1000h\033[?1006h'
events wheel:up:"$(cx 3)":"$(ry 1)"
expect_input "alternate screen, 1007 and tracking" '\033[<64;4;2M'
send '\033[?1000l\033[?1006l'
events wheel:up:"$(cx 3)":"$(ry 1)"
expect_input "alternate screen, tracking off again" ""
send '\033[?1007l\033[?1049l'
events wheel:up:"$(cx 3)":"$(ry 1)"
expect_input "primary screen again" ""
expect_top 0172 "" "primary screen scrolls locally again"
stop_terminal

# Denied Mouse Ops leaves the wheel local even when tracking is requested.
start_terminal -xrm 'XTerm*allowMouseOps: false'
child_command lines:200
send '\033[?1000h\033[?1006h'
events wheel:up:"$(cx 3)":"$(ry 1)"
expect_input "Mouse Ops denied, tracking requested" ""
expect_top 0172 "" "denied reports scroll locally"
stop_terminal

# Drags across viewport scrolling. At the bottom, row R shows line 177+R.
drag()
{
    label=$1
    line_count=$2
    expected=$3
    shift 3
    start_terminal
    child_command "lines:$line_count"
    events pause:400 "$@"
    expect_selection "$label" "$expected"
    stop_terminal
}

drag "forward drag, wheel up, extend" 200 'line 0182\nline 0183\nline' \
    press:1:"$(cx 0)":"$(ry 5)" motion:"$(cx 4)":"$(ry 10)" wheel:up \
    motion:"$(cx 4)":"$(ry 12)" release:1:"$(cx 4)":"$(ry 12)"
drag "reverse drag, two ticks up, extend" 200 "$("$python" -c '
print("ne 0172\\n" + "\\n".join(f"line {n:04d}" for n in range(173, 197)) + "\\nline 019", end="")')" \
    press:1:"$(cx 8)":"$(ry 20)" motion:"$(cx 8)":"$(ry 18)" wheel:up wheel:up \
    motion:"$(cx 2)":"$(ry 5)" release:1:"$(cx 2)":"$(ry 5)"
drag "wheel down after up, extend" 200 "$("$python" -c '
print("\\n".join(f"line {n:04d}" for n in range(172, 181)) + "\\nline", end="")')" \
    wheel:up:"$(cx 0)":"$(ry 5)" wheel:up pause:300 press:1:"$(cx 0)":"$(ry 5)" \
    motion:"$(cx 4)":"$(ry 8)" wheel:down motion:"$(cx 4)":"$(ry 9)" \
    release:1:"$(cx 4)":"$(ry 9)"
drag "wheel, release without motion" 200 'line 018' \
    press:1:"$(cx 0)":"$(ry 5)" motion:"$(cx 8)":"$(ry 10)" wheel:up \
    release:1:"$(cx 8)":"$(ry 10)"
drag "wheel, release outside the grid" 200 'line 018' \
    press:1:"$(cx 0)":"$(ry 5)" motion:"$(cx 8)":"$(ry 10)" wheel:up release:1:0:"$(ry 10)"
# 60 lines: the bottom shows 0037..0060 and the top of scrollback 0001..0024.
drag "autoscroll up to the top, extend" 60 "$("$python" -c '
print("ne 0001\\n" + "\\n".join(f"line {n:04d}" for n in range(2, 42)), end="")')" \
    press:1:"$(cx 0)":"$(ry 5)" motion:"$(cx 0)":-20 pause:1500 \
    motion:"$(cx 2)":"$(ry 0)" release:1:"$(cx 2)":"$(ry 0)"
drag "autoscroll down to the bottom" 60 "$("$python" -c '
print("e 0024\\n" + "\\n".join(f"line {n:04d}" for n in range(25, 60)) + "\\nline", end="")')" \
    wheel:up:"$(cx 0)":"$(ry 5)" wheel:up wheel:up pause:300 press:1:"$(cx 3)":"$(ry 2)" \
    motion:"$(cx 3)":400 pause:1500 motion:"$(cx 5)":"$(ry 23)" release:1:"$(cx 5)":"$(ry 23)"

echo "wheel reports, handoffs and drags across scrolling are exact"
