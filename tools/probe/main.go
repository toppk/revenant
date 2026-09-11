// Probe is a human-run terminal exerciser. It never updates TDN support claims.
package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/signal"
	"path/filepath"
	"slices"
	"strings"
	"syscall"
	"time"
)

type stringsFlag []string

func (v *stringsFlag) String() string     { return strings.Join(*v, ",") }
func (v *stringsFlag) Set(s string) error { *v = append(*v, s); return nil }

type Options struct {
	Timeout, Delay, Seconds                               float64
	Assess, NoPause, BEL, Styles, CaptureControls         bool
	Output, Terminal, Version, Configuration, Observation string
	Target, Text, Color, Mode, CWD, Font, Regime, Palette string
	Caps, XRM                                             stringsFlag
	Frames, FrameMS, HoldMS, PauseMS, Index               int
	Program, Geometry                                     string
}
type Case struct {
	ID, EntryFeature, Path, Title, Policy, Expected, Cleanup string
	Features                                                 []string
	Options                                                  []string
	Run                                                      func(*Session)
}
type Event struct {
	Direction string `json:"direction"`
	Bytes     []byte `json:"bytes_base64"`
}
type Finding struct {
	Name    string `json:"name"`
	Outcome string `json:"outcome"`
	Detail  string `json:"detail"`
}
type Result struct {
	SchemaVersion  int               `json:"schema_version"`
	Case           string            `json:"case"`
	CaseID         string            `json:"case_id"`
	EntryFeatureID string            `json:"entry_feature_id,omitempty"`
	FeatureIDs     []string          `json:"feature_ids"`
	Started        string            `json:"started"`
	Terminal       string            `json:"terminal"`
	Version        string            `json:"as-of"`
	Configuration  string            `json:"configuration"`
	Environment    map[string]string `json:"environment"`
	Arguments      []string          `json:"arguments"`
	Outcome        string            `json:"outcome"`
	Observation    string            `json:"observation,omitempty"`
	Error          string            `json:"error,omitempty"`
	Findings       []Finding         `json:"findings,omitempty"`
	Events         []Event           `json:"events,omitempty"`
	TraceTruncated bool              `json:"trace_truncated,omitempty"`
	traceBytes     int
}

