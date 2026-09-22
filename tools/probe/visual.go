package main

import (
	"bytes"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"
)

func underline(s *Session) {
	s.page("Underline styles and colors")
	s.cleanup(func() { s.send(esc + "[0m") })
	for style := 1; style <= 5; style++ {
		for _, color := range []string{"58;2;255;40;40", "58;2;40;120;255", "58;5;46"} {
			s.say("%s[4:%d;%smUnderline style %d: AaBb gjpq 界%s[59m default color%s[0m", esc, style, color, style, esc, esc)
		}
	}
	s.pause()
}
func pointer(s *Session) {
	s.cleanup(func() { s.osc(22, "default") })
	for _, shape := range []string{"text", "pointer", "crosshair", "wait", "default"} {
		s.osc(22, shape)
		s.say("Requested pointer shape %s; move over the grid.", shape)
		s.pause()
	}
}
func glyphs(s *Session) {
	s.page("Glyph samples")
	for _, line := range []string{"┌────────┬────────┐  ╔════════╦════════╗", "│ normal │ boxes  │  ║ double ║ boxes  ║", "├────────┼────────┤  ╠════════╬════════╣", "└────────┴────────┘  ╚════════╩════════╝", "▁▂▃▄▅▆▇█ ▉▊▋▌▍▎▏ ▀▄▌▐░▒▓█"} {
		s.say("%s", line)
	}
	s.cleanup(func() { s.send(esc + "[0m") })
	s.say("%s", "Braille dots 1-8: ⠁⠂⠄⠈⠐⠠⡀⢀  full ⣿  columns ⡇⢸  blank [⠀]  graph ⣀⣤⣶⣿⣷⣦⣄⣀")
	s.say("%s", "Powerline U+E0B0-U+E0BF: \ue0b0\ue0b1\ue0b2\ue0b3 \ue0b4\ue0b5\ue0b6\ue0b7 \ue0b8\ue0b9\ue0ba\ue0bb \ue0bc\ue0bd\ue0be\ue0bf")
	s.say("%s", esc+"[30;42m main "+esc+"[32;44m\ue0b0"+esc+"[30;44m status "+esc+"[34;41m\ue0b4"+esc+"[30;41m warn "+esc+"[31;49m\ue0b8"+esc+"[0m  "+esc+"[35;49m\ue0b2"+esc+"[30;45m right \ue0b3 end "+esc+"[0;35m\ue0b4"+esc+"[0m")
	s.say("%s", "Font-owned, outside the drawn range: \ue0a0 \ue0a2 \ue0c0 \ue0d2")
	s.say("Check joins at multiple sizes, bitmap/Xft, bold, inverse, selection and the cursor, and forceBoxChars off/on.")
	s.say("Braille dots are equal squares in two columns; Powerline shapes span the cell height and meet segment colors without a seam.")
	s.pause()
}

type codepointRange struct {
	first rune
	last  rune
}

type proceduralGlyphGroup struct {
	id       string
	title    string
	note     string
	examples []string
	ranges   []codepointRange
	chars    []rune
}

