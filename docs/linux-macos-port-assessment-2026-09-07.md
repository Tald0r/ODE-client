# Linux and macOS port assessment

**Date:** 2026-09-07

**Scope:** what has to change for the `DarkEden` executable to build and run
on Linux and macOS, in what order, and how much of it there is. This is a
sizing document and a plan; it changes no code. It is the "separate
assessment" that `docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md`
deferred the OS port to.

**Current live platform:** Windows x64, Visual Studio 2022, vcpkg, SDL2.

## Executive conclusion

This is a **revival plus a first Linux bring-up**, not a port from scratch.

The tree descends from upstream's macOS SDL port (January to February 2026,
last commit `2e12ebb8` "Windows Dependency Cleanup", which recorded "Active
Windows API calls: 0" and a green macOS build that reached login and zone
rendering). This fork then spent 447 commits over 2,037 files making the same
tree build under MSVC. Since then:

- the non-Windows branches of every `#ifdef PLATFORM_WINDOWS` have not been
  compiled, and
- **Linux has never been configured at all** - every non-Windows branch in
  CMake spells the platform as `PLATFORM_MACOS`, and the Linux macro the socket
  layer tests for is defined by nothing.

The language half of the problem is already paid for. The `conformance/*`
branches remove the dynamic exception specifications and `register` uses
that GCC and Clang reject under C++20, and `conformance/clang-fixes`
(`1666a9dc`) builds every target including `DarkEden` with clang-cl 19, unit
binary 596 tests, 0 failed. What clang-cl does not prove is the Win32 and
MSVC CRT surface (it targets the MSVC ABI) and the non-Windows branches. That
is the work here.

| Delivery level | Estimated effort for one engineer | Result |
|---|---:|---|
| Libraries and `unit_tests` green on Linux GCC and Clang, ASan and UBSan | **8-13 working days** | The same 590+ tests the Windows trees run, plus the sanitizer coverage Windows cannot give. No executable. |
| `DarkEden` links and reaches the title screen on Linux | **+6-9 working days** | Entry point unified, Win32 startup paths guarded, CRT residue shimmed. |
| Login, zone, chat and shutdown verified on Linux against a live server; macOS on top; CI on every push | **+11-18 working days** | The supported outcome. |

**Expected total: 25-40 engineer-days after the conformance branches
merge, touching roughly 150-250 files, most of them one line.** The spread
is dominated by the runtime bring-up, because nothing has run off Windows
since February, nothing has ever run on Linux, and no test binary reaches
executable-only code. Everything above the runtime is measurable by a
compiler and sized accordingly.

**Recommendation:** branch from the conformance merge, run a Linux compile
probe in a container on day one and let its error list replace the
estimates below, then take the libraries green with tests before touching
the executable.

## What "supported" means here

Three outcomes are distinct and should be named separately:

1. **Builds:** every configured target compiles and links on Linux and
   macOS with GCC and Clang.
2. **Runs:** the client starts from `bin/`, finds its data, logs in, enters a
   zone, chats and exits cleanly against the docker server, on both
   platforms.
3. **Stays that way:** a Linux job in Actions on every push, and a ratchet on
   the platform-macro spellings, so the port cannot rot again the way the
   macOS path did between February and August.

Only all three together is "supported". The estimate covers all three.

## Measured state

### Already done, needs no work

- **DirectX is gone.** No live translation unit includes `ddraw.h`,
  `dsound.h`, `dinput.h`, `dmusic.h` or `d3d*.h`. `Client/D3DLib/` no longer
  exists (CLAUDE.md still lists it). `Client/DXLib/` has one backend,
  `DXLibBackendSDL.cpp`; `Client/SpriteLib/` has zero GDI; `Client/TextSystem/`
  has zero Win32 identifiers; Korean IME already runs through
  `VS_UI/src/hangul/Ci_macOS.cpp` on `SDL_TEXTINPUT`/`SDL_TEXTEDITING`;
  clipboard, DirectShow, MCI video and Bink: zero.
