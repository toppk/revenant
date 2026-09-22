package main

import (
	"bytes"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"
)

type Key struct {
	Code, Modifiers, Event int
	Shifted, Base          int
	Text, Name             string
}

func parseKey(raw []byte) (Key, bool) {
	k := Key{Event: 1, Shifted: -1, Base: -1}
	if len(raw) < 3 || string(raw[:2]) != esc+"[" {
		return k, false
	}
	final := raw[len(raw)-1]
	params := strings.Split(string(raw[2:len(raw)-1]), ";")
	num := func(text string, def int) (int, bool) {
		if text == "" {
			return def, true
		}
		n, e := strconv.Atoi(text)
		return n, e == nil && n >= 0 && n <= 0x10ffff
	}
	keyparts := strings.Split(params[0], ":")
	code, ok := num(keyparts[0], 0)
	if !ok {
		return k, false
	}
	k.Code = code
	if len(params) > 1 {
		mods := strings.Split(params[1], ":")
		m, ok := num(mods[0], 1)
		if !ok || m < 1 {
			return k, false
		}
		k.Modifiers = m - 1
		if len(mods) > 1 {
			k.Event, ok = num(mods[1], 1)
			if !ok || k.Event < 1 || k.Event > 3 {
				return k, false
			}
		}
	}
	switch final {
	case 'u':
		if len(keyparts) > 3 || len(params) > 3 {
			return k, false
		}
		if len(keyparts) > 1 {
			k.Shifted, ok = num(keyparts[1], -1)
			if !ok {
				return k, false
			}
		}
		if len(keyparts) > 2 {
			k.Base, ok = num(keyparts[2], -1)
			if !ok {
				return k, false
			}
		}
		if len(params) > 2 && params[2] != "" {
			for _, v := range strings.Split(params[2], ":") {
				n, ok := num(v, 0)
				if !ok || !utf8.ValidRune(rune(n)) {
					return k, false
				}
				k.Text += string(rune(n))
			}
		}
	case '~':
		if code == 27 && len(params) == 3 {
			k.Code, ok = num(params[2], 0)
			if !ok {
				return k, false
			}
			k.Name = "modifyOtherKeys"
		} else {
			k.Code = -1
			k.Name = map[int]string{2: "Insert", 3: "Delete", 5: "PageUp", 6: "PageDown", 15: "F5", 17: "F6", 18: "F7", 19: "F8", 20: "F9", 21: "F10", 23: "F11", 24: "F12"}[code]
			if k.Name == "" {
				k.Name = fmt.Sprintf("tilde key %d", code)
			}
		}
	default:
		k.Name = map[byte]string{'A': "Up", 'B': "Down", 'C': "Right", 'D': "Left", 'H': "Home", 'F': "End", 'P': "F1", 'Q': "F2", 'S': "F4", 'E': "KP_Begin"}[final]
		if k.Name == "" {
			return k, false
		}
		k.Code = -1
	}
	return k, true
}
func (k Key) String() string {
	mods := []string{}
	for i, name := range []string{"Shift", "Alt", "Ctrl", "Super", "Hyper", "Meta", "CapsLock", "NumLock"} {
		if k.Modifiers&(1<<i) != 0 {
			mods = append(mods, name)
		}
	}
	return fmt.Sprintf("event=%d key=%d name=%q modifiers=%s shifted=%d base=%d text=%q", k.Event, k.Code, k.Name, strings.Join(mods, "+"), k.Shifted, k.Base, k.Text)
}

