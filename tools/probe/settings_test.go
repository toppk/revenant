package main

import (
	"io"
	"reflect"
	"slices"
	"testing"
)

func TestSettingsShareCommandLineDefaultsAndValidation(t *testing.T) {
	for _, c := range cases {
		fs, _ := caseFlags(&c, io.Discard)
		args := commonSettings(defaults())
		for _, name := range c.Options {
			f := fs.Lookup(name)
			if f == nil || settingLabels[name] == "" {
				t.Fatalf("%s: missing setting %s", c.Path, name)
			}
			args = settingArgs(args, name, flagValues(f))
		}
		got, err := parseOptionsTo(&c, args, io.Discard)
		want, _ := parseOptionsTo(&c, nil, io.Discard)
		if err != nil || !reflect.DeepEqual(got, want) {
			t.Fatalf("%s: %v; %v != %v", c.Path, err, got, want)
		}
	}
	c := matching("clipboard set")[0]
	args := settingArgs(commonSettings(defaults()), "text", []string{""})
	args = settingArgs(args, "target", []string{"primary"})
	got, err := parseOptionsTo(&c, args, io.Discard)
	if err != nil || got.Text != "" || got.Target != "primary" {
		t.Fatalf("empty text lost: %+v %v", got, err)
	}
	if _, err := parseOptionsTo(&c, settingArgs(args, "timeout", []string{"NaN"}), io.Discard); err == nil {
		t.Fatal("invalid setting accepted")
	}
	c = matching("identity tcap")[0]
	args = settingArgs(nil, "cap", []string{"TN", "Co"})
	args = settingArgs(args, "cap", []string{"RGB", "TN"})
	got, err = parseOptionsTo(&c, args, io.Discard)
	if err != nil || !slices.Equal(got.Caps, []string{"RGB", "TN"}) {
		t.Fatalf("repeatable replacement: %+v %v", got, err)
	}
	args = settingArgs(args, "cap", nil)
	got, err = parseOptionsTo(&c, args, io.Discard)
	if err != nil || len(got.Caps) != 0 {
		t.Fatal("list clear failed")
	}
	for _, args := range [][]string{nil, {"--assess"}} {
		got, err = parseOptionsTo(nil, args, io.Discard)
		if err != nil || got.Assess != (len(args) > 0) {
			t.Fatal("assessment must be opt-in")
		}
	}
}
func TestEmojiSectionsPartitionAllSamples(t *testing.T) {
	all := selectedWidthSections("text emoji all")
	names := []string{"single", "variation-selectors", "skin-tones", "zwj", "flags", "boundaries", "capacity"}
	var combined []WidthSection
	for _, name := range names {
		combined = append(combined, selectedWidthSections("text emoji "+name)...)
	}
	if !reflect.DeepEqual(combined, all) {
		t.Fatal("section cases omit or duplicate all-case data")
	}
	focus := selectedWidthSections("text emoji mode-2027")
	if len(focus) != 1 || len(focus[0].Samples) != 2 {
		t.Fatal("mode check must stay focused")
	}
	for _, sample := range focus[0].Samples {
		if slices.Equal(sample.Legacy, sample.Cluster) {
			t.Fatalf("not a mode discriminator: %s", sample.Label)
		}
	}
}
