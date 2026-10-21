---
type: Rule
title: "Restructuring status lines"
description: "Every task in the restructuring plan has a checkbox and a Status line updated in the same commit as the work; a task is done only once its Owner exists, and a substantial task gets an adversarial review by two reviewers before merge."
tags: [process]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Restructuring status lines

The [restructuring plan](../reviews/RESTRUCTURING.md) is complete as of 2026-09-09, and what it
leaves behind is machinery, not a to-do list; a later slice of any kind reads its *What the review
rounds settled* section first. Its conventions still govern a task that extracts a seam:

- Every task has a checkbox and a `> **Status:**` line, updated **in the same commit** as the
  work it describes. Allowed values: `not started` | `in progress (<what remains>)` |
  `done (<commit>)` | `dropped (<why>)`.
- A status line records the **current** state and what the next reader needs to act: the owner,
  the conventions the reviews settled, the open bugs, what remains. The per-change narrative
  belongs in the PR description and the commit message, not here.
- A task is only `done` when its **Owner** exists: the test or mechanism that keeps the rule true
  from then on. Landing the change without the owner is `in progress (owner missing)`.
- [Ratchet](../testing/ratchets.md) numbers only go down, re-measured before and after, with the
  updated number committed with the change.
- Substantial tasks get an **adversarial review** before merge (two reviewers; every round so far
  has found real defects in the change itself).
- Verification for every extraction: both Debug trees build, the suite is green in a plain tree
  and an ASan tree, `wire_inventory_fresh` passes, and the applicable CI checks pass. Live-server
  verification is optional ([executable-only code](../testing/executable-only-code.md)).
