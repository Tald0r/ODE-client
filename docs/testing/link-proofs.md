---
type: Rule
title: "Link proofs"
description: "A link proof takes the address of non-virtual members, because a pointer to a virtual member is a vtable index that links with the file removed; verify a proof by taking the file back out."
tags: [testing, architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Link proofs

**A link proof takes the address of non-virtual members.** A pointer to a virtual member is a
vtable index and need not reference the defining object, so such a proof links cleanly with the
file removed from the library. Verify a link proof by taking the file back out. A header include
or a zero-valued ratchet alone does not prove that a component links independently
([library boundaries](../architecture/library-boundaries.md)); the wire-layout test's construction
of every factory is the link proof for the written CG/CL directions
([wire layout](../protocol/wire-layout.md)).
