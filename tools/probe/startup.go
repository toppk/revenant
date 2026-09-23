package main

import (
	"bytes"
	"context"
	"fmt"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"slices"
	"strconv"
	"strings"
	"syscall"
	"time"
)

const holdMarker = "PROBE-HOLD-END"

// sessionHold is the terminal's direct child: it writes its output, ending without a newline, and exits.
func sessionHold(o Options) int {
	fmt.Fprintf(os.Stdout, "session-hold: child pid %d exits with status %d\n", os.Getpid(), o.Status)
	fmt.Fprintln(os.Stdout, "With hold, this text stays until the window is closed.")
	fmt.Fprintf(os.Stdout, "%s status=%d", holdMarker, o.Status)
	return o.Status
}
func sessionHoldGuide(s *Session) {
	exe, err := os.Executable()
	if err != nil {
		exe = "probe"
	}
	s.page("Hold the window after the child exits")
	s.say("This case must be the terminal's own child, so a shell here cannot run it. Launch:")
	s.say("")
	s.say("  TERMINAL -hold -e %s startup-hold-after-exit session-hold --status 3", exe)
	s.say("")
	s.say("It prints a final %s line without a newline and exits with the chosen status.", holdMarker)
	s.say("Without -hold the window closes; with it, the text stays until the window is closed.")
	s.pause()
}

// Set by the login-shell launcher; the probe the terminal starts records itself and exits.
const argvReportEnv = "PROBE_ARGV_REPORT"

// argvReport writes the executable and argv this process was started with.
func argvReport(path string) int {
	exe, _ := os.Readlink("/proc/self/exe")
	var report strings.Builder
	fmt.Fprintf(&report, "exe\t%s\n", exe)
	for _, arg := range os.Args {
		fmt.Fprintf(&report, "argv\t%s\n", arg)
	}
	if os.WriteFile(path+".tmp", []byte(report.String()), 0o600) != nil || os.Rename(path+".tmp", path) != nil {
		return 1
	}
	return 0
}

// Marks every process a launch starts, so cleanup also finds those that left its process group.
const launchMarkEnv = "PROBE_LOGIN_SHELL_LAUNCH"

// Established before any override, so inherited resources cannot decide loginShell.
var loginShellBaseline = []string{"-xrm", "*loginShell: false"}

type loginShellScenario struct {
	name string
	args []string
	want []string
}

func loginShellScenarios(exe string) []loginShellScenario {
	name := filepath.Base(exe)
	return []loginShellScenario{
		{"default", nil, []string{name}},
		{"-ls", []string{"-ls"}, []string{"-" + name}},
		{"+ls", []string{"+ls"}, []string{name}},
		{"loginShell resource", []string{"-xrm", "*loginShell: true"}, []string{"-" + name}},
		{"+ls over the resource", []string{"-xrm", "*loginShell: true", "+ls"}, []string{name}},
		{"-ls with -e", []string{"-ls", "-e", exe, "e-argument"}, []string{exe, "e-argument"}},
	}
}

// loginShellEnv drops the caller's resource files and HOME, and marks the launch.
func loginShellEnv(home, exe, report, mark string) []string {
	var env []string
	for _, entry := range os.Environ() {
		switch name, _, _ := strings.Cut(entry, "="); name {
		case "HOME", "SHELL", "XENVIRONMENT", "XFILESEARCHPATH", "XUSERFILESEARCHPATH", "XAPPLRESDIR", argvReportEnv, launchMarkEnv:
			continue
		}
		env = append(env, entry)
	}
	return append(env, "HOME="+home, "XENVIRONMENT=/dev/null", "XFILESEARCHPATH=/dev/null",
		"XUSERFILESEARCHPATH=/dev/null", "XAPPLRESDIR=/dev/null", "SHELL="+exe,
		argvReportEnv+"="+report, launchMarkEnv+"="+mark)
}

// markedProcesses lists processes whose environment carries mark; none where /proc is absent.
func markedProcesses(mark string) []int {
	entries, err := os.ReadDir("/proc")
	if err != nil {
		return nil
	}
	needle := []byte("\x00" + launchMarkEnv + "=" + mark + "\x00")
	var pids []int
	for _, entry := range entries {
		pid, err := strconv.Atoi(entry.Name())
		if err != nil || pid == os.Getpid() {
			continue
		}
		data, err := os.ReadFile(filepath.Join("/proc", entry.Name(), "environ"))
		if err == nil && bytes.Contains(append(append([]byte{0}, data...), 0), needle) {
			pids = append(pids, pid)
		}
	}
	return pids
}

// stopLaunch ends the terminal and everything it started: TERM, then KILL, each bounded.
func stopLaunch(cmd *exec.Cmd, exited <-chan struct{}, mark string) error {
	for _, kill := range []bool{false, true} {
		signalLaunch(cmd, markedProcesses(mark), kill)
		deadline := time.Now().Add(time.Second)
		for time.Now().Before(deadline) {
			select {
			case <-exited:
				if len(markedProcesses(mark)) == 0 {
					return nil
				}
			default:
			}
			time.Sleep(20 * time.Millisecond)
		}
	}
	if survivors := markedProcesses(mark); len(survivors) != 0 {
		return fmt.Errorf("launched processes survived: %v", survivors)
	}
	return nil
}