- **`basic/Platform.h` (2,081 lines) and `basic/PlatformSDL.cpp` (554)** are
  the shim: fixed-width `BYTE/WORD/DWORD/LONG/BOOL`, pthread-backed
  `CRITICAL_SECTION`, `RECT/POINT/SIZE`, a `WideCharToMultiByte`, threads,
  mutexes, events, ticks, dynamic libraries, and a config store in place of
  the registry, plus about sixty no-op Win32 stubs. 95 of the 105 direct
  `windows.h` includes are already `#ifdef PLATFORM_WINDOWS ... #else
  Platform.h`; the ten unguarded ones are dead code or CMake-excluded.
- **MSVC language extensions are near zero.** Zero live inline assembly (the
  one `_asm` block in `CSprite.cpp:931` is inside a comment), zero SEH, zero
  `#pragma pack`, `#pragma comment`, `strcpy_s`-family, `<tchar.h>`, `_mbs*`
  or `%I64d`. Include-path case: 16 mismatches in 10,585 directives, all in
  comments; zero backslash includes outside comments. Source encoding: 1,683
  of the 1,687 files with non-ASCII bytes are UTF-8, GCC and Clang's
  default; three are CP949 (`Client/MFakeCreature.h`, `Client/MPlayer.h`,
  `Client/TextSystem/TextBackendSDL.cpp`). No x86 intrinsics outside a
  `PLATFORM_WINDOWS` guard, so Apple Silicon is not blocked.
- **CMake is about 85% cross-platform by construction.** 2,129 lines over
  nine files; about 135 are strictly MSVC and every one sits in a block that
  is skipped off MSVC or already has a GCC/Clang `elseif` (the sanitizer
  wiring at `CMakeLists.txt:113-134` already emits `-fsanitize=...`). Every
  dependency comes through `find_package` (SDL2, SDL2_image, SDL2_ttf,
  SDL2_mixer, Iconv, JPEG, Threads); `vcpkg.json` is platform-neutral; the
  Makefile is a Unix wrapper; the test scripts are bash and perl with LF
  pinned in `.gitattributes`.

### Owned by the conformance stream

Not counted here, and not to be redone:

- dynamic exception specifications: 9,447 sites in 558 files
  (`conformance/packet-*`, `conformance/executable-exception-specs`);
- `register`: 643 sites in 27 SpriteLib files (`conformance/remove-register`);
- the two ISO defects clang-cl found and the `windows-clang` preset
  (`conformance/clang-fixes`).

Without that merge the first Linux compile stops in
`Client/Packet/SocketAPI.h` at the first `throw(...)`.

### Remaining work, by area

**A. Build contract.** Small, and gating: nothing configures for Linux
until it is done.

- `PLATFORM_MACOS` is defined for every non-Windows target:
  `CMakeLists.txt:332, 395, 462, 617-619, 1096` and
  `Client/DXLib/CMakeLists.txt:33`. `tests/CMakeLists.txt:78-80` adds
  `__WIN32__ __WINDOWS__` on Windows and has no non-Windows `else`, so off
  Windows the test translation units and `packetwire` already preprocess
  differently - a vtable/ODR hazard on top of a porting task.
- Four spellings of "Linux" coexist and **the build defines none of them**:
  `__LINUX__` (63 uses in 14 files, mostly `Client/Packet/SocketAPI.cpp`,
  `PacketFileAPI.cpp` and `Types/SystemTypes.h`), `_LINUX` (1,
  `Client/CSystemInfo.cpp:44`), `__linux__` (8, compiler builtin) and
  `PLATFORM_LINUX` (8, from `Platform.h:38`). On Linux `SocketAPI.cpp:15-22`
  therefore includes neither the Winsock nor the BSD headers, and
  `SystemTypes.h:61-67` leaves `separatorChar` undeclared. CMake documents
  this exact failure mode for the Windows side at `CMakeLists.txt:1104-1110`
  and never fixed the Linux side.
- Three `list(FILTER ... "^Client/...")` regexes at `CMakeLists.txt:946, 950,
  951` never match because the glob yields absolute paths. Two are
  documented; the third silently keeps every root-level `*Handler.cpp` in
  the non-Windows executable.
