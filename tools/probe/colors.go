package main

import (
	"bytes"
	_ "embed"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
)

type Palette struct {
	Colors []string `json:"colors"`
	BG     string   `json:"bg"`
	FG     string   `json:"fg"`
	Cursor string   `json:"cursor"`
}

//go:embed data/palettes.json
var paletteData []byte

func palettes() map[string]Palette {
	var p map[string]Palette
	if err := json.Unmarshal(paletteData, &p); err != nil {
		panic(err)
	}
	return p
}

var rgbPattern = regexp.MustCompile(`^rgb:([0-9a-fA-F]{1,4})/([0-9a-fA-F]{1,4})/([0-9a-fA-F]{1,4})$`)

func rgbHex(value string) (string, bool) {
	m := rgbPattern.FindStringSubmatch(value)
	if m == nil {
		return "", false
	}
	out := "#"
	for _, part := range m[1:] {
		n, _ := strconv.ParseInt(part, 16, 32)
		max := int64(1)<<(4*len(part)) - 1
		v := (n*255 + max/2) / max
		out += fmt.Sprintf("%02x", v)
	}
	return out, true
}
func colorQuery(s *Session, code int) string {
	prefix := fmt.Sprintf("%d;", code)
	raw := s.query(fmt.Sprintf("Color %d", code), esc+"]"+prefix+"?"+s.terminator(), oscPattern(prefix))
	if raw == nil {
		return ""
	}
	value := oscBody(raw, prefix)
	if approx, ok := rgbHex(value); ok {
		s.say("  approximately %s", approx)
		return value
	}
	s.say("  Unexpected RGB payload %q", value)
	return ""
}
func dynamicColors(s *Session) {
	codes := []int{10, 11, 12}
	names := []string{"foreground", "background", "cursor"}
	action := strings.TrimPrefix(s.result.Case, "colors dynamic ")
	if action == "set" || action == "reset" {
		for i, code := range codes {
			if s.opts.Target == names[i] || s.opts.Target == "all" {
				if action == "set" {
					s.osc(code, s.opts.Color)
				} else {
					s.resetColor(code)
				}
			}
		}
		s.say("Requested persistent %s of %s; query or inspect to verify.", action, s.opts.Target)
		return
	}
	if action == "query" {
		for _, code := range codes {
			colorQuery(s, code)
		}
		return
	}
	originals := make(map[int]string)
	changed := false
	s.cleanup(func() {
		if changed {
			for _, code := range codes {
				if value := originals[code]; value != "" {
					s.osc(code, value)
				} else {
					s.resetColor(code)
				}
			}
		}
	})
	for _, code := range codes {
		originals[code] = colorQuery(s, code)
	}
	sample := func() {
		s.say("Default foreground/background: AaBb 0123456789")
		s.say(esc + "[38;2;255;255;255;48;2;24;24;24m Explicit RGB reference " + esc + "[0m")
	}
	s.cleanup(func() { s.send(esc + "[0m") })
	s.say("Unreadable original colors will reset to configured defaults; keep permission enabled for cleanup.")
	sample()
	s.pause()
	changed = true
	for i, color := range []string{"#ffe080", "#142850", "#ff60c0"} {
		s.osc(codes[i], color)
	}
	s.say("Requested yellow text, dark blue background, pink cursor. Existing default cells should change too.")
	for _, code := range codes {
		colorQuery(s, code)
	}
	sample()
	s.pause()
	for _, code := range codes {
		s.resetColor(code)
		s.say("Requested reset of color %d only", code)
		colorQuery(s, code)
		sample()
		s.pause()
	}
}

