package main

import (
	"bytes"
	"fmt"
	"time"
)

const inspectionKeys = "Space/Enter: continue | q/Esc: exit test"
const captureKeys = "q/Esc: exit test | Space/Enter: captured as input"

type probeExit struct{}

// Navigation applies only while inspecting or waiting, never inside a protocol
// opaque answerback buffer. Framed queries also use this parser: q inside a
// complete or fragmented escape sequence is data, while a bare q is navigation.
func navigationAction(raw []byte) string {
	if bytes.Equal(raw, []byte("q")) || bytes.Equal(raw, []byte{27}) {
		return "exit"
	}
	if bytes.Equal(raw, []byte(" ")) || bytes.Equal(raw, []byte("\r")) || bytes.Equal(raw, []byte("\n")) {
		return "continue"
	}
	if k, ok := parseKey(raw); ok && k.Event != 3 && k.Modifiers & ^(64|128) == 0 {
		if k.Code == 'q' || k.Code == 27 {
			return "exit"
		}
		if k.Code == ' ' || k.Code == 13 {
			return "continue"
		}
	}
	return ""
}
func (s *Session) navigation(data []byte) bool {
	s.navPending = append(s.navPending, data...)
	events, left := splitKeys(s.navPending)
	s.navPending = left
	if bytes.Equal(left, []byte{27}) {
		if s.navEscape.IsZero() {
			s.navEscape = time.Now()
		}
		if !s.querying && len(data) == 0 && time.Since(s.navEscape) >= 100*time.Millisecond {
			panic(probeExit{})
		}
	} else {
		s.navEscape = time.Time{}
	}
	if len(s.navPending) > maxReply {
		panic(probeFailure{fmt.Errorf("incomplete input event exceeds %d bytes", maxReply)})
	}
	advance := false
	for _, event := range events {
		switch navigationAction(event) {
		case "exit":
			panic(probeExit{})
		case "continue":
			advance = true
		}
	}
	return advance
}

// Auxiliary browser inspection (completed replies or selectable location text)
// can be dismissed without aborting a running case, because the case is over.
func (s *Session) dismissiblePause(label string) (exited bool) {
	defer func() {
		if p := recover(); p != nil {
			if _, ok := p.(probeExit); ok {
				exited = true
			} else {
				panic(p)
			}
		}
	}()
	s.pauseLabel(label)
	return
}
