#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
tool=$3

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# The child sends one notification per step and waits for the test to inspect
# WM_HINTS externally before the next; the last step exits with urgency set.
script=$test_dir/child.sh
cat >"$script" <<EOF
stty raw -echo
wait_step() { while ! test -e "$test_dir/step-\$1"; do sleep 0.05; done; }
wait_step 0
printf '\\033]9;first\\033\\\\'
wait_step 1
printf '\\033]777;notify;Title;second body\\a'
wait_step 2
printf '\\033]9;third\\033\\\\'
wait_step 3
printf 'ab\\033]9;fourth\\033\\\\cd\\033[6n'
IFS= read -r -t 2 -N 6 reply || true
printf '%s' "\$reply" >"$test_dir/cpr"
wait_step 4
EOF

log=$test_dir/log
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug -e sh "$script" >"$test_dir/out" 2>"$log" &
terminal_pid=$!
xtp_wait_for_log "$log" "startup: event loop starting" "startup"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
test -n "$window" || { echo "no realized window in the log" >&2; exit 1; }

query()
{
    "$tool" query "$window"
}

rest()
{
    printf '%s' "$1" | sed 's/^urgent=[01] //'
}

expect_query()
{
    actual=$(query)
    if test "$actual" != "$1"
    then
        echo "$2: expected WM_HINTS '$1', got '$actual'" >&2
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
        sed -n '1,200p' "$log" >&2
        exit 1
    fi
}

"$tool" focus "$window" out
sleep 0.3
# Distinctive values in every hint field prove that only the urgency bit changes.
"$tool" seed "$window"
baseline=$(query)
seeded="urgent=0 flags=0x7f input=1 initial-state=3 icon-pixmap=0x51 icon-window=0x52 icon-x=37 icon-y=41 icon-mask=0x53 group=$window"
if test "$baseline" != "$seeded"
then
    echo "seeded WM_HINTS did not read back: '$baseline'" >&2
    exit 1
fi
fields=$(rest "$baseline")

touch "$test_dir/step-0"
xtp_wait_for_log "$log" "notification received count=1 focus=out" "first notification"
xtp_wait_for_log "$log" "urgency hint set" "urgency after the first notification"
expect_query "urgent=1 $fields" "unfocused notification"

touch "$test_dir/step-1"
xtp_wait_for_log "$log" "notification received count=2 focus=out" "repeated notification"
xtp_wait_for_log "$log" 'notification title bytes=5 preview="Title"' "OSC 777 title"
xtp_wait_for_log "$log" 'notification body bytes=11 preview="second body"' "OSC 777 body"
expect_count "urgency hint set" 1 "repeat must not re-apply the hint"
expect_query "urgent=1 $fields" "repeated notification"

"$tool" focus "$window" in
xtp_wait_for_log "$log" "urgency hint cleared" "focus clears urgency"
xtp_wait_for_log "$log" "cursor focus=in" "widget focus"
expect_query "$baseline" "focus clear"

touch "$test_dir/step-2"
xtp_wait_for_log "$log" "notification received count=3 focus=in" "focused notification"
expect_count "urgency hint set" 1 "focused notification must not set the hint"
expect_query "$baseline" "focused notification"

"$tool" focus "$window" out
xtp_wait_for_log "$log" "cursor focus=out" "widget unfocus"
touch "$test_dir/step-3"
xtp_wait_for_log "$log" "notification received count=4 focus=out" "notification after unfocus"
xtp_wait_for_log "$log" 'notification body bytes=6 preview="fourth"' "fourth body"
expect_count "urgency hint set" 2 "urgency after refocus cycle"
expect_query "urgent=1 $fields" "notification after unfocus"
attempt=0
while ! test -s "$test_dir/cpr"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || { echo "no CPR reply after the notification" >&2; exit 1; }
    sleep 0.05
done
printf '\033[1;5R' >"$test_dir/cpr-expected"
if ! cmp -s "$test_dir/cpr-expected" "$test_dir/cpr"
then
    echo "neighboring output or CPR was disturbed" >&2
    od -An -c "$test_dir/cpr" >&2
    exit 1
fi

# Teardown with the hint still set must exit cleanly.
touch "$test_dir/step-4"
if ! wait "$terminal_pid"
then
    terminal_pid=
    echo "xterm+ failed while exiting with urgency set" >&2
    sed -n '1,200p' "$log" >&2
    exit 1
fi
terminal_pid=
expect_count "notification body bytes=" 4 "notification count"

echo "OSC 9/777 notifications set WM urgency while unfocused, focus clears it, other hints survive"
