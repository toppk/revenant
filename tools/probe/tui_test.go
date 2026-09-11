package main

import (
	"testing"
)

func TestMenuKeysAndMouse(t *testing.T) {
	for raw, want := range map[string]string{esc + "[A": "up", esc + "OB": "down", esc + "[C": "right", esc + "[D": "left", "\r": "open", esc: "back", esc + "[5~": "page-up", esc + "[6~": "page-down", esc + "[3~": "delete", "3": "text"} {
		if got := decodeMenuEvent([]byte(raw)); got.Kind != want {
			t.Errorf("%q: %+v", raw, got)
		}
	}
	for _, suffix := range []string{"M", "m"} {
		got := decodeMenuEvent([]byte(esc + "[<0;12;7" + suffix))
		if got.X != 12 || got.Y != 7 || got.Button != 0 {
			t.Fatal(got)
		}
		if suffix == "m" && got.Kind != "release" {
			t.Fatal(got)
		}
	}
	got := decodeMenuEvent([]byte(esc + "[<65;12;7M"))
	if got.Button != 65 {
		t.Fatal(got)
	}
	// Arrow and SGR mouse reports are preserved when their bytes arrive singly.
	wire := []byte(esc + "[B" + esc + "[<0;8;6M" + esc + "[<0;8;6m")
	var pending []byte
	var events [][]byte
	for _, b := range wire {
		pending = append(pending, b)
		parts, left := splitKeys(pending)
		events = append(events, parts...)
		pending = left
	}
	if len(events) != 3 || len(pending) != 0 {
		t.Fatalf("events=%q pending=%q", events, pending)
	}
	if decodeMenuEvent(events[1]).Kind != "mouse" || decodeMenuEvent(events[2]).Kind != "release" {
		t.Fatal(events)
	}
}
func TestMenuShortcutsAndLayout(t *testing.T) {
	entries := browserChildren("")
	if chooseMenuSelection(entries, "2") != 1 || chooseMenuSelection(entries, "clipboard") != 1 || chooseMenuSelection(entries, "not-real") != -1 {
		t.Fatal(entries)
	}
	for _, size := range [][2]int{{120, 40}, {80, 24}, {40, 12}} {
		l := layoutMenu(size[0], size[1])
		if l.Top > l.Bottom || l.Bottom >= l.Footer || l.Footer >= l.Height {
			t.Fatal(l)
		}
	}
}

func TestBreadcrumbHitRegionsAt80Columns(t *testing.T) {
	c := featureCases("csi-sgr")[0]
	for _, title := range []string{caseTrail(c), caseTrail(c) + " / Settings", caseTrail(c) + " / Reply timeout"} {
		crumbs := breadcrumbLayout(title, 76)
		for i, crumb := range crumbs {
			if crumb.Start < 3 || crumb.End > 78 || crumb.Start > crumb.End {
				t.Fatalf("outside 80 columns: %+v", crumb)
			}
			if i > 0 && crumb.Start <= crumbs[i-1].End {
				t.Fatal("overlapping breadcrumb targets")
			}
		}
		if !crumbs[0].Active || crumbs[0].Target != "Home" {
			t.Fatal("Home missing")
		}
	}
}
