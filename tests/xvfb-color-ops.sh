#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xvfb=$1
terminal=$2
python=$3
driver=$4
toggle=$5
keys=$6
ink=$7
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/home"

checkpoint()
{
    attempt=0
    while test ! -f "$case_dir/$1.ready"; do
        attempt=$((attempt+1))
        if test "$attempt" -gt 400 || ! kill -0 "$terminal_pid" 2>/dev/null; then
            cat "$case_dir/out" "$log" >&2
            exit 1
        fi
        sleep .02
    done
}

for initial in true false; do
    case_dir=$test_dir/$initial
    mkdir "$case_dir"
    log=$case_dir/log
    HOME="$test_dir/home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 \
        -fg '#102030' -bg '#304050' -cr '#506070' \
        -xrm "XTerm*allowColorOps: $initial" \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' \
        -e "$python" "$driver" "$case_dir" "$initial" >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'color-policy window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    checkpoint initial
    if test "$initial" = false; then
        "$toggle" "$window" color
        xtp_wait_for_log "$log" 'terminal: allowColorOps=true' 'initial enable'
    fi
    touch "$case_dir/initial.done"
    checkpoint disable
    "$toggle" "$window" color
    xtp_wait_for_log "$log" 'terminal: allowColorOps=false' 'disable colors'
    touch "$case_dir/disable.done"
    checkpoint enable
    "$toggle" "$window" color
    touch "$case_dir/enable.done"
    xtp_wait_for_title "$log" color-ops-done 'color policy completion' 2000
    wait "$terminal_pid"
    terminal_pid=
    test -f "$case_dir/passed"
done
# allowSendEvents, measured against XTerm(411) with the same requests: it blocks the
# blanket permission (allowColorOps && !allowSendEvents), while disallowedColorOps
# still selects what is refused, and palette writes are never gated.
bindings='XTerm*VT100.translations: #override <Key>F1: allow-color-ops(on)\n<Key>F2: allow-color-ops(off)\n<Key>F3: allow-color-ops(toggle)\n<Key>F4: allow-color-ops()\n<Key>F5: allow-color-ops(bogus)\n<Key>F6: allow-color-ops(ON)\n<Key>F7: allow-color-ops(OFF)\n<Key>F8: allow-color-ops(on,off)\n<Key>F9: allow-color-ops(true)'
serve_runs=0

serve_terminal()
{
    serve_runs=$((serve_runs + 1))
    case_dir=$test_dir/serve$serve_runs
    mkdir "$case_dir"
    log=$case_dir/log
    step=0
    HOME="$test_dir/home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 -fg '#102030' -bg '#304050' \
        -xrm "$bindings" \
        -xrm 'XTerm*fontMenu*font: fixed' -xrm 'XTerm*fontMenu*vertSpace: 0' "$@" \
        -e "$python" "$driver" "$case_dir" serve >"$case_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'color-policy window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
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

hex()
{
    printf '%b' "$1" | od -An -tx1 | tr -d ' \n'
}

send()
{
    serve_command "send:$(hex "$1")"
}

# The exact reply bytes to REQUEST ("" means the terminal chose silence).
expect_reply()
{
    serve_command "ask:$(hex "$2")"
    actual=$(od -An -tx1 "$case_dir/res.$step" | tr -d ' \n')
    wanted=$(hex "$3")
    if test "$actual" != "$wanted"; then
        echo "$label $1: got ${actual:-silence}, expected ${wanted:-silence}" >&2
        exit 1
    fi
}

# Every pixel of the rectangle has COLOR.
expect_pixels()
{
    sleep 0.2
    result=$("$ink" "$window" --sample "$2" "$3" "$4" "$5" "$6")
    case $result in
    class=blank*) ;;
    *) echo "$label $1: rectangle $2,$3 ${4}x$5 is not $6: $result" >&2; exit 1 ;;
    esac
}

allowed()
{
    test "$blanket" = true && return 0
    case ",$disallowed," in *",$1,"*) return 1 ;; esac
    return 0
}

stop_serving()
{
    serve_command quit
    wait "$terminal_pid"
    terminal_pid=
}

