---
type: Reference
title: Protocol and shared rules
description: Locate packet contracts, cross-repository checks, and the server-owned formula source.
tags: [client, protocol, server, decore]
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:27:48Z"
source_revision: eabcf17f23cafebf9b60825edfc33a27e544820c
sources:
  - id: working-brief
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CLAUDE.md
  - id: restructuring
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/RESTRUCTURING.md
  - id: decore-guide
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/third_party/decore/README.md
---

# Protocol and shared rules

Packet implementations live under `Client/Packet`; handler behavior lives under
`Client/PacketHandler`, with the model-only exceptions listed in gamemodel's
membership file. The executable installs wire host services and dispatches packet
handlers. Keep parsing, model mutation and UI/runtime effects distinct when
tracing a failure.[^working-brief]

Wire layouts are pinned by `tests/wire-layout.txt` and golden byte fixtures under
`tests/golden`. A wire layout change needs the identical server change and the
cross-repository inventory comparison described in the restructuring rules.
Do not re-record a failing golden merely to make a test pass. Existing client and
server fixtures can contain different values, so distinguish fixture differences
from layout changes.[^restructuring][^working-brief]

`third_party/decore/domain` is a manifested, byte-identical subset of the server's
`src/domain`. Change shared formulas in the server, then use
`tools/decore/sync.pl <server-root>`; do not edit or format the copied files by
hand. `decore_vendored` checks copy integrity and source membership, while
`decore_tests` checks shared vector results.[^decore-guide]

A shared formula does not prove equivalent gameplay: compare the client call
site's inputs, server caller, data tables and surrounding gates. Task 4.12 in
[the restructuring record](../RESTRUCTURING.md) tracks residual differences;
[the vendoring guide](../../third_party/decore/README.md) owns the sync procedure.
Use [architecture](architecture.md) to find the callable seam before adding a
parity test.[^decore-guide][^restructuring]

[^working-brief]: [CLAUDE.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CLAUDE.md)
[^restructuring]: [docs/RESTRUCTURING.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/RESTRUCTURING.md)
[^decore-guide]: [third_party/decore/README.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/third_party/decore/README.md)
