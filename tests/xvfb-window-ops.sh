#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xvfb=$1
terminal=$2
python=$3
driver=$4
toggle=$5
reader=$6
xwininfo=$7
owner=$8
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS
for policy in default setonly
do
    case_dir=$test_dir/$policy
    mkdir "$case_dir"
    log=$case_dir/log
    disallowed='GetSelection,SetSelection'
    expected=enabled
    if test "$policy" = setonly
    then
        disallowed=GetSelection
        expected=after
    fi
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed \
        -xrm "XTerm*disallowedWindowOps: $disallowed" \
        -e "$python" "$driver" "$case_dir" "$policy" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    for number in 1 2
    do
        xtp_wait_for_title "$log" "window-ops-toggle-$number" "toggle $number"
        window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
        "$toggle" "$window"
        state=true
        test "$number" = 1 || state=false
        xtp_wait_for_log "$log" "selection: allowWindowOps=$state" "runtime policy $state"
        touch "$case_dir/toggled-$number"
    done
    xtp_wait_for_title "$log" window-ops-check-selection "final selection"
    actual=$("$reader" CLIPBOARD)
    test "$actual" = "$expected" || { echo "clipboard: expected $expected, got $actual" >&2; exit 1; }
    touch "$case_dir/checked"
    xtp_wait_for_title "$log" window-ops-done "driver completion"
    if ! wait "$terminal_pid"
    then
        terminal_pid=
        cat "$log" >&2
        exit 1
    fi
    terminal_pid=
done
echo "Allow Window Ops toggles live reads/writes and restores configured restrictions"

# CSI 14/16/18 t through the same policy. Expectations are XTerm(411)'s, measured with
# the same resources: CSI 16 t is gated by GetScreenSizeChars (19) and "16" names
# nothing; allowSendEvents blocks the blanket permission while the list still decides.
serve_runs=0

