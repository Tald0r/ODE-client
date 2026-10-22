---
type: Rule
title: "The vendored de-core"
description: "third_party/decore is a byte-identical copy of the server's src/domain, resynced with tools/decore/sync.pl and never edited by hand; a shared formula proves parity of the formula, not of the gameplay around it."
tags: [architecture, protocol, server]
enforced_by: [decore_vendored, decore_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# The vendored de-core

**`third_party/decore` is the server's, not ours.** It is a static library of the server's shared
rules: a byte-identical copy of the subset of the server's `src/domain` that the client calls,
which `gamemodel` links. It holds the item price, repair price and maximum durability, the grade
policy, the per-race stat rules, the equip requirements, the castle tax, the per-skill output
formulas (`SkillOutputFormulas`, of which the client calls Will of Life's) and a slayer skill's
range ([restructuring plan](../reviews/RESTRUCTURING.md) task 4.12).

Change a shared formula in the server, then run `tools/decore/sync.pl <server-root>`; never edit
or format the copied files by hand. The server lands first, and
[third_party/decore/README.md](../../third_party/decore/README.md) owns the sync procedure and
names the server commit the copy came from.

Two ctests keep it honest:

- `decore_vendored` (`sync.pl --verify-manifest`) checks the copy against its `MANIFEST`: the
  hashes match, nothing under `third_party/decore/domain` is unlisted, and the `decore` target
  builds every vendored `.cpp`. A hand edit fails it; resync instead.
- `decore_tests` asserts the shared parity vectors (the `.tsv` rows under `domain/vectors/`) on
  this toolchain. A changed row is a balance change: review the `.tsv` diff as one.

**A shared formula does not prove equivalent gameplay.** Compare the client call site's inputs,
the server caller, the data tables and the surrounding gates. Task 4.12 in the restructuring plan
tracks the residual differences; find the callable seam ([library boundaries](library-boundaries.md))
before adding a parity test.
