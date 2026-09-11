package main

import (
	"strings"
	"testing"
)

func TestInspectionNavigationUsesWholeKeys(t *testing.T) {
	for _, item := range []struct{ wire, want string }{
		{" ", "continue"}, {"q", "exit"}, {"\r", "continue"}, {"\n", "continue"}, {esc, "exit"},
		{esc + "[32;1u", "continue"}, {esc + "[113;1u", "exit"}, {esc + "[27;1u", "exit"}, {esc + "[13;1u", "continue"},
		{esc + "[113;1:3u", ""}, {esc + "[113;3u", ""}, {esc + "[ q", ""},
	} {
		if got := navigationAction([]byte(item.wire)); got != item.want {
			t.Fatalf("%q: %q", item.wire, got)
		}
	}
	for _, wire := range []string{esc + "]lquiet title" + st, esc + "P1+r7171=20" + st, "\x9dquiet\x9c", esc + "]lœ space q" + st, esc + "]l" + strings.Repeat("x", 5000) + "q" + st, "\x9b q"} {
		s := &Session{}
		for _, c := range []byte(wire) {
			if s.navigation([]byte{c}) {
				t.Fatalf("reply advanced inspection: %q", wire)
			}
		}
		if len(s.navPending) != 0 {
			t.Fatalf("incomplete reply: %q", s.navPending)
		}
		if !s.navigation([]byte(" ")) {
			t.Fatal("navigation did not resume")
		}
	}
}