serve_terminal()
{
    serve_runs=$((serve_runs + 1))
    case_dir=$test_dir/serve$serve_runs
    mkdir "$case_dir"
    log=$case_dir/log
    step=0
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fn fixed -T reports \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' "$@" \
        -e "$python" "$driver" "$case_dir" serve >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'report window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    grid=$(sed -n 's/.*shell: geometry request pixels=[0-9x]* grid=\([0-9]*x[0-9]*\) cell=\([0-9]*x[0-9]*\).*/\1 \2/p' "$log" | tail -1)
    set -- $grid
    columns=${1%x*} rows=${1#*x} cell_width=${2%x*} cell_height=${2#*x}
    reply14="\033[4;$((rows * cell_height));$((columns * cell_width))t"
    reply16="\033[6;${cell_height};${cell_width}t"
    reply18="\033[8;${rows};${columns}t"
}

serve_command()
{
    step=$((step + 1))
    printf '%s\n' "$1" >"$case_dir/go.$step.tmp"
    mv "$case_dir/go.$step.tmp" "$case_dir/go.$step"
    attempt=0
    while test ! -f "$case_dir/done.$step"; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 400 || { echo "driver did not finish: $1" >&2; cat "$log" >&2; exit 1; }
        sleep 0.02
    done
    if grep -q '^ERROR' "$case_dir/res.$step"; then
        echo "driver after '$1': $(cat "$case_dir/res.$step")" >&2
        exit 1
    fi
}

hex()
{
    printf '%b' "$1" | od -An -tx1 | tr -d ' \n'
}

# LABEL COMMAND WANTED: the exact bytes before the status reply ("" is silence).
expect()
{
    serve_command "$2"
    actual=$(od -An -tx1 "$case_dir/res.$step" | tr -d ' \n')
    wanted=$(hex "$3")
    test "$actual" = "$wanted" ||
        { echo "$case $1: got ${actual:-silence}, expected ${wanted:-silence}" >&2; exit 1; }
}

stop_serving()
{
    serve_command quit
    wait "$terminal_pid"
    terminal_pid=
}

# CASE "Y/N for 14 16 18" RESOURCE...
report_case()
{
    case=$1
    answers=$2
    shift 2
    serve_terminal "$@"
    set -- $answers
    for op in 14 16 18; do
        eval "wanted=\$reply$op"
        test "$1" = y || wanted=''
        expect "CSI $op t" "ask:$(hex "\033[${op}t")" "$wanted"
        shift
    done
    stop_serving
    printf 'report %-52s 14/16/18 answered: %s\n' "$case" "$answers"
}

report_case defaults 'y y y'
report_case 'disallowed=GetWinSizePixels' 'n y y' -xrm 'XTerm*disallowedWindowOps: GetWinSizePixels'
report_case 'disallowed=14' 'n y y' -xrm 'XTerm*disallowedWindowOps: 14'
report_case 'disallowed=16 (names nothing)' 'y y y' -xrm 'XTerm*disallowedWindowOps: 16'
report_case 'disallowed=GetScreenSizeChars' 'y n y' -xrm 'XTerm*disallowedWindowOps: GetScreenSizeChars'
report_case 'disallowed=19' 'y n y' -xrm 'XTerm*disallowedWindowOps: 19'
report_case 'disallowed=GetWinSizeChars' 'y y n' -xrm 'XTerm*disallowedWindowOps: GetWinSizeChars'
report_case 'disallowed=GetWinSize*' 'n y n' -xrm 'XTerm*disallowedWindowOps: GetWinSize*'
report_case 'disallowed=*,~GetWinSizeChars' 'n n y' -xrm 'XTerm*disallowedWindowOps: *,~GetWinSizeChars'
report_case 'allowWindowOps=true disallowed=*' 'y y y' -xrm 'XTerm*allowWindowOps: true' \
    -xrm 'XTerm*disallowedWindowOps: *'
report_case 'allowSendEvents allowWindowOps=true disallowed=*' 'n n n' \
    -xrm 'XTerm*allowSendEvents: true' -xrm 'XTerm*allowWindowOps: true' \
    -xrm 'XTerm*disallowedWindowOps: *'
report_case 'allowSendEvents allowWindowOps=true defaults' 'y y y' \
    -xrm 'XTerm*allowSendEvents: true' -xrm 'XTerm*allowWindowOps: true'

# Geometry against the X window, split requests and neighbouring replies.
case='geometry and ordering'
serve_terminal
child=$("$xwininfo" -tree -id "$window" | awk '/^ +0x/ { print $1; exit }')
size=$("$xwininfo" -id "$child" | awk '/Width:/ { w = $2 } /Height:/ { h = $2 } END { print h ";" w }')
border=$(sed -n 's/.*internalBorder=\([0-9]*\).*/\1/p' "$log" | head -1)
text_area="$(( ${size%;*} - 2 * border ));$(( ${size#*;} - 2 * border ))"
test "\033[4;${text_area}t" = "$reply14" ||
    { echo "CSI 14 t $reply14 does not match the X text area $text_area" >&2; exit 1; }
expect 'split CSI 14 t' "split:$(hex '\033[1'):$(hex '4t')" "$reply14"
expect 'split CSI 18 t' "split:$(hex '\033[18'):$(hex 't')" "$reply18"
expect 'CPR, CSI 14 t, CPR' "ask:$(hex '\033[6n\033[14t\033[6n')" "\033[1;1R$reply14\033[1;1R"
stop_serving
echo "report geometry: CSI 14 t matches the X window's text area $text_area; split and neighbouring replies in order"
case='denied ordering'
serve_terminal -xrm 'XTerm*disallowedWindowOps: GetWinSize*'
expect 'split denied CSI 14 t' "split:$(hex '\033[1'):$(hex '4t')" ''
expect 'CPR, denied CSI 14 t, CPR' "ask:$(hex '\033[6n\033[14t\033[6n')" '\033[1;1R\033[1;1R'
expect 'CPR, denied CSI 18 t, CSI 16 t' "ask:$(hex '\033[6n\033[18t\033[16t')" "\033[1;1R$reply16"
stop_serving
echo "report denials: silent, split or not, with neighbouring replies intact"

# Live policy: the menu entry changes the blanket permission, and title reports follow it.
case='live menu'
serve_terminal -xrm 'XTerm*disallowedWindowOps: *'
expect 'denied at startup' "ask:$(hex '\033[14t\033[21t')" ''
"$toggle" "$window" >/dev/null
xtp_wait_for_log "$log" 'selection: allowWindowOps=true' 'menu on'
expect 'after the menu turned it on' "ask:$(hex '\033[14t\033[21t')" "$reply14\033]lreports\033\\\\"
"$toggle" "$window" >/dev/null
xtp_wait_for_log "$log" 'selection: allowWindowOps=false' 'menu off'
expect 'after the menu turned it off' "ask:$(hex '\033[14t\033[21t')" ''
stop_serving
echo "report live: menu off -> on -> off gates CSI 14 t and CSI 21 t together"

case='allowSendEvents menu'
serve_terminal -xrm 'XTerm*allowSendEvents: true' -xrm 'XTerm*allowWindowOps: true' \
    -xrm 'XTerm*disallowedWindowOps: *'
before=$(grep -c 'selection: allowWindowOps=' "$log" || true)
"$toggle" "$window" >/dev/null
sleep 0.5
test "$(grep -c 'selection: allowWindowOps=' "$log" || true)" = "$before" ||
    { echo 'the insensitive Allow Window Ops entry changed the setting' >&2; exit 1; }
expect 'still blocked' "ask:$(hex '\033[14t')" ''
stop_serving
echo "report allowSendEvents: the Allow Window Ops entry is insensitive; reports stay blocked"
# OSC 52 shares the predicate: with allowSendEvents true the blanket is blocked even
# though allowWindowOps is true, and disallowedWindowOps alone decides each direction.
# An external owner serves "seed" first, so a refused write leaves it in place.
b64()
{
    printf '%s' "$1" | base64
}

osc52_case()
{
    case=$1
    sends=$2
    list=$3
    write_allowed=$4
    read_allowed=$5
    : >"$test_dir/owner.out"
    "$owner" --serve CLIPBOARD "UTF8_STRING=UTF8_STRING:$(printf seed | od -An -tx1 | tr -d ' \n')" \
        >"$test_dir/owner.out" 2>&1 &
    aux_pid=$!
    attempt=0
    until grep -q ready "$test_dir/owner.out"; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || { echo 'external owner did not start' >&2; exit 1; }
        sleep 0.02
    done
    serve_terminal -xrm "XTerm*allowSendEvents: $sends" -xrm 'XTerm*allowWindowOps: true' \
        -xrm "XTerm*disallowedWindowOps: $list"
    text="w1-$serve_runs"
    expect 'OSC 52 write' "ask:$(hex "\033]52;c;$(b64 "$text")\007")" ''
    sleep 0.2
    held=$("$reader" CLIPBOARD)
    wanted=seed
    test "$write_allowed" = y && wanted=$text
    test "$held" = "$wanted" || { echo "$case: CLIPBOARD holds '$held', expected '$wanted'" >&2; exit 1; }
    reply=''
    test "$read_allowed" = y && reply="\033]52;c;$(b64 "$wanted")\007"
    expect 'OSC 52 query' "ask:$(hex '\033]52;c;?\007')" "$reply"
    stop_serving
    kill "$aux_pid" 2>/dev/null || true
    wait "$aux_pid" 2>/dev/null || true
    aux_pid=
    answer=silence
    test -n "$reply" && answer=reply
    printf 'osc52 %-52s CLIPBOARD=%-6s query=%s\n' "$case" "$held" "$answer"
}

osc52_case 'allowSendEvents=false disallowed=* (blanket)' false '*' y y
osc52_case 'allowSendEvents=true disallowed=* (deny all)' true '*' n n
osc52_case 'allowSendEvents=true disallowed=GetSelection' true GetSelection y n
osc52_case 'allowSendEvents=true disallowed=SetSelection' true SetSelection n y
echo "Window Ops size reports and OSC 52 under allowSendEvents verified"
