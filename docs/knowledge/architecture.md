---
type: Reference
title: Client architecture
description: Find library boundaries, runtime composition, and the owners of architectural checks.
tags: [client, architecture, testing]
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:27:48Z"
source_revision: eabcf17f23cafebf9b60825edfc33a27e544820c
sources:
  - id: build-targets
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CMakeLists.txt
  - id: working-brief
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CLAUDE.md
  - id: restructuring
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/RESTRUCTURING.md
---

# Client architecture

The C++20 client combines SDL rendering/input with legacy game logic. `packetwire`
compiles wire packets and socket support; `gamemodel` holds extracted model logic.
Their source lists live in `tests/arch/packetwire_files.txt` and
`tests/arch/gamemodel_files.txt` and are consumed by CMake. Other libraries include
`basic`, `SpriteLib`, `dxlib`, `framelib`, `TextSystem` and `VS_UI`.[^build-targets]

For a change, first identify the target that owns the implementation. Library
code can be linked by unit tests; executable-only game logic needs an existing
exemption or an extraction into a testable library. A header include or a
zero-valued ratchet alone does not prove that a component links independently.
The restructuring document records the extraction and review rules.[^restructuring]

Runtime dependencies cross named host interfaces such as `Client/Packet/WireHost.h`
and `MItemHost` in `Client/MItem.h`; the executable installs its adapters in
`Client/GameInit.cpp`. Inspect both the host and its installer when changing a
callback, and follow actual callers before declaring a path live or dead.[^working-brief][^restructuring]

Start with [the working brief](../../CLAUDE.md),
[the architecture rules](../RESTRUCTURING.md#what-the-review-rounds-settled),
and [build/test entry points](builds-tests.md). For packets or vendored formulas,
continue with [protocol and shared rules](protocol-shared-rules.md).

[^build-targets]: [CMakeLists.txt](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CMakeLists.txt)
[^working-brief]: [CLAUDE.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/CLAUDE.md)
[^restructuring]: [docs/RESTRUCTURING.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/RESTRUCTURING.md)
