package main

import (
	"fmt"
	"regexp"
	"slices"
	"strconv"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"
)

// The browser borrows the terminal only while a chooser is visible. Probe
// handlers run with the caller's original modes and without a UI input reader.
type Browser struct {
	s             *Session
	original      map[int]int
	active        bool
	pending       []byte
	crumbs        []breadcrumb
	selecting     bool
	textSelectRow int
	escapeSince   time.Time
}

var browserModes = []int{9, 1000, 1002, 1003, 1004, 1005, 1006, 1015, 1016, 25, 1049}

func newBrowser(s *Session) *Browser {
	b := &Browser{s: s, original: map[int]int{}}
	previous := s.quiet
	s.quiet = true
	defer func() { s.quiet = previous }()
	for _, m := range browserModes {
		b.original[m] = s.mode(m)
	}
	return b
}
func (b *Browser) enter() {
	if b.active {
		return
	}
	b.active = true
	if b.original[1049] != 1 && b.original[1049] != 3 {
		b.s.send(modeSequence(1049, true))
	}
	for _, m := range browserModes {
		if m != 25 && m != 1049 {
			b.s.send(modeSequence(m, false))
		}
	}
	b.s.send(modeSequence(1000, true) + modeSequence(1006, true) + modeSequence(25, false) + esc + "[0m" + esc + "[2J")
	b.pending = nil
	b.escapeSince = time.Time{}
}
func (b *Browser) leave() {
	if !b.active {
		return
	}
	b.active = false
	b.selecting = false
	// Cleanup must still run after SIGTERM or Ctrl+C cancels the session.
	cleaning := b.s.cleaning
	b.s.cleaning = true
	defer func() { b.s.cleaning = cleaning }()
	for _, m := range browserModes {
		if m != 25 && m != 1049 {
			b.s.send(modeSequence(m, false))
		}
	}
	for _, m := range browserModes {
		state := b.original[m]
		if m == 1049 {
			continue
		}
		if state == 1 || state == 3 || state == 0 && m == 25 {
			b.s.send(modeSequence(m, true))
		}
		if m == 25 && (state == 2 || state == 4) {
			b.s.send(modeSequence(m, false))
		}
	}
	b.s.send(esc + "[0m")
	if b.original[1049] != 1 && b.original[1049] != 3 {
		b.s.send(modeSequence(1049, false))
	}
	b.pending = nil
}

const menuFooter = "F2 Select text  ↑↓ Select  Space/Enter Open  q/Esc Back"

const browserBase = esc + "[0;37;40m"

type menuEntry struct{ Label, Detail string }
type menuLayout struct {
	Width, Height, ListWidth, Top, Bottom, Footer int
	Split                                         bool
}

func layoutMenu(width, height int) menuLayout {
	return menuLayout{Width: width, Height: height, ListWidth: min(34, width), Top: 5, Bottom: max(5, height-5), Footer: height - 2, Split: width >= 70 && height >= 15}
}
func plainText(text string) string {
	return strings.Map(func(r rune) rune {
		if unicode.IsControl(r) {
			return ' '
		}
		return r
	}, text)
}

// Chrome is ASCII; allow a conservative two cells for non-ASCII text so
// option values cannot overwrite an adjacent pane. This is a UI clipping bound,
// not a terminal width oracle (the width probes use their separate accept sets).
func menuTextWidth(text string) int {
	width := 0
	for _, r := range text {
		if unicode.Is(unicode.Mn, r) || unicode.Is(unicode.Me, r) || unicode.Is(unicode.Cf, r) {
			continue
		}
		if r > 127 {
			width += 2
		} else {
			width++
		}
	}
	return width
}
func clipText(text string, width int) string {
	text = plainText(text)
	if width <= 0 {
		return ""
	}
	if menuTextWidth(text) <= width {
		return text
	}
	suffix := "…"
	reserve := 2
	if width == 1 {
		suffix = "."
		reserve = 1
	}
	out := ""
	for _, r := range text {
		next := out + string(r)
		if menuTextWidth(next) > width-reserve {
			break
		}
		out = next
	}
	return out + suffix
}

