//go:build !linux

package main

import (
	"errors"
	"os"
	"time"
)

type terminalState struct{}

var unsupportedTTY = errors.New("interactive probe currently requires Linux; list/help work on any platform")

func getState(*os.File) (terminalState, error)           { return terminalState{}, unsupportedTTY }
func setState(*os.File, terminalState) error             { return unsupportedTTY }
func makeRaw(*os.File, terminalState) error              { return unsupportedTTY }
func readReady(*os.File, time.Duration) (bool, error)    { return false, unsupportedTTY }
func terminalSize(*os.File) (int, int)                   { return 80, 24 }
func terminalPixels(*os.File) (int, int, int, int, bool) { return 0, 0, 0, 0, false }
