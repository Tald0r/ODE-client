---
type: Record
title: "Next work plan"
description: "The ranked, self-contained tasks left in the client after the 2026-10-25 merges, each written so an agent with no other context can take it: the handler fuzzers' reach, the packet-safety routes the walks did not follow, the open code-health rows, the last drawing stubs, and the owner decisions that are not an agent's to make."
tags: [review, plan]
---

# Next work plan

Written 2026-10-26, after the merges of PRs #562-#568. The
[restructuring plan](RESTRUCTURING.md) is complete except task 4.12, and the
[code-health review](code-health-review-2026-08-29.md) had four rows still marked open (one
since tasks 1 and 2), all in its *Found by fuzzing* and *Found by reading* tables. What is left is below,
ranked by the risk it closes, with the cheap undefined-behaviour rows ahead of the long walks and
the cosmetic stubs last. Each task names the code, the tests it must flip or add, and the rule it
must keep, so it can be taken cold. One task per PR; the merge gate (a two-reviewer adversarial
review of the whole diff, then the four platform workflows dispatched by hand) applies to each.

## How to take a task

1. Read `CLAUDE.md`, then the concept each task links. The rules there bind; this page points.
2. Code a test can reach is written test-first in `tests/unit/` (the directory is globbed, so a
   new file needs no CMake edit); a fixed crash gets a unit test and, where a fuzzer found it, a
   regression input ([library fixes are test-first](../testing/library-fixes-test-first.md),
   [test framework](../testing/test-framework.md)). `VS_UI/src/**` rendering that reaches game
   globals is `Test path: exempt` with the regression-guard wording.
3. A `fix:` commit carries `Test path: lib + test`, `moved, then fixed` or `exempt`; the hook
   that refuses one without is installed once per clone with
   `git config core.hooksPath tools/git-hooks` ([commit messages](../process/commit-messages.md)).
   The trailer is `Co-Authored-By: <the authoring model> <noreply@anthropic.com>`.
4. Verify on `linux` and `linux-asan` (`tools/ci/verify-linux.sh <preset>`, from a worktree with
   the common git directory mounted), and on Windows when the code is Windows-reachable; a
   library fix is done only when the plain and the ASan tree are both green. On Apple Silicon
   the arm64 image has no warning budget, so check the log against the x86_64 profile
   ([reading build output](../build/reading-build-output.md)). CI runs on master and by hand:
   `for w in windows linux macos web; do gh workflow run $w.yml --ref <branch>; done`
   ([CI](../build/ci.md)). `git status` must list every file you wrote.
5. Update this page in the same commit as the work: move the task to *Done* with its PR number
   (its Owner is the test the task names), or narrow it to what is left. A changed concept gets
   a `docs/log.md` entry; moving a task here needs none. Run
   `perl tests/tools/check_knowledge_bundle.pl .` for any docs change.
6. Do not decide an item under *Owner decisions*; they are the repository owner's calls.

## Tasks

### 3. `Assert`-only bounds that hostile input reaches

**What:** the open *Found by fuzzing* row: `CGSMSSend::read`'s four length and count checks and
`GCTimeLimitItemInfo::read`'s count check are `Assert`s, which throw in Debug and vanish in
Release, where the values are then accepted. Each guards a growing container under the frame's
size cap, so it is not a memory error, which is why it was recorded as a class.

**How:** replace each `Assert` on a wire value in those two reads with
`throw InvalidProtocolException(...)`, the exception `CGSMSSend::write` already throws; neither read has a non-`Assert`
bound check (`GCTimeLimitItemInfo::read` only skips a duplicate id). Then sweep `grep -rn Assert Client/Packet --include='*.cpp'`
(quoted, or zsh expands the glob) for other wire values behind an `Assert`. `CGSMSSend` is
client-to-server and is parsed only because `PacketFactoryManager` registers its factory and
`CPS_NORMAL` accepts every registered id; narrowing that set is a protocol-table change, so do
the refusals first and record the narrowing here as a follow-up.

**Test:** one test per site in `test_packet_fuzz_findings.cpp` asserting the refusal, and one
configure with `-DCMAKE_BUILD_TYPE=Release` (the Linux and macOS presets are Debug; only Windows
has a release preset) to see the `Assert`s gone.

### 4. The handler fuzzers stop at the presentation hosts

