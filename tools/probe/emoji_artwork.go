package main

// These are visual acceptance cases. Do not turn successful output or CPR
// into a pass: neither proves that the terminal drew the requested glyph.
func emojiArtwork(s *Session) {
	s.page("Emoji artwork: inspect every sample")
	s.say("Tofu, blank ink or a clipped/unrecognizable symbol is a failure.")
	switch s.result.CaseID {
	case "monochrome-emoji":
		s.say("Bare U+1F6E0, no selector: 🛠 Installed demo-1.0")
		s.say("Text-default bases: [🛠] hammer/wrench [⚒] crossed tools [ℹ] information [❤] heart")
		s.say("Explicit VS15 text: [🛠\ufe0e] [⚒\ufe0e] [ℹ\ufe0e] [❤\ufe0e]")
		s.say("Check recognizable monochrome ink; repeat with color rendering disabled.")
	case "emoji-presentation":
		s.say("Columns: bare | VS15 (text) | VS16 (emoji)")
		s.say("Hammer/wrench: [🛠] [🛠\ufe0e] [🛠\ufe0f]")
		s.say("Heart:        [❤] [❤\ufe0e] [❤\ufe0f]")
		s.say("Information:  [ℹ] [ℹ\ufe0e] [ℹ\ufe0f]")
		s.say("Default emoji: [📦] package [😀] grinning face")
		s.say("VS15 selects text presentation and VS16 selects emoji presentation; bare")
		s.say("bases follow the Unicode default for that character.")
		s.say("Whether emoji presentation is drawn in color depends on the terminal's font")
		s.say("policy, not on this selector alone: a face the user configured, or the")
		s.say("primary text face, may legitimately serve the base in monochrome when it")
		s.say("covers it, and color paint can also be disabled. Record which face served")
		s.say("and whether color was enabled, then judge against that policy.")
		s.say("A missing glyph or the wrong presentation is a failure; monochrome artwork")
		s.say("from an explicitly preferred face is not.")
	case "emoji-cell-fitting":
		s.say("Spaced:   🛠 🛠 🛠")
		s.say("Adjacent: 🛠🛠🛠")
		s.say("Neighbors: A🛠B A🛠\ufe0eB A🛠\ufe0fB A📦B")
		s.say("Check visible tools/package, no lost neighbors or vertical clipping.")
		s.say("Spacing-dependent artwork size is an observation, not a width measurement.")
		s.say("Repeat at small/large font sizes; use the width probes for cursor advance.")
	case "symbol-whitespace-expansion":
		s.say("Optional drawing behavior, not a standard and not required.")
		s.say("Spaced:   🛠 🛠 🛠   (each symbol has a blank cell after it)")
		s.say("Adjacent: 🛠🛠🛠   (no blank cells between symbols)")
		s.say("Some terminals fit a one-cell symbol to a two-cell box when the next")
		s.say("cell is blank, so the spaced row can look bigger than the adjacent row.")
		s.say("The first symbol of a run may differ from the rest; compare whole rows.")
		s.say("The cursor advance is identical either way; use the width probes for that.")
		s.say("Report whether the two rows differ in artwork size, and by roughly how much;")
		s.say("a wider box grants drawing room, not a fixed multiple of glyph size.")
		s.say("Equal rows: record 'no enlargement observed for these samples'. That is not")
		s.say("proof of strict in-cell drawing; expansion can be absent for one font or")
		s.say("symbol and present for another. Either way it is not a failure.")
		s.say("Each symbol's cursor advance is the same in both rows; the rows differ in")
		s.say("total advance only because the spaced row contains spaces.")
		s.say("Eligible neighbors are an empty cell, SPACE or EN SPACE; the right edge and")
		s.say("a preceding symbol restrict expansion.")
		s.say("Then edit around a borrowed cell: insert and delete text beside a symbol and")
		s.say("watch resizing, repainting, selection and the cursor.")
		s.say("Occupied neighbors must never be overdrawn: A🛠B A📦B")
	case "emoji-sequences":
		s.say("Skin tone: [👍🏽] medium-tone thumbs up")
		s.say("ZWJ: [👩‍💻] woman technologist [👨‍👩‍👧‍👦] family")
		s.say("Regional flag: [🇺🇸] United States; keycap: [1\ufe0f\u20e3]")
		s.say("Tag flag: [🏴\U000e0067\U000e0062\U000e0065\U000e006e\U000e0067\U000e007f] England")
		s.say("Newer base: [🫩] face with bags under eyes (Unicode 16)")
		s.say("Each bracket should contain the named artwork, not detached components.")
		s.say("Record Unicode/font versions and mode 2027; this case does not change it.")
	}
	s.say("Visual-only: remains unassessed unless you record a human assessment.")
	s.pause()
}
