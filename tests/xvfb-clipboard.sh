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
key_sender=$8
encoding_child=$9

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

# Encoding and reply boundaries, by exact bytes and declared type. An external owner
# serves chosen bytes under a chosen type, so a requestor is graded on the encoding
# the owner declared, not on how the bytes look; an external reader reports the type
# and bytes the terminal serves as an owner. An OSC 52 round trip alone could not show
# either. Expected values match xterm-411, measured with the same helpers.
work=$test_dir/encoding
mkdir "$work"
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug -fn fixed -geometry 80x24 -xrm 'xterm.vt100.allowWindowOps: true' \
    -e "$python" "$encoding_child" "$work" >"$test_dir/encoding.out" 2>"$test_dir/encoding.log" &
terminal_pid=$!
log=$test_dir/encoding.log
xtp_wait_for_log "$log" 'shell: realized window=' "encoding terminal"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
step=0

wait_file()
{
    attempt=0
    while ! test -e "$1"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 300 || fail "encoding: timed out waiting for $1"
        sleep 0.03
    done
}

# The child treats go.N's existence as readiness, so the command is written aside
# and renamed into place: it can never see an empty or partial command.
publish_step()
{
    step=$((step + 1))
    printf '%s' "$1" >"$work/go.$step.tmp"
    mv "$work/go.$step.tmp" "$work/go.$step"
}

step_child()
{
    publish_step "$1"
    wait_file "$work/done.$step"
}

start_owner()
{
    selection=$1
    shift
    : >"$work/owner.out"
    "$owner" --serve "$selection" "$@" >"$work/owner.out" 2>&1 &
    aux_pid=$!
    attempt=0
    until grep -q ready "$work/owner.out"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 200 || fail "encoding: external owner did not start"
        sleep 0.02
    done
}

stop_owner()
{
    kill "$aux_pid" 2>/dev/null || true
    wait "$aux_pid" 2>/dev/null || true
    aux_pid=
}

# The decoded OSC 52 payload of the last step: hex, or "length sha256" when large.
payload()
{
    "$python" - "$work/res.$step" "${1:-hex}" <<'PAYLOAD'
import base64, hashlib, re, sys
data = open(sys.argv[1], "rb").read()
match = re.fullmatch(rb"\x1b\]52;c;([A-Za-z0-9+/=]*)(?:\x07|\x1b\\)", data)
if match is None:
    print("unframed:" + data[:64].hex())
    sys.exit()
body = base64.b64decode(match.group(1), validate=True)
print(body.hex() if sys.argv[2] == "hex" else f"{len(body)} {hashlib.sha256(body).hexdigest()}")
PAYLOAD
}

# OSC 52 read of CLIPBOARD from an owner serving TARGET=TYPE:HEX... .
expect_read()
{
    label=$1
    expected=$2
    shift 2
    start_owner CLIPBOARD "$@"
    step_child query
    actual=$(payload)
    stop_owner
    printf 'read  %-42s payload=%s\n' "$label" "$actual"
    test "$actual" = "$expected" || fail "encoding: read $label gave $actual, expected $expected"
}

# Paste of PRIMARY from such an owner: the exact bytes the application receives.
expect_paste()
{
    label=$1
    expected=$2
    shift 2
    start_owner PRIMARY "$@"
    publish_step paste
    wait_file "$work/listening.$step"
    "$key_sender" "$window" keysym Insert shift >/dev/null
    wait_file "$work/done.$step"
    actual=$(od -An -tx1 -v "$work/res.$step" | tr -d ' \n')
    stop_owner
    printf 'paste %-42s pty=%s\n' "$label" "$actual"
    test "$actual" = "$expected" || fail "encoding: paste $label gave $actual, expected $expected"
}

# The terminal owns CLIPBOARD with UTF-8 TEXT (hex); an external reader asks for TARGET.
expect_owned()
{
    text=$1
    target=$2
    expected=$3
    actual=$("$reader" --describe CLIPBOARD "$target" 2>/dev/null || echo refused)
    printf 'owner %-12s %-11s %s\n' "$text" "$target" "$actual"
    test "$actual" = "$expected" || fail "encoding: owner $text $target gave $actual, expected $expected"
}

own_text()
{
    step_child "set:$("$python" -c 'import base64, sys; print(base64.b64encode(bytes.fromhex(sys.argv[1])).decode())' "$1")"
    xtp_wait_for_log "$log" "publish source=CLIPBOARD selection=CLIPBOARD bytes=$(( ${#1} / 2 )) owned=true" "own $1"
}

# Owner first, while this terminal's timestamps are the newest the server has seen:
# taking a selection with an older time than its last change is refused by ICCCM.
# Owner: STRING is Latin-1; a character outside it becomes "?", exactly as xterm-411's
# STRING conversion does; UTF8_STRING and TEXT carry the UTF-8 text.
own_text 636166c3a9
expect_owned cafe-acute UTF8_STRING "type=UTF8_STRING bytes=636166c3a9"
expect_owned cafe-acute STRING "type=STRING bytes=636166e9"
expect_owned cafe-acute TEXT "type=UTF8_STRING bytes=636166c3a9"
own_text c383c2a9
expect_owned A-tilde-copy STRING "type=STRING bytes=c3a9"
own_text 61e29c9362
expect_owned a-check-b STRING "type=STRING bytes=613f62"
expect_owned a-check-b UTF8_STRING "type=UTF8_STRING bytes=61e29c9362"