- `Client/Client.cpp` (4,508 lines, defines `WinMain`) is appended to `VS_UI`
  on non-Windows at `CMakeLists.txt:535-537`, labelled "unverified".
- Pre-`project()` Homebrew include hacks at `CMakeLists.txt:4-8` and
  `197-201`; `SDL2::SDL2main` linked for the five viewer tools only
  `if(WIN32)` (`:681, 720, 749, 781, 822`), which is backwards for macOS; no
  `RPATH`; `.gitignore`'s blanket `*.cmake` swallows any new `cmake/*.cmake`.
- Presets and CI are Windows-only (`CMakePresets.json`,
  `.github/workflows/windows.yml`), and `tests/ratchet/ratchets.sh:145-172`
  R1 counts `<ClCompile>` entries in a `.vcxproj` and fails open without one.

**B. Shim hygiene.** Small edits with a wide blast radius; each is a runtime
trap rather than a compile error.

- `Platform.h:25-29` `#define assert(e) ((void)(e))` disables every assert
  off Windows before `<assert.h>` loads.
- `Platform.h:1889-1895` defines `min`, `max`, `TRANSPARENT`, `IN`, `OUT` as
  macros, which collide with `<algorithm>` and function parameters on
  libstdc++ and libc++.
- Silent-success stubs: the registry functions at `:1614-1648` return
  `ERROR_SUCCESS` with empty data, `CreateFontIndirect` at `:355` returns
  `(void*)1`, `EnumDisplaySettings` reports a fixed 1024x768, `MessageBox`
  prints to stderr, and `wsprintf` is unbounded.
- `WaitForSingleObject`, `WAIT_OBJECT_0`, `SetThreadPriority` and
  `LPTHREAD_START_ROUTINE` are shimmed in `Client/MWorkThread.h:57-84`, a game
  header, not in `Platform.h`; `Client/GameMain.cpp`, `CGameUpdate.cpp` and
  `MWorkThread.cpp` reach them only through that include.
  `sizeof(CRITICAL_SECTION)` depends on include order
  (`Client/ProfileManager.h:116-126`, `Client/GameInitInfo.cpp:11`).
- Two divergent `Client_PCH.h` files, `VS_UI/Client_PCH.h` and
  `Client/Client_PCH.h`, both define `RECT/POINT` off Windows and are chosen
  by include-path order across 1,125 translation units.
- Unshimmed or unguarded: `OutputDebugString`
  (`Client/LeakMemoryDumper.cpp:96-102`), NetBIOS MAC-address collection
  (`Client/PacketFunction.cpp:5485-5524`, in a file compiled on all
  platforms), a raw `CreateThread` (`Client/GameInit.cpp:1378`),
  `CreateToolhelp32Snapshot` (`Client/CGameUpdate.cpp:5894`).

**C. Wire-layer widths.** Small and correctness-critical; library code, so
test-first.

- `Client/Packet/Types/SystemTypes.h:47` `typedef unsigned long ulong;` is
  32 bits on Windows and 64 on LP64 Linux and macOS, and `ulong` reaches the
  wire: `Datagram.h:62,83` (`read(ulong&)`, `write(ulong)`),
  `Encrypter.h:44-57` (`m_ulongCode`, `convert(long)`, the packet cipher key),
  `ModifyInfo.cpp:161`. The read side already narrows through a temporary;
  the write side does not.
- `DWORD` is `unsigned long` under MSVC and `uint32_t` in the shim: the same
  width but a different type, so overload sets and `static_assert`s that
  name `DWORD` resolve differently. The typed-scalar work already stages
  `ObjectID_t` through `std::uint32_t` for exactly this reason.
- `SocketAPI.cpp:31` `extern int errno;` has to go; `errno` is a macro on
  glibc and Darwin.

**D. Entry point and startup.** Medium; executable-only, so no test path.