func wrapped(text string, width int) []string {
	out := []string{}
	for _, paragraph := range strings.Split(text, "\n") {
		line := ""
		for _, word := range strings.Fields(paragraph) {
			if menuTextWidth(line)+menuTextWidth(word)+1 > width && line != "" {
				out = append(out, line)
				line = ""
			}
			if line != "" {
				line += " "
			}
			line += word
		}
		out = append(out, line)
	}
	return out
}
func (b *Browser) draw(title string, entries []menuEntry, selected, offset int, typed string) menuLayout {
	width, height := terminalSize(b.s.out)
	l := layoutMenu(width, height)
	var frame strings.Builder
	put := func(row, col, width int, text, style string) {
		if row < 1 || row > height || col > l.Width {
			return
		}
		text = clipText(text, width)
		fmt.Fprintf(&frame, "%s[%d;%dH%s%s%s", esc, row, col, style, text, browserBase)
	}
	frame.WriteString(esc + "[H" + browserBase + esc + "[2J")
	if width < 32 || height < 10 {
		put(1, 1, width, "Terminal probe", esc+"[1;36m")
		put(3, 1, width, "Resize to at least 32 x 10", "")
		put(5, 1, width, "Esc / q: back", "")
		b.s.send(frame.String())
		return l
	}
	put(1, 1, width, strings.Repeat(" ", width), esc+"[44m")
	put(1, 3, width-4, "TERMINAL PROBE", esc+"[1;97;44m")
	b.crumbs = breadcrumbLayout(title, width-4)
	for i, crumb := range b.crumbs {
		style := esc + "[1;36m"
		if crumb.Active {
			style = esc + "[4;36m"
		}
		put(2, crumb.Start, crumb.End-crumb.Start+1, crumb.Label, style)
		if i < len(b.crumbs)-1 {
			put(2, crumb.End+1, 3, " / ", esc+"[90m")
		}
	}
	put(3, 3, width-4, "Click an underlined ancestor. Help shows full details.", esc+"[90m")
	listWidth := width - 4
	if l.Split {
		listWidth = l.ListWidth - 3
		put(4, l.ListWidth+3, width-l.ListWidth-4, "DETAILS", esc+"[1;36m")
	}
	for row, index := l.Top, offset; row <= l.Bottom && index < len(entries); row, index = row+1, index+1 {
		style := esc + "[37m"
		pointer := "  "
		if index == selected {
			style = esc + "[1;30;46m"
			pointer = "> "
		}
		label := fmt.Sprintf("%s%2d  %s", pointer, index+1, entries[index].Label)
		if index == selected {
			label = clipText(label, listWidth)
			label += strings.Repeat(" ", max(0, listWidth-menuTextWidth(label)))
		}
		put(row, 2, listWidth, label, style)
	}
	if l.Split && len(entries) > 0 {
		for row := 4; row < height-3; row++ {
			put(row, l.ListWidth, 1, "|", esc+"[90m")
		}
		detail := entries[selected].Detail
		for i, line := range wrapped(detail, width-l.ListWidth-5) {
			if i >= height-8 {
				break
			}
			put(5+i, l.ListWidth+3, width-l.ListWidth-4, line, esc+"[37m")
		}
	}
	if !l.Split && len(entries) > 0 && len(entries) < l.Bottom-l.Top-1 {
		row := l.Top + len(entries) + 1
		for _, line := range wrapped(entries[selected].Detail, width-4) {
			if row > l.Bottom {
				break
			}
			put(row, 3, width-4, line, esc+"[90m")
			row++
		}
	}
	status := fmt.Sprintf("%d / %d", selected+1, len(entries))
	if typed != "" {
		status = "Jump: " + typed + "  (Enter to open)"
	}
	if offset > 0 || offset+l.Bottom-l.Top+1 < len(entries) {
		status += "  |  scroll for more"
	}
	put(height-3, 3, width-4, status, esc+"[90m")
	put(l.Footer, 1, width, strings.Repeat(" ", width), esc+"[44m")
	b.textSelectRow = l.Footer
	put(l.Footer, 2, width-3, menuFooter, esc+"[97;44m")
	put(height-1, 2, width-3, "Number/name + Enter  Wheel Scroll  Click Open  q exits at Home", esc+"[90m")
	b.s.send(frame.String())
	return l
}

type menuEvent struct {
	Kind, Text   string
	X, Y, Button int
}

var sgrMouse = regexp.MustCompile(`^\x1b\[<([0-9]+);([0-9]+);([0-9]+)([Mm])$`)

