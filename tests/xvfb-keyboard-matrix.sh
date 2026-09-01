#!/bin/sh

set -eu

if test "$#" -ne 3
then
    echo "usage: $0 XVFB REVENANT SEND-KEY" >&2
    exit 2
fi

xvfb=$1
terminal=$2
sender=$3
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
XMODIFIERS=@im=none
XTP_KEY_CAPTURE=$test_dir/key.capture
XTP_KEY_READY_NORMAL=$test_dir/key.ready.normal
XTP_KEY_READY_APPLICATION=$test_dir/key.ready.application
XTP_KEY_READY_XIM=$test_dir/key.ready.xim
XTP_KEY_NORMAL=$test_dir/key.normal
XTP_KEY_APPLICATION=$test_dir/key.application
XTP_KEY_XIM=$test_dir/key.xim
export DISPLAY XMODIFIERS XTP_KEY_CAPTURE XTP_KEY_READY_NORMAL \
    XTP_KEY_READY_APPLICATION XTP_KEY_READY_XIM XTP_KEY_NORMAL XTP_KEY_APPLICATION XTP_KEY_XIM

printf '\033[A\033[1;2A\033[1;5A\033[1;3A\033[1;9A' >"$XTP_KEY_NORMAL"
printf '\033OP\033[15~\033[H\033[3~1\r\303\244' >>"$XTP_KEY_NORMAL"
printf '\033OA\033Oq\033OM' >"$XTP_KEY_APPLICATION"
printf '\303\251' >"$XTP_KEY_XIM"
XTP_KEY_NORMAL_COUNT=$(wc -c <"$XTP_KEY_NORMAL")
XTP_KEY_APPLICATION_COUNT=$(wc -c <"$XTP_KEY_APPLICATION")
XTP_KEY_XIM_COUNT=$(wc -c <"$XTP_KEY_XIM")
export XTP_KEY_NORMAL_COUNT XTP_KEY_APPLICATION_COUNT XTP_KEY_XIM_COUNT

log=$test_dir/keyboard-matrix.log
"$terminal" -debug +sb -e sh -c '
    stty raw -echo
    : >"$XTP_KEY_CAPTURE"
    printf "\033[?1l\033>"
    : >"$XTP_KEY_READY_NORMAL"
    dd if=/dev/tty bs=1 count="$XTP_KEY_NORMAL_COUNT" 2>/dev/null >>"$XTP_KEY_CAPTURE"
    printf "\033[?1035l\033[?1h\033="
    : >"$XTP_KEY_READY_APPLICATION"
    dd if=/dev/tty bs=1 count="$XTP_KEY_APPLICATION_COUNT" 2>/dev/null >>"$XTP_KEY_CAPTURE"
    : >"$XTP_KEY_READY_XIM"
    dd if=/dev/tty bs=1 count="$XTP_KEY_XIM_COUNT" 2>/dev/null >>"$XTP_KEY_CAPTURE"
    sleep 20
' >"$test_dir/keyboard-matrix.out" 2>"$log" &
terminal_pid=$!

wait_for_ready()
{
    ready=$1
    description=$2
    attempt=0
    while ! test -f "$ready"
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "revenant did not become ready for $description" >&2
            sed -n '1,360p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

wait_for_ready "$XTP_KEY_READY_NORMAL" normal-keyboard-matrix
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
for key in up shift-up ctrl-up alt-up super-up f1 f5 home delete kp-1 kp-enter adiaeresis
do
    "$sender" "$window" "$key" >/dev/null
done

wait_for_ready "$XTP_KEY_READY_APPLICATION" application-keyboard-matrix
for key in up app-kp-1 kp-enter
do
    "$sender" "$window" "$key" >/dev/null
done
wait_for_ready "$XTP_KEY_READY_XIM" xim-compose
"$sender" "$window" compose-e-acute >/dev/null

expected=$test_dir/key.expected
cat "$XTP_KEY_NORMAL" "$XTP_KEY_APPLICATION" "$XTP_KEY_XIM" >"$expected"
expected_count=$(wc -c <"$expected")
attempt=0
while test "$(wc -c <"$XTP_KEY_CAPTURE" 2>/dev/null || true)" -lt "$expected_count"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
    then
        echo "revenant did not encode the complete keyboard matrix" >&2
        od -An -tx1 "$XTP_KEY_CAPTURE" >&2
        sed -n '1,420p' "$log" >&2
        exit 1
    fi
    sleep 0.05
done

if ! cmp -s "$expected" "$XTP_KEY_CAPTURE" || \
   ! grep -F -q -- 'input-method=open input-context=created' "$log"
then
    echo "ordinary/application keyboard or non-US UTF-8 bytes did not match" >&2
    echo "expected:" >&2
    od -An -tx1 "$expected" >&2
    echo "actual:" >&2
    od -An -tx1 "$XTP_KEY_CAPTURE" >&2
    sed -n '1,460p' "$log" >&2
    exit 1
fi
