package main

import (
	_ "embed"
	"encoding/json"
	"fmt"
	"slices"
	"strings"
)

// Generated from the TDN registry and its independently curated navigation.
//
//go:embed data/features.json
var featureData []byte

type Feature struct {
	ID             string   `json:"id"`
	Title          string   `json:"title"`
	Breadcrumb     []string `json:"breadcrumb"`
	Specifications []string `json:"specifications"`
	Sequence       string   `json:"sequence"`
}

var features = func() []Feature {
	var result []Feature
	if err := json.Unmarshal(featureData, &result); err != nil {
		panic(err)
	}
	return result
}()

func featureByID(id string) *Feature {
	for i := range features {
		if features[i].ID == id {
			return &features[i]
		}
	}
	return nil
}
func featureCases(id string) []Case {
	var result []Case
	for _, c := range cases {
		if slices.Contains(c.Features, id) {
			c.EntryFeature = id
			result = append(result, c)
		}
	}
	return result
}
func caseCommand(c Case) string {
	id := c.EntryFeature
	if id == "" {
		id = c.Features[0]
	}
	return id + " " + c.ID
}
func featureTrail(f Feature) string {
	return strings.Join(append(append([]string(nil), f.Breadcrumb...), f.Title), " / ")
}
func caseTrail(c Case) string {
	if f := featureByID(c.EntryFeature); f != nil {
		return featureTrail(*f) + " / " + c.ID
	}
	return c.Path // retained command-oriented browser
}
func featureDetail(f Feature) string {
	detail := f.Title + "\n\n" + f.ID + "\n\n" + strings.Join(f.Breadcrumb, " / ")
	if f.Sequence != "" {
		detail += "\n\nSequence: " + f.Sequence
	}
	return detail + "\nSources: " + strings.Join(f.Specifications, ", ") + fmt.Sprintf("\n\n%d related test scenarios. Choose a scenario to inspect its scope and settings.", len(featureCases(f.ID)))
}
func listFeatures() {
	for _, f := range features {
		fmt.Printf("%-44s %s\n", f.ID, featureTrail(f))
	}
}
func listFeatureCases(f Feature) {
	fmt.Printf("%s\n%s\nSources: %s\n\n", f.ID, featureTrail(f), strings.Join(f.Specifications, ", "))
	for _, c := range featureCases(f.ID) {
		fmt.Printf("probe %-68s %s\n", caseCommand(c), c.Title)
	}
	fmt.Println("\nA slug opens its scenario menu. Add a case ID to run directly; append --help for settings.")
}
func browseFeature(b *Browser, f Feature, results *[]Result) error {
	choices := featureCases(f.ID)
	entries := []menuEntry{}
	for _, c := range choices {
		entries = append(entries, menuEntry{c.ID, caseDetail(c)})
	}
	selected := 0
	for {
		index, ok := b.choose(featureTrail(f), entries, selected)
		if !ok {
			return nil
		}
		selected = index
		if err := browseCase(b, choices[index], results); err != nil {
			return err
		}
	}
}

// Navigation paths are authored explicitly. Slugs and specification provenance
// never determine tree position; moving a feature does not rename its command.
func browseFeatureTree(b *Browser, prefix []string, flat bool, results *[]Result) error {
	entries := []menuEntry{}
	ids := []string{}
	for _, f := range features {
		if !flat && (len(f.Breadcrumb) < len(prefix) || !slices.Equal(f.Breadcrumb[:len(prefix)], prefix)) {
			continue
		}
		if flat || len(f.Breadcrumb) == len(prefix) {
			entries = append(entries, menuEntry{f.ID, featureDetail(f)})
			ids = append(ids, f.ID)
		} else {
			name := f.Breadcrumb[len(prefix)]
			if slices.ContainsFunc(entries, func(e menuEntry) bool { return e.Label == name }) {
				continue
			}
			entries = append(entries, menuEntry{name, "Browse features in " + strings.Join(append(append([]string(nil), prefix...), name), " / ") + ".\n\nFeature slugs remain the same wherever they appear in this hierarchy."})
			ids = append(ids, "")
		}
	}
	title := strings.Join(prefix, " / ")
	if title == "" {
		title = "Home"
	}
	if flat {
		title = "Features"
	}
	selected := 0
	for {
		index, ok := b.choose(title, entries, selected)
		if !ok {
			return nil
		}
		selected = index
		var err error
		if ids[index] != "" {
			err = browseFeature(b, *featureByID(ids[index]), results)
		} else {
			err = browseFeatureTree(b, append(append([]string(nil), prefix...), entries[index].Label), false, results)
		}
		if err != nil {
			return err
		}
	}
}