for sends in false true; do
    for colors in true false; do
        for disallowed in SetColor,GetColor,GetAnsiColor '' GetColor SetColor; do
            label="allowSendEvents=$sends allowColorOps=$colors disallowedColorOps=$disallowed"
            blanket=false
            test "$colors" = true && test "$sends" = false && blanket=true
            serve_terminal -xrm "XTerm*allowSendEvents: $sends" -xrm "XTerm*allowColorOps: $colors" \
                -xrm "XTerm*disallowedColorOps: $disallowed"
            grep -q "allowSendEvents=$sends effective=$blanket" "$log" ||
                { echo "$label: startup did not report effective=$blanket" >&2; exit 1; }
            fg='' ansi='' ansi_green='' bg_after='' set_color=304050 mixed_fg=102030
            allowed GetColor && fg='\033]10;rgb:1010/2020/3030\033\\'
            allowed GetAnsiColor && ansi='\033]4;1;rgb:cdcd/0000/0000\033\\'
            allowed GetAnsiColor && ansi_green='\033]4;1;rgb:0000/ffff/0000\033\\'
            allowed SetColor && set_color=ff0000
            allowed SetColor && mixed_fg=aabbcc
            if allowed GetColor; then
                bg_after='\033]11;rgb:3030/4040/5050\033\\'
                allowed SetColor && bg_after='\033]11;rgb:ffff/0000/0000\033\\'
            fi
            expect_reply 'OSC 10 query' '\033]10;?\033\\' "$fg"
            expect_reply 'OSC 4 query' '\033]4;1;?\033\\' "$ansi"
            send '\033[H\033[41m  \033[42m  \033[0m'
            send '\033]4;1;#00ff00\033\\'
            expect_pixels 'palette write' 3 3 8 8 0x00ff00
            expect_reply 'OSC 4 query and write in one list' '\033]4;1;?;2;#0000ff\033\\' "$ansi_green"
            expect_pixels 'palette write in a mixed list' 15 3 8 8 0x0000ff
            send '\033]11;#ff0000\033\\'
            expect_pixels 'OSC 11 background write' 0 0 2 2 "0x$set_color"
            # Reverse-video spaces show the default foreground, whatever GetColor allows.
            send '\033[2;1H\033[7m  \033[0m'
            expect_pixels 'default foreground before the mixed list' 3 16 8 8 0x102030
            expect_reply 'OSC 10 write and OSC 11 query in one list' '\033]10;#aabbcc;?\033\\' "$bg_after"
            expect_pixels 'OSC 10 write in the mixed list' 3 16 8 8 "0x$mixed_fg"
            stop_serving
            printf '%-86s fg=%s palette=%s background=%s mixed-fg=%s\n' "$label" "${fg:+reply}" "${ansi:+reply}" \
                "$set_color" "$mixed_fg"
        done
    done
done

# Live changes act on the configured flag; allowSendEvents keeps the blanket blocked.
press()
{
    key=$1
    label="allowSendEvents=$sends $key ($2)"
    before=$(grep -c 'terminal: allowColorOps=\|allow-color-ops left' "$log" || true)
    if test "$key" = menu; then
        "$toggle" "$window" color >/dev/null
    else
        "$keys" "$window" keysym "$key" >/dev/null
    fi
    attempt=0
    while test "$(grep -c 'terminal: allowColorOps=\|allow-color-ops left' "$log" || true)" -le "$before"; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || { echo "$label: no response" >&2; exit 1; }
        sleep 0.02
    done
    last=$(grep 'terminal: allowColorOps=\|allow-color-ops left' "$log" | tail -1)
    case $last in
    *'allow-color-ops left'*) rang=yes ;;
    *) rang=no ;;
    esac
    configured=$(printf '%s\n' "$last" | sed -n 's/.*allowColorOps=\([a-z]*\).*/\1/p')
    if test "$4" = allowed; then reply='\033]10;rgb:1010/2020/3030\033\\'; else reply=''; fi
    expect_reply 'effective permission' '\033]10;?\033\\' "$reply"
    printf 'live %-40s configured=%-5s bell=%-3s effective=%s\n' "$label" "$configured" "$rang" "$4"
    test "$configured" = "$3" && test "$rang" = "$5" ||
        { echo "$label: expected configured=$3 bell=$5" >&2; exit 1; }
}

for sends in true false; do
    serve_terminal -xrm "XTerm*allowSendEvents: $sends" -xrm 'XTerm*allowColorOps: true'
    on=allowed
    test "$sends" = true && on=denied
    press F1 'on while on' true "$on" yes
    press F2 off false denied no
    press F2 'off while off' false denied yes
    press F1 on true "$on" no
    press F3 toggle false denied no
    press F3 toggle true "$on" no
    press F5 bogus true "$on" yes
    press F4 'no argument' false denied no
    press F4 'no argument' true "$on" no
    press F7 OFF false denied no
    press F6 ON true "$on" no
    press F8 'two arguments' true "$on" yes
    press F9 true true "$on" yes
    press menu 'menu toggle' false denied no
    # A write refused while denied leaves no trace once the permission returns.
    send '\033]10;#abcdef\033\\'
    press F1 'on after the menu' true "$on" no
    press menu 'menu toggle' false denied no
    press menu 'menu toggle' true "$on" no
    stop_serving
done
echo 'Dynamic-color permissions, allowSendEvents, exceptions, mixed lists and live menu verified'
