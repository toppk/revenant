#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
sender=$3
ink=$4
resizer=$5
focuser=$6

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# Six OSC 133 command blocks: a prompt row (inverse video so the top row's ink
# shows which row is visible), the command, then 30 empty output rows. Prompt 3
# is 150 characters, so it wraps into two rows at 80 columns and three at 60.
long=$(printf 'x%.0s' $(seq 1 140))
script=$test_dir/child.sh
cat >"$script" <<EOF
wait_step() { while ! test -e "$test_dir/step-\$1"; do sleep 0.05; done; }
rows=\${OUTPUT_ROWS:-30}
block() {
    printf '\\033]133;A\\033\\\\\\033[7mP%s%s\\033[0m\$ \\033]133;B\\033\\\\cmd%s\\r\\n\\033]133;C\\033\\\\' "\$1" "\$2" "\$1"
    i=0
    while test \$i -lt "\$rows"; do printf '\\r\\n'; i=\$((i + 1)); done
    printf '\\033]133;D;0\\033\\\\'
}
n=1
while test \$n -le "\${BLOCKS:-6}"
do
    if test \$n -eq 3; then block 3 '$long'; else block \$n ''; fi
    n=\$((n + 1))
done
printf '\\033]133;A\\033\\\\\\033[7mLIVE\\033[0m\$ \\033]133;B\\033\\\\'
printf '\\033]2;prompts-ready\\007'
wait_step 1
printf '\\033]2;resized-ready\\007'
wait_step 2
printf '\\033[?1049h\\033[Halternate\\r\\n\\033]2;alt-ready\\007'
wait_step 3
printf '\\033[?1049l\\033]2;primary-ready\\007'
wait_step 4
EOF

start_terminal()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fn fixed -geometry 80x24 \
        -xrm 'xterm.vt100.internalBorder: 2' \
        -xrm 'xterm.vt100.background: #ffffff' \
        -xrm 'xterm.vt100.foreground: #000000' "$@" >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
}

stop_terminal()
{
    kill "$terminal_pid" 2>/dev/null || true
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

press()
{
    "$sender" "$window" "$1"
}

# The top text row's first four cells: a prompt there is inverse video (ink),
# an empty output row is blank.
top_row_class()
{
    "$ink" "$window" --expose 2 2 24 13 0xffffff | sed 's/^class=\([a-z]*\).*/\1/'
}

expect_top()
{
    actual=$(top_row_class)
    case $actual in
        mono|color) actual=ink ;;
    esac
    if test "$actual" != "$1"
    then
        echo "$2: expected the top row to be $1, sampled $actual" >&2
        sed -n '1,200p' "$log" >&2
        exit 1
    fi
}

expect_count()
{
    actual=$(grep -c -F -- "$1" "$log" || true)
    if test "$actual" -ne "$2"
    then
        echo "$3: expected $2 occurrences of '$1', found $actual" >&2
        grep -F "prompt navigation" "$log" >&2 || true
        exit 1
    fi
}

jump()
{
    press "$1"
    xtp_wait_for_log "$log" "prompt navigation direction=$2 from=$3 to=$4" "$2 from $3"
    expect_top ink "$2 to row $4"
}

# Rows at 80 columns: block i starts at 31 * (i - 1), plus one for the wrapped
# prompt 3 from block 4 on; the live prompt is row 187 and the active area
# starts at row 164.
start_terminal primary -sl 1000 -e sh "$script"
xtp_wait_for_title "$log" prompts-ready "fixture"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
expect_top blank "live view"
jump ctrl-shift-up previous 164 156
jump ctrl-shift-up previous 156 125
jump ctrl-shift-up previous 125 94
jump ctrl-shift-up previous 94 62
jump ctrl-shift-up previous 62 31
jump ctrl-shift-up previous 31 0
press ctrl-shift-up
xtp_wait_for_log "$log" "prompt navigation direction=previous from=0: no previous prompt" "top boundary"
expect_top ink "top boundary"
jump ctrl-shift-down next 0 31
jump ctrl-shift-down next 31 62
jump ctrl-shift-down next 62 94
jump ctrl-shift-down next 94 125
jump ctrl-shift-down next 125 156
press ctrl-shift-down
xtp_wait_for_log "$log" "prompt navigation direction=next from=156 to=187" "live prompt"
expect_top blank "live prompt returns to the active view"
press ctrl-shift-down
xtp_wait_for_log "$log" "prompt navigation direction=next from=164: already at the live view" "bottom boundary"
expect_top blank "bottom boundary"

