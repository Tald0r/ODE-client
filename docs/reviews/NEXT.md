---
type: Record
title: "Next work plan"
description: "The ranked, self-contained tasks left in the client after the 2026-10-25 merges, each written so an agent with no other context can take it: the handler fuzzers' reach, the packet-safety routes the walks did not follow, the open code-health rows, the last drawing stubs, and the owner decisions that are not an agent's to make."
tags: [review, plan]
---

# Next work plan

Written 2026-10-26, after the merges of PRs #562-#568. The
[restructuring plan](RESTRUCTURING.md) is complete except task 4.12, and the
[code-health review](code-health-review-2026-08-29.md) had four rows still marked open (none
since tasks 1 to 3), all in its *Found by fuzzing* and *Found by reading* tables. What is left
is below, ranked by the risk it closes, with the cheap undefined-behaviour rows ahead of the long
walks and the cosmetic stubs last. Each task names the code, the tests it must flip or add, and the
rule it must keep, so it can be taken cold. One task per PR; the merge gate (a two-reviewer
adversarial review of the whole diff, then the four platform workflows dispatched by hand) applies
to each.

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
follow: the descriptor renderers other than the blood bible's; the view's pointers to the names it
draws; `VS_UI`'s own reads of the model objects it draws (inventory grids, skill trees, party
slots). Each is a route a server value takes into an index or a buffer that no test has read.

**How:** one PR per route, in this order ((a), the text system, is done: PR #577, under *Done*):
(b) the descriptor renderers under `VS_UI/src` that take a model object
(`grep -an Descript VS_UI/src/*.cpp`, with `-a`: the NUL bytes in `VS_UI_GameCommon.cpp` stop a
plain grep); (c) the view's name pointers (`MTopView`'s hover and name draws); (d) the `VS_UI`
grid and tree reads. A route found clean is written up on the safety
page as followed, with the line it stopped at; a route found open gets a lib fix, a test and a
review row.

**Keep:** a value a library function could check is checked in the library; the exemption is for
the rendering alone.

### 10. Kept in view, no task

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

### 3. `Assert`-only bounds that hostile input reaches (PR #572)

