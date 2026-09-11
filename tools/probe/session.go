package main

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"regexp"
	"time"
	"unicode/utf8"
)

const esc = "\x1b"
const st = esc + "\\"
const maxReply = 65536

var errInterrupted = errors.New("interrupted")

type probeFailure struct{ err error }
type Session struct {
	in, out           *os.File
	saved             terminalState
	ctx               context.Context
	opts              Options
	result            *Result
	cleanups          []func()
	cleaning          bool
	quiet             bool
	writes, inspected uint64
	navPending        []byte
	quit              bool
	navEscape         time.Time
	querying          bool
}

func openSession(ctx context.Context, opts Options) (*Session, error) {
	saved, err := getState(os.Stdin)
	if err != nil {
		return nil, fmt.Errorf("run directly inside a terminal: %w", err)
	}
	if _, err := getState(os.Stdout); err != nil {
		return nil, fmt.Errorf("stdout must be the terminal under test: %w", err)
	}
	if err := makeRaw(os.Stdin, saved); err != nil {
		return nil, err
	}
	return &Session{in: os.Stdin, out: os.Stdout, saved: saved, ctx: ctx, opts: opts}, nil
}
func (s *Session) close() error { return setState(s.in, s.saved) }
func (s *Session) check() {
	if !s.cleaning && s.ctx.Err() != nil {
		panic(probeFailure{errInterrupted})
	}
}
func (s *Session) send(text string) {
	s.check()
	if !s.cleaning {
		s.writes++
	}
	for len(text) > 0 {
		n, err := io.WriteString(s.out, text)
		if err != nil {
			panic(probeFailure{err})
		}
		if n == 0 {
			panic(probeFailure{io.ErrNoProgress})
		}
		text = text[n:]
	}
}
func (s *Session) say(format string, args ...any) { s.send(fmt.Sprintf(format, args...) + "\r\n") }
func (s *Session) osc(code int, payload string) {
	s.send(fmt.Sprintf("%s]%d;%s%s", esc, code, payload, s.terminator()))
}
func (s *Session) terminator() string {
	if s.opts.BEL {
		return "\a"
	}
	return st
}
func (s *Session) resetColor(code int) { s.send(fmt.Sprintf("%s]%d%s", esc, code+100, s.terminator())) }
func (s *Session) read(deadline time.Time) []byte {
	for time.Now().Before(deadline) {
		s.check()
		ready, err := readReady(s.in, min(time.Until(deadline), 50*time.Millisecond))
		if err != nil {
			panic(probeFailure{err})
		}
		if !ready {
			continue
		}
		data := make([]byte, 8192)
		n, err := s.in.Read(data)
		if err != nil {
			panic(probeFailure{err})
		}
		if n == 0 {
			panic(probeFailure{io.EOF})
		}
		data = data[:n]
		if bytes.Contains(data, []byte{3}) && !s.opts.CaptureControls {
			panic(probeFailure{errInterrupted})
		}
		return data
	}
	return nil
}
func (s *Session) wait(d time.Duration) {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		s.navigation(s.read(minTime(deadline, time.Now().Add(100*time.Millisecond))))
	}
}
func (s *Session) pause() { s.pauseLabel(inspectionKeys) }
func (s *Session) pauseLabel(label string) {
	defer func() { s.inspected = s.writes }()
	if s.opts.NoPause {
		return
	}
	if s.opts.Delay > 0 {
		s.wait(seconds(s.opts.Delay))
		return
	}
	s.send(label)
	for {
		if s.navigation(s.read(time.Now().Add(100 * time.Millisecond))) {
			s.say("")
			return
		}
	}
}
func (s *Session) cleanup(fn func()) { s.cleanups = append(s.cleanups, fn) }
func (s *Session) restore() (err error) {
	s.cleaning = true
	var unexpected any
	defer func() {
		s.cleaning = false
		s.cleanups = nil
		if unexpected != nil {
			panic(unexpected)
		}
	}()
	for i := len(s.cleanups) - 1; i >= 0; i-- {
		func() {
			defer func() {
				if p := recover(); p != nil {
					if f, ok := p.(probeFailure); ok {
						err = errors.Join(err, f.err)
					} else if unexpected == nil {
						unexpected = p
					}
				}
			}()
			s.cleanups[i]()
		}()
	}
	return err
}
func (s *Session) event(direction string, data []byte) {
	if s.result == nil {
		return
	}
	// Keep enough protocol evidence without allowing unbounded captures.
	if s.result.traceBytes+len(data) > 1024*1024 {
		s.result.TraceTruncated = true
		return
	}
	s.result.traceBytes += len(data)
	s.result.Events = append(s.result.Events, Event{direction, append([]byte(nil), data...)})
}
func (s *Session) query(label, request, pattern string) []byte {
	querying := s.querying
	s.querying = true
	defer func() { s.querying = querying }()
	s.navPending = nil
	s.navEscape = time.Time{}
	defer func() { s.navPending = nil; s.navEscape = time.Time{} }()
	s.event("request", []byte(request))
	s.send(request)
	matcher := regexp.MustCompile(pattern)
	reply := []byte{}
	deadline := time.Now().Add(seconds(s.opts.Timeout))
	for time.Now().Before(deadline) && len(reply) < maxReply {
		data := s.read(minTime(deadline, time.Now().Add(100*time.Millisecond)))
		s.event("received", data)
		if s.result != nil && !s.cleaning {
			s.navigation(data)
		}
		reply = append(reply, data[:min(len(data), maxReply-len(reply))]...)
		if found := matcher.FindString(wireView(reply)); found != "" {
			match := make([]byte, 0, len(found))
			for _, r := range found {
				if r >= 0xe000 {
					r -= 0xe000
				}
				match = append(match, byte(r))
			}
			if !s.quiet {
				s.say("%s: %q", label, match)
			}
			return match
		}
	}
	if !s.quiet {
		s.say("%s: no complete matching reply (denied, unsupported, empty, or timed out); captured %q", label, reply)
	}
	return nil
}
func modeSequence(mode int, on bool) string {
	suffix := "l"
	if on {
		suffix = "h"
	}
	return fmt.Sprintf("%s[?%d%s", esc, mode, suffix)
}
func (s *Session) mode(mode int) int {
	raw := s.query(fmt.Sprintf("Mode %d", mode), fmt.Sprintf("%s[?%d$p", esc, mode), fmt.Sprintf("\x1b\\[\\?%d;[0-4]\\$y", mode))
	if len(raw) == 0 {
		return 0
	}
	return int(raw[len(raw)-3] - '0')
}
func (s *Session) preserveModes(modes ...int) {
	states := make(map[int]int)
	s.cleanup(func() {
		for _, m := range modes {
			state, ok := states[m]
			if !ok {
				continue
			}
			if state == 0 {
				s.send(modeSequence(m, m == 25))
			} else if state == 1 || state == 2 {
				s.send(modeSequence(m, state == 1))
			}
		}
	})
	for _, m := range modes {
		states[m] = s.mode(m)
		if states[m] == 0 {
			s.say("Mode %d startup state unknown; cleanup will request its baseline default (visible cursor; other modes off).", m)
		}
	}
}
func seconds(v float64) time.Duration { return time.Duration(v * float64(time.Second)) }

// Valid UTF-8 bytes cannot be mistaken for standalone C1 delimiters. A reversible
// private-use representation lets regexp match wire bytes, including raw C1 ST.
func wireView(data []byte) string {
	view := []rune{}
	for i := 0; i < len(data); {
		_, n := utf8.DecodeRune(data[i:])
		if n > 1 {
			for _, b := range data[i : i+n] {
				view = append(view, 0xe000+rune(b))
			}
			i += n
		} else {
			view = append(view, rune(data[i]))
			i++
		}
	}
	return string(view)
}

// Visual pages use the normal screen with mouse capture released. Keep a stable
// case ID visible so screenshots identify the failing scenario.
func (s *Session) page(title string) {
	width, _ := terminalSize(s.out)
	s.send(esc + "[0m" + esc + "[H" + esc + "[2J")
	id := "probe"
	if s.result != nil {
		id = s.result.CaseID
	}
	s.say("%s", clipText("Test: "+id+" / "+title, max(1, width-1)))
	s.say("")
}
