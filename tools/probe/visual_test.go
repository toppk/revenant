package main

import (
	"strings"
	"testing"
)

func TestProceduralGlyphInventory(t *testing.T) {
	seen := make(map[rune]string)
	for _, group := range proceduralGlyphGroups {
		if group.note == "" || len(group.examples) == 0 {
			t.Errorf("%s lacks decision-catalog context", group.id)
		}
		for _, codepoint := range groupCodepoints(group) {
			if previous, ok := seen[codepoint]; ok {
				t.Errorf("U+%04X occurs in both %s and %s", codepoint, previous, group.id)
			}
			seen[codepoint] = group.id
		}
	}
	for _, codepoint := range []rune{0x2500, 0x2588, 0x2800, 0xe0b0, 0xe0d2, 0x1fb00, 0x1cd00, 0xf5d0} {
		if _, ok := seen[codepoint]; !ok {
			t.Errorf("representative U+%04X is missing", codepoint)
		}
	}
	if _, ok := seen[0x1fb93]; ok {
		t.Error("unassigned U+1FB93 must not appear in the character catalog")
	}
}

// Every split of the same input yields the same keys and pastes in the same order.
func TestPasteReaderSplitDelimiters(t *testing.T) {
	input := "a" + esc + "[200~one\rtwo" + esc + "[201~b" + esc + "[200~" + esc + "[201~" + esc + "[200~three" + esc + "[201~q"
	want := "keys:a|paste:one\rtwo|keys:b|paste:|paste:three|keys:q"
	for size := 1; size <= len(input); size++ {
		var reader pasteReader
		var got []string
		add := func(event pasteEvent) {
			switch {
			case event.paste:
				got = append(got, "paste:"+event.text)
			case len(got) > 0 && strings.HasPrefix(got[len(got)-1], "keys:"):
				got[len(got)-1] += string(event.keys) // keys may arrive in fragments
			case len(event.keys) > 0:
				got = append(got, "keys:"+string(event.keys))
			}
		}
		for start := 0; start < len(input); start += size {
			for _, event := range reader.feed([]byte(input[start:min(start+size, len(input))])) {
				add(event)
			}
		}
		add(pasteEvent{keys: reader.idle()})
		if joined := strings.Join(got, "|"); joined != want {
			t.Errorf("chunk size %d: got %q, want %q", size, joined, want)
		}
	}
}

func TestPasteReaderReleasesLoneEscape(t *testing.T) {
	var reader pasteReader
	if events := reader.feed([]byte(esc)); len(events) != 0 {
		t.Fatalf("a possible opener was released early: %v", events)
	}
	if held := string(reader.idle()); held != esc {
		t.Fatalf("idle released %q, want a lone Esc", held)
	}
	reader.feed(pasteStart)
	reader.feed([]byte("partial"))
	if held := reader.idle(); held != nil {
		t.Fatalf("idle released %q from inside a paste", held)
	}
}

func TestNumberedPasteVerdicts(t *testing.T) {
	for _, c := range []struct{ text, want string }{
		{"ne 0172\rline 0173\rline 0174\rline 019", "complete lines 0173..0174 consecutive. Whether the ends"},
		{"line 0001\rline 0002\n", "complete lines 0001..0002 consecutive"},
		{"line 0001\rgarbage\rline 0002", "MALFORMED interior line 2: \"garbage\""},
		{"line 0001\r\rline 0002", "MALFORMED interior line 2: \"\""},
		{"line 0010\rline 0012", "NOT consecutive, line 0012 follows 0010"},
		{"ne 0001\rline 0002\rline", "insufficient evidence, fewer than two complete lines"},
		{"line 0005", "insufficient evidence"},
		{"", "Pasted nothing."},
	} {
		if got := numberedPaste(c.text); !strings.Contains(got, c.want) {
			t.Errorf("%q: got %q, want it to contain %q", c.text, got, c.want)
		}
	}
}
