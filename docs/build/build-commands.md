---
type: Guide
title: "Build commands"
description: "The two Windows Debug trees and why neither subsumes the other, the first configure's vcpkg toolchain, and the presets and verification scripts every platform's full check runs."
tags: [build, testing]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T11:11:26Z }
---

# Build commands

The [README](../../README.md) owns the prerequisites, vcpkg and generating the solution; this
concept is the commands an agent runs. Preset names live in
[CMakePresets.json](../../CMakePresets.json).

# The two Windows Debug trees

Windows + MSVC is the live path. There are two Debug trees and neither subsumes the other,
because `/RTC1` and `/fsanitize=address` are mutually exclusive:

| Tree | Flags | Catches |
|---|---|---|
| `build/vs2022` | `/RTC1` | uninitialised locals, stack frame damage |
| `build/vs2022-asan` | `/fsanitize=address` | heap/stack overflow, use-after-free, double free |

Use the ordinary tree day to day and the sanitized one to chase a memory error; a library fix is
verified only when both are green ([library fixes are test-first](../testing/library-fixes-test-first.md)).
Configure prints exactly one of the two check-set lines; if neither appears, the flags are wrong.

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

`verify-linux.sh` writes `build.log`, `test.log` and `warnings.json` to
`build/verification/<preset>/`; `verify-windows.ps1` writes `<build preset>-build.log`,
`-tests.xml` and `-warnings.json` to `build/verification/<configure preset>/`. CI runs the Linux
presets on `ubuntu-24.04` directly; on a Windows machine or a Mac they run in the `darkeden-linux` image
(`tools/linux/Dockerfile`, with the commands in the [README](../../README.md)), and run from a git
worktree the container also needs the common git directory mounted
([testing overview](../testing/overview.md#running-them)).
