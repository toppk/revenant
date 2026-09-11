package main

import (
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"
)

const oscEnd = "(?:\x07|\x1b\\\\|\\x9c)"

func oscPattern(prefix string) string {
	return "(?:\x1b\\]|\\x9d)" + regexp.QuoteMeta(prefix) + "[^\x07\x1b\\x9c]*" + oscEnd
}
func oscBody(raw []byte, prefix string) string {
	text := string(raw)
	text = strings.TrimPrefix(text, esc+"]")
	text = strings.TrimPrefix(text, "\x9d")
	text = strings.TrimPrefix(text, prefix)
	for _, end := range []string{st, "\a", "\x9c"} {
		text = strings.TrimSuffix(text, end)
	}
	return text
}
func answerback(s *Session) {
	// Answerback is arbitrary data, including ETX; only an actual process
	// signal may interrupt its bounded read window.
	captureControls := s.opts.CaptureControls
	s.opts.CaptureControls = true
	defer func() { s.opts.CaptureControls = captureControls }()
	s.event("request", []byte{5})
	s.send("\x05")
	deadline := time.Now().Add(seconds(s.opts.Timeout))
	reply := []byte{}
	for time.Now().Before(deadline) && len(reply) < maxReply {
		data := s.read(deadline)
		s.event("received", data)
		reply = append(reply, data[:min(len(data), maxReply-len(reply))]...)
	}
	s.say("Answerback (%d bytes): %q", len(reply), reply)
	if len(reply) == 0 {
		s.say("Silence may be an empty configuration or unsupported ENQ.")
	}
}
func identity(s *Session) {
	for _, q := range []struct{ name, request, pattern string }{
		{"DA1", esc + "[c", "\x1b\\[\\?[0-9;]+c"},
		{"DA2", esc + "[>c", "\x1b\\[>[0-9;]+c"},
		{"DA3", esc + "[=c", "\x1bP!\\|[^\x1b]*\x1b\\\\"},
		{"XTVERSION", esc + "[>q", "\x1bP>\\|[^\x1b]*\x1b\\\\"},
	} {
		s.query(q.name, q.request, q.pattern)
	}
}

type TcapReply struct {
	Supported   bool
	Name, Value []byte
}