- `Client/SDLMain.cpp` (572 lines, wrapped in `#ifndef PLATFORM_WINDOWS`) is
  a hand-copied partial of `WinMain`. It re-declares about twenty globals by
  hand at `:224-241` with the wrong types - `extern bool g_MyFull` against
  the definition `BOOL g_MyFull` at `Client.cpp:230`, `extern int
  g_SECTOR_WIDTH` against `LONG` - which links silently on GCC (variable
  names do not mangle their type) and writes the wrong width. It has never
  had a green build to diverge from.
- `Client/Client.cpp:3007-3008` takes `GetModuleFileName` and
  `strrchr(g_CWD, '\\')`, and returns `FALSE` when there is no backslash;
  `:3110-3128` a `CreateMutex` single-instance lock; `:706-760` a real Win32
  window class, progress window and message pump for the patcher;
  `:3251-3290` and `GameInit.cpp:1606` the DLL whitelist; `:3531` the one
  live registry read; five `_chdir` calls.

**E. CRT and identifier residue.** Mechanical.

- About 85 live CRT call sites in about 35 files (`_chdir`, `_getcwd`,
  `_mkdir`, `_access`, `stricmp`, `_snprintf`, `_vsnprintf`, `_itoa`, and
  `__int64` 21 times in 9 files). Over half already resolve through
  `Platform.h` defines. About 140 `TCHAR`/`LPCTSTR`/`_T()` uses in about 25
  files. 131 `#pragma warning` lines, harmless with `-Wno-unknown-pragmas`,
  which is set today for `basic` alone.
- 32 includes of MSVC-only headers (`<io.h>`, `<direct.h>`, `<process.h>`,
  `<crtdbg.h>`, `<intrin.h>`), mostly guarded.
- `__declspec`: 81 uses, 26 in the vendored `VS_UI/src/Imm/` force-feedback
  SDK (excluded off Windows), two live.

**F. Runtime: paths and data.** Medium, and the one area macOS hides.

- `Data/Info/FileDef.inf` stores its paths with backslashes
  (`Data\\Image\\Etc.spk`), and backslash literals appear in 35 source files
  (`Client/MHelpMessageManager.cpp` alone has 26). Normalisation belongs
  where paths are read, not only in literals.
- **Measured against the shipped data tree: of the 206 entries in
  `FileDef.inf`, 152 match the on-disk name exactly, 38 exist only under a
  different letter case, and 16 are missing on Windows too.** Linux's
  filesystem is case-sensitive, so those 38 opens fail there; macOS's
  default APFS is case-insensitive and hides it. The other tables (sounds,
  UI skins, sprite packs) add to the count. The fix belongs in one place: a
  case-insensitive open in `basic/`, folding each path component the way
  `basic/DirectoryListing` already folds names, or a one-time normaliser of
  the shipped tree with lowercase lookups.
- Data is found relative to the working directory (`GameInit.cpp:1503`), and
  only the MSVC-only `VS_DEBUGGER_WORKING_DIRECTORY` establishes that today.
  `platform_get_executable_dir` exists in `PlatformSDL.cpp` off Windows; the
  code-health review recorded two one-byte overflows in it.
- Half the data is still packed (`.rpk`, the CLAUDE.md trap). Same on every
  platform, not port work.

**G. macOS specifics.** Small on top of Linux.

- SDL2 discovery through Homebrew or vcpkg, `SDL2::SDL2main` on every
  executable, the `-I/opt/homebrew` hack removed, `TargetConditionals.h`
  already handled at `Platform.h:40-46`, an optional `.app` bundle with
  `SDL_GetBasePath()` for data. The display-mode path (`ChangeDisplaySettings`
  stubs) needs its SDL equivalent verified under Retina scaling. arm64 is
  native; nothing in the tree is x86-only.

## Estimated work breakdown

