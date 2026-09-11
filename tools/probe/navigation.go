package main

import "strings"

type breadcrumb struct {
	Label, Target string
	Start, End    int
	Active        bool
}
type browserJump struct{ Target string }

// Destinations are resolved against curated navigation, not guessed from a
// protocol prefix. A jump is caught only by the browser, never by a probe run.
func browserRoute(b *Browser, target string, results *[]Result) func() error {
	if target == "Home" {
		return func() error { return browseFeatureTree(b, nil, false, results) }
	}
	if target == "Features" {
		return func() error { return browseFeatureTree(b, nil, true, results) }
	}
	for _, f := range features {
		if target == featureTrail(f) {
			return func() error { return browseFeature(b, f, results) }
		}
		for i := 1; i <= len(f.Breadcrumb); i++ {
			if target == strings.Join(f.Breadcrumb[:i], " / ") {
				return func() error { return browseFeatureTree(b, f.Breadcrumb[:i], false, results) }
			}
		}
		for _, c := range featureCases(f.ID) {
			if target == caseTrail(c) {
				return func() error { return browseCase(b, c, results) }
			}
		}
	}
	if found := matching(target); len(found) > 0 {
		if len(found) == 1 && found[0].Path == target {
			return func() error { return browseCase(b, found[0], results) }
		}
		return func() error { return browse(b, target, results) }
	}
	return nil
}
func browserAttempt(run func() error) (jump *browserJump, err error) {
	defer func() {
		if value := recover(); value != nil {
			if target, ok := value.(browserJump); ok {
				jump = &target
			} else {
				panic(value)
			}
		}
	}()
	err = run()
	return
}
func navigateBrowser(b *Browser, initial func() error, results *[]Result) error {
	run := initial
	for {
		jump, err := browserAttempt(run)
		if jump == nil {
			return err
		}
		next := browserRoute(b, jump.Target, results)
		if next != nil {
			run = next
		}
	}
}
func breadcrumbLayout(title string, width int) []breadcrumb {
	parts := strings.Split(title, " / ")
	entries := []breadcrumb{{Label: "Home", Target: "Home"}}
	if title != "Home" {
		for i, label := range parts {
			entries = append(entries, breadcrumb{Label: label, Target: strings.Join(parts[:i+1], " / ")})
		}
	}
	sizes := make([]int, len(entries))
	total := 3 * (len(entries) - 1)
	for i, e := range entries {
		sizes[i] = menuTextWidth(e.Label)
		total += sizes[i]
	}
	for total > width {
		biggest := -1
		for i, size := range sizes {
			if size > 4 && (biggest < 0 || size > sizes[biggest]) {
				biggest = i
			}
		}
		if biggest < 0 {
			break
		}
		sizes[biggest]--
		total--
	}
	col := 3
	for i := range entries {
		entries[i].Label = clipText(entries[i].Label, sizes[i])
		entries[i].Start = col
		entries[i].End = min(width+2, col+menuTextWidth(entries[i].Label)-1)
		entries[i].Active = i < len(entries)-1 && browserRoute(nil, entries[i].Target, nil) != nil
		col = entries[i].End + 4
	}
	return entries
}
func (b *Browser) breadcrumbEvent(event menuEvent) {
	if event.Kind != "mouse" || event.Y != 2 || event.Button&(64|32|3) != 0 {
		return
	}
	for _, crumb := range b.crumbs {
		if crumb.Active && event.X >= crumb.Start && event.X <= crumb.End {
			panic(browserJump{crumb.Target})
		}
	}
}
