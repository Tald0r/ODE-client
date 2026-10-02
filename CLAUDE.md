# OpenDarkEden Client

Isometric horror MMORPG client — slayers, vampires, ousters. Korean original from
roughly 2000-2010, ported from Win32 + DirectX to SDL2. C++20, CMake, MSVC.

`README.md` is the human setup guide: prerequisites, vcpkg, generating the solution,
running the game, troubleshooting. **Link to it rather than restating it.** This file
is the working brief for an agent in this repo.

## Build

CI runs only on pushes to `master` and manual `workflow_dispatch` invocations.
Keep pull-request triggers disabled to conserve GitHub Actions minutes (user
instruction, 2026-09-25). Verify changes locally and use manual CI when needed.

Windows + MSVC is the live path. There are two Debug trees and neither subsumes the
other, because `/RTC1` and `/fsanitize=address` are mutually exclusive:

| Tree | Flags | Catches |
|---|---|---|
| `build/vs2022` | `/RTC1` | uninitialised locals, stack frame damage |
| `build/vs2022-asan` | `/fsanitize=address` | heap/stack overflow, use-after-free, double free |

Use the ordinary tree day to day; switch to the sanitized one to chase a memory error.
README has the full table. Configure prints exactly one of the two check-set lines — if
neither appears, the flags are wrong.

```bash
cmake --build build/vs2022 --config Debug -- -m
```

Ignore the Makefile's `make debug-asan` and friends: they target Unix build dirs
(`build/debug-asan`) and are not the path used here.

The **first** configure of a fresh tree must pass the vcpkg toolchain or SDL2 is not
found:

```bash
cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

`build/` is gitignored and bakes absolute paths into the generated projects —
regenerate locally, never commit it. `/MP` is set once for all targets in
`CMakeLists.txt`; if it ever looks missing, fix it there and never in a generated
`.vcxproj`, which the next configure discards.

### Reading build output

A clean Debug build is **57 distinct warnings (about 900 lines) and 0 errors**
on the CI runner (measured 2026-09-28), none of them in project code:
IXWebSocket's C4244/C4267, `third_party/`'s C4996 and the Windows SDK's
C4668. The project's own code builds without warnings under MSVC, Apple
Clang, GCC and Clang (`docs/compiler-warnings-2026-09-27.md`), so a warning
in `Client/`, `VS_UI/`, `basic/`, `tests/` or `tools/` is new: the budget
(`tools/ci/warnings.md`) fails on it. Clear a C4996 through
`basic/CrtCompat.h`'s portable spelling, never `_CRT_SECURE_NO_WARNINGS`.
The LNK4217 imports of `MStatusManager` are gone too: `__EX` no longer marks
classes compiled into the same image `dllimport`. Judge a build by `error C####`, `error LNK`, `error MSB`
or `fatal error` — never by grepping `"error"`, which matches ~2,700 identifiers such
as `GCMoveErrorHandler`. Redirect builds to a log file; piping through `tail` buffers
the output and hides all progress.

## Testability

### What can be tested