func decodeMenuEvent(raw []byte) menuEvent {
	if string(raw) == esc+"OQ" || string(raw) == esc+"[12~" {
		return menuEvent{Kind: "select-text"}
	}
	text := string(raw)
	if m := sgrMouse.FindStringSubmatch(text); m != nil {
		button, _ := strconv.Atoi(m[1])
		x, _ := strconv.Atoi(m[2])
		y, _ := strconv.Atoi(m[3])
		kind := "mouse"
		if m[4] == "m" {
			kind = "release"
		}
		return menuEvent{Kind: kind, Button: button, X: x, Y: y}
	}
	switch text {
	case esc + "[A", esc + "OA":
		return menuEvent{Kind: "up"}
	case esc + "[B", esc + "OB":
		return menuEvent{Kind: "down"}
	case esc + "[C", esc + "OC":
		return menuEvent{Kind: "right"}
	case "\r", "\n":
		return menuEvent{Kind: "open"}
	case esc + "[D", esc + "OD":
		return menuEvent{Kind: "left"}
	case esc:
		return menuEvent{Kind: "back"}
	case esc + "[H", esc + "OH", esc + "[1~", esc + "[7~":
		return menuEvent{Kind: "home"}
	case esc + "[F", esc + "OF", esc + "[4~", esc + "[8~":
		return menuEvent{Kind: "end"}
	case esc + "[5~":
		return menuEvent{Kind: "page-up"}
	case esc + "[6~":
		return menuEvent{Kind: "page-down"}
	case esc + "[3~":
		return menuEvent{Kind: "delete"}
	case "\x7f", "\x08":
		return menuEvent{Kind: "erase"}
	}
	if utf8.Valid(raw) && !strings.HasPrefix(text, esc) && len(raw) > 0 {
		for _, r := range text {
			if unicode.IsControl(r) {
				return menuEvent{}
			}
		}
		return menuEvent{Kind: "text", Text: text}
	}
	return menuEvent{}
}
func (b *Browser) nextEvent() menuEvent {
	deadline := time.Now().Add(100 * time.Millisecond)
	for {
		events, left := splitKeys(b.pending)
		if len(events) > 0 {
			b.escapeSince = time.Time{}
			// Consume exactly one event. A click release or the following key stays
			// queued for the next frame, never for a probe that the event launches.
			raw := events[0]
			b.pending = b.pending[len(raw):]
			event := decodeMenuEvent(raw)
			return event
		}
		b.pending = left
		if len(left) == 1 && left[0] == 27 && b.escapeSince.IsZero() {
			b.escapeSince = time.Now()
		}
		data := b.s.read(deadline)
		if len(data) == 0 {
			if len(b.pending) == 1 && b.pending[0] == 27 && time.Since(b.escapeSince) >= max(seconds(b.s.opts.Timeout), 100*time.Millisecond) {
				b.pending = nil
				b.escapeSince = time.Time{}
				return menuEvent{Kind: "back"}
			}
			return menuEvent{}
		}
		b.pending = append(b.pending, data...)
		if len(b.pending) > 4096 {
			b.pending = nil
			return menuEvent{}
		}
	}
}
func chooseMenuSelection(entries []menuEntry, typed string) int {
	if n, err := strconv.Atoi(typed); err == nil && n >= 1 && n <= len(entries) {
		return n - 1
	}
	for i, entry := range entries {
		if strings.EqualFold(entry.Label, typed) {
			return i
		}
	}
	return -1
}
func (b *Browser) choose(title string, entries []menuEntry, selected int) (int, bool) {
	if len(entries) == 0 {
		return -1, false
	}
	b.enter()
	selected = min(max(selected, 0), len(entries)-1)
	offset := 0
	typed := ""
	dirty := true
	previousWidth, previousHeight := 0, 0
	var l menuLayout
	for {
		b.s.check()
		w, h := terminalSize(b.s.out)
		if dirty || w != previousWidth || h != previousHeight {
			l = layoutMenu(w, h)
			visible := max(1, l.Bottom-l.Top+1)
			if selected < offset {
				offset = selected
			}
			if selected >= offset+visible {
				offset = selected - visible + 1
			}
			l = b.draw(title, entries, selected, offset, typed)
			previousWidth, previousHeight = w, h
			dirty = false
		}
		event := b.next()
		if (w < 32 || h < 10) && event.Kind != "back" && event.Kind != "left" && !(event.Kind == "text" && event.Text == "q") {
			continue
		}
		old := selected
		switch event.Kind {
		case "redraw":
			dirty = true
		case "up":
			selected = max(0, selected-1)
			typed = ""
		case "down":
			selected = min(len(entries)-1, selected+1)
			typed = ""
		case "home":
			selected = 0
			typed = ""
		case "end":
			selected = len(entries) - 1
			typed = ""
		case "page-up":
			selected = max(0, selected-max(1, l.Bottom-l.Top))
			typed = ""
		case "page-down":
			selected = min(len(entries)-1, selected+max(1, l.Bottom-l.Top))
			typed = ""
		case "back", "left":
			return -1, false
		case "open", "right":
			if typed == "0" {
				return -1, false
			}
			if typed != "" {
				if n := chooseMenuSelection(entries, typed); n >= 0 {
					selected = n
				} else {
					typed = ""
					dirty = true
					continue
				}
			}
			return selected, true
		case "text":
			if event.Text == " " && typed == "" {
				return selected, true
			}
			if event.Text == "q" && typed == "" {
				return -1, false
			}
			if len(typed) < 128 {
				typed += event.Text
				if n := chooseMenuSelection(entries, typed); n >= 0 {
					selected = n
				}
				dirty = true
			}
		case "erase":
			if len(typed) > 0 {
				_, n := utf8.DecodeLastRuneInString(typed)
				typed = typed[:len(typed)-n]
				dirty = true
			} else {
				return -1, false
			}
		case "mouse":
			if event.Button&64 != 0 {
				if event.Button&1 == 0 {
					selected = max(0, selected-3)
				} else {
					selected = min(len(entries)-1, selected+3)
				}
				typed = ""
				break
			}
			if event.Button&3 == 0 && event.Button&32 == 0 {
				if event.Y == 2 {
					continue
				}
				backStart := 2 + utf8.RuneCountInString(menuFooter[:strings.Index(menuFooter, "q/Esc Back")])
				if event.Y == l.Footer && event.X >= backStart && event.X < backStart+len("q/Esc Back") {
					return -1, false
				}
				listEnd := l.Width - 2
				if l.Split {
					listEnd = l.ListWidth - 1
				}
				if event.Y >= l.Top && event.Y <= l.Bottom && event.X >= 2 && event.X <= listEnd {
					n := offset + event.Y - l.Top
					if n < len(entries) {
						return n, true
					}
				}
			}
		}
		if old != selected {
			dirty = true
		}
		if event.Kind == "up" || event.Kind == "down" {
			dirty = true
		}
	}
}
func browserChildren(prefix string) []menuEntry {
	out := []menuEntry{}
	for _, c := range matching(prefix) {
		tail := strings.TrimSpace(strings.TrimPrefix(c.Path, prefix))
		name := strings.Split(tail, " ")[0]
		if slices.ContainsFunc(out, func(e menuEntry) bool { return e.Label == name }) {
			continue
		}
		path := strings.TrimSpace(prefix + " " + name)
		group := matching(path)
		detail := fmt.Sprintf("%s\n\n%d cases\n\nRight / Enter or click to browse.", strings.ToUpper(name), len(group))
		if len(group) == 1 && group[0].Path == path {
			detail = caseDetail(group[0])
		} else {
			for _, child := range group {
				detail += "\n" + child.Path
			}
		}
		out = append(out, menuEntry{name, detail})
	}
	return out
}
func caseDetail(c Case) string {
	return c.Title + "\n\nprobe " + caseCommand(c) + "\n\n" + c.Expected + "\n\nPolicy: " + c.Policy + "\n\nTDN: " + strings.Join(c.Features, ", ") + "\n\nCleanup: " + c.Cleanup
}