| Step | Files | Nature | Effort |
|---|---:|---|---:|
| 0. Linux compile probe in a container from the conformance tip; record the error inventory | 0 | measurement | 1 day |
| A. Build contract: one POSIX macro, the four Linux spellings collapsed onto `Platform.h`'s detection, test definitions mirroring the libraries, the three dead regexes, `Client.cpp` out of `VS_UI`, `linux`/`linux-asan`/`macos` presets, `.gitignore` for `cmake/` | ~12 | mechanical | 2-3 days |
| B. Shim hygiene: `assert`, `min`/`max`, the thread waits moved into `Platform.h`, loud stubs, one `Client_PCH.h` | ~10 | small edits, wide blast radius | 2-3 days |
| C. Wire widths in `Datagram.h`, `Encrypter.h`, `ModifyInfo.cpp`, `SystemTypes.h`, goldens pinning the bytes | 4-6 + tests | test-first | 1-2 days |
| Libraries green on Linux GCC and Clang, `unit_tests` at the Windows baseline under ASan and UBSan: `basic`, `SpriteLib`, `TextSystem`, `dxlib`, `framelib`, `packetwire`, `gamemodel`, `VS_UI` | 40-80 | compile and fix (socket macros, CRT shims, `errno`, the three CP949 files) | 4-6 days |
| D. Entry point: `Client.cpp` split into a `ClientMain(cmdline)` shared by `WinMain` and `main`; `SDLMain.cpp` reduced to the SDL bootstrap; patcher window, mutex and registry behind `PLATFORM_WINDOWS` | ~5, ~600 lines moved | refactor, no test path | 3-5 days |
| E. Executable link: CRT residue, NetBIOS, `CreateThread`, `OutputDebugString`, backslash literals | 50-70, ~250 sites | mechanical | 3-4 days |
| F. Runtime on Linux against the docker server: data path, case-insensitive open, display mode, text input, audio; title, login, character select, zone, chat, shutdown | ~10 | the unknown | 5-10 days |
| G. macOS: dependency discovery, `SDL2main`, bundle, Retina, arm64 runner | ~5 | small | 2-4 days |
| H. CI (`ubuntu-latest` GCC and Clang, `macos-latest`), R1 off the `.vcxproj`, README and CLAUDE.md, adversarial review | ~8 | process | 2-3 days |

The sum is 25-40 engineer-days. The C++20 assessment put ISO-clean C++20 at
15-25 days; the OS port is of the same order, and the largest single lever
is that the conformance branches have already paid for the language half.

## Recommended implementation sequence

1. **Probe first.** In a Linux container from the conformance merge,
   configure with `-DBUILD_TESTS=ON` and build target by target: `basic`,
   then `packetwire`, then `unit_tests`. Keep the log. The first error list
   replaces every estimate above, and step 0 is the only step whose cost is
   certain.
2. **A, then B, then C**, one PR each, keeping Windows green on both trees
   throughout, because every `Platform.h` edit compiles on Windows as well.
3. **Libraries green on Linux with tests** before the executable is touched.
   GCC ASan and UBSan pay back immediately on the SpriteLib RLE walks the
   code-health review flagged as the weakest memory-safety area.
4. **D and E** to a linking `DarkEden`. Guard the Win32 patcher and
   single-instance paths; do not delete them.
5. **F** against the live server, one milestone per screen, each recorded as
   verified or not verified, never inferred from a compile.
6. **G** after Linux runs; macOS inherits everything but the filesystem-case
   work.
7. **H.** A Linux job on every push is the only thing that keeps the port
   from rotting again; the code-health review asked for it twice.

## Files that carry the most change

- `CMakeLists.txt` at the platform-macro sites listed under A,
  `tests/CMakeLists.txt:78-80`, `Client/DXLib/CMakeLists.txt:33`,
  `CMakePresets.json`, `.github/workflows/`
- `basic/Platform.h`, `basic/PlatformSDL.cpp`
- `Client/Packet/SocketAPI.cpp`, `PacketFileAPI.cpp`, `Types/SystemTypes.h`,
  `Datagram.h`, `Encrypter.h`
- `Client/Client.cpp`, `Client/SDLMain.cpp`, `Client/GameInit.cpp`,
  `Client/PacketFunction.cpp`
- `VS_UI/Client_PCH.h`, `Client/Client_PCH.h`, `Client/MWorkThread.h`
- `Client/MHelpMessageManager.cpp` and the other backslash-literal files

