package main

import (
	"fmt"
	"os"
)

const holdMarker = "PROBE-HOLD-END"

// sessionHold is the terminal's direct child: it writes its output, ending without a newline, and exits.
func sessionHold(o Options) int {
	fmt.Fprintf(os.Stdout, "session-hold: child pid %d exits with status %d\n", os.Getpid(), o.Status)
	fmt.Fprintln(os.Stdout, "With hold, this text stays until the window is closed.")
	fmt.Fprintf(os.Stdout, "%s status=%d", holdMarker, o.Status)
	return o.Status
}
func sessionHoldGuide(s *Session) {
	exe, err := os.Executable()
	if err != nil {
		exe = "probe"
	}
	s.page("Hold the window after the child exits")
	s.say("This case must be the terminal's own child, so a shell here cannot run it. Launch:")
	s.say("")
	s.say("  TERMINAL -hold -e %s startup-hold-after-exit session-hold --status 3", exe)
	s.say("")
	s.say("It prints a final %s line without a newline and exits with the chosen status.", holdMarker)
	s.say("Without -hold the window closes; with it, the text stays until the window is closed.")
	s.pause()
}
