#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$1"
# As in xvfb-clipboard: libXt retains a per-atom selection context after
# XtDisownSelection. Keep normal-exit checks enabled for all other leaks.
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS
"$3" "$script_dir/request-ops.py" "$test_dir" "$2" "$4" "$5"
