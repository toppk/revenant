#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
python=$3
helper=$4
timeout_program=$5
preload=${6:-}
env_program=$(command -v env)

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
# libXt leaks its display-name copy when XtOpenDisplay fails, even after
# XtDestroyApplicationContext (reproduced without any terminal code). Suppress
# only that library stack and only for the deliberately invalid-display launch.
printf 'leak:XtOpenDisplay\n' >"$test_dir/no-display-lsan"

fail()
{
    echo "mask $mask: $*" >&2
    for file in launch child status log fault
    do
        test -s "$work/$file" && { echo "--- $file" >&2; sed -n '1,80p' "$work/$file" >&2; }
    done
    exit 1
}

# Every process started for a run carries its work directory in the environment.
survivors()
{
    grep -l -s -a -F "XTP_STDIO_RUN=$work" /proc/[0-9]*/environ | cut -d/ -f3 | tr '\n' ' ' || true
}

expect_no_survivors()
{
    attempt=0
    while test -n "$(survivors)"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 40 || fail "$1 left processes behind: $(survivors)"
        sleep 0.05
    done
}

# Runs the terminal with the standard descriptors in MASK closed by the launcher;
# the rest are the files in: out and log. A set FAULT preloads the /dev/null fault
# into the terminal alone, logging to fault.
run()
{
    : >"$work/in"
    rm -f "$work/fault"
    if test -n "${fault:-}"
    then
        set -- "$env_program" LD_PRELOAD="$preload" XTP_NULL_FAULT="$fault" \
            XTP_NULL_FAULT_PROGRAM="$(basename "$terminal")" XTP_NULL_FAULT_LOG="$work/fault" \
            "$terminal" "$@"
    else
        set -- "$terminal" "$@"
    fi
    status=0
    LSAN_OPTIONS=${run_lsan_options-${LSAN_OPTIONS:-}} \
        DISPLAY=${run_display:-$DISPLAY} HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null \
        XFILESEARCHPATH=/dev/null XTP_STDIO_RUN="$work" "$timeout_program" 30 \
        "$python" "$helper" launch "$mask" "$work" "$@" \
        <"$work/in" >"$work/out" 2>"$work/log" || status=$?
}

described()
{
    if test $((mask & $1)) -ne 0
    then
        echo /dev/null
    else
        echo "$work/$2"
    fi
}

check_child_run()
{
    rm -f "$work/child" "$work/status"
    # -debug and a missing font make the terminal write to stderr before and after
    # the PTY exists.
    run -debug -fn no-such-font-xyz -geometry 80x24 -e /bin/sh -c \
        '"$0" "$1" child "$2"; echo "$?" >"$2/status"' "$python" "$helper" "$work"
    test "$status" -eq 0 || fail "terminal exited with status $status"
    test ! -s "$work/launch" || fail "launcher failed"
    test "$(cat "$work/status" 2>/dev/null)" = 3 || fail "child exit status was not 3"
    grep -v '^pids=' "$work/child" >"$work/actual" || fail "no child report"
    cat >"$work/expected" <<EOF
stdio-tty=yes,yes,yes
stdio-same-terminal=yes
controlling-terminal=stdio
session-leader=parent
foreground=yes
inherited-fds=none
terminal-holds-master=yes
terminal-stdio=$(described 1 in),$(described 2 out),$(described 4 log)
stdout-status=1b5b306e
stderr-cursor=1b5b323b3452
EOF
    diff -u "$work/expected" "$work/actual" >&2 || fail "child report differs"
    if test $((mask & 4)) -eq 0
    then
        grep -q 'Cannot convert string "no-such-font-xyz"' "$work/log" ||
            fail "the font warning did not reach stderr"
        grep -q 'shutdown complete status=0' "$work/log" || fail "no clean shutdown"
    fi
    expect_no_survivors "the run"
    child_pids=$(sed -n 's/^pids=//p' "$work/child")
}

for mask in 0 1 2 3 4 5 6 7
do
    work="$test_dir/m$mask"
    mkdir "$work"

    check_child_run

    run -e /nonexistent/xtp-command
    test "$status" -eq 0 || fail "exec failure: terminal exited with status $status"
    expect_no_survivors "exec failure"

    run_display=127.0.0.1:65000
    run_lsan_options="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/no-display-lsan"
    run -e /bin/true
    unset run_lsan_options
    run_display=
    test "$status" -eq 1 || fail "no display: terminal exited with status $status, not 1"
    expect_no_survivors "display failure"

    printf 'mask %d closed=%-5s child exit=3 report exact, terminal stdio=%s, children %s gone\n' \
        "$mask" "$(for fd in 0 1 2; do test $((mask & (1 << fd))) -ne 0 && printf '%s' "$fd"; done)" \
        "$(sed -n 's/^terminal-stdio=//p' "$work/actual" | sed "s|$work/||g")" "$child_pids"
done
echo "all eight closed-stdio combinations start, round-trip the PTY and tear down"

if test -z "$preload"
then
    echo "fault paths not run: the preload cannot be combined with a sanitizer"
    exit 0
fi

# The first /dev/null open fails: startup must stop before the X connection,
# any socket or the PTY, and say why when stderr is open.
fault=fail
for mask in 1 2 3 4 5 6 7
do
    work="$test_dir/fail$mask"
    mkdir "$work"
    run -e /bin/true
    test "$status" -eq 1 || fail "failed reservation: terminal exited with status $status, not 1"
    test "$(cat "$work/fault")" = "open /dev/null -> EACCES" ||
        fail "failed reservation went on to: $(tr '\n' ' ' <"$work/fault")"
    if test $((mask & 4)) -eq 0
    then
        grep -q 'cannot open /dev/null on a closed standard descriptor: Permission denied' \
            "$work/log" || fail "failed reservation was not reported"
    fi
    expect_no_survivors "failed reservation"
    printf 'mask %d reservation fails: exit 1, calls: %s\n' "$mask" "$(tr '\n' ' ' <"$work/fault")"
done

# With nothing closed, nothing is reserved, and the fault is never reached first.
mask=0
work="$test_dir/fail0"
mkdir "$work"
check_child_run
test "$(sed -n 1p "$work/fault")" != "open /dev/null -> EACCES" ||
    fail "reserved /dev/null with every standard descriptor open"
echo "mask 0 reservation not attempted: child report exact"

# An interrupted open is retried, and a descriptor opened at the wrong number is
# moved to the one it replaces; either way the run is the ordinary one.
for fault in eintr elsewhere
do
    for mask in 4 7
    do
        work="$test_dir/$fault$mask"
        mkdir "$work"
        check_child_run
        first=$(sed -n 1p "$work/fault")
        case $fault in
        eintr) test "$first" = "open /dev/null -> EINTR" &&
            test "$(sed -n 2p "$work/fault")" = "open /dev/null -> $((mask == 4 ? 2 : 0))" ;;
        elsewhere) test "$first" = "open /dev/null -> 20" ;;
        esac || fail "$fault: reservation calls were: $(tr '\n' ' ' <"$work/fault")"
        # The shim sees the calls a failed reservation must never reach.
        for call in XOpenDisplay socket forkpty
        do
            grep -qx "$call" "$work/fault" || fail "$fault: the preload did not see $call"
        done
        printf 'mask %d %s: child report exact, terminal stdio=%s\n' "$mask" "$fault" \
            "$(sed -n 's/^terminal-stdio=//p' "$work/actual" | sed "s|$work/||g")"
    done
done
echo "descriptor reservation fails closed, retries EINTR and places each replacement"
