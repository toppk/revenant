#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
python=$3
driver=$4
reader=$5
owner=$6
sender=$7

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"
# libXt keeps one selection context per atom for the display's lifetime, even
# after XtDisownSelection; these cases exit normally, so LeakSanitizer sees it.
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS

start_case()
{
    phase=$1
    shift
    log=$test_dir/$phase.log
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -fn fixed -geometry 80x24 "$@" -e "$python" "$driver" "$phase" \
        >"$test_dir/$phase.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" osc52-start "$phase startup"
}

finish_case()
{
    xtp_wait_for_title "$log" osc52-done "$phase completion"
    if ! wait "$terminal_pid"
    then
        terminal_pid=
        echo "xterm+ failed during the $phase clipboard case" >&2
        sed -n '1,300p' "$log" >&2
        exit 1
    fi
    terminal_pid=
}

fail()
{
    echo "$1" >&2
    grep -n -E 'title changed|selection:' "$log" >&2
    exit 1
}

# Read SELECTION from outside the terminal; empty output when nobody serves it.
read_selection()
{
    "$reader" "$1" 2>/dev/null || true
}

expect_selection()
{
    actual=$(read_selection "$1")
    test "$actual" = "$2" || fail "$phase: $1 holds '$actual', expected '$2'"
}

run_denied_case()
{
    start_case denied "$@"
    xtp_wait_for_title "$log" osc52-set-sent "denied set"
    grep -q 'OSC 52 SetSelection denied target=CLIPBOARD bytes=11' "$log" ||
        fail "denied: set was not refused by policy"
    expect_selection CLIPBOARD ""
    grep -q 'OSC 52 GetSelection denied; queries stay unanswered' "$log" ||
        fail "denied: read effect was installed despite the policy"
    finish_case
    grep -q 'preview="osc52-query-silent"' "$log" || fail "denied: query was answered"
}

run_setonly_case()
{
    start_case setonly "$@"
    xtp_wait_for_title "$log" osc52-set-c "set-only set"
    expect_selection CLIPBOARD set-allowed
    finish_case
    grep -q 'preview="osc52-query-silent"' "$log" || fail "setonly: query was answered"
}

# Default policy: xterm's disallowedWindowOps refuses both directions, and so
# do wildcard rules, including a negated pattern that re-allows one name.
run_denied_case
run_denied_case -xrm 'XTerm*disallowedWindowOps: *'
run_denied_case -xrm 'XTerm*disallowedWindowOps: *Selection'
run_setonly_case -xrm 'XTerm*disallowedWindowOps: GetIconTitle,GetWinTitle,GetSelection'
run_setonly_case -xrm 'XTerm*disallowedWindowOps: *,~SetSelection'
run_setonly_case -xrm 'XTerm*disallowedWindowOps: Get*'

# allowWindowOps=true: sets own the named selections, queries are answered exactly.
start_case allowed -xrm 'XTerm*allowWindowOps: true'
xtp_wait_for_title "$log" osc52-set-c "set c"
grep -q 'publish source=CLIPBOARD selection=CLIPBOARD bytes=5 owned=true' "$log" ||
    fail "allowed: c did not own CLIPBOARD"
expect_selection CLIPBOARD hello
test "$("$reader" CLIPBOARD STRING)" = hello || fail "allowed: STRING target not served"
xtp_wait_for_title "$log" osc52-set-p "set p"
expect_selection PRIMARY primary-text
xtp_wait_for_title "$log" osc52-set-s "set s"
grep -q 'publish source=SELECT selection=PRIMARY bytes=11 owned=true' "$log" ||
    fail "allowed: s did not resolve to PRIMARY under selectToClipboard=false"
expect_selection PRIMARY select-text
expect_selection CLIPBOARD hello
xtp_wait_for_title "$log" osc52-clear-c "clear c"
grep -q 'clear source=CLIPBOARD owned-before=true' "$log" || fail "allowed: clear did not disown"
expect_selection CLIPBOARD ""
expect_selection PRIMARY select-text
xtp_wait_for_title "$log" osc52-need-owner "external owner request"
grep -q 'preview="osc52-empty-ok"' "$log" || fail "allowed: empty query reply was wrong"
grep -q 'preview="osc52-self-ok"' "$log" || fail "allowed: self-owned query reply was wrong"
grep -q 'OSC 52 GetSelection target=CLIPBOARD result=unavailable bytes=0' "$log" ||
    fail "allowed: unowned query was not reported unavailable"
