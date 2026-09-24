package main

import (
	"fmt"
	"regexp"
	"strconv"
)

// These cases demonstrate terminal behavior, not the availability of a library API.
func mouseModeOrder(s *Session) {
	s.say("Use a disposable terminal: requested mode bits can be restored, but their")
	s.say("original effective precedence cannot be queried or restored from DECRQM alone.")
	s.say("Compare actual reports with the requested mode bits. This does not test a C getter.")
	modes := []int{9, 1000, 1002, 1003, 1004, 1005, 1006, 1015, 1016}
	s.preserveModes(modes...)
	for _, stage := range []struct{ label, requests string }{
		{"1000 only", modeSequence(1000, true)},
		{"1003 only", modeSequence(1003, true)},
		{"1002 only", modeSequence(1002, true)},
		{"1000 then 1003", modeSequence(1000, true) + modeSequence(1003, true)},
		{"1003 then 1000", modeSequence(1003, true) + modeSequence(1000, true)},
		{"1003 then 1000 then reset 1000", modeSequence(1003, true) + modeSequence(1000, true) + modeSequence(1000, false)},
	} {
		for _, m := range modes {
			s.send(modeSequence(m, false))
		}
		s.send(modeSequence(1006, true) + stage.requests)
		s.say("Stage: %s (SGR encoding requested)", stage.label)
		for _, m := range []int{1000, 1002, 1003} {
			s.mode(m)
		}
		mouseStep(s, "Motion without a button", "move across several cells without pressing a mouse button, then press Space.", "observe reports; 1000 alone normally has none, 1003 reports motion. Unknown/denied modes may be silent.")
		mouseStep(s, "Button drag", "press and drag across several cells, release, then press Space.", "1000 reports button events; 1002/1003 also report motion. Compare counts with your actions; no automatic verdict.")
	}
	s.say("Why the API matters: a host needs the encoder's effective mode for permissions;")
	s.say("the mode bits above and a precedence guess cannot establish that state.")
}

func windowObserve(s *Session) {
	s.say("Queries only. No terminal menus or permission changes are required.")
	s.say("Observe application geometry before and after resizing this terminal manually.")
	s.say("A reply demonstrates reporting, not a host permission callback or mode 2048.")
	for _, stage := range []string{"Before resize", "After resize"} {
		values := map[int][2]int{}
		for _, r := range []struct {
			code  int
			units string
		}{{14, "pixels height;width"}, {16, "cell pixels height;width"}, {18, "rows;columns"}} {
			reply := regexp.MustCompile(fmt.Sprintf("\\x1b\\[%d;([0-9]+);([0-9]+)t", r.code-10))
			found := statusQuery(s, fmt.Sprintf("%s CSI %d t (%s)", stage, r.code, r.units), fmt.Sprintf("%s[%dt", esc, r.code), "", reply)
			if m := reply.FindStringSubmatch(found); m != nil {
				a, _ := strconv.Atoi(m[1])
				b, _ := strconv.Atoi(m[2])
				values[r.code] = [2]int{a, b}
			}
		}
		compareWindowReports(s, values)
		if stage == "Before resize" {
			s.say("Resize the window now, then continue. The new window size persists.")
			s.pause()
		}
	}
	s.say("Why the API matters: the host must distinguish solicited report kinds to")
	s.say("allow or deny each without suppressing unsolicited resize notifications.")
	s.pause()
}

func shellEvents(s *Session) {
	s.say("Disposable terminal recommended: synthetic prompt marks remain in scrollback.")
	s.say("No displayed command is executed. Use your terminal's prompt navigation;")
	s.say("in Ghostty the configurable actions are jump_to_prompt:-1 and jump_to_prompt:1.")
	s.say("No terminal is required to visibly display an exit status. These are event inputs,")
	s.say("not evidence of a C callback. JSON records sent bytes, not parser acceptance.")
	active := false
	s.cleanup(func() {
		if active {
			s.osc(133, "C")
			s.osc(133, "D")
		}
	})
	for i, ending := range []string{"D;0", "D;7", "D"} {
		active = true
		s.osc(133, "A")
		s.send(fmt.Sprintf("event-%d$ ", i+1))
		s.osc(133, "B")
		s.say("synthetic command %d", i+1)
		s.osc(133, "C")
		s.say("EVENT-%d-BEGIN", i+1)
		for j := 1; j <= 10; j++ {
			s.say("event %d output line %d", i+1, j)
		}
		s.say("EVENT-%d-END", i+1)
		s.osc(133, ending)
		active = false
	}
	active = true
	s.osc(133, "A")
	s.send("event-ready$ ")
	s.osc(133, "B")
	s.say("[inspect markers]")
	s.say("Navigate between event-1$, event-2$, event-3$ and event-ready$ using your bindings.")
	s.say("Completion inputs were success (0), failure (7), and unspecified status, respectively.")
	s.say("Why the API matters: a host can identify the completed command and its output")
	s.say("at the accepted event, even if another prompt arrives in the same read.")
	s.pause()
}
