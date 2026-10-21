---
type: Rule
title: "Source style"
description: "Tabs, English only, Hungarian notation and banner comments as the surrounding file has them; sources are CRLF, so edit them with the Edit tool and never sed -i or awk."
tags: [process]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Source style

- **Tabs**, not spaces, in C++ sources. There is no `.clang-format` and `make fmt` is a stub, so
  match the surrounding file by hand.
- **English only.** Remaining Korean and Chinese comments are upstream's; translate them when
  you touch that code, and never add more.
- Hungarian notation (`m_pFoo`, `g_pBar`, `bFlag`) and banner comments above functions and
  sections: follow the file you are in.
- **Sources are CRLF**; use the Edit tool, not `sed -i`/`awk`, to modify them. A scripted
  rewrite that must touch many files bounds its pattern and is followed by a sweep
  ([scripted rewrites](scripted-rewrites.md)). `tests/tools/check_source_encoding.pl` (ctest
  `source_encoding`) holds every source to the UTF-8 compiler contract on every platform.
- New library targets use explicit source lists, never `file(GLOB)`
  ([library boundaries](../architecture/library-boundaries.md)).
