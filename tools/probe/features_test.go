package main

import (
	"regexp"
	"slices"
	"strings"
	"testing"
)

func TestFeatureCatalogAndCanonicalCommands(t *testing.T) {
	ids := map[string]bool{}
	slug := regexp.MustCompile(`^[a-z0-9]+(?:-[a-z0-9]+)*$`)
	for _, c := range cases {
		if !slug.MatchString(c.ID) || ids[c.ID] {
			t.Fatalf("invalid/duplicate case ID: %s", c.ID)
		}
		ids[c.ID] = true
		for _, id := range c.Features {
			if featureByID(id) == nil {
				t.Fatalf("missing curated feature: %s", id)
			}
		}
	}
	seen := map[string]bool{}
	for _, f := range features {
		if seen[f.ID] || !slug.MatchString(f.ID) || len(f.Breadcrumb) == 0 || len(f.Specifications) == 0 {
			t.Fatalf("bad feature: %+v", f)
		}
		seen[f.ID] = true
		mapped := featureCases(f.ID)
		if len(mapped) == 0 {
			t.Fatalf("feature without a test: %s", f.ID)
		}
		for _, c := range mapped {
			if caseCommand(c) != f.ID+" "+c.ID || !slices.Contains(c.Features, f.ID) {
				t.Fatal(c)
			}
			if !strings.HasPrefix(caseTrail(c), strings.Join(f.Breadcrumb, " / ")) {
				t.Fatal("lost breadcrumb", c)
			}
		}
	}
	if featureByID("osc-8-missing") != nil || len(featureCases("osc-8-missing")) != 0 {
		t.Fatal("unknown feature accepted")
	}
	// Moving navigation never changes IDs or canonical commands.
	f := *featureByID("osc-8-hyperlinks")
	c := featureCases(f.ID)[0]
	before := caseCommand(c)
	f.Breadcrumb = []string{"Another group"}
	if caseCommand(c) != before {
		t.Fatal("navigation changed identity")
	}
}
