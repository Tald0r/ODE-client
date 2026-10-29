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
the blood bible's, and the view, which keeps pointers to the names it draws (both followed since, below). No ratchet pins the result: a spelling count would have
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

**The descriptor renderers: three server values reached a `NULL` that a `std::string` cannot
take** (followed 2026-10-27, task 5 (b) of the [next work plan](../reviews/NEXT.md)). Of the eight
renderers `g_InstallDescriptor` registers (`VS_UI/src/VS_UI_Description.cpp`), the item's takes a
model object and the two skill renderers a skill id (the tree's a domain too). The others take
strings, a help page or the blood bible's status, which were followed before. The item renderers
take an `MItem` and a second item, a socketed core zap. `MItemHost::ItemDestroyed` drops the
descriptor when either one is deleted, and the description dialog (`RunDescDialog`) reads its item
once, when it is built, and keeps only the text. Every lookup by the item's class, type, option ids
or enchant level goes through a type table that answers a default row, or through `GetPartName`,
which range-checks the part. The option loop stops at the option table's size, the price per unit
is divided only under `GetNumber() >= 1`, and the core zap's lines are drawn only for types 0 to 3.
A default row's names are `NULL`, which `g_Print`, `g_GetStringWidth` and `SafeFormat` accept, but
two names reached code that does not:
- the item's own name. `MItem::GetName` and `GetEName` answered the default row's `NULL` for a
  type the table lacks; the description dialog appends both to `std::string`s, and
  `GCRemoveFromGearHandler` formats the name with `snprintf`'s `%s`. They answer an empty name now
  (`ItemCore.ATypeTheTableLacksIsNamedEmpty`);
- a pet's option name, which prefixes its tooltip (`MPetItem::GetPetOptionName` and `EName`). The
  pet's level is its item number, the server's byte, and below ten the unsigned level less ten
  wrapped to 429,496,728: for a pet with the luck or all-attribute part, the name was looked up
  that far past the game string table and its `NULL` assigned to a `std::string`. A pet under level
  ten has no option name now, and the lookup goes through `GetGameString`, which answers an empty
  string for any id (`PetOptionPart.APetUnderLevelTenHasNoOptionName`,
  `PetOptionPart.APetPastItsPartsRowsWithNoStringIsUnnamed`).

The tree renderer's skill id comes from its domain's tree, which is data. The skill renderer's
(`DID_SKILL`) comes from the usable skill set (`MSkillSet`): the skill window's wheel cycles
through it, skipping passive skills, and the window's timer shows the selected skill's description. A learned skill is one its domain's tree holds (`MSkillDomain::LearnSkill`), and
`GCLearnSkillOK` adds only an id the skill table holds (for a slayer or vampire, even one the tree
refused). A bomb or mine adds its item row's action, and a type the item table lacks reads the
default row, whose action is `ACTIONINFO_NULL` (0xFFFF): past the skill table and past the values
the action enum can hold (0 to 2,047), so the cast to it was undefined too. Both the rebuild (`MSkillSet::SetAvailableSkills`) and the pickup reply
(`GCDeleteandPickUpOK`) added it. Its description read the default skill row, whose learn level, 100,
is under the default limit of 255, so it appended the row's `NULL` English name to a
`std::string`, which crashes. Neither site adds the null action now
(`SkillAvailable.ABombOrMineOfATypeTheTableLacksShowsNoAction`,
`PickupReplies.ABombOrMineOfATypeTheTableLacksGivesNoSkill`). The
[code-health review](../reviews/code-health-review-2026-08-29.md) has a row for each of the three.

The server values the skill renderers draw are a skill's level and exp, as numbers and an exp
percent through `SafeFormat`. The slayer window's exp bar, outside the renderers, clips the same
percent to a rect, and `WithSpriteClip` draws nothing for the inverted rect a negative exp makes.
Two hazards come from the data files, not the server: the percent divides by a hundredth of the
row's max exp, which is zero for a max exp of 1 to 99, and an unnamed skill row's `NULL` name
would reach the same `std::string`, as an unnamed option row or a missing game string at a fixed id
would reach the item tooltip's (the quest prefix, the pet's option name, "not exist"). The walk stops at the skill window's own drawing of the ids in
the usable set, which is route (d).

