---
type: Report
title: "Modernization stabilization slices"
description: "Three slices after the C++20 build, span I/O and typed wire scalar work: repeatable verification and bounded input handling."
tags: [report, build]
---

# Modernization stabilization slices

Date: 2026-09-05

Verification policy updated 2026-09-17: per the user's instruction, live-server
checks are optional and do not block merging or completion. Automated build,
test and wire checks remain required; historical runtime results below are not
claims that additional runtime testing was performed.

These three slices follow the C++20 build, span I/O, and typed wire scalar work.
They concentrate on repeatable verification and bounded input handling. Strict
second-compiler conformance, wider fuzzing, timing/rendering extraction, and
startup diagnostics remain separate follow-ups.

## Build verification

`CMakePresets.json` defines a VS2022 x64 tree for Debug and Release and a separate
ASan Debug tree. All presets enable tests and require Bash and Perl. CMake 3.21
is the minimum because that version introduced the VS2022 generator and supports
the checked-in preset schema.

`vcpkg.json` declares the six existing direct dependencies and pins the baseline
to the version set installed on the development machine when this work began.
The feature selections preserve the existing dependency set. CI checks out the
matching vcpkg revision before bootstrapping. Preset trees share their manifest
installation, so configure them sequentially on one machine.

Owner: `tools/ci/verify-windows.ps1` builds all configured targets, checks that
the vcpkg checkout matches the baseline and the six required CTest entries exist,
and runs them. The Windows Actions workflow
runs ordinary Debug/Release and ASan Debug as two jobs and retains logs and test
reports. Automatic runs are limited to pushes to `master`, including merged PRs,
to conserve Actions minutes; manual dispatch remains available. PR updates do
not trigger CI. The ordinary Debug configuration retains `/RTC1`; ASan uses its own
configuration and runtime. The workflow does not launch the client or fetch game
assets. Hosted verification runs after merge; local automated verification
provides the pre-merge checks. Live-server smoke testing is optional.

## UI text preparation

The line editor no longer stages UTF-8 cursor prefixes or password masks in
1,024-byte stack arrays. `UISafeText.h` provides dynamic storage and the shared
editor decoder. Password masking preserves the existing one-star-per-byte
behavior. Cursor prefixes include complete decoded characters; the old loop
stopped after the final lead byte of a multibyte character. Decoder rejection
rules and legacy string encodings are preserved.

Tooltip name construction uses strings and one shared construction function for
measurement and drawing. The renderer's option selection and masking determine
the measured text too, including a third option and valid grade suffixes. Grade
tokens retain their legacy two-byte indexing but are appended only when both
bytes exist. The grade-table pointer is read afresh instead of cached across
table lifetimes.

Owner: `tests/unit/test_ui_safe_text.cpp` covers long and multibyte strings,
supplementary characters, truncated UTF-8, output capacity, and grade-table
boundaries. These are regression guards; the original widget overflows were
identified from the source and were not reproduced through a running game.
Visual checks of text editing and tooltips are optional runtime diagnostics.

## Packet framing

Destination spans alone do not establish a packet body boundary. The framed
stream read now preflights the complete header and declared body before consuming
anything. During parsing, reads, peeks, skips, and visible length are constrained
to that body. Acceptance requires exact body consumption.

A fragmented transport frame remains untouched for a later fill. A complete
frame whose parser fails is rejected and its unread declared body is discarded,
leaving subsequent frames aligned. An `InsufficientDataException` thrown during
parsing a complete body becomes a protocol error, so receive loops cannot mistake
it for transport fragmentation. Existing connection-level error policy remains
in the callers. Packet ownership in receive loops uses RAII through parsing,
dispatch, and successful transfer into packet history.

Owners: malformed-frame tests in `test_packetwire_parsers.cpp`, receive-loop
tests, and the framed round trips added to `test_packet_goldens.cpp`. Existing
golden files and the wire-layout inventory must remain byte-identical. Exact
consumption can expose pre-existing reader/writer disagreements in packet
families without fixtures. Live-server checks of login, zone entry, inventory/shop
operations, chat and reconnect are optional, not a merge gate.

## Validation performed

- Full target builds and all six CTest entries passed in ordinary Debug,
  Release, and ASan Debug. The final suite contains 407 tests and 5,946 checks.
  Ordinary Debug was rebuilt cleanly after the stream layout settled.
- The malformed-frame overread was also reproduced against the unchanged
  pre-change library: the parser accepted a one-byte body while reading two
  bytes, leaving seven bytes of the following eight-byte frame.
- Framed golden round trips cover 137 decodes across 29 packet classes.
  The client/server wire inventories agree, and all 132 golden files shared
  by the two local checkouts are byte-identical.
- Fresh manifest-backed configuration passed. Configuration and preset parsing
  also passed with portable CMake 3.21.7; the full builds used CMake 4.4.3.
- Missing Bash and missing Perl each failed strict configuration. A mismatched
  vcpkg checkout failed the verification script before configuration.
- The Actions workflow passed actionlint, and the PowerShell script was executed
  locally with Windows PowerShell 5, including native warning handling.

The initial hosted Actions runs were canceled to conserve minutes; hosted
verification remains pending after merge. The in-game smoke test has not been
performed. No merge or deployment is part of this change.
