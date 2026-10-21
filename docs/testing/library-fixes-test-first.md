---
type: Rule
title: "Library fixes are test-first"
description: "A library fix starts with a failing test that asserts the observable contract, not the crash, then runs green in both a plain and an ASan tree; a fix that could not be reproduced is called a regression guard, and substantial remediation gets an adversarial review."
tags: [testing, process]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Library fixes are test-first

**Library fixes are written test-first.** Every fix in the remediation table of the
[code-health review](../reviews/code-health-review-2026-08-29.md) was.

**Assert the observable contract, not the crash.** An out-of-bounds read in C++ usually returns
garbage rather than failing an assertion, so a memory-safety test asserts the rejected input or
the preserved value. **Then run the same suite under ASan**, where the invalid access aborts the
process. Both trees green, or the fix is not verified ([build commands](../build/build-commands.md)).
An uninitialised member is reproduced the same way, by reading the fill pattern
([uninitialised members](uninitialised-members.md)).

A fix that could not be reproduced is called a **regression guard** in its commit message, not
described as a reproduction, and every fix commit names its `Test path:`
([commit messages](../process/commit-messages.md)).

Substantial remediation work gets an **adversarial review** afterwards. The last one returned
"significant problems" and found three real defects the fixes had introduced or missed; assume
your own fixes deserve the same scrutiny
([restructuring status lines](../process/restructuring-status-lines.md)).
