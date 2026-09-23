//go:build unix

package main

import (
	"os/exec"
	"syscall"
)

// A launch gets its own process group so one signal reaches the terminal and its children.
func isolateLaunch(cmd *exec.Cmd) { cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true} }

func signalLaunch(cmd *exec.Cmd, pids []int, kill bool) {
	sig := syscall.SIGTERM
	if kill {
		sig = syscall.SIGKILL
	}
	if cmd.Process != nil {
		_ = syscall.Kill(-cmd.Process.Pid, sig)
	}
	for _, pid := range pids {
		_ = syscall.Kill(pid, sig)
	}
}