var proceduralGlyphGroups = []proceduralGlyphGroup{
	{id: "box", title: "Box Drawing", note: "Frames, junctions, mixed weights, rounded corners, dashes and diagonals.", examples: []string{
		"Light:   ┌────────┬────────┐", "         │        │        │", "         ├────────┼────────┤", "         │        │        │", "         └────────┴────────┘",
		"Heavy:   ┏━━━━━━━━┳━━━━━━━━┓", "         ┣━━━━━━━━╋━━━━━━━━┫", "         ┗━━━━━━━━┻━━━━━━━━┛",
		"Double:  ╔════════╦════════╗", "         ╠════════╬════════╣", "         ╚════════╩════════╝",
		"Other:   ╭────╮  ┌┄┄┄┄┐  ╲╱ ╳",
	}, ranges: []codepointRange{{0x2500, 0x257f}}},
	{id: "block", title: "Block Elements and shades", note: "Bars, fills, shades and quadrant mosaics.", examples: []string{
		"Horizontal fractions: ▏▎▍▌▋▊▉█", "Vertical fractions:   ▁▂▃▄▅▆▇█", "Solid cell seam:      ████████████████",
		"Shade ramp:           ░░▒▒▓▓██", "Quadrant tiles:       ▖▗ ▘▝  ▙▟ ▛▜  ▚▞",
	}, ranges: []codepointRange{{0x2580, 0x259f}}},
	{id: "braille", title: "Braille patterns", note: "A two-by-four dot matrix used for text, plots and dense terminal graphics.", examples: []string{
		"Individual dots: ⠁ ⠂ ⠄ ⡀   ⠈ ⠐ ⠠ ⢀", "Columns and fill: ⡇ ⢸ ⣿", "Plot-like chain:  ⡀⡄⡆⡇⣇⣧⣷⣿⣶⣤⣀",
	}, ranges: []codepointRange{{0x2800, 0x28ff}}},
	{id: "powerline", title: "Powerline separators", note: "Private-use separators intended to join colored status-line segments.", examples: []string{
		"Filled arrows:  left ████ right", "Thin arrows:    left ──── right", "Slants:         ███    ███", "Half circles:   ███    ███", "Extra flame:    ████",
	}, ranges: []codepointRange{{0xe0b0, 0xe0bf}}, chars: []rune{0xe0d2, 0xe0d4}},
	{id: "geometric", title: "Geometric terminal graphics", note: "Triangles, diagonals, a diamond and a vertical rectangle.", examples: []string{
		"Triangles: ◢◣  ◥◤  ◸◹◺  ◿", "DEC shapes: ◆ ▮",
	}, chars: []rune{0x25ae, 0x25c6, 0x25e2, 0x25e3, 0x25e4, 0x25e5, 0x25f8, 0x25f9, 0x25fa, 0x25ff}},
	{id: "scanline", title: "DEC horizontal scan lines", note: "Four vertical positions used with the ordinary box-drawing horizontal line.", examples: []string{
		"Levels: ⎺⎺⎺⎺  ⎻⎻⎻⎻  ────  ⎼⎼⎼⎼  ⎽⎽⎽⎽",
	}, ranges: []codepointRange{{0x23ba, 0x23bd}}},
	{id: "controls", title: "DEC control pictures", note: "Printable pictures for control functions; these do not join across cells.", examples: []string{
		"Control pictures: ␉ ␊ ␋ ␌ ␍ ␤",
	}, chars: []rune{0x2409, 0x240a, 0x240b, 0x240c, 0x240d, 0x2424}},
	{id: "symbols", title: "DEC mathematical and other symbols", note: "Non-joining compatibility members of the DEC Special Graphics repertoire.", examples: []string{
		"Symbols: ° ± · £ π ≠ ≤ ≥",
	}, chars: []rune{0x00b0, 0x00b1, 0x00b7, 0x00a3, 0x03c0, 0x2260, 0x2264, 0x2265}},
	{id: "legacy-sextants", title: "Legacy Computing: sextant mosaics", note: "Every nonempty, nonfull combination of a two-by-three cell grid.", examples: []string{
		"Density samples: 🬀 🬂 🬆 🬎 🬝 🬫 🬺", "Tiled strip:     🬀🬂🬆🬎🬝🬫🬺",
	}, ranges: []codepointRange{{0x1fb00, 0x1fb3b}}},
	{id: "legacy-smooth", title: "Legacy Computing: smooth mosaics", note: "Diagonal wedges and triangular fractional blocks for tiled graphics.", examples: []string{
		"Wedges: 🬼🭁🭌🭒  🭗🭢🭨🭭", "Fractions: 🭬 🭭 🭮 🭯",
	}, ranges: []codepointRange{{0x1fb3c, 0x1fb6f}}},
	{id: "legacy-eighths", title: "Legacy Computing: eighth blocks and fills", note: "Fine horizontal/vertical fractions, split shades and diagonal fills.", examples: []string{
		"Vertical eighths:   🭰🭱🭲🭳🭴🭵", "Horizontal eighths: 🭶🭷🭸🭹🭺🭻", "Fills:              🮌🮍🮎🮏 🮐🮕🮘🮙",
	}, ranges: []codepointRange{{0x1fb70, 0x1fb92}, {0x1fb94, 0x1fb9b}}},
	{id: "legacy-extra", title: "Legacy Computing: lines, circles and extra mosaics", note: "Additional standard diagonals, box pieces, partial circles and blocks.", examples: []string{
		"Diagonal pieces: 🮠🮡🮢🮣 🮽🮾🮿", "Circle pieces:   🯠🯡🯢🯣 🯨🯩🯪🯫", "Fraction blocks: 🯎 🯏",
	}, ranges: []codepointRange{{0x1fb9c, 0x1fbaf}, {0x1fbbd, 0x1fbbf}, {0x1fbce, 0x1fbef}}},
	{id: "supplement-quadrants", title: "Legacy Supplement: separated quadrants", note: "Quadrant mosaics with visible gutters between their pieces.", examples: []string{
		"Separated quadrants: 𜰡𜰢𜰣𜰤 𜰥𜰦𜰧 𜰨𜰩𜰪𜰫 𜰬𜰭𜰮𜰯",
	}, ranges: []codepointRange{{0x1cc21, 0x1cc2f}}},
	{id: "supplement-circles", title: "Legacy Supplement: circle pieces", note: "Twelve pieces that form edge-touching circles across a four-by-four cell area.", examples: []string{
		"𜰰𜰱𜰲𜰳", "𜰴𜰵𜰶𜰷", "𜰸𜰹𜰺𜰻", "𜰼𜰽𜰾𜰿",
	}, ranges: []codepointRange{{0x1cc30, 0x1cc3f}}},
	{id: "supplement-octants", title: "Legacy Supplement: octant mosaics", note: "Combinations of a two-by-four cell grid: a denser counterpart to sextants.", examples: []string{
		"Density samples: 𜴀 𜴂 𜴎 𜴟 𜵿 𜷥", "Tiled strip:     𜴀𜴂𜴎𜴟𜵿𜷥",
	}, ranges: []codepointRange{{0x1cd00, 0x1cde5}}},
	{id: "supplement-separated", title: "Legacy Supplement: separated sextants and sixteenths", note: "Guttered sextants followed by individual sixteenth-cell pieces.", examples: []string{
		"Separated sextants: 𝹑𝹓𝹗𝹟𝺏", "Sixteenth pieces:   𝺐𝺓𝺗𝺛𝺟",
	}, ranges: []codepointRange{{0x1ce51, 0x1ceaf}}},
	{id: "supplement-extra", title: "Legacy Supplement: extra boxes and ellipses", note: "A small set of line junctions, split circles and ellipse halves.", examples: []string{
		"Box pieces: 𜰛 𜰝 𜰞", "Circle/ellipse pieces: 𝸀 𝸁 𝸋 𝸌 𝸖𝸗𝸘𝸙",
	}, ranges: []codepointRange{{0x1cc1b, 0x1cc1e}, {0x1ce00, 0x1ce01}, {0x1ce0b, 0x1ce0c}, {0x1ce16, 0x1ce19}}},
	{id: "branch", title: "Private branch symbols", note: "A private-use icon range; meanings depend on the font/project that assigned it.", examples: []string{
		"Private-use catalog follows; missing-font boxes are expected elsewhere.",
	}, ranges: []codepointRange{{0xf5d0, 0xf60d}}},
}