// Sends REQUEST and a status request, then classifies everything that came back:
// the expected reply, the status reply, and any other bytes, reported separately.
func statusQuery(s *Session, label, request, what string, expected *regexp.Regexp) string {
	querying := s.querying
	s.querying = true
	defer func() { s.querying = querying; s.navPending = nil; s.navEscape = time.Time{} }()
	ack := []byte(esc + "[0n")
	request += esc + "[5n"
	s.event("request", []byte(request))
	s.send(request)
	reply := []byte{}
	deadline := time.Now().Add(seconds(s.opts.Timeout))
	for time.Now().Before(deadline) && len(reply) < maxReply && !bytes.Contains(reply, ack) {
		data := s.read(minTime(deadline, time.Now().Add(100*time.Millisecond)))
		s.event("received", data)
		reply = append(reply, data...)
	}
	acked := bytes.Contains(reply, ack)
	rest := bytes.Replace(reply, ack, nil, 1)
	found := expected.Find(rest)
	if found != nil {
		rest = bytes.Replace(rest, found, nil, 1)
	}
	switch {
	case len(reply) == 0:
		s.say("%s: timeout, no reply and no status reply.", label)
	case len(rest) > 0 && acked:
		s.say("%s: unexpected bytes %q with the status reply; not a valid %sreply.", label, reply, what)
	case len(rest) > 0:
		s.say("%s: unexpected bytes %q and no status reply.", label, reply)
	case found != nil && acked:
		s.say("%s: reply %q", label, found)
		return string(found)
	case found != nil:
		s.say("%s: reply %q without the status reply; the status request was not answered.", label, found)
		return string(found)
	default:
		s.say("%s: silence (the terminal answered the status request, not the query).", label)
	}
	return ""
}
func colorPolicyQuery(s *Session, label, request, prefix string) string {
	color := regexp.MustCompile("\x1b\\]" + regexp.QuoteMeta(prefix) +
		"rgb:[0-9a-fA-F]{1,4}/[0-9a-fA-F]{1,4}/[0-9a-fA-F]{1,4}(?:\x07|\x1b\\\\)")
	return statusQuery(s, label, request, prefix, color)
}
func colorsDynamicPolicy(s *Session) {
	s.say("Startup resources decide this case: allowColorOps, allowSendEvents, disallowedColorOps.")
	s.say("xterm blocks the blanket permission while allowSendEvents is true; operations missing")
	s.say("from disallowedColorOps are still allowed. Compare in a disposable terminal, e.g.:")
	s.say("  xterm -xrm 'XTerm*allowSendEvents: true' -xrm 'XTerm*disallowedColorOps: GetColor'")
	s.say("")
	s.say("This case changes the default foreground. On exit, including q/Esc, it requests the")
	s.say("original foreground back if the startup query read it; otherwise it can only request")
	s.say("a reset to the configured default (OSC 110). Both are refused while SetColor is")
	s.say("denied, so the change can outlive the case: close the disposable terminal then.")
	query := func(stage string) string {
		fg := colorPolicyQuery(s, stage+" OSC 10", esc+"]10;?"+s.terminator(), "10;")
		colorPolicyQuery(s, stage+" OSC 4;1", esc+"]4;1;?"+s.terminator(), "4;1;")
		return fg
	}
	original := query("Startup:")
	restore := ""
	if original != "" {
		restore = strings.TrimSuffix(strings.TrimSuffix(strings.TrimPrefix(original, esc+"]10;"), st), "\a")
	}
	s.cleanup(func() {
		if restore != "" {
			s.osc(10, restore)
			s.say("Requested the original foreground %s back; applied only if SetColor is allowed.", restore)
		} else {
			s.resetColor(10)
			s.say("Requested a reset to the configured default foreground (OSC 110), since the original")
			s.say("was unreadable; applied only if SetColor is allowed.")
		}
	})
	s.osc(10, "#ffe080")
	s.say("Requested foreground #ffe080 (SetColor); this line shows whether it applied.")
	query("After the write:")
	s.say("Now turn Allow Color Ops off (menu, or bind allow-color-ops(off)) and continue.")
	s.pause()
	s.osc(10, "#60c0ff")
	s.say("Requested foreground #60c0ff; with the blanket permission off, SetColor decides.")
	query("Allow Color Ops off:")
	s.say("Turn Allow Color Ops on again and continue. With allowSendEvents true the menu and")
	s.say("action still change the setting, but the blanket permission stays blocked.")
	s.pause()
	query("Allow Color Ops on:")
	s.say("The source tree checks exact bytes and pixels in tests/xvfb-color-ops.sh.")
	s.pause()
}
func scheme(s *Session) {
	raw := s.query("Color scheme", esc+"[?996n", "\x1b\\[\\?997;[12]n")
	if raw != nil {
		if raw[len(raw)-2] == '1' {
			s.say("dark")
		} else {
			s.say("light")
		}
	}
}
func paletteQuery(s *Session, index int) string {
	prefix := fmt.Sprintf("4;%d;", index)
	raw := s.query(fmt.Sprintf("Palette %d", index), esc+"]"+prefix+"?"+s.terminator(), oscPattern(prefix))
	if raw == nil {
		return ""
	}
	value := oscBody(raw, prefix)
	if rgb, ok := rgbHex(value); ok {
		s.say("  approximately %s", rgb)
		return value
	}
	s.say("  Unexpected RGB payload %q", value)
	return ""
}
func paletteSet(s *Session, index int, color string) { s.osc(4, fmt.Sprintf("%d;%s", index, color)) }
func paletteReset(s *Session, index int)             { s.osc(104, strconv.Itoa(index)) }
func palette(s *Session) {
	action := strings.TrimPrefix(s.result.Case, "colors palette ")
	switch action {
	case "query":
		paletteQuery(s, s.opts.Index)
		return
	case "set":
		paletteSet(s, s.opts.Index, s.opts.Color)
		s.say("Requested persistent palette index %d = %q", s.opts.Index, s.opts.Color)
		return
	case "reset":
		s.send(esc + "]104" + s.terminator())
		s.say("Requested reset of all palette indices to configured defaults.")
		return
	case "spawn":
		if code := spawnPalette(s.opts); code != 0 {
			panic(probeFailure{fmt.Errorf("terminal spawn failed")})
		}
		return
	case "list":
		p := palettes()
		names := []string{}
		for name := range p {
			names = append(names, name)
		}
		sort.Strings(names)
		for _, name := range names {
			s.say("%s: %v", name, p[name].Colors)
		}
		return
	}
	p, ok := palettes()[s.opts.Palette]
	if !ok {
		panic(probeFailure{fmt.Errorf("unknown palette %q", s.opts.Palette)})
	}
	original := map[int]string{}
	changed := false
	s.cleanup(func() {
		if changed {
			for i := range p.Colors {
				if original[i] != "" {
					paletteSet(s, i, original[i])
				} else {
					paletteReset(s, i)
				}
			}
		}
	})
	for i := range p.Colors {
		original[i] = paletteQuery(s, i)
	}
	s.say("Unreadable entries reset to configured defaults on exit. Keep query/set policy stable through cleanup.")
	changed = true
	for i, color := range p.Colors {
		paletteSet(s, i, color)
	}
	s.say("Requested palette %s", s.opts.Palette)
	colorSamples(s)
}
func colorSamples(s *Session) {
	s.cleanup(func() { s.send(esc + "[0m") })
	s.page("SGR styles")
	for _, style := range []int{0, 1, 2, 3, 4, 5, 7, 8, 9, 53} {
		s.say("%s[%dmSGR %d: AaBb gjpq 0123%s[0m", esc, style, style, esc)
	}
	s.pause()
	s.page("16 colors: normal and bold on indexed backgrounds")
	for fg := 0; fg < 16; fg++ {
		for bg := 0; bg < 8; bg++ {
			s.send(fmt.Sprintf("%s[38;5;%d;48;5;%dm Aa %s[1mBb%s[0m", esc, fg, bg, esc, esc))
		}
		s.say("")
	}
	s.pause()
	s.page("256 indexed colors")
	cols, rows := terminalSize(s.out)
	perRow := max(1, min(16, (cols-1)/4))
	perPage := max(1, rows-5) * perRow
	for i := 0; i < 256; i++ {
		s.send(fmt.Sprintf("%s[48;5;%dm%3d %s[0m", esc, i, i, esc))
		if (i+1)%perRow == 0 {
			s.say("")
		}
		if (i+1)%perPage == 0 && i < 255 {
			s.pause()
			s.page("256 indexed colors (continued)")
		}
	}
	s.say("")
	s.pause()
	s.page("Truecolor gradients")
	width, _ := terminalSize(s.out)
	width = min(width-2, 120)
	if width < 1 {
		width = 1
	}
	for _, sep := range []string{";", ":"} {
		s.say("Truecolor (%q separators): red/green, green/blue, blue/red", sep)
		for band := 0; band < 3; band++ {
			for x := 0; x < width; x++ {
				v := 255 * x / max(1, width-1)
				rgb := []int{0, 0, 0}
				rgb[band] = v
				rgb[(band+1)%3] = 255 - v
				code := fmt.Sprintf("48;2;%d;%d;%d", rgb[0], rgb[1], rgb[2])
				if sep == ":" {
					code = fmt.Sprintf("48:2::%d:%d:%d", rgb[0], rgb[1], rgb[2])
				}
				s.send(esc + "[" + code + "m ")
			}
			s.say(esc + "[0m")
		}
	}
	s.pause()
}
func reverseVideo(s *Session) {
	s.preserveModes(5)
	s.cleanup(func() { s.send(esc + "[0m") })
	s.send(modeSequence(5, false))
	s.say("Normal default colors")
	s.say(esc + "[7mSGR 7 inverse defaults" + esc + "[0m")
	s.say(esc + "[38;2;255;128;0mExplicit orange must remain orange" + esc + "[0m")
	s.say("This is the untouched marker. Inspect these existing rows after DECSCNM.")
	s.pause()
	s.send(modeSequence(5, true))
	s.say("DECSCNM enabled without redrawing old rows.")
	s.pause()
	s.send(modeSequence(5, false))
	s.say("DECSCNM reset. Toggle widget Reverse Video manually, then restore it.")
	s.pause()
}
func spawnArgs(o Options, exe string) ([]string, error) {
	p, ok := palettes()[o.Palette]
	if !ok {
		return nil, fmt.Errorf("unknown palette %q", o.Palette)
	}
	args := []string{"-geometry", o.Geometry}
	for i, c := range p.Colors {
		args = append(args, "-xrm", fmt.Sprintf("XTerm*color%d: %s", i, c))
	}
	for _, resource := range []string{"foreground: " + p.FG, "background: " + p.BG, "cursorColor: " + p.Cursor} {
		args = append(args, "-xrm", "XTerm*"+resource)
	}
	for _, xrm := range o.XRM {
		args = append(args, "-xrm", xrm)
	}
	args = append(args, "-e", exe, "colors", "samples")
	return args, nil
}
func spawnPalette(o Options) int {
	exe, err := os.Executable()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	args, err := spawnArgs(o, exe)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	cmd := exec.Command(o.Program, args...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err = cmd.Run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	return 0
}
