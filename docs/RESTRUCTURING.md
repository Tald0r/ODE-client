# Client Restructuring Plan

**Original plan complete as of 2026-09-09.** Its tasks are `done` and have their owners;
PRs #144 and #145 carry the last slices. What the plan leaves behind is the
machinery, not a to-do list: the membership files, the include checker, the
ratchets and the fix policy (task 3.1) are what keep the end state
true from here, and *What the review rounds settled* is what a later slice
of any kind should read first. 5.2's *Candidates for a next slice* - dead
code the plan found and did not take unasked - was the last list open and
closed on 2026-09-16 with the eleventh slice; what still shrinks is the
exemption list, whenever a task extracts a seam. Phase 4 records the completed
game-model extractions, including the rank-bonus table. The follow-up is reconciled in
[the completion evidence](code-health-reconciliation-2026-09-23.md), with current
regression owners and explicit asset/runtime limits.

Living, trackable plan for moving the OpenDarkEden client's game code out of the
`DarkEden` executable and into testable static libraries, while the outstanding
findings of `docs/code-health-review-2026-08-29.md` keep getting fixed. The end
state: **a fix that touches library code ships with a unit test in the same
commit; code that cannot be tested is a shrinking, named exception rather than
the default.**

This mirrors the server repo's `docs/RESTRUCTURING.md` (same parent directory,
`../server`), which has already executed the exact refactoring this client
needs on its half of the shared codebase: `execute()` stripped off the packet
classes, a dispatch table at the composition root, the whole wire layer
compiled once into a standalone `de-kernel` library, membership held by a file
and enforced by an include-graph checker, and legacy debt tracked by
shrink-only ratchets. Where a task here has a proven server twin, the task
cites it — read the server's status notes before starting, because its
adversarial reviews recorded the traps.

## How to use this document

- Every task has a checkbox and a `> **Status:**` line. Update the status line
  **in the same commit** as the work it describes. Allowed values:
  `not started` | `in progress (<what remains>)` | `done (<commit>)` |
  `dropped (<why>)`.
- A status line records the **current** state and what the next reader
  needs to act: the owner, the conventions the reviews settled, the open
  bugs, what remains. The per-change narrative — what moved, what the
  review round found, the test list, the suite count, the ratchet delta —
  belongs in the PR description and the commit message, not here. PRs #33
  through #77, then #142 to #145, and their commits hold that history for
  everything below.
- A task is only `done` when its **Owner** exists — the test or mechanism that
  keeps the rule true from then on. Landing the change without the owner is
  `in progress (owner missing)`.
- Ratchet numbers only go **down**. Re-measure with the given command before
  and after a change that claims progress; commit the updated number with the
  change. A recorded growth (a split that adds an executable translation
  unit) moves the baseline with the reason written beside it.
- Substantial tasks get an **adversarial review** before merge (two
  reviewers; standing practice in this repo — every round so far has found
  real defects in the change itself).
- Verification for every extraction: both Debug trees build
  (`build/vs2022` with /RTC1, `build/vs2022-asan` with ASan), the test suite
  is green in a plain tree **and** an ASan tree, `wire_inventory_fresh`
  passes, and the applicable CI checks pass. **Live-server verification is
  optional, not a merge or completion gate** (user instruction, 2026-09-17).
  Do not wait for a runtime report or request one as approval to merge.
  Do not launch `DarkEden.exe` yourself unless the user asks.

### Working agreements for refactoring agents

- **New library targets use explicit source lists, never `file(GLOB)`.** The
  existing globs don't use `CONFIGURE_DEPENDS`, so a moved file silently stays
  in the old target until someone reconfigures; explicit lists make membership
  reviewable and are what the ratchet script parses. After moving files,
  reconfigure both trees.
- **Moving a file between targets must not change its bytes** in the same
  commit when it has no seam to cut. Move first, prove the build, then fix —
  separate commits, so the diff that changes behavior is readable. A file
  that only links once its reaches go through a host moves and cuts in one
  commit (the Phase 4 pattern), and that commit names every change that is
  not the move.
- Sources are CRLF; use the Edit tool, not `sed -i`/`awk`, to modify them.
- Wire layout is pinned: any change under `Client/Packet` that touches
  `read()`/`write()`/`getPacketMaxSize()` must keep
  `tests/wire-layout.txt` in sync (regenerate via
  `tests/tools/gen_wire_inventory.pl`, re-record with `UPDATE_GOLDENS=1`) and
  must be diffed against the server with
  `server/tests/tools/wire_inventory_diff.sh`. A layout change is a protocol
  change and needs the identical change in the server repo.

### What the review rounds settled

Each of these cost at least one review finding; they are the rules the
per-task status lines no longer restate.

- **Measure the value, not the spelling.** A checker that matches
  `array[pPacket->getSlotID()]` misses the hundred sites that copy the
  value into a local first; one that hardcodes a receiver name misses the
  handlers that call it something else. `check_packet_indices.pl` walks
  the value; R7 and R8 exist as a pair because R7 counts a spelling.
- **A ratchet at zero is a claim about the ratchet.** R7 reached 0 and was
  read as finding C19 closed while 24 live sites used an entry copied into
  a static array first. Anything stronger than "the pattern sees nothing"
  needs a different instrument, and a new instrument gets the same
  adversarial reading as the code — it will not get it from the person who
  just built it (R8 was recorded at 13, and the population was 43).
- **A checker pins its own denominator.** `check_format_arity.pl` passed
  three times while checking nothing (a Perl list assignment, `find(1)`
  under PowerShell, a `//` inside a string literal). Both the sites it
  finds and the sites it resolves are floors now; every way a scan can
  shrink must fail rather than report a smaller number and exit 0.
- **A search that finds nothing is a fact about the search.** A grep that
  excludes the name it is searching for (`grep RenderText | grep -v
  TextService`, when every call is `TextService::RenderText`); a per-file
  grep over CP949 sources, which grep classifies as binary and stops
  reading silently; a grep for `#define __USE_ENCRYPTER__` that missed the
  define in `Encrypter.h` while the golden tests already said the branch
  was live. Read what the tests say before declaring code dead.
- **A link proof takes the address of non-virtual members.** A pointer to
  a virtual member is a vtable index and need not reference the defining
  object, so such a proof links cleanly with the file removed from the
  library. Verify a link proof by taking the file back out.
- **Scripted rewrites bound their patterns and are followed by a comment
  sweep.** A non-greedy `.*?` under `/s` swallowed 87 lines of `GameUI.cpp`
  into one argument; the ratchets could not have caught it, because a
  corrupted file has the right number of sites. After a scripted removal,
  sweep for orphaned banners and `//#include` lines.
- **Uninitialised members are reproduced, not guarded.** `/RTC1` fills
  stack locals with `0xCC` and the CRT fills heap with `0xCD`, so a test
  that reads a fresh object fails deterministically; but neither `/RTC1`
  nor ASan flags an uninitialised *member*, so the test reads the fill
  pattern rather than waiting for a crash. Every `gamemodel` slice found
  at least one constructor that set every field but a few.
- **A host entry answers without a host with the value a missing config
  would give, never zero.** A test binary, or a host whose entries are
  NULL, gets the `ClientConfig` constructor's defaults; a missing clock
  means no delay; a missing player means a skipped refresh rather than a
  crash — recorded as a behaviour delta where the old code dereferenced
  unguarded. Host readers are private statics on the class (`MItem::Clock()`),
  guard the function pointer, and are re-read on every call, never cached.
- **A host installer is designated-initialised.** When `WireHost` reached
  24 entries, fourteen shared a signature with another, and the
  executable that installs them is never linked into a test, so a
  positional slip compiled and passed the whole suite; the fourth slice
  recorded the order as "checked by reading". C++20 designators make an
  entry in the wrong slot, or one that does not exist, a compile error.
  What they cannot check is the body an entry points at.
- **A reference on a live line can still be dead.** The whisper seam was
  described as live behaviour because the code that reaches it is
  compiled; every path *to* that code sits behind `0 &&`. Before
  describing what a seam does, find the caller that runs.
- **A class split across a library and the executable** costs nothing when
  the class has no virtuals (`MSkillSet`, `TextService`) or when the
  library constructs none of the split classes (`MBomb`, `MHolyWater`), and
  is otherwise a vtable referencing symbols a test binary cannot link, so
  the class moves whole.
- **Every fix commit names its `Test path:`** (`lib + test`, `moved, then
  fixed`, `exempt`); a fix that could not be reproduced is a *regression
  guard* in its message, not a reproduction. `tools/git-hooks/commit-msg`
  refuses a `fix:` without one.
- **Two ratchets on the same finding see different things.** R3 counts
  `sprintf` lines but `\b` rejects the `w` in `wsprintf`; R7 counts both.
  R4 greps `g_p*` and cannot see a library file calling an executable-side
  *function* (which is what the link proofs are for) or a global under
  another name (`g_Mode`). Read each ratchet's comment for what it cannot
  see before quoting its number.

## Goals / non-goals

**Goals**

1. **Every new fix is unit-tested.** The code a fix touches gets moved into
   (or already lives in) a static library linked by `unit_tests`, and the fix
   is written test-first. Exceptions are named in the exemption list below,
   not decided ad hoc.
2. **The wire layer becomes a standalone library** (`packetwire`): streams,
   encrypter, info classes, and eventually all packet classes — so the #1
   open risk (unvalidated server input in `Client/Packet/Gpackets/`,
   code-health review priority 1) becomes directly testable with hostile
   inputs instead of only greppable.
3. **Game-model logic leaves the executable** piecewise (tables, inventories,
   price/trade logic), cutting `g_p*` global seams as it goes.
4. Architecture rules owned by tests (membership files, include-graph
   checker, ratchets), never by memory.

**Non-goals**

- No behavior change on the wire or in game rules unless a task says so
  explicitly. Layout stays pinned by the wire inventory + the server diff.
- No rewrite of the render/game loop (`GameMain.cpp`, `MZone` drawing,
  `GameUI.cpp`) or the VS_UI widget tree. These stay executable-or-UI-side
  and are covered by builds and available regression checks; runtime
  verification is optional.
- No new test framework. `tests/framework/test_framework.h` stays; tests are
  files in `tests/unit/` (globbed — reconfigure after adding one).
- Not a port of the server's kernel wholesale: the two repos stay separate
  codebases pinned to one wire contract by the shared inventory, and to one
  rule implementation by the vendored de-core subset (task 4.12).

## Exemption list (tangled, outside the unit-test libraries)

Code on this list is fixed executable-side with build verification, available
automated checks and a **regression guard** note in the commit message when
the defect was not reproduced. Live-server verification is optional.
Shrink it when a task extracts a seam, and record the removal here.
`MEventManager.cpp`'s background-image loading and host adapter left the
list with task 4.224, which retired the never-drawn images and moved the
host into `GameInitInfo`.
`MGuildMarkManager::LoadGuildMark`'s integration left it with task 4.230:
the manager compiles in VS_UI, where `ui_tests` runs it on real packs and
a real guild info mapper.
`ModifyStatusManager.cpp`'s `Function_MODIFY_*` bodies left it with task
4.235: they compile in gamemodel, where `test_modify_status.cpp` runs each
one through hosts that record the UI, the player and the zone.
The executable halves of split classes left it with task 4.237, whose
last, `MItemUse.cpp`, compiles in gamemodel, where `test_item_use.cpp`
runs each item's use.
`UIDialog.cpp`'s popups left it with task 4.238: they compile in gamemodel,
and `test_ui_dialog.cpp` runs them over windows that record.
`MCreature`'s movement and attached effects left it with task 4.242: they
compile in gamemodel, and `test_creature.cpp` runs them over hosts that
record.

| Code | Why exempt |
|---|---|
| `GameMain.cpp`, `GameInit.cpp`, `GameUI.cpp`, `Client.cpp`, `SDLMain.cpp` | process lifecycle, DLL whitelist, render loop and quest UI events; also where the hosts (`MItemHost`, `MPriceHost`, `WireHost`) are installed, which no test can prove - `WireHost`'s installer is designated-initialised since 2026-09-09, so a wrong slot there is a compile error, but a wrong *body* still is not |
| `MZone` live sector allocation/application, rendering and visual-effect ownership | map parsing is tested in `ZoneMapData`; application and effects reach live sectors, creature status, sprite tables and `MTopView`; drawing uses live surfaces. Viewer tools cover some drawing; ownership guards use full builds and source-path audits. (`TileRenderer`'s draw paths left this row with task 4.213: SpriteLib's `test_tile_renderer.cpp` covers them.) |
| `MTopView` draw calls and the per-frame state that only the draw calls read (`DrawItemBroken` and the other draw branches) | the view draws on the live surface and reads the frame clock (`g_CurrentFrame`, `g_bFrameChanged`), `g_pEventManager` and the player's gear globals; it is the render loop the goals keep executable-side. Rules that can be stated without those globals move into a library first and get a test. Fade direction, stepping, delay and stopping now belong to `gamemodel`'s `MScreenFade` (task 4.18), with the 31 to -1 transition tested under both char signednesses; `MTopView` keeps suppression, colour conversion and surface calls. Fixes to the draw calls use full builds; the MinGW and Emscripten compiles some fix commits cite are a local check that is not in the tree, and no CI job repeats them. |
| `MPlayer`, `MFakeCreature` movement and attached-effect orchestration; `PacketFunction::ExecuteActionInfoFromMainNode` | virtual character classes reach the live zone, UI, sprite tables and effect generators; action results transfer to `MEffectTarget` and execute through the same game objects. Bounds/queue/ownership guards stay here; extracting those classes would require the render/game-loop rewrite excluded above. Review regression guards use full builds and existing automated checks, without a runtime gate. |
| `VS_UI/src/**` rendering and dialogs that still reach game globals | these paths use full builds and available automated checks; live verification is optional. `ui_tests` now links the real Button, EventButton, SkinManager, LineEditor, LineEditorVisual state/focus methods, InputFocusManager and the UI result receiver, so those independently reachable components require test-first fixes. `LineEditorVisual::Show` remains separate because it reaches the game's renderer. |
| `Client/PacketHandler/*Handler.cpp` bodies, except the handlers `tests/arch/gamemodel_files.txt` lists | mutate `g_pZone`/creature state; the *parsers* they consume are in `packetwire` and testable, the mutations are not. The reason does not hold for a handler whose body reaches only model state: such a handler compiles in `gamemodel`, a test runs it on a real packet, and a fix to it is test-first (lib + test). Amended 2026-09-30 (task 4.15), when eight handlers moved in: the four phone-slot handlers (`g_pUserInformation`), the trade money and offer handlers (`g_pTradeManager`, `g_pMoneyManager`), `GCSystemAvailabilities` (`g_pSystemAvailableManager`) and `GCMonsterKillQuestInfo` (`g_pQuestInfoManager`, `g_pCreatureTable`). The row is keyed to the membership file so that a later handler move needs no further edit here. The rest of `Client/` is presumed movable until a task proves otherwise, and code has left this list by moving before: `RequestClientPlayerManager.cpp` from the connect-paths row (task 5.1). The login world/server list rebuilds and their complete handlers now run in `gamemodel` (tasks 4.19 and 4.84); designated hosts retain UI refreshes and mode transitions in the executable. |
| `UIMessageManager::Execute_UI_CHAT_RETURN` | application chat callback reaches the current game mode, player, party/guild state, live socket, help events and dialogs. Its payload borrowing is source-audited with full builds; the queue that owns deferred text is independently tested in `ui_tests`. |
| `Client/MinTr.h` raw trace transport | no caller remains: the last one, MCreatureWear's, logs through DebugLog since task 4.243. The header is still included (DebugInfo.h, MZone.cpp) and can be retired with them. Unused variadic and command formatting paths are retired. |
| `PacketFunction.cpp` connect paths | Winsock + connection state machine. `RequestClientPlayerManager.cpp` was listed here until 2026-09-09; task 5.1's fifth slice put its seams behind `WireHost`, and task 5.2's eighth slice deleted it with the rest of the outbound peer side |
| `PacketFunction.cpp` error popups and addon item tables (`PopupErrorMessage`, `InitPacketItemTable`) | the popups reach the live UI and message tables, and the tables store item templates the executable builds at start-up (`g_pPacketItemShoulder` and its siblings); neither has a harness. Precedent `0557a039`; also `10b1da62` and `6ebdc97c`. Added 2026-09-29. The connect paths in the row above are a separate case. |
| `UIMessageManager.cpp` handlers other than `Execute_UI_CHAT_RETURN` (the other `Execute_UI_*` bodies) | each reaches the live storages, inventory, `g_pTempInformation`, the player and the socket, like the chat handler above; the class has no harness. Precedent `ace28baf`; also `dfaeda5b`. Added 2026-09-29. |
| `DebugKit.cpp` (`CDebugKit`) | a debug facility compiled into the executable and reached only under `DEBUG_INFO`, which no build defines; moving it into `gamemodel` would change the W2/M2 include rules. Precedent `6a46b721`. Added 2026-09-29. |
| `tools/viewers/**` | developer tools outside the libraries and the game: a fix is checked by building the viewer and, where it has one, running its self-test (`d1c3d719`, `d54f7fed`). Added 2026-09-29. |

`UserOption` persistence is compiled once in `VS_UI/src/UserOption.cpp`.
`user_option_tests` links that library object and exercises the real settings
reader/writer together with `KeyAccelerator`; persistence fixes are test-first.

`SlayerPortalData` parses the portal resource independently of the dialog.
`test_portal_data.cpp` links the production reader and covers truncation, field
and count bounds, little-endian unaligned input and atomic replacement. The
dialog's sprite coordinates, zone filtering and navigation remain UI exemptions.

Everything else under `Client/*.cpp` and `Client/Packet/**` is presumed
movable until a task proves otherwise and adds it here with a reason.

## Ratchets (shrink-only)

Compiler warnings also have a shared target policy and a clean-build budget
checker in `tools/ci/check-warnings.pl` ([operation](../tools/ci/warnings.md)).
The generated target-option inventory and parser fixtures own the mechanism;
measured budgets cover all ten CI preset/architecture combinations. Every
increase fails, and a decrease requires tightening the committed budget.

`tests/ratchet/ratchets.sh` (ctest `ratchets`) holds the baselines inline
and fails the suite when a count **rises** or drops unrecorded, so
tightening lands in the same commit as the progress. Measurements are
grep-only by design; nothing is generated or overwritten. The script's
comment on each ratchet says what it cannot see and keeps the history of
every baseline move; the table below is the current reading.

| # | Metric | Now | What it counts, and does not |
|---|--------|---:|---|
| R1 | Application translation units compiled directly into the `DarkEden` target | **9** | `ClCompile` entries in `DarkEden.vcxproj`, excluding CMake's generated `CMakeFiles/DarkEden.dir/cmake_pch.cxx`, read from the ctest run's own build dir; SKIP (never PASS) without a supported generated project. Both the Visual Studio configure stamp and Ninja's `build.ninja` must be newer than CMakeLists.txt and the three library membership files (packetwire's, gamemodel's and gameview's). Ninja's baseline is **8**, since the non-Windows source list is one file shorter; Linux/macOS CI verifies that count. Baseline 1,044 on 2026-09-01; 484 after task 5.2's eighth slice. **482 on 2026-09-17:** the earlier deletion of `md5.cpp` had left the recorded 484 one too high (483 measured before this extraction); task 4.5 moves `RankBonusTable.cpp` into `gamemodel` (483 → 482). **478 later that day:** delete the four unused Direct3D texture/shadow cache translation units; their object declarations were commented out and the active sprite path never constructed them. **477 later still:** remove the unused Windows-only WinINet downloader; Ninja remains 475. It counts what still cannot be unit-tested. Recorded growths, each the executable side of a split: `PacketHandlerRegistry.cpp`, `GCExchangeBuyHandler.cpp`, `MItemUse.cpp`, `MObjectScreen.cpp`, `MSkillAvailable.cpp`, `TextServiceScreen.cpp`. **473 Windows / 471 Ninja later that day:** six map-record/header translation units move to `gamemodel`, while two new executable files retain screen geometry and live interaction actions; `ShowTimeChecker`'s pure constructor and I/O move to `ShowTimeData.cpp`. A further executable unit moved to `basic` with `CMessageArray` (2026-09-18). **470 Windows / 468 Ninja on 2026-09-21:** `UserOption.cpp` moves unchanged into `VS_UI`; its test links the same object as the game. **469 Windows / 467 Ninja:** the corrected `SXml.cpp` moves unchanged into `VS_UI`, replacing the older duplicate and allowing XML tests to link the production implementation. **468 Windows / 466 Ninja:** the help-message loader moves unchanged into `VS_UI` for real-library parser tests. **467 on 2026-09-21:** `CToken.cpp` joins `gamemodel` byte-identically so token and reset lifetimes can be tested. **466 Windows / 464 Ninja:** the empty handwritten `Client_PCH.cpp` is retired when CMake starts producing the private PCH. Generated PCH producers are excluded from both inventories; ordinary application `.cxx` and `.cc` files remain counted.  **465 Windows / 463 Ninja on 2026-09-24:** the inactive Client/DebugInfo.cpp implementation is removed.  **460 Windows / 458 Ninja on 2026-09-24:** five unchanged world metadata implementations join gamemodel. **456 Windows / 454 Ninja:** quest metadata joins gamemodel; profile and shrine file adapters join VS_UI. **455 Windows / 453 Ninja:** party membership joins gamemodel behind its live-creature host. **456 Windows / 454 Ninja:** the generated English NPC name table (`MNPCTableEnglish.cpp`) is a recorded growth; it writes an executable global. **454 Windows / 452 Ninja on 2026-09-28:** the NPC talk boxes (`TalkBox.cpp`, `MStringList.cpp`) join gamemodel so the menu-answer mapping can be tested. **453 Windows / 451 Ninja later that day:** `MStatusManager.cpp` joins gamemodel so the character-select stat preview can be tested (task 4.12, slice 3). **449 Windows / 447 Ninja on 2026-09-29:** the action and effect tables (`MActionInfoTable.cpp`, `MEffectSpriteTypeTable.cpp`, `MEffectStatusTable.cpp`, `MCreatureSpriteTable.cpp`) join gamemodel so their loaders can be tested (task 4.1); the move changed no loader, and later commits fixed the loaders test-first. **447 Windows / 445 Ninja on 2026-09-29:** the chat filter (`MStringMap.cpp`, `MChatManager.cpp`) joins gamemodel, its one option read behind `MChatHost`, so the curse filter can be tested (task 4.13). **445 Windows / 443 Ninja on 2026-09-29, after merging both:** the creature status array (`MStatus.cpp`) and the request/answer mode register (`TempInformation.cpp`) join gamemodel unchanged so they can be tested (task 4.14); each branch removed two from 449/447, and the merged tree was measured, not added up. (The merge had left this entry's bold marker doubled and its first clause repeated; tidied 2026-09-30.) **437 Windows / 435 Ninja on 2026-09-30:** the eight packet handlers that reach only model state (`GCPhoneConnectedHandler.cpp`, `GCPhoneDisconnectedHandler.cpp`, `GCPhoneSayHandler.cpp`, `GCRingHandler.cpp`, `GCTradeMoneyHandler.cpp`, `GCTradeRemoveItemHandler.cpp`, `GCSystemAvailabilitiesHandler.cpp`, `GCMonsterKillQuestInfoHandler.cpp`) join gamemodel from `Client/PacketHandler` so a test can run them on real packets (task 4.15). **436 Windows / 434 Ninja on 2026-10-02:** `CServerInformation.cpp` joins `gamemodel` unchanged so login world/server selection can be tested (task 4.19). **434 Windows / 432 Ninja later that day:** `MFunctionManager.cpp` and `MRequestMode.cpp` join `gamemodel` unchanged so callback dispatch and player/view request state can be tested (task 4.20). **433 Windows / 431 Ninja later that day:** `MathTable.cpp` joins `gamemodel` unchanged so effect trigonometry and steering can be tested (task 4.21). **432 Windows / 430 Ninja later that day:** `ServerInfoFileParser.cpp` joins `gamemodel` after removing its unused `MinTr.h` include, so dimension-specific login and reconnect configuration can be tested (task 4.22). **431 Windows / 429 Ninja later that day:** `MWeather.cpp` joins `gamemodel` behind player-origin and viewport callbacks (task 4.23). **428 Windows / 426 Ninja later that day:** `MJusticeAttackManager.cpp` and its add/remove packet handlers join `gamemodel` after dropping unobserved timestamps (task 4.25). **427 Windows / 425 Ninja later that day:** `PCConfigTable.cpp` joins `gamemodel` unchanged so saved character slots and per-world account settings can be tested (task 4.26). **425 Windows / 423 Ninja later that day:** `MonsterNameTable.cpp` and `MLevelNameTable.cpp` join `gamemodel` unchanged, with the title/hallucination selection state extracted from `MCreature` (task 4.27). **424 Windows / 422 Ninja later that day:** `MWarManager.cpp` joins `gamemodel` with live zone and UI actions behind `MWarHost` (task 4.28). **423 Windows / 421 Ninja later that day:** `MShopTemplate.cpp` joins `gamemodel` unchanged, with the NPC shelf-building rules extracted behind `NPCShopHost` (task 4.29). **421 Windows / 419 Ninja later that day:** NPC dialogue (`MNPCScriptTable.cpp`) and its English overlay join `gamemodel` unchanged after a separate unused-include cleanup (task 4.30). **420 Windows / 418 Ninja later that day:** effect-target state (`MEffectTarget.cpp`) joins `gamemodel` behind its player-roster removal host, alongside the extracted deferred-result queue (task 4.31). **419 later still:** portal records join `gamemodel` (task 4.32; Ninja 417). **418 later still:** music metadata joins `gamemodel` with track selection (task 4.33; Ninja 416). **417 later still:** delayed sound records join `gamemodel` with queue scheduling (task 4.34; Ninja 415). **416 later still:** show-time scheduling joins `gamemodel` (task 4.36; Ninja 414). **415 later still:** the help-string table joins `gamemodel` (task 4.37; Ninja 413). **414 later still:** interaction-object metadata joins `gamemodel` (task 4.38; Ninja 412). **413 Windows / 411 Ninja:** base effect state joins `gamemodel` behind frame/light services (task 4.44). **411 Windows / 409 Ninja:** moving and screen effects join `gamemodel` (task 4.45). **410 Windows / 408 Ninja:** randomized draw-skipping effects join `gamemodel` (task 4.46). **409 Windows / 407 Ninja:** complete linear effects join `gamemodel` (task 4.48). **407 Windows / 405 Ninja:** creature-guided and chase effects join `gamemodel` (task 4.49). **406 Windows / 404 Ninja:** complete homing effects join `gamemodel` (task 4.50). **405 Windows / 403 Ninja:** complete parabolic effects join `gamemodel` (task 4.51). **404 Windows / 402 Ninja:** attached effects join `gamemodel` (task 4.52). **403 Windows / 401 Ninja:** complete orbiting attachments join `gamemodel` (task 4.53). **401 Windows / 399 Ninja:** effect ownership and screen-effect managers join `gamemodel` (task 4.54). **400 Windows / 398 Ninja:** inventory screen-effect generation joins `gamemodel` (task 4.55). **399 Windows / 397 Ninja:** falling projectile generation joins `gamemodel` (task 4.56). **398 Windows / 396 Ninja:** rising projectile generation joins `gamemodel` (task 4.57). **397 Windows / 395 Ninja:** multi-projectile falling generation joins `gamemodel` (task 4.58). **396 Windows / 394 Ninja:** zone-attack generation joins `gamemodel` (task 4.59). **394 Windows / 392 Ninja:** parabolic zone and bomb generation join `gamemodel` (task 4.60). **393 Windows / 391 Ninja:** stationary zone generation joins `gamemodel` (task 4.61). **392 Windows / 390 Ninja:** cross-shaped zone generation joins `gamemodel` (task 4.62). **390 Windows / 388 Ninja:** fixed X and rhombus generation joins `gamemodel` (task 4.63). **389 Windows / 387 Ninja:** empty-cross generation joins `gamemodel` (task 4.64). **387 Windows / 385 Ninja:** horizontal and vertical empty-wall generation joins `gamemodel` (task 4.65). **386 Windows / 384 Ninja:** empty-rectangle generation joins `gamemodel` (task 4.66). **385 Windows / 383 Ninja:** full-rectangle generation joins `gamemodel` (task 4.67). **384 Windows / 382 Ninja:** complete wall generation joins `gamemodel` (task 4.68). **383 Windows / 381 Ninja:** multiple stationary generation joins `gamemodel` (task 4.69). **382 Windows / 380 Ninja:** random-zone generation joins `gamemodel` (task 4.70). **381 Windows / 379 Ninja:** selectable stationary generation joins `gamemodel` (task 4.71). **380 Windows / 378 Ninja:** spread-out generation joins `gamemodel` (task 4.72). **379 Windows / 377 Ninja:** around-zone generation joins `gamemodel` (task 4.73). **378 Windows / 376 Ninja:** follow-path generation joins `gamemodel` (task 4.74). **377 Windows / 375 Ninja:** meteor-drop generation joins `gamemodel` (task 4.75). **376 Windows / 374 Ninja:** creature parabola generation joins `gamemodel` (task 4.76). **375 Windows / 373 Ninja:** wide ripple generation joins `gamemodel` (task 4.77). **374 Windows / 372 Ninja:** Bloody Breaker generation joins `gamemodel` (task 4.78). **373 Windows / 371 Ninja:** Bloody Wall generation joins `gamemodel` (task 4.79).  **365 Windows / 363 Ninja (tasks 4.80-4.82):** eight further sources join gamemodel; Ninja is measured from a fresh Linux configure. Both counts were subsequently verified by Windows and Linux CI at `68b1e2c7`. **346 Windows / 344 Ninja (tasks 4.83-4.87):** twelve more live sources join tested libraries and seven unused or empty sources are retired. Windows and Ninja counts are verified by fresh CI-generated targets; Debug, Release and sanitizer checks pass. **342 Windows / 340 Ninja (tasks 4.88-4.90):** the regen-zone, skip-draw and skill-downgrade slices. **329 Windows / 327 Ninja (task 4.91):** thirteen handlers that ignore their packet join gamemodel unchanged. **320 Windows / 318 Ninja (task 4.92):** nine guild and union reply handlers join gamemodel behind `GuildPresentation::Host`. **316 Windows / 314 Ninja (task 4.93):** friend, SMS address-book and whisper-failure reply handlers join gamemodel behind `CommunicationPresentation::Host`. **311 Windows / 309 Ninja (task 4.94):** quest and war status reply handlers join gamemodel behind `QuestWarPresentation::Host`. **308 Windows / 306 Ninja (task 4.95):** trade, crafting and item-mall reply handlers join gamemodel behind `TradePresentation::Host`. **305 Windows / 303 Ninja (task 4.96):** three sources whose whole body is behind a macro no build defines are retired. **301 Windows / 299 Ninja (task 4.97):** NPC map, skill teaching, mini-game and prize-notice reply handlers join gamemodel behind `GameInfoPresentation::Host`. **292 Windows / 290 Ninja (task 4.98):** nine message-box and item-release reply handlers join gamemodel behind `DialogPresentation::Host`. **285 Windows / 283 Ninja (task 4.99):** seven login-server character, ID and version reply handlers join gamemodel behind `LoginPresentation::Host`. **269 Windows / 267 Ninja (task 4.100):** sixteen replies about the client's own player join gamemodel behind `PlayerState::Host`. **257 Windows / 255 Ninja (task 4.101):** twelve replies about the zone and its creatures join gamemodel behind `ZoneState::Host` and `CreatureState::Host`. **250 Windows / 248 Ninja (task 4.102):** seven replies handed on whole to the world and connections join gamemodel behind `WorldEvents::Host`. **249 Windows / 247 Ninja (task 4.103):** `MItem::NewItem` joins gamemodel over the factory the executable installs, and the trade box's new-item reply with it. **244 Windows / 242 Ninja (task 4.104):** guild-list, guild-membership and character-error replies join gamemodel over the guild, player and login hosts. **238 Windows / 236 Ninja (task 4.105):** the client mode and the message logs' pointers join gamemodel, and six replies that read them. **232 Windows / 230 Ninja (task 4.106):** six skill and damage results on zone creatures and the player join gamemodel over the creature service. **228 Windows / 226 Ninja (task 4.107):** the personal-store and nickname replies join gamemodel over the UI object's host. **226 Windows / 224 Ninja (task 4.108):** the wall see-through test and the peer disconnect request join gamemodel. **225 Windows / 223 Ninja (task 4.109):** the peer file-send queue joins gamemodel. **224 Windows / 222 Ninja (task 4.110):** the help display joins gamemodel over the frame clock, whose definition it now owns. **223 Windows / 221 Ninja (task 4.111):** the installed-mine reply joins gamemodel over the world-event and item hosts. **221 Windows / 219 Ninja (task 4.112):** the login and registration error replies join gamemodel, with the update flag the client mode now owns. **220 Windows / 218 Ninja (task 4.113):** the whisper reply joins gamemodel over the communication and player hosts. **219 Windows / 217 Ninja (task 4.114):** the kick-warning reply joins gamemodel over the world-event host's new event entry and the player host. **218 Windows / 216 Ninja (task 4.115):** the made-item reply joins gamemodel over the dialog and player hosts. **217 Windows / 215 Ninja (task 4.116):** the pet-stash reply joins gamemodel over two new dialog entries. **216 Windows / 214 Ninja (task 4.117):** the system-message reply joins gamemodel over the communication host's option queries. **214 Windows / 212 Ninja (task 4.118):** the refused rank bonus and the guild-union offers join gamemodel over the dialog and UI-object hosts. **212 Windows / 210 Ninja (task 4.119):** the quest element and quest item replies join gamemodel over the zone and UI-object hosts. **211 Windows / 209 Ninja (task 4.120):** the quest-status reply joins gamemodel over the UI-object host. **210 Windows / 208 Ninja (task 4.121):** the party-leave reply joins gamemodel over the zone, player and communication hosts. **209 Windows / 207 Ninja (task 4.122):** the shop-sold reply joins gamemodel over the zone and creature hosts. **208 Windows / 206 Ninja (task 4.123):** the party-error reply joins gamemodel over the player, dialog and communication hosts. **207 Windows / 205 Ninja (task 4.124):** the stash reply joins gamemodel over the dialog host and the gun loader. **206 Windows / 204 Ninja (task 4.125):** the nickname-verify reply joins gamemodel over the communication and UI-object hosts. **203 Windows / 201 Ninja (task 4.126):** the zone flags move to gamemodel, and the war-list and party-position replies join it over the zone, player and world-event hosts. **201 Windows / 199 Ninja (task 4.127):** the auth-key and sub-inventory replies, their dead blocks retired, join gamemodel. **200 Windows / 198 Ninja (task 4.128):** the peer whisper joins gamemodel over the player and communication hosts. **199 Windows / 197 Ninja (task 4.129):** the action-result nodes join gamemodel over the zone, creature, player, world-event and UI-object hosts. **195 Windows / 193 Ninja (task 4.130):** four special-action skill replies join gamemodel over the creature host's special actions. **191 Windows / 189 Ninja (task 4.131):** four basic-action attack and skill replies join gamemodel over the creature host's basic action. **187 Windows / 185 Ninja (task 4.132):** four player-target skill replies join gamemodel over the player and creature hosts. **181 Windows / 179 Ninja (task 4.133):** six player attack replies join gamemodel over the player host's action result and magazine. **179 Windows / 177 Ninja (task 4.134):** two self-skill replies join gamemodel with the result-node factory and the main-node effect entry. **176 Windows / 174 Ninja (task 4.135):** the cross-counter replies join gamemodel over the world-event host. **174 Windows / 172 Ninja (task 4.136):** the add-effect and corpse-head replies join gamemodel over the zone's corpse lookup. **171 Windows / 169 Ninja (task 4.137):** the three bomb replies join gamemodel with the mine and bomb action lookups. **168 Windows / 166 Ninja (task 4.138):** the unburrow, remove-effect and change-shape replies join gamemodel. **164 Windows / 162 Ninja (task 4.139):** four skill-to-tile replies join gamemodel. **161 Windows / 159 Ninja (task 4.140):** the mine explosion and unseen-caster sector replies join gamemodel over `WorldEvents::SkillToSector`. **158 Windows / 156 Ninja (task 4.141):** the take-off and motorcycle get-off replies join gamemodel. **156 Windows / 154 Ninja (task 4.142):** the unseen skill on the player and the untransform reply join gamemodel. **154 Windows / 152 Ninja (task 4.143):** the say and global chat replies join gamemodel. **151 Windows / 149 Ninja (task 4.144):** the learn-skill, cannot-add and remove-from-gear replies join gamemodel. **149 Windows / 147 Ninja (task 4.145):** the two skill-failed replies join gamemodel. **146 Windows / 144 Ninja (task 4.146):** the motorcycle ride and gear-to-zone replies join gamemodel. **144 Windows / 142 Ninja (task 4.147):** the shop purchase and sale replies join gamemodel with `CheckItemForSkillIcon`. **142 Windows / 140 Ninja (task 4.148):** the trade error and verify replies join gamemodel. **140 Windows / 138 Ninja (task 4.149):** the inventory-item and object deletion replies join gamemodel. **139 Windows / 137 Ninja (task 4.150):** the cannot-use reply joins gamemodel. **136 Windows / 134 Ninja (task 4.151):** the three NPC question replies join gamemodel. **135 Windows / 133 Ninja (task 4.152):** the blood bible list joins gamemodel. **133 Windows / 131 Ninja (task 4.153):** the storage price and other-creature status replies join gamemodel. **131 Windows / 129 Ninja (task 4.154):** the set-position and dark-light replies join gamemodel. **129 Windows / 127 Ninja (task 4.155):** the real-wearing and item-use replies join gamemodel. **128 Windows / 126 Ninja (task 4.156):** the shop market condition joins gamemodel. **127 Windows / 125 Ninja (task 4.157):** the player's own skill success joins gamemodel. **124 Windows / 122 Ninja (task 4.158):** the three shop list replies join gamemodel. **121 Windows / 119 Ninja (task 4.159):** the power point and point exchange replies join gamemodel. **120 Windows / 118 Ninja (task 4.160):** the party invitation joins gamemodel. **119 Windows / 117 Ninja (task 4.161):** the party joined joins gamemodel. **117 Windows / 115 Ninja (task 4.162):** the vampire portal and item drop replies join gamemodel. **115 Windows / 113 Ninja (task 4.163):** the fast move and fake move replies join gamemodel. **114 Windows / 112 Ninja (task 4.164):** the peer's file request joins gamemodel. **112 Windows / 110 Ninja (task 4.165):** the login and new-account welcomes join gamemodel. **111 Windows / 109 Ninja (task 4.166):** the full quest list joins gamemodel. **110 Windows / 108 Ninja (task 4.167):** the pet's skill joins gamemodel. **109 Windows / 107 Ninja (task 4.168):** the tile effect joins gamemodel. **107 Windows / 105 Ninja (task 4.169):** the personal shop replies join gamemodel. **106 Windows / 104 Ninja (task 4.170):** the created item joins gamemodel. **105 Windows / 103 Ninja (task 4.171):** the item-to-item verdict joins gamemodel. **104 Windows / 102 Ninja (task 4.172):** the burrowing creature joins gamemodel. **102 Windows / 100 Ninja (task 4.173):** the bat and wolf replies join gamemodel. **100 Windows / 98 Ninja (task 4.174):** the monster from a burrow or a transformation joins gamemodel. **99 Windows / 97 Ninja (task 4.175):** the monster appearing joins gamemodel. **98 Windows / 96 Ninja (task 4.176):** a creature's skill on another creature joins gamemodel. **96 Windows / 94 Ninja (task 4.177):** the player's skill on a creature, and a skill on the player, join gamemodel. **95 Windows / 93 Ninja (task 4.178):** a creature turning into a slayer joins gamemodel. **94 Windows / 92 Ninja (task 4.179):** a creature turning into a vampire joins gamemodel. **92 Windows / 90 Ninja (task 4.180):** the vampire from a burrow or a transformation joins gamemodel. **91 Windows / 89 Ninja (task 4.181):** the vampire appearing joins gamemodel. **90 Windows / 88 Ninja (task 4.182):** the slayer appearing joins gamemodel. **89 Windows / 87 Ninja (task 4.183):** the ouster appearing joins gamemodel. **88 Windows / 86 Ninja (task 4.184):** a monster's corpse appearing joins gamemodel. **87 Windows / 85 Ninja (task 4.185):** a vampire's corpse appearing joins gamemodel. **86 Windows / 84 Ninja (task 4.186):** a slayer's corpse appearing joins gamemodel. **85 Windows / 83 Ninja (task 4.187):** an ouster's corpse appearing joins gamemodel. **84 Windows / 82 Ninja (task 4.188):** an NPC appearing joins gamemodel. **83 Windows / 81 Ninja (task 4.189):** a trade's preparation joins gamemodel. **82 Windows / 80 Ninja (task 4.190):** the player's transformation joins gamemodel. **81 Windows / 79 Ninja (task 4.191):** an item's pick-up joins gamemodel. **80 Windows / 78 Ninja (task 4.192):** a skill used on an inventory item joins gamemodel. **79 Windows / 77 Ninja (task 4.193):** the event notices join gamemodel. **78 Windows / 76 Ninja (task 4.194):** a gun's reload joins gamemodel. **77 Windows / 75 Ninja (task 4.195):** the reconnect to the login server joins gamemodel. **76 Windows / 74 Ninja (task 4.196):** the reconnect to the game server joins gamemodel. **75 Windows / 73 Ninja (task 4.197):** an NPC's answers join gamemodel. **74 Windows / 72 Ninja (task 4.198):** the player's update on entering a zone joins gamemodel, the last packet handler. **71 Windows / 69 Ninja (task 4.199):** the three generators that attach effects to creatures join gamemodel. **69 Windows / 67 Ninja (task 4.200):** the guided and homing attacks on a creature join gamemodel. **67 Windows / 65 Ninja (task 4.201):** fire piercing and playing with fire join gamemodel. **65 Windows / 63 Ninja (task 4.202):** the attach-zone and pixel-ripple generators join gamemodel. **63 Windows / 61 Ninja (task 4.203):** the unused door actions go and the colour-set table joins gamemodel. **61 Windows / 59 Ninja (task 4.204):** the pack-file record and the objects' screen rects join gamemodel. **58 Windows / 56 Ninja (task 4.205):** the video stub, the update base and the work-queue node base join gamemodel. **57 Windows / 55 Ninja (task 4.206):** the debug kit joins gamemodel. **56 Windows / 54 Ninja (task 4.207):** `Globals.cpp` retired. **55 Windows / 53 Ninja (task 4.208):** `RenderText` takes its surface and TextSystem defines it. **54 Windows / 52 Ninja (task 4.209):** the packet wait joins gamemodel. **53 Windows / 51 Ninja (task 4.210):** the opening's loop, `g_pAvi` and the video stub retired. **52 Windows / 50 Ninja (task 4.211):** the frame present joins dxlib. **51 Windows / 49 Ninja (task 4.212):** the append patch joins gamemodel. **50 Windows / 48 Ninja (task 4.213):** the tile renderer joins SpriteLib. **49 Windows / 47 Ninja (task 4.214):** the MIDI player's state joins gamemodel. **48 Windows / 46 Ninja (task 4.215):** the MP3 player retired. **47 Windows / 45 Ninja (task 4.216):** the effect resource container joins framelib. **46 Windows / 44 Ninja (task 4.217):** the loading thread's sprite-pack jobs join SpriteLib. **45 Windows / 43 Ninja (task 4.218):** the packet-handler registry joins gamemodel. **44 Windows / 42 Ninja (task 4.219):** the effect generator table joins gamemodel. **43 Windows / 41 Ninja (task 4.220):** the Netmarble launch check joins gamemodel. **42 Windows / 40 Ninja (task 4.221):** the help decision tree's conditions retired. **41 Windows / 39 Ninja (task 4.222):** help events' mail joins gamemodel. **40 Windows / 38 Ninja (task 4.223):** the zone's tile provider becomes a SpriteLib template. **39 Windows / 37 Ninja (task 4.224):** the never-drawn event background images retired. **38 Windows / 36 Ninja (task 4.225):** the object selector joins gamemodel. **37 Windows / 35 Ninja (task 4.226):** the helicopters join gamemodel. **36 Windows / 34 Ninja (task 4.227):** the zone's sounds join gamemodel. **35 Windows / 33 Ninja (task 4.228):** the FL2 text layer joins VS_UI. **34 Windows / 32 Ninja (task 4.229):** the creature draw exceptions join gamemodel. **33 Windows / 31 Ninja (task 4.230):** the guild marks join VS_UI. **32 Windows / 30 Ninja (task 4.232):** the account input rules join gamemodel and the BMP loader SpriteLib. **31 Windows / 29 Ninja (task 4.233):** the map cell joins gamemodel. **30 Windows / 28 Ninja (task 4.234):** the player's available skills join gamemodel. **29 Windows / 27 Ninja (task 4.235):** the status changes join gamemodel. **28 Windows / 26 Ninja (task 4.236):** ClientFunction splits into gamemodel, SpriteLib and UiRuntime. **27 Windows / 25 Ninja (task 4.237):** the item uses join gamemodel. **26 Windows / 24 Ninja (task 4.238):** the talk box and the message box join gamemodel. **25 Windows / 23 Ninja (task 4.242):** the creature joins gamemodel. **24 Windows / 22 Ninja (task 4.243):** the creature that wears gear joins gamemodel. **23 Windows / 21 Ninja (task 4.244):** the NPC joins gamemodel. **21 Windows / 19 Ninja (task 4.245):** the fake creature joins gamemodel and MemoryPool.cpp, empty of pools, is retired. **20 Windows / 18 Ninja (task 4.246):** the zone joins gamemodel. **19 Windows / 17 Ninja (task 4.247):** the player joins gamemodel. **18 Windows / 16 Ninja (task 4.248):** the divine-guidance silhouettes join gameview, a new library. **17 Windows / 15 Ninja (task 4.249):** the creature shadows join gameview. **16 Windows / 14 Ninja (task 4.250):** the creatures drawn with an effect join gameview. **14 Windows / 13 Ninja (task 4.251):** the crash report and the Windows version join gamemodel. **13 Windows / 12 Ninja (task 4.252):** the start-up's loading joins gameclient, a new library. **12 Windows / 11 Ninja (task 4.253):** the packet handlers' helpers join gameclient. **11 Windows / 10 Ninja (task 4.255):** the waiting screens' update joins gameclient. **10 Windows / 9 Ninja (task 4.256):** the game's UI calls join gameclient. **9 Windows / 8 Ninja (task 4.258):** the UI's messages join gameclient. |
| R2 | Packet `.cpp` files still defining a packet-style `::execute(Player` | **0** | `grep -rlE '^void\s+\w+::execute\s*\(\s*Player' Client/Packet/{Gpackets,Cpackets,Lpackets,Rpackets,Upackets} --include='*.cpp' \| grep -v Handler \| wc -l`. Baseline 448. Holds the line since `Packet::execute` itself was deleted; the client twin of the server's R4. |
| R3 | Live `sprintf`/`strcpy`/`strcat` lines under `Client/Packet` and `Client/PacketHandler` | **0** | Line-based; strips `//` tails before matching, so a commented-out call does not count. `\b` rejects the `w` in `wsprintf`, which R7 sees instead. Baseline 61 (a quarter of it commented-out code). Holds the line since the packet-tree copy pass (2026-09-04, PR #76). |
| R4 | Library-compiled `.cpp` files referencing `g_p*` client globals no library file defines | **0** | Over the library dirs (minus CMake-excluded files) plus the `packetwire` and `gamemodel` membership files; comment lines excluded; the subtraction is library-wide, so a library file reading a global another library defines is not a seam. Blind to a library file calling an executable-side *function* (the link proofs cover that) and to a global not named `g_p*`. Baseline 83. **20 on 2026-09-18:** the editor uses the common TextService-backed printer, removing its direct `g_pLast`/`g_pBack` declarations. The printer still reaches game state; this is a reduction in direct references, not a claim that rendering is independent. **10 on 2026-09-21:** moving `UserOption` and `g_pUserOption` into `VS_UI` makes ten existing UI files resolve their only executable-owned global within the library; an ownership reclassification, not ten further file extractions. The text-mode scan now includes the NUL-bearing `VS_UI_GameCommon.cpp` on every platform: **11** is the corrected count; GNU grep previously omitted that file while BSD grep counted it.  **10 on 2026-09-24:** the duplicate inactive VS_UI/DebugInfo.cpp implementation is removed.  **5 on 2026-09-24:** zone, creature, NPC and guild metadata plus the game calendar belong to gamemodel, with real object/link tests; their implementation bytes are unchanged. **4 later that day:** CImm's unused sound-manager declaration is removed. **0 later that day:** party ownership and eight UI host callbacks remove the remaining live references; an inline comment naming the player global is translated without changing the scanner. This is not a claim that every UI object links independently. |
| R5 | Direct packet `execute()` call sites outside `Client/Packet` | **0** | The unused, commented-out `PacketAttackMelee` block in `CGameUpdate.cpp` was deleted on 2026-09-24. Added when the 2.2 review found the client fabricates packets locally and calls `execute()` on them; a live caller is a compile error now, before it is a ratchet failure. |
| R6 | *retired* — `packetwire` members calling `SendBugReport` | — | Lived one slice (2026-09-03). Added to replace the failed-link detector that stubbing the symbol had disabled; fired on the next promotion (count 2), which said "move the function, not the seam". `SendBugReport` is in `Client/Packet/WireHost.cpp`, the stub is gone, and the link is the detector again — narrower, since a link catches a call only in a library `unit_tests` links and in an object a test pulls in, which is what the address-taking link proofs in `test_wire_host.cpp` and `test_player_base.cpp` guarantee. |
| R7 | Call sites handing a game string table entry to a printf as its **format**, where the lookup is spelled at the call site | **0** | Five alternatives: the `sprintf` family (`fprintf` included), the size-taking family, `AddFormat`, the offset-append form `sprintf(buf + strlen(buf), …)`, and `.Format` (`MString::Format` is a printf reached as a method). The tree is joined before matching because sites put destination and format on different lines. **Blind to indirection**: an entry copied into a static array or a local first is invisible. Baseline 293; holds the line since PR #71. On its own it is not a measure of finding C19. |
| R8 | printf-family calls whose **format argument is not a string literal**, across `Client`, `VS_UI` and `basic`, headers included | **0** | The population R7 measures a spelling of; it cannot tell a table entry from a legitimate forward, so renaming cannot satisfy it. The 40 were read: 27 vararg forwarders, 6 inside `SafeFormat`'s `Emit`, 2 literals behind `TEXT()`/`_T()`, 5 declarations. The family list was enumerated from the tree (`fprintf`, `vswprintf` included). Cannot see a destination containing parentheses; those 16 sites were audited by hand, all literal formats. Added 2026-09-04 (PR #73).  **34 on 2026-09-24:** SafeFormat uses literal CRT conversions and bounded field padding; its six computed format calls are gone.  **0 on 2026-09-24:** live diagnostic and message entry points carry tagged arguments into SafeFormat; inactive debug formatters are retired and two macro-wrapped UI literals are bounded. The scanner and its scope are unchanged. |
| R9 | Dynamic exception specifications (`throw()`, `throw(X, Y)`) under `basic/` | **0** | Not a removal: the first conformance slice went looking and found `basic/` had never carried one. The parentheses must hold type names or nothing, which is what tells a specification from a `throw` statement - the six live `throw ("...")` statements in the shop packets do not count, and neither does `throw Error(...)`. Blind to a type list it cannot spell (a template argument, a pointer, a comment between the parens), to a specification inside a `/* */` block, and to one behind a macro. The file list is asserted non-empty, so a renamed `basic/` fails instead of measuring 0 of nothing. Added 2026-09-06. |
| R10 | The same, across the library set: `basic`, `Client/SpriteLib`, `Client/TextSystem`, `Client/DXLib`, `Client/framelib` and `VS_UI` as whole trees, the `gamemodel` membership file, the `packetwire` membership file **and every `.h` under `Client/Packet`** | **0** | All of it is the packet tree - every other library is already at 0. 2,211 in the membership file's `.cpp` and 9,252 in the headers, over 1,042 of the set's 1,473 files; 8,528 empty `throw()` and 2,971 type lists. The headers are in the set because a specification is part of the function type, so a `.cpp`-only metric would have counted half of every edit as progress. Files are joined before matching: five specifications in `SocketAPI.cpp` span two lines, and line-based this set reads 11,494. Outside it, and the rest of the workload: `Client/PacketHandler` (284), the remaining executable sources (49) and `tests/` (9). Added 2026-09-06. **9,664 on 2026-09-07:** the 162 files directly under `Client/Packet` are at 0 - 143 carried a specification - and so is `tests/` (1,799 + 9 sites: the wire core, the packet framework and players, the info classes); ten destructors that carried a type list are spelled `noexcept(false)`, which the pattern does not count, and the remainder is the packet directories. **9,055 on 2026-09-07:** Lpackets, Upackets and Rpackets at 0 as well (609 sites, by script), leaving Cpackets and Gpackets. **5,883 on 2026-09-07:** Cpackets at 0 (3,172 sites, by script; the one packet that derives from another keeps its base unspecified), leaving Gpackets. **0 on 2026-09-07:** Gpackets at 0 (5,883 sites, by script; two packets deriving from GCChangeInventoryItemNum keep its getPacketSize unspecified). The library set is clean; what R10 never covered - `Client/PacketHandler` (284) and the executable sources (34) - is the next slice, with a ratchet of its own. |
| R11 | `register` storage-class specifiers in every `.cpp`, `.h` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`, `third_party` and `tests` | **0** | Closed finding 4 of the assessment: 627 in 23 files removed 2026-09-07, all mechanical. Counted by `tests/tools/count_register.pl`, a tokenizer that strips comments and string literals per file, because the R9/R10 pipeline strips `/* */` before `//` and reads the blitters' `//*pDest = ...` line comments as block-comment openers (over the 23 files on master it saw 538 of the 627, a figure that moves with file order) and would count the NPC script strings that say "register as a couple". The `.c` files stay outside: they compile as C, where the keyword is valid. Added 2026-09-07. |
| R12 | Dynamic exception specifications anywhere: every `.h`, `.cpp` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`, `third_party` and `tests` | **0** | R9 and R10 cover the libraries; this holds the executable side too, with the same pattern and blind spots. The last 319 outside the library set - 284 in `Client/PacketHandler` (one per handler `execute` definition), 32 in the two request-side packet factory managers, three in `RequestFileManager` and `Updater/UpdateManager.h` - went 2026-09-07, all type lists or `throw()` deleted, none promoted. Finding 3 of the assessment is closed on the source side. Added 2026-09-07. |
| R13 | Platform macros spelled outside `basic/Platform.h`: `__LINUX__`/`_LINUX` anywhere in the C++ tree, `PLATFORM_MACOS` outside `basic/`, and `PLATFORM_MACOS` in any CMake file | **0** | The port's build-contract slice (`docs/linux-macos-port-assessment-2026-09-07.md`, area A). Before it, four spellings of Linux coexisted and the build defined none - `__LINUX__` at 63 sites, so the socket and file APIs compiled neither their Windows nor their POSIX branch off Windows - while CMake handed every non-Windows target `PLATFORM_MACOS` in six places. `Platform.h` now detects the platform alone and adds `PLATFORM_POSIX`; the POSIX branches test that, and CMake passes no platform macro off Windows. `basic/` is exempt from the macOS count because `PlatformSDL.cpp`'s mach-o code is genuinely Darwin-only. The one counted use is the macOS font list in `TextBackendSDL.cpp`, a real Darwin branch; a second is a deliberate baseline change. Blind to the compiler builtins (`_WIN32`, `__APPLE__`, `__linux__`). Counted by `tests/tools/count_identifier.pl`, the register tokenizer generalised. Added 2026-09-08.  **0 on 2026-09-24:** the unchanged ordered font fallback list moves into `basic/PlatformSDL.cpp`; TextSystem consumes the platform API. |
| R14 | Live `GetTickCount()`/`timeGetTime()` calls under `Client` and `VS_UI` | **0** | The clocks work of the C++20 assessment (priority 5): the 32-bit tick wraps every 49.7 days, and every `previous + delay <= now` over it fires early or stalls across the wrap. Counted by `tests/tools/count_tick_reads.pl`, a character-level scanner over comments and string literals, because the regex strip the clocks slices first counted with reads a `//*` line comment as a block opener and swallows live code up to the next `*/` - the failure R11's tool was written for - and so missed the `srand(GetTickCount())` in `VS_UI_Item.cpp` outright (209 in 32 files where this reads 214 in 34). `basic/` is outside the count: its three hits are `Platform.h`'s definitions. Counts calls and definitions alike, so the 186 includes the non-Windows `GetTickCount` shim in `VS_UI_widget.h`, and counts `#if`-disabled code. 214 before the widget timers outside `GameCommon` moved to `MonotonicClock::IntervalTimer`; 100 of the 186 were the two `VS_UI_GameCommon` sources. Added 2026-09-10. **138 later that day:** the interval and window gates in those two sources moved (45 calls), the last `GetTickCount` in `VS_UI` with them - two `srand` seeds reseeded and the `VS_UI_widget.h` stub definition deleted make the 48; `VS_UI` holds 56 `timeGetTime()` sites in three files (the header's `SetTimer` is the third), four of them `srand` seeds and the rest deadline and elapsed-time shapes; `Client` 82. **113 later still:** the deadlines set and read within `VS_UI` and the flag-war end its handler sets moved to `TimePoint` (19 of the 25 calls; the gamble spin's two went to `IntervalTimer` and four are `srand` seeds reseeded from the same tick under another name); the two mission structs `VS_UI` had redeclared from the wire layer became typedefs of it; `VS_UI` holds 33 (the minigames' clocks and the three deadlines the executable sets through shared structs), `Client` 80. **93 on 2026-09-16:** the three minigames' clocks moved to `TimePoint` (20 calls): the minesweeper's start point and the elapsed time it keeps once a game ends (one `DWORD` had held both in turn), the arrow tile's per-character start, end and move points, a trap-delay point nothing sets, and its monster-move gate, and the crazy mine's start point; the elapsed millisecond counts reach the score message as before. `VS_UI` holds 13, the three deadlines the executable sets through shared structs; `Client` 80. **72 later that day:** those three deadlines moved with their setters (21 calls): the quest status's `quest_time` and the war list's `left_time` to `MonotonicClock::SecondPoint`, the whole-second point the `timeGetTime() / 1000` they were counted in floors to, the effect status's `delayFrame` to `TimePoint`. `VS_UI` holds no live call (a dozen mentions inside comments), `Client` 72. **53 later still:** the event register's start stamp (`MEvent::eventStartTickCount`, set by `AddEvent` and reset by the two ending cinematics in `MTopView` as each starts, read by the show-time blink, the expiry, the countdown captions, the scrolls, the fades and two dead script gates through one `ElapsedMillis()`) and the HP-modify list's stamp moved to `TimePoint` (19 calls) - a width and clock change only, since every read was a wrap-safe unsigned subtraction; `MTopView` holds 6, the quest event's parameter fields read as ticks; `MPlayer` 9 (two in its header, feeding the failing `previous + delay < now` shape), `MFakeCreature` 7. **35 later still:** the player's four stamps (the repeat and lock gates and the pet's dissection delay, all three the failing sum shape, and the trace limit, a subtraction), the fake creature's next-move deadline (sum-shaped too) and the two `PacketFunction` setters that fed them moved to `TimePoint` (18 calls); `MTopView` holds 6, the rest ones to fours. **10 on 2026-09-17:** the scatter (25 calls) - the wait screens' double-click gate and the game screen's write-only click stamp, the title fade's start, the `CGVerifyTime` deadline (sum-shaped, read against the frame clock), the update handler's loading stopwatch, the profiler's start stamp, the name window's send stamp (a tick parked in `TempInformation`'s generic `intptr_t` slot, now a `TimePoint` slot of its own), the reconnect handler's two `OUTPUT_DEBUG` stopwatches and the socket stream's byte-rate window (an `IntervalTimer`) moved to the monotonic clock, and `MTopView`'s six went with the quest caption they were in, a block no live event reaches. What is left is the frame clock (`g_CurrentTime`'s three live writers, `g_StartTime`, `CWinUpdate::m_CurrentTime`) and four calls that are not clocks: the two log-file names and the hack check that compares `timeGetTime()` against `GetTickCount()` on purpose. **5 on 2026-09-17:** the frame clock's first slice - one `StampFrameClock()` stamps `g_FrameNow`, a `TimePoint`, beside `g_CurrentTime` (the three writers call it), `CWinUpdate`'s clock members, which nothing outside the class read, are deleted with their two calls, and `g_StartTime` is a `TimePoint`; what is left is that one writer, the two log-file names and the hack check. R16 counts the readers of the `DWORD` from here. **4 on 2026-09-17:** the frame clock's `DWORD` and its writer are gone (the fifteenth slice); the four are the floor - two log-file names that are not clocks, and the hack check that compares two clocks on purpose. **2 later that day:** the code-health follow-up found the hack check was behind an unconditional return. Its body, call and unused timing state are deleted; only the two log-file names remain.  **0 on 2026-09-24:** both diagnostic-log names use the existing 64-bit monotonic clock; their buffers fit the full working directory and suffix. |
| R15 | The five side macros the once-shared sources switched on (`__GAME_CLIENT__`, `__GAME_SERVER__`, `__LOGIN_SERVER__`, `__SHARED_SERVER__`, `__UPDATE_SERVER__`) and the four spellings no translation unit ever had defined that guarded dead code the same way (`__UPDATE_CLIENT__`, `__EXPO_CLIENT__`, `__FULLSCREEN_MODE__` - defined only inside a dead branch - and `__GUILD_MANAGER_TOOL__`): live tokens under `Client/Packet`, live tokens in the rest of the C++ tree, and the names anywhere in a `CMakeLists.txt` or `.cmake` file, three counts checked separately | **0**, **0**, **0** | `__GAME_CLIENT__` was defined for every translation unit that read it (from CMake on three targets and the listed test sources, from `Client_PCH.h` everywhere else) and the other four never were, so a conditional on any of them had one value in every translation unit. Task 5.2's ninth slice evaluated the 190 under `Client/Packet` out (2026-09-13), the tenth the 361 in the rest of the tree and then the definition itself, from `CMakeLists.txt`, `tests/CMakeLists.txt` and `Client_PCH.h` in one commit. The first count keeps a server half behind one of the five from coming back through a copy from the server repo unnoticed (one behind another spelling is the include checker's both-branch walk to catch); the second makes a `#ifdef __GAME_CLIENT__` written from habit fail the suite, since nothing defines it and the guarded code would be dead; the third keeps the name out of the CMake files, where a definition put back would make every such guard live again - matched without word boundaries, because `-D__GAME_CLIENT__` has a word character on each side of the name (the same hole was in R13's CMake count and is closed there too). All three counts pin their file counts (a baseline of 0 cannot notice a shrunken scan); the CMake count does not see `CMakePresets.json`, the workflows or the Makefile, none of which passes a macro today. Counted by `tests/tools/count_identifier.pl`, so a comment or a string does not count - "zero live tokens", never "zero mentions". Blind to a spelling assembled by the preprocessor; a `#define` of the macro in a source file is one token and fails the count. Added 2026-09-13; both source counts at 0 and the CMake count added later that day. **Nine names since 2026-09-16:** the eleventh slice took `__EXPO_CLIENT__` (ten sites) and `__UPDATE_CLIENT__` (one) out, and with them `__FULLSCREEN_MODE__`, which only `__EXPO_CLIENT__` ever defined; `__GUILD_MANAGER_TOOL__` was already at 0 (its three sites went with the tenth slice). 13 live tokens to 0 in the nine files; the pattern names all nine so a guard on any of them written from habit, or copied back, fails the suite the same way. |
| R16 | Live spellings of `g_CurrentTime`, the frame clock's `DWORD`, under `Client` and `VS_UI` | **0** | The frame clock's move (the twelfth clocks slice, 2026-09-17): `g_FrameNow`, a `MonotonicClock::TimePoint` stamped by the same `StampFrameClock()`, sits beside the `DWORD`, and the readers move over a group at a time - the `DWORD` and its one tick call go when this reaches its definition, declarations and writer alone. Counted by `tests/tools/count_identifier.pl` (comments and strings stripped), with a 1,000-file floor on the scan. 144 before the slice; the 57 are the creatures' and the player's member stamps, two of `UserInformation`'s three deadlines (a `gamemodel` member; `GlobalSayTime`'s reads are commented out), `MGameTime`'s two, the item host's clock pointer and the wire host's clock function (two library seams), the debug prints, the writer and the externs. **18 on 2026-09-17:** the creatures' and the player's member stamps moved (nine deadlines in `MCreature` and `MPlayer`, `MFakeCreature`'s inherited read) with their four extern declarations; what is left is `UserInformation`'s two live deadlines and their seven sites, `MGameTime`'s three spellings, the two seams, the definition, the write, one debug print and three externs. **6 on 2026-09-17:** `UserInformation`'s two live deadlines moved behind five small members with a test (`GlobalSayTime`, whose three uses are commented out, deleted) and `MGameTime`'s start and current time take the point; what is left is the two seams, the definition, the write, one debug print and the extern in `Client.h`. **0 on 2026-09-17:** the two seams carry a `TimePoint` (the item host's clock pointer, the wire host's clock function, with their tests and two legacy-wrap cases) and `g_CurrentTime` is deleted; the count stays as the guard that nothing brings the name back. |
| R17 | Unbounded format and copy lines (`sprintf`, `wsprintf`, `strcpy`, `strcat`) under `basic`, `VS_UI` and `Client` outside the packet tree | **0** | The bounded-formatting work of the C++20 assessment (priority 7): R3's grep with `wsprintf` added and headers scanned beside sources, over the rest of the tree, `//` tails stripped, read with `-a` for `VS_UI_GameCommon.cpp`'s NUL bytes. A site leaves it when its destination is a real array and the call is `snprintf(dst, sizeof(dst), ...)` or an exact-length `memcpy`, checked by hand. 1,102 before the first slice (2026-09-17), which took `basic` to 0 (`C_DIRECTORY`, unreferenced and unlinkable on Windows, deleted; PlatformSDL and SafeFormat bounded) and `SpriteLib` to 0; `VS_UI` holds 783, the rest of `Client` 303. **1,016 on 2026-09-17:** the second slice took ten Client utility files to zero (70 lines: `GetWinVersion` takes its buffer's size, `CMd5`, which nothing constructs, deleted with its two files (its 50-byte error text overflowed for a long path nothing supplied), `MMusic`'s error text becomes the `char` array it was cast to, the rest `snprintf(sizeof)` into local arrays, and sixteen lines of block-commented debug code deleted with their blocks). **970 on 2026-09-17:** the third slice took `Client.cpp`, `GameMain.cpp`, `MZone.cpp` and `MCreature.cpp` to zero (46 lines: three live overflows - the Futec parser's 32-byte argument slots, the 128-byte log-file name under a `_MAX_PATH` directory, the 80-byte server-group name from the login server's world list - three `sprintf` calls that read their own destination made appends, the rest `snprintf(sizeof)` or an exact-length `memcpy`, and fifteen dead lines - `get_rand_str` and thirteen block-commented debug dumps - deleted with their blocks). **969 later that day:** remove the unused WinINet downloader and its one `strcpy`. **966 on 2026-09-18:** move `CMessageArray` into `basic` and replace its three unbounded copies with owned filename storage and bounded row copies. **963 later that day:** delete three dead formatting lines with the abandoned GL drawing blocks. **587 on 2026-09-21:** all remaining raw `wsprintf` calls are bounded or removed with an unused API; R18 prevents their return. **586 later that day:** `CToken` replaces `strcpy` with an exact-length copy into owned replacement storage. **584 later that day:** team introduction and billing dialog wrapping replace whole-remainder `strcpy` calls with owned UTF-8 rows. **570 later that day:** descriptors use owned rows and remove the unused substitution buffers (14 counted copy lines). **569 later that day:** rich-help removes the unused substitution loop, including one counted dead copy. **568 later that day:** notice mail date formatting uses the checked formatter. **518 on 2026-09-22:** option names use owned strings and checked indices; their formatted output and percentage appends are bounded. Two obsolete option-rendering blocks are removed (eight of the 50 counted lines).  **512 on 2026-09-24:** retire unused MinTrace formatting paths and bound the two TEXT-wrapped UI literals (six counted lines). **0 on 2026-09-24:** remaining fixed-buffer formatting and text assembly use proven array capacities or named allocation capacities; names, credentials and file paths retain full owned storage. Aliased item/UI string replacement and pixel-measured UTF-8 history wrapping have library regression tests. Remove 108 counted lines with obsolete comment blocks and seven with unused APIs. The original pattern, roots, packet exclusions and file floor are unchanged; this only closes the measured spellings, not all memory-safety debt. |
| R18 | Raw `wsprintf`, `wsprintfA` and `wsprintfW` identifiers | **0** | `tests/tools/count_wsprintf.pl` scans tracked and unignored C/C++ sources and headers, masks comments/literals and joins line splices. It counts aliases as well as calls, pins the source inventory and checks its lexer fixtures. Token-pasted spellings are outside this source check; the POSIX shim is also deleted. |

R7 and R8 exist as a pair on purpose: R7 is precise and blind to
indirection, R8 is coarse and cannot be evaded by spelling. Finding C19's
closure rests on both plus the hand audits listed in the review's C19 entry,
never on either number alone.

**Two checkers sit beside the ratchets**, in `tests/tools/`, because what
they measure needs parsing rather than grepping. `check_format_arity.pl`
(ctest `format_arity`) compares every converted format site's arguments
against the built-in English table in `MGameStringTable.cpp` (which
`InitGameStringTable()` installs over the file data on the English path, so
it is what the default build formats with); it fails when an entry asks for
more arguments than the site passes or a conversion's argument is provably
the other kind, and it floors both the sites it finds (295) and the sites it
resolves (283). `check_packet_indices.pl` (ctest `packet_indices`) is the index
half of code-health priority 1: over `Client/Packet` and
`Client/PacketHandler` it walks packet-derived values into subscripts —
through locals, across lines, one hop — and reports **103**, 91 into a
named, verified `CTypeTable` and a ceiling of **12** into a container that
is not, all guarded today. It read 114 and 13 when this paragraph was
written; `32cef4d2` took out the vampire addon lookup (113, 12), and
`4fa24b9e` turned ten `CTypeTable` writes into `GetMutable(x)` calls,
which answer NULL out of range and which the checker, reading `[]` only,
no longer counts (103). Seven of the 12 are the phone-slot subscripts of
the handlers task 4.15 moved into `gamemodel`; they stay in
`Client/PacketHandler`, so the checker still reads them, and
`test_model_handlers.cpp` runs every out-of-range slot byte past them. Its range-checked list is a named allowlist that
fails closed (when introduced, `CMessageArray::operator[]` truncated rather
than checked; it gained bounds checks on 2026-09-18, but the spelling of a
dereference still cannot prove a container is checked); a thirteenth raw
subscript has to be read before the number moves.

---

## Phase 0 — Scaffolding

- [x] **0.1 This document.**
  > **Status:** done (2026-09-01).
  - Owner: the status-line discipline itself; CLAUDE.md points here.

- [x] **0.2 Ratchet script.** `tests/ratchet/ratchets.sh`, registered in
  `tests/CMakeLists.txt` as a ctest, ports the server's script shape: fails
  on increase and on unrecorded decrease; generates into scratch space, never
  overwrites tracked files in place.
  > **Status:** done (2026-09-01, PR #36). `.gitattributes` carries
  > `eol=lf` for committed test data and scripts (the server's lesson).
  > `check()` fails on an unmeasurable value instead of passing it, and
  > every ratchet asserts the directories it greps exist, so a rename
  > cannot fail-open at a baseline of 0.
  - Owner: the ratchet test.

- [x] **0.3 Include-graph checker.** `tests/arch/check_includes.pl` (perl —
  Git for Windows ships it), run by ctest as `arch_includes`.
  > **Status:** done (2026-09-01, PR #36; rules extended per phase).
  > Current rules: **W0** every `.cpp` under `Client/Packet` is in exactly
  > one of `tests/arch/packetwire_files.txt` and
  > `packetwire_holdouts.txt` (keys case-folded; an indented membership
  > line is refused, because CMake anchors at column 1); **W1** a
  > `packetwire` member's include closure stays inside `packetwire`,
  > `basic/` and system headers; **W2** no `MinTr.h`/`DebugInfo.h`/
  > `DebugKit.h` in that closure; **M0–M2** the same for
  > `gamemodel_files.txt` (every listed file exists under `Client/`; the
  > closure may include only `basic/`, `Client/framelib/`,
  > `Client/Packet/`, `Client_PCH.h` and the file's own `.h` lines).
  > Angle includes that resolve inside the tree are checked like quoted
  > ones (the library's include path carries `Client/`); the search order
  > mirrors `target_include_directories(packetwire)`; `#if`/`#ifdef` on
  > the one-meaning macros (`__GAME_CLIENT__` defined, server macros
  > never - all five undefined since task 5.2's tenth slice retired
  > the client one) are evaluated so dead server headers are skipped as the
  > compiler skips them; an unresolvable include is a violation; the walk
  > dies on an empty file list. `tests/arch/baseline.txt` is empty by
  > design.
  - Owner: the `arch_includes` ctest.

---

## Phase 1 — `packetwire`: the wire-support library (pilot)

The first extraction, chosen so that no source file needed to change to
move (see *First candidate* at the end) and the result immediately covered
the top-risk area's foundations: streams, framing, crypto, sockets and the
info classes the GC packets delegate their parsing to.

- [x] **1.1 Create the `packetwire` static library** from the game-free
  subset of the `Client/Packet` root.
  > **Status:** done (2026-09-01, PR #34; 51 files then, the whole wire
  > layer bar one file now — see 2.4 and 5.1). Membership is
  > `tests/arch/packetwire_files.txt`, read by CMake, the include checker
  > and the ratchet script; the executable's list drops members by
  > absolute path. Build wiring: `__GAME_CLIENT__` from `Client_PCH.h`
  > (until task 5.2's tenth slice retired it),
  > `__WIN32__`/`__WINDOWS__` on WIN32, include dirs `Client/Packet`,
  > `Client`, `basic`; `DarkEden` and `VS_UI` link it.
  - Owner: the membership file + W0/W1/W2 in the include checker + R1.

- [x] **1.2 Link `packetwire` into `unit_tests`** and land the first parser
  tests. Stream construction in tests goes through a one-line
  `friend class SocketInputStreamTestAccess;` in the stream header
  (access-only, declared unconditionally so the class definition is
  identical in every TU); the helper is `tests/support/packet_stream_access.h`.
  Test TUs compile with the library's own defines (the Windows wire
  macros; `__GAME_CLIENT__=1` as well until task 5.2's tenth slice
  retired it): `Packet`'s virtual set changed under that one and the
  packet headers still switch on the others, so a mismatch is a real
  vtable/ODR break.
  > **Status:** done (2026-09-01, PR #34). Pinned: stream bounds
  > (zero-length rejected, over-read throws `InsufficientDataException`
  > and consumes nothing, wrap-around reassembly), the `read(std::string&,
  > len)` contract including the truncate-at-embedded-NUL-but-consume-
  > full-length asymmetry, `ModifyInfo` and `InventoryInfo` hostile
  > counts. The five allocate → read → push_back parser sites
  > (`InventoryInfo`, `GearInfo`, `ExtraInfo`, `RideMotorcycleInfo`,
  > `PCItemInfo`) push_back before read so a truncated payload does not
  > leak the in-flight slot — a regression guard, because MSVC's ASan
  > does no leak detection.
  - Owner: the tests themselves; `unit_tests` link line.

- [x] **1.3 First test-first fix: `StringStream` stack overflow.**
  `operator<<(float)` did `sprintf(buf, "%f", T)` into `char buf[12]`;
  `double` the same into `buf[22]`. The server fixed this family on its
  side (server RESTRUCTURING 1.4).
  > **Status:** done (2026-09-01, PR #34). The nine-operator numeric
  > family uses `snprintf` with range-sized buffers (`double` 352 — `%f`
  > of `-DBL_MAX` is 317 characters), and `m_Size` is `size_t` (it was
  > `ushort` and wrapped at 64 KiB, which the widened entries made
  > reachable). Tests pin the exact formatting at each old overflow
  > threshold.
  - Owner: the unit test.

---

## Phase 2 — Strip `execute()`, move the packet classes

The client twin of the server's tasks 2.3/2.4: the migration recipe, the
dispatcher design and the traps are recorded in the server's status notes.
This is what makes the `Gpackets` parsers (priority 1 in the code-health
review) directly unit-testable.

- [x] **2.1 `PacketDispatcher` in `packetwire`.** Table of packet id →
  `void(*)(Packet*, Player*)`, written only at startup; the server's
  `PacketDispatcher.h` shape verbatim, `DE_REGISTER_PACKET_HANDLER` macros
  included.
  > **Status:** done (2026-09-01, PR #37). Fixed `PACKET_MAX` table,
  > unconditional throws on double registration and out-of-range ids,
  > `InvalidProtocolException` from `dispatch` on an unregistered id.
  > All four receive loops (`ClientPlayer`, `Player`,
  > `RequestServerPlayer` - `RequestClientPlayer` was a fifth until task
  > 5.2's eighth slice - and `ClientCommunicationManager` with a NULL
  > player) call
  > `PacketDispatcher::dispatch` unconditionally; the transitional
  > `tryDispatch` fallback was deleted with `Packet::execute` itself in
  > 2.4, so the base class carries no handler entry point, as on the
  > server.
  - Owner: R2 ratchet + the dispatcher unit tests.

- [x] **2.2 Migrate the GC direction** and
- [x] **2.3 Migrate CG / LC / CR-RC / U.**
  > **Status:** done (2026-09-01, PRs #38 and #39). Every `execute()` body
  > in every direction was mechanically classified and stripped.
  > `Client/PacketHandlerRegistry.cpp` is the composition root, called
  > from `InitSocket()` (per login attempt), idempotent, its flag set
  > after success: the standard delegations, the packet-only handlers
  > (the datagram connection family), the `__BEGIN_DEBUG` thunks (kept
  > for the cout-branch platforms), `GLIncomingConnectionError`'s cout
  > trace, `GCExchangeList` and `CGConnectSetKey` as explicit no-ops.
  > The `DE_REGISTER` thunks carry `__BEGIN_TRY`/`__END_CATCH`, so the
  > per-packet stack-annotation frame the deleted bodies had is kept.
  > The 163 CG `execute()` bodies were deleted unregistered, with
  > `CGHandlersStub.cpp`; the honest basis, written in the registry
  > header: the server never sends CG/CL ids and the bodies were no-ops
  > (`PacketValidator`'s `CPS_NORMAL` accepts any id and
  > `Player::processCommand` has no validator, so the only change is
  > that a protocol-violating peer now disconnects — the server's trade).
  > **The receive loops were never the whole story**: the client
  > fabricates packets locally (skill echoes in `CGameUpdate.cpp`, GM
  > messages in `Client.cpp`, `CGConnectSetKey` on the login and
  > reconnect paths); all route through `PacketDispatcher::dispatch`
  > and R5 owns the rule.
  - Owner: R2 and R5 ratchets.

- [x] **2.4 Move the packet classes into `packetwire`**, plus
  `PacketFactoryManager` / `PacketValidator` / `PacketIDSet`; handlers move
  to `Client/PacketHandler/`, compiled into the exe, so `Client/Packet` is
  wire-only and the include checker locks the whole directory.
  > **Status:** done (2026-09-01, PR #41). Handlers moved by pure `git
  > mv` after one prep commit qualified their includes
  > (`"Gpackets/GCSay.h"`); the never-compiled CG handlers were deleted.
  > The last game reaches came out by seam: `CLLogin`'s
  > `g_pUserInformation` read is a packet member the sender sets,
  > `WHISPER_MESSAGE` sits beside `CRWhisper`, five root info classes
  > moved under `Client/Packet`, two dead `__GAME_SERVER__` bodies that
  > built wire fields from live game objects were deleted.
  > `PacketDiagnostics` is the hook through which `Datagram::read`
  > reports without linking the executable; since 5.1 it is an
  > interception point (how a test captures the text), and reporting goes
  > straight to the library's `SendBugReport` when no hook is installed.
  > **Goldens (the owner):** `test_packet_goldens.cpp` writes through the
  > real streams and pins the `.hex` files under `tests/golden/`, **54 of
  > them byte-identical copies of the server's** (all 19 encrypter packets at codes 0–5, GCMoveOK
  > framed, CGSay, CGWhisper) — `diff -r` of the two golden directories
  > is the cross-repo check. The first run found a real wire defect:
  > `CGMove` at encrypt code 0 wrote x,y,dir where the server reads
  > dir,x,y, and code 0 is reachable (the session code cancels for zone
  > 1301 in the shipped data); fixed on the client, the reading side
  > being the authority. Two asymmetries are pinned as fact-tests:
  > `CLLogin::read` here is one byte short of the login server's, and
  > `GCDropItemToZone` round-trips here while the server pins only its
  > `write()`. `test_packet_factories.cpp` proves the link for the
  > received directions and pins that the manager refuses CG and
  > out-of-range ids.
  > **Deliberately not done then** (task 5.2's ninth and tenth slices
  > took them on 2026-09-13): the ~186 `__GAME_SERVER__`/`__GAME_CLIENT__`
  > conditionals in 157 packet sources (they have one meaning in every
  > target, so the checker evaluates them; sweeping the dead server
  > halves is 5.2 work), and goldens for the ~500 unpinned packets.
  - Owner: W0/W1 over the whole `Client/Packet` tree; the goldens; the
    factory link test.

- [x] **2.5 Retire the wire-inventory workaround.** `test_wire_layout.cpp`
  calls the real factories instead of perl-lifted method bodies;
  `tests/wire-layout.txt` stays byte-compatible with the server's
  `wire_inventory_diff.sh`.
  > **Status:** done (2026-09-02, PR #42). `tests/generated/WireInventory.inc`
  > has the server's registry shape (one include and one registration per
  > factory class; `gen_wire_inventory.pl` emits it, `wire_inventory_fresh`
  > pins it, and the generator refuses a `getPacketName()` spelling it
  > cannot check). The test constructs every factory and its packet (the
  > link proof for the written CG/CL directions), checks id uniqueness,
  > and checks every id `PacketFactoryManager::init()` serves is a listed
  > factory at its own max size. Rpackets are constructed but kept out of
  > the rendered file. Two divergences went first: 112 factory classes
  > had been compiled out of every client build (`__DEBUG_OUTPUT__`,
  > `__GAME_CLIENT__` guards) and are unconditional now, and `CRRequest2`,
  > a dead duplicate of `PACKET_CR_REQUEST`, is deleted. Two wire defects
  > found and fixed test-first: `GCUpdateInfo`'s constructor never
  > initialised `m_pBloodBibleSign` while its destructor deletes it (a
  > truncated body freed a garbage pointer); and `GCAddItemToItemVerify`
  > read two parameters for `UP_GRADE_OK` where the server writes one, so
  > every successful item-grade upgrade over-read four bytes into the next
  > packet (its `write()` also dropped both `THREE_ENCHANT_OK` parameters;
  > the handler now sets the grade the server sends). The other seven
  > packets a cross-repo sweep had flagged are identical layouts.
  - Owner: `wire_inventory_fresh`; the all-factory construction in
    `test_wire_layout.cpp`; the `GCAddItemToItemVerify` goldens.

**Phase exit criteria:** met — R2 = 0; `Client/Packet` contains no handler
code; parser fixes are written test-first against real packet objects;
live-server smoke tests passed per slice.

---

## Phase 3 — The fix policy (in force from Phase 1 onward)

Not a code phase — the standing rule this plan exists to enable, stated once:

- [x] **3.1 Every fix names its test path in the commit message.** One of:
  (a) *lib + test* — the code is in a static library and the fix commit
  contains the test; (b) *moved, then fixed* — the fix's first commit moves
  the unit into a library, the second fixes it test-first; (c) *exempt* —
  the code is on the exemption list, the commit says so and carries the
  regression-guard wording. A fix commit that is none of the three is wrong.
  > **Status:** adopted 2026-09-02; enforced since PR #53 by
  > `tools/git-hooks/commit-msg`, which refuses a `fix:` commit without a
  > `Test path:` line naming one of the three (no bypass flag on purpose;
  > mode 100755, pattern anchored). Installed per clone with
  > `git config core.hooksPath tools/git-hooks` — CLAUDE.md carries the
  > instruction. Review-round repair commits are not exempt.
  - Owner: the hook.

The creature allocator is now directly testable in `basic/MemoryPool`. Its
four creature-specific pool instances stayed in `Client/MemoryPool.cpp` until
tasks 4.242-4.245 moved each beside its class; 4.245 retired the file.
`tests/unit/test_memory_pool.cpp` owns the size, alignment, membership and
reuse contracts; the move and the behavior change are separate commits.

---

## Phase 4 — Game-model extraction

Background work, one class family per branch, each independently mergeable.
Target library: `gamemodel` — links `basic` + `packetwire` + `framelib`
+ `decore` (the server's rules, vendored; task 4.12) (+ iconv for
`MString`), **no** SDL/dxlib/VS_UI. Membership is
`tests/arch/gamemodel_files.txt` (`.cpp` lines compiled, `.h` lines
allowed in the closure), read by CMake, which removes each member from the
executable by absolute path and asserts it, by the include checker (M0–M2)
and by the ratchet script (R4). Extraction means adding a file to that list
and cutting its `g_p*`/UI seams through a host struct the executable
installs at start-up (`MItemHost`, `MPriceHost`; see *What the review
rounds settled* for the host rules). Test fixtures share
`tests/support/gamemodel_world.h`.

- [x] **4.0 Compile the VS_UI client sources once.** The 36 `Client/*.cpp`
  files on `VS_UI_CLIENT_SOURCES` were compiled into both `VS_UI` and the
  executable (the list was relative, the exe's `REMOVE_ITEM` absolute, so
  it never matched — the LNK4217 noise CLAUDE.md used to describe).
  > **Status:** done (2026-09-02, PRs #46 and #47). The list is gone and
  > the files compile once, into the executable, which is the side that
  > linked all along: a linker map of `DarkEden.exe` before and after
  > (989,469 symbol → object rows, 0 differ) is the proof. `_LIB` is
  > `PUBLIC` on the `VS_UI` target: its ~100 `#ifndef _LIB` regions are
  > the standalone UI test harness, some of them class members, so the
  > 72 executable translation units that include VS_UI headers must see
  > the same layout the library's objects do. Unverified, not knowingly
  > broken: on `NOT WIN32` `Client/Client.cpp` stays in `VS_UI` (the
  > non-Windows executable filters out its WinMain).
  - Owner: the linker-map comparison (in PR #46); R4 no longer parses a
    CMake list.

- [x] **4.1 Pure tables first:** `ExperienceTable`, `MItemOptionTable`,
  `MGameStringTable`, `MSoundTable`, `SystemAvailabilities`, `FameInfo`.
  > **Status:** done (2026-09-02, PR #44). `gamemodel` exists with the
  > six tables, `ExpInfo`, `MString`, `MStringArray`. Seams cut:
  > `UseEnglishText` takes the `Properties` table (`UseEnglishTextFrom`
  > underneath is the testable core), and
  > `SystemAvailabilitiesManager::LoadFromStream(std::istream&)` takes
  > the lines `GameInit.cpp` reads out of the archive — the shape every
  > later file loader took. Fixed test-first: `ITEMOPTION_TABLE::LoadFromFile`
  > wrote part names past two fixed `MAX_PART` arrays for whatever count
  > the file declared; `ITEMTABLE_INFO`'s constructor left `Price`,
  > `Race`, `DropFrameID` unset. **Known, not fixed:** a refused
  > item-option table leaves the part-name `MString`s NULL and three
  > `VS_UI` call sites `strcpy` them unguarded (pre-existing for any
  > unset part; the alternative was heap corruption at startup).
  > `CToken` also joins unchanged (2026-09-21). `test_token.cpp` links the
  > actual tokenizer and pins token order, remainder ownership and null resets.
  > An ASan test then reproduced `SetString` reading a token after freeing its
  > own buffer. Replacement now copies before releasing the previous string;
  > interior/long tokens, suffixes and empty terminators have lifetime tests.
  > The action and effect tables join `gamemodel` (2026-09-29):
  > `MActionInfoTable` (`Action.inf`), `MEffectSpriteTypeTable`
  > (`EffectSpriteType.inf` and the per-action frames of
  > `ActionEffectSpriteType.inf`), `MEffectStatusTable` (`EffectStatus.inf`)
  > and `MCreatureSpriteTable` (`CreatureSprite.inf`), with their globals.
  > They reach no executable symbol. The move commit's only source change is
  > the creature sprite header naming the model's `DrawTypeDef.h` (the same
  > typedefs as SpriteLib's copy); the loader fixes below came after it, so
  > the `gamemodel` copies are not the pre-move bytes. 42, 49, 11 and 11
  > executable translation units link against them, in that order (nm,
  > 2026-09-29). `test_action_info_table.cpp` and `test_effect_tables.cpp` pin
  > the five file layouts through the real loaders and writers: round trips,
  > counts larger than the file refused, cuts that fail the stream. Crafted
  > records then found four loader defects, fixed test-first in separate
  > commits after the move: the action table read seven flags straight into
  > `bool` storage and cast its packet-type byte and effect-status word to
  > their enums unchecked (Clang's UBSan trapped the attack flag; a plain
  > Clang build read a flag byte of 2 as false), and read its four-byte
  > casting-action field into the two-byte member, over the castingAction
  > flag stored after it; the effect
  > status table read its two flags the same way; the effect sprite table cast
  > its draw type to `BLT_TYPE` unchecked and padded a short pair list with
  > the last frame read; the creature sprite table read its four-byte file
  > positions into half of a `long` that is eight bytes on the LP64 targets,
  > macOS and Linux (Windows and the wasm32 web build, where `long` is four
  > bytes, already read it whole). Out-of-range enum values now read as the
  > constructor's `NONE`/`EFFECTSTATUS_NULL`, or `BLT_EFFECT` for a draw type;
  > what the writers emit loads unchanged. The action table's writer changed
  > with its loader: it writes the casting action info zero-extended to four
  > bytes, where it wrote the two-byte member followed by the castingAction
  > flag and a padding byte, so a regenerated `Action.inf` differs from an
  > older one in those two bytes only for a casting row (the flag set) or a
  > row whose padding byte was not zero; readers only ever kept the low 16
  > bits, and a test pins the saved bytes. A follow-up fix keeps the action
  > table's casting action info and effect status when the file ends part-way
  > into them: the first fix had assigned the local those bytes were read
  > into, a mix of file and seed bytes.
  - Owner: the membership file, the CMake assertion, the include checker.

- [x] **4.2 Money/price/trade logic:** `MMoneyManager`, `MTradeManager`
  (with `MSortedItemManager`), `MPriceManager`.
  > **Status:** done (PRs #45, #57, #58). The money manager's one reach —
  > the storage-box help hint past 100,000 — is a per-wallet hook the
  > executable installs on the player's wallet (the trade and storage
  > wallets carry none); `operator=` keeps the target's hook. The trade
  > manager's accept delay reads the clock `MItemHost` carries. The price
  > manager goes through **`MPriceHost`** (race, level, stat sums, the
  > potion and gamble half-price events, and, until task 4.12's slice 5
  > removed it, the shop tax percentage carried unsigned as the server
  > sends it); without a host a price carries no player, event or skill
  > adjustment. Fixed test-first: `CanAddMoney`
  > ignored the balance, so a wallet near the limit said yes and the
  > `AddMoney` after it said no, with the other side's money nowhere to
  > go; `MItem`'s constructor never set `m_bTrade`, the grid position or
  > the durability, so an item arriving during a trade could be deleted
  > by `Trade`; the gamble price's tax multiply overflowed a 32-bit `int`
  > above 21,474,836 (64-bit on both paths now). Executable-side, exempt:
  > `MPetItem` never set its remaining experience or food type.
  > **Known, not fixed:** `CancelTrade` refunds money only — the offered
  > items keep their flag until the next trade start clears it, and a
  > refused refund still answers true; a star price for item type 0 is
  > −20 stars. (`GetItemPrice`'s dead `bMysterious` parameter, also
  > listed here, went with the purchase quote of task 4.12's slice 5.)
  - Owner: the membership file, the CMake assertion, the include checker.

- [x] **4.3 Containers:** `MItemManager`, `MGridItemManager`,
  `MSlotItemManager`, `MQuickSlot`, `MInventory`, `MStorage`, `MShopShelf`.
  > **Status:** done (PRs #54 and #56). The containers' two reaches — the
  > player's affect check on an item and the inventory sound it makes —
  > are `MItemHost::RefreshAffect` and `PlayItemSound`, host-guarded
  > statics on `MItem` so no caller checks for a host; a NULL player is a
  > skipped refresh where the old code would have crashed (recorded).
  > `MCorpse` stays executable-side (it owns an `MCreature`). Fixed
  > test-first: the slot manager wrote the item into its slot before the
  > id map could refuse it, and the grid's `ReplaceItem` removed the
  > occupant and returned true when the map refused the newcomer — in
  > both, the belt/stash/store handlers then `delete`d the refused item,
  > leaving the container holding a freed pointer; and
  > `MShopShelf::NewShelf` indexed its three-entry factory table with the
  > shelf type straight off the wire (`GCShopList`), calling through a
  > code pointer past the table for a value of 3 or more — the factory
  > answers NULL now and both handlers return on it (`SHOP_RACK_SPECIAL`
  > and `SHELF_SPECIAL` are both 1, said at the call).
  - Owner: the membership file, the CMake assertion, the include checker;
    `test_item_containers.cpp`, `test_inventory_storage_shop.cpp`.

- [x] **4.4 Item/skill cores:** `MItemTable`, `MItem`, `MObject`,
  `UserInformation`, `ClientConfig`, `MTimeItemManager`, the gear
  (`MPlayerGear` and the three race gears), `MShop`, `MSkillManager`,
  `MSkillInfoTable`, `SkillDef`.
  > **Status:** done (PRs #48, #53, #59, #60, #61). Everything the task
  > lists is in the library but the halves that are the packet and UI
  > side of items and skills, executable by design: **`MItemUse.cpp`**
  > (every class with a `UseInventory`/`UseQuickItem`/`UseGear` body,
  > moved whole, with the factory table), **`MObjectScreen.cpp`** (the
  > two screen-rectangle members that read draw interpolation state) and
  > **`MSkillAvailable.cpp`** (`SetAvailableSkills`, its vampire
  > counterpart and `CheckMP` — what the player can use *right now* —
  > plus the two war-bonus arrays). **`MItemHost`** carries the animation
  > clock (also the skill-use and trade delays), the top view's item-drop
  > frame pack, `RefreshAffect`, `PlayItemSound`, `RecalculateStatus`,
  > `ResetQuickItemSlot`, `RepairHint` and `EmptyMagazineFor` (the
  > magazine-fitting loop lives in `GameInit`); `GameInit.cpp`'s
  > `InitSkillTree` feeds `LoadFromFileServerDomainInfo` a stream. The
  > `__GAME_CLIENT__` guards in the moved files (always on) went with the
  > includes they wrapped.
  > **Fixed test-first, in the library:** `IsQuestItem` tested the item's
  > own flag only when the timed-item register existed; the requirement
  > getters returned `BYTE` while the slayer ceiling is 295 (a level-150
  > item looked easy to equip; 295 was the client's own bug, and task
  > 4.12's slice 4 makes it the server's 290); `ITEMOPTION_INFO`, `SKILLINFO_NODE`
  > (`m_SkillStep`, which `AddSkill` branches on) and the item table rows
  > had constructors that left fields unset; `CheckItemStatus` compared an
  > unsigned percentage against `int` thresholds read unchecked from the
  > configuration, so a negative threshold graded every worn piece as
  > almost broken; the zap branch of `AddItem` in all three gears read the
  > wrong slot and accepted a ring-less zap whenever that slot was full;
  > `RemoveItem(GEAR_*)` indexed the slot array with an unbounded id from
  > `GCRemoveFromGear`; `MShop::SetShelf` wrote `m_pShelf[n]` unbounded;
  > `LoadFromFileServerDomainInfo` indexed its eight-row table with an
  > `int` off the file through the raw pointer; `MSkillDomain::Clear`
  > could never reset its level counters (its `!=NULL` branch sat after
  > the pointer was nulled), so a reload left a domain whose learned-level
  > array was NULL and the next `UnLearnSkill` indexed it —
  > `SetStateFromSkillList` now derives the step lists and the array from
  > the skill list, `ClearSkillStep` clears the step lists a rebuild used
  > to append to, the loader checks its reads and a refused experience row
  > costs only itself; `LearnSkill` wrote `m_pLearnedSkillID[level]` with
  > the level straight from the skill file into an array sized by the tree
  > walk (refused now, before the skill enters the usable set).
  > **Known, not fixed:** the Ousters gear plays the gear sound and
  > recomputes the stats twice per item that goes on through its slot
  > table; `MSkillDomain::SaveToFile`/`LoadFromFile` have no caller
  > anywhere (reachable only through `CTypeTable`'s own file I/O; deleting
  > them is 5.2); `SKILLDOMAIN` (`SkillDef.h`) and `SkillDomain`
  > (`Packet/Types/CreatureTypes.h`) are the same eight names and
  > `MSkillManager.cpp` indexes itself with both; `LearnSkill`'s
  > "next level" gate is commented out, so any level can be learned in
  > any order; `IsPassive()` and `GetNextSkillList()` have always read
  > defaults in the client (their setters' only callers were the deleted
  > server data); the gear and shop sources hold ~490 Korean comment lines
  > and their headers ~180 (translating them wholesale would swamp the
  > byte-identity the move rests on). Untested library code:
  > `MSlayerGear::ReplaceItem`, `GetFitSlot`, the PDA, shoulder and
  > blood-bible slots, the Ousters stones, `InitSkillList`,
  > `AddSkillStep`, `GetExpInfo`.
  - Owner (all of 4.x): `gamemodel`'s membership file, the M0–M2 include
    rules, R4 shrinking; `test_item_table.cpp`, `test_item_core.cpp`,
    `test_player_gear.cpp`, `test_skill_core.cpp`.

- [x] **4.5 Rank-bonus table:** `RankBonusTable`, `RankBonusInfo` and
  their enum definitions.
  > **Status:** done (2026-09-17, PR #172, `ad86a5aa`).
  > `RankBonusTable.cpp` moves unchanged into `gamemodel`, including the
  > `g_pRankBonusTable` definition; its header and `RankBonusDef.h` join
  > the membership closure. No host is needed: the loader reads only its
  > own state and uses `MString`, `CTypeTable` and the wire race enum.
  > Packet-driven selection and UI rendering stay with their callers.
  > This is an extraction, not a loader-hardening pass: malformed or
  > truncated row handling is unchanged. Both Windows Debug builds and
  > their complete CTest suites pass, including `wire_inventory_fresh`.
  > Windows Release and all four macOS CI configurations also pass.
  > Live-server verification was not performed and is not a completion gate.
  - Owner: the membership file, CMake's executable-source exclusion,
    M0–M2, R1, and the `RankBonusInfo` / `RankBonusTable` cases in
    `test_gamemodel_tables.cpp` (defaults, binary field layout for all
    three races, row status, lookup bounds, release and table counts).
    The tests fail to link without the extracted constructor and loader.

- [x] **4.6 Map-file loading (code-health follow-up):** the header and five
  scenery record classes, with pure show-time data, are in `gamemodel`.
  > **Status:** implemented 2026-09-18. `MImageObjectScreen.cpp` retains
  > screen geometry; `MInteractionObjectAction.cpp` kept the
  > door/trap/switch actions (never called, retired in 4.203);
  > `ShowTimeChecker.cpp` retains scheduling. `ZoneMapData` now owns parsing
  > and validates the complete map before live sectors are replaced. Its
  > budgets bound input bytes, dimensions, sectors, objects and positions.
  > The executable allocates rows under RAII and transfers accepted records
  > to the zone. Regression tests use real record classes and binary fixtures,
  > including every truncation point and inclusive resource limits. R1 falls
  > from 477/475 to 473/471; no test invents game globals.
  - Owner: `tests/arch/gamemodel_files.txt`, M0-M2, R1,
    `test_zone_map_records.cpp` and `test_zone_map_loader.cpp`.

---

- [x] **4.7 Shrine-position loading:** shrine records and the row loop are
  in `gamemodel` behind a line-reader callback.
  > **Status:** done (2026-09-22). `ShrineInfoData.cpp` validates bounded,
  > complete, unique rows before replacing the table; failed reloads retain
  > positions and owners. Records initialize every field. Counts fit the
  > wire's one-byte identifier, zones fit the three minimap rectangles, and
  > coordinates fit their 128 by 256 maps. Rendering and hit testing use the
  > same validity predicate before indexing a rectangle. Resource access and
  > the shared pointer now live in `VS_UI/src/ShrineInfoManager.cpp`. Its bounded
  > text open rejects embedded NULs and excessive input before supplying
  > lines; `test_ui_metadata.cpp` exercises the real adapter. UI rendering
  > callers remain build/source regression guards.
  - Owner: `gamemodel_files.txt`, M0–M2, and `test_regen_tower.cpp`.

- [x] **4.8 World metadata ownership:** `MZoneTable`, `MCreatureTable`,
  `MNPCTable`, `MGuildInfoMapper` and `MGameTime` compile in `gamemodel`.
  > **Status:** done (2026-09-24). Implementations move unchanged; the
  > creature and guild headers use model sprite-ID declarations. Tests
  > construct all five production objects and exercise table ownership,
  > NPC-to-creature updates and the supplied game clock. This extraction
  > does not change the legacy file readers or calendar arithmetic.
  - Owner: `tests/arch/gamemodel_files.txt`, M0-M2, R1/R4 and
    `tests/unit/test_world_metadata.cpp`.

- [x] **4.9 UI metadata ownership:** quest records compile in `gamemodel`;
  the profile map and shrine resource adapter compile in `VS_UI`.
  > **Status:** done (2026-09-24). Implementations move unchanged after
  > the quest include preparation. Tests construct the real owners,
  > update quest/profile records and load shrine text through `CRarFile`.
  > Existing quest readers and profile image conversion are unchanged.
  - Owner: M0-M2, R1/R4, `test_quest_metadata.cpp` and `test_ui_metadata.cpp`.

- [x] **4.10 Party membership:** `MParty` records and ownership compile in
  `gamemodel`; `MPartyHost` supplies creature actions and the frame clock.
  > **Status:** done (2026-09-24). The executable installs named callbacks
  > and clears them at shutdown. Missing live creatures skip flag updates;
  > a missing clock means no kick delay, and missing config uses its default.
  > Membership ownership and callback order are tested without game globals.
  - Owner: M0-M2, R1, `test_party_model.cpp` and the designated host installer.

- [x] **4.11 UI runtime services:** `UiRuntime` supplies event flags, guild
  marks, copied portal/pet data and exchange requests through eight callbacks.
  > **Status:** done (2026-09-24). The executable installs named callbacks
  > before UI initialization and clears them at shutdown. Guild cache misses
  > retain find/load/find order; sprites stay renderer-owned. Exchange requests
  > construct real packets and borrow them synchronously for one send. Missing
  > live owners and out-of-range portal maps now return empty/false/null instead
  > of dereferencing them; failed pet reads preserve the last displayed data.
  > The host object links without executable globals. The larger UI translation
  > units still call executable functions; R4 zero does not prove their isolation.
  - Owner: R4, `UiRuntime.h`, the designated installer in `GameInit.cpp` and
    `tests/ui/test_ui_runtime.cpp` (real sprites, packets and copied snapshots).

- [ ] **4.12 Shared rules library:** the rules the client and the server both
  compute are implemented once, in the server's de-core (`src/domain`), and
  this repo builds a byte-identical, hash-manifested copy of a listed subset,
  `third_party/decore/` (the server's task 3.6 is the other half). Six slices,
  each a server change and then this repo's: (1) the shop buy, sell and repair
  price and the maximum durability; (2) the per-class grade policy and
  durability class table; (3) callers of the existing de-core functions (this
  repo only); (4) equip requirements; (5) the castle tax; (6)
  `SkillOutputFormulas` and the small rules.
  > **Status:** the client halves of all six slices are in (slice 6 on
  > `feat/shared-skill-rules`); the task stays open for what each slice
  > leaves over below, which needs the owner's decisions (the plan's
  > section 8) or the data export its section 7 names. Slice 1 is in: `MPriceManager`'s
  > buy, sell and repair quotes and the gear maximum durability call
  > `decore`, the inputs the server never sends are `MPriceHost` entries
  > with documented defaults, and `unit_tests` checks the adapters against
  > the vector rows. Slice 2 is in: `MGearItem`'s grade getters (maximum
  > durability, damage, critical, defense, protection, luck) read
  > `decore::gradePolicyOf`, `hasDurability` and `gradeOffsets` by the item
  > class. Four classes keep rules of their own rather than the table's:
  > the two couple rings, which the server builds outside `ConcreteItem`
  > (the client keeps the ring's rule and quotes their repair at the
  > server's 0); `MBloodBibleSign`, which the client makes itself and which
  > keeps the gear rule of 1000 a grade; and `MMotorcycle`, which keeps its
  > own maximum, since the server's is a placeholder 1. Dermis, Fascia, CarryingReceiver and
  > CoreZap start their maximum from the server info's 1, not the table.
  > `tests/unit/test_item_grade.cpp` checks every gear class
  > (`c87c4d0b`, and the fixes `5a3a1430`, `d63e2c5c`, `d31bc7c9`,
  > `dcd47512`, `bd801202`, `3f542250`). The named residuals are in
  > `docs/compiler-warnings-2026-09-27.md`, *Shop prices*. Slice 3 is
  > in (`feat/shared-stat-rules`): `MStatusManager` joined `gamemodel`,
  > and the character-select preview's to-hit, defense, protection and
  > damage call the server's per-race rules with its 10000 caps and a
  > combat damage bonus of 0; the slots feed it the slayer's real weapon
  > domain level (the cross in the heal domain, the mace in the enchant
  > domain) and the vampire's level, not its experience. The attack
  > speed stays the client's own (no rows; `MPlayer::CalculateStatus`'s
  > recompute is to be deleted). `MSkillInfoTable::GetVampireConsumeMP`
  > gives the skill bar and both skill descriptions the server's
  > INT-discounted cost, except for the sixteen vampire skills whose
  > server handlers (`execute(Vampire*)` under
  > `src/server/gameserver/skill`) never call `decreaseConsumeMP`: six
  > charge the table cost undiscounted, three a cost of their own and
  > seven nothing; fifteen keep the table cost (it lists them), and Will
  > of Life, one of the three, costs its own formula since slice 6, and
  > `MCreature::SetRegen` takes the DEX bonus from `decore`, which it
  > already equalled. `tests/unit/test_status_manager.cpp` and
  > `test_vampire_skill_cost.cpp` check them against `stats.tsv` rows.
  > Left on the host side: a vampire's skill is enabled at cost <= HP
  > where the server wants HP > cost, no race's cost applies the gear's
  > consume-MP ratio. The character list's four-bit weapon field, which
  > turned a mace into a sword, is fixed on the wire in both repos
  > (server PR #285, `c3b563a8`; client `2e0f39c3`): bits 17-18 carry
  > an extension code for cross1, mace and mace1, and the slot previews
  > them at their own domain (`test_status_manager.cpp`,
  > `test_slayer_outlook.cpp`). Slice 4 is in
  > (`feat/shared-equip-and-tax`): an item's requirement, which the
  > descriptions show (`MItem::GetRequireSTR/DEX/INT/SUM/Level`), is
  > `decore::requiredStats` over the item table and each option's
  > `RequireSUM` and `RequireLevel` in option order, by the rule of the
  > item's race flag (ousters, then vampire, then slayer). Whether the
  > player may use an item moved from `MCreature::CheckAffectStatus`
  > into `MItem::IsUsableBy` (`gamemodel`), which asks
  > `decore::meetsRequirement`. That brought the server's slayer cap of
  > 290 (the client had 295), its ousters level cap of 150, a vampire
  > table level of 0 raised by the options, and the 16- and 8-bit
  > widths the options are added at. The client's two gender flags map
  > to `decore::gender`: neither is Both, one alone is that sex, and
  > both set is read as Both, since no server value names both sexes and
  > the server's seed has no such item; nor has the client's own item
  > data in history (below), with 40 male-only rows, 40 female-only
  > and none with both. `MItemLimits.h`'s caps and the unused copies in
  > the two VS_UI game files are gone.
  > `tests/unit/test_equip_requirement.cpp` checks the requirement and
  > the check against `equip.tsv` rows (`2cef2abf`..`e0f27757`; the
  > fixes `bd3072a0`, `78bf217c`, `50110b35`, `ec6601fa`). The check
  > takes the requirement by the rule of the user's race, as
  > `isRealWearing` does, also for an item made for several races,
  > whose description still shows its first flag's rule (the fix
  > `02a57a4f`). The server's seed has such classes that ask
  > something: the blood bibles and castle symbols for all three races
  > and the relics for slayers and vampires, each asking a sum of 30
  > with no option, which every race's rule returns as it is, so the
  > wearer's rule moves none of them. The pets, three of them made for
  > all three races and any of them given an option by the pet
  > enchant, take no rule at all (below). The client's own item data
  > says the same where it reaches: the in-code item table `a8b0a5af`
  > deleted (`git show a8b0a5af~1:Client/MItemTable.cpp`, the same
  > file as at `9e6c4a34~1`, and its `InitItem2` companion
  > `Client/MitemTableInit.cpp`) has those three classes as its only
  > multi-race ones that ask anything (`MItemTable.cpp` lines 18992,
  > 22255 and 22618), each asking a sum of 30, and its couple rings
  > (lines 22801-22924) are for slayers (Race 1) and vampires (Race 2)
  > with every requirement 0 and neither gender flag, so the
  > couple-ring gate below changes nothing against it. That data stops
  > at class 69 (`ITEM_CLASS_OUSTERS_SUMMON_ITEM`) and predates the
  > pets (classes 74-76): it has no pet class, and nothing in the tree
  > says how the client's own `Item.inf` flags a pet. Four gates come
  > before the server's check, in this order: an item without the
  > user's race flag is refused; a pet of the user's race is usable
  > while it lives, whatever its table, options or gender flags ask,
  > and a dead one lends nothing; a quest item (`IsQuestItem`) asks
  > nothing and is usable by a slayer or vampire its gender allows and
  > by any ousters; and a couple ring is usable by a slayer or a
  > vampire whatever it asks, stats, level or gender. The first is the
  > client's own, and for a pet it is stricter than the server, whose
  > `isUsableItem` lets any race use a pet item and which never reads
  > `PetItemInfo`'s `Race` column; it is kept so the affect status
  > agrees with the client's use handlers (below). The second is the
  > server's (the fixes `3954a3b0` and the race-gate order after it):
  > `executePetItem` refuses a pet with no HP left (the client counts
  > down the HP the server sends as the pet's durability) and a
  > second-stage pet to an owner under quest level 40, which the
  > client does not check, and asks no wearing requirement. The
  > third is the server's time-limited gate for an item the timed-item
  > register holds, which `IsQuestItem` counts (`isRealWearing` asks a
  > time-limited item only its gender, and an ousters nothing), and
  > the client's own only for an item flagged a quest item
  > (`m_Quest`). The fourth is the server's: `Slayer` and
  > `Vampire::isRealWearing` let a couple ring through
  > (`isCoupleRing`) after their time-limited and premium-zone gates,
  > before any requirement, and `Ousters::isRealWearing` has no such
  > case (the fix `e5a331f7`).
  > Left over: the server's advancement-class check is not in de-core and the
  > client has none (the server refuses an item asking
  > `reqAdvancedLevel > 0` unless the wearer is advanced with a class
  > level at least that, and refuses an advanced wearer a non-advanced
  > weapon, coat, trousers or ousters boots), so the client shows such
  > an item usable where the server refuses it; the owner chooses
  > between a client-local check and a de-core function with a per-race
  > class predicate. The server asks the advancement class of a slayer
  > or a vampire before its time-limited gate, and of an ousters after
  > it. The premium-zone pay gate stays server-only: in a premium zone
  > a player who does not pay may not use a unique item or one with
  > several options, whatever the race, nor, as a slayer or a vampire,
  > a couple ring (`Ousters::isRealWearing` has that clause commented
  > out). A living pet of the player's race now asks nothing, as the
  > server asks nothing of a pet item. Before, the client
  > judged it by the gear rule: a pet gets an option from the pet
  > enchant, asking a level of 20 at most, which the vampire rule adds
  > to a level of 0, so a vampire under that level saw a vampire-only
  > pet drawn as unusable, and `02a57a4f` had briefly moved a pet made
  > for all three races, as the seed's types 0-2 are, to that rule
  > too, from the ousters rule its flags chose, which leaves a level
  > of 0 alone. The server seed's pets (`PetItemInfo`) are the Gara
  > Bezz, the Wolfdog Leash and the Wolverine Leash (types 0-2, Race
  > 7) for all three races, the Radio Controller (3, Race 1) for
  > slayers, the Stirge Bag (4, Race 2) for vampires and the Summon
  > Pixie (5, Race 4) for ousters. The client still asks a pet's race:
  > `UIMessageManager`'s `Execute_UI_ITEM_USE` and
  > `Execute_UI_ITEM_USE_SUBINVENTORY` send a use only for an item
  > flagged for the player's race, a pet included, and `IsUsableBy`'s
  > race gate agrees with them, so another race's pet is drawn unusable
  > and cannot be summoned from the client, where the server would let
  > it; letting a pet through both is the owner's call. Slice 5
  > is in (the same branch): a shop purchase is quoted in one place,
  > `MPriceManager::GetPurchasePrice`, which the buy check and the
  > shop tooltip ask, and it charges what the server's buy handler
  > does: the item at a market condition of 100 times the count, or
  > the mysterious rack's price once, then `decore::applyCastleTax`
  > at the castle's ratio, in the server's unsigned 32-bit width; a
  > motorcycle, which the handler prices in `executeMotorcycle`, is
  > the price of one at that market condition and untaxed. The
  > ratio has its own state (`SetShopTaxRatio`, from `GCShopVersion`,
  > `GCShopList` and `GCShopListMysterious`), apart from the market
  > condition the sell dialog's `GCShopMarketCondition` sets, and the
  > tax-change notice's percentage (`EVENTID_TAX_CHANGE`, the removed
  > `MPriceHost::ShopTaxPercent`), which taxed every buy price a
  > second time and was the mysterious rack's only tax, reaches no
  > price (`5bc25d48`, the fix `5536db6e`).
  > `tests/unit/test_price_manager.cpp` checks the server slice's
  > worked table. Left over, all from the wire (the plan's open
  > decision 7: a separate ratio field, at least 32 bits wide, with
  > goldens re-recorded in both repos): the packets that open the
  > shop carry the ratio where the NPC's market condition would be,
  > so a purchase takes the item at a market condition of 100, every
  > seed shop's, and a shop set to sell at another rate would be
  > quoted wrong; `GCShopVersion` sends the NPC's market condition in
  > the ratio's place when the ratio is 100, which the client takes
  > as the ratio (no tax while that condition is 100); and the field
  > is the 16-bit `MarketCond_t`, so a ratio outside -32768..32767,
  > which only the GM command can set, wraps (40000 arrives as
  > -25536 and is quoted untaxed where the server charges x400). The
  > gamble's Blood Bible adjustment still differs (the server applies
  > `getGamblePriceRatio`'s percentage, the client halves), and the
  > server's own product of price and count can wrap in `Price_t`
  > before the tax, which the client now reproduces. The item
  > description dialog prints `GetItemPrice(NPC_TO_PC)`, the item's
  > untaxed price at the sell dialog's market condition, not a
  > purchase quote, and the notice is still recorded as a zone event
  > that nothing reads. Slice 6 is in (`feat/shared-skill-rules`).
  > Will of Life's numbers come from `decore::skillformula::WillOfLife`
  > through one helper in the skill table (`MSkillInfoTable.cpp`,
  > `GetWillOfLifeHP` and `GetWillOfLifeDelay`), which fills the input
  > as the server's `SkillInput(Vampire*)` fills what that formula reads,
  > the party size 0 included, and says it is valid for that formula
  > only. The skill description's HP cost and `MCreature::SetRegen`'s
  > bonus while the effect lasts take its Damage, which they already
  > equalled (`5c0e7c7c`; `SetRegen` at the level of its last
  > `CheckRegen`, below). The
  > reuse time, which the cooldown bar (`C_VS_UI_SKILL::GetDelay`) and
  > the two skill packet handlers (`GCSkillToSelfOK1`, `GCSkillFailed1`)
  > computed as (3 + level / 10) * 2 s, is the server's run time, Delay *
  > 100 ms = 6000 + 200 * level ms, so the skill no longer lights up to
  > 1.8 s before the server accepts it; the three sites read one level,
  > the character window's (the fix `75aa0285`, whose message says the
  > server sets the run time on success and on failure alike: it does so
  > only when its handler ran and the time check passed, below). The
  > skill bar's cost gate (`GetVampireConsumeMP`, which now takes the
  > level) charges Will of Life that Damage, not the table's Mana (50 in
  > the server's seed), which also corrects the skill tree's description
  > (the fix `be8d8795`). `tests/unit/test_will_of_life.cpp` and
  > `test_vampire_skill_cost.cpp` check them against `skill_output.tsv`
  > rows. A slayer's skill range is `decore::skillRange`, through
  > `GetSkillRangeAtLevel`, which `MPlayer::GetActionInfoRange` asks
  > after its own paths (Head Shot's 3, the skills that take the
  > weapon's range, and Rapid Gliding, Bloody Zenith and Soul Rebirth's
  > formulas) and which moved into `gamemodel` for its test
  > (`52d872e3`, `8d2b12b9`); a vampire's and an ousters' keep the
  > client's integer step, to level 100 and to level 30. The server
  > ranges no vampire skill by its level (Rapid Gliding's, Bloody
  > Zenith's and Set Afire's ranges are its stats'). It ranges
  > Blunting, Tendril, Prominence, Teleport and Charging Attack by their
  > formulas' Range, the minimum plus the slot's level / 10, which the
  > step to level 30 gives for the span of 3 each has in the seed and
  > in the upstream `SkillInfo.inf`; `test_skill_range.cpp` checks the
  > step against those `decore::skillformula` formulas at levels 0 to
  > 30 (`8d2b12b9`'s message says the ousters' step has no server
  > counterpart). Soul Rebirth uses its server formula plus mastery:
  > 2 + active skill level / 10 + mastery level / 10, with no combined cap.
  > `GetSoulRebirthRange` in `gamemodel` owns the calculation; `MPlayer`
  > reads both live skill levels. Test: `test_soul_rebirth_range.cpp` covers
  > ten-level boundaries and all active/mastery level pairs from 0 to 30
  > against the server formula and mastery bonus. Vendored formulas and
  > packet bytes are unchanged.
  > The slayer change was a refactor: the rule
  > differs from the integer step only at a span of 50 or more (one
  > lower at levels 29 and 58), with a maximum below the minimum, and
  > past 255, and no slayer range in the data that could be checked
  > reaches them. `tests/unit/test_skill_range.cpp` checks it against
  > `skill_range.tsv` rows. The server's party experience pool and dark
  > and light rule have no client copy to replace: the client computes
  > no party experience and applies the dark and light levels the
  > server sends. `MParty::IsMemberInSight` stays, though the plan's
  > section 7 once listed it for deletion: it feeds
  > `PARTY_INFO::bInSight`, which the RC HP update and the off-screen
  > party arrow read. Left over:
  > - The `SkillInfo.inf` the client ships is not in the repository.
  >   What was checked for a slayer range the rule changes is the copy
  >   the upstream tree carried (`fe130dd8`'s
  >   `SkillTool/data/info/SkillInfo.inf`, 394 records, deleted in
  >   `718016dd`), the server's `SkillBalance` seed (374 rows) and the
  >   in-code overrides in `MSkillInfoTable.cpp`: 29 range pairs, whose
  >   only maximum below the minimum is the vampires' Raising Dead and
  >   Summon Servant. The ranges also drift as data: 25 of the seed's
  >   rows differ from that copy, and the in-code overrides agree with
  >   the seed where it holds the skill except Heter Chakram (4, the
  >   seed 5; Throw Holy Water and the bombs are not in it; `8d2b12b9`'s
  >   message says they all follow the seed). That is the plan's data
  >   export task, not a rule.
  > - Every client caller passes the party size 0 for a vampire (the
  >   server passes 0 for a vampire or a monster and 1 for a slayer or
  >   an ousters), never the party's size, so no formula grants a party
  >   bonus on either side (the server's FIXES.md records it as "No
  >   party bonus is ever granted").
  > - Will of Life's gate leaves out the gear's consume-MP ratio, which
  >   the server's `hasEnoughMana` adds, and enables the skill at
  >   cost <= HP where the server wants HP > cost, as for the other
  >   skills. The cost the tooltip and the skill tree show also leaves
  >   out what `decreaseMana` applies to the HP it charges (the Wisdom of
  >   Blood rank bonus's discount, then that ratio), so a vampire with
  >   the rank bonus is shown more than it pays; the bonus does not
  >   change whether the server allows the cast. A level-up does not
  >   recompute the skill bar (`Function_MODIFY_LEVEL` calls no
  >   `CheckMP`), so the cost's step every seventh level waits for the
  >   next HP change.
  > - The server sets Will of Life's run time only when `WillOfLife`'s
  >   handler ran and its time check passed. `GCSkillFailed1` also
  >   answers a cast `CGSkillToSelfHandler` refused before the handler
  >   (a complete safe zone, Paralyze, Cause Critical Wounds, Explosion
  >   Water, Coma, the werewolf form, no slot or a skill
  >   `isAbleToUseSelfSkill` refuses) and the handler's own time-check
  >   failure, neither of which touches the run time. The client cannot
  >   tell them apart and waits the full reuse time after every
  >   `GCSkillFailed1`, longer than the server, the safe direction; it
  >   did so before this slice, with the shorter old time.
  > - `MCreature::SetRegen` predicts Will of Life's bonus at the level
  >   current when `CheckRegen` last ran, where the server's
  >   `EffectWillOfLife` keeps the Damage of the level it was cast at.
  >   A level change alone does not skew it (`Function_MODIFY_LEVEL`
  >   calls no `CheckRegen`), so the prediction is the cast level's
  >   unless another `CheckRegen` (an effect status added, which
  >   `MPlayer::AddEffectStatus` follows with one for every status; a
  >   removal calls it only for Will of Life's own; a
  >   creature type change, `MODIFY_BASIC_DEX`, a rank bonus packet)
  >   follows a level change across a multiple of seven while the
  >   effect lasts (3 to 18 s); then it is off by 1 HP a tick until the
  >   server's HP updates correct it. It did so before this slice.
  > - The server checks most skills other than its four sliding and
  >   walking ones against the table's maximum range, not this rule, so
  >   the client's walking range for them errs short, the safe
  >   direction. Three slayer skills are checked against their
  >   formula's Range instead: Hit Convert (2 + level / 33), Multi
  >   Amputate (2 + level / 25) and Ultimate Blow (1 + level / 50).
  >   Where the client ranges them by the table (the action info file,
  >   not in the repository, decides whether they take the weapon's
  >   range), `decore::skillRange` at the seed's ranges equals that for
  >   Multi Amputate (2 to 6) and Ultimate Blow (1 to 3) at every level,
  >   and is one tile short for Hit Convert (2 to 5) at levels 33, 66
  >   and 99, also the safe direction.
  > The copy is
  > in: `decore`, a static library linked `PUBLIC` by `gamemodel`, synced
  > from server `831a8edc` (PR #283, the skill output formulas:
  > `SkillOutputFormulas`, whose party tables are read through the
  > clamping `partyEffectBoost` and `partyDurationBoost`, and
  > `skill_output.tsv`; PR #284, the slayer skill range: `SkillRange` and
  > `skill_range.tsv`, while its party experience pool and dark and light
  > rule stay on the server, with their rows in `vectors/server/`, which
  > the sync does not copy; before them PR #281, the equip requirements:
  > `EquipRequirement` and `equip.tsv`; PR #282, the castle tax:
  > `applyCastleTax` and the `tax-*` rows of `price.tsv`, which
  > `decore_tests` asserts and `MPriceManager::GetPurchasePrice` calls;
  > and PR #280, PR #278, PR #277 and the review fixes in PR #279).
  > `decore_tests` asserts both new files; the client calls
  > `WillOfLife` and `skillRange` (slice 6, above).
  > Never edit it: `perl tools/decore/sync.pl <server-root>` rewrites it,
  > `MANIFEST` and the README's commit line; a new vendored `.cpp` also goes
  > on the explicit list in `third_party/decore/CMakeLists.txt`. Its vector
  > rows are recorded on the server; a row that fails here is a toolchain
  > difference to investigate, never a row to re-record. Every
  > `ItemPriceInput` and `SkillInput` field is set by the caller
  > (value-initialise with `= {}`), and each adapter `static_assert`s `decore::itemclass` against
  > its `ITEM_CLASS_*`.
  - Owner: `decore_vendored` (`sync.pl --verify-manifest`: hashes, no
    unlisted file, every vendored `.cpp` built), `decore_tests` (every vector
    row on every toolchain, and under node in `web.yml`), the
    `decore-upstream` job in `linux.yml` (`sync.pl --check` against server
    master), and `arch_includes` rule DC1.

- [x] **4.13 Chat filter:** `MStringMap` and `MChatManager` compile in
  `gamemodel`; `MChatHost` supplies the player's "filter bad words" option.
  > **Status:** moved (2026-09-29). `RemoveCurse` rewrites every server
  > chat line (`GCSay`, `GCWhisper`, `GCGlobalChat`, `GCGuildChat`,
  > `GCPartySay`, `RCSay`, `CRWhisper`) in place, and checks what a
  > player types: the new-character name (`Execute_UI_NEW_CHARACTER`,
  > `Execute_UI_NEWCHARACTER_CHECK`), a new account's login ID
  > (`RegisterNewUser`), the custom nickname
  > (`Execute_UI_CHANGE_CUSTOM_NAMING`) and the personal-store sign
  > (`Execute_UI_STORE_SIGN`). All of these run after `InitGame` has
  > called `InitGameObject`, which installs the host. Both implementations move unchanged but for one read (and
  > `RemoveCurse`'s comments, translated from Korean): the filter asked `g_pUserOption->FilteringCurse` (VS_UI) directly, and now
  > asks the host, which the executable installs in `InitGameObject` and
  > clears at shutdown. Without a host the answer is `UserOption`'s
  > default, on; the executable's adapter answers the same while
  > `g_pUserOption` is NULL (the old read dereferenced it unguarded; no
  > filter call is known to run then). `MStringMap` needed nothing.
  > `tests/unit/test_chat_filter.cpp` then pinned the map and the filter
  > through the real loaders (English case, punctuation, word edges,
  > adjacent and overlapping words; Korean words of one to four
  > syllables in CP949; stray lead bytes, every byte value, 255-byte
  > lines; `AddMask` at every percent), under ASan and Clang UBSan too,
  > and seven defects were fixed test-first: a repeated English word was masked only the
  > first time, and the second match's marks landed on the wrong letters;
  > both mask texts (165 characters in 256-byte arrays) were indexed by a
  > running count, so a word past the 165th letter, or an `AddMask` line
  > of about 150 bytes, was cut by a NUL and read past the array from 256
  > on; a Korean replacement longer than the Korean bytes left ran on
  > through its index array's entries past them, which still held the
  > English pass's letter positions (so "abcdefgh " and a three-syllable
  > word came back "abcdefou love y" on every platform, ASan included)
  > or, with fewer English letters than Korean bytes, were never written
  > (the fix's commit named only this case, and called its test a
  > regression guard under ASan; the review added the deterministic
  > cases); an empty English word in the
  > binary list hung the filter; and the map loader stored an unset
  > pointer for an entry whose value is not its key, read its flag
  > straight into a `bool`, looped over an uninitialised count and
  > inserted a key it could not read. The seventh was found by the
  > branch's review, since the first `AddMask` tests used only percents
  > 0 and 100: a kept two-byte character stepped two bytes, so at any
  > percent in between (`GCSay` passes 50) a line that ends in a lone
  > lead byte stepped over its NUL and masked the memory after it.
  > Two more were found by the branch's first Windows CI run (36639592150),
  > where `unit_tests` crashed in both Windows jobs. `MString`'s
  > comparisons passed a NULL string (an empty `MString` keeps no
  > storage) to `strcmp`, so an empty key crashed `MStringMap` on every
  > platform, and with it `IsAcceptID("")` for a player with an ID on the
  > list; a NULL string now compares as "". The chat handlers pass
  > a wire-supplied name to `IsAcceptID`: `GCGuildChat`'s sender
  > (`GCGuildChatHandler.cpp:42`), `GCWhisper`'s name
  > (`GCWhisperHandler.cpp:50`), `GCSay`'s creature name
  > (`GCSayHandler.cpp:76`), and `GCPartySay`, `GCGlobalChat`, `CRWhisper`,
  > `RCSay` and `GCNPCSay` likewise, so an empty sender name from a server
  > would have crashed the client: the fix is a live defensive fix. Found
  > by reading, not seen in a game. And `LoadFromFileCurse`
  > tested `eof()` before each read instead of the read: MSVC's stream
  > library turns the failed read after a file's final newline into an
  > empty word, which is what reached the map; with libc++ and libstdc++
  > the last word was added twice and a blank file gave an uninitialised
  > buffer; and by MSVC's source (not run) a NUL byte looped forever.
  > **Known, not fixed:** the Korean lists the game loads from its
  > binary file pass through `MString::LoadFromFile`, which converts each
  > word from CP949 to UTF-8, while the filter pairs bytes and looks the
  > words up in two- to eight-byte windows, so the shipped Korean lists
  > match no text, CP949 or UTF-8 (pinned by
  > `KoreanWordsFromTheBinaryListAreConvertedAndMatchNothing`); fixing it
  > needs a decision on the encoding chat is filtered in. A replacement
  > longer than its word (the built-in English "love you" for three
  > syllables, "I love you" for four) runs on over the next Korean
  > character (pinned). The shipped Korean string table is not in the
  > repository, so its replacements' lengths are not checked: the header
  > gives the one-syllable one as a heart symbol, two bytes in CP949 but
  > three if the table goes through the same UTF-8 conversion as the
  > word lists.
  > `LoadFromFileCurse` has no caller. The new-character name checks
  > (`Execute_UI_NEW_CHARACTER`, `Execute_UI_NEWCHARACTER_CHECK`) call
  > `RemoveCurse` on both lists without `bForce`, so a player who turned
  > the filter off is stopped by neither (read, not reproduced).
  - Owner: `tests/arch/gamemodel_files.txt`, M0-M2, R1, the designated
    installer in `GameInit.cpp` and `tests/unit/test_chat_filter.cpp`.

- [x] **4.14 Status array and mode register:** `MStatus`,
  `TempInformation` and `AffectModifyInfo` compile in `gamemodel`.
  > **Status:** done (2026-09-29). `MStatus` is the array of
  > `MAX_MODIFY` values every ModifyInfo packet writes: `MCreature`
  > derives from it and overrides `SetStatus`, and `MPlayer` overrides it
  > again, both bounding the index before they write. `TempInformation`
  > is the mode register a dialog sets before it sends a request and the
  > reply handler reads (`g_pTempInformation`, used by 38 executable
  > translation units). Both implementations and headers move unchanged:
  > neither reaches an executable symbol, and `MStatus`'s vtable is
  > emitted in its own object, so the class moves whole. R1 falls by two
  > (449 to 447 Windows, 447 to 445 Ninja). Then `AffectModifyInfo`, the
  > function 28 handlers apply their packet's ModifyInfo through, left
  > `PacketFunction.cpp` for `Client/AffectModifyInfo.cpp` with its body
  > unchanged (its only reach was `DEBUG_ADD`, which `basic` provides);
  > `PacketFunction.h` includes its header, so no handler changed, and R1
  > did not move. `test_status_model.cpp` pins every slot, the 59 named
  > accessors, `ApplyStatus` (only slots that are not `MODIFY_NULL`,
  > through the virtual `SetStatus`) and every mode;
  > `test_affect_modify_info.cpp` reads ModifyInfo bodies from wire bytes
  > into a stand-in ModifyInfo, and into a `GCModifyInformation` and a
  > `GCOtherModifyInfo` from their factories, and pins both entry kinds
  > and widths, every in-range type, wire order (shorts first), draining
  > and every out-of-range type byte over them. Its sweep of all 29 packet
  > classes deriving from ModifyInfo, each from its factory, reads no
  > bytes: it adds one short (`MODIFY_CURRENT_HP`) and one long
  > (`MODIFY_GOLD`) entry and checks only that both apply and drain.
  > **Fixed test-first:** `TempInformation`'s constructor set only `Mode`,
  > and `GCPartyInviteHandler`'s `GC_PARTY_INVITE_ACCEPT` looks up
  > `PartyInviter` whether or not an invitation (`UI_RunPartyRequest`,
  > `UI_RunPartyAsk`) wrote it; the value slots, the inviter and `pValue`
  > now start at 0/NULL (the handler path was read, not reproduced).
  > **Known, not fixed:** the base `MStatus::SetStatus`/`GetStatus` index
  > the array unchecked, and `AffectModifyInfo` passes the wire type byte
  > (0-255; `MAX_MODIFY` is 73) on unchecked. Applied to a bare `MStatus`
  > a type past the array writes past it (a probe, not committed, aborted
  > under the `macos-asan` preset). Not reachable today: all 28
  > `AffectModifyInfo` calls pass `g_pPlayer` or a zone creature, both
  > bounded by their overrides (`GCThrowItemOK1Handler`'s direct
  > `SetStatus` with a wire index is inside a comment); every `GetStatus`
  > call names a `MODIFY_*` constant; `ApplyStatus` stays in range; the
  > only `new MStatus` is inside a comment in `Client.cpp`. The library
  > function now accepts any `MStatus*`, so a future caller with a bare
  > status would reach it. The tests pin only that `AffectModifyInfo`
  > passes every wire type byte on unchecked, so the bound must live in
  > `SetStatus`; they check against `BoundedStatus`, a test double that
  > copies the overrides' check. The real bounds in `MCreature::SetStatus`
  > and `MPlayer::SetStatus` are executable-side and untested: either
  > could lose its check with `unit_tests` still passing.
  - Owner: `tests/arch/gamemodel_files.txt`, M0-M2, R1,
    `test_status_model.cpp` and `test_affect_modify_info.cpp`.

- [x] **4.15 Model-reach packet handlers:** `GCPhoneConnectedHandler`,
  `GCPhoneDisconnectedHandler`, `GCPhoneSayHandler`, `GCRingHandler`,
  `GCTradeMoneyHandler`, `GCTradeRemoveItemHandler`,
  `GCSystemAvailabilitiesHandler` and `GCMonsterKillQuestInfoHandler`
  compile in `gamemodel`.
  > **Status:** moved (2026-09-30). The shrink survey at `deac9b56`
  > found these eight the only handlers that are link-clean and reach
  > only model state: the phone slots of `g_pUserInformation`, the trade
  > box (`g_pTradeManager`) and the wallet (`g_pMoneyManager`), the
  > server's switches (`g_pSystemAvailableManager`) and the quest goals
  > (`g_pQuestInfoManager`, reading `g_pCreatureTable`), every one
  > defined in `gamemodel`. The files stay in `Client/PacketHandler`, so
  > `check_packet_indices.pl`, which scans that directory, still reads
  > the seven phone-slot subscripts they hold, and
  > `PacketHandlerRegistry.cpp` binds them as before (the executable
  > links `gamemodel`). Six included `ClientDef.h`, which pulls in
  > `MPlayer.h`, `MZone.h` and `DebugInfo.h`, for nothing but the
  > `DEBUG_ADD` macros; they include `basic`'s `DebugLog.h` instead,
  > which is where `DebugInfo.h` takes those macros from, so the calls
  > expand as before. The other two move unchanged. No include rule changed:
  > the membership file, CMake's reader (`^Client/.*\.cpp$`), M0
  > (`^Client/`), M1 (the eight include only `Client_PCH.h`, the
  > `Gpackets` headers, listed model headers and `basic`), and R4's and
  > R10's readers (`Client/[A-Za-z0-9_/]+\.cpp`) all accept a file under
  > `Client/PacketHandler` already, and the executable's glob of that
  > directory drops what the membership lists, with the case-mismatch
  > check it has for every member. The exemption row for handler bodies
  > is amended to exclude the handlers the membership file lists. R1
  > falls by eight (445 to 437 Windows, 443 to 435 Ninja); R4 stays 0.
  > `tests/unit/test_model_handlers.cpp` then runs each handler on a
  > real packet: wire bytes built from the packet types' widths, read
  > into the packet the real factory creates (consumed exactly, at the
  > packet's own size), `execute()` with no `Player`, and the model
  > state checked. It pins the phone slots (each slot stored or cleared
  > with no neighbour touched, and every slot byte from 3 to 255
  > changing nothing; `GCPhoneSay` over every byte 0-255 with every
  > slot named, and on a slot with no name), the trade
  > money flow in both directions and on both sides with the OK
  > cancellation and accept delay, the other side's item removal, the
  > system switches (every switch the manager exposes; it has no getter
  > for the bits past them), skill limit and open degree (the wire's
  > degree less one, so 0 wraps to 255 and opens every zone), and the quest goals.
  > Three defects were fixed test-first. `GCMonsterKillQuestInfo`
  > assigned the creature table's name for the server's type to a
  > `std::string`; for a type past the table or a nameless row that name
  > is a NULL `MString`, and the client crashed in `strlen` (SEGV under
  > both presets); the quest now keeps an empty name.
  > `MMoneyManager::AddMoney`/`UseMoney` took the new balance in `int`,
  > so two amounts within the limit could overflow it (UBSan halted);
  > they now refuse a balance outside 0..limit first, and `CanUseMoney`
  > refuses a negative amount, as `CanAddMoney` does, although
  > `UseMoney` still accepts one that leaves the balance within
  > 0..limit (no production code calls `CanUseMoney`; only its tests
  > do). And
  > `GCTradeMoney` ran an amount past `INT_MAX`, which no wallet holds
  > (the server's `MAX_MONEY` is two billion), through its `int`, so
  > every move ran backwards; such a packet now changes nothing. The
  > same commit (`83fd754b`) also made the two result codes move the
  > wallet and the trade box all or nothing; review reverted that part.
  > It rested on the premise that the server never moves part of a
  > transfer. That
  > premise misread the server: `decideMoneyIncrease` and
  > `decideMoneyDecrease` (its `trade/TradeTableDecision.cpp`) reject a
  > request before anything moves, and otherwise set the server's wallet
  > and stake and only then send the result. A result therefore reports
  > a committed move, and the server trims against the receiving
  > player's purse plus the stake, not against the sender's box. When the client's wallet cannot follow, it
  > already disagrees with the server's, and holding the box back made
  > the box wrong too. `e9f1fe87` applies each side on its own and logs
  > the side that cannot follow, which is the effect each side had
  > before `83fd754b`, plus the log; its tests assert the server's side.
  > Whether the wallets disagree in play is not established; the trade
  > cases were found by reading and reproduced only in the test binary.
  > `check_packet_indices.pl` still counts 103 subscripts, 12 raw: the seven phone-slot ones are
  > guarded and now tested, but the checker reads the subscript, not the
  > guard, and the files did not leave the directory it scans.
  > A fourth fix could not be test-first. `GCPhoneSay` formatted a
  > slot that never connected, or was hung up, with `%s` over its NULL
  > name. That is undefined, but every C library the project builds
  > against prints "(null)", and the line goes nowhere, since its chat
  > call is commented out. `782432c0` passes "" instead. Its test runs
  > both empty-slot paths in every build, but it passed on the unfixed
  > code too.
  > **Known, not fixed:** the other side's `INCREASE`/`DECREASE`
  > still apply one box's refusal silently; no money is moved between
  > two wallets there. The money fix's commit (`fdc1d7e4`) named a
  > `UseMoney`/`AddMoney` pair in `UIMessageManager.cpp` as the same
  > pattern left out of scope; that pair is inside a `/* */` block and
  > never compiled. And the tests' commit (`b8e9258e`) announced two
  > fixes to follow; four did: the wallet overflow was found while
  > writing the trade money's tests, and the phone line's in review.
  > `83fd754b`'s subject and message say the all-or-nothing move is
  > what the server does; it is not (see above).
  > And `b8e9258e`'s message says the system test shows that bits past
  > the last switch are dropped; nothing in it can observe that.
  > These notes stand in for rewording the messages: the branch's
  > commits are not rewritten.
  - Owner: `tests/arch/gamemodel_files.txt`, M0-M2, R1, the
    exemption row, `test_model_handlers.cpp` and `test_money_manager.cpp`.

- [x] **4.16 The skill-info rebuild:** `GCSkillInfoHandler`'s decision
  compiles in `gamemodel` as `ApplySkillInfo`, with the two duration
  conversions it reads.
  > **Status:** extracted (2026-09-30). `GCSkillInfoHandler` rebuilds the
  > player's skill model from the server's per-race domain lists: 55
  > branches over the domains (`g_pSkillManager`), the per-skill state in
  > `g_pSkillInfoTable` and five skill flags of `g_pUserInformation`, all
  > in `gamemodel`. The shrink survey at `deac9b56` measured its reach
  > into the executable as three symbols: `ConvertDurationToMillisecond`,
  > a pure conversion in `PacketFunction.cpp`, and
  > `MSkillSet::SetAvailableSkills` and `g_abSweeperBonusSkills`, both in
  > `MSkillAvailable.cpp`, the executable half of the skill core. The
  > pattern is shared-rules slice 4's for Will of Life
  > (`WillOfLifeOutput`): a `gamemodel` function holds the decision, the
  > caller keeps what reaches the executable. `Client/ApplySkillInfo.cpp`
  > takes the real `GCSkillInfo` and holds the body from the five flag
  > resets through the domain loop, unchanged but for its Korean
  > comments, which are translated; the handler calls it, then clears
  > the sweeper bonus skills and calls `SetAvailableSkills`, in the order
  > it did before, and no longer includes `ClientDef.h` or
  > `UserInformation.h`, which only the moved body used.
  > `ConvertDurationToMillisecond` and `ConvertDurationToFrame` (which
  > reads `g_pClientConfig->FPS`, also `gamemodel`'s) move unchanged into
  > `Client/ConvertDuration.cpp`; `ClientDef.h` includes their header in
  > place of its two declarations, so their callers compile unchanged:
  > 42 live calls in 26 files (24 handlers, `PacketFunction.cpp` and the
  > six in the moved body; another 19 mentions are in comments), and
  > `ClientDef.h` was the only declaration any of them saw.
  > `ConvertMillisecondToFrame`, their neighbour, reads the same frame
  > rate and was left in `PacketFunction.cpp`: nothing in this task
  > calls it. No include rule changed: M1 accepts the four new files
  > (they include `Client_PCH.h`, the `Gpackets` header, listed model
  > headers and `basic`'s `Platform.h`), and the checker reads 77
  > members. R1 does not move (435 Ninja, measured): the handler and
  > `PacketFunction.cpp` stay in the executable. What this buys is
  > testability, not a smaller executable.
  > `tests/unit/test_skill_info.cpp` then runs `ApplySkillInfo` on
  > `GCSkillInfo` packets read from wire bytes into the packet the real
  > factory creates (consumed exactly, at the packet's own size): bytes
  > built from the packet types' widths as the server's `Slayer.cpp`,
  > `Vampire.cpp` and `Ousters.cpp` write them, and the server's five
  > goldens for the packet (its `origin/master` `7f833cef`, embedded as
  > hex; no golden file was added here). Its fixture builds the info
  > table, the domains and the usable-skill set over a small tree per
  > domain, with a clock host so the delays can be read. It pins, per
  > race, which skills each domain learns and offers next, the skills
  > added to `g_pSkillAvailable`, the table's exp level, exp, reuse and
  > remaining delay and enable, each domain's new-skill flag, the five
  > user flags (Hallucination's case is commented out, so its flag stays
  > false), the bomb and mine levels Throw Bomb and Install Mine pass
  > on, the ETC-step skill tried in every domain (seven for a slayer or
  > vampire, eight for an ousters), and the domain levels, which the
  > packet does not carry and which survive it. The hostile values: a
  > slayer domain past the domain table (8, 0x91, 0xB2, 0xFF) learns
  > nothing, since the table's lookup answers nothing there, but still
  > sets the skill's table entry and flag; exp levels up to 0xFFFF are
  > stored as sent; a duplicate skill is learned once and its last
  > state wins; empty lists learn nothing; a race byte past the three is
  > refused by the read (`InvalidProtocolException`), and a packet built
  > with one has its entries dropped unapplied.
  > **Fixed test-first:** a skill type at or past the info table's size
  > (`MIN_RESULT_ACTIONINFO`, 512, as constructed, until `LoadFromFile`
  > resizes it to the count its data file declares; the fixture loads no
  > file, so its table has 512 rows) was taken for a learn: the table
  > answered its empty entry, the domain refused a skill it does not
  > hold, and the new-skill flag the rebuild had set for the learn
  > stayed up, so the skill window offered that domain's next skill;
  > from 2048 the type is also past `ACTIONINFO`'s range of values
  > (`MAX_ACTIONINFO` is 1191), and its load in `LearnSkill` stopped the
  > `macos-asan` run. Such an entry is now skipped whole (`4e9b593d`).
  > The server's skill types end at its `SKILL_MAX`, 397, so given a
  > skill info file of at least 397 rows a live server does not send
  > one; its goldens do. That condition is not checked.
  > `LoadFromFileServerSkillInfo`, which runs after the resize, indexes
  > the table by its own file's skill types unchecked, so an install
  > that loads without writing past the table covers every type that
  > file lists; whether it lists every type up to 396 is not
  > established, and the repository holds neither file. With a shorter
  > file, types from its size to 396 are now skipped where before they
  > were tried, refused and left the domain's new-skill flag up; the
  > skip also passes over the race's flag switch, so for a file of
  > fewer than 185 rows the Restore (113), Ground Attack (179), Bloody
  > Warp (183) and Bloody Snake (184) flags those types used to set
  > from the wire are no longer set. The bomb and mine exp levels a
  > learned throw or install passes on were already no-ops at such
  > sizes (`BOMB_*` 413-417, `MINE_*` 419-423, past the table).
  > Such an install cannot load cleanly anyway: the same
  > `LoadFromFileServerSkillInfo` then writes fixed rows up to
  > `BOMB_TWISTER` (418) without a check, so any table under 419 rows
  > is overrun at every start-up, and the short-file effects above
  > only matter to an install that already corrupts memory at load.
  > `4e9b593d`'s message says the table holds 512 rows and that nothing
  > the server sends changes; both hold only under that condition. And
  > `ConvertDurationToMillisecond` multiplied in `int`: a wire turn (a
  > `DWORD`) past `INT_MAX` arrives negative, and any duration past
  > about 25 days overflowed (UBSan stopped the `macos-asan` run;
  > the plain builds wrapped). It now multiplies as a `DWORD`, the value
  > every build computed before (`27740e9f`); a remaining delay of
  > `INT_MAX` milliseconds or more still goes on through
  > `SetAvailableTime`'s `int` as negative, and the skill reads as
  > usable at once (pinned).
  > **Known, not fixed:** a learn the domain refuses leaves the
  > new-skill flag the rebuild set for it. An ETC-step skill (Soul
  > Chain, the one the three races share) is tried in every domain, so
  > every domain whose tree lacks it is left offering a new skill; in
  > play that depends on the trees `SkillInfo.inf` builds, which the
  > repository does not hold, so whether it shows is not established. A
  > duplicate does the same; the server sends each skill once (its slot
  > map is keyed by skill type). The rebuild also never clears a flag
  > an earlier packet set; only a learn does. Both are pinned in the
  > tests as today's behaviour. A slayer entry may name the vampire or
  > ousters domain and learns there; the server sends a slayer's domains
  > 0 to 5 only. The exp level is stored unchecked; nothing in the
  > rebuild indexes by it, and its readers in the executable and `VS_UI`
  > were not audited for a level past 100. `ApplySkillInfo` casts each
  > entry to the class the packet's race names; the read creates the
  > entries from that race, so only a packet built in code can disagree.
  > `ConvertDurationToFrame` still multiplies in `int` by the frame
  > rate; the rebuild does not call it, and of its callers only the two
  > that pass the player's `MODIFY_DURATION` status (a wire long value)
  > can reach past `INT_MAX / FPS`; the others pass `WORD` durations,
  > an effect's `WORD` times 10, or constants.
  > `MSkillInfoTable::LoadFromFileServerSkillInfo` indexes the table by
  > the file's skill type unchecked (a shipped data file, not the wire).
  > The first fix's subject line is 74 characters, two past the limit;
  > the commits are not rewritten.
  - Owner: `tests/arch/gamemodel_files.txt`, M0-M2,
    `Client/ApplySkillInfo.h`'s contract and `test_skill_info.cpp`.

- [x] **4.17 Event queue:** `MEvent` and `MEventQueue` compile in `gamemodel`;
  the executable's `MEventManager` inherits the queue and retains its images.
  > **Status:** done (2026-10-02). Each queue borrows an `MEventHost` for
  > gamma, player-effect queries and fade requests; the executable adapter
  > installs a designated, static host in its constructor. The existing
  > `MonotonicClock` seam drives timing tests. Missing callbacks skip rendering
  > and retain effect events until their explicit expiry; the adapter uses
  > the same lifetime rule when there is no player. Strict expiry, any-bit
  > flag matching, ID ordering and gamma refresh behavior are preserved.
  > The image cache still clears after the events at destruction. R1 stays
  > 437 Windows / 435 Ninja: the executable's image/host translation unit
  > remains, while the queue implementation is compiled only in the library.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_event_queue.cpp`, including compile-time checks that
    the screen manager uses the tested library methods.

- [x] **4.18 Screen-fade progression:** `MScreenFade` compiles in `gamemodel`;
  `MTopView` supplies the current frame and draws the current value before
  advancing it.
  > **Status:** done (2026-10-02). The model owns direction, value, endpoints,
  > delay and the strict 80-frame cutscene hold. Zero-step death shading,
  > logic-tick gating, one delayed step after skipped frames, wrapping frame
  > subtraction and the delay phase retained across restarts are preserved.
  > Fresh state is explicitly idle, and the frame stamp belongs to the view
  > instead of a function static shared by every view. Its first active,
  > unsuppressed draw starts that clock. Surface operations and event-based
  > suppression stay in `MTopView`; R1 remains 437 Windows / 435 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, `test_screen_fade.cpp`
    against the production library, and `screen_fade_signed_tests` /
    `screen_fade_unsigned_tests`, which rebuild the same implementation
    under both char modes. Both native CI verification scripts require
    those tests to be registered.

- [x] **4.19 Login server selection:** `CServerInformation`, its world/server
  records, owning `CTypeMap2` template and the login-list updates belong to
  `gamemodel`.
  > **Status:** done (2026-10-02). The model moved unchanged before a separate
  > test-first fix initialized every selection field and cleared server status
  > on release. `ApplyWorldList` replaces worlds and their servers;
  > `ApplyServerList` replaces the selected world's server snapshot (task 4.86).
  > Both consume packet records and preserve the requested-ID and
  > first-row fallback rules, including duplicate and zero IDs. A missing
  > selected world leaves the server packet untouched. The complete handlers
  > now join the library (task 4.84), with UI refreshes and mode transitions
  > supplied by a designated host; an empty accepted list still reaches the UI.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_server_information.cpp` (including fresh state over nonzero
    storage), and `tests/unit/test_server_lists.cpp` over factory-created packets
    read from wire bytes. Both link the same library objects as the executable.

- [x] **4.20 Callback dispatch and request modes:** `MFunctionManager` and
  `MRequestMode` compile in `gamemodel` with unchanged sources and headers.
  > **Status:** done (2026-10-02). Status reactions and keyboard accelerators
  > inherit the same callback table; registration, bounds, synchronous payload
  > forwarding and table lifetime can be tested independently of their game/UI
  > callbacks. The player's and view's trade, party and information request
  > state likewise needs no rendering or creature objects. Callback bodies,
  > request targeting and live input handling remain with their callers.
  > R1 is 434 Windows / 432 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`,
    `tests/unit/test_function_manager.cpp` (including compile-time checks of
    both real dispatch callers and callbacks that release or replace the table),
    and `tests/unit/test_request_mode.cpp`, all linking the production library.

- [x] **4.21 Effect math:** `MathTable` compiles in `gamemodel`; the initial move
  preserved source and header bytes before the separate endpoint fix.
  > **Status:** done (2026-10-02). Fixed-point sine/cosine, target
  > angles, turn direction and one-turn clipping are reachable without the
  > effect classes or a live view. The legacy `FCreateSines` startup contract
  > still requires one call because it scales the atan table in place. Tests
  > initialize once and use the atan lookup's valid table indices. Initialization
  > scales both atan endpoints, correcting the northwest and southeast
  > diagonal targets; the regression tests reproduced the missing endpoint
  > and the 22.5-degree error before the fix. Target-angle differences, sign
  > comparisons and ratio products now use 64-bit arithmetic across the full
  > integer coordinate span, retaining lookup quantization. The other angle
  > helpers keep their existing ranges. R1 at the initial move was 433 Windows /
  > 431 Ninja; the later arithmetic fix does not change membership.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_math_table.cpp`, linked against the production library.

- [x] **4.22 Server-info parser:** `ServerInfoFileParser` compiles in
  `gamemodel` after removing its unused `MinTr.h` include; the initial move
  preserves every method body and the header.
  > **Status:** done (2026-10-02). Dimension sections, key matching, values,
  > integer conversion and reopening configuration files can be tested through
  > the same implementation used by non-Korean login and reconnect. Socket
  > setup and configuration ownership remain in their executable callers.
  > The follow-up reader fix terminates on failed opens/reads, consumes long
  > lines without truncation and normalizes CRLF on every host. Space removal
  > is linear for the now-unbounded lines. Dimension/key rules and the legacy
  > integer conversion remain unchanged; required-property and integer-range
  > validation are outside this parser.
  > R1 is 432 Windows / 430 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_server_info_parser.cpp`, linked against the real library.

- [x] **4.23 Weather simulation:** `MWeather` compiles in `gamemodel` behind
  `MWeatherHost`; `GameInit` installs the player's pixel origin and live viewport
  readers using designated initializers and clears the host at shutdown.
  > **Status:** done (2026-10-02). Particle movement, rain/snow landing phases,
  > spawning, density progression and stopping are reachable without a player
  > or surface. The extraction removes unused debug/global declarations and
  > replaces direct player/viewport reads. A missing player skips a new weather
  > start; absent or invalid dimensions use the existing 800-by-600 defaults.
  > Sound, shadows, weather selection and drawing stay executable-side.
  > Follow-up tests reproduce uninitialized particles/origins, lost active
  > counts on resize, premature landing phases, advancing unpublished slots
  > while stopping, the BYTE density wrap and unsigned-char motion. Particles
  > now start initialized in a distinct new phase; only published slots are
  > generated/advanced, resizing preserves survivors and the ramp clamps before
  > narrowing. Signed velocities work under either plain-char ABI.
  > R1 is 431 Windows / 429 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_weather.cpp`, linked against the production library, plus
    `weather_signed_tests` / `weather_unsigned_tests` rebuilding that same
    implementation under both char modes.

- [x] **4.24 Login endpoint selection:** `SelectLoginEndpoint` in `gamemodel`
  owns address rotation, configured/randomized ports and launcher/environment
  override precedence for both real configuration readers.
  > **Status:** done (2026-10-02). `GameInit` supplies the current attempt and
  > owned override values. The library draws randomness only when choosing a
  > configured range; tests supply a deterministic draw. DNS, sockets, advancing
  > attempts after connection failure and UI transitions remain executable-side.
  > The initial extraction preserves the selection rules. The test-first
  > follow-up validates complete decimal numbers and entire port ranges before
  > modulo/addition, uses full-width retry indices and address suffixes, and
  > applies override precedence before reading unused configuration. Missing
  > optional counts/bases now share the same defaults across both readers.
  > Effective malformed values report `ConnectException`; ports stay in
  > 1–65535 and owned overrides reject embedded NUL bytes. R1 remains
  > 431 Windows / 429 Ninja because `GameInit.cpp` still owns startup.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_login_endpoint.cpp`, linking both configuration readers
    and the production selection implementation.

- [x] **4.25 Self-defense target membership:** `MJusticeAttackManager` and
  `GCAddInjuriousCreatureHandler` / `GCRemoveInjuriousCreatureHandler` compile
  in `gamemodel`, with the existing registry bindings in the executable.
  > **Status:** done (2026-10-02). The server-controlled name set is testable
  > without a frame clock, player or view. Unobserved per-name timestamps and
  > an unused config include are removed; the remove handler includes the
  > library logger directly. The manager still owns its global, and the
  > executable retains allocation, reset and teardown. Null names from an
  > unnamed creature are ignored and never match an entry; empty-string
  > behavior is preserved. R1 is 428 Windows / 426 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_justice_attack.cpp`, linking the production manager and
    running both handlers on packets read through their real factories.

- [x] **4.26 Per-world character-selection settings:** `PCConfigTable` compiles
  in `gamemodel` so the saved slot, recent-account ordering and version-2 file
  format can be tested through the same implementation as the client.
  > **Status:** done (2026-10-02). The initial move preserves the implementation,
  > header and owned global. The library now validates record lengths, counts,
  > names and slots, publishes only complete tables, and validates saved data
  > before truncating an existing file. Recency saturates instead of wrapping;
  > repeated ownership transfers at the same key are harmless. `GameMain` still
  > selects the world/account, applies the remembered slot to the UI and chooses
  > the settings file. The version-2 layout and twenty-account save limit are
  > preserved. R1 is 427 Windows / 425 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_player_config.cpp`, linked against the production library.

- [x] **4.27 Creature naming:** `MonsterNameTable` and `MLevelNameTable`
  compile in `gamemodel`, with per-creature title/hallucination selection in
  `CreatureNameSelection`.
  > **Status:** done (2026-10-02). Both existing table implementations, headers
  > and globals move unchanged. `MCreature` delegates its selected indexes,
  > lookups and operator-prefix exception to the library, supplying each random
  > draw and the live tables. Empty tables and negative samples select index
  > zero safely; hallucination indexes retain the tables' full int range.
  > Missing or empty operator prefixes keep names masked, and unnamed creatures
  > or missing alias tables are handled without dereferencing null. Actual
  > creature-name ownership, drawing and chat presentation remain executable-side.
  > R1 is 425 Windows / 423 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_creature_names.cpp`, using the production tables and
    selection state.

- [x] **4.28 War state:** `MWarManager` compiles in `gamemodel`, keeping
  war records transferred by packets, castle lookup aliases and display-row deadlines
  testable without the main executable.
  > **Status:** done (2026-10-02).
  > `MWarHost` supplies the current zone and race-war UI notifications;
  > `GameInit` retains dialogs, chat and skill refresh. Missing actions are
  > skipped, and a missing current zone skips level-war presentation. The
  > manager consumes incoming records, releases shared records only after their
  > last zone is removed or replaced, and releases level/empty/rejected records.
  > Repeated updates refresh one display row per zone and war type; removals
  > clear all matching rows. Missing user information skips row publication,
  > and missing zone metadata gives an empty row name. Castle lookup aliases
  > and existing-row metadata are preserved. R1 is 424 Windows / 422 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_war_manager.cpp`, using the real manager, packet records,
    user information and zone table.

- [x] **4.29 NPC shop stock:** `MShopTemplateTable` and `BuildNPCShopShelf`
  compile in `gamemodel`, making the fixed/mysterious shelf build used by
  shop packets testable outside the executable.
  > **Status:** done (2026-10-02).
  > The existing template implementation and header initially move unchanged.
  > `MNPC` retains shop creation and supplies item factories, live gender and
  > portal setters through `NPCShopHost`. The library builds shelves, selects
  > template ranges, initializes items and selects default portal destinations.
  > Missing callbacks create no items, use nonfemale selection, or skip portal
  > writes. Stock stops at shelf capacity; rejected or unfinished items are
  > released. Female pair selection stays within the template range, and invalid
  > classes or absent tables produce no stock. Template reloads clear old rows
  > and publish only complete input; duplicates keep the first row and the
  > eleven-byte row layout is preserved. Saves reject null rows before writing
  > to the supplied stream. R1 is 423 Windows / 421 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_npc_shop.cpp`, using the production builder, template table,
    shelves and item model with instrumented executable-service callbacks.

- [x] **4.30 NPC dialogue:** `MNPCScriptTable` and its generated English
  overlay compile in `gamemodel`, making dialogue lookup, parameter replacement
  and file loading testable outside the executable.
  > **Status:** done (2026-10-02).
  > After removing unused executable includes separately, all four files move
  > unchanged. Ask, variable-ask and say handlers retain UI and creature actions;
  > game initialization retains language selection. The generated English
  > overlay is still owned by `tools/i18n/npcscript.en.tsv` and its generator.
  > Missing text clears substitution output; null parameters keep their marker.
  > Keys retain map-order cascading but do not rescan their own inserted text.
  > Table reloads clear prior rows and publish only complete input; string
  > counts are bounded by remaining bytes. The table's own count only caps its
  > rows, and a table also ends where fewer bytes remain than a row header:
  > the shipped `NPCScript.inf` declares more scripts than it holds and ends
  > in a 4-byte remnant, and treating that tail as truncation discarded every
  > NPC's dialogue (fixed 2026-10-12). Saves validate every row's encoded
  > strings before writing to the supplied stream. Binary layout and duplicate-key
  > precedence are preserved; owning rows and tables cannot be shallow copied.
  > R1 is 421 Windows / 419 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_npc_dialogue.cpp`, using the production table, overlay,
    resource-string codec and packet parameters.

- [x] **4.31 Deferred action results and effect targets:** result-queue
  ownership and `MEffectTarget` state compile in `gamemodel`.
  > **Status:** done (2026-10-02).
  > The owning `MActionResult` methods are split unchanged into
  > `MActionResultQueue.cpp`; concrete node actions stay executable-side.
  > `MEffectTargetHost` supplies removal from the player's non-owning roster;
  > missing services skip removal. Portal names use the library's real zone
  > table. Queued and active nodes have unique ownership through recursive
  > execution, destruction and exceptions. Duplicate pointers are ignored and
  > owning queues cannot be shallow copied. Targets initialize coordinates and
  > object IDs, stop at their final phase and update portal base state without
  > constructing a temporary that unregisters the target. Result replacement
  > publishes new ownership before callbacks; destruction clears the result
  > and discards any new result offered by callbacks. R1 is 420 Windows / 418 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_effect_results.cpp`, using the production queue and targets,
    instrumented action nodes and the real zone table.

- [x] **4.32 Zone-info loading:** portal records and `ZoneInfoData` parsing
  compile in `gamemodel`.
  > **Status:** done (2026-10-02).
  > `MPortal` moves unchanged. `GameMain::LoadZoneInfo` consumes `ZoneInfoData`;
  > zone-sector updates, minimap publication and horn NPC creation stay in the
  > executable. Record loads preserve the previous portal on failure; saves
  > reject counts that the format cannot represent before writing. Only
  > `TYPE_MULTI_PORTAL` stores a count, including zero; all other type bytes
  > carry one destination. Complete-file loads validate positive matching
  > dimensions, every read, remaining bytes and resource budgets (64 MiB from
  > the current position, 65,536 records per table), then publish atomically.
  > Failed loads preserve prior data. Opaque flags/types, empty multi-portals,
  > rectangle coordinates and an unread legacy trailer are preserved. The
  > live caller skips publication on failure and initializes each minimap
  > rectangle even for a portal without destinations. R1 is 419 Windows / 417 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_zone_info.cpp`, using production portal records and the
    complete-file reader.

- [x] **4.33 Background-music metadata and selection:** `MMusicTable` and
  `SelectZoneMusic` compile in `gamemodel`.
  > **Status:** done (2026-10-02).
  > The table files move unchanged. `GameMain::PlayMusicCurrentZone` supplies
  > the active zone ID, game hour, Holy Land flag, wave-music preference and
  > war state. Two-hour rotation, named Holy Land tracks, war fallback and
  > lair overrides remain; playback and event/mode suppression stay in the
  > executable. A missing clock supplies hour zero and a missing war manager
  > supplies no war. R1 is 418 Windows / 416 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_music_selection.cpp`, using the production selector and
    music table with real resource-string encoding.

- [x] **4.34 Delayed sound scheduling:** sound records, thunder selection
  and `DelayedSoundQueue` compile in `gamemodel`.
  > **Status:** done (2026-10-02).
  > `SetLightning` supplies the player position and frame timestamp.
  > `MZone` owns queued records as values and supplies playback during its
  > update, keeping ambient sounds and propeller logic. The one-second
  > thunder threshold, insertion order and strict deadline comparison are
  > preserved; absent playback still consumes ready sounds. Each ready record
  > is consumed before playback and remains readable through callbacks that
  > append, clear, recursively update or throw. Callbacks must keep the queue
  > alive. Normal dispatch scans the list once; callback mutations restart
  > traversal. R1 is 417
  > Windows / 415 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_delayed_sounds.cpp`, using real records and queue dispatch.

- [x] **4.35 Ambient sound scheduling:** `AmbientSoundState` makes propeller
  and random-sound decisions in `gamemodel`.
  > **Status:** done (this commit).
  > `GameMain` keeps the shared state and its mode/zone resets; `MZone`
  > supplies the active zone and player position and applies stop before play.
  > Missing zone metadata or position skips random playback but advances its
  > timer. Empty sound lists retain the null-ID request and coordinate draws.
  > The scheduler uses the existing random sequence, strict deadlines,
  > 10–14 second initial delay and 6–15 second recurring interval. Random
  > position offsets saturate at the int limits. R1 stays 417 Windows / 415 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_ambient_sounds.cpp`, using the production scheduler and
    zone records with explicit random draws.

- [x] **4.36 Show-time scheduling:** `ShowTimeChecker`'s hour and deadline
  decisions join its existing data methods in `gamemodel`.
  > **Status:** done (this commit).
  > The zone-sound manager supplies the frame timestamp and optional game
  > hour and retains audio playback. Hour windows are inclusive, may cross
  > midnight and do not normalize stored bytes. Missing game time suppresses
  > all playback, including loops. Equal delay bounds retain the immediate
  > deadline without drawing randomness; other bounds use the original DWORD
  > arithmetic and an exclusive upper bound. R1 is 416 Windows / 414 Ninja.
  > The manager's only construction site is commented out in `GameMain`, so
  > this retained scheduler is currently inactive; its extraction does not
  > activate zone sounds.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_show_time.cpp`, using the production scheduling methods.

- [x] **4.37 Help-string display state:** `MHelpStringTable` joins `gamemodel`.
  > **Status:** done (this commit).
  > `GameInitInfo` retains loading and the help displayer retains message
  > presentation. The table owns help text and the flags set by its two
  > lookup methods. Reloads publish text and fresh display history together;
  > failed input preserves both. Empty loads and release leave no displayed
  > entries. Copying the owning table is disabled. R1 is 415 Windows / 413 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_help_strings.cpp`, using the production table and files.

- [x] **4.38 Interaction-object metadata:** `MInteractionObjectTable` joins
  `gamemodel` with the existing record and table interfaces.
  > **Status:** done (this commit).
  > `GameInitInfo` retains loading the metadata file. Records initialize all
  > fields and publish only complete reads. Their eleven-byte little-endian
  > format keeps the legacy four-byte sound slot: the low word is the ID and
  > the upper word, historically object padding, is ignored on read and zero
  > on save. Generic table reload semantics remain. R1 is 414 Windows / 412 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_interaction_metadata.cpp`, using production records.

- [x] **4.39 Orbit-effect movement:** `EffectOrbit` holds cached paths and
  progression in `gamemodel`.
  > **Status:** done (this commit).
  > `MAttachOrbitEffect` supplies effect activity and applies pixel offsets;
  > the generator keeps creature lookup, step inheritance and pause selection.
  > `GameInit` initializes the cached positions after the shared math tables.
  > The first two paths repeat twice per 64 steps, and the small path once.
  > Signed steps normalize into that cycle; unknown types have zero offset.
  > Only the constructor's -1 step requests randomness. R1 stays 414 Windows /
  > 412 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_effect_orbits.cpp`, using production paths and state.

- [x] **4.40 Linear-effect movement:** `LinearEffectMotion` holds target,
  velocity and path length in `gamemodel`.
  > **Status:** done (this commit).
  > `MLinearEffect`, `MGuidanceEffect` and `MChaseEffect` share its advance and
  > arrival operation. The executable supplies the current position and arrival
  > distance, and keeps target lookup, direction, lifetime, animation and light.
  > Arrival uses strict per-axis distances after moving, then snaps all axes and
  > clears velocity. Zero speed retains the existing non-arrival behavior.
  > Curved subclasses retain access to the trajectory state. R1 stays 414
  > Windows / 412 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_linear_effect_motion.cpp`, using production motion state.

- [x] **4.41 Homing-effect steering:** `HomingEffectSteering` holds turn state
  and calculates horizontal displacement in `gamemodel`.
  > **Status:** done (this commit).
  > `MHomingEffect` supplies integer coordinates and speed, and applies the
  > returned displacement. Target lookup, height, arrival, lifetime and drawing
  > remain in the executable. Halo attacks continue without retargeting.
  > MathTable initialization remains in GameInit. Legacy turning behavior,
  > including alignment clearing the turn magnitude, is retained. Horizontal
  > fixed-point products use wide arithmetic for the full WORD speed range.
  > R1 stays 414 Windows / 412 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_homing_steering.cpp`, using production steering and math.

- [x] **4.42 Parabolic-effect movement:** `ParabolaEffectMotion` holds arc
  progression and applies movement and arrival over `LinearEffectMotion`.
  > **Status:** done (this commit).
  > `MParabolaEffect` supplies the current position and speed, emits cannonade
  > smoke after advancing but before snapping, and retains impact actions,
  > lifetime and animation. Arrival requires strict XY proximity after half a
  > turn, or falling below the target height; it snaps every axis and clears
  > linear velocity. Zero speed and integer arc-step truncation retain their
  > existing behavior. Arc phase stays within one turn while a separate flag
  > keeps the arrival gate open. Fixed-point products use wide arithmetic;
  > arc-step calculation bounds the quotient before integer conversion.
  > R1 stays 414 Windows / 412 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`,
    `tests/unit/test_parabola_motion.cpp` and `tests/unit/test_parabola_step.cpp`,
    using production motion and shared math tables.

- [x] **4.43 Effect timing:** `EffectTiming` holds lifetime, link, draw-delay
  and wait deadlines in `gamemodel`, with explicit current-frame inputs.
  > **Status:** done (this commit).
  > `MEffect` supplies its host frame to timing methods; derived effects keep
  > their existing deadline updates. `MAttachEffect` selects the attached
  > duration convention: only 0xFFFF becomes the maximum DWORD deadline.
  > Count deadlines retain the minus-one offset; delay/wait deadlines do not.
  > Absolute unsigned comparisons and DWORD wrap remain. All deadlines start
  > at zero, including wait state; callers explicitly schedule a wait.
  > R1 stays 414 Windows / 412 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_effect_timing.cpp`, using production timing state.

- [x] **4.44 Base effect state:** `MEffect` compiles in `gamemodel`, including
  animation progression, position projection, metadata and target ownership.
  > **Status:** done (this commit).
  > `MEffectHost` supplies the live frame clock and frame lighting. GameInit
  > installs and clears the host; each call re-reads it. Missing lighting is
  > zero; a missing clock means expired lifetime and no delay/wait. Coordinate
  > projection keeps the same tile arithmetic and unsigned sector conversion.
  > The effect viewer links the production object and supplies its existing
  > frozen clock, replacing game-global stubs; its vptr sanitizer suppression
  > is removed. Rendering and concrete effect actions remain executable-side.
  > Both constructors initialize metadata; relinking the owned target updates
  > its action without deleting it. Copy operations are disabled because an
  > effect owns its target. R1 is 413 Windows / 411 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_effect_base.cpp`, using production effects and targets.

- [x] **4.45 Moving and screen effects:** `MMovingEffect` and `MScreenEffect`
  compile in `gamemodel` using the base effect's clock and lighting host.
  > **Status:** done (this commit).
  > Both stop before changing state when expired and refresh lighting only
  > for alpha effects. Moving updates project pixels back onto sectors;
  > screen effects keep offsets relative to the shared screen basis without
  > changing sector coordinates. The host uses the view's existing bounds
  > checks for missing frame lighting. Screen subtraction and projection use
  > wide arithmetic; the final coordinate saturates at int bounds. Fractional
  > offsets still truncate before adding the basis; NaN uses the basis and
  > infinities saturate. Rendering stays executable-side.
  > R1 is 411 Windows / 409 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_moving_screen_effects.cpp`, using production effects.

- [x] **4.46 Draw-skipping effects:** `MSkipEffect` compiles in `gamemodel`
  using the base effect's guarded deadline query and lighting callback.
  > **Status:** done (this commit).
  > Active updates consume one value from the existing C random stream,
  > choose whether to draw, advance animation and refresh alpha lighting.
  > Nonpositive skip intervals become one, keeping every frame visible. The
  > early cutoff cannot underflow for unscheduled or short-lived effects.
  > The deadline remains four frames before the stored end frame; position
  > and link deadlines do not control progression. Missing clocks stop
  > updates; the lighting callback retains the view's bounds checks.
  > R1 is 410 Windows / 408 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_skip_effect.cpp`, using production skip effects.

- [x] **4.47 Facing direction:** `SelectFacingDirection` holds the view's
  eight-way slope classification in `gamemodel`.
  > **Status:** done (this commit).
  > `MTopView` delegates its existing static entry point; linear projectiles
  > call the model function directly. Coincident points face down, and the
  > existing asymmetric low/high slope cutoffs retain their float rounding.
  > Coordinate differences use wide integers before float conversion so a
  > long displacement cannot overflow into the wrong direction.
  > Rendering, creature actions and projectile lifetime stay executable-side.
  > R1 stays 410 Windows / 408 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt` and
    `tests/unit/test_direction_selection.cpp`, using production selection.

- [x] **4.48 Linear-effect lifecycle:** `MLinearEffect` compiles in
  `gamemodel` over the existing motion and facing models.
  > **Status:** done (this commit).
  > Expiry is checked before movement. Ordinary arrival snaps to the target
  > and stops before sector projection or animation; HALO arrival retains
  > those updates and caps lifetime at the current frame plus eight without
  > extending an earlier deadline. Link deadlines stay independent. Clock
  > and lighting come from the base host; alpha light uses the view's existing
  > bounds checks. Absolute unsigned clock comparisons and wrap remain.
  > Base pixel getters and sector projection saturate at integer bounds;
  > NaN projects to zero. Target facing uses the same bounded base getters,
  > preserving stored-position semantics when display getters are overridden.
  > Float motion state and ordinary truncation remain unchanged.
  > R1 is 409 Windows / 407 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1,
    `tests/unit/test_linear_effect.cpp` and `tests/unit/test_effect_base.cpp`,
    using production linear and base effects.

- [x] **4.49 Creature guidance and chase:** `MGuidanceEffect` and
  `MChaseEffect` compile in `gamemodel`. A borrowed `MGuidanceEffectHost`
  returns creature pixel X/Y and height; GameInit installs the live-zone
  adapter and clears it at shutdown. Missing hosts, entries or creatures
  clear the trace id and end deadline. Readers are private, guarded and
  re-read each time; no creature pointer crosses the seam.
  > Setting a non-null id traces immediately. Guidance checks expiry before
  > retracing and stops on arrival before projection, animation and light.
  > Chase ignores lifetime, traces only non-null ids and animates before
  > moving. Arrival marks it complete for the helicopter manager but keeps
  > it active, skipping projection/light until movement resumes. Clearing
  > its id retains the trajectory for an explicit departure destination.
  > Both retain strict arrival distance, zero-speed behavior, independent
  > link/delay/wait deadlines and alpha-only light refresh. Homing shares
  > the lookup and missing-target handling in task 4.50.
  > R1 is 407 Windows / 405 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_guidance_chase_effects.cpp`, using production effects.

- [x] **4.50 Homing-effect lifecycle:** `MHomingEffect` compiles in
  `gamemodel` and shares guidance's creature-position resolution and
  missing-target invalidation. The private host reader remains guarded;
  no new executable adapter is needed. Its header joins unchanged.
  > Tracing updates target X/Y while retaining the separately set height.
  > Ordinary updates steer before fixed-point movement and stop on strict
  > horizontal arrival, snapping height before returning without projection,
  > animation or light. HALO attack keeps its turn and skips tracing and
  > arrival. Display facing is not automatically changed during flight.
  > Height settling compares the magnitude of the step in either direction,
  > so descent stops at its target. Steering reads bounded base coordinates,
  > preserving stored-position semantics when display getters are overridden.
  > Lifetime, independent deadlines and unsigned clock comparisons remain;
  > missing clocks stop flight and missing light services return zero.
  > R1 is 406 Windows / 404 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1,
    `tests/unit/test_homing_effect.cpp` and
    `tests/unit/test_guidance_chase_effects.cpp`, using production effects.

- [x] **4.51 Parabolic-effect lifecycle:** `MParabolaEffect` compiles in
  `gamemodel`, including construction and configuration of cannonade smoke.
  `MParabolaEffectHost` supplies sprite metadata, an ownership-taking smoke
  queue and cannonade impact execution. GameInit installs and clears it.
  Missing metadata skips creation; missing queue/impact services discard
  that output. Readers are private, guarded and re-read on each call.
  > Movement advances before smoke is emitted, then landing snaps the
  > projectile and ends its lifetime before impact. Arrival returns before
  > sector projection, animation and light. Smoke owns its frame, position,
  > direction, multi flag and nine-count lifetime, independently of its parent.
  > Queue submission transfers a `unique_ptr`; the live adapter releases it
  > only into the zone's owning `AddEffect`. Its wait argument of ten selects
  > the wait list without setting a wait deadline. Missing views or missing/
  > short sprite tables skip smoke; a missing zone releases queued smoke.
  > Smoke copies bounded base pixel coordinates: large values saturate, NaN
  > maps to zero and derived display offsets do not affect its stored position.
  > Absolute unsigned deadlines, target-tile conversion and ordinary motion
  > remain. R1 is 405 Windows / 403 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1,
    `tests/unit/test_parabola_effect.cpp` and
    `tests/unit/test_parabola_motion.cpp`, using production effects.

- [x] **4.52 Attached-effect lifecycle:** `MAttachEffect` compiles in
  `gamemodel`, including sprite/color state, attached lifetimes and the ordered
  live/fake/corpse lookup. `MAttachEffectHost` supplies sprite metadata and
  creature-position snapshots; the existing pointer entry point passes its
  borrowed identity to a reader without dereferencing it in the model.
  GameInit installs and clears the guarded services.
  > **Status:** done (2026-10-02). Construction resolves sprite metadata once
  > before initializing the base.
  > Missing metadata uses the existing unknown-type defaults. The base host
  > supplies the clock for the attached-duration convention, including its
  > permanent sentinel and independent link deadline. Missing positions end
  > lifetime while retaining the prior id and coordinates. Successful lookup
  > stores the resolved creature's id and projects its position; it does not
  > restart lifetime. Creature owners refresh position explicitly; Update
  > only advances animation and alpha light while alive. Colour/sprite setters
  > do not reload frames. Unsigned timing and frame-count narrowing remain.
  > Missing services skip reads or use these fallbacks.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_attach_effect.cpp`, using production attached effects.

- [x] **4.53 Orbiting-attachment lifecycle:** `MAttachOrbitEffect` compiles in
  `gamemodel` and combines the attached lifecycle with `EffectOrbit` paths.
  > **Status:** done (2026-10-02). The base effect animates and refreshes
  > light before an active, running orbit advances. Pausing the orbit keeps attachment animation active;
  > expiry stops both. Explicit step advancement ignores those gates. Display
  > coordinates truncate the stored base position, add the orbit in wide
  > arithmetic, then saturate to int; NaN uses zero as its base. Opposite
  > signs can cancel before saturation. Sector coordinates and height stay
  > unchanged. Creature owners refresh attachment positions explicitly. Random starting steps, signed step normalization,
  > unknown-type fallback and cached path references belong to `EffectOrbit`.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1,
    `tests/unit/test_attach_orbit_effect.cpp` and
    `tests/unit/test_effect_orbits.cpp`, using production effects and paths.

- [x] **4.54 Screen-effect ownership and lifecycle:** `MEffectManager` and
  `MScreenEffectManager` compile in `gamemodel`. The base owns each effect
  pointer once and cannot be copied; repeated insertion is a no-op.
  The screen manager updates them and requests linked-effect generation
  through `MScreenEffectManagerHost`, installed and cleared by GameInit.
  > **Status:** done (2026-10-02). New effects enter at the front. Update
  > visits only the effects present on entry, leaving generated effects
  > for the next pass. Active effects request generation once their link
  > deadline is reached; expired effects request it regardless of that
  > deadline, then are deleted and erased. Generation may transfer the
  > owned target before deletion. Missing clocks skip active links; missing
  > generators skip the request and retain ordinary target ownership.
  > Absolute unsigned deadlines and generation-before-deletion order remain.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_screen_effect_manager.cpp`, using production managers,
    effects and effect targets.

- [x] **4.55 Inventory-effect generation:** `MStopInventoryEffectGenerator`
  creates production screen effects in `gamemodel`. The executable supplies
  sprite metadata, inventory placement snapshots and the owning screen manager
  through `MInventoryEffectHost`, installed and cleared by GameInit.
  > **Status:** done (2026-10-02).
  > Placement and metadata resolve before construction. Failed reads or a
  > missing manager return false without taking the caller's target. Successful
  > generation queues the effect before transferring the target, retaining
  > screen-basis updates, item-size centering, frame narrowing, finite count,
  > independent link lifetime, zero direction and step, and input power.
  > Centering widens before subtracting and multiplying, divides toward zero,
  > then adds the cell origin and saturates to int. Screen-effect float storage
  > and the executable's UI coordinate lookup retain their existing behavior.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_inventory_effect_generator.cpp`, using production effects
    and the owning screen manager.

- [x] **4.56 Falling-effect generation:** `MFallingEffectGenerator` creates
  production linear effects in `gamemodel`. `MFallingEffectHost` supplies
  sprite metadata and a consuming zone queue, installed and cleared by GameInit.
  > **Status:** done 2026-10-02; production generator, projectile behavior and
  > queue ownership contracts covered by library tests.
  > Falling begins 300 pixels above the destination. Linear target selection
  > retains its facing calculation, finite lifetime and independent link count.
  > Queue submission consumes the new effect on acceptance, rejection and
  > exception; only acceptance transfers the caller's target. Missing metadata
  > rejects before construction; missing queue services destroy the unlinked
  > effect. Readers are guarded and re-read for each operation.
  > A separate test-first fix clamps the starting height before adding 300
  > would overflow; existing float storage and linear arrival tolerance remain.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_falling_effect_generator.cpp`.

- [x] **4.57 Rising-effect generation:** `MRisingEffectGenerator` creates
  real linear projectiles and firework patterns in `gamemodel`, behind borrowed
  sprite metadata and a consuming zone queue installed and cleared by GameInit.
  > **Status:** done 2026-10-02; production patterns, target ownership and
  > boundary arithmetic covered by library tests.
  > Ordinary effects rise from the source by speed times duration. Volley and
  > dragon actions create three shots; storm creates four. Accepted shot index 1
  > owns the caller's original target, while other accepted shots copy it.
  > Queue acceptance precedes target attachment and target-coordinate updates.
  > Firework success reports original-target transfer, or any accepted shot
  > when no target was supplied; rejected originals remain with the caller.
  > Missing metadata rejects before construction; missing queue services destroy
  > each unlinked effect. Readers are guarded and re-read per operation.
  > Destination offsets clamp to the integer range. Side-speed calculations
  > retain the existing truncation points with widened integer intermediates;
  > zero duration gives side shots zero speed. Float storage remains unchanged.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_rising_effect_generator.cpp`.

- [x] **4.58 Multi-projectile falling generation:** `MMultipleFallingEffectGenerator`
  creates storm and hail patterns in `gamemodel`, with sprite metadata and a
  consuming zone queue installed and cleared by GameInit.
  > **Status:** done 2026-10-02; production patterns, ownership, seeded sampling
  > and arithmetic boundaries covered by library tests.
  > Four shots form each phase, with action-specific spread and phase counts.
  > Each phase consumes twelve random draws before constructing its effects,
  > and increases the duration before submission. The first accepted effect
  > takes the original target without retargeting; later accepted effects own
  > retargeted copies. All projectiles use the shared destination height.
  > Missing metadata rejects before construction; missing queues destroy
  > unlinked effects. Readers are guarded and re-read per operation.
  > Coordinate offsets clamp after widened addition. Zero speed adds no phase
  > duration, so effects retain their configured lifetime and animate in place.
  > Float storage, random sampling and nonzero-speed duration rules remain.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_multiple_falling_effect_generator.cpp`.

- [x] **4.59 Zone-attack generation and tile geometry:** `MAttackZoneEffectGenerator`
  creates real linear projectiles in `gamemodel`. `WorldTileGeometry` supplies
  the same conversions and sector stepping used by MTopView and MCreature.
  > **Status:** done (this commit).
  > Sprite metadata resolves before construction; the executable adapter
  > requires a table and view. HALO extends three tiles in the selected facing;
  > wind-divider fallback retains unsigned sector narrowing and one-tile stepping.
  > Supplied facing overrides linear selection. Wind link-target coordinates
  > retain their unnormalized offset and update before queue submission.
  > Missing metadata rejects; missing queues destroy unlinked effects. The
  > consuming queue takes each new effect, and acceptance transfers the target.
  > Tile origins and wind endpoints clamp after widened arithmetic. Wind distance
  > retains a floored length and integer division; linked targets keep their
  > distinct unnormalized calculation. Existing float position storage remains.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1,
    `tests/unit/test_world_tile_geometry.cpp` and
    `tests/unit/test_zone_attack_effect_generator.cpp`.

- [x] **4.60 Parabolic projectile generation:** `MAttackZoneParabolaEffectGenerator`
  and `MAttackZoneBombEffectGenerator` create real parabolic effects in `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite metadata resolves before construction; consuming queues
  > transfer target ownership only on acceptance. Parabola effects
  > extend one tile in the supplied direction, then target selection sets facing.
  > Cannonade keeps its source height and original impact tile; other shots
  > start two tiles higher. Bomb destinations retain their requested coordinates.
  > MTopView delegates directional offsets to the shared WorldTileGeometry.
  > Source/target height lifts and directional endpoint offsets clamp after
  > widened addition. Float storage, cannonade source height and impact-tile
  > narrowing, and unlifted bomb destinations retain their existing behavior.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1,
    `tests/unit/test_world_tile_geometry.cpp` and
    `tests/unit/test_parabolic_effect_generators.cpp`.

- [x] **4.61 Stationary zone-effect generation:** `MStopZoneEffectGenerator`
  selects variants and creates stationary effects in `gamemodel`.
  > **Status:** done (this commit).
  > Sprite selection precedes meteor event submission; final frame-count lookup
  > follows it. Shared geometry retains sector narrowing. Firework/pet positions
  > and multi-effect exceptions, sword-wave facing adjustments, random variant
  > and animation selection, and stone-auger cross ordering retain their rules.
  > Missing metadata rejects and consuming queues release unlinked effects.
  > Stone-auger success reports transfer of the original target to slot zero;
  > without a target, it reports any accepted effect. Other slots own copies.
  > Nonpositive animation counts skip random start selection. Positive counts
  > retain one random sample per accepted effect and existing byte narrowing.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_stopped_zone_effect_generator.cpp`.

- [x] **4.62 Cross-shaped zone-effect generation:** `MStopZoneCrossEffectGenerator`
  creates the center and clipped cross arms in `gamemodel`.
  > **Status:** done (this commit).
  > Sprite metadata resolves before construction; zone bounds are read after
  > center submission. The first accepted effect takes the original target;
  > later accepted effects own copies, offset from the clipped lower bounds.
  > SAND_CROSS fixes the arm radius and power at three while retaining input
  > power on the center. Missing metadata rejects, missing bounds stop the
  > remaining arms, and consuming queues release unlinked effects. Copied
  > target coordinate additions now widen and clamp after four tests reproduced
  > 21 failed checks and UBSan reported both signed-overflow sites. Thirty-seven
  > production tests pass in full plain and strict ASan/UBSan suites. Removing
  > the generator object from a copied archive breaks the actual unit link.
  > R1 is 392 Windows / 390 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_cross_zone_effect_generator.cpp`, using production effects.

- [x] **4.63 Fixed zone-effect patterns:** `MStopZoneXEffectGenerator` and
  `MStopZoneRhombusEffectGenerator` construct their real effects in `gamemodel`.
  > **Status:** done (this commit).
  > Both generators use borrowed sprite metadata and consuming submission,
  > installed and cleared by GameInit. Each retains its fixed tile order and
  > unsigned sector wrap. Only slot zero takes the original target; later
  > accepted slots receive unchanged copies. The result reports slot zero,
  > including targetless calls. Missing services are guarded. Twenty-five tests
  > cover both real generators, all 48 acceptance masks, sector boundaries,
  > callbacks, lifetime and animation. Full plain and strict ASan/UBSan suites
  > pass; removing either object from a copied archive breaks the unit link.
  > R1 is 390 Windows / 388 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_fixed_zone_effect_patterns.cpp`, using production effects.

- [x] **4.64 Empty-cross generation:** `MStopZoneEmptyCrossEffectGenerator`
  creates the four arms without a center in `gamemodel`.
  > **Status:** done (this commit).
  > The fixed-pattern host supplies sprite metadata and consuming submission.
  > Tile order, unsigned sector wrap and slot-zero ownership remain. Later
  > accepted arms own copies aimed one tile from the original source pixels,
  > keeping the sub-tile remainder; they use source Z and supplied creature ID.
  > The result reports slot zero even without a target. Copied target offsets
  > now widen and clamp after three tests reproduced five failed checks and
  > UBSan reported overflow at all three sites. Thirty production tests pass
  > in full plain and strict ASan/UBSan suites. Removing the object
  > from a copied archive breaks the actual unit link.
  > R1 is 389 Windows / 387 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_empty_cross_effect_generator.cpp`, using production effects.

- [x] **4.65 Empty-wall generation:** the horizontal and vertical empty-wall
  generators construct their complete patterns in `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed metadata and consuming submission replace executable globals.
  > Direction tables, skipped middle index, mine source-tile placement and
  > ordinary destination-tile placement remain. The first accepted effect
  > takes the original target. Later copies follow destination-pixel increments,
  > including rejected and skipped iterations. Neither pattern randomizes frames.
  > Directions 8–255 now reject before table access. Two tests reproduced four
  > failed checks; ASan confirmed a horizontal-table buffer overread. All invalid
  > direction bytes are covered at four step-count boundaries. Destination-pixel
  > accumulators now use wide arithmetic and clamp copied coordinates; four
  > tests reproduced 38 failed checks and UBSan reported all four overflow sites.
  > Thirty-six production tests pass in full plain and strict ASan/UBSan suites. Removing either object from a copied archive breaks the unit link.
  > R1 is 387 Windows / 385 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_empty_wall_effect_generators.cpp`, using production effects.

- [x] **4.66 Empty-rectangle generation:** `MStopZoneEmptyRectEffectGenerator`
  fills a clipped square except for its center in `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite metadata and bounds resolve before effect construction;
  > consuming submission replaces zone globals. Row order, source tile narrowing,
  > clipping and power remain. The first accepted effect owns the original
  > target; later copies use offsets anchored at the clipped lower bounds.
  > Animation starts at frame zero without consuming randomness.
  > Copied-target pixel additions widen before saturating at the integer limits;
  > clipping and the original target are unchanged. R1 is 386 Windows / 384 Ninja,
  > measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_empty_rectangle_effect_generator.cpp`.

- [x] **4.67 Full-rectangle generation:** `MStopZoneRectEffectGenerator` submits
  its center and clipped surrounding square from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite and frame metadata resolve before construction. Bounds
  > follow center submission; the consuming queue carries each tile's delay.
  > Darkness phases, randomized variants, hail and explosion delays retain
  > their ordering. Ice-field and mine radius overrides occur after the center.
  > The first accepted effect takes the original target; later copies use
  > offsets from the clipped lower bounds.
  > Copied pixel sums widen before saturating at integer limits. R1 is
  > 385 Windows / 383 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_rectangle_effect_generator.cpp`.

- [x] **4.68 Complete wall generation:** `MStopZoneWallEffectGenerator` creates
  stationary walls around destination tiles from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite metadata and consuming submission replace executable
  > globals. The full directional sequence includes its middle tile. The first
  > accepted effect takes the original target; copies advance from destination
  > pixels on every iteration, including rejected submissions.
  > Invalid direction bytes reject before table access or effect construction.
  > Target accumulators use 64-bit arithmetic and saturate only when assigning
  > copied coordinates. R1 is 384 Windows / 382 Ninja, measured in both local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_wall_effect_generator.cpp`.

- [x] **4.69 Multiple stationary generation:** `MStopZoneMultipleEffectGenerator`
  emits staggered groups of four randomized pixel effects from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite metadata, an optional shake event and consuming submission
  > replace executable globals. Ordinary calls emit four phases; wide acid and
  > poison storms emit six with wider offsets. Each phase draws every position
  > before submission. Delays advance for every attempt; the first accepted
  > effect owns the original target and later copies use generated pixels.
  > Generated pixel sums widen before saturating at integer limits, including
  > the fixed horizontal offsets. R1 is 383 Windows / 381 Ninja, measured in
  > both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_multiple_stationary_effect_generator.cpp`.

- [x] **4.70 Random-zone generation:** `MStopZoneRandomEffectGenerator` creates
  four randomly offset quadrant effects from `gamemodel`.
  > **Status:** done (this commit).
  > The existing fixed-pattern host supplies borrowed sprite metadata and
  > consuming submission through an independent installer. Each quadrant draws
  > two offsets immediately before construction and submission. Only slot zero
  > takes the original target and determines the result, including targetless
  > calls; later accepted slots receive unchanged target copies. Source tiles
  > narrow to unsigned sector coordinates before offsets are applied. R1 is
  > 382 Windows / 380 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_random_zone_effect_generator.cpp`.

- [x] **4.71 Selectable stationary generation:** `MStopZoneSelectableEffectGenerator`
  creates selectable stationary effects from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed services resolve sprite metadata, then the final frame count after
  > darkness variants and sword-wave direction selection. Submission consumes
  > each effect; only acceptance transfers the original target and randomizes
  > a repeating animation start. Source tiles retain unsigned narrowing;
  > repeat draws use the full frame count while animation stores its byte value.
  > R1 is 381 Windows / 379 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_selectable_stationary_effect_generator.cpp`.

- [x] **4.72 Spread-out generation:** `MSpreadOutEffectGenerator` creates
  eight linear trajectories from `gamemodel`.
  > **Status:** done (this commit).
  > The fixed-pattern host provides borrowed sprite metadata and consuming
  > submission through an independent installer. Each numbered direction steps
  > one unsigned source tile before computing its scaled pixel destination.
  > Slot zero alone takes the original target and determines the result; later
  > accepted slots get copies, and every accepted target uses its destination.
  > Coordinate differences, distance and travel calculations widen before
  > converting destinations back to bounded integers; speed rounding is retained.
  > R1 is 380 Windows / 378 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_spread_out_effect_generator.cpp`.

- [x] **4.73 Around-zone generation:** `MAroundZoneEffectGenerator` creates
  stationary variants and staggered stream effects from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed metadata is resolved after each attempt's selection. Selected
  > variants persist between attempts while positions reset. Submission consumes
  > effects with their wait count; first acceptance takes the original target,
  > and later copies use destination coordinates. Missing metadata skips an
  > attempt without discarding an earlier acceptance result. Pixel offsets widen
  > before saturating, including complete fire offsets. Stream durations add
  > their wait count in unsigned frame arithmetic. R1 is
  > 379 Windows / 377 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_around_zone_effect_generator.cpp`.

- [x] **4.74 Follow-path generation:** `MFollowPathEffectGenerator` creates
  phase-selected linear effects from `gamemodel`.
  > **Status:** done (this commit).
  > The fixed-pattern host supplies borrowed metadata and consuming submission
  > through an independent installer. Wild Typhoon rebuilds the shared paths;
  > other actions reuse the current cache. Source tiles narrow before signed
  > offsets, and acceptance alone transfers and retargets the original target.
  > Invalid directions reject after metadata and cache work, before indexing.
  > R1 is 378 Windows / 376 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_follow_path_effect_generator.cpp`.

- [x] **4.75 Meteor-drop generation:** `MMeteorDropEffectGenerator` creates
  falling projectiles and their fade events from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite metadata, consuming effect submission and event submission
  > are installed by the executable. Acceptance transfers the original target
  > before scheduling the one-second red fade. Missing event services skip the
  > fade while retaining the accepted effect. Launch offsets saturate before
  > integer overflow, retaining float position storage and original destinations.
  > R1 is 377 Windows / 375 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_meteor_drop_effect_generator.cpp`.

- [x] **4.76 Creature parabola generation:** `MAttackCreatureParabolaEffectGenerator`
  creates projectiles toward sampled creature positions from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed services resolve sprite metadata, creature tile/height and then
  > animation length, preserving their order. Submission consumes the effect;
  > acceptance alone transfers the original target without changing its state.
  > The executable retains live creature lookup. Source-height offsets saturate
  > before integer overflow; creature heights keep their signed short range.
  > R1 is 376 Windows / 374 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_creature_parabola_effect_generator.cpp`.

- [x] **4.77 Wide ripple generation:** `MRippleZoneWideEffectGenerator` creates
  widening rows of stationary effects from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite metadata, refreshed per-candidate bounds and consuming
  > submission preserve unsigned tile steps and power growth. Center acceptance
  > alone transfers the unchanged original target and determines success; side
  > effects have no target. Invalid directions reject after sprite lookup and
  > before bounds or construction. Clipped positions advance to the next row
  > candidate without shifting center ownership. R1 is 375 Windows / 373 Ninja,
  > measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_wide_ripple_effect_generator.cpp`.

- [x] **4.78 Bloody Breaker generation:** `MBloodyBreakerEffectGenerator`
  creates phase-driven directional rows of stationary effects from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed sprite and animation services preserve per-attempt refresh and the
  > original blit type across Bloody Wall variants. Consuming submission links
  > only an accepted center to the unchanged original target and reports that
  > transfer as success; rejected centers leave caller ownership. Sides stay targetless.
  > Missing services stop with any earlier target transfer preserved.
  > Invalid directions reject after initial metadata, before pattern indexing.
  > R1 is 374 Windows / 372 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_bloody_breaker_effect_generator.cpp`.

- [x] **4.79 Bloody Wall generation:** `MBloodyWallEffectGenerator` creates
  five directional stationary effects and linked target copies from `gamemodel`.
  > **Status:** done (this commit).
  > Borrowed metadata preserves initial blit/repeat policy and per-attempt refresh.
  > Consuming submission gives the first accepted effect the unchanged original
  > target; later accepted effects own copies at destination-relative pixels.
  > Missing services preserve earlier acceptance. Source tiles wrap as before.
  > Invalid directions reject after initial metadata and before pattern indexing.
  > Nonpositive animation lengths skip frame randomization. Destination pixel
  > offsets saturate before integer overflow without retargeting the original.
  > R1 is 373 Windows / 371 Ninja, measured in both generated local trees.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_bloody_wall_effect_generator.cpp`.

- [x] **4.80 Peer and sound metadata and English NPC names:** move
  `RequestUserManager`, `MZoneSound`, `SectorSoundInfo` and `MNPCTableEnglish`
  into `gamemodel` without changing their production bytes.
  > **Status:** done (2026-10-05; PR #372, verified with the cumulative stack).
  > Tests call the real endpoint book, sound record I/O and English overlay.
  > They preserve same-IP port retention, serialized field order, runtime-only
  > sound deadlines, and unrelated NPC gameplay metadata.
  - Owner: M0-M2, membership, R1, `test_request_users.cpp`,
    `test_zone_sound_metadata.cpp` and `test_npc_names.cpp`.

- [x] **4.81 Bloody Wave and Ripple Zone generation:** move both complete
  generators into `gamemodel` behind borrowed frame, bounds and queue services.
  > **Status:** done (2026-10-05; PR #374, verified with the cumulative stack).
  > Tests cover phase/direction bytes, acceptance masks, frame/random ordering,
  > target ownership, missing/replaced services and callback exceptions.
  > Wave preserves its first-accepted target transfer; nonpositive frame counts
  > skip random advancement and targetless phase progression saturates above
  > the last distinct pattern. Accepted ground ripples now own their target and
  > action link, preserving later phases and releasing the target on destruction.
  > The ground adapter owns submissions until the zone actually
  > retains them, so rejection returns false and allocation failure frees them.
  > `MEffectTargetOwner` now owns pending targets at both generation entry
  > points until `MEffect::SetLink` adopts them. Destruction disarms pending
  > ownership; nested guards transfer responsibility and cannot rearm a dying
  > target. Library tests reproduce the detached-target exception boundary and
  > cover both pre- and post-adoption failure; the complete executable dispatcher
  > is source-audited and build-verified, not linked into those tests.
  > `PrepareInventoryEffectTarget` attaches an optional result before the
  > consuming call. `AddNewInventoryEffect` no longer writes to a target that
  > synchronous generation may have deleted or allocates an unowned empty
  > result. Its library tests are regression guards; the caller reorder is
  > verified by source review and the complete build.
  - Owner: M0-M2, membership, R1, the designated `GameInit.cpp` hosts,
    `test_bloody_wave_effect_generator.cpp`, `test_ripple_zone_effect_generator.cpp`,
    `test_effect_target_owner.cpp` and `test_inventory_effect_target.cpp`.

- [x] **4.82 Rank-bonus packet application:** move `GCRankBonusInfoHandler`
  and `GCSelectRankBonusOKHandler` into `gamemodel`, with player regeneration
  behind `RankBonusHandlers::Host` installed by `GameInit.cpp`.
  > **Status:** done (2026-10-05; PR #373, verified with the cumulative stack).
  > Real wire-decoded packets test learned state, neighboring exclusions,
  > packet ordering, request mode clearing and regeneration ordering.
  > Missing host/callback skips only the live player refresh. Wire selection IDs
  > remain unsigned through bounds validation: a reproduced `UINT32_MAX`
  > packet previously changed unrelated learned rows; IDs above `INT_MAX`
  > also reached unsafe signed arithmetic. Six boundary values now preserve
  > the rows while still clearing the request mode.
  - Owner: M0-M2, membership, R1, `RankBonusHandlerHost.h`, designated
    installation/cleanup and `test_rank_bonus_handlers.cpp`.

- [x] **4.83 Audio owners and diagnostic models:** compile cached sound buffers
  and caller-owned music streams in `dxlib`, and crash-report records and
  named profiler state in `gamemodel`.
  > **Status:** done (2026-10-05; PR #376, verified with the cumulative stack).
  > Audio tests use the real mixer when available and retain a fallback-safe
  > default-state case. Production ownership and diagnostic serialization are
  > preserved. Rejected elemental insertion now returns after freeing the
  > unaccepted creature; that executable path is source-audited/build-verified.
  - Owner: explicit dxlib sources, membership, R1, `test_audio_owners.cpp`
    and `test_diagnostic_models.cpp`.

- [x] **4.84 Bonus eligibility and login-list handlers:** move five complete
  handlers behind designated player snapshots, skill refresh and UI services.
  > **Status:** done (2026-10-05; PR #377, verified with the cumulative stack).
  > Bonus replies replace all twelve eligibility flags, clearing omitted or
  > no-longer-eligible entries. Live framing already caps the bonus list;
  > direct-call bounds checks are defense in depth. Login publication follows
  > model application, and mode selection follows successful publication.
  - Owner: membership, R1, `BonusSkillHost`, `LoginListHost`, their designated
    installation/reset, `test_bonus_skill_handlers.cpp` and
    `test_login_list_handlers.cpp`.

- [x] **4.85 Ground ring, selectable and rectangle generation:** move three
  complete generators behind sprite, consuming ground-queue and event services.
  > **Status:** done (2026-10-05; PR #378, verified with the cumulative stack).
  > Successful submissions transfer the original target once; ring branches
  > own independent copies. Invalid axe directions are rejected, and blood
  > jitter saturates at pixel limits without changing random-call order.
  > Copied targets retain continuation identity without inheriting player-list
  > removal responsibility. New identities restore that responsibility;
  > assignment preserves the destination's registration identity. Destruction
  > and executable phase completion use the same guarded removal method.
  - Owner: membership, R1, designated hosts, the three ground-generator tests,
    `test_effect_target_registration.cpp` and real-result destruction checks
    in the existing branching-generator tests.

- [x] **4.86 Login server snapshots:** replace the selected world's server
  records when a complete topology reply arrives.
  > **Status:** done (2026-10-05; PR #379, verified with the cumulative stack).
  > Removed servers disappear and an empty reply clears server selection.
  > The selected world object, its metadata and other worlds remain intact.
  - Owner: `ApplyServerList`, `CServerInformation::ClearServerSelection`,
    `test_server_lists.cpp` and `test_login_list_handlers.cpp`.

- [x] **4.87 Retire unused and empty executable sources:** remove the unused
  `GAME1024`, `CheckSystem` and `CSystemInfo` classes plus four empty/header-only
  time/PCH translation units.
  > **Status:** done (2026-10-05; PR #375, verified with the cumulative stack).
  > Still-used headers remain. The obsolete explicit time-source entry and
  > commented CPU-report caller are removed. R1 keeps its existing population
  > rule; the files themselves are gone, with no new exclusions.
  - Owner: generated target inventory, full executable link and R1.

- [x] **4.88 Regen-zone ownership packet application:** move the complete
  `GCRegenZoneStatusHandler` into `gamemodel` beside its existing tower model.
  > **Status:** done (2026-10-05; PR #382, implementation `3e620117`).
  > `RegenZoneStatus::Host` borrows the current UI-owned table per packet;
  > `GameInit.cpp` installs and resets the designated callback. Missing services
  > skip updates. Eight wire ownership bytes and the fixed race ownership of
  > later rows retain their existing behavior, including unknown-byte values.
  - Owner: membership, R1, `RegenZoneStatusHost.h`, designated installation/reset
    and `test_regen_zone_status_handler.cpp` using the real production handler.

- [x] **4.89 Skip-draw ground patterns:** move the complete
  `MSkipEffectGenerator` into `gamemodel`, using the existing ground queue and
  `WorldTileGeometry` conversions.
  > **Status:** done (2026-10-05; PR #383, implementation `2467cc9f`).
  > Line, cross and square order are preserved. Only retained effects receive
  > links; the first retained effect takes the original target and later
  > effects own independent visual continuations. Complete rejection and
  > unsupported grades return false. Pending effects and copies are owned
  > through rejection and exceptions; snapshots survive earlier-effect removal.
  - Owner: membership, R1, designated `GameInit.cpp` installation/reset and
    `test_skip_effect_generator.cpp`, with real effects and all acceptance masks.

- [x] **4.90 Skill downgrade replies:** move `GCDownSkillOKHandler` and
  `GCDownSkillFailedHandler` into `gamemodel` behind the popup-only
  `SkillDowngrade::Host` installed and reset by `GameInit.cpp`.
  > **Status:** done (2026-10-05; PR #384, implementation `cf76fca8`).
  > Success replies require an allocated skill row with a positive level.
  > Repeated replies at zero and invalid IDs preserve state without announcing
  > success. Valid decrements retain existing `CanDelete` and domain-membership
  > policy; a missing manager skips only optional relearning. All failure-byte
  > message mappings remain unchanged. Missing presentation skips only the popup.
  - Owner: membership, R1, designated host installation/reset and
    `test_skill_downgrade_handlers.cpp`, including the reproduced decrement bug.

- [x] **4.91 Handlers that ignore their packet:** move the thirteen handlers
  whose bodies act on nothing in any shipped build into `gamemodel`
  unchanged: `CURequestLoginMode`, `GCCastingSkill`, `GCDisconnect`,
  `GCPhoneConnectionFailed`, `GCReconnect`, `GCRequestFailed`,
  `GCRequestedIP`, `GCShowMessageBox`, `GLIncomingConnectionError`,
  `GLIncomingConnectionOK`, `LCVersionCheckOK`, `LGIncomingConnection` and
  `UCRequestLoginMode`.
  > **Status:** done (2026-10-13; PR #389). After a separate commit drops an
  > unused `ClientDef.h` include from three of them, the files move byte for
  > byte and need no host. `GCAuthKeyHandler` (includes the executable's
  > `Client.h` for a dead `__NPROTECT__` call) and `GCSubInventoryInfoHandler`
  > (a dead `__TEST_SUB_INVENTORY__` block names `g_pZone`/`g_pPlayer`, which
  > R4 counts) stay until those blocks are retired. The three `GL`/`LG`
  > handlers are server-to-server datagrams the client's datagram validator
  > never accepts. R1 is 329 Windows / 327 Ninja.
  - Owner: membership, R1 and `test_ignored_packet_handlers.cpp`, which runs
    every handler and requires its packet unchanged, decoding it first where
    the client can (all but `GCShowMessageBox` and `UCRequestLoginMode`);
    removing the thirteen files from the membership breaks the test link on
    each.

- [x] **4.92 Guild and union replies:** move `GCGuildChatHandler`,
  `GCGuildMemberListHandler`, `GCGuildResponseHandler`,
  `GCOtherGuildNameHandler`, `GCShowGuildInfoHandler`,
  `GCShowGuildJoinHandler`, `GCShowGuildMemberInfoHandler`,
  `GCShowUnionInfoHandler` and `GCShowWaitGuildInfoHandler` into `gamemodel`
  behind the presentation-only `GuildPresentation::Host` installed and reset
  by `GameInit.cpp`.
  > **Status:** done (2026-10-13; PR #390). Each `UI_*` call becomes the host
  > entry of the same name; nothing else in the handlers changes. Guild chat keeps its
  > chat-manager ID filter and curse masking, and the union reply its
  > twelve-code message map. Missing presentation skips only the call.
  > R1 is 320 Windows / 318 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_guild_presentation_handlers.cpp` (decoded chat, response, name and
    join packets; pass-through identity for the five information packets;
    missing hosts, empty entries and host replacement).

- [x] **4.93 Friend, SMS address-book and whisper-failure replies:** move
  `GCFriendChattingHandler`, `GCSMSAddressListHandler`,
  `GCAddressListVerifyHandler` and `GCWhisperFailedHandler` into `gamemodel`
  behind the presentation-only `CommunicationPresentation::Host` installed
  and reset by `GameInit.cpp`.
  > **Status:** done (2026-10-13; PR #391). As in 4.92, each `UI_*` call becomes the
  > host entry of the same name; `GCSMSAddressListHandler` also drops an unused
  > `PacketFunction.h` include. The friend reply keeps its command switch
  > (`GC_ADD_FRIEND_ERROR`, `GC_NO_ONLINE` and request commands present
  > nothing), and the address-book reply its request checks and the
  > unconditional reset of `TempInformation`'s mode. R1 is 316 Windows / 314
  > Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_communication_presentation_handlers.cpp` (every friend command and
    address-book code, on decoded packets, with the request reset after each;
    missing hosts, empty entries and host replacement).

- [x] **4.94 Quest and war status replies:** move `GCQuestStatusHandler`,
  `GCSelectQuestIDHandler`, `GCWarScheduleListHandler`,
  `GCFlagWarStatusHandler` and `GCBloodBibleStatusHandler` into `gamemodel`
  behind the presentation-only `QuestWarPresentation::Host` installed and
  reset by `GameInit.cpp`.
  > **Status:** done (2026-10-13; PR #392). As in 4.92, each `UI_*` call
  > becomes the host entry of the same name, and all five entries take the UI functions
  > directly; the `MEventManager.h`, `DebugInfo.h` and `ClientDef.h` includes
  > go, and the two handlers that log include `DebugLog.h`, which defines the
  > `DEBUG_ADD` those headers brought in. The quest list still opens the quest-selection
  > request (with `g_pTempInformation` unguarded, as before) after presenting,
  > with or without a host; the flag-war reply still ignores 14400 seconds
  > (four hours) or more. R1 is 311 Windows / 309 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_quest_war_presentation_handlers.cpp` (pass-through identity; the
    selection request and its order; decoded flag-war packets for the end
    time, counts and the four-hour boundary; missing hosts, empty entries and
    host replacement).

- [x] **4.95 Trade, crafting and item-mall replies:** move
  `GCTradeFinishHandler`, `GCMakeItemFailHandler` and `GCGoodsListHandler`
  into `gamemodel` behind the presentation-only `TradePresentation::Host`
  installed and reset by `GameInit.cpp`.
  > **Status:** done (2026-10-13; PR #393). As in 4.92, each `UI_*` call
  > becomes the host entry of the same name; `IsRunningExchange` answers that no exchange
  > window is open when no host is installed, so a rejection then closes
  > nothing. The trade reply keeps driving the trade manager (accept,
  > reconsider, execute, and on a rejection returning the offered money)
  > with or without a host; `GCMakeItemFailHandler` includes `DebugLog.h`
  > for the `DEBUG_ADD_ERR` the executable headers brought in. Both handlers
  > still use `g_pTempInformation` unguarded. R1 is 308 Windows / 306 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_trade_presentation_handlers.cpp` (decoded trade replies against a
    real trade manager, executed and refused trades; the refund before the
    close, which ends the request as the real one does; the crafting
    request check and reset order; missing hosts, empty entries and host
    replacement).

- [x] **4.96 Retire sources that compile to nothing:** delete
  `LeakMemoryDumper.cpp` (and its header), `MTestDef.cpp` and
  `SizeOfObjects.cpp`, whose whole bodies are behind `ENABLE_LEAK_TRACKER`,
  `__METROTECH_TEST__` and `__DEBUG_OUTPUT__`, none of which any build,
  preset or header defines.
  > **Status:** done (2026-10-13; PR #394). No compiled code goes, so there is nothing
  > to test: on every platform the three objects held only inlined library
  > code. `MTestDef.h` stays (sixteen files include its macros); its
  > `extern UDPTest g_UDPTest` is now defined nowhere, behind the same dead
  > macro. Two commented-out `SizeOfObjects()` references in `Client.cpp` go
  > with it. R1 is 305 Windows / 303 Ninja.
  - Owner: R1, which would count any of them again if restored.

- [x] **4.97 NPC map, skill teaching, mini-game and prize-notice replies:**
  move `GCNPCInfoHandler`, `GCTeachSkillInfoHandler`,
  `GCMiniGameScoresHandler` and `GCNotifyWinHandler` into `gamemodel` behind
  the presentation-only `GameInfoPresentation::Host` installed and reset by
  `GameInit.cpp`.
  > **Status:** done (2026-10-13; PR #395). As in 4.92, each `UI_*` call becomes
  > the host entry of the same name; four entries take the UI functions directly
  > and the notice's DWORD gift id goes through a lambda. The NPC reply still
  > clears the list and then presents and frees each NPC it decoded, with or
  > without a host; skill teaching still opens the skill-learn request (the
  > domain and the fixed level cap 100, not the packet's target level) after
  > presenting, with `g_pTempInformation` unguarded as before. R1 is 301
  > Windows / 299 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_game_info_presentation_handlers.cpp` (decoded NPC, teacher and
    notice packets; NPCs freed; request order and values; missing hosts,
    empty entries and host replacement).

- [x] **4.98 Message-box and item-release replies:** move
  `GCLearnSkillFailedHandler`, `GCSearchMotorcycleFailHandler`,
  `GCSearchMotorcycleOKHandler`, `GCTakeOutFailHandler`,
  `GCTakeOutOKHandler`, `GCUseBonusPointFailHandler`, `GCShopBuyFailHandler`,
  `GCShopSellFailHandler` and `GCPetStashVerifyHandler` into `gamemodel`
  behind the presentation-only `DialogPresentation::Host` installed and
  reset by `GameInit.cpp`.
  > **Status:** done (2026-10-13). `g_pUIDialog->PopupFreeMessageDlg(msg)`
  > becomes the host's `PopupFreeMessageDlg(msg)`, whose adapter calls the
  > dialog unguarded as the handlers did; each `UI_X` call becomes the entry
  > of the same name, and `IsRunningSwapAdvancementItem` answers that no swap
  > is running without a host. The executable headers go
  > (`GCTakeOutOKHandler` also drops its own `extern` of the UI function), and
  > `GCPetStashVerifyHandler` includes `DebugLog.h` for its `DEBUG_ADD*`. The
  > replies keep their request-register resets and the pet stash its moves
  > between `g_pStorage` and `g_pInventory`, all with `g_pTempInformation`
  > unguarded as before. R1 is 292 Windows / 290 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_dialog_presentation_handlers.cpp` (every reason code and message,
    call order with unlocks and resets, both motorcycle excuses, the found
    message's zone and position, both pet moves against real containers;
    missing hosts, empty entries and host replacement).

- [x] **4.99 Login-server character, ID and version replies:** move
  `LCCreatePCOKHandler`, `LCDeletePCOKHandler`, `LCPCListHandler`,
  `LCQueryResultPlayerIDHandler`, `LCQueryResultCharacterNameHandler`,
  `LCSelectPCErrorHandler` and `LCVersionCheckErrorHandler` into `gamemodel`
  behind `LoginPresentation::Host` installed and reset by `GameInit.cpp`;
  their message boxes go through 4.98's `DialogPresentation::Host`.
  > **Status:** done (2026-10-13). Each `UI_X` call becomes the entry of the
  > same name, and `UI_SetCharacter`'s three overloads become
  > `SetSlayerCharacter`, `SetVampireCharacter` and `SetOustersCharacter`.
  > Login modes follow 4.84's `LoginLists` precedent, one entry per
  > decision: `EnterSelectPCMode` is `SetMode(MODE_WAIT_SELECTPC)`,
  > `QueueWaitPCListMode` and `QueueLoginWrongMode` set `g_ModeNext`, and
  > `IsSelectPCMode` asks whether `g_Mode` is `MODE_WAIT_SELECTPC` (false
  > without a host). The version error's exit-after message box, the one
  > call with arguments beyond the message, is `PopupExitMessageDlg`, and
  > the selection refusal's `Sleep(1000)` stalls are the `Sleep` entry,
  > whose adapter calls the same `Sleep`, so a test need not wait three
  > seconds. The `__LINUX__` console bodies of `LCPCListHandler` and
  > `LCQueryResultPlayerIDHandler` stay commented out as they were, and
  > `LCVersionCheckErrorHandler` drops two `extern`s used only by commented
  > lines. The character list still fills `g_pUserInformation`'s slots and
  > frees `g_pStorage`, and created or deleted characters still ask for the
  > list again on the login connection. R1 is 285 Windows / 283 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_login_presentation_handlers.cpp` (the list request's bytes and
    player status after a create or delete, both ID answers, the name
    answer only for the asked-about name while choosing a character, the
    version error's order, every selection refusal code, each race's slot
    and an empty slot kept; missing hosts, empty entries and host
    replacement).

- [x] **4.100 Replies about the client's own player:** move
  `GCRideMotorCycleFailedHandler`, `GCGetOffMotorCycleFailedHandler`,
  `GCUnburrowFailHandler`, `GCUntransformFailHandler`, `GCVisibleFailHandler`,
  `GCVisibleOKHandler`, `GCHPRecoveryStartToSelfHandler`,
  `GCHPRecoveryEndToSelfHandler`, `GCMPRecoveryStartHandler`,
  `GCMPRecoveryEndHandler`, `GCModifyInformationHandler`,
  `GCUseBonusPointOKHandler`, `GCAddGearToInventoryHandler`,
  `GCTimeLimitItemInfoHandler`, `GCMoveOKHandler` and `GCMoveErrorHandler`
  into `gamemodel` behind `PlayerState::Host`, installed and reset by
  `GameInit.cpp`.
  > **Status:** done (2026-10-13). The player's status array is the host's
  > `Status()`, `g_pPlayer` as an `MStatus*` (NULL without a player), so
  > `AffectModifyInfo` and the status reads and writes are unchanged and
  > `SetStatus` still dispatches to `MPlayer`'s override; every other
  > `g_pPlayer->X(...)` is the entry `X` with the same arguments, and
  > `g_pPlayer!=NULL` is `Status()!=NULL`. `MPlayer`'s `WAIT_VERIFY` enum
  > moves to `PlayerStateHost.h` unchanged, and `MPlayer` brings it back
  > into its scope (`using enum`), so `MPlayer::WAIT_VERIFY_X` still names
  > the same values everywhere. Without a host the queries answer as for no
  > player (no status or gear, nothing awaited, a slayer, false) and actions
  > are skipped, though the slayer answer still lets the gear reply move a
  > slayer's gear; the handlers that dereferenced `g_pPlayer` unguarded
  > dereference `Status()` and `GetGear()` the same way. `ClientDef.h` gives
  > way to the host and library headers, `GCMoveOKHandler` drops an unused
  > `VS_UI.h`, and `GCUseBonusPointOKHandler` loses a commented-out block
  > that named `g_pPlayer` (which R4, reading text, counted) and has its
  > comments translated. R1 is 269 Windows / 267 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_player_state_handlers.cpp` (both motorcycle failures; the skill
    failures and visibility only for a skill wait; recovery start with the
    converted delay and end at the server's value, after stopping; status
    application through a real `MStatus` and the carried effect with its
    converted duration; the bonus point spent only for its request; gear
    moved from each race's real gear to the named inventory cell; time
    limits recorded and worn items re-applied; the move reply on a real
    `ClientPlayer`; missing hosts and entries).

- [x] **4.101 Replies about the zone and its creatures:** move
  `GCDeleteEffectFromTileHandler`, `GCAddHelicopterHandler`,
  `GCCreatureDiedHandler`, `GCStatusCurrentHPHandler`,
  `GCHPRecoveryStartToOthersHandler`, `GCHPRecoveryEndToOthersHandler`,
  `GCModifyNicknameHandler`, `GCMoveHandler`, `GCAttackHandler`,
  `GCNPCSayHandler`, `GCNPCSayDynamicHandler` and `GCKnockBackHandler` into
  `gamemodel` behind `ZoneState::Host` and `CreatureState::Host`, installed
  and reset by `GameInit.cpp`.
  > **Status:** done (2026-10-13). `MCreature` stays the executable's: the
  > zone's `GetCreature` hands out the pointer, the replies pass it back to
  > `CreatureState` and never dereference it, so `pCreature->X(...)` is
  > `CreatureState::X(pCreature, ...)`; a creature's status array is
  > `CreatureState::Status(pCreature)` (an `MStatus*`, through which
  > `SetStatus` still dispatches to the creature's override), and
  > `pCreature->SetStatus(...)` is the library helper
  > `CreatureState::SetStatus(pCreature, ...)`, which skips a creature with
  > no status rather than dereferencing NULL. `g_pZone->X`
  > is `ZoneState::X`, `g_pZone==NULL` and `!=NULL` are `!ZoneState::Exists()`
  > and `ZoneState::Exists()`, and the knock-back's `g_pPlayer->GetID()` is
  > a new `PlayerState::GetID`. Without a host there is no zone and no
  > creature: queries answer false, NULL, 0 or an empty name and actions are
  > skipped. `ClientDef.h` gives way to the host and library headers;
  > `GCDeleteEffectFromTileHandler` drops `MinTr.h` (rule M2) and its
  > "g_pZone is NULL" log line now reads "Zone is Not Init.. yet." like its
  > siblings, and `GCMoveHandler` loses a commented-out block that created
  > a creature through `g_pZone` (both counted by R4, which reads text) and
  > has its comments translated. R1 is 257 Windows / 255 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_zone_creature_handlers.cpp` (fake creatures behind opaque
    handles: tile effects and helicopters only with a zone; a death once,
    then the corpse; hit points and recovery on the named creature; custom,
    indexed and out-of-range nicknames; moves and facing attacks; NPC lines
    from the script or the message, or "......" for a refused name; a
    knock-back that resets other creatures but only pushes the player;
    missing hosts and entries).

- [x] **4.102 Replies handed on to the world and connections:** move
  `CRDisconnectHandler`, `GCAddNewItemToZoneHandler`,
  `GCAddVampirePortalHandler`, `GCBloodBibleSignInfoHandler`,
  `GCChangeWeatherHandler`, `GCLightningHandler` and `GCPetInfoHandler`
  into `gamemodel` behind `WorldEvents::Host`, installed and reset by
  `GameInit.cpp`.
  > **Status:** done (2026-10-13). Each reply calls one executable function
  > with what it read; the call becomes the entry of the same name
  > (`AddItemToZone`, `AddVampirePortal`, `SetBloodBibleSlot`, `SetWeather`,
  > `SetLightning`, `SetPetInfo`, `RequestDisconnect`), and the adapters call
  > those functions unchanged. `AddItemToZone`'s `BOOL` dropping flag is a
  > `bool` at the boundary and `FALSE` again in the adapter. The executable
  > headers go: `RequestFunction.h`, `PacketFunction.h`, `MFakeCreature.h`
  > and `ClientDef.h` give way to the host header (with `ConvertDuration.h`
  > for the portal and `DebugLog.h` for the new-item reply's debug output),
  > and `GCPetInfoHandler`'s local `extern` of `SetPetInfo` moves into
  > `PacketFunction.h` beside its siblings. Without a host nothing is added
  > and nothing is called. `GCAddInstalledMineToZoneHandler` stays: its mine
  > branch needs an `MMine`, whose vtable is the executable's.
  > R1 is 250 Windows / 248 Ninja.
  - Owner: membership, R1, designated host installation/reset and
    `test_world_event_handlers.cpp` (a disconnect only from a peer
    connection, by its name; the new item passed as the base packet, not
    dropping; the portal's owner, place, target and frame duration, started
    from the main node only for create flag 0; weather, lightning in
    milliseconds, the blood bible and pet info passed on; missing hosts and
    entries).

- [x] **4.103 The item factory's dispatcher:** move `MItem::NewItem` into
  `gamemodel` (`MItem.cpp`) over a factory slot, `MItemHost::NewItem`, and
  `GCTradeAddItemHandler` with it.
  > **Status:** done (2026-10-13). `MItem::NewItem` range-checks the class
  > from the wire, then asks the installed factory; without one, or for a
  > class the factory cannot make, it returns NULL, which every caller
  > already handles. The constructor table stays in `MItemUse.cpp` (its
  > classes' vtables reference the executable) as `NewItemOfClass`, which
  > keeps its own range and empty-slot checks and is installed by
  > `GameInit.cpp` in `s_ItemHost`. The symbol `MItem::NewItem` now
  > resolves in the library for the 28 objects that called it from the
  > executable (27 still there, plus the handler below), so it no longer
  > holds any of them back. `GCTradeAddItemHandler`, blocked by
  > nothing else, joins `gamemodel` with `DebugLog.h` for the `ClientDef.h`
  > it drops. R1 is 249 Windows / 247 Ninja.
  - Owner: membership, R1, `s_ItemHost`'s factory slot and
    `test_item_factory_handlers.cpp` (the range check before the factory;
    no factory or an unknown class gives NULL; an offered item built from
    every field of the packet into the other inventory, both acceptances
    withdrawn; an item that does not fit freed; an unknown class or no
    trade changes nothing; a belt's and an arms band's pockets filled, an
    unknown pocket class skipped; a sword's listed pockets not made). The
    gun branch, which builds an `MMagazine` (an executable vtable), is not
    reached by the test.

- [x] **4.104 Guild-list, guild-membership and character-error replies:**
  move `GCActiveGuildListHandler`, `GCWaitGuildListHandler`,
  `GCModifyGuildMemberInfoHandler`, `LCCreatePCErrorHandler` and
  `LCDeletePCErrorHandler` into `gamemodel` over the existing hosts.
  > **Status:** done (2026-10-13). `GuildPresentation::Host` gains
  > `ShowActiveGuildList`, `ShowWaitGuildList` and `SetGuild` (guild ID and
  > rank as `std::uint16_t` and `std::uint8_t`, `UI_SetGuild`'s `WORD` and
  > `BYTE`); `PlayerState::Host` gains `SetGuildNumber`; and
  > `LoginPresentation::Host` gains `PopupErrorMessage` (the error code as
  > an `int`, cast back to `ErrorID` by the adapter) and
  > `QueueWaitSelectPCMode` (`g_ModeNext = MODE_WAIT_SELECTPC`). The guild
  > lists' `MPlayer::WAIT_VERIFY_NPC_ASK` check reads
  > `PlayerState::GetWaitVerify()`. The two error replies lose their local
  > `extern` of `PopupErrorMessage`, which `GameInit.cpp` now declares for
  > its adapter. R1 is 244 Windows / 242 Ninja.
  - Owner: membership, R1, the hosts' new entries and adapters, and
    `test_guild_pc_error_handlers.cpp` (a guild list ends a pending NPC
    question before it is presented and leaves any other wait; a
    membership change records the guild name and grade, then the player's
    guild number, then presents it, with the name and grade recorded before
    either service is called; a refused creation or deletion shows its error
    code (any byte), then queues the return to selection; missing hosts and
    entries).

- [x] **4.105 The client mode and the message logs:** move the
  `CLIENT_MODE` enum and `g_Mode`/`g_ModeNext` (`ClientMode.h/.cpp`) and the
  message logs' pointers `g_pSystemMessage`, `g_pPlayerMessage`,
  `g_pNoticeMessage`, `g_pGameMessage` and `g_pHelpMessage`
  (`GameMessages.h/.cpp`) into `gamemodel`, with `GCLearnSkillReadyHandler`,
  `CRConnectHandler`, `RCCharacterInfoHandler`, `RCStatusHPHandler`,
  `GCPartySayHandler` and `RCSayHandler`.
  > **Status:** done (2026-10-13). The definitions leave `GameMain.cpp`
  > unchanged in value; the enum leaves `ClientDef.h`, which now includes
  > `ClientMode.h`, so every executable user compiles as before. `SetMode`,
  > which enters a mode with its side effects, stays the executable's
  > (`LoginPresentation::EnterSelectPCMode` and its kin). The executable
  > still creates the logs at start-up; only the pointers moved, like
  > `g_pUserOption` into `VS_UI`. The four replies that only read the mode
  > and logs move with their includes changed. The two says present
  > through `CommunicationPresentation::AddChatToHistory` and ask
  > `PlayerState::HasEffectStatus` (new) whether the player is a ghost;
  > their commented-out masking blocks, which named `g_pPlayer`, go (R4).
  > `g_pDebugMessage` stays in `GameMain.cpp` under `OUTPUT_DEBUG`. R1 is
  > 238 Windows / 236 Ninja.
  - Owner: membership, R1, the new modules, `PlayerState::HasEffectStatus`
    and `test_client_mode_message_handlers.cpp` (a ready skill domain is
    marked and announced, an out-of-range one refused; a peer accepted
    only in game and only for this character, with the reply bytes and
    the session's status, refused otherwise or without a connection or
    log; a peer's address and guild recorded while in or entering the
    game, by name or by address; its hit points only while out of sight;
    party and peer chat filtered, shown only in game from accepted names,
    unfiltered for a ghost, a game master or the master's words; missing
    hosts and entries).

- [x] **4.106 Skill and damage results on zone creatures and the player:**
  move `GCAttackArmsOK4Handler`, `GCSkillToObjectOK4Handler`,
  `GCSkillToInventoryOK2Handler`, `GCKnocksTargetBackOK4Handler`,
  `GCShopBoughtHandler` and `GCGetDamageHandler` into `gamemodel` over
  4.101's creature service.
  > **Status:** done (2026-10-13). `CreatureState::Host` gains `GetX`,
  > `GetY`, `SetEffectDelayFrame`, `PacketSpecialActionResult`,
  > `PacketSpecialActionToInventoryItem`, `AddEffectStatus`, `SetTurning`,
  > `IsNPC` and `GetShop` (an NPC's shop, asked only of an NPC; the adapter
  > casts to `MNPC`, which stays the executable's), and
  > `PlayerState::Host` gains `Creature`, the player as an opaque
  > `MCreature*`, so `GCGetDamage`'s player half calls the same creature
  > entries; `PacketSpecialActionResult` is virtual and `MPlayer` overrides
  > it, so the call reaches the player's override as before. The action
  > table, the shop and its shelves are already `gamemodel`'s.
  > `GCSkillToObjectOK4Handler` drops the unused `PacketFunction2.h`;
  > `SendBugReport`, which reached it through `ClientDef.h`, comes from
  > `Packet/WireHost.h` directly. The `GetShop` adapter answers no shop for
  > a creature that is not an NPC instead of casting it. R1 is 232 Windows
  > / 230 Ninja.
  - Owner: membership, R1, the new entries and adapters, and
    `test_creature_action_result_handlers.cpp` (arms skills land as their
    own result or the gun's default; a skill on an object delays, lands
    as its result (the graded step when the skill has steps), adds its
    effect for its converted duration and turns the target for Dragon
    Tornado; past the table, an unknown target or no zone, nothing; an
    inventory skill delays then acts; a knock-back moves, stops, then
    lands where it moved; damage on the player even without a zone, or
    on the named creature; an item bought from an NPC fills its shelf
    slot with the packet's fields, and nothing for a non-NPC, a shopless
    NPC, a missing shelf or an unknown class, the shelf's version kept,
    and no item for a slot past the shelf; every arms skill; a skill
    without effect or steps only lands, grade 0 takes no step, and only
    Dragon Tornado turns; without a zone no creature is looked up, and
    without a target none is touched; missing entries).
  - Follow-up: `GCShopBought` sets the shelf's version before
    `MShopShelf::SetItem` refuses a slot past the shelf (20 or more), so
    the shelf reads as current without the item, as before the move.
- [x] **4.107 The personal store and nickname replies:** move
  `GCAddStoreItemHandler`, `GCRemoveStoreItemHandler`,
  `GCAddNicknameHandler` and `GCNicknameListHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). A new `VsUiPresentation::Host` hands
  > the four replies what they read of the UI object (`gC_vs_ui`): which
  > inventory view is open, and its nickname list, filled through
  > `AddNickNameList` and `SetNickNameList`. A missing host or entry
  > answers inventory mode 0 (the store view closed) and skips the lists.
  > The store itself (`g_pStorage2`) is already `gamemodel`'s. A gun added
  > to the store is loaded through `MItem::LoadGun` (4.103) with one round,
  > as the magazine `MItem::NewItem` made started, instead of building the
  > magazine and touching `MMagazine`/`MGunItem` in the handler; a gun no
  > magazine can be made for is stored unloaded with a log line rather
  > than dereferencing a null magazine (the factory always makes one
  > today). R1 is 228 Windows / 226 Ninja.
  - Owner: membership, R1, the new host and its adapter, and
    `test_store_nickname_handlers.cpp` (an item added to the open store
    fills its first free slot with the packet's fields and price; a gun
    is loaded with one round whatever number the packet carries, or
    stored unloaded when it cannot be; a belt or an arms band keeps the
    pockets that fit, with their type and number, and frees the rest;
    another owner's store, a closed store (even for an owner of 0), no
    store or an unknown class adds nothing; a sold item leaves the
    packet's slot, and only that one, only while the store view is open
    (modes 1 and 2) and only from its owner's open store; nicknames reach
    the UI's list; a missing host or entry skips the UI and keeps the
    store view closed).
  - Follow-ups, all as before the move:
    - `GCRemoveStoreItem` drops the item `MStorage::RemoveItem` hands
      back without freeing it; the store view may still point at it, so
      freeing it needs that view's ownership read first.
    - `GCNicknameList` never frees the entries its `read` allocates.
    - `MStorage` leaves its open-store id uninitialised until a store
      opens (`GCMyStoreInfo` sets it only for a non-zero player), and
      both store replies compare against it.
    - `GCAddStoreItem` takes the first free slot rather than the
      packet's index, which `GCMyStoreInfo` and `GCRemoveStoreItem`
      use; ignores `isItemExist`, so an empty entry still makes an item
      of class 0; and ignores the item number, so a stack reads as 1.
- [x] **4.108 The wall see-through test and the peer disconnect request:**
  move `MImageObjectScreen.cpp` and `RequestFunction.cpp` into `gamemodel`.
  > **Status:** done (2026-10-13). `MImageObject::IsWallTransPosition`
  > stayed in the executable only for `MTopView::PixelToMapX`, which is
  > `WorldTileGeometry::PixelToTileX`; it now calls that directly.
  > `RequestDisconnect`, which `GameInit.cpp` installs as the world-event
  > host's entry, drops the named peer through the peer manager, which is
  > `packetwire`'s; it logs through `DebugLog.h` instead of the
  > executable's `DebugInfo.h`. R1 is 226 Windows / 224 Ninja.
  - Owner: membership, R1, and `test_wall_transparency.cpp` (a wall
    running down to the right hides the cells past its diagonal, one
    running up to the right the cells short of it, both measured from the
    wall's pixel X in whole tiles less or plus its viewpoint; anything not
    a wall hides every cell), with `test_peer_self_disconnect.cpp`, whose
    world-event host now calls the real `RequestDisconnect` (the named
    peer of the installed manager is dropped; without a manager nothing
    is).
- [x] **4.109 The peer file-send queue:** move `RequestFileManager.cpp`
  into `gamemodel`.
  > **Status:** done (2026-10-13). `RequestFileManager` keeps the files
  > other players asked this client for (a player's profile picture and its
  > index, which `CRRequestHandler` queues)
  > and sends them over each player's peer connection, which
  > `RequestServerPlayer::processCommand` pumps through the wire host's
  > `SendOtherRequest`. It reached nothing of the executable's: the
  > profile, server-info, type and UI headers it included were unused, and
  > it logs through `DebugLog.h` instead of `DebugInfo.h`. Its Korean
  > comments are translated. R1 is 225 Windows / 223 Ninja.
  - Owner: membership, R1, and `test_request_file_manager.cpp`, over a
    real peer connected over loopback whose far end decodes what arrives
    (each file is announced by an `RCRequestedFile` with its name, type
    (the index, then the picture, as `CRRequestHandler` queues them) and
    size, by a call that answers false, then sent in 4K chunks by
    calls that answer true; a whole-chunk read is not yet the end, the
    empty read after it is; after the last file, `RCRequestVerify`
    (`PROFILE_DONE`) and the request is forgotten; a file that cannot be
    opened is announced empty and the request still ends; one request
    per player, a second refused; a removed or unknown request sends
    nothing).
  - Follow-up: `CRRequestHandler`, which builds the requests, stayed in
    the executable for the profile manager until task 4.164 moved it.

- [x] **4.110 The frame clock and the help display:** move `g_FrameNow`'s
  definition into a new `FrameClock.cpp` and `MHelpDisplayer.cpp` into
  `gamemodel`.
  > **Status:** done (2026-10-13). `g_FrameNow`, the time the current
  > frame began, which the main loop stamps once per frame, was defined
  > in `Client.cpp`; it is now `FrameClock.cpp`'s, declared in
  > `FrameClock.h`, as 4.105 did for the client mode. The main loop still
  > stamps it (and so do the start of a packet wait and the zone load), and every
  > executable reader keeps its declaration (15 executable sources read
  > it). `MHelpDisplayer`, which shows a help
  > message unless the last one is still up, reached nothing else of the
  > executable's: the help table and the help log are `gamemodel`'s. Its
  > Korean comments are translated. R1 is 224 Windows / 222 Ninja.
  - Owner: membership, R1, and `test_help_displayer.cpp`, which sets the
    frame clock (a help is shown and keeps the next back for 10 seconds,
    the window's last moment included, and a help dropped meanwhile
    stays unshown; a help already shown is skipped, without holding the
    next back, until the display history is cleared; a help past the
    table is ignored and holds nothing back).

- [x] **4.111 The installed-mine reply:** move
  `GCAddInstalledMineToZoneHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). The reply adds a mine to the zone
  > through `WorldEvents::AddItemToZone` (4.102), without a drop, and
  > marks it installed through a new `MItemHost::SetMineInstalled`, as
  > 4.103 did for loading a gun: a mine's vtable is the executable's, so
  > `MMine` is touched only in `GameInit.cpp`'s adapter. The packet is a
  > `GCAddItemToZone`, so the cast the handler made is an implicit
  > upcast now. R1 is 223 Windows / 221 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_world_event_handlers.cpp` (a mine is added without a drop and
    marked installed; anything but a mine, or nothing added, is left as
    it is; without an item service, or with one that cannot mark a mine,
    the mine is added and left as it is; the mine reply is in the
    missing-host sweep).

- [x] **4.112 The login and registration error replies:** move
  `LCLoginErrorHandler` and `LCRegisterPlayerErrorHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). Both read `g_bNeedUpdate`, whether the
  > client is quitting to be updated, which was defined in `Client.cpp`;
  > it is now `ClientMode.cpp`'s, beside the client mode (4.105). The
  > login error pops its error through `LoginPresentation::PopupErrorMessage`
  > (4.104); the registration error shows its message through a new
  > `LoginPresentation::ShowMessage` entry (`g_ShowMessage`). A dead,
  > commented-out branch naming `g_pUIDialog` leaves the login error with
  > it. R1 is 221 Windows / 219 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_login_error_handlers.cpp` (a refused login shows its error,
    any byte passed on whole, and goes back to the login; a client
    quitting to be updated shows nothing and keeps its mode; a refused
    registration shows the message for each of its eight errors, the
    general one for any other, and goes back to the login, even while
    quitting to be updated, when it shows nothing; missing services skip
    the messages, not the mode).

- [x] **4.113 The whisper reply:** move `GCWhisperHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). A whisper reached the executable for
  > the chat history (`UI_AddChatToHistory`, now
  > `CommunicationPresentation::AddChatToHistory`) and for the player:
  > whether it is an operator (a new `PlayerState::IsOperator`, which is
  > `MCreature::IsOperator`, the same three creature types the handler
  > listed), a ghost, its race and its INT (through `PlayerState::Status`,
  > the player as its `MStatus`; clamped at 300, where every honest INT
  > already reads at the cap, before the int arithmetic, since the server
  > can set any INT; an INT past about 43 million, which the executable's
  > unsigned arithmetic wrapped below the cap, now reads at it). The chat filter, the user information,
  > the notice log and the string table are already `gamemodel`'s. A
  > dead `__METROTECH_TEST__` clause and commented-out code naming
  > `g_pPlayer` leave with it, and its Korean comments are translated.
  > R1 is 220 Windows / 218 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_whisper_handler.cpp` over a real chat filter with a curse list
    (an accepted whisper shows filtered as a whisper and makes the sender
    the target; an ignored sender's is dropped; an operator's shows
    whole even when ignored and goes to the notices, and "GM" counts
    only at the start of a name; an operator, master or ghost player sees
    it whole; the filter is forced even when the player turned it off; a
    ten-character name shows whole; another race's is masked, measured
    over 114 letters, heavily with no INT and lightly with 150, at the
    cap for any INT the server sets, a vampire player's too, with no INT
    without a player, and not at all in a debug build; missing services
    still set the target).
  - Follow-up: a race byte past the `Race` values is cast to the enum
    and compared, as before the move.

- [x] **4.114 The kick-warning reply:** move `GCKickMessageHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). A kick warning queues a zone event
  > that logs the player out after the packet's seconds. It reached the
  > executable's event manager (`g_pEventManager`, an `MEventQueue`) and
  > asked whether the player is a slayer. It now queues through a new
  > `WorldEvents::AddEvent` entry, whose adapter adds to the event manager
  > when there is one, and asks `PlayerState::GetRace` (a slayer is
  > exactly `RACE_SLAYER`). `MEvent` is `gamemodel`'s. R1 is 219 Windows /
  > 217 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_kick_message_handler.cpp` (an ended pay time queues the
    logout after the packet's seconds, with its reason and the
    countdown; an ended free play is worded for a slayer's level, or
    another race's; any other kind, or no world service, queues
    nothing).
  - Follow-up: a kind byte past the `KickMessageType` values is cast to
    the enum by `GCKickMessage::read`, as before the move.

- [x] **4.115 The made-item reply:** move `GCMakeItemOKHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). An item made with a skill reached the
  > executable to let go of the items locked for the skill
  > (`UI_UnlockItem`, now `DialogPresentation::UnlockItem`) and to change
  > the player's status (`AffectModifyInfo(g_pPlayer, ...)`, now over
  > `PlayerState::Status`, the player as its `MStatus`, whose `SetStatus`
  > the player overrides). The temp-mode register, the inventory and the
  > item factory are already `gamemodel`'s. R1 is 218 Windows / 216
  > Ninja.
  - Owner: membership, R1, and `test_make_item_handler.cpp` (the items
    locked for the skill are let go only when the skill was waiting for
    it, and the mode clears; the used stacks are recounted, an unknown
    ID skipped; the made item goes where the packet says with its
    fields, or is freed if it does not fit; the status changes; missing
    services skip the unlock and the status).
  - Follow-up: an item class the factory cannot make returns before the
    status change, so that change is lost, as before the move.

- [x] **4.116 The pet-stash reply:** move `GCPetStashListHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). The stash replaces the storage
  > (`g_pStorage`) with one holding a pet item per stashed pet, then
  > opens the pet storage window on it. It reached the executable only
  > for that window (`UI_RunPetStorage`, `UI_SetPetStorage`), now two
  > new `DialogPresentation` entries, `RunPetStorage` and
  > `SetPetStorage`. The storage, the item factory, `MPetItem` and the
  > experience table are already `gamemodel`'s. A slot whose item the
  > factory does not make as a pet item is now skipped (the item freed)
  > instead of dereferencing a failed cast; the game's factory always
  > makes one. R1 is 217 Windows / 215 Ninja.
  - Owner: membership, R1, the new entries and their adapter, and
    `test_pet_stash_handler.cpp` over the stash as the server sends it,
    each pet's item ID put in place (each stashed pet becomes a pet item
    in its slot with its kind, HP, level, attribute, food, experience
    left to its next level, abilities, option, item ID and days kept,
    and the window opens on the new storage; an empty stash still
    replaces the storage; a factory that makes no pet item skips the
    slot; missing services still build the stash).
  - Follow-ups, as before the move: `GCPetStashList` never frees the pet
    details its `read` allocates, and a stashed `PET_NONE` leaves its
    attribute level and food type uninitialised, which the reply reads.

- [x] **4.117 The system-message reply:** move `GCSystemMessageHandler`
  into `gamemodel`.
  > **Status:** done (2026-10-13). A system message goes to the system
  > log marked as one, unless the player hides its kind, and a repeat is
  > added again only once it has left the log; a ranger's chat and a
  > player message go elsewhere. It reached the executable for the
  > player's options (`g_pUserOption`, `VS_UI`'s) and the ranger chat
  > line. `CommunicationPresentation` gains `HideHolyLandMessages`,
  > `HideLairMessages` and `HideWarMessages` (the options, false without
  > them) and `SetRangerChatString`; which option hides which kind stays
  > in the handler. The logs and the string table are already
  > `gamemodel`'s. Two commented-out duplicate checks go, and the Korean
  > and garbled Chinese comments are translated. R1 is 216 Windows / 214
  > Ninja.
  - Owner: membership, R1, the new entries and their adapter, and
    `test_system_message_handler.cpp` (every other kind goes to the
    system log as "[System]<message>"; each option hides its own kind
    only; a repeat is added only once it has left the log; a ranger's
    chat and a player message go where they belong, unmarked, and not
    when empty; without the service nothing is hidden and no ranger chat
    shown).

- [x] **4.118 The refused rank bonus and the guild-union offers:** move
  `GCSelectRankBonusFailedHandler` and `GCUnionOfferListHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). A refused rank bonus ends the pending
  > request, says why through `DialogPresentation::PopupFreeMessageDlg`,
  > and replays the mouse once (`gbl_global_empty_move`, `VS_UI`'s), now
  > a new `DialogPresentation::EmptyMove`. The union offers become
  > letters on the union mail tab through two new `VsUiPresentation`
  > entries, `AddMail` (the window's width and height instead of a
  > Windows `SIZE`) and `AddMailContents`. The temp-mode register and the
  > string table are already `gamemodel`'s. R1 is 214 Windows / 212
  > Ninja.
  - Owner: membership, R1, the new entries and their adapters, and
    `test_rank_bonus_union_handlers.cpp` (the refused bonus clears the
    mode, pops the message and replays the mouse, in that order; each
    offer becomes a letter from its guild's master, on its date, worded
    for joining (wire kind 1) or leaving (2), with its text; an offer of
    any other kind stops the rest; missing services skip the
    presentation, not the request).

- [x] **4.119 The quest element and quest item replies:** move
  `GCExecuteElementHandler` and `GCGQuestInventoryHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). In a zone (`ZoneState::Exists`), a
  > quest element goes to the quest window to run and the quest
  > inventory becomes quest items (each listed type, durability 1) that
  > the window keeps. Both reached the UI object (`gC_vs_ui`), now two
  > new `VsUiPresentation` entries, `PushGQuestExecuteElement` and
  > `UpdateQuestItemInfo`; the forwarder of the second answers whether a
  > host took the items, and the reply frees them when none did. A
  > factory that makes no quest item (none in the game) skips the type
  > instead of dereferencing a null item. R1 is 212 Windows / 210 Ninja.
  - Owner: membership, R1, the new entries and their adapter, and
    `test_quest_element_inventory_handlers.cpp` (an element reaches the
    window only in a zone; the inventory becomes quest items of the
    listed types, durability 1, kept by the window, and an empty one
    still empties it; without a zone nothing happens, and without a
    window the items are freed; without a factory nothing is made).
  - Follow-up, as before the move: the UI object drops the items when it
    has no game window or no quest manager behind the entry.

- [x] **4.120 The quest-status reply:** move `GCGQuestStatusModifyHandler`
  into `gamemodel`.
  > **Status:** done (2026-10-13). A quest's new status becomes a UI
  > record (`UI_GQuestInfo`, the packet layer's) handed to the quest
  > window with the packet's kind less two; the window keeps a record it
  > takes, and the reply frees one it refuses. It reached the UI object
  > (`gC_vs_ui`), now a new `VsUiPresentation::ModifyQuestManagerInfo`
  > that answers false without a host, so the record is freed then too.
  > R1 is 211 Windows / 209 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_quest_status_handler.cpp` (the quest's ID and status and each
    mission's condition, index, status and arguments, untimed, reach the
    window with the kind less two; a refused record, or one with no
    window, is freed, which ASan watches).
  - Follow-up: `GCGQuestStatusModify` never frees the status its `read`
    allocates, and its constructor leaves that pointer uninitialised.
  - Follow-up: the quest window's `SetQuestManagerInfo` answers true for a
    quest with no XML element without keeping the record, which then
    leaks; the reply cannot tell.

- [x] **4.121 The party-leave reply:** move `GCPartyLeaveHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). A member leaving or being expelled
  > changes the party (`MParty`, `gamemodel`'s) and says so in the game
  > log; this player leaving, or being expelled, ends the whole party,
  > and a leave or this player's own expulsion that empties it closes
  > the party window and says it was disbanded. It reached the executable
  > for the zone and the player (now `ZoneState::Exists`,
  > `PlayerState::Creature`, `GetWaitVerify` and `SetWaitVerifyNULL`)
  > and for the party window (`UI_CloseParty`, now a new
  > `CommunicationPresentation::CloseParty`). R1 is 210 Windows / 208
  > Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_party_leave_handler.cpp` over a real party and game log
    (members leaving empty the party, which is then disbanded; someone
    not in it leaving changes nothing; this player leaving ends the
    party and closes its window; expelling is worded for who expelled
    whom, and only this player's expelling disbands an emptied party;
    being expelled ends and disbands it; a leave this player asked for
    is answered and another pending request stays; nothing happens
    without a zone, a player, the user information, the party or the
    log; without the communication service the party still goes).

- [x] **4.122 The shop-sold reply:** move `GCShopSoldHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). An item an NPC sold leaves its special
  > shelf (freed), and the sold-from shelf takes the packet's version.
  > It reached the executable for the zone, the NPC and its shop, now
  > 4.106's `ZoneState::GetCreature`, `CreatureState::IsNPC` and
  > `GetShop`, and for `UI_RemoveDescriptor`, which it called just
  > before deleting the item whose destructor already calls it (through
  > `MItemHost::ItemDestroyed`); that first call goes, so the descriptor
  > is dropped once. R1 is 209 Windows / 207 Ninja.
  - Owner: membership, R1, and `test_shop_sold_handler.cpp` (the special
    shelf loses the sold item, its descriptor dropped once, and takes
    the version, an empty slot too; another shelf keeps its items and
    takes the version; nothing changes for no zone, no creature, no NPC,
    no shop or no shelf).

- [x] **4.123 The party-error reply:** move `GCPartyErrorHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). A refused party request says why, and
  > an impossible invite ends the request this player made. It reached
  > the executable for the player (now `PlayerState::Creature`,
  > `GetWaitVerify` and `SetWaitVerifyNULL`), the message box (now
  > `DialogPresentation::PopupFreeMessageDlg`) and the party "cancel?"
  > box (`UI_ClosePartyCancel`, now a new
  > `CommunicationPresentation::ClosePartyCancel`). The dialog's
  > existence check goes with `g_pUIDialog`: without a dialog host the
  > messages are skipped and the request still ends. The installed
  > adapter reads `g_pUIDialog` unguarded, which is safe: it exists from
  > start-up, before any connection, until shutdown. The debug-only
  > logs keep their `OUTPUT_DEBUG` guard without naming
  > `g_pDebugMessage`. R1 is 208 Windows / 206 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_party_error_handler.cpp` (an absent, other-race or
    unsafe-place invite closes the box, answers the party wait, clears
    the mode, then closes the box again and says why; without a pending
    request only the box closes and the reason shows, and another wait
    or mode stays; the other refusals keep the request, and only the
    transformed and busy ones close the box; nothing happens without a
    player or the temp mode; missing services skip the UI).

- [x] **4.124 The stash reply:** move `GCStashListHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). The stash replaces the storage with
  > the packet's racks, each item in its rack and slot, sets the stashed
  > money and opens the stash window; with no stash it says so and keeps
  > the storage. It reached the executable for the window and the
  > message (`UI_RunStorage`, `UI_SetStorage`, now new
  > `DialogPresentation` entries `RunStorage` and `SetStorage`, and
  > `DialogPresentation::PopupFreeMessageDlg`), and built a gun's
  > magazine through `MMagazine`/`MGunItem`, which is now
  > `MItem::LoadGun` with the stashed count (4.103, 4.107). R1 is 207
  > Windows / 205 Ninja.
  - Owner: membership, R1, the new entries and their adapter, and
    `test_stash_list_handler.cpp` over the stash as the server sends it
    (each item goes to its rack and slot with the packet's fields, the
    old storage and its items freed; a gun is loaded with its count and
    keeps no number or silver; a belt or arms band keeps the pockets
    that fit; an unknown class is skipped and an unloadable gun stashed
    unloaded; the money is set and the window opens; no stash says so
    and keeps the storage; missing services still build the stash).

- [x] **4.125 The nickname-verify reply:** move `GCNicknameVerifyHandler`
  into `gamemodel`.
  > **Status:** done (2026-10-13). An accepted custom nickname change
  > shows the new nickname in the naming window (for a nickname item,
  > 22 or the timed 25), closes the window and says it changed; the
  > other answers only say why; every answer ends the pending request.
  > It reached the executable for the messages (`UI_PopupMessage`, now
  > `CommunicationPresentation::PopupMessage`) and the naming window
  > (`gC_vs_ui`, now new `VsUiPresentation` entries `ChangeCustomNaming`
  > and `CloseNamingChange`). Item types 22 and 25 share one case; the
  > others, which did nothing, are a comment. R1 is 206 Windows / 204
  > Ninja.
  - Owner: membership, R1, the new entries and their adapter, and
    `test_nickname_verify_handler.cpp` (an accepted nickname item shows
    the name, closes the window and says so; another item type only
    closes and says so; an accepted change nobody asked for only ends
    the request; the other answers say why; missing services still end
    the request).

- [x] **4.126 The zone flags, the war list and party positions:** move the
  current zone's flags into `gamemodel`, then `GCWarListHandler`,
  `GCPartyPositionHandler` and `RCPositionInfoHandler`.
  > **Status:** done (2026-10-13). The zone loading sets four flags the
  > replies read: whether the zone is safe (`g_bZoneSafe`), the large and
  > the small zone (`g_nZoneLarge`, `g_nZoneSmall`) and which one the
  > player is in (`g_bZonePlayerInLarge`). They were `GameMain`'s and are
  > now `ZoneFlags.cpp`'s, declared in `ZoneFlags.h`. The war list hands
  > each war to the war manager, makes the zone unsafe when a race war is
  > fought in it, plays the zone's music again and drops the war effect's
  > event; the zone (`g_pZone`) is now `ZoneState::Exists` and a new
  > `GetID`, and the music and the event manager are new `WorldEvents`
  > entries `PlayMusicCurrentZone` and `RemoveEvent`. A party member's
  > position, from the server (`GCPartyPosition`) or the member's client
  > (`RCPositionInfo`, which also refreshes the peer's address), is
  > stored with whether the member is in sight; the player (`g_pPlayer`)
  > is now `PlayerState::Creature`, its cell `CreatureState::GetX`/`GetY`
  > and its sight a new `PlayerState::GetSight`. R1 is 203 Windows / 201
  > Ninja.
  - Owner: membership, R1, the flags, the new entries and their
    adapters, and `test_war_list_party_position_handlers.cpp` (a race war
    in the current zone makes it unsafe; another war, or one elsewhere,
    does not; an empty list still plays the music and drops the effect;
    nothing happens without a zone or a war manager, and missing services
    still store the wars; a member's position and health are stored, in
    sight within one and a half times the player's sight in the zone the
    player is in, large or small; nothing changes for no name, a stranger,
    out of the game or without a player, the party, the log or the peer
    roster; a peer's address and position are stored by name or by
    address).

- [x] **4.127 The auth-key and sub-inventory replies:** retire the dead
  blocks that kept `GCAuthKeyHandler` and `GCSubInventoryInfoHandler` out
  of task 4.91, and move both into `gamemodel`.
  > **Status:** done (2026-10-13). Each body was one block no build
  > compiles. `GCAuthKeyHandler`'s called `Set_Auth`, nProtect's, under
  > `__NPROTECT__`, which only `Client.cpp` defines (for itself);
  > `Set_Auth` itself is defined nowhere since the SDL port removed
  > nProtect, and the handler's `Client.h` include served that call.
  > `GCSubInventoryInfoHandler`'s ran the test sub-inventory under
  > `__TEST_SUB_INVENTORY__`, which no CMake build defines (only the
  > orphan `Client/SpriteLib/SpriteLib.vcxproj`, which nothing builds,
  > names it), and named `g_pZone` and `g_pPlayer`. Both handlers
  > now accept their packet and do nothing, as they did. The rest of the
  > sub-inventory feature's dead blocks elsewhere stay. R1 is 201 Windows /
  > 199 Ninja.
  - Owner: membership, R1, and `test_ignored_packet_handlers.cpp`, which
    now decodes both packets, runs both handlers and requires each packet
    to encode to the same bytes; removing either file from the membership
    breaks the test link on its `execute()`.

- [x] **4.128 The peer whisper:** move `CRWhisperHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). Another client whispers over its own
  > connection, which closes after every whisper. A whisper for this
  > character, in this world, while in or entering the game, records the
  > sender's address; if the player accepts the sender, each message is
  > shown as a whisper and the sender becomes the whisper target, filtered
  > and masked as `GCWhisperHandler` does (4.113), except that an operator
  > player is not exempt from the filter. Any other whisper is
  > answered "whisper failed". It reached the executable for the player
  > (`g_pPlayer`, now `PlayerState::Creature`, `HasEffectStatus`,
  > `GetRace` and `Status`) and the chat history (`UI_AddChatToHistory`,
  > now `CommunicationPresentation::AddChatToHistory`). The race mask
  > reads INT as 4.113's does, clamped at 300 before the int arithmetic;
  > the percent is unchanged until INT 42,949,673, where the old unsigned
  > product wrapped (and read anywhere from 25% to 75%). A
  > `__METROTECH_TEST__` line no
  > build compiles and two commented-out blocks are dropped. R1 is 200
  > Windows / 198 Ninja.
  - Owner: membership, R1 and `test_peer_whisper_handler.cpp` (each
    message is filtered, shown and makes the sender the target; a known
    sender's address is kept; a whisper for another character, world or
    mode, or without a player, chat filter or user information, is
    answered "whisper failed"; one from someone ignored is dropped but its
    address kept; an operator's shows whole and goes to the notices; a
    master or ghost player sees it whole; another race's is masked less
    the more INT, and any INT reads as the cap; without a chat service the
    target is still set).

- [x] **4.129 The action-result nodes:** move `MActionResult.cpp`, the
  results a skill reply queues, into `gamemodel`; its queue
  (`MActionResultQueue.cpp`) moved in task 4.31.
  > **Status:** done (2026-10-13). Nineteen node classes run when the
  > action reaches them: a skill's result on its target, a creature or
  > the player moved, the player's status, a death, a burrow,
  > (in)visibility, snipping, an effect status, a creature-type change, a
  > casket, a turn, and an inventory item removed, re-optioned or
  > re-gendered. Their reaches into the executable are now host entries:
  > the zone (`g_pZone`, `ZoneState::Exists`/`GetCreature`); the player
  > (`g_pPlayer`, `PlayerState::Creature`/`GetID`/`Status`/
  > `SetServerPosition` and new `SetUndergroundCreature` and item-check
  > buffer entries, since `MPlayer` hides the creature's burrow); twenty
  > new `CreatureState` entries for the creature's visibility, movement
  > kind, type, gender, body colors, server cell, casket, move buffer and
  > Bloody Zenith, and a grade on `PacketSpecialActionResult` (default 0,
  > so 4.106's callers are unchanged); the skill in hand
  > (`gC_vs_ui`, new `VsUiPresentation::SelectSkill`/`UnselectSkill`); the
  > fanfare (`PlaySound`, new `WorldEvents::PlaySound`). The morph type a
  > creature-type change clears (`g_MorphCreatureType`) was
  > `GCUpdateInfoHandler`'s and is now `MorphState.cpp`'s. Calls on the
  > player that `MPlayer` overrides (`SetDead`, `AddEffectStatus`) go
  > through the creature entries, which dispatch virtually as before. The
  > dead effect-generation and sound blocks, commented out, and the help
  > events go; the Korean comments are translated. R1 is 199 Windows /
  > 197 Ninja.
  - Owner: membership, R1, the entries and adapters, `MorphState`, and
    `test_action_result_nodes.cpp` (a skill lands as its result, three
    skills as another's, Bloody Zenith marks its user, Blitz Sliding lands
    on the user, a graded skill with its step; nothing without a target,
    zone or result row; a position change moves the player or a creature;
    a status change applies to the player only, value by value; death,
    burrow, (in)visibility, snipping, turning and effect status reach the
    player or the creature, and nothing without a zone, a player or the
    creature; a type change keeps the gender and colors, a werewolf's
    second color as its first, flying by the type table, a buried
    creature unchanged, and the player's skill in hand follows the new
    form and the morph ends; a casket closes on a stopped creature at its
    server cell and opens; an inventory item is removed and freed with
    the held item; options change, add or upgrade with the fanfare; the
    held item decides a pet's revival, enchants, a mixing clear or a
    grade; a gender change flips the type and refreshes the item; missing
    services skip their part).

- [x] **4.130 Special-action skill replies:** move
  `GCKnocksTargetBackOK5Handler`, `GCSkillToTileOK3Handler`,
  `GCThrowItemOK3Handler` and `GCBloodDrainOK3Handler` into `gamemodel`.
  > **Status:** done (2026-10-13). Each is a skill between two creatures
  > in the zone, neither of them this player: the user faces its target
  > or cell and acts, handing the action its result queue (4.129's
  > nodes). A knock-back's queue hits the target and moves it, and the
  > target's server cell is set at once; a tile skill acts on its cell, as
  > its step for a graded skill; holy water hits its target; a blood drain
  > also tells the target who drains it, clears the user's stop and marks
  > the target drained for twelve hours. `CreatureState` gains the four
  > special actions (`PacketSpecialActionToSector`, `ToOther`, `ToNobody`
  > and `ToSelf`), which take the result queue as the members do and free
  > it when there is no entry to take it, and `SetDrainCreatureID` and
  > `ClearStopBloodDrain`. The Korean comments are translated. R1 is 195
  > Windows / 193 Ninja.
  - Owner: membership, R1, the new entries and their adapters, and
    `test_special_action_replies.cpp`, whose host runs each handed-over
    queue at once so its nodes show in the record (a knock-back hits,
    moves and sets the target's server cell; a tile skill acts on and
    faces its cell, as its step when graded; holy water and a blood drain
    hit the target, the drain also naming the drainer, clearing the
    user's stop and marking the target drained; nothing without a zone,
    either creature or a skill in the table; without the creature service
    nothing is done).

- [x] **4.131 Basic-action attack and skill replies:** move
  `GCAttackArmsOK3Handler`, `GCAttackArmsOK5Handler`,
  `GCAttackMeleeOK3Handler` and `GCSkillToObjectOK3Handler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). Attacks and skills between creatures
  > in the zone that fall back on the attacker's basic action (its
  > weapon's), now a new `CreatureState::GetBasicActionInfo`: a gun fired
  > at a cell or at a target, where six gun skills act as themselves; a
  > melee blow; a skill aimed at an object, as its step when graded and
  > as the basic action for a weapon skill unless that is a result. With
  > only the target in the zone it is shown hit, with only the attacker
  > attacking nobody (four of the six gun skills as themselves, Quick
  > Fire and Harpoon Bomb as the weapon). The Korean comments are
  > translated, and 4.130's knock-back reply's "The target creature."
  > comment, which named the wrong creature, now reads "The creatures
  > involved.". R1 is 191 Windows / 189 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_basic_action_replies.cpp` (each reply with both creatures,
    with one, with neither, and without a zone; every own-action gun
    skill; a miss acts without a result; a graded skill as its step and
    grade 0 as itself; a weapon skill as the basic action, or nothing
    when that is a result; a skill past the table).

- [x] **4.132 Player-target skill replies:** move `GCBloodDrainOK1Handler`,
  `GCBloodDrainOK2Handler`, `GCKnocksTargetBackOK2Handler` and
  `GCThrowItemOK2Handler` into `gamemodel`.
  > **Status:** done (2026-10-13). Skill replies between the player and a
  > creature, each also carrying the player's changed status. The player
  > drains a creature (the wait for the skill is answered, the creature
  > learns who drains it and is hit and marked drained, the player's
  > drain goes on); a creature drains the player, knocks it back or hits
  > it with holy water (the creature faces the player and acts, and the
  > player is hit; a drained player takes the effect its status names).
  > The player (`g_pPlayer`) is now `PlayerState::Creature`, `GetID`,
  > `Status` and the wait-verify entries, with its creature calls through
  > `CreatureState` on that handle (the overridden `SetServerPosition`,
  > `SetStop`, `PacketSpecialActionResult` and `AddEffectStatus` dispatch
  > virtually, as the member calls did). Without a player nothing
  > happens; the game dereferenced the NULL player on every path but
  > holy water's with no thrower and an empty status. Commented-out
  > blocks that named `g_pPlayer` go; the Korean comments are translated.
  > R1 is 187 Windows / 185 Ninja.
  - Owner: membership, R1, and `test_player_target_replies.cpp` (the
    player's drain answers its wait, hits and marks the creature and goes
    on, or only goes on without the creature; a creature draining the
    player faces it, acts and hits it, or the player is still shown
    drained without it, and a drained player takes its effect for the
    converted duration; a knock-back moves, stops and hits the player,
    with or without the attacker, and without a zone or skill only
    answers the wait; holy water hits the player; every reply applies the
    status, except a knock-back that returns early; without a player
    nothing happens).

- [x] **4.133 Player attack replies:** move `GCAttackArmsOK1Handler`,
  `GCAttackMeleeOK1Handler`, `GCKnocksTargetBackOK1Handler`,
  `GCThrowItemOK1Handler`, `GCAttackArmsOK2Handler` and
  `GCAttackMeleeOK2Handler` into `gamemodel`.
  > **Status:** done (2026-10-13). The player's own attack (a gun, a
  > melee blow, a knock-back, holy water) answers the wait for it and
  > hands the player's current action its result, now a new
  > `PlayerState::PacketAddActionResult` (which takes the queue, as
  > `MPlayer`'s does, and frees it unrun without an entry): the creature
  > hit with the skill or the player's basic action, or moved. A gun's
  > magazine takes the bullets left through a new
  > `PlayerState::SetCurrentMagazineNumber`, since `MMagazine` keeps its
  > vtable in the executable. A creature's attack on the player (a gun, a
  > melee blow) faces the player and acts, the player hit with the
  > creature's skill or basic action; without the creature the player is
  > still shown hit. Each but holy water applies the player's status.
  > Without a player nothing happens. Dead blocks naming `g_pPlayer` go;
  > the Korean comments are translated. R1 is 181 Windows / 179 Ninja.
  - Owner: membership, R1, the new entries and their adapters, and
    `test_player_attack_replies.cpp` (each reply's hit, with the player's
    basic action and every own-action gun skill; misses and missing
    targets only spend bullets; the knock-back's move and server cell;
    a creature's shot or blow at the player, with or without the
    creature; no zone; no player; without the entries the queue is freed
    unrun and no bullets are set).

- [x] **4.134 The result-node factory and two self-skill replies:** move
  `CreateActionResultNode` from `PacketFunction.cpp` into `MActionResult.cpp`,
  and `GCSkillToSelfOK2Handler` and `GCSkillToSelfOK3Handler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). The factory makes the node a skill's
  > result kind in the action table names (burrow, invisible, visible,
  > snipping, turning, casket, fake death, event; a Typhoon only at grade
  > 1), clearing a visible creature's invisibility and snipping and
  > marking a faking creature, over new `CreatureState` entries
  > `RemoveEffectStatus` and `SetFakeDie`; it is declared in
  > `MActionResult.h`, and `PacketFunction2.h`, its old home, includes
  > that. `WorldEvents` gains `ExecuteActionInfoFromMainNode`, which starts
  > an action's effect from the effect generator's main node and takes the
  > result queue (freed unrun without an entry). A skill on oneself acts
  > with a queue that hits the creature (as the skill's step when graded)
  > for the converted duration, adds the skill's effect status and the
  > factory's node; a skill at a cell starts its result effect there. R1
  > is 179 Windows / 177 Ninja.
  - Owner: membership, R1, the entries and adapters, the factory tests in
    `test_action_result_nodes.cpp` (each result kind, its side effects
    and what its node does; other kinds and an ungraded Typhoon make
    nothing) and `test_self_skill_replies.cpp` (the self skill's hit,
    effect status and factory node, its step, and its guards; the cell
    skill's effect and step and its guards; the effect entry frees a
    queue it is not there to take).

- [x] **4.135 The cross-counter replies:** move `GCCrossCounterOK1Handler`,
  `GCCrossCounterOK2Handler` and `GCCrossCounterOK3Handler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). A cross counter is shown as a fading
  > copy of the user striking back at the target, which
  > `SkillCrossCounter` builds from an `MFakeCreature`, the executable's;
  > it is now a new `WorldEvents::SkillCrossCounter`. The player's own
  > counters a creature (answering the wait for the skill), a creature's
  > counters the player, or one creature's counters another; the
  > player's two also carry its status, and a missing creature is passed
  > on as none, as before; without a player, the player's two do
  > nothing. The handlers no longer include
  > `MFakeCreature.h`. R1 is 176 Windows / 174 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_cross_counter_replies.cpp` (each counter's user and target, the
    wait, the status, a missing creature, no zone, no player).

- [x] **4.136 The add-effect and corpse-head replies:** move
  `GCAddEffectHandler` and `GCRemoveCorpseHeadHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). An effect status lands on a creature
  > standing in the zone or lying in its corpse: its sound plays, its
  > action effect starts for six statuses (with their own delays, a
  > triggered trap from the main node), and it is added for the converted
  > duration, except doom, critical wounds and a third back stab; a siege
  > laser is hidden from attacking players, and a trying status flashes
  > the regen tower's bead. A creature standing, or lying in its corpse,
  > loses its head. The corpse lookup (`MCorpse` is the executable's) is a
  > new `ZoneState::GetCorpseCreature`, which checks the item is a corpse as
  > both replies did; a corpse with no creature is now skipped where
  > `GCRemoveCorpseHead` dereferenced it, and the two error logs for no
  > item and a non-corpse item are one. `CreatureState` gains
  > `GetDirection` and `RemoveHead`; the sound and the action are
  > `WorldEvents`'. R1 is 174 Windows / 172 Ninja.
  - Owner: membership, R1, the new entries and adapters, and
    `test_corpse_effect_replies.cpp` (the status with and without its
    sound, on a standing or lying creature; the three not added; the six
    action statuses and their delays; the siege laser per attacker status
    and without a player; the regen bead; the head; the guards).

- [x] **4.137 The bomb replies:** move `GetMineActionInfo` and
  `GetBombActionInfo` from `PacketFunction.cpp` into a new gamemodel
  `ExplosiveActions.cpp`, and `GCThrowBombOK1Handler`,
  `GCThrowBombOK2Handler` and `GCThrowBombOK3Handler` into `gamemodel`.
  > **Status:** done (2026-10-13). A mine or bomb item's type names its
  > action (five each; -1 for any other type); the two lookups are
  > declared in `ExplosiveActions.h`, which `PacketFunction.h` includes. A
  > bomb lands at a cell: the queue hits the cell (the thrower, or nobody
  > when the action's result is an action of its own), then adds, for
  > each creature it lands on, the action's effect status and the node its
  > result kind names. The player's bomb answers the wait and goes to the
  > player's current action; another creature's is thrown from its user
  > facing the cell (a new `CreatureState::SetDirection`), or starts at
  > the cell without the user; the player's pending effect status rides on
  > a bomb near the player. Without a player, the player's bomb and a
  > bomb near the player do nothing, where they dereferenced the player.
  > R1 is 171 Windows / 169 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_bomb_replies.cpp` (the lookups at and past their ends; the
    player's bomb with an effect status and a factory node, with an
    action result on each creature, as the weapon, and not when that is
    a result; unknown types and no zone; a creature's bomb, with and
    without the player; without its user; the player's pending effect;
    the guards).

- [x] **4.138 The unburrow, remove-effect and change-shape replies:** move
  `GCUnburrowOKHandler`, `GCRemoveEffectHandler` and
  `GCChangeShapeHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). The player comes up from a burrow at
  > the server's cell: it answers a waiting skill, moves and turns, lands
  > through a new `PlayerState::SetGroundCreature` (MPlayer's own, which
  > hides the creature's), and plays the burrowing effect where it
  > stands. Effect statuses leave a creature standing, or lying in its
  > corpse (the 4.136 corpse lookup): a sweeper's end sounds, a fade-out
  > fades back in through a new `CreatureState::SetVisibleSoon`, and when
  > the player loses the aura shield's status the shield is ready again
  > at once. A creature's shape changes by an addon item made for the
  > packet: a creature wearing gear (a new `CreatureState::IsWear`) stops,
  > puts on a copy (`SetAddonItem`) and takes the weapon speed
  > (`SetWeaponSpeed`); the item is freed. Without a player the unburrow
  > reply does nothing, where it dereferenced the player. R1 is
  > 168 Windows / 166 Ninja.
  - Owner: membership, R1, the five entries and their adapters, and
    `test_shape_unburrow_replies.cpp` (with and without a waiting skill,
    and without a player; every sweeper status and its neighbours, the
    corpse, nobody, no zone; the shield's reuse time on the player alone
    and for that skill alone; the addon with and without the quest flag
    and an option; a creature not wearing gear, nobody, no zone and an
    item class nothing makes, with the item freed every time).

- [x] **4.139 The skill-to-tile replies:** move `GCSkillToTileOK1Handler`,
  `GCSkillToTileOK2Handler`, `GCSkillToTileOK5Handler` and
  `GCSkillToTileOK6Handler` into `gamemodel` (OK3 has been there since
  4.130; OK4 follows in 4.140).
  > **Status:** done (2026-10-13). A skill cast at a cell builds a queue
  > that hits the cell (the caster, or nobody when the skill's result is
  > an action of its own), then adds, for each creature it lands on, the
  > skill's effect status and the node its result kind names. A weapon
  > skill acts as the caster's basic action (not Piercing), a skill with
  > steps as the step its grade names, and a graded skill's cell node
  > carries the range. The player's skill (OK1) answers the wait, marks
  > the cell for Burning Soul (a new `PlayerState::SetTempSelectPos`) and
  > goes to the player's current action. Another creature's is cast from
  > the caster facing the cell (OK2 near the player, OK5 elsewhere);
  > without the caster near the player it starts at the cell, and without
  > one at all (OK6) where the server says it came from. The player's
  > pending effect status rides on a skill near the player. Upstream's
  > quirks are kept: a graded cell node near the player or elsewhere is
  > credited to the player, and OK2's action nodes act as the skill
  > itself where OK1 and OK5 act as the weapon. Without a player, OK1, OK2
  > and OK6 do nothing, where they dereferenced the player. A Burning
  > Soul launch near the player plays its launch effect only with its
  > caster; it read the missing caster and crashed. R1 is 164 Windows /
  > 162 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_skill_tile_replies.cpp` (each reply's queue, turn, steps,
    grades, weapon and Piercing, Burning Soul and Rotten Apple, the
    pending effect, the direction past eight, and the guards).

- [x] **4.140 The sector skill replies:** move `GCMineExplosionOK1Handler`,
  `GCMineExplosionOK2Handler` and `GCSkillToTileOK4Handler` into
  `gamemodel` over a new `WorldEvents::SkillToSector`.
  > **Status:** done (2026-10-13). `SkillToSector` (PacketFunction.cpp)
  > starts a skill's effect at a cell with nobody casting it; it places
  > the effect on the view and asks the effect generators, so it stays
  > in the executable behind the new entry, which takes the result queue
  > (freed unrun without a host). A mine going off near the player (OK1)
  > or elsewhere (OK2) acts as itself on each creature it lands on, by
  > that creature, after adding the mine's effect status; OK1 also
  > changes the player's status, now only when there is a player (it
  > dereferenced the player). An unseen caster's skill (OK4) adds the
  > node its result kind names (the result value's action; a graded
  > skill hands it the range, which the result applies when the value's
  > row is graded too and the range is not zero), acts as its step, and
  > starts as its result, facing down unless its range is its
  > direction. R1 is 161 Windows / 159 Ninja.
  - Owner: membership, R1, the new entry and its adapter, and
    `test_sector_skill_replies.cpp` (a mine near the player with its
    status, of no type, without a zone or a player; a mine elsewhere;
    the unseen caster's direction, result value, grade, step and
    guards; the forwarder's hand-over and free).

- [x] **4.141 The take-off and motorcycle get-off replies:** move
  `GCTakeOffHandler`, `GCGetOffMotorCycleHandler` and
  `GCGetOffMotorCycleOKHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). Three entries are new:
  > `CreatureState::RemoveAddon` (the adapter casts to `MCreatureWear`
  > only after `IsWear`), `CreatureState::SetMoveDevice`, and
  > `WorldEvents::PlaySoundAt`, a sound heard from a cell. `MOVE_DEVICE`
  > moves from `MCreature` into `CreatureState`, as `WAIT_VERIFY` moved
  > into `PlayerState`; `MCreature` brings it back into its scope with
  > `using enum`, so `MCreature::MOVE_DEVICE_WALK` still names it. A
  > creature wearing gear stops and takes an addon off. A creature gets
  > off its motorcycle: the motorcycle is no longer drawn, it walks, and
  > the player hears it from where the player stands; without a player
  > there is no sound, where the reply dereferenced the player. The
  > player's own answer does the same for the player, answers the wait,
  > and plays the sound. R1 is 158 Windows / 156 Ninja.
  - Owner: membership, R1, the three entries and their adapters, the
    enum's move, and `test_addon_off_replies.cpp` (a wearing creature,
    one not wearing gear, nobody, no zone; the sound from the player's
    cell, and none without a player; the player's answer; the missing
    hosts and the enum's values).

- [x] **4.142 The unseen skill on the player and the untransform reply:**
  move `GCSkillToObjectOK6Handler` and `GCUntransformOKHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). An unseen caster's skill lands on the
  > player: it starts from the cell the server names, facing the player
  > (`SelectFacingDirection`, gamemodel's, which the executable's
  > `GetDirectionToPosition` forwarded to through `MTopView`), as the
  > packet's skill's result, and its queue adds to the player the
  > pending effect status, or else the skill's own, and the skill's (or
  > its step's) action. A dead block that named the player is removed.
  > The player turns back into a vampire: the drawn type becomes the
  > coat's, the plain vampire's or an operator's (a creature-type node
  > on the queue), the player waits a second (a new
  > `PlayerState::SetDelay`, MPlayer's own), stands at the server's cell
  > and faces its direction at once (a new
  > `CreatureState::SetCurrentDirection`), and a werewolf takes back its
  > skin and coat colours. `CreatureState::GetCreatureType` and
  > `GetCompetence` are new too. Without a player both do nothing, where
  > they dereferenced the player. R1 is 156 Windows / 154 Ninja.
  - Owner: membership, R1, the four entries and their adapters, and
    `test_untransform_replies.cpp` (the facing, the pending and the
    skill's effect, the step against the packet's skill, the guards;
    each type the untransform names, the wait, the stand, and a
    werewolf's colours with and without a coat).

- [x] **4.143 The say and global chat replies:** move `GCSayHandler` and
  `GCGlobalChatHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). A creature's words show over its head
  > (`CreatureState::SetChatString`, which now takes the packet's colour;
  > the NPC replies keep MCreature's white default) and, for a player
  > character, in the history (`CommunicationPresentation::
  > AddChatToHistory`). Refused words show as "......" unless an operator
  > (or Gilles de Rais) said them. Curses are filtered from a player
  > character's words unless an operator said them, the player is one,
  > or the player is a ghost. Another race's words are masked in a zone
  > that masks by race, under its hallucination name, unless the player
  > reads them (the Hillel skill, translation, an operator type); a
  > hallucinating player sees any words masked (a debug build masks only
  > a player character's). A shout names its sender before the first
  > space. New entries: `CreatureState::GetHalluName`, `IsSlayer`,
  > `IsOusters` and `GetRace`; the player's race, effects and INT come
  > through `PlayerState`. Without a player, or its status, both do
  > nothing; most of their paths dereferenced the player. A debug build
  > (`_DEBUG`) masks by race nowhere, as before. Found by reading and
  > kept, as upstream's: a shout's race mask lands on the copied sender
  > name, so its words arrive unmasked; both replies read the current
  > zone's row in the zone table unchecked (a zone missing from the
  > table crashes, as before); and unlike the whisper replies, the
  > player's INT is not clamped to 300 before the mask's arithmetic. R1
  > is 154 Windows / 152 Ninja.
  - Owner: membership, R1, the entries and adapters, the colour on
    `SetChatString`, and `test_chat_replies.cpp` (filtering and its
    exemptions, refusal, the race mask and every way to read past it,
    hallucination, the shout's sender and its unmasked words, the
    guards, the missing host).

- [x] **4.144 The learn-skill, cannot-add and remove-from-gear replies:**
  move `GCLearnSkillOKHandler`, `GCCannotAddHandler` and
  `GCRemoveFromGearHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). A learned skill joins its domain and
  > the usable set, starts at level 1 with its full reuse delay, and is
  > announced; the usable set is refreshed through
  > `BonusSkills::RefreshAvailableSkills` (the executable's
  > `SetAvailableSkills`). An ousters that already knows the skill raises
  > its level instead; without a player there is no ousters, where the
  > reply dereferenced the player for a skill already known. An item the
  > server would not take leaves the item check buffer, back on the mouse
  > (a new `DialogPresentation::PickUpItem`) when it was being dropped to
  > the zone. `ITEM_CHECK_BUFFER` moves from `MPlayer` into
  > `PlayerState`, as `WAIT_VERIFY` did, behind a new
  > `PlayerState::GetItemCheckBufferStatus`; `MPlayer` brings it back with
  > `using enum` and loses the elaborated `enum` keyword on its five uses.
  > A broken piece of gear comes off the player: it is announced with its
  > options (not a couple ring, except for an ousters), the player stops
  > and the addon it drew comes off, and it is freed (its description
  > closes with it, through the item's destroy hook); the
  > `OUTPUT_DEBUG` log no longer prints the addon's frame. Without a
  > player the last two do nothing, where they dereferenced it. R1 is
  > 151 Windows / 149 Ninja.
  - Owner: membership, R1, the entries and adapters, the enum's move,
    and `test_gear_learn_replies.cpp` (learning, an ousters' level, a
    known skill learned again, unknown domains and skills; each branch's
    buffer state; each race's broken pieces, options, couple rings, a
    two-handed weapon, empty slots, no player).

- [x] **4.145 The skill-failed replies:** move `GCSkillFailed1Handler` and
  `GCSkillFailed2Handler` into `gamemodel`.
  > **Status:** done (2026-10-13). The player's skill failed: the wait is
  > answered, a skill to the inventory leaves the item check buffer, an
  > ousters' soul absorption or a blood drain stops (new
  > `CreatureState::SetStopAbsorbSoul` and `SetStopBloodDrain`), a
  > slayer's Eternity waits out its delay and Will of Life its level's
  > (new `GameInfoPresentation::SetDelayEternity` and `GetCharInfoLevel`),
  > Soul Chain cannot trace (a new `PlayerState::SetCannotTrace`),
  > Burning Soul loses its charge, and the skill is ready again unless
  > its row ignores a failure; the status changes too. Another
  > creature's skill failed: it shows the skill (its weapon's for a plain
  > attack, its step's for a grade) failing on its target, on nobody at
  > its cell, on itself, or at its cell; an ousters absorbing a soul
  > finds it in a corpse and stops; a blood drain stops; a grenade shows
  > nothing. The corpse came from an unchecked cast of whatever item had
  > the target's ID; the move kept it in a temporary entry, and a fix
  > that follows (moved, then fixed) takes it through 4.136's
  > `ZoneState::GetCorpseCreature`, so an item that is no corpse holds
  > no soul.
  > Without a player the player's reply does nothing, where it
  > dereferenced the player. R1 is 149 Windows / 147 Ninja.
  - Owner: membership, R1, the entries and adapters, and
    `test_skill_failed_replies.cpp` (the reuse time and its exemption,
    Will of Life, each undoing, the guards; each target kind, the
    weapon and the step, the corpse and an item that is none, the
    drain, the grenade, the guards).

- [x] **4.146 The motorcycle ride and gear-to-zone replies:** move
  `GCRideMotorCycleHandler`, `GCRideMotorCycleOKHandler` and
  `GCAddGearToZoneHandler` into `gamemodel` over three new zone entries,
  `ZoneState::GetItem`, `RemoveItem` (which frees the item) and
  `AddItem` (which takes it).
  > **Status:** done (2026-10-13). A creature wearing gear rides off on
  > a motorcycle lying in the zone: the motorcycle goes on as an addon
  > (a copy) and leaves the zone, and the player hears it from where the
  > player stands; with no such creature the motorcycle just leaves the
  > zone; anything that is not a motorcycle stays. The player's own ride
  > does the same for the player and answers the wait. A piece of the
  > player's gear falls into the zone, or without a zone is freed.
  > Without a player the ride is not heard, though MZone's RemoveItem
  > still dereferences the player first, so the executable still crashes
  > there without one; and the player's ride and the gear drop do
  > nothing, where they dereferenced the player. R1 is
  > 146 Windows / 144 Ninja.
  - Owner: membership, R1, the three entries and their adapters, and
    `test_ride_gear_replies.cpp` (the ride, the sound and its absence,
    each refusal, the riderless motorcycle, the player's ride, each
    race's dropped piece, no zone, an empty slot, no player, the
    missing host).

- [x] **4.147 The shop purchase and sale replies:** move
  `CheckItemForSkillIcon` from `PacketFunction.cpp` into a new gamemodel
  `ItemSkillIcons.cpp`, and `GCShopBuyOKHandler` and
  `GCShopSellOKHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). Some items change the player's skill
  > icons while carried (a slayer's holy water, portal items, bombs,
  > mines and bomb materials; a vampire's portal items and other vampire
  > goods); gaining or losing one refreshes the usable skills through
  > `BonusSkills::RefreshAvailableSkills`. The race test reads the
  > player through `CreatureState::IsSlayer` and a new `IsVampire`;
  > without a player nothing changes, where it dereferenced the player.
  > It was declared by hand in each user; `ItemSkillIcons.h` declares it
  > now. A purchase the player waited for is made and put in the chosen
  > inventory cell, replacing a stack of the same item there unless the
  > total passes the stack's maximum, or freed when it does not fit; a
  > special shelf loses it, and the shelf takes the shop's new version;
  > a normal shop sets the money, a star shop takes the stars the item
  > costs. A sale the player waited for takes the item out of the
  > inventory (every skull, for a sale of all skulls) and sets the
  > money; a sale from storage removes the item there and adds the
  > money. The mode ends and trading is unlocked either way. The sale no
  > longer drops each item's description before freeing it, since the
  > item's destroy hook does (4.144's reasoning); a sale of an item the
  > inventory does not hold no longer hides whatever description is
  > showing, as `UI_RemoveDescriptor(NULL)` did. The sale's
  > `SAFE_DELETE` (the executable's `Client.h`) is written out, and the
  > purchase's own unused copy goes. Found by reading and kept, as
  > upstream's: the storage search sets the first page on every pass,
  > the sale reads the inventory cell without checking it, and a sale
  > from storage frees its item without a sound or a skill-icon check. A star
  > shop priced a purchase that did not fit after freeing it; a fix that
  > follows (moved, then fixed) prices it first. R1 is
  > 144 Windows / 142 Ninja.
  - Owner: membership, R1, the new file and entry, and
    `test_shop_replies.cpp` (each icon item for each race; the purchase
    and its fields, the shelves, a full cell, a replaced stack and one
    past its maximum, a star shop's payment, a purchase not waited for;
    a sale, a stale sale, all skulls, a sale from storage with and
    without it, a sale not waited for).

- [x] **4.148 The trade error and verify replies:** move
  `GCTradeErrorHandler` and `GCTradeVerifyHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). A trade error ends a trade request
  > the player was waiting on, says why in a message box
  > (`DialogPresentation::PopupFreeMessageDlg`; a refusal also closes the
  > "cancel?" box through `TradePresentation::CloseExchangeCancel`), gives
  > the player's trade money back, and closes the exchange window
  > (`TradePresentation::CloseExchange`). A trade verification confirms
  > what the player did in the trade box: an item added or taken out,
  > picked up (onto the mouse, `DialogPresentation::PickUpItem`), put
  > down (off the mouse, new `GetMouseItem` and `DropItem`), or stacked
  > onto another; money added or taken out; the trade refused or
  > reconsidered. Each takes back both sides' acceptance; an item not
  > part of the trade asks the other side to reconsider, and one that is
  > tells the other side what changed, through the game connection. The
  > error reply no longer requires the UI dialog to exist (a missing
  > dialog host skips the message; the installed adapter reads
  > `g_pUIDialog` unguarded, which is safe, since it exists from start-up
  > until shutdown), and loses `if (g_pDebugMessage)` guards inside its
  > never-compiled `OUTPUT_DEBUG` logs. The error reply already did
  > nothing without a player. The verification now does nothing without
  > one either, where its item branches dereferenced the player and its
  > money and finish branches ran; the player is missing only after the
  > game connection is left for the login server, or at release, when
  > no trade is open. R1 is 142 Windows / 140 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_trade_replies.cpp` (each refusal with and without the wait,
    each other error, the money returned, the guards; each verification
    with the packets it sends to the other side, the wrong modes, the
    guards).

- [x] **4.149 The inventory-item and object deletion replies:** move
  `GCDeleteInventoryItemHandler` and `GCDeleteObjectHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). An inventory item deletion frees the
  > item, from the mouse when it is there (letting the mouse go,
  > `DialogPresentation::GetMouseItem` and `DropItem`) or else from the
  > inventory; an item the player was dropping onto a relic table or a
  > creature also ends that drop (`PlayerState::GetItemCheckBufferStatus`,
  > `GetItemCheckBuffer`, `ClearItemCheckBuffer`), letting the mouse go
  > when the deleted item is the one dropped. An object deletion takes a
  > creature (new `ZoneState::RemoveCreature`), or else an item, out of
  > the zone; an item the player was dropping onto a relic table ends
  > that drop first. The inventory deletion no longer drops the item's
  > description before freeing it, since the item's destroy hook does
  > (4.144's reasoning; the item is never NULL there). Without a player
  > the check buffer reads as empty, where both deletions dereferenced
  > the player (the inventory one once it found the item); MZone's
  > RemoveItem still does. Found by reading and kept, as upstream's:
  > - a drop status with an empty check buffer would dereference NULL
  >   (unreachable: the buffer is set with its status);
  > - the dropped item is always the mouse's, so an item deleted while
  >   it is being dropped lets the mouse go twice (harmless: letting go
  >   only clears the mouse's pointer);
  > - an object deletion of the relic being dropped lets the mouse go,
  >   but the relic is not in the zone, so nothing frees it.
  >
  > R1 is 140 Windows / 138 Ninja.
  - Owner: membership, R1, the zone entry and adapter, and
    `test_delete_replies.cpp` (the mouse, the inventory, a missing item;
    each check-buffer status with the dropped item and another; the
    creature-then-item order; no zone; the entry without a host).

- [x] **4.150 The cannot-use reply:** move `GCCannotUseHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). A refused item use ends what the
  > player was waiting on. Waiting on a love chain, it ends that wait
  > when the refused item is the couple ring the player wears (the
  > vampire ring for a vampire, the slayer one otherwise, found in
  > `PlayerState::GetGear`), lifting the chain
  > (`CreatureState::RemoveEffectStatus` on the player) and saying the
  > partner cannot be found; otherwise it leaves everything alone.
  > Without that wait it ends the item check: an item used on another
  > says the option could not be removed (but for a code sheet) and ends
  > the mode; one used from the inventory says why where the item has a
  > reason (a guild tree, types 26-28; a mixing item, which also closes
  > the forge through new `DialogPresentation::CloseMixingForge`; the
  > sex-change dye, type 48, by the packet's code); one used from the
  > quick slot unlocks the gear (new `DialogPresentation::UnlockGear`);
  > a couple ring used from the gear says the partner cannot be found;
  > a quest-inventory use or a drop onto a creature only ends the check.
  > Messages go through `CommunicationPresentation::PopupMessage` and the
  > system message log. Without a player it does nothing, where it
  > dereferenced the player. Found by reading and kept, as upstream's:
  > ending the check already unlocks the gear unless the player is in a
  > casket, so the quick slot's own unlock matters only there, where it
  > undoes that deliberate exception. R1 is 139 Windows / 137 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_cannot_use_replies.cpp` (each race's ring, another item or
    none; each check-buffer status and item reason; another status, an
    empty buffer, no player; the entries without a host).

- [x] **4.151 The NPC question replies:** move `GCNPCAskHandler`,
  `GCNPCAskDynamicHandler` and `GCNPCAskVariableHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). An NPC's question answers the
  > player's wait on it (`PlayerState::GetWaitVerify`,
  > `SetWaitVerifyNULL`): the PC talk box is cleared and filled with the
  > question's subject, the NPC, its creature type
  > (`CreatureState::GetCreatureType` on `ZoneState::GetCreature`), the
  > script and each answer the system availabilities allow (with its
  > index), then shown through new `DialogPresentation::PopupPCTalkDlg`.
  > The question comes from the NPC's script (`GCNPCAsk`; NPC IDs
  > 634-637 need no creature in the zone and are their own creature
  > type), whole in the packet (`GCNPCAskDynamic`, with at least one
  > answer counted), or from the script with its variables filled in
  > (`GCNPCAskVariable`). A question from an NPC not in the zone only
  > ends the wait. The scripted and variable questions no longer require
  > the UI dialog to exist (a missing dialog host skips the popup; the
  > installed adapter reads `g_pUIDialog` unguarded, which is safe, since
  > it exists from start-up until shutdown). Without a player the whole
  > question does nothing, where it dereferenced the player. Found by
  > reading: a whole question took an answer out of the packet only when
  > the availabilities allowed it, so after a refused answer every later
  > one showed the text before it; moved as it was, then fixed (the next
  > commit). The review then found that the packet's read counted every
  > answer but kept only the non-empty ones, so the handler popped an
  > empty list (undefined behaviour) and later answers shifted; the read
  > now keeps an empty answer in its place (38aa7cfc, in 4.152's PR).
  > No question shows an empty answer: each leaves it out like a refused
  > one, since an empty MString holds NULL and the talk-box dialog then
  > handed its menu each line's string as it was (445a9109); it hands ""
  > since 4.238's follow-up (ff6f4a91, whose message names this record
  > for 4.152's). R1 is 136 Windows / 134 Ninja.
  - Owner: membership, R1, the entry and adapter, and
    `test_npc_ask_replies.cpp` (each question with and without a refused
    answer, the four creatureless NPCs and their neighbours, no answer,
    an NPC not in the zone, not waiting, each guard, the entry without a
    host).

- [x] **4.152 The blood bible list:** move `GCBloodBibleListHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). The list of blood bibles the player
  > may rent ends the player's wait and asks as an NPC would: the PC
  > talk box offers each bible by its type, name and bonus, and a way
  > out (999), under the rental subject; it is shown through
  > `DialogPresentation::PopupPCTalkDlg`, and the next answer is a
  > skill-learning one. It no longer requires the UI dialog to exist (a
  > missing dialog host skips the popup, as in 4.151). Without a player,
  > the zone or the talk box it does nothing, as before. Its subject and
  > way-out strings are read through `GetGameString`, which now answers
  > "" for an empty entry as well as for one past the table (445a9109).
  > Still open then, in executable code: `UIDialog` reads a list line's
  > first three characters as its id, so a way-out string that is empty
  > leaves an empty, NULL menu name, which crashed the dialog's menu as an
  > empty answer would (only with an empty way-out entry; the English
  > table has text there). Fixed after 4.238 moved UIDialog into
  > gamemodel: an empty name goes to the menu as "". An id of four digits
  > would still be misread (the server sends small ids). R1 is 135
  > Windows / 133 Ninja.
  - Owner: membership, R1, and `test_blood_bible_replies.cpp` (two
    bibles, one without strings, none; each guard; no dialog host).

- [x] **4.153 The storage price and another creature's status:** move
  `GCStashSellHandler` and `GCOtherModifyInfoHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). The storage price an NPC asks answers
  > the player's question to it (`PlayerState::GetWaitVerify`,
  > `SetWaitVerifyNULL`): a price of 0 means no more storage can be
  > bought, so the talk box comes back (new
  > `DialogPresentation::ShowPCTalkDlg`) with a message saying so; any
  > other price asks whether to buy one (new
  > `DialogPresentation::RunStorageBuy`) and keeps the price for the
  > answer. Another creature's changed status is applied to it (its
  > `CreatureState::Status`), and when the other-info window shows that
  > creature (new `GameInfoPresentation::GetOtherInfoName`, by name) and
  > it is of the player's race, the window shows it again (new
  > `GameInfoPresentation::RunOtherInfo`). Without a player the price
  > does nothing and the window is left alone, where both dereferenced
  > the player; without the creature's status entry nothing is applied.
  > R1 is 133 Windows / 131 Ninja.
  - Owner: membership, R1, the four entries and adapters, and
    `test_stash_info_replies.cpp` (a price, none, not asked, no player;
    a status with and without the window's name, race and player; no
    such creature, no zone, no status entry; the entries without a
    host).

- [x] **4.154 The first position and the hour's dark and light:** move
  `GCSetPositionHandler` and `GCChangeDarkLightHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). The game server's first word places
  > the player: the connection takes the cell and is ready for normal
  > play, the player is placed there facing the direction (new
  > `LoginPresentation::InitPlayer`), and the game starts (new
  > `LoginPresentation::EnterGameMode`). The hour's dark and light set
  > how dark the view draws (new `WorldEvents::SetDarkBits`) and how far
  > the player's light reaches (new `PlayerState::SetTimeLightSight`); an
  > operator always sees the full light. The two new adapters for the
  > dark and the light skip a missing view or player, where the reply
  > dereferenced both. R1 is 131 Windows / 129 Ninja.
  - Owner: membership, R1, the four entries and adapters, and
    `test_position_light_replies.cpp` (the position with and without the
    host; the dark and light, an operator's, and without the hosts).

- [x] **4.155 Which worn items count, and an allowed item use:** move
  `GCRealWearingInfoHandler` and `GCUseOKHandler` (with its
  `GCUseSkillCardOK` handler) into `gamemodel`.
  > **Status:** done (2026-10-13). Which worn items count: a bit per gear
  > slot says whether the item worn there adds its values. Each item of
  > the player's race gear (`PlayerState::GetRace`) is marked to count
  > or not, a blood bible sign left alone; a slayer's item also adds to
  > or leaves the player (`PlayerState::SetAddonItem`, new
  > `RemoveAddonItem`), and a weapon that changes whether it counts
  > refreshes the skills the player can use
  > (`BonusSkills::RefreshAvailableSkills`). Then the player's status is
  > worked out again (new `PlayerState::CalculateStatus`). An item use
  > the server allowed spends the item (new `PlayerState::UseItemOK`) and
  > changes the player's status (`PlayerState::Status`); a skill card it
  > allowed is spent (new `PlayerState::UseSkillCardOK`). Without a
  > player the wearing reply does nothing, where it logged and then
  > dereferenced the player to work its status out. Found by reading and
  > kept: a blood bible sign's slot skips the bit shift, which is
  > harmless because the bible slots come last in every race's gear.
  > R1 is 129 Windows / 127 Ninja.
  - Owner: membership, R1, the four entries and adapters, and
    `test_wearing_use_replies.cpp` (each bit pattern for a slayer, the
    weapon's bit alone and while it still counts, no weapon, a vampire's
    and an ouster's slots past an empty one, the bibles of every race,
    no player; an item use with and without the player's status, spent
    before the status changes, a skill card, no host).

- [x] **4.156 A shop's market condition:** move
  `GCShopMarketConditionHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). A shop's market condition comes
  > before the player sells to its NPC (`ZoneState::GetCreature`,
  > `CreatureState::IsNPC`): an NPC without a shop
  > (`CreatureState::GetShop`) is given an empty one (new
  > `CreatureState::SetShop`, which the NPC then owns), the price manager
  > takes the rates the NPC buys and sells at, and the inventory opens to
  > sell (new `DialogPresentation::OpenInventoryToSell`). A creature that
  > is not an NPC, or not in the zone, is ignored. A shop no creature
  > takes (only without the entry) is freed rather than leaked; no test
  > can see that free, since no tree here checks leaks. R1 is 128
  > Windows / 126 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_market_condition_replies.cpp` (a new shop and an existing one,
    the rates; another creature, none, no zone; no shop entry, no dialog
    host).

- [x] **4.157 The player's own skill success:** move
  `GCSkillToSelfOK1Handler` into `gamemodel`, beside its two siblings.
  > **Status:** done (2026-10-13). The player's own skill succeeded: the
  > wait on it ends, the player keeps the result (its step when graded)
  > for the converted duration (`CreatureState::SetEffectDelayFrame`)
  > and shows it where it stands (`PacketSpecialActionResult`), the
  > packet changes the player's status (`PlayerState::Status`), and the
  > effect status the packet names, or else the skill's own, is added
  > for that duration; the skill's result node runs. Some skills do
  > more: Eternity resurrects a slayer after five seconds
  > (`WorldEvents::AddEvent`) and closes the resurrect window (new
  > `DialogPresentation::CloseRequestResurrectWindow`); Concealment
  > rounds its duration to its animation; Soul Chain starts its delay
  > now; Will of Life checks the player's regeneration
  > (`RankBonusHandlers::CheckRegen`) and waits as long as its level says
  > (`GameInfoPresentation::GetCharInfoLevel`). An ouster's untransform
  > ends there (the answer to getting off a sylph), and so does a
  > turret's (`PlayerState::HasEffectStatus`). Without a player it
  > does nothing, where it dereferenced the player. R1 is 127 Windows /
  > 125 Ninja.
  - Owner: membership, R1, the entry and adapter, and
    `test_own_skill_replies.cpp` (a success waited on and not, the
    duration at 16 frames a second, the packet's effect status, a graded
    skill and a grade without steps, a skill past the table, an ouster's
    and a turret's untransform, Eternity by race, Concealment's rounding,
    Soul Chain's delay and another skill's, Will of Life's, no player).

- [x] **4.158 An NPC's shop lists and versions:** move
  `GCShopListHandler`, `GCShopListMysteriousHandler` and
  `GCShopVersionHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). An NPC's shop list fills one rack of
  > its shop with the items the server lists: an NPC without a shop is
  > given an empty one (`CreatureState::SetShop`), filled with its fixed
  > and mysterious racks (new `CreatureState::CreateFixedShelf`); the
  > shop's kind decides the rack shown first; the buying rate and the
  > castle's tax ratio are set; the shop window opens on it (new
  > `DialogPresentation::SetShop`, `RunShop`); and the shop's help is
  > shown (new `GameInfoPresentation::ExecuteHelpEvent`). A mysterious
  > list fills a rack with unidentified items of a class (the type the
  > packet carries is not used, as upstream). A shop's versions ask the
  > server for the special rack when the client's is out of date, through
  > the connection the reply arrived on (upstream's `g_pSocket`, the same
  > game connection), and open the shop when nothing is. A shop or rack
  > nothing takes (only without the entry) is freed. `MPriceManager`
  > gains a `GetShopTaxRatio` for the tests. Found by reading: the list
  > casts the NPC's shop kind, a wire byte, to an enum with two values;
  > moved as it was, then fixed by giving `MShop::SHOP_TYPE` a fixed
  > `int` type, so an unknown kind is kept and shows the fixed rack
  > first (ca8f5c3e). R1 is 124 Windows / 122 Ninja.
  - Owner: membership, R1, the four entries and adapters, and
    `test_shop_list_replies.cpp` (a new shop and an existing one, an
    event-star shop and an unknown kind, each item field, a refused item
    class, a rack past the shop's, not an NPC, nobody, no zone; the
    mysterious list, a refused class, an existing shop, nobody; versions
    out of date and up to date, nobody; no hosts, no shop entry), and
    `MShop.h`'s fixed `SHOP_TYPE`.

- [x] **4.159 Power points and a point exchange purchase:** move
  `GCRequestPowerPointResultHandler`, `GCUsePowerPointResultHandler` and
  `GCExchangeBuyHandler` into `gamemodel`.
  > **Status:** done (2026-10-13). Power points asked for: on success
  > the player's points are the new sum (new
  > `VsUiPresentation::SetPowerjjangPoint`) and a message says how many
  > were added; each failure has its message
  > (`CommunicationPresentation::PopupMessage`). Power points spent on a
  > gamble: on success the points are what is left and the gamble's
  > prize is shown (new `VsUiPresentation::PowerjjangGambleResult`); a
  > full inventory or too few points have their messages. A point
  > exchange purchase shows the server's message
  > (`DialogPresentation::PopupFreeMessageDlg`), or a general error when
  > it failed without one, and a success reloads the exchange window
  > (new `VsUiPresentation::RefreshPointExchange`). R1 is 121 Windows /
  > 119 Ninja.
  - Owner: membership, R1, the three entries and adapters, and
    `test_power_point_replies.cpp` (a request and each failure, a gamble
    and each failure, a purchase with and without a message and
    success, no host).

- [x] **4.160 A party invitation's answers:** move `GCPartyInviteHandler`
  into `gamemodel`.
  > **Status:** done (2026-10-13). Someone invites the player: when the
  > player already invited that one, the party is formed (the player
  > accepts, through the connection the reply arrived on); when the
  > player invited someone else, the player is busy; otherwise the
  > player is asked (new `CommunicationPresentation::RunPartyRequest`),
  > unless waiting on something else, talking to an NPC (new
  > `DialogPresentation::IsLockInput`), a bat or a wolf, or not taking
  > invitations (then the player refuses). The inviter cancelling closes
  > the asking windows (new `ClosePartyAsk`, `ClosePartyRequest`). The
  > one the player invited accepting forms the party; refusing, being
  > busy, being full, or in another party (unless it is the player's own,
  > new `CreatureState::IsPlayerParty`) ends the player's wait and says
  > why. A party formed is announced in the game messages with the
  > inviter's name. The reply no longer requires the UI dialog to exist
  > (a missing dialog host skips the message and counts input as
  > unlocked; the installed adapters read `g_pUIDialog` unguarded, which
  > is safe, since it exists from start-up until shutdown), and reads its
  > refusal text through `GetGameString`. Found by reading and kept: the
  > announcement names `PartyInviter`, which only being asked to join
  > sets, so a party formed from the player's own invitation names
  > whoever last asked the player, or no one; and, in executable code,
  > `UI_ClosePartyCancel` compares `IsWaitVerify()` (a BOOL) with
  > `WAIT_VERIFY_PARTY`, so it never clears the party wait. R1 is 120
  > Windows / 118 Ninja.
  - Owner: membership, R1, the five entries and adapters, and
    `test_party_invite_replies.cpp` (an invitation from the invited one,
    from another while inviting, asked or refused for each reason, asked
    in the request mode without the wait; a cancel in and out of the
    request mode; an acceptance waited on and not, in another mode, an
    inviter gone or none; each refusal waited on, one not, one waited on
    in no mode; another party for the packet's target and the player's
    own; nobody, an unknown code; each guard; no hosts).

- [x] **4.161 The party joined:** move `GCPartyJoinedHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). The party the player is in, as the
  > server lists it, replaces the old one: the old members leave
  > (`MParty::UnSetPlayerParty`), and each listed member but the player
  > joins with its sex, face (new `CommunicationPresentation::GetFaceStyle`),
  > address and what the old party knew of it. A member in the zone (new
  > `ZoneState::GetCreatureID`) is in sight with its hit points, the
  > player's zone (the large zone or the small one, from `ZoneFlags`),
  > its cell and guild (new `CreatureState::GetGuildNumber`),
  > and is marked as the player's party (new `SetPlayerParty`); one
  > elsewhere is out of sight in no zone the client knows. The asking
  > windows close, the party window opens when the party changed size
  > (new `RunParty`), a party that was empty starts its clock, and the
  > player's information goes to the party (new
  > `PlayerState::SendInfoToParty`, the three sends in their order).
  > Without a member's status entry its hit points stay as the old party
  > knew them. R1 is 119 Windows / 117 Ninja.
  - Owner: membership, R1, the six entries and adapters, and
    `test_party_joined_replies.cpp` (a party replacing an old one with
    a member in sight it knew, one away, one listed but not there; the
    small zone; a member in sight without a status entry; a first
    party, the same size again; each guard; no hosts).

- [x] **4.162 A vampire portal entered, and an item dropped:** move
  `GCEnterVampirePortalHandler` and `GCDropItemToZoneHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-13). A creature entering a vampire portal
  > stops at the portal's cell, takes it as its server position, vanishes
  > into the portal (new `WorldEvents::MoveIntoPortal`) and leaves the
  > zone. An item dropped to the zone lands there, falling
  > (`WorldEvents::AddItemToZone`); when a creature dropped it for its
  > pet (new `CreatureState::GetPetID`, `ZoneState::GetFakeCreature`),
  > the pet acts drained (new `CreatureState::SetAction`) and makes its
  > sound by its pet item (new `CreatureState::GetPetItem`, for fake
  > creatures only): a dog growls at 30 or more and barks otherwise, a
  > slayer's pet makes its creature type's drained sound, and a later pet
  > makes none. R1 is 117 Windows / 115 Ninja. Recorded, not fixed:
  > `MoveIntoPortal` (`PacketFunction.cpp`) deletes the portal's fake
  > creature when the zone refuses it, then reads its ID for the tunnel
  > effect.
  - Owner: membership, R1, the five entries and adapters, and
    `test_portal_drop_replies.cpp` (a portal entered, nobody, no zone;
    an item dropped by nobody, by a creature not there, without a pet,
    with a pet not there; a dog growling and barking; a slayer's pet with
    and without its sound, a later pet, no pet item; no hosts).

- [x] **4.163 A fast move, and its afterimage:** move
  `GCFastMoveHandler` and `GCFakeMoveHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). A creature moving fast by a skill
  > moves at once to the cell, as the server put it (new
  > `CreatureState::FastMovePosition`):
  > - a charging attack starts its effect from where it was to where it
  >   lands, and leaves a ghost trail (new `SetGhost`);
  > - a teleport starts one effect where it was and another where it
  >   lands;
  > - a ducking wallop's effect faces where it goes (new
  >   `GetDirectionToPosition`);
  > - a bike crash puts the creature on a motorcycle facing the cell from
  >   where it stands (`SelectFacingDirection`, new
  >   `SetInstallTurretDirect` and `SetInstallTurretCount`);
  > - any other skill only moves it.
  > The player waiting on the skill's success is verified, and the player
  > is held for as long as its own skill lasts. A fast-moving creature
  > leaves an afterimage: a fake copy of it at its cell (new
  > `ZoneState::NewFakeCreature`) running the same move (new
  > `CreatureState::SetFakeFastMove`), which the zone takes (new
  > `AddFakeCreature`) or which is deleted (new `DeleteFakeCreature`).
  > Once moved, the reply stopped casting the wire's 16-bit skill type to
  > `ACTIONINFO`, whose values end at 2047: a type past that is now just
  > a fast move, where Clang's UBSan stopped at the enum load. Two
  > adapters differ only where the old code was undefined or could not
  > run: `SetAddonItem` asks `IsWear` before the cast the old code made
  > blindly (a bike crash is a slayer's, and slayers wear gear), and
  > `AddFakeCreature` checks for a zone `GetCreature` has already read.
  > R1 is 115 Windows / 113 Ninja.
  - Owner: membership, R1, the nine entries and adapters, and
    `test_fast_move_replies.cpp` (each skill for the player and another
    creature, the player not waiting on it, a skill type past the enum,
    no motorcycle, nobody, no player; an afterimage taken, refused, for
    nobody, not made; no hosts).

- [x] **4.164 A peer's file request:** move `CRRequestHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). Another client asks over its own
  > connection for one of the player's files; a peer with no name goes
  > by its address. Asked for the player's profile (new
  > `CommunicationPresentation::GetProfileFilename`), the connection
  > queues the profile's index file and the profile to send, unless that
  > peer has a request queued already; with no profile, or asked for
  > someone else's, it answers why not and the session ends. Asked for
  > the guild mark, the connection switches to sending it. Without the
  > user information, the request managers or the UI's profiles (new
  > `HasProfiles`) nothing is done. Once moved:
  > - `CR_REQUEST_CODE` took `BYTE` as its fixed type. The request's
  >   code is a byte a peer sends, the old enum held 0-3, and converting
  >   any other byte to it was undefined; Clang's UBSan stopped at the
  >   load in the reply's switch.
  > - So did `RC_CONNECT_VERIFY_CODE` and `REQUEST_VERIFY`. The client
  >   writes those answers, but its game connection, which accepts any
  >   packet id in the normal status, reads them too before it finds no
  >   reply.
  > - The reply returns at once without a peer's connection (the game
  >   connection reads a `CRRequest` as well, and the cast gave NULL),
  >   and before a character is chosen no profile is the player's (the
  >   compare ran `strlen` on the empty `CharacterID`'s NULL). The
  >   first crash needed the game server to send the request; the second
  >   needed only a peer asking before a character was chosen.
  >   `CRWhisperHandler` had the first crash too, and has the same
  >   return. `CRConnectHandler` still answers a request off the game
  >   connection by throwing `DisconnectException` (a follow-up).
  > R1 is 114 Windows / 112 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_peer_request_replies.cpp` (the profile queued, a request
    queued already, no profile, someone else's, the guild mark, no
    request, a code no request has, a nameless and a named peer, each
    guard, no host, the game connection, no character yet),
    `test_request_verify_codes.cpp` and the whisper test's game
    connection case.

- [x] **4.165 The login server's welcome:** move `LCLoginOKHandler` and
  `LCRegisterPlayerOKHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). After a login or a new account the
  > login ID is kept for the next login's ID box (new
  > `LoginPresentation::BackupLoginID`; "DarkEden" for an ID of 15 or
  > more characters). Unless the client is quitting to update, the
  > connection becomes the login connection, asks for the world list and
  > waits for it (new `EnterWaitWorldListMode`), and blood shows unless
  > the player chose the teen version (new `UseTeenVersion`,
  > `SetGoreLevel`). After a new account the connection also joins the
  > logged-in status and the server group is kept (new
  > `SetServerGroupName`). After a login the account's family flag and
  > premium days are kept: a Netmarble account has none to show, and the
  > dormant-account event's first login starts a free week with a
  > notice, whose missing string now reads as empty (`GetGameString`)
  > rather than NULL. `g_LeftPremiumDays` and `g_bFamily` moved from
  > `Client.cpp` to `ClientMode.cpp`, beside `g_bNeedUpdate`. R1 is 112
  > Windows / 110 Ninja.
  - Owner: membership, R1, the five entries and adapters, and
    `test_login_welcome_replies.cpp` (a login, a new account, the ID's
    length either side of 15, the teen version, quitting to update, the
    dormant-account event, a Netmarble account, no host).

- [x] **4.166 The full quest list:** move `GCGQuestStatusInfoHandler`
  into `gamemodel`.
  > **Status:** done (2026-10-14). The full quest list rebuilds the quest
  > window: the window is closed if it runs (new
  > `VsUiPresentation::IsRunningQuestManager`, `CloseQuestManager`) and
  > opened again, loading its quest XML (new `RunQuestManager`); each
  > quest becomes a UI record (its ID and status, and each mission's
  > condition, index, status and arguments, untimed) handed to the window
  > in the list's order (new `SetQuestManagerInfo`); and the XML is
  > released (new `ReleaseQuestXML`). The window keeps a record it takes;
  > one it refuses, or one with no window to go to, is freed. The game's
  > quest window takes every record but keeps only one whose quest has
  > an XML element, so the rest leak, as `GCGQuestStatusModify`'s do; the
  > move does not change that. R1 is 111 Windows / 109 Ninja.
  - Owner: membership, R1, the five entries and adapters, and
    `test_quest_list_replies.cpp` (two quests in order, a running window
    closed first, refused records with and without missions, no host or
    an empty one).

- [x] **4.167 A pet's skill:** move `GCPetUseSkillHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). A creature's pet uses its skill on a
  > target: the target is hit, and the pet makes its creature type's
  > attack sound. Its skill follows its pet item:
  > - a tank (type 3) shows the target being shelled and fires its
  >   level's attack (3 below 40, 4 below 50, 5 from 50);
  > - a stirge (type 4) drains blood, and a pixie (type 5) absorbs a
  >   soul, at its level's grade (1 below 20, then one grade per ten
  >   levels, up to 5 from 50);
  > - another pet, or one without a pet item, fires the tank's first
  >   attack.
  > The pet's turret recoils by 3 (new `CreatureState::SetTurretDelay`)
  > and the skill is aimed at the target. A Centauro turret (creature
  > types 702-704) is given the target's direction as its final one (new
  > `SetTurretFinalDirection`); upstream hands it the 8-way direction
  > while the turret counts 32 steps, which the move keeps. Type 702
  > fires from the muzzle's offset between the way it faced and the way
  > it aims. The skill then starts from the pet. R1 is 110 Windows / 108
  > Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_pet_skill_replies.cpp` (each pet type either side of each
    level bound, another pet, no pet item, no attack sound, the three
    Centauro turrets and 702's offsets, no attacker, target or pet, no
    hosts).

- [x] **4.168 An effect added to a tile:** move
  `GCAddEffectToTileHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). An effect added to a tile starts its
  > effect status's skill at the tile for the effect's duration. A trap
  > shows only to a slayer, and a siege turret's laser not to the
  > siege's attackers. A dragon tornado (or its child) is its own effect
  > (new `WorldEvents::SetDragonTornado`). Some effects do more:
  > - the regen tower's trying shows its light;
  > - luck and misfortune show their auras;
  > - a ground elemental's aura, a forbidden darkness and the fury of
  >   gnome mark the zone's cell (new `ZoneState::SetGroundElemental`,
  >   `SetDarknessForbidden`, `SetFuryOfGnome`);
  > - a halo strikes its creature from the tile, facing it
  >   (`SelectFacingDirection`, which `MTopView`'s static was; the
  >   `g_pTopView` check went, as the view outlives every zone).
  > The four effect calls that stopped at `NULL` relied on
  > `bStartFromMainNode`'s default, `true`; they now pass it, and the
  > halo's passes `sX`/`sY` too. Once moved, the reply stopped casting
  > the wire's 16-bit effect ID to `EFFECTSTATUS`, whose values end at
  > 511: an ID past that now shows nothing, where Clang's UBSan stopped
  > at the enum load. `GCAddEffectHandler` had the same cast before its
  > own range check, and now checks first. Recorded, not fixed: the three
  > ground marks write `MZone::GetSector(x, y)`, which does not check the
  > wire's cell against the zone's size, as the old code did not. R1 is
  > 109 Windows / 107 Ninja.
  - Owner: membership, R1, the four entries and adapters, and
    `test_tile_effect_replies.cpp` (a skill and none, the trying, luck
    and misfortune, the three ground marks with and without a zone, a
    halo and its creature gone, a trap for a slayer, another race and no
    player, the laser for each attacker, a defender and no player, the
    tornado and its child, an ID past the effect statuses, no hosts).

- [x] **4.169 A personal shop's contents:** move `GCMyStoreInfoHandler`
  and `GCOtherStoreInfoHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). The player's own shop:
  > - the player stops following anyone (new `PlayerState::IsTraceCreature`,
  >   `TraceNULL`);
  > - unless the player's type is an NPC's, its shop is marked open or
  >   shut (new `CreatureState::SetPersnalShop`); an open shop shows its
  >   sign, or the default one, in the player's chat colour (new
  >   `SetPersnalString`, `CommunicationPresentation::GetChattingColor`);
  > - closing it (window mode 3) takes the sign away (new
  >   `ClearChatString`), closes the window and drops the shop's storage;
  >   opening it for sale (mode 2) closes the window;
  > - otherwise the shop's items fill a new storage, the player's own
  >   items for sale are marked personal, and the window shows the storage
  >   when the player is stocking it (mode 1), or when the server asks and
  >   neither window nor shop is up.
  > Another creature's shop is marked open or shut and an open one shows
  > its sign; the shop the player is looking at closing closes its
  > window; a shop the player asked for fills a new storage and opens the
  > window beside the inventory. Each item keeps its price, options,
  > durability, grade and enchantment; a gun is loaded with an empty
  > magazine holding one round (`MItem::LoadGun`, the loop upstream ran
  > inline, which needed `MMagazine`'s executable vtable), and a belt or
  > arms band gets its sub items. The window's seven calls are new
  > `VsUiPresentation` entries, and `IsNPCType` reads the creature table
  > as `MCreature::IsNPC` did. R1 is 107 Windows / 105 Ninja.
  - Owner: membership, R1, the fifteen entries and adapters, and
    `test_personal_shop_replies.cpp` (a shop asked for with a sword, an
    empty slot, a gun and a belt; news of a shop open and shut; an NPC's
    type and nobody; the shop looked at closing, another seller's, the
    window down; the player's shop stocked with an item marked personal;
    the window asked for or not; closing and selling; no player, no
    hosts).

- [x] **4.170 An item created in the inventory:** move
  `GCCreateItemHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). An item created in the player's
  > inventory takes the packet's type, options, ID, durability, grade,
  > enchantment and number, and goes to its inventory cell, or is
  > dropped when the cell is taken. A gun is loaded with that many
  > rounds instead (`MItem::LoadGun`, the loop upstream ran inline).
  > While the player picks up part of a stack, the part in hand leaves
  > the inventory onto the mouse, the rest being the new item. A whole
  > Christmas tree (types 12, 25 and 41) made while merging takes the
  > place of its twelve parts: each cell of the 3x4 block gives up a
  > part of any tree that belongs at that place. An operator's "*C2G on"
  > puts each item it creates on at once (new
  > `VsUiPresentation::IsCreatingToGear`, `MoveCreatedToGear`). Once
  > moved, a partial pickup whose stack is not where the inventory has it
  > leaves the cell alone; it used to take out whatever item was there and
  > lose it. R1 is 106 Windows / 104 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_created_item_replies.cpp` (an item and a gun created, a taken
    cell, no class, part of a stack picked up, the part not where the
    inventory has it, nothing in hand, another buffer, each whole tree,
    no merge, a part of the tree, parts by their place, the operator's
    items, no player, no host).

- [x] **4.171 The verdict on an item put on an item:** move
  `GCAddItemToItemVerifyHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). The server's verdict on an item put on
  > an inventory item (an enchant, a sex change, a pet's revival, an
  > option cleared or removed, a grade raised) or on two items mixed. The
  > item is the one at the inventory cell the request remembered (two for
  > a mix), and the request is over. A refusal says why, lets the item in
  > hand go back and ends the item check; a result shows its effect on
  > the item (new `WorldEvents::AddNewInventoryEffect`), lets the item in
  > hand go, and some chime or say the change worked. A three-way enchant
  > changes up to three options, each byte pair of the parameters an
  > option's new and old values. A mix goes to the mixing forge (new
  > `DialogPresentation::OkMixingForge`) and clears the request; an
  > option taken off goes to its window (new `OkRemoveOption`). Once
  > moved, nine results that read the item no longer crash when it is gone
  > (no request remembered, or its cell empty); they do nothing, as the
  > revival and the option taken off always did. R1 is 105 Windows / 103
  > Ninja.
  - Owner: membership, R1, the three entries and adapters, and
    `test_item_to_item_replies.cpp` (each refusal, the premium refusal
    by race, the mix of one option group, a sex change refused, an
    unknown code, each enchant result, the three-way enchant with and
    without zero options, the other results, a mix and an option taken
    off, an item gone, each result for an item no longer there, no
    hosts).

- [x] **4.172 A burrowing creature:** move
  `GCAddBurrowingCreatureHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). A creature burrowed underground. One
  > the zone does not know yet is made (new `ZoneState::NewCreature`) as
  > a male vampire with the packet's name, ID and cell (new
  > `CreatureState::SetName`, `SetID`, `SetPosition`), underground,
  > facing down and standing, in no guild (new `SetGuildNumber`), at
  > full health of 100, and added to the zone (new `AddCreature`), or
  > dropped if the zone refuses it (new `DeleteCreature`). One the zone
  > knows goes underground under the packet's name and moves to the
  > cell, leaving any guild. Without a zone nothing happens. R1 is 104
  > Windows / 102 Ninja.
  - Owner: membership, R1, the seven entries and adapters, and
    `test_burrowing_creature_replies.cpp` (a new creature made and
    added, one refused, a known one, no zone, no hosts).

- [x] **4.173 A vampire as a bat or a wolf:** move `GCAddBatHandler`
  and `GCAddWolfHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). One the zone does not know yet is made
  > as a wearing creature (new `ZoneState::NewCreatureWear`) of the form's
  > type with the packet's name, ID, cell, direction, guild and health,
  > standing, and added to the zone, or dropped if the zone refuses it; a
  > bat flies with the packet's colour (new `CreatureState::SetBatColor`;
  > its own, 0xFFFF, when none), and a wolf walks. One the zone knows
  > takes the form in place: it moves to the cell, takes the direction,
  > guild and health, and the transformation's effect starts from it,
  > its result changing the creature's type. A werewolf (a wolf of item
  > type 39) takes the packet's colour as its body colour. R1 is 102
  > Windows / 100 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_bat_wolf_replies.cpp` (a new bat with and without a colour, one
    refused, a known creature turning into a bat, a new wolf and
    werewolf, one refused, a known creature turning into a wolf and a
    werewolf, no zone, no hosts; the health recorded in its order).

- [x] **4.174 A monster out of a burrow, or out of a transformation:**
  move `GCAddMonsterFromBurrowingHandler` and
  `GCAddMonsterFromTransformationHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). One the zone does not know yet is made
  > of the packet's type, walking, with its ID, cell, direction, health
  > and name, standing, in guild 1, and added to the zone (or dropped if
  > the zone refuses it). Out of a burrow, one the zone knows takes all
  > that in place; out of a transformation, a plain creature the zone
  > knows (new `CreatureState::IsPlainCreature`) moves to the cell and
  > takes the type, direction, health and guild, while any other is
  > removed and made again as a plain one. The monster then takes the
  > packet's effect statuses (new `WorldEvents::SetEffectInfo`) and comes
  > up out of the ground; a land mine or a pleasure station trap out of a
  > burrow also shows its light. In a siege, a trap marked hidden is not
  > shown to an attacker at all; without a player it is shown (the old
  > code dereferenced `g_pPlayer`). Once moved, the hidden mark's check
  > stopped casting the list's 16-bit statuses to `EFFECTSTATUS`, whose
  > values end at 511. R1 is 100 Windows / 98 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_monster_appear_replies.cpp` (a new monster and one refused, a
    known one, each trap's light, a hidden trap for each attacker and
    trap type, the mark alone, a defender, a non-trap above and below
    the traps, the mark as a duration, a status past the enum, no
    player, a transformed monster made, one refused, a plain one in
    place, another made again, one refused, no zone, no hosts, no
    status array).

- [x] **4.175 A monster appearing:** move `GCAddMonsterHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). One in a slayer's or an ouster's shape
  > is the host's to make whole (new `WorldEvents::AddRaceSlayerMonster`
  > and `AddRaceOustersMonster`). Any other the zone does not know yet is
  > made of the packet's type, walking, with its ID, cell, direction and
  > health, standing, named, in guild 1, and added to the zone (or dropped
  > if the zone refuses it); one the zone knows takes the type if it
  > changed and the rest in place, its name kept. The monster then takes
  > the packet's effect statuses, and its type may add more: Gilles de
  > Rais's map effect (new `WorldEvents::AddGDREffect`), a trap's light,
  > the Great Ruffian's body, a draining stance, a lasting effect status,
  > a sacrifice's wounds. One that came through a portal comes up out of
  > it (new `WorldEvents::ComeFromPortal`) over a bloody tunnel, unless
  > its type shows itself otherwise. In a siege, a trap marked hidden is
  > not shown to an attacker at all; without a player it is shown (the
  > old code dereferenced `g_pPlayer`). Once moved, the hidden mark's
  > check stopped casting the list's 16-bit statuses to `EFFECTSTATUS`,
  > as 4.174's did. R1 is 99 Windows / 97 Ninja.
  - Owner: membership, R1, the four entries and adapters, and
    `test_add_monster_replies.cpp` (a new monster and one refused, a known
    one of the same type and of another, each race shape, each type's own
    effects, the portal with and without them, a flag other than 1, a
    hidden trap for each attacker and trap type, the mark alone, a
    defender, a non-trap above and below the traps, the mark as a
    duration, a status past the enum, no player, no zone, no hosts).

- [x] **4.176 A creature's skill on another creature:** move
  `GCSkillToObjectOK5Handler` into `gamemodel`.
  > **Status:** done (2026-10-14). The user faces the target's cell and
  > casts the skill at it; its queue hits the target with the skill's
  > result action, then the node its result kind names and its effect
  > status. A weapon skill acts as the user's basic action (none at all
  > when that is a result action); a skill with steps acts as the step
  > its grade names. Some skills also show a copy or a summoned creature
  > (five new `WorldEvents` entries, which OK1 and OK2 use from 4.177):
  > Shadow Dancing, Illusion Inversion and Illendue's fading copies, Blaze
  > Walk's strikes, the plasma rocket, and the wild wolf, which runs at a
  > target or at the creature in a corpse (the skill then goes on against
  > that one). Set Afire marks the user, Bite of Death marks the target as
  > drained by the user. Without a zone, the user or the target nothing
  > happens; a skill past the action table is refused. R1 is 98 Windows /
  > 96 Ninja.
  - Owner: membership, R1, the five entries and adapters, and
    `test_skill_object_replies.cpp` (a hit with and without its node and
    effect, a weapon skill and a basic result action, a step and grade
    0, each copy and summon, Set Afire, Bite of Death, the wolf at a
    target, a corpse, nothing and without its user, another skill at a
    corpse, a missing user or target, past the table, no zone, no hosts).

- [x] **4.177 The player's skill on a creature, and a skill on the
  player:** move `GCSkillToObjectOK1Handler` and
  `GCSkillToObjectOK2Handler` into `gamemodel`.
  > **Status:** done (2026-10-14). Both land as 4.176's does, through its
  > five skill entries. The player's skill (OK1) answers the wait,
  > changes the player's status, and rides on the effect it is casting;
  > Heal Pass also passes from the player to the target. A skill on the
  > player (OK2) changes the player's status and carries the player's
  > pending effect status in place of the skill's; a Typhoon holds the
  > player still a second; with the user unseen the skill rises from the
  > player (at its height: new `CreatureState::GetZ`); Absorb Soul lets
  > the user's draining go on; an acid on a player protected from it is
  > cast as the protection. Without a player nothing happens (the old
  > code dereferenced `g_pPlayer`). R1 is 96 Windows / 94 Ninja.
  - Owner: membership, R1, the entry and its adapter, and the OK1 and
    OK2 cases of `test_skill_object_replies.cpp` (the wait, the status,
    the effect it rides on, the node and effect, a weapon skill and a
    basic result action, a step, each copy and summon, Heal Pass, Bite
    of Death, Set Afire, the wolf at a target and a corpse with either
    grade, the pending effect status, a Typhoon at each grade, an acid
    with and without the protection, an unseen user, no zone, no
    player, past the table).
  - Recorded, not fixed: OK2 returns without freeing its queue when the
    user's basic action is a result action. In the executable,
    `SkillShadowDancing`, `SkillIllendue` and `SkillBlazeWalk` go on with
    a fake creature they deleted when the zone refused it, and from grade
    3 `SkillBlazeWalk` leaves its direction unset and reads its
    three-cell target list past the end.

- [x] **4.178 A creature turning into a slayer:** move
  `GCMorphSlayer2Handler` into `gamemodel`.
  > **Status:** done (2026-10-14). One the zone does not know yet is
  > made as a wearing creature, a male or female slayer by the packet's
  > sex, walking, with its ID, cell, direction, skin colour, health and
  > name, standing, dressed as the packet describes (new
  > `CreatureState::SetAddonToSlayer3`), in no guild (-1), and added to
  > the zone (or dropped if the zone refuses it). A plain wearing
  > creature the zone knows (new `CreatureState::IsPlainCreatureWear`,
  > the old class check, which `IsWear` would widen to the player and
  > NPCs) takes the slayer's type, walks, leaves any guild, moves to the
  > cell and takes the direction, skin colour, health, name and dress in
  > place; any other creature it knows is left alone. R1 is 95 Windows /
  > 93 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_morph_slayer_replies.cpp` (a new male and female slayer, one
    refused, a known wearing creature, another creature, no zone, no
    hosts, no status array).

- [x] **4.179 A creature turning into a vampire:** move
  `GCMorphVampire2Handler` into `gamemodel`, with `GetVampireCreatureType`
  and `GetOustersCreatureType` lifted out of `PacketFunction.cpp` into
  `CreatureTypeSelection.cpp`.
  > **Status:** done (2026-10-14). A vampire's creature type follows its
  > shape: in its own shape, the frame its coat gives a man or a woman (a
  > lookup in the item table, which gamemodel holds); a bat's, a wolf's or
  > a werewolf's otherwise. An ouster's is the ouster's. One the zone does
  > not know yet is made as a wearing creature with the packet's name,
  > that type and sex, walking, with its ID, cell, direction, skin and
  > coat colours, attack speed, health and alignment, standing, in guild
  > 2, and added to the zone (or dropped if the zone refuses it); one the
  > zone knows takes all that in place, moving to the cell. The
  > executable's other callers reach the two functions through
  > `PacketFunction.h`, which includes the new header. R1 is 94 Windows /
  > 92 Ninja.
  - Owner: membership (the handler and the new file), R1, and
    `test_morph_vampire_replies.cpp` (each shape and sex, a coat past
    the table, the ouster's type, a new vampire and one refused, a known
    creature, no zone, no hosts, no status array).

- [x] **4.180 A vampire out of a burrow, or out of a transformation:**
  move `GCAddVampireFromBurrowingHandler` and
  `GCAddVampireFromTransformationHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). Its creature type follows its shape
  > and coat (4.179's `GetVampireCreatureType`), or is the operator's for
  > competence 0 (new `CreatureState::SetCompetence`); a bat flies and
  > any other walks. One the zone does not know yet is made as a wearing
  > creature with the packet's advancement, type, sex, name, guild, ID,
  > cell, direction, health, alignment, colours and attack speed,
  > standing, and added to the zone (or dropped if the zone refuses it).
  > Out of a burrow, one the zone knows takes all that in place, moving
  > to the cell; out of a transformation, a plain wearing creature it
  > knows does the same, while any other is removed and made again,
  > without the advancement, which went on the old one (as before). The
  > vampire then takes the packet's effect statuses and comes up out of
  > the ground, or out of its transformation. The burrow's known branch
  > set the creature's zone again, a no-op: `MZone::AddCreature` sets it
  > on every creature it takes, and `SetZone` is not virtual. A dead
  > `__METROTECH_TEST__` block naming an executable global is gone. R1
  > is 92 Windows / 90 Ninja.
  - Owner: membership, R1, the entry and its adapter, and
    `test_vampire_appear_replies.cpp` (a new vampire and one refused out
    of each, a known one out of a burrow, a plain wearing one and
    another one transformed, one made again and refused, a bat, an
    operator, no zone, no hosts, no status array).

- [x] **4.181 A vampire appearing:** move `GCAddVampireHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). Its creature type follows its shape
  > and coat, or is the operator's for competence 0; a bat flies and any
  > other walks. One the zone does not know yet is made as a wearing
  > creature with the packet's advancement, name, type, sex, ID, cell,
  > direction, colours (a werewolf's body in its coat colour twice),
  > master's aura (new `CreatureState::SetMasterEffectType`), health,
  > alignment, rank, attack speed, guild and union (new
  > `CreatureState::SetUnionGuildID`) and bat colour (its own, 0xFFFF,
  > for none), standing, and added to the zone (or dropped if the zone
  > refuses it); one the zone knows takes all that in place, its name
  > kept. A creature that is no NPC by its type (the creature table, as
  > `MCreature::IsNPC` reads it) shows whether its personal shop is
  > open, and an open one shows the shop's sign, or the default message,
  > to a vampire player in the chat colour. The vampire then takes the
  > packet's effect statuses, comes up out of a portal when it came
  > through one, takes its pet, and takes its nickname, a custom one as
  > sent and another by its index (the first past the table). Without a
  > player no sign is shown (the old code dereferenced `g_pPlayer`). R1
  > is 91 Windows / 89 Ninja.
  - Owner: membership, R1, the two entries and adapters, and
    `test_add_vampire_replies.cpp` (a new vampire and one refused, a
    known one, each sex, a bat, an operator, a werewolf, no bat colour,
    an open shop with a sign and without, a player no vampire, no
    player, an NPC's type, the portal, the pet, each kind of nickname,
    no zone, no hosts, no status array).

- [x] **4.182 A slayer appearing:** move `GCAddSlayerHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). One the zone does not know yet is
  > made as a wearing creature: a male or female slayer by the packet's
  > sex (an operator's type for competence 0), with its advancement,
  > guild and union, walking, with its ID, cell, direction, skin colour,
  > master's aura, health, alignment, rank and name, standing, dressed as
  > the packet describes, and added to the zone (or dropped if the zone
  > refuses it). One the zone knows takes the advancement, guild, union,
  > walking, cell, direction, standing, skin, aura, health, alignment and
  > rank in place, its type, name and dress kept. A creature that is no
  > NPC by its type shows whether its personal shop is open, and an open
  > one shows the shop's sign (or the default message, as the string
  > table holds it) to a slayer player in the chat colour. The slayer
  > then takes the packet's effect statuses, its pet and its nickname.
  > Without a player no sign is shown (the old code dereferenced
  > `g_pPlayer`). An unused `AddonDef.h` include, an executable header,
  > is gone. R1 is 90 Windows / 88 Ninja.
  - Owner: membership, R1, and `test_add_slayer_replies.cpp` (a new
    slayer of each sex and one refused, a known one, an operator new and
    known, an open shop with a sign and without, new and known, an empty
    default message, a player no slayer and no player, new and known,
    an NPC's type, the pet, each kind of nickname, no zone, no hosts, no
    status array).

- [x] **4.183 An ouster appearing:** move `GCAddOustersHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). Its type is the ouster's (an
  > operator's for competence 0). One the zone does not know yet is made
  > as a wearing creature with the packet's advancement, name, type, ID,
  > cell, direction, hair and coat colours, master's aura, health,
  > alignment, rank, attack speed, guild and union, standing, and added
  > to the zone (or dropped if the zone refuses it); one the zone knows
  > takes the type, advancement, guild, union, cell, direction,
  > standing, colours, aura, health, alignment, rank and speed in place,
  > its name kept. A creature that is
  > no NPC by its type shows whether its personal shop is open, and an
  > open one shows the shop's sign (or the default message) to an ouster
  > player. The ouster then takes the packet's effect statuses, is
  > dressed as the packet describes (new `CreatureState::SetAddonToOusters3`;
  > the old code cast whatever creature it held to a wearing one, which
  > is undefined behaviour for a plain creature (a stale ID can name a
  > monster), and the adapter, executable-side and so unpinned by the
  > test, now dresses only one that wears gear), and takes its pet and
  > its nickname. Without a player no sign is shown (the old code
  > dereferenced `g_pPlayer`). The debug line logs the creature's ID,
  > which its `%d` asked for; it passed the name. R1 is 89 Windows / 87
  > Ninja.
  - Owner: membership, R1, the entry and its adapter, and
    `test_add_ousters_replies.cpp` (a new ouster and one refused, a
    known one, an operator new and known, the shop and sign cases, new
    and known, an NPC's type, the pet, each kind of nickname, no zone,
    no hosts, no status array).
  - Recorded, not fixed: the executable's `SetAddonToOusters` indexes
    its arm table by the packet's arm type unchecked.

- [x] **4.184 A monster's corpse appearing:** move
  `GCAddMonsterCorpseHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). A monster shaped as a slayer, a
  > vampire or an ouster (735 to 737, 792 to 800) is ignored. When no
  > creature (new `ZoneState::GetCreatureOnly`) and no item of the ID is
  > in the zone, a dead creature is made of the packet's type, walking,
  > with its ID, cell, direction, health (0 of 100), laid down as a
  > corpse (new `CreatureState::SetCorpse`), named, with its killer, in
  > guild 1 and headless when the packet says so; a corpse item of the
  > same ID at the cell holds it (new `ZoneState::NewCorpse`, which hands
  > the creature to the corpse) and the treasure count, and is laid in
  > the zone, after the item in its cell (new `ZoneState::GetItemID`) if
  > the zone refuses it first. A Christmas tree (482) also takes its
  > effect and, as a 650 does, shows its name. An item already there
  > takes the treasure count. A creature the zone knows dies where it is
  > (new `CreatureState::IsDead` and `SetItemCount`). The first time the
  > player is the killer the kill's help shows (once a run, a function
  > static as before); without a player it does not (the old code
  > dereferenced `g_pPlayer`). Once moved, a corpse the zone refused
  > twice, or whose cell's item would not go, is freed with its creature
  > and the creature left alone: the old code then read the freed
  > creature's type, and for a Christmas tree wrote to it. R1 is 88
  > Windows / 86 Ninja.
  - Owner: membership, R1, the six entries and adapters, and
    `test_monster_corpse_replies.cpp` (the ignored types and their
    edges, a new corpse with and without its head, one laid after the
    item in its cell (a Christmas tree too), one freed when the zone
    will not take it, the Christmas tree and 650, an item already
    there, a known creature alive and dead, the player's first kill, no
    player, no zone, no hosts, no corpse, no status array).

- [x] **4.185 A vampire's corpse appearing:** move
  `GCAddVampireCorpseHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). When no creature and no item of the
  > ID is in the zone, a dead creature that wears gear is made with the
  > packet's advancement, type (by its shape and coat, or the operator's
  > for competence 0) and sex, flying if a bat and walking otherwise,
  > with its ID, cell, direction and health (0 of its most), laid down
  > as a corpse, named, in its guild, with its attack speed, alignment
  > and skin and coat colours; a corpse item of the same ID at the cell
  > holds it and the treasure count, and is laid in the zone, after the
  > item in its cell if the zone refuses it first, or freed with its
  > creature if it still cannot be laid. A corpse already there takes
  > the treasure count; another item, nothing. A creature the zone knows
  > dies where it is. R1 is 87 Windows / 85 Ninja.
  - Owner: membership, R1, and `test_vampire_corpse_replies.cpp` (a new
    corpse of each sex and a bat, an operator's, one laid after the
    item in its cell, one freed when the item will not go or the zone
    refuses again, a corpse and another item already there, a known
    creature alive and dead, one now a bat (which walks) and an
    operator, no zone, no hosts, no corpse, no status array).

- [x] **4.186 A slayer's corpse appearing:** move
  `GCAddSlayerCorpseHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). When no creature and no item of the
  > ID is in the zone, a dead creature that wears gear is made with the
  > packet's advancement and type (a male or female slayer by its sex,
  > the operator's for competence 0), in its guild, walking, with its
  > ID, cell, direction, skin colour and health (0 of its most), dressed
  > as the packet describes (4.178's `SetAddonToSlayer3`), laid down as
  > a corpse and named; a corpse item of the same ID at the cell holds
  > it and the treasure count, and is laid in the zone, after the item
  > in its cell if the zone refuses it first, or freed with its creature
  > if it still cannot be laid. A corpse already there takes the
  > treasure count; another item, nothing. A creature the zone knows
  > dies where it is. Two unused includes, `AddonDef.h` (an executable
  > header) and `DrawTypeDef.h`, are gone. R1 is 86 Windows / 84 Ninja.
  - Owner: membership, R1, and `test_slayer_corpse_replies.cpp` (a new
    corpse of each sex, an operator's of each sex, one laid after the
    item in its cell, one freed when the item will not go or the zone
    refuses again, a corpse and another item already there, a known
    creature alive and dead and an operator of each sex, no zone, no
    hosts, a zone that cannot make a creature, no creature host, no
    corpse, no status array).

- [x] **4.187 An ouster's corpse appearing:** move
  `GCAddOustersCorpseHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). When no creature and no item of the
  > ID is in the zone, a dead creature that wears gear is made with the
  > packet's advancement and type (an ouster's, the operator's for
  > competence 0), with its ID, cell, direction and health (0 of its
  > most), laid down as a corpse and named, in its guild, with its
  > attack speed, alignment, hair and coat colours, and dressed as the
  > packet describes (`SetAddonToOusters3`); the corpse item, the
  > refused and freed corpse and an item already there behave as in
  > 4.184-4.186. A creature the zone knows dies where it is and is
  > dressed if it wears gear, which the old `dynamic_cast` to
  > `MCreatureWear` decided and the adapter's `IsWear()` check now
  > does. Its type changes only when it differs from an ouster's, so an
  > ouster told it is an operator keeps the ouster's type, as before.
  > R1 is 85 Windows / 83 Ninja.
  - Owner: membership, R1, and `test_ousters_corpse_replies.cpp` (a new
    corpse and an operator's, one laid after the item in its cell, one
    freed when the item will not go or the zone refuses again, a corpse
    and another item already there, a known creature alive and dead, an
    operator of another type and of the ouster's, no zone, no hosts, a
    zone that cannot make a creature, no creature host, no corpse, no
    status array).

- [x] **4.188 An NPC appearing:** move `GCAddNPCHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). When the zone has no creature of the
  > ID, an NPC is made (`ZoneState::NewNPC`, new) in the guild its
  > sprite says: Eve for a slayer's, the ousters' for an ouster's,
  > otherwise Tepez in zones 1007, 1114 and 1115 and Bathory elsewhere,
  > the zone being the player's (`ZoneFlags.h`). It takes the NPC ID's
  > type, walks, and takes its ID, cell and direction. It stands, is
  > named by the packet in zones 1500-1506 and by the creature table
  > elsewhere, takes the packet's colours and its NPC ID
  > (`CreatureState::SetNPCID`, new), and has 100 health of 100
  > (Bathory, NPC 217, 10 and an alignment of -10000). The zone takes
  > it, and it is dressed as its NPC ID wears (`SetAddonToNPC`, new,
  > for a creature that wears gear). Types 638 and 657 take Amata's
  > lasting effect and types 639 and 654 Ripaty's
  > (`AttachLastingEffect`, new). If the zone refuses it, it is
  > deleted. An NPC the zone knows walks to the cell, faces the
  > direction, stands and takes the colours; another creature is left
  > alone. `MGuildType.h`, a header of guild ID constants, joins
  > `gamemodel` with it. The executable header `MinTr.h` and the unused
  > `ClientDef.h` and `MNPC.h` includes are gone, along with the
  > handler's own `extern` declarations of the zone flags. R1 is 84
  > Windows / 82 Ninja.
  - Owner: membership, R1, and `test_add_npc_replies.cpp`:
    - a new NPC of each guild rule, including the large or small zone;
    - the packet's name at the zone range's edges;
    - Bathory, and the four effect types;
    - one the zone refuses;
    - a known NPC, and a known creature that is no NPC;
    - no zone, no NPC made, no status array, no creature host, and no
      hosts.
  - Fixed after the move: an NPC whose creature table row has no
    name crashed the client. That is an NPC ID past the client's
    table, which a server with a newer table sends, or a row a
    truncated table file left unread (a row read from the file holds
    "" for an empty name). The empty `MString` converts to NULL, and
    `MCreature::SetName` runs `strlen` on it. `CreatureState::SetName`
    now passes a NULL name as an empty one for every caller;
    `test_add_npc_replies.cpp` makes both kinds of NPC.

- [x] **4.189 A trade's preparation:** move `GCTradePrepareHandler`
  into `gamemodel`.
  > **Status:** done (2026-10-14). When someone asks the player to
  > trade, three things can happen. If the player already asked that
  > one, the exchange opens and the player accepts. If the player asked
  > someone else, the player answers busy. Otherwise the player is
  > asked. Then the player faces the one asking, if the zone knows it.
  > A player who is busy refuses whatever comes: waiting on something
  > else, repeating an action (`PlayerState::IsRepeatAction`, new),
  > talking to an NPC, or a bat or a wolf. When the one asking cancels,
  > the exchange window closes, or the asking one. When the one the
  > player asked accepts, the exchange opens. When it refuses or is
  > busy, the player's wait ends, the windows close and the player is
  > told why. Opening the exchange and asking go through the new
  > `TradePresentation::RunExchange` and `RunExchangeAsk`. Without the
  > player or the trade register the reply does nothing. The old code
  > dereferenced the player first thing, and the register on the paths
  > that read it; its refusal, and its plain asking when the player
  > waited on no trade, ran without the register. The game creates the
  > register at start-up and frees it at shutdown. Facing the one asking needs a zone, which the old code
  > read unchecked. R1 is 83 Windows / 81 Ninja.
  - Owner: membership, R1, and `test_trade_prepare_replies.cpp`. It
    covers being asked by the one the player asked, by another while
    asking, and otherwise (by one the zone does not know, or without a
    zone). It covers each reason to refuse, and that waiting on the
    trade is not one. It covers cancelling with and without the
    exchange open; accepting, refusing and being busy, each when asked
    and when not; a code past the last; no player, no trade register
    and no trade host.

- [x] **4.190 The player transforming:** move `GCMorph1Handler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). With sound muted
  > (`WorldEvents::MuteSound`, new), the player takes the ID of the
  > packet's slayer or vampire. Under the new ID it leaves the zone
  > under its old one and joins it again (`ZoneState::SetPlayer`, new,
  > which sets the zone's player and the player's zone, as `MPlayer`).
  > It turns from a slayer into a vampire, in guild 2, or from anything
  > else into a slayer, in no guild (-1). It loses its quick slot and
  > arms bands, and attacks with bare hands at a reach of one cell. It
  > then takes its details, gear, inventory and the item at the mouse
  > from the packet, with its status calculated between the gear and
  > the inventory. Then sound plays again. New `PlayerState` entries,
  > which `GCUpdateInfo` will need too: `ChangeToSlayer`,
  > `ChangeToVampire`, `SetBasicAttack`, `SetPCSlayerInfo`,
  > `SetPCVampireInfo`, `SetPCOustersInfo`, `SetGearInfo`,
  > `SetInventoryInfo` and `SetExtraInfo`. The error log no longer
  > names `g_pZone` and `g_pPlayer`, because R4 reads a string as a
  > reference. R1 is 82 Windows / 80 Ninja.
  - Owner: membership, R1, and `test_morph_replies.cpp`: a slayer
    turning, a vampire and an ouster turning, a new ID, no zone, no
    player, and no hosts.
  - Fixed after the move: an ouster's info (or any type but a slayer's
    or a vampire's) left the client silent until something else
    unmuted it (the window coming back to the front, the sound option,
    a later transformation or `GCUpdateInfo`). The reply muted sound,
    refused the type, and returned without letting sound play again.
    The first fix unmuted on that return, which also unmuted a client
    muted for being in the background. The reply now mutes only once it
    takes the type, so a refused one leaves the sound as it was.
    `test_morph_replies.cpp` sends an ouster's info and expects no call
    at all, with the player untouched.

- [x] **4.191 An item picked up:** move `GCDeleteandPickUpOKHandler`
  into `gamemodel`.
  > **Status:** done (2026-10-14). The item in the player's check
  > buffer is taken out of it. When its ID is the packet's:
  > - an event skull (types 12-16) sounds and leaves the zone;
  > - money is added (the durability its low word, the silver its
  >   high), up to what the player may still carry;
  > - into the inventory, the item goes to its cell (and is picked off
  >   the ground) or piles on the same item there: the sum up to the old
  >   item's most, the picked item's most past it;
  > - onto the mouse, it goes there;
  > - into the quick slot (when the trade register waits on it), it goes
  >   into the slayer's belt or the ouster's arms bands (slots 3-5 the
  >   second), or onto the same item there, and the server is told.
  >
  > An item the player now has is marked usable or not for the player's
  > race, stats and sex (`PlayerState::CheckAffectStatus`, new) and
  > gives the skills it
  > enables, through gamemodel's own available-skill set
  > (`g_pSkillAvailable`, `MSkillManager.cpp`). Refreshing that set
  > goes through `BonusSkills::RefreshAvailableSkills`. A vampire's
  > portal item asks `MItem::IsPortalMarked` (new on `MItemHost`): the
  > portal item's vtable is the executable's. Picking an item off the
  > ground is `ZoneState::PickupItem` (new). A help event follows. A dead
  > `/* */` block and its `IsBombMaterial` declaration are gone. Two
  > paths that crashed now do nothing: removing an item from the zone
  > without a zone (the `RemoveItem` adapter checks it), and refreshing
  > the skills of bomb material without an available-skill set (the
  > `RefreshAvailableSkills` adapter checks it). R1 is 81 Windows / 79
  > Ninja.
  - Owner: membership, R1, and `test_pickup_replies.cpp`. It covers an
    empty buffer; another ID; event skulls and another class's type 12;
    money and its limit; the inventory cell, without a zone, and outside
    the grid; piling, up to and past each most, with an old item that
    does not pile, another type and another class; the mouse; the belt,
    empty, fitting, too full and another type; the arms bands at slots
    1, 3 and 4; a slayer without a belt; the trade register's wait; a
    slayer's skills (disabled and enabled bomb and mine uses); and a
    vampire's (only skills it has).
  - Fixed after the move: the quick-slot path asked
    `PlayerState::GetRace()==RACE_OUSTERS` where the old code asked
    `IsOusters()`. `GetRace` answers ousters for any tribe that is not a
    slayer's or a vampire's, an NPC's too. It now asks
    `CreatureState::IsOusters` of the player, as `GCAddOusters` does,
    and the test has a player whose race reads ousters but whose
    creature is not one.

- [x] **4.192 A skill used on an inventory item:** move
  `GCSkillToInventoryOK1Handler` into `gamemodel`.
  > **Status:** done (2026-10-14). The player's wait ends. When the
  > check buffer holds an item used that way, the buffer is cleared and
  > the skill acts:
  > - **Making holy water, a bomb, a mine, a pupa or a holy potion**
  >   (`PacketSkillToMakeItem`, which moves with the handler) takes one
  >   item from the stack. It takes all of it when one is left or the
  >   made item goes to the source's own cell. The made item goes to the
  >   packet's cell or piles on the same item there; holy water then
  >   numbers the stack the source was.
  > - **Installing a mine, opening a bloody tunnel or transforming**
  >   makes the item one used from the inventory
  >   (`PlayerState::SetItemCheckBuffer`, new).
  > - **A bloody mark** marks an unmarked portal item with the player's
  >   zone and cell (`MItem::MarkPortal`, new on `MItemHost` beside
  >   4.191's `IsPortalMarked`).
  > - **A transformation** changes the player's type through its result
  >   action.
  >
  > The item then shows the skill's effect. Whatever was in the buffer,
  > the skill's result delays the player's effects, the packet's
  > changes apply, and an effect status goes on. The
  > `__TEST_SUB_INVENTORY__` branches, which no build enables, are gone.
  > R1 is 80 Windows / 78 Ninja.
  - Owner: membership, R1, and `test_skill_to_inventory_replies.cpp`. It
    covers making each item (again onto the made stack, the last one,
    into the source's own cell, another item at the cell), the holy
    potion's type, the mine, the tunnel, the mark (unmarked, marked,
    another item, the small zone), each transformation, a skill past the
    action table, another buffer, the effect status from the packet and
    from the skill, and no player.

- [x] **4.193 The event notices:** move `GCNoticeEventHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). The server's event notices do five
  > kinds of thing:
  > - **Timed events.** They queue and remove events through
  >   `WorldEvents`: the tax, the master's combat, a kick-out, ground
  >   attacks, the premium half price, the flag war's items, Gilles de
  >   Rais's ending and the race war's effect.
  > - **Logs.** They write the system and notice logs, which are
  >   gamemodel's (`GameMessages`).
  > - **Wars.** They end wars through gamemodel's war manager. While the
  >   player's zone is at war, any war's end reads the player's zone's
  >   safety back from the zone table. A race war's end clears the race
  >   wars only.
  > - **Windows.** They show notice windows, popups and a few game
  >   windows.
  > - **Login and prices.** A login sets the head price, sends the crash
  >   report, and warns when five days or fewer of premium are left. The
  >   crown's price goes to gamemodel's price manager.
  >
  > The windows, popups, the crash report, the holiday flags and the
  > quest's ending go through a new `NoticePresentation` host
  > (`NoticePresentationHost.h`, 13 entries; the VS_UI popup is
  > `PopupText`). R1 is 79 Windows / 77 Ninja.
  - Owner: membership, R1, and `test_notice_event_replies.cpp`. It
    covers every timed event; the premium half price by race; wars over
    in and out of the player's zone; level and race wars over, with and
    without a war manager; every window and popup; the notice windows;
    the flag war, with the winner's race clamped; the login's premium
    warning at 0, 3, 5, 6, uncounted and negative days; the medals and
    the crown's price, read back through `GetItemPrice`; the eight
    present boxes; and no hosts. One mutant survives, and it is
    equivalent: the uncounted-days check is redundant, since 0xFFFE is
    never five days or fewer. Without a zone, `ZoneState::GetID`
    answers `ZONEID_NULL` (0xFFFF) where the old code dereferenced
    `g_pZone`. The horn then shows for zone 0xFFFF and the war checks
    find no war, instead of crashing.

- [x] **4.194 A gun reloaded:** move `GCReloadOKHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). A vampire takes no reload. The
  > magazine waiting in the player's check buffer, used from the
  > inventory or the quick slot, sounds its use and leaves the buffer.
  > One magazine from its stack goes into the gun in the player's right
  > hand, holding the packet's bullets. The last one leaves its
  > container; a larger stack makes a new magazine of the same type and
  > options and becomes one smaller. The player's status is calculated
  > again. Loading the gun is `MItem::ReloadGun` (new on `MItemHost`).
  > The adapter sets the magazine in the `MGunItem`, makes it the
  > current one and frees the one it held: a magazine's vtable is the
  > executable's, so gamemodel never casts to `MMagazine`. Freeing an
  > item forgets its description through `MItemHost::ItemDestroyed`, so
  > the old explicit call is not needed. R1 is 78 Windows / 76 Ninja.
  - Owner: membership, R1, and `test_reload_replies.cpp`. It covers the
    last magazine and a stack (of two and of three) in the inventory and
    the quick slot, an item that is no magazine, a vampire, another
    buffer status, and an empty buffer.
  - Fixed after the move: a reload without a gun in the right hand.
    With the hand empty, the old branch logged through the NULL item and
    crashed; its cleanup, never reached, would have freed a whole stack's
    remaining magazines. With another item there, the spent magazine
    leaked. Now the spent magazine is freed either way: the last one has
    already left its container, and of a stack only the new magazine
    goes. The test
    covers both hands, both containers, and the last magazine and a
    stack.

- [x] **4.195 The reconnect to the login server:** move
  `GCReconnectLoginHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). The game server sends the player
  > back to the login server. The connection closes and opens again to
  > the packet's address, non-blocking with no linger; both are
  > packetwire's `ClientPlayer` and `Socket`. A key packet goes out:
  > it is flushed (`LoginPresentation::FlushSocketOutput`, new, the
  > executable's `UpdateSocketOutput`), followed by a half second's
  > wait, and handed to the dispatcher, whose handler is a no-op. Then
  > the reconnect login goes out with the packet's key and whether the
  > player logs in automatically. The player then waits for the
  > character list (`LoginPresentation::EnterWaitPCListMode`, new). A
  > login server that cannot be reached throws, and nothing follows.
  > R1 is 77 Windows / 75 Ninja.
  - Owner: membership, R1, and `test_reconnect_login_replies.cpp`. It
    runs the real reconnect against a loopback listener, with and
    without automatic login, and checks the bytes sent (the key
    packet's random keys aside). It also covers a closed port.

- [x] **4.196 The reconnect to the game server:** move
  `LCReconnectHandler` into `gamemodel`.
  > **Status:** done (2026-10-14). The login server sends the player on
  > to the game server:
  > - The connection closes and opens again to the packet's address.
  > - A key packet goes out: it is flushed, followed by a half second's
  >   wait, and handed to the dispatcher.
  > - The connect goes out with the packet's key, the PC's type and
  >   name, and the machine's MAC address, and is flushed.
  > - Unless the transport is a web socket, the login server's check
  >   port is told of the player's character over UDP, through
  >   packetwire's `ClientCommunicationManager`. The address and port
  >   come from the Korean config, or from the foreign one for the
  >   player's dimension.
  > - The player waits for the game server's first update.
  >
  > New `LoginPresentation` entries: `EnterWaitUpdateInfoMode`,
  > `MacAddress` and `Dimension`. Without a host the connect carries six
  > zero bytes for the MAC address, rather than reading a NULL pointer.
  > R1 is 76 Windows / 74 Ninja.
  - Owner: membership, R1, and `test_reconnect_game_replies.cpp`. It
    runs the real reconnect against a loopback listener and checks the
    bytes the connect carries. It flushes at the queued sizes. It sends
    real UDP to probes for the Korean config and each foreign
    dimension, and covers no MAC address and an unreachable game server
    (`Error`, the old connection closed).

- [x] **4.197 An NPC's answers:** move `GCNPCResponseHandler` into
  `gamemodel`.
  > **Status:** done (2026-10-14). A player waiting on the NPC's answer
  > stops waiting. By its code, about 160 of them, the answer does one
  > of these:
  > - opens a window: repair, waypoints, silvering, the tutorial's, a
  >   guild's registration, the castle's tax and its rate, the
  >   advancement swap, the campaign and event item dialogs, a couple's
  >   name, the lottery or the skill downgrade;
  > - opens the stash, asking the server for its list when there is
  >   none yet;
  > - pops up a message: plain, with the player's or the guild's name,
  >   by the player's race, or by a couple's or a quest's reason;
  > - closes a talk, a repair, a silver coating or a stash purchase,
  >   with their mode, money, sounds and items;
  > - heals with a sound by race, takes balls from the inventory, or
  >   updates the castle's tax balance.
  >
  > A new `NPCPresentation` host (`NPCPresentationHost.h`, 22 entries)
  > carries the windows and popups. `DialogPresentation` gains
  > `ClosePCTalkDlg` and `UnSetLockInputPCTalk`, and `PlayerState`
  > gains `SetWaitVerify`. The 104 trailing Korean comments, which
  > quoted each popup's message (the string table holds those texts)
  > or named the window or code, are dropped. The other Korean comments are translated, and a dead
  > `/* */` block naming `g_pPlayer` is removed. R1 is 75 Windows / 73
  > Ninja.
  - Owner: membership, R1, and `test_npc_response_replies.cpp`. Its
    tables come from the moved handler, a faithful copy of the old one:
    46 dialog popups and 23 string popups. It also covers every window,
    the named messages, the race-bound messages, the stash, single and
    whole-gear repairs, failed repairs and silverings, silver coating
    and the weapon's damage, a stash purchase, healing and donations,
    the balls, the castle's tax, the couples and the quests' reasons,
    the lottery, the gnomes and the event messages.

- [x] **4.198 The player's update on entering a zone:** move
  `GCUpdateInfoHandler` into `gamemodel`, the last packet handler the
  executable compiled.
  > **Status:** done (2026-10-14). The game server's first word on a
  > zone carries the player's whole state. In order, the handler:
  > - drops the zone's events, sets the mode to wait for this update,
  >   shows the server's status, loads every addon sprite and mutes
  >   sound;
  > - makes the player anew. The old player, if any, leaves the zone
  >   and its special action is kept. The new player's addons are
  >   removed and its basic attack is melee;
  > - resets the skill table and tree. The previous creature type is
  >   the last transform the effects carry. The guilds' names are
  >   forgotten;
  > - stores the PC's record by race (changing the interface's race if
  >   it must), the SMS charge, the nickname, the non-PK flag, the
  >   union and the Powerjjang points;
  > - clears the stash, the windows, the talk box and the quick slots;
  > - runs the progress bar while the view starts. It sets the weather
  >   (rain or snow that stops is cleared), the darkness and light (a
  >   master's are fixed), the game time, the gear, the inventory, the
  >   item at the mouse, the motorcycle and the skills;
  > - sets the zone's creature sprites (only a large zone's move to a
  >   large zone releases the others), the position, and the zone
  >   (moved to from the game, loaded otherwise);
  > - turns on the connection's encryption and places the NPCs, named
  >   by the creature table outside the event zones 1500-1506;
  > - catches the frame count up with the time this took, sets the bat
  >   colour and applies the effects;
  > - turns sound back on, shows the HP, clears the messages and waits
  >   for the player's position.
  >
  > Hosts:
  > - A new `ZoneLoading` host (`ZoneLoadingHost.h`, 26 entries)
  >   carries the sprites, windows, progress bar, view, zone,
  >   encryption, frame clock and mode.
  > - `PlayerState` gains `GetSpecialActionInfo`,
  >   `SetSpecialActionInfo`, `SetAttackModeNormal`, `Recreate` and
  >   `SetXY`.
  > - `g_PreviousCreatureType` moves with the handler.
  > - `Client/COrderedList.h`, the forwarder to `basic`'s list, joins
  >   the membership.
  >
  > Comments: the Korean ones are translated, and the commented-out
  > code is dropped (it named `g_pPlayer`, and R4 counts that). R1 is
  > 74 Windows / 72 Ninja, and no packet handler is left in the
  > executable.
  - Owner: membership, R1, and `test_update_info_replies.cpp`. It
    builds each update from bytes as the server writes them and pins:
    - the whole call order of a slayer's update;
    - the player-less, morphed and transformed cases;
    - the skill table and guild resets, each race's record and
      interface, the bat colour, every nickname kind, the stash;
    - the weather's stops, a master's light, the game time and its
      ratio, the motorcycle;
    - the progress arithmetic, the sprite choice of a large zone, a
      move or a load, and the NPCs' names.
  - Found, not fixed:
    - `BloodBibleSignInfo`'s constructor leaves its list size unset,
      so its `write()` (which the client never calls) reads garbage.
    - An update for a zone the zone table lacks, while in a large
      zone, dereferences the missing row (`pZoneInfo->Property`), as
      before.

- [x] **4.199 The generators that attach effects to creatures:** move
  `MAttachCreatureEffectGenerator`, `MAttachCreatureEffectGroundGenerator`
  and `MAttachCreatureOrbitEffectGenerator` into `gamemodel`.
  > **Status:** done (2026-10-14). Each generator finds its creature: in
  > the zone, else a fake one, else (for the plain generator) the
  > creature lying in a corpse item. It then attaches an effect, which
  > takes the direction, the action and target it follows, and the
  > power. There is none for no creature or no effect.
  > - The ground generator lays the effect on the ground, turning
  >   Sword Wave 1 one step back.
  > - The orbit generator keeps a summoned elemental's orbit step from
  >   the orbit effect already on the creature, or picks one at random.
  >   For an elemental's attack or heal it clears the creature's effects
  >   first and stops the new effect orbiting.
  > - The plain generator:
  >   - raises a water emission by its step;
  >   - picks a golemer's, alcan's or bat's Jabbing Vein;
  >   - picks a random bomb or stone skin frame;
  >   - picks a Distance Blitz frame by the tiles covered;
  >   - turns Piercing round.
  >
  > `CreatureState` gains `CreateAttachEffect`, `IsExistAttachEffect`,
  > `FirstAttachEffect` and `ClearAttachEffect`. The corpse's creature
  > comes through `ZoneState::GetCorpseCreature`, because `MCorpse` is
  > the executable's (`MItemUse.cpp`). The pixel-to-tile conversion is
  > `WorldTileGeometry`'s, which `MTopView`'s static one forwarded to.
  > The Korean comments are translated. R1 is 71 Windows / 69 Ninja.
  - Owner: membership, R1, and `test_attach_creature_effect_generators.cpp`
    (real attach and orbit effects over fake zones and creatures).
  - Found, not fixed: the plain generator, given an item that is not a
    corpse (or a corpse with no creature), still asks to attach the
    effect to no creature, and the executable's adapter then
    dereferences null, as before.

- [x] **4.200 The guided and homing attacks on a creature:** move
  `MAttackCreatureEffectGenerator` and
  `MAttackCreatureHomingEffectGenerator` into `gamemodel`.
  > **Status:** done (2026-10-14). Both read the sprite, the creature
  > and the frames, and make nothing without them. Each effect takes
  > the steps and frames, the power and, once the zone keeps it, the
  > action and target it follows.
  > - The guided attack traces the creature, turning to it as it
  >   starts. A second and third arrow, bolt or tooth at their skill's
  >   steps 2 and 3 fly 12 pixels to either side: across for the
  >   directions 2 and 6, up and down otherwise. Each gets its own copy
  >   of the target.
  > - The homing attack starts a tile above its point and heads for a
  >   tile above the creature, at the angle of its direction, turning
  >   13 degrees a step. A halo attack turns 30 degrees (45 across, and
  >   then it lasts 8 frames) and starts two directions to one side or
  >   the other at random.
  >
  > Both use the services the creature parabola generator already has
  > (`MCreatureParabolaEffectHost`): the sprite, the creature's
  > position, the frames and the zone's queue. `GameInit.cpp` installs
  > that one host for all three. The Korean comments are translated.
  > R1 is 69 Windows / 67 Ninja.
  - Owner: membership, R1, and `test_creature_attack_effect_generators.cpp`.
    Each made effect is flown for three updates beside a twin set as the
    generator should set it. This pins the guided effect's turn to its
    creature, and the homing effect's starting and turning angles.
  - Fixed after the move: when the zone kept the first guided effect but
    refused a later one, the generator returned false, and the effect
    generator table then deleted the target the first effect owned (a
    double free). It now returns true once the first effect is kept.

- [x] **4.201 Fire piercing and playing with fire:** move
  `MFirePiecingEffectGenerator` and `MPlayingWithFireEffectGenerator`
  into `gamemodel`.
  > **Status:** done (2026-10-14). Both lay fires on the tiles.
  > - **A start** (no previous fire): a fire stands half a tile from
  >   the point, in its direction, with that direction's fire sprite
  >   from one of the two sets at random. Fire piercing adds the two
  >   neighbouring fires, 3 in all. Playing with fire adds all 16 round
  >   it and shakes the screen.
  > - **A continuation** from a fire: one more stands beside it, where
  >   that fire points. Fire piercing switches sets at random. Playing
  >   with fire turns to the next fire, wrapping from the last to the
  >   first, and from the 113th continuation after a start switches
  >   sets at random.
  >
  > The first sprite must be in the table. Each effect takes the steps,
  > frames, direction and power, and may overlap. The first effect the
  > zone keeps follows the action and target; later ones follow a copy
  > of the target set at their place.
  >
  > Services:
  > - Both use the zone attack's services (`MZoneAttackEffectHost`: the
  >   sprite with its frames, and the queue). `GameInit.cpp` installs
  >   that one host for all three.
  > - The screen shake goes through `WorldEvents::AddEvent`.
  > - A later fire whose sprite is past the table is now skipped. The
  >   old code read the table's range-checked default row for it and
  >   still laid a fire. A later fire is always one of the 32 fires, so
  >   only a table loaded shorter than that (or a missing view) refuses
  >   one.
  >
  > The Korean comments are translated. R1 is 67 Windows / 65 Ninja.
  - Owner: membership, R1, and `test_fire_effect_generators.cpp`. It
    covers every direction's start, the continuation from each of the
    32 fires, the turn after 112, the target and its copies, and the
    refusals.
  - Found, not fixed:
    - `MPlayingWithFireEffectGenerator` leaves its continuation count
      unset until its first start. A continuation before any start
      would read it uninitialised, though a continuation always
      follows a start's fire.
    - A start in no known direction keeps the sprite it was given,
      and playing with fire shakes the screen even when that sprite is
      then refused.
    - Both return true even when the zone keeps no fire (a fire point
      off the map). The caller then neither deletes the target nor
      hands it on, so it leaks, and its result runs only if it was due.

- [x] **4.202 Ground effects at a point and pixel ripples:** move
  `MAttachZoneEffectGenerator` and `MRippleZonePixelEffectGenerator`
  into `gamemodel`. They were the last effect generators the
  executable compiled.
  > **Status:** done (2026-10-14).
  > - **The attach-zone generator** lays a ground effect at a point:
  >   - blood at a random spot near it, in a random frame;
  >   - a quest's magic zone up and to the left;
  >   - Great Ruffian's axe, which shakes the screen;
  >   - by the action's grade, a row, a cross or a square of tiles round
  >     the point. The first follows the target and the others copies.
  >   - Mercy Ground's nine parts at the point.
  > - **The pixel-ripple generator** lays an effect a step from its start
  >   toward the target, and points the target past it:
  >   - Power of Land's stones and earthquakes go on the ground, a few
  >     pixels on, faster at the target's first phase;
  >   - a blood transfusion flows toward a creature unless it is
  >     already there, green when gore is off;
  >   - any other effect lies at the target itself.
  > - A sprite past the table makes none, and only an effect the zone
  >   keeps is linked.
  >
  > Services:
  > - The attach-zone generator uses the ground-pattern services (an
  >   `MFixedZoneEffectHost` whose queue keeps an effect only if the
  >   zone retains it on the ground). The pixel ripple uses the ripple's
  >   (`MRippleZoneEffectHost`, ground or not).
  > - The shake goes through `WorldEvents::AddEvent`. The creature
  >   comes through `ZoneState`/`CreatureState`. The tile maths is
  >   `WorldTileGeometry`'s, which `MTopView` and `MCreature` forwarded
  >   to.
  > - `MercyGroundEffect` becomes a private static member, so it can
  >   reach the host.
  >
  > Behaviour changes:
  > - `MZone::AddGroundEffect` only borrows the pointer, so a refused
  >   ground effect used to leak. The host now frees it, and the
  >   generator links only effects the zone keeps. (The grade branch
  >   used to link a copy of the target to a refused, leaked effect.)
  > - `AddGroundEffect` returns true even when it refuses (Mercy or
  >   Magic Elusion already on the tile, an ice field on the regen
  >   tower). The plain attach branch and a ground pixel ripple
  >   therefore always returned true. The host checks that the zone kept
  >   the effect, so both now return false on a refusal. The effect
  >   generator table then plays no skill sound, runs the target's
  >   result at once and deletes the target. Before, the target rode
  >   the leaked effect, and its result ran only if it was already due.
  >   Recasting Magic Elusion on the same tile is now silent.
  > - A sprite past the table makes no effect. Before, the table's
  >   range-checked default row still drew one.
  >
  > The dead comments naming `g_pTopView` are dropped (R4 counts them),
  > and the Korean comments are translated. R1 is 65 Windows / 63
  > Ninja.
  - Owner: membership, R1, and `test_ground_effect_generators.cpp`,
    which pins the hand-computed pixel steps.
  - Found, not fixed: the attach-zone generator returns true from the
    grade and Mercy Ground branches when the zone keeps nothing, or
    refuses only the first tile or part, or when Mercy part 1's sprite
    is refused. The original target then goes to no effect, and the
    caller neither deletes it nor hands it on, so it leaks.

- [x] **4.203 The unused door actions and the colour-set table:** retire
  `MInteractionObjectAction.cpp`, and move `CUsingColorSetTable` into
  `gamemodel`.
  > **Status:** done (2026-10-14).
  >
  > **`MInteractionObjectAction.cpp`** held `MInteractionObject`'s door,
  > trap and switch actions (a door blocked its tile while closed).
  > Nothing calls them: `SetAction` and `SetNextAction` are disabled in
  > this client. So the file and the three declarations go. No compiled
  > behaviour is lost.
  >
  > **`CUsingColorSetTable`** marks which of the index sprites'
  > `MAX_COLORSET` colour sets a sprite may use:
  > - all are off at first;
  > - a set, or a range (start below end, both in range), turns on or
  >   off, and anything out of range is ignored;
  > - a node saves its count and a byte a set, and loads only a file of
  >   its own count.
  >
  > `GameInitInfo.cpp` still loads it at start-up (nothing reads it
  > afterwards), so it moves rather than goes. Its three constants move
  > from `CIndexSprite.h` (SpriteLib) to `basic/ColorSetDef.h`, which
  > both include. The Korean comments in the touched files are
  > translated. R1 is 63 Windows / 61 Ninja.
  - Owner: membership, R1, and `test_using_color_set_table.cpp`.
  - Fixed after the move: `SaveToFile` wrote the first set's flag for
    every set (the address of the array, not of each element). It now
    writes each set's own flag, and a saved node loads back as it was.
    Only a commented-out tool path saves.
  - Fixed later (4.204's PR): `LoadFromFile` read the count into an
    uninitialised int. A file cut short before its count filled only
    the bytes it had and left the rest garbage. The skip loop could then
    make up to 2^31 failed reads at start-up. A count that came out as
    495 returned true with the flags unchanged, since failed reads store
    nothing. It now returns false when the count cannot be read. (A file
    cut short inside the flags still returns true with the unread flags
    unchanged; the table's loader stops on the bad stream.)

- [x] **4.204 The pack-file record and the objects' screen rects:** move
  `PackFileManager.cpp` (`PackFileInfo`) and `MObjectScreen.cpp` into
  `gamemodel`.
  > **Status:** done (2026-10-14).
  >
  > **`PackFileInfo`** is a packed file's record: its ID, name and
  > position in the data file. It is saved as the ID (4 bytes), the
  > name's length (1 byte) and bytes, and the position (4 bytes), and
  > loaded in that order. The wave pack (`WavePackFileInfo`, Windows
  > only) derives from it.
  >
  > **`MObject::SetScreenRect` and `AddScreenRect`** set an object's
  > screen rectangle as given, or grow it to take in another. While the
  > world draw interpolates, both rectangles are moved back by the
  > object's draw gap and the camera's, so picking works in exact tick
  > space.
  > - The interpolation state these read (`g_bInterpolateDraw`,
  >   `g_DrawCamGapX/Y`) moves with them, from `CGameUpdate.cpp` to
  >   `MObjectScreen.cpp`, together with the comment saying why picking
  >   adds the gap back.
  > - The game update still sets that state, through `extern`s.
  >
  > R1 is 61 Windows / 59 Ninja.
  - Owner: membership, R1, `test_pack_file_info.cpp` and
    `test_object_screen_rect.cpp`.
  - Fixed after the move: `PackFileInfo::LoadFromFile` read the name's
    bytes into an unterminated stack buffer and built the name from
    it, so a name loaded after a longer one kept the longer one's tail
    (or ran past the buffer). It now takes exactly the name's bytes. No
    shipped path loads one today: the wave pack's index load is
    commented out in `GameInit.cpp`, and it looks records up by ID.
  - Found, not fixed: a name over 255 bytes saves as its length's low
    byte and that many bytes, since the format holds one length byte.
    Only tools save.

- [x] **4.205 The video stub, the update base and the work-queue node
  base:** move `CAvi.cpp`, `WinLib/CWinUpdate.cpp` and `MWorkNode.cpp`
  into `gamemodel`.
  > **Status:** done (2026-10-14). Three small classes the game builds
  > on, with no executable dependency:
  > - `CAVI`, the opening video player, is a stub in this client:
  >   nothing opens or plays, and its end flag starts clear.
  > - `CWinUpdate`, the base of the game's update loops, does nothing
  >   itself.
  > - `MWorkNode`, a job for the loading thread, starts with type 0 and
  >   not running. Its type can be set and asked for, and `Stop` marks it
  >   stopped.
  >
  > `CAvi.h` now includes `basic/Platform.h` for its Windows types,
  > rather than counting on the precompiled header. R1 is 58 Windows /
  > 56 Ninja.
  - Owner: membership, R1, and `test_small_base_classes.cpp`.

- [x] **4.206 The debug kit:** move `DebugKit.cpp` (`CDebugKit`,
  `CMessageStringTable`) into `gamemodel`.
  > **Status:** done (2026-10-14).
  > - **`CDebugKit`** reads `DebugInfo.ini` (`key : value` lines,
  >   through packetwire's `Properties`) from the working directory: the
  >   message file's name, three logging flags (on when above 0), the
  >   authentication key map and the game version. Each takes its
  >   default when absent, and with no file it does not load.
  > - **`CMessageStringTable`** loads one name a line (up to a comma,
  >   past leading blanks, blank lines skipped) and answers
  >   `UNKNOWN_MESSAGE` for an ID it lacks.
  > - `InitDebugInfo` and `ClearDebugInfo`, which fill `Client.cpp`'s
  >   debug globals from the kit, move into `Client.cpp`, which defines
  >   those globals and calls `InitDebugInfo` (`GameInit.cpp` calls
  >   `ClearDebugInfo` through its extern, also under `DEBUG_INFO`).
  >   They compile only with `DEBUG_INFO`, which `DebugKit.h` defines
  >   only with `WINDOWS_SCREEN_DISPLAY`, and no build sets that.
  >   So the kit's classes name no executable symbol.
  > - The include checker's M2 now allows `DebugKit.h` in gamemodel,
  >   whose classes are now gamemodel's. W2 still bans it in packetwire,
  >   which does not link gamemodel. `StdAfx.h`, which the kit includes,
  >   joins the membership.
  >
  > R1 is 57 Windows / 55 Ninja.
  - Owner: membership, R1, and `test_debug_kit.cpp`.
  - Found, not fixed: `CMessageStringTable::LoadFromFile` drops a last
    line that has no newline (it stops on eof before storing the line).

- [x] **4.207 Retire `Globals.cpp`:** delete it, with the clipboard
  parse in `Netmarble.cpp` that its last stub fed and `NMCrypt.h`.
  > **Status:** done (2026-10-14). Nothing compiled referenced
  > anything in `Globals.cpp`, except one stub that fed only dead code:
  > - **`g_pWavePackFileManager`:** off Windows, only a Windows-guarded
  >   delete and comments name it. On Windows, `GameInit.cpp`'s extern
  >   is `WavePackFileManager*`, which `WavePackFileManager.cpp` (built
  >   only there) defines. `Globals.cpp`'s `CWavePackFileManager*` was a
  >   separate, unreferenced symbol.
  > - **`gbl_info_show` and `gbl_show_item`:** every reader is in VS_UI
  >   under `#ifndef _LIB`, and VS_UI defines `_LIB` publicly, so no
  >   build compiles one.
  > - **`CheckMacScreenMode`:** the off-Windows empty body. The one call
  >   (`Client.cpp`'s `InitApp`) sits under `PLATFORM_WINDOWS`, where
  >   `PacketFunction.cpp` defines the real function.
  > - **`SendUserIDToGameMonA`:** nothing calls it.
  > - **`GetNMClipData`:** a stub returning false, since NMCrypt's
  >   `.lib` left the build in bb70900b. It was the only source for
  >   Netmarble's `AnalyzeArgument`, which therefore always returned
  >   FALSE at its first line. So `ParsingNetmarble` returned false when
  >   the command line had no space, or 4096 bytes or more after it, and
  >   otherwise always showed its "launch from the homepage" message and
  >   exited.
  >
  > `ParsingNetmarble` now does exactly that and nothing more.
  > `AnalyzeArgument`, `_StrTok`, `HexStringToDec` (named only in
  > comments), the cookie globals nothing reads, a dead struct copy and
  > `NMCrypt.h` (no other includer) go. `g_CpCookie` stays: GameUI's
  > `UI_RunConnect` reads it, and it was empty before as now.
  >
  > R1 is 56 Windows / 54 Ninja.
  - Owner: R1. Retired code, so no test. The argument is above, and
    every tree that builds `DarkEden` links without the file.

- [x] **4.208 Text on a given surface:** `TextService::RenderText`
  takes the surface it draws on, and TextSystem defines it;
  `TextServiceScreen.cpp` goes.
  > **Status:** done (2026-10-14).
  > - **Before:** `RenderText` was the one `TextService` member the
  >   executable defined (task 5.3's split), because it drew on
  >   `g_pLast`.
  > - **Now:** it takes the surface. It is defined in
  >   `RenderTargetSpriteSurface.cpp` with the target it wraps the
  >   surface in, and it draws exactly what `DrawLine` does with the
  >   default style in white at (x, y). It draws nothing on a null
  >   surface or one without a backend.
  > - **Callers:**
  >   - The executable's 31 calls (`CGameUpdate`, `CWaitUIUpdate`,
  >     `GameMain`, `MTopView`) pass `g_pLast`.
  >   - VS_UI's two (the title's server list, `PLATFORM_POSIX`) pass
  >     `gpC_base->m_p_DDSurface_back`, which `GameInit` sets to
  >     `g_pLast`. VS_UI no longer needs an executable definition for
  >     this.
  >
  > R1 is 55 Windows / 53 Ninja.
  - Owner: R1, and two tests in `test_text_service.cpp`. The first
    compares the pixels with `DrawLine`'s and checks they are white and
    below and right of the point. The second covers the null and bare
    surfaces.
  - Mutants: 8 of 11 are caught. The channels, alpha, a swapped or
    shifted point and a different font size fail. Deleting the
    definition breaks the link.
  - Equivalent mutants:
    - A `maxWidth` other than 0: it only moves centred and right-aligned
      text.
    - The default font set explicitly at size 16.
    - Dropping the surface guard: the target and `DrawLine` already
      ignore a missing surface.

- [x] **4.209 The packet wait:** move `CWaitPacketUpdate.cpp` into
  `gamemodel` behind `CWaitPacketHost`.
  > **Status:** done (2026-10-14).
  > - **The loop:** the mode loop that waits for the server (character
  >   select, the world and server lists, the zone's first position).
  >   - `Init` clears the input receivers.
  >   - `SetDelay` stamps the frame clock and gives up `delay` ms later.
  >   - Each `Update`: when strictly past the limit, it goes back to the
  >     main menu, disconnected, and carries on. It then reads the
  >     socket and stops when that fails. Next it enters the queued next
  >     mode, clearing it, and stops. Otherwise it writes the socket.
  > - **The host:** the input receivers, the frame-clock stamp,
  >   `SetMode`, the disconnect and the socket reads and writes go
  >   through `CWaitPacketHost`. GameInit passes it to the constructor.
  >   `g_FrameNow` and `g_ModeNext` were already gamemodel's.
  > - **The instance:** `g_pCWaitPacketUpdate`'s definition moves to
  >   `GameInit.cpp`, which creates it.
  > - Korean comments in the moved file and its header are translated.
  >
  > R1 is 54 Windows / 52 Ninja.
  - Owner: membership, R1, and `test_wait_packet_update.cpp` (5 tests,
    one recording host).
  - Mutants: all 12 are caught. Taking the source out of the
    membership breaks the test link.

- [x] **4.210 Retire the opening:** delete `COpeningUpdate.cpp`, the
  opening mode's loop, with `g_pAvi` and the `CAVI` stub it was the only
  user of.
  > **Status:** done (2026-10-14).
  >
  > **Why it is dead:** nothing enters `MODE_OPENING`.
  > - Its one `SetMode` call (`GameInit.cpp`'s start-up) is commented
  >   out.
  > - `g_Mode` starts at `MODE_NULL`.
  > - Every other `SetMode` argument is another literal mode or
  >   `g_ModeNext`. `g_ModeNext` is assigned only `MODE_NULL`,
  >   `MODE_QUIT`, `MODE_WAIT_PCLIST`, `MODE_LOGIN_WRONG` and
  >   `MODE_WAIT_SELECTPC` (corrected after review: the first account
  >   listed only three).
  > - The direct writes to `g_Mode` outside `SetMode` are literals too:
  >   `MODE_MAINMENU`, `MODE_CHANGE_OPTION`, `MODE_WAIT_UPDATEINFO`.
  > - Nothing casts a number to `CLIENT_MODE`, takes `g_Mode`'s address
  >   or does arithmetic on it.
  >
  > So the mode's case in `SetMode` and `COpeningUpdate`'s `Init`,
  > `PlayMPG` and `Update` never ran. `g_pAvi`, which only `PlayMPG`
  > set, was always null, which makes three more blocks dead:
  > - `Client.cpp`'s `MM_MCINOTIFY` branch, whose other half was all
  >   commented out;
  > - its `WM_KEYDOWN` branch, which ended the opening;
  > - `GameInit.cpp`'s release block.
  >
  > **What goes:** those blocks, the loop's creation and deletion, and
  > `g_pAvi`. With them goes `CAVI`, the stubbed MCI player that joined
  > gamemodel in 4.205, now without a user, together with its test.
  >
  > **What stays:** the enum keeps the value, so no mode renumbers. (The
  > retirement first kept an empty `MODE_OPENING` case in `SetMode` on
  > the belief that its switch had no default. It has one, and the
  > follow-up dropped the case.)
  >
  > R1 is 53 Windows / 51 Ninja.
  - Owner: R1. Retired code, so no test. The argument is above, and
    every tree that builds `DarkEden` links without the files.

- [x] **4.211 The frame present:** move `CSDLGraphics::Flip` from
  `CSDLGraphicsFlip.cpp` into dxlib.
  > **Status:** done (2026-10-14).
  > - **Before:** `Flip` lived in the executable only because it read
  >   `g_pBack`, and dxlib cannot include `CSpriteSurface.h`.
  > - **Now:** dxlib holds a back-buffer source the executable
  >   installs at the top of `ClientMain`. The source gives `g_pBack`'s
  >   backend surface, or none while `g_pBack` is null, because some
  >   flips run during the title loading screen, before the back buffer
  >   exists. `BackBuffer()` asks it.
  > - **`Flip`** is `PresentFrame(m_pSDLRenderer, BackBuffer())`.
  >   `PresentFrame`, the old body over a given renderer, does nothing
  >   without a renderer. Otherwise it finishes the sprite rendering,
  >   clears, draws a valid back buffer (letterboxed by
  >   `spritectl_present_surface`) and presents.
  > - **Callers:** all 11 live callers keep calling `Flip()` unchanged.
  >   Five more calls sit in `/* */` comments; the first account said
  >   13.
  >
  > R1 is 52 Windows / 50 Ninja.
  - Owner: R1, and `test_frame_present.cpp`. Its 5 tests, the last two
    driving a software renderer:
    - `BackBuffer` follows the installed source;
    - `Flip` asks the source once a flip (added by the follow-up);
    - nothing happens without a renderer;
    - an empty frame is cleared;
    - a 16x12 back buffer fills the middle 80x60 of a 100x60 target,
      with black bars.
  - Mutants: an ignored or unstored source, `Flip` not asking it, no
    clear and no draw fail. Without `PresentFrame` the test link fails.
  - Unpinned:
    - `Flip`'s one-line delegation: the class's renderer exists only
      after `Init`, which on Windows wraps a real window.
    - The renderer guard: SDL and SpriteLib ignore a null renderer.
    - `spritectl_finish_rendering`: no GPU path in the tests, and the
      present finishes first anyway.
    - `SDL_RenderPresent`: a no-op on a surface renderer.
    - The `backBuffer` validity guard: `spritectl_present_surface`
      rejects a null surface on the software path. On the GPU path,
      dropping it would checkpoint every modified surface on each
      title-loading flip.

- [x] **4.212 The append patch:** move `AppendPatchInfo.cpp` into
  `gamemodel` behind `AppendPatchHost`.
  > **Status:** done (2026-10-14).
  > - **The patch:** a sprite pack (a 2-byte count, then the data) and
  >   the packs to append to it in turn.
  >   - Each node starts writing where the file before it ended, adds
  >     its own count, and the size grows by its pack less the 2-byte
  >     count.
  >   - `ExecutePatch` copies each append file after its count to that
  >     position, writes the new count, and deletes the append file. A
  >     missing append file is skipped; a missing original fails.
  >   - The final information is checked against the file or by
  >     calculation.
  >   - The table saves and loads `AppendPatch.inf`.
  > - **Users:** both are live. `Client.cpp`'s `CheckTerriblePatch` runs
  >   at start-up and exits the client without the file, and the guild
  >   marks append through it.
  > - **The host:** its two progress-bar calls (a step per node, the
  >   text when a node applies) are the host's. `ClientMain` installs
  >   `UpdateProgressBar` and `SetProgressBarText`.
  > - **The header** includes `Platform.h` for `WORD` instead of relying
  >   on the executable's precompiled header. Its Korean comments, and
  >   those of the source, are translated; the runtime progress text
  >   stays.
  >
  > R1 is 51 Windows / 49 Ninja.
  - Owner: membership, R1, and `test_append_patch.cpp` (12 tests on
    scratch files, with the `.inf` layout built byte by byte; the fixes
    and #508's review added the 4-byte, sign, `Release` and short-file
    tests).
  - Mutants: all 21 are caught. Taking the source out of the membership
    breaks the test link.
  - Fixed, moved then fixed: the loader read each 4-byte size and
    position straight into a `long`. On LP64 (Linux, macOS) that filled
    only the low half, and a node from `new` kept garbage in the high
    half, so a loaded patch wrote at a wild position. The ASan tree's
    poisoned allocations showed it. Each field is now read as a 32-bit
    integer. `ANodeLoadsItsFourByteFieldsWhole` loads a node whose
    fields were all bits set and fails without the fix.
  - Fixed afterwards (#508's review): the loader trusted the file's node
    count, and the count was indeterminate when the file ended before
    it. A truncated or corrupt `AppendPatch.inf` loaded garbage nodes, or
    allocated a huge count of them from a failed stream. The count now
    starts at 0, and the load stops at the first node the file fails
    on, dropping that node. `ALoadStopsWhereTheFileDoes` covers a count
    of a million with one node, a node cut short, and no count. The first
    two fail without the fix every time; the third read an indeterminate
    count, which MSVC's /RTC1 fill makes negative, so that tree passed it.

- [x] **4.213 The tile renderer:** move `TileRenderer.cpp`/`.h` into
  SpriteLib (`Client/SpriteLib/`).
  > **Status:** done (2026-10-14).
  > - **What it is:** `MTopView`'s tile layer draws through it. It
  >   draws a pack's sprite at a point on its surface:
  >   - 0 draws nothing;
  >   - the null tile (`SPRITEID_NULL` or -1) draws the null-tile
  >     pack's sprite when there is one;
  >   - an ID past the pack or not loaded fails.
  >   `DrawTiles` walks an `ITileDataProvider`'s map within a viewport
  >   clamped to the map, a tile's width and height apart.
  > - **Why SpriteLib:** it depends only on `CSpriteSurface` and
  >   `CSpritePack`, so its home is SpriteLib. gamemodel, which does
  >   not link SpriteLib, could not take it.
  > - **Its header** includes `Platform.h` and SpriteLib's own headers
  >   instead of the executable's precompiled header.
  > - **Callers:** `MTopView.h` and `MZoneTileProvider.h` include
  >   `SpriteLib/TileRenderer.h`. The map viewer, which compiled its own
  >   copy, now links SpriteLib's; its Chinese comments are translated.
  >
  > R1 is 50 Windows / 48 Ninja.
  - Owner: R1, and `test_tile_renderer.cpp` (6 tests on real surfaces
    and packs).
  - Mutants: all are caught but two equivalents. Dropping the file from
    SpriteLib breaks the test link. (The first account called four
    equivalent. #509's review showed two were not: the ID bounds matter
    because `operator[]` takes a `WORD`, so 65537 and -65535 wrap to a
    loaded entry; tests now pin both. It also added pins for the
    viewport's left clamp on the no-lock path MTopView uses, drawing
    before `Init`, `Release` forgetting the null-tile pack, and
    `DrawTiles` unlocking.)
  - Equivalent mutants:
    - the null tile's loaded check, since an unloaded sprite draws
      nothing;
    - `IsInit` with `||`, since `Init` and `Release` never leave one of
      the two set.

- [x] **4.214 The MIDI player's state:** retire `MMusic`'s unbuilt
  MCI branch and move the stub that every build compiles into
  `gamemodel`.
  > **Status:** done (2026-10-14).
  > - **The dead branch:** the real Win32 MCI/MIDI implementation (the
  >   original VC6 code) sat behind `__USE_REAL_MIDI__` since 9e60d44e
  >   disabled it, and nothing defines that, so every build compiled the
  >   `#else` stub. The branch is retired, with its
  >   `<MMSystem.h>` include, the error-text buffer only it used, and
  >   the window handle nothing read.
  > - **The stub:** its bodies are unchanged. The script that rewrote it
  >   checked them token for token against the old ones.
  > - **What it keeps:** the state the game asks about.
  >   - `Init` and `Play` report failure (nothing plays) but move the
  >     state.
  >   - `Stop`, `Pause`, `Resume` and `RePlay` move between loaded,
  >     playing and paused.
  >   - Before `Init`, every call but `SetVolume` refuses.
  >   - `g_Music` is gamemodel's.
  > - **The header** includes `Platform.h`. Its Korean notes on the MCI
  >   notification go; its member comments are translated.
  >
  > R1 is 49 Windows / 47 Ninja.
  - Owner: membership, R1, and `test_music.cpp` (8 tests, with #510's
    follow-up).
  - Mutants: 19 of 21 are caught. Taking the source out of the
    membership breaks the test link. #510's review found more:
    - A real survivor, now pinned: `Pause` without `|| !m_bPlay`. A
      second `Init` leaves a file loaded, neither playing nor paused,
      and then `Pause` must change nothing.
    - More equivalents, not in the first count: `Stop` not clearing
      the paused flag (no call leaves the player paused while
      playing), and any change to `UnInit`'s guard (`Stop` does
      nothing unless initialised and playing, and playing means
      loaded).
  - Equivalent mutants:
    - `Play` not stopping a loaded file first, since its flags are
      overwritten at once;
    - `Pause` not asking whether a file is loaded, since no call leaves
      one playing without being loaded.

- [x] **4.215 Retire the MP3 player:** delete `CMP3.cpp`/`.h` and every
  `__USE_MP3__` block.
  > **Status:** done (2026-10-14).
  > - **The macro:** `__USE_MP3__` was only ever commented out
  >   (`SoundSetting.h`), so since 9e60d44e moved the real MCI class
  >   behind it, every build compiled the `CMP3` stub (Windows compiled
  >   the MCI class before that, though nothing constructed it). The
  >   music streams as OGG.
  > - **Why the class is dead:** its one `new` sat under the macro, so
  >   `g_pMP3` stayed null. Outside the macro and comments, only three
  >   spots named it:
  >   - `SAFE_DELETE` at shutdown, of a null pointer;
  >   - two `g_pMP3->Play(false)` calls in `MTopView`'s Ousters finale
  >     events. They were unreachable: they sat in `case 1000` and
  >     `case 2000` of `ExcuteOustersFinEvent`, whose only creator sets
  >     the step to 0, and the one assignment of 1000/2000 is commented
  >     out, so the step only goes 0, 9998, 9999. (The first account
  >     called them live member calls through a null pointer; #511's
  >     review found them dead. Those two cases are dead code for a
  >     later slice.)
  >   - a Ctrl+0 debug key in `CGameUpdate`, behind `g_bTestMusic`,
  >     which nothing ever sets, inside `#ifdef OUTPUT_DEBUG`, which no
  >     build defines.
  > - **What goes:** those spots, `g_bTestMusic`, the class and its
  >   includes. The 21 `__USE_MP3__` blocks in `Client.cpp`,
  >   `GameInit.cpp`, `GameMain.cpp`, `GameUI.cpp`, `UIMessageManager.cpp`
  >   and `SoundSetting.h` are resolved as undefined: each `#ifndef` and
  >   `#else` body kept, each `#ifdef` body dropped, nested conditionals
  >   kept. A script did it, and the diff was read.
  > - **What stays:** `case DIK_0` keeps its label, falling through to
  >   `DIK_Z` as before.
  >
  > R1 is 48 Windows / 46 Ninja.
  - Owner: R1. Retired code, so no test. The argument is above, and
    every tree that builds `DarkEden` links without the files.

- [x] **4.216 The effect resource container:** move
  `EffectResourceContainer.cpp`/`.h` into framelib (`Client/framelib/`).
  > **Status:** done (2026-10-14).
  > - **What it is:** the effect frame, sprite and palette packs of a
  >   data directory, by blt type. `Load` reads each pack it finds and
  >   warns about the rest. It succeeds even with none, and once loaded
  >   it does not load again. The alpha packs take the first of their
  >   names that exists. Every getter answers null past its pack, and
  >   for a blt type without such a pack.
  > - **Why framelib:** it holds framelib frame packs and SpriteLib
  >   sprite and palette packs, and framelib already links SpriteLib.
  >   The header includes framelib's and SpriteLib's own headers, and
  >   the source `Frame_PCH.h`.
  > - **Users:**
  >   - The effect viewer uses it. It compiled its own copy and now
  >     takes framelib's.
  >   - `MTopView.h` includes `framelib/EffectResourceContainer.h` for
  >     its `m_EffectResources` member. The game never loads or reads
  >     that member; the review of a later slice may retire it.
  >
  > R1 is 47 Windows / 45 Ninja.
  - Owner: R1, and `test_effect_resource_container.cpp` (7 tests on
    scratch directories with saved packs; the sprite-getter bounds use
    directly sized packs, because a pack of unloaded sprites does not
    load back).
  - Mutants: 21 of 24 are caught. Renaming `Load`'s definition away
    breaks the test link. #511's review found more that survived, now
    pinned: every sprite getter's `>= 0` as `> 0` (ID 0 was never asked
    for), and the alpha palette's later names (only the first was
    saved). Still unpinned: the lowercase second names (`effect.efpk`,
    `effect.ppk`), which a case-insensitive disk cannot tell from the
    first.
  - Equivalent mutants:
    - `GetSprite`'s `spriteID >= 0`: the `DWORD` cast already puts a
      negative ID past the pack.
    - `GetSprite`'s shadow bound: `GetShadowSprite` checks it again.
  - Platform-dependent: the alpha palette's and the alpha frame pack's
    first names ("Effect.ppk", "Effect.efpk") survive on macOS and
    Windows, whose case-insensitive disks find them as the second
    names. Linux CI catches them.
  - Unpinned: loading the alpha, shadow and screen sprite packs. Their
    formats would need hand-built sprites; the normal pack's load, by
    the same code, is pinned.

- [x] **4.217 The loading thread's sprite-pack jobs:** move
  `MWorkNode` into basic and `MLoadingSPKWorkNode.cpp`/`.h` into
  SpriteLib, behind `MLoadingSPKHost`.
  > **Status:** done (2026-10-14).
  > - **The jobs:** each loads sprites of a pack from its SPK file:
  >   - Node1 reads a file of (sprite, position) pairs, then loads
  >     those sprites;
  >   - Node2 loads by a pair array it owns;
  >   - Node3 loads one sprite at a position;
  >   - Node4 loads a run of sprites from a position.
  >
  >   Node1-3 skip a sprite already loaded; Node4 loads every one, to
  >   keep its file position. A job stopped mid-run hands the rest back
  >   as a new job of the same type. A file that does not open fails the
  >   job.
  > - **Why the two moves:** the jobs need SpriteLib's packs, which
  >   gamemodel does not link, and derive from `MWorkNode`, which was
  >   gamemodel's (4.205). `MWorkNode` needs only `Platform.h`, so it
  >   moves down to basic, where SpriteLib and the executable's
  >   `MWorkThread` both see it. The jobs then move into SpriteLib.
  > - **The host:** opening a data file is the executable's
  >   (`FileOpenBinary`: the data-path spelling and the init-failure
  >   report). It goes behind `MLoadingSPKHost`, which `ClientMain`
  >   installs first thing. Without a host no file opens.
  > - Korean comments in both files are translated.
  >
  > R1 is 46 Windows / 44 Ninja.
  - Owner: R1, and `test_loading_spk_work_node.cpp` (7 tests on a real
    three-sprite SPK file and pair files).
  - Mutants: 16 of 17 are caught. Renaming the host setter away breaks
    the test link.
  - Unpinned: Node1 and Node2 copying the rest from the stop point `i`,
    and Node4 handing back from `i` and `tellg()`. A single-threaded
    test can only stop a job before it starts (`i` = 0, the start
    position); a stop mid-run needs the loading thread. (#512's review
    added pins for Node2's skip and failed open, every job's stop on
    success, and Node3's cleared hand-back.)

- [x] **4.218 The packet-handler registry:** move
  `PacketHandlerRegistry.cpp`/`.h` into `gamemodel`.
  > **Status:** done (2026-10-14).
  > - **What it is:** the composition root's table that binds each
  >   packet the client can receive to its handler. A packet the client
  >   accepts but nobody registered disconnects the client on arrival.
  > - **Why it can move:** every handler it names has been gamemodel's
  >   since 4.198, and it reaches nothing else of the executable.
  >   `GameInit` still calls `registerClientPacketHandlers()` at start-up.
  > - **The dispatcher** (packetwire) gains `handlerFor(id)`, the bound
  >   handler or null. Tests clear the table through
  >   `tests/support/PacketDispatcherTestAccess.h`, a befriended struct
  >   (the first account added a public `clearForTests()`; #512's review
  >   moved it out of the production interface). The tests that bind
  >   one handler now bind it whenever the table lacks it, so a clear
  >   between tests is safe.
  >
  > R1 is 45 Windows / 43 Ninja.
  - Owner: membership, R1, and `test_packet_handler_registry.cpp`. It
    walks the whole wire inventory (`tests/generated/WireInventory.inc`).
    Every received direction must be bound, except the three peer-file
    replies (`RCConnectVerify`, `RCRequestVerify`, `RCRequestedFile`)
    whose handlers task 5.2's eighth slice deleted. Of the packets the
    client only writes, only `CGConnectSetKey` may be. It also pins that
    a second call registers nothing, and pins the two dispatcher
    additions.
  - Mutants: all 10 are caught. Each kind of registration line dropped
    (plain, packet-only, debug-wrapped, and the three hand-written
    thunks), the run-once flag, and both dispatcher additions.

- [x] **4.219 The effect generator table:** move
  `MEffectGeneratorTable.cpp`/`.h` into `gamemodel`.
  > **Status:** done (2026-10-14).
  > - **What it is:** `Init` holds one generator of each kind (43), by
  >   ID. `Generate` starts an action's current phase on its target:
  >   - an action past the table, or no target: nothing;
  >   - a target whose phases do not match the action's, or a phase
  >     naming no generator: the result released unrun and the target
  >     dropped;
  >   - an action without phases: the result's time.
  >
  >   Otherwise the phase's generator gets the phase's step, count (the
  >   target's delay on a delay node), link count and sprite (Acid Touch
  >   and Bloody Nail at power 2 take their own); the start; the
  >   target's place and ID; and the target, unless this was its last
  >   phase. On success the phase's sound plays at the start's cell,
  >   and a result-time phase marks the result's time; on failure the
  >   result's time comes at once. At the result's time or the last
  >   phase the result runs and the effect leaves the player. When
  >   nothing was generated, or at the last phase, the target goes.
  >   `GenerateNext` carries an effect's target to its next phase the
  >   same way, from the effect's place.
  > - **Why it can move:** every generator has been gamemodel's since
  >   4.202, and the action-info table and the generator table's own
  >   global are library-defined. Its one executable reach, the phase's
  >   sound (`MTopView::PixelToMap` and `PlaySound`), now goes through
  >   gamemodel's `WorldEvents::PlaySoundAt` and `WorldTileGeometry`.
  >   GameInit's `WorldEvents` host maps `PlaySoundAt` to the same
  >   `PlaySound` call. The `MTopView.h`, `ClientDef.h` and `DebugInfo.h`
  >   includes go (`DebugLog.h` instead).
  >
  > R1 is 44 Windows / 42 Ninja.
  - Owner: membership, R1, and `test_effect_generator_table.cpp` (15
    tests over a fake generator, recording hosts and counting targets
    and results).
  - Mutants: 27 of 31 are caught. Taking the source out of the
    membership breaks the test link.
  - Equivalent mutants:
    - a mismatched target's result not released before the target is
      deleted (the destructor discards it unrun anyway);
    - an action without phases not marking the result's time (its
      target has already ended);
    - a failed `GenerateNext` not marking the result's time (its
      condition also tests `!bGenerated`);
    - `Generate`'s second run of the result (the first block has always
      run it already).

- [x] **4.220 The Netmarble launch check:** move `Netmarble.cpp` into
  `gamemodel` behind `NetmarbleHost`, and retire the dead copy of its
  unfilled result.
  > **Status:** done (2026-10-14).
  > - **The check:** `ParsingNetmarble` answers false when the command
  >   line has no space, or what follows the first space is 4096 bytes
  >   or more. Otherwise the launch is refused through the host: the
  >   game shows the "launch from the Netmarble homepage" message and
  >   exits, as before. Without a host it only returns false.
  >   `g_CpCookie` (always empty, read by GameUI) moves with it, in a
  >   new `Netmarble.h`.
  > - **Its `NETMARBLE_INFO` parameter goes.** Since 4.207 it was never
  >   filled, and `ParsingNetmarble` cannot return true. So `bNetmarble`
  >   is false by the time `ClientMain` copied that struct's
  >   (uninitialised) fields into the user information. That copy is
  >   retired, its `else` branch kept as every launch's path, and the
  >   struct goes from `Client.cpp`.
  >
  > R1 is 43 Windows / 41 Ninja.
  - Owner: membership, R1, and `test_netmarble.cpp` (5 tests).
  - Mutants: all 8 are caught. The review added two: the first space,
    not the last, and a host without `RefuseLaunch`. Taking the source
    out of the membership breaks the test link.

- [x] **4.221 Retire the help decision tree:** delete
  `MCompareManager.cpp`/`.h` and the dead tree classes of
  `MHelpManager`.
  > **Status:** done (2026-10-14).
  > - **The tree:** the help system's decision tree (`MHelpManager`,
  >   `MHelpNode`, `MCompareHelpNode`, `MOutputHelpNode`) chose help by
  >   conditions. `GameInitInfo` created `g_pHelpManager` empty: its one
  >   `LoadFromFile` is commented out. Nothing ever ran it: its only
  >   `ExecuteEvent` call is commented out in `ExecuteHelpEvent`, and
  >   `ProcessEvent` appears only in a comment.
  > - **The conditions:** `MCompareManager` (`g_pCompareManager`, 20
  >   conditions over the UI, the player, the belt and the inventory)
  >   was consulted only by `MCompareHelpNode::GetNext`, during that
  >   walk.
  > - **What goes:** both classes' code and globals, and their
  >   creation and deletion.
  > - **What stays:** `MHelpManager.cpp` keeps `ExecuteHelpEvent`
  >   (live: in game mode it adds a help mail), and the header keeps it
  >   and the `__BEGIN_HELP_EVENT` macros.
  > - **Not retired here:** `MHelpDisplayer` (gamemodel since 4.110,
  >   `g_pHelpDisplayer`) was used only by that tree's output nodes. It
  >   is created and deleted, never asked; a later slice can retire it.
  >
  > R1 is 42 Windows / 40 Ninja.
  - Owner: R1. Retired code, so no test. The argument is above, and
    every tree that builds `DarkEden` links without the files.

- [x] **4.222 Help events' mail:** move `ExecuteHelpEvent` into
  `gamemodel` (`HelpEvent.cpp`/`.h`, the former `MHelpManager.cpp`)
  behind `MHelpHost`.
  > **Status:** done (2026-10-14).
  > - **The rule:** a help event adds its help mail in game mode only.
  >   The mailbox is the UI's, so it goes behind
  >   `MHelpHost::AddHelpMail`, which `ClientMain` installs (it calls
  >   `gC_vs_ui.AddHelpMail`, as before). Without a host there is no
  >   mail.
  > - **The header split:** `MHelpManager.h` stays executable-side.
  >   It includes `HelpEvent.h` and keeps the `__BEGIN_HELP_EVENT`
  >   macros and the `UserOption.h` include they (and, transitively,
  >   `MCreature.cpp`) need.
  > - **Left as is:** `MHelpDef.h`'s `HELP_COMPARE` enum is unused
  >   since 4.221, and a later slice can drop it.
  >
  > R1 is 41 Windows / 39 Ninja.
  - Owner: membership, R1, and `test_help_event.cpp` (3 tests).
  - Mutants: all 6 are caught; the two that drop a null guard crash.
    Taking the source out of the membership breaks the test link.

- [x] **4.223 The zone's tile provider:** `MZoneTileProvider` becomes
  `ZoneTileProvider<Zone>`, a header-only template in SpriteLib beside
  the `TileRenderer` it feeds. `MTopView` holds a
  `ZoneTileProvider<MZone>`.
  > **Status:** done (2026-10-14).
  > - **What it does:** it gives the renderer the zone's size and each
  >   sector's sprite. Sprite 0 or `SPRITEID_NULL`, a cell off the zone,
  >   or no zone at all is the null tile (-1), and no zone is 0 by 0.
  > - **The logic was already inline** in the old header.
  >   `MZoneTileProvider.cpp` held only an empty constructor and
  >   destructor, and goes. Over a template parameter, the logic no
  >   longer needs `MZone` to compile, so a test can run it over a fake
  >   zone.
  >
  > R1 is 40 Windows / 38 Ninja.
  - Owner: R1, and `test_zone_tile_provider.cpp` (4 tests).
  - Mutants: all 12 are caught, among them each bound, both null
    sprites and swapped axes. The fake zone answers its own sprite for
    a cell off the zone, so a bound that lets one through shows.
  - There is no source to take out of a membership: the template is
    compiled where it is used.

- [x] **4.224 Retire the event background images:** delete
  `MEventManager.cpp`. The images it loaded were never drawn.
  `g_pEventManager` becomes the gamemodel `MEventQueue` it already was
  underneath.
  > **Status:** done (2026-10-14).
  > - **The images:** `MEventManager` added images to `MEventQueue`:
  >   Adam's cloud behind the tiles, and an event's background
  >   (`parameter4`; `UIMessageManager` asks for Quest 2's). It decoded
  >   each JPEG into a `CDirectDrawSurface`. In this port that class is a
  >   stub:
  >   - `InitOffsurface` allocates nothing;
  >   - `Lock` sets the surface pointer to null, so the conversion loop
  >     never ran;
  >   - its `m_pDDSurface` is set only to null, in its constructor, so
  >     `GetSurface()` is always null and the JPEG was decoded again on
  >     every request.
  > - **The blit:** `MTopView` `reinterpret_cast` the surface to a
  >   `CSpriteSurface` and blitted it. Both classes hold a pointer at
  >   offset 0 (no virtuals; `CSDLGraphics` has no data). So `Blt` read
  >   a null backend surface and returned before drawing. Nothing was
  >   ever drawn, at the cost of a file read and a decode a frame.
  > - **Retired:** the decode, the image table, both blits, and the
  >   punned casts.
  > - **Unchanged:** the cloud branch's tile blit was the other
  >   branch's (`BltNoColorkey` is `Blt`), so the tiles draw the same.
  > - **Moved:** the event host (gamma, the player's effect status, the
  >   fade) goes to `GameInitInfo`, which creates the queue. The global
  >   is defined beside `MEventQueue`. `MEventManager.h` keeps the
  >   global and the background IDs, and becomes a gamemodel header.
  > - **Not done:** restoring the backgrounds means decoding into a
  >   `CSpriteSurface`. That is a feature, not this slice.
  > - **Left behind:** `MPlayer::IsShowAdamCloud` has no reader now.
  >   Its flag is still set by packets, for the cloud's return.
  >   `CJpeg.h`, whose last includer this was, goes with #515's
  >   follow-up.
  >
  > R1 is 39 Windows / 37 Ninja.
  - Owner: R1. Retired code, so no new test. The argument is above;
    `test_event_queue.cpp` now asserts that `g_pEventManager` is an
    `MEventQueue*`.

- [x] **4.225 Whom the player may select and attack:** move
  `MObjectSelector` into `gamemodel` behind `MObjectSelectorHost`.
  > **Status:** done (2026-10-14).
  > - **The rules:** `CanSelect`, `CanAttack` and `IsWarEnemy` decide
  >   by race or guild, with these exceptions:
  >   - forced attack, hallucination, self-defense (justice) targets and
  >     war or siege enemies;
  >   - for selection, also NPCs and operators;
  >   - under the non-PK option, a non-NPC wearing a player-only sprite
  >     is never attacked.
  > - **War and siege:** in a siege, a defender fights whoever is
  >   neither defender nor reinforcement, the same union is never an
  >   enemy, and an attacker of side N fights whoever is not of side N
  >   (any of the player's sides). In a guild war at the zone's castle,
  >   a joined guild fights every other guild; otherwise only the
  >   joined guilds are enemies.
  > - **The host:** it asks the game for the player's state, the
  >   creature's state (creatures are passed back opaque), the zone,
  >   and the justice and war managers. Library code may not read their
  >   globals (R4), and the selector uses the managers themselves.
  >   GameInit installs it when it creates the selector.
  > - **The rules are kept as they were:** one disabled 2004 non-PK
  >   check in `CanSelect` (its flag was always true) and the commented
  >   PK-zone variants go. The siege sides' `else if` chain becomes a
  >   loop with the same meaning: sharing one side does not end the
  >   check.
  >
  > R1 is 38 Windows / 36 Ninja.
  - Owner: membership, R1, and `test_object_selector.cpp` (15 tests,
    over fake creatures and real justice and war managers).
  - Mutants: 47 of 50 are caught. One of them, dropping the war-type
    check, is caught only by reading a race war as a guild war, past its
    object: ASan stops that reliably, a plain build usually crashes. No
    test can reach it without undefined behaviour. The 3 left are
    equivalent:
    - `IsExist` before `GetWarInfo`, whose null answer returns the
      same;
    - the null check after `IsExist`;
    - `break` against `continue` once the player's guild matched.
  - Taking the source out of the membership breaks the test link.

- [x] **4.226 The helicopters:** move `MHelicopterManager` (with
  `MHelicopter`) into `gamemodel` behind `MHelicopterHost`.
  > **Status:** done (2026-10-14).
  > - **Calling one** for a creature in the zone:
  >   - starts its looping propeller sound;
  >   - puts its shadow, a chase effect with the summon-helicopter
  >     sprite, 500 pixels right and 300 up of the creature, stepping
  >     20 pixels;
  >   - calling again replaces the sound;
  >   - a failed call releases it.
  > - **The manager:**
  >   - keeps one per creature;
  >   - moves each sound with its shadow;
  >   - opens the way-point selection when the player's arrives;
  >   - sends one away on request: it stops chasing and flies 1000 left
  >     and 600 down;
  >   - drops one whose shadow is gone, or one that arrived after being
  >     sent away.
  > - **The host:** the zone (a creature's sector, its effects), the
  >   view's frame count, the player's ID, the way-point UI, and the
  >   propeller sound. `ZONESOUND_NODE` stays opaque. GameInit installs
  >   it with the effect hosts and clears it after the zones are
  >   deleted. Without a host there is no zone: no creature to chase,
  >   and no shadow.
  > - **Unchanged:** the call maps the creature's sector to pixels with
  >   `WorldTileGeometry`, which `MTopView::MapToPixel` was.
  >
  > R1 is 37 Windows / 35 Ninja.
  - Owner: membership, R1, and `test_helicopter_manager.cpp` (12
    tests). They run real chase effects to their arrival, with the
    guidance host tracing the fake zone's creatures.
  - Mutants: all 36 are caught. #516's follow-up added pins for the
    shadow's first direction, the send-away's z and step, and the
    remove's effect-type check (a guidance effect stand-in, so that
    one no longer needs a crash). Taking the source out of the
    membership breaks the test link.

- [x] **4.227 The zone's sounds:** move `MZoneSoundManager` (with
  `ZONESOUND_NODE` and `MZoneSoundTable`) into `gamemodel` behind
  `ZoneSoundHost`.
  > **Status:** done (2026-10-14).
  > - **One sound** plays from its sector, heard from the player's:
  >   - it is panned 128 a sector past 3 either side;
  >   - it is 4 steps quieter a sector, times the volume option;
  >   - from 40 sectors away it is not heard.
  >   - Its buffer duplicates the cached one, which is loaded on first
  >     use; what the cache pushes out is released.
  >   - While the player stands still, a playing sound keeps its
  >     nearer source.
  > - **The manager**, every 5 seconds or when the player moves:
  >   - plays the sounds heard in the player's sector whose time has
  >     come, and schedules their next time;
  >   - stops one outside its hours (or before the game's hour is
  >     known);
  >   - keeps a looping one within its hours looping;
  >   - stops looping the ones no longer heard.
  > - **The host:** the audio device and its buffers, the sound cache,
  >   the options, the player's sector, the zone's sector sounds, and
  >   the frame clock and game hour. GameInit installs it with the
  >   effect hosts.
  > - **One reordering:** the check that there is a zone now comes
  >   first inside the update (the sector's sounds were always read only
  >   when an update is due). Without a zone nothing changes, as before,
  >   in every case. The rest is unchanged, the log lines included (#516's
  >   follow-up restored the ones the move had dropped).
  >
  > R1 is 36 Windows / 34 Ninja.
  - Owner: membership, R1, and `test_zone_sound_manager.cpp` (11 tests,
    over a fake device and cache, with real show times).
  - Mutants: all 56 are caught. Taking the source out of the
    membership breaks the test link.

- [x] **4.228 The FL2 text layer:** move `Client/RenderingFunctions.cpp`
  into VS_UI as `VS_UI/src/hangul/FL2Rendering.cpp`, tested by
  `ui_tests`.
  > **Status:** done (2026-10-14).
  > - **What it does:** the UI prints through it.
  >   - Plain strings, in their colour and font size, left-aligned,
  >     right-aligned or centred by their measured width.
  >   - Coloured strings return where they end.
  >   - A shadow falls 1 pixel down-right, by default a quarter of the
  >     colour.
  >   - An outline is printed in the four directions.
  >   - It also measures strings, takes the FL2 "DC" (the outer take
  >     only), formats numbers and money, and converts the hangul
  >     editor's DBCS strings.
  > - **Where it prints:** on the FL2 surface the UI sets. Before that,
  >   it used to fall back to the executable's `g_pLast`, then
  >   `g_pBack`. Library code may not read those globals (R4), so
  >   GameInit now installs the same choice through
  >   `g_SetFL2FallbackSurface` when it creates `g_pBack`.
  > - **Retired as unreachable:**
  >   - The `void*` overloads of `g_Print`, `g_PrintColorStr`,
  >     `g_PrintColorStrShadow` and `g_PrintColorStrOut`: no header or
  >     source declared them, so nothing else could call them.
  >   - `InitializeGL` and the `FillRect(S_SURFACEINFO*)` stub, which
  >     have no callers. The `rectangle` stub stays: it has callers.
  > - **Three defects found and kept here**, fixed by the next three
  >   commits (test path: moved, then fixed):
  >   - Right-aligned text was centred: `TA_CENTER` (6) holds
  >     `TA_RIGHT`'s bit. VS_UI_Base's value and money fonts are
  >     right-aligned.
  >   - The default shadow swapped red and blue. The original Win32
  >     `FL2.cpp` had the same swap, so this corrects an original bug.
  >     No caller uses the default shadow today (each passes a colour;
  >     the two that once did are commented out), so nothing visible
  >     changes yet.
  >   - The DBCS conversions read and wrote wide characters at odd
  >     addresses, which is undefined behaviour (UBSan stops there); they
  >     now copy the two bytes.
  > - **A known deviation, kept:** `g_GetNumberString` prints a number
  >   without grouping ("1234"). The original grouped thousands
  >   ("1,234"); the SDL port (a6e21d27, #45) dropped that, so the EXP
  >   tooltips show plain digits. The test pins today's behaviour, not
  >   the original's.
  >
  > R1 is 35 Windows / 33 Ninja.
  - Owner: R1, and `tests/ui/test_fl2_rendering.cpp` (11 tests since
    #517's follow-up, real TextService rendering on real surfaces).
  - Mutants: 38 of 40 are caught (and 9 more after #517's follow-up
    pinned what its review found). The 2 left are equivalent:
    - an empty string draws nothing either way;
    - a length of 0 or less never enters the loop.
  - Hiding the source from the VS_UI glob breaks the `ui_tests` link.

- [x] **4.229 How some creature types are drawn:** move
  `MTopviewExceptionProc.cpp` into `gamemodel` as
  `CreatureDrawExceptions.cpp`/`.h` (a git mv).
  > **Status:** done (2026-10-14).
  > - **What it holds:** for some creature types, a procedure that
  >   changes the action, frame and direction about to be drawn. Some
  >   face south; some stand still; Tepez and the horn stand until coma
  >   or death; the sweeper keeper walks in place and rings every 14th
  >   frame; Gildre floats draining and kneels dead; frozen Gildre turns
  >   every other frame; the guild war gate shows its damage; 764/765
  >   shake the screen on their steps and turn while drained.
  > - **The table:** `MapCreatureDrawExceptions` fills it; MTopView
  >   calls it where `InitMapingExceptionProc` was, and its two
  >   members go.
  > - **The creature:** reached through `CreatureState`, which gains
  >   four entries: `HasEffectStatus`, `GetActionCount`,
  >   `GetActionCountMax` and `SetShadowCount`.
  > - **The frame counter:** the procedures take it as an argument;
  >   `MTopViewDraw` passes `g_CurrentFrame`. The sound goes through
  >   `WorldEvents::PlaySound`.
  > - The bodies are otherwise unchanged; their Korean comments are
  >   translated.
  >
  > R1 is 34 Windows / 32 Ninja.
  - Owner: membership, R1, and `test_creature_draw_exceptions.cpp` (11
    tests since #518's follow-up: the table's types, each procedure,
    and the four new creature entries).
  - Mutants: all 54 are caught, among them each registered type range
    and each procedure's rule. Taking the source out of the membership
    breaks the test link. #518's review found more unpinned, which its
    follow-up pins (12 more mutants, all caught):
    - the fake creature keeps where it turns to (`SetDirection`) apart
      from where it faces (`GetDirection`), as `MCreature` does;
    - the drained turn from direction 6, and frame 5 held at 4;
    - the sweeper's sound at frame 14, Gildre's frame mask at 14, and
      a dead Gildre's action count past its maximum;
    - every waypoint, standing and obstacle type, alive and dead;
    - the four new creature entries with no host, an empty host and a
      real one.

- [x] **4.230 The guild, grade and level marks:** move
  `MGuildMarkManager.cpp` into VS_UI (`VS_UI/src/MGuildMarkManager.cpp`,
  a git mv). Its header stays in `Client/`.
  > **Status:** done (2026-10-14).
  > - **What it does:** the grade and level marks come from their packs,
  >   two to a mark (large, then small; no grade mark for a race past the
  >   races). A guild's two marks come from the guild mark pack at the
  >   sprite ID the guild info mapper gives it. They load once and are
  >   cached, a failure as well (an empty pair).
  > - **Why VS_UI:** it needs SpriteLib, which gamemodel does not link,
  >   and the guild info mapper (gamemodel's, so R4 allows it). VS_UI
  >   links both.
  > - **Retired as unreachable:**
  >   - creating a guild mark from an image (both `CreateGuildMark`s);
  >   - adding it (`AddGuildMark`) and saving it into the pack and the
  >     mapper's file (`SaveGuildMark`);
  >   - merging two packs (`MergeGuildMark`).
  >   GameInit's only calls of `CreateGuildMark` and `MergeGuildMark` are
  >   commented out; nothing else calls them or the other two. With them
  >   go the includes of the executable's `UtilityFunction.h` and
  >   `AppendPatchInfo.h`, and of `DebugInfo.h` (now `DebugLog.h`).
  >
  > R1 is 33 Windows / 31 Ninja.
  - Owner: R1, and `tests/ui/test_guild_mark_manager.cpp` (7 tests, on
    packs the tests write and a real `MGuildInfoMapper`).
  - Mutants: 21 of 23 were caught at first. #519's review found three
    more rules unpinned, which its follow-up pins:
    - a half-loaded pair is never published (a pack whose large, or
      small, sprite is malformed);
    - an empty pack is refused;
    - each mark is read at its own index entry (a pack with swapped
      entries).
    Of the 2 left, `offsets.size() >= 1` for `>= 2` is equivalent (the
    sprite ID bound rejects a pack of one). Letting the last sprite
    through reads one past the index; ASan would report that, a plain
    build cannot. `Is565()` is a compile-time true, so its other branch
    is unreached.
  - Hiding the source from the VS_UI glob breaks the `ui_tests` link.

- [x] **4.231 Retire UtilityFunction's dead image path:** the stub
  surface's image loader, JPEG decoding and encoding, the image saver,
  the SSN check and the free-space query.
  > **Status:** done (2026-10-14).
  > - **The loader was dead in fact.** `LoadImageToSurface(const char*,
  >   CDirectDrawSurface&)` decoded a .bmp or .jpg into a
  >   `CDirectDrawSurface`, which in this port is a stub (task 4.224).
  >   For a .bmp, its `InitFromBMP` returns false. For a .jpg, `Lock`
  >   gives a null surface pointer, and `bOK` was set only past that
  >   null check. So it always answered false, after reading and
  >   decoding the file.
  > - **Its one caller:** UIMessageManager's profile-picture selection
  >   (`Execute_UI_CLOSE_FILE_DIALOG`) went on only when it answered
  >   true, so choosing a profile picture has never done anything in
  >   this port. That branch goes, with a comment saying how to restore
  >   it: load into a `CSpriteSurface` sized to the picture first (the
  >   BMP loader cannot size one, see 4.232).
  > - **With it go:** `LoadJPG` (its only user), `SaveJPG` and
  >   `SaveSurfaceToImage` (no callers), and `IsValidSSN` and
  >   `GetDiskFreeSpace` (no callers). `git grep -a` over the tree
  >   finds none.
  > - **What stays:** `RemoveStringSpace`, `IsValidID`,
  >   `IsValidPassword` and the live `LoadImageToSurface(const char*,
  >   CSpriteSurface&)`, which the next task moves into libraries.
  > - **The JPEG library** went in #520's follow-up: CMake no longer
  >   looks for it or links it, the manifest no longer names it (SDL_image
  >   still brings it for the help artwork), and `Client/JpegLib`'s
  >   headers, which nothing included, are gone.
  - Owner: retired code, so no test. The argument above is the proof.

- [x] **4.232 The account input rules and the BMP loader:**
  `UtilityFunction.cpp` (now only the input rules) joins `gamemodel`, and
  its BMP loader moves into SpriteLib as `SurfaceImage.cpp`/`.h`.
  > **Status:** done (2026-10-14).
  > - **The input rules.** `RemoveStringSpace` trims spaces at both
  >   ends. `IsValidID` takes 4 to 10 bytes with no space and no
  >   leading digit, holding letters and digits (and the permitted
  >   characters), or hangul (2-byte pairs from a high byte) without
  >   letters. A pair that is a lone jamo of the source's jamo list, at
  >   an even byte offset, is refused. `IsValidPassword` refuses a
  >   backslash or a quote.
  > - **A quirk kept, not changed:** the jamo list is UTF-8 since the
  >   source became UTF-8 (and MSVC builds with `/utf-8`), while the check
  >   reads 2-byte (CP949) pairs. The list's bytes are E3 8x yy, so no
  >   real CP949 lone jamo (A4 A1 to A4 D3) is refused any more, and some
  >   valid two-byte syllables are (0x82 0xE3, 0x85 0x83). The tests pin
  >   what it does today.
  > - **Recorded, not changed:** the file dialog's preview never shows.
  >   Its `CSpriteSurface` is never sized, so the surface cannot be
  >   locked and the load always fails (#520's review).
  > - **The BMP loader.** `LoadImageToSurface(const char*,
  >   CSpriteSurface&)` loads a .bmp (a name of at least 8 characters,
  >   the extension in any case) into a sprite surface, converted to
  >   RGB565, as much as fits from the top left. Its one caller is
  >   VS_UI's file dialog (`VS_UI_ExtraDialog.cpp`), which now includes
  >   `SpriteLib/SurfaceImage.h`.
  >
  > R1 is 32 Windows / 30 Ninja.
  - Owner: membership, R1, `test_utility_function.cpp` (5 tests) and
    `test_surface_image.cpp` (7 tests since #520's follow-up, on BMPs
    the tests write through SDL).
  - Mutants: 28 of 30 were caught at first; #520's follow-up catches
    the trimming one (an all-space string after a character) and pins
    the height clamp and the 8-character boundary. The 1 left is
    visible only through undefined behaviour: a high byte last reads
    the uninitialised rest of IsValidID's copy.
    Taking `UtilityFunction.cpp` out of the membership breaks the test
    link.

- [x] **4.233 The map cell:** `MSector.cpp`/`.h` (with
  `EffectSpriteTypeHelper.h`) join `gamemodel`.
  > **Status:** done (2026-10-14).
  > - **What it holds.** A sector is one map cell: its tile, its block
  >   flags (the map's, and the server's per layer), its safety flags and
  >   light. It also holds what is on it:
  >   - one item;
  >   - creatures by layer, each in its own key range: 13 underground, 35
  >     on the ground, 30 in the air. A freed slot is reused, and creatures
  >     are read underground first;
  >   - up to 69 image objects;
  >   - effects, which raise the darkness and sanctuary flags by their
  >     sprite type;
  >   - portals and sounds, kept sorted, once each.
  > - **Creatures.** `MCreature` stays executable-side, so the sector
  >   reaches its creatures through two new CreatureState entries:
  >   `ObjectOf` (the creature as the `MObject` it is) and `CreatureOf`
  >   (back). Without a host a creature is held as the NULL object the
  >   host gives.
  > - **Corpses.** A sweeper's corpse (creature type 672) blocks the
  >   ground. A corpse's vtable is executable-side too, so its creature
  >   comes from a new `MItemHost::CorpseCreature` entry.
  > - **A preparatory commit** translates the comments and drops
  >   commented-out code. The preprocessed source is identical before and
  >   after it.
  > - **Recorded, not changed:**
  >   - Image objects are looked up from the first key, so once the first
  >     is removed none is found. After a middle removal the others are
  >     still found, but the next add lands on a taken key: it is dropped
  >     yet counted, and a walk for an ID not there then runs past the
  >     map's end. Nothing in the executable removes image objects (the
  >     header says they must not be).
  >   - The ousters safety flag and the server's underground block are
  >     both 0x10 of the second flag byte: a sector safe for ousters reads
  >     as blocked underground, a burrowed creature's block makes it safe
  >     for ousters, and setting the safety replaces the server's blocks
  >     (#521's review). MZone::UnSetServerBlock clears all three server
  >     blocks, so it also takes the sector's ousters safety away.
  >   - Removing one of two darkness (or sanctuary) effects clears the
  >     flag.
  >   - Removing a sweeper's corpse lifts the ground block even where the
  >     map blocks the ground.
  >   - Loading a sector replaces all its flags, including those of what
  >     it holds.
  >
  > R1 is 31 Windows / 29 Ninja.
  - Owner: membership, R1, and `test_sector.cpp` (33 tests since #521's
    follow-up, on real effects and image objects, with plain objects as
    creatures, handed out as tokens of their own so that a sector reaching
    them past the host fails).
  - Mutants: 88 of 91 are caught. The 3 left are equivalent: each
    layer's "after the last" slot, which is never reached. A taken next
    slot always has a free one below it while the counts hold. Taking
    `MSector.cpp` out of the membership breaks the test link. #521's
    follow-up adds 18: one per host conversion cast past the host (14
    caught, the 3 "after the last" inserts equivalent) and the safety
    assignment (caught).

- [x] **4.234 Which skills the player can use now:** `MSkillAvailable.cpp`
  (MSkillSet's SetAvailableSkills, SetAvailableVampireSkills and CheckMP)
  joins `gamemodel`.
  > **Status:** done (2026-10-14).
  > - **What it decides:**
  >   - The set is rebuilt from the learned skills whose cost fits. A
  >     slayer or ouster pays in MP, plus twice its HP under Sacrifice; a
  >     vampire pays in HP at its consume cost.
  >   - A slayer's blade, sword and gun skills need that weapon in hand
  >     (active skills do not).
  >   - The inventory and the mouse item add skills: a portal, holy water,
  >     bombs, mines and their materials.
  >   - A vampire's form and items gate its skills, and invisibility is
  >     refused in four lairs.
  >   - An ouster's weapon and element levels gate its skills.
  >   - Then come the bonus skills, the couple ring, and the light sight.
  >   - Zone 3001 has no skills. An installed turret and a casket have
  >     their own.
  > - **How it reaches the executable:**
  >   - The player through `PlayerState`: `Status()` for HP, MP, INT,
  >     level and elements, its race and effects, and two new entries,
  >     `IsInvisible` and `CalculateLightSight`. Its form and casket go
  >     through `CreatureState`.
  >   - The zone through `ZoneState::GetID`.
  >   - The mouse item through `DialogPresentation::GetMouseItem`, and
  >     the casket's skill selection through `VsUiPresentation`.
  >   - The vampire portal finder (`MItemFinder.h`) reads a portal's mark
  >     through `MItem::IsPortalMarked`, because the portal item's vtable
  >     is executable-side.
  > - **Moved and retired:**
  >   - `IsBombMaterial`, a pure check of the item's class and type, moves
  >     from MItemUse.cpp into this file, its only user.
  >   - `IsMineMaterial` had no callers and goes.
  >   - Most commented-out code goes, and the Korean comments are
  >     translated (with MItemFinder.h's).
  >
  > R1 is 30 Windows / 28 Ninja.
  - Owner: membership, R1, and `test_skill_available.cpp` (30 tests since
    #522's follow-up, on a real skill tree, info table, inventory and
    gears, with fake items).
  - Mutants: 80 of the 81 run are caught. The one left is equivalent:
    dropping the stop after the first slayer portal only inserts the same
    enabled entry again, which the set ignores. #522's review named more
    of that kind (the holy water's stop, the bomb's insert guard) and some
    it did not run, which its follow-up pins: element levels above and
    below, a slayer without gear or inventory, Will of Life's level, and
    the casket's creature. Taking `MSkillAvailable.cpp` out of the
    membership breaks the test link.

- [x] **4.235 What a change of the player's status does:**
  `ModifyStatusManager.cpp`/`.h` join `gamemodel`, with the alignment
  steps (`Alignment.cpp`/`.h`) and a new `StatusPresentation` host.
  > **Status:** done (2026-10-14).
  > - **What it does.** MPlayer::SetStatus stores a value, then runs this
  >   manager's function for it:
  >   - HP is capped below the silver damage.
  >   - The bars and the character window show the values, and the
  >     character sheet keeps its fields.
  >   - A gear slot's durability lands on the right hand for a two-handed
  >     weapon.
  >   - The gold, the magazine, a weapon's silver and the player's sight
  >     are kept, and the zone drops what the player no longer sees.
  >   - A stat change rechecks the items and recomputes the status.
  >   - A domain or level change is told, with a new skill to learn and
  >     the help at 5, 30, 31 and 32.
  >   - The alignment's value and step are kept and told.
  >   - The guild, rank, elements, skill levels, pet and union values are
  >     kept.
  > - **How it reaches the executable:**
  >   - The UI goes through `StatusPresentation`: one entry per UI setter,
  >     and `SetSheet(field, value)` for `g_char_slot_ingame`.
  >   - The player through PlayerState, with a new `SetSight`, and
  >     CreatureState.
  >   - The zone through ZoneState, with a new `KeepObjectInSight`.
  >   - A pet's name through a new `MItemHost::PetName`.
  >   - The regeneration through RankBonusHandlers' `CheckRegen`, the help
  >     through GameInfoPresentation, and the skill check directly
  >     (`g_pSkillAvailable` is gamemodel's since 4.234).
  > - **ConvertAlignment** moves out of PacketFunction.cpp, which includes
  >   its header.
  > - **Recorded, not changed:** the old and new alignments (a negative one
  >   sent as 16 bits) are compared unsigned, so when exactly one of them is
  >   negative the move is told the wrong way: from 2000 to -3000 as one
  >   towards good, and from -9000 to 9000 as one towards evil (with the
  >   fame help). The step itself is right.
  >
  > R1 is 29 Windows / 27 Ninja.
  - Owner: membership, R1, and `test_modify_status.cpp` (32 tests, every
    function, with fakes recording the UI, the player and the zone, on
    real gears, inventory, skill tree and string table). The English
    table words most "_1"/"_2" message pairs alike (the Korean differed
    by the number's last digit), so the tests make them distinct to pin
    the choice.
  - Mutants: 84 of the 85 that build are caught. The one left is
    equivalent: the UI value's own 16-bit conversion, a no-op on a
    `short`. #523's follow-up gives the pet a real experience row, which
    catches 2 more (the row read). Taking `ModifyStatusManager.cpp` out of
    the membership breaks the test link.

- [x] **4.236 ClientFunction split:** the game logic joins `gamemodel`, the
  UI's translucent box goes into SpriteLib, and the blood bible's gear
  effect into VS_UI's UiRuntime host.
  > **Status:** done (2026-10-14).
  > - **gamemodel** (`ClientFunction.cpp`/`.h`):
  >   - `GetWhisperID`.
  >   - `IsPlayerInSafePosition` (0 not, 1 safe for the player's race, 2
  >     safe for everyone), through a new `PlayerState::IsInSafeSector`.
  >   - `AddNewInventoryEffect`: an item action's effect starts at the
  >     item's grid cell, with what the action does to the item as its
  >     result. That is the remove, the gender turn or the option change,
  >     or for a pet only freeing the item put on it. The player's check
  >     buffer goes through PlayerState.
  > - **SpriteLib** (`AlphaBox.cpp`/`.h`): `DrawAlphaBox(surface, screen,
  >   box, colour, alpha)`.
  >   - The box is clipped one short of the screen's right and bottom.
  >   - A black box darkens through GammaBox565. A coloured one blends by
  >     hand, or on Windows through BltColorAlpha.
  >   - The inner black branches, unreachable since the early return,
  >     go. VS_UI_Base.cpp keeps the UI's `DrawAlphaBox(RECT*, r, g, b,
  >     alpha)` over the back surface and g_GameRect.
  > - **UiRuntime**: `DrawBloodBibleEffect_InGear(x, y)`. The executable's
  >   host draws it through the view.
  > - **Retired:**
  >   - `IsPlayerInSafeZone` (no callers).
  >   - `DrawTitleEffect` (its callers commented out).
  >   - `DrawInventoryEffect`, which only read the inventory's position;
  >     the inventory window's call goes too. #524's follow-up retires
  >     the view's `DrawInventoryEffect` and `DrawTitleEffect`, which
  >     nothing called after that.
  >   - Commented-out minimap code.
  > - **Recorded, not changed:**
  >   - On Windows a coloured box draws nothing, since BltColorAlpha is a
  >     stub. The test pins it.
  >   - The inventory's item effects (making holy water, an enchant) are
  >     generated and advanced, but nothing draws them: the port left the
  >     draw a TODO ("not implemented for SDL backend"). The retired view
  >     member is in git history.
  >
  > R1 is 28 Windows / 26 Ninja.
  - Owner: membership, R1, `test_client_function.cpp` (8 tests, with a
    fake generator that declines so the result runs),
    `test_alpha_box.cpp` (4 tests on real surfaces) and
    `test_ui_runtime.cpp`'s forwarder test.
  - Mutants: 22 of 26 are caught. The 4 left are equivalent:
    - an action at the table's end reads a default row, which targets
      nothing;
    - both drawing paths clamp at 0 and skip an empty box themselves, so
      the left, top and empty checks repeat them.
    Taking `ClientFunction.cpp` out of the membership breaks the test
    link.

- [x] **4.237 Using items:** `MItemUse.cpp`, the item factory table and
  every item class's use body, joins `gamemodel`. So do the classes'
  vtables: magazines, mines, corpses and portal items can be built in a
  test now.
  > **Status:** done (2026-10-14).
  > - The bodies send through `g_pSocket`, whose definition moves from
  >   GameMain into gamemodel's `GameSocket.cpp`. `g_bHolyLand` joins
  >   `ZoneFlags`.
  > - What they reach goes through hosts:
  >   - PlayerState: the check buffer, the wait, effects, the safe sector.
  >   - CreatureState: the player's creature type, pet, direction and
  >     race, and two new entries, `GetMoveDevice` and `IsStop`.
  >   - ZoneState: the zone's ID, and two new entries, `HasHelicopter` and
  >     `IsExistCorpseFromPlayer` (UIMessageManager's, which looks around
  >     the player for a corpse of a creature type). A corpse frees its
  >     creature through `DeleteCreature`: `MCreature` stays the
  >     executable's.
  >   - DialogPresentation and NoticePresentation for the refusals.
  >   - VsUiPresentation, five new entries: `LockGear`, `RunMixingForge`
  >     (the forge's class as `FORGE_CLASS`, GameInit's adapter asserting
  >     the order), `RunBulletinBoardWindow`, `RunXmasCardWindow` and
  >     `RunQuestInventory`.
  >   - WorldEvents for the empty gun's sound.
  > - `GetMakeItemFitPosition`, where an item made from another goes,
  >   moves from GameMain into `ClientFunction.cpp`.
  > - **Retired:**
  >   - The event tree's look-around for a tree or a board already
  >     standing. It asked whether the tree itself, not the item on the
  >     ground, was a corpse, which it never is, so it never refused.
  >   - The dye potion's commented-out refusal of the master effect
  >     colours to advanced players.
  >   - The never-defined `__TEST_SUB_INVENTORY__` arms that armed the
  >     check buffer with a sub-inventory ID.
  > - **Recorded, not changed:**
  >   - The event tree never refuses a second tree or board nearby (the
  >     retired look-around above).
  >   - `CGSkillToInventory`'s constructor leaves the target cell unset.
  >     The vampire portal, `MVampireETC` and `MSkull` set neither
  >     coordinate and the mine only X, so they send whatever the stack
  >     held.
  >   - `MVampireETC`'s skull branch cannot run: no item class makes an
  >     `MVampireETC` a skull (`MSkull` has its own body).
  >   - The uses read `g_pZoneTable->Get(zone)` unchecked: an unknown zone
  >     crashes.
  >   - Pet food reads the player's pet before checking for a player; the
  >     executable's adapter dereferences the missing player, as before.
  >   - A quick slot key arms the check buffer and locks the gear twice; a
  >     mine arms the buffer as a use, then as a skill.
  > - **A follow-up:** the item host entries that exist only because these
  >   vtables were the executable's (`LoadGun`, `ReloadGun`,
  >   `SetMineInstalled`, `IsPortalMarked`, `MarkPortal`, `CorpseCreature`,
  >   `PetName`, `UsePotionFromInventory`, `UsePetFromInventory`) can give
  >   way to direct calls.
  >
  > R1 is 27 Windows / 25 Ninja.
  - Owner: membership, R1, `test_item_use.cpp`: 40 tests. A ClientPlayer
    that keeps what is sent stands in as `g_pSocket`; the hosts record;
    the items are real, in a real inventory, gear and arms band; a fake
    effect generator records the motion's action and length.
  - Mutants: 127 of the 128 that build are caught. The one left is
    equivalent: with a sword in hand, a magazine's inventory use gets
    past the gun check to the fit check, which refuses it the same way;
    only the log line differs.
    Taking `MItemUse.cpp` out of the membership breaks the test link.

- [x] **4.238 The talk box and the message box:** `UIDialog.cpp` joins
  `gamemodel`. Its windows go behind a new host, and the code only the
  never-defined `OUTPUT_DEBUG` compiled retires.
  > **Status:** done (2026-10-15).
  > - `DialogWindow` (`DialogWindowHost.h`) holds the two kinds of VS_UI
  >   dialog UIDialog opens, as opaque windows:
  >   - `NewTalk` (an NPC talk window) and `NewMessage` (a message box);
  >   - `SetMenu`, `SetMessage`, `Start`, `Finish`, `Run` and `Delete`.
  >
  >   GameInit's adapters build `C_VS_UI_NPC_DIALOG` and `C_VS_UI_DIALOG`.
  >   They hand each choice back to `UIDialog::ProcessPCTalkDlg` or
  >   `ProcessMessageDlg`, and assert that the exit ID is VS_UI's.
  > - DialogPresentation gains the windows an answer waits for
  >   (`IsRunningStorage`, `IsRunningShop`, `IsRunningBookcase`,
  >   `IsRunningRepair`, `IsRunningSell`), `IsRunningParty` and
  >   `CloseAllDialog`. The exchange and the swap come from the existing
  >   entries, and the party window from `CommunicationPresentation::RunParty`.
  > - The player's wait goes through PlayerState. `g_bActiveGame`'s
  >   definition moves from Client.cpp into ClientMode.cpp.
  > - **Retired:**
  >   - The test client's addon, action and direction dialogs, about 590
  >     lines. Only `OUTPUT_DEBUG` compiled them, and nothing defines it.
  >   - The commented-out help dialog and fixed-message popup.
  > - **Recorded, not changed:** leaving a plain talk (escape) still sends
  >   an answer, and the exit ID cut to the answer's byte goes as answer 0.
  >
  > R1 is 26 Windows / 24 Ninja.
  - Owner: membership, R1, `test_ui_dialog.cpp`: 18 tests (22 with the
    follow-ups'). The windows,
    the other windows, the party window and the player are fakes that
    record; a ClientPlayer keeps what is sent; the talk box and the temp
    information are real.
  - Mutants: 68 of the 70 that build are caught. The two left are
    equivalent: the talk box's `else` that forgets the party window runs
    only when it is already forgotten, and copying one character more of
    a long message's first line is undone by the terminator written at
    the break.
    Taking `UIDialog.cpp` out of the membership breaks the test link.

- [x] **4.239 An advanced creature's actions:** the conversions from a
  plain action to an advanced (second class) one, by race, and the
  advanced actions' frame counts join `gamemodel` (`AdvancementAction.cpp`).
  MCreature reads them, so it needs them in a library before it can move
  (see *Remaining R1 work*).
  > **Status:** done (2026-10-15).
  > - The three tables and the slayer's stand, move and damaged rules move
  >   from MTopViewDraw.cpp; the frame count arrays from Client.cpp, which
  >   still fills them from the view's frame packs at start-up.
  > - The slayer's conversion takes the right hand's item class
  >   (`ConvAdvancementSlayerAction`). MTopViewDraw.cpp keeps
  >   `ConvAdvancementSlayerActionFromSlayerAction(action, MCreatureWear*)`
  >   for the view's callers, reading the hand from the creature.
  > - **Recorded, not changed:**
  >   - An action at or past `ACTION_MAX_SLAYER` (35) and below 100 reads
  >     past the slayer's and the vampire's tables. No creature has one.
  >   - The ousters' rows 7 and 8 (standing and walking) give back the
  >     plain actions 0 and 1, and the view's advanced ousters drawing
  >     (MTopViewDraw.cpp's body and DrawCreatureShadow.cpp's shadow)
  >     subtracts `ADVANCEMENT_ACTION_START` from them, a negative frame
  >     pack index (#527's review); only MCreature's frame count checks
  >     for one. The table's comments say rows 7 and 8 still lack the rule
  >     upstream marked them for; 12 and 13 were marked too, but already
  >     answer advanced actions.
  >
  > R1 is unchanged: no executable source leaves.
  - Owner: membership, `test_advancement_action.cpp` (6 tests, the three
    tables pinned row by row).
  - Mutants: 14 of 14 caught. Taking `AdvancementAction.cpp` out of the
    membership breaks the test link.

- [x] **4.240 The frame count and the zone's creature colours:**
  `g_CurrentFrame` and `g_DrawAlphaNum` (how far the drawing is between
  two frames) join FrameClock, and `g_ZoneCreatureColorSet` joins
  ZoneFlags, all in `gamemodel`. They were Client.cpp's, CGameUpdate.cpp's
  and GameMain.cpp's; MCreature reads all three. Their writers are
  unchanged.
  > **Status:** done (2026-10-15). R1 is unchanged.

- [x] **4.241 Repairing display text:** `TextEncoding::NormalizeUtf8`
  (basic) does what TextService's uncached normalisation did: valid UTF-8
  as it is, each malformed byte replaced by U+FFFD. `NormalizeText` keeps
  its cache over it. MCreature's chat wrapping can call it without
  TextSystem, which gamemodel does not link.
  > **Status:** done (2026-10-15). R1 is unchanged.
  - Owner: `test_text_encoding.cpp` (one new test); TextService's 13
    normalisation tests run through it.

- [x] **4.242 The creature:** `MCreature.cpp` joins `gamemodel`. The
  player, the creature that wears gear, the NPC and the fake creature all
  derive from it, so the zone and they can follow.
  > **Status:** done (2026-10-15).
  > - What it asked of the executable goes through the hosts it already
  >   used, with new entries:
  >   - ZoneState: the zone's size, a sector, the ground effects and removing
  >     a fake creature. For the zone the creature is in (its own `MZone`,
  >     which need not be the player's): the server's blocks it leaves and
  >     takes, and its move by move type.
  >   - CreatureState: turning a turret's barrel with it, and the item class
  >     in a creature's right hand.
  >   - WorldEvents: whether the view exists, an effect sprite's frame count
  >     and an ousters' elemental.
  >   - StatusPresentation: the weapon speed.
  >   - A new `GameOptions` host (`GameOptionsHost.h`): the user options'
  >     blood drops, white chat and the player's chat colour.
  > - The creature pool (`g_CreatureMemoryPool`) is defined in MCreature.cpp;
  >   MemoryPool.cpp keeps the pools of the executable's creature classes.
  > - A cell's pixel is WorldTileGeometry's, as it was through MTopView.
  > - **Retired:** the commented-out light-sight blocks, two
  >   commented-out blocks (a can't-move log under the never-defined
  >   `OUTPUT_DEBUG`, a switch on the occupied cell) that alone would have
  >   needed host entries, and `g_bCheckError`'s definition under
  >   `OUTPUT_DEBUG`. Its readers, also under it, stay, so defining the
  >   macro no longer compiles.
  > - **Recorded, not changed:**
  >   - `UpdateAttachEffect` recolours a sylph only when its effect sprite is
  >     both `SYLPH_GREEN_FLY` and `SYLPH_BLACK_STOP`, which never holds.
  >   - A crashed bike ends its spin the third time it faces its start, after
  >     two crash skills.
  > - **Fixed after the move:**
  >   - The constructor read `m_bPlayerParty` (through `SetStatus`) before
  >     setting it; UBSan stopped on the load.
  >   - The bleeding interval's `(percentHP<<2) * rand()` overflowed `int`
  >     where `RAND_MAX` is 2^31-1 (macOS, Linux); UBSan stopped on it. It
  >     is taken in 64 bits, so its result is Windows' as before: the
  >     remainder of the product by 20, not a random multiple of the HP as
  >     the formula may have meant.
  >   - A glacier's skill lasts twenty frames less than the status, and
  >     `max(0, delayFrame - 20)` was meant to stop at 0. The frames are
  >     unsigned, so under twenty it wrapped to about four billion frames.
  >     It stops at 0.
  >   - In a coffin, the regeneration bonus heals silver damage with
  >     `max(0, damage - bonus)`, and the damage is unsigned: less damage
  >     than the bonus wrapped to about four billion. It stops at 0
  >     (found by Linux CI's warning budget, a `-Wsign-compare` in
  >     Platform.h's `max`).
  >
  > R1 is 25 Windows / 23 Ninja.
  - Owner: membership, R1, `test_creature.cpp`: 61 tests. The hosts
    are fakes that record; the tables, the sector, the effects and the
    event queue are real, with a recording effect generator.
  - Mutants: 140 of the 141 caught. The one left is equivalent: a blitz slide turns to its target twice, and the second turn is the same as the first.
    Taking `MCreature.cpp` out of the membership breaks the test link.

- [x] **4.243 The creature that wears gear:** `MCreatureWear.cpp` joins
  `gamemodel`, with `AddonDef.h` (an enum of the gear's frame IDs).
  > **Status:** done (2026-10-15).
  > - Its two skills (a sylph's end, a vampire coming to) go through
  >   `WorldEvents::ExecuteActionInfoFromMainNode`, and the fast-move
  >   sprites' frame counts through `WorldEvents::GetMaxEffectFrame`, the
  >   entries MCreature uses; no new host entry.
  > - Its pool (`g_CreatureWearMemoryPool`) is defined in MCreatureWear.cpp;
  >   MemoryPool.cpp keeps the NPC's and the fake creature's.
  > - The last `_MinTraceA` call ("remove effectsprite", on every removed
  >   status sprite) goes to the debug log, which shows it only with
  >   debug tracing on (`DARKEDEN_TRACE`). Before, on Windows it looked for
  >   the optional external trace window each time, and elsewhere it
  >   printed to stderr every time.
  >
  > R1 is 24 Windows / 22 Ninja.
  - Owner: membership, R1, `test_creature_wear.cpp`: 21 tests (23 with
    the follow-up's). What a wearer wears by race and gender, the colour
    sets it takes, hair, the gear it puts on and takes off (and what each
    takes off with it, the weapons' statuses and a vampire's coat among
    them), its pool, and the skills and frame counts it asks for, over
    recording hosts; the item, option and creature tables are real.
  - Mutants: 48 of the 50 caught. The two left are equivalent: the item rebuilt to take a slot off is read only for its class and slot, and the slot past the array is the shoulder, which nothing wears, so removing it answers false either way.
    Taking `MCreatureWear.cpp` out of the membership breaks the test link.

- [x] **4.244 The NPC:** `MNPC.cpp` joins `gamemodel`.
  > **Status:** done (2026-10-15).
  > - Its two reads of the player (the shop's gender query; Bathory and
  >   Tepez turning to face the player) go through `PlayerState::Creature`.
  >   Without a player the gender query answers male, as before, and the
  >   turn is skipped, where the original dereferenced the player.
  > - Its pool (`g_NPCCreatureMemoryPool`) is defined in MNPC.cpp;
  >   MemoryPool.cpp keeps only the fake creature's.
  >
  > R1 is 23 Windows / 21 Ninja.
  - Owner: membership, R1, `test_npc.cpp`: 8 tests (9 with the
    follow-up's). Its info, its pool, a new shop freeing the old one's
    stock, the shelves it builds (a female player's paired types, a
    portal item's zone), Bathory's and Tepez's turns (one stand end in
    five over about 10000, within four standard deviations; never while
    attacking, dead or without a player) and the effects NPCs 636 to 639
    restart whenever their action count is back at 0. That is each
    stand's start, and every frame of a walk begun from a stand, whose
    count stays 0; the test pins the stand. The shop builder's own rules are
    `test_npc_shop.cpp`'s.
  - Mutants: 19 of 19 caught.
    Taking `MNPC.cpp` out of the membership breaks the test link.

- [x] **4.245 The fake creature:** `MFakeCreature.cpp` joins `gamemodel`,
  and `Client/MemoryPool.cpp` is retired.
  > **Status:** done (2026-10-15).
  > - Its reads of the player's zone (the fake IDs taken, the creatures it
  >   traces, the zone's size, the path search's visited flags, the free
  >   cells round a pet's owner) go through `ZoneState`. Those of its own
  >   zone (free cells, size, the master a pet follows) go through the new
  >   `ZoneState::CanMoveIn`, `GetWidthIn`, `GetHeightIn` and
  >   `GetCreatureIn`, which take the zone. The player goes through
  >   `PlayerState`; the view's first sector and the skills and sounds it
  >   starts go through `WorldEvents`; MTopView's pixel statics become
  >   `WorldTileGeometry`'s and its direction static `SelectFacingDirection`.
  > - The view's sector sizes (`g_SECTOR_*`) move from Client.cpp to
  >   `ViewSectors.cpp`, and `GetNextTileByDirection` from PacketFunction.cpp
  >   to `WorldTileGeometry`, over `DirectionOffset`.
  > - Its pool is defined in MFakeCreature.cpp. That was the last of the
  >   four, so `Client/MemoryPool.cpp` and its header are retired: R1 falls
  >   by two.
  > - Recorded, not changed: nothing makes a turning fake
  >   (`FAKE_CREATURE_TURNING_MOVE`, upstream's test kind), and one could
  >   never end. Its end asks for the last frame of its stand, which
  >   `Action` resets within the same frame.
  >
  > R1 is 21 Windows / 19 Ninja.
  - Owner: membership, R1, `test_fake_creature.cpp`: 22 tests (26 with
    the review's follow-up, whose zone fake answers the player's zone and
    the fake's own zone differently and records the move type asked). Its
    IDs (taken ones skipped, 9999 wrapping to 0), another player's pet
    item freed with it, its kinds' drawing flags and ends (a rocket's
    time and target, a wild wolf's last frames, a dragon tornado leaving
    the player's view), the turning slide, its next destination walked
    back from a blocked cell, a pet's spot by its owner, the turret
    turning to its owner's target, the path search (round a wall, within
    the view, an unreachable cell remembered, asleep unless the player is
    a ghost), the ghost wandering in view, the rocket circling its
    target, the wild wolf's three leaps, its sidesteps, its end frame and
    its feeding at a corpse, a pet chasing its master, walking near it by
    its level, cutting a drink short and following it in its own zone,
    a tank pet's smoke, a fast move's end, and a pet's step and its
    sound, over a zone of free and blocked cells and recording hosts.
    `test_world_tile_geometry.cpp` pins `GetNextTileByDirection`.
  - Mutants: 50 of 50 caught: 49 in MFakeCreature.cpp and GetNextTileByDirection's one. Two of them abort (they free a pet item the player's inventory or the test still holds), and one, the path search no longer skipping visited cells, never ends and was stopped at a time limit.
    The review's follow-up: 30 of 31 more caught (each own-zone read
    swapped for the player's zone and back, each move type, the wolf's
    later leaps and its end frame, a walking pet's level, the drain
    catch-up, a tracing pet, a fast move's end). The one left drops a
    tracing pet's flag just before `SetStop`, which clears it anyway. The
    path search's own bound on its zone's size is redundant with the
    zone's `CanMove`, which bounds itself.
    Taking `MFakeCreature.cpp` out of the membership breaks the test link.

- [x] **4.246 The zone:** `MZone.cpp` joins `gamemodel`.
  > **Status:** done (2026-10-15).
  > - Its reads of the player go through `PlayerState`; sending the
  >   player's position and status to its party on entering a zone are
  >   new entries (`SendPositionInfoToParty`, `SendStatusInfoToParty`).
  >   What it asks of the view goes through new `WorldEvents` entries: a
  >   sound stopped, the random monster type a hallucination shows, a
  >   redraw of the tiles, and the sprites the tile and image-object packs
  >   hold. The personal shops' refresh time is a `GameOptions` entry.
  > - An effect's sprite type comes from `EffectSpriteTypeHelper.h`, the
  >   pure twin of `MTopView::GetEffectSpriteType` that `MSector` already
  >   used.
  > - `g_pZone` is defined with the zone, so gamemodel owns it now;
  >   `g_ZoneAmbientSounds` moves from GameMain.cpp to
  >   `AmbientSoundState.cpp`. `GetNearSpriteSet` hands back `std::set`s,
  >   and GameMain fills the view's sprite set managers from them (both
  >   keep their IDs sorted and unique, so the sets are the same).
  > - Recorded, not changed: removing or picking up an item, and removing
  >   a tile's effect by status, reach `g_pZone` for tile effects and
  >   sector marks rather than the zone itself; a lengthened effect ends a
  >   frame before the newcomer would have (`SetCount` counts the frame it
  >   is set in); the lasting sanctuary (`_2`) is numbered after the
  >   vanishing one (`_3`), so the sanctuary checks leave it out; a
  >   refused ground effect is neither kept nor freed; removing a trap or
  >   a vampire portal by status answers false after deleting it; a tile's
  >   darkness is matched by comparing frame IDs with sprite type numbers;
  >   `LoadFromFileSectorSound` trusts the file's counts.
  > - Fixed after the move (the first two found by reading, the third by
  >   a test of the move crashing): removing a tile's effect
  >   read the sector of the packet's tile unchecked, and
  >   `GCDeleteEffectFromTile` passes the server's tile straight on. A
  >   tile off the map now finds nothing (and sets no sector mark by
  >   status) instead of reading past the sector rows. The marks
  >   `GCAddEffectToTile` sets (a ground elemental's aura, darkness
  >   forbidden, the gnome's fury) went the same way through the
  >   `ZoneState` adapters; their forwarders now pass on only a cell
  >   inside the zone's size. And releasing a zone's objects took every
  >   effect out of its sector unchecked, though the zone keeps chasing
  >   effects off the map (a helicopter's shadow near an edge): those are
  >   now freed without a sector.
  > - The review of #533 found two more, fixed in the next branch: a
  >   creature's own and server cells (`GCFastMove` stores the server's
  >   tile unchecked) were looked up unchecked when it died
  >   (`AddCorpseFromCreature`, which also leaked a corpse the zone
  >   refused) and when it moved (`Move{Ground,Flying,Underground}Creature`,
  >   its old cell and its server cell). Both go through `SectorAt` now, and a refused corpse
  >   is freed with its creature.
  >
  > R1 is 20 Windows / 18 Ninja.
  - Owner: membership, R1, `test_zone.cpp`: 45 tests (47 with the fixes
    in #533, 51 with the review's follow-up and its fix), over a real
    zone that stands behind `ZoneState` as `g_pZone`, recording player,
    view and option hosts. The grid and its blocks; creatures on their
    layers (refusals, a sector's capacity, removal and its server block,
    the player, names, moves, party members, hallucination); corpses
    (the server cell, the item they replace, a dying corpse answering for
    its creature, the relic tables and their blocks, the corpse of my
    kill); items (falling and landing, the special items' tile effects
    shown and taken away, the player's check buffer); effects (waits,
    refusals freed, the same frame lengthening, darkness merging and
    where it may not fall, sanctuary, updates, ground effects, the
    sweeping vice); a tile's effect removed by type and by status; fakes,
    helicopters, safe cells and portals, the PK type; what leaves the
    player's sight; the creatures' turn (the dragon-eyed, the player,
    personal shops, the invisible); sounds; the sprites near the player;
    image objects; `Update`; and `ReleaseObject`.
  - Mutants: 82 written; 74 caught at first (three of them by crashing), the six that showed gaps since caught by the tests above. The two left are equivalent: an item with no drop frame does not fall anyway (`SetDropping` checks it again), and a status past the table reads a row whose sprite type is `EFFECTSPRITETYPE_NULL`.
    Taking `MZone.cpp` out of the membership breaks the test link.

- [x] **4.247 The player:** `MPlayer.cpp` joins `gamemodel`.
  > **Status:** done (2026-10-15).
  > - What it asks of the UI goes through a new host, `PlayerPresentation`
  >   (`PlayerPresentationHost.h`, 32 entries): the mouse's picked-up item
  >   and attack cursor, whether the inventory and gear windows are open,
  >   the windows it runs or closes (the trace and elevator windows, the
  >   death and resurrection requests, the shrine's minimap), the mine and
  >   bomb progress bars, the chat input line, force feedback (Windows
  >   only; the guard moves into the adapter), the vampire countdown, the
  >   silver damage, and the `UI_` calls it made (pet info, a descriptor,
  >   the item and trade locks, the damage line, the party and exchange
  >   cancels, the way points, the effect statuses, the level-up button).
  >   The hosts it
  >   already had carry the rest: `VsUiPresentation` (the skill selected,
  >   the gear lock), `DialogPresentation` (the unlocks),
  >   `StatusPresentation` (the HP bar, to-hit, protection, defense),
  >   `ZoneLoading` (the interface's race), `GameInfoPresentation` (another
  >   player's info, help events) and `NoticePresentation` (the horn).
  > - The view's selection, its fades, its request mode and the screen's
  >   pixel under the cursor are new `WorldEvents` entries; tile and pixel
  >   conversions use `WorldTileGeometry`, which `MTopView`'s statics
  >   forward to, and the facing direction `SelectFacingDirection`. (An
  >   entry for a creature action's frame count went with the commented-out
  >   code that alone named it, after the review of #535.)
  > - `SetAction`'s gear lock and unlocks, which called `gC_vs_ui`
  >   directly, now go through the `UI_LockGear` and `UI_UnlockGear` the
  >   hosts already used; those add a debug log line each, the only
  >   difference.
  > - The input globals it reads (`g_x`, `g_y`, the mouse buttons, the
  >   selected sector, the network status) move to `InputState.cpp`, and
  >   `PETINFO` to `PetWindowInfo.h`.
  > - The help-event brackets, which expand to nothing without
  >   `OUTPUT_DEBUG`, go, and so does a commented-out block that named the
  >   executable's debug log. Two skill ranges cast their sum to `int`
  >   before taking the smaller of it and 6 (GCC's `-Wsign-compare`).
  > - Recorded, not changed: dying spares the blood drain and the siege
  >   marks the status removal, but the attached effects' clearing after
  >   it takes their flags all the same; three rows of the vampire
  >   countdown's blink table cannot be reached (their thresholds are 1 h,
  >   30 min and 10 s, where their comments say 10 min, 30 s and 5 s); an
  >   item that vanishes while the player walks to it leaves the player
  >   standing with its item trace set; following a creature answers
  >   false; `CanActionByZoneInfo` reads the zone table's row without a
  >   check; the inventory-skill branch of `ActionToSendPacket` builds a
  >   `CGSkillToInventory` it never sends, yet locks the item and waits,
  >   and only the player's own trace loop starts `TraceInventoryItem`,
  >   its one way in; the observing eye's light is counted at the next
  >   reckoning, not as it opens; the party's position report goes out on
  >   every call outside the peer-to-peer path, which alone remembers the
  >   last one sent.
  > - Fixed after the move, both found by the tests under the sanitizers:
  >   the constructor left `m_bTraceCreatureToForceAttack` unset, and a
  >   skill on itself held down from the start (`SelfSpecialAction`, then
  >   `SetRepeatAction`) made `OnAttacking` read it, so the view chose its
  >   attack cursor by whatever the heap held; it starts cleared, and so
  >   do the two members set beside it (`m_bNextForceAttack`,
  >   `m_TraceIDBuffer`), which nothing read before their time. And
  >   `SetDestination` read the target cell's safe zones for a carried
  >   relic, blood bible or flag before its own check that the cell lies
  >   on the map, which a sector trace hands it unchecked: one row past
  >   the map, ASan stopped on the row array. Those checks read the cell
  >   on the map only; off it the move is refused as before.
  >
  > R1 is 19 Windows / 17 Ninja.
  - Owner: membership, R1, `test_player.cpp`: 62 tests (64 with the
    fixes, 73 with the review's follow-up), over a real zone behind `ZoneState` as `g_pZone`, with the
    player as `g_pPlayer`, the packets it sends caught on a `ClientPlayer`
    socket, and recording UI, view and help hosts. What it is (identity,
    attack modes, a stop, the path cost, darkness, safe cells, a drink cut
    short, the death delay); where it goes (the next destination, the path
    round a wall, a place it found no way to, a sanctuary, a relic into
    its safe zone); walking (one `CGMove` a step, the unanswered moves'
    cap, the server's OK and NO, its cell keeping the zone in sight, zone
    74's clouds); death and life; its statuses (the UI, the drain's
    countdown to conversion, a turret, a coma, a ghost, a sylph);
    attacking (a blow struck once, a friend followed, a forced blow, the
    self-defence roster, refusals, the safe-zone rule, an NPC greeted);
    picking up (to the inventory, walking there, what does not fit or is
    not its race's, money, to the hand, a motorcycle); skills on itself, a
    creature and a cell; requests to trade, to party and for information;
    moves by force; finding an enemy; changing race; action results; its
    light; its party reports; held-down blows and ground skills; the item
    check buffer's locks; a skill's range.
  - Mutants: 85 written; 80 caught at first, and the four gaps they showed are now pinned (a monster outside the safe zone, the first result's info chosen as a skill, a safe zone that keeps the button held, a weapon's reach). One is equivalent: the knockback's own reset of the sent moves, which the creature's knockback already makes through `SetServerPosition`.
    Taking `MPlayer.cpp` out of the membership breaks the test link.

- [x] **4.248 A view library, and the divine-guidance silhouettes:** the
  creature drawing of `DrawCreatureDivineGuidance.cpp` joins a new library,
  `gameview`, as `CreatureDivineGuidanceDraw.cpp`.
  > **Status:** done (2026-10-15).
  > - `gameview` is the view's drawing that reads the model and draws with
  >   SpriteLib, which `gamemodel` may not include. It has gamemodel's
  >   discipline: `tests/arch/gameview_files.txt` lists its members and the
  >   headers it may include; CMake builds it from the `.cpp` lines and
  >   keeps them out of the executable; the include checker walks its
  >   closure (V0 a listed file exists, once, under `Client/`, and not
  >   for gamemodel too; V1 an include stays in what M1 allows, SpriteLib
  >   or gameview's own list; V2 no `MinTr.h` or `DebugInfo.h`); and the
  >   ratchets count its members as library code (R4, R9, R10) and its
  >   list as a reason to reconfigure before measuring R1. It links
  >   gamemodel, SpriteLib and framelib; the executable and `unit_tests`
  >   link it.
  > - The six silhouettes were `MTopView` members over its packs and
  >   surface; they are free functions over a `CreatureDrawPacks`
  >   (`CreatureDrawPacks.h`), which `MTopView::GetCreatureDrawPacks`
  >   fills from its members at each draw. The choice by race and class,
  >   made in `MTopView::DrawCreature` until then, is
  >   `DrawDivineGuidanceCharacter`.
  > - The five helpers the creature drawing took from `MTopViewDraw.cpp`
  >   (the vampire's new actions, the advanced slayer's part for an item
  >   class and its gun attack, the new slayer action, the advanced slayer
  >   action by the right hand) move unchanged into `AdvancementAction.cpp`,
  >   gamemodel's, with their tests.
  > - Recorded, not changed: SpriteLib's SDL backend draws
  >   `BltIndexSpriteDarkness` as a plain `BltIndexSprite` (a stub that
  >   ignores the darkness), so the silhouettes are drawn at full
  >   brightness, and the darkness each pass asks for is not pinned; an
  >   ousters' arms take the right hand's second colour set whether it is
  >   a wristlet or a chakram, by two branches that come to the same; the
  >   advanced vampire is drawn from frame set 0 whatever body it is
  >   given.
  >
  > R1 is 18 Windows / 16 Ninja.
  - Owner: membership, R1, `test_creature_divine_guidance.cpp`: 17
    tests. Each draws through the moved code onto a surface in memory and
    lays the blits it should make, with the colour sets named, onto
    another (sprites whose four indexed pixels show the four colour sets
    and whose fixed pixel marks them apart); the pictures must match. The
    slayer's worn parts, darkened and turning with the frame clock, and
    only from a frame 30; what it does not wear; a unique item's colour in
    both sets but a coat's; a vampire's body bobbing in its colours, and
    its special colours looked up; an ousters' hair, coat, arms and boots,
    bare arms and feet in the default coat colour, its chakram, and a
    ghost's parts left out; the advanced vampire and ousters from their
    packs; the choice of silhouette by race and class.
  - Mutants: 31 written; 29 caught (four by crashing), after a test that
    the colour sets the tests name are lit and can be told apart, a
    vampire's bob past frame 10, and sprites at a gradation that is not
    black. The two left change only a pass's darkness, which the SDL
    backend ignores (they are equivalent there). Taking `gameview` out of
    `unit_tests`' link breaks it.
  - Fixed after the move: the advanced slayer's silhouette walked the
    parts' order to `ADDON_MAX` (7) over a row of `AC_ADDON_MAX` (6),
    reading one past the row, and past the table when the slayer faces
    direction 7; it walks the row, as the advanced slayer's other drawings
    do. Two tests reach it (the creature's facing is its current
    direction, `SetCurrentDirection`): facing direction 2, a sword over a
    shield from either gender's pack; facing direction 7, a shield over a
    coat, at frame 6 of the clock. The old bound stops under UBSan. In a
    plain build it showed too, as the review of #536 found: every row of
    the order starts with the motorcycle, so facing 0 to 6 the walk past
    the row drew a mounted slayer's motorcycle a second time, last, over
    the rider. A third test, a coat over a motorcycle facing 2, fails
    there on the old bound (added after the review). 12 mutants on the
    advanced slayer (the old bound, the order's row and direction, the
    lookup by AC slot, the part, gender, frame, height, clock and colour
    sets), all caught. The review also split the ousters' ghost check, so
    each flag is seen to hide its own part (the coat 1, the chakram 2),
    the advanced ousters' too: 6 more mutants, all caught.

- [x] **4.249 The creature shadows:** `DrawCreatureShadow.cpp` joins
  gameview as `CreatureShadowDraw.cpp`.
  > **Status:** done (2026-10-15).
  > - Its seven `MTopView` members (the shadow of a creature, and the
  >   six by race and class) are free functions over the
  >   `CreatureDrawPacks`, which gains the shadow packs (the frames and
  >   shadow sprites of the creatures, the slayers' parts, the ousters'
  >   and the advanced classes') and the other sprites (a slayer's
  >   cross, the caskets). The player's shadow option
  >   (`UserOption::BlendingShadow`, VS_UI's) comes in as a parameter.
  >   `MTopView::DrawCreatureShadow` stays, a one-line forwarder for its
  >   seven callers in `MTopView.cpp`, and passes the option.
  > - `SpriteIDDef.h` (the sprite IDs, no includes) joins gameview's
  >   header list. The six `OUTPUT_DEBUG_DRAW_PROCESS` traces go: the
  >   macro is defined nowhere, and they needed `DebugInfo.h`, which
  >   gameview may not include.
  > - Fixed before the move, in SpriteLib: shadow sprites drew nothing
  >   on the SDL backend. The adapter's raster was meant black, as the
  >   DirectDraw blit drew it, but 5:6:5 black is the colour key the
  >   backend makes transparent, on the CPU path and the GPU path; and
  >   `BltShadowSpriteDarkness`, which the original drew by shifting each
  >   channel under the shadow right, was a stub. The raster is RGBA now,
  >   opaque black where the sprite casts shadow, and a darkened shadow is
  >   that black at alpha 256 - 256 >> DarkBits. Shadows appear under
  >   creatures and objects again; nobody has watched it on a display.
  >   With `BlendingShadow` off, its default, a creature's shadow is
  >   solid black, as the original drew it; objects' and effects' are
  >   half dark. `BltShadowSpriteSmall` and the two 4444 forms, which have
  >   no callers, forward to the plain blit and so draw a full-size black
  >   shadow now (they drew nothing): a caller would need them written.
  > - Recorded, not changed: the advanced slayer's "always the body"
  >   test reads the loop index (`i == AC_ADDON_BODY`, which is the
  >   motorcycle's slot in every row of the order), not the part, so it
  >   adds nothing while unworn slots hold `ITEM_CLASS_NULL`; the
  >   advanced vampire's weapon frames are behind a `bWeapon` that is
  >   always false; the claymore (740) falls through into the guild
  >   tower's case (734), which comes to the same; an installing turret's
  >   shadow is solid whatever the option; the shadow code keeps its own
  >   copy of the special creature types, apart from
  >   `CreatureDrawExceptions`.
  >
  > R1 is 17 Windows / 15 Ninja.
  - Owner: membership, R1, `test_creature_shadow.cpp`: 35 tests.
    Shadows are black on a white surface in memory, blended ones grey,
    and each test shadow sprite has a shape of its own (a row of nine,
    pixel 0 and its id's bits), so a picture shows which sprite went
    where and how; each test lays the blits it expects onto a second
    surface and the two must match. Covered: a body's frame at its
    offset, and none past its frames; the event that hides shadows, a
    whole ghost, the ghost status; a blinded player (yellow poison,
    flare, blindness) seeing only the tiles around it, and itself; an
    ousters player in a ground elemental hidden but from ousters; a pet
    whose owner is in darkness; the slayer's parts, solid or blended at
    the player's choice, an installing turret, no addon shadows; the
    ousters' coat and chakram by ghost flag, the wristlet and its stand
    and walk; the advanced slayer, vampire (bats and wolves plain) and
    ousters from their packs, and an advanced slayer's plain attack,
    which has no advanced action; a dead player vampire's dust, the dust
    without gore and a slayer's cross, the types always or never dust;
    the three caskets and a closing one; underground, cut height; a
    headless corpse's frame; a dying creature's first frame without
    gore; darkness (a vampire player, Lightness but in zone 3001, an
    NPC, the player itself); hallucination; the relics' list and every
    special type of the switch; the pets' turrets, 702's offset and the
    recoil.
  - Mutants: 135 written; 132 caught (three by crashing). Two of the first pass mutated a commented-out copy of their line and were aimed again at the live one. One is equivalent: a blinded player is always on its own tile, so leaving it out of the distance check changes nothing. A second pass, after tests of a dead waypoint against every frame it might draw, a slayer's part past its frames and an advanced vampire with no advanced action, caught the rest.
    Taking `gameview` out of `unit_tests`' link breaks it.

- [x] **4.250 The creatures drawn with an effect:** `DrawCreatureEffect.cpp`
  joins gameview as `CreatureEffectDraw.cpp`.
  > **Status:** done (2026-10-15).
  > - Its ten `MTopView` members are free functions over the
  >   `CreatureDrawPacks`: a slayer's parts fading out to both sides,
  >   moving fast, invisible, its weapon's afterimages (each with the
  >   advanced slayer's form), a slayer's pet's turret and an installing
  >   slayer's turret. The packs gain the selection: the selected
  >   creature's ID and the outline manager its sprites go to instead of
  >   the surface. The thirteen call sites in `MTopViewDraw.cpp` pass
  >   `GetCreatureDrawPacks()`.
  > - Dropped with the move: the `OUTPUT_DEBUG` declarations of the wipe
  >   globals (the macro is defined nowhere, and their definitions were
  >   under it too), three unused colour declarations, and the helpers'
  >   local declarations, which `AdvancementAction.h` has.
  > - Recorded, not changed: SpriteLib's SDL backend draws a darkened,
  >   brightened or colour-set index sprite as a plain one (stubs), so the
  >   fades' and afterimages' darkness, a bright pet's brightness and the
  >   effect colours show as positions and colours only; the invisible
  >   wipe-in draws over a darkened (so, plain) copy of itself, so it adds
  >   nothing to the picture; the advanced slayer's parts are drawn inside
  >   `try { } catch (...) { }`; the "new items" (coats from item type 28,
  >   swords, blades, guns from 16, shields and helms from 15, crosses and
  >   maces from 14) are drawn from their frame IDs on the new slayer's
  >   actions, a coat from two frame sets.
  >
  > R1 is 16 Windows / 14 Ninja.
  - Owner: membership, R1, `test_creature_effect.cpp`: 23 tests (with the fix's). Each
    draws through the moved code onto a surface in memory and lays the
    blits it expects, in their colour sets, onto another (index sprites
    whose four indexed pixels show the colour sets and whose fixed pixel
    marks them apart). Covered, for each of the four effects and its
    advanced form: where each part is laid (to both sides, along the
    move, in place), in its colours (a unique or quest item in both
    sets, a coat keeping its first), the screen rect over the parts,
    nothing past the action's frames or for what is not worn; an
    advanced slayer from its gender's pack, the parts of its items, the
    new items' frames and actions, a new item outside its pack, no
    advanced action; a pet moving fast; the invisible wipe-in's effect
    and count; up to five afterimages of a swung weapon, under Rediance
    and its effect colour, fading; the pets' turrets (direction, 702's
    offset, the recoil, the body's colours) and an installing turret
    (its facing, count and frame, the screen rect on the first frame
    index), each to the outlines when selected.
  - Mutants: 296 written, one per live copy of each line chosen (the four effects and their advanced forms repeat their code); 277 caught (some by crashing under ASan), 184 in a first pass and 93 in a second, after tests of the parts' order, the advanced slayer's special colours, screen rect, frames, new items class by class and invisibility, and a bright advanced pet. Of the 19 left, 16 are in the advanced slayer's afterimages, which no test reached: they were drawn for the shoulder piece, not the weapon (fixed below). Three are equivalent: the afterimage loop's coat test, which only a weapon reaches.
    Taking `CreatureEffectDraw.cpp` out of gameview's list leaves
    `unit_tests` with undefined symbols.
  - Fixed after the move: the advanced slayer's weapon afterimages were
    drawn for the shoulder piece, not the weapon. The parts are walked in
    the AC order, whose right hand is `AC_ADDON_RIGHTHAND` (3), but the
    test compared the slot with the plain slayer's `ADDON_RIGHTHAND` (5),
    which in the AC order is `AC_ADDON_SHOULDER`. It compares with
    `AC_ADDON_RIGHTHAND` now, in both branches (a weapon's part and a new
    weapon). A test of a sword and a shoulder piece, frames 3 and 7, a
    unique sword, failed first. The 16 mutants in the afterimages that no
    test reached before are caught now, so 293 of the 296 are; the three
    left are the equivalent coat tests.
  - The review of #538 found the advanced weapon fade's other branches
    unreached: Rediance's afterimages, an effect-coloured weapon and a
    fading one. A test draws them now, a sword's part and a new sword
    (SDL draws the colour-set and darkened blits plainly, so the test
    sees that each is drawn, not how); deleting any of the six blits
    fails it. Two mutants stay alive and equivalent: flipping the
    Rediance test (both blits draw the same picture on SDL), and giving
    the new items' branch the shoulder's afterimages too (shoulder armour
    is never a new item). The surface's wipe-out selection
    (`CSpriteSurface::SetEffect`) is not seen by SDL, whose blit reads
    only the index sprite's effect. The test of the fix keeps the
    shoulder piece on the surface, so a slot test that let both through
    fails it, and the invisibility tests put the effects' global state
    back.

- [x] **4.251 The crash report:** `CrashReport.cpp` and `GetWinVer.cpp`
  join gamemodel.
  > **Status:** done (2026-10-15).
  > - Windows installs `RecordExceptionInfo` as the unhandled-exception
  >   filter (`InitCrashReport` in `ClientMain`); it gathers the crash and
  >   logs it to `CrashReport2.log`. What it writes is split out, so it is
  >   the same code on every platform: `GetExceptionDescription` (the
  >   exception's name, Windows-only and static until then),
  >   `FillCrashReport` (the record from the gathered facts: the
  >   executable's time, the client's version, the OS, the faulting
  >   address, the exception's name and code, the call stack's
  >   addresses) and `ReadClientVersion` (the first four bytes of the file
  >   `FILE_INFO_VERSION` names). The handler gathers the same facts as
  >   before, the same way, and hands them over; off Windows it is the
  >   stub it was.
  > - `GetWinVer.cpp` (the Windows version; "Non-Windows Platform"
  >   elsewhere), which only the handler calls, comes too: it was a
  >   Windows-only source of the executable, so this slice takes two from
  >   Windows' count and one from Ninja's. `CrashReport.h` declares it and
  >   includes `Platform.h` itself; the handler reaches the socket through
  >   `GameSocket.h` and `ClientPlayer.h`, not `ServerInfo.h`.
  >
  > R1 is 14 Windows / 13 Ninja.
  - Owner: membership, R1, `test_crash_report.cpp`: 6 tests on every
    platform but one: every exception code's name and unknown ones; a
    report holding what was gathered (the version a WORD, addresses as 16
    hex digits, the call stack's), and an empty one; a report through its
    log file and back; the version from the first four bytes of its file,
    a short file, none; the OS's name and the refusals; off Windows, the
    handler's stub. On Windows instead of the stub, the handler on a
    divide by zero at the test thread's own context: the log's message,
    address, version, OS, executable time and a stack walk that starts at
    the address (run by the Windows CI).
  - Fixed after the move, found by reading (the reviews of #539): the
    stack walk passed `StackWalk64` no context record, which on x64 it
    requires, seeded no stack pointer and loaded no symbols for its
    function-table and module-base callbacks, so on x64 the log's call
    stack was empty at best. It walks a copy of the crash's context now,
    with the stack pointer, after `SymInitialize` (loaded from dbghelp
    like the rest; if something loaded the symbols first, as a
    sanitizer's runtime does, it walks anyway and leaves them to their
    owner). Not reproduced here, with no
    Windows machine: the Windows test, which asks for a stack that starts
    at the faulting address and goes on, runs in the Windows CI, before
    the merge.
  - Mutants: 44 on the code every platform compiles (each table entry,
    the table walk, each field and format, the version read, the OS
    name's refusals and result), all caught. The Windows gathering is
    covered by the Windows test only. Taking `CrashReport.cpp` out of
    gamemodel's list leaves `unit_tests` with undefined symbols.

- [x] **4.252 A client library, and the start-up's loading:** `GameInitInfo.cpp`
  joins a new library, `gameclient`.
  > **Status:** done (2026-10-15).
  > - `gameclient` is the client that puts the game together: what starts
  >   it, runs it and wires the model, the view and the UI, which it may
  >   use. It has gameview's discipline: `tests/arch/gameclient_files.txt`
  >   lists its members and the headers it may include; CMake builds it
  >   from the `.cpp` lines and keeps them out of the executable; the
  >   include checker walks its closure (C0 a listed file exists, once,
  >   under `Client/`, and not for gamemodel or gameview too; C1 an include
  >   stays in what V1 allows, gameclient's own list, or a header of VS_UI,
  >   dxlib or TextSystem, which it takes at the library's boundary
  >   without walking into it; C2 no `MinTr.h` or `DebugInfo.h`); and the
  >   ratchets count its members as library code (R4, R9, R10) and its
  >   list as a reason to reconfigure before measuring R1. It links
  >   gameview, VS_UI, gamemodel, dxlib, SpriteLib and framelib; the
  >   executable and `unit_tests` link it.
  > - `GameInitInfo.cpp` loads the client's configuration
  >   (`InitClientConfig`, which writes it back), the player's options
  >   (`InitUserOption`) and every information table (`InitInfomation`).
  >   What it asked of the executable goes through `GameInitInfoHost`,
  >   which `ClientMain` installs before `FileOpenBinary`'s first caller
  >   (`CheckTerriblePatch`): the event queue's fade, the packets' item
  >   table, the personal shop's update time, and the message for a file
  >   that will not open. `FileOpenBinary` and the definition of
  >   `g_pSoundManager` come with it from `GameMain.cpp`;
  >   `GameInitInfo.h` declares them.
  > - Linking it into a test links the whole UI, since the accelerator
  >   table binds the UI's hot keys, and the UI reached five symbols only
  >   the executable defined. `g_MyFull`, `g_GameRect`, `g_Dimension` and
  >   `g_bEnable3DHAL` are defined in gamemodel (`ClientMode.cpp`;
  >   `Client_PCH.h` declares them). `PlaySound(TYPE_SOUNDID)`, which the
  >   UI's buttons call, is gamemodel's forwarder to
  >   `WorldEvents::PlaySound`; its body is `GameMain.cpp`'s
  >   `PlaySoundAtPlayer`, the host's entry. `InitGameObject` installs
  >   that host before the UI is made, and nothing plays a sound between
  >   `InitInfomation`, which makes the sound table, and it.
  > - The test found that `CTypeMap`'s loader (the NPC table, the shop
  >   templates, the zone sounds) looped over an uninitialised count on an
  >   empty file and kept loading from an ended stream when the count was
  >   past the file's end; fixed with its own test
  >   (`test_type_map.cpp`). Thirteen other loaders read a count the same
  >   way, and the next commits fix each, test-first: the creature sprite
  >   mapper, the guild mapper, the monster-kill quests, an NPC's and a
  >   server NPC's shop templates, a skill's next skills and an Ousters
  >   skill's skill types, the server skill information (which also
  >   indexed the table by an unchecked skill type), a zone's sounds, the
  >   zone table and the sound pack's index (`test_loader_counts.cpp`);
  >   the mailbox, which also indexed its three tabs by the file's count
  >   (`test_mailbox_file.cpp`, which the UI linking into `unit_tests`
  >   made possible); and the key bindings, where the start-up test spent
  >   a minute on an empty options file (`test_user_option.cpp`). The one
  >   left, `Client/Updater/UpdateManager.cpp`, is not compiled.
  >
  > R1 is 13 Windows / 12 Ninja.
  - Owner: membership, R1, `test_game_init_info.cpp`: 13 tests over a
    scratch directory `FileDef` points at, every table written with a
    count or a record of its own: everything made and each table loaded
    from its file (each experience file into its own table, both skill
    files into the one skill); each of the 35 files the loading opens
    missing in turn, which stops it with that file's message; the item
    options and guild names optional; a missing regen-tower file stops
    it; the configuration loaded and, without a file, its defaults
    written; the options loaded, the sprites' smoothing set from them,
    wave music forced and the shop time the host's; the host's defaults;
    each host entry passed on; the event queue's fade through the host
    and an effect's event kept without a player; a reload (the debug
    build's Alt+F1) starting from new objects; the English client's
    overlays on the NPCs, their scripts and the skills' display names,
    and another language's keeping the data's names; an empty effect
    sprite type table stopping the loading.
    `test_world_event_handlers.cpp` adds the client's `PlaySound`
    reaching the host's entry.
  - Mutants: 178 over `GameInitInfo.cpp` (every load, every file's open
    check and return, the regen towers', the English overlays, the host's
    forwarders and defaults, the Inits, each `SAFE_DELETE`), run against
    the slice's tests alone; 140 caught. Of the 38 left, 34 drop a
    `SAFE_DELETE` before a reload, which only the debug build's Alt+F1
    runs: each table's own loader replaces what it held, and the objects
    whose state a reload must clear (the event queue, the self-defense
    roster, the key bindings, the profiles, the timed items) and the two
    optional files are pinned. The other four are two log lines, the key
    table's `Init` (binding the defaults grows the table) and the
    creatures' move table (a static every earlier test has filled).
  - The linker's map of `unit_tests` shows `InitInfomation`,
    `InitClientConfig` and `InitUserOption` from
    `libgameclient.a(GameInitInfo.cpp.o)`, `PlaySound` and the four
    globals from gamemodel.

- [x] **4.253 The packet handlers' helpers:** `PacketFunction.cpp` joins
  gameclient.
  > **Status:** done (2026-10-15).
  > - `PacketFunction.cpp` is what the packet handlers, and the hosts that
  >   carry them, do to the model: the packet item tables that stand for
  >   a creature's look, a creature's addons, the player's info,
  >   inventory, gear, effects and pets from their packets, items into the
  >   zone, item and skill-card use, portals, the skills' and the
  >   monsters' shows, and a few helpers (the fades, the crash report, the
  >   flag).
  > - What it reached in the executable goes through the hosts the
  >   handlers already use: the view through `WorldEvents` (two new
  >   entries, `CreatureSpriteHeight` and `SetFlagSprites`), the UI through
  >   `DialogPresentation`, `PlayerPresentation` (a new `SetHotKey`) and
  >   `NoticePresentation`; `MTopView`'s static helpers give way to
  >   `WorldTileGeometry` and `SelectFacingDirection`. The large and small
  >   zones and their image flags are `ZoneFlags.cpp`'s, the inventory's
  >   effect manager `MScreenEffectManager.cpp`'s, `g_bSetHotKey` the
  >   file's own, and the window, full-screen flag and MAC address, which
  >   the Windows-only `CheckMacScreenMode` reads, gamemodel's
  >   `ClientMode.cpp`'s. R4 reads a global's definition with an array
  >   bound now (the packet item tables).
  > - Four functions nothing called are retired (`ConvertPositionMapToScreen`,
  >   `SetServerName`, `GetMacAddressFromSock`, `Send_nProtect_Auth`); the
  >   header declares the fifteen that only their callers' externs did;
  >   the Korean comments are English (four Korean strings in live
  >   `DEBUG_ADD` calls are left).
  > - The tests found seven defects, each fixed after its test failed on
  >   the moved code: the wire's hair style enum had no fixed type, so a
  >   style past the three was undefined (`HairStyle : BYTE`);
  >   `SetAddonToSlayer` read the hair tables at the wire's style and the
  >   NPC overload the item tables at the creature file's bytes;
  >   `ReleasePacketItemTable` left seven tables holding freed items;
  >   `SetBloodBibleSlot` wrote the twelve-entry bonus array at the
  >   server's sign type and leaked a sign the gear refused; Blaze Walk's
  >   server-sent grade ran past its three directions; the sex-change dye
  >   read a slayer's hair from the addon the new body had cleared, turned
  >   the slayer's flag back and left a vampire's body as it was; and a
  >   used item's unsigned count went from none to 4294967295 (the mixed
  >   `max` that did it would have been gameclient's one new
  >   `-Wsign-compare`: under the executable's precompiled header Clang
  >   never reported it, so no budget counted it). gameclient links
  >   `netapi32` on Windows for the MAC address lookup's `Netbios`.
  > - The tests pin, for task 4.254, the logic defects and leaks they
  >   found: the vampire's guild name given to the guild mapper before
  >   it is updated; a gear item the gear refused still worn; one bad
  >   pocket item ending the inventory's, gear's or held item's loading
  >   (and leaking it); a refused pocket item and other creatures' pet
  >   items leaked; `AddItemToZone` returning nothing after it re-added an
  >   item, so a mine is never marked installed; `ComeFromPortal`'s
  >   creature without a sprite showing at once; `AddItemToZone`'s skipped
  >   sub-items leaked. `SetPetInfo` and five skill shows delete a fake
  >   creature the zone refused and then use it; `NewFakeCreature` takes an
  >   ID the zone does not hold, so the zone cannot refuse it, and they are
  >   recorded, not changed.
  > - Recorded, not changed: `CHECK_VERSION_ERROR` shows the game string
  >   table's entry at a UI string index
  >   (`UI_STRING_MESSAGE_CHECK_VERSION_ERROR`); `DeleteFile` is a stub off
  >   Windows, so the crash log is never deleted there and its report is
  >   sent at every login; `CheckMacScreenMode` asks `MessageBox` when the
  >   local address is one of a hard-coded list (a developer's hack); a
  >   sex change takes the clothes off with the body and nothing puts them
  >   back; `SetSubInventoryInfo` is compiled out
  >   (`__TEST_SUB_INVENTORY__` is never defined); the packets' `toString`
  >   (`PCSlayerInfo`, `PCSlayerInfo2`, `CLCreatePC`) reads
  >   `HairStyle2String` at the style unchecked, past it for a style past
  >   the three, and nothing calls it on a received packet.
  >
  > R1 is 12 Windows / 11 Ninja.
  - Owner: membership, R1, and 161 tests in seven files:
    `test_packet_helpers.cpp` (10: the server's names, the fades, the
    durations, the crash report, the flag, the face styles),
    `test_packet_item_table.cpp` (24: the packet item tables, their
    release, and the three overloads that dress a slayer, a vampire and an
    ouster), `test_hair_style.cpp` (1), `test_packet_player_info.cpp`
    (16: the three races' info, statuses, sheet, domains, hot keys and
    guild) and `test_packet_player_items.cpp` (25: inventory, gear, the
    blood bible, the held item, effects and pets),
    `test_packet_zone_items.cpp` (45: items into the zone, item and
    skill-card use for every status and dye, portals, the GDR and race
    monsters, rocket, wolf, tornado and elemental) and
    `test_packet_skill_shows.cpp` (40: the action start, sector skills,
    Cross Counter, Shadow Dancing, Illendue and Blaze Walk). Each builds a
    real zone, player and tables; recording hosts stand in for the UI, the
    view, the item host and the effect generators.
  - Mutants: 1561 over the 51 functions the tests call (deleted
    statements, negated conditions, flipped comparisons and returns),
    run against a test binary of the seven files: 1274 caught (2 by
    hanging), 25 did not build, 262 survived. 82 are in the Windows-only
    MAC address and screen-mode code, compiled out where the run was
    (macOS); 44 change only what the UI is told (`gC_vs_ui`, the IMM's
    force feedback, `stderr`, the pet window's record); 35 change
    nothing a fresh object does not already hold (a new item's options
    cleared, the none slots set to NULL, a fake creature's zone and
    server position); 5 are dead here (`GuildName.Release` after the
    info was read, `DeleteFile`'s stub). The other 96 are test gaps (bug
    reports not recorded, the ousters' arms band in the gear, sub-item
    types left at 0, boundaries such as a sign type of exactly 12); task
    4.254 runs them again over its tests.
  - Link: the linker's map of `unit_tests` shows `SetPCSlayerInfo`,
    `SetInventoryInfo`, `AddItemToZone`, `UseItemOK`, `SkillBlazeWalk`,
    `SetBloodBibleSlot`, `InitPacketItemTable`, `SetAddonToSlayer`,
    `g_pPacketItemHelm` and `g_bSetHotKey` from
    `libgameclient.a(PacketFunction.cpp.o)`; the executable compiles no
    object of it.

- [x] **4.254 The packet helpers' defects:** what 4.253's tests pinned
  is fixed, each test first.
  > **Status:** done (2026-10-15).
  > - Fixed: the vampire's guild mapper is told the new guild name, not
  >   the sheet's old one; a gear item the gear refused is no longer
  >   worn; a pocket item (or a held item) past the item table is skipped
  >   and freed instead of ending the inventory's, gear's or held item's
  >   loading; a pocket item the belt or arms band refuses, another
  >   creature's pet items, and `AddItemToZone`'s skipped sub-items are
  >   freed; `AddItemToZone` hands back the item it laid after making way
  >   for it, so `GCAddInstalledMineToZone` marks such a mine installed;
  >   `ComeFromPortal`'s creature without a sprite height rises over
  >   sixteen frames instead of showing at once.
  > - The fake creatures the zone refuses: 4.253 recorded the use after
  >   free that follows as unreachable, because `NewFakeCreature` takes an
  >   ID the zone does not hold. A test reaches it by hiding a fake from
  >   the ID counter, and six sites used the freed creature (Cross
  >   Counter, Shadow Dancing, Illendue, Blaze Walk, `MoveIntoPortal`,
  >   `SetPetInfo`, which also wrote to the freed pet item); each now
  >   skips the refused creature. `MFakeCreature`'s constructor sets the
  >   turret's direction and its final one, which it left uninitialised,
  >   and `SetGearInfo`'s `bAdd` starts false (a regression guard: every
  >   race sets it).
  > - Tests over the 96 mutants 4.253's run counted as test gaps: 51 now
  >   fail a check (the bug reports recorded, sub-items with types and
  >   counts of their own, the ousters' arms band in the gear, every
  >   race's blood bible bound, the sex change's checks, the copies'
  >   looks), 8 failed already through the fixes' tests; 16 are
  >   equivalent, 19 sit in code no build compiles
  >   (`__TEST_SUB_INVENTORY__`, `OUTPUT_DEBUG`), 2 were on the refused
  >   fake's branch, now fixed.
  > - Recorded for the UI's slice: `UI_SetCharacter` indexes the
  >   three-entry face and hair tables with a two-bit style
  >   (`GameUI.cpp`), so a style of 3 reads past them, as
  >   `SetAddonToSlayer` did.
  >
  > R1 is unchanged: 12 Windows / 11 Ninja.
  - Owner: the eleven fixes' tests in `test_packet_player_info.cpp`,
    `test_packet_player_items.cpp`, `test_packet_zone_items.cpp`,
    `test_packet_skill_shows.cpp` and `test_fake_creature.cpp`; each
    but `bAdd`'s (a regression guard) failed on the code before its fix
    (an ASan report, a crash or a failed check), named in its commit.
  - Mutants: 32 on the lines this task changed (deleted
    statements, negated conditions, flipped comparisons): 31 caught; the
    survivor deletes `SetPetInfo`'s `SetZone(g_pZone)`, which
    `NewFakeCreature` has already done. A run over the whole file was
    stopped at 394 of 1482: its 19 survivors were all of the kinds 4.253's
    run showed equivalent (the none slots set to NULL, a new item's
    defaults, the effect target's zeroes, the action start set to the
    target's values).

- [x] **4.255 The waiting screens' update:** `CWaitUIUpdate.cpp` joins
  gameclient.
  > **Status:** done (2026-10-15).
  > - `CWaitUIUpdate` is the update loop of the login and waiting screens:
  >   the keys and the mouse handed to the UI, the frame's input, the
  >   socket's reads and writes, the five-second program check, the
  >   modes it hands on, the draw and the title's fade.
  > - What it asked of the executable goes through `CWaitUIHost`, beside
  >   `CWaitPacketHost` in its header: the mouse and input updates, the
  >   program check, the socket's input and output, `SetMode` and the
  >   title loading's draw and end. `InitGameUpdate` passes it to the one
  >   `CWaitUIUpdate` it makes. Seven globals the loop reads are
  >   gamemodel's `ClientMode.cpp`'s (the frame count, whether the
  >   application is active, the double-click time, the update delay,
  >   the back and last surfaces, the title's alpha; `Client_PCH.h`
  >   declares them), and `g_pUpdate` is `CWinUpdate.cpp`'s.
  > - The test found that `ProcessInput` sent the UI the center button's
  >   release when the right button was released (`m_rb_up`), so a right
  >   release also gave a center release and a center release gave none;
  >   it reads `m_cb_up` now, as `CGameUpdate` does. Nothing in the UI
  >   handles that message today.
  > - The 26 `OUTPUT_DEBUG` and `OUTPUT_DEBUG_UPDATE_LOOP` blocks, which
  >   no build compiled and which named what only the executable defines,
  >   are retired (the preprocessed file is the same but for one log's
  >   line number), and the Korean comments are English.
  >
  > R1 is 11 Windows / 10 Ninja.
  - Owner: membership, R1, `test_wait_ui_update.cpp`: 21 tests over the
    real loop and UI with a recording host, a test window, a fake clock
    and the frame clock set by hand: `Init` installing both input
    receivers and starting text input; the twelve editing keys as
    `WM_KEYDOWN` and then their scan code, other keys as the scan code,
    releases ignored; the double click inside the time and a pixel
    either way, the reset after one, the wheel; `Update`'s draw and tick
    cadence, the program check, a failed socket read or write, the
    application going inactive, each waiting mode and the early stops;
    every button flag of `ProcessInput`, taken by the UI or not; and
    `UpdateDraw`'s order, the copy rectangle from `g_GameRect` and the
    title's fade at each boundary.
  - Mutants: 79 over its six functions (deleted statements,
    negated conditions, flipped comparisons), run against a test binary
    of the one test file: 70 caught, 9 survived. 5 were inside the
    `OUTPUT_DEBUG` blocks that no build defined, since retired; 3 change
    only the log line the first ten editing keys write; 1 negates the
    early return of the function's last statement, which returns either
    way.
  - Link: the linker's map of `unit_tests` shows `CWaitUIUpdate`'s
    members from `libgameclient.a(CWaitUIUpdate.cpp.o)`, `g_FrameCount`
    and `g_pBack` from `libgamemodel.a(ClientMode.cpp.o)` and
    `g_pUpdate` from `libgamemodel.a(CWinUpdate.cpp.o)`.

- [x] **4.256 The game's UI calls:** `GameUI.cpp` joins gameclient.
  > **Status:** done (2026-10-15).
  > - `GameUI.cpp` holds the `UI_` functions the game calls into the UI:
  >   the character sheet's setters, the effect statuses on the slot, the
  >   friends, the world and server lists, the trade's ask, open and
  >   close, storage, the skill-learning talk box, the shop windows, the
  >   three `UI_SetCharacter` slot builders, progress, the HP and MP help
  >   events, the chat history, party and guild windows, quests, the item
  >   shop, the mixing forge, notices, connect, and some 127 one-line
  >   forwards into `C_VS_UI`.
  > - What it asked of the executable goes through `GameUI::Host`
  >   (`UIFunction.h`), which `InitGameObject` installs before the UI is
  >   given `UI_ResultReceiver`: the zone information's reload and
  >   `ExecuteUIMessage`, whose executable entry keeps making
  >   `g_pUIMessageManager` and running it. `g_pOGG` is dxlib's
  >   (`COGGSTREAM.cpp`) and `g_pWebBrowser` VS_UI's (`Vs_ui.cpp`).
  > - What the tests cannot see: without the title or game UI, which load
  >   the data's sprite packs, `C_VS_UI` answers nothing, so a one-line
  >   forward whose only effect is inside the UI is shown only to be
  >   harmless there (and its `IsRunning` questions to answer no). Every
  >   function that does something to the model or calls a host is
  >   pinned. Not run: `UI_StartGame`, `UI_StartProgress` and
  >   `UI_ShowTargetArrow` build or draw windows from the sprite packs;
  >   `UI_DownSkill` and `UI_CloseAllDialog` go through
  >   `C_VS_UI::CloseAllDialog`, which keeps the first string table in
  >   statics; `UI_RunOtherInfo` with a creature and
  >   `UI_DeleteCharacterFailed` dereference UI objects that exist only
  >   with the UI; and the paths below whose behaviour is undefined.
  > - Defects, for task 4.257. Pinned by the tests as they are:
  >   `UI_ClosePartyCancel` never ends the party wait; `UI_SetMP`'s help
  >   mails fire once a process; the quest list repeats the last offer's
  >   line; the guild functions leak their records, and the friend
  >   functions their names without the game UI and the entries they
  >   erase. Found by the test
  >   agents' reading, on paths not run because they are undefined: the
  >   slayer `UI_SetCharacter` reads past its three- and four-entry look
  >   tables for the login packet's bit fields, and `Character[slotID]` is
  >   unchecked; the mixing forge uses an item it deleted when both items
  >   share an id; `UI_RunQuestList` reads an uninitialised string;
  >   `QUEST_INFO` leaves two fields unset; an unknown zone in the lair
  >   message, an unknown creature in the party ask and an unknown server
  >   group crash; and, seen by reading only (no run checks for leaks),
  >   a second connect leaks the login strings it replaces and the item
  >   shop the items its list refuses.
  >   Pinned and left as they are: a negative effect status reads the
  >   table's default row; an effect's delay in frames times 1000 wraps as
  >   a DWORD; `BackupPrevId` writes through `g_pUserOption` unchecked;
  >   `GetMyLevelWarStair` puts level 0 and a negative sum on stair 2.
  > - The Korean comments are English (169 lines; two were Chinese read
  >   as Korean).
  >
  > R1 is 10 Windows / 9 Ninja.
  - Owner: membership, R1, and 99 tests in three files:
    `test_game_ui_1.cpp` (40: the host, the interface race, the sheet's
    setters, the effect statuses with their boundaries, friends, the
    world and server lists' Netmarble queue through a real `Base`, the
    user option's file, the descriptor, the computer, the trade's every
    branch, storage, the login id, the ID message boxes, the mouse
    item), `test_game_ui_2.cpp` (28: the skill-learning talk box and its
    boundaries, the windows that hide the NPC talk, the slot builders'
    combat stats, progress drawing, the HP and MP help events, the chat
    history, pick-up and drop, `UI_ResultReceiver` through the host,
    party and the guild packets) and `test_game_ui_3.cpp` (31: weapon
    speed, guild, elementals, the war stair, notices, the name window,
    the war list, the quest list and status, the item shop, the mixing
    forge and option removal, pop-ups, SMS errors, the lair message,
    mini-game scores, connect, the cursor).
  - Mutants: 968 over the 199 of its 228 functions the generator
    recognised (deleted statements,
    negated conditions, flipped comparisons), run against a test binary
    of the three files: 339 caught, 8 did not build, 621 survived. 162
    change only a call into `C_VS_UI`, a dialog, the descriptor manager
    or the cursor, which without the title or game UI does nothing. 303
    are in what `UI_SetCharacter`'s three overloads (258) and
    `UI_RunOtherInfo` (45) build: a slot and a player record that go only
    to the title's character manager and the other-info window, which
    need the sprite packs; task 4.257 splits the building from the
    hand-off and tests it. 52 are in the guild windows' records, 11 in
    the cursor's correction loop, and 93 in 23 other functions, most of
    them also feeding a window (the blood bible status 17, progress 11,
    the war list 11).
  - Link: the linker's map of `unit_tests` shows `UI_SetCharacter`,
    `UI_ResultReceiver`, `UI_RunExchangeAsk`, `UI_AddEffectStatus` and
    the other `UI_` functions from `libgameclient.a(GameUI.cpp.o)`.

- [x] **4.257 The game's UI calls' defects:** what 4.256's tests found is
  fixed, each test first, and the slots and the other player's record are
  built where a test can read them.
  > **Status:** done (2026-10-16).
  > - Memory and undefined behaviour: a slayer slot's outlook past its
  >   three- and four-entry tables takes the first entry (the login
  >   packet's bit fields); a character-select slot past the user's three
  >   names is refused, and a name left empty is an empty name, not NULL; the HP and MP bars' percentage is worked out
  >   without overflow; a negative chat condition is dropped; the forge
  >   refuses one item as both, which it deleted and then used; a quest
  >   with nothing to say gets an empty text, not an uninitialised one,
  >   and `QUEST_INFO` starts its event fields at 0 with only five event
  >   titles; the lair message says nothing of a zone the table does not
  >   know; the input-name window refuses a mode it does not have; a shop
  >   gun with no magazine is listed empty; an empty world or server name
  >   is listed as one and an unknown current group lists nothing; the
  >   party invitation checks the zone and the inviter first; the other
  >   player's face outside the sex's three hairs is the first, and
  >   `C_VS_UI::SetOtherInfo` does nothing without the game window.
  > - Logic and leaks: the party cancel ends the party wait; `UI_SetMP`
  >   remembers its percent on every call, as `UI_SetHP` does; each quest
  >   in an NPC's list has its own line; the guild windows' records, a
  >   friend's name when no dialog will own it, a connect's login strings
  >   it replaces and an item shop item the list does not take are freed.
  >   An erased friend entry is still not freed (a chatting window may hold
  >   it), and `CLD_MASTER`'s lines are dropped from the chat history on
  >   purpose (the chat window has no filter for them).
  > - `MakeCharacterSlot` (one per race) and `FillOtherInfo`
  >   (`GameUIBuilders.h`) build what `UI_SetCharacter` and
  >   `UI_RunOtherInfo` hand the title's character manager and the
  >   other-info window, which need the sprite packs; the `UI_` functions
  >   are guard, build, hand over, with the same order and side effects.
  > - Recorded, not changed: the title's login mallocs the strings the
  >   login handler frees with `delete[]`; `C_VS_UI_GAME::RunFriendWaitAsk`
  >   drops the name when a wait dialog is open, and
  >   `C_VS_UI_FRIEND_INFO`'s destructor never frees its lists;
  >   `UI_RunOtherInfo` takes any slayer creature for an `MCreatureWear`;
  >   `MQuestInfo`'s constructor leaves `m_TimeLimit` unset; the pet
  >   record `UI_RunOtherInfo` builds is still inside the hand-over.
  >
  > R1 is unchanged: 10 Windows / 9 Ninja.
  - Owner: the fixes' tests in `test_game_ui_1.cpp`, `test_game_ui_2.cpp`
    and `test_game_ui_3.cpp` (each seen failing on the code before its
    fix: an ASan or UBSan report or a failed check), and nine for the
    builders: every field of each race's slot with values unlike the
    defaults and each other, every outlook value with the ones past the
    tables, the 19 weapons, the colours and their clamps, and the other
    player's face and name.
  - Mutants: over the builders, 290: 250 caught, 1 did not build, 39
    survived, all equivalent (defaults written again, an alignment block
    that does nothing to a `short`, clamps that only ever see 0). Over the
    lines the fixes changed, 60 before the builders: 34 caught, 16 more
    are the builders' and caught by their tests, 3 (the first slot in two
    overloads and the first input-name mode) caught by checks added for
    them; the other 7 change only calls into `C_VS_UI` or a chat condition
    no test can run.

- [x] **4.258 The UI's messages:** `UIMessageManager.cpp` joins gameclient.
  > **Status:** done (2026-10-16).
  > - `UIMessageManager` turns the UI's results into packets and model
  >   changes: `Execute` dispatches 221 handlers (of 231 messages) for the
  >   title and character screens, login and registration, chat and its
  >   commands, the item drops and pick-ups, the shop, repair and
  >   silvering, storage, trade, options, the elevator, parties, guilds,
  >   friends, the personal store, quests and the small windows.
  > - What it asked of the executable goes through `UIMessages::Host`
  >   (`UIMessageManager.h`), which `InitGameObject` installs before the
  >   manager is made and `ReleaseAllObjects` clears after it is deleted:
  >   the socket's start, the add-on sprite loading, the game objects'
  >   release, the loading thread's stop, the last selected character,
  >   `SetMode`, and the view's selected creature and item. The weather
  >   and the zone's music go through `WorldEvents`, the socket's flush
  >   through `LoginPresentation`. `g_bUIInput` is gamemodel's
  >   `InputState`'s, `g_bWatchMode` and the two sequence numbers
  >   `ClientMode`'s, `g_oggfile` dxlib's `COGGSTREAM`'s, and the sound
  >   range constants `basic/AudioTypes.h`'s. Three dead `/* */` blocks
  >   that named executable globals are gone; the mini-game time's
  >   `min` takes operands of one type (the move would otherwise have
  >   surfaced a `-Wsign-compare` the precompiled header hid).
  > - The Korean comments are English (753 lines); eight lines keep
  >   their Korean, in live strings the game uses (the curse filter's
  >   defines, the ID length message, two chat commands it matches on).
  > - The tests found defects, which task 4.259 fixes where they are
  >   memory-unsafe, undefined, put uninitialised bytes on the wire, throw
  >   out of `Execute` or leak; the client-side game rules the server
  >   checks again (prices, counts, money, couple and forge rules, chat
  >   routing) are pinned as they are and left.
  >
  > R1 is 9 Windows / 8 Ninja.
  - Owner: membership, R1, and 270 tests in four files over `Execute`
    with recording hosts and a real connection that writes every packet
    (byte for byte against the expected one): `test_ui_messages_1.cpp`
    (42: the table, `Execute`'s gate, the music, the new character, the
    registration, the ID check, the deletion, the login and connect),
    `test_ui_messages_2.cpp` (62: chat and every command, the logout's
    countdown, skill selection, the four item drops), `test_ui_messages_3.cpp`
    (111: pick-ups and inserts, the shop, repair, silvering, storage and
    money, trade, options, the elevator) and `test_ui_messages_4.cpp` (55:
    some 160 small handlers, 64 plain senders in one table). The login and
    registration send their version packet only without `_DEBUG`, and the
    tests expect it the same way (MSVC's Debug builds define it); the
    handler table is counted by slot, not by address (an optimising
    linker folds identical bodies).
  - Mutants: a seeded random sample of 1200 of the 2367
    mutants over its 247 functions (the whole run would take hours: four
    login and registration tests sleep 500 ms each as the handlers do),
    run against a test binary of the four files: 900 caught, 5 did not
    build, 295 survived. 106 change only what the UI or the message log
    is told; 7 are frees no test sees (no run checks for leaks); 17 are
    in `ExecuteLogout`'s title path and 13 in the music's audio-only
    branches, neither run; the other 152, across the handlers, are not
    classified one by one and stand as test gaps.
  - Link: the linker's map of `unit_tests` shows `UIMessageManager::Execute`
    and its handlers (`Execute_UI_LOGIN`, `Execute_UI_CHAT_RETURN`, ...)
    from `libgameclient.a(UIMessageManager.cpp.o)`, `g_bUIInput` from
    `libgamemodel.a(InputState.cpp.o)`, `g_bWatchMode` from
    `libgamemodel.a(ClientMode.cpp.o)` and `g_oggfile` from
    `libdxlib.a(COGGSTREAM.cpp.o)`.

- [x] **4.259 The UI's messages' defects:** what 4.258's tests found
  unsafe is fixed, each test first.
  > **Status:** done (2026-10-16).
  > - Memory and undefined behaviour: `Execute` checks a message against
  >   its table in every mode, not only in the game; a new character's
  >   slot or face past the three, an empty slot or one past the three to
  >   delete or connect, a bonus point button outside the three stats, a
  >   gear slot at or past the race's gear count, a negative game string
  >   id and an elevator button past the floor list are refused; an empty
  >   master name makes no character a master; the login checks its
  >   socket before it sends; a quick slot without a belt, the buy without
  >   the NPC's talk box, the UI's item messages with no item, a NULL
  >   string from the UI and a zone the zone table lacks are taken as
  >   nothing; chat commands lower their bytes through `unsigned char`.
  > - The wire: the relic packet's coordinates, the two guild requests'
  >   guild id and the self skill's effect id start at 0 in their
  >   constructors, so no indeterminate bytes go out (the server reads
  >   none of them); the store-info packets are sent only with a store,
  >   which none has today.
  > - Exceptions out of `Execute`: a login ID or password the login
  >   packet cannot carry, an empty guild or ranger line, and a missing
  >   file name in the options' close are refused or skipped instead of
  >   thrown.
  > - Leaks: the registration and ID-check strings on their early
  >   returns, an item taken off the stall, a friend's name with no game
  >   UI; a held item dropped past the storage rack stays on the mouse,
  >   since the server was never told it moved.
  > - Recorded, not changed: the client-side rules the server checks
  >   again (prices and star prices, buy counts, money piles, repair and
  >   silvering checks, trade amounts, the couple, forge and rank-bonus
  >   rules, popup ids, lottery steps, pet slots, the chat's routing of
  >   `*` lines); the gear drop that empties the mouse when the gear
  >   refuses; the handlers that never set the guild id or Eternity's
  >   effect id; `g_pPCTalkBox` read unchecked in sell, sell all, the
  >   couple and the swap; `GCRemoveStoreItemHandler`'s undeleted item.
  >
  > R1 is unchanged: 9 Windows / 8 Ninja.
  - Owner: the fixes' tests in `test_ui_messages_1.cpp` to
    `test_ui_messages_4.cpp` and `test_packet_goldens.cpp`, each seen
    failing on the code before its fix (an ASan or UBSan report, an
    exception or a failed check) but the `tolower` one, a regression
    guard (macOS's libc returns a negative value unchanged).
  - Mutants: the 100 mutants on the lines the fixes changed:
    93 caught, 1 did not build, 6 survived. Two are in the friend
    handlers' game-UI condition, which no test can raise; one moves the
    message box's lower bound to 1, seen only as a message box for string
    0; three remove the store-info sends, which no store reaches today.

### Remaining R1 work after tasks 4.80-4.259

The cumulative implementation at `3c72418a` passes all 24 CTest checks,
including 3,334 unit tests, in local GCC Debug and Clang ASan/UBSan builds.
[Windows Debug, Release and ASan CI](https://github.com/bound2/opendarkeden-client/actions/runs/37294455682)
verifies that code revision and R1 = 346.
[Linux GCC, Clang and ASan/UBSan CI](https://github.com/bound2/opendarkeden-client/actions/runs/37290300830)
verifies `dfcf0b65` and R1 = 344; later corrections only parenthesize
`std::max` for Windows macros and use the existing portable file opener in
one audio test. Warning budgets remain unchanged.

A fresh Linux Ninja inventory contains 344 executable sources: 263 packet
handlers, 13 other effect-named files and 68 other files. These are inventory
categories, not independent modules. Windows retains two additional sources.

Task 4.88 further reduces the Linux inventory to 343 sources (262 handlers),
with the corresponding Windows baseline at 345. The earlier CI links above
cover tasks 4.80-4.87, not this later extraction; its PR records verification.

Stack handoff (2026-10-05): #372 reached `master`, but #373-#379 merged into
their original stack bases after those parent PRs closed. Recovery PR #381
brings the complete `88b4f3af` tree into `master` without implementation changes.
Merge #381 first, then retarget #382 to `master` before merging it. Both PRs
remain unmerged. #382 passes all 24 local Debug and Clang ASan/UBSan CTest
checks and the negative production-object link check. Its Windows and Linux
CI runs have now passed; exact revisions and results are linked in that PR.
Zero remains unfinished.

Task 4.89 reduces the Linux inventory to 342 executable sources, with the
corresponding Windows baseline at 344. All 24 local Clang Debug and ASan/UBSan
CTest checks pass (3,345 unit tests), as do Linux and Windows CI. PR #383
records the production-object link proof and exact verified revisions; it
depends on #382 and must be retargeted to `master` after that parent lands.

Task 4.90 further reduces R1 to 340 Linux / 342 Windows. Before its guard was
applied, tests against the extracted original arithmetic observed zero becoming
negative and invalid IDs displaying success; Clang UBSan stopped on `INT_MIN - 1`.
The new tests cover these replies and the valid relearning sequence.
All 24 local Clang Debug and ASan/UBSan CTest checks pass, including 3,354 unit
tests. Removing either handler object independently breaks the test link.
PR #384 records those results and the pending platform CI; it remains draft.
The complete dependency order is #381, #382, #383, #384. After each parent
lands in `master`, retarget its child to `master` before merging that child.
No PR was merged by the agent. Task-owned build trees and scratch files are
removed at handoff; the pre-existing `build/presets/linux` cache is preserved.

Task 4.91 reduces R1 to 327 Linux / 329 Windows by moving thirteen handlers
that ignore their packet; 247 handlers remain executable-side. Task 4.92 reduces
it to 318 Linux / 320 Windows with nine guild and union reply handlers; 238
remain. Task 4.93 reduces it to 314 Linux / 316 Windows with the friend, SMS
address-book and whisper-failure replies; 234 remain. Task 4.94 reduces it to
309 Linux / 311 Windows with the quest and war status replies; 229 remain.
Task 4.95 reduces it to 306 Linux / 308 Windows with the trade, crafting and
item-mall replies; 226 remain. Task 4.96 retires three sources that compile to
nothing, for 303 Linux / 305 Windows. Task 4.97 reduces it to 299 Linux / 301
Windows with the NPC map, skill teaching, mini-game and prize-notice replies;
222 handlers remain. Task 4.98 reduces it to 290 Linux / 292 Windows with nine
message-box and item-release replies; 213 remain. Task 4.99 reduces it to 283
Linux / 285 Windows with seven login-server replies; 206 remain. Task 4.100
reduces it to 267 Linux / 269 Windows with sixteen replies about the client's
own player; 190 remain. Task 4.101 reduces it to 255 Linux / 257 Windows with
twelve replies about the zone and its creatures; 178 remain. Task 4.102 reduces
it to 248 Linux / 250 Windows with seven replies handed on to the world and
connections; 171 remain. Task 4.103 moves `MItem::NewItem` into `gamemodel`
over an installed factory, for 247 Linux / 249 Windows with the trade box's
new-item reply; 170 remain. Task 4.104 reduces it to 242 Linux / 244 Windows
with the guild-list, guild-membership and character-error replies; 165 remain.
Task 4.105 moves the client mode and the message logs' pointers into
`gamemodel`, for 236 Linux / 238 Windows with six replies that read them; 159
remain. Task 4.106 reduces it to 230 Linux / 232 Windows with six skill and
damage results on zone creatures and the player; 153 remain. Task 4.107
reduces it to 226 Linux / 228 Windows with the personal-store and nickname
replies; 149 remain. Task 4.108 reduces it to 224 Linux / 226 Windows with
the wall see-through test and the peer disconnect request. Task 4.109
reduces it to 223 Linux / 225 Windows with the peer file-send queue, and
task 4.110 to 222 Linux / 224 Windows with the help display over the frame
clock. Task 4.111 reduces it to 221 Linux / 223 Windows with the
installed-mine reply; 148 handlers remain. Task 4.112 reduces it to 219
Linux / 221 Windows with the login and registration error replies; 146
remain. Task 4.113 reduces it to 218 Linux / 220 Windows with the whisper
reply; 145 remain. Task 4.114 reduces it to 217 Linux / 219 Windows with
the kick-warning reply; 144 remain. Task 4.115 reduces it to 216 Linux /
218 Windows with the made-item reply; 143 remain. Task 4.116 reduces it to
215 Linux / 217 Windows with the pet-stash reply; 142 remain. Task 4.117
reduces it to 214 Linux / 216 Windows with the system-message reply; 141
remain. Task 4.118 reduces it to 212 Linux / 214 Windows with the refused
rank bonus and the guild-union offers; 139 remain. Task 4.119 reduces it
to 210 Linux / 212 Windows with the quest element and quest item replies;
137 remain. Task 4.120 reduces it to 209 Linux / 211 Windows with the
quest-status reply; 136 remain. Task 4.121 reduces it to 208 Linux / 210
Windows with the party-leave reply; 135 remain. Task 4.122 reduces it to
207 Linux / 209 Windows with the shop-sold reply; 134 remain. Task 4.123
reduces it to 206 Linux / 208 Windows with the party-error reply; 133
remain. Task 4.124 reduces it to 205 Linux / 207 Windows with the stash
reply; 132 remain. Task 4.125 reduces it to 204 Linux / 206 Windows with
the nickname-verify reply; 131 remain. Task 4.126 reduces it to 201 Linux /
203 Windows with the war-list and party-position replies; 128 remain. Task 4.127
reduces it to 199 Linux / 201 Windows with the auth-key and sub-inventory
replies; 126 remain. Task 4.128 reduces it to 198 Linux / 200 Windows
with the peer whisper; 125 remain. Task 4.129 reduces it to 197 Linux /
199 Windows with the action-result nodes, which 32 of the remaining
skill-reply handlers construct in live code (29 after 4.130, 27 after
4.131, 25 after 4.132, 19 after 4.133 and 18 after 4.134; counting nodes
inside `/* */` blocks too gives 34 and 31). Task 4.130 reduces it to 193 Linux /
195 Windows with four special-action skill replies; 121 handlers remain.
Task 4.131 reduces it to 189 Linux / 191 Windows with four basic-action
attack and skill replies; 117 remain. Task 4.132 reduces it to 185 Linux /
187 Windows with four player-target skill replies; 113 remain. Task 4.133
reduces it to 179 Linux / 181 Windows with six player attack replies; 107
remain.
Task 4.134 reduces it to 177 Linux / 179 Windows with two self-skill replies
and the result-node factory; 105 remain. Task 4.135 reduces it to 174
Linux / 176 Windows with the cross-counter replies; 102 remain. Task 4.136
reduces it to 172 Linux / 174 Windows with the add-effect and corpse-head
replies; 100 remain. Task 4.137 reduces it to 169 Linux / 171 Windows
with the three bomb replies; 97 remain. Task 4.138 reduces it to 166
Linux / 168 Windows with the unburrow, remove-effect and change-shape
replies; 94 remain. Task 4.139 reduces it to 162 Linux / 164 Windows
with four skill-to-tile replies; 90 remain. Task 4.140 reduces it to
159 Linux / 161 Windows with the two mine explosions and the unseen
caster's sector skill; 87 remain. Task 4.141 reduces it to 156 Linux /
158 Windows with the take-off and motorcycle get-off replies; 84 remain.
Task 4.142 reduces it to 154 Linux / 156 Windows with the unseen skill
on the player and the untransform reply; 82 remain. Task 4.143 reduces
it to 152 Linux / 154 Windows with the say and global chat replies; 80
remain. Task 4.144 reduces it to 149 Linux / 151 Windows with the
learn-skill, cannot-add and remove-from-gear replies; 77 remain. Task
4.145 reduces it to 147 Linux / 149 Windows with the two skill-failed
replies; 75 remain. Task 4.146 reduces it to 144 Linux / 146 Windows
with the motorcycle ride and gear-to-zone replies; 72 remain. Task
4.147 reduces it to 142 Linux / 144 Windows with the shop purchase and
sale replies; 70 remain. Task 4.148 reduces it to 140 Linux / 142
Windows with the trade error and verify replies; 68 remain. Task 4.149
reduces it to 138 Linux / 140 Windows with the inventory-item and object
deletion replies; 66 remain. Task 4.150 reduces it to 137 Linux / 139
Windows with the cannot-use reply; 65 remain. Task 4.151 reduces it to
134 Linux / 136 Windows with the three NPC question replies; 62 remain.
Task 4.152 reduces it to 133 Linux / 135 Windows with the blood bible
list; 61 remain. Task 4.153 reduces it to 131 Linux / 133 Windows with
the storage price and other-creature status replies; 59 remain. Task
4.154 reduces it to 129 Linux / 131 Windows with the set-position and
dark-light replies; 57 remain. Task 4.155 reduces it to 127 Linux / 129
Windows with the real-wearing and item-use replies; 55 remain. Task
4.156 reduces it to 126 Linux / 128 Windows with the shop market
condition; 54 remain. Task 4.157 reduces it to 125 Linux / 127 Windows
with the player's own skill success; 53 remain. Task 4.158 reduces it to
122 Linux / 124 Windows with the three shop list replies; 50 remain.
Task 4.159 reduces it to 119 Linux / 121 Windows with the power point and
point exchange replies; 47 remain. Task 4.160 reduces it to 118 Linux /
120 Windows with the party invitation; 46 remain. Task 4.161 reduces it
to 117 Linux / 119 Windows with the party joined; 45 remain. Task 4.162
reduces it to 115 Linux / 117 Windows with the vampire portal and item
drop replies; 43 remain. Task 4.163 reduces it to 113 Linux / 115
Windows with the fast move and fake move replies; 41 remain. Task
4.164 reduces it to 112 Linux / 114 Windows with the peer's file
request; 40 remain. Task 4.165 reduces it to 110 Linux / 112 Windows
with the login and new-account welcomes; 38 remain. Task 4.166
reduces it to 109 Linux / 111 Windows with the full quest list; 37
remain. Task 4.167 reduces it to 108 Linux / 110 Windows with the pet's
skill; 36 remain. Task 4.168 reduces it to 107 Linux / 109 Windows with
the tile effect; 35 remain. Task 4.169 reduces it to 105 Linux / 107
Windows with the personal shop replies; 33 remain. Task 4.170 reduces
it to 104 Linux / 106 Windows with the created item; 32 remain. Task
4.171 reduces it to 103 Linux / 105 Windows with the item-to-item
verdict; 31 remain. Task 4.172 reduces it to 102 Linux / 104 Windows
with the burrowing creature; 30 remain. Task 4.173 reduces it to 100
Linux / 102 Windows with the bat and wolf replies; 28 remain. Task 4.174
reduces it to 98 Linux / 100 Windows with the monster from a burrow or a
transformation; 26 remain. Task 4.175 reduces it to 97 Linux / 99
Windows with the monster appearing; 25 remain. Task 4.176 reduces it to
96 Linux / 98 Windows with a creature's skill on another creature; 24
remain. Task 4.177 reduces it to 94 Linux / 96 Windows with the player's
skill on a creature and a skill on the player; 22 remain. Task 4.178
reduces it to 93 Linux / 95 Windows with a creature turning into a
slayer; 21 remain. Task 4.179 reduces it to 92 Linux / 94 Windows with a
creature turning into a vampire; 20 remain. Task 4.180 reduces it to
90 Linux / 92 Windows with the vampire from a burrow or a
transformation; 18 remain. Task 4.181 reduces it to 89 Linux / 91
Windows with the vampire appearing; 17 remain. Task 4.182 reduces it to
88 Linux / 90 Windows with the slayer appearing; 16 remain. Task 4.183
reduces it to 87 Linux / 89 Windows with the ouster appearing; 15
remain. Task 4.184 reduces it to 86 Linux / 88 Windows with a monster's
corpse appearing; 14 remain. Task 4.185 reduces it to 85 Linux / 87
Windows with a vampire's corpse appearing; 13 remain. Task 4.186
reduces it to 84 Linux / 86 Windows with a slayer's corpse appearing;
12 remain. Task 4.187 reduces it to 83 Linux / 85 Windows with an
ouster's corpse appearing; 11 remain. Task 4.188 reduces it to 82 Linux
/ 84 Windows with an NPC appearing; 10 remain. Task 4.189 reduces it to
81 Linux / 83 Windows with a trade's preparation; 9 remain. Task 4.190
reduces it to 80 Linux / 82 Windows with the player transforming; 8
remain. Task 4.191 reduces it to 79 Linux / 81 Windows with an item
picked up; 7 remain. Task 4.192 reduces it to 78 Linux / 80 Windows
with a skill used on an inventory item; 6 remain. Task 4.193 reduces it
to 77 Linux / 79 Windows with the event notices; 5 remain. Task 4.194
reduces it to 76 Linux / 78 Windows with a gun reloaded; 4 remain.
Task 4.195 reduces it to 75 Linux / 77 Windows with the reconnect to
the login server; 3 remain. Task 4.196 reduces it to 74 Linux / 76
Windows with the reconnect to the game server; 2 remain. Task 4.197
reduces it to 73 Linux / 75 Windows with an NPC's answers; 1 remains.
Task 4.198 reduces it to 72 Linux / 74 Windows with the player's update
on entering a zone. No packet handler is left in the executable. What
remains is its core: the player, creatures, zone, view, game loop,
composition root and UI glue. Task 4.199 reduces it to 69 Linux / 71
Windows with the three generators that attach effects to creatures.
Task 4.200 reduces it to 67 Linux / 69 Windows with the guided and
homing attacks on a creature. Task 4.201 reduces it to 65 Linux / 67
Windows with fire piercing and playing with fire. Task 4.202 reduces it
to 63 Linux / 65 Windows with the attach-zone and pixel-ripple
generators, the last effect generators the executable compiled. Task
4.203 reduces it to 61 Linux / 63 Windows: it retires the unused door
actions and moves the colour-set table. Task 4.204 reduces it to 59
Linux / 61 Windows with the pack-file record and the objects' screen
rects. Task 4.205 reduces it to 56 Linux / 58 Windows with the video
stub, the update base and the work-queue node base. Task 4.206 reduces
it to 55 Linux / 57 Windows with the debug kit. Task 4.207 reduces it
to 54 Linux / 56 Windows by retiring `Globals.cpp`. Task 4.208 reduces
it to 53 Linux / 55 Windows by folding `TextServiceScreen.cpp` into
TextSystem. Task 4.209 reduces it to 52 Linux / 54 Windows with the
packet wait. Task 4.210 reduces it to 51 Linux / 53 Windows by retiring
the opening's loop. Task 4.211 reduces it to 50 Linux / 52 Windows with
the frame present. Task 4.212 reduces it to 49 Linux / 51 Windows with
the append patch. Task 4.213 reduces it to 48 Linux / 50 Windows with
the tile renderer. Task 4.214 reduces it to 47 Linux / 49 Windows with
the MIDI player's state. Task 4.215 reduces it to 46 Linux / 48 Windows
by retiring the MP3 player. Task 4.216 reduces it to 45 Linux / 47
Windows with the effect resource container. Task 4.217 reduces it to 44
Linux / 46 Windows with the loading thread's sprite-pack jobs. Task 4.218
reduces it to 43 Linux / 45 Windows with the packet-handler registry.
Task 4.219 reduces it to 42 Linux / 44 Windows with the effect generator
table. Task 4.220 reduces it to 41 Linux / 43 Windows with the Netmarble
launch check. Task 4.221 reduces it to 40 Linux / 42 Windows by retiring
the help decision tree's conditions. Task 4.222 reduces it to 39 Linux /
41 Windows with help events' mail. Task 4.223 reduces it to 38 Linux /
40 Windows with the zone's tile provider. Task 4.224 reduces it to 37
Linux / 39 Windows by retiring the event background images. Task 4.225
reduces it to 36 Linux / 38 Windows with the object selector. Task 4.226
reduces it to 35 Linux / 37 Windows with the helicopters. Task 4.227
reduces it to 34 Linux / 36 Windows with the zone's sounds. Task 4.228
reduces it to 33 Linux / 35 Windows with the FL2 text layer. Task 4.229
reduces it to 32 Linux / 34 Windows with how some creature types are drawn.
Task 4.230 reduces it to 31 Linux / 33 Windows with the guild marks.
Task 4.232 reduces it to 30 Linux / 32 Windows with the account input rules
and the BMP loader (4.231 retired the rest of their file).
Task 4.233 reduces it to 29 Linux / 31 Windows with the map cell.
Task 4.234 reduces it to 28 Linux / 30 Windows with the player's available
skills.
Task 4.235 reduces it to 27 Linux / 29 Windows with the status changes.
Task 4.236 reduces it to 26 Linux / 28 Windows with ClientFunction's split.
Task 4.237 reduces it to 25 Linux / 27 Windows with the item uses.
Task 4.238 reduces it to 24 Linux / 26 Windows with the talk box and the
message box.
Task 4.242 reduces it to 23 Linux / 25 Windows with the creature.
Task 4.243 reduces it to 22 Linux / 24 Windows with the creature that wears
gear.
Task 4.244 reduces it to 21 Linux / 23 Windows with the NPC.
Task 4.245 reduces it to 19 Linux / 21 Windows with the fake creature and
the retired MemoryPool.cpp.
Task 4.246 reduces it to 18 Linux / 20 Windows with the zone.
Task 4.247 reduces it to 17 Linux / 19 Windows with the player.
Task 4.248 reduces it to 16 Linux / 18 Windows with the divine-guidance
silhouettes, gameview's first member.
Task 4.249 reduces it to 15 Linux / 17 Windows with the creature shadows.
Task 4.250 reduces it to 14 Linux / 16 Windows with the creatures drawn with an
effect.
Task 4.251 reduces it to 13 Linux / 14 Windows with the crash report.
Task 4.252 reduces it to 12 Linux / 13 Windows with the start-up's loading,
the first member of a new library, gameclient.
Task 4.253 reduces it to 11 Linux / 12 Windows with the packet handlers'
helpers.
Task 4.255 reduces it to 10 Linux / 11 Windows with the waiting screens'
update.
Task 4.256 reduces it to 9 Linux / 10 Windows with the game's UI calls.
Task 4.258 reduces it to 8 Linux / 9 Windows with the UI's messages.

Continue with reviewable boundaries. The packet handlers (4.198) and the
effect generators (4.202) are all in gamemodel. What remains:
- small sources that touch a few executable globals, behind a host;
- dead code to retire, with the argument that it is dead;
- the coupled cores: `MPlayer`, `MTopView`, `UIMessageManager`,
  `GameMain`, `GameInit` and `Client`. They need their state extracted
  before they can run without the executable. `MCreature` went first
  (4.242), the classes derived from it but the player followed
  (4.243-4.245), and the zone (4.246).
Each slice needs production-object link evidence and contract tests, not
merely a new static-library wrapper.
Their extraction needs independent ownership and packet-contract tests before
any suspected defect is treated as reproduced.

Follow-up from 4.88's source review: `GCRegenZoneStatus::read` appends on repeated
decoding into one packet object. Production currently creates fresh packets;
reuse would retain old ownership bytes. This extraction leaves wire code
unchanged and tests consecutive replies with distinct decoded packet objects.

Follow-up from 4.91: `GCShowMessageBox::read` and `write` throw "too long
Message size" for every non-empty message (an unconditional `else throw`),
and `write` never writes the length byte. The server reads and writes a
1-255 byte string and sends the packet nowhere, so nothing reaches it today;
if a server did send it, the client's reader would throw and
`UpdateSocketInput` would report the error and drop the player to the main
menu. A fix needs both halves to match the server's encoding.

Follow-ups from 4.92's review, both older than the move: `GCGuildChatHandler`
uses `g_pChatManager` without a null check (the game creates it in
`InitInfomation` before any packet; `CRWhisperHandler` checks it), and the
executable's `UI_ShowGuildMemberList` pops every `GuildMemberInfo` off the
packet with `popFrontGuildMemberInfoList` and never deletes it, so each
member of each `GCGuildMemberList` leaks; the packet's destructor frees only
what is still listed.

Follow-ups from 4.93, all older than the move:
- `GCSMSAddressList::read` allocates its `AddressUnit`s and its destructor
  frees none, because the executable's SMS list adopts them:
  `C_VS_UI_SMS_LIST::SetSMSList` keeps the raw pointers (read through its own
  same-layout `AddressUnit`) and frees one only in `DeleteList`. Closing the
  list and replacing it drop the rest without deleting, so they leak in the
  UI. Freeing them in the packet's destructor would leave the list dangling;
  settle ownership first. `DeleteList` also deletes the last unit it visited
  when the id is not found, which stays listed.
- `GCWhisperFailedHandler` sends its message to chat line 5, commented
  "info"; since `CLD_UNION` was inserted (2004) line 5 is the union line and
  info is 6, so whisper failures show among union chat.
- `GCAddressListVerifyHandler` resets `TempInformation`'s mode after every
  code, but sending an SMS sets no mode, so a send reply clears whatever
  request the player started in between; it also uses `g_pTempInformation`
  unguarded.
- `GCFriendChatting::read` prints debug lines to standard output on every
  packet.

Follow-up from 4.94, older than the move: `GCSelectQuestIDHandler` opens the
quest-selection request (`MODE_SELECT_QUEST`) even when `UI_RunQuestList`
returns without a dialog (an empty list, or no quest manager). Nothing then
clears it, so requests that need an idle register, such as trading, stay
blocked until something else resets it; it also overwrites any request
already pending. `UI_SetBloodBibleStatus` leaves `status.status`
uninitialised for storage or race values it does not handle.

Follow-ups from 4.95, older than the move: crafting overwrites any pending
request with `MODE_SKILL_MAKE_ITEM` and locks the item, and the reply
unlocks it only if that mode is still set, so anything that changes the
mode meanwhile (`UI_RunItemShop`, which clears any pending request, the
address-book reset, the quest list) leaves the item locked. If the purse
is too full to take the offer back, `CancelTrade` leaves it in the trade
box and `UI_CloseExchange` then deletes the trade manager with it.

Follow-ups from 4.98, all older than the move:
- `GCPetStashVerifyHandler` removes `pItem` from one container but inserts
  the request's `pValue` into the other. A null `pValue` drops the removed
  pet (leaked, gone from the client); a `GCPetStashList` arriving between
  request and reply rebuilds `g_pStorage`, leaving `pValue` dangling while
  `SetPetKeepedDay` and `AddItem` write through it; a failed `AddItem` or
  `SetItem` leaks the removed item with only a log line. `MStorage::SetItem`
  deletes a slot's occupant, so a confirmation for a slot that is not empty
  destroys that item. `MPlayer::ChangeToSlayer` and `ChangeToVampire` delete
  `g_pStorage` without clearing a pending `MODE_PETITEM_*` request, which a
  later `PET_STASH_OK` then dereferences.
- `GCLearnSkillFailed`, `GCUseBonusPointFail`, `GCShopBuyFail` and
  `GCShopSellFail` clear whatever request is pending, related or not (the
  class of 4.93's address-book reset), all with `g_pTempInformation`
  unguarded.
- `GCShopBuyFailHandler` ignores its code and amount: every "not enough
  star" code shows "not in stock".

Follow-ups from 4.99, all older than the move:
- `LCCreatePCOKHandler` and `LCDeletePCOKHandler` use the
  `dynamic_cast<ClientPlayer*>` of their player unchecked.
- `LCPCListHandler` leaves an empty slot's name, race and face style in
  `g_pUserInformation`, so a deleted character's name stays in its slot;
  the delete and select paths in `UIMessageManager.cpp` read the slot by
  index and are safe only while the UI never offers an emptied slot. The
  test pins this as today's behaviour, not as intended. `UserInformation`'s
  slot arrays are a literal `[3]` while the loop runs to `SLOT_MAX`
  (equal today; nothing ties them).
- Each refusal in `LCSelectPCErrorHandler` stalls the frame loop for a
  second, which on the web build blocks the browser's main thread; the
  `Sleep` entry's adapter is where a fix would go.
  `SELECT_PC_CANNOT_PLAY_BY_ATTR` shows the non-PK login message rather
  than its own string.
- `LCQueryResultCharacterNameHandler` uses `g_pUserInformation` unguarded,
  and once `WhisperID` is released an empty name during selection compares
  equal to it and is answered.

Follow-ups from 4.100, all older than the move:
- `GCModifyInformationHandler` casts the wire's effect status to
  `EFFECTSTATUS` unchecked. On MSVC that enum is a signed `int`, so a value
  of 2^31 or more turns negative, passes `MCreature::AddEffectStatus`'s
  `status >= GetSize()` check and writes before `m_bEffectStatus`; GCC and
  Clang give the enum an unsigned type and reject it. Several skill
  handlers still in the executable share the cast. Now that the handler is
  in `gamemodel`, a range check can be written test-first. Separately,
  `MPlayer::AddEffectStatus` updates the UI and regeneration even when
  `MCreature` refused the status.
- `GCAddGearToInventoryHandler` leaks the item it took off the gear when
  the inventory refuses it, and the client's view then differs from the
  server's.
- The null-player guards are uneven: the HP replies check for a player,
  while the MP end, status, bonus-point and time-limit replies dereference
  the status array or gear unchecked. `GCMoveOKHandler` uses its
  `dynamic_cast<ClientPlayer*>` unchecked.
- `MPlayer::GetGear` C-casts each race's gear to `MPlayerGear*`, correct
  only because each gear class has a single base at offset 0.

Follow-ups from 4.101, all older than the move:
- `GCKnockBackHandler` has no zone check: with `g_pZone` NULL the zone
  adapter dereferences it, and the player's ID is read unguarded too.
- `GCModifyNicknameHandler` falls back to index 0 of an empty nickname
  table, whose out-of-range dummy row has a NULL string, and assigns that
  to the creature's `std::string` nickname (undefined behaviour; a loaded
  row with a NULL string does the same). `g_pNickNameStringTable` itself
  is unguarded.

Follow-ups from 4.102, all older than the move:
- **Fixed: a peer could make the client free the connection it was
  reading from.** `CR_DISCONNECT` is accepted from a peer in
  `CPS_REQUEST_SERVER_NORMAL`, which any peer that reaches the port gets:
  through `CRConnect` naming this character in game, or with no check at
  all through a guild-mark `CRRequest`, which names the peer by its address.
  The handler asks `RequestDisconnect` to drop the sender by the sender's
  own name. `RequestServerPlayerManager::Disconnect` deleted that
  `RequestServerPlayer` while its `processCommand()` was still dispatching:
  the dispatch then wrote and read the freed object, and `Update()` called
  `processOutput()` on it and advanced an erased iterator. A disconnect
  asked for during `Update()` is now queued and carried out after the
  loop; outside `Update()` it is immediate, as before, and `Disconnect()`
  takes the peer out of the list and releases its hold on the lock before
  disconnecting it. The queued drops each catch their own failure. A
  peer's own `disconnect()` now gives up output a reset or closed
  connection refuses, and logs a close the system refuses, and still ends
  the session, so `Update()`'s drop of a failed peer no longer throws a
  second time. Before, also on master, a broken peer with output queued
  stayed listed, aborted every later update at its turn and starved every
  peer behind it.
  `test_peer_self_disconnect.cpp` drives real loopback peers through the
  real factory, validator and handler; before the fix the ASan tree
  reported the heap-use-after-free and the plain tree crashed. It also
  breaks peers with output queued (a closed socket refuses the flush): a
  broken peer at the head of the list is dropped and the peer behind it is
  still served, and dropping a broken peer outside an update frees it. A
  refused close (EINTR or EIO) is not reproduced; its guard is a
  regression guard.
- `SetBloodBibleSlot` indexes `g_abHolyLandBonusSkills` (12 entries) with
  the wire's unchecked item type, follows a sign list whose length
  `BloodBibleSignInfo::read` never bounds, and uses `g_pPlayer` unguarded.
  `GCBloodBibleSignInfo` leaves `m_pInfo` uninitialised until `read`, never
  frees the info `read` allocates, and `BloodBibleSignInfo` leaves
  `m_ListSize` uninitialised for `write`.
- `GCPetInfo` never frees the `PetInfo` its `read` allocates; `SetPetInfo`
  uses `g_pZone` unguarded.
- `GCChangeWeather::read` casts any byte to `Weather` (values 0-4, no fixed
  underlying type), undefined from 8 up; `SetWeather` does not bound the
  level. `SetLightning` uses `g_pZone` and `g_pPlayer` unguarded.
- `GCAddInstalledMineToZoneHandler` stayed in the executable for its mine
  branch, which needs an `MMine`; 4.111 moves it over an item-host entry
  that marks a mine installed on the executable side.

Follow-ups from 4.103:
- An offer of a class the factory cannot make returns before either
  acceptance is withdrawn, so both sides stay accepted although the other
  side's offer changed; the test pins this as today's behaviour, and a
  change is a decision about the trade, not a refactor.
- `PacketFunction.cpp` repeated the unchecked belt-pocket `AddItem` that
  4.103's fix made `GCTradeAddItemHandler` check, so a refused pocket item
  leaked there on each packet (fixed in 4.254).

Follow-ups from 4.104, all older than the move:
- `ErrorID` has no fixed underlying type and holds 0-31, so a wire error
  byte of 32 or more cast to it is undefined. The two moved replies now
  pass the byte as the host's `int`; `LCLoginErrorHandler` and
  `PopupErrorMessage`'s adapter still cast it (the class `ITEM_CLASS` had
  until 4.103's fix).
- `UI_ShowActiveGuildList` and `UI_ShowWaitGuildList` pop every
  `GuildInfo` off the packet and never free it (4.92's member-list leak),
  and both packets' `read` leak the current `GuildInfo` if its `read`
  throws and append without clearing first.
- `GCModifyGuildMemberInfoHandler` uses `g_pUserInformation` unguarded;
  `UserInformation` leaves `GuildGrade` uninitialised, and an empty guild
  name is stored as NULL for later readers. `PopupErrorMessage` uses
  `g_pUIDialog` and `g_pGameStringTable` unguarded.

Follow-ups from 4.105, all older than the move:
- **Master-name spoofing.** Both chat replies treat any sender whose name
  starts with the master's prefix ("GM") as the master: past the ignore
  list and the curse filter. `RCSay` comes from an unauthenticated UDP
  peer, so any peer can do it at will.
- **Party-record spoofing.** `RCCharacterInfo` and `RCStatusHP` rewrite a
  named party member's address, port, guild or hit points from any
  datagram that names it; nothing authenticates the sender.
- Both refuse to run when `g_pGameMessage` is NULL although they use it
  only under `OUTPUT_DEBUG`, which nothing defines.
  `GCLearnSkillReadyHandler` uses `g_pSkillManager`, `g_pGameMessage` and
  `g_pGameStringTable` unguarded.
- With the mode now `gamemodel`'s, the login host's mode-only entries
  (`QueueWaitPCListMode`, `QueueLoginWrongMode`, `IsSelectPCMode`,
  `QueueWaitSelectPCMode`) could give way to the globals; whether they stay
  as seams is a decision.

Literal zero also requires an explicit bootstrap design: `SDLMain.cpp` and
`Client.cpp` provide platform entry points and are currently counted. This
slice adds no exemptions or changes to R1's denominator. Decide the entry-point
boundary before claiming that the whole client has reached zero.

## Phase 5 — Long tail

- [x] **5.1 Split the debug facilities** so `DebugInfo.h`/`MinTr.h` stop
  gating `packetwire` membership. The task named `SocketAPI.cpp`,
  `DatagramSocket.cpp` and `NPCInfo.cpp`; the real list was
  `tests/arch/packetwire_holdouts.txt`, and the task became "take the
  holdouts in".
  > **Status:** done (2026-09-09, fifth slice; PR #144). **Every `.cpp`
  > under `Client/Packet` is a `packetwire` member**; the holdouts file
  > lists nothing and stays only because W0 reads it and the next
  > holdout needs somewhere to be written down. The last one,
  > `RequestClientPlayerManager.cpp`, went behind nine `WireHost`
  > entries and was deleted four hours later: the review
  > of the move found that the whisper mode it served was compiled out
  > upstream (`0 &&` around every writer of the queue and every
  > whisper-mode `Connect()` - see *A reference on a live line can
  > still be dead*), and reading on from there showed the whole
  > outbound peer side - this client dialling other clients for a
  > profile or a whisper - was dead with it: `GCRequestedIPHandler`
  > began `if (bKorean == false || 1) return;`, so no peer address ever
  > arrived from the server. 5.2's seventh and eighth slices deleted the
  > lot (same PR); what the request-service family keeps in the library
  > is the **inbound** side - `RequestServerPlayer` and its manager,
  > peers dialling this client - and `WireHost` is down to **11
  > entries**, the fourth slice's clock and three file-sender calls
  > among them. The move itself is in the PR's first commit for anyone
  > who needs the seam again. **Kept from the work, both test-first:**
  > `CRWhisper::isSlayer()` compared against `RACE_VAMPIRE`;
  > `CRWhisper::write` checked every length and not the race and now
  > refuses one outside the three before writing a byte (the packet is
  > still received; `test_crwhisper.cpp`). The host installers in
  > `GameInit.cpp` and the tests are **designated initialisers**
  > (C++20); the "checked by reading" caveat the fourth slice recorded
  > is retired.
  > Done before that (PRs #63, #64, #74, #75): the logging facility
  > (`DebugLog.{h,cpp}`) lives in `basic/`, so every library may log and
  > the one object is no longer compiled into two libraries; `DebugInfo.h`
  > is one `#include "MinTr.h"` above two no-op macros and nothing in the
  > wire layer includes it. **`Client/Packet/WireHost.h`** declares what
  > the wire layer asks of the program around it — the three
  > `ClientConfig` tuning values, the millisecond clock, the encrypt-seed
  > inputs (zone id, server number, region flags), and three calls on the
  > peer file-transfer manager (six, and an in-game test, until the
  > eighth slice), which stays executable-side because it writes the
  > profile directory and reads the UI — and the
  > executable fills it in beside the other two hosts in `GameInit`; every
  > tuning accessor answers without a host with the value `ClientConfig`'s own
  > constructor sets, and `WIRE_DEFAULT_*` keeps the executable's
  > fallbacks from drifting. `SendBugReport` is `WireHost.cpp`'s second
  > half. `setEncryptCode()` **is live code** (`Encrypter.h` defines
  > `__USE_ENCRYPTER__`; `GCUpdateInfoHandler` calls it on every login),
  > and `WireEncryptSeed`'s four region branches collapse to two distinct
  > expressions, which a test asserts across every server number.
  > Fixed test-first: `Player`'s socket constructor never set
  > `pHashTable`, which `delKey()` (two live callers, both on every
  > reconnect) `delete[]`s — both constructors set it, the destructor
  > frees it, `setKey` frees the previous table; `Wire::ReceiveMyRequest`
  > and `SendOtherRequest` no longer declare `throw ()` over a manager
  > that throws by design. Seven `OUTPUT_DEBUG` blocks and `ProcessMode`'s
  > commented-out body were deleted rather than moved, because R4 counts
  > a `g_p*` name on a dead line as readily as on a live one.
  > **Was untested by construction** until the fifth slice's review: the
  > host installation in `GameInit.cpp` was positional, so transposing
  > two same-signature entries in `s_WireHost` compiled and passed the
  > whole suite; it is designated now, and the forwarder test proves
  > that each `WireHost.cpp` forwarder calls its matching member. **Known, not fixed:** `SocketImpl`'s default
  > constructor is the only one of its four that leaves `m_key` unset;
  > `Player::processCommand`, `processInput`, `processOutput`,
  > `sendPacket`, `disconnect` and `toString` dereference the socket or a
  > stream the default constructor leaves NULL.
  - Owner: W0 over the (empty) holdouts file, so a new `Client/Packet`
    source is a library member unless a line there says why not;
    `test_wire_host.cpp`, `test_player_base.cpp` and their
    address-taking link proofs (non-virtual members — see *What the
    review rounds settled*).

- [x] **5.2 Dead/duplicate source removal** (code-health priority 3).
  > **Status:** done for what the task named (PRs #49, #50, #52, #62).
  > Deleted: the tracked `_bak` item table and the two other `InitItem2`
  > twins (one of which the Windows `VS_UI` target was still compiling);
  > the server's 23,000-line `__INIT_ITEM__` item data and 8,500-line
  > `MitemTableInit.cpp`; the 2,300-line `#ifndef __GAME_CLIENT__` skill
  > data in `MSkillInfoTable.cpp`; and the exclusion graveyard whole
  > (`GlobalVariables`, `MissingGlobals`, `GameHelpers`, `GameFunctions`,
  > `GamePacketFunctions`, `ActionFunctions`, the pre-port `GCNotifyWin`
  > trio) after a scan of all compiled translation units showed none of
  > its 171 function definitions was the only one. The client has always
  > loaded `Item.inf` and `SkillInfo.inf`; git history keeps the data.
  > Closes the review's Medium dead-code finding.
  > **Sixth slice (2026-09-09):** deleted `VS_UI/WinMain.cpp` (3,799
  > lines; a second `WinMain` that drove the widget system without ever
  > calling `InitGame()`, excluded from every configuration by two
  > `list(FILTER)` rules that went with it, and named by the ratchet
  > script's R4 member list, which no longer needs to) and the four
  > files of `Client/OtherClass/` (two request-side packet factory
  > managers, 1,700 lines, in no glob and referenced by nothing; the
  > R12 conformance slice had cleaned 32 exception specifications in
  > files that were never compiled). R1 and R4 unchanged, because
  > neither was ever built - the build is the owner, as before. The two
  > `^Client/`-anchored filters the earlier list named were already
  > gone.
  > **Candidates for a next slice:** none; the list closed 2026-09-16.
  > `__UPDATE_CLIENT__` and `__EXPO_CLIENT__`, the two spellings outside
  > R15's five that guarded dead code the same way, are the eleventh
  > slice below. `MSkillDomain::SaveToFile`/`LoadFromFile` stay, decided
  > rather than deleted: the template question resolves the other way -
  > `CTypeTable<MSkillDomain>`'s own file I/O members are never
  > instantiated (nothing calls them on `MSkillManager`), so the
  > template does not reach the pair, and its callers are the ten sites
  > in `tests/unit/test_skill_core.cpp` that pin the domain's
  > serialisation: the round trip, the truncated-file and bad-status
  > guards, and the reload over a live domain. It is the domain's one
  > persistence format and a
  > tested library contract; deleting that to shorten a dead-code list
  > is the wrong trade. (The packet-source conditionals and the residue
  > files this list once named are the ninth slice below, the remaining
  > conditionals and the macro itself the tenth; the peer-to-peer
  > whisper path is the seventh.)
  > **Seventh slice (2026-09-09, on PR #144, at the user's request):**
  > the peer-to-peer whisper path, which upstream compiled out (`0 &&`
  > around every writer of `WhisperManager`'s queue and every
  > whisper-mode `Connect()`) and 5.1's fifth slice found. Deleted:
  > `WhisperManager.{h,cpp}` whole (its one live member sent a
  > `CGWhisper`, which `UIMessageManager` now sends itself), its
  > construction, deletion and `Update` calls, the whisper `case` in
  > `ProcessMode` and in the connection thread's failure path, the
  > whisper branches of `GCRequestedIPHandler` and
  > `GCRequestFailedHandler`, and seven `WireHost` entries (the
  > character's world and race, the four queue calls, the request-user
  > notification) with their forwarders, installers and tests. R1
  > 488 → 487. `REQUEST_CLIENT_MODE_WHISPER` stays in its enum (the
  > receiving side still names the mode; the values are shared
  > vocabulary - `REQUESTING_FOR_WHISPER` went with its enum in the
  > eighth slice), as do
  > `CRWhisper` and its handler (a Korean-build peer can still whisper
  > *to* this client, and `tests/wire-layout.txt` pins the packet).
  > **Found on the way:** `GCRequestedIPHandler::execute` begins
  > `if (bKorean == false || 1) return;`, so the reply to a
  > `CGRequestIP` is never acted on and a peer's address is learned
  > only from a `CRWhisper` it sends us - the profile fetch, the one
  > peer path left, can dial only a peer that has already whispered.
  > Whether that is worth keeping, or the whole outbound peer side
  > should follow the whisper path, was put to the user, who said
  > remove it.
  > **Eighth slice (2026-09-09, on PR #144):** the rest of the
  > outbound peer side. Deleted: `RequestClientPlayer.{h,cpp}` and
  > `RequestClientPlayerManager.{h,cpp}` from `packetwire` (526 → 524
  > members); the three handlers that took that player
  > (`RCConnectVerify`, `RCRequestVerify`, `RCRequestedFile`) and their
  > registry lines (R1 487 → 484; their handler classes stay declared
  > in the packet headers like every other handler-less packet's); the
  > receive half of `RequestFileManager` (`ReceiveFileInfo`,
  > `RequestReceiveInfo`, the "my request" map and its four calls, the
  > empty `Update`); the dialling half of `ProfileManager`
  > (`RequestProfile`, the require map, `Update`, `CGRequestIP`) and its
  > three callers in `MPlayer`, `GCPartyJoinedHandler` and the
  > `/profile` chat command - the map now holds what `InitProfiles`
  > finds in the profile directory, which is what the three UI readers
  > and the inbound file sender read; the requesting half of
  > `RequestUserManager` (the second map, `REQUESTING_FOR`,
  > `RemoveRequestUser`, `RemoveRequestUserLater`, `Update`) and the
  > `Status`/`TCPPort` fields nothing read - what stays is the address
  > book `CRWhisperHandler` and the three UDP party handlers write and
  > the party datagrams read for their port, inert in a fleet built from
  > this source (nothing here creates an entry); the "no profile"
  > marker (`AddProfileNULL`/`HasProfileNULL`), whose only writer was a
  > deleted handler, and the `VS_UI_GameCommon.cpp` branch that cached
  > it; `RequestConnect`; six `WireHost` entries (`InGameMode`, the
  > three "my request" calls, and the fifth slice's two) with
  > forwarders, installers and tests; the per-frame `Update` calls of
  > all four managers in `GameMain` (`RequestFileManager`'s was empty).
  > **The commit's first message said nothing this deletes could run.
  > That was wrong**, and the review said so: `ProfileManager::Update`
  > ran every ~330 ms and, for a requested name with no known address,
  > sent `CGRequestIP` to the game server on every turn - `bKorean` is
  > true by default - and against a server answering `GCRequestFailed`
  > that was a 3 Hz re-request loop per name, since only a profile
  > arriving cleared the request; and a peer that had whispered to this
  > client first (an original Korean client) could be dialled for its
  > profile. What changes on the wire: **this client no longer sends
  > `CGRequestIP`**, and no longer dials anyone.
  > `GCRequestedIPHandler` and `GCRequestFailedHandler` are explicit
  > no-ops rather than deregistered, because an unregistered id throws
  > `InvalidProtocolException` on the *game server* connection.
  > `RequestDisconnect` stays (a peer's `CRDisconnect` reaches it) and
  > its log line printed the name with `%d`. Kept on purpose: the enum
  > values, `CRWhisper`/`CRConnect`/`CRRequest` and the `RC*` packet
  > classes (wire layout pinned), the four `RC*` handlers the UDP party
  > channel receives (`RCPositionInfo`, `RCCharacterInfo`, `RCStatusHP`,
  > `RCSay`), and the inbound side whole. Owner: the build, W0 over the
  > membership file, and the profile UI in `VS_UI_GameCommon.cpp`
  > reading only what `InitProfiles` provides - which a live server
  > shows as one's own profile image still displaying, and a packet
  > capture as no `CGRequestIP` leaving the client.
  > **Ninth slice (2026-09-13):** the first two candidates above.
  > `__GAME_CLIENT__` reaches every translation unit that reads it
  > (CMake passes `=1` to the executable, `packetwire`, `gamemodel`
  > and the test sources that share their definitions; every other
  > reader includes `Client_PCH.h`, which defines it) and the four
  > server macros are defined nowhere, so every conditional on them
  > has one value; a scripted evaluation (whole-line deletion only, so
  > the tree's mixed CRLF/LF files kept their bytes) took 190 of them
  > out of 156 `Client/Packet` files - the `#ifndef __GAME_CLIENT__`
  > handler declarations at the foot of 133 of the 163 `Cpackets`
  > headers, the `#ifdef __GAME_SERVER__` includes of server headers
  > this repo does not have, the login- and game-server packet sets in
  > `PacketValidator` and the server player states in `PlayerStatus`,
  > the server-side factory list in `PacketFactoryManager` - and the
  > four files outside it the list named (`RankBonusTable`'s save
  > path and editor constructor, `MSectorInfo.h`'s portal fields,
  > `Updater/Update.cpp`'s assert stub): some 3,000 lines, the
  > banners that headed a deleted block swept with them. Two sites
  > mixed a known macro with `__DEBUG_OUTPUT__` (`Packet.h`'s
  > `getPacketName`/`toString` pair and the manager's `getPacketName`)
  > and reduce to `#ifdef __DEBUG_OUTPUT__`, which nothing defines;
  > the test that mirrors the pair's condition spells it the same way
  > now. No wire change: the goldens and the layout inventory are
  > untouched. **Found by the review:** two test sources
  > (`test_player_base.cpp`, `test_wire_host.cpp`) had never been on
  > the list that gives a test the library's definitions, so for as
  > long as that conditional existed they compiled a `Packet` with two
  > more pure virtuals than the library's - the pair above - and an
  > empty `PlayerStatus` enum; removing the conditional made the views
  > agree, and the two are on the list now for the Windows wire macros
  > they still lacked. Also the review's: four handler banners and
  > three `//#ifdef __DEBUG_OUTPUT__` openers the sweep had left, the
  > `#ifndef __GAME_CLIEMT__` (sic) that hid `SaveToFile`'s
  > declaration from the script and from R15, the commented-out
  > server includes in `CGSay.h` and `CGVerifyTime.h`, and the wrong
  > numbers in the first version of this entry. What remains reads
  > the macros only outside `Client/Packet`: 364 tokens - always-true
  > `#ifdef __GAME_CLIENT__` wrappers in 237 of the 280
  > `Client/PacketHandler` files and in 19 files directly under
  > `Client/`, ten always-false server guards in five of those
  > handlers, and `Client_PCH.h`'s own definition, which goes together
  > with the CMake ones when its two tokens are all that is left.
  > **Tenth slice (2026-09-13):** that remainder, and the macro with
  > it. The same script, restricted this time to expressions that
  > mention one of the five macros (its first version would also have
  > resolved `#if 0`, which the packet tree happened not to contain),
  > evaluated the 363 conditionals in 256 files outside `Client/Packet`
  > (two of them reduced rather than deleted, below):
  > the always-true wrappers around the handler bodies and the
  > `Client/` sources, the editor `#else` halves under them (the
  > sound path, the stubs and the alpha arithmetic in
  > `ClientFunction.cpp`), the ten
  > always-false server guards, and `Client_PCH.h`'s own definition;
  > about 1,100 lines. Two sites mixed the macro with `OUTPUT_DEBUG`
  > (`CMessageArray.cpp`'s lock, `MHelpManager.h`'s help-event
  > macros) and reduce to `#ifdef OUTPUT_DEBUG`. Then the definition
  > left `CMakeLists.txt` (three targets) and `tests/CMakeLists.txt`
  > in the same commit, so no translation unit anywhere defines
  > `__GAME_CLIENT__` now - which changes nothing, because nothing
  > reads it: R15 says so on both sides of the packet tree, at 0 each,
  > both file counts pinned, and a third count over the CMake files
  > holds the definition out. The include checker evaluates the five
  > as undefined, which they are. What is left of the family is
  > comments - a `#ifdef` inside a `/* */` block in
  > `LCReconnectHandler.cpp` and in `UIMessageManager.cpp`,
  > commented-out functions in `ClientFunction.cpp` and `MItemUse.cpp`,
  > historical notes in `MSkillInfoTable.cpp`, `PacketHandlerRegistry.cpp`,
  > the two precompiled headers and this file - and, outside the five,
  > `__UPDATE_CLIENT__` in one handler and `__EXPO_CLIENT__` at ten
  > sites, dead the same way and not measured until the eleventh slice
  > took them (`__GUILD_MANAGER_TOOL__`
  > went with this slice: its three sites were `defined(__GAME_CLIENT__)
  > || defined(__GUILD_MANAGER_TOOL__)`). `PlaySound`, `GetWhisperID`,
  > `IsPlayerInSafePosition` and `IsPlayerInSafeZone` had editor-side
  > definitions in `ClientFunction.cpp` behind
  > `!defined(__GAME_CLIENT__)`; the client's own - `PlaySound` in
  > `GameMain.cpp`, the other three in `ClientFunction.cpp`'s client
  > half - are what it always compiled. The script collapses one blank
  > on each side of a hole it made (nine files lost a blank line beyond
  > the directives for that reason) and leaves runs it did not create,
  > so about a hundred handler files now hold three or more blank lines
  > where a wrapper stood between two; deletions only, either way.
  > **Eleventh slice (2026-09-16):** the two spellings the list above
  > named, and the list with them. `__EXPO_CLIENT__` (ten live sites,
  > one commented out) and `__UPDATE_CLIENT__` (one) are defined
  > nowhere - not in CMake, not in a source, not in a preset - so each
  > conditional on them had one value in every translation unit, like
  > the five before them. Deleted by hand, whole lines only (197 lines
  > in nine files, bytes otherwise untouched): the expo build's addon
  > enum in `AddonDef.h` (the `#else` half, the one always compiled,
  > stays); the `__FULLSCREEN_MODE__` definition in `MViewDef.h` and,
  > since that was the macro's only definition, the fullscreen sector
  > geometry it selected (the `#else` half stays, as before); the
  > `g_UserInformation.Invisible` walk-through-anything clause in the
  > two path finders (`MFakeCreature.cpp`, `MPlayer.cpp` twice) and in
  > `MPlayer`'s move check; the expo `#else` half of the skill-to-object
  > send in `MPlayer.cpp`, which sent `BOMB_TWISTER` to a tile; the
  > early `return` in the three weather handlers (`GCChangeDarkLight`,
  > `GCChangeWeather`, `GCLightning`); the two includes
  > `UCRequestLoginModeHandler.cpp` wanted only in an update client;
  > and the commented-out `//#ifdef __EXPO_CLIENT__` opener in
  > `CGameUpdate.cpp` with the note above it. 13 live tokens to 0. R15's
  > pattern names nine macros now - the five, these two,
  > `__FULLSCREEN_MODE__` and `__GUILD_MANAGER_TOOL__` (0 since the
  > tenth slice) - and the include checker evaluates all nine as
  > undefined. No wire change.
  - Owner: the build (nothing deleted was compiled, so R1 held at each
    step); the wrong-file-edited trap is closed for the files named;
    **R15** for the ninth, tenth and eleventh slices - the nine macros
    as live tokens under `Client/Packet` and outside it, 0 and 0, and
    their definitions in the CMake files, 0.

- [x] **5.3 TextSystem stub retirement.** Split `TextService.cpp`'s pure
  text utilities from its `g_pLast` drawing entry point.
  > **Status:** done (2026-09-03, PR #65). `TextService::RenderText` — a
  > compatibility shim for `SDL_RenderText` that draws through `g_pLast`
  > — is defined in `Client/TextServiceScreen.cpp`, which the executable
  > compiles (until task 4.208, which gave it the surface as a parameter
  > and folded it into TextSystem); `TextService` has no virtuals, so the split costs no
  > vtable. A split rather than a host, because the thing behind this
  > seam is the drawing surface itself: a host would move the reach, not
  > remove it. **`tests/stubs/` is gone**; `tests/CMakeLists.txt` says
  > why in its place: a library that needs something from the program
  > around it asks through a host struct a test can install, never
  > through a symbol a test binary has to invent. `unit_tests` links
  > `TextSystem` with no stub translation unit — that link is the proof.
  > Whether the client still draws its FPS counter and debug overlays is
  > what running it shows.
  > **Input follow-up (2026-09-18):** `unit_tests` also links `dxlib`.
  > `DXInput::Host` carries six application callbacks (position, activation,
  > focus and text delivery), designated-initialized by `InitInput`; the
  > backend keeps SDL processing and key mapping. No test supplies `g_x`,
  > `g_y`, `g_bActiveApp` or a fake text editor. Ten adapter tests exercise
  > the real event dispatcher, callback ordering, wheel consumption and key
  > bounds; a queued key also verifies the polling loop. Wheel/text fixtures
  > enter after polling because SDL2 compatibility libraries can discard or
  > reject synthetic versions of those events during SDL3 conversion.
  > The strict UTF-8 decoder/validator is shared from `basic/TextUtf8.h`,
  > so resource codecs and byte-boundary helpers need no rendering dependency.
  > `test_text_utf8.cpp` owns its scalar validation and truncated-input rules.
  > **Text-cache follow-up (2026-09-21):** normalization retains at most
  > 1,024 entries and 1 MiB of owned string capacity per calling thread;
  > oversized text bypasses the cache. Glyph metrics have an independent
  > 4,096-entry bound and are shared across bitmap colors. Diagnostic counters
  > expose actual normalization and metric work.
  > **Resource-codec follow-up (2026-09-21):** `MString` now converts declared
  > resource bytes to UTF-8 on load on every platform and encodes saves back
  > to the configured page. `RESOURCE_TEXT_ENCODING` in `FileDef.inf` defaults
  > to CP949 for the shipped pack. `basic/TextEncoding` also supplies the
  > renderer's converter, with checked capacities and scoped iconv ownership.
  > Damaged resource bytes become replacement characters without losing later
  > text. Strict saves reject unrepresentable text before writing the prefix.
  > `test_mstring_files.cpp` and `test_text_encoding.cpp` cover both directions,
  > embedded NULs, record bounds and encoding selection. The renderer still
  > guesses the encoding of legacy direct callers; migrating those ingress
  > paths and retiring that fallback remain part of the encoding finding.
  > **XML ingress preparation (2026-09-21):** the corrected
  > `Client/SXml/SXml.cpp` moves byte-for-byte into `VS_UI`, replacing its
  > older duplicate. A compatibility header shares one declaration. All
  > platforms now link that parser from the library, and `test_xml.cpp`
  > now reproduces and guards parser bounds, truncated documents, repeated
  > quest names and UTF-8 save/load round trips. `ResourceText` decodes XML
  > declarations/BOMs before the pack default. The parser builds temporary
  > trees before appending, with per-document size/depth/node limits; owned
  > strings replace fixed stack buffers. Wide conversion queries its UTF-8
  > capacity and XML saves escape markup. `CRarFile::OpenText` provides a
  > bounded, once-per-file decoding path and UTF-8-safe line clipping;
  > `test_resource_reader.cpp` owns its bounds, cursor and buffer-lifetime
  > contracts. Packed RAR/RPK fallback and archive enumeration are restored
  > through `basic/RarArchive`, backed by a pinned static UnRAR dependency.
  > Reads retain loose overrides, passwords and raw/text separation; archive
  > members never go to disk. `test_rar_archive.cpp` owns malformed input,
  > limits, solid encryption and Unicode path contracts. Dependency notices
  > accompany built binaries and the macOS package.
  > Common/race chat tips and welcome/event popups now use that
  > loader; popups test the open result before reading. The subsequent plain-text
  > caller migration and renderer fallback retirement are recorded under
  > the encoding finding.
  > Notice mail templates now open before reading and use `basic/MailTemplate`
  > for four complete decoded rows and viewport-bounded geometry. Popup drawing
  > consumes only visible rows. `basic/MasterCommands` loads complete command
  > files with scoped handles, strictly decodes their declared page/BOM and
  > validates an ordered plan before dispatch. Cycles, excessive nesting/bytes/
  > rows, missing files and commands above the packet allowance reject the whole
  > plan. Local `*mc N` selectors are consumed rather than sent to the server.
  > `test_mail_template.cpp` and `test_master_commands.cpp` own these parser,
  > encoding, lifetime and failure contracts; UI dispatch remains a regression
  > guard. The 2026-09-23 ingress audit and renderer change below complete
  > retirement of the fallback.
  > **Renderer UTF-8 contract (2026-09-23):** resources, XML, SDL editor input,
  > source literals and paired-server text now have explicit ingress contracts;
  > see [the source and installed-XML audit](text-ingress-audit-2026-09-23.md).
  > Normalization preserves valid UTF-8 and replaces each malformed byte with
  > U+FFFD, without selecting a legacy code page. The bounded cache remains.
  > `test_textservice_normalize.cpp` and `test_text_service.cpp` own replacement,
  > suffix preservation, idempotence, explicit resource decoding and cache
  > independence. The malformed optional ghost-position asset is documented
  > separately; it does not supply displayed text.
  > The obsolete `g_PossibleStringCut` declaration and SDL definition are now
  > removed after all compiled callers migrated. Shared UTF-8 prefix and row
  > helpers, descriptor/help layout tests and builds without the old symbol
  > own this contract; the excluded native GDI source retains its old code.
  > Help messages also load through the shared decoder. Their implementation
  > moved unchanged into `VS_UI` before the parser fix. `test_help_messages.cpp`
  > owns complete-record publication, numeric/race bounds, default singleton
  > loading, long legacy-encoded fields and UTF-8 saves with a BOM.
  > `MHelpMessage::IsEligible` also owns the mailbox's race-specific ranges:
  > inclusive Slayer attribute bounds, vampire/ouster level bounds, and a
  > disabled interval when its lower bound is -1. Four further tests guard
  > those rules and invalid races; the mailbox widens its attribute sum before
  > calling it and indexes message arrays only after eligibility succeeds.
  > `TextUtf8.h` also owns the scalar-boundary prefix operation used by resource
  > line clipping and all three in-place string reducers. Their byte/storage
  > bounds and valid UTF-8 output are tested independently. Findings 116/135
  > are closed by the caller migrations below and retirement of the old
  > wrapping predicate; the shared declaration and compiled implementation
  > are gone.
  > Multiline tooltip sizing and drawing now share `basic/TextWrap` rows:
  > complete UTF-8 scalars, explicit newlines and progress at narrow columns,
  > with owned strings instead of temporary writes into borrowed text. Width
  > comes from the widest actual row. `test_text_wrap.cpp` owns splitting and
  > lifetime contracts; the two game-global UI callbacks remain regression
  > guards. Creature chat and personal-shop signs also use the splitter,
  > with explicit policies preserving chat spaces and ordinary-chat newlines.
  > The row writer clips decorated tree names on scalar boundaries within
  > their allocated capacity. Personal-shop signs accept read-only input.
  > Dialog, quest, lottery, billing and team-introduction rows also use the
  > shared splitter. `NextUtf8Line` reports exact consumption for a narrower
  > first row, with bounded searches, leading-space and escaped-newline
  > policies. Their fixed scratch buffers are gone; zero glyph widths and
  > narrow columns still advance safely. The unused personal-shop text loop
  > is removed.
  > Rich-help attribute lookup now uses `basic/HelpMarkup` during layout.
  > `test_help_markup.cpp` owns exact-name matching, bounded
  > quoted values and result lifetime; missing quotes no longer reach pointer
  > arithmetic or a fixed scratch array. `basic/HelpLayout` owns typed text
  > runs, colors, image placements and displayed row counts; `test_help_layout.cpp`
  > covers scalar-safe progress, append mode, image geometry, limits and atomic
  > publication. `VS_UI/HelpImageCache` owns decoded RGB565 surfaces with pixel,
  > dimension and entry limits. `test_help_image_cache.cpp` covers corrupt/missing
  > files, real JPEG decoding, channel conversion, cache reuse and failed loads.
  > The manifest enables SDL_image's JPEG feature. The game-global help window
  > renders the same rows it scrolls and clips images without changing the prior
  > clip; its integration remains a regression guard. Images share the text row
  > origin, including centered art. The uncalled substitution API is removed.
  > The old cut predicate's compiled callers, definition and declaration are
  > retired under findings 116/135.
  > Descriptors now share `basic/DescriptorText` for bounded, owned UTF-8
  > rows, title extraction, header/image spacing and checked resource indices.
  > File input uses `OpenText`; string input keeps resource tags as plain text.
  > Candidate rows and file-created icon packs publish together after validation;
  > caller-supplied packs and explicit titles retain their separate ownership.
  > `test_descriptor_text.cpp` owns the layout contracts. Game-global loading
  > and drawing remain regression guards, including viewport/surface clipping.
  > `test_ctypepack_indexed.cpp` directly exercises both indexed pack loaders:
  > incomplete index records and invalid offsets reject before decoding, and
  > element decoder failures propagate to callers.
  - Owner: the `unit_tests` link line, `test_textservice_normalize.cpp` and
    `test_glyph_cache.cpp` for repeated work, ownership, eviction and bounds.

- [x] **5.4 Format-string audit** (code-health C19/C20/C22: `sprintf`
  sites whose format is a `Data/Info/String.inf` entry).
  > **Status:** done (2026-09-03/04, PRs #68 through #73); **finding C19
  > is closed**, and its entry in the code-health review lists the five
  > measurements it rests on (R7, R8, the arity audit, and two hand
  > sweeps) rather than a ratchet reading zero. **315 call sites** are
  > converted across `Client`, `VS_UI` and `Client/PacketHandler`.
  > **`basic/SafeFormat.{h,cpp}`** is the checked formatter: a conversion
  > consumes the next argument only if that argument's type can satisfy
  > it; a conversion with no argument left, an argument of the wrong
  > type, a `%n`, or a width taken from an argument is copied out as
  > text instead of performed, so `"[System]%s%s%s%s%n"` against one
  > argument prints `[System]hello%s%s%s%n` rather than reading three
  > stack words. The destination is bounded by its own size, and the
  > array overload takes that size from the declaration. It is in
  > `basic` because the call sites are in the executable, `VS_UI` and
  > the packet handlers, and `basic` is the one library all three link.
  > Front ends: `SafeFormat::Format`, `CMessageArray::AddSafeFormat` (a
  > new entry point, because `AddFormat`'s other 18 callers take literal
  > formats; the two share `StoreRow`), `MString::FormatChecked` (library
  > code with tests — `MString::Format` is a varargs printf reached as a
  > method, which no sweep over printf names could see), and
  > `AllocAskMessage` in `VS_UI_ExtraDialog.cpp`, which allocates and
  > formats in one place so the bound cannot drift from the destination.
  > `GetGameString()` answers `""` for an out-of-range lookup rather than
  > the NULL a default-constructed `MString` gives.
  > **Message-ring follow-up (2026-09-18):** `CMessageArray` moved into
  > `basic`, retaining its Client compatibility header, before its remaining
  > lifetime/index defects were fixed. Eight tests link its real file I/O and
  > every formatting path, including a 300-row ring, empty/released storage,
  > failed opens and filename aliasing across reinitialization. R1 falls to
  > 472 Windows / 470 Ninja and R17 to 966; no game-global stubs are needed.
  > **String-reduction follow-up (2026-09-18):** the three production
  > `ReduceString` functions moved into `basic/StringReduction.cpp` with
  > unchanged bodies and four direct memory-bound regression guards. R1 is
  > now 471 Windows / 469 Ninja. The duplicate memory-safety finding is
  > closed; the character-boundary findings remain separate work.
  > **The load-time half is separate:** `SanitizeGameStringTable` scrubs
  > `String.inf` as it is read, cannot check arity, and does not run in
  > the default English build. No `String.inf` ships in this repository,
  > so for `LANGUAGE != 3` none of these entries can be checked here; the
  > formatter's run-time refusal is what protects that build.
  > **Defects fixed on the way:** `GCBloodBibleListHandler` formatted
  > `"%3d %s"` into a buffer four bytes too small; `VS_UI_GameCommon.cpp`
  > formatted two `BYTE` coordinates into a `static char[10]` (needs 12);
  > `VS_UI_ExtraDialog.cpp` appended slayer requirement lines with an
  > unbounded `wsprintf(sz_temp + strlen(sz_temp), …)` into a
  > `char[200]`, and its ask dialogs `sprintf`'d table entries carrying
  > `%s` with **no varargs at all** into `new char[strlen(format)+1]`;
  > `C_VS_UI_ASK_DIALOG` never assigned six `ASK_FRIEND_*` rows of
  > `m_sz_question_msg` (upstream commented the assignments out and left
  > the readers live), so opening any friend dialog handed an
  > indeterminate pointer to `sprintf` as a format — server-triggered
  > through `GCFriendChatting`; `MString::operator=` keeps no allocation
  > for an empty string, so `GetString()` came back NULL into
  > `g_pSystemMessage->Add`; `ModifyStatusManager.cpp` built an inner
  > `sprintf` from a table entry and handed the result to a converted
  > outer call (the two-stage shape of C22 — a sweep for it now returns
  > nothing).
  > **Recorded, not fixed:** `C_VS_UI_INFO::GetChinhoLevel` caches its
  > eleven table entries in a function-local `const static char*[11]`
  > while `InitGameStringTable()` runs at least twice per session and
  > deletes every `MString` — stale by construction if populated before
  > the last init; today both inits finish before any in-game UI runs. A
  > NULL entry there now yields `""` and the write is bounded, but a
  > dangling non-NULL pointer is indistinguishable from a live one.
  > **Thirty-nine** table lookups with a computed subscript remain (15
  > in `Client`, 24 in `VS_UI`, counting every subscript that is not a
  > bare identifier). Latent in the instruments: `SafeFormat::FormatV`
  > used directly at a call site is invisible to the audit and R7 (its
  > only use is inside `CMessageArray`); R7's comment stripping is not
  > string-literal aware where the audit's is; the audit's parenthesis
  > matcher is not literal-aware where its argument splitter is.
  - Owner: R7 and R8 (the pair); `check_format_arity.pl` with both its
    floors; `test_safe_format.cpp`, the `FormatChecked` tests.

---

- [x] **5.5 Sprite-pack input and rejection diagnostics.**
  > **Status:** done (2026-09-22). Both `CTypePack` templates validate complete
  > matching counts and in-file offsets before replacing lazy-load state.
  > Rejected entries are attempted once per load; revisiting one cannot close
  > the file before other entries load. Eager and indexed loads propagate
  > decoder/stream failure and log the source, entry and offset where available.
  > Empty loads clear prior state; invalid index reloads preserve it.
  - Owner: `test_ctypepack_indexed.cpp`, real sprite and template instantiations,
    plus full build, architecture and sanitizer checks. Broader asset-format
    auditing remains in the code-health review's caveats.

- [x] **5.6 Pixel-sprite input validation.**
  > **Status:** done (2026-09-22). `CSprite555` and `CSprite565` share the
  > checked `CSprite::LoadPixels` implementation. Header and row reads must
  > complete; encoded runs and cumulative pixel widths are validated before
  > publication. A shipped one-column width discrepancy is normalized into the
  > reported width; larger overruns and 16-bit width overflow are rejected.
  > Complete malformed records consume every row before rejection,
  > keeping the next packed sprite readable. Only color words convert to 555;
  > padding survives. Truncation/exception paths clear data and loading state.
  - Owner: `test_pixel_sprite_loading.cpp` and the existing 555 cursor tests,
    exercised with the actual SpriteLib implementations under ASan.

- [x] **5.7 Shadow-sprite input validation.**
  > **Status:** done (2026-09-22). `CShadowSprite` checks complete headers and
  > rows, encoded pair counts and cumulative width before publishing data.
  > Complete rejected records retain the next packed-record cursor; truncated
  > or throwing reads release pending rows and leave an empty object. Explicit
  > release also resets complete empty-header state. Row padding is preserved.
  - Owner: `test_shadow_sprite_loading.cpp`, exercising the actual loader,
    drawing, maximum lengths, reloads and release under ASan.

- [x] **5.8 Shadow-sprite copy ownership.**
  > **Status:** done (2026-09-22). Copy construction and assignment own their
  > rows independently, preserve self-assignment and empty initialized records,
  > and start without a borrowed backend cache. Loaded and generated rows retain
  > allocation spans; copies validate those spans before reading, preserve
  > padding, and reject corrupted source rows. Pending copies use scoped owners.
  - Owner: `test_shadow_sprite_loading.cpp`, covering independent destruction,
    real backend caches, all three generated forms and malformed source rows.

- [x] **5.9 Shadow-sprite adapter bounds.**
  > **Status:** done (2026-09-22). The SDL shadow adapter validates retained
  > row spans before creating or refreshing a backend sprite. Empty geometry
  > and rasters beyond the backend's signed pixel-count limit are rejected
  > before allocation. Raster multiplication and row strides use `size_t`.
  - Owner: the `ShadowSpriteAdapter` cases in `test_shadow_sprite_loading.cpp`,
    covering the reproduced overflow, dirty caches and geometry boundaries.

- [x] **5.10 Effect-shadow resource loading.**
  > **Status:** done (2026-09-23). `MTopView::InitSprites` loads the small
  > effect-shadow pack eagerly through the checked generic pack loader, matching
  > the effect viewer. It reports and propagates failure instead of depending
  > on the installed index's unsupported wrapper. Other packs retain lazy loads.
  - Owner: `ShadowSpriteLoading.EagerPackReadsAllRecordsWithoutTheLegacyIndex`,
    the production-loader asset audit and full builds for the game-side wiring.

- [x] **5.11 Light-filter pack input validation.**
  > **Status:** done (2026-09-23). `CFilterPack` checks complete counts and
  > filter records before replacing the current pack; invalid reloads preserve
  > all prior filters. An explicit empty pack clears storage. Pending filter
  > rows remain releasable if allocation throws. Game initialization reports
  > and propagates rejected or empty light packs, and both light draw paths
  > skip empty packs before calculating an index.
  - Owner: the `CFilterPack` cases in `test_cfilter.cpp`; full builds cover the
    game-side connection. The installed `Light2D.ftp` passes the production
    loader under ASan with all 12 records consumed and no trailing bytes.

- [x] **5.12 Specialized alpha-pack input and ownership.**
  > **Status:** done (2026-09-23). `CAlphaSpritePack` owns and indexes typed
  > 555/565 arrays and cannot be shallow-copied. Full reloads validate complete
  > records before publication. Partial loads validate the whole destination
  > range first, report rejected rows and retain earlier complete rows. Indexed
  > reads check both file headers, IDs and offsets and rewind a reused index.
  > Empty initialization releases prior storage; invalid element access throws.
  - Owner: `test_alpha_sprite_pack.cpp`, linking the real library API in both
    formats. This specialized pack has no active game construction; the live
    effect-alpha resource uses `CAlphaSpritePalPack` instead.

- [x] **5.13 File-index table input validation.**
  > **Status:** done (2026-09-23). `CFileIndexTable` stages complete little-endian
  > counts and signed 32-bit offsets before publication. Rejected reloads keep
  > the previous table; copies own their storage and lookup checks the full ID.
  > Title loading publishes only a successfully decoded sprite. The disconnect
  > screen retains three owned empty sprites when an index or record fails and
  > continues its existing shutdown flow, with source/ID diagnostics.
  - Owner: nine `test_file_index_table.cpp` cases, plus existing production
    sprite-reader tests. The installed `UI.spk` passes the changed index reader
    and both pixel decoders for all six indexed records under ASan. Game-side
    wiring uses the executable exemption and full builds.

- [x] **5.14 Game sprite-pack failure propagation.**
  > **Status:** done (2026-09-23). `MTopView::InitSprites` checks all 26 active
  > lazy sprite/palette opens and propagates rejected indexes. Eager screen
  > palettes, effect shadows, miscellaneous sprites and weather sprites clear
  > partially loaded packs after rejection. The Ousters ending stops before
  > drawing unless its pack loads and contains all eleven required sprites.
  > Empty miscellaneous/weather packs also fail initialization; clearing the
  > failed pack permits later retries through the existing size check.
  > Startup and display restoration stop through existing cleanup/quit paths
  > on view failure. The tile renderer is discarded before replacing its
  > borrowed surface, preventing stale references and leaks during retry.
  - Owner: existing `CTypePack` and concrete decoder tests; these game-coupled
    branches use the executable exemption, full builds and a comment-aware
    caller audit. Offline ASan validation covers the production loading paths,
    including both effect palettes (5,126 decoder calls in both pixel formats).

- [x] **5.15 Ending sprite-pack failure cleanup.**
  > **Status:** done (2026-09-23). The advancement ending requires a complete
  > seven-sprite pack and validates its selected pair before adding one to the
  > event index. Both advancement and Ousters endings reset their timer, remove
  > the input-blocking event and release the pack when an open or decode fails.
  > Normal background drawing resumes instead of retaining a failed ending.
  - Owner: existing sprite-pack decoder tests, full builds and a source audit
    of the event setters, flags, cleanup and draw callers. These executable
    integrations are regression guards. All seven installed advancement sprites
    pass the production eager loader in both pixel formats under ASan.

- [x] **5.16 Guild-mark indexed input.**
  > **Status:** done (2026-09-23). The guild-mark loader uses the checked pack
  > index reader's four-byte offsets on every platform. It requires both IDs,
  > independently seeks and decodes the large/small sprites, and publishes
  > only a complete owned pair. Failure retains the existing negative-cache
  > policy and remains false on repeated calls; missing mapper entries are safe.
  - Owner: `test_ctypepack_indexed.cpp` owns the shared index/decode contracts;
    pixel-loader tests own record bounds. Game cache publication uses the named
    exemption and full builds. All 4,758 installed guild-mark records pass the
    same index helper and both pixel decoders under ASan (9,516 decoder calls).

- [x] **5.17 Partial sprite-load results.**
  > **Status:** done (2026-09-23). Both generic pack variants return rejected
  > lazy-record results from range/set preloads, including cached failures
  > after the input stream closes. They validate ranges and IDs, retain valid
  > later rows and preserve the absent-sprite sentinel. Set iteration uses its
  > actual end rather than a narrowed 16-bit count. Addon/creature callers do
  > not mark failed preloads complete, and zone loading follows its existing
  > quit/priority-reset path when tile preload or image-pack reopening fails.
  - Owner: four `test_ctypepack_indexed.cpp` cases cover results, bounds,
    sentinels, full ID sets and valid empty sprites, with three test-first cases
    reproducing 24 failed checks. Game-side integration is a regression guard.

- [x] **5.18 Output queue budget.**
  > **Status:** done (2026-09-23). The output ring has a 16 MiB capacity ceiling,
  > including its empty sentinel, enforced on construction, resize and writes.
  > Automatic growth reserves space geometrically. Rejection preserves queued
  > bytes; packet rejection restores the frame and sequence through the existing
  > rollback path. A drained ring retains its bounded allocation for reuse.
  - Owner: four test-first cases in `test_output_stream_flush.cpp` reproduce
    22 failed checks and cover growth, the exact limit, wrapped backpressure,
    header/body rejection and resumed delivery. The wire inventory checks that
    four copies of every declared maximum frame fit within the budget.

- [x] **5.19 Effect selection diagnostics.**
  > **Status:** done (2026-09-23). Both surface effect selectors validate the
  > table index and report unavailable effects once per effect/family, with one
  > bounded bucket for invalid IDs. Missing selections retain the plain-copy
  > fallback. The supported pixel grayscale/gradation and palette screen paths
  > remain registered; unsafe legacy palette routines remain disabled.
  - Owner: two `test_spritesurface_pal_blit.cpp` cases cover diagnostics,
    repeated selections, fallback pixels, supported entries and the table-end
    sentinel. Before the fix, the sentinel produced an ASan global-buffer
    overflow and absent diagnostics produced two failed checks.

## Build and review follow-up (2026-09-18)

Table reads now expose const rows; `CTypeTable::GetMutable` and `Set` reject
invalid writes without publishing a shared fallback. Callers use checked
mutation for skills, status, sprite state and text initialization. The owners
are `test_type_table_access.cpp` and `test_experience_input.cpp`; the latter
also pins count, level, truncation and record-boundary handling for all ten
experience loaders. The format checker parses the migrated built-in string
setters and retains its site and resolution floors.

Skill-domain experience loading also publishes only complete rows. The manager
validates its record count and domain IDs, preserves failed stream state, and
leaves existing experience unchanged when a count or row is truncated.
`test_skill_core.cpp` owns these input and following-record contracts.

Item-option tables publish only complete validated records; failed reloads
retain their prior names and rows. Their name arrays are private, with checked
owned-string access for UI callers and race-specific mana labels. Option
formatting and percentage suffixes respect the output capacity.
`test_gamemodel_tables.cpp` owns malformed-input, reload and name contracts.

`MItem` destruction notifies its host to invalidate borrowed tooltip pointers
for every owning container and direct deletion path. The executable connects
that identity-only notification to `UI_RemoveDescriptor`, which clears either
tooltip payload, and clears the host after shutdown releases the item owners.
`test_item_lifetime.cpp` owns destruction, container and address-reuse contracts;
the callback must not read the already destroyed subtype.

`Player` no longer owns the unused transport hash table or key setters, and
socket streams no longer expose no-op setters. Reconnect paths retain their
connection packets; the separate per-field encryption and packet layouts are
unchanged. `test_player_base.cpp` owns the surviving player contracts, while
`test_packet_goldens.cpp` pins the plain login and encrypted field bytes.

The scrollbar's position calculations now live in `basic/ScrollRange.h`,
inherited by the real UI widget. The move preserves the existing behavior;
the following fix makes the state private, clamps updates, validates pixel
geometry, and bounds container counts before subtraction and narrowing.
Nine `test_scroll_range.cpp` tests own these contracts, including the
reproduced negative positions and divide-by-zero. Sprite ownership, skin
dimensions, and drawing stay in `VS_UI`.

Source globs now trigger CMake regeneration when files are added or removed.
The optional map viewer follows `BUILD_ENGINE`; the effect viewer no longer
links an unused engine library. Hand-written CMake modules and Makefiles are
visible to Git, while local compilation databases stay ignored. The unused
Emscripten workflow was removed. GNU Make and the effect-viewer helper select
portable job counts and explicit build configurations; README documents their
options. A direct MSBuild target can need a retry after a source-list change if it
loaded the old project before CMake regenerated it.

`basic` and `packetwire` now publish the SDL and Windows wire definitions
their public headers require. Consumers inherit them through target links,
including VS_UI and newly added test sources. The manual list of test files
needing wire macros is gone; a consumer compiled without source-specific
defines guards the contract and reproduced both missing-definition failures.

The timer-manager follow-up keeps only live callbacks, releases deleted
storage, and preserves unique IDs so stale handles cannot target replacements.
Storage is allocated inside `Add`, preserving an allocation-free global
constructor and recoverable allocation failure. Tests cover storage churn,
stale handles, callbacks that delete/replace themselves and ID exhaustion,
alongside the existing monotonic-time tests.

`TArray` now has one implementation in `basic`, with compatibility headers
in SpriteLib and framelib. Its existing tests exercise all include paths.
The unused `CDataTable` raw-object serializer is deleted, and 43 top-level
client includes now name the actual relative path to `Platform.h`.

The indexed-sprite 555/565 loaders now share checked header and row decoding.
Complete rejected sprites leave the following packed record readable, while
truncation leaves an empty object and failed stream. Encoded counts, decoded
width and palette indices are validated before publication; conversion changes
only fixed colors. `test_index_sprite_loading.cpp` owns these contracts and
empty-sprite release state. Rejection diagnostics are owned by task 5.5;
the [2026-09-23 asset audit](sprite-asset-audit-2026-09-23.md) records offline
production-loader validation and its explicit coverage limits.

The unused GL import and TGA/IMG interfaces are deleted, together with their
abandoned UI drawing comments. VS_UI uses the existing SDL surface helper;
it no longer imports declarations from the missing legacy graphics DLL.

MSVC compiles every C/C++ target with UTF-8 source and execution encoding.
The source-encoding CTest checks the repository's C/C++ files, and the unit
suite compares narrow literals from BOM and non-BOM sources with explicit
UTF-8 bytes. Existing UTF-8 BOMs are supported; they do not change literal bytes.

`ui_tests` links the real VS_UI library for independently reachable components.
Button tests cover default state, callback dispatch and event-button image
selection. The Windows feedback pointer lives with its existing CImm adapter,
so these tests link that adapter without the game UI loop or fake globals.

The same UI target tests SkinManager through its actual CRarFile reader.
Header tokens use owned strings, coordinate rows are checked before insertion,
and rejected reloads preserve the last valid skin.

The code-health review also records 18 previously completed fixes that still
had open headings, with their current source/test evidence. These status
corrections do not claim new runtime reproductions. Live-server verification
remains optional and is not a merge or completion requirement.

## First candidate (decided 2026-09-01)

**Task 1.1: the `packetwire` library.** Chosen over the alternatives
because:

1. **Zero-edit move.** Every member file's includes were scanned; none
   reached game code, so the move commit was pure CMake — the safest
   possible first step for agents to execute and review.
2. **It sits under the #1 open risk.** The info classes *are* the parsers
   the GC packets delegate server-supplied data to; the streams are where
   every length check ultimately lands. Tests aimed there have the highest
   defect yield per hour — and the first known defect was already queued
   (task 1.3, the `StringStream` float/double stack overflow, already
   proven real on the server's identical copy).
3. **It unblocks Phase 2.** The dispatcher, and eventually all 700+ packet
   classes, land in this target; creating it first meant every later phase
   was "grow the membership list", not "invent a target".

Runner-up considered and deferred: extracting `ExperienceTable`-class pure
tables (4.1) — testable, but it neither touched the top risk nor unblocked
anything else.
