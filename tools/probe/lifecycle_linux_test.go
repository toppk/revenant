//go:build linux

package main

import (
	"context"
	"os"
	"slices"
	"strings"
	"testing"
	"time"
)

func testSession(t *testing.T) (*Session, *os.File) {
	t.Helper()
	in, writer, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	out, err := os.CreateTemp(t.TempDir(), "output")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { in.Close(); writer.Close(); out.Close() })
	return &Session{in: in, out: out, ctx: context.Background(), opts: Options{Timeout: .02, NoPause: true}}, writer
}

func TestUnexpectedPanicRunsAllCaseCleanups(t *testing.T) {
	for _, cleanupPanics := range []bool{false, true} {
		s, _ := testSession(t)
		var order []int
		original := new(int)
		c := Case{ID: "panic-fixture", Features: []string{"diagnostics-unknown-apc"}, Run: func(s *Session) {
			s.cleanup(func() { order = append(order, 1) })
			s.cleanup(func() {
				order = append(order, 2)
				if cleanupPanics {
					panic("cleanup panic")
				}
			})
			s.cleanup(func() { order = append(order, 3) })
			panic(original)
		}}
		func() {
			defer func() {
				if got := recover(); got != original {
					t.Errorf("panic = %v, want original", got)
				}
			}()
			runCase(s, c, nil)
		}()
		if !slices.Equal(order, []int{3, 2, 1}) || s.result != nil || s.cleaning || len(s.cleanups) != 0 {
			t.Fatalf("cleanup incomplete: %v", order)
		}
	}
}

func TestPollExpiredWaitIsNonblocking(t *testing.T) {
	s, _ := testSession(t)
	ready, err := readReady(s.in, -time.Second)
	if ready || err != nil {
		t.Fatalf("expired poll: %v %v", ready, err)
	}
}

func TestModeAndPositionReports(t *testing.T) {
	for state := 0; state <= 4; state++ {
		s, w := testSession(t)
		w.WriteString(esc + "[?2027;" + string(rune('0'+state)) + "$y")
		if got := s.mode(2027); got != state {
			t.Fatalf("mode = %d, want %d", got, state)
		}
	}
	for _, wire := range []string{esc + "[24;80R", esc + "[?24;80R"} {
		s, w := testSession(t)
		w.WriteString(wire)
		row, col, ok := position(s)
		if !ok || row != 24 || col != 80 {
			t.Fatalf("position = %d %d %v", row, col, ok)
		}
	}
	for _, wire := range []string{esc + "[0;1R", esc + "[999999999999999999999;1R"} {
		s, w := testSession(t)
		w.WriteString(wire)
		if _, _, ok := position(s); ok {
			t.Fatalf("accepted invalid CPR %q", wire)
		}
	}
}

func TestBrowserRestoresModeMatrix(t *testing.T) {
	for state := 0; state <= 4; state++ {
		s, _ := testSession(t)
		b := &Browser{s: s, original: map[int]int{}}
		for _, mode := range browserModes {
			b.original[mode] = state
		}
		b.enter()
		start, _ := s.out.Seek(0, 1)
		b.leave()
		data, err := os.ReadFile(s.out.Name())
		if err != nil {
			t.Fatal(err)
		}
		leave := string(data[start:])
		for _, mode := range browserModes {
			if mode == 1049 {
				if strings.Contains(leave, modeSequence(mode, false)) != (state != 1 && state != 3) {
					t.Fatalf("alternate-screen restore state %d", state)
				}
				continue
			}
			on := state == 1 || state == 3 || state == 0 && mode == 25
			if strings.Contains(leave, modeSequence(mode, true)) != on {
				t.Fatalf("mode %d state %d", mode, state)
			}
		}
	}
}

func TestBrowserWaitsForSlowEscapePrefix(t *testing.T) {
	s, w := testSession(t)
	s.opts.Timeout = .5
	b := &Browser{s: s}
	// A preceding key may already have been consumed from the same read.
	w.WriteString("x" + esc)
	if e := b.nextEvent(); e.Kind != "text" || e.Text != "x" {
		t.Fatalf("preceding key lost: %+v", e)
	}
	for i := 0; i < 2; i++ {
		if e := b.nextEvent(); e.Kind == "back" {
			t.Fatal("reply prefix treated as Escape")
		}
	}
	w.WriteString("]lquiet title" + st)
	if e := b.nextEvent(); e.Kind == "back" {
		t.Fatal("reply treated as navigation")
	}
	w.WriteString("q")
	if e := b.nextEvent(); e.Kind != "text" || e.Text != "q" {
		t.Fatalf("navigation did not resume: %+v", e)
	}
}

func TestColorSchemeReports(t *testing.T) {
	for _, item := range []struct{ wire, want string }{
		{esc + "[?997;1n", "dark\r\n"},
		{esc + "[?997;2n", "light\r\n"},
		{esc + "[?997;3n", "no complete matching reply"},
	} {
		s, w := testSession(t)
		w.WriteString(item.wire)
		scheme(s)
		data, err := os.ReadFile(s.out.Name())
		if err != nil || !strings.Contains(string(data), item.want) {
			t.Fatalf("scheme %q: %q %v", item.wire, data, err)
		}
	}
}

func TestEmojiSectionIdentitySurvivesReordering(t *testing.T) {
	sections := widthSections(emojiSamples)
	want := selectEmojiSections(sections, "flags")[0]
	slices.Reverse(sections)
	got := selectEmojiSections(sections, "flags")[0]
	if got.ID != "flags" || got.Title != want.Title || got.Samples[0].Text != want.Samples[0].Text {
		t.Fatal("reordering changed flags")
	}
	mode := selectEmojiSections(sections, "mode-2027")[0]
	if mode.Samples[1].Text != want.Samples[0].Text {
		t.Fatal("reordering changed mode comparison")
	}
}
