package main

import "testing"

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
