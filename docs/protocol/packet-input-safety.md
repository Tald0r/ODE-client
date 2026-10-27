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
maximum), is cut to the talk box's 2048-byte row. Not followed: the descriptor renderers other than
the blood bible's, and the view, which keeps pointers to the names it draws. No ratchet pins the result: a spelling count would have
seen none of the routes walked here, so a new string sink is found by the same walk.

**The text system holds a server string safely** (followed 2026-10-27, task 5 (a) of the
[next work plan](../reviews/NEXT.md)). `TextService`'s measuring, wrapping and drawing loop over
the string's own bytes: `Utf8Decode` never reads past the count it is given and, given at least one
byte, always moves on by one or more, and every loop stops at a NUL, so a string is measured and
drawn up to its first; a width of zero or below keeps the row on one line, a width narrower than any
glyph puts one glyph on each line and ends, the normalisation, glyph and metric caches refuse an
entry past their budget before they evict, and a glyph's blit clips to its target however far the
pen runs. Two things are layout, not memory: a wrapped line can overhang the width by the one
glyph that made it break, and `DrawLine` draws past a newline that the measure it aligns by stops
at. The walk stops at `TTF_RenderUTF8_Blended` and `spritectl_blt_sprite`, SDL_ttf's and the
backend's. `tests/unit/test_text_wrap_bounds.cpp` pins a 2048-byte row of mixed and malformed
UTF-8 wrapped, measured and drawn, and the two widths.

**One string could grow without bound instead.** `GCNPCAskVariable`'s parameters fill an NPC
line's `%(Name)` markers through `ApplyParameters` (`Client/MNPCScriptTable.cpp`), which runs the
keys in map order, so a value may hold a later key's marker. Each level multiplied the text: up to
255 values of up to 255 bytes could grow a 1.3 KB packet's text toward gigabytes, each in-place
`replace` moving the rest of it, until the game thread stalled and an uncaught `bad_alloc` ended
the client. Fixed in the review of the audit: each key is one linear pass, and substitution
stops before the text passes 65,536 bytes, what a saved script string holds
(`NPCDialogue.CascadingParametersStopAtTheLongestTextAScriptHolds`).

**Next: server values that reach UI memory as an index or a position.** `packet_indices` stops at
`Client/Packet` and `Client/PacketHandler`, and nothing past them is instrumented. The audit met
four such sites, recorded under *Found by reading* in the
[code-health review](../reviews/code-health-review-2026-08-29.md), and all four are fixed:
- a guild member's rank, a `BYTE`, used as an index into the four grade names
  (`C_VS_UI_TEAM_MEMBER_INFO::Show`). A rank past them has no name, as in the member list;
- each listed member's rank, used as an index into a five-entry sort table
  (`C_VS_UI_TEAM_MEMBER_LIST::AddMemberList`). A rank past it sorts last;
- a blood bible's zone looked up with no `NULL` test (`C_VS_UI_BLOOD_BIBLE_STATUS::MouseControl`),
  one of 32 lines under `Client` and `VS_UI` that dereference `g_pZoneTable->Get(...)` untested.
  Three more took a server's zone: the party window's face hover and the party arrow's
  (`GCPartyPosition`, and `RCPositionInfo` from other clients) and the vampire portal cursor's
  (`GCAddVampirePortal`). Each now tests its lookup, and a zone the table lacks is described as no
  place, or not at all;
- a mail date too short for the `/` that `C_VS_UI_MAILBOX::AddMail` inserts, which threw a
  `std::out_of_range` that no caller catches. The date is zero-padded to its six digits.

`tests/unit/test_mailbox_file.cpp` and `tests/unit/test_ui_server_indices.cpp` pin each of them
but the party arrow, which only the whole game UI draws; its description strings are static now,
since the descriptor manager read them after the block that owned them had ended. The review of
those fixes found one more: a vampire portal item's mark is its silver, which the server sends
with the item, and its two descriptions printed the marked zone unchecked. They print
`MVampirePortalItem::GetZoneName` now, which answers `NULL` for a zone the table lacks. Following
the other zone lookups found one more server value: `GCUpdateInfoHandler` read the new zone's
entry unchecked when the player left a large zone; a zone the table lacks now keeps the creature
sprites, and `tests/unit/test_update_info_replies.cpp` pins it.

**The walk (2026-10-25).** Every presentation host entry that takes a packet or an integer from a
handler (`Client/*PresentationHost.h`, `VsUiPresentationHost.h`, `ZoneLoadingHost.h`; their
installers in `GameInit.cpp` and `GameUI.cpp`) was followed into `VS_UI`, asking of each integer
whether it indexes or dereferences. Guarded already: effect statuses by the table's size; character
slots by the array; game string ids by `MAX_GAME_STRING`; the lair message's zone by a `NULL` test;
quest ids by `GetInfo`'s `NULL`; the blood bible id by its window (0 to 11); shop goods by
`MItem::NewItem` and `CTypeTable` (every creature, item and skill table range-checks and answers a
static default); the war list by fixed fives on both sides; the mail tab by a constant at every
caller; notices, lottery steps and gift ids searched by value; the minigame, notice, elemental and
sheet selectors by `switch`; the created item's grid by `MGridItemManager::AddItem`, which refuses
a cell off the grid before the UI is told; `SetHotKey` is a no-op. Found unguarded: a creature's
direction byte, which 79 handler lines store as it came through three `MCreature` setters and two packet methods, and which
indexes the eight-way addon-order tables at 17 draw lines, and the direction the skill handlers
hand to `MEffectGeneratorTable::Generate`, which the homing attack's angle table takes unchecked.
The setters now keep the facing for a direction past the eight and the generator table faces such
an effect left, where the zone-around generator refused it and drew nothing (`tests/unit/test_creature.cpp`, `tests/unit/test_effect_generator_table.cpp`; the
[code-health review](../reviews/code-health-review-2026-08-29.md) has the row). Not followed:
`VS_UI`'s own reads of the model objects it draws (inventory grids, skill trees, party slots) beyond
those named, and the view's frame packs, which `CTypeTable` range-checks.