The row is fixed: `CGSMSSend::read`'s four checks and `GCTimeLimitItemInfo::read`'s count throw
`InvalidProtocolException` at the bound each `Assert` stopped at, in every build. A sweep of every
`Assert` under `Client/Packet`, headers included, found no other wire value behind one: the rest
check pointers or the packet's own state, and the stash, update-info and name reads check their
values with a throw before any `Assert` sees them. Owners: `PacketFuzzFindings`'
`SMSSendCountsAndLengthsAtTheirBoundsAreRefused`, `SMSSendJustInsideItsBoundsStillReads` and
`TimeLimitItemCountPastItsBoundIsRefused`; a Release tree showed the unfixed reads accepting the
values. In the receive loop the refusal also sends the server a bug report, as every
`InvalidProtocolException` does (`GameMain.cpp`'s `UpdateSocketInput`), which an `AssertionError`
did not; and `GCTimeLimitItemInfo`'s frame cap (`getPacketMaxSize`, 801 bytes, exactly 100 items)
already stopped a longer list there, so its new refusal only names the reason. Follow-ups, none of
them a memory error: the client still parses `CGSMSSend`, a client-to-server packet, because
`PacketFactoryManager` registers its factory and `CPS_NORMAL` accepts every registered id, and
narrowing that set is a protocol-table change; `CGSMSSend::read`
reads its receivers' numbers and drops them, so `m_Numbers` stays empty; and its `write` allows a
list of `MAX_RECEVIER_NUM` numbers and numbers of `MAX_NUMBER_LENGTH` digits that its `read`
(and, if the server's copy matches, the server) refuses, while its `getPacketMaxSize` counts the
caller's number as `MAX_RECEVIER_NUM` bytes, not `MAX_NUMBER_LENGTH`, so `write` can emit 113
bytes against a stated 108.

### 6. `BltIndexSpriteColor` and `BltIndexSpriteBrightness`: the last drawing stubs (PR #575)

Both draw now, as `BltIndexSpriteColorSet` does: the sprite is decoded by `CIndexSprite`'s own blit
(`BltColor`, `BltBrightness`) and drawn as a transient RLE sprite, decoded twice over black and
white so a pixel the blit wrote stays opaque, black included; `BltIndexSpriteColorSet` shares the
helper, so a black pixel of its decode is no longer a hole. `BltColor` masks with `TintMask`
(`0xf800`, `0x07e0`, `0x001f`, as `memcpyColor`) instead of the never-filled
`ColorDraw::s_wMASK_RGB`, and `memcpyBrightness` is ported: it adds the brightness to each channel
and wraps, green at 64, so the 32 every caller passes leaves every pixel as it is (the original,
reading green as five bits, also dropped green's lowest bit), and a multiple of 32 draws the
cached sprite instead of decoding. Owners: `SpriteGpu.IndexSpriteColorTintsEveryPixelToOneChannel` and
`IndexSpriteBrightnessAddsToTheFixedPixelsAndWraps`, and
`TopView3Item.AnInstalledMineIsDrawnRedAndAnotherPlain`, which fails if an installed mine draws
plain. No GPU path: the cached backend sprite holds the sprite's own colour sets, and the tint
needs row 384 for its indexed pixels.

### 7. `memcpyPalEffectGradation` reads five bits of a six-bit green (latent) (PR #573)

Fixed: the routine takes green's top five bits, as the registered `memcpyEffectGradation` does,
so a green of 32 sums to 16 and white to 93, the table's last entry; it stays unregistered, and the
[native GPU renderer](../rendering/native-gpu-renderer.md) page says so. The sweep's other
compiled hit, `CSprite::memcpyDarkerFilter`, had the same mask and scales the six-bit green now,
and the review found four more spelled `& 0x0000001F`, the 1:5:5:5 conversions `CSprite::memcpy1555`
and `memcpy1555Small` and their `CSpritePal` twins, which take green's top five bits now. No draw
reaches any of them: the backend's `BltSpriteDarkerFilter` and `BltSprite*1555*NotTrans` are stubs
with no caller. Owners: `SpriteGpu.PaletteGradationTakesGreensTopFiveBits`,
`DarkerFilterScalesTheSixBitGreen` and `Conversions1555TakeGreensTopFiveBits` in
`tests/render/test_sprite_gpu.cpp`. Left: `memcpyPalEffectGradation` indexes `ColorSet[s_Value1]`
without the range check `memcpyEffectGradation` makes, which matters once it is registered; and
the table's scale, task 9.

### 8. Packets that never free what their read allocates (PR #574)

Fixed. The seven packets free their members in their destructors and delete their copy operations
(`GCBloodBibleSignInfo`'s constructor also nulls its pointer); every reply copies what it shows.
The SMS list (`C_VS_UI_SMS_LIST`, `VS_UI`) keeps copies of its own `AddressUnit` type now and frees
them on a new list, in `DeleteList`, which deletes only the unit the id names, and in its
destructor; so `GCSMSAddressList` frees its units too. Owners: `PacketFuzzFindings`'
`MemberReadsLeakNothing` and `GameListReadsThatThrowLeakNothing`, which measures the SMS list's
packet whole now that the test frees nothing for it. The `VS_UI` change has no test of its own: a
window cannot be built in the unit suite.

### 9. The gradation table is built on one green scale and read on another (PR #576)

Fixed: `GradationValue` and `GetIndexColor(WORD)` sum green's top five bits, as the original
build did (its `ColorDraw::Green` was `(c >> 6) & 0x1F`, `fa74958e`), so the table, its three
readers and `GetIndexColor` share one scale and white takes the ramp's first gradation. It is the
original's scale to within one: the port builds the ramp itself from a six-bit green, so five of the
table's 94 entries (sums 48, 66, 70, 84 and 88) come out one gradation brighter than the
original's, which an exact match would need the ramp's five-bit interpolation for. The
distance `GetColorToGradation` measures from `GradationValue[g-1]` is left: the original measures
it the same way, so the gradations it drew depend on it. Owner:
`SpriteGpu.TheGradationTableTakesWhiteToTheRampsFirstGradation`; no other test pinned a colour of
the old scale, since they read their expectations from the table.

### 5 (a). The text system's handling of a server string (PR #577)

Followed and found clean: [packet input safety](../protocol/packet-input-safety.md) writes it up
and says where it stopped (SDL_ttf's rasteriser and the backend's blit). Owner:
`tests/unit/test_text_wrap_bounds.cpp` (a 2048-byte row of mixed and malformed UTF-8 wrapped,
measured and drawn; widths of zero and one). Routes (b) to (d) are task 5.
