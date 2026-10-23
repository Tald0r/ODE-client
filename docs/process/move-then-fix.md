---
type: Rule
title: "Move, then fix"
description: "Moving a file between targets does not change its bytes in the same commit when it has no seam to cut: move first, prove the build, then fix, so the diff that changes behaviour is readable."
tags: [process, architecture]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Move, then fix

**Moving a file between targets must not change its bytes** in the same commit when it has no
seam to cut. Move first, prove the build, then fix, in separate commits, so the diff that changes
behaviour is readable. A file that only links once its reaches go through a host moves and cuts
in one commit (the Phase 4 pattern of the [restructuring plan](../reviews/RESTRUCTURING.md)), and
that commit names every change that is not the move. After moving files, reconfigure both Debug
trees ([library boundaries](../architecture/library-boundaries.md)). A `fix:` that follows a move
names its test path as `moved, then fixed` ([commit messages](commit-messages.md)); a class that
cannot be cut moves whole ([class splits](../architecture/class-splits.md)).
