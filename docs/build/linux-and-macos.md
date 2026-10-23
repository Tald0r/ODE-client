---
type: Guide
title: "Linux and macOS"
description: "The darkeden-linux Docker image and its x86_64 run on Apple Silicon, what is verified of the Linux and macOS builds and runs, and the platform notes: Homebrew's SDL, the macOS warning budget, the app bundle's data lookup and Retina scaling."
tags: [build, operations]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# Linux and macOS

Windows is the live platform. Linux and macOS build every target, run the same test suite, and
the client starts; the [README](../../README.md) owns the packages, presets, commands and data
links that get a clone there. This concept is the verification recipes and what has been
verified. The port's plan and sizing are the
[port assessment](linux-macos-port-assessment-2026-09-07.md).

# The Linux image

`tools/ci/verify-linux.sh <preset>` (any Linux or macOS preset but the fuzz ones) runs configure,
build and test the way the Linux workflow (`.github/workflows/linux.yml`) does, with UBSan reports
made fatal under `linux-asan` ([build commands](build-commands.md#the-full-check)). On a Windows
machine or a Mac with Docker (OrbStack, which this was verified with, or Docker Desktop),
`tools/linux/Dockerfile` is the same environment as an image, on an x86_64 host:

```bash
docker build -t darkeden-linux tools/linux
docker run --rm -v "$PWD:/src" -v darkeden-build:/src/build darkeden-linux \
    tools/ci/verify-linux.sh linux
```

On Apple Silicon the image is arm64, so it is not the CI environment: plain `char` is unsigned
there, and the warning profile differs. The configure, the build and every test pass, but the
script's last step fails with `missing warning baseline for linux-aarch64`, because
`tools/ci/warning-baselines.json` records the x86_64 runner's warnings only
([reading build output](reading-build-output.md)). To get the x86_64 profile and its warning
baseline, build and run the image as x86_64, with its own build volume so it does not reuse the
arm64 CMake tree:

```bash
docker build --platform linux/amd64 -t darkeden-linux-amd64 tools/linux
docker run --rm --platform linux/amd64 -v "$PWD:/src" -v darkeden-build-amd64:/src/build \
    darkeden-linux-amd64 tools/ci/verify-linux.sh linux
```

This was run once on an Apple Silicon Mac (2026-09-29, Docker under OrbStack): the `linux` preset
configured, built with `--clean-first`, passed its tests (1243 tests, 0 failed) and the x86_64
warning check, in under 30 minutes emulated. It is the x86_64 profile, not the CI runner itself:
`linux.yml` runs on `ubuntu-24.04` directly, not in this image. Run from a git worktree, the
container also needs the common git directory mounted
([testing overview](../testing/overview.md#running-them)).

# Status on Linux

What is verified, in the order the port assessment's acceptance criteria list them:

- **Builds:** every library, `unit_tests` and `DarkEden`, with GCC and Clang, and the GCC tree
  under ASan and UBSan with nothing reported. The suite is the Windows suite.
- **Runs, partly:** launched from `build/presets/linux/bin` with the `Data/` tree beside the
  executable (the resolver in `basic/DataPath.h` takes care of the tables' backslashes and letter
  case), the client loads every data table, reaches the main menu and exits cleanly on
  `SDL_QUIT`. That has been exercised headless (`SDL_VIDEODRIVER=dummy`, where the quit came from
  SIGTERM), not on a display. Login, character select, a zone and chat against the docker server
  are **not verified** off Windows (the assessment's area F).

The Korean IME is SDL text input on every platform, sound is SDL_mixer, and the launcher arguments
and `UserSet/Display.ini` go through the same code as on Windows; none of that has been watched on
a Linux display yet. Nothing under `Data/` is a font: `Client/TextSystem/TextBackendSDL.cpp` looks
for Noto CJK and DejaVu under `/usr/share/fonts`, and without one of them the client draws no text.

# Status on macOS

CI builds and tests the `macos` and `macos-asan` presets on GitHub's arm64 and Intel runners
(`.github/workflows/macos.yml`, macOS 15). One Apple Silicon Mac (macOS 27.0, Apple Clang 21,
CMake 4.4) built every target and passed all 20 ctest tests on 2026-09-30, among them the three
packet-read fuzz replay tests (`fuzz_replay_client_stream`, `fuzz_replay_client_login_stream`,
`fuzz_replay_client_datagram`) and their corpus steps. Nothing has been watched on a Mac's display
yet.

- **Homebrew's `sdl2` is an alias of `sdl2-compat`**, the SDL2 API over SDL3; that is what the
  build above linked. The linker warns that Homebrew's libraries were built for a newer macOS than
  the preset's 13.0 deployment target, which is harmless for a build run on the machine that made
  it. CMake finds Homebrew through the preset's prefix path, or through `HOMEBREW_PREFIX` when
  configuring by hand. Includes spell `<SDL.h>`, never `<SDL2/...>`, which breaks the Homebrew
  build (`basic/Platform.h` says why).
- **Fonts** are the system's own (Apple SD Gothic Neo, Arial Unicode, Hiragino Sans GB,
  Helvetica), through `Client/TextSystem/TextBackendSDL.cpp`; no font package is needed.
- **The warning budget is recorded from the CI runners' compilers.** The project's own code
  builds without warnings; what `verify-linux.sh macos` counts is the vendored code under
  `third_party/` and `build/_deps/` and the linker, so a different compiler version can count
  differently. Under Apple Clang 21 the build and the tests pass, but the budget step reports the
  linker's deployment-target warnings and a few more vendored-code diagnostics than the runner's
  Apple Clang.
- **The data links** in the README (two symbolic links and a one-time copy, the equivalent of the
  Windows junctions) were written from the code, not run against the game data.
- **The app bundle** (`-DDARKEDEN_MACOS_BUNDLE=ON` builds `bin/DarkEden.app`): launched from
  Finder or as a bundle, the client looks for `Data/Info/FileDef.inf` under its working
  directory, its executable's directory and the directory the bundle sits in, runs from the first
  that has it, and says on stderr which directories it tried when none has it. The bundle does not
  copy the data, and the game writes beside its data (`UserSet/`, `Log/`), so the data does not go
  inside the bundle: put `Data/` beside `DarkEden.app`. A bundle downloaded with a quarantine flag
  is run by macOS from a random read-only copy (App Translocation), where "beside the bundle" is
  nowhere; build it locally or clear the flag with `xattr -d com.apple.quarantine`.
- **Retina displays**: the window asks for `SDL_WINDOW_ALLOW_HIGHDPI`, so the game frame is scaled
  to the native pixel grid and mouse points are mapped through the point-to-pixel ratio
  (`tests/unit/test_present_geometry.cpp` pins the mapping; no test machine has such a display).
  Whether macOS honours the flag for a bare executable, which has no `Info.plist` to say
  `NSHighResolutionCapable`, or magnifies it as it does an app without the key, has not been seen;
  the bundle declares the key. The xBRZ filter's factor is chosen from the pixel size, so the same
  display costs more CPU per frame than a non-Retina one of the same point size.

The packet-read fuzzers have their own presets, `linux-fuzz` and `macos-fuzz`
([fuzzing](../protocol/fuzzing.md)).
