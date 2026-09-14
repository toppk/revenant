#!/bin/sh

set -eu

if test "$#" -ne 5
then
    echo "usage: $0 XVFB XTERM_PLUS URGENCY-TOOL FAKE-NOTIFICATIONS compiled|absent" >&2
    exit 2
fi

xvfb=$1
terminal=$2
urgency=$3
fake=$4
mode=$5
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home" "$test_dir/steps"
steps=$test_dir/steps
records=$test_dir/records
log=$test_dir/log

# The child sends each request and waits for the test to inspect it before the next step.
cat >"$test_dir/child.sh" <<'CHILD'
dir=$1
mode=$2
stty raw -echo
step()
{
    : >"$dir/ready-$1"
    while ! test -e "$dir/go-$1"
    do
        sleep 0.05
    done
}
osc9()
{
    printf '\033]9;%s\033\\' "$1"
}
osc777()
{
    printf '\033]777;notify;%s;%s\033\\' "$1" "$2"
}
step start
if test "$mode" = absent
then
    osc9 'one'
    osc777 'Two' 'two'
    osc9 'three'
    step sent
    exit 0
fi
osc9 'Grüße ✓ <b>data</b>'
step plain
osc777 'Titel ✓' 'Körper & <i>x</i>'
step titled
printf '\033]777;notify;;\033\\'
step empty
osc9 'focused request'
step focused
printf '\033]9;a\001b\377c\033\\'
step controls
osc777 "$(printf '%0300d' 0 | tr 0 x)" "$(printf '%01500d' 0 | tr 0 y)"
step long
i=0
while test "$i" -lt 12
do
    osc9 "burst $i"
    i=$((i + 1))
done
step burst
osc9 'after window'
step after-window
osc9 'rejected'
step rejected
osc9 'during backoff'
step backoff
osc9 'recovered'
step recovered
osc9 'missing'
step missing
osc9 'missing again'
step missing-again
step pre-queue
i=0
while test "$i" -lt 4
do
    osc9 "queued $i"
    i=$((i + 1))
done
step queued
i=0
while test "$i" -lt 5
do
    osc9 "stalled $i"
    i=$((i + 1))
done
step stalled
i=0
while test "$i" -lt 5
do
    osc9 "late $i"
    sleep 0.05
    i=$((i + 1))
done
step late
CHILD

