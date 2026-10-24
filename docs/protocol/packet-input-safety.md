---
type: Rule
title: "Packet input safety"
description: "Unvalidated network input is the top open risk, instrumented two ways: ratchet R3 holds server-supplied lengths into copies at zero, and the packet_indices ctest reads every packet-indexed subscript by value and fails closed on a container it does not know."
tags: [protocol, security, testing]
enforced_by: [ratchets, packet_indices]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Packet input safety

**Unvalidated network input is the top open risk**, priority 1 of the
[code-health review](../reviews/code-health-review-2026-08-29.md). `Client/Packet/Gpackets/`
passes server-supplied lengths, indices and item classes into array subscripts, `strcpy`/`sprintf`
targets and a function-pointer table. The two halves have instruments rather than estimates.

**Lengths into copies: ratchet R3, at 0.** Every `sprintf`/`strcpy`/`strcat` under
`Client/Packet` and `Client/PacketHandler` is bounded or gone, so a new one fails the suite
([ratchets](../testing/ratchets.md)).

**Indices into subscripts: ctest `packet_indices`** (`tests/tools/check_packet_indices.pl`),
over `Client/Packet` and `Client/PacketHandler`. It walks the *value*, not the spelling:
`array[pPacket->getSlotID()]` has two live instances while `int slot = pPacket->getSlotID();`
twenty lines above `array[slot]` has a hundred. **103** packet-indexed subscripts, 91 of them into
a named, verified `CTypeTable` that range-checks itself, **12 into a container that is not**, all
guarded. A thirteenth fails the suite and has to be read. Seven of the 12 are in the phone
handlers `gamemodel` compiles, and `tests/unit/test_model_handlers.cpp` runs every out-of-range
slot byte (3 to 255) past each of them. It **fails closed**: a container it does not recognise
counts as raw, so adding a name to its allowlist is a deliberate act. Its first version hardcoded
the receiver name `pPacket` and so was blind to the 19 handlers that call their parameter
something else, the same measure-the-spelling mistake it was built to answer for R7
([measure the value](../process/measure-the-value.md)), which is why its header is long.

Both passes found live defects, listed under *Found by reading* in the review, including one that
needs no hostile server at all.

**What remains unaudited** is everything the two instruments do not model: a length or index that
reaches memory by some third route. The packet-read [fuzz targets](fuzzing.md) are the first check
that does not model a route at all: they drive every `read()` with hostile bytes under ASan and
UBSan, and found a heap overflow in `StoreInfo::read` and an out-of-array store in
`LCPCList::read` that neither instrument counts. The handler targets go on into the handlers and
the model, but not into the UI or the view behind the presentation hosts.

# Server strings into fixed buffers: none left (audit, 2026-10-24)

For a *string*, the third route is closed by reading, not by an instrument. Every string field a
reader fills (the `GC`, `LC`, `CR` and `RC` packets and the `*Info` records under `Client/Packet`)
was followed by value from its `read()`, with the cap that `read()` applies, through its handler
and one or two calls further: into `gamemodel`, the `gameclient` presentation adapters
(`GameUI.cpp`, `UIDialog.cpp`, `UIMessageManager.cpp`) and `VS_UI`, read with `grep -a`. The
spellings R3 and R17 do not count were swept as well: `memcpy`, `memmove`, `strncpy`,
`std::copy`, `string::copy`, `Basic::CopyBounded`, `snprintf` and `SafeFormat` given a size of
their own, `lstrcpy` and `_snprintf`, character loops, and fixed `char[N]` members (R17 and R18
already hold `wsprintf` at zero).

**No site lets a server-chosen length write past a fixed buffer or leave one unterminated.** Each
string ends in a `std::string`, an `MString`, a `StringCell` or a heap block sized from its own
length, or in a fixed array through a call bounded by that array: `snprintf(dst, sizeof(dst))`,
`SafeFormat`'s array overloads, or `CopyBounded` with a constant count and an explicit terminator.
The 128-byte stack buffers this page used to name are of that kind now. The longest string,
`GCNPCAskDynamic`'s subject and answers (`WORD` lengths, bounded only by the frame's 11,294-byte
maximum), is cut to the talk box's 2048-byte row. Not followed: the text system's own wrapping,
measuring and drawing, the descriptor renderers other than the blood bible's, and the view, which
keeps pointers to the names it draws. No ratchet pins the result: a spelling count would have seen none of the routes walked
here, so a new string sink is found by the same walk.

**Next: server values that reach UI memory as an index or a position.** `packet_indices` stops at
`Client/Packet` and `Client/PacketHandler`. The audit met four such sites past it, recorded under
*Found by reading* in the [code-health review](../reviews/code-health-review-2026-08-29.md):
- a guild member's rank, a `BYTE`, used as an index into the four grade names
  (`C_VS_UI_TEAM_MEMBER_INFO::Show`);
- each listed member's rank, used as an index into a five-entry sort table
  (`C_VS_UI_TEAM_MEMBER_LIST::AddMemberList`);
- a blood bible's zone looked up with no `NULL` test (`C_VS_UI_BLOOD_BIBLE_STATUS::MouseControl`).
  It is one of 32 lines under `Client` and `VS_UI` that dereference `g_pZoneTable->Get(...)` untested;
- a mail date too short for the `/` that `C_VS_UI_MAILBOX::AddMail` inserts, which throws a
  `std::out_of_range` that no caller catches.
