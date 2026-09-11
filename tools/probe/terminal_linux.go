//go:build linux

package main

import (
	"fmt"
	"os"
	"syscall"
	"time"
	"unsafe"
)

type terminalState syscall.Termios

func ioctl(fd uintptr, request uintptr, arg unsafe.Pointer) error {
	_, _, errno := syscall.Syscall(syscall.SYS_IOCTL, fd, request, uintptr(arg))
	if errno != 0 {
		return errno
	}
	return nil
}
func getState(f *os.File) (terminalState, error) {
	var state terminalState
	err := ioctl(f.Fd(), syscall.TCGETS, unsafe.Pointer(&state))
	return state, err
}
func setState(f *os.File, state terminalState) error {
	return ioctl(f.Fd(), syscall.TCSETS, unsafe.Pointer(&state))
}
func makeRaw(f *os.File, state terminalState) error {
	state.Iflag &^= syscall.IGNBRK | syscall.BRKINT | syscall.PARMRK | syscall.ISTRIP | syscall.INLCR | syscall.IGNCR | syscall.ICRNL | syscall.IXON
	state.Oflag &^= syscall.OPOST
	state.Lflag &^= syscall.ECHO | syscall.ECHONL | syscall.ICANON | syscall.ISIG | syscall.IEXTEN
	state.Cflag &^= syscall.CSIZE | syscall.PARENB
	state.Cflag |= syscall.CS8
	state.Cc[syscall.VMIN] = 1
	state.Cc[syscall.VTIME] = 0
	return setState(f, state)
}
func readReady(f *os.File, wait time.Duration) (bool, error) {
	// Linux poll, avoiding select's descriptor-number limit and word-size layout.
	p := struct {
		FD      int32
		Events  int16
		Revents int16
	}{int32(f.Fd()), 1, 0}
	timeout := syscall.NsecToTimespec(int64(max(wait, 0)))
	_, _, errno := syscall.Syscall6(syscall.SYS_PPOLL, uintptr(unsafe.Pointer(&p)), 1, uintptr(unsafe.Pointer(&timeout)), 0, 0, 0)
	if errno == syscall.EINTR {
		return false, nil
	}
	if errno != 0 {
		return false, errno
	}
	if p.Revents&0x20 != 0 {
		return false, fmt.Errorf("invalid terminal descriptor")
	}
	return p.Revents != 0, nil
}
func terminalSize(f *os.File) (int, int) {
	var size struct{ Rows, Columns, X, Y uint16 }
	if ioctl(f.Fd(), syscall.TIOCGWINSZ, unsafe.Pointer(&size)) != nil || size.Columns == 0 || size.Rows == 0 {
		return 80, 24
	}
	return int(size.Columns), int(size.Rows)
}
