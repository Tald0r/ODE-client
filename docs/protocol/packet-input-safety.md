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
`LCPCList::read` that neither instrument counts. They stop at `read()`; handlers are not reached.
Fixed-size buffers fed by variable-length server strings are the next item (the 21-byte chat rows
are fixed; 128-byte stack buffers remain in other handlers).
