---
type: Report
title: "Packet read failure stabilization"
description: "Why exact body consumption alone did not refuse a malformed body, and the stabilization that followed PR #86."
tags: [protocol, report]
---

# Packet read failure stabilization

Date: 2026-09-05

## Problem and reproduction

PR #86 established packet body boundaries. The running client was subsequently
smoke-tested by the maintainer and the PR merged. A follow-up test of malformed
bodies found that exact consumption alone was insufficient: legacy decoders such
as `SubItemInfo::read` catch `Throwable`, print the error, and return normally.
If a missing field starts exactly at the declared body's end, that error is
swallowed while the stream has zero bytes remaining. The framed read then
accepts a partially decoded object.

New tests were run against the unchanged production code before applying this
fix. Both `GCAddNewItemToZone` and `GCAddInstalledMineToZone` accepted five short
body lengths per encryption code: 25, 29, 30, 32, and 33 bytes of a valid 34-byte
fixture. Across codes 0 through 5, that was 60 incorrectly accepted frames.
These two registered packet types share the `GCAddItemToZone` base decoder.
A separate synthetic decoder reproduced swallowed failures in eleven distinct
stream operations, both before and after consuming the valid body byte.

## Contract and scope

`SocketInputStream` now remembers a failed stream operation for the duration of
the current framed read. Catching that operation's exception or successfully
reading the remaining bytes cannot clear the failure. The outer framed read
rejects the packet before dispatch even when the decoder returns normally.

The failure state covers the stream's read, destination-span validation, string,
peek, skip, and nested framed-read guards. Typed and encrypted scalar reads
delegate to these paths. Shared cleanup discards only the unread declared body
and resets both the body boundary and failure state on every exit. The next
frame stays aligned and can be parsed independently; connection error policy
remains with the receive loop.

Outside a framed read, insufficient data and unsuccessful header peeks retain
their existing fragmentation semantics. No packet field, ID, size declaration,
golden file, or factory registration changes. The legacy decoder catch blocks
and ownership conventions remain as they were.

This is a stream-failure guarantee. It cannot detect a validation exception
that a decoder both creates and swallows without calling a failing stream
operation. Removing those catches and auditing ownership during partial
construction remain separate work.

## Verification

- The real-packet tests exercise every shorter body prefix for both item types
  and all six encryption codes: 408 malformed frames and 408 transport fragments.
  Each malformed frame is followed by a valid frame on the same wrapped ring;
  reserialization must reproduce the original valid frame byte-for-byte.
- The eleven-operation matrix proves that a later successful read cannot clear
  failure, nested reads cannot reset it, and the following frame still parses.
- A receive-loop regression checks factory-created packet destruction, zero
  handler calls for a swallowed failure, and successful dispatch of the next
  valid packet.
- Full target builds and all six CTest entries passed in Debug, Release, and
  ASan Debug. The final suite contains 411 tests and 10,180 checks, with no
  failures. The receive-loop fixture registers its shared handler once per
  process, matching the dispatcher's startup-only registration contract.
- The client/server wire inventory comparison passed, and all 132 shared golden
  files remain byte-identical. The staged diff passed its whitespace check.
- An independent Sol/xhigh review checked the stream lifecycle, compatibility,
  regression tests, and the shared fixture setup correction.

The game has not been launched for this change. The prior runtime report applies
to PR #86, not this follow-up.
