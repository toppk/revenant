package main

import "fmt"

// These are visual acceptance cases. Do not turn successful output or CPR
// into a pass: neither proves that the terminal drew the requested glyph.
// The width regime changes how a sequence is segmented, so artwork has to say which
// regime produced it. A DECRPM of 1/3 is the cluster contract and 2/4 legacy;
// anything else -- no answer, or "not recognized" -- is unknown, and is never
// reported as a confirmed legacy contract.
func regimeContract(state int) string {
	switch state {
	case 1, 3:
		return "cluster"
	case 2, 4:
		return "legacy"
	}
	return "unknown"
}

func regimeStateLabel(state int) string {
	if regimeContract(state) == "unknown" {
		return "unknown"
	}
	return fmt.Sprint(state)
}

// Artwork cases other than emoji-sequences do not select a regime; they report the
// one they found and leave it alone.
func reportRegime(s *Session) {
	state := s.mode(2027)
	s.say("Mode 2027 left as found: %s; segmentation contract: %s.", regimeStateLabel(state), regimeContract(state))
}

// Sequence artwork reuses the width runner's request, query and restore behavior:
// one pass per requested regime, each showing the state actually reported.
func sequencePasses(s *Session) {
	s.preserveModes(2027)
	regimes := []string{s.opts.Regime}
	if s.opts.Regime == "both" {
		regimes = []string{"legacy", "cluster"}
	}
	for index, regime := range regimes {
		if index > 0 {
			s.pause()
			s.page("Emoji artwork: inspect every sample")
		}
		s.send(modeSequence(2027, regime == "cluster"))
		state := s.mode(2027)
		contract := regimeContract(state)
		s.say("[%s pass] Requested %s; mode 2027=%s; segmentation contract: %s.", regime, regime, regimeStateLabel(state), contract)
		if contract != regime {
			s.say("The requested regime is not confirmed active; judge these samples without a segmentation expectation.")
		}
		s.say("Tofu, blank ink or a clipped/unrecognizable symbol is a failure.")
		sequenceSamples(s, contract)
	}
}

func sequenceSamples(s *Session, contract string) {
	s.say("Joined sequences -- each bracket should draw one image, not its components:")
	s.say("  Skin tone: [👍🏽] medium-tone thumbs up")
	s.say("  ZWJ: [👩‍💻] woman technologist [👨‍👩‍👧‍👦] family")
	s.say("  Regional flag: [🇺🇸] United States; keycap: [1\ufe0f\u20e3]")
	s.say("  Tag flag: [🏴\U000e0067\U000e0062\U000e0065\U000e006e\U000e0067\U000e007f] England")
	s.say("Single bases (artwork appears only if a font covers them):")
	s.say("  Newer base: [🫩] face with bags under eyes (Unicode 16)")
	s.say("  Unicode 18 base: [🫝] pickle; a missing glyph is a font observation")
	s.say("Unicode 18 segmentation samples -- these are NOT all expected to join:")
	s.say("  Indic conjunct: [?\u094d\u0924]")
	switch contract {
	case "cluster":
		s.say("    Unicode 18 makes this one cluster (Unicode 17 made two). Expect the")
		s.say("    question mark and the conjunct drawn together; what each face draws is")
		s.say("    a font observation, not evidence of the Unicode version.")
	case "legacy":
		s.say("    The legacy contract keeps separate atoms in both releases: the question")
		s.say("    mark and the conjunct may be drawn independently. Nothing here separates")
		s.say("    Unicode 17 from 18.")
	default:
		s.say("    Regime unknown: record what is drawn without a segmentation expectation.")
	}
	s.say("  Sequence boundary: [😀\u200d\U0001f7ff]")
	switch contract {
	case "cluster":
		s.say("    Unicode 18 makes this TWO clusters: the grinning face, then U+1F7FF as a")
		s.say("    separate symbol (often a missing glyph in current fonts). Unicode 17")
		s.say("    joined them into one cluster; what a font drew for that varied.")
	case "legacy":
		s.say("    Separate atoms in both releases: the grinning face, then a separate")
		s.say("    symbol. It must not be drawn as one merged image.")
	default:
		s.say("    Regime unknown: record what is drawn without a segmentation expectation.")
	}
	s.say("Record Unicode/font versions and the regime reported for this pass.")
}

func emojiArtwork(s *Session) {
	s.page("Emoji artwork: inspect every sample")
	if s.result.CaseID == "emoji-sequences" {
		sequencePasses(s)
		s.say("Visual-only: remains unassessed unless you record a human assessment.")
		s.pause()
		return
	}
	reportRegime(s)
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
	}
	s.say("Visual-only: remains unassessed unless you record a human assessment.")
	s.pause()
}