fail()
{
    echo "$1" >&2
    shift
    printf '%s\n' "$@" >&2
    grep -E 'notify:|notification received|urgency|title changed|progress:' "$log" 2>/dev/null | tail -40 >&2
    test -f "$records" && sed -n '1,40p' "$records" >&2
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
        test "$attempt" -lt 200 || fail "did not log '$1' $2 times"
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

expect_urgent()
{
    "$urgency" query "$window" | grep -q '^urgent=1 ' || fail "$1: WM urgency is not set" "$("$urgency" query "$window")"
}

report()
{
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -report-config 2>/dev/null
}

start_terminal()
{
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -T startup -e bash "$test_dir/child.sh" "$steps" "$mode" \
        >"$test_dir/out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_log "$log" 'shell: realized window=' 'terminal window'
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    "$urgency" focus "$window" out
    sleep 0.3
}

finish_terminal()
{
    status=0
    wait "$terminal_pid" || status=$?
    terminal_pid=
    test "$status" = 0 || fail "the terminal exited with status $status"
    test "$(count_of 'shell: title changed')" = 0 || fail "a notification changed the window title"
    # Exit always clears progress; nothing may have shown it.
    if grep -E -q 'progress: state=(set|error|pause|indeterminate)' "$log"
    then
        fail "a notification changed the progress indicator"
    fi
}

if test "$mode" = absent
then
    report | grep -q '^! desktop notifications: compiled without libnotify$' ||
        fail "-report-config does not say libnotify is absent" "$(report | grep -i notif)"
    start_terminal
    ready start
    go start
    ready sent
    xtp_wait_for_log "$log" 'notification received count=3 ' 'three notifications'
    expect_urgent "notifications without libnotify"
    test "$(count_of 'desktop notifications are not compiled in')" = 1 ||
        fail "unavailable delivery was not reported exactly once"
    go sent
    finish_terminal
    echo "without libnotify, notifications log and set urgency, and delivery reports unavailable once"
    exit 0
fi

hex()
{
    # -v keeps od from folding repeated bytes into '*'.
    printf '%s' "$1" | od -An -v -tx1 | tr -d ' \n'
}

record_count()
{
    if test -f "$records"
    then
        wc -l <"$records" | tr -d ' '
    else
        echo 0
    fi
}

wait_records()
{
    attempt=0
    while test "$(record_count)" -lt "$1"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || fail "the fake service received $(record_count) of $1 notifications"
        sleep 0.05
    done
}

wait_record()
{
    attempt=0
    while ! grep -q "body=$(hex "$1") .*result=$2" "$records" 2>/dev/null
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || fail "the service did not receive '$1' ($2)"
        sleep 0.05
    done
}

record()
{
    sed -n "$1p" "$records"
}

field()
{
    printf '%s\n' "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"
}

start_daemon()
{
    rm -f "$test_dir/daemon-ready"
    "$fake" "$records" "$test_dir/daemon-ready" "$@" >"$test_dir/daemon.out" 2>&1 &
    aux_pid=$!
    attempt=0
    while ! test -e "$test_dir/daemon-ready"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || fail "the fake notification service did not start" "$(cat "$test_dir/daemon.out")"
        sleep 0.05
    done
}

stop_daemon()
{
    if test -n "$aux_pid"
    then
        kill "$aux_pid" 2>/dev/null || true
        wait "$aux_pid" 2>/dev/null || true
        aux_pid=
    fi
}

# A daemon that owns the name but never answers cannot stall -report-config.
start_daemon --hold-all
started=$(date +%s%N)
output=$(report)
elapsed=$((($(date +%s%N) - started) / 1000000))
printf '%s\n' "$output" | grep -q '^! notification daemon: no reply within 1000 ms$' ||
    fail "-report-config did not give up on a silent daemon" "$output"
test "$elapsed" -lt 5000 || fail "-report-config waited $elapsed ms for a silent daemon"
stop_daemon

start_daemon
report | grep -q '^! desktop notifications: compiled with libnotify$' ||
    fail "-report-config does not say libnotify is compiled in" "$(report | grep -i notif)"
report | grep -q '^! notification daemon: xtp-fake 1 (specification 1.2)$' ||
    fail "-report-config does not name the running daemon" "$(report | grep -i notif)"
start_terminal
ready start
go start

# An OSC 9 request has no title: the application label is the summary and its icon is used.
ready plain
wait_records 1
line=$(record 1)
app=$(field "$line" app)
test -n "$app" || fail "the notification carried no application name" "$line"
test "$(field "$line" image)" = "$app" || fail "the notification did not use the application icon" "$line"
test "$(field "$line" summary)" = "$(hex "$app")" || fail "an untitled request did not use the application label" "$line"
test "$(field "$line" body)" = "$(hex 'Grüße ✓ &lt;b&gt;data&lt;/b&gt;')" ||
    fail "the OSC 9 body was not delivered exactly with its markup escaped" "$line"
test "$(field "$line" actions)" = 0 || fail "the notification offered actions" "$line"
go plain

# OSC 777 carries a title; the body's markup is escaped for a body-markup server.
ready titled
wait_records 2
line=$(record 2)
test "$(field "$line" summary)" = "$(hex 'Titel ✓')" || fail "the OSC 777 title was not delivered exactly" "$line"
test "$(field "$line" body)" = "$(hex 'Körper &amp; &lt;i&gt;x&lt;/i&gt;')" ||
    fail "the OSC 777 body was not delivered exactly with its markup escaped" "$line"
go titled

# Empty title and body still make a valid notification.
ready empty
wait_records 3
line=$(record 3)
test "$(field "$line" summary)" = "$(hex "$app")" || fail "an empty title did not use the application label" "$line"
test "$(field "$line" body)" = - || fail "an empty body was not delivered empty" "$line"
"$urgency" focus "$window" in
xtp_wait_for_log "$log" 'cursor focus=in' 'focus in'
go empty

# A focused request is logged but never reaches the service.
ready focused
xtp_wait_for_log "$log" 'desktop notification suppressed reason=focused' 'focused request'
sleep 0.3
test "$(record_count)" = 3 || fail "a focused request reached the notification service"
"$urgency" focus "$window" out
xtp_wait_for_log "$log" 'cursor focus=out' 'focus out'
go focused

# Control and invalid bytes arrive as valid UTF-8 with no control characters.
ready controls
wait_records 4
line=$(record 4)
python3 -c '
import sys
data = bytes.fromhex(sys.argv[1])
data.decode("utf-8")
sys.exit(1 if any((b < 32 and b not in (9, 10)) or b == 127 for b in data) else 0)
' "$(field "$line" body)" || fail "control or invalid bytes reached the notification service" "$line"
go controls

# Title and body are cut at their byte limits.
ready long
wait_records 5
line=$(record 5)
test "$(field "$line" summary)" = "$(hex "$(printf '%0256d' 0 | tr 0 x)")" ||
    fail "the title was not cut at 256 bytes" "summary bytes: $(($(printf '%s' "$(field "$line" summary)" | wc -c) / 2))"
test "$(field "$line" body)" = "$(hex "$(printf '%01024d' 0 | tr 0 y)")" ||
    fail "the body was not cut at 1024 bytes" "body bytes: $(($(printf '%s' "$(field "$line" body)" | wc -c) / 2))"
xtp_wait_for_log "$log" 'desktop notification truncated title-bytes=300 body-bytes=1500 limits=256/1024' 'truncation'
go long

# A burst is rate limited, reported once, and every request still logs and keeps urgency.
ready burst
xtp_wait_for_log "$log" 'notification received count=18 ' 'burst requests'
xtp_wait_for_log "$log" 'desktop notifications rate limited: at most 5 per 10000 ms' 'rate limit'
sleep 1
burst=$(grep -c "body=$(hex 'burst ')" "$records" || true)
test "$burst" -le 5 || fail "a burst delivered $burst notifications"
test "$(count_of 'desktop notifications rate limited')" = 1 || fail "rate limiting was reported more than once"
expect_urgent "rate-limited notifications"
# After the window a request is delivered again, to a service that now rejects it.
sleep 11
stop_daemon
start_daemon --reject
go burst

ready after-window
attempt=0
while ! grep -q "body=$(hex 'after window') .*result=rejected" "$records"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 200 || fail "the request after the rate window was not attempted"
    sleep 0.05
done
xtp_wait_for_log "$log" 'desktop notification failed:' 'rejected delivery'
go after-window

# During the backoff nothing reaches the service and the failure is not reported again.
ready rejected
xtp_wait_for_log "$log" 'desktop notifications paused for 5000 ms after a failure' 'backoff'
go rejected
ready backoff
sleep 0.3
grep -q "body=$(hex 'rejected')" "$records" && fail "a request during the backoff reached the service"
grep -q "body=$(hex 'during backoff')" "$records" && fail "a request during the backoff reached the service"
test "$(count_of 'desktop notification failed:')" = 1 || fail "the failure was reported more than once"
expect_urgent "notifications during the backoff"
# Once the backoff has passed, a working service receives notifications again.
sleep 5.5
stop_daemon
start_daemon
go backoff

ready recovered
attempt=0
while ! grep -q "body=$(hex 'recovered') .*result=accepted" "$records"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 200 || fail "delivery did not recover when the service returned"
    sleep 0.05
done
xtp_wait_for_log "$log" 'desktop notifications recovered' 'recovery'
stop_daemon
report | grep -q '^! notification daemon: unavailable$' ||
    fail "-report-config does not say the daemon is unavailable" "$(report | grep -i notif)"
go recovered

# With no service at all the failure is reported once and urgency still works.
ready missing
wait_count 'desktop notification failed:' 2
go missing
ready missing-again
wait_count 'desktop notifications paused for 5000 ms after a failure' 2
sleep 0.3
test "$(count_of 'desktop notification failed:')" = 2 || fail "a missing service was reported repeatedly"
expect_urgent "notifications with no service"
go missing-again

# Requests queued behind a call that fails fall under its backoff instead of reaching the service.
sleep 5.5
start_daemon --reject --hold
ready pre-queue
go pre-queue
ready queued
xtp_wait_for_log "$log" 'notification received count=28 ' 'queued requests'
wait_record 'queued 0' rejected
sleep 0.3
kill -USR1 "$aux_pid"
wait_count 'desktop notifications paused for 5000 ms after a failure' 3
sleep 0.5
for i in 1 2 3
do
    if grep -q "body=$(hex "queued $i") " "$records"
    then
        fail "a request queued before the failure reached the service during the backoff"
    fi
done
stop_daemon
sleep 5.5
start_daemon --hold
go queued

# A stalled call cannot bunch calls: queued and new requests are gated when they are sent.
ready stalled
wait_record 'stalled 0' accepted
xtp_wait_for_log "$log" 'notification received count=33 ' 'stalled requests'
sleep 11
kill -USR1 "$aux_pid"
go stalled
ready late
wait_record 'stalled 4' accepted
wait_count 'desktop notifications rate limited' 2
sleep 0.5
python3 -c '
import re, sys
times = [int(m.group(1)) for m in (re.search(r" at=([0-9]+) ", line) for line in open(sys.argv[1])) if m]
worst = max(sum(1 for u in times if t <= u < t + 10000) for t in times)
print(worst)
sys.exit(0 if worst <= 5 else 1)
' "$records" >"$test_dir/worst" || fail "$(cat "$test_dir/worst") calls reached the service within 10 s"
expect_urgent "rate-limited notifications after a stall"
go late
stop_daemon

finish_terminal
grep -q 'notify: desktop notifications stopped' "$log" || fail "delivery did not stop cleanly at exit"
echo "desktop notifications deliver exact escaped text while unfocused, bound, rate limit, back off and recover"