**The view's name draws hold their names, but three things the server reaches were open**
(followed 2026-10-28, task 5 (c) of the [next work plan](../reviews/NEXT.md)). A `DRAWTEXT_NODE`
keeps a `const char*`, not a copy. `MTopView::DrawZone` queues the nodes while it draws, then prints
and frees them at its end (`DrawTextList`, `ClearTextList`), and no handler runs in between. If the
surface does not lock, `DrawZone` returns early, and rows `DrawEvent` queued before that wait for
the next frame: they point into the player's own chat ring, which outlives them. Every queued
string lives in the model or a table at least that long: a creature's name, its nickname's
`std::string`, its chat ring, its level name and hallucination name (indices `CreatureNameSelection`
bounds by the table's size), the player's character id, and an item's name. `g_Print` and
`DrawTextList` both accept `NULL`. The item name list is rebuilt at the start of each `DrawZone`,
and its node copies the item's id; `SelectItemName`, which runs between frames, looks the item up
in the zone by that id, never through the pointer. The hovered creature (`m_pSelectedCreature`) is
reset at the same point and read only during the frame.

The indices in the name draws hold:
- the name's colour is one of five alignment colours, and `MCreature::SetStatus` stores every
  creature's alignment through `ConvertAlignment`, which answers 0 to 4. The player's own name
  takes the converted step in its character slot, not its stored value, which is raw;
- the guild id is a key into the guild info map, and the mark's sprite id is tested against the
  mark file's index (`MGuildMarkManager`);
- the grade and pet-level marks are bounded by their callers, and the sprite pack (`CTypePack2`)
  answers an empty sprite past its end;
- the request icon's frame is clamped to its last.

Three things were open, each fixed with a test that failed on the unfixed code; the
[code-health review](../reviews/code-health-review-2026-08-29.md) has a row for each:
- **the HP bars overflowed an `int`.** The bars under a creature's name, over the player's own and
  beside a party member computed `maxPixels * currentHP / maxHP` in `int`. A creature's HP is 32
  bits from the server (`GCOtherModifyInfo`'s long entries set any status of another creature, and
  `GCModifyInformation` the player's), so for HP from about 21 million (2^31 over the bar's width)
  the product overflowed: undefined, and in practice a bar of the wrong length, empty at
  0x7FFFFFFF. The product is 64 bits now, and the party bar clamps a current HP negative as an
  `int` to 0 as the other two do, so a maximum past 2^31 draws full there too
  (`TopView1Name.TheHPBarIsQueuedUnderTheName`,
  `TopView1MyName.ThePlayersNameAndBarSitOverItsHead`,
  `TopView3PartyHP.AFullBarAtTheTopOfThirtyOneBitsIsFull`). The name bars' boxes are clipped to
  the surface, and the party bar draws its foreground only when its edge is right of the surface's
  left side, so the overflow never reached memory;
- **a nameless creature's name went to `strncmp`.** A fake creature other than a pet (an afterimage, a
  turret) is never named, and the fake ids (0 to 9999) are chosen without regard to the server's,
  which can use the same; the hovered id then selects the fake instead when it is drawn after, and
  `DrawCreatureName` compared its `NULL` name with the operator prefix. It tests the name first now
  (`TopView1Name.ANamelessCreatureQueuesNoName`);
- **an undrawn item name kept an uninitialised box.** `DRAWITEMNAME_NODE` got its box only from
  `DrawItemNameList`, which skips an empty name and everything in darkness or a coffin, while
  `SelectItemName` hit-tests every node on every update while the list is on: a node over recycled memory could select
  an item whose name was never shown. The box starts empty now
  (`TopView1ItemNames.ANameNotYetDrawnHoldsNoPoint`).

The walk stops at the text system, followed in (a), and at the chat ring's contents, which the
string audit above bounded.

