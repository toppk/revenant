#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# The child writes two ENQ bytes, then copies whatever arrives within two
# seconds to REPLY-FILE; exact bytes are compared, so the reply is read raw.
reader='stty raw -echo; printf "\005\005"; IFS= read -r -t 2 -N 32 reply || true; printf "%s" "$reply" >"$1"'

run_case()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    reply=$test_dir/$case_name.reply
    if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug "$@" -e bash -c "$reader" bash "$reply" \
        >"$test_dir/$case_name.out" 2>"$log"
    then
        echo "xterm+ failed during the $case_name answerback case" >&2
        sed -n '1,120p' "$log" >&2
        exit 1
    fi
}

run_case configured -xrm 'XTerm*answerbackString: xterm+ ok'
printf 'xterm+ okxterm+ ok' >"$test_dir/expected"
if ! cmp -s "$test_dir/expected" "$test_dir/configured.reply"
then
    echo "configured answerback replied unexpectedly" >&2
    od -An -c "$test_dir/configured.reply" >&2
    sed -n '1,120p' "$test_dir/configured.log" >&2
    exit 1
fi
test "$(grep -c 'ENQ answered bytes=9' "$test_dir/configured.log")" -eq 2 ||
    { echo "configured answerback did not log both replies" >&2; exit 1; }

run_case default
if test -s "$test_dir/default.reply"
then
    echo "default answerback was not silent" >&2
    od -An -c "$test_dir/default.reply" >&2
    exit 1
fi
grep -q 'answerbackString bytes=0' "$test_dir/default.log" ||
    { echo "default answerback was not applied" >&2; exit 1; }

echo "answerbackString replies exactly and the empty default stays silent"
