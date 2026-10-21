---
type: Rule
title: "Commit messages"
description: "A commit is type: lowercase imperative summary, then prose on why the change is right, what was verified and what is out of scope; a fix: commit names its Test path, and the commit-msg hook refuses one without."
tags: [process]
enforced_by: [tools/git-hooks/commit-msg]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Commit messages

**`type: lowercase imperative summary`**, then prose covering why the change is right, what was
verified, and what is deliberately out of scope. `a41eec9` is the model.

**A `fix:` commit also carries a `Test path:` line** (the [restructuring plan](../reviews/RESTRUCTURING.md)'s
task 3.1, the fix policy): `lib + test` (the code is in a static library and the commit carries
the test), `moved, then fixed` (the previous commit moved the unit into a library) or `exempt`
(the code is on the exemption list). `tools/git-hooks/commit-msg` refuses a `fix:` without one,
after `git config core.hooksPath tools/git-hooks` once per clone. A fix that could not be
reproduced is a *regression guard* in its message, not a reproduction
([library fixes are test-first](../testing/library-fixes-test-first.md)).

Trailer: `Co-Authored-By: <the authoring model> <noreply@anthropic.com>`, e.g.
`Claude Fable 5.1`. The per-change narrative (what moved, what the review round found, the test
list, the suite count, the ratchet delta) belongs in the PR description and the commit message,
not in a status line ([restructuring status lines](restructuring-status-lines.md)).
