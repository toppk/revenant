#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$1"
"$4" "$script_dir/cursor-modes.py" "$2" "$3" "$test_dir/modes" "meson-logs/cursor-modes.json"
