#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xvfb=$1
terminal=$2
python=$3
driver=$4
xprop=$5
toggle=$6
keys=$7
watcher=$8
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/home"

checkpoint()
{
    name=$1
    wanted=$2
    attempt=0
    while test ! -f "$case_dir/$name.ready"; do
        attempt=$((attempt + 1))
        if test "$attempt" -gt 400; then
            cat "$case_dir/out" "$log" >&2
            exit 1
        fi
        sleep 0.02
    done
    "$xprop" -id "$window" WM_NAME > "$case_dir/property"
    grep -Fx "WM_NAME(STRING) = \"$wanted\"" "$case_dir/property"
}

for initial in true false; do
    case_dir=$test_dir/$initial
    mkdir "$case_dir"
    log=$case_dir/log
    HOME="$test_dir/home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 -T startup \
        -xrm 'XTerm*allowWindowOps: true' \
        -xrm "XTerm*allowTitleOps: $initial" \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' \
        -e "$python" "$driver" "$case_dir" "$initial" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'terminal window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    if test "$initial" = true; then
        checkpoint initial first
    else
        checkpoint initial startup
        "$toggle" "$window" title
        xtp_wait_for_log "$log" 'shell: allowTitleOps=true' 'enable initially denied titles'
    fi
    touch "$case_dir/initial.done"
    checkpoint disable current
    "$toggle" "$window" title
    xtp_wait_for_log "$log" 'shell: allowTitleOps=false' 'disable titles'
    touch "$case_dir/disable.done"
    checkpoint enable current
    "$toggle" "$window" title
    touch "$case_dir/enable.done"
    checkpoint restored current
    touch "$case_dir/restored.done"
    wait "$terminal_pid"
    terminal_pid=
    test -f "$case_dir/passed"
done
# allow-title-ops, menu agreement, sameName and the title stack. Expectations are
# XTerm(411)'s, measured with the same keys and observer.
bindings='XTerm*VT100.translations: #override <Key>F1: allow-title-ops(on)\n<Key>F2: allow-title-ops(off)\n<Key>F3: allow-title-ops(toggle)\n<Key>F4: allow-title-ops()\n<Key>F5: allow-title-ops(bogus)\n<Key>F6: allow-title-ops(ON)\n<Key>F7: allow-title-ops(OFF)\n<Key>F8: allow-title-ops(on,off)'
serve_runs=0

serve_terminal()
{
    serve_runs=$((serve_runs + 1))
    case_dir=$test_dir/serve$serve_runs
    mkdir "$case_dir"
    log=$case_dir/log
    step=0
    probes=0
    HOME="$test_dir/home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 -T startup -n starticon \
        -xrm 'XTerm*allowWindowOps: true' -xrm "$bindings" \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' "$@" \
        -e "$python" "$driver" "$case_dir" serve >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'terminal window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    : >"$case_dir/props"
    "$watcher" "$window" >>"$case_dir/props" &
    aux_pid=$!
    attempt=0
    while ! grep -q '^ready$' "$case_dir/props"; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || { echo 'property observer did not start' >&2; exit 1; }
        sleep 0.02
    done
}

stop_serving()
{
    serve_command quit
    wait "$terminal_pid"
    terminal_pid=
    kill "$aux_pid" 2>/dev/null || true
    wait "$aux_pid" 2>/dev/null || true
    aux_pid=
}

serve_command()
{
    step=$((step + 1))
    printf '%s\n' "$1" >"$case_dir/go.$step.tmp"
    mv "$case_dir/go.$step.tmp" "$case_dir/go.$step"
    attempt=0
    while test ! -f "$case_dir/done.$step"; do
        attempt=$((attempt + 1))
        if test "$attempt" -gt 400; then
            echo "driver did not finish: $1" >&2
            cat "$log" >&2
            exit 1
        fi
        sleep 0.02
    done
    if grep -q '^ERROR' "$case_dir/res.$step"; then
        echo "driver after '$1': $(cat "$case_dir/res.$step")" >&2
        exit 1
    fi
}

send()
{
    serve_command "send:$(printf '%b' "$1" | od -An -tx1 | tr -d ' \n')"
    sleep 0.2
}

wm_name()
{
    "$xprop" -id "$window" WM_NAME | sed 's/^WM_NAME(STRING) = //'
}

mark()
{
    echo "--- $1" >>"$case_dir/props"
}

# WM_NAME/WM_ICON_NAME notifications since the named mark.
notifications()
{
    awk -v m="--- $1" -v n="$2" '$0 == m { on = 1; next } /^---/ { on = 0 } on && $1 == n' \
        "$case_dir/props" | wc -l | tr -d ' '
}

# A fresh title shows whether Title Ops currently allows changes.
title_state()
{
    probes=$((probes + 1))
    send "\033]2;probe$probes\033\\"
    if test "$(wm_name)" = "\"probe$probes\""; then state=allowed; else state=denied; fi
}