# At 60 columns prompt 3 takes three rows, so every later prompt moves down one.
"$resizer" "$window" --grid 60 60 24 100 >/dev/null
xtp_wait_for_log "$log" "VT100 grid changed 80x24 -> 60x24" "reflowed grid"
touch "$test_dir/step-1"
xtp_wait_for_title "$log" resized-ready "resize"
jump ctrl-shift-up previous 165 157
jump ctrl-shift-up previous 157 126
jump ctrl-shift-up previous 126 95
jump ctrl-shift-up previous 95 62
jump ctrl-shift-down next 62 95
press ctrl-shift-down
press ctrl-shift-down
press ctrl-shift-down
xtp_wait_for_log "$log" "prompt navigation direction=next from=157 to=188" "live prompt after reflow"
expect_top blank "live view after reflow"

touch "$test_dir/step-2"
xtp_wait_for_title "$log" alt-ready "alternate screen"
press ctrl-shift-up
xtp_wait_for_log "$log" "prompt navigation direction=previous: no history" "alternate screen"
press ctrl-shift-down
xtp_wait_for_log "$log" "prompt navigation direction=next: no history" "alternate screen forward"
touch "$test_dir/step-3"
xtp_wait_for_title "$log" primary-ready "primary screen"
jump ctrl-shift-up previous 165 157
touch "$test_dir/step-4"
if ! wait "$terminal_pid"
then
    terminal_pid=
    echo "xterm+ failed while exiting from the primary case" >&2
    sed -n '1,200p' "$log" >&2
    exit 1
fi
terminal_pid=

# Thirty blocks against a 40-line scrollback: libghostty prunes whole pages,
# so early prompts are gone and how many survive depends on page boundaries.
# The walk must end with a boundary report, land only on prompt rows, and
# reach fewer prompts than were written.
rm -f "$test_dir"/step-*
start_terminal evicted -sl 40 -e env BLOCKS=30 sh "$script"
xtp_wait_for_title "$log" prompts-ready "evicted fixture"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
presses=0
while test "$presses" -lt 40
do
    press ctrl-shift-up
    presses=$((presses + 1))
    attempt=0
    while test "$(grep -c -F 'prompt navigation direction=previous' "$log" || true)" -lt "$presses"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || { echo "evicted case: no navigation log" >&2; exit 1; }
        sleep 0.05
    done
    if grep -q -F "no previous prompt" "$log"
    then
        break
    fi
    expect_top ink "evicted case jump $presses"
done
if ! grep -q -F "no previous prompt" "$log" || test "$presses" -ge 30
then
    echo "evicted case never reached the boundary or retained every prompt: $presses presses" >&2
    grep -F "prompt navigation" "$log" >&2
    exit 1
fi
touch "$test_dir/step-1"; touch "$test_dir/step-2"; touch "$test_dir/step-3"; touch "$test_dir/step-4"
stop_terminal