## What to reuse

- `basic/DirectoryListing` already matches names case-insensitively in NTFS
  order; the case-insensitive open extends it rather than adding a second
  matcher.
- `platform_config_get_string`/`set_string` (`PlatformSDL.cpp:452-526`) is the
  registry replacement for `Client.cpp:3531`.
- `platform_thread_create` (`PlatformSDL.cpp:119` on Windows, `:161`
  elsewhere) for `GameInit.cpp:1378`.
- `basic/MonotonicClock`, `basic/DisplaySettings` and `basic/SafeFormat` are
  already portable.
- `tests/golden/*.hex` and `tests/wire-layout.txt` are the cross-platform byte
  check for area C. A port that needs `UPDATE_GOLDENS=1` has changed the
  protocol.

## Acceptance criteria

- `cmake -S . -B build/linux -DBUILD_TESTS=ON -DREQUIRE_TEST_TOOLS=ON`
  configures and builds every target with GCC and again with Clang; `ctest`
  is green; a `-DUSE_ASAN=ON -DUSE_UBSAN=ON` tree is green; `diff -r` of the
  goldens against the server repo is empty.
- The same on macOS, arm64.
- Both Windows trees stay at 0 errors and the test baseline throughout.
- The client, launched from `bin/` on Linux against the docker server,
  reaches title, login, character select, a zone, chat and a clean exit,
  and each of those is recorded in the PR as verified or not.
- R9 and R10 stay at 0 after the conformance merge, and a new ratchet holds
  `__LINUX__`, `_LINUX` and `PLATFORM_MACOS` at zero outside `Platform.h` so
  the macro sprawl cannot return.
- A Linux job runs in Actions on every push to master.
- The port has had its adversarial review before the PR, as CLAUDE.md
  requires.

## Assumptions

- Targets: x86-64 Linux with GCC 13 or later and Clang 17 or later; macOS 13
  or later on arm64 and x86-64; SDL2, not SDL3.
- Dependencies come from the existing `vcpkg.json` manifest on all three
  platforms in CI; system packages are accepted locally, since `find_package`
  already handles both.
- The `conformance/*` branches merge first. This plan does not edit exception
  specifications or `register`.
- Nothing here changes packet bytes. The 32-bit `ulong` width on the wire is
  preserved, not widened.
- Modernising the platform layer beyond what the port needs, the `.rpk`
  extraction, and the intro video (`Client/CAvi.cpp`, stubbed on every
  platform) are out of scope.

## Progress record

**2026-09-08.** Eight stacked branches, one per step. Every step was built
on the Windows Debug tree with the full test suite; the steps that touch
library code (B, C, the libraries, the data paths) also on the Windows
ASan tree; and from the libraries step on, in an Ubuntu 24.04 container
under GCC 13, Clang 18, and GCC with ASan and UBSan - the earlier steps
were compiled there only as far as the tree compiled at the time. Each
commit message records exactly what it verified. What the estimates got
right and wrong:

