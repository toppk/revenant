//go:build linux

package main

import (
	"bytes"
	"context"
	"errors"
	"os"
	"reflect"
	"strings"
	"testing"
	"time"
)

func actionCase(t *testing.T, id string) Case {
	t.Helper()
	for _, c := range cases {
		if c.ID == id {
			return c
		}
	}
	t.Fatalf("missing case %s", id)
	return Case{}
}

func runActionCase(t *testing.T, c Case) (Result, []byte) {
	t.Helper()
	s, _ := testSession(t)
	s.opts = defaults()
	s.opts.NoPause = true
	s.opts.Timeout = .001
	s.opts.Target = "all"
	if strings.HasPrefix(c.ID, "clipboard-") {
		s.opts.Target = "clipboard"
	}
	s.opts.Text = "path invariant"
	s.opts.Color = "#123456"
	s.opts.Program = "/not/a/real/terminal"
	result, err := runCase(s, c, nil)
	if c.ID == "colors-palette-spawn" && err == nil {
		t.Fatal("spawn fixture unexpectedly launched a terminal")
	}
	if err != nil && c.ID != "colors-palette-spawn" {
		t.Fatalf("%s: %v", c.ID, err)
	}
	output, err := os.ReadFile(s.out.Name())
	if err != nil {
		t.Fatal(err)
	}
	return result, output
}

func TestProbeActionsIgnoreDisplayPath(t *testing.T) {
	for _, test := range []struct{ id, want string }{
		{"clipboard-query", esc + "]52;c;?" + st},
		{"clipboard-set", esc + "]52;c;cGF0aCBpbnZhcmlhbnQ=" + st},
		{"clipboard-clear", esc + "]52;c;" + st},
		{"clipboard-invalid", esc + "]52;c;!!!!" + st},
		{"colors-dynamic-demo", esc + "]10;#ffe080" + st},
		{"colors-dynamic-query", esc + "]10;?" + st},
		{"colors-dynamic-set", esc + "]10;#123456" + st},
		{"colors-dynamic-reset", esc + "]110" + st},
		{"colors-palette-query", esc + "]4;1;?" + st},
		{"colors-palette-set", esc + "]4;1;#123456" + st},
		{"colors-palette-reset", esc + "]104" + st},
		{"colors-palette-demo", esc + "]4;0;" + palettes()["verify"].Colors[0] + st},
		{"colors-palette-list", "verify:"},
		{"colors-palette-spawn", ""},
	} {
		t.Run(test.id, func(t *testing.T) {
			c := actionCase(t, test.id)
			original, originalOutput := runActionCase(t, c)
			c.Path = "presentation moved elsewhere"
			renamed, renamedOutput := runActionCase(t, c)
			if test.want != "" && !bytes.Contains(originalOutput, []byte(test.want)) {
				t.Fatalf("%s did not emit %q", test.id, test.want)
			}
			if strings.HasPrefix(test.id, "clipboard-") &&
				bytes.Contains(originalOutput, []byte("This replaces the")) != (test.id != "clipboard-query") {
				t.Fatalf("%s used the wrong clipboard consent prompt", test.id)
			}
			if !bytes.Equal(originalOutput, renamedOutput) ||
				!reflect.DeepEqual(original.Events, renamed.Events) ||
				original.Outcome != renamed.Outcome || original.Error != renamed.Error {
				t.Fatalf("renaming %s changed behavior: output lengths %d/%d, events %d/%d, outcomes %s/%s",
					test.id, len(originalOutput), len(renamedOutput), len(original.Events), len(renamed.Events), original.Outcome, renamed.Outcome)
			}
			if original.CaseID != test.id || renamed.CaseID != test.id || original.Case == renamed.Case ||
				!reflect.DeepEqual(original.FeatureIDs, renamed.FeatureIDs) ||
				!reflect.DeepEqual(original.Arguments, renamed.Arguments) {
				t.Fatalf("renaming %s changed result identity: original=%+v, renamed=%+v", test.id, original, renamed)
			}
		})
	}
}

func TestProbeActionsRejectInvalidInternalID(t *testing.T) {
	for _, test := range []struct{ id, invalid string }{
		{"clipboard-query", "clipboard-not-an-action"},
		{"colors-dynamic-query", "colors-dynamic-not-an-action"},
		{"colors-palette-query", "colors-palette-not-an-action"},
	} {
		t.Run(test.invalid, func(t *testing.T) {
			c := actionCase(t, test.id)
			c.ID = test.invalid
			s, _ := testSession(t)
			s.opts = defaults()
			s.opts.Timeout = .001
			s.opts.NoPause = true
			result, err := runCase(s, c, nil)
			if err == nil || !strings.Contains(err.Error(), test.invalid) || result.Outcome != "error" || len(result.Events) != 0 {
				t.Fatalf("invalid action was not rejected without a request: result=%+v, err=%v", result, err)
			}
			output, err := os.ReadFile(s.out.Name())
			if err != nil || bytes.Contains(output, []byte("This replaces the")) {
				t.Fatalf("invalid action reached the consent prompt: err=%v", err)
			}
		})
	}
}

func TestRenamedColorDemosRestoreOnInterruption(t *testing.T) {
	for _, test := range []struct {
		id, mutation string
		cleanup      []string
	}{
		{"colors-dynamic-demo", esc + "]12;#ff60c0" + st, []string{esc + "]110" + st, esc + "]111" + st, esc + "]112" + st}},
		{"colors-palette-demo", esc + "]4;0;" + palettes()["verify"].Colors[0] + st, []string{esc + "]104;0" + st, esc + "]104;15" + st}},
	} {
		for _, renamed := range []bool{false, true} {
			t.Run(test.id+map[bool]string{false: "/original", true: "/renamed"}[renamed], func(t *testing.T) {
				c := actionCase(t, test.id)
				if renamed {
					c.Path = "presentation moved elsewhere"
				}
				s, input := testSession(t)
				s.opts = defaults()
				s.opts.Timeout = .001
				ctx, cancel := context.WithCancel(context.Background())
				defer cancel()
				s.ctx = ctx
				monitored := make(chan bool, 1)
				go func() {
					deadline := time.Now().Add(3 * time.Second)
					continued := false
					for {
						output, _ := os.ReadFile(s.out.Name())
						if !continued && test.id == "colors-dynamic-demo" && bytes.Contains(output, []byte(inspectionKeys)) {
							input.WriteString("\r")
							continued = true
						}
						if bytes.Contains(output, []byte(test.mutation)) {
							monitored <- true
							cancel()
							return
						}
						if time.Now().After(deadline) {
							monitored <- false
							cancel()
							return
						}
						time.Sleep(time.Millisecond)
					}
				}()
				result, err := runCase(s, c, nil)
				if !<-monitored || !errors.Is(err, errInterrupted) || result.Outcome != "interrupted" || len(s.cleanups) != 0 {
					t.Fatalf("%s did not interrupt after mutation and restore: result=%+v, err=%v", test.id, result, err)
				}
				output, err := os.ReadFile(s.out.Name())
				if err != nil {
					t.Fatal(err)
				}
				for _, sequence := range test.cleanup {
					if !bytes.Contains(output, []byte(sequence)) {
						t.Fatalf("%s did not send cleanup %q", test.id, sequence)
					}
				}
			})
		}
	}
}