**What:** `tests/fuzz/fuzz_client_stream_handlers.cpp` and its login twin dispatch every reply
into the model with the executable's host adapters removed, so "what an adapter does with a
handler's value (`GameMain`'s `SetWeather`, a dialog's text) is not reached"
([fuzzing](../protocol/fuzzing.md)). The 2026-10-25 walk found the unguarded values on that side
by reading, and unit tests pin them; no fuzzer runs the host side.

**How:** give `tests/fuzz/handler_world.h` a *recording* host set: the host interfaces are
function-pointer structs ([host interfaces](../architecture/host-interfaces.md)), so an adapter
per interface stores the last call's arguments and aborts (the harness's crash path, as a
`CHECK` does) on a value the real host would index out of range: zone ids against the zone
table, direction bytes against eight, grade and rank bytes against their tables. Start with the
hosts [packet input safety](../protocol/packet-input-safety.md) names under the walk. Do not link
`VS_UI` into the fuzzer. Build and run per the fuzzing page's recipe (Clang with libFuzzer and
the sanitizers; the replay ctests over `tests/fuzz/regressions/` run everywhere).

**Test:** the replay ctests stay green; a value a recording host refuses gets a regression input
and a unit test. Record the run in the fuzzing page's *Triage* paragraph and in the code-health
review's *Found by fuzzing* table, which is where runs are recorded (the fuzzing page has no
table).

### 5. The packet-safety walk's unfollowed routes

**What:** [packet input safety](../protocol/packet-input-safety.md) says what each walk did not
follow: the text system's own wrapping, measuring and drawing of a server string; the descriptor
renderers other than the blood bible's; the view's pointers to the names it draws; `VS_UI`'s own
reads of the model objects it draws (inventory grids, skill trees, party slots). Each is a route
a server value takes into an index or a buffer that no test has read.

**How:** one PR per route, in this order: (a) the text system, since every server string ends
there: read `Client/TextSystem` for a server-length-sized loop or buffer, pin what it does with a
2048-byte row and a row of zero width; (b) the descriptor renderers under `VS_UI/src` that take a
model object (`grep -an Descript VS_UI/src/*.cpp`, with `-a`: the NUL bytes in
`VS_UI_GameCommon.cpp` stop a plain grep); (c) the view's name pointers (`MTopView`'s hover and
name draws); (d) the `VS_UI` grid and tree reads. A route found clean is written up on the safety
page as followed, with the line it stopped at; a route found open gets a lib fix, a test and a
review row.

**Keep:** a value a library function could check is checked in the library; the exemption is for
the rendering alone.

### 6. `BltIndexSpriteColor` and `BltIndexSpriteBrightness`: the last drawing stubs

**What:** `CSpriteSurface::BltIndexSpriteColor(POINT*, CIndexSprite*, BYTE rgb)` and
`BltIndexSpriteBrightness(POINT*, CIndexSprite*, BYTE)` in
`Client/SpriteLib/CSpriteSurface_Adapter.cpp` ignore their effect and call `BltIndexSprite`.
Live callers: an installed mine drawn in red (`Client/MTopView.cpp`, the `ITEM_CLASS_MINE`
branch, `rgb` 0) and the `FAKE_CREATURE_BRIGHTNESS` fake's three draws in
`Client/CreatureEffectDraw.cpp`. The item pack's two tinted draws (`C_VS_UI_ITEM::BltColor` and
`BltLockedColor`, `VS_UI/src/VS_UI_Item.cpp`) have no live caller; the one reference is
commented out. This is a `fix:`, `Test path: lib + test`; the files are library members through
`tests/arch/*_files.txt`.

