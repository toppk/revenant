#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$1"
"$3" "$script_dir/user-signals.py" "$2" "$test_dir"
