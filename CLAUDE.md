# CLAUDE.md

OpenDarkEden client: the isometric horror MMORPG client (slayers, vampires, ousters), a Korean
original from roughly 2000-2010 ported from Win32 + DirectX to SDL2. C++20 and CMake; Windows +
MSVC is the live path, and Linux, macOS and the WebAssembly build build and test too.

Guidance for agent sessions in this repo. [README.md](README.md) is the human setup guide
(prerequisites, vcpkg, generating the solution, running the game, troubleshooting): link to it
rather than restating it. The rules, and the incidents behind them, live in the knowledge bundle
under `docs/` (Google's Open Knowledge Format): start at [docs/index.md](docs/index.md);
[docs/knowledge-bundle.md](docs/knowledge-bundle.md) says how it is kept. This file is the index:
one line per rule and a link to the concept that explains it. **Open the linked concept before you
work in its area** - the line here is a reminder, not the rule. Every line binds; one marked
*(user instruction)* is in the user's own words.

## Build & test

```bash
# Windows + MSVC. The first configure of a fresh tree must pass the vcpkg toolchain.
cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/vs2022 --config Debug -- -m

# The test tree; add -DUSE_ASAN=ON in a separate tree for the sanitized run.
cmake -S . -B build/tests -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/tests --config Debug --target unit_tests -- -m
cd build/tests && ctest -C Debug --output-on-failure

# The full check CI runs, per platform.
./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release
tools/ci/verify-linux.sh linux                 # or linux-clang, linux-asan, macos, macos-asan
perl tests/tools/check_knowledge_bundle.pl .   # before pushing any docs change
```

- **CI runs only on pushes to `master` and manual `workflow_dispatch`; keep pull-request triggers disabled** to conserve GitHub Actions minutes *(user instruction, 2026-09-25)*. Verify locally and dispatch by hand when a change needs CI - [CI](docs/build/ci.md).
- Two Debug trees, `build/vs2022` (`/RTC1`) and `build/vs2022-asan` (`/fsanitize=address`), and neither subsumes the other; configure prints exactly one check-set line - [build commands](docs/build/build-commands.md).
- `BUILD_TESTS` defaults to `OFF`; ignore the Makefile's `make debug-asan` and friends, which target Unix build directories - [build commands](docs/build/build-commands.md).
- `build/` bakes in absolute paths and is never committed; `/MP` is set once in `CMakeLists.txt`, never in a generated `.vcxproj` - [build commands](docs/build/build-commands.md).
- Judge a build by `error C####`, `error LNK`, `error MSB` or `fatal error`, never by grepping `"error"`; redirect it to a log file, never pipe it through `tail` - [reading build output](docs/build/reading-build-output.md).
- A warning in project code is new and fails its preset's budget; clear a C4996 through `basic/CrtCompat.h`, never `_CRT_SECURE_NO_WARNINGS` - [reading build output](docs/build/reading-build-output.md).
- On Apple Silicon the arm64 Docker image has no warning budget: check its log against the x86_64 profile before pushing - [reading build output](docs/build/reading-build-output.md), [Linux and macOS](docs/build/linux-and-macos.md).
- A skipped check is not a pass: a ctest that must run everywhere goes on both verify scripts' required lists - [CI](docs/build/ci.md).
- A docs-only change still runs `knowledge_bundle`, and `git status` must list every file you wrote - [CI](docs/build/ci.md).
- Run the Linux Docker image from a git worktree with the common git directory mounted, or `ratchets` and `source_encoding` fail on "not a git repository"; configure the `windows` and `windows-asan` trees one after the other (they share `vcpkg_installed`) - [testing overview](docs/testing/overview.md#running-them), [build commands](docs/build/build-commands.md#the-windows-presets).

## Testability and testing

- Only code in a static library links into a test binary; membership is the four `tests/arch/*_files.txt` lists, and the executable's own source is `Client/SDLMain.cpp` alone (R1 = 1) - [library boundaries](docs/architecture/library-boundaries.md).
- Code no test can reach is verified with builds and the automated checks. **Live-server verification is optional and never blocks a merge or completion; never ask for a runtime report as merge approval** *(user instruction, 2026-09-17)* - [executable-only code](docs/testing/executable-only-code.md).
- Library fixes are written test-first, assert the observable contract rather than the crash, and are verified only when the plain and the ASan tree are both green - [library fixes are test-first](docs/testing/library-fixes-test-first.md).
- A fix you could not reproduce is a *regression guard*, and substantial remediation gets an adversarial review - [library fixes are test-first](docs/testing/library-fixes-test-first.md).
- `tests/framework/test_framework.h` is not GoogleTest: `TEST`, `CHECK` and `CHECK_EQ` are the API, and `tests/unit/*.cpp` is globbed, so a new test is a new file - [the test framework](docs/testing/test-framework.md).
- An uninitialised member is reproduced by reading the fill pattern, not guarded - [uninitialised members](docs/testing/uninitialised-members.md).
- Ratchet numbers only go down, tightened in the commit that makes the progress; a ratchet at zero is a claim about the ratchet - [ratchets](docs/testing/ratchets.md).
- A link proof takes the address of non-virtual members; verify one by taking the file back out - [link proofs](docs/testing/link-proofs.md).
- What each ctest covers, and the last measured baseline (dated, not live) - [testing overview](docs/testing/overview.md).

## Protocol

- Wire layout is pinned in `tests/wire-layout.txt` and `tests/golden/*.hex`: a layout change is a protocol change the server ships identically, the inventory is regenerated with `tests/tools/gen_wire_inventory.pl`, and a golden is never re-recorded to make a test pass - [wire layout](docs/protocol/wire-layout.md).
- Unvalidated network input is the top open risk: ratchet R3 holds lengths into copies at 0, and `packet_indices` reads every packet-indexed subscript by value and fails closed - [packet input safety](docs/protocol/packet-input-safety.md).
- A format string from a data file goes through `SafeFormat::Format`; to find a new site, sweep the *format argument*, never a spelling of the lookup - [format strings](docs/protocol/format-strings.md).
- Three libFuzzer targets drive the packet readers; a fixed crash gets a regression input *and* a unit test, and only a Clang sanitizer sees a bad enum load - [fuzzing](docs/protocol/fuzzing.md).
- Parsing, model mutation and UI effects are separate layers, and every handler is bound to its id in `Client/PacketHandlerRegistry.cpp` - [protocol overview](docs/protocol/overview.md).
- `third_party/decore` is the server's: change a shared formula there, resync with `tools/decore/sync.pl`, never edit the copy - [de-core](docs/architecture/decore.md).

## Architecture and libraries

- A new library target names its sources, never `file(GLOB)`; the source layout table is in the same concept - [library boundaries](docs/architecture/library-boundaries.md).
- A library reaches the executable only through a designated host: a reader guards and re-reads the pointer, a missing host answers like a missing config, an installer is designated-initialised - [host interfaces](docs/architecture/host-interfaces.md).
- A class split across a library and the executable moves whole - [class splits](docs/architecture/class-splits.md).
- `packetwire` is every `.cpp` under `Client/Packet`; libraries log through `basic/DebugLog.h` and never include `Client/DebugInfo.h` - [packetwire](docs/architecture/packetwire.md).
- What the extracted libraries hold and the hosts they use - [gamemodel](docs/architecture/gamemodel.md), [gameview and gameclient](docs/architecture/gameview-and-gameclient.md).
- The WebAssembly client and the WebSocket gateway - [browser client](docs/browser/browser-client.md); when the browser cannot log in or out - [login and logout](docs/browser/login-and-logout.md).
- Rendering - [native GPU renderer](docs/rendering/native-gpu-renderer.md), [60 fps interpolation](docs/rendering/frame-rate-interpolation-2026-08-30.md), [widescreen scaling](docs/rendering/widescreen-scaling-2026-08-30.md).

## Traps

- `_DEBUG` IS defined in MSVC Debug builds, so every `#ifdef _DEBUG` block is live in Debug and dead only in Release - [client traps](docs/architecture/client-traps.md).
- A silent `-1` exit with no log written is the DLL whitelist, and the DLL survives reverting the change that brought it - [client traps](docs/architecture/client-traps.md).
- **Don't launch `DarkEden.exe` yourself unless asked**: it runs from the Visual Studio debugger, and a startup failure that has to be bisected is a reason to ask - [running the client](docs/operations/running-the-client.md).
- Resources are read loose first, then from the `.rpk` - [running the client](docs/operations/running-the-client.md).
- The 5:5:5 sprite paths are latent, and sprite rejection is silent - [client traps](docs/architecture/client-traps.md).
- `grep` stops at the NUL bytes in `VS_UI/src/VS_UI_GameCommon.cpp`: measure `VS_UI/` with `grep -a` - [client traps](docs/architecture/client-traps.md).
- Include `<SDL.h>`, never `<SDL2/...>`, which breaks the Homebrew build - [Linux and macOS](docs/build/linux-and-macos.md).

## Conventions and process

- Tabs, English only, Hungarian notation and banner comments as the file has them; line endings vary by file (`git ls-files --eol`) and each keeps its own, so edit with the Edit tool or `perl -pi -e` on the exact files, never `sed -i` or `awk` - [source style](docs/process/source-style.md).
- Commit messages are `type: lowercase imperative summary`, then prose on why the change is right, what was verified and what is deliberately out of scope; `a41eec9` is the model - [commit messages](docs/process/commit-messages.md).
- A `fix:` commit carries a `Test path:` line (`lib + test`, `moved, then fixed` or `exempt`); `tools/git-hooks/commit-msg` refuses one without, after `git config core.hooksPath tools/git-hooks` once per clone - [commit messages](docs/process/commit-messages.md).
- Trailer: `Co-Authored-By: <the authoring model> <noreply@anthropic.com>`, e.g. `Claude Fable 5.1` - [commit messages](docs/process/commit-messages.md).
- Moving a file with no seam to cut does not change its bytes in the same commit: move, prove the build, then fix - [move, then fix](docs/process/move-then-fix.md).
- A scripted rewrite bounds its pattern and is followed by a sweep for orphaned banners - [scripted rewrites](docs/process/scripted-rewrites.md).
- Measure the value, not the spelling: a search that finds nothing is a fact about the search - [measure the value](docs/process/measure-the-value.md).
- A restructuring task's status line changes in the commit that does the work, a task is done only once its Owner exists, and a substantial task gets a two-reviewer adversarial review - [restructuring status lines](docs/process/restructuring-status-lines.md).
- A change to a rule or a design updates its concept in the same commit; moving a document fixes every reference to it and adds a `log.md` entry - [the knowledge bundle](docs/knowledge-bundle.md).

## Current focus

- [Packet input safety](docs/protocol/packet-input-safety.md) - unvalidated network input is the top open risk; fixed-size buffers fed by server strings are next.
- [Fuzzing the packet readers](docs/protocol/fuzzing.md) - the one check that models no route; it stops at `read()`.
- [Format strings](docs/protocol/format-strings.md) - C19 is closed; read its review entry before trusting a ratchet at zero.
- [Code-health reconciliation](docs/reviews/code-health-reconciliation-2026-09-23.md) - the current disposition of the [review](docs/reviews/code-health-review-2026-08-29.md)'s 197 findings.
- [The restructuring plan](docs/reviews/RESTRUCTURING.md) - original plan complete as of 2026-09-09 (task 4.12 is still open); its *What the review rounds settled* section is the standing rulebook for library work.