**How, tint:** the DirectDraw original is `CIndexSprite::BltColor` (`Client/SpriteLib/CIndexSprite.cpp`):
an indexed pixel takes the file-local row `defaultGradationColor` (384) at its own gradation,
masked by `ColorDraw::s_wMASK_RGB[rgb]`; a fixed pixel goes through `memcpyColor`. Do not reuse it
unchanged: `s_wMASK_RGB` is all zeros in this build (`basic/ColorDraw.cpp`; `InitMask` has no
caller, and would set shift counts, not masks), so every indexed pixel would decode to 0, a hole.
Mask with `0xf800`, `0x07e0`, `0x001f` as the SDL `memcpyColor` (`CSpriteSurface_SDL.cpp`) does.
Two patterns exist to copy: `BltIndexSpriteColorSet` (PR #566) decodes into a transient backend
sprite and draws it with `spritectl_blt_sprite`; `BltIndexSpriteDarkness` (PR #563) first tries
`SpriteGpu::DrawEffect` and falls back to two CPU decodes written into the locked surface. The
pixel-sprite tint is `BltSpriteColor` through `DrawEffect` with `Effect::Color`, so the GPU path
is the one to try first; the cached backend sprite holds the sprite's own colour sets, so a GPU
tint over it must still take row 384 for the indexed pixels.

**How, brightness:** `CIndexSprite::BltBrightness` sends fixed pixels through
`CSpriteSurface::memcpyBrightness`, which is an empty SDL stub (`CSpriteSurface_SDL.cpp`;
`memcpyDarkness` beside it was ported). Port it, then draw as the darkness routine does.

**Tests to flip:** `TopView3Item.AMineIsDrawnPlainInstalledOrNot` in
`tests/unit/test_top_view_3.cpp` and `APetMovingFastIsDrawnToo` (with the file header) in `tests/unit/test_creature_effect.cpp`
pin the stubs as recorded defects; the [native GPU renderer](../rendering/native-gpu-renderer.md)
page says `BltIndexSpriteColor` draws plain. **Tests to add:** an oracle draw per effect in
`tests/render/test_sprite_gpu.cpp`, next to `IndexSpriteColorSetDrawsEveryPixelFromTheSet` and
the darkness one, with a low gradation (high gradations decode to black).

### 7. `memcpyPalEffectGradation` reads five bits of a six-bit green (latent)

**What:** `CSpriteSurface::memcpyPalEffectGradation` in
`Client/SpriteLib/CSpriteSurface_Effects.cpp` computes `g = (s >> s_bSHIFT_G) & 0x1F` on a 5:6:5
pixel, so greens 32 to 62 wrap before `ColorToGradation[r + g + b]`. It is latent: the
`EFFECT_GRADATION` slot of `s_pMemcpyPalEffectFunctionTable` (`CSpriteSurface_SDL.cpp`) is
`NULL`, only `EFFECT_SCREEN` is registered, and the file's header says the palette effects stay
unregistered until ported because they assume a 5-bit green. Noted in the #566 review
(`3c3db3e6`).

**How:** take green's top five bits as `ColorSetDarkness` does through `ColorDraw::Green` since
#563, keep the `ColorToGradation` index in range (94 entries since #566; say in the renderer page
whether the fix registers the routine). Sweep `grep -rn 'SHIFT_G) & 0x1F' Client/SpriteLib`:
ignore the hits in `CSpriteSurface.cpp` (not compiled) and decide `CSprite::memcpyDarkerFilter`
(`CSprite.cpp`), which has the same mask.

**Test:** a pixel whose green is 32 (`0x0400`): the bug sums it to 0 and the fix to 16; a pure
green of 63 agrees under both. `tests/render/test_sprite_gpu.cpp` already uses that pixel.

### 8. Packets that never free what their read allocates

**What:** measured while taking task 2, with its harness (`ReadShortBodies` in
`tests/unit/test_packet_fuzz_findings.cpp`): seven packets allocate a member in `read` that their
destructor never frees, so each one received leaks it, refused or read whole. `GCAddSlayer`,
`GCAddVampire` and `GCAddOusters` free their `EffectInfo` but not their `PetInfo` or
`NicknameInfo`; `GCPetInfo` frees no `PetInfo`, `GCBloodBibleSignInfo` no `BloodBibleSignInfo`,
and `GCMyStoreInfo` and `GCOtherStoreInfo` no `StoreInfo` (about a megabyte held after 512 short
frames each). Beside them, `GCSMSAddressList` frees none of its `AddressUnit`s by design: its
handler hands them to the executable's SMS list, and that list leaks every one in the end. It keeps
them only while its window exists (`C_VS_UI_GAME::SetSMSAddressList` returns early otherwise), and
`C_VS_UI_SMS_LIST::SetSMSList` clears the previous list without freeing it, while its destructor
frees none; a frame refused after its read (trailing bytes) leaks them too. Its `DeleteList` also
deletes the last unit when no id matches, while that unit is still in the list: a use after free
(`VS_UI/src/vs_ui_gamecommon2.cpp`; read it with `grep -a`).

**How:** trace each member's owner first: a handler or a presentation host may keep the pointer
(`GCPetInfo`'s pet, the sign info, the store info), and freeing it then is a use after free. Where
the reply copies, free it in the destructor and delete the copy operations, as `GCPetStashList`
and `GCNicknameList` do since task 2; where something keeps it, make the hand-off explicit. The
SMS units want the SMS list to copy what it shows, as the nickname window does, which is a
`VS_UI` change.

**Test:** add each packet to a `CHECK_SHORT_BODIES_LEAK_NOTHING` list once its owner is settled,
and drop `GCSMSAddressList` from `FreeWhatThePacketDoesNotOwn` when its units are the packet's.

### 9. Kept in view, no task

- The 5:5:5 sprite decoders are latent ([client traps](../architecture/client-traps.md)) and
  tested; nothing in the shipped data uses them.
- The [browser client](../browser/browser-client.md) builds and logs in; a task appears when the
  server's WebSocket gateway changes.

## Owner decisions (not for an agent to make)

Recorded so the next agent does not decide them unasked. Each stays open until the owner answers.

- Task 4.12's slice residuals (its Status line has the detail): the advancement-class equip
  check; a separate castle-tax wire field (the plan's open decision 7); letting pets be used by
  any race.
- The chat filter (task 4.13, done; its *Known, not fixed* note): the Korean lists are converted
  from CP949 to UTF-8 on load while the filter pairs bytes and looks words up in two- to
  eight-byte windows, so the shipped lists match no text (pinned by
  `KoreanWordsFromTheBinaryListAreConvertedAndMatchNothing`). Which encoding chat is filtered in
  is the owner's call.
- Transport encryption: the wire is unencrypted, login credentials included
  ([reconciliation](code-health-reconciliation-2026-09-23.md), *Boundaries retained*). It needs a
  matching server protocol change and is a design decision, not a fix.

## Done

### 1. Five enum casts UBSan does not see (PR #570)

Both review rows are fixed, with eight more sites two sweeps found: `GCWarList::read`, the login
host adapter's `ErrorID` cast, and a pet's attribute cast to `ITEMOPTION_PART` in
`SetPetInfo` and five readers of the pet item's enchant level. `REQUEST_FILE_TYPE` has the fixed
type `BYTE`; the other values are switched on, compared or passed as integers; the eleven
`toString()`s print a sex through `SexName`. Owners: `PacketFuzzFindings`'
`RequestedFileTypePastTheEnumReadsBackAsSent`, `WarTypePastTheLastIsRefused` and
`OustersInfoStringPrintsASexItHasNoNameForAsItsNumber`;
`ShopSoldHandler.AShelfTypePastTheShelvesChangesNothing`; `PacketHelpers.EachLoginErrorHasItsMessageBox`;
`PacketPlayerItems.PetInfoAttributePastTheOptionPartsTakesItsRowsColour`; the two `PetOptionPart`
tests; and `Player.APetsAttributeAddsToTheStatItNamesAndAnyOtherNumberToNone`. Left, and named in the
rows: `CLRegisterPlayer::read`'s `Nation` cast, in a packet the client registers no factory for,
and the name-table indexes of stored values in `toString()`s compiled only with `__DEBUG_OUTPUT__`.

### 2. The login list reads leak the entry whose read throws (PR #571)

The row is fixed with every site of its shape the sweep found: 31 reads under `Client/Packet` hold
the entry they allocate in a `std::unique_ptr` until the list takes it, the two login lists, the
skill infos, the guild, union, nickname, quest, war, goods, stash, party, SMS and peer lists among
them, `GCWarList::read`'s `switch` and `RCRequestedFile::read` included; `GCNPCAskVariable` also
frees a parameter its map refuses as a duplicate. `GCPetStashList`, `GCNicknameList` and
`GCGQuestStatusModify` (whose read is inline in its header, which the first sweep missed) free
their entries in their destructors now, their replies copying what they show: the follow-ups
recorded by restructuring tasks 4.116, 4.107 and 4.120. Owners: `PacketFuzzFindings`'
`LoginListReadsThatThrowLeakNothing`, `GameListReadsThatThrowLeakNothing` and
`PeerListReadsThatThrowLeakNothing`, which read each packet from frames of every body length up to
255 and check ASan's allocator holds nothing more. Left: the short bodies reach no entry read of
`GCAddItemToZone`, `GCStashList`, `GCTradeAddItem` or `GCUpdateInfo`, whose fixes are regression
guards, and the member and SMS leaks the harness found, which are task 8.
