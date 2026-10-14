#!/bin/sh
set -eu
xvfb=$1
pointer_test=$2
. "$3"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
unset XCURSOR_THEME XCURSOR_SIZE
HOME="$test_dir" XENVIRONMENT=/dev/null "$pointer_test"

# No themed files can resolve here: exercise the core-font fallback path too.
XCURSOR_PATH="$test_dir/no-cursor-themes" XCURSOR_THEME=missing "$pointer_test"

# A private theme distinguishes core selection, -xrm handling and environment precedence.
mkdir "$test_dir/themes"
export XCURSOR_PATH="$test_dir/themes" XTP_CURSOR_FIXTURE="$test_dir/themes"
export HOME="$test_dir" XENVIRONMENT=/dev/null
policy_test() {
    "$pointer_test" -xrm 'XTerm*pointerColor: blue' \
        -xrm 'XTerm*pointerColorBackground: yellow' "$@"
}
XTP_EXPECT_CURSOR=core policy_test -xrm 'Xcursor.theme:'
XTP_EXPECT_CURSOR=core policy_test -xrm 'Xcursor.size: 44'
XCURSOR_THEME='' XTP_EXPECT_CURSOR=core policy_test -xrm 'Xcursor.theme:'
XTP_EXPECT_CURSOR=24 policy_test -xrm 'Xcursor.theme: test-theme' -xrm 'Xcursor.size: 24'
XTP_EXPECT_CURSOR=44 policy_test -xrm 'Xcursor.theme: test-theme' -xrm 'Xcursor.size: 44'
XCURSOR_THEME=test-theme XTP_EXPECT_CURSOR=24 policy_test \
    -xrm 'Xcursor.theme:' -xrm 'Xcursor.size: 24'
XCURSOR_SIZE=44 XTP_EXPECT_CURSOR=44 policy_test \
    -xrm 'Xcursor.theme: test-theme' -xrm 'Xcursor.size: 24'
XCURSOR_THEME='' XTP_EXPECT_CURSOR=core policy_test \
    -xrm 'Xcursor.theme: test-theme' -xrm 'Xcursor.size: 24'
XCURSOR_THEME=missing XTP_EXPECT_CURSOR=core policy_test \
    -xrm 'Xcursor.theme: test-theme'