**`VS_UI`'s own reads of the model hold its grids and trees, but its bars and the skill window's
delay divided by the server's values, and its colour sets were the server's** (followed
2026-10-29, task 5 (d) of the [next work plan](../reviews/NEXT.md)). The skill window draws an id
from the usable skill set through the skill table's sprite id and a sprite pack, both of which
answer a default past their end, and since task 5 (b) the set holds only ids the table has; its
hotkeys come from the player's own file. The skill trees draw from static tables and constant
domains. An inventory grid draws an item at the cell `MGridItemManager` accepted it into, with a
frame from the item table and a sprite pack, which both answer a default.

Three things were open, each fixed in the library or the window with a test; the
[code-health review](../reviews/code-health-review-2026-08-29.md) has a row for each:
- **the bars divided by a maximum the server sends.** The player's HP and MP bars
  (`C_VS_UI_HPBAR`) divided by the maxima in the player's slot (16-bit in the player's info,
  32-bit in `GCModifyInformation`'s long entries), the party manager by each member's maximum HP
  (`GCPartyPosition`), and the experience bars (the HP bar's own for vampires and ousters, the
  ousters window's, the slayer window's domain bars) by the goal of the player's level or domain
  level, which reads 0 past the experience table's last row. A maximum or goal of 0 was an integer
  division by zero on every draw, a trap on x86; the ousters MP bar's `MP_MAX + 1` was 0 for a
  maximum of 0xFFFFFFFF, and the info window's domain bars and their total multiplied the server's
  32-bit domain levels in `int`. Every such bar goes through `BarFill` (`VS_UI_util.h`) now, which
  multiplies in 64 bits, clamps to the bar and draws nothing for no maximum, and the total is
  summed in 64 bits. `HPBar.AFillIsSixtyFourBitsClampedAndEmptyWithNoMaximum` pins `BarFill`;
  `HPBar.EveryBarIsDrawnWithNoMaximumToDivideBy` and `PartyManager.AMemberWithNoMaximumHPIsDrawn`
  draw the HP bar (across and down, every race, maxima of 0 and 0xFFFFFFFF) and the party manager,
  where UBSan reported a division by zero at 20 lines on the unfixed code. Apple Silicon does not
  trap on one, so on macOS they are regression guards, and no test draws the slayer, ousters
  or info window;
- **the skill window's delay divided by a formula of the server's level.** `C_VS_UI_SKILL::GetDelay`
  shows the share of a usable skill's delay left, and Will of Life's delay is its formula's for the
  player's level, after the table's delay was tested: `GetWillOfLifeDelay` multiplies decore's
  `(30 + level) * 2` by 100, which is 0 at -30 and overflows an `int` from about 10.7 million.
  `WillOfLifeOutput` takes a level outside the byte the PC infos carry (0 to 255) as that byte's
  nearer end now, where the delay neither overflows nor reaches 0
  (`VampireSkillCost.WillOfLifeTakesALevelPastTheWireByteAsItsLast`: on the unfixed code the delay
  at -30 was 0, and UBSan reported the window's division);
- **a colour set past the table indexed `ColorSet`.** Every indexed pixel reads
  `CIndexSprite::ColorSet[s_IndexValue[channel]]`, a static table of `MAX_COLORSET` (495) rows,
  and the server chooses some colour sets: a pet item's main colour (`SetItemColorSet` from the
  item's slot info, which `GetItemOptionColorSet` answers for a pet), the player's skin and hair
  (the player's info), and the character list's skin, which `MakeCharacterSlot` clamps to the
  table except for the two special values. `SetUsingColorSet` and `SetUsingColorSetOnly` store a
  set the table lacks as the first row now (`CIndexSpriteColorSet.ASetPastTheTableIsDrawnWithTheFirst`).
  Most callers map the quest and unique items' special values (0xFFFE, 0xFFFF) first, as before;
  the few that pass one unmapped (that skin, an operator's body colour taken from a unique coat)
  draw the first row now, where they read past the table.

The walk stops at the sprite data: a pixel's gradation byte indexes within a row of `ColorSet`
(0 to 255 into 30 entries) unchecked in the CPU decoders and the SDL wipe, which only the sprite
packs choose.

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

