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

| Code | Why exempt |
|---|---|
| `GameMain.cpp`, `GameInit.cpp`, `GameUI.cpp`, `Client.cpp`, `SDLMain.cpp` | process lifecycle, DLL whitelist, render loop and quest UI events; also where the hosts (`MItemHost`, `MPriceHost`, `WireHost`) are installed, which no test can prove - `WireHost`'s installer is designated-initialised since 2026-09-09, so a wrong slot there is a compile error, but a wrong *body* still is not |
| `MZone` live sector allocation/application, rendering and visual-effect ownership / `TileRenderer` draw paths | map parsing is tested in `ZoneMapData`; application and effects reach live sectors, creature status, sprite tables and `MTopView`; drawing uses live surfaces. Viewer tools cover some drawing; ownership guards use full builds and source-path audits. |
| `MTopView` draw calls and the per-frame state that only the draw calls read (`DrawItemBroken` and the other draw branches) | the view draws on the live surface and reads the frame clock (`g_CurrentFrame`, `g_bFrameChanged`), `g_pEventManager` and the player's gear globals; it is the render loop the goals keep executable-side. Rules that can be stated without those globals move into a library first and get a test. Fade direction, stepping, delay and stopping now belong to `gamemodel`'s `MScreenFade` (task 4.18), with the 31 to -1 transition tested under both char signednesses; `MTopView` keeps suppression, colour conversion and surface calls. Fixes to the draw calls use full builds; the MinGW and Emscripten compiles some fix commits cite are a local check that is not in the tree, and no CI job repeats them. |
| `MGuildMarkManager::LoadGuildMark` integration | binds the live guild mapper to the renderer's owned sprite cache; index and sprite decoding stay in the tested SpriteLib helpers. Publication and negative-cache guards use full builds and source/ownership review. |
| `MCreature`, `MPlayer`, `MFakeCreature` movement and attached-effect orchestration; `PacketFunction::ExecuteActionInfoFromMainNode` | virtual character classes reach the live zone, UI, sprite tables and effect generators; action results transfer to `MEffectTarget` and execute through the same game objects. Bounds/queue/ownership guards stay here; extracting those classes would require the render/game-loop rewrite excluded above. Review regression guards use full builds and existing automated checks, without a runtime gate. |
| `VS_UI/src/**` rendering and dialogs that still reach game globals | these paths use full builds and available automated checks; live verification is optional. `ui_tests` now links the real Button, EventButton, SkinManager, LineEditor, LineEditorVisual state/focus methods, InputFocusManager and the UI result receiver, so those independently reachable components require test-first fixes. `LineEditorVisual::Show` remains separate because it reaches the game's renderer. |
| `Client/PacketHandler/*Handler.cpp` bodies, except the handlers `tests/arch/gamemodel_files.txt` lists | mutate `g_pZone`/creature state; the *parsers* they consume are in `packetwire` and testable, the mutations are not. The reason does not hold for a handler whose body reaches only model state: such a handler compiles in `gamemodel`, a test runs it on a real packet, and a fix to it is test-first (lib + test). Amended 2026-09-30 (task 4.15), when eight handlers moved in: the four phone-slot handlers (`g_pUserInformation`), the trade money and offer handlers (`g_pTradeManager`, `g_pMoneyManager`), `GCSystemAvailabilities` (`g_pSystemAvailableManager`) and `GCMonsterKillQuestInfo` (`g_pQuestInfoManager`, `g_pCreatureTable`). The row is keyed to the membership file so that a later handler move needs no further edit here. The rest of `Client/` is presumed movable until a task proves otherwise, and code has left this list by moving before: `RequestClientPlayerManager.cpp` from the connect-paths row (task 5.1). The login world/server list rebuilds now run in `gamemodel` through `ApplyWorldList` and `ApplyServerList` (task 4.19); their handlers retain global model creation, UI refreshes and mode transitions. Those integrations and the other handlers stay exempt. |
| `UIMessageManager::Execute_UI_CHAT_RETURN` | application chat callback reaches the current game mode, player, party/guild state, live socket, help events and dialogs. Its payload borrowing is source-audited with full builds; the queue that owns deferred text is independently tested in `ui_tests`. |
| `Client/MinTr.h` raw trace transport | the one remaining caller sends a fixed text message to the optional external Win32 trace window. Unused variadic and command formatting paths are retired. |
| `PacketFunction.cpp` connect paths | Winsock + connection state machine. `RequestClientPlayerManager.cpp` was listed here until 2026-09-09; task 5.1's fifth slice put its seams behind `WireHost`, and task 5.2's eighth slice deleted it with the rest of the outbound peer side |
| The executable halves of split classes: `MItemUse.cpp`, `MObjectScreen.cpp`, `MSkillAvailable.cpp`, `TextServiceScreen.cpp` | the packet/dialog/drawing side of a class whose core is in a library, by design |
| `ModifyStatusManager.cpp` `Function_MODIFY_*` bodies | the player's reactions to a status change: each reaches `g_pPlayer`, the UI setters, the gear and inventory, `g_pGameMessage` or `g_pSkillAvailable`, and runs from `MPlayer::SetStatus`. The rules they apply are in libraries and tested there (a vampire's skill cost is `MSkillInfoTable::GetVampireConsumeMP` in `gamemodel`); the reactions use full builds and a regression guard. Added 2026-09-29 with the call that recomputes a vampire's skills on an INT change. |
| `Client/UIDialog.cpp` dialog popups (`PopupPCTalkDlg` and its siblings) | each builds a VS_UI dialog from `g_pCreatureTable`, `g_pPCTalkBox` and `gC_vs_ui`, so a fix has no logic left to move into a library and no harness to run in. Regression guards use full builds and a read of the function. Added 2026-09-29 for `7d16c726`. |
| `PacketFunction.cpp` error popups and addon item tables (`PopupErrorMessage`, `InitPacketItemTable`) | the popups reach the live UI and message tables, and the tables store item templates the executable builds at start-up (`g_pPacketItemShoulder` and its siblings); neither has a harness. Precedent `0557a039`; also `10b1da62` and `6ebdc97c`. Added 2026-09-29. The connect paths in the row above are a separate case. |
| `UIMessageManager.cpp` handlers other than `Execute_UI_CHAT_RETURN` (the other `Execute_UI_*` bodies) | each reaches the live storages, inventory, `g_pTempInformation`, the player and the socket, like the chat handler above; the class has no harness. Precedent `ace28baf`; also `dfaeda5b`. Added 2026-09-29. |
| `MEventManager.cpp` background-image loading and live host adapter | owns the `CDirectDrawSurface` cache and supplies rendering/player callbacks. Event records, flag queries and expiry are inherited from `gamemodel`'s `MEventQueue` and require unit tests (task 4.17); only the image loader and callback bodies remain executable-side. |
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
| R1 | Application translation units compiled directly into the `DarkEden` target | **409** | `ClCompile` entries in `DarkEden.vcxproj`, excluding CMake's generated `CMakeFiles/DarkEden.dir/cmake_pch.cxx`, read from the ctest run's own build dir; SKIP (never PASS) without a supported generated project. Both the Visual Studio configure stamp and Ninja's `build.ninja` must be newer than CMakeLists.txt and both library membership files. Ninja's baseline is **407**, since the non-Windows source list is two files shorter; Linux/macOS CI verifies that count. Baseline 1,044 on 2026-09-01; 484 after task 5.2's eighth slice. **482 on 2026-09-17:** the earlier deletion of `md5.cpp` had left the recorded 484 one too high (483 measured before this extraction); task 4.5 moves `RankBonusTable.cpp` into `gamemodel` (483 → 482). **478 later that day:** delete the four unused Direct3D texture/shadow cache translation units; their object declarations were commented out and the active sprite path never constructed them. **477 later still:** remove the unused Windows-only WinINet downloader; Ninja remains 475. It counts what still cannot be unit-tested. Recorded growths, each the executable side of a split: `PacketHandlerRegistry.cpp`, `GCExchangeBuyHandler.cpp`, `MItemUse.cpp`, `MObjectScreen.cpp`, `MSkillAvailable.cpp`, `TextServiceScreen.cpp`. **473 Windows / 471 Ninja later that day:** six map-record/header translation units move to `gamemodel`, while two new executable files retain screen geometry and live interaction actions; `ShowTimeChecker`'s pure constructor and I/O move to `ShowTimeData.cpp`. A further executable unit moved to `basic` with `CMessageArray` (2026-09-18). **470 Windows / 468 Ninja on 2026-09-21:** `UserOption.cpp` moves unchanged into `VS_UI`; its test links the same object as the game. **469 Windows / 467 Ninja:** the corrected `SXml.cpp` moves unchanged into `VS_UI`, replacing the older duplicate and allowing XML tests to link the production implementation. **468 Windows / 466 Ninja:** the help-message loader moves unchanged into `VS_UI` for real-library parser tests. **467 on 2026-09-21:** `CToken.cpp` joins `gamemodel` byte-identically so token and reset lifetimes can be tested. **466 Windows / 464 Ninja:** the empty handwritten `Client_PCH.cpp` is retired when CMake starts producing the private PCH. Generated PCH producers are excluded from both inventories; ordinary application `.cxx` and `.cc` files remain counted.  **465 Windows / 463 Ninja on 2026-09-24:** the inactive Client/DebugInfo.cpp implementation is removed.  **460 Windows / 458 Ninja on 2026-09-24:** five unchanged world metadata implementations join gamemodel. **456 Windows / 454 Ninja:** quest metadata joins gamemodel; profile and shrine file adapters join VS_UI. **455 Windows / 453 Ninja:** party membership joins gamemodel behind its live-creature host. **456 Windows / 454 Ninja:** the generated English NPC name table (`MNPCTableEnglish.cpp`) is a recorded growth; it writes an executable global. **454 Windows / 452 Ninja on 2026-09-28:** the NPC talk boxes (`TalkBox.cpp`, `MStringList.cpp`) join gamemodel so the menu-answer mapping can be tested. **453 Windows / 451 Ninja later that day:** `MStatusManager.cpp` joins gamemodel so the character-select stat preview can be tested (task 4.12, slice 3). **449 Windows / 447 Ninja on 2026-09-29:** the action and effect tables (`MActionInfoTable.cpp`, `MEffectSpriteTypeTable.cpp`, `MEffectStatusTable.cpp`, `MCreatureSpriteTable.cpp`) join gamemodel so their loaders can be tested (task 4.1); the move changed no loader, and later commits fixed the loaders test-first. **447 Windows / 445 Ninja on 2026-09-29:** the chat filter (`MStringMap.cpp`, `MChatManager.cpp`) joins gamemodel, its one option read behind `MChatHost`, so the curse filter can be tested (task 4.13). **445 Windows / 443 Ninja on 2026-09-29, after merging both:** the creature status array (`MStatus.cpp`) and the request/answer mode register (`TempInformation.cpp`) join gamemodel unchanged so they can be tested (task 4.14); each branch removed two from 449/447, and the merged tree was measured, not added up. (The merge had left this entry's bold marker doubled and its first clause repeated; tidied 2026-09-30.) **437 Windows / 435 Ninja on 2026-09-30:** the eight packet handlers that reach only model state (`GCPhoneConnectedHandler.cpp`, `GCPhoneDisconnectedHandler.cpp`, `GCPhoneSayHandler.cpp`, `GCRingHandler.cpp`, `GCTradeMoneyHandler.cpp`, `GCTradeRemoveItemHandler.cpp`, `GCSystemAvailabilitiesHandler.cpp`, `GCMonsterKillQuestInfoHandler.cpp`) join gamemodel from `Client/PacketHandler` so a test can run them on real packets (task 4.15). **436 Windows / 434 Ninja on 2026-10-02:** `CServerInformation.cpp` joins `gamemodel` unchanged so login world/server selection can be tested (task 4.19). **434 Windows / 432 Ninja later that day:** `MFunctionManager.cpp` and `MRequestMode.cpp` join `gamemodel` unchanged so callback dispatch and player/view request state can be tested (task 4.20). **433 Windows / 431 Ninja later that day:** `MathTable.cpp` joins `gamemodel` unchanged so effect trigonometry and steering can be tested (task 4.21). **432 Windows / 430 Ninja later that day:** `ServerInfoFileParser.cpp` joins `gamemodel` after removing its unused `MinTr.h` include, so dimension-specific login and reconnect configuration can be tested (task 4.22). **431 Windows / 429 Ninja later that day:** `MWeather.cpp` joins `gamemodel` behind player-origin and viewport callbacks (task 4.23). **428 Windows / 426 Ninja later that day:** `MJusticeAttackManager.cpp` and its add/remove packet handlers join `gamemodel` after dropping unobserved timestamps (task 4.25). **427 Windows / 425 Ninja later that day:** `PCConfigTable.cpp` joins `gamemodel` unchanged so saved character slots and per-world account settings can be tested (task 4.26). **425 Windows / 423 Ninja later that day:** `MonsterNameTable.cpp` and `MLevelNameTable.cpp` join `gamemodel` unchanged, with the title/hallucination selection state extracted from `MCreature` (task 4.27). **424 Windows / 422 Ninja later that day:** `MWarManager.cpp` joins `gamemodel` with live zone and UI actions behind `MWarHost` (task 4.28). **423 Windows / 421 Ninja later that day:** `MShopTemplate.cpp` joins `gamemodel` unchanged, with the NPC shelf-building rules extracted behind `NPCShopHost` (task 4.29). **421 Windows / 419 Ninja later that day:** NPC dialogue (`MNPCScriptTable.cpp`) and its English overlay join `gamemodel` unchanged after a separate unused-include cleanup (task 4.30). **420 Windows / 418 Ninja later that day:** effect-target state (`MEffectTarget.cpp`) joins `gamemodel` behind its player-roster removal host, alongside the extracted deferred-result queue (task 4.31). **419 later still:** portal records join `gamemodel` (task 4.32; Ninja 417). **418 later still:** music metadata joins `gamemodel` with track selection (task 4.33; Ninja 416). **417 later still:** delayed sound records join `gamemodel` with queue scheduling (task 4.34; Ninja 415). **416 later still:** show-time scheduling joins `gamemodel` (task 4.36; Ninja 414). **415 later still:** the help-string table joins `gamemodel` (task 4.37; Ninja 413). **414 later still:** interaction-object metadata joins `gamemodel` (task 4.38; Ninja 412). **413 Windows / 411 Ninja:** base effect state joins `gamemodel` behind frame/light services (task 4.44). **411 Windows / 409 Ninja:** moving and screen effects join `gamemodel` (task 4.45). **410 Windows / 408 Ninja:** randomized draw-skipping effects join `gamemodel` (task 4.46). **409 Windows / 407 Ninja:** complete linear effects join `gamemodel` (task 4.48). |
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

The creature allocator is now directly testable in `basic/MemoryPool`, with
its four creature-specific pool instances retained in `Client/MemoryPool.cpp`.
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
  > screen geometry; `MInteractionObjectAction.cpp` retains live actions;
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
  > formulas' Range (and Soul Rebirth by its own, 2 + level / 10 plus
  > the passive skill's level / 10, which the client's own Soul Rebirth
  > override does not follow: a left-over), the minimum plus the slot's
  > level / 10, which the
  > step to level 30 gives for the span of 3 each has in the seed and
  > in the upstream `SkillInfo.inf`; `test_skill_range.cpp` checks the
  > step against those `decore::skillformula` formulas at levels 0 to
  > 30 (`8d2b12b9`'s message says the ousters' step has no server
  > counterpart). The slayer change was a refactor: the rule
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
  > (`GCGuildChatHandler.cpp:43`), `GCWhisper`'s name
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
  > `ApplyServerList` updates the selected world's servers, retaining unlisted
  > records. Both consume packet records and preserve the requested-ID and
  > first-row fallback rules, including duplicate and zero IDs. A missing
  > selected world leaves the server packet untouched. The handlers retain
  > global model creation, UI refreshes and mode transitions; an empty accepted
  > server list still reaches the UI. R1 is 436 Windows / 434 Ninja.
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
  > initialize once and use the atan lookup's valid table indices and the
  > steering helpers' existing coordinate/angle ranges. Initialization now
  > scales both atan endpoints, correcting the northwest and southeast
  > diagonal targets; the regression tests reproduced the missing endpoint
  > and the 22.5-degree error before the fix. R1 is 433 Windows / 431 Ninja.
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
  > Table reloads clear prior rows and publish only complete input; all counts
  > are bounded by remaining bytes. Saves validate every row's encoded strings
  > before writing to the supplied stream. Binary layout and duplicate-key
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
  > lifetime and drawing. Arrival requires strict XY proximity after half a
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
  > R1 is 409 Windows / 407 Ninja.
  - Owner: M0-M2, `tests/arch/gamemodel_files.txt`, R1 and
    `tests/unit/test_linear_effect.cpp`, using production linear effects.

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
  > compiles; `TextService` has no virtuals, so the split costs no
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
