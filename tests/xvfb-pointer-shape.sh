#!/bin/sh
set -eu
xvfb=$1
pointer_test=$2
. "$3"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
"$pointer_test"

# No themed files can resolve here: exercise the core-font fallback path too.
XCURSOR_PATH="$test_dir/no-cursor-themes" XCURSOR_THEME=missing "$pointer_test"