func groupCodepoints(group proceduralGlyphGroup) []rune {
	var result []rune
	for _, span := range group.ranges {
		for r := span.first; r <= span.last; r++ {
			result = append(result, r)
		}
	}
	result = append(result, group.chars...)
	return result
}

func proceduralGlyphs(s *Session) {
	selected := ""
	if s.result != nil {
		selected = strings.TrimPrefix(s.result.CaseID, "text-procedural-")
	}
	groups := proceduralGlyphGroups
	if selected != "union" {
		groups = nil
		for _, group := range proceduralGlyphGroups {
			if group.id == selected {
				groups = append(groups, group)
				break
			}
		}
	}
	columns, rows := terminalSize(s.out)
	entriesPerLine := max(1, columns/13)
	linesPerPage := max(1, rows-5)
	for _, group := range groups {
		s.page(group.title + " / constructions")
		s.say("%s", group.note)
		s.say("")
		for _, example := range group.examples {
			s.say("%s", example)
		}
		s.pause()

		codepoints := groupCodepoints(group)
		perPage := entriesPerLine * linesPerPage
		pages := (len(codepoints) + perPage - 1) / perPage
		for page := 0; page < pages; page++ {
			s.page(fmt.Sprintf("%s / code points (%d/%d)", group.title, page+1, pages))
			s.say("Each entry is U+codepoint followed by the glyph.")
			end := min(len(codepoints), (page+1)*perPage)
			pageCodepoints := codepoints[page*perPage : end]
			for line := 0; line < len(pageCodepoints); line += entriesPerLine {
				lineEnd := min(len(pageCodepoints), line+entriesPerLine)
				var entries []string
				for _, r := range pageCodepoints[line:lineEnd] {
					entries = append(entries, fmt.Sprintf("U+%04X [%s]", r, string(r)))
				}
				s.say("%s", strings.Join(entries, "  "))
			}
			s.pause()
		}
	}
}
func copyFixture(s *Session) {
	s.say("COPY-PROBE alpha café 界 omega COPY-END")
	s.say("Start with -xrm 'XTerm*copyFlashDuration: 300' and optionally copyFlashColor.")
	s.say("Select and copy; inspect the flash, its expiry, the selection and exact paste contents.")
	s.pause()
}