"$owner" CLIPBOARD from-x >"$test_dir/owner.out" &
aux_pid=$!
attempt=0
while ! grep -q ready "$test_dir/owner.out"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || fail "allowed: external owner did not start"
    sleep 0.05
done
xtp_wait_for_title "$log" osc52-invalid-sent "invalid base64"
grep -q 'preview="osc52-external-ok"' "$log" || fail "allowed: external owner query reply was wrong"
grep -q 'preview="osc52-st-ok"' "$log" || fail "allowed: ST-terminated query reply was wrong"
grep -q 'OSC 52 GetSelection target=CLIPBOARD result=success bytes=6' "$log" ||
    fail "allowed: external read was not logged as success"
expect_selection CLIPBOARD from-x
xtp_wait_for_title "$log" osc52-replaced "replacement set"
expect_selection CLIPBOARD replaced
grep -q lost "$test_dir/owner.out" || fail "allowed: external owner kept CLIPBOARD"
# A mouse selection into PRIMARY must leave the OSC 52 CLIPBOARD alone, and an
# OSC 52 CLIPBOARD write must leave the mouse PRIMARY alone.
xtp_wait_for_title "$log" osc52-mouse-window "mouse selection window"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
cell=$(sed -n 's/.*config: VT100 resolved renderer=.* cell=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p' "$log" | tail -1)
cell_width=${cell%x*}
cell_height=${cell#*x}
"$sender" "$window" $((2 + cell_width / 2)) $((2 + cell_height / 2)) \
    $((2 + 5 * cell_width + cell_width / 2)) $((2 + cell_height / 2)) >/dev/null
xtp_wait_for_log "$log" 'publish source=SELECT selection=PRIMARY bytes=5 owned=true' "mouse PRIMARY"
expect_selection PRIMARY alpha
expect_selection CLIPBOARD replaced
xtp_wait_for_title "$log" osc52-set-after-mouse "set after mouse"
expect_selection CLIPBOARD after-mouse
expect_selection PRIMARY alpha
sed -n '/preview="osc52-mouse-window"/,$p' "$log" | grep -q 'selection: lost' &&
    fail "allowed: an owned selection was lost during independence checks"
finish_case
kill "$aux_pid" 2>/dev/null || true
wait "$aux_pid" 2>/dev/null || true
aux_pid=

# maxStringParse bounds the encoded payload like xterm's control-string limit.
start_case limit -xrm 'XTerm*allowWindowOps: true' -xrm 'XTerm*maxStringParse: 100'
xtp_wait_for_title "$log" osc52-large-sent "large set"
grep -q 'OSC 52 SetSelection target=CLIPBOARD bytes=80 exceeds maxStringParse=100' "$log" ||
    fail "limit: oversized payload was not refused"
expect_selection CLIPBOARD ""
xtp_wait_for_title "$log" osc52-small-sent "small set"
expect_selection CLIPBOARD small
finish_case

# A reply that arrives after the read timeout must not answer a later query,
# neither for another selection nor for a repeat of the same one.
start_case late -xrm 'XTerm*allowWindowOps: true'
"$owner" CLIPBOARD CLIPBOARD-OLD 1500 >"$test_dir/slow.out" &
aux_pid=$!
"$owner" PRIMARY PRIMARY-X >"$test_dir/fast.out" &
fast_pid=$!
xtp_wait_for_title "$log" osc52-late-owners-ok "late-reply owners"
finish_case
kill "$aux_pid" "$fast_pid" 2>/dev/null || true
wait "$aux_pid" "$fast_pid" 2>/dev/null || true
aux_pid=
grep -q 'preview="osc52-late-first-ok"' "$log" || fail "late: slow owner did not time out to an empty reply"
grep -q 'preview="osc52-late-primary-ok"' "$log" || fail "late: PRIMARY query was answered with stale data"
grep -q 'preview="osc52-late-second-ok"' "$log" || fail "late: repeated CLIPBOARD query reused a stale reply"
test "$(grep -c 'OSC 52 read timed out' "$log")" -eq 2 || fail "late: expected two read timeouts"
grep -q 'result=success bytes=13' "$log" && fail "late: CLIPBOARD-OLD reached a reply"

echo "OSC 52 policy, ownership, independence, and query replies verified"
