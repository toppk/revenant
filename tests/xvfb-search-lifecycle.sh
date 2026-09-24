#!/bin/sh

set -eu

if test "$#" -ne 2
then
    echo "usage: $0 XVFB SEARCH-LIFECYCLE" >&2
    exit 2
fi

. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$1"
mkdir "$test_dir/empty-home"

if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    XUSERFILESEARCHPATH=/dev/null \
    "$2" >"$test_dir/stdout" 2>"$test_dir/stderr"
then
    sed -n '1,240p' "$test_dir/stderr" >&2
    exit 1
fi
cat "$test_dir/stdout"