// checkLoginShellReport requires one exe line naming this probe and the exact expected argv.
func checkLoginShellReport(data []byte, exe string, want []string) error {
	var reported string
	var argv []string
	exes := 0
	for _, line := range strings.Split(strings.TrimSuffix(string(data), "\n"), "\n") {
		field, value, ok := strings.Cut(line, "\t")
		switch {
		case ok && field == "exe":
			exes++
			reported = value
		case ok && field == "argv":
			argv = append(argv, value)
		default:
			return fmt.Errorf("malformed report line %q", line)
		}
	}
	if exes != 1 || len(argv) == 0 {
		return fmt.Errorf("malformed report: %d exe lines, %d argv lines", exes, len(argv))
	}
	if reported != exe {
		return fmt.Errorf("executable %q, not %q", reported, exe)
	}
	if !slices.Equal(argv, want) {
		return fmt.Errorf("argv %q, not xterm-411's %q", argv, want)
	}
	return nil
}

// loginShellLaunch starts the terminal once per scenario with SHELL set to this probe. It fails
// on a missing, malformed or mismatched report and on any launched process left behind.
func loginShellLaunch(o Options) int {
	exe, err := os.Executable()
	if err == nil {
		exe, err = filepath.EvalSymlinks(exe)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	dir, err := os.MkdirTemp("", "probe-login-shell")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	defer os.RemoveAll(dir)
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM, syscall.SIGHUP)
	defer cancel()
	fmt.Printf("Launching %s with SHELL=%s. Each child records its executable and argv, then exits.\n", o.Program, exe)
	fmt.Printf("Resources are isolated (HOME and X resource files replaced); every launch starts with %s.\n", strings.Join(loginShellBaseline, " "))
	for _, xrm := range o.XRM {
		fmt.Printf("Deliberate override after the baseline: -xrm %q; expectations assume it leaves loginShell alone.\n", xrm)
	}
	fmt.Println("Expected: this executable, argv[0] its basename, dashed for a login shell; -e runs as given (xterm-411).")
	failures := 0
	for index, scenario := range loginShellScenarios(exe) {
		report := filepath.Join(dir, fmt.Sprintf("report%d", index))
		home := filepath.Join(dir, fmt.Sprintf("home%d", index))
		if err := os.Mkdir(home, 0o700); err != nil {
			fmt.Fprintln(os.Stderr, err)
			return 1
		}
		args := append([]string{}, loginShellBaseline...)
		for _, xrm := range o.XRM {
			args = append(args, "-xrm", xrm)
		}
		args = append(args, scenario.args...)
		mark := fmt.Sprintf("%d.%d", os.Getpid(), index)
		cmd := exec.Command(o.Program, args...)
		cmd.Env = loginShellEnv(home, exe, report, mark)
		isolateLaunch(cmd)
		fmt.Printf("%s: %s %s\n", scenario.name, o.Program, strings.Join(args, " "))
		if err := cmd.Start(); err != nil {
			fmt.Fprintln(os.Stderr, err)
			return 1
		}
		exited := make(chan struct{})
		go func() { _ = cmd.Wait(); close(exited) }()
		deadline := time.Now().Add(time.Duration(o.Seconds * float64(time.Second)))
		data, readErr := os.ReadFile(report)
		for readErr != nil && time.Now().Before(deadline) && ctx.Err() == nil {
			time.Sleep(50 * time.Millisecond)
			data, readErr = os.ReadFile(report)
		}
		stopErr := stopLaunch(cmd, exited, mark)
		if ctx.Err() != nil {
			fmt.Println("  interrupted; the launched terminal and its children were stopped")
			if stopErr != nil {
				fmt.Println("  " + stopErr.Error())
			}
			return 130
		}
		switch {
		case readErr != nil:
			err = fmt.Errorf("no report within %gs: the terminal did not run this probe", o.Seconds)
		default:
			err = checkLoginShellReport(data, exe, scenario.want)
		}
		if err == nil {
			fmt.Printf("  matches xterm-411: exe %s argv %q\n", exe, scenario.want)
		} else {
			failures++
			fmt.Println("  FAIL: " + err.Error())
		}
		if stopErr != nil {
			failures++
			fmt.Println("  FAIL: " + stopErr.Error())
		}
	}
	if failures != 0 {
		fmt.Printf("%d failure(s)\n", failures)
		return 1
	}
	return 0
}

func loginShellGuide(s *Session) {
	exe, err := os.Executable()
	if err != nil {
		exe = "probe"
	}
	s.page("Login-shell invocation")
	s.say("This case launches the terminal itself, with SHELL set to this probe, so a shell here cannot run it.")
	s.say("From an ordinary shell run:")
	s.say("")
	s.say("  %s startup-login-shell login-shell --program TERMINAL", exe)
	s.say("")
	s.say("It prints each started child's executable and argv beside xterm-411's.")
	s.pause()
}
