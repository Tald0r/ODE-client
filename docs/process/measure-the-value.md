---
type: Rule
title: "Measure the value, not the spelling"
description: "A checker walks the value a site uses, not one spelling of it; a search that finds nothing is a fact about the search; and a reference on a live line can still be dead, so find the caller that runs before describing a seam."
tags: [process, testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Measure the value, not the spelling

**Measure the value, not the spelling.** A checker that matches `array[pPacket->getSlotID()]`
misses the hundred sites that copy the value into a local first; one that hardcodes a receiver
name misses the handlers that call it something else. `tests/tools/check_packet_indices.pl` walks
the value ([packet input safety](../protocol/packet-input-safety.md)); R7 and R8 exist as a pair
because R7 counts a spelling ([format strings](../protocol/format-strings.md)).

**A search that finds nothing is a fact about the search.** A grep that excludes the name it is
searching for (`grep RenderText | grep -v TextService`, when every call is
`TextService::RenderText`); a per-file grep over CP949 sources, which grep classifies as binary
and stops reading silently (`VS_UI/src/VS_UI_GameCommon.cpp` still carries three NUL bytes, so
every measurement over `VS_UI/` passes `grep -a`, [client traps](../architecture/client-traps.md));
a grep for `#define __USE_ENCRYPTER__` that missed the define in `Encrypter.h` while the golden
tests already said the branch was live. Read what the tests say before declaring code dead.

**A reference on a live line can still be dead.** The whisper seam was described as live
behaviour because the code that reaches it is compiled; every path *to* that code sits behind
`0 &&`. Before describing what a seam does, find the caller that runs; inspect both a host and its
installer when changing a callback ([host interfaces](../architecture/host-interfaces.md)).
