package main

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
	"testing"
)

func TestTcapExactDecoding(t *testing.T) {
	for _, test := range []struct {
		wire        string
		name, value string
		supported   bool
	}{
		{esc + "P1+r544E=787465726D2D323536636F6C6F72" + st, "TN", "xterm-256color", true},
		{esc + "P0+r626164" + st, "bad", "", false},
		{esc + "P1+r436F=323536" + st, "Co", "256", true},
	} {
		got, err := decodeTcap([]byte(test.wire))
		if err != nil || string(got.Name) != test.name || string(got.Value) != test.value || got.Supported != test.supported {
			t.Fatalf("%+v, %v", got, err)
		}
	}
	for _, bad := range []string{esc + "P1+rF=00" + st, esc + "P1+rzz=00" + st, "garbage"} {
		if _, err := decodeTcap([]byte(bad)); err == nil {
			t.Fatalf("accepted malformed reply %q", bad)
		}
	}
}
func TestDeviceAttributesDecoding(t *testing.T) {
	for _, test := range []struct{ wire, want string }{
		{esc + "[?62;6;21;22c", "level 62 (VT220 class); features: 6 selective erase, 21 horizontal scrolling, 22 ANSI color"},
		{esc + "[?1;2c", "level 1; features: 2 printer"},
		{esc + "[?64;99c", "level 64 (VT420 class); features: 99 unknown"},
		{esc + "[?6c", "level 6; no feature codes"},
	} {
		got, err := decodeDA1([]byte(test.wire))
		if err != nil || got != test.want {
			t.Fatalf("%q: %q, %v", test.wire, got, err)
		}
	}
	for _, test := range []struct{ wire, want string }{
		{esc + "[>1;700;0c", "type 1 (VT220), firmware 700, cartridge 0"},
		{esc + "[>41;411;0c", "type 41 (VT420), firmware 411, cartridge 0"},
		{esc + "[>7;1;2c", "type 7 (unknown), firmware 1, cartridge 2"},
	} {
		got, err := decodeDA2([]byte(test.wire))
		if err != nil || got != test.want {
			t.Fatalf("%q: %q, %v", test.wire, got, err)
		}
	}
	for _, bad := range []string{esc + "[?c", esc + "[?6;c", esc + "[>1;2c", "garbage", esc + "[>1;2;3;4c"} {
		if _, err := decodeDA1([]byte(bad)); err == nil && !strings.HasPrefix(bad, esc+"[>") {
			t.Fatalf("DA1 accepted malformed reply %q", bad)
		}
		if _, err := decodeDA2([]byte(bad)); err == nil {
			t.Fatalf("DA2 accepted malformed reply %q", bad)
		}
	}
}
func TestOSCFramingPreservesUTF8AndC1(t *testing.T) {
	matcher := regexp.MustCompile(oscPattern("l"))
	for _, wire := range []string{esc + "]lÜ title" + st, "\x9dlÜ title\x9c", esc + "]l漢Ü\a"} {
		view := matcher.FindString(wireView([]byte("unrelated" + wire + "trailing")))
		got := []byte{}
		for _, r := range view {
			if r >= 0xe000 {
				r -= 0xe000
			}
			got = append(got, byte(r))
		}
		if string(got) != wire {
			t.Fatalf("got %q want %q", got, wire)
		}
	}
}
func TestKeyDecodingAndFragmentation(t *testing.T) {
	events := [][]byte{[]byte(esc + "[113:81:97;69:2;81:233u"), []byte(esc + "[27;5;105~"), []byte(esc + "[1;2:3A"), []byte("é")}
	k, ok := parseKey(events[0])
	if !ok || k.Code != 113 || k.Modifiers != 68 || k.Event != 2 || k.Shifted != 81 || k.Base != 97 || k.Text != "Qé" {
		t.Fatalf("%+v %v", k, ok)
	}
	k, ok = parseKey(events[1])
	if !ok || k.Code != 105 || k.Modifiers != 4 {
		t.Fatalf("%+v %v", k, ok)
	}
	k, ok = parseKey(events[2])
	if !ok || k.Name != "Up" || k.Event != 3 {
		t.Fatalf("%+v %v", k, ok)
	}
	stream := bytes.Join(events, nil)
	for split := 0; split <= len(stream); split++ {
		a, left := splitKeys(stream[:split])
		b, left := splitKeys(append(left, stream[split:]...))
		got := append(a, b...)
		if len(left) != 0 || len(got) != len(events) {
			t.Fatalf("split %d lost events", split)
		}
		for i := range events {
			if !bytes.Equal(got[i], events[i]) {
				t.Fatalf("split %d: %q", split, got)
			}
		}
	}
	for _, bad := range []string{esc + "[x;1u", esc + "[1;0u", esc + "[1;2:9u", esc + "[1;1;1114112u"} {
		if _, ok := parseKey([]byte(bad)); ok {
			t.Errorf("accepted %q", bad)
		}
	}
}
func TestWidthContractsRemainDistinct(t *testing.T) {
	sample := WidthSample{Legacy: []int{1}, Cluster: []int{2}}
	for _, test := range []struct {
		requested, contract string
		cells               int
		want                string
	}{{"legacy", "legacy", 1, "legacy-faithful"}, {"legacy", "legacy", 2, "UNILATERAL"}, {"cluster", "legacy", 1, "legacy-faithful"}, {"cluster", "cluster", 2, "cluster-capable"}, {"cluster", "cluster", 1, "wrong"}} {
		got, _ := gradeWidth(sample, test.requested, test.contract, test.cells)
		if got != test.want {
			t.Fatalf("%+v got %s", test, got)
		}
	}
	sample.Cluster = []int{1, 2}
	got, _ := gradeWidth(sample, "cluster", "cluster", 1)
	if got != "underspecified" {
		t.Fatal(got)
	}
}
func TestCasesHaveUniquePathsAndMetadata(t *testing.T) {
	seen := map[string]bool{}
	for _, c := range cases {
		if seen[c.Path] || len(c.Features) == 0 || c.Run == nil || c.Expected == "" || c.Cleanup == "" {
			t.Fatalf("invalid case %s", c.Path)
		}
		seen[c.Path] = true
		for _, other := range cases {
			if strings.HasPrefix(other.Path, c.Path+" ") {
				t.Fatalf("case %s hides subtree %s", c.Path, other.Path)
			}
		}
	}
}
func TestOptionsRejectInvalidBeforeTTY(t *testing.T) {
	c := matching("rendering sync")[0]
	for _, args := range [][]string{{"--timeout", "NaN"}, {"--delay", "Inf"}, {"--mode", "unknown"}, {"--frames", "0"}, {"--hold-ms", "-1"}} {
		if _, err := parseOptions(&c, args); err == nil {
			t.Fatalf("accepted %v", args)
		}
	}
	c = matching("colors dynamic set")[0]
	for _, value := range []string{"red;?", "red\x1b[2J"} {
		if _, err := parseOptions(&c, []string{"--color", value}); err == nil {
			t.Fatalf("accepted %q", value)
		}
	}
}
func TestEvidenceAndSpawnKeepArgumentsAsData(t *testing.T) {
	text := "space $HOME `id` ' quoted"
	o := defaults()
	o.XRM = []string{"XTerm*title: " + text}
	args, err := spawnArgs(o, "/tmp/path with spaces/probe")
	if err != nil {
		t.Fatal(err)
	}
	if !slices.Contains(args, "XTerm*title: "+text) || !slices.Contains(args, "/tmp/path with spaces/probe") {
		t.Fatal(args)
	}
	file := filepath.Join(t.TempDir(), "results.json")
	r := []Result{{SchemaVersion: 1, Outcome: "unassessed", Events: []Event{{"received", []byte{0, 27, 255}}}}}
	if err := saveResults(file, r); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(file)
	if err != nil {
		t.Fatal(err)
	}
	var got []Result
	if err = json.Unmarshal(data, &got); err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(got[0].Events[0].Bytes, r[0].Events[0].Bytes) {
		t.Fatal(got)
	}
}
func TestRGBScaling(t *testing.T) {
	for input, want := range map[string]string{"rgb:f/0/8": "#ff0088", "rgb:ffff/0000/8080": "#ff0080"} {
		got, ok := rgbHex(input)
		if !ok || got != want {
			t.Fatal(input, got)
		}
	}
}

func TestDisplayedCommandDoesNotEmitControls(t *testing.T) {
	arg := "\x1b[31mspace $(echo BAD) '"
	displayed := commandText("clipboard set", []string{"--text", arg})
	if strings.Contains(displayed, esc) {
		t.Fatal("command display emitted a control")
	}
	// Passing the displayed command to Bash must preserve the one argument as
	// data. Here we inspect its quoting without executing the displayed command.
	if !strings.Contains(displayed, "$'\\033") {
		t.Fatal(displayed)
	}
}
