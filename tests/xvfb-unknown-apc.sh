#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# Unknown APCs whole, fragmented, with embedded C0 bytes, and oversized sit
# between a Kitty graphics query and a status query; only those two may reply.
big=$(printf 'z%.0s' $(seq 1 1000))
script=$test_dir/child.sh
cat >"$script" <<EOF
stty raw -echo
printf 'ab\\033_unknown-probe-dispatch\\033\\\\cd'
printf '\\033_frag'
sleep 0.2
printf ';men'
sleep 0.2
printf 't\\033'
sleep 0.2
printf '\\\\'
printf '\\033_c0\\a\\r\\001z\\033\\\\'
printf '\\033_%s\\033\\\\' '$big'
printf '\\033_Ga=q,i=31,f=24,s=1,v=1;AAAA\\033\\\\\\033[5n'
IFS= read -r -t 2 -N 256 reply || true
printf '%s' "\$reply" >"$test_dir/reply"
EOF

log=$test_dir/log
if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug -e bash "$script" >"$test_dir/out" 2>"$log"
then
    echo "xterm+ failed during the unknown APC case" >&2
    sed -n '1,120p' "$log" >&2
    exit 1
fi

printf '\033_Gi=31;OK\033\\\033[0n' >"$test_dir/expected-reply"
if ! cmp -s "$test_dir/expected-reply" "$test_dir/reply"
then
    echo "unknown APCs changed the PTY replies" >&2
    od -An -c "$test_dir/reply" >&2
    sed -n '1,120p' "$log" >&2
    exit 1
fi

grep -F -- 'unknown APC ignored' "$log" | sed 's/^.*unknown APC ignored //' >"$test_dir/events" || true
preview_big=$(printf 'z%.0s' $(seq 1 256))
cat >"$test_dir/expected" <<EOF
truncated=no bytes=22 preview="unknown-probe-dispatch"
truncated=no bytes=9 preview="frag;ment"
truncated=no bytes=6 preview="c0\\a\\r\\x01z"
truncated=yes bytes=256 preview="$preview_big"
EOF
if ! diff -u "$test_dir/expected" "$test_dir/events" >&2
then
    echo "unknown APC diagnostics differ from the expected sequence" >&2
    sed -n '1,120p' "$log" >&2
    exit 1
fi

echo "unknown APCs are logged with bounded payloads and never answered"