# Requestor: the declared encoding decides, never the look of the bytes.
expect_read "STRING Latin-1 caf\\xe9" 636166c3a9 STRING=STRING:636166e9
expect_read "STRING c3 a9 is Latin-1, not UTF-8" c383c2a9 STRING=STRING:c3a9
expect_read "UTF8_STRING cafe-acute" 636166c3a9 UTF8_STRING=UTF8_STRING:636166c3a9
expect_read "UTF8_STRING answered as STRING" c3a9 UTF8_STRING=STRING:e9
expect_read "UTF8_STRING answered as COMPOUND_TEXT" c3a9 UTF8_STRING=COMPOUND_TEXT:1b2d41e9
expect_read "unreadable type falls back to STRING" c3a9 \
    UTF8_STRING=XTP_UNREADABLE_TYPE:41 STRING=STRING:e9
expect_read "unreadable type and no STRING" "" UTF8_STRING=XTP_UNREADABLE_TYPE:41
# Malformed UTF8_STRING is carried as sent, as xterm-411 does; the next request recovers.
expect_read "UTF8_STRING malformed" 41fffe42 UTF8_STRING=UTF8_STRING:41fffe42
expect_read "recovery after malformed" 6f6b UTF8_STRING=UTF8_STRING:6f6b
# Boundaries: an INCR transfer is refused with the empty unavailable reply, and large
# direct replies arrive whole and framed, never cut short.
expect_read "INCR announced" "" UTF8_STRING=INCR:41414141
expect_read "recovery after INCR" 6f6b UTF8_STRING=UTF8_STRING:6f6b
"$python" -c 'import sys; sys.stdout.buffer.write(b"a" * (1 << 20))' >"$work/1m"
"$python" -c 'import sys; sys.stdout.buffer.write(b"b" * (8 << 20))' >"$work/8m"
for size in 1m 8m
do
    start_owner CLIPBOARD "UTF8_STRING=UTF8_STRING:@$work/$size"
    step_child query
    actual=$(payload digest)
    stop_owner
    expected="$(wc -c <"$work/$size" | tr -d ' ') $(sha256sum "$work/$size" | cut -d' ' -f1)"
    printf 'read  %-42s payload=%s\n' "large $size" "$actual"
    test "$actual" = "$expected" || fail "encoding: large $size reply was not complete: $actual"
done
expect_read "recovery after large" 6f6b UTF8_STRING=UTF8_STRING:6f6b

# Backpressure: the reader stops reading while an 8 MiB reply is due. The whole reply
# is queued at once (the terminal logs it), the bytes waiting on the PTY stay far
# below it and do not grow while the reader is stalled, so the terminal is holding the
# rest; when reading resumes the reply arrives whole, and the next request works.
start_owner CLIPBOARD "UTF8_STRING=UTF8_STRING:@$work/8m"
step_child query-paused:1500
actual=$(payload digest)
stop_owner
read -r waiting_first waiting_last <"$work/paused.$step"
reply_length=$((7 + (8388608 + 2) / 3 * 4 + 1))
printf 'read  %-42s waiting=%s->%s of %s payload=%s\n' "paused reader, 8m" \
    "$waiting_first" "$waiting_last" "$reply_length" "$actual"
grep -q "pty: write queued=$reply_length " "$log" ||
    fail "encoding: the paused reply was not queued whole ($reply_length bytes)"
test "$waiting_last" -gt 0 && test "$waiting_last" -eq "$waiting_first" &&
    test "$waiting_last" -lt "$reply_length" ||
    fail "encoding: the reader was not stalled with output pending ($waiting_first -> $waiting_last)"
test "$actual" = "8388608 $(sha256sum "$work/8m" | cut -d' ' -f1)" ||
    fail "encoding: the reply after the stall was not complete: $actual"
expect_read "recovery after the stall" 6f6b UTF8_STRING=UTF8_STRING:6f6b

# The same declared-type rules on the paste path.
expect_paste "STRING Latin-1 caf\\xe9" 636166c3a9 STRING=STRING:636166e9
expect_paste "STRING c3 a9 is Latin-1, not UTF-8" c383c2a9 STRING=STRING:c3a9
expect_paste "UTF8_STRING answered as COMPOUND_TEXT" c3a9 UTF8_STRING=COMPOUND_TEXT:1b2d41e9
expect_paste "unreadable type falls back to STRING" c3a9 \
    UTF8_STRING=XTP_UNREADABLE_TYPE:41 STRING=STRING:e9
expect_paste "UTF8_STRING malformed" 41fffe42 UTF8_STRING=UTF8_STRING:41fffe42

step_child quit
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=

echo "OSC 52 policy, ownership, independence, query replies, encodings and reply boundaries verified"
