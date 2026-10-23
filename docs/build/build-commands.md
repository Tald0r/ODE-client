---
type: Guide
title: "Build commands"
description: "The two Windows Debug trees and why neither subsumes the other, the first configure's vcpkg toolchain, and the presets and verification scripts every platform's full check runs."
tags: [build, testing]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# Build commands

The [README](../../README.md) owns the prerequisites, vcpkg and generating the solution; this
concept is the commands an agent runs and the detail behind them. Preset names live in
[CMakePresets.json](../../CMakePresets.json).

# The two Windows Debug trees

Windows + MSVC is the live path. There are two Debug trees and neither subsumes the other,
because `/RTC1` and `/fsanitize=address` are mutually exclusive, so the checks are split across
them:

| | `build/vs2022` | `build/vs2022-asan` |
|---|---|---|
| Uninitialised local read | **`/RTC1`** | |
| Stack frame damage around a call | **`/RTC1`** | |
| Heap overflow, use-after-free, double free | | **ASan** |
| Stack buffer overflow | | **ASan** |
| Invalidated iterator, bad comparator | debug iterators | debug iterators |
| Speed | normal | about 2x slower |

Use the ordinary tree day to day and the sanitized one to chase a memory error (a wild pointer, a
crash inside the allocator, damage that shows up far from its cause); a library fix is verified
only when both are green ([library fixes are test-first](../testing/library-fixes-test-first.md)).
`/RTC1` is requested explicitly by `CMakeLists.txt` rather than inherited from CMake's built-in
Debug flags, so overriding `CMAKE_CXX_FLAGS_DEBUG` cannot drop it silently. Configure prints
exactly one of the two check-set lines; if neither appears, the flags are wrong:

```
-- Debug runtime checks: /RTC1 (uninitialised locals, stack frames)
-- AddressSanitizer enabled (/fsanitize=address)
```

The **first** configure of a fresh tree must pass the vcpkg toolchain, or SDL2 is not found:

```bash
cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/vs2022 --config Debug -- -m
```

A test tree needs `-DBUILD_TESTS=ON` (it defaults to `OFF`, and a tree without it has no test
target) and the sanitized one `-DUSE_ASAN=ON` in a separate tree
([testing overview](../testing/overview.md#running-them)). Ignore the Makefile's
`make debug-asan` and friends: they target Unix build directories (`build/debug-asan`) and are
not the path used here.

`build/` is gitignored and bakes absolute paths into the generated projects: regenerate it
locally, never commit it. `/MP` is set once for every target in `CMakeLists.txt`; if it ever
looks missing, fix it there, never in a generated `.vcxproj`, which the next configure discards.

## The sanitized tree

`USE_ASAN=ON` changes the compile and link flags of every target, so it never shares a tree with
the ordinary build. It reports heap and stack overflows, use-after-free and double frees at the
instruction that causes them; before the extractions moved the game into libraries, it was the only
realistic way to reach the memory-safety defects in code no unit test could link.

It needs Visual Studio's *C++ AddressSanitizer* component (the README has the install command);
configure stops with instructions when the component is missing, because `cl.exe` accepts
`/fsanitize=address` without it and fails only at link time. The build does three things on your
behalf, because the sanitizer will not work otherwise:

- **Drops `/RTC1`**, which MSVC rejects next to `/fsanitize=address`. That is the real loss the
  table above records.
- **Turns off incremental linking**, which the sanitizer does not support.
- **Copies `clang_rt.asan_dynamic-x86_64.dll` beside the executables** at configure time. Visual
  Studio puts the compiler directory on `PATH` when it launches the debugger, so F5 would find it
  anyway; running the `.exe` from a shell or from Explorer would not. A tree configured before the
  component was installed lacks the copy until it is configured again.

Expect roughly twice the run time and appreciably more memory. A report goes to stderr and the
debugger breaks at the faulting instruction. Leak detection is a Linux-only part of
AddressSanitizer, so this tree catches invalid accesses, not leaks; the CRT leak dump at exit
covers those.

# The Windows presets

The checked-in presets use separate trees under `build/presets/`, enable all tests, and need Bash
and Perl from Git for Windows on `PATH` (or `verify-windows.ps1`, which locates them) and
`VCPKG_ROOT` set. `windows` configures a tree with `windows-debug` and `windows-release` build and
test presets; `windows-asan` has its own. Configure the two trees one after the other: they share
the manifest dependency installation under `build/presets/vcpkg_installed`.

`windows-clang` checks that the sources are ISO C++20 rather than MSVC's dialect of it: it builds
every target with the clang-cl of Visual Studio's *C++ Clang tools for Windows* component, through
NMake, so it runs from a VS developer prompt (`vcvars64.bat`, or the x64 Native Tools Command
Prompt) where `clang-cl` and `nmake` are on the path. NMake compiles one file at a time, so it
takes several times longer than the Visual Studio tree, and ratchet R1, which reads a `.vcxproj`
this generator does not produce, is skipped there.

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --preset windows-clang
cmake --build --preset windows-clang
ctest --test-dir build/presets/windows-clang --output-on-failure
```

`CMakeUserPresets.json` holds local overrides and is ignored by git. When updating dependencies,
change the manifest baseline in `vcpkg.json` deliberately and check the deployed DLL names
against the client's startup whitelist ([running the client](../operations/running-the-client.md)).

# The full check

Each platform's full check is one script, the same one CI runs ([CI](ci.md)). It builds every
library, the test binaries and the `DarkEden` executable from clean, runs ctest, asserts the
required tests are registered and holds the warning budget ([reading build output](reading-build-output.md)):

```bash
tools/ci/verify-linux.sh linux          # or linux-clang, linux-asan, macos, macos-asan
```

```powershell
./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release
./tools/ci/verify-windows.ps1 -ConfigurePreset windows-asan -BuildPresets windows-asan
```

If Windows PowerShell reports that scripts are disabled, run the script in a child process with a
policy override limited to that process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& ./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release"
```

`verify-linux.sh` writes `build.log`, `test.log` and `warnings.json` to
`build/verification/<preset>/`; `verify-windows.ps1` writes `<build preset>-build.log`,
`-tests.xml` and `-warnings.json` to `build/verification/<configure preset>/`. CI runs the Linux
presets on `ubuntu-24.04` directly; on a Windows machine or a Mac they run in the `darkeden-linux`
image ([Linux and macOS](linux-and-macos.md#the-linux-image)), and run from a git worktree the
container also needs the common git directory mounted
([testing overview](../testing/overview.md#running-them)).
