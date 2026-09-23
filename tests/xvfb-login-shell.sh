#!/bin/sh
# loginShell: the shell's executable and argv, observed from /proc, against XTerm(411)'s.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
sender=$3
reader=$4
closer=$5

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
runs=0
# Rows are read through PRIMARY, and libXt keeps that selection context until exit.
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS

fail()
{
    echo "$label: $*" >&2
    test -f "$work/log" && grep -E 'startup:|pty:|shell: realized' "$work/log" | tail -20 >&2
    exit 1
}

# start LABEL SHELL-VALUE ARGS...: an empty SHELL-VALUE leaves SHELL unset.
start()
{
    label=$1
    shell=$2
    shift 2
    runs=$((runs + 1))
    work=$test_dir/run$runs
    mkdir "$work"
    if test -n "$shell"; then
        set -- env SHELL="$shell" "$terminal" -debug +sb -fn fixed "$@"
    else
        set -- env -u SHELL "$terminal" -debug +sb -fn fixed "$@"
    fi
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        XTP_LOGIN_RUN="$work" "$@" >"$work/out" 2>"$work/log" &
    terminal_pid=$!
    xtp_wait_for_log "$work/log" 'pty: spawned pid=' 'the child'
    child=$(sed -n 's/.*pty: spawned pid=\([0-9]*\).*/\1/p' "$work/log")
    sleep 0.3
}

stop()
{
    kill "$terminal_pid" 2>/dev/null || true
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
    no_survivors
}

survivors()
{
    grep -l -s -a -F "XTP_LOGIN_RUN=$work" /proc/[0-9]*/environ | wc -l | tr -d ' ' || true
}

no_survivors()
{
    attempt=0
    while test "$(survivors)" != 0; do
        attempt=$((attempt + 1))
        test "$attempt" -lt 60 || fail 'processes left behind'
        sleep 0.05
    done
}

# expect EXECUTABLE ARG...: the running child's /proc view.
expect()
{
    want_exe=$(readlink -f "$1")
    shift
    want_argv=$(printf '%s|' "$@")
    got_exe=$(readlink "/proc/$child/exe") || fail 'the child is not running'
    got_argv=$(tr '\0' '|' <"/proc/$child/cmdline")
    test "$got_exe" = "$want_exe" || fail "executable $got_exe, not $want_exe"
    test "$got_argv" = "$want_argv" || fail "argv $got_argv, not $want_argv"
    tr '\0' '\n' <"/proc/$child/environ" | grep -qx 'TERM=xterm-256color' || fail 'TERM changed'
    echo "$label: $got_argv"
}

start default /bin/sh
expect /bin/sh sh
stop
start -ls /bin/sh -ls
expect /bin/sh -sh
stop
start +ls /bin/sh +ls
expect /bin/sh sh
stop
start 'loginShell resource' /bin/sh -xrm 'XTerm*loginShell: true'
expect /bin/sh -sh
stop
start 'vt100 loginShell resource' /bin/sh -xrm 'XTerm.vt100.loginShell: true'
expect /bin/sh -sh
stop
# loginShell belongs to the vt100 widget, so an application-level setting is ignored.
start 'application-level loginShell' /bin/sh -xrm 'XTerm.loginShell: true'
expect /bin/sh sh
stop
start '+ls after the resource' /bin/sh -xrm 'XTerm*loginShell: true' +ls
expect /bin/sh sh
stop
start '+ls before the resource' /bin/sh +ls -xrm 'XTerm*loginShell: true'
expect /bin/sh sh
stop
start '-ls with -e' /bin/sh -ls -e /bin/sh -c 'sleep 30; :' probe-arg
expect /bin/sh /bin/sh -c 'sleep 30; :' probe-arg
stop

# Without a usable $SHELL, xterm takes the password-file shell if /etc/shells lists it.
fallback=$(getent passwd "$(id -un)" | cut -d: -f7)
if ! test -x "$fallback" || ! grep -qx "$fallback" /etc/shells 2>/dev/null; then
    fallback=/bin/sh
fi
fallback_name=${fallback##*/}
for shell in '' relative/sh /nonexistent/sh; do
    start "SHELL='$shell' -ls" "$shell" -ls
    expect "$fallback" "-$fallback_name"
    stop
done

# A shell that cannot run reports itself on the terminal for five seconds, then exits 30.
printf '#!/nonexistent/interpreter\n' >"$test_dir/broken-shell"
chmod +x "$test_dir/broken-shell"
label='failed shell'
started=$(date +%s)
start "$label" "$test_dir/broken-shell" -ls
status=0
wait "$terminal_pid" || status=$?
terminal_pid=
elapsed=$(($(date +%s) - started))
test "$status" = 0 || fail "terminal exited with $status"
test "$elapsed" -ge 4 && test "$elapsed" -le 9 || fail "exited after ${elapsed}s, not about 5"
no_survivors
echo "$label: exited 0 after about ${elapsed}s, nothing left"

label='failed shell, held'
start "$label" "$test_dir/broken-shell" -ls -hold
xtp_wait_for_log "$work/log" 'pty: held child exited status=30' 'the failed shell exit'
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$work/log")
"$sender" "$window" 5 8 474 8 >/dev/null
sleep 0.2
case $("$reader" PRIMARY) in
*'Could not exec '*) ;;
*) fail "row 0 is '$("$reader" PRIMARY)'" ;;
esac
"$closer" "$window"
wait "$terminal_pid" || fail 'closing did not exit 0'
terminal_pid=
no_survivors
echo "$label: message shown, status 30 reaped, closed cleanly"

label='closed during the failure delay'
start "$label" "$test_dir/broken-shell"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$work/log")
"$closer" "$window"
wait "$terminal_pid" || fail 'closing did not exit 0'
terminal_pid=
no_survivors
echo "$label: exited 0, nothing left"
