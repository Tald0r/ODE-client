---
type: Playbook
title: Builds and tests
description: Choose the existing verification path and distinguish automated coverage from gameplay evidence.
tags: [client, build, testing, ci]
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:27:48Z"
source_revision: eabcf17f23cafebf9b60825edfc33a27e544820c
sources:
  - id: setup
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/README.md
  - id: presets
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CMakePresets.json
  - id: native-verification
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/tools/ci/verify-linux.sh
  - id: test-registration
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/tests/CMakeLists.txt
  - id: working-brief
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CLAUDE.md
  - id: browser-guide
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/webgl-client.md
---

# Builds and tests

[README](../../README.md) owns prerequisites and reproducible native setup;
[CMakePresets.json](../../CMakePresets.json) owns preset names. Windows Debug and
ASan use separate configurations because `/RTC1` and AddressSanitizer cannot be
combined. Linux and macOS have their own compiler/sanitizer presets.[^setup][^presets]

Use the repository verification scripts for a full check: Windows has
`tools/ci/verify-windows.ps1`; Linux and macOS use
`tools/ci/verify-linux.sh <preset>`. The latter checks test registration as well
as building and running tests, since an empty CTest inventory can exit
successfully. Compiler warnings also have an enforced budget.[^setup][^native-verification]

`tests/CMakeLists.txt` registers compiled tests and source checks, including
architecture, wire inventory and ratchets. Bash and Perl availability affects
registration; inspect the actual inventory instead of treating a skipped check as
a pass. For a code fix, reproduce the failure in its owning library test, then
run the applicable preset checks.[^test-registration][^working-brief]

[Browser verification](../webgl-client.md#verification) adds renderer and transport
probes, asset tests and smoke entry points. The transport fixture needs neither an
account nor a database; passing it does not establish an authenticated gameplay
session. Keep run-specific results and limitations with the change, rather than
copying volatile test totals into this map.[^browser-guide]

See [architecture](architecture.md) to locate a test owner and
[browser login/logout](browser-login-logout.md) for flow diagnostics.

[^setup]: [README.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/README.md)
[^presets]: [CMakePresets.json](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CMakePresets.json)
[^native-verification]: [tools/ci/verify-linux.sh](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/tools/ci/verify-linux.sh)
[^test-registration]: [tests/CMakeLists.txt](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/tests/CMakeLists.txt)
[^working-brief]: [CLAUDE.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CLAUDE.md)
[^browser-guide]: [docs/webgl-client.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/webgl-client.md)