// Split complete CSI/SS3/UTF-8 events, keeping a partial suffix across reads.
func splitKeys(pending []byte) ([][]byte, []byte) {
	var events [][]byte
	for len(pending) > 0 {
		n := 1
		stringStart := 0
		oscString := false
		if len(pending) >= 2 && pending[0] == 27 && bytes.ContainsAny(pending[1:2], "]P_^X") {
			stringStart = 2
			oscString = pending[1] == ']'
		}
		if pending[0] == 0x9d || pending[0] == 0x90 || pending[0] == 0x9f || pending[0] == 0x9e || pending[0] == 0x98 {
			stringStart = 1
			oscString = pending[0] == 0x9d
		}
		if stringStart > 0 {
			end := -1
			for i := stringStart; i < len(pending); i++ {
				if oscString && pending[i] == 7 || pending[i] == 0x9c && (i == 0 || !utf8.Valid(pending[stringStart:i+1])) {
					end = i + 1
					break
				}
				if pending[i] == 27 && i+1 < len(pending) && pending[i+1] == '\\' {
					end = i + 2
					break
				}
			}
			if end < 0 {
				return events, pending
			}
			events = append(events, append([]byte(nil), pending[:end]...))
			pending = pending[end:]
			continue
		}
		if pending[0] == 0x9b || pending[0] == 0x8f {
			n = 1
			for n < len(pending) && (pending[n] < 0x40 || pending[n] > 0x7e) {
				n++
			}
			if n == len(pending) {
				return events, pending
			}
			n++
			events = append(events, append([]byte(nil), pending[:n]...))
			pending = pending[n:]
			continue
		}
		if pending[0] == 27 {
			if len(pending) < 2 {
				return events, pending
			}
			if pending[1] == '[' || pending[1] == 'O' {
				n = 2
				for n < len(pending) && (pending[n] < 0x40 || pending[n] > 0x7e) {
					n++
				}
				if n == len(pending) {
					return events, pending
				}
				n++
			} else {
				_, size := utf8.DecodeRune(pending[1:])
				if !utf8.FullRune(pending[1:]) {
					return events, pending
				}
				n = 1 + size
			}
		} else if pending[0] >= 128 {
			if !utf8.FullRune(pending) {
				return events, pending
			}
			_, n = utf8.DecodeRune(pending)
		}
		events = append(events, append([]byte(nil), pending[:n]...))
		pending = pending[n:]
	}
	return events, pending
}
func mouse(s *Session) {
	s.preserveModes(9, 1000, 1002, 1003, 1004, 1005, 1006, 1015, 1016)
	for _, m := range []int{9, 1000, 1002, 1003, 1004, 1005, 1006, 1015, 1016} {
		s.send(modeSequence(m, false))
	}
	mode, _ := strconv.Atoi(s.opts.Mode)
	switch s.opts.Scenario {
	case "counts":
		mouseCounts(s, mode)
	case "handoff":
		mouseHandoff(s, mode)
	case "all":
		mouseCounts(s, mode)
		mouseHandoff(s, mode)
		mouseCapture(s, mode)
	default:
		mouseCapture(s, mode)
	}
}
func mouseCapture(s *Session, mode int) {
	for _, m := range []int{mode, 1004, 1006} {
		s.send(modeSequence(m, true))
	}
	s.say("Move, click, scroll and focus; toggle Allow Mouse Ops.")
	s.say(captureKeys)
	deadline := time.Now().Add(seconds(s.opts.Seconds))
	for time.Now().Before(deadline) {
		data := s.read(minTime(deadline, time.Now().Add(100*time.Millisecond)))
		s.navigation(data)
		if len(data) > 0 {
			s.event("input", data)
			s.say("Mouse/focus input: %q", data)
		}
	}
}

var sgrMouseReport = regexp.MustCompile("\x1b\\[<([0-9]+);[0-9]+;[0-9]+([Mm])")

type mouseTally struct{ up, down, releases, other, keyUp, keyDown int }

func tallyMouse(data []byte) mouseTally {
	var t mouseTally
	for _, m := range sgrMouseReport.FindAllSubmatch(data, -1) {
		code, _ := strconv.Atoi(string(m[1]))
		button := code &^ (4 | 8 | 16 | 32)
		switch {
		case (button == 64 || button == 65) && string(m[2]) == "m":
			t.releases++
		case button == 64:
			t.up++
		case button == 65:
			t.down++
		default:
			t.other++
		}
	}
	t.keyUp = bytes.Count(data, []byte(esc+"[A")) + bytes.Count(data, []byte(esc+"OA"))
	t.keyDown = bytes.Count(data, []byte(esc+"[B")) + bytes.Count(data, []byte(esc+"OB"))
	return t
}
func (t mouseTally) String() string {
	return fmt.Sprintf("wheel up %d, wheel down %d, wheel releases %d, other mouse reports %d, cursor Up %d, cursor Down %d",
		t.up, t.down, t.releases, t.other, t.keyUp, t.keyDown)
}

