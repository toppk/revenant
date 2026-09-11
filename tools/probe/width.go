package main

import (
	_ "embed"
	"encoding/json"
	"fmt"
	"regexp"
	"slices"
	"strconv"
	"strings"
)

// These samples and accept sets are migrated verbatim from the Python probes.
//
//go:embed data/emoji.json
var emojiSamples []byte

//go:embed data/fonts.json
var fontSamples []byte

type WidthSample struct {
	Label, Text, Note string
	Legacy, Cluster   []int
	RightMargin       bool `json:"right_margin"`
}
type WidthSection struct {
	ID      string
	Title   string
	Samples []WidthSample
}

func widthSections(data []byte) []WidthSection {
	var sections []WidthSection
	if err := json.Unmarshal(data, &sections); err != nil {
		panic(err)
	}
	return sections
}
func position(s *Session) (int, int, bool) {
	raw := s.query("CPR", esc+"[6n", "\x1b\\[\\??[0-9]+;[0-9]+R")
	m := regexp.MustCompile(`\x1b\[\??([0-9]+);([0-9]+)R`).FindSubmatch(raw)
	if m == nil {
		return 0, 0, false
	}
	row, rowErr := strconv.Atoi(string(m[1]))
	col, colErr := strconv.Atoi(string(m[2]))
	return row, col, rowErr == nil && colErr == nil && row > 0 && col > 0
}
func gradeWidth(sample WidthSample, requested, contract string, cells int) (string, string) {
	accept := sample.Legacy
	if contract == "cluster" {
		accept = sample.Cluster
	}
	verdict := "wrong"
	if slices.Contains(accept, cells) {
		verdict = "legacy-faithful"
		if contract == "cluster" {
			verdict = "cluster-capable"
		}
		if len(accept) > 1 {
			verdict = "underspecified"
		}
	} else if requested == "legacy" && slices.Contains(sample.Cluster, cells) && !slices.Equal(sample.Legacy, sample.Cluster) {
		verdict = "UNILATERAL"
	}
	return verdict, fmt.Sprintf("measured %d cells; %s accepts %v", cells, contract, accept)
}

// Section cases reuse the all-case samples and their original accept sets.
func selectedWidthSections(path string) []WidthSection {
	if path == "text fonts" {
		return widthSections(fontSamples)
	}
	return selectEmojiSections(widthSections(emojiSamples), strings.TrimPrefix(path, "text emoji "))
}
func selectEmojiSections(sections []WidthSection, id string) []WidthSection {
	byID := map[string]WidthSection{}
	for _, section := range sections {
		if section.ID == "" || len(section.Samples) == 0 {
			panic("emoji section lacks ID or samples")
		}
		if _, exists := byID[section.ID]; exists {
			panic("duplicate emoji section: " + section.ID)
		}
		byID[section.ID] = section
	}
	get := func(id string) WidthSection {
		section, ok := byID[id]
		if !ok {
			panic("missing emoji section: " + id)
		}
		return section
	}
	if id == "all" {
		return sections
	}
	if id == "mode-2027" {
		return []WidthSection{{Title: "Mode 2027: compare ZWJ and flag widths with the mode off/on", Samples: []WidthSample{get("zwj").Samples[0], get("flags").Samples[0]}}}
	}
	return []WidthSection{get(id)}
}

func widthProbe(s *Session) {
	s.preserveModes(2027)
	s.cleanup(func() { s.send(esc + "[0m") })
	sections := selectedWidthSections(s.result.Case)
	regimes := []string{s.opts.Regime}
	if s.opts.Regime == "both" || s.result.Case == "text emoji mode-2027" {
		regimes = []string{"legacy", "cluster"}
	}
	// CPR queries must not print their replies between the two measured positions.
	for _, regime := range regimes {
		s.send(modeSequence(2027, regime == "cluster"))
		state := s.mode(2027)
		contract := "legacy"
		if state == 1 || state == 3 {
			contract = "cluster"
		}
		s.say("Requested %s; mode 2027=%d; grading against %s. Artwork is not graded.", regime, state, contract)
		if regime == "cluster" && contract != "cluster" {
			s.say("Cluster mode not verified active; do not infer unsupported from silence or denied reports.")
		}
		for _, section := range sections {
			pageTitle := section.Title + " [" + regime + " pass]"
			startPage := func() { s.page(pageTitle); s.say("Requested %s; mode 2027=%d; contract: %s", regime, state, contract) }
			startPage()
			used := 0
			for _, sample := range section.Samples {
				codes := []string{}
				for _, r := range sample.Text {
					codes = append(codes, fmt.Sprintf("U+%04X", r))
				}
				cols, rows := terminalSize(s.out)
				label := fmt.Sprintf("%s (%s)", sample.Label, strings.Join(codes, " "))
				needed := len(wrapped(label, max(1, cols-1))) + 4
				if sample.Note != "" {
					needed += len(wrapped(sample.Note, max(1, cols-3)))
				}
				if used > 0 && used+needed > max(1, rows-6) {
					s.pause()
					startPage()
					used = 0
				}
				used += needed
				s.say("%s", label)
				verdict, detail := "visual-only", "no CPR"
				if sample.RightMargin {
					s.send(esc + "7" + esc + "[1A" + esc + "[999G")
					r1, c1, ok1 := quietPosition(s)
					s.send(sample.Text)
					r2, c2, ok2 := quietPosition(s)
					s.send(esc + "8")
					if ok1 && ok2 {
						verdict = "wrong"
						if r2 == r1+1 && (c2 == 1 || c2 == 3) {
							verdict = "wrap-faithful"
						}
						detail = fmt.Sprintf("right margin %d;%d -> %d;%d; expected next row col 1 or 3", r1, c1, r2, c2)
					}
				} else {
					s.send("    ")
					r1, c1, ok1 := quietPosition(s)
					s.send(sample.Text)
					r2, c2, ok2 := quietPosition(s)
					accept := sample.Legacy
					if contract == "cluster" {
						accept = sample.Cluster
					}
					s.send(fmt.Sprintf("|%s[%dG%s[32m|%s[0m\r\n", esc, 5+accept[0], esc, esc))
					if ok1 && ok2 {
						if r1 != r2 || c2 < c1 {
							verdict = "wrapped"
							detail = fmt.Sprintf("CPR %d;%d -> %d;%d", r1, c1, r2, c2)
						} else {
							verdict, detail = gradeWidth(sample, regime, contract, c2-c1)
						}
					}
				}
				s.say("  %s: %s", verdict, detail)
				if sample.Note != "" {
					s.say("  %s", sample.Note)
				}
				s.result.Findings = append(s.result.Findings, Finding{regime + ": " + sample.Label, verdict, detail})
			}
			s.pause()
		}
	}
	counts := map[string]int{}
	for _, f := range s.result.Findings {
		counts[f.Outcome]++
	}
	s.say("Width summary: %v", counts)
}
func quietPosition(s *Session) (int, int, bool) {
	s.quiet = true
	defer func() { s.quiet = false }()
	return position(s)
}