// The M4 study corpus, dark on light then light on dark; run it at several -fs sizes.
func textContrast(s *Session) {
	s.page("Fractional-size and contrast sample")
	s.cleanup(func() { s.send(esc + "[0m") })
	lines := []string{
		"Hamburgefonstiv 0Oo1lI|{}[]",
		"cafe\u0301 n\u0303 a\u0308 q\u0323\u0307 e\u0302\u0323",
		esc + "[3mHamburgefonstiv 0Oo1lI|{}[]" + esc + "[23m",
		" \U0001f6e0 \U0001f6e0\ufe0e x\U0001f6e0x",
	}
	for _, theme := range []struct{ label, sgr string }{
		{"dark on light", esc + "[30;107m"}, {"light on dark", esc + "[97;40m"},
	} {
		s.say("%s:", theme.label)
		for _, line := range lines {
			s.say("%s %-28s %s", theme.sgr, line, esc+"[0m")
		}
	}
	s.say("Ordinary text, combining marks, the italic face and a fitted text-presentation")
	s.say("\U0001f6e0 (U+1F6E0, not emoji-presentation). Compare stroke weight between the two")
	s.say("blocks, mark placement, and whether \U0001f6e0 stays inside its cell. Repeat with")
	s.say("fractional sizes (for example -fs 7, 7.5, 8.25) and note the reported cell size.")
	s.say("This is a human visual assessment; the measured study is")
	s.say("docs/maintainers/linear-light-study.md.")
	s.pause()
}
func dragScrollFixture(s *Session) {
	s.preserveModes(2004)
	for n := 1; n <= 300; n++ {
		s.say("line %04d", n)
	}
	s.say("Drag across scrolling. Watching the highlight is a human visual assessment; the")
	s.say("paste check is exact. After each drag, paste the selection here (middle-click):")
	s.say(" 1. Press on a line, drag, turn the wheel while holding the button, drag on, release.")
	s.say(" 2. Drag past the top or bottom edge until the view autoscrolls, come back, release.")
	s.say(" 3. Repeat both dragging the other way.")
	s.say("Each paste should run without gaps from where the drag began to where it ended.")
	s.say("q/Esc: exit test | Space/Enter: finish")
	s.send(modeSequence(2004, true))
	var reader pasteReader
	deadline := time.Now().Add(seconds(s.opts.Seconds))
	for time.Now().Before(deadline) {
		data := s.read(minTime(deadline, time.Now().Add(100*time.Millisecond)))
		if len(data) > 0 {
			s.event("input", data)
		}
		events := reader.feed(data)
		if len(data) == 0 {
			events = append(events, pasteEvent{keys: reader.idle()})
		}
		for _, event := range events {
			if event.paste {
				s.say("%s", numberedPaste(event.text))
			} else if s.navigation(event.keys) {
				return
			}
		}
	}
}