# Overriding the bindings with xterm's insert-seven-bit action restores key
# delivery: the raw child receives CSI 1;6 A and no navigation runs. A control
# run with the default bindings receives nothing.
override_case()
{
    case_name=$1
    expected=$2
    gesture=$3
    shift 3
    rm -f "$test_dir/keys" "$test_dir/keys-ready"
    start_terminal "$case_name" -sl 1000 "$@" -e sh -c "stty raw -echo; printf '\033[>2u'; : >'$test_dir/keys-ready'; dd if=/dev/tty of='$test_dir/keys' bs=1 count=64 2>/dev/null; sleep 20"
    attempt=0
    while ! test -f "$test_dir/keys-ready"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || { echo "$case_name: raw child did not start" >&2; exit 1; }
        sleep 0.05
    done
    sleep 0.2
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    press "$gesture"
    sleep 0.7
    printf '%s' "$expected" >"$test_dir/keys-expected"
    if ! cmp -s "$test_dir/keys-expected" "$test_dir/keys"
    then
        echo "$case_name: the application received unexpected bytes" >&2
        od -An -c "$test_dir/keys" >&2
        sed -n '1,120p' "$log" >&2
        exit 1
    fi
    stop_terminal
}
# Kitty event reporting (flag 2) is on in the raw child so a leaked release would show.
override_case override "$(printf '\033[1;6:1A\033[1;6:3A')" ctrl-shift-up -xrm 'xterm.vt100.translations: #override Ctrl Shift <KeyPress> Up: insert-seven-bit()'
expect_count "prompt navigation" 0 "override must not navigate"
override_case default '' ctrl-shift-up
expect_count "prompt navigation direction=previous" 1 "default binding must navigate"
expect_count "owned by local Xt action" 2 "default binding must own the press and release"
# Twelve press/release pairs in one flush: every one is owned, none reaches the child.
override_case burst '' ctrl-shift-up-burst
expect_count "prompt navigation direction=previous" 12 "burst must run every navigation"
expect_count "owned by local Xt action" 24 "burst must own every press and release"
expect_count "delivering" 0 "burst must never deliver a bound key"
# Ctrl released before the arrow: the arrow's release is still owned; only the sentinel
# key reaches the child, as a press and a Kitty release report.
override_case modifiers-first "$(printf 'a\033[97;1:3u')" ctrl-shift-up-modifiers-first
expect_count "prompt navigation direction=previous" 1 "modifier-first case must navigate once"
# Focus moves away while Ctrl+Shift+Up is still down: the stale ownership must not eat the
# release of a later plain Up, which the child sees as a press and a Kitty release.
rm -f "$test_dir/keys" "$test_dir/keys-ready"
start_terminal focus-loss -sl 1000 -e sh -c "stty raw -echo; printf '\033[>2u'; : >'$test_dir/keys-ready'; dd if=/dev/tty of='$test_dir/keys' bs=1 count=64 2>/dev/null; sleep 20"
attempt=0
while ! test -f "$test_dir/keys-ready"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || { echo "focus-loss: raw child did not start" >&2; exit 1; }
    sleep 0.05
done
sleep 0.2
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
"$focuser" focus "$window" in
xtp_wait_for_log "$log" "cursor focus=in" "focus-loss focus in"
press ctrl-shift-up-press
xtp_wait_for_log "$log" "prompt navigation direction=previous" "focus-loss navigation"
"$focuser" focus "$window" out
xtp_wait_for_log "$log" "cursor focus=out" "focus-loss focus out"
"$focuser" focus "$window" in
sleep 0.2
press up
sleep 0.7
printf '\033[1;1:1A\033[1;1:3A' >"$test_dir/keys-expected"
if ! cmp -s "$test_dir/keys-expected" "$test_dir/keys"
then
    echo "focus-loss: plain Up after refocus was not delivered whole" >&2
    od -An -c "$test_dir/keys" >&2
    sed -n '1,120p' "$log" >&2
    exit 1
fi
stop_terminal

# Without OSC 133 markers both directions report nothing to do.
rm -f "$test_dir"/step-*
start_terminal plain -sl 1000 -e sh -c 'i=0; while test $i -lt 60; do printf "line %s\r\n" $i; i=$((i + 1)); done; printf "\033]2;plain-ready\007"; sleep 30'
xtp_wait_for_title "$log" plain-ready "plain fixture"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
press ctrl-shift-up
xtp_wait_for_log "$log" "no previous prompt" "plain previous"
press ctrl-shift-down
xtp_wait_for_log "$log" "no next prompt" "plain next"
expect_count "viewport row=" 0 "plain case must not scroll"
stop_terminal

echo "prompt navigation moves the viewport between OSC 133 prompts through history, reflow, eviction, and the alternate screen"
