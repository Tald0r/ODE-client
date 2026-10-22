---
type: Rule
title: "Scripted rewrites"
description: "A scripted rewrite bounds its pattern, because a non-greedy match once swallowed 87 lines into one argument that no ratchet could see, and is followed by a sweep for orphaned banners and //#include lines."
tags: [process]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Scripted rewrites

**Scripted rewrites bound their patterns and are followed by a comment sweep.** A non-greedy
`.*?` under `/s` swallowed 87 lines of `GameUI.cpp` into one argument; the ratchets could not
have caught it, because a corrupted file has the right number of sites. After a scripted removal,
sweep for orphaned banners and `//#include` lines. Sources are CRLF, so a rewrite never goes
through a tool that normalises line endings ([source style](source-style.md)).
