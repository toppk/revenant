package main

import (
	"flag"
	"fmt"
	"io"
	"slices"
	"strconv"
	"strings"
)

func commonSettings(o Options) []string {
	return []string{fmt.Sprintf("--timeout=%g", o.Timeout), fmt.Sprintf("--delay=%g", o.Delay), fmt.Sprintf("--no-pause=%t", o.NoPause)}
}

// Labels describe the input to the test. Defaults, parsing and validation stay
// in the same flag definitions used by the command-line interface.
var settingLabels = map[string]string{
	"target": "Target", "text": "Text", "bel": "Use BEL terminator", "cap": "Capabilities",
	"color": "Color", "index": "Palette index", "palette": "Palette", "seconds": "Capture time (s)",
	"mode": "Mode", "styles": "Cycle cursor styles", "cwd": "Directory", "font": "Font name",
	"regime": "Width mode", "frames": "Frames", "frame-ms": "Drawing time (ms)", "hold-ms": "Extra hold (ms)",
	"pause-ms": "Between frames (ms)", "program": "Terminal executable", "geometry": "Window geometry",
	"xrm": "X resources", "timeout": "Reply timeout (s)", "delay": "Stage delay (s)",
	"no-pause": "Skip stage pauses",
}

func settingChoices(c Case, name string) []string {
	switch name {
	case "target":
		switch {
		case strings.HasPrefix(c.Path, "clipboard "):
			return []string{"clipboard", "primary", "select"}
		case strings.HasPrefix(c.Path, "titles "):
			return []string{"both", "title", "icon"}
		case strings.HasPrefix(c.Path, "colors dynamic "):
			return []string{"background", "foreground", "cursor", "all"}
		}
	case "regime":
		return []string{"both", "legacy", "cluster"}
	case "palette":
		return []string{"verify", "xterm", "tango", "solarized", "gruvbox", "nord"}
	case "mode":
		if c.Path == "input mouse" {
			return []string{"1000", "1002", "1003", "9"}
		}
		return []string{"compare", "off", "on"}
	}
	return nil
}
func settingArgs(args []string, name string, values []string) []string {
	result := []string{}
	for _, arg := range args {
		if !strings.HasPrefix(arg, "--"+name+"=") {
			result = append(result, arg)
		}
	}
	for _, value := range values {
		result = append(result, "--"+name+"="+value)
	}
	return result
}
func flagValues(f *flag.Flag) []string {
	if values, ok := f.Value.(*stringsFlag); ok {
		return append([]string(nil), (*values)...)
	}
	return []string{f.Value.String()}
}
func (b *Browser) settings(c Case, args []string) []string {
	edited := append([]string(nil), args...)
	names := append(append([]string(nil), c.Options...), "timeout", "delay", "no-pause")
	selected, notice := 0, ""
	for {
		fs, _ := caseFlags(&c, io.Discard)
		if err := fs.Parse(edited); err != nil {
			panic(err)
		} // generated, previously validated arguments
		entries := []menuEntry{}
		for _, name := range names {
			f := fs.Lookup(name)
			value := strings.Join(flagValues(f), ", ")
			if value == "" {
				value = "(empty)"
			}
			entries = append(entries, menuEntry{settingLabels[name] + ": " + value, notice + "Current: " + strconv.Quote(strings.Join(flagValues(f), ", ")) + "\n\n" + f.Usage + "\n\nDefault: " + strconv.Quote(f.DefValue) + "\nCommand-line flag: --" + name})
		}
		entries = append(entries, menuEntry{"Done", "Use these settings for this case."}, menuEntry{"Reset defaults", "Restore case defaults and the browser's pacing settings."}, menuEntry{"Cancel", "Discard these setting changes."})
		choice, ok := b.choose(caseTrail(c)+" / Settings", entries, selected)
		if !ok || choice == len(names)+2 {
			return args
		}
		selected, notice = choice, ""
		if choice == len(names) {
			return edited
		}
		if choice == len(names)+1 {
			edited = commonSettings(b.s.opts)
			selected = 0
			continue
		}
		name := names[choice]
		f := fs.Lookup(name)
		values := flagValues(f)
		if _, repeatable := f.Value.(*stringsFlag); repeatable {
			values = b.settingList(settingLabels[name], values)
		} else if boolean, ok := f.Value.(interface{ IsBoolFlag() bool }); ok && boolean.IsBoolFlag() {
			values = []string{strconv.FormatBool(f.Value.String() != "true")}
		} else if choices := settingChoices(c, name); len(choices) > 0 {
			rows := []menuEntry{}
			for _, value := range choices {
				rows = append(rows, menuEntry{value, f.Usage})
			}
			picked, ok := b.choose(caseTrail(c)+" / "+settingLabels[name], rows, max(0, slices.Index(choices, values[0])))
			if !ok {
				continue
			}
			values = []string{choices[picked]}
		} else {
			value, ok := b.edit(caseTrail(c)+" / "+settingLabels[name], values[0])
			if !ok {
				continue
			}
			values = []string{value}
		}
		candidate := settingArgs(edited, name, values)
		if _, err := parseOptionsTo(&c, candidate, io.Discard); err != nil {
			notice = "Setting unchanged: " + err.Error() + "\n"
		} else {
			edited = candidate
		}
	}
}
func (b *Browser) settingList(title string, original []string) []string {
	values := append([]string(nil), original...)
	selected := 0
	for {
		entries := []menuEntry{}
		for _, value := range values {
			entries = append(entries, menuEntry{value, "Edit or remove this value."})
		}
		entries = append(entries, menuEntry{"Add value", "Add another value; spaces are preserved."}, menuEntry{"Done", "Use this list."}, menuEntry{"Cancel", "Discard list changes."})
		choice, ok := b.choose(title, entries, min(selected, len(entries)-1))
		if !ok || choice == len(values)+2 {
			return original
		}
		selected = choice
		if choice == len(values)+1 {
			return values
		}
		if choice == len(values) {
			if value, ok := b.edit(title+" / Add", ""); ok {
				values = append(values, value)
			}
			continue
		}
		action, ok := b.choose(title+" / Value", []menuEntry{{"Edit", values[choice]}, {"Remove", "Remove this value."}, {"Back", "Return to the list."}}, 0)
		if !ok {
			continue
		}
		if action == 1 {
			values = append(values[:choice], values[choice+1:]...)
		} else if action == 0 {
			if value, ok := b.edit(title+" / Edit", values[choice]); ok {
				values[choice] = value
			}
		}
	}
}
