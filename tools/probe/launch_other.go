//go:build !unix

package main

import (
	"os"
	"os/exec"
)

func isolateLaunch(cmd *exec.Cmd) {}

func signalLaunch(cmd *exec.Cmd, pids []int, kill bool) {
	if cmd.Process != nil {
		_ = cmd.Process.Kill()
	}
	for _, pid := range pids {
		if process, err := os.FindProcess(pid); err == nil {
			_ = process.Kill()
		}
	}
}
