---
type: Rule
title: "Class splits"
description: "A class split across a library and the executable moves whole, unless it has no virtuals or the library constructs none of the split classes."
tags: [architecture, testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Class splits

**A class split across a library and the executable moves whole.** The split costs nothing when
the class has no virtuals (`MSkillSet`, `TextService`) or when the library constructs none of the
split classes (`MBomb`, `MHolyWater`). Otherwise the split leaves a vtable referencing symbols a
test binary cannot link, so the class moves as one unit, in a [move-then-fix](../process/move-then-fix.md)
commit. A [link proof](../testing/link-proofs.md) that takes the address of a virtual member does
not see the problem, which is why a proof names non-virtual members.
