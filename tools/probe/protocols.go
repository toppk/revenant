package main

import (
	"bytes"
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
	"unicode/utf8"
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

// DA1 codes from DEC 070 and xterm's ctlseqs; 52 is Ghostty's clipboard extension.
var da1Features = map[int]string{
	1: "132 columns", 2: "printer", 3: "ReGIS", 4: "Sixel", 6: "selective erase",
	8: "user-defined keys", 9: "national replacement charsets", 15: "technical characters",
	16: "locator", 17: "terminal state interrogation", 18: "windowing", 21: "horizontal scrolling",
	22: "ANSI color", 28: "rectangular editing", 29: "ANSI text locator", 52: "clipboard",
}
var da2Types = map[int]string{
	0: "VT100", 1: "VT220", 2: "VT240", 18: "VT330", 19: "VT340", 24: "VT320",
	32: "VT382", 41: "VT420", 61: "VT510", 64: "VT520", 65: "VT525",
}
var da1Levels = map[int]string{
	61: "VT100 class", 62: "VT220 class", 63: "VT320 class", 64: "VT420 class", 65: "VT500 class",
}

func daParams(raw []byte, prefix string) ([]int, error) {
	text := string(raw)
	if !strings.HasPrefix(text, prefix) || !strings.HasSuffix(text, "c") {
		return nil, fmt.Errorf("malformed device attributes reply")
	}
	params := []int{}
	for _, field := range strings.Split(strings.TrimSuffix(strings.TrimPrefix(text, prefix), "c"), ";") {
		n, err := strconv.Atoi(field)
		if err != nil {
			return nil, fmt.Errorf("malformed device attributes parameter %q", field)
		}
		params = append(params, n)
	}
	return params, nil
}
func decodeDA1(raw []byte) (string, error) {
	params, err := daParams(raw, esc+"[?")
	if err != nil {
		return "", err
	}
	level := fmt.Sprintf("level %d", params[0])
	if name, ok := da1Levels[params[0]]; ok {
		level += " (" + name + ")"
	}
	features := []string{}
	for _, code := range params[1:] {
		name, ok := da1Features[code]
		if !ok {
			name = "unknown"
		}
		features = append(features, fmt.Sprintf("%d %s", code, name))
	}
	if len(features) == 0 {
		return level + "; no feature codes", nil
	}
	return level + "; features: " + strings.Join(features, ", "), nil
}
func decodeDA2(raw []byte) (string, error) {
	params, err := daParams(raw, esc+"[>")
	if err != nil {
		return "", err
	}
	if len(params) != 3 {
		return "", fmt.Errorf("DA2 reply has %d parameters, expected 3", len(params))
	}
	kind, ok := da2Types[params[0]]
	if !ok {
		kind = "unknown"
	}
	return fmt.Sprintf("type %d (%s), firmware %d, cartridge %d", params[0], kind, params[1], params[2]), nil
}
func identity(s *Session) {
	for _, q := range []struct {
		name, request, pattern string
		decode                 func([]byte) (string, error)
	}{
		{"DA1", esc + "[c", "\x1b\\[\\?[0-9;]+c", decodeDA1},
		{"DA2", esc + "[>c", "\x1b\\[>[0-9;]+c", decodeDA2},
		{"DA3", esc + "[=c", "\x1bP!\\|[^\x1b]*\x1b\\\\", nil},
		{"XTVERSION", esc + "[>q", "\x1bP>\\|[^\x1b]*\x1b\\\\", nil},
	} {
		raw := s.query(q.name, q.request, q.pattern)
		if raw == nil || q.decode == nil {
			continue
		}
		if text, err := q.decode(raw); err != nil {
			s.say("Decode error: %v", err)
		} else {
			s.say("%s claims %s", q.name, text)
		}
	}
	s.say("Claims are what the terminal advertises; compare each code with its documented support.")
}

// DECRQM in both forms. Each reply is graded on three things, separately: the
// private marker must match the request's, the mode number must be echoed exactly
// (a 15-bit truncation turns 32793 into 25), and the status is reported as the
// terminal gave it. Known modes are set and reset around their queries so that a
// blanket "not recognized" cannot pass; both are restored afterwards.
func modeQueries(s *Session) {
	s.preserveModes(25)
	irm := s.ansiMode(4)
	s.cleanup(func() {
		s.send(esc + "[4" + map[bool]string{true: "h", false: "l"}[irm == 1])
	})
	s.say("DECRQM replies, ANSI (CSI Ps $ p) and DEC private (CSI ? Ps $ p).")
	s.say("One exact reply shows that request was answered; it does not establish")
	s.say("support for any mode. Timeouts are reported, never read as a status.")
	type query struct {
		label, setup string
		private      bool
		mode         int
		want         string
	}
	queries := []query{
		{"ANSI IRM after set", esc + "[4h", false, 4, "1 set"},
		{"ANSI IRM after reset", esc + "[4l", false, 4, "2 reset"},
		{"ANSI unknown mode", "", false, 9999, "0 not recognized"},
		{"ANSI 32767 (largest signed 16-bit)", "", false, 32767, "0 not recognized"},
		{"ANSI 32768 (first past the signed boundary)", "", false, 32768, "0 not recognized"},
		{"ANSI 32772 (IRM + 32768, with IRM set)", esc + "[4h", false, 32772, "0 not recognized"},
		{"ANSI 65535 (largest 16-bit)", esc + "[4l", false, 65535, "0 not recognized"},
		{"private DECTCEM after show", esc + "[?25h", true, 25, "1 set"},
		{"private DECTCEM after hide", esc + "[?25l", true, 25, "2 reset"},
		{"private unknown mode", esc + "[?25h", true, 9999, "0 not recognized"},
		{"private 32767", "", true, 32767, "0 not recognized"},
		{"private 32768", "", true, 32768, "0 not recognized"},
		{"private 32793 (DECTCEM + 32768, with DECTCEM set)", "", true, 32793, "0 not recognized"},
		{"private 65535", "", true, 65535, "0 not recognized"},
	}
	reply := regexp.MustCompile(`^\x1b\[(\??)([0-9]+);([0-9]+)\$y$`)
	for _, q := range queries {
		marker := ""
		if q.private {
			marker = "?"
		}
		if q.setup != "" {
			s.send(q.setup)
		}
		raw := s.query(q.label, fmt.Sprintf("%s[%s%d$p", esc, marker, q.mode), "\x1b\\[\\??[0-9]+;[0-9]+\\$y")
		verdict, detail := "no reply", "timed out; the status is unknown, not reset"
		if m := reply.FindSubmatch(raw); m != nil {
			number, _ := strconv.Atoi(string(m[2]))
			switch {
			case string(m[1]) != marker:
				verdict = "wrong form"
				detail = fmt.Sprintf("answered %q to a %s request", m[1], map[bool]string{true: "private", false: "ANSI"}[q.private])
			case number != q.mode:
				verdict = "wrong mode"
				detail = fmt.Sprintf("answered mode %d for %d (truncated or aliased)", number, q.mode)
			default:
				verdict = "exact"
				detail = fmt.Sprintf("status %s; this case expects %s", m[3], q.want)
				if !strings.HasPrefix(q.want, string(m[3])+" ") {
					verdict = "unexpected status"
				}
			}
		}
		s.say("  %-50s %s: %s", q.label, verdict, detail)
		if s.result != nil {
			s.result.Findings = append(s.result.Findings, Finding{q.label, verdict, detail})
		}
	}
	s.pause()
}

// The ANSI form of DECRQM for one mode; 0 when unanswered or not recognized.
func (s *Session) ansiMode(mode int) int {
	raw := s.query(fmt.Sprintf("ANSI mode %d", mode), fmt.Sprintf("%s[%d$p", esc, mode), fmt.Sprintf("\x1b\\[%d;[0-4]\\$y", mode))
	if len(raw) == 0 {
		return 0
	}
	return int(raw[len(raw)-3] - '0')
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
	// Every action but query replaces the selection, and nothing here saves it first.
	if action != "query" {
		s.say("This replaces the %q selection for every application; its current", target)
		s.say("contents are not saved. Press q or Esc now to leave it untouched.")
		s.pause()
	}
	switch action {
	case "set":
		sent := []byte(s.opts.Text)
		s.osc(52, target+";"+base64.StdEncoding.EncodeToString(sent))
		s.say("Requested selection %s = %q (%x).", target, s.opts.Text, sent)
		// A round trip compares the bytes this terminal hands back with the bytes it was
		// given. It cannot show how the terminal converts for other X clients (STRING
		// Latin-1, UTF8_STRING): that needs an external owner and reader, which is what
		// the xvfb-clipboard suite uses.
		if value, named, ok := clipboardRead(s, target); ok {
			// Exact means the reply names the selection that was set and carries its
			// bytes; the right bytes under another target are not a round trip.
			switch {
			case named != target:
				s.say("Round trip: DIFFERS, the reply names %q instead of %q.", named, target)
			case !bytes.Equal(value, sent):
				s.say("Round trip: DIFFERS, sent %x, read back %x.", sent, value)
			default:
				s.say("Round trip: exact (%d bytes). Conversion for other clients is not shown.", len(value))
			}
		}
	case "clear":
		s.osc(52, target+";")
		s.say("Requested selection clear.")
	case "invalid":
		s.osc(52, target+";!!!!")
		s.say("Sent invalid base64. xterm clears, libghostty ignores; inspect ownership/content.")
		s.say("A query below shows the result where reads are permitted.")
		if value, _, ok := clipboardRead(s, target); ok {
			s.say("After invalid base64: %d bytes (%x).", len(value), value)
		}
	case "query":
		clipboardRead(s, target)
	}
}

// One OSC 52 read, reported exactly: the target the reply names (it should name the
// requested one), the decoded bytes in hex, and whether they are valid UTF-8. A
// timeout or refusal leaves the result unknown.
func clipboardRead(s *Session, target string) ([]byte, string, bool) {
	raw := s.query("OSC 52", esc+"]52;"+target+";?"+s.terminator(), oscPattern("52;"))
	if raw == nil {
		return nil, "", false
	}
	parts := strings.SplitN(oscBody(raw, "52;"), ";", 2)
	if len(parts) != 2 {
		s.say("Malformed selection reply")
		return nil, "", false
	}
	value, err := base64.StdEncoding.Strict().DecodeString(parts[1])
	if err != nil {
		s.say("Malformed base64: %v", err)
		return nil, "", false
	}
	named := "names the requested target"
	if parts[0] != target {
		named = fmt.Sprintf("names %q, not the requested %q", parts[0], target)
	}
	s.say("Reply %s; decoded %d bytes %x (%q), valid UTF-8: %t", named, len(value), value, value,
		utf8.Valid(value))
	return value, parts[0], true
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

// Saves both labels, reports the window label after each request, and restores on exit.
// Reports the window label; ok is false when no report arrived (an empty title is a reply).
func titlePolicyReport(s *Session) (label string, ok bool) {
	raw := s.query("Label l", esc+"[21t", oscPattern("l"))
	if raw == nil {
		s.say("No title report arrived; CSI 21 t may be refused (Window Ops), unsupported or slow.")
		return "", false
	}
	label = oscBody(raw, "l")
	s.say("Window label now %q.", label)
	return label, true
}

// Explains the cleanup limit before any label changes, then requests a save of both labels.
func titlePolicySave(s *Session) {
	s.say("This case now requests a push of both labels (CSI 22 t) and, on exit including q/Esc,")
	s.say("a pop (CSI 23 t). Either may be refused: they are Window Ops stack operations. The pop")
	s.say("restores the labels only if the stack operations are permitted and Title Ops is")
	s.say("effective (allowTitleOps on and allowSendEvents false); turning Allow Title Ops on")
	s.say("cannot help while allowSendEvents is true. Otherwise the changed labels stay: use a")
	s.say("disposable terminal.")
	s.send(esc + "[22;0t")
	s.cleanup(func() {
		s.send(esc + "[23;0t")
		s.say("Requested a pop of the saved labels. It restores them only if the stack operations")
		s.say("were permitted and Title Ops is effective; this case cannot confirm either.")
	})
}
func titlePolicy(s *Session) {
	titlePolicySave(s)
	s.say("Startup settings that decide labels: allowTitleOps and allowSendEvents. Reports")
	s.say("and the title stack are Window Ops (allowWindowOps, disallowedWindowOps) instead.")
	s.say("Configured versus effective: the menu check mark and the allow-title-ops action")
	s.say("show and change the configured allowTitleOps. Labels change only while it is on and")
	s.say("allowSendEvents is false. With allowSendEvents true, xterm greys the menu entry; the")
	s.say("action still changes the configured value (a redundant on/off rings the bell), but no")
	s.say("label changes, and a pop consumes a saved entry without restoring it.")
	s.say("Bind the action to try it, for example:")
	s.say("  -xrm 'XTerm*VT100.translations: #override Ctrl<Key>F2: allow-title-ops(off)\\n Ctrl<Key>F3: allow-title-ops(on)'")
	steps := []struct{ label, ask string }{
		{"probe policy start", "Title Ops as configured at startup."},
		{"probe policy off", "Now turn Title Ops off (allow-title-ops(off) or the menu), then continue. The label should not change."},
		{"probe policy on", "Now turn Title Ops on again, then continue. The label should change unless allowSendEvents is true."},
		{"probe policy redundant", "Invoke allow-title-ops(on) while it is on: xterm rings the bell and changes nothing. Then continue."},
	}
	for index, step := range steps {
		if index > 0 {
			s.say("%s", step.ask)
			s.pause()
		} else {
			s.say("%s", step.ask)
		}
		s.osc(2, step.label)
		s.say("Requested %q.", step.label)
		switch label, ok := titlePolicyReport(s); {
		case !ok:
			s.say("Observed: no report, so the reported label cannot be compared.")
		case label == step.label:
			s.say("Observed: the reported label matches the requested one.")
		default:
			s.say("Observed: the reported label %q differs from the requested %q.", label, step.label)
		}
	}
	s.say("Property changes are measured externally by tests/xvfb-title-ops.sh.")
	s.pause()
}

// Property inspection is external: OSC title reports cannot expose X property types.
func titleUTF8(s *Session) {
	s.say("Use a disposable X11 terminal. Record its version, locale, utf8Title,")
	s.say("allowC1Printable, sameName and Title/Window Ops settings with the observation.")
	s.say("The menu can be insensitive in a UTF-8 locale. Launch with action bindings:")
	s.say("  -xrm 'XTerm*VT100.translations: #override <Key>F10: set-utf8-title(on)\\n<Key>F11: set-utf8-title(off)'")
	s.say("F10 requests ON; F11 requests OFF. Record utf8Title at launch too: xterm's menu")
	s.say("check follows UTF-8 mode and need not show the action's current setting.")
	s.say("Note the UTF-8 Titles menu state now; manual changes persist. Restore it before")
	s.say("finishing, or after q/Esc. If the menu/action is missing, record unavailable;")
	s.say("do not pretend the requested setting took effect. --no-pause is a corpus dump only.")
	s.cleanup(func() { s.say("Restore the UTF-8 Titles setting you noted; this case cannot restore it.") })
	titlePolicySave(s)
	s.say("In ANOTHER terminal, set id to THIS terminal's top-level X window ID.")
	s.say("Obtain it with xwininfo by clicking this window. Do not use the observer's WINDOWID.")
	s.say("Run this command at every pause (8x prints raw bytes, including NUL bytes):")
	s.say("xprop -id \"$id\" -f WM_NAME 8x -f WM_ICON_NAME 8x -f _NET_WM_NAME 8x -f _NET_WM_ICON_NAME 8x WM_NAME WM_ICON_NAME _NET_WM_NAME _NET_WM_ICON_NAME")
	s.say("Keep each snapshot with its stage/sample label. Record property type, bytes and")
	s.say("absent versus present-empty separately. Inspect the initial properties now.")
	s.pause()
	for _, stage := range []string{"as found", "UTF-8 Titles ON", "UTF-8 Titles OFF", "UTF-8 Titles ON again"} {
		s.say("Stage: %s. Set it through the menu or set-utf8-title action if available, then continue.", stage)
		s.say("Take a snapshot BEFORE the next label: toggling alone may update or delete properties.")
		s.pause()
		for _, sample := range []struct{ name, label string }{
			{"ASCII", "probe ASCII"}, {"UTF-8", "café — 日本語 🛠"}, {"empty", ""},
		} {
			for _, selector := range []int{2, 1, 0} {
				label := sample.label
				if label != "" {
					label = fmt.Sprintf("%s OSC%d %s", stage, selector, label)
				}
				s.osc(selector, label)
				s.say("Stage %s; sample %s; OSC %d; payload UTF-8 hex: %x", stage, sample.name, selector, []byte(label))
				s.say("Snapshot all four properties now. OSC 2 requests the title, OSC 1 the icon,")
				s.say("OSC 0 both. Unsupported selectors or denied changes are separate observations.")
				s.pause()
			}
		}
	}
	s.say("Compare ON/OFF/ON snapshots for encoding, stale properties and recreation.")
	s.say("Do not infer support, deletion or legibility from a title report or a clean exit.")
	s.say("Repeat in separately launched C and UTF-8 locales, with utf8Title false/true.")
	s.say("Restore your original UTF-8 Titles setting before continuing to the requested pop.")
	s.pause()
}

func titleSameName(s *Session) {
	titlePolicySave(s)
	s.say("Count WM_NAME changes from another terminal while this runs:")
	s.say("  xprop -spy -id \"$WINDOWID\" WM_NAME   (xterm sets WINDOWID; otherwise use xwininfo)")
	s.say("Space/Enter sends each group.")
	for _, group := range []struct {
		labels []string
		expect string
	}{
		{[]string{"probe same A", "probe same A"}, "sameName on (the default): one change; off: two."},
		{[]string{"probe same B", "probe same B", "probe same C"}, "on: two changes; off: three."},
	} {
		s.pause()
		for _, label := range group.labels {
			s.osc(2, label)
		}
		s.say("Sent %q. Expected: %s", group.labels, group.expect)
		titlePolicyReport(s)
	}
	s.say("A label changed by another client is not rewritten by a repeated request while sameName is on.")
	s.say("Exact counts come from tests/xvfb-title-ops.sh, which observes PropertyNotify events.")
	s.pause()
}

// CSI 14/16/18 t: queried one at a time, each classified as reply, silence, unexpected
// bytes or timeout, and compared with the kernel's window size.
func windowReports(s *Session) {
	s.say("Startup settings: allowWindowOps (the blanket permission, blocked while")
	s.say("allowSendEvents is true) and disallowedWindowOps, which decides when the blanket is")
	s.say("off. Names: GetWinSizePixels (14), GetWinSizeChars (18). xterm gates CSI 16 t by")
	s.say("GetScreenSizeChars (19); \"16\" names nothing.")
	s.say("This case sends only queries, but it asks you to change Allow Window Ops in the menu,")
	s.say("and that change persists after the case ends. Note now whether the entry is checked,")
	s.say("and set it back when the case finishes or when you quit with q/Esc.")
	s.cleanup(func() {
		s.say("Set Allow Window Ops back to the state you noted at the start; this case cannot.")
	})
	reports := []struct {
		number int
		units  string
	}{
		{14, "text area height;width in pixels"},
		{16, "cell height;width in pixels"},
		{18, "text area rows;columns"},
	}
	query := func(stage string) map[int][2]int {
		values := map[int][2]int{}
		for _, report := range reports {
			reply := regexp.MustCompile(fmt.Sprintf("\\x1b\\[%d;([0-9]+);([0-9]+)t", report.number-10))
			label := fmt.Sprintf("%s CSI %d t (%s)", stage, report.number, report.units)
			found := statusQuery(s, label, fmt.Sprintf("%s[%dt", esc, report.number), "", reply)
			if match := reply.FindStringSubmatch(found); match != nil {
				first, _ := strconv.Atoi(match[1])
				second, _ := strconv.Atoi(match[2])
				values[report.number] = [2]int{first, second}
			}
		}
		compareWindowReports(s, values)
		return values
	}
	query("Startup:")
	s.say("Toggle Allow Window Ops in the menu (Ctrl+right-click), then continue. With it off,")
	s.say("only the reports your disallowedWindowOps list permits answer. While allowSendEvents")
	s.say("is true the entry is greyed, and the list alone decides.")
	s.pause()
	query("After the first toggle:")
	s.say("Toggle it again, back to the state you noted, and continue.")
	s.pause()
	query("After the second toggle:")
	s.say("The source tree checks exact bytes and geometry in tests/xvfb-window-ops.sh.")
	s.pause()
}

// Compares the replies with each other and with TIOCGWINSZ; a missing value is not compared.
func compareWindowReports(s *Session, values map[int][2]int) {
	columns, rows, width, height, ok := terminalPixels(s.out)
	if chars, have := values[18]; have && ok {
		s.say("CSI 18 t %dx%d against the kernel's %dx%d rows x columns: %s", chars[0], chars[1], rows, columns,
			sameOrDifferent(chars[0] == rows && chars[1] == columns))
	}
	if pixels, have := values[14]; have && ok && width > 0 && height > 0 {
		s.say("CSI 14 t %dx%d against the kernel's %dx%d pixels: %s", pixels[0], pixels[1], height, width,
			sameOrDifferent(pixels[0] == height && pixels[1] == width))
	}
	cell, haveCell := values[16]
	chars, haveChars := values[18]
	if pixels, have := values[14]; have && haveCell && haveChars {
		s.say("CSI 16 t x CSI 18 t = %dx%d against CSI 14 t %dx%d: %s", cell[0]*chars[0], cell[1]*chars[1],
			pixels[0], pixels[1], sameOrDifferent(cell[0]*chars[0] == pixels[0] && cell[1]*chars[1] == pixels[1]))
	}
}
func sameOrDifferent(same bool) string {
	if same {
		return "same"
	}
	return "DIFFERENT"
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
		s.say("wrapped: " + strings.Repeat("w", 110))
		// Enough output that the prompts leave a 24-row window, so navigation has somewhere to go.
		for k := 1; k <= 8; k++ {
			s.say("command-%d output line %d", i, k)
		}
		s.say("COMMAND-%d-END", i)
		s.osc(133, "D;0")
		active = false
	}
	active = true
	s.osc(133, "A")
	s.send("probe-ready$ ")
	s.osc(133, "B")
	s.say("[waiting for manual action]")
	if s.result.Case == "shell prompts" {
		s.say("CHECK 1: Repeated Ctrl+Shift+Up should visit earlier probe-N$ prompts.")
		s.say("CHECK 2: Another Ctrl+Shift+Up at probe-1$ should leave the view unchanged.")
		s.say("CHECK 3: Repeated Ctrl+Shift+Down should visit later prompts and return to probe-ready$.")
		s.say("CHECK 4: Another Ctrl+Shift+Down at probe-ready$ should leave the live view unchanged.")
		s.say("PASS: each jump puts the named prompt at the top; both boundary checks stay put.")
	} else {
		s.say("PASS: the pipe contains only COMMAND-3-BEGIN through COMMAND-3-END.")
	}
	s.pauseLabel("Space/Enter: finish test | q/Esc: stop test")
}
func notify(s *Session) {
	s.say("Switch focus away; requests are sent after three seconds.")
	s.wait(3 * time.Second)
	s.osc(9, "probe OSC 9 notification")
	s.osc(777, "notify;probe;Manual OSC 777 notification")
	s.say("Requests sent. While unfocused the window's WM_HINTS urgency flag should be set (xprop -id WINDOW WM_HINTS); focusing it should clear the flag.")
	s.pause()
}
func desktopNotify(s *Session) {
	s.say("Switch focus away; requests are sent after three seconds.")
	s.wait(3 * time.Second)
	s.osc(9, "probe desktop notification: Grüße ✓ <b>not bold</b>")
	s.osc(777, "notify;Probe title ✓;Body with <i>markup</i> & an ampersand")
	s.say("Requests sent. Expect two desktop notifications (subject to the rate limit) showing the markup as literal text.")
	s.pause()
	s.say("With the terminal focused, a request is sent now; it should log but not appear.")
	s.osc(9, "probe focused request: this should not appear")
	s.pause()
}
func progress(s *Session) {
	s.cleanup(func() { s.osc(9, "4;0") })
	s.say("Inspect at 80x24: the terminal draws a small bar in the top-right corner, and the window title must not change.")
	s.say("Error and paused without a value keep the last percentage; 150 is shown as a full bar.")
	for _, state := range []string{"1;10", "1;60", "2", "2;30", "4", "4;80", "3", "1;150", "0"} {
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