func defaults() Options {
	return Options{Timeout: .5, Seconds: 20, Target: "clipboard", Mode: "compare", Regime: "both", Frames: 8, FrameMS: 400, PauseMS: 150, Index: 1, Palette: "verify", Program: "xterm", Geometry: "100x40"}
}
func parseOptions(c *Case, args []string) (Options, error) { return parseOptionsTo(c, args, os.Stdout) }
func caseFlags(c *Case, output io.Writer) (*flag.FlagSet, *Options) {
	o := defaults()
	if c != nil {
		if strings.HasPrefix(c.Path, "titles ") {
			o.Target = "both"
		}
		if strings.HasPrefix(c.Path, "colors dynamic ") {
			o.Target = "background"
		}
		if c.Path == "input mouse" {
			o.Mode = "1000"
		}
	}
	fs := flag.NewFlagSet("probe", flag.ContinueOnError)
	fs.SetOutput(output)
	fs.Float64Var(&o.Timeout, "timeout", o.Timeout, "seconds per reply (positive)")
	fs.Float64Var(&o.Delay, "delay", 0, "seconds per visual stage instead of Space/Enter")
	fs.BoolVar(&o.Assess, "assess", false, "ask for a visual assessment after each menu run")
	fs.BoolVar(&o.NoPause, "no-pause", false, "skip visual pauses")
	fs.StringVar(&o.Output, "output", "", "write JSON evidence to FILE (menus collect all cases)")
	fs.StringVar(&o.Terminal, "terminal", "", "terminal ID, supplied by the tester")
	fs.StringVar(&o.Version, "terminal-version", "", "assessed version, supplied by the tester")
	fs.StringVar(&o.Configuration, "configuration", "", "resources, fonts, locale or other setup notes")
	fs.StringVar(&o.Observation, "observation", "", "tester observation; never automatically graded")
	if c != nil {
		for _, name := range c.Options {
			switch name {
			case "target":
				usage := "selection: clipboard/primary/select (aliases c/p/s)"
				if strings.HasPrefix(c.Path, "titles ") {
					usage = "label: both/title/icon"
				}
				if strings.HasPrefix(c.Path, "colors dynamic ") {
					usage = "color to change: foreground/background/cursor/all"
				}
				fs.StringVar(&o.Target, name, o.Target, usage)
			case "text":
				fs.StringVar(&o.Text, name, "probe clipboard", "text to send (persistent for clipboard set)")
			case "bel":
				fs.BoolVar(&o.BEL, name, false, "terminate OSC with BEL instead of ST")
			case "cap":
				fs.Var(&o.Caps, name, "capability name; repeatable (empty list uses TN, Co, RGB, not-a-capability)")
			case "color":
				fs.StringVar(&o.Color, name, "#142850", "color name, #RRGGBB or rgb:R/G/B")
			case "index":
				fs.IntVar(&o.Index, name, 1, "palette index, 0..255")
			case "palette":
				fs.StringVar(&o.Palette, name, o.Palette, "named palette: xterm/tango/solarized/gruvbox/nord/verify")
			case "seconds":
				fs.Float64Var(&o.Seconds, name, o.Seconds, "input capture duration")
			case "mode":
				usage := "synchronized output: compare/off/on"
				if c.Path == "input mouse" {
					usage = "mouse tracking: 9 (press), 1000 (press/release), 1002 (drag), 1003 (all motion)"
				}
				fs.StringVar(&o.Mode, name, o.Mode, usage)
			case "styles":
				fs.BoolVar(&o.Styles, name, false, "cycle DECSCUSR styles after inspecting startup")
			case "cwd":
				cwd, _ := os.Getwd()
				fs.StringVar(&o.CWD, name, cwd, "existing local directory to report")
			case "font":
				fs.StringVar(&o.Font, name, "", "font name (persistent OSC 50 request)")
			case "regime":
				fs.StringVar(&o.Regime, name, o.Regime, "legacy/cluster/both width contracts")
			case "frames":
				fs.IntVar(&o.Frames, name, o.Frames, "frames per mode")
			case "frame-ms":
				fs.IntVar(&o.FrameMS, name, o.FrameMS, "milliseconds drawing each frame")
			case "hold-ms":
				fs.IntVar(&o.HoldMS, name, 0, "additional mid-frame hold in milliseconds")
			case "pause-ms":
				fs.IntVar(&o.PauseMS, name, o.PauseMS, "milliseconds between frames")
			case "program":
				fs.StringVar(&o.Program, name, o.Program, "terminal executable for explicit spawn")
			case "geometry":
				fs.StringVar(&o.Geometry, name, o.Geometry, "spawned terminal geometry")
			case "xrm":
				fs.Var(&o.XRM, name, "additional X resource for explicit spawn; repeatable")
			}
		}
	}
	fs.Usage = func() {
		if c != nil {
			fmt.Fprintf(output, "probe %s\n%s\nTDN: %s\nPolicy: %s\nExpected: %s\nCleanup: %s\n\n", caseCommand(*c), c.Title, strings.Join(c.Features, ", "), c.Policy, c.Expected, c.Cleanup)
		}
		fs.PrintDefaults()
	}
	return fs, &o
}
func parseOptionsTo(c *Case, args []string, output io.Writer) (Options, error) {
	fs, values := caseFlags(c, output)
	err := fs.Parse(args)
	o := *values
	if err != nil {
		return o, err
	}
	if fs.NArg() > 0 {
		return o, fmt.Errorf("unexpected arguments: %v", fs.Args())
	}
	for name, v := range map[string]float64{"timeout": o.Timeout, "delay": o.Delay, "seconds": o.Seconds} {
		if math.IsNaN(v) || math.IsInf(v, 0) || v < 0 || v > 3600 || (name != "delay" && v == 0) {
			return o, fmt.Errorf("%s must be finite and within 0..3600 seconds (timeout/seconds must be positive)", name)
		}
	}
	if o.Frames < 1 || o.Frames > 10000 || o.FrameMS < 0 || o.FrameMS > 3600000 || o.HoldMS < 0 || o.HoldMS > 3600000 || o.PauseMS < 0 || o.PauseMS > 3600000 {
		return o, fmt.Errorf("invalid frame count or duration")
	}
	if o.Index < 0 || o.Index > 255 {
		return o, fmt.Errorf("index must be 0..255")
	}
	if !slices.Contains([]string{"legacy", "cluster", "both"}, o.Regime) {
		return o, fmt.Errorf("regime must be legacy, cluster, or both")
	}
	for _, v := range append([]string{o.Font, o.Color}, o.Caps...) {
		if !safePayload(v) || strings.Contains(v, ";") {
			return o, fmt.Errorf("font/color/capability values must not contain controls or semicolons")
		}
	}
	if c != nil {
		if strings.HasPrefix(c.Path, "colors dynamic ") && o.Target == "clipboard" {
			o.Target = "background"
		}
		if strings.HasPrefix(c.Path, "titles ") {
			if o.Target == "clipboard" {
				o.Target = "both"
			}
			if !slices.Contains([]string{"both", "title", "icon"}, o.Target) {
				return o, fmt.Errorf("target must be both, title, or icon")
			}
		}
		if strings.HasPrefix(c.Path, "clipboard ") {
			if _, ok := selectionTarget(o.Target); !ok {
				return o, fmt.Errorf("unknown selection target %q", o.Target)
			}
		}
		if c.Path == "rendering sync" && !slices.Contains([]string{"compare", "on", "off"}, o.Mode) {
			return o, fmt.Errorf("mode must be compare, on, or off")
		}
		if c.Path == "input mouse" {
			if o.Mode == "compare" {
				o.Mode = "1000"
			}
			if !slices.Contains([]string{"9", "1000", "1002", "1003"}, o.Mode) {
				return o, fmt.Errorf("mouse mode must be 9, 1000, 1002, or 1003")
			}
		}
		if strings.HasPrefix(c.Path, "colors dynamic ") && slices.Contains([]string{"colors dynamic set", "colors dynamic reset"}, c.Path) && !slices.Contains([]string{"foreground", "background", "cursor", "all"}, o.Target) {
			return o, fmt.Errorf("target must be foreground, background, cursor, or all")
		}
	}
	return o, nil
}
func safePayload(s string) bool {
	for _, r := range s {
		if r < 32 || (r >= 127 && r <= 159) {
			return false
		}
	}
	return true
}
func runCase(s *Session, c Case, args []string) (r Result, err error) {
	r = Result{SchemaVersion: 1, Case: c.Path, CaseID: c.ID, EntryFeatureID: c.EntryFeature, FeatureIDs: c.Features, Started: time.Now().UTC().Format(time.RFC3339), Terminal: s.opts.Terminal, Version: s.opts.Version, Configuration: s.opts.Configuration, Arguments: append(strings.Fields(caseCommand(c)), args...), Outcome: "unassessed", Observation: s.opts.Observation, Environment: map[string]string{"TERM": os.Getenv("TERM"), "TERM_PROGRAM": os.Getenv("TERM_PROGRAM"), "LANG": os.Getenv("LANG")}}
	s.result = &r
	s.inspected = s.writes
	s.quit, s.navPending = false, nil
	s.navEscape = time.Time{}
	defer func() {
		var unexpected any
		if p := recover(); p != nil {
			if _, ok := p.(probeExit); ok {
				s.quit = true
				r.Outcome = "stopped"
			} else if f, ok := p.(probeFailure); ok {
				err = f.err
			} else {
				unexpected = p
			}
		}
		func() {
			defer func() {
				if p := recover(); p != nil && unexpected == nil {
					unexpected = p
				}
			}()
			err = errors.Join(err, s.restore())
		}()
		s.result = nil
		if unexpected != nil {
			panic(unexpected)
		}
		if err != nil {
			r.Outcome = "error"
			r.Error = err.Error()
			if errors.Is(err, errInterrupted) {
				r.Outcome = "interrupted"
			}
		}
	}()
	s.say("\r\n%s\r\nCommand: probe %s\r\nTDN: %s\r\nPolicy: %s\r\nExpected: %s\r\nCleanup: %s\r\n", c.Title, commandText(caseCommand(c), args), strings.Join(c.Features, ", "), c.Policy, c.Expected, c.Cleanup)
	c.Run(s)
	if r.Observation != "" {
		r.Outcome = "observed"
	}
	return
}
func saveResults(path string, results []Result) error {
	if path == "" {
		return nil
	}
	data, err := json.MarshalIndent(results, "", "  ")
	if err != nil {
		return err
	}
	f, err := os.CreateTemp(filepath.Dir(path), ".probe-results-*")
	if err != nil {
		return err
	}
	name := f.Name()
	defer os.Remove(name)
	if _, err = f.Write(append(data, '\n')); err != nil {
		f.Close()
		return err
	}
	if err = f.Close(); err != nil {
		return err
	}
	return os.Rename(name, path)
}
func matching(prefix string) []Case {
	var out []Case
	for _, c := range cases {
		if prefix == "" || c.Path == prefix || strings.HasPrefix(c.Path, prefix+" ") {
			out = append(out, c)
		}
	}
	return out
}
func list(prefix string) {
	for _, c := range matching(prefix) {
		fmt.Printf("%-30s %s\n", c.Path, c.Title)
	}
}
func menu(s *Session, prefix string, results *[]Result) error {
	b := newBrowser(s)
	defer b.leave()
	return navigateBrowser(b, func() error { return browse(b, prefix, results) }, results)
}
func browse(b *Browser, prefix string, results *[]Result) error {
	selected := 0
	for {
		entries := browserChildren(prefix)
		title := prefix
		if title == "" {
			title = "Home"
		}
		index, ok := b.choose(title, entries, selected)
		if !ok {
			return nil
		}
		selected = index
		path := strings.TrimSpace(prefix + " " + entries[index].Label)
		found := matching(path)
		if len(found) == 1 && found[0].Path == path {
			if err := browseCase(b, found[0], results); err != nil {
				return err
			}
		} else {
			if err := browse(b, path, results); err != nil {
				return err
			}
		}
	}
}
func browseCase(b *Browser, c Case, results *[]Result) error {
	s := b.s
	args := commonSettings(s.opts)
	selected := 0
	notice := ""
	for {
		detail := caseDetail(c)
		if notice != "" {
			detail = notice + "\n\n" + detail
		}
		if len(args) > 0 {
			detail += "\n\nCommand: probe " + commandText(caseCommand(c), args)
		}
		choices := []menuEntry{{"Run probe", detail}, {"Settings", "Choose named settings for this test, such as the target, duration, or width mode.\n\n" + detail}, {"Help", detail}, {"Test location", "Show selectable case ID, breadcrumb and command for a bug report. Mouse capture is released."}, {"Back", "Return to the case list."}}
		choice, ok := b.choose(caseTrail(c), choices, selected)
		if !ok || choice == 4 {
			return nil
		}
		selected = choice
		if choice == 3 {
			b.leave()
			s.say("\r\nTest: %s\r\nLocation: %s\r\nCommand: probe %s", c.ID, caseTrail(c), commandText(caseCommand(c), args))
			s.say("Select this text using your terminal's selection gesture.")
			saved := s.opts
			s.opts.NoPause, s.opts.Delay = false, 0
			s.dismissiblePause("Space/Enter: return to test | q/Esc: back")
			s.opts = saved
			continue
		}
		if choice == 2 {
			var help strings.Builder
			parseOptionsTo(&c, []string{"--help"}, &help)
			b.help(caseTrail(c), help.String())
			continue
		}
		if choice == 1 {
			args = b.settings(c, args)
			notice = ""
			selected = 0
			continue
		}
		var help strings.Builder
		opts, e := parseOptionsTo(&c, args, &help)
		if e != nil {
			notice = e.Error()
			selected = 1
			continue
		}
		saved := s.opts
		for name, copyValue := range map[string]func(){"timeout": func() { opts.Timeout = saved.Timeout }, "delay": func() { opts.Delay = saved.Delay }, "no-pause": func() { opts.NoPause = saved.NoPause }, "observation": func() { opts.Observation = saved.Observation }} {
			present := false
			for _, arg := range args {
				if arg == "--"+name || strings.HasPrefix(arg, "--"+name+"=") {
					present = true
				}
			}
			if !present {
				copyValue()
			}
		}
		opts.Assess = saved.Assess
		opts.Output = saved.Output
		opts.Terminal = saved.Terminal
		opts.Version = saved.Version
		opts.Configuration = saved.Configuration
		b.leave()
		s.opts = opts
		r, e := runCase(s, c, args)
		s.opts = saved
		if s.ctx.Err() != nil || errors.Is(e, errInterrupted) {
			*results = append(*results, r)
			return errors.Join(e, errInterrupted)
		}
		resultIndex := len(*results)
		*results = append(*results, r)
		if saveError := saveResults(saved.Output, *results); saveError != nil {
			return saveError
		}
		if e != nil {
			s.say("Stopped: %v", e)
		}
		dismissed := false
		if e == nil && !s.quit && s.writes != s.inspected {
			// Query-only cases still need inspection; a final case pause already
			// inspected visual output. Cleanup writes do not create a new stage.
			s.opts = opts
			dismissed = s.dismissiblePause("Probe complete. Space/Enter: menu | q/Esc: exit test")
			s.opts = saved
		}
		if e == nil && !s.quit && !dismissed && saved.Assess {
			assessments := []menuEntry{{"Unassessed", "Keep the result without a visual judgment."}, {"Matches expected", "Record a human observation; this does not assert broad feature support."}, {"Differs from expected", "Record that the visible result differs from this case's expectation."}, {"Unsure", "Record that a human checked but could not determine the result."}}
			if assessment, ok := b.choose("Assessment / "+caseTrail(c), assessments, 0); ok {
				switch assessment {
				case 1:
					r.Outcome = "observed"
					r.Observation = "Tester reports expected appearance/behavior"
				case 2:
					r.Outcome = "observed"
					r.Observation = "Tester reports a difference from expected appearance/behavior"
				case 3:
					r.Observation = "Tester unsure"
				}
			}
		}
		(*results)[resultIndex] = r
		if e := saveResults(saved.Output, *results); e != nil {
			return e
		}
		selected = 0
		notice = "Last run: " + r.Outcome
		if e == nil && !s.quit && !saved.Assess {
			notice = "Last run: completed"
		}
	}
}
func run(args []string) (code int) {
	defer func() {
		if p := recover(); p != nil {
			if f, ok := p.(probeFailure); ok {
				fmt.Fprintln(os.Stderr, f.err)
				code = 1
				if errors.Is(f.err, errInterrupted) {
					code = 130
				}
			} else {
				panic(p)
			}
		}
	}()
	if len(args) == 2 && args[0] == "features" && args[1] == "--json" {
		json.NewEncoder(os.Stdout).Encode(features)
		return 0
	}
	if len(args) > 0 && (args[0] == "list" || args[0] == "--list") {
		if len(args) == 2 && args[1] == "--json" {
			type entry struct {
				Path     string   `json:"path"`
				Features []string `json:"feature_ids"`
				ID       string   `json:"case_id"`
				Command  string   `json:"command"`
			}
			entries := []entry{}
			for _, c := range cases {
				entries = append(entries, entry{c.Path, c.Features, c.ID, caseCommand(c)})
			}
			json.NewEncoder(os.Stdout).Encode(entries)
		} else {
			listFeatures()
		}
		return 0
	}
	if len(args) > 0 && (args[0] == "--help" || args[0] == "help" || args[0] == "-h") {
		fmt.Print("probe [FEATURE-SLUG [CASE-ID]] [OPTIONS]\nNo command opens curated feature navigation. A slug opens its test scenarios.\nUse probe SLUG CASE-ID --help for expected behavior and settings.\nUse probe emoji for emoji sections (or probe emoji flags / all).\nUse probe features for a flat slug browser; probe cases for the old command hierarchy.\nUse --assess for visual judgments; --output FILE for optional JSON evidence.\n\n")
		listFeatures()
		return 0
	}
	// Human-facing topic shortcut; handlers and canonical feature/case IDs stay shared.
	if len(args) > 0 && args[0] == "emoji" {
		args = append([]string{"text", "emoji"}, args[1:]...)
	}
	legacy := false
	if len(args) > 0 && args[0] == "cases" {
		legacy = true
		args = args[1:]
	}
	if len(args) > 0 && args[0] == "feature" {
		args = args[1:]
		if len(args) == 0 {
			fmt.Fprintln(os.Stderr, "feature requires a TDN slug")
			return 2
		}
	}
	n := 0
	for n < len(args) && !strings.HasPrefix(args[n], "-") {
		n++
	}
	path := strings.Join(args[:n], " ")
	var selected *Case
	var feature *Feature
	flat := path == "features"
	if n > 0 {
		feature = featureByID(args[0])
	}
	if feature != nil {
		if n > 2 {
			fmt.Fprintln(os.Stderr, "expected a feature slug and optional case ID")
			return 2
		}
		if n == 2 {
			for _, c := range featureCases(feature.ID) {
				if c.ID == args[1] {
					copy := c
					selected = &copy
					break
				}
			}
			if selected == nil {
				fmt.Fprintln(os.Stderr, "case does not exercise feature:", path)
				return 2
			}
		}
		if selected == nil && slices.Contains(args[n:], "--help") {
			listFeatureCases(*feature)
			return 0
		}
	} else if !flat && (path != "" || legacy) {
		found := matching(path)
		if len(found) == 0 {
			fmt.Fprintf(os.Stderr, "unknown feature or probe %q; use probe list\n", path)
			return 2
		}
		legacy = true
		if len(found) == 1 && found[0].Path == path {
			selected = &found[0]
		}
		if selected == nil && slices.Contains(args[n:], "--help") {
			list(path)
			return 0
		}
	}
	if flat && slices.Contains(args[n:], "--help") {
		listFeatures()
		return 0
	}
	opts, err := parseOptions(selected, args[n:])
	if err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return 0
		}
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	// Explicit spawning has no need to acquire the caller's terminal.
	if selected != nil && selected.Path == "colors palette spawn" {
		return spawnPalette(opts)
	}
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM, syscall.SIGHUP)
	defer cancel()
	// After the first signal, restore default signal handling so a second
	// signal can terminate the process even if cleanup is blocked on output.
	go func() { <-ctx.Done(); cancel() }()
	s, err := openSession(ctx, opts)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	defer func() {
		if e := s.close(); e != nil {
			fmt.Fprintln(os.Stderr, "restore tty:", e)
			code = 1
		}
	}()
	results := []Result{}
	if selected == nil {
		if legacy {
			err = menu(s, path, &results)
		} else {
			b := newBrowser(s)
			defer b.leave()
			if feature != nil {
				err = navigateBrowser(b, func() error { return browseFeature(b, *feature, &results) }, &results)
			} else {
				err = navigateBrowser(b, func() error { return browseFeatureTree(b, nil, flat, &results) }, &results)
			}
		}
	} else {
		var r Result
		r, err = runCase(s, *selected, args[n:])
		results = append(results, r)
	}
	if e := saveResults(opts.Output, results); e != nil {
		err = errors.Join(err, e)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		if errors.Is(err, errInterrupted) {
			return 130
		}
		return 1
	}
	return 0
}
func main() { os.Exit(run(os.Args[1:])) }

func commandText(path string, args []string) string {
	out := path
	for _, arg := range args {
		if !safePayload(arg) {
			out += " $'"
			for _, b := range []byte(arg) {
				out += fmt.Sprintf("\\%03o", b)
			}
			out += "'"
		} else {
			out += " '" + strings.ReplaceAll(arg, "'", "'\"'\"'") + "'"
		}
	}
	return out
}
