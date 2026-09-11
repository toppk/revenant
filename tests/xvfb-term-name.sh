#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# The child records its TERM, then asks XTGETTCAP for TN in raw mode and
# records whatever arrives within two seconds.
reader='printf "%s" "$TERM" >"$1"; stty raw -echo; printf "\033P+q544e\033\\\\"; IFS= read -r -t 2 -N 64 reply || true; printf "%s" "$reply" >"$2"'

run_case()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug "$@" -e bash -c "$reader" bash "$test_dir/$case_name.term" \
        "$test_dir/$case_name.tn" >"$test_dir/$case_name.out" 2>"$log"
    then
        echo "xterm+ failed during the $case_name terminal-name case" >&2
        sed -n '1,120p' "$log" >&2
        exit 1
    fi
}

# libghostty encodes XTGETTCAP keys and values in uppercase hex.
hex()
{
    printf '%s' "$1" | od -An -tx1 | tr -d ' \n' | tr a-f A-F
}

expect_agreement()
{
    case_name=$1
    name=$2
    term=$(cat "$test_dir/$case_name.term")
    test "$term" = "$name" || { echo "$case_name: child TERM is '$term', expected '$name'" >&2; exit 1; }
    printf '\033P1+r544E=%s\033\\' "$(hex "$name")" >"$test_dir/$case_name.expected"
    if ! cmp -s "$test_dir/$case_name.expected" "$test_dir/$case_name.tn"
    then
        echo "$case_name: TN reply did not match TERM=$name" >&2
        od -An -c "$test_dir/$case_name.tn" >&2
        sed -n '1,120p' "$test_dir/$case_name.log" >&2
        exit 1
    fi
}

run_case default
expect_agreement default xterm-256color

run_case option -tn xtp-test
expect_agreement option xtp-test

run_case resource -xrm 'XTerm*termName: xterm-direct'
expect_agreement resource xterm-direct

# Tcap Ops off: TERM still follows the name while the TN reply stays silent.
run_case denied -tn xtp-quiet -xrm 'XTerm*allowTcapOps: false'
test "$(cat "$test_dir/denied.term")" = xtp-quiet ||
    { echo "denied: child TERM did not follow -tn" >&2; exit 1; }
if test -s "$test_dir/denied.tn"
then
    echo "denied: TN was answered despite Tcap Ops being off" >&2
    od -An -c "$test_dir/denied.tn" >&2
    exit 1
fi
grep -q 'XTGETTCAP reply denied by Tcap Ops' "$test_dir/denied.log" ||
    { echo "denied: the TN denial was not logged" >&2; exit 1; }

echo "termName sets the child's TERM and the XTGETTCAP TN reply together"