var (
	pasteStart = []byte(esc + "[200~")
	pasteEnd   = []byte(esc + "[201~")
)

// Keys and bracketed pastes in arrival order; either delimiter may be split across reads.
type pasteEvent struct {
	paste bool
	keys  []byte
	text  string
}
type pasteReader struct {
	pending []byte
	body    []byte
	inside  bool
}

func (r *pasteReader) feed(data []byte) []pasteEvent {
	var events []pasteEvent
	r.pending = append(r.pending, data...)
	for {
		if r.inside {
			if i := bytes.Index(r.pending, pasteEnd); i >= 0 {
				events = append(events, pasteEvent{paste: true, text: string(append(r.body, r.pending[:i]...))})
				r.body, r.inside = nil, false
				r.pending = r.pending[i+len(pasteEnd):]
				continue
			}
			keep := delimiterPrefix(r.pending, pasteEnd)
			r.body = append(r.body, r.pending[:len(r.pending)-keep]...)
			r.pending = append([]byte(nil), r.pending[len(r.pending)-keep:]...)
			return events
		}
		if i := bytes.Index(r.pending, pasteStart); i >= 0 {
			if i > 0 {
				events = append(events, pasteEvent{keys: append([]byte(nil), r.pending[:i]...)})
			}
			r.inside = true
			r.pending = r.pending[i+len(pasteStart):]
			continue
		}
		keep := delimiterPrefix(r.pending, pasteStart)
		if len(r.pending) > keep {
			events = append(events, pasteEvent{keys: append([]byte(nil), r.pending[:len(r.pending)-keep]...)})
		}
		r.pending = append([]byte(nil), r.pending[len(r.pending)-keep:]...)
		return events
	}
}

// After a quiet read a held partial opener is ordinary input, so a lone Esc still exits.
func (r *pasteReader) idle() []byte {
	if r.inside {
		return nil
	}
	held := r.pending
	r.pending = nil
	return held
}

// The length of the longest suffix of data that begins delimiter.
func delimiterPrefix(data, delimiter []byte) int {
	for n := min(len(data), len(delimiter)-1); n > 0; n-- {
		if bytes.HasSuffix(data, delimiter[:n]) {
			return n
		}
	}
	return 0
}

var (
	numberedLine = regexp.MustCompile(`^line ([0-9]{4})$`)
	lineBreak    = regexp.MustCompile("\r\n|\r|\n")
)