| Step | Estimate | Outcome |
|---|---:|---|
| 0. Probe | 1 day | The first Linux compile failed 1,155 of 1,243 translation units on one line, `Platform.h`'s `min`/`max` macros, which masked everything else; the real inventory needed area B first. |
| A. Build contract | 2-3 days | As planned, plus the `extern int errno` declarations (area C's) that its guards made live, and two `__LINUX__` blocks that were upstream's console test client, not platform branches. Ratchet R13 holds the macro spellings at zero. |
| B. Shim hygiene | 2-3 days | The `min`/`max` macros became function templates that keep the tree's 518 mixed-type call sites; `assert` is real; the silent stubs are gone or loud; the thread shims left `MWorkThread.h` and the two request-service managers; one `Client_PCH.h`. The merged PCH exposed that `VS_UI` had compiled packet classes without `__GAME_CLIENT__`, a live vtable mismatch on Windows. |
| C. Wire widths | 1-2 days | Already narrowed at every stream; what was missing was a test to pin it, `long long` in the wire-scalar concept, and one width defect the layout inventory caught (`ShopVersion_t` was `long`: GCShopBought 288 bytes on LP64 against the server's 284). |
| Libraries green | 4-6 days | Five compile fixes, one link gap the key-function rule exposed (`MUsePotionItem::UseInventory`, now through `MItemHost`), four test assumptions, and 15 UBSan reports - every palette sprite's scanline pointer table sat at an odd offset - fixed with an aligned allocation. At that step 601 tests, 294,423 checks on Windows and 294,422 on Linux, on GCC, Clang and GCC with ASan and UBSan; the suite is 607 tests after the data-path step. |
| D. Entry point | 3-5 days | `ClientMain` is `WinMain`'s body; `SDLMain.cpp` went from 572 lines to 60. `CSDLGraphics::Init` creates the SDL window off Windows. |
| E. Executable link | 3-4 days | Four translation units and no undefined references: areas A to D had paid for the rest. |
| F. Runtime | 5-10 days | First slice only: `basic/DataPath.h` resolves the Windows-spelled paths, and the headless client loads every table, reaches the main menu and exits cleanly on `SDL_QUIT`. Login, zone and chat against the server, and anything on a display, are open. |
| G. macOS | 2-4 days | Built without a Mac, on GitHub's arm64 runner (`.github/workflows/macos.yml`, `macos` and `macos-asan` presets; the runner is the only Darwin compiler, so the workflow also runs on pull requests). Small, as estimated: one include spelling (`<SDL.h>`, the only one every SDL2 package's CMake target guarantees - Homebrew's autotools config exports no parent directory, so `<SDL2/SDL.h>` did not resolve on Apple Silicon), the two global `-I/opt/homebrew` hacks gone, `SDL2main` unlinked off Windows (it renames nothing there), an optional `.app` bundle with the data-root search that Finder and a bundle need (`Basic::FindDataRoot`), `SIGPIPE` ignored at the entry point (macOS has no `MSG_NOSIGNAL`), the system font list, and the Retina case the assessment flagged: the window asks for `SDL_WINDOW_ALLOW_HIGHDPI` and the mouse mapping goes through the point-to-pixel ratio, read from the window `CSDLGraphics` hands the present path, pinned by `tests/unit/test_present_geometry.cpp` since no test machine has such a display. What the real compiler found that the proxies had not: Apple Clang's `-Warray-bounds` flagged five subscripts in `VS_UI` (three fixed, two colour-table reads left open), and its `-fsanitize=enum` - on in Clang's `-fsanitize=undefined`, not GCC's - aborted the suite on four wire-byte-to-enum casts made before the range check (`MShopShelf::NewShelf`, the three gear `RemoveItem`s), which the Linux job's GCC sanitizers had passed. The adversarial review (four fresh-context reviewers) found the mapping's SDL 2.0.22 dependency, three more unbounded copies beside the fixed ones, a candidate buffer that dropped every bundle candidate past 258 bytes, and the usual overstated claims. A Clang + libc++ build in the Linux container was the local proxy for Apple Clang and found nothing; the source audit for glibc-only calls found nothing either. Nothing has been watched on a Mac's display. (Note added 2026-09-29: the text above is as of 2026-09-07. Since then an Apple Silicon maintainer Mac has built the tree and run the `macos` preset's tests, and `.github/workflows/macos.yml` runs on pushes to master and `workflow_dispatch`, not on pull requests.) |
| H. CI | 2-3 days | `.github/workflows/linux.yml` (three presets, on push to master like the Windows job), `tools/ci/verify-linux.sh`, `tools/linux/Dockerfile`, R1 measured from `build.ninja`, README and CLAUDE.md. The adversarial review of the whole series ran on 2026-09-08 (four fresh-context reviewers) and its findings are repaired in the last commit; the review's own record is in that commit message. |

The build-side steps came in under their estimates, mostly because the
macOS port of February had left more shim than the measured state
suggested and the conformance branches had removed the language obstacles;
the two things the estimates could not have priced - the `min`/`max`
mask over the first probe and the `VS_UI` vtable mismatch - were both found
by the compiler, not by reading.