# Presses KEY, waits for the terminal to change the permission or ring the bell,
# and checks the resulting state against WANT and whether it rang. The probe title
# that follows fails if any byte of the key reached the application.
press()
{
    key=$1
    want=$2
    bell=$3
    before=$(grep -c 'shell: allowTitleOps=\|allow-title-ops left allowTitleOps=' "$log" || true)
    "$keys" "$window" keysym "$key" >/dev/null
    attempt=0
    while test "$(grep -c 'shell: allowTitleOps=\|allow-title-ops left allowTitleOps=' "$log" || true)" -le "$before"; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || { echo "no response to $key" >&2; exit 1; }
        sleep 0.02
    done
    rang=no
    tail -n +1 "$log" | grep 'shell: allowTitleOps=\|allow-title-ops left allowTitleOps=' | tail -1 |
        grep -q 'allow-title-ops left' && rang=yes
    title_state
    printf 'action %-26s -> %-7s bell=%s\n' "$key ($4)" "$state" "$rang"
    test "$state" = "$want" && test "$rang" = "$bell" || {
        echo "$key ($4): expected $want bell=$bell" >&2
        grep -E 'allowTitleOps|allow-title-ops|title changed|denied|key ' "$log" | tail -20 >&2
        exit 1
    }
}

menu_toggle()
{
    "$toggle" "$window" title >/dev/null
    xtp_wait_for_log "$log" "shell: allowTitleOps=$1" "menu toggle to $1"
    title_state
    printf 'menu   %-26s -> %s\n' "toggle" "$state"
}

serve_terminal
title_state
test "$state" = allowed || { echo 'titles denied at startup' >&2; exit 1; }
press F1 allowed yes 'on while on'
press F2 denied no off
press F2 denied yes 'off while off'
press F1 allowed no on
press F3 denied no toggle
press F3 allowed no toggle
press F4 denied no 'no argument'
press F4 allowed no 'no argument'
press F5 allowed yes bogus
press F7 denied no OFF
press F6 allowed no ON
press F8 allowed yes 'two arguments'
# The action and the menu share one state.
press F2 denied no off
menu_toggle true
press F1 allowed yes 'on after the menu'
menu_toggle false
press F2 denied yes 'off after the menu'
press F1 allowed no on
stop_serving

serve_terminal -xrm 'XTerm*allowTitleOps: false'
title_state
test "$state" = denied || { echo 'allowTitleOps: false was not honored' >&2; exit 1; }
press F2 denied yes 'off at startup off'
press F1 allowed no on
stop_serving

# allowSendEvents blocks Title Ops completely, as XTerm(411) measured: no exceptions,
# the action still records the configured value, and the menu entry is insensitive.
# It is startup-only here, so no recovery can be exercised.
expect_report()
{
    serve_command report
    actual=$(od -An -tx1 "$case_dir/res.$step" | tr -d ' \n')
    wanted=$(printf '%b' "$1" | od -An -tx1 | tr -d ' \n')
    test "$actual" = "$wanted" || { echo "$2: report $actual, expected $wanted" >&2; exit 1; }
}

log_changes()
{
    grep -c 'shell: allowTitleOps=\|allow-title-ops left allowTitleOps=' "$log" || true
}

# KEY LABEL CONFIGURED BELL: effective permission stays denied whatever the key does.
press_blocked()
{
    before=$(log_changes)
    "$keys" "$window" keysym "$1" >/dev/null
    attempt=0
    while test "$(log_changes)" -le "$before"; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || { echo "no response to $1" >&2; exit 1; }
        sleep 0.02
    done
    last=$(grep 'shell: allowTitleOps=\|allow-title-ops left allowTitleOps=' "$log" | tail -1)
    rang=no
    case $last in *'allow-title-ops left'*) rang=yes ;; esac
    configured=$(printf '%s\n' "$last" | sed -n 's/.*allowTitleOps=\([a-z]*\).*/\1/p')
    case $last in
    *effective=true*) echo "$1 ($2): effective permission while allowSendEvents is true" >&2; exit 1 ;;
    esac
    title_state
    printf 'allowSendEvents action %-18s configured=%-5s bell=%-3s effective=%s\n' "$1 ($2)" \
        "$configured" "$rang" "$state"
    test "$state" = denied && test "$configured" = "$3" && test "$rang" = "$4" ||
        { echo "$1 ($2): expected configured=$3 bell=$4 denied" >&2; exit 1; }
}