// Only the two ends of a paste may be partial; every interior line must be a complete
// numbered line, and the complete lines must be consecutive.
func numberedPaste(text string) string {
	if text == "" {
		return "Pasted nothing."
	}
	lines := lineBreak.Split(text, -1)
	last := len(lines) - 1
	summary := fmt.Sprintf("Pasted %d line(s), first %q, last %q", len(lines), lines[0], lines[last])
	var numbers []int
	for index, line := range lines {
		m := numberedLine.FindStringSubmatch(line)
		if m == nil {
			if index != 0 && index != last {
				return fmt.Sprintf("%s: MALFORMED interior line %d: %q.", summary, index+1, line)
			}
			continue
		}
		number, _ := strconv.Atoi(m[1])
		if len(numbers) > 0 && number != numbers[len(numbers)-1]+1 {
			return fmt.Sprintf("%s: NOT consecutive, line %04d follows %04d.", summary, number, numbers[len(numbers)-1])
		}
		numbers = append(numbers, number)
	}
	if len(numbers) < 2 {
		return summary + ": insufficient evidence, fewer than two complete lines to compare."
	}
	return fmt.Sprintf("%s: complete lines %04d..%04d consecutive. Whether the ends match where the drag began and ended is your assessment.",
		summary, numbers[0], numbers[len(numbers)-1])
}
func searchFixture(s *Session) {
	width, _ := terminalSize(s.out)
	for n := 0; n < 120; n++ {
		value := "ordinary scrollback text"
		if n%17 == 0 {
			value = "FIND-ME café 界 e\u0301"
		}
		s.say("row %03d %s", n, value)
	}
	s.say("%sWRAPPED-NEEDLE", strings.Repeat("x", max(1, width-4)))
	s.say("Press Ctrl+Shift+F; search FIND-ME, café, a missing string and WRAPPED-NEEDLE.")
	s.say("Up/Down move between matches, Enter copies the match to PRIMARY, Escape restores the viewport.")
	s.pause()
}
func links(s *Session) {
	s.page("Hyperlinks")
	s.cleanup(func() { s.osc(8, ";"); s.send(esc + "[0m") })
	link := func(uri, label string) { s.osc(8, ";"+uri); s.send(label); s.osc(8, ";") }
	s.say("Local default bindings: Shift-hover decorates links; Shift+Button 1 opens HTTP(S) only.")
	for _, uri := range []string{"http://example.com", "https://example.com/path?q=probe", "mailto:nobody@example.com", "file:///tmp/probe-osc8"} {
		link(uri, uri)
		s.say("")
	}
	s.say("Autolinks: http://example.com/path. https://example.com/a_(b).")
	s.say("Not URLs: example.com and mailto:nobody@example.com")
	for style := 1; style <= 5; style++ {
		s.send(fmt.Sprintf("%s[4:%dm", esc, style))
		link("https://example.com/underlined", "underlined OSC 8 label")
		s.say("  https://example.com/underlined%s[0m", esc)
	}
	s.osc(8, ";https://example.com/mixed")
	s.say("plain / %s[4munderlined%s[24m / plain", esc, esc)
	s.osc(8, ";")
	s.say("https://example.com/auto-%s[4mmixed%s[24m-underline", esc, esc)
	s.pause()
}
func syncOutput(s *Session) {
	width, rows := terminalSize(s.out)
	if width < 24 || rows < 8 {
		panic(probeFailure{fmt.Errorf("sync probe requires at least 24 columns by 8 rows")})
	}
	s.preserveModes(25, 2026)
	alt := s.mode(1049)
	if alt != 1 && alt != 3 {
		s.cleanup(func() { s.send(esc + "[?1049l") })
		s.send(esc + "[?1049h")
	}
	s.cleanup(func() { s.send(modeSequence(2026, false) + esc + "[0m") })
	s.send(modeSequence(2026, false) + modeSequence(25, false))
	modes := []string{s.opts.Mode}
	if s.opts.Mode == "compare" {
		modes = []string{"off", "on", "boundaries"}
	}
	line := func(row int, text, style string, width int) {
		if len(text) > width {
			text = text[:width]
		}
		s.send(fmt.Sprintf("%s[%d;1H%s[0m%s[2K%s%s%s[0m", esc, row, esc, esc, style, text, esc))
	}
	for _, mode := range modes {
		if mode == "boundaries" {
			syncBoundaries(s, width)
			continue
		}
		s.send(modeSequence(2026, false) + esc + "[0m" + esc + "[2J" + esc + "[H")
		for frame := 1; frame <= s.opts.Frames; frame++ {
			width, rows = terminalSize(s.out)
			width--
			bottom := rows - 1
			if width < 23 || bottom < 7 {
				panic(probeFailure{fmt.Errorf("window became too small for sync probe")})
			}
			line(1, "Synchronized output requested: "+strings.ToUpper(mode), "", width)
			expected := "Expect a sweep: old and new rows mix."
			if mode == "on" {
				expected = "Expect complete-frame swaps; all rows match."
			}
			line(2, expected, "", width)
			line(3, fmt.Sprintf("Draw %d ms; mid-frame hold %d ms", s.opts.FrameMS, s.opts.HoldMS), "", width)
			line(4, "q/Esc: exit test | Resize tests held repaint.", "", width)
			body := bottom - 4
			label := fmt.Sprintf("frame %03d ", frame)
			trackWidth := max(1, width-len(label))
			pos := frame * 4 % trackWidth
			track := strings.Repeat(".", pos) + "|" + strings.Repeat(".", trackWidth-pos-1)
			style := esc + "[30;46m"
			if frame%2 == 0 {
				style = esc + "[30;43m"
			}
			if mode == "on" {
				s.send(modeSequence(2026, true))
			}
			for row := 0; row < body; row++ {
				line(row+5, label+track, style, width)
				s.wait(time.Duration(s.opts.FrameMS) * time.Millisecond / time.Duration(body))
				if row == body/2 {
					s.wait(time.Duration(s.opts.HoldMS) * time.Millisecond)
				}
			}
			s.send(modeSequence(2026, false))
			s.wait(time.Duration(s.opts.PauseMS) * time.Millisecond)
		}
		s.send(fmt.Sprintf("%s[%d;1H", esc, rows))
		s.pause()
	}
}

