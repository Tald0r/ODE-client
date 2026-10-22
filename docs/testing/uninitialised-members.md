---
type: Rule
title: "Uninitialised members are reproduced, not guarded"
description: "/RTC1 fills stack locals with 0xCC and the CRT fills the heap with 0xCD, but neither flags an uninitialised member, so a test reads the fill pattern out of a fresh object rather than waiting for a crash."
tags: [testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Uninitialised members are reproduced, not guarded

**Uninitialised members are reproduced, not guarded.** `/RTC1` fills stack locals with `0xCC`
and the CRT fills heap with `0xCD`, so a test that reads a fresh object fails deterministically;
but neither `/RTC1` nor ASan flags an uninitialised *member*, so the test reads the fill pattern
rather than waiting for a crash. Every `gamemodel` slice found at least one constructor that set
every field but a few ([the restructuring plan](../reviews/RESTRUCTURING.md#what-the-review-rounds-settled)).