Only code compiled into a **static library**: `basic`, `SpriteLib`, `dxlib`,
`gamemodel` (the pure data tables, the action and effect tables
(`MActionInfoTable`, `MEffectSpriteTypeTable`, `MEffectStatusTable`,
`MCreatureSpriteTable`), the item table, the money manager, the item
core - `MItem`, with what an item requires and whether the player may
use it (`MItem::IsUsableBy`, which `MCreature::CheckAffectStatus` asks),
the gear families, the item managers, the containers
(inventory, storage, shop shelves), the trade manager over them, the gear the
three races wear and the shop, behind the `MItemHost` the executable installs,
the price manager behind its `MPriceHost`, the skill core (the info
table with a vampire's skill cost, Will of Life's cost and reuse time
and a skill's range at its level, the skill set, the domains and their
tree; what the player can use right now stays executable-side), and the
combat-stat preview
(`MStatusManager`: the character-select to-hit, defense, protection
and damage, and the attack speed `MPlayer::CalculateStatus` takes from
it), the chat filter (`MChatManager`'s curse lists and `RemoveCurse`,
over `MStringMap`, behind its `MChatHost`), and the creature status array
(`MStatus`), with `AffectModifyInfo`, which applies a ModifyInfo packet to
it, and the request/answer mode register (`TempInformation`), and the
eight packet handlers whose bodies reach only model state (the phone
slots `GCPhoneConnected`, `GCPhoneDisconnected`, `GCPhoneSay` and
`GCRing`; the trade box `GCTradeMoney` and `GCTradeRemoveItem`;
`GCSystemAvailabilities`; `GCMonsterKillQuestInfo`), which stay in
`Client/PacketHandler`, and `ApplySkillInfo`, the skill-model rebuild
`GCSkillInfoHandler` runs on its packet (the handler keeps the sweeper
bonus reset and `SetAvailableSkills`), and the duration conversions
`ConvertDurationToFrame` and `ConvertDurationToMillisecond`, and the event
queue (`MEventQueue`, inherited by the executable's `MEventManager`, with
gamma, effect and fade actions behind `MEventHost`; background images stay
executable-side), and screen-fade progression (`MScreenFade`, which `MTopView`
draws and advances with the frame number); with the user, config and timed-item
loaders gamemodel reads, and their string support; membership in
`tests/arch/gamemodel_files.txt` —
`docs/RESTRUCTURING.md` tasks 4.1, 4.2, 4.3, 4.4, 4.12, 4.13, 4.14, 4.15,
4.16, 4.17 and 4.18),
`framelib`, `TextSystem`, `VS_UI`, and `packetwire` — the whole wire layer: the
sockets (TCP and datagram), the socket streams, the `Player` base under both
player classes, the game-server player and the inbound peer player with its manager, the
encrypter, the info classes, every packet class in every direction and
the factory/validator tables (`docs/RESTRUCTURING.md` tasks 1.1, 2.4 and 5.1;
membership is `tests/arch/packetwire_files.txt`, read by CMake, the include checker
and the ratchet script). **Every `.cpp` under `Client/Packet` is a member**
since 2026-09-09; `tests/arch/packetwire_holdouts.txt` is empty and is where
the next exception, if one is ever needed, gets written down with what it
*reaches*, not what it includes. What the wire layer needs from the program
around it goes through `Client/Packet/WireHost.h`, 11 entries the executable
installs in `GameInit.cpp`. The logging facility (`DebugLog.h`) is in
`basic`, so every library may log; `Client/DebugInfo.h` is the executable's
front end to it and pulls in `MinTr.h`, which is why the libraries may not
include it. The checked formatter (`SafeFormat.h`, `docs/RESTRUCTURING.md`
task 5.4) is in `basic` for the same reason — the call sites that need it are
in the executable, in `VS_UI` and in the packet handlers, and `basic` is the
one library all three link. Game logic compiled straight into the `DarkEden` executable —
including the packet *handlers* under `Client/PacketHandler/`, all but the eight
`gamemodel` lists — cannot be linked into
a test binary. That is a structural limit, and it is the single biggest constraint on
how work gets verified here.