func decodeTcap(raw []byte) (TcapReply, error) {
	m := regexp.MustCompile("^\x1bP([01])\\+r([0-9A-Fa-f]+)(?:=([0-9A-Fa-f]*))?\x1b\\\\$").FindSubmatch(raw)
	if m == nil {
		return TcapReply{}, fmt.Errorf("malformed XTGETTCAP reply")
	}
	name, e := hex.DecodeString(string(m[2]))
	if e != nil {
		return TcapReply{}, e
	}
	value, e := hex.DecodeString(string(m[3]))
	return TcapReply{m[1][0] == '1', name, value}, e
}
func tcap(s *Session) {
	caps := s.opts.Caps
	if len(caps) == 0 {
		caps = []string{"TN", "Co", "RGB", "not-a-capability"}
	}
	for _, cap := range caps {
		key := strings.ToUpper(hex.EncodeToString([]byte(cap)))
		raw := s.query(cap, esc+"P+q"+key+st, "\x1bP[01]\\+r(?i:"+key+")(?:=[^\x1b]*)?\x1b\\\\")
		if raw != nil {
			r, e := decodeTcap(raw)
			if e != nil {
				s.say("Decode error: %v", e)
			} else {
				s.say("supported=%v name=%q value=%q", r.Supported, r.Name, r.Value)
			}
		}
	}
}
func selectionTarget(target string) (string, bool) {
	m := map[string]string{"clipboard": "c", "primary": "p", "select": "s", "c": "c", "p": "p", "s": "s"}
	v, ok := m[strings.ToLower(target)]
	return v, ok
}
func clipboard(s *Session) {
	target, _ := selectionTarget(s.opts.Target)
	action := strings.TrimPrefix(s.result.Case, "clipboard ")
	switch action {
	case "set":
		s.osc(52, target+";"+base64.StdEncoding.EncodeToString([]byte(s.opts.Text)))
		s.say("Requested selection %s = %q; query or paste to verify.", target, s.opts.Text)
	case "clear":
		s.osc(52, target+";")
		s.say("Requested selection clear.")
	case "invalid":
		s.osc(52, target+";!!!!")
		s.say("Sent invalid base64. xterm clears, libghostty ignores; inspect ownership/content.")
	case "query":
		raw := s.query("OSC 52", esc+"]52;"+target+";?"+s.terminator(), oscPattern("52;"))
		if raw != nil {
			parts := strings.SplitN(oscBody(raw, "52;"), ";", 2)
			if len(parts) != 2 {
				s.say("Malformed selection reply")
				return
			}
			value, err := base64.StdEncoding.Strict().DecodeString(parts[1])
			if err != nil {
				s.say("Malformed base64: %v", err)
			} else {
				s.say("target=%q decoded (%d bytes)=%q", parts[0], len(value), value)
			}
		}
	}
}
func titles(s *Session) {
	targets := []int{1, 2}
	operand := 0
	if s.opts.Target == "icon" {
		targets = []int{1}
		operand = 1
	}
	if s.opts.Target == "title" {
		targets = []int{2}
		operand = 2
	}
	report := func() {
		for _, target := range targets {
			marker := "L"
			if target == 2 {
				marker = "l"
			}
			raw := s.query("Label "+marker, fmt.Sprintf("%s[%dt", esc, 19+target), oscPattern(marker))
			if raw != nil {
				s.say("label=%q", oscBody(raw, marker))
			}
		}
	}
	if s.result.Case == "titles query" {
		report()
		return
	}
	depth := 0
	pop := func() { s.send(fmt.Sprintf("%s[23;%dt", esc, operand)); depth-- }
	s.cleanup(func() {
		for depth > 0 {
			pop()
		}
	})
	push := func() { depth++; s.send(fmt.Sprintf("%s[22;%dt", esc, operand)) }
	for _, stage := range []string{"A", "B"} {
		push()
		for _, target := range targets {
			s.osc(target, "probe titles "+stage)
		}
		s.say("Requested %s", stage)
		report()
		s.pause()
	}
	for _, stage := range []string{"A", "original"} {
		pop()
		s.say("Requested restoration to %s", stage)
		report()
		s.pause()
	}
}
func font(s *Session) {
	if s.result.Case == "font set" {
		if s.opts.Font == "" {
			panic(probeFailure{fmt.Errorf("font set requires --font NAME")})
		}
		s.osc(50, s.opts.Font)
		s.say("Requested persistent font %q", s.opts.Font)
		return
	}
	s.query("Font", esc+"]50;?"+s.terminator(), oscPattern("50;"))
}
func shellFixture(s *Session) {
	path, err := filepath.Abs(s.opts.CWD)
	if err != nil {
		panic(probeFailure{err})
	}
	info, err := os.Stat(path)
	if err != nil {
		panic(probeFailure{err})
	}
	if !info.IsDir() {
		panic(probeFailure{fmt.Errorf("cwd must be a directory")})
	}
	uri := func(path string) string { return (&url.URL{Scheme: "file", Host: "localhost", Path: path}).String() }
	cwd, _ := os.Getwd()
	s.cleanup(func() { s.osc(7, uri(cwd)) })
	s.osc(7, uri(path))
	s.say("Reported cwd %q", path)
	if s.result.Case == "shell cwd" {
		s.pause()
		return
	}
	active := false
	s.cleanup(func() {
		if active {
			s.osc(133, "C")
			s.osc(133, "D;0")
		}
	})
	s.say("Synthetic shell markers; displayed commands are never executed.")
	for i := 1; i <= 3; i++ {
		active = true
		s.osc(133, "A")
		s.send(fmt.Sprintf("probe-%d$ ", i))
		s.osc(133, "B")
		s.say("synthetic-command-%d", i)
		s.osc(133, "C")
		s.say("COMMAND-%d-BEGIN", i)
		s.say("output with Unicode: café 界; literal shell text: $(do-not-execute)")
		s.say("COMMAND-%d-END", i)
		s.osc(133, "D;0")
		active = false
	}
	active = true
	s.osc(133, "A")
	s.send("probe-ready$ ")
	s.osc(133, "B")
	s.say("[waiting for manual navigation or pipe action]")
	s.say("Pipe must contain only COMMAND-3-BEGIN through COMMAND-3-END.")
	s.pause()
}
func notify(s *Session) {
	s.say("Switch focus away; requests are sent after three seconds.")
	s.wait(3 * time.Second)
	s.osc(9, "probe OSC 9 notification")
	s.osc(777, "notify;probe;Manual OSC 777 notification")
	s.say("Requests sent. Inspect urgency/delivery and focus behavior.")
	s.pause()
}
func progress(s *Session) {
	s.cleanup(func() { s.osc(9, "4;0") })
	for _, state := range []string{"1;10", "1;60", "2;60", "3", "4;60", "0"} {
		s.osc(9, "4;"+state)
		s.say("Requested progress state %s", state)
		s.pause()
	}
}
func unknown(s *Session) {
	s.send(esc + "_unknown-probe-dispatch" + st)
	s.say("Sent unsupported APC; check terminal diagnostics. No OSC/CSI support is implied.")
}
func graphics(s *Session) {
	id := 100000 + os.Getpid()%100000
	s.cleanup(func() { s.send(fmt.Sprintf("%s_Ga=d,d=I,i=%d%s", esc, id, st)) })
	pixels := []byte{255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255}
	request := fmt.Sprintf("%s_Ga=T,f=32,s=2,v=2,c=12,r=6,i=%d;%s%s", esc, id, base64.StdEncoding.EncodeToString(pixels), st)
	s.query("Kitty placement", request, "\x1b_G[^\x1b]*\x1b\\\\")
	s.say("Expect red/green above blue/yellow; scroll and resize.")
	s.pause()
}
func cursor(s *Session) {
	raw := s.query("Cursor style", esc+"P$q q"+st, "\x1bP[01]\\$r[0-9]+ q\x1b\\\\")
	s.say("Inspect startup cursor before changing its style.")
	s.pause()
	if !s.opts.Styles {
		return
	}
	restore := 0
	m := regexp.MustCompile("\x1bP1\\$r([0-6]) q").FindSubmatch(raw)
	if m != nil {
		restore, _ = strconv.Atoi(string(m[1]))
	} else {
		s.say("Original cursor style unreadable; cleanup requests default.")
	}
	s.cleanup(func() { s.send(fmt.Sprintf("%s[%d q", esc, restore)) })
	for style := 1; style <= 6; style++ {
		s.send(fmt.Sprintf("%s[%d q", esc, style))
		s.say("Requested cursor style %d", style)
		s.pause()
	}
}