for titles in true false; do
    # allowSendEvents also blocks the Window Ops blanket; an empty list keeps reports and
    # the stack available, as in the XTerm(411) measurement.
    serve_terminal -xrm 'XTerm*allowSendEvents: true' -xrm "XTerm*allowTitleOps: $titles" \
        -xrm 'XTerm*disallowedWindowOps:'
    mark blocked
    send '\033]2;blocked\033\\\033]2;blocked\033\\\033]0;blocked0\033\\'
    test "$(notifications blocked WM_NAME)" = 0 ||
        { echo "allowSendEvents titles=$titles: WM_NAME changed" >&2; exit 1; }
    # Window Ops reports and the stack stay governed by Window Ops alone.
    expect_report '\033]lstartup\033\\' "allowSendEvents titles=$titles"
    send '\033[22;0t\033[22;0t\033]2;popped\033\\\033[23;0t'
    expect_report '\033]lstartup\033\\' "allowSendEvents blocked pop, titles=$titles"
    grep -q 'title restoration denied allowTitleOps='"$titles"' allowSendEvents=true' "$log" ||
        { echo 'blocked pop was not refused' >&2; exit 1; }
    grep -q 'title pop target=0 slot=0 used=1' "$log" ||
        { echo 'the blocked pop did not consume its entry' >&2; exit 1; }
    send '\033[23;0t'
    grep -q 'title pop target=0 slot=0 used=0' "$log" ||
        { echo 'the second pop did not consume the remaining entry' >&2; exit 1; }
    send '\033[23;0t'
    grep -q 'title pop ignored: stack empty' "$log" ||
        { echo 'a third pop found entries left' >&2; exit 1; }
    printf 'allowSendEvents=true allowTitleOps=%-5s labels blocked, report exact, blocked pops consume\n' "$titles"
    if test "$titles" = true; then
        press_blocked F1 'on while on' true yes
        press_blocked F2 off false no
        press_blocked F2 'off while off' false yes
        press_blocked F1 on true no
        press_blocked F3 toggle false no
        press_blocked F3 toggle true no
        press_blocked F4 'no argument' false no
        press_blocked F4 'no argument' true no
        press_blocked F5 bogus true yes
        press_blocked F7 OFF false no
        press_blocked F6 ON true no
        press_blocked F8 'two arguments' true yes
        # The insensitive menu entry does not change the configured value.
        before=$(log_changes)
        "$toggle" "$window" title >/dev/null
        sleep 0.5
        test "$(log_changes)" = "$before" ||
            { echo 'the insensitive Allow Title Ops entry changed the setting' >&2; exit 1; }
        press_blocked F1 'on after the menu' true yes
        echo 'allowSendEvents=true menu click: entry insensitive, configured value unchanged'
    fi
    stop_serving
done

for same in default true false; do
    if test "$same" = default; then
        serve_terminal
    else
        serve_terminal -xrm "XTerm*sameName: $same"
    fi
    mark repeated
    send '\033]2;A\033\\\033]2;A\033\\\033]2;B\033\\\033]2;B\033\\'
    repeated=$(notifications repeated WM_NAME)
    mark osc0
    send '\033]0;B\033\\\033]0;B\033\\'
    osc0=$(notifications osc0 WM_NAME)
    "$xprop" -id "$window" -set WM_NAME external
    sleep 0.2
    mark resend
    send '\033]2;B\033\\'
    resend=$(notifications resend WM_NAME)
    after_external=$(wm_name)
    send '\033]2;P\033\\\033[22;0t\033[22;0t'
    mark stack-same
    send '\033]2;P\033\\\033[23;0t'
    stack_same=$(notifications stack-same WM_NAME)/$(notifications stack-same WM_ICON_NAME)
    mark stack-next
    send '\033]2;Q\033\\\033[23;0t'
    stack_next=$(notifications stack-next WM_NAME)
    after_next=$(wm_name)
    mark stack-empty
    send '\033]2;R\033\\\033[23;0t'
    stack_empty=$(notifications stack-empty WM_NAME)
    after_empty=$(wm_name)
    stop_serving
    printf 'sameName %-7s A,A,B,B=%s  OSC0 B,B=%s  resend after external=%s %s  push,P,pop=%s  Q,pop=%s %s  R,pop=%s %s\n' \
        "$same" "$repeated" "$osc0" "$resend" "$after_external" "$stack_same" \
        "$stack_next" "$after_next" "$stack_empty" "$after_empty"
    if test "$same" = false; then
        expected='4 2 1 "B" 2/1 2 "P" 1 "R"'
    else
        expected='0 0 0 "external" 0/0 2 "P" 1 "R"'
        expected="2 ${expected#0 }"
    fi
    actual="$repeated $osc0 $resend $after_external $stack_same $stack_next $after_next $stack_empty $after_empty"
    test "$actual" = "$expected" || { echo "sameName $same: got [$actual], expected [$expected]" >&2; exit 1; }
done
echo 'Allow Title Ops resources, live menu, action, sameName, reports, and pop policy verified'
