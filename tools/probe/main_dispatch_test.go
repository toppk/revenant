package main

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func renamedCase(t *testing.T, id string) *Case {
	t.Helper()
	for _, c := range cases {
		if c.ID == id {
			c.Path = "renamed display label"
			return &c
		}
	}
	t.Fatalf("missing case %s", id)
	return nil
}

func TestOptionsIgnoreDisplayPath(t *testing.T) {
	for _, tt := range []struct {
		id, target, mode, help string
	}{
		{"titles-query", "both", "compare", "label: both/title/icon"},
		{"titles-stack", "both", "compare", "label: both/title/icon"},
		{"colors-dynamic-set", "background", "compare", "color to change: foreground/background/cursor/all"},
		{"colors-dynamic-reset", "background", "compare", "color to change: foreground/background/cursor/all"},
		{"clipboard-query", "clipboard", "compare", "selection: clipboard/primary/select"},
		{"input-mouse", "clipboard", "1000", "mouse tracking: 9"},
		{"rendering-sync", "clipboard", "compare", "synchronized output: compare/off/on"},
		{"login-shell", "clipboard", "compare", "seconds to wait for each launched child's report"},
	} {
		t.Run(tt.id, func(t *testing.T) {
			c := renamedCase(t, tt.id)
			var output bytes.Buffer
			o, err := parseOptionsTo(c, nil, &output)
			if err != nil || o.Target != tt.target || o.Mode != tt.mode {
				t.Fatalf("defaults: target=%q mode=%q err=%v", o.Target, o.Mode, err)
			}
			fs, _ := caseFlags(c, &output)
			fs.Usage()
			if !strings.Contains(output.String(), tt.help) {
				t.Fatalf("help lacks %q: %s", tt.help, output.String())
			}
		})
	}
	for _, tt := range []struct{ id, option, problem string }{
		{"titles-query", "--target=bogus", "target must be"},
		{"clipboard-query", "--target=both", "unknown selection target"},
		{"colors-dynamic-set", "--target=bogus", "target must be"},
		{"colors-dynamic-reset", "--target=bogus", "target must be"},
		{"rendering-sync", "--mode=1000", "mode must be"},
		{"input-mouse", "--mode=bogus", "mouse mode must be"},
		{"input-mouse", "--scenario=bogus", "mouse scenario must be"},
	} {
		t.Run(tt.id+tt.option, func(t *testing.T) {
			_, err := parseOptionsTo(renamedCase(t, tt.id), []string{tt.option}, io.Discard)
			if err == nil || !strings.Contains(err.Error(), tt.problem) {
				t.Fatalf("wanted %q, got %v", tt.problem, err)
			}
		})
	}
	for _, tt := range []struct{ id, option, target, mode string }{
		{"titles-query", "--target=icon", "icon", "compare"},
		{"titles-query", "--target=clipboard", "both", "compare"},
		{"clipboard-query", "--target=primary", "primary", "compare"},
		{"colors-dynamic-set", "--target=foreground", "foreground", "compare"},
		{"colors-dynamic-set", "--target=clipboard", "background", "compare"},
		{"input-mouse", "--mode=1002", "clipboard", "1002"},
		{"input-mouse", "--mode=compare", "clipboard", "1000"},
		{"rendering-sync", "--mode=boundaries", "clipboard", "boundaries"},
	} {
		o, err := parseOptionsTo(renamedCase(t, tt.id), []string{tt.option}, io.Discard)
		if err != nil || o.Target != tt.target || o.Mode != tt.mode {
			t.Errorf("%s %s: target=%q mode=%q err=%v", tt.id, tt.option, o.Target, o.Mode, err)
		}
	}
	for _, option := range []string{"--target=title", "--mode=1000", "--program=other", "--status=3"} {
		if _, err := parseOptionsTo(renamedCase(t, "identity-reports"), []string{option}, io.Discard); err == nil {
			t.Errorf("unrelated case accepted %s", option)
		}
	}
}

func TestSpecialLaunchesIgnoreDisplayPath(t *testing.T) {
	// The palette and login-shell stand-in records each invocation without opening X.
	dir := t.TempDir()
	marker := filepath.Join(dir, "launches")
	program := filepath.Join(dir, "terminal")
	if err := os.WriteFile(program, []byte(fmt.Sprintf("#!/bin/sh\nprintf '%%s\\n' \"$*\" >> %q\n", marker)), 0o700); err != nil {
		t.Fatal(err)
	}
	for _, tt := range []struct {
		id   string
		args []string
		want int
	}{
		{"colors-palette-spawn", []string{"--program", program}, 0},
		{"login-shell", []string{"--program", program, "--seconds", "0.1"}, 1},
		{"session-hold", []string{"--status", "7"}, 7},
	} {
		t.Run(tt.id, func(t *testing.T) {
			var index = -1
			for i := range cases {
				if cases[i].ID == tt.id {
					index = i
					break
				}
			}
			if index < 0 {
				t.Fatal("missing case")
			}
			original := cases[index].Path
			cases[index].Path = "renamed display label"
			defer func() { cases[index].Path = original }()
			before, _ := os.ReadFile(marker)
			args := append([]string{cases[index].Features[0], tt.id}, tt.args...)
			if tt.id == "session-hold" {
				reader, writer, err := os.Pipe()
				if err != nil {
					t.Fatal(err)
				}
				stdout := os.Stdout
				os.Stdout = writer
				defer func() { os.Stdout = stdout; reader.Close(); writer.Close() }()
				code := run(args)
				writer.Close()
				output, err := io.ReadAll(reader)
				if err != nil {
					t.Fatal(err)
				}
				if code != tt.want || !strings.HasSuffix(string(output), "PROBE-HOLD-END status=7") {
					t.Errorf("hold status=%d output=%q", code, output)
				}
				return
			}
			code := run(args)
			if code != tt.want {
				t.Errorf("exit status = %d, want %d", code, tt.want)
			}
			after, err := os.ReadFile(marker)
			if err != nil || len(after) <= len(before) {
				t.Errorf("stand-in was not launched: %v", err)
			}
			if tt.id == "colors-palette-spawn" && !strings.Contains(string(after), "-e ") {
				t.Errorf("palette spawn did not pass child command: %q", after)
			}
			if tt.id == "login-shell" && len(after) >= len(before) && strings.Count(string(after[len(before):]), "\n") != 6 {
				t.Errorf("login-shell launched %d scenarios, want 6", strings.Count(string(after[len(before):]), "\n"))
			}
		})
	}
}