// A labelled step: the instruction, what to expect, then what the application received.
func mouseStep(s *Session, label, instruction, expected string) mouseTally {
	s.say("")
	s.say("%s: %s", label, instruction)
	s.say("  Expected: %s", expected)
	s.say("  Space/Enter: next step | q/Esc: exit test")
	var data []byte
	deadline := time.Now().Add(seconds(s.opts.Seconds))
	for time.Now().Before(deadline) {
		chunk := s.read(minTime(deadline, time.Now().Add(100*time.Millisecond)))
		if len(chunk) > 0 {
			s.event("input", chunk)
		}
		data = append(data, chunk...)
		if s.navigation(chunk) {
			break
		}
	}
	t := tallyMouse(data)
	s.say("  Received: %s.", t)
	return t
}
func mouseAssessment(s *Session, mode int) {
	s.say("Human assessment: you know how many notches you turned; the probe counts what the")
	s.say("application received (SGR encoding). The source tree's automated exact-byte check")
	s.say("is tests/xvfb-mouse-scroll.sh, which injects the wheel under Xvfb.")
	if mode == 9 {
		s.say("Mode 9 reports buttons 1-3 only: expect no wheel reports in any step.")
	}
}
func mouseCounts(s *Session, mode int) {
	s.page("Report count")
	mouseAssessment(s, mode)
	on := modeSequence(mode, true) + modeSequence(1006, true)
	s.send(on)
	mouseStep(s, "Step 1/2, tracking on", "turn the wheel exactly 3 notches up.",
		"3 wheel-up reports, whatever the terminal's local lines per notch; xterm reports no wheel releases.")
	mouseStep(s, "Step 2/2, tracking on", "turn the wheel exactly 3 notches down.",
		"3 wheel-down reports and no releases.")
	s.send(modeSequence(mode, false) + modeSequence(1006, false))
}
func mouseHandoff(s *Session, mode int) {
	s.preserveModes(1007, 1049)
	s.page("Mode handoff")
	mouseAssessment(s, mode)
	on := modeSequence(mode, true) + modeSequence(1006, true)
	off := modeSequence(mode, false) + modeSequence(1006, false)
	s.send(on)
	mouseStep(s, "Step 1/5, tracking on", "turn one notch up.", "exactly 1 wheel-up report.")
	s.send(off)
	mouseStep(s, "Step 2/5, tracking off", "turn one notch up; the terminal may scroll its own history.",
		"nothing: a report here means tracking outlived the mode change.")
	s.send(on)
	mouseStep(s, "Step 3/5, tracking on again", "turn one notch down.",
		"exactly 1 wheel-down report; the notch from step 2 is not replayed.")
	s.send(off + modeSequence(1049, true) + esc + "[H" + esc + "[2J" + modeSequence(1007, true))
	s.say("Alternate screen, mode 1007 (alternate scroll) set.")
	mouseStep(s, "Step 4/5, tracking off", "turn one notch up.",
		"xterm sends cursor Up keys, one per line it would scroll; a terminal without alternate scroll sends nothing.")
	s.send(on)
	mouseStep(s, "Step 5/5, tracking on", "turn one notch up.",
		"exactly 1 wheel-up report and no cursor keys: tracking takes precedence over alternate scroll.")
	s.send(off + modeSequence(1007, false) + modeSequence(1049, false))
}
func keyboard(s *Session) {
	kind := strings.TrimPrefix(s.result.Case, "input keyboard ")
	s.opts.CaptureControls = true
	defer func() { s.opts.CaptureControls = false }()
	if kind == "cooked" {
		if err := setState(s.in, s.saved); err != nil {
			panic(probeFailure{err})
		}
		s.cleanup(func() {
			if err := makeRaw(s.in, s.saved); err != nil {
				panic(probeFailure{err})
			}
		})
		s.say("Cooked input: the terminal edits and delivers whole lines.")
		s.say("q or Esc then Enter: exit test | Space: captured as input")
	} else {
		s.say("Capture keys, modifiers and releases; Ctrl+C is captured as input.")
		s.say(captureKeys)
	}
	if kind == "kitty" {
		s.query("Kitty flags before", esc+"[?u", "\x1b\\[\\?[0-9]*u")
		s.cleanup(func() { s.send(esc + "[<u") })
		s.send(esc + "[>31u")
		s.query("Kitty flags active", esc+"[?u", "\x1b\\[\\?[0-9]*u")
	}
	pending := []byte{}
	deadline := time.Now().Add(seconds(s.opts.Seconds))
	for time.Now().Before(deadline) {
		data := s.read(minTime(deadline, time.Now().Add(100*time.Millisecond)))
		if len(data) == 0 {
			if bytes.Equal(pending, []byte{27}) {
				panic(probeExit{})
			}
			if len(pending) > 0 {
				s.say("Partial/escape input: %q", pending)
				pending = nil
			}
			continue
		}
		s.event("input", data)
		if kind == "cooked" {
			s.say("Line bytes: %q", data)
			if strings.TrimSpace(string(data)) == "q" || bytes.Equal(bytes.TrimSpace(data), []byte{27}) {
				panic(probeExit{})
			}
			continue
		}
		pending = append(pending, data...)
		if len(pending) > maxReply {
			s.say("Input event exceeds capture limit; discarded.")
			pending = nil
			continue
		}
		var events [][]byte
		events, pending = splitKeys(pending)
		for _, raw := range events {
			if k, ok := parseKey(raw); ok {
				s.say("%s raw=%q", k.String(), raw)
				if navigationAction(raw) == "exit" || (k.Code == 'q' && k.Modifiers & ^(64|128) == 0 || k.Code == 'q' && k.Modifiers & ^(64|128) == 4) && k.Event != 3 {
					panic(probeExit{})
				}
			} else {
				s.say("Input bytes: %q", raw)
				if bytes.Equal(raw, []byte("q")) || bytes.Equal(raw, []byte{17}) {
					panic(probeExit{})
				}
			}
		}
	}
}
func minTime(a, b time.Time) time.Time {
	if a.Before(b) {
		return a
	}
	return b
}
