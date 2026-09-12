package main

import (
	"fmt"
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
func copyFixture(s *Session) {
	s.say("COPY-PROBE alpha café 界 omega COPY-END")
	s.say("Start with -xrm 'XTerm*copyFlashDuration: 300' and optionally copyFlashColor.")
	s.say("Select and copy; inspect the flash, its expiry, the selection and exact paste contents.")
	s.pause()
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
	s.say("Search FIND-ME, café, a missing string, WRAPPED-NEEDLE. Check next/previous and Escape.")
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
		modes = []string{"off", "on"}
	}
	line := func(row int, text, style string, width int) {
		if len(text) > width {
			text = text[:width]
		}
		s.send(fmt.Sprintf("%s[%d;1H%s[0m%s[2K%s%s%s[0m", esc, row, esc, esc, style, text, esc))
	}
	for _, mode := range modes {
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