// Mode 2026 transitions inside one write and across consecutive writes. Each scenario
// is labelled with what should be on screen while it holds. This is a visual check:
// a PTY write is not guaranteed to reach the terminal's parser as one batch, nor two
// writes as two, so a passing look is not proof of batch-boundary handling -- the
// deterministic check feeds the parser directly (the xvfb-sync-output suite).
func syncBoundaries(s *Session, width int) {
	hold := time.Duration(s.opts.HoldMS) * time.Millisecond
	at := func(row int, text string) string {
		if len(text) > width-1 {
			text = text[:width-1]
		}
		return fmt.Sprintf("%s[%d;1H%s[2K%s", esc, row, esc, text)
	}
	clear := modeSequence(2026, false) + esc + "[0m" + esc + "[2J" + esc + "[H"
	title := func(name string) string {
		return at(1, "Mode 2026 boundaries: "+name) +
			at(2, "Visual only; the parser may split or merge writes.")
	}

	s.send(clear + title("[same write] output, then hold") +
		at(4, "VISIBLE: completed before the hold") +
		modeSequence(2026, true) + at(6, "HIDDEN until release: written after the hold"))
	s.wait(hold)
	s.send(modeSequence(2026, false))
	s.wait(time.Duration(s.opts.PauseMS) * time.Millisecond)

	for _, label := range []string{
		"[same write] release, new frame, hold again",
		"[consecutive writes] release, new frame, hold again",
	} {
		frame := title(label) + at(4, "VISIBLE: frame completed between the holds") +
			at(5, "Expect no OLD line and no HIDDEN line while held.")
		rehold := modeSequence(2026, true) + at(6, "HIDDEN until release: after the second hold")
		s.send(clear + at(4, "OLD: must be replaced") + modeSequence(2026, true) + esc + "[2J" +
			at(7, "HIDDEN: half-drawn in the first hold"))
		if strings.HasPrefix(label, "[same write]") {
			s.send(modeSequence(2026, false) + esc + "[2J" + frame + rehold)
		} else {
			s.send(modeSequence(2026, false))
			s.send(esc + "[2J" + frame)
			s.send(rehold)
		}
		s.wait(hold)
		s.send(modeSequence(2026, false))
		s.wait(time.Duration(s.opts.PauseMS) * time.Millisecond)
	}
	s.send(fmt.Sprintf("%s[%d;1H", esc, 8))
	s.pause()
}