`decore` is a static library of its own: the server's shared rules
(`third_party/decore`), a byte-identical copy that is resynced with
`tools/decore/sync.pl` and never edited by hand, which `gamemodel` links
(`third_party/decore/README.md`). It holds the item price, repair price
and maximum durability, the grade policy, the per-race stat rules, the
equip requirements, the castle tax, the per-skill output formulas
(`SkillOutputFormulas`, of which the client calls Will of Life's) and a
slayer skill's range (`docs/RESTRUCTURING.md` task 4.12). Two ctests keep it honest: `decore_tests`
asserts the shared parity vectors on this toolchain, and `decore_vendored`
checks the copy against its manifest.

`unit_tests` links `basic`, `SpriteLib`, `TextSystem`, `packetwire`, `gamemodel`
and `dxlib`. The input pump reaches application state and text editors through
`DXInput::Host`, installed in `GameInit.cpp`; tests link the real adapter and
backend without game-global stubs. `ui_tests` links `VS_UI` for independently
reachable widgets; `user_option_tests` also links it for key bindings.
Packet tests construct real packets through the real
factories and pin their bytes against `tests/golden/*.hex` — 144 of those files
share a name with a server golden and 122 are byte-identical copies of it
(measured 2026-09-29 against the server's `c1157eb1`, with the two `LCPCList`
goldens). The other 22 differ: the server re-recorded `CGMove` and `GCMoveOK`
(every code, and the framed one), `CGSay` and `CGWhisper` with stronger
fixtures on 2026-09-16 (its `93883b4f`), and `CGBloodDrain`, `CGSkillToNamed`,
`CLLogin`, `GCAddItemToItemVerify.threeenchant`, `GCGuildChat`, `GCSay` and
`GCSystemMessage` hold other fixture values in the two repos. A slice that adds
a shared golden moves these counts. `diff -r` of the two golden directories
is the cross-repo wire check (`tests/unit/test_packet_goldens.cpp` has
the recipe and the `UPDATE_GOLDENS=1` re-record rule).

The **wire-layout inventory** (`tests/unit/test_wire_layout.cpp`,
`tests/wire-layout.txt`): packet id, name and max body size for every factory under
`Client/Packet`, produced from the real factory objects. `tests/tools/gen_wire_inventory.pl`
writes `tests/generated/WireInventory.inc`, one include and one registration per
factory class; the test constructs every factory and its packet (the link proof for
the written CG/CL directions, which no manager ever creates), checks the ids are
unique, and checks that every id `PacketFactoryManager` registers is a listed factory
at the factory's own size. Re-run the generator after adding or changing a factory
(the `wire_inventory_fresh` ctest fails otherwise) and re-record with
`UPDATE_GOLDENS=1`. The server repo commits the same file from its own packet
classes; `server/tests/tools/wire_inventory_diff.sh` diffs the two, and a diff there
is a protocol bug in one repo or the other — see `RESTRUCTURING.md` task 1.4 in the
server repo for the findings and their status. The Rpackets factories are constructed
and checked but kept out of the rendered file, because the server deleted its copies.

### The framework

`tests/framework/test_framework.h` — a minimal self-registering framework, not
GoogleTest. Tests register during static initialisation and `tests/unit/*.cpp` is
globbed, so adding a test means adding a file; there is no runner to edit.

```cpp
TEST(TArray, LoadFromFileRejectsCountLargerThanTheFile)
{
	CHECK_EQ(false, arr.LoadFromFile(truncated));
}
```

`CHECK(expr)` and `CHECK_EQ(expected, actual)` are the whole API. `RunAll()` returns
the failure count, so the exit code is 0 only when the suite is clean.

### How we work

- **Library fixes are written test-first.** Every fix in the remediation table of
  `docs/code-health-review-2026-08-29.md` was.
- **Assert the observable contract, not the crash.** An out-of-bounds read in C++
  usually returns garbage rather than failing an assertion, so a memory-safety test
  asserts the rejected input or the preserved value.
- **Then run the same suite under ASan**, where the invalid access aborts the process.
  Both trees green, or the fix is not verified.
- **Executable-only code cannot be linked by the unit binary.** Verify changes
  with builds and available automated regression checks. **Live-server verification
  is optional and must not block a merge or completion** (user instruction,
  2026-09-17); do not request a runtime report as merge approval. The ten defects
  under *Runtime defects* in the review were found against a live server, and none
  were reachable from a test binary. The ones in the
  short table below them were found by *reading*, during remediation passes, and are
  filed separately for exactly that reason — the heading is a claim about how a
  defect was found, not a bin for anything executable-side.
- A fix that could not be reproduced is called a **regression guard** in its commit
  message, not described as a reproduction.
- Substantial remediation work gets an **adversarial review** afterwards. The last one
  returned "significant problems" and found three real defects the fixes had introduced
  or missed — assume your own fixes deserve the same scrutiny.

### Running them

```bash
cmake -S . -B build/tests -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/tests --config Debug --target unit_tests -- -m
cd build/tests && ctest -C Debug --output-on-failure
```

Add `-DUSE_ASAN=ON` in a separate tree for the sanitized run. `BUILD_TESTS` defaults
to `OFF`, so a tree configured without it generates no test target at all. Baseline
measured on 2026-09-30 on the skill-info branch merged with master at
`71159e1c`, so the tree holds both. From the branch
(`docs/RESTRUCTURING.md` task 4.16): `ApplySkillInfo`, the rebuild
`GCSkillInfoHandler` runs, on real packets of every race in
`test_skill_info.cpp`, with the two fixes that followed (a skill type past
the info table, a duration's milliseconds past the int range). From
master: PR #298, the datagram type-confusion, bug-report and
empty-datagram fixes, the test that a refused datagram id creates no
packet, and the datagram fuzz target's goldens, with their 14 tests and
586 checks. Master alone read 1463 tests and 1,458,200 checks; the merged
tree reads **1483 tests, 1,458,777 checks, 0 failed**, identical in all
four builds, this run (`decore_tests`: 13 tests, 2009 checks on the same
four). Linux:
`unit_tests` built by `tools/ci/verify-linux.sh linux` (GCC 13.3) and
`linux-clang` (Clang 18.1) in the Docker image, its native arm64 on an Apple
Silicon Mac, with the totals read by running `build/presets/<preset>/bin/unit_tests`
in it (the scripts stop at the warning step, which has no `aarch64`
baseline, after ctest passed). macOS: Apple Clang 21, the `macos` preset, read with
`build/defects/run-tests.sh unit_tests ''`, and the `macos-asan` preset's
`unit_tests` under `ASAN_OPTIONS=detect_leaks=0` and
`UBSAN_OPTIONS=halt_on_error=1`, with no ASan or UBSan report. Run from a git
worktree, the container also needs the common git directory mounted, at its
host path or with `GIT_DIR` and `GIT_WORK_TREE` set, or `ratchets` and
`source_encoding` fail on "not a git repository". The Windows trees were not
re-measured for this figure, and no CI run produced it. A
platform-conditional test can make the check totals differ by one between
platforms; that is not a failure. The Linux recipe is the `linux`,
`linux-clang` and `linux-asan` presets in `CMakePresets.json`, the macOS one
the `macos` and `macos-asan` presets.
**Clang's UBSan checks enum loads and GCC's does not**: a wire byte cast to
an enum before its range check passed the Linux job and aborted the first
macOS sanitizer run, so a Clang sanitizer build (the `macos-asan` preset, or
the Linux container with `clang++` and the `libclang-rt` package) is the
one that sees that class.
`DarkEden` builds and links on both, and on Linux run headless
(`SDL_VIDEODRIVER=dummy`) with the data tree beside it reaches the main menu and
exits cleanly on `SDL_QUIT`; login and beyond are unverified off Windows (the
port assessment's area F). **The macOS CI job** (`.github/workflows/macos.yml`,
arm64 and Intel runners, invoked on master pushes or manually) has not
produced the totals above. One Apple Silicon Mac (macOS 27.0, Apple Clang 21)
built every target and ran the `macos` preset's 20 ctest tests green on
2026-09-30 (the three fuzz replay tests and their corpus steps among them;
`verify-linux.sh` and `verify-windows.ps1` require 16 of them by name,
`fuzz_replay_client_datagram` the latest), and is where the macOS
totals above were read; nothing has been watched on a Mac's display, and a
`<SDL2/...>` include spelling breaks the Homebrew build - it is `<SDL.h>`
everywhere (`basic/Platform.h` says why).

### Fuzzing the packet readers

Two libFuzzer targets read the bytes a server sends the client, one per
connection of its one `ClientPlayer`: `tests/fuzz/fuzz_client_stream.cpp`,
the game connection, with the player in `CPS_NORMAL` (every registered id
accepted), and `tests/fuzz/fuzz_client_login_stream.cpp`, the login
connection, which reads every input once in each of the seven statuses the
player holds there (`CPS_AFTER_SENDING_CL_LOGIN` to
`CPS_AFTER_SENDING_CL_SELECT_PC`), so a frame reaches a parser only when that
status's validator set accepts its id, as in production. Both read through
`tests/fuzz/client_stream_reader.h`: `[encrypt code byte][stream]`, read
frame by frame through the same gates as `ClientPlayer::processCommand` (its
header lists them with their line numbers; keep the two in step), once from
the start of the input ring and once with the ring's wrap point in the middle
of the stream. Only a `Throwable` is a rejected input, since that is all
`UpdateSocketInput` catches; a `std::exception` from a reader is a crash, as
in the client. A third, `tests/fuzz/fuzz_client_datagram.cpp`, reads one
UDP datagram arriving at the client's peer-to-peer socket, which is bound on
every interface at start-up and authenticates nothing: the raw datagram
(header, body, pad; no code byte), through `Datagram::read` and the
`CPS_CLIENT_COMMUNICATION_NORMAL` validator check, as
`ClientCommunicationManager::Update` does, stopping before dispatch. Every
native test tree builds each target with
`tests/fuzz/replay_main.cpp` as `fuzz_replay_<target>`, and the ctest of
that name replays two sets of inputs through it: the seed corpus, which the
`fuzz_corpus_<target>` setup step writes into the build tree with
`tools/fuzz/golden2corpus.pl` (one seed per golden of the connection's
packets, plus an empty and a zero-filled frame per id: 536 `GC` seeds for
`client_stream`, 32 `LC` seeds for `client_login_stream` today; its
`--datagram` mode writes the 12 `RC` seeds of `client_datagram` from the
four `RC` `.datagram.` goldens, with the pad byte those leave out), and
`tests/fuzz/regressions/<target>/*.hex`, one file per fixed crash in the
goldens' hex style (`xxd -p crash-... | tr -d '\n'`), named after the packet
and the value. A crash there is a finding that came back, but only a
finding that aborted (a libc `assert`, an escaping `std::exception`)
crashes in every tree. One that corrupted memory replays green in a tree
without a sanitizer that sees it: on the unfixed code the `LCPCList` slot
input replays green in the plain `macos` tree and stops at an invalid
`Slot` load under Clang's UBSan. So each fix also carries a test in
`tests/unit/test_packet_fuzz_findings.cpp`, which catches it in every tree
(the datagram type confusion, found by reading, is pinned in
`tests/unit/test_datagram_frame.cpp`).

The fuzzer itself needs Clang with a libFuzzer runtime and ASan
(`BUILD_FUZZERS`, off by default, refused on other compilers):

```bash
cmake --preset macos-fuzz        # or linux-fuzz
cmake --build --preset macos-fuzz --target fuzz_client_stream fuzz_client_login_stream fuzz_client_datagram
perl tools/fuzz/golden2corpus.pl tests/golden tests/wire-layout.txt /tmp/fz/corpus '^GC'
cd /tmp/fz && ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  <repo>/build/presets/macos-fuzz/bin/fuzz_client_stream -max_total_time=600 \
  -timeout=10 -rss_limit_mb=2048 -max_len=32768 -close_fd_mask=3 corpus
```

The login target is the same run with its own seeds and binary, in its own
scratch directory:

```bash
perl tools/fuzz/golden2corpus.pl tests/golden tests/wire-layout.txt /tmp/fz-login/corpus '^LC'
cd /tmp/fz-login && ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  <repo>/build/presets/macos-fuzz/bin/fuzz_client_login_stream -max_total_time=600 \
  -timeout=10 -rss_limit_mb=2048 -max_len=32768 -close_fd_mask=3 corpus
```

`-max_len=32768` is the longest input the harness reads: the code byte
and a stream of at most 32767 bytes, one less than the 32768-byte input
ring; a longer input returns without being read.

The datagram target has its own seed mode and a longer cap, 65536 bytes,
the most `recvfrom()` hands `DatagramSocket` (`DATAGRAM_SOCKET_BUFFER_LEN`):

```bash
perl tools/fuzz/golden2corpus.pl --datagram tests/golden /tmp/fz-dgram/corpus '^RC'
cd /tmp/fz-dgram && ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  <repo>/build/presets/macos-fuzz/bin/fuzz_client_datagram -max_total_time=900 \
  -timeout=10 -rss_limit_mb=2048 -max_len=65536 -close_fd_mask=3 corpus
```

The recipes set `detect_leaks=0`, so no run looks for leaks. libFuzzer's
"disabled leak detection after every mutation" notice, which every
login-target run past its first crash and a game-target run printed (logs
checked 2026-09-30), says only that over a thousand inputs ended with more
allocations than frees. It names no allocation and has not been traced to
any, so it is neither a finding nor a clean bill: a leak in a reader is
found by reading (the review's `LCServerList`/`LCWorldList` row).

Run it from a scratch directory: a failed `Assert` appends to
`assertion_failed.log` in the working directory. Hostile input to the game
target fails about a thousand in fifteen minutes; the login target failed
none in 25 minutes. The datagram target failed none in 16 minutes run
with `DE_FUZZ_ABORT_ON_ASSERT=1` (the `RC` readers hold no `Assert`;
`Datagram::read` holds two). Apple Clang ships no libFuzzer, so
`macos-fuzz` compiles with Apple Clang and links Homebrew `llvm@21`'s
`libclang_rt.fuzzer_osx.a` (`DARKEDEN_LIBFUZZER_ARCHIVE`); llvm@21's own
ASan hangs at startup on macOS 27. The preset names the Apple Silicon
Homebrew path of `llvm@21`; on an Intel Mac (Homebrew under `/usr/local`)
or with another LLVM version, configure stops with "does not exist" until
you pass `-DDARKEDEN_LIBFUZZER_ARCHIVE=<path to libclang_rt.fuzzer_osx.a>`.
With that hybrid, `-fork` reports an ASan container-overflow inside
libFuzzer's own merge code, not in ours, so fuzz single-process on a Mac.
`linux-fuzz` uses the distribution Clang and `-fsanitize=fuzzer`, which
needs the `libclang-rt-18-dev` package; the
`darkeden-linux` image does not carry it yet, and `apt-get install` of it in
the container is enough (done on 2026-09-29: the target built, fuzzed for two
minutes without a finding, and `unit_tests` and the fuzz ctests passed under
that preset's Clang ASan and UBSan). `-close_fd_mask=3` hides the
crash's own message, so triage by replaying the file through the replay
binary of an ASan tree:
`build/presets/macos-asan/bin/fuzz_replay_client_stream crash-...` (or
`fuzz_replay_client_login_stream`, `fuzz_replay_client_datagram`).
`DE_FUZZ_ABORT_ON_ASSERT=1` turns a failed `Assert` (an `AssertionError`,
normally a rejected input) into a crash, to see which ones hostile input
reaches. The findings so far are under *Found by fuzzing* in the review.

## Traps

- **`_DEBUG` IS defined in MSVC Debug builds.** `CMakeLists.txt:16` only declines to
  *add* it; MSVC defines it automatically under `/MDd`, which CMake cannot undo. The
  ~50 `#ifdef _DEBUG` blocks in shipped code are therefore **live in every Debug
  build** and dead only in Release. Both directions of this trap have bitten: the
  `_DEBUG`-guarded bounds checks (e.g. `CTypeTable::operator[]`) do run in Debug but
  vanish in Release, and upstream's `_CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF)` in
  `Client.cpp` — long believed dead — ran on every Debug launch and made the CRT
  retain every freed block, which presented as a ~200 MB/min in-game "memory leak"
  (root-caused and removed 2026-08-30, see
  `docs/memory-leak-investigation-2026-08-30.md`). Judge every `_DEBUG` guard by the
  build configuration, not by the old doctrine that it never fires.
- **An unlisted DLL kills the client before `main` does anything.** `Client.cpp` walks
  `*.dll` in the working directory against a hardcoded whitelist and does a bare
  `return -1` on the first name it does not recognise — no message, no log line, and
  well before `InitGame()` sets up logging. Adding any library that deploys a new DLL
  beside the executable therefore breaks startup until that DLL is added to the list.
  A silent `-1` exit with no log written is this check until proven otherwise, and note
  that the DLL stays in the output directory after you revert the build change that
  brought it in, so reverting alone does not restore a working tree.
- **Don't launch `DarkEden.exe` yourself** unless asked. It is normally run from the
  Visual Studio debugger, and launching it separately steals the stack trace and locks
  the build output. When a startup failure has to be bisected, ask — running it is
  sometimes the only way to see the exit code, and no test binary can reach that code.
- **Packed resource reading is restored.** `CRarFile` first reads a loose override
  beside its `.rpk`, then falls back to the password-protected archive. The pinned
  static UnRAR dependency reads into bounded memory and never extracts to disk.
  `GetList` returns owned regular-member names from that archive. Keep its notices
  beside distributed binaries; see `third_party/unrar/README.md`.
- **The 5:5:5 sprite paths are latent.** `ColorDraw::Is565()` returns a hardcoded
  `true`, so the `CSprite555` family is never constructed and fixes there have no
  runtime effect today.
- **Sprite rejection is silent** — a rejected sprite is dropped with no log line and an
  ignored return value, so bad art would vanish without a signal.
- **`grep` stops reading `VS_UI/src/VS_UI_GameCommon.cpp` at its first NUL byte.**
  The file carries three NUL bytes, so ripgrep and `grep -I` class it as binary and
  either skip it or stop at the first one; a tree-wide count that does not pass
  `grep -a` (or read the file in binary mode, as `count_identifier.pl` does)
  undercounts by whatever that file holds - 296 of the tree's ~364 `wsprintf` calls,
  for one. Two counts in the port assessment were wrong by a factor of two for this
  reason (found 2026-09-08). Force text mode for any measurement over `VS_UI/`.

## Conventions

- **Tabs**, not spaces, in C++ sources. There is no `.clang-format` and `make fmt` is a
  stub, so match the surrounding file by hand.
- **English only.** Remaining Korean and Chinese comments are upstream's; translate them
  when you touch that code, and never add more.
- Hungarian notation (`m_pFoo`, `g_pBar`, `bFlag`) and banner comments above functions
  and sections — follow the file you are in.
- Commit messages: `type: lowercase imperative summary`, then prose covering why the
  change is right, what was verified, and what is deliberately out of scope. `a41eec9`
  is the model. A `fix:` commit also carries a `Test path:` line (`lib + test`,
  `moved, then fixed` or `exempt` - `docs/RESTRUCTURING.md` task 3.1); the
  `tools/git-hooks/commit-msg` hook refuses one without, after
  `git config core.hooksPath tools/git-hooks` once per clone. Trailer: `Co-Authored-By: <the authoring model> <noreply@anthropic.com>`, e.g. `Claude Fable 5.1`.

## Layout

| Path | What |
|---|---|
| `Client/` | game logic — `GameMain`, `MZone`, `MCreature`, `MPlayer`, `MItem`, `MSkill` |
| `Client/Packet/` | the wire layer, compiled once as `packetwire`; `Gpackets/` is server → client |
| `Client/PacketHandler/` | packet handlers, executable-side but for the eight `gamemodel` lists (task 4.15), all bound to ids in `Client/PacketHandlerRegistry.cpp` |
| `Client/SpriteLib/` | sprite decode and blitting, SDL backend, the 555/565 variants |
| `Client/DXLib/` | input, sound and music behind a DirectX-shaped interface, SDL underneath |
| `Client/TextSystem/`, `TextLib/` | UTF-8 text rendering on SDL + freetype2 |
| `Client/D3DLib/` | compatibility stub only; `CDirect3D::GetDevice()` returns `nullptr` |
| `Client/framelib/`, `VolumeLib/`, `DEUtil/`, `MZLib/` | frames, collision, utilities, compression |
| `VS_UI/` | UI framework — widgets, dialogs, skinning, Korean IME |
| `basic/` | memory, exceptions, typedefs, platform abstraction |
| `tests/` | framework and unit tests |
| `docs/` | code health review; `RESTRUCTURING.md`, the extraction plan, complete as of 2026-09-09 - its *What the review rounds settled* section is the standing rulebook for library work |

## Current focus

`docs/code-health-review-2026-08-29.md` holds 197 findings, 88 fixed — every
Critical among them. In priority order:

1. **Unvalidated network input is the top open risk**, and the two halves of it
   now have instruments rather than estimates. `Client/Packet/Gpackets/` passes
   server-supplied lengths, indices and item classes into array subscripts,
   `strcpy`/`sprintf` targets, and a function-pointer table.
   - **Lengths into copies: ratchet R3, at 0.** Every `sprintf`/`strcpy`/`strcat`
     under `Client/Packet` and `Client/PacketHandler` is bounded or gone, so a
     new one fails the suite.
   - **Indices into subscripts: ctest `packet_indices`**, over `Client/Packet`
     and `Client/PacketHandler`. It walks the *value*, not the spelling —
     `array[pPacket->getSlotID()]` has two live instances while
     `int slot = pPacket->getSlotID();` twenty lines above `array[slot]` has a
     hundred. **103** packet-indexed subscripts, 91 of them into a named,
     verified `CTypeTable` that range-checks itself, **12 into a container that
     is not**, all guarded. A thirteenth fails the suite and has to be read.
     Seven of the 12 are in the phone handlers `gamemodel` compiles (task
     4.15), and `test_model_handlers.cpp` runs every out-of-range slot
     byte (3 to 255) past each of them.
     It **fails closed**: a container it does not recognise counts as raw, so
     adding a name to its allowlist is a deliberate act. Its first version
     hardcoded the receiver name `pPacket` and so was blind to the 19 handlers
     that call their parameter something else — the same
     measure-the-spelling mistake it was built to answer for R7, which is why
     its header is long.
   - Both passes found live defects, listed under *Found by reading* in the
     review — including one that needs no hostile server at all.
   What remains unaudited is everything the two instruments do not model: a
   length or index that reaches memory by some third route. The packet-read
   fuzz targets (*Fuzzing the packet readers* above) are the first check that
   does not model a route at all: they drive every `read()` with hostile bytes
   under ASan and UBSan, and found a heap overflow in `StoreInfo::read` and an
   out-of-array store in `LCPCList::read` that neither instrument counts. They
   stop at `read()`; handlers are not reached.
2. Fixed-size buffers fed by variable-length server strings (the 21-byte chat rows
   are fixed; 128-byte stack buffers remain in other handlers), and format strings
   loaded from data files passed to sprintf (C19/C20/C22). That last one is
   **closed** (C19, 2026-09-04, task 5.4's fifth slice). **315 sites** are
   converted to `SafeFormat::Format` in `basic/SafeFormat.h`, which checks a
   table entry's conversions against the arguments the call site really passed,
   across `Client`, `VS_UI`, the `AddFormat` family (through
   `CMessageArray::AddSafeFormat`) and `MString::FormatChecked`.
   **Two ratchets hold it, and the pair is the point.** R7 counts a lookup
   *spelled at the format argument*; R8 counts every printf-family call whose
   format is not a string literal, whatever it is spelled as. This file claimed
   C19 was closed once before, for about an hour on 2026-09-04, on R7 alone —
   and 24 live sites were reading the entry out of a static array or a local
   first, invisible to it. **A ratchet at zero is a claim about the ratchet.**
   If you need to know the state of this finding, read the C19 entry in the
   review: it lists five measurements, and R7 is one of them. If you need to
   look for a new site, sweep the *format argument*, never another spelling of
   the lookup.
   `tests/tools/check_format_arity.pl` (ctest `format_arity`) audits every
   converted site against the built-in English table and fails the suite when an
   entry asks for more arguments than its call site passes; it also ratchets how
   many sites it finds *and* how many it resolves, because it has three times
   passed while silently checking less than it should.
3. Dead and duplicate source sitting alongside live code, which is a correctness trap
   when the wrong file gets edited.
