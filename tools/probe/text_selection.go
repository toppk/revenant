package main

import "fmt"

const selectionHint = "Select text: drag/copy | F2/Space/Enter: resume | q/Esc: back"

// Keep the current screen intact. Mouse reporting is disabled so selection and
// copying are handled by the host terminal; no clipboard protocol is required.
func (b *Browser) startTextSelection() {
	b.selecting = true
	for _, mode := range browserModes {
		if mode != 25 && mode != 1049 {
			b.s.send(modeSequence(mode, false))
		}
	}
	width, height := terminalSize(b.s.out)
	b.s.send(fmt.Sprintf("%s[%d;1H%s[2K%s[1;36m%s%s", esc, max(1, height-1), esc, esc, clipText(selectionHint, max(1, width-1)), browserBase))
}
func (b *Browser) stopTextSelection() {
	b.selecting = false
	b.s.send(modeSequence(1000, true) + modeSequence(1006, true))
	b.pending = nil
}
func (b *Browser) next() menuEvent {
	for {
		event := b.nextEvent()
		selectClick := event.Kind == "mouse" && event.Button&(64|32|3) == 0 && event.Y == b.textSelectRow && event.X >= 2 && event.X <= 16
		if b.selecting {
			if event.Kind == "select-text" || event.Kind == "open" || event.Kind == "back" || event.Kind == "text" && (event.Text == "q" || event.Text == " ") {
				b.stopTextSelection()
				return menuEvent{Kind: "redraw"}
			}
			// No menu redraws or navigation during selection, including after resize.
			continue
		}
		if event.Kind == "select-text" || selectClick {
			b.startTextSelection()
			continue
		}
		b.breadcrumbEvent(event)
		return event
	}
}
