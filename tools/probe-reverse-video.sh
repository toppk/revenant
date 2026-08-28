#!/bin/sh
# Human-run probe for SGR 7, DECSCNM, and widget-level reverse video.

set -eu

pause_enabled=true
case ${1-} in
--no-pause)
    pause_enabled=false
    ;;
"")
    ;;
*)
    echo "usage: $0 [--no-pause]" >&2
    exit 2
    ;;
esac

cleanup()
{
    # Restore terminal-controlled reverse video and all SGR attributes. The
    # widget reverseVideo setting is deliberately controlled by the user.
    printf '\033[?5l\033[0m'
}

continue_phase()
{
    if "$pause_enabled"
    then
        printf '\n\033[2m[Press Enter to continue]\033[0m '
        IFS= read -r reply || :
    fi
}

trap cleanup 0
trap 'exit 130' HUP INT TERM

printf '\033[?5l\033[0m\033[2J\033[H'
printf '\033[1mReverse-video and inverse rendering probe\033[0m\n\n'
printf 'This screen separates three independent mechanisms:\n'
printf '  1. SGR 7 styles one cell range.\n'
printf '  2. DECSCNM reverses the terminal screen.\n'
printf '  3. reverseVideo swaps the widget defaults.\n\n'
printf 'Plain defaults:      foreground text on the screen background\n'
printf '\033[7mSGR 7 inverse:       default colors exchanged for this row\033[0m\n'
printf '\033[48;2;255;128;0mExplicit background: orange must remain orange through DECSCNM\033[0m\n'
printf '\nNext, DECSCNM will be enabled without redrawing these rows.\n'
printf 'Expect plain cells and the untouched marker to reverse, SGR 7 to\n'
printf 'cancel against DECSCNM, and the explicit orange to remain orange.\n'
printf '\033[18;36Huntouched marker'
printf '\033[14;1H'
continue_phase

printf '\033[?5h'
printf '\nDECSCNM is enabled. Inspect the original rows and the marker at row 18.\n'
printf 'The SGR 7 and DECSCNM inversions should have normal net polarity.\n'
printf 'Disabling DECSCNM next must restore every original cell.\n'
continue_phase

printf '\033[?5l'
printf '\nDECSCNM is disabled. The baseline, SGR 7 row, orange row, and marker\n'
printf 'should all match their first-phase appearance again.\n'
continue_phase

printf '\nWidget reverseVideo is separate from DECSCNM and SGR 7.\n'
printf 'Open VT Options with Ctrl+Button 2 and enable Reverse Video now.\n'
printf 'The configured foreground/background should swap across this whole screen.\n'
continue_phase

printf '\nWith widget reverse video enabled, the effective screen background should\n'
printf 'remain the translucent surface when backgroundOpacity is below 1.0.\n'
printf 'Use VT Options to disable Reverse Video, then finish the probe.\n'
continue_phase

cleanup
trap - 0
printf '\n\nProbe complete. DECSCNM and SGR state have been restored.\n'
