---
type: Guide
title: "Testing overview"
description: "What the test binaries and script ctests cover, how to configure and run them in a plain and an ASan tree, and the measured baseline the suite last read."
tags: [testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Testing overview

Only code compiled into a static library can be tested ([library boundaries](../architecture/library-boundaries.md));
everything else is verified by builds and the automated checks, never by a runtime report
([executable-only code](executable-only-code.md)).

# The suite

| ctest | What it runs |
|---|---|
| `unit_tests` | `tests/unit/*.cpp`, globbed, on the [framework](test-framework.md); links `basic`, `SpriteLib`, `TextSystem`, `packetwire`, `gamemodel`, `gameview`, `gameclient` (and through it `VS_UI`) and `dxlib`; the packet goldens, the wire-layout inventory and the fuzz findings are in it |
| `ui_tests`, `user_option_tests` | `VS_UI` widgets reachable on their own; key bindings |
| `decore_tests` | the vendored formulas against the shared parity vectors ([de-core](../architecture/decore.md)) |
| `sprite_gpu_tests` | the GPU sprite composition ([native GPU renderer](../rendering/native-gpu-renderer.md)) |
| `screen_fade_<signed\|unsigned>_tests`, `weather_<signed\|unsigned>_tests` | the same model built with both `char` signednesses |
| `fuzz_replay_client_stream`, `fuzz_replay_client_login_stream`, `fuzz_replay_client_datagram` | the seed corpus and every fixed crash through the packet readers ([fuzzing](../protocol/fuzzing.md)) |
| `ratchets` (bash) | the shrink-only counts ([ratchets](ratchets.md)) |
| `arch_includes`, `decore_vendored`, `source_encoding`, `format_arity`, `packet_indices`, `wire_inventory_fresh`, `warning_budget_parser`, `knowledge_bundle` (perl) | the include graph, the vendored copy, UTF-8 sources, [format strings](../protocol/format-strings.md), [packet indices](../protocol/packet-input-safety.md), the [wire inventory](../protocol/wire-layout.md), the warning budget's parser, the [knowledge bundle](../knowledge-bundle.md) |
| `warning_policy` (cmake) | the warning options every target compiles with ([reading build output](../build/reading-build-output.md)) |

Bash and perl availability affects registration: a check that could not be registered is
skipped, not passed, so the CI scripts assert the inventory by name
([CI](../build/ci.md)).

# Running them

```bash
cmake -S . -B build/tests -DBUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/tests --config Debug --target unit_tests -- -m
cd build/tests && ctest -C Debug --output-on-failure
```

Add `-DUSE_ASAN=ON` in a separate tree for the sanitized run. `BUILD_TESTS` defaults to `OFF`,
so a tree configured without it generates no test target at all. Linux and macOS use the
`linux`, `linux-clang`, `linux-asan`, `macos` and `macos-asan` presets in `CMakePresets.json`;
`tools/ci/verify-linux.sh <preset>` is the full check there. Run from a git worktree, the Linux
container also needs the common git directory mounted, at its host path or with `GIT_DIR` and
`GIT_WORK_TREE` set, or `ratchets` and `source_encoding` fail on "not a git repository".

# Baseline

Measured on 2026-09-30 on the skill-info branch merged with master at `71159e1c`: master alone
read 1463 tests and 1,458,200 checks; the merged tree reads **1483 tests, 1,458,777 checks, 0
failed**, identical in the four Linux and macOS builds (GCC 13.3 and Clang 18.1 in the Docker
image, Apple Clang 21 plain and with ASan and UBSan; `decore_tests`: 13 tests, 2009 checks). The
Windows trees were not re-measured for this figure, and no CI run produced it. A
platform-conditional test can make the check totals differ by one between platforms; that is not a
failure. Keep a new measurement with the change that produced it; this concept records the last
one, not a live count.

`DarkEden` builds and links on Linux and macOS, and on Linux run headless
(`SDL_VIDEODRIVER=dummy`) with the data tree beside it reaches the main menu and exits cleanly on
`SDL_QUIT`; login and beyond are unverified off Windows. A `<SDL2/...>` include spelling breaks
the Homebrew build: it is `<SDL.h>` everywhere (`basic/Platform.h` says why).
