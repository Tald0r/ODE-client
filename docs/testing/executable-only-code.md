---
type: Rule
title: "Executable-only code"
description: "Code compiled straight into DarkEden cannot be linked by the unit binary, so it is verified with builds and the automated checks; live-server verification is optional and never a merge or completion gate."
tags: [testing, process]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# Executable-only code

**Executable-only code cannot be linked by the unit binary.** Code compiled straight into
the `DarkEden` executable has no test binary that can reach it. Since task 4.265 (2026-10-19) that
is `Client/SDLMain.cpp` alone, and the rest of the game, every packet handler among it, is library
code ([library boundaries](../architecture/library-boundaries.md)). Verify changes there with
builds and the available automated regression checks: both Debug trees build, the suite is green
in a plain tree and an ASan tree, `wire_inventory_fresh` passes, and the applicable CI checks
pass.

**Live-server verification is optional and must not block a merge or completion** (user
instruction, 2026-09-17); do not request a runtime report as merge approval, and do not launch
`DarkEden.exe` yourself unless asked ([running the client](../operations/running-the-client.md)).
The ten defects under *Runtime defects* in the code-health review were found against a live
server, and none were reachable from a test binary. The ones in the short table below them were
found by *reading*, during remediation passes, and are filed separately for exactly that reason:
the heading is a claim about how a defect was found, not a bin for anything executable-side.