func (b *Browser) help(title, text string) {
	b.enter()
	b.crumbs = nil
	offset := 0
	dirty := true
	lastW, lastH := 0, 0
	for {
		w, h := terminalSize(b.s.out)
		lines := wrapped(text, max(12, w-4))
		visible := max(1, h-6)
		offset = min(offset, max(0, len(lines)-visible))
		if dirty || w != lastW || h != lastH {
			var frame strings.Builder
			frame.WriteString(esc + "[H" + browserBase + esc + "[2J")
			fmt.Fprintf(&frame, "%s[1;2H%s[1;36m%s%s", esc, esc, clipText("Help / "+title, w-3), browserBase)
			for i, line := range lines[offset:min(len(lines), offset+visible)] {
				fmt.Fprintf(&frame, "%s[%d;2H%s", esc, i+3, clipText(line, w-3))
			}
			fmt.Fprintf(&frame, "%s[%d;2H%s[36m%s%s[0m", esc, max(1, h-2), esc, clipText("[ Back ]  Space/Enter Next page  Up/Down/Wheel  q/Esc Back", w-3), esc)
			b.textSelectRow = max(1, h-1)
			fmt.Fprintf(&frame, "%s[%d;2H%s", esc, b.textSelectRow, clipText("F2 Select text", w-3))
			b.s.send(frame.String())
			dirty = false
			lastW, lastH = w, h
		}
		e := b.next()
		switch e.Kind {
		case "back", "left":
			return
		case "text":
			if e.Text == " " {
				offset = min(max(0, len(lines)-visible), offset+visible)
				dirty = true
			}
			if e.Text == "q" || e.Text == "0" {
				return
			}
		case "redraw":
			dirty = true
		case "up":
			offset = max(0, offset-1)
			dirty = true
		case "down":
			offset = min(max(0, len(lines)-visible), offset+1)
			dirty = true
		case "right", "page-down", "open":
			offset = min(max(0, len(lines)-visible), offset+visible)
			dirty = true
		case "page-up":
			offset = max(0, offset-visible)
			dirty = true
		case "home":
			offset = 0
			dirty = true
		case "end":
			offset = max(0, len(lines)-visible)
			dirty = true
		case "mouse":
			if e.Button&64 != 0 {
				if e.Button&1 == 0 {
					offset = max(0, offset-3)
				} else {
					offset = min(max(0, len(lines)-visible), offset+3)
				}
				dirty = true
			} else if e.Button&35 == 0 && e.Y == h-2 && e.X <= 10 {
				return
			}
		}
	}
}
func (b *Browser) edit(title, value string) (string, bool) {
	text := []rune(value)
	cursor := len(text)
	dirty := true
	previousW, previousH := 0, 0
	for {
		w, h := terminalSize(b.s.out)
		if dirty || w != previousW || h != previousH {
			b.draw(title, []menuEntry{{"Save (Enter)", "Type a value. Spaces and Unicode are preserved. Left/Right move the cursor; Backspace/Delete edit. Esc cancels."}, {"Cancel (Esc)", "Discard this edit."}}, 0, 0, "")
			// Editor is below the two action rows. Avoid echoing control sequences.
			start := max(0, cursor-max(1, (w-8)/2))
			display := string(text[start:])
			if cursor < len(text) {
				display = string(text[start:cursor]) + "│" + string(text[cursor:])
			} else {
				display += "│"
			}
			row := min(9, h-3)
			b.s.send(fmt.Sprintf("%s[%d;2H%s[2K%s[1;36m%s%s[0m", esc, row, esc, esc, clipText(display, w-4), esc))
			previousW, previousH = w, h
			dirty = false
		}
		// Input framing is shared, but horizontal arrows edit rather than activate.
		event := b.next()
		switch event.Kind {
		case "redraw":
			dirty = true
		case "open":
			return string(text), true
		case "back":
			return value, false
		case "left":
			cursor = max(0, cursor-1)
			dirty = true
		case "right":
			cursor = min(len(text), cursor+1)
			dirty = true
		case "delete":
			if cursor < len(text) {
				text = append(text[:cursor], text[cursor+1:]...)
				dirty = true
			}
		case "erase":
			if cursor > 0 {
				text = append(text[:cursor-1], text[cursor:]...)
				cursor--
				dirty = true
			}
		case "text":
			runes := []rune(event.Text)
			if len(text)+len(runes) <= 1024 {
				text = append(text[:cursor], append(runes, text[cursor:]...)...)
				cursor += len(runes)
				dirty = true
			}
		case "home":
			cursor = 0
			dirty = true
		case "end":
			cursor = len(text)
			dirty = true
		case "mouse":
			if event.Button&3 == 0 && event.Button&96 == 0 {
				if event.Y == 5 {
					return string(text), true
				}
				if event.Y == 6 {
					return value, false
				}
			}
		}
	}
}
