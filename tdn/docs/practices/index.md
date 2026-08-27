# Practices

The sections before this one document what a standard, a DEC manual, or an
emulator says a sequence does. This section documents what nobody wrote
down: techniques that work across emulators because everyone converged on
them, not because a document required it.

Everything here carries the status **Convention**. A practice page cites
the feature pages it builds on and the emulators it was checked against;
it does not cite a specification, because there is none. If one appears,
the material moves to the relevant feature page and this section links to
it.

## Pages

- [Multiplexers](multiplexers.md): how tmux and screen intercept
  sequences, and how to pass one through to the outer terminal.

## Candidates

Material that belongs here and is currently spread across feature pages or
not yet written:

- querying a terminal without blocking on one that never answers
  (partly in [Queries](../csi/queries.md#application-requirements));
- restoring terminal state on exit and on crash
  (partly in [Modes](../csi/modes.md#ownership-and-cleanup));
- detecting an emulator when `TERM` lies
  (partly in [Environment](../environment.md#guesswork));
- choosing a terminator and an escape form that survives every hop;
- feature negotiation order at startup: which queries to send, in what
  order, and what to do with each answer.
