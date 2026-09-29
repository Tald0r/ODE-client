# Compiler-warning cleanup (2026-09-27)

The client's own code - `Client/`, `VS_UI/`, `basic/`, `tests/`, `tools/` -
now builds without compiler warnings under Apple Clang 21 (macOS arm64),
GCC 13 and Clang 18 (Linux), and MSVC (Windows x64) after the follow-up
in [MSVC-only warnings](#msvc-only-warnings). Before, the macOS build reported 4,105
distinct warnings and the Linux GCC one about 4,000; about 6,100 distinct
project sites across both compilers were cleared, in 842 files. What the
warning budget still counts (`tools/ci/warnings.md`) is vendored code
(`third_party/`, IXWebSocket under `build/_deps/`) and the linker.

The cleanup changed no behaviour on purpose, so the defects it came across
were written down rather than fixed. On 2026-09-28 each of the 178 was
checked again against `c23806f4` and, for packets and game rules, against
the server's code. 101 were real and are fixed, with a unit test wherever
the code is in a library a test binary links. The other 77 were not defects,
or the cleanup had already fixed them. Every entry below carries an ID,
which the fix commits cite, and a line giving its outcome.
[Found while fixing](#found-while-fixing) lists what that work turned up.

## How

Every change was meant to leave behaviour exactly as it was: a warning was
cleared by making the current behaviour explicit (parentheses that spell
the existing precedence, `default: break;` in switches, casts that spell
the conversions the compiler already made, `(void)name;` for unused
parameters), never by changing what the code does. Code the macOS and
Linux builds never compile was protected by rule: no name was removed that
anything still references, including inside `PLATFORM_WINDOWS`, `_DEBUG`,
`__EMSCRIPTEN__`, `_LIB` and the debug-output branches, and the changed
translation units were compiled before and after the change for Windows
(MinGW, with and without `_DEBUG`), with `_DEBUG` and the unused debug
macros defined, and for Emscripten, with no new errors. Each batch was
fixed by one agent and reviewed by another.

Where a warning pointed at a real defect, the defect was kept and written
down below; the exceptions are these deliberate changes:

- **Virtual destructors** for classes deleted through a base pointer
  (`MEffect`, `MStatus`, the gear classes, `MInventory`, `PetInfo`,
  `C_VS_UI_GAME`): undefined behaviour before. Only `MEffect` has subclasses
  deleted through it, and their destructors are empty.
- **`Properties::load` and DebugKit** kept `std::string` search positions in
  a `uint`, which truncates `npos` on 64-bit builds: whitespace-only lines
  threw and lines without a separator were accepted. Fixed test-first
  (`tests/unit/test_properties_parse.cpp`).
- **Plain `char` holding `-1`**: the quest inventory's cells are read
  through `signed char`, identical where `char` is signed (Windows, macOS,
  x86-64 Linux, Emscripten) and now also correct on arm64 Linux, where
  `char` is unsigned and the `-1` tests could never match. The fade was
  only half done by that change: the `m_FadeEnd == -1` test was cast, but
  the direction (`start<end`, `m_FadeInc > 0`) and the end tests still
  compared plain `char`, so on arm64 Linux the Gilles de Rais darkening
  (`SetFadeStart(31, -1, 1, ...)`) counted up and ended after one step. The
  fade's value, end and step and `SetFadeStart`'s parameters are now
  `signed char`, so every comparison is signed on every ABI.
- **Packet destructors** no longer wrap their bodies in
  `__BEGIN_TRY`/`__END_CATCH`: a destructor is `noexcept`, so the rethrow
  could only reach `std::terminate` (GCC's `-Wterminate`). The nine declared
  `noexcept(false)` keep theirs.
- **Locals read uninitialised** on paths for an unknown race or an unset
  status now start at zero or null, and `Client.cpp`'s update-directory
  buffer is large enough for its longest path.

## Findings and what became of them

Line numbers are those of the code before the cleanup (`30f74752`). Each
entry was reported by the agent that cleared the warning at that site. The
line under it gives the outcome: the commits that fixed it, or why no change
was needed.

### `Client/AppendPatchInfo.cpp`

- **F001** line 66: The unused local `afterNum = max(afterSpkSize, orgSpkSize)` was removed. Its comment says the count should be set to the larger value, but the next line writes `afterSpkSize`, not `afterNum`. The author probably meant to write the max. The current behaviour (writing afterSpkSize) is kept.
  - **Not a defect:** the comment is wrong, not the write, because `afterSpkSize` is already the exact count `CheckFinalInfo` requires (original plus every appended pack), and writing the max would leave a stale larger count.

### `Client/CTypeTable.h`

- **F002** line 211: Removed `if (i==557) { i=i; }` in SaveToFile and `if (i==700) { i=i; }` in LoadFromFile. They look like leftover debugger breakpoint anchors and have no effect.
  - **Not a defect:** both statements were self-assignments with no effect, so removing them leaves the save and load loops, including the `file.good()` guard, unchanged.

### `Client/Client.cpp`

- **F003** line 4217: Fixing -Wformat-truncation meant enlarging the local `UpdateDir` from `_MAX_PATH` to `_MAX_PATH + sizeof(DIRECTORY_UPDATE)`. Before, a working directory longer than about 252 chars produced a truncated path, and `_rmdir` was called on that. It now gets the full path. Behaviour changes only in that edge case.
  - **Already fixed:** 2c8c3fc6 made this change, and the buffer now holds the longest path `_getcwd` can return (259 characters, the separator, `Update` and the terminator, exactly 267 bytes).

### `Client/DebugKit.cpp`

- **F004** line 138: `uint key_begin = line.find_first_not_of(" \t")` truncated npos, so where size_t is 64-bit a whitespace-only line reached `line.substr(0xFFFFFFFF, ...)` and threw std::out_of_range (reachable only under DEBUG_INFO). Fixed with Properties: the positions are `std::string::size_type`. Still open, same file: `GetMsgFileName()` does `return NULL;` into a std::string (undefined behaviour) when the key is missing.
  - **Fixed** in `6a46b721`: Return an empty message file name when the key is missing.

### `Client/DrawCreatureShadow.cpp`

- **F005** line 297: In DrawCreatureShadow's creature_type switch, case 740 (claymore) has no break and falls through into case 734 (guild tower); every neighbouring case ends with break. It looks like a missing break, but the fallthrough is harmless here: case 734 sets direction = 1 again and, only if the creature is alive, action = ACTION_STAND and frame = 0, which case 740 has already set. I kept the behaviour with [[fallthrough]].
  - **Not a defect:** dead or alive, case 734 leaves `action`, `frame` and `direction` exactly as case 740 already set them, so the fallthrough behaves the same as a `break`.
- **F006** line 439: bTeenVersion parses as '(GoreLevel == false && !(relic/tree/waypoint list)) \|\| (creature_type 377..386) \|\| creature_type == 480', so types 377-386 and 480 (dark guardian) are treated as teen version (drawn as dust) even when gore is enabled. That may be intended (the comment says the dark guardian is shown as dust), but the author may have meant GoreLevel == false to apply to all three alternatives. I made the current grouping explicit and did not change it.
  - **Not a defect:** the body draw in `MTopViewDraw.cpp` has the same grouping, with the comment that the dark guardian is shown as dust, so these types are dust regardless of gore and the shadow matches the body.

### `Client/GameInit.cpp`

- **F007** line 498: In PrepareLoadingAddonSPK a local `bool g_AddonSPKLoaded[g_AddonSPKLoadingTimes]` hid the file-scope global of the same name. The `g_AddonSPKLoaded[i] = false;` meant to reset the global therefore only wrote the dead local. I removed the local and its write; behaviour is the same because the global was never touched either way. The intended reset of the global has never happened.
  - **Not a defect:** `PrepareLoadingAddonSPK` and `LoadingAddonSPK` both return `true` before doing anything, and even if they ran, the loop runs only on the first call, when the zero-initialised global is already all false.

### `Client/GameUI.cpp`

- **F008** line 423: In UI_RemoveEffectStatus, `if(ai == SKILL_BLOOD_DRAIN) g_char_slot_ingame.bl_drained = false;` was indented one level less than the statements around it, inside the `if (itr->actionInfo == ai)` block. I re-indented it to match the actual control flow; no logic changed. Also note that `endItr` there is cached before `status.erase(itr)`, but the function returns right after the erase, so the stale iterator is never used.
  - **Not a defect:** the statement was always inside the `if (itr->actionInfo == ai)` block, so `bl_drained` is cleared only when the blood-drain entry is removed, mirroring `UI_AddEffectStatus`, and the function returns straight after the erase.
- **F009** line 4103: `switch( mkq->GetGameType() )` switches on a MINI_GAME_TYPE value but its labels are GameType enumerators (GAME_MINE, GAME_ARROW, from GCMiniGameScores.h). It only works because both enums happen to number 0..3 the same way. I kept that by switching on static_cast<int>(...).
  - **Not a defect:** `MINI_GAME_TYPE` and `GameType` both number MINE, NEMO, PUSH and ARROW 0 to 3, as the server's copy does, so the switch picks the right strings.

### `Client/MAttachCreatureOrbitEffectGenerator.cpp`

- **F010** line 50: The condition reads `... \|\| ( type == SUMMON_FIRE_ELEMENTAL \|\| type == SUMMON_WATER_ELEMENTAL && pCreature->GetAttachEffectSize() > 0 )`, which parses as FIRE \|\| (WATER && size > 0). I kept that meaning and made it explicit with parentheses. The layout suggests the author meant (FIRE \|\| WATER) && size > 0. If so, the FIRE_ELEMENTAL case currently dereferences *pCreature->GetAttachEffectIterator() even when the creature has no attached effects.
  - **Fixed** in `44b14665`, `2b926cd5`: Read the previous orbit step only from an attached orbit effect.

### `Client/MAttachOrbitEffect.h`

- **F011** line 53: NextOrbitStep used `m_OrbitStep = ++m_OrbitStep & 0x0000003F;`, which GCC flags as a possible sequence-point problem. Under C++17 sequencing the intended result, (m_OrbitStep+1) & 0x3F, is what gets stored, so I rewrote it that way without changing the value. This is a note, not a likely functional bug.
  - **Not a defect:** `m_OrbitStep = ++m_OrbitStep & 0x3F` is well defined since C++11 and the project builds as C++20, so both spellings store `(old + 1) & 63`.

### `Client/MCreature.cpp`

- **F012** line 164: In the ActionMoveNextPosition macro, the removed `m_NextDirection != SECTORPOSITION_NULL` was always true: m_NextDirection is a BYTE and SECTORPOSITION_NULL is 0xFFFF. MoveNextPosition() resets m_NextDirection to DIRECTION_NULL, so the check was probably meant to be `m_NextDirection != DIRECTION_NULL`. Behaviour is preserved: the operand was constant true.
  - **Fixed** in `16f41524`: Take no direction-9 step when a pending move has no direction.
- **F013** line 1208: RemoveGlacierEffect and RemoveCauseCriticalWoundsEffect (around line 10669) both set a flag `re` that was never read. RemoveEffectStatus has the same loop and returns the flag, so these look like a lost return value or early exit. Both functions return void, and the dead flag was removed with no change in behaviour.
  - **Not a defect:** both void functions are meant to strip every matching glacier or critical-wounds sprite, which the full loops do, and no caller could use a result, so no return value or early exit was lost.
- **F014** line 1587: In RemoveEffectStatus, the ground-effect loop parses as `(IsEffectSprite() && type==T) \|\| (type2!=NULL && type2==T)`. The sibling attached-effect loop at 1548-1550 uses `IsEffectSprite() && (type==T \|\| (type2!=NULL && type2==T))`. As a result the ground loop can remove a non-sprite effect whose sprite type equals type2. The existing grouping was kept and only made explicit.
  - **Not a defect:** every ground effect is created with `m_bEffectSprite` true and the only colour effect goes to the body list, so `IsEffectSprite()` is always true in the ground loop and both groupings are equivalent.

### `Client/MCreatureWear.cpp`

- **F015** line 1156: In RemoveEffectStatus, `case EFFECTSTATUS_COMA` (SetAlive plus the vampire resurrect action) falls through into `case EFFECTSTATUS_GHOST`, which calls SetGroundCreature() for non-flying creatures, with no break. It may be unintended. I kept it and marked it with `[[fallthrough]];`.
  - **Fixed** in `0dc92783`: End mcreaturewear's coma case with a break of its own.

### `Client/MEffectTarget.cpp`

- **F016** line 24: The copy constructor guarded `*this = target;` with `if (&target!=NULL)`. A reference can never be null, so compilers already treat the check as true. I removed the dead check and the assignment now always runs, exactly as before.
  - **Not a defect:** a reference cannot be null, so the check was always true, and every copy site dereferences a pointer it has just checked.

### `Client/MEventManager.cpp`

- **F017** line 116: RemoveEvent computed `bFadeScreen = (event->eventFlag \| EVENTFLAG_FADE_SCREEN) != false`, which is always true. `&` was almost certainly meant, so the fade-screen gamma reset runs for every removed event. I kept that behaviour as `bool bFadeScreen = true;` with a one-line comment giving the original expression.
  - **Not a defect:** `SetAddGammaRamp`, `SetGammaRamp` and `RestoreGammaRamp` are empty inline stubs in this port (`Client/DXLib/CDirectDraw.h`), so the always-true test here and the same one in `AddEvent` change nothing reachable.
- **F018** line 178: MEventManager::GetEventCountByFlag tests `eventFlag \| flag`, true for every event whenever flag is non-zero, so it returns the total event count; GetEventByFlag uses `&`, which was evidently meant.
  - **Fixed** in `d1baf52b`: Test event flags with & in the flag count and empty queries.
- **F019** line 198: MEventManager::IsEmptyEventByFlag tests `eventFlag \| flag`, so it reports non-empty whenever any event exists; `&` was evidently meant.
  - **Fixed** in `d1baf52b`: Test event flags with & in the flag count and empty queries.

### `Client/MFakeCreature.cpp`

- **F020** line 48: The local ActionMoveNextPosition() macro compared `m_NextDirection` (a BYTE) with SECTORPOSITION_NULL (0xFFFF), which is always true; DIRECTION_NULL was probably intended. I dropped that operand. MCreature.cpp's copy of the macro has had the same operand dropped in the working tree, so the two stay consistent.
  - **Not a defect:** dropping the always-true operand changes nothing, and testing `DIRECTION_NULL` would break `MCreature`'s snap to a fast-move target, because `FastMovePosition` sets `m_NextX` and `m_NextY` without setting `m_NextDirection`.
- **F021** line 945: TYPE_SECTORPOSITION is `unsigned short` on every platform. So `sX >= 0 && sY >= 0` in the SetNextDestination search loop (line 945) and `sX<0 \|\| sY<0` in the zone-bounds check (line 1066) were always true and always false. They never guarded anything and have been removed, with identical results. Any negative-coordinate intent was already lost when the values became unsigned.
  - **Not a defect:** a wrapped negative coordinate is far larger than any zone, so the remaining width and height checks still reject it, and inside the search loop the coordinates only step toward the creature's in-bounds position.

### `Client/MHelpManager.cpp`

- **F022** line 461: In the help-node walk, a node of type TYPE_NULL (or MAX_NODE_TYPE) leaves pHelpNode unchanged, so `while (pHelpNode!=NULL)` loops forever. This predates my change: the new `default: break;` keeps the same behaviour.
  - **Not a defect:** `MHelpNode` has exactly two concrete subclasses, both returning a handled type, and `NewNode(TYPE_NULL)` returns NULL, so the loop always advances; `ExecuteEvent` also has no live caller.

### `Client/MItem.cpp`

- **F023** line 369: MItem::GetDropFrameID computed `frameID` for EVENT_GIFT_BOX type 2 and then threw it away, which looks like an unfinished special case. I deleted it because it had no side effects, and the function returns the same value as before.
  - **Not a defect:** the discarded `frameID` was the same table lookup the function returns anyway, so gift box type 2 already gets that frame.

### `Client/MItemUse.cpp`

- **F024** line 1586: In MEventTreeItem::UseInventory, `(pCreature != NULL && pCreature->GetCreatureType() == 482) \|\| pCreature->GetCreatureType() == 650` dereferences pCreature when it is NULL (right operand of \|\|). The NULL check probably meant to cover both comparisons. Kept as is; parentheses only spell the existing parse.
  - **Fixed** in `242002bf`: Cover both creature checks with the event tree null test.
- **F025** line 2256: In MMoonCardItem::UseInventory, `GetItemType() == 2 \|\| (GetItemType() ==3 && GetNumber() > 1 && GetNumber() < GetMaxNumber())` lets type 2 split with no count check. The commented-out original (`type == 2 && number > 1 && number < max`) suggests `(type == 2 \|\| type == 3) && ...` was meant. Behaviour kept.
  - **Fixed** in `fb2c36c0`: Apply the moon card split count checks to type 2 as well.

### `Client/MPlayer.cpp`

- **F026** line 2843: In the trace timer check, `if( pCreature != NULL && (pCreature->IsSlayer() && IsSlayer()) \|\| (pCreature->IsVampire() && IsVampire()) \|\| (pCreature->IsOusters() && IsOusters()) )` parses as `(pCreature != NULL && slayer) \|\| vampire \|\| ousters`. The NULL guard therefore covers only the slayer term: if pCreature is NULL, pCreature->IsVampire() dereferences NULL. The grouping is preserved as it was (now written as `(pCreature != NULL && (...)) \|\| ...`).
  - **Fixed** in `c4b75917`: Guard all three race tests in keeptracecreature's timer check.
- **F027** line 3078: In the abort condition `pCreature==NULL \|\| IsDead() \|\| darkness-term \|\| ground-elemental-term && !g_pPlayer->HasEffectStatus(EFFECTSTATUS_GHOST) [&& !g_bLight]`, the GHOST (and METROTECH g_bLight) exemption binds only to the ground-elemental term. The analogous condition at original line 2860 applies `&& !GHOST` to the whole disjunction, so the author probably meant the same here. The current binding is kept and made explicit with parentheses.
  - **Fixed** in `5639c3fa`: Exempt a ghost from the darkness gate of a basic-action trace.
- **F028** line 4546: In the pickup darkness check `!IsVampire() && (!HasEffectStatus(EFFECTSTATUS_LIGHTNESS) \|\| (IsVampire() && g_pZone->GetID() == 3001))`, the inner IsVampire() is always false because !IsVampire() was just required, so the zone-3001 clause is dead. Compare original line 4527, which uses a bare `\|\| g_pZone->GetID() == 3001`. Behaviour preserved; only parentheses were added.
  - **Fixed** in `c7a4bc41`: Apply one darkness rule to tracing and picking up items.
- **F029** line 5231: In the trace-complete pickup condition, `\|\|!IsOusters()` is a top-level disjunct inside the darkness test. For any non-Ousters player the whole darkness/lightness/ghost test is therefore true, and they pick up items in darkness regardless. Its sibling check (current 4544) has no such clause, so this looks like a misplaced operator. Behaviour preserved; only parentheses were added.
  - **Fixed** in `c7a4bc41`: Apply one darkness rule to tracing and picking up items.
- **F030** line 5610: The same dead clause appears in the trace-item stop check: `!IsVampire() && (!HasEffectStatus(LIGHTNESS) \|\| (IsVampire() && g_pZone->GetID() == 3001))`. The inner IsVampire() can never be true. Behaviour preserved.
  - **Fixed** in `c7a4bc41`: Apply one darkness rule to tracing and picking up items.
- **F031** line 9774: (Original line numbers.) In the condition '... \|\| HasEffectStatus(EFFECTSTATUS_HAS_SWEEPER_12) && !g_pPlayer->HasEffectStatus(EFFECTSTATUS_GHOST) [&& !g_bLight]' (orig 9774-9779), the '&& !ghost' / '&& !g_bLight' guard applies only to the SWEEPER_12 term. The author evidently meant it to guard the whole OR chain. I kept the current parse and made it explicit with parentheses.
  - **Fixed** in `291d1bce`: Exempt a ghost from every half-speed move effect, not only one.
- **F032** line 9929: The condition is 'm_bNextAction \|\| (...) && m_NextAction!=ACTION_STAND && ... && m_NextAction!=m_MoveAction'. m_bNextAction alone satisfies it, so the action checks only matter when it is false; '&&' may have been meant. I kept the current parse and made it explicit with parentheses.
  - **Not a defect:** `m_bNextAction` forces a readied skill action to run whatever `m_NextAction` holds, every sibling buffered-action site has the same shape, and `&&` would drop those forced actions.
- **F033** line 11911: The condition is '(g_pQuickSlot!=NULL&&IsSlayer()) \|\| ((armsband)&&IsOusters()) && IsItemCheckBufferNULL() && mode==MODE_NULL && (item race match)'. For a slayer with a quickslot, the item-check-buffer, temp-mode and item-race checks are all skipped. I kept the current parse and made it explicit with parentheses.
  - **Fixed** in `da1754a6`, `cafbcc86`: Apply the quickslot pickup checks to the slayer belt too.
- **F034** line 12266: If GetRace() returns RACE_MAX, pGear (orig 12266) and maxDAM/minDAM (orig 12551) were left uninitialized: the switch was skipped and then pGear->SetBegin() and UI_SetCharInfoDAM(maxDAM, minDAM) ran on garbage. To clear the -Wsometimes-uninitialized warnings that the new default: labels exposed, I initialized them to NULL/0. The RACE_MAX path now dereferences a null pointer every time instead of reading garbage. Other paths are unchanged.
  - **Not a defect:** `GetRace()` can only return `RACE_SLAYER`, `RACE_VAMPIRE` or `RACE_OUSTERS`, so neither `default:` label is reachable and the NULL `pGear` and zero damage values are never used.
- **F035** line 12772: 'return NULL;' in int MPlayer::FindEnemy() returns 0, which is not OBJECTID_NULL (0xFFFFFFFF). Every caller compares the result to OBJECTID_NULL, so this early return (no special action set) yields creature id 0. I changed it to 'return 0;' (same value).
  - **Fixed** in `0cd9e75f`: Report no enemy from findenemy when no special skill is set.

### `Client/MPlayer.h`

- **F036** line 152: MPlayer declares its own enum REQUEST_MODE {REQUEST_NULL, REQUEST_TRADE, REQUEST_PARTY}, which shadows the enumerators of the inherited MRequestMode::REQUEST_MODE {..., REQUEST_INFO}. m_RequestMode is an MRequestMode::REQUEST_MODE, so the switch at orig MPlayer.cpp:11051 compared it against MPlayer:: enumerators. The values are the same, so behaviour was correct, but it is a trap for later edits. I did not change the enum; I only qualified the case labels.
  - **Not a defect:** both enums give NULL, TRADE and PARTY the same values, the case labels now name `MRequestMode`, and nothing uses `MPlayer::REQUEST_*`, so the shadowing enum is dead.
- **F037** line 394: MPlayer::KnockBackPosition(x, y) does not override MCreature::KnockBackPosition(x, y, BYTE Action = 0); the only caller goes through MCreature*, so the player-specific follow-up (ResetSendMove, the action-info reset) never runs on knockback.
  - **Fixed** in `7e0f7116`: Make mplayer's knockbackposition override the virtual.

### `Client/MSector.cpp`

- **F038** line 76: MSector::RemoveAllObject had the no-op statement `m_nImageObject;`, probably meant to be `m_nImageObject = 0;`. So the image-object count is not reset while m_mapObject is cleared. I removed the no-op and kept the behaviour: the count is still not reset.
  - **Fixed** in `9db704be`: Reset the image object count in MSector::RemoveAllObject.
- **F039** line 1938: MSector::RemoveEffect(id, MEffect*& pEffect): inside the loop, a local `MEffect* pEffect` hides the reference out-parameter, so the caller's pEffect is never set, although the banner comment says the removed effect is handed back. I added `(void)pEffect;` and changed nothing else.
  - **Fixed** in `0ca94912`: Return the removed effect from MSector::RemoveEffect.

### `Client/MSkillAvailable.cpp`

- **F040** line 1015: pGear was uninitialized when GetRace() matched none of the three races (e.g. RACE_MAX); pGear->FindItem() was then undefined behaviour. After adding default: break; it is initialized to NULL, so that unreachable case now dereferences NULL. No reachable path changes.
  - **Fixed** in `b456a902`: Skip the couple ring check when the player has no gear.

### `Client/MSkillInfoTable.cpp`

- **F041** line 124: -Wself-assign: the loop contained `if (skillType==219) { i=i; }`, a no-op that was apparently a debugger breakpoint spot. I removed the whole if block; the condition has no side effects, so behaviour is unchanged.
  - **Not a defect:** the condition reads a local and the body assigns `i` to itself, so removing the block leaves the load loop unchanged.

### `Client/MSkillManager.cpp`

- **F042** line 776: MSkillDomain::AddSkill had two `if (id == SKILL_ABERRATION) int a = 0;` debug no-ops, and IsAvailableDeleteSkill had an unused `int TempSkillID = *iNextSkill;`. I removed them, which is behaviour-neutral; they were only leftover breakpoint anchors.
  - **Not a defect:** each `int a = 0;` was the whole if-body in its own scope and `TempSkillID` was an unused read of a valid iterator, so removing them changes no control flow.

### `Client/MTopView.cpp`

- **F043** line 14738: In DrawItem, `TYPE_OBJECTID temp = m_SelectCreatureID;` saves the selected creature ID, but afterwards the code writes `m_SelectCreatureID = OBJECTID_NULL;` instead of restoring `temp`. The previous selection is therefore lost. I deleted the dead save; the non-restore is unchanged.
  - **Not a defect:** `SetSelectedItem` clears `m_SelectCreatureID` and every other setter clears `m_SelectItemID`, so whenever this branch runs the creature selection was already `OBJECTID_NULL`, and resetting it restores that value.
- **F044** line 15120: DrawItemShadow has the same pattern: `temp` saves m_SelectCreatureID but the code resets it to OBJECTID_NULL instead of restoring it. I deleted the dead save; the behaviour is unchanged.
  - **Not a defect:** as in `DrawItem`, item and creature selection are mutually exclusive, so `m_SelectCreatureID` is already `OBJECTID_NULL` whenever this branch runs and resetting it restores the saved value.
- **F045** line 16316: The condition is now spelled `(bRediance && (SWORD \|\| SWORD_FAST \|\| SWORD_SLOW)) \|\| SWORD_2 \|\| SWORD_2_SLOW \|\| SWORD_2_FAST`, which is the existing precedence. The SWORD_2* actions trigger the frame override whether bRediance or bLarSlash is set. That may be intended, or the SWORD_2* terms may have been meant to pair with bLarSlash.
  - **Not a defect:** Rediance attaches an effect for the `SWORD_2` actions and Lar Slash exists only during them, so the current expression is the intended rule, and pairing `SWORD_2` with `bLarSlash` would skip that Rediance effect.
- **F046** line 16999: In DrawItemBroken, adding `default: break;` for RACE_MAX made clang report that pGear is used uninitialised, so pGear is now initialised to NULL. On the RACE_MAX path, which cannot occur for a real race, `pGear->HasBrokenItem()` goes from an indeterminate-pointer dereference to a null dereference. spriteID, frameType, toSomewhatBroken and toAlmostBroken are equally uninitialised on that path and were left as they are.
  - **Fixed** in `4ab401e8`: Return from DrawItemBroken for a race it has no gear for.
- **F047** line 17222: In GetMaxEffectFrame, I removed `if (frameID < 0) return 0;`. TYPE_FRAMEID is unsigned short in every DrawTypeDef.h, so the check was dead on every platform and was presented as a bounds check that never fired. The remaining per-case upper-bound checks are unchanged.
  - **Not a defect:** `TYPE_FRAMEID` is `unsigned short` in all three `DrawTypeDef.h` copies, and every case keeps its upper bound against the pack size, which also rejects `FRAMEID_NULL`.
- **F048** line 17773: In the HP-modify list drawing, the else branch has the bare statement `RGB(150, 255, 150);`. It is almost certainly missing `color =`, so `color` was indeterminate for positive modify values. With `COLORREF color = 0;` that path now draws in black (0) instead of an indeterminate colour. The missing assignment is not fixed.
  - **Fixed** in `35a4e732`, `5bf4e2c9`: Draw healed hp numbers in light green instead of black.
- **F049** line 17958: RequestSpriteID was left uninitialised when none of IsRequestTrade/IsRequestParty/IsRequestInfo is true, and was then used as an m_EtcSPK[] index. It is now initialised to 0, so that path uses sprite 0 instead of an indeterminate value.
  - **Not a defect:** the block runs only under `IsRequestMode()`, where the mode can only be trade, party or info, so one branch always assigns `RequestSpriteID` and the `= 0` path cannot be reached.

### `Client/MZone.cpp`

- **F050** line 2346: In AddPortal, the swap `if (left > right) { int temp=left; left=right; right=left; }` is broken: `right=left` should be `right=temp`. When left > right, both end up equal to the old `right` instead of being swapped. I kept this behaviour and removed only the unused `temp`. The top/bottom swap on the line above is correct.
  - **Fixed** in `d7ed9d8e`, `80ebedfc`: Order zone rect edges before clamping them to the grid.
- **F051** line 3549: In RemoveItem, the loop `while (tempItr != m_mapItem.end()) { tempItr++; }` walks the whole map for nothing. It is leftover debug code whose only purpose was the now-removed `idd`. I left the loop in place so behaviour is unchanged.
  - **Not a defect:** the loop only reads the map and `theIterator` comes from `find(id)` afterwards, so it changes no state or result and costs only a walk over the zone's items.
- **F052** line 4456: In RemoveTileEffect, the removed inner `BOOL found = FALSE;` shadowed the outer `found` tested by the enclosing `if (!found)`, so the ground-effect search never updated the outer variable. It looks like the outer one was meant to be reused. This has no effect today because the outer `found` is not read afterwards.
  - **Not a defect:** nothing after the ground-effect block reads either `found`, so the shadowing never affected control flow and the ground effect is erased as before.
- **F053** line 5635: SetSafeSector has the same broken left/right swap as AddPortal (`right=left` instead of `right=temp`). I kept the behaviour and removed only the unused `temp`.
  - **Fixed** in `d7ed9d8e`, `80ebedfc`: Order zone rect edges before clamping them to the grid.

### `Client/Packet/Gpackets/GCAddItemToInventory.cpp`

- **F054** line 105: write() used to compute optionSize but never writes it, then writes each option. read() (line 66) first reads a BYTE optionSize and then that many options. write() therefore produces bytes that read() cannot parse back. Behaviour is unchanged: the removed local was never used.
  - **Fixed** in `2f862326`: Write the item option count ahead of the options.

### `Client/Packet/Gpackets/GCBloodBibleStatus.cpp`

- **F055** line 56: write() narrows m_OwnerName.size() to BYTE and then checked that BYTE against > 256, which is always false. A name of 256 bytes or more has its length wrapped on the wire while the whole string is still written. This is the narrowing bug that commit 3ab97461 fixed for Cpackets. The dead check is removed and the behaviour is unchanged.
  - **Fixed** in `7be81c0a`: Cap single-string GC packet writes before narrowing the length.

### `Client/Packet/Gpackets/GCModifyGuildMemberInfo.cpp`

- **F056** line 53: The write() cap `szGuildName > 256` on a BYTE could never fire. The guild name length is narrowed with no working cap. The dead check is removed and the behaviour is unchanged.
  - **Fixed** in `7be81c0a`: Cap single-string GC packet writes before narrowing the length.

### `Client/Packet/Gpackets/GCNPCSayDynamic.cpp`

- **F057** line 23: The caps `szMessage > 2048` on a BYTE, in both read() and write(), could never fire. The limit of 2048 cannot be expressed in a BYTE length prefix at all, which suggests the length field was once wider. Both dead checks are removed.
  - **Fixed** in `7be81c0a`: Cap single-string GC packet writes before narrowing the length.

### `Client/Packet/Gpackets/GCNotifyWin.cpp`

- **F058** line 23: The caps `szMessage > 2048` on a BYTE, in both read() and write(), could never fire, the same as in GCNPCSayDynamic. Both dead checks are removed.
  - **Fixed** in `7be81c0a`: Cap single-string GC packet writes before narrowing the length.

### `Client/Packet/Gpackets/GCShopBought.cpp`

- **F059** line 95: Same read/write mismatch: read() reads a BYTE option count (line 62), but write() never writes it.
  - **Fixed** in `2f862326`: Write the item option count ahead of the options.

### `Client/Packet/Gpackets/GCShopBuyOK.cpp`

- **F060** line 91: Same read/write mismatch: read() reads a BYTE option count (line 58), but write() never writes it.
  - **Fixed** in `2f862326`: Write the item option count ahead of the options.

### `Client/Packet/Gpackets/GCShowGuildInfo.cpp`

- **F061** line 75: The write() cap `szGuildIntro > 256` on a BYTE could never fire, so the narrowed intro length has no working cap. The dead check is removed. The read() check at line 43 was also dead but harmless, since a BYTE cannot exceed 255.
  - **Fixed** in `6416b508`: Check guild info string sizes before narrowing them in write().

### `Client/Packet/Gpackets/GCShowGuildMemberInfo.cpp`

- **F062** line 55: The write() cap `szGuildMemberIntro > 256` on a BYTE could never fire, so the narrowed length has no working cap. The dead check is removed.
  - **Fixed** in `6416b508`: Check guild info string sizes before narrowing them in write().

### `Client/Packet/Gpackets/GCShowUnionInfo.h`

- **F063** line 110: The SingleGuildInfo::write() cap `szGuildIntro > 256` on a BYTE could never fire, so the narrowed length has no working cap. The dead check is removed, as is the matching dead check in read() (line 83).
  - **Fixed** in `6416b508`: Check guild info string sizes before narrowing them in write().

### `Client/Packet/Gpackets/GCShowWaitGuildInfo.cpp`

- **F064** line 92: The write() cap `szGuildIntro > 256` on a BYTE could never fire, so the narrowed length has no working cap. The dead check is removed, as is the matching dead check in read() (line 44).
  - **Fixed** in `6416b508`: Check guild info string sizes before narrowing them in write().

### `Client/Packet/Gpackets/GCSystemMessage.cpp`

- **F065** line 56: The write() cap `szMessage > 256` on a BYTE could never fire. If szMessage is the narrowed m_Message.size(), messages of 256 bytes or more are not refused. The dead check is removed.
  - **Fixed** in `7be81c0a`: Cap single-string GC packet writes before narrowing the length.

### `Client/Packet/GuildInfo.cpp`

- **F066** line 73: 'm_GuildExpireDate == "";' is a comparison used as a statement; '=' was surely meant, so a zero-length expire date does not clear the string, which keeps its previous value. Behaviour kept; the statement is now cast to (void).
  - **Fixed** in `be75ecd4`: Clear the guild expire date on a zero-length read.

### `Client/Packet/PCOustersInfo2.h`

- **F067** line 51: getPacketSize() and getMaxSize() ended with 'return ... + szLevel;' followed by a dead '+ szExp;' statement, so both sizes leave out one szExp. write() emits m_AdvancementLevel and m_AdvancementGoalExp. The dead statements were removed, so the returned values are unchanged. tests/wire-layout.txt pins getMaxSize, so correcting the size would change the wire inventory.
  - **Fixed** in `7cc8deec`: Count the advancement goal exp in the ousters and vampire sizes.

### `Client/Packet/PCVampireInfo2.h`

- **F068** line 50: Same as PCOustersInfo2.h: the dead '+ szExp;' after 'return ... + szLevel;' in getPacketSize() and getMaxSize() means both sizes leave out szExp for m_AdvancementGoalExp. The dead statements were removed and the values are unchanged. getMaxSize also contains 'szGuildMemberRank + + szBYTE' (unary plus, harmless).
  - **Fixed** in `7cc8deec`: Count the advancement goal exp in the ousters and vampire sizes.

### `Client/Packet/Properties.cpp`

- **F069** line 116: key_begin, sep and value_begin are uint but hold std::string::find* results. Where size_t is 64-bit, the '== std::string::npos' checks are always false. An all-whitespace line then continues with key_begin = 0xFFFFFFFF. The 'missing separator' and 'missing value' throws are skipped, and sep - 1 and sep + 1 are computed from truncated values. Left unchanged (see platform_dependent).
  - **Already fixed:** 8957741e changed the search positions to `std::string::size_type`, so the `npos` checks work, and `tests/unit/test_properties_parse.cpp` covers the whitespace-only line and the missing separator.

### `Client/Packet/Rpackets/RCRequestedFile.cpp`

- **F070** line 70: RCRequestedFileInfo::write stores the filename length with `BYTE szFilename = m_Filename.size();`, which truncates before any check. A 256-byte filename throws the 'szFilename == 0' error. A filename of 257 bytes or more writes a wrapped length byte followed by the full string, giving a malformed packet. The removed `szFilename > 255` check (always false on a BYTE, in read() and write()) never guarded against this. Behaviour is unchanged.
  - **Fixed** in `2feef93c`: Cap Gpacket, Lpacket and Rpacket strings before narrowing.

### `Client/Packet/TextInfo.cpp`

- **F071** line 71: In write(), 'BYTE szTopic = m_Topic.size();' truncates silently. The 'szTopic > 255' guard after it could never fire, so a topic longer than 255 characters writes a wrong length byte and then the full string. The dead guard was removed (behaviour identical); the truncation remains. The matching read-side guard at the old line 38 was merely dead.
  - **Fixed** in `663e559b`: Check textinfo writer and topic sizes before narrowing them.

### `Client/PacketFunction.cpp`

- **F072** line 335: InitPacketItemTable declares a local `MItem* g_pPacketItemShoulder[SHOULDER_MAX]` that shadows the file-scope array at line 150. The global therefore never receives pShoulder1 and stays all-NULL, so `PacketItem(g_pPacketItemShoulder, pInfo->getShoulderType())` (about line 952) never finds a shoulder item. pShoulder1 is also leaked. I kept this behaviour by deleting the local and both of its writes together.
  - **Fixed** in `6ebdc97c`: Store the shoulder armour template in its packet item table.
- **F073** line 3728: `case CHECK_VERSION_ERROR:` opens its dialog and then falls through into `default:`, which opens a second, generic STRING_ERROR_ETC_ERROR dialog. This looks unintended (every other case ends in `break`). I kept it and marked it `[[fallthrough]]`.
  - **Fixed** in `10b1da62`: Show one dialog for the version check login error.

### `Client/PacketHandler/GCAddNPCHandler.cpp`

- **F074** line 68: The condition `(spriteInfo.IsNPCSprite() && spriteInfo.IsSlayerSprite()) \|\| spriteInfo.IsSlayerSprite()` reduces to `IsSlayerSprite()`, so the IsNPCSprite() test has no effect. It is probably a typo for another predicate. The added parentheses only spell out the existing binding.
  - **Not a defect:** testing the slayer bit alone is the rule, matching the next branch, which tests `IsOustersSprite()` alone, so the redundant NPC term is harmless and no other predicate fits a typo.

### `Client/PacketHandler/GCDeleteandPickUpOKHandler.cpp`

- **F075** line 268: Low confidence. The condition now reads `(g_pQuickSlot!=NULL&&g_pPlayer->IsSlayer()) \|\| (g_pPlayer->IsOusters() && (g_pArmsBand1 != NULL \|\| g_pArmsBand2 != NULL))`. The Slayer branch requires g_pQuickSlot and the Ousters branch does not. This is probably intended (Ousters use armsbands instead of the quickslot belt), but it is worth a look.
  - **Not a defect:** each race has its own quick-slot container, the belt for slayers and the armsbands for Ousters, and `PickupItemToQuickslot` only picks a non-NULL armsband, so the Ousters branch needs no `g_pQuickSlot` check.

### `Client/PacketHandler/GCDownSkillOKHandler.cpp`

- **F076** line 29: The guard `if( curLevel <= 0 && curLevel >= 29 ) { UI_PopupMessage( STRING_ERROR_ETC_ERROR ); return; }` (original line 29, right after `curLevel --;`) could never be true, so it was dead code. I removed it, which keeps behaviour identical. It was clearly meant as a range check (probably `curLevel < 0 \|\| curLevel > 29`; `curLevel == 0` is handled further down). As a result, a server-sent skill down to an out-of-range level is never rejected.
  - **Not a defect:** the server sends `GCDownSkillOK` only after it has lowered the level itself, so the client must mirror it, and the original `<= 0` bound would even have blocked the level-0 re-learn path just below.

### `Client/PacketHandler/GCPartyPositionHandler.cpp`

- **F077** line 60: The in-sight test compared `pInfo->zoneID == pInfo->zoneID`, which is always true. The unused local just above it, `int zoneID = (g_bZonePlayerInLarge ? g_nZoneLarge : g_nZoneSmall);`, was clearly the intended right-hand side, meaning 'is the member in my current zone'. I kept the current behaviour: `bInSight` still depends only on the tile distance, not on the zone, so a party member in another zone at nearby coordinates is marked in sight. The self-comparison and the unused `zoneID` local were removed, so this report is the only record of what was intended.
  - **Fixed** in `9aa4e5c9`, `20216839`: Mark a party member in sight only when in the player's zone.

### `Client/PacketHandler/GCShopBuyOKHandler.cpp`

- **F078** line 107: `int total = //pOldItem->GetNumber() + pItem->GetNumber();` has its first addend commented out. The 'MaxNum exceed' check therefore compares only the incoming item's count, not the merged stack, so it almost never fires. This was already the case; I only added a sign cast on the comparison line.
  - **Not a defect:** the packet's item count is already the merged stack total the server computed, so adding the old stack back would count it twice and reject valid merges near the maximum.

### `Client/PacketHandler/RCPositionInfoHandler.cpp`

- **F079** line 75: The code computed `int zoneID = (g_bZonePlayerInLarge ? g_nZoneLarge : g_nZoneSmall);` and then tested `pInfo->zoneID == pInfo->zoneID`, which is always true for an int. The intent was almost certainly `pInfo->zoneID == zoneID`. As written, a party member's bInSight depends only on X/Y distance and ignores which zone they are in. I kept that behaviour: removed the always-true test and the now-unused local.
  - **Fixed** in `9aa4e5c9`, `20216839`: Mark a party member in sight only when in the player's zone.

### `Client/SpriteLib/CShadowSprite.cpp`

- **F080** line 1649: In memcpyShadowDarkness, case 3 (pixels % 4 == 3) handles the leftover pixels wrongly. After darkening one WORD at qpDest it never moves the pointer on, so the next DWORD write darkens pixel 0 a second time and also pixel 1. The pointer then advances by only 4 bytes, so pixel 2 is never darkened. The code probably needs `qpDest = (QWORD*)((WORD*)qpDest + 1);` after the single-pixel write, the way case 1 does it. I left the behaviour as it is.
  - **Fixed** in `32209527`: Darken every pixel of a 3-mod-4 shadow darkness run once.

### `Client/SpriteLib/CSpriteSurface_Effects.cpp`

- **F081** line 221: memcpyPalEffectScreenAlpha has an empty body. The palette 'Screen alpha' effect draws nothing, as its comment says ('Legacy placeholder'). I only added (void) casts.
  - **Not a defect:** the empty body is upstream's own, and nothing on the SDL build reaches it, because the palette table's screen-alpha slot is NULL and every palette-effect caller passes `EFFECT_SCREEN`.
- **F082** line 365: memcpyPalEffectWipeOut ignores the palette ('pal' is unused, now (void)pal;). It memcpy's (drawPixels<<1) bytes from the BYTE* palette-index source into the WORD* destination, so it copies raw index bytes as 16-bit pixels and reads twice as many source bytes as there are pixels. It also advances pSource by pixel count, not by byte pairs. The code comment already calls this a 'legacy raw copy, not palette expansion'. The behaviour is unchanged.
  - **Fixed** in `5668f6c7`: Expand palette indices in the palette wipe-out effect.

### `Client/SpriteLib/MPalette.cpp`

- **F083** line 3: The include line held a literal backslash-n followed by 'using namespace std;' (an earlier mechanical edit wrote a literal backslash-n instead of a newline). The preprocessor ignored those extra tokens, so the using-directive never applied. I removed the dead tokens to keep today's behaviour. I did not add a real using-directive, because that would change name lookup.
  - **Not a defect:** the file spells `std::ifstream` and `std::ofstream` in full and needs no using-directive, so dropping the ignored tokens changes nothing.

### `Client/SpriteLib/SpriteLibBackendSDL.cpp`

- **F084** line 1233: spritectl_load_sprite_from_file accepts a colorkey parameter but never uses it. Callers such as spritectl_load_pack pass a colorkey that has no effect. I kept this as is and added (void)colorkey;.
  - **Not a defect:** transparency comes from the RLE runs kept on the sprite, not from a key colour, and the only caller passes 0xFFFF, meaning none, so the parameter is vestigial.

### `Client/UIDialog.cpp`

- **F085** line 318: `if( m_AnswerIDMap.size() >= answerID ) answerID = m_AnswerIDMap[answerID-1] + 1;` is always true when answerID is 0, and then reads m_AnswerIDMap[-1] (out of bounds). I kept this and made the existing conversion explicit with static_cast<size_t>.
  - **Fixed** in `1ab28807`, `635ce278`: Pass a talk menu id below 1 through instead of reading map[-1].
- **F086** line 571: `strlen(content)` where content is `g_pPCTalkBox->GetContent()`, i.e. `MString::GetString()`, which returns NULL for an empty MString. The result is unused (only commented-out code reads lenContent), but the strlen on a possibly NULL pointer is still undefined behaviour. I kept the call and marked the variable [[maybe_unused]].
  - **Fixed** in `7d16c726`: Drop the unused strlen of the npc talk subject.

### `Client/UIMessageManager.cpp`

- **F087** line 1594: Execute_UI_CHECK_EXIST_ID did delete[] on a void* through DeleteNewArray(void_ptr), which is undefined behaviour. It now deletes through pName, the same address as a const char*. For a trivially destructible type this behaves the same in practice, but the real element type of the allocation is unknown. git grep finds no sender of UI_CHECK_EXIST_ID anywhere in the tree, so the handler is registered but never dispatched today.
  - **Already fixed:** 2c8c3fc6 changed the delete to go through the `const char*` `pName`, which is well-formed, and nothing sends `UI_CHECK_EXIST_ID`, so the handler never runs anyway.
- **F088** line 2555: The call SafeFormat::Format(strTemp, GetGameString(STRING_USER_REGISTER_ID_LENGTH), PlayerInfo::minIDLength, nameLen>PlayerInfo::maxIDLength) passes a bool as the maximum length, so the message prints 1 or 0. The same call at orig 2423 passes PlayerInfo::maxIDLength. The cast preserves the current behaviour. The same bug is at orig 2617.
  - **Fixed** in `3d732740`: Show the real maximum in the ignore and accept id-length messages.
- **F089** line 2617: The same bool-for-maxIDLength argument as at orig 2555 (nameLen>PlayerInfo::maxIDLength passed to SafeFormat::Format). Preserved.
  - **Fixed** in `3d732740`: Show the real maximum in the ignore and accept id-length messages.
- **F090** line 3587: For the Christmas tree (ITEM_CLASS_EVENT_TREE), the filter excludes types 12, 25-28 and 41, but mx/my are only assigned for type < 12, < 25 and < 41. A type of 42 or higher reached pointX/pointY with indeterminate values (undefined behaviour). They are now initialised to 0, which changes nothing except that undefined case.
  - **Already fixed:** 2c8c3fc6 initialised `mx` and `my` to 0, so a type of 42 or more scans from its own cell, finds no matching part and sets no merge; the server knows tree parts only up to 41.
- **F091** line 3878: In Execute_UI_ITEM_DROP_TO_GEAR the RACE_OUSTERS case calls g_pOustersGear->ReplaceItem(pMouseItem, (MVampireGear::GEAR_VAMPIRE)left, ...), a vampire enum cast. The RemoveItem counterpart (orig 4281) uses MOustersGear::GEAR_OUSTERS. Preserved; no warning points at it.
  - **Fixed** in `161ae651`: Cast the ousters gear slot to the ousters enum.
- **F092** line 8453: Execute_UI_FINISH_REQUEST_DIE_BUTTON: the comment above it says `left == TRUE : timeout` and the flag was stored in bTimeout, but nothing ever read it. A timeout and a manual close therefore do exactly the same thing (resurrect/waypoint request). That is kept as is: the unused local was removed and nothing else changed.
  - **Not a defect:** a timeout is meant to confirm the death dialog exactly as the button does, and neither `CGResurrect`, which has no fields, nor `CGSelectWayPoint` could carry the flag.
- **F093** line 10997: Execute_UI_UNDISPLAY_ITEM called g_pStorage2->GetItem(right) before any null check on g_pStorage2 and discarded the result. MStorage::GetItem reads m_pStorage through `this`, so a null g_pStorage2 was undefined behaviour at that point. The removed line had no defined side effects, and an optimising compiler would drop the dead read anyway. The later g_pStorage2->RemoveItem(left) call still has no null guard, as before.
  - **Fixed** in `dfaeda5b`: Return from the personal-shop undisplay handler without a storage.
- **F094** line 11141: Execute_UI_BUY_STORE_ITEM compares an object ID (TYPE_OBJECTID) with NULL, which means comparing with 0. The author probably meant OBJECTID_NULL. The comparison with 0 is kept exactly as it was.
  - **Not a defect:** server object IDs start above 10000, and the selection is `OBJECTID_NULL` while the mouse is on the dialog, so `== OBJECTID_NULL` would block every personal-shop purchase; the packet never uses the selection.

### `VS_UI/DIK_Defines.h`

- **F095** line 109: Five macros in DIK_Defines.h have different values from basic/InputCodes.h and the real DirectInput scan codes: DIK_LWIN/RWIN/APPS 0x5B/0x5C/0x5D (Windows VK codes) vs 0xDB/0xDC/0xDD, DIK_SYSRQ 0x54 vs 0xB7, DIK_KANJI 0x71 vs 0x94. Which value a TU sees depends on include order. Also DIK_PRINT (0xD2) equals DIK_INSERT, and DIK_BREAK (0xD3) equals DIK_DELETE. The #undef keeps the current later-definition-wins behaviour and does not resolve the mismatch.
  - **Fixed** in `2b25be1b`: Give VS_UI's DIK key codes their DirectInput values.

### `VS_UI/src/UserOption.cpp`

- **F096** line 235: fscanf('%d', &ALPHA_DEPTH) wrote 4 bytes into a 1-byte BYTE member. This was undefined behaviour and only harmless because 3 padding bytes follow it before the BOOL DefaultAlpha. The '%hhu' fix stores the same value on every little-endian target (including negative or >255 input, which wraps to the same low byte), but it no longer writes the 3 padding bytes. UserOption is never byte-copied or byte-compared, so this has no visible effect.
  - **Already fixed:** 2c8c3fc6 changed the conversion to `%hhu`, which matches the `BYTE`, and `SaveToFile` writes values from 0 to 255, so the option round-trips and nothing writes into the padding.

### `VS_UI/src/VS_UI_Dialog.cpp`

- **F097** line 613: In ShowButtonWidget, the guard `static_cast<size_t>(i) <= m_p_menu[...].sz_menu_str.size()` uses `<=`, and the code then indexes `sz_menu_str[i]`. That is an off-by-one in the guard. It is harmless today only because the enclosing for-loop already keeps `i < size()`. Left as is.
  - **Fixed** in `99b0f027`: Bound the dialog menu-string guard below the vector size.

### `VS_UI/src/VS_UI_Exchange.cpp`

- **F098** line 775: The condition parses as (gpC_Imm && m_focus_grid_x != ...) \|\| m_focus_grid_y != ..., and the parentheses now spell that out. If gpC_Imm is NULL and the y focus changed, gpC_Imm->ForceUI() is called through a null pointer. The intent was probably gpC_Imm && (x changed \|\| y changed). Behaviour is unchanged.
  - **Fixed** in `941afb45`: Guard both grid-focus tests with the gpC_Imm null check.
- **F099** line 879: Same pattern: (gpC_Imm && m_focus_grid_x != NOT_SELECTED) \|\| m_focus_grid_y != NOT_SELECTED can call gpC_Imm->ForceUI() with gpC_Imm NULL. The intent was probably gpC_Imm && (x selected \|\| y selected). Behaviour is unchanged.
  - **Fixed** in `941afb45`: Guard both grid-focus tests with the gpC_Imm null check.

### `VS_UI/src/VS_UI_ExtraDialog.cpp`

- **F100** line 465: In C_VS_UI_MONEY_DIALOG::KeyboardControl, case MONEY_WITHDRAW calls RunWithdrawLimit() and then falls through into MONEY_BRING_FEE, so it also calls RunBringFeeLimit(). The break is probably missing. I kept the behaviour and made the fallthrough explicit with [[fallthrough]];.
  - **Fixed** in `1200c2bc`: Stop the withdraw-limit notice from also raising the fee notice.
- **F101** line 1897: In the DESC_DIALOG core-zap block, TempColor was computed (green if IsHasAllCoreZap(), dark gray otherwise) and never used. The matching code in VS_UI_Description.cpp uses it to colour the reward lines, so the colour looks lost here. I removed the dead computation; what is displayed is unchanged.
  - **Not a defect:** the DESC dialog draws every row of `m_rep_string` in one colour, so the colour could never be shown there, and colouring the reward lines would be a new feature.

### `VS_UI/src/VS_UI_GameCommon.cpp`

- **F102** line 1047: C_VS_UI_TRIBE::ShowButtonDescription indexes slayer_string/vampire_string/ousters_string[p_button->GetID()] guarded only by GetID() <= EXEC_MAX. The removed '>= 0' test never did anything because id_t is unsigned. Kept as is.
  - **Fixed** in `36cffe75`: Realign the tribe panel tooltips with their button IDs.
- **F103** line 2539: C_VS_UI_GEAR::C_VS_UI_GEAR(): when g_eRaceInterface is not SLAYER/VAMPIRE/OUSTERS (e.g. RACE_MAX), the race switch sets nothing. m_pC_gear_spk and m_pC_gear_slot_spk are not assigned and Set() is not called. The six local button offsets were read uninitialised (undefined behaviour); they are now initialised to 0, so that path uses defined values of 0. The rest of the behaviour is kept.
  - **Fixed** in `4cd964e8`: Define the out-of-range race paths in the common windows.
- **F104** line 3610: In C_VS_UI_GEAR's use-item path, `pGear` is assigned only for the three real races and is then dereferenced without a check. For any other g_eRaceInterface value (RACE_MAX), it used to be an uninitialised pointer dereference. It is now initialised to NULL, which crashes deterministically instead. This path is unreachable with a real race.
  - **Fixed** in `4cd964e8`: Define the out-of-range race paths in the common windows.
- **F105** line 8176: `gpC_Imm != NULL` is always true on non-Windows builds, because the macro expands to the address of a static stub. It is a real NULL check on Windows. Left unchanged.
  - **Not a defect:** on Windows `gpC_Imm` is a nullable `CImm*` and the test is real, and elsewhere it is a static stub whose `ForceUI` does nothing, so the always-true test is harmless.
- **F106** line 9565: `bool C_VS_UI_SKILL::IsPixel2(int _x, int _y) {}` has no return statement, which is undefined behaviour if it is ever called. It was not in my list and I did not touch it.
  - **Not a defect:** the empty body quoted above never existed, because `IsPixel2` has returned `GetFocusSlot(_x, _y) != NOT_SELECTED` since the first commit.
- **F107** line 10030: Mine progress check in the skill window's Show: `p_item && (MINE...) \|\| (BOMB_MATERIAL && type>4 && MAKE_MINE) \|\| (BOMB_MATERIAL && type<5 && MAKE_BOMB)` parses as `(p_item && A) \|\| B \|\| C`. The second and third alternatives call p_item->GetItemClass() without the null guard, so a NULL p_item crashes whenever the first alternative is false. I kept that precedence and only added the parentheses that make it explicit.
  - **Fixed** in `d8bd5a86`: Null-check the mine progress item before every item test.
- **F108** line 10801: The C_VS_UI_PARTY_MANAGER constructor had the no-effect statement `m_away_button_focused;`, probably meant as `m_away_button_focused = false;`. The bool member is never initialized in the constructor, yet Show() reads it (`m_away_focused == i && m_away_button_focused`) before MouseControl first sets it. I removed the statement without adding an initialization, so behaviour is unchanged.
  - **Not a defect:** `Show()` never reads it first, because its only read sits behind `m_away_focused == i`, which is true only after `MouseControl`'s `M_MOVING` branch has assigned `m_away_button_focused`.
- **F109** line 11109: In C_VS_UI_PARTY_MANAGER::Show, the lines at 11109 and 11118 test `i != 0 && g_pParty->GetMemberInfo(i-1)->HP == 0` without a null check, although the same loop guards GetMemberInfo(i-1) against NULL elsewhere (`if(info)` at 11084, `else if(... != NULL)` at 11013). I only added the parentheses that spell the existing precedence.
  - **Fixed** in `deebe7a4`: Null-check the party member before reading its HP for the face tint.
- **F110** line 12604: `MyGrade` is left unset when g_eRaceInterface is RACE_MAX, in ShowButtonDescription (12604), ShowButtonWidget (12774) and Run (13069), and is then used. The added `default: break;` keeps that behaviour.
  - **Fixed** in `4cd964e8`: Define the out-of-range race paths in the common windows.
- **F111** line 12931: In ShowButtonWidget (GRADE3 branch), `status` was read in switch(status) without ever being set when GetFocusState() was false or no branch assigned it. That read garbage (undefined behaviour). It now starts as SKILLSTATUS_NULL, so on that path the switch does nothing. `sprID` is also used without being set when status==SKILLSTATUS_OTHER was never assigned on this path; no warning is reported for it and I left it unchanged.
  - **Already fixed:** 2c8c3fc6 initialised `status` to `SKILLSTATUS_NULL`, so an unfocused button does nothing, and `sprID` is read only for `SKILLSTATUS_OTHER`, which is always assigned together with it.
- **F112** line 12951: In C_VS_UI_INFO::ShowButtonWidget, `ss= ss = list.begin()+...` appears twice (vampire and ousters branches, original 12951 and 12969). It is a harmless typo that assigns twice; I rewrote it as one assignment with the same result.
  - **Not a defect:** assigning the same iterator twice has no side effect, so the single assignment gives the same result.
- **F113** line 12960: In ShowButtonWidget, the vampire and ousters branches dereference `*ss` without the `list.size() >=` bounds check the slayer branch has. They also call GetSkillStatus on `(*g_pSkillManager)[m_skill_domain]` rather than on the SKILLDOMAIN_VAMPIRE/OUSTERS domain whose list they walk. I kept both as they are.
  - **Fixed** in `81253b22`, `8cd6d728`: Pick the ousters grade3 text box row as the icon buttons do.
- **F114** line 14482: Not touched: SafeFormat::Format(sz_temp, "%d", (goal_exp - exp_remain)*100/max(1, goal_exp)) passes an __int64 to %d. It is not in my list and no compiler warned; noting it for SafeFormat review.
  - **Not a defect:** `SafeFormat` is not a varargs printf, since it stores the 64-bit argument with its size and prints `%d` through `%lld`, as `tests/unit/test_safe_format.cpp` covers.
- **F115** line 14564: SkillInfoMouseControl, RACE_VAMPIRE: the unused local domain_level dereferenced g_pSkillManager one line before the if(g_pSkillManager && ...) null check. Deleting the unused local also removes that unguarded dereference, which would have been undefined behaviour if the pointer was null.
  - **Already fixed:** 2c8c3fc6 removed the unused `domain_level` line, so the block goes straight to the guarded `if (g_pSkillManager && ...)` and no unguarded dereference remains.
- **F116** line 15282: C_VS_UI_INFO mouse control, grade description: pszGrade is only assigned for SLAYER/VAMPIRE/OUSTERS and was read uninitialised (passed to %s) when g_eRaceInterface is any other value. After adding default: break; it is now initialised to NULL, so that path passes a NULL string. Not reachable in practice.
  - **Already fixed:** 2c8c3fc6 initialised `pszGrade` to NULL, which `SafeFormat` prints as an empty string, and `g_eRaceInterface` only ever holds the three races, so that path is unreachable anyway.
- **F117** line 15503: Face sprite load: spriteID was uninitialised for a race outside the three handled ones and was passed to LoadFromFileData. It is now initialised to 0 (needed after adding default:).
  - **Already fixed:** 2c8c3fc6 initialised `spriteID` to 0, the same fallback the party-face loop has always used, and `g_eRaceInterface` only ever holds the three races, so that path is unreachable anyway.
- **F118** line 16720: GetChinhoLevel computed a title-bucket index (cur_ching_num) and last_num that the 2007 edit stopped using: it always formats chingho_name[0] with level. The dead computation was removed as the recipe says. That the function ignores the level bucket looks intentional (the comment says so) but may be worth confirming.
  - **Not a defect:** the 2007 edit deliberately formats `ADVANTE_0` with the full advancement level, every ADVANTE string has the same `<name> %d` shape, and the removed bucket computation had no effect.
- **F119** line 20026: The C_VS_UI_HPBAR ctor (and likewise the C_VS_UI_EFFECT_STATUS ctor at original line 21024) leaves the sprite pack pointer NULL for any race outside the three handled ones and then dereferences it at once in Set(...). Preserved; the added default: break; runs no statements.
  - **Fixed** in `4cd964e8`: Define the out-of-range race paths in the common windows.
- **F120** line 20276: C_VS_UI_HPBAR::MouseControl: 'bool re;' is only assigned inside the RACE_SLAYER/VAMPIRE/OUSTERS cases. Any other g_eRaceInterface (e.g. RACE_MAX) falls to the new default: break; and later reads 're' uninitialised in the M_LEFTBUTTON_DOWN case. The behaviour was preserved and not initialised.
  - **Fixed** in `4cd964e8`: Define the out-of-range race paths in the common windows.
- **F121** line 21292: C_VS_UI_EFFECT_STATUS::MouseControl: the inner check 'select <= STATUS.size() + WarInfo.size()' uses <= where < looks intended. It is harmless because the enclosing if already requires select < that sum. Preserved as is; only the casts were added.
  - **Not a defect:** the enclosing test already requires `select` below the combined size, so the inner upper clause is always true with `<=` or `<`, and `WarInfo[count]` stays in bounds.
- **F122** line 21608: C_VS_UI_EFFECT_STATUS::Show() (also line 21720 in vertical mode): the status loop declares its own `for(int i ...)`, which hides the outer `int i=0`. The WarInfo loop then starts from the outer i, still 0, so war icons are drawn on top of the first status icons instead of after them. Kept as is; only the sign-compare cast was added.
  - **Fixed** in `5f146ff4`: Draw war icons after the status icons in the effect status window.
- **F123** line 23390: C_VS_UI_TEAM_LIST constructor (search_x, search_x2, search_y) and C_VS_UI_TEAM_LIST::Show (search_y, line 23522): these were never assigned when g_eRaceInterface is RACE_MAX (undefined behaviour). They are now 0 on that path, per the -Wmaybe-uninitialized recipe. Every real race path is unchanged.
  - **Already fixed:** 2c8c3fc6 initialised `search_x`, `search_x2` and `search_y` to 0 and added `default: break;`, and the three real races still assign the same values.
- **F124** line 23587: C_VS_UI_TEAM_LIST::Show (also line 23644): while a search filter is active, a row is still skipped when m_scroll+i is past the end of the unfiltered list. This is probably harmless because the search list is a subset of the full list. The added parentheses spell out the existing precedence and do not change it.
  - **Not a defect:** the search lists are rebuilt as subsets of the full lists, which only grow, so a row past the end of the full list is always past the end of the search list too.
- **F125** line 23998: C_VS_UI_TEAM_MEMBER_LIST constructor: RACE_SLAYER opens SPK_VAMPIRE_TEAM_MEMBER, while RACE_VAMPIRE and RACE_OUSTERS open SPK_SLAYER_TEAM_MEMBER. The mapping looks swapped. Kept as is; only `default: break;` was added.
  - **Fixed** in `dd49d6eb`: Open each race's own pack for the team member list close button.
- **F126** line 27170: C_VS_UI_TEAM_MEMBER_INFO::IsPixel: `re == false && GuildGrade == 1 \|\| GuildGrade == 2` parses as `(re == false && GuildGrade == 1) \|\| GuildGrade == 2`. The intent was almost certainly `re == false && (GuildGrade == 1 \|\| GuildGrade == 2)`. I added parentheses that keep the current parse.
  - **Not a defect:** the two parses differ only when `re` is already true, and the block can only set `re = true` after a pure pixel lookup, so both return the same value for every input.
- **F127** line 27972: C_VS_UI_OTHER_INFO constructor: ZeroMemory (memset) over PLAYER_INFO, which contains `std::string PLAYER_NAME`, overwrites the string's internals. That is undefined behaviour and can leak or corrupt the string's buffer. Kept as is, with a (void*) cast to state the raw-memory intent.
  - **Fixed** in `00d31a78`: Reset the other-info player record by assignment, not memset.
- **F128** line 27972: Not edited (not in this batch). ZeroMemory/memset over C_VS_UI_OTHER_INFO::PLAYER_INFO (VS_UI/src/header/VS_UI_GameCommon.h:3371), whose first member is std::string PLAYER_NAME. It is undefined behaviour: in the constructor at :27972 it zeroes an already-constructed string, and in Start() at :29360 it clobbers a string that may own a heap buffer, which leaks or corrupts it.
  - **Fixed** in `00d31a78`: Reset the other-info player record by assignment, not memset.
- **F129** line 28373: C_VS_UI_OTHER_INFO::MouseControl: `if(temp_str[0]!=NULL)` tests a static char array, which is never null, so the check was always true. I removed it and the Set call is now unconditional, as it effectively was before. The commented-out `default: temp_str[0]=NULL;` shows the author meant a 'no text' sentinel that never worked. In practice num is always 0..2 because of the _y bounds.
  - **Not a defect:** the region test bounds `_y` so that `num` is always 0, 1 or 2, and each case fills both strings, so the unconditional `Set` is always right and the sentinel could never apply.
- **F130** line 29360: C_VS_UI_OTHER_INFO::Start: the same ZeroMemory over PLAYER_INFO. Here it runs on an already-constructed std::string PLAYER_NAME, which is then assigned "". This can leak the old heap buffer or corrupt the string. Kept as is, with a (void*) cast.
  - **Fixed** in `00d31a78`: Reset the other-info player record by assignment, not memset.
- **F131** line 29452: C_VS_UI_OTHER_INFO::RefreshImage: spriteID is set only for SLAYER, VAMPIRE and OUSTERS. With g_eRaceInterface == RACE_MAX the original passed an uninitialised spriteID to LoadFromFileData. Adding `default: break;` made clang report -Wsometimes-uninitialized, so I initialised it as `int spriteID = 0;` per the recipe. This is the only change of an observable value: the unreachable-in-practice RACE_MAX path now uses 0 instead of an indeterminate value. The same pattern (sz_string left unset on RACE_MAX) exists in C_VS_UI_TEAM_REGIST::Show, which no compiler flagged, so I left it alone.
  - **Fixed** in `4cd964e8`: Define the out-of-range race paths in the common windows.
- **F132** line 34132: C_VS_UI_WORLDMAP constructor: line 34131 sets m_surface_w = m_pC_minimap_spk->GetWidth(MINIMAP_MAIN), and line 34132 then sets m_surface_w = m_pC_minimap_spk->GetHeight(MINIMAP_MAIN), overwriting it. The second assignment was evidently meant for m_surface_h. I left it unchanged. It is not a listed warning site.
  - **Fixed** in `e72e9402`: Set the minimap surface height, not the width twice.

### `VS_UI/src/VS_UI_PetStorage.cpp`

- **F133** line 1409: ExecF_sellConfirm2 computed item_count from m_pC_dialog_multi_buy_confirm->GetValue() but never used it: SendMessage(UI_SELECT_PERSNALSHOP_SLOT, ...) ignores the quantity entered in the multi-buy dialog. Behaviour preserved: the unused variable is gone, and the GetValue() call is kept as an expression statement.
  - **Not a defect:** `ExecF_sellConfirm2` is referenced only from commented-out code, `UI_SELECT_PERSNALSHOP_SLOT` has no handler, and the server's `CGBuyStoreItem` carries no quantity, so the ignored count cannot matter.

### `VS_UI/src/VS_UI_Title.cpp`

- **F134** line 6060: C_VS_UI_OPTION::MouseControl: `if(false == m_IsTitle) _x -=m_vampire_plus_x; _y-=m_vampire_plus_y;` guards only the _x adjustment, so the _y adjustment always runs. The author almost certainly meant both to be guarded. I kept the current behaviour by putting `_y-=m_vampire_plus_y;` on its own line at the outer indentation.
  - **Not a defect:** in title mode, the only case the guard would exclude, the constructor sets `m_vampire_plus_y` to 0, so the unguarded subtraction does nothing there and behaves exactly like the guarded form.

### `VS_UI/src/VS_UI_mouse_pointer.cpp`

- **F135** line 262: In the switch(g_eRaceInterface) under the CURSOR_RESIZE branch, `temp_cursor` is never assigned when g_eRaceInterface is RACE_MAX (or any other unhandled value), but it is then used as an index into g_mouse_point_fix and as a frame number. That is undefined behaviour. I left it as it is and only added `default: break;`.
  - **Fixed** in `e8fc57e2`: Start the resize cursor index from the slayer cursor.
- **F136** line 293: Same pattern in the mouse-focused-window branch: `temp_cursor` is never assigned for RACE_MAX before it is used as the sprite index. After `default: break;` was added, clang raised -Wsometimes-uninitialized, so it is now initialised to 0. That changes the value only on this path, which was undefined behaviour before; every defined path behaves as before.
  - **Already fixed:** 2c8c3fc6 initialised `temp_cursor` to 0, which is `CURSOR_SLAYER_NORMAL`, a valid sprite, and `g_eRaceInterface` only ever holds the three races, so that path is unreachable anyway.

### `VS_UI/src/vs_ui_gamecommon2.cpp`

- **F137** line 2290: Original line number. In C_VS_UI_MIXING_FORGE::MouseControl: `if(gpC_Imm && m_focus_grid_x != distance_x/GRID_UNIT_PIXEL_X \|\| m_focus_grid_y != distance_y/GRID_UNIT_PIXEL_Y) gpC_Imm->ForceUI(...)`. It parses as `(gpC_Imm && x-changed) \|\| y-changed`, so the gpC_Imm null guard does not cover the `\|\|` branch. If gpC_Imm is NULL and only the y focus changes, it dereferences NULL. The intended meaning was probably `gpC_Imm && (x-changed \|\| y-changed)`. I kept the current behaviour and only added parentheses.
  - **Fixed** in `941afb45`: Guard both grid-focus tests with the gpC_Imm null check.
- **F138** line 2339: Original line number. Same pattern in C_VS_UI_MIXING_FORGE::MouseControl: `if(gpC_Imm && m_focus_grid_x != NOT_SELECTED \|\| m_focus_grid_y != NOT_SELECTED) gpC_Imm->ForceUI(...)`. If gpC_Imm is NULL and m_focus_grid_y != NOT_SELECTED, it dereferences NULL. I kept the behaviour and only added parentheses.
  - **Fixed** in `941afb45`: Guard both grid-focus tests with the gpC_Imm null check.
- **F139** line 3001: C_VS_UI_MIXING_FORGE::IsCorrectType starts with an unconditional `return true;`, so the whole switch after it is dead code. Preserved; the switch only got its default label.
  - **Not a defect:** the dead table disagrees with the server's `MixingItemInfo` levels (Armor Type A allows Slayer 5 and Vampire 2, the table 11 and 5), so leaving that rule to the server is correct.
- **F140** line 3805: The C_VS_UI_OUSTERS_SKILL_INFO constructor, and likewise the C_VS_UI_FINDING_MINE constructor (original ~5935), each assign `m_pC_button_group = new ButtonGroup(this);` twice, leaking the first ButtonGroup. Not warning-related; left unchanged.
  - **Fixed** in `347e05d8`: Allocate the skill-info and mine-finder button groups once.
- **F141** line 5081: C_VS_UI_MAILBOX::Show: titleSpriteID and contentsSpriteID were read uninitialized (undefined behaviour) when g_eRaceInterface is none of SLAYER/VAMPIRE/OUSTERS. Making the default case explicit surfaced -Wsometimes-uninitialized, so both are now initialised to 0 per the recipe. On that path (not reachable in normal play) the value changed from indeterminate to 0.
  - **Already fixed:** 2c8c3fc6 initialised `titleSpriteID` and `contentsSpriteID` to 0 and added `default: break;`, so the unhandled-race path reads defined values and the three real races are unchanged.
- **F142** line 5109: C_VS_UI_MAILBOX::Show: `m_mail[m_currentTab].size()-m_listCount` is unsigned arithmetic. It wraps when the tab holds fewer than m_listCount mails, so the clamp on m_overcnt never fires. Related: the loops at original lines 5136/5161 index `m_mail[m_currentTab][mailIndex+m_overcnt]` but only check `mailIndex < size()`, so the subscript can run past the end. The static_cast<size_t> added only spells the conversion the compiler already did; behaviour is unchanged.
  - **Fixed** in `58ad9d3c`: Keep the mailbox rows and clicks inside the scrolled mail list.
- **F143** line 8979: C_VS_UI_QUEST_INVENTORY::SetInventory(int i, BYTE Option) computed `int offset = i*2;` but never used it, and writes m_Inventory[i] and m_Inventory[i+1]. The unused offset suggests the writes were meant to go to [offset] and [offset+1]. As written, consecutive calls overwrite each other's second slot. The unused variable was deleted per the recipe; behaviour is unchanged. Line numbers are original.
  - **Not a defect:** `SetInventory(int, BYTE)` has no callers, so the overwrite can never happen, and the constructor does the real decoding at offset `2*i`.
- **F144** line 11061: C_VS_UI_SMS_MESSAGE::MouseControl (orig 11061) and C_VS_UI_SMS_LIST::MouseControl (orig 11619) collect the child widgets' MouseControl results into `re` and then ignore them, always returning true. The unused `re` was removed and all calls kept; behaviour is unchanged. Probably harmless, noted for completeness.
  - **Not a defect:** every child call still runs, and `WindowManager::MouseControl` discards the window's return value, so returning `re` would change nothing.
- **F145** line 14477: C_VS_UI_QUEST_LIST::SetQuestListInfo erases the list entries while `DeleteNew(TempInfo2)` is commented out. The _GQuestInfo pointers may therefore leak, or may be owned elsewhere. I only removed the unused local; the behaviour is unchanged.
  - **Not a defect:** `C_VS_UI_QUEST_MANAGER` owns the pointers and frees them in `ClearQuestInfo`, while the list only holds aliases, so nothing leaks and re-enabling the delete would free them twice.
- **F146** line 16073: C_VS_UI_QUEST_ITEM::Run: in case CLOSE_ID the `break;` is commented out, so control falls into ALPHA_ID. Closing the quest-item window therefore also toggles alpha and runs EMPTY_MOVE. I kept this behaviour with [[fallthrough]];. It looks unintended.
  - **Fixed** in `0981b0c3`: End the quest-item window's close case with a break.
- **F147** line 17268: C_VS_UI_POWER_JJANG::PowerjjangGambleResult: the loop condition is `i < m_Powerjjang_ItemList.size()-1`. On an empty list size()-1 wraps to SIZE_MAX, so the loop keeps calling ScrollDown until the scroll position matches, with a possible int overflow of i. The cast keeps this behaviour unchanged.
  - **Fixed** in `7c551284`: Stop the power-jjang gamble result loop wrapping on an empty list.

### `basic/DebugLog.cpp`

- **F148** line 251: log_line[2048] could cut off a full 2047-byte message plus its '[timestamp] [level] [file:line] ' prefix (GCC -Wformat-truncation). Following the local-array recipe, it is now sizeof(message) + 256 bytes. Lines that used to be truncated are now written in full.
  - **Already fixed:** 2c8c3fc6 sized `log_line` as `sizeof(message) + 256`, and the prefix is about 51 characters plus the source file's basename, so a full message fits.

### `basic/InputCodes.h`

- **F149** line 257: Five DIK_* values disagree with VS_UI/DIK_Defines.h: DIK_KANJI is 0x94 vs 0x71, DIK_SYSRQ 0xB7 vs 0x54, and DIK_LWIN/RWIN/APPS 0xDB-0xDD vs 0x5B-0x5D. Both headers now #undef before they #define, so the header included last still wins, exactly as before. The value a translation unit actually uses therefore depends on its include order.
  - **Fixed** in `2b25be1b`: Give VS_UI's DIK key codes their DirectInput values.

### `basic/PlatformSDL.cpp`

- **F150** line 514: g_config_file_path[PATH_MAX] could cut off the 'DarkEden.conf' suffix when the executable's directory is close to PATH_MAX long (GCC -Wformat-truncation). It is now PATH_MAX + sizeof("DarkEden.conf") bytes. It is a file-scope static, not a local, but I treated it as the recipe's local array: it is private to this .cpp and only ever passed on as a const char*. A long exe directory now gives the full config path instead of a truncated one.
  - **Already fixed:** 2c8c3fc6 sized the buffer as `PATH_MAX + sizeof("DarkEden.conf")`, and the executable's directory is at most `PATH_MAX - 1` characters with its slash, so the full path always fits.

### `tools/engine/sprite/src/frame.c`

- **F151** line 98: frame_array_get takes a `const FrameArray*` but returns a mutable `Frame*` into its storage, so the const is silently dropped (now an explicit cast). Callers can modify an array they were given as const. The signature is unchanged.
  - **Not a defect:** like `strchr`, it returns a mutable pointer into storage the vector owns on the heap, never a const object, so writing through it is well defined, and no caller holds a const `FrameArray*`.

### `tools/viewers/effect_viewer/main.cpp`

- **F152** line 257: The 'boundary check' self-test passes 99999 as a TYPE_FRAMEID (unsigned short), so it actually asks for frame 34463, not 99999. It only tests the boundary if 34463 is out of range. The truncation is now an explicit cast and the behaviour is unchanged.
  - **Fixed** in `d54f7fed`: Probe the effect pack's real end in effect_viewer's boundary test.

### `tools/viewers/item_viewer/main.cpp`

- **F153** line 159: `m_currentIndex < GetSize() - 1` is unsigned arithmetic. With an empty pack, GetSize()-1 wraps to 0xFFFFFFFF, so SDLK_RIGHT still increments the index. Later uses are protected by the `< GetSize()` checks. Behaviour preserved.
  - **Fixed** in `d1c3d719`: Stop item and sprite viewers stepping past an empty pack.

### `tools/viewers/sprite_viewer/main.cpp`

- **F154** line 160: Same as item_viewer: with an empty sprite pack, `m_currentIndex < GetSize() - 1` wraps to 0xFFFFFFFF and lets the index increment. Behaviour preserved.
  - **Fixed** in `d1c3d719`: Stop item and sprite viewers stepping past an empty pack.

## MSVC-only warnings

After the cleanup above, the Windows Debug build still reported 1,106
distinct warnings in project code (CI run 36356004472): MSVC `/W3`
diagnostics that GCC and Clang do not raise under `-Wall -Wextra`. They are
cleared the same way, with the same differential compiles (Windows through
MinGW, `_DEBUG`, the debug macros, Emscripten) and nothing suppressed:

- **C4267/C4244** (942 sites), narrowing conversions: a `static_cast` to the
  destination's declared type, which is the conversion MSVC already made.
  A compound assignment becomes `x = static_cast<T>(x + y)`. **C4312**
  (14) goes through `intptr_t`.
- **C4996** (128), CRT deprecations: each call goes through the Windows
  CRT's non-deprecated equivalent, and through the same call as before
  elsewhere. `basic/CrtCompat.h` holds the portable spellings, tested in
  `tests/unit/test_crt_compat.cpp`; nothing defines
  `_CRT_SECURE_NO_WARNINGS`.
- **C4273** (11) and the 15 **LNK4217** link warnings: `__EX` marked
  `dllimport` classes that are compiled into the executable using them. It
  is now empty, as it already was off Windows.
- **C4005** (6), **C4116** (2), **C4477** (2): `DIK_Defines.h` `#undef`s
  the six keys `dinput.h` also defines, `framepack.c` casts the element
  pointer instead of naming a type inside the cast, and `sizeof` prints
  with `%zu`.
- **C4146** (1) was a defect and is fixed; see below.

The Windows budgets fell from 1,254 to 57 distinct warnings in Debug, ASan
and Release (CI run 36360661797). None of the 57 is in project code:
IXWebSocket's C4244/C4267, `third_party/`'s C4996 and the Windows SDK's
C4668.

The deliberate changes:

- **`SendFileInfo::SendBack`** handed the bytes a socket did not take back
  to the file by seeking `-nBack`, a negated `DWORD`, so it seeked about
  4 GB forward. Every later read then failed, and the peer never received
  the rest of the profile file. The file reading moved into `basic`
  (`Basic::FileChunkReader`) and was fixed test-first
  (`tests/unit/test_file_chunk_reader.cpp`). The fix also clears the
  stream, because a short last chunk leaves it failed and a failed stream
  ignores `seekg`.
- **`Basic::LocalTime`** leaves a zeroed `tm` where `localtime` returned
  NULL, which `DebugLog`'s Windows timestamp then dereferenced. It zeroes
  the `tm` itself, because `localtime_s` sets every field to -1 on
  failure.
- **On Windows, the `_s` scanners** fail a `%s` conversion whose word would
  have overflowed its buffer. The old call overflowed it.
- **The two `GetSystem()` bodies** no longer query the version. `Client.cpp`'s
  rejected only Windows 9x or a failed query, neither possible for an x64
  build, so it returns `TRUE`; its one call is inside a comment.
  `CheckSystem::GetSystem()` had every return commented out and returns
  `FALSE`, as it always did.

### Findings and what became of them (MSVC)

Line numbers are those of `5bee7fd9`. Each entry was reported by the agent
that cleared the warning at that site, and the line under it gives the
outcome.

#### `Client/Client.cpp`

- **F155** lines 484, 494, 503: frame-pack `GetSize()` (unsigned short) is stored in the `BYTE` max-action arrays.
  - **Not a defect:** the arrays' only reader, `GetCreatureActionCountMax`, returns `BYTE`, and every consumer stores action counts as `BYTE`, so widening these three arrays would change nothing.
- **C4244** lines 615, 1669: `static_cast<long>` on `DefWindowProc`'s result cleared the warning, but the defect it pointed at was the signature. `WindowProc` and `PatchLogWindowProc` returned `long`, 32 bits on x64, and were registered through a `(WNDPROC)` cast as procedures that return the 64-bit `LRESULT`, so a handle or pointer result (`WM_GETICON`, `WM_GETFONT`, `WM_GETOBJECT`) lost its upper half.
  - **Fixed** after this cleanup: both are `LRESULT CALLBACK` with the `WNDPROC` signature, return `DefWindowProc`'s result unconverted, and are assigned to `lpfnWndProc` without a cast.

#### `Client/MEffectGeneratorTable.cpp`

- **F156** lines 375, 699 (latent): `Step` narrowed to `BYTE` would break the `(x<<8)|y` coordinate encoding `MActionInfoTable.h` describes. Nothing uses it today.
  - **Not a defect:** the `(x<<8)|y` encoding is dead, since `bUseCoord` is never set and `SetCoord`, `GetX` and `GetY` have no callers, so no step value carries coordinates.

#### `Client/MEffectSpriteTypeTable.cpp`

- **F157** line 92: the pair count is saved as one byte, so more than 255 pairs corrupt the table.
  - **Not a defect:** the one-byte count is the file format, and the only writer of `PairFrameIDList` is `LoadFromFile`, which reads that byte, so the list never holds more than 255 entries.

#### `Client/MFakeCreature.cpp`

- **F158** lines 725, 2276 (and the `IsInSector` arguments at 2318-2336): `firstSector.x/y` is the player position plus a negative skip minus 1. It goes negative within about 9-17 tiles of the map's left or top edge and wraps to about 65527 as an unsigned short. `IsInSector` then returns false, and the fake creature or ghost is treated as off-screen.
  - **Fixed** in `bb639298`: Compare fake creatures to the screen rectangle as signed values.

#### `Client/MGuildMarkManager.cpp`, `Client/MLoadingSPKWorkNode.cpp`, `Client/MItemTable.cpp`

- **F159** `MGuildMarkManager.cpp` lines 384, 387, 516, 528 and `MLoadingSPKWorkNode.cpp` line 325: file offsets are held in `long`, which is 32-bit on Windows, so SPK files over 2 GB are out of reach. `MItemTable.cpp` line 233 writes the option-list size as one byte, which truncates past 255.
  - **Fixed** in `51af88a8`: Cap saved default option lists at the one-byte count's 255.

#### `Client/MParabolaEffect.cpp`

- **F160** line 56: `m_RadStep = FPI / (float)steps`, where `steps = (int)m_Len / speed` is 0 when the distance is shorter than the speed. The infinity converted to `int` is undefined (INT_MIN on x64).
  - **Fixed** in `ba3ed450`, `2563dc3e`: Count a parabola shorter than one move as a single step.

#### `Client/MPriceManager.cpp`

- **F161** line 236: `finalPrice * damaged` is computed in `float`, so prices above 2^24 lose precision, and the result is truncated rather than rounded.
  - **Fixed** in `1f1e5bda`, `981d7fee`: Price graded gear with the server's unrounded grade factor.

#### `Client/MSector.cpp`

- **F162** lines 2076, 2093: the sector x/y (unsigned short) is stored in `SECTORSOUND_INFO`'s `unsigned char X/Y`, so coordinates above 255 wrap. That is a limit of the format.
  - **Not a defect:** the one live caller passes a `SECTORSOUND_INFO` that `LoadFromFile` read with one-byte X and Y, so the round trip is lossless and no value above 255 reaches the narrowing.

#### `Client/MZone.cpp`

- **F163** lines 2728, 2729: party HP and MaxHP go from `DWORD` into `WORD` `PARTY_INFO` fields.
  - **Not a defect:** `HP_t` is `WORD` on client and server, and every other writer of these fields carries a `WORD`, so no HP or maximum HP the game produces exceeds 65535.

#### `Client/PackFileManager.h`, `Client/RequestFileManager.cpp`

- **F164** `PackFileManager.h` line 437 and `RequestFileManager.cpp` line 109 (now `basic/FileChunkReader.cpp`): file positions are held in a 32-bit `long`/`DWORD`, so a failed `tellg` (-1) becomes 0xFFFFFFFF bytes left.
  - **Fixed** in `0bf4653b`: Treat a file of unreadable size as unopened in FileChunkReader.

#### `Client/Packet/Cpackets/CGAbsorbSoul.h`

- **F165** lines 79, 82: `getTargetZoneX()`/`Y()` return `Coord_t` (`BYTE`) over `ZoneCoord_t` (`WORD`) members, so zone coordinates above 255 are cut. Nothing calls them today.
  - **Fixed** in `7ba06861`: Return packet fields at the width they have on the wire.

#### `Client/Packet/Gpackets/GCExecuteElement.h`, `GCGuildResponse.h`, `GCActiveGuildList.h`, `GCNoticeEvent.h`, `GCRequestFailed.h`

- **F166** `GCExecuteElement.h` line 40: `getQuestID()` returns `WORD` over a quest ID that is read and written as a `DWORD`, so IDs of 65536 or more are cut.
  - **Fixed** in `7ba06861`: Return packet fields at the width they have on the wire.
- **F167** `GCGuildResponse.h` line 50 returns `BYTE` over a `WORD` wire code; `GCActiveGuildList.h` line 63 returns `BYTE` over a `WORD` count; `GCNoticeEvent.h` line 120 returns `BYTE` over a `WORD` code; `GCRequestFailed.h` line 44 stores a `WORD` in a `BYTE` code. The values fit today.
  - **Fixed** in `7ba06861`, `130fb6f9`: Return the wait-guild list count as a WORD.

#### `Client/Packet/Gpackets/GCMiniGameScores.cpp`

- **F168** line 74 (and 96): `BYTE count = m_Scores.size(); if (count > 10) count = 10;` truncates before clamping, so 256 entries become 0. Its `getPacketSize()` loop (lines 101-104) never advances the iterator, so the size is wrong whenever the names differ in length.
  - **Fixed** in `c99ff05f`: Clamp and size mini game scores the way the server does.

#### `Client/Packet/Gpackets/GCSkillToInventoryOK2.h`, `GCSkillToTileOK3.h`

- **F169** `GCSkillToInventoryOK2.h` line 64, `GCSkillToTileOK3.h` line 66: `getObjectID()` returns `CEffectID_t` (16-bit) from a 32-bit `ObjectID_t` member, and both handlers pass it to `g_pZone->GetCreature()`. A creature ID of 65536 or more is cut to 16 bits, and the lookup fails.
  - **Fixed** in `7ba06861`: Return packet fields at the width they have on the wire.

#### Packet string and count prefixes

- **F170** The length prefix is checked after truncation to `BYTE` (`BYTE sz = str.size(); if (sz > N) throw ...`) in `GCWhisper.cpp` 60/74, `GLIncomingConnectionError.cpp` 64/79, `GLIncomingConnectionOK.cpp` 60, `LCQueryResultCharacterName.cpp` 55, `LCQueryResultPlayerID.cpp` 55, `LCReconnect.cpp` 98, `LGIncomingConnection.cpp` 80/95/110, `CRWhisper.cpp` 135/148/171, `RCCharacterInfo.cpp` 57, `RCPositionInfo.cpp` 61, `RCSay.cpp` 62/74 and `RCStatusHP.cpp` 58. A 257-byte string passes the check as 1 and is written whole behind a 1-byte prefix, which desyncs the stream. `CRConnect.cpp` 72/82, `CRRequest.cpp` 76, `RCRequestedFile.cpp` 70, `LCRegisterPlayerOK.cpp` 36 and `GCWarScheduleList.cpp` 112/118 write the same prefix with no upper bound.
  - **Fixed** in `2feef93c`: Cap Gpacket, Lpacket and Rpacket strings before narrowing.
- **F171** Element counts written as `BYTE` wrap past 255 unchecked: `GCWarList.cpp` 110, `GCWarScheduleList.cpp` 94, `LCServerList.cpp` 78, `LCWorldList.cpp` 78, `CRWhisper.cpp` 161, `RCRequestedFile.cpp` 175.
  - **Fixed** in `1c0cb280`: Cap packet element counts before narrowing them to a byte.

#### `Client/Packet/PCVampireInfo.cpp`

- **F172** line 18: `m_CoatType = flag` narrows a `DWORD` to the `WORD` `ItemType_t`. The commented-out `(flag & 7)` suggests a mask was meant.
  - **Not a defect:** the server's `Vampire::getShapeInfo` now stores the coat's whole `ItemType_t` in the flag, so the narrowing loses nothing, and the old `(flag & 7)` mask would corrupt every coat type above 7.

#### `Client/Packet/SocketImpl.cpp`

- **F173** line 278 (`accept`): a 64-bit Windows `SOCKET` is narrowed to the `uint` `ClientID` and stored back into the `SOCKET` member.
  - **Fixed** in `048a52bb`: Keep the accepted socket handle as a SOCKET.

#### `Client/PacketHandler/GCPartyJoinedHandler.cpp`, `GCPartyPositionHandler.cpp`

- **F174** `GCPartyJoinedHandler.cpp` lines 113-117, `GCPartyPositionHandler.cpp` lines 53-54: a `DWORD` HP goes into a `WORD`, and `WORD` zone coordinates go into `MParty`'s `BYTE` fields.
  - **Not a defect:** `HP_t` is `WORD` on both sides and zones are at most 256 tiles wide and high, so every value fits, and a forged coordinate at worst misplaces that member's party arrow.

#### `Client/UIMessageManager.cpp`

- **F175** line 11778: `setGold(left*10000)`. A donation above 429,496 wraps in the 32-bit `Gold_t`; the value comes from the donation dialog.
  - **Fixed** in `5268c795`, `64f7e251`: Refuse a donation the balance does not cover.

#### `VS_UI/src/KeyAccelerator.cpp`

- **F176** lines 209, 223, 232: `capacity()` is used as the element count, so `SaveToFile` can read past `size()` and write the wrong count. `size()` was meant.
  - **Fixed** in `91d8cacf`: Bound key accelerator lookups and saves by size, not capacity.

#### `VS_UI/src/VS_UI_GameCommon.cpp`

- **F177** lines 24042-24051: the branches check only `size() != 0` before `size()-9`, which wraps for 1-8 entries and divides by zero for 9.
  - **Fixed** in `dc36e444`: Gate team list scroll dragging on the list that is shown.
- **F178** line 30939: `(m_v_war_list.size()-print_list)` wraps with fewer than 10 entries when `m_scroll != 0`.
  - **Not a defect:** `m_scroll` becomes non-zero only while the list has at least 10 entries and the list never shrinks, so with fewer entries the wrapped difference is multiplied by 0.

## Found while fixing

Fixing the entries above turned up these, which no entry listed.

Fixed:

- A player in GM ghost mode whose traced creature vanished crashed in
  `MPlayer::KeepTraceCreature`: its NULL test sat under the ghost gate
  (`5a161e83`).
- The Ousters grade-3 text boxes on rows 1-2 opened the wrong skill's
  description (`8cd6d728`), and every grade-3 skill list read up to three
  entries past its end when the scroll was larger than a shorter list
  (`cead473b`).
- An orbit effect's random start step could be 64, one past its 64-step
  table, and a patrolling turret's random direction could be 32, so it
  twitched between two directions (`f37a6bf7`).
- Seven mouse-move paths called `gpC_Imm->ForceUI()` without the null test
  their siblings make (`991b1601`).
- Graded gear was priced from a grade factor truncated to an integer, where
  the server multiplies in double (`981d7fee`).
- `GCWaitGuildList::getListNum()` returned a `BYTE` over a `WORD` count
  (`130fb6f9`, completing F167).
- `ValueList<T>::toString` did not compile under `__DEBUG_OUTPUT__`
  (`ec9d41bc`).

Not fixed, for whoever takes them up:

- **Shop prices.** Partly fixed by slices 1 and 2 of the shared rules
  library (`docs/RESTRUCTURING.md` task 4.12). `MPriceManager::GetItemPrice`
  and the gear maximum-durability getters now call the server's own rule,
  vendored as `decore` (`05cf98e8`, `8eb744e1`), and the gear getters the
  grade moves read the server's per-class grade and durability table by
  item class (`c87c4d0b`). As a result:
  - the skull's race share is taken before truncating, and the quote is
    floored at 1 last (`7b209c6f`);
  - the head-price bonus pays whole multiples, so 150% pays x1
    (`0e1ef314`);
  - a time-limited item is priced at 50 (`1353c5c6`);
  - a repair is the server's repair price alone (`fc76cc38`);
  - the maximum durability is floored at 1000 before the options and
    not capped at 65000 (`cc8edf02`);
  - ShoulderArmor, Persona, Fascia and Mitten are graded with the grocery
    armor's policy (`5a3a1430`), and CoreZap's grade moves nothing
    (`d63e2c5c`);
  - VampireAmulet's maximum durability is the table's, untouched by the
    grade (`d31bc7c9`);
  - Dermis, Fascia, CarryingReceiver and CoreZap keep no durability, and
    the server's item info for them has none to read (`ItemInfo`'s 1), so
    their maximum is 1 whatever the client's table row holds: they are
    priced in full with no wear, and a repair quotes 0 (`dcd47512`,
    `bd801202`);
  - a couple ring's repair quotes the server's 0, so the gear window's
    repair-all total no longer counts it, or opens for it alone
    (`3f542250`).

  What remains:
  - **Castle tax.** The tax-change percentage is still applied to each
    quote, where the server's buy handler applies the castle tax to the
    total. The market condition may also already carry the tax (slice 5).
  - **The no-durability gear's description.** The item description prints
    the durability of Dermis, Fascia and CarryingReceiver as "1/1", the
    durability and maximum the server keeps for items that wear nothing.
    It hides the durability of VampireAmulet, CoreZap, the couple rings
    and the blood bible sign.
  - **Repair-all charges what it will not repair.** The server's
    `CGRequestRepairHandler::executeAll` adds `getRepairPrice` for every
    worn item without asking `isRepairableItem`, and then repairs only
    the repairable ones. A worn VampireAmulet is charged nearly a tenth
    of its price on every repair-all, since its durability stays 1; a
    damaged unique or time-limited item is charged and not repaired. The
    client's quote for the amulet is that charge, so the first repair-all
    total includes it (see "Durability the client sets itself" below for
    the later ones); a
    time-limited item it quotes 0. A single repair of any of them the
    server refuses, and the gear window does not ask for one. These are
    the server's to change.
  - **Durability options on gear that wears nothing.** The server scales
    the maximum of 1 by durability options as it scales any other, and
    truncates. With options totalling 200% or more, a Dermis, Fascia or
    carrying receiver's maximum is above the 1 it sends: the client
    quotes the repair the server's repair-all charges, but the gear
    window also offers a single repair, which the server refuses
    (`NPC_RESPONSE_REPAIR_FAIL_ITEM_TYPE`). The client quotes a couple
    ring's repair at 0 by class, so a couple ring with such options would
    be charged more in a repair-all than the total the player confirmed.
  - **Pay state and server-only inputs.** The server never sends these, so
    `MPriceHost` answers them with documented defaults: the player pays,
    no item was given away, and there is no Blood Bible potion percentage.
    The client's half price also fires for the NEMA blood bible, which the
    skill table describes as a stat bible (the potion bible there is
    GREGORI), and applies it to all five consumable classes, where the
    server's Blood Bible percentage covers potions and serums only.
  - **Charged items.** A portal's or summon gem's buy and sell quote is
    still the client's own: its charges plus the rate, with no floor at 1
    and no half price or tax.
  - **Unidentified items.** An unidentified item's repair quote is the
    gamble price, and an unidentified time-limited or given-away item gets
    the gamble price before the 50 and 1 rules apply.
  - **Classes with no durability entry.** An item whose class has no
    durability (`MItem`'s -1) quotes 0 to repair; the server charges 1.
  - **Classes the server builds outside `ConcreteItem`.** For the
    motorcycle, the relics, the castle symbols, the blood bibles and the
    sweepers, the server reports a maximum durability of 1. That is a
    placeholder, not a maximum: it stores and sends their durability
    unbounded, and its price never discounts their wear (`> 1`). The two
    couple rings report the same placeholder and send a durability of 1.
    The client shows what it did before. For the motorcycle that is
    `MMotorcycle::GetMaxDurability`, the table's durability scaled by the
    options: no floor, the 65000 cap, and no options at a 0% total. It is
    deliberately not `maxDurabilityBase`. For the couple rings it is the
    ring's rule, an exception kept in `MItem.cpp`, and the description
    hides that value. So the client discounts wear on a motorcycle's and a
    couple ring's buy and sell quote, where the server does not; no sale
    follows, since the server refuses to sell either (`canSell`). A couple
    ring's repair quote is the server's 0 (`3f542250`): its repair-all
    adds `getRepairPrice`, which is 0 at a durability and maximum of 1,
    and it refuses a single repair (`isRepairableItem`). The server
    repairs a motorcycle only through its key (`executeMotorcycle`), at
    its own price. The relic, castle symbol, blood bible and sweeper have
    no maximum on the client (`MItem`'s -1), so they are priced in full
    as on the server. Four classes still keep rules of their own on the
    client rather than the server's table: the two couple rings, the
    motorcycle, and the blood bible sign, which the client makes alone and
    which keeps its old gear rule.
  - **Durability the client sets itself.** For the classes without
    durability, the server's durability stays at the 1 it sends. The
    client sets an item's current durability to its maximum on the NPC
    shop rack (`MNPC.cpp`) and after a repair (`GCNPCResponseHandler`), so a
    vampire amulet in those states is quoted at full price where the
    server charges 1/max of it, and its repair at 0 where the server's
    repair-all goes on charging nearly a tenth of it: every repair-all
    total after the first understates the charge by that much. Dermis,
    Fascia, carrying receiver and CoreZap are unaffected, since their
    maximum is the 1 they already hold.
  - **Negative option totals.** Durability options that sum below -100%
    wrap to a huge unsigned maximum on the server; the client's `int`
    getters receive it as a negative number.
- **Fast-move wobble.** After a fast move, `MCreature::AffectMoveBuffer` can
  still take one step in the previous direction before the next buffered move
  corrects the position; a stale `m_NextDirection` from the jump branch can
  add one step after a fast-move jump. Both are what is left where F012
  removed an out-of-bounds read.
- **Fake creatures at the map edge.** `MFakeCreature`'s on-screen rectangle
  is centred on the player rather than taken from
  `g_pTopView->GetFirstSector()`, so at the map's right and bottom edges it
  still differs from the real screen.
- **Darkness in zone 3001.** The item renderer draws items on dark tiles to
  everyone there, and the click picker applies a player-position rule of its
  own. Neither follows the item darkness rule tracing now uses (F028).
- **Packet copies that disagree with the server's.** These run only in test
  and golden writes. About a hundred more GC and shared-info writers narrow a
  string length without a cap (`ServerGroupInfo` and `WorldInfo` among
  them). `GCShowUnionInfo::write` narrows its guild count without a cap, as
  the server's copy does. `GCWarScheduleList::getPacketMaxSize` budgets
  `szBYTE*16` per guild name where the server budgets `szBYTE+16`.
  `GCWarList::getPacketMaxSize` budgets `(RaceWarInfo + GuildWarInfo) * 12`
  where the server budgets the widest war times `kMaxWars`.
  `GuildInfo::getSize` leaves out the expire date, which the server counts.
- **The effect viewer's boundary.** A unit test of
  `EffectResourceContainer::GetEffectFrame`'s `>=` check would need the
  container in a library a test binary links.
