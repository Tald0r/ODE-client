# opendarkeden-client

[Client knowledge map](docs/knowledge/index.md): architecture, verification, protocol, and browser flow entry points.

## Build on Windows (from scratch)

The current build is **CMake + SDL2 + Visual Studio 2022**. The old DirectX code
has been migrated to SDL2, so no DirectX SDK is needed any more.

### 1. Prerequisites

Install **Visual Studio 2022** with the *Desktop development with C++* workload.
That gives you the MSVC toolset (v143) and a Windows 10/11 SDK.

You also need the **C++ ATL** individual component, which that workload does
*not* install by default. `VS_UI/src/header/VS_UI_WebBrowser.h` includes
`atlbase.h` for the legacy in-game IE browser control, and those sources are
only excluded on non-Windows builds — so on Windows ATL is mandatory. In the
Visual Studio Installer: *Modify → Individual components → "C++ ATL for latest
v143 build tools (x86 & x64)"*. Or from an **elevated** terminal (`--quiet`
silently refuses with exit code 5007 if the shell isn't already elevated — it
does not prompt for UAC):

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" --add Microsoft.VisualStudio.Component.VC.ATL --quiet --norestart
```

Close Visual Studio first. The installer aborts with exit code 8006
(`Pre-check verification failed with warning(s) : VSProcessesRunning`) if
anything VS-related is alive, and "anything" is broader than it sounds. Beyond
`devenv` itself:

- idle `MSBuild.exe` worker nodes, which linger for ~15 minutes after a
  `cmake --build ... -- -m` run,
- `vctip.exe`, the VC++ telemetry uploader that `cl.exe` spawns and leaves
  behind,
- `cl.exe` and `Tracker.exe` orphaned by an interrupted build, which have no
  parent left to reap them and can sit there indefinitely.

Chasing those by name is a losing game — `taskkill /F /IM MSBuild.exe` does not
touch the last two. Ask instead what is running out of the Visual Studio
directory, which is the same question the installer is asking:

```powershell
Get-Process | Where-Object { $_.Path -like 'C:\Program Files*\Microsoft Visual Studio\*' } | Select-Object Id, ProcessName, Path
```

Everything it lists has to be gone before the installer will proceed. Note that
`--quiet` prints nothing on success, so check `$LASTEXITCODE`: `0` is done,
`3010` is done-but-wants-a-reboot, `8006` is the pre-check above.

Verify it landed before building — this should print an install path, not
nothing:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -requires Microsoft.VisualStudio.Component.VC.ATL -property installationPath
```

Install CMake 3.21+ (the first version with the VS2022 generator) and reopen
your terminal so it lands on `PATH`:

```bash
winget install Kitware.CMake
```

### 2. Install the dependencies with vcpkg

`vcpkg.json` declares the dependencies and pins their baseline. Clone vcpkg,
then select that same revision before bootstrapping (run from this repo):

```bash
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
```

```powershell
$baseline = (Get-Content vcpkg.json -Raw | ConvertFrom-Json).'builtin-baseline'
git -C C:\vcpkg checkout $baseline
C:\vcpkg\bootstrap-vcpkg.bat
```

CMake installs these dependencies automatically when configuring a fresh build.
The first install can compile libraries from source and take a while. Existing
classic-mode build trees keep their old dependency setup; use the presets below
to validate the pinned manifest without changing those trees.

Existing classic-mode installs need SDL_image's JPEG feature for help artwork:
`vcpkg install sdl2-image[libjpeg-turbo]:x64-windows --classic --recurse`.

| Package | Used for |
| --- | --- |
| `sdl2`, `sdl2-image[libjpeg-turbo]`, `sdl2-ttf` | rendering, sprites, help JPEGs, text (required) |
| `sdl2-mixer` | sound and music |
| `openssl` | verified TLS for optional native WebSocket connections |

The [browser and WebSocket guide](docs/webgl-client.md) covers the WebAssembly /
WebGL 2 build, asset packaging, gateway configuration and browser tests. Existing
classic vcpkg trees also need `vcpkg install openssl:x64-windows --classic`.
| `libiconv` | shared resource and legacy text conversion in `basic/TextEncoding.cpp` |
| `libjpeg-turbo` | JPG load/save in `UtilityFunction.cpp` |

vcpkg can live anywhere — just point `-DCMAKE_TOOLCHAIN_FILE` at wherever you
put it in the next step. `vcpkg integrate install` is optional.

### 3. Generate the solution

The toolchain file must be passed on the **first** configure, or vcpkg's
packages won't be found:

```bash
cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 4. Build

```bash
cmake --build build/vs2022 --config Debug
```

Or open `build/vs2022/DarkEdenClient.sln` in Visual Studio, set **DarkEden** as
the startup project, and build from there.

The executable lands in `build/vs2022/bin/Debug/DarkEden.exe`.

The optional GNU Make wrapper requires a POSIX shell (Git Bash on Windows).
Its build and test targets select Debug/Release explicitly, including with the
Visual Studio generator. Pass initial configure options through `CMAKE_ARGS`,
for example `make debug CMAKE_ARGS=-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`.
`NPROCS` overrides the detected parallel job count. The direct CMake commands
above remain the Windows setup path.

Ninja and Makefile generators write `compile_commands.json` into the selected
build directory. Point clangd or another editor at that directory; a local copy
or symlink at the repository root is ignored. Visual Studio generators do not
produce this database.

`BUILD_ENGINE=OFF` omits the map viewer, whose loader requires the optional
sprite engine. The effect viewer uses the client libraries and remains available.
Its Bash build/run helper accepts `DARKEDEN_DIR` (the directory containing
`Data/`, default `DarkEden/` beside this README), `BUILD_DIR` and `NPROCS`.

### Reproducible builds and the complete test suite

The checked-in presets use separate trees under `build/presets/`, enable all
tests, and require Bash and Perl from Git for Windows. Add Git's `usr/bin`
directory to your terminal's `PATH`, or use the verification script below,
which locates those tools explicitly. Set `VCPKG_ROOT` for this terminal:

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --preset windows
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The same configured tree supports `windows-release` build and test presets.
For ASan, configure `windows-asan`, then use the `windows-asan` build and test
presets (the component described in step 5 is required). Configure the two
trees sequentially: they share a manifest dependency installation under
`build/presets/vcpkg_installed`.

A second compiler checks that the sources are ISO C++20 rather than MSVC's
dialect of it. The `windows-clang` preset builds every target with the
clang-cl that ships with Visual Studio's "C++ Clang tools for Windows"
component, through NMake, so it has to run from a **VS developer prompt**
(`vcvars64.bat`, or the "x64 Native Tools Command Prompt"), where `clang-cl`
and `nmake` are on the path:

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --preset windows-clang
cmake --build --preset windows-clang
ctest --test-dir build/presets/windows-clang --output-on-failure
```

NMake compiles one file at a time, so expect this tree to take several times
longer than the Visual Studio one. The ratchet that counts translation units
in the executable (R1) is skipped here, because it reads a `.vcxproj` that
this generator does not produce.

To run the same complete verification as CI, including checking that every
required CTest entry is registered:

```powershell
./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release
./tools/ci/verify-windows.ps1 -ConfigurePreset windows-asan -BuildPresets windows-asan
```

If Windows PowerShell reports that scripts are disabled, run the script in a
child process with a policy override limited to that process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& ./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release"
```

This builds every configured target, then runs the unit suite, ratchets,
include-graph rules, format-arity audit, packet-index audit, and wire-inventory
freshness check. Logs go under `build/verification/`. The Windows Actions
workflow runs these commands on pushes to `master` (including merged PRs) or
manual dispatch, caches dependency binaries, and retains diagnostic artifacts
for seven days. PR updates do not trigger CI. It does not require
game assets or launch the game; live-server smoke testing remains separate.

Use `CMakeUserPresets.json` for local overrides; it is ignored by git. When
updating dependencies, change the manifest baseline deliberately and check
the deployed DLL names against the client's startup whitelist.

### 5. AddressSanitizer (optional)

MSVC has its own AddressSanitizer, wired up behind `USE_ASAN`. It reports heap
and stack overflows, use-after-free and double frees at the instruction that
causes them, which is the only realistic way to reach the memory-safety defects
in code no unit test can link against.

It needs the **C++ AddressSanitizer** individual component, which the *Desktop
development with C++* workload does not install. Same procedure as ATL in step 1
— close Visual Studio first, and run from an **elevated** terminal:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" --add Microsoft.VisualStudio.Component.VC.ASAN --quiet --norestart
```

Verify it landed:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -requires Microsoft.VisualStudio.Component.VC.ASAN -property installationPath
```

Then generate a **separate** build tree. Do not turn `USE_ASAN` on in your normal
one: it changes the compile and link flags of every target, so the two cannot
share a cache.

```bash
cmake -S . -B build/vs2022-asan -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DUSE_ASAN=ON
cmake --build build/vs2022-asan --config Debug
```

Configure stops immediately with instructions if the component is missing,
rather than letting the build run and fail at link time.

Three things the build does on your behalf, because the sanitizer will not work
otherwise:

- **Drops `/RTC1`.** MSVC rejects the runtime checks outright next to
  `/fsanitize=address`. That is a real loss, since `/RTC1` is what catches
  uninitialised locals, so keep the ordinary Debug tree for day-to-day work and
  switch to this one when chasing a memory error.
- **Turns off incremental linking**, which the sanitizer does not support.
- **Copies `clang_rt.asan_dynamic-x86_64.dll` beside the executables.** Visual
  Studio puts the compiler directory on `PATH` when it launches the debugger, so
  F5 would find it regardless; running the `.exe` from a shell or from Explorer
  would not.

Expect roughly 2x the run time and appreciably more memory. A report goes to
stderr and the debugger breaks at the faulting instruction. Note that leak
detection is a Linux-only part of AddressSanitizer — this catches invalid
accesses, not leaks. The CRT leak dump at exit already covers those.

### Which Debug build to use

There are two, and neither subsumes the other. `/RTC1` and `/fsanitize=address`
are mutually exclusive, so the checks are split across them:

| | `build/vs2022` | `build/vs2022-asan` |
|---|---|---|
| Uninitialised local read | **`/RTC1`** | |
| Stack frame damage around a call | **`/RTC1`** | |
| Heap overflow, use-after-free, double free | | **ASan** |
| Stack buffer overflow | | **ASan** |
| Invalidated iterator, bad comparator | debug iterators | debug iterators |
| Speed | normal | ~2x slower |

Use the ordinary tree day to day. Switch to the sanitized one when you have a
memory error to chase — a wild pointer, a crash inside the allocator, damage
that shows up far from its cause.

`/RTC1` is requested explicitly by `CMakeLists.txt` rather than inherited from
CMake's built-in Debug flags, so overriding `CMAKE_CXX_FLAGS_DEBUG` cannot drop
it silently. Configure prints which checks are on:

```
-- Debug runtime checks: /RTC1 (uninitialised locals, stack frames)
-- AddressSanitizer enabled (/fsanitize=address)
```

Exactly one of those two lines should appear on a Debug configure. If neither
does, something has gone wrong with the flags.

### Never commit `build/`

CMake bakes **absolute paths** into the generated `.sln`/`.vcxproj` files, along
with the exact Windows SDK version and toolset of the machine that generated
them. Those files are not portable between clones, which is why `build/` is
gitignored. Always regenerate locally.

### Troubleshooting

**Diagnostic logging**

The client keeps informational messages, warnings and errors visible by default
in every build. Rendering, input and packet breadcrumbs use DEBUG and are quiet
unless requested. For detailed native diagnostics, set `DARKEDEN_TRACE=1` before
starting the client (PowerShell: `$env:DARKEDEN_TRACE = '1'`; Linux/macOS:
`DARKEDEN_TRACE=1 ./DarkEden`). Unset it afterwards. Only the exact value `1`
enables tracing; this does not change assertions or other build checks.

Logs go to stderr. Windows Debug builds also write `DarkEden-debug.log` beside
the executable, replacing it on each launch. Startup logging omits launch
arguments, and reconnect diagnostics omit authentication keys. Generic packet
traces report metadata rather than packet bodies. Trace logs can still contain
player names and game activity, so review them before sharing.

**`The Windows SDK version 10.0.xxxxx.0 was not found`** or
**`The referenced project <some other path>\ZERO_CHECK.vcxproj does not exist`**

You're opening a solution that was generated on a different machine. "Retarget
solution" won't fix it — it only rewrites the SDK and toolset, leaving the
foreign absolute paths behind. Delete the stale build tree and regenerate with
step 3:

```powershell
Remove-Item -Recurse -Force build\vs2022
```

**`find_package(SDL2 REQUIRED)` fails / configure stops at `CMakeLists.txt:90`**

Either vcpkg packages are missing (step 2), or the toolchain file wasn't passed
on the first configure. Delete `build/vs2022` and redo step 3 — adding the
toolchain to an existing cache does not work.

**`fatal error C1083: Cannot open include file: 'ATLBASE.H'`**

The C++ ATL component is missing — see step 1. CMake locates `atlmfc/include` by
globbing installed VS instances at **configure** time, so after installing ATL
you must re-run step 3 (an incremental build alone won't pick it up).

**Link errors mentioning `iconv.lib`**

`libiconv:x64-windows` isn't installed. See step 2.

**`LNK1104: cannot open file 'atls.lib'`**

Same cause as the `ATLBASE.H` error above — ATL headers embed a
`#pragma comment(lib, "atls.lib")`. Install the ATL component and re-configure.

**`LNK1104: cannot open file 'clang_rt.asan_dynamic_runtime_thunk-x86_64.lib'`**

The C++ AddressSanitizer component is missing — see step 5. `cl.exe` accepts
`/fsanitize=address` without it and only fails at link time, which is why a
`USE_ASAN=ON` configure now checks for the runtime and stops early instead.

**The sanitized build exits immediately with a missing-DLL dialog**

`clang_rt.asan_dynamic-x86_64.dll` is not beside the executable. Re-run the
configure step: the copy happens at configure time, so a build tree generated
before the component was installed will not have it.


## Run

### Code and data are separate

This repository contains the client's **source code only** — no game assets.
The art, maps, audio and configuration are a separate ~1.8 GB download from the
original publisher, too large for git and not ours to redistribute. Building the
client gives you a complete, fully functional executable; it just needs that
data at runtime, the same way an open Doom or Quake engine still needs the
original WAD or PAK files.

Download it from [Mediafire](https://www.mediafire.com/file/017bif66kyieviw/DARKEDEN.zip/file)
or [Baidu Netdisk](https://pan.baidu.com/s/1-DufSEmnydMbOtTwOo_h8A) (extract
code `6bcl`) and unpack it anywhere you like.

The archive also contains `fengshen.exe` along with `DEUtil.dll`, `GL.dll`,
`IFC22.dll` and `basics.dll`. Those are the **old VC6 client** from ~2010, built
against DirectX. The CMake build replaces all of them: `DarkEden.exe` is the
same client built from this source against SDL2, and it links those libraries
statically from `basic/`, `Client/DXLib`, `Client/SpriteLib` and
`Client/framelib`. You do not need to copy any of them — only `Data/` and
`UserSet/`.

### Resource text encoding

`MString` resource records decode to UTF-8 when loaded and encode back to the
same configured code page when saved. The shipped Korean pack defaults to CP949.
For a converted or regional pack, set its actual encoding in
`Data/Info/FileDef.inf`, for example:

```text
RESOURCE_TEXT_ENCODING : UTF-8
```

Accepted values are `UTF-8`, `CP949`, `EUC-KR`, `GBK`, `GB2312`, and `BIG5`
(case insensitive). The setting applies to the whole pack and is read before
resource tables load; it does not change network text. Unknown names fail
initialization. Damaged resource bytes display as replacement characters while
later text remains readable. Saving fails if text cannot be represented in the
selected encoding or its encoded record exceeds 65,536 bytes.

XML resources honor their `encoding` declaration or UTF-8 BOM before the pack
default. Their trees store UTF-8, and XML saves declare UTF-8 and escape markup.
The parser rejects truncated documents, unsupported DTDs, files over 16 MiB,
nesting beyond 64 levels, or more than 100,000 elements and attributes per
document. A rejected document leaves the existing tree intact.

Chat tips, popup files and help-message resources use the same resource encoding
policy, with a UTF-8 BOM overriding the pack default. Help-message reloads keep
the previous collection if a document is incomplete or invalid; saves use UTF-8
with a BOM so they can be read under any configured pack encoding.

### Native GPU sprite rendering

The native client now uses SDL's accelerated texture renderer for ordinary
sprites, tiles, glyphs, fills, scaling, and surface copies when the device
supports render targets. Sprite images are uploaded on first use and reused;
draw order remains the game's original order. No new graphics SDK is required.
Set `DARKEDEN_SPRITE_RENDERER=software` before launching to compare against the
original CPU renderer. A device without acceleration keeps the software path.

The client prefers SDL's OpenGL renderer for shader-based gamma, sprite color
effects, palette screen blending, and palette alpha. Explicit `SDL_RENDER_DRIVER`
overrides are honored; other devices retain ordinary GPU composition with CPU
effect fallbacks. No additional runtime DLL is required.

Clipped UI sprites, world lighting, minimap overlays and scratch-card drawing
also use the GPU surface API. xBRZ runs in two shader passes when OpenGL supports
`GL_ARB_gpu_shader_fp64`; older devices retain the CPU filter. The saved
**Options > Graphics > xBRZ smoothing** preference is honored.

The startup log names the rendering device; shutdown reports texture draws,
uploads and readbacks. GPU tests and a repeatable synthetic benchmark are
documented in [the native renderer notes](docs/native-gpu-renderer.md).

### Toggle xBRZ rendering

Open **Options > Graphics** and use **xBRZ smoothing**, immediately below the FPS
option. It is enabled by default, including with older settings files. Changes
apply immediately and are saved with the other options when you close the menu.
Uncheck it to restore original presentation. No restart or environment variable
is needed. Fullscreen and resolution are also available in Graphics; those two
settings take effect after restarting the client.

This filters the completed frame, including the world, effects, and UI. It uses
the existing assets and preserves game coordinates, mouse mapping, aspect ratio
and letterboxing. It does not add widescreen world visibility or replace artwork.
Because filtering happens after composition, character edges may differ from
the individual transparent sprites in the offline comparison gallery.

xBRZ filters at an integer scale of 2x to 4x, then SDL fits the result to the
window. The GPU path keeps the composed frame and filter intermediates on the
device, with a combined 64 MiB intermediate texture budget. Unchanged frames
reuse the filtered texture. Unsupported devices or GPU allocation failures use
the existing CPU filter (up to four workers); failure of both paths restores
original presentation. Moving scenes filter each new frame, so compare frame
time with the toggle on your machine. The CPU fallback is compiled with `/O2`
in every configuration, Debug included.

The pinned xBRZ source and upstream license are under `third_party/xbrz/`.

### Point the build at the data

`DarkEden.exe` resolves game files relative to its working directory, which is
its own folder (`CMakeLists.txt` sets `VS_DEBUGGER_WORKING_DIRECTORY` so that
F5 in Visual Studio matches double-clicking from Explorer). So `Data/` and
`UserSet/` must sit next to the executable.

Rather than copying 1.8 GB into the build tree, link it. From an ordinary
prompt — directory junctions do not need administrator rights:

```powershell
$dest = "build\vs2022\bin\Debug"
$src  = "C:\path\to\your\unpacked\darkeden"
New-Item -ItemType Junction -Path "$dest\Data"    -Target "$src\Data"
New-Item -ItemType Junction -Path "$dest\UserSet" -Target "$src\UserSet"
```

Repeat for `bin\Release` if you build that configuration.

The shipped data is Korean. The client carries English for the tables it
loads (UI strings, NPC names and dialogue, skill names), but the item and
skill descriptions, help pages, books and tutorial mails are packed in
`Data/Ui/txt/*.rpk`, and their English is a set of loose files the client
prefers over the packed originals. Copy them over the data once:

```powershell
Copy-Item -Recurse -Force tools\i18n\ui-text\Data\* "$src\Data\"
```

`tools/i18n/README.md` describes the translation tables and how to regenerate
the files.

### Launch

Double-click `DarkEden.exe`, or run it without arguments:

```powershell
.\build\vs2022\bin\Debug\DarkEden.exe
```

The default is **borderless fullscreen**, with the original 800x600 game view
scaled to your desktop and xBRZ smoothing enabled. In **Options > Graphics**:

- **Borderless fullscreen (restart)**: uncheck for a window.
- **1024x768 resolution (restart)**: check for the larger game view and UI layout;
  uncheck for the original 800x600 view.
- **xBRZ smoothing**: switches instantly.

Close Options to save. Display preferences live in `UserSet/Display.ini` and are
read before creating the window. On Windows, paths resolve from the EXE folder
even when a shortcut has a different working directory. Restart after changing
fullscreen or resolution; no launch arguments are needed.

Existing launcher arguments remain optional overrides for compatibility:

| Argument | Mode |
| --- | --- |
| `0000000001` | window, scaled |
| `0000000002` | fullscreen, scaled |
| `0000000003` | window, 1024x768 |
| `0000000004` | fullscreen, 1024x768 |

### Connect to a server

Set the login server IP and port in `Data/Info/GameClient.inf`. With a server in
Docker on the same machine, use `127.0.0.1`.

## Build on Linux

Windows is the live platform; Linux builds every target, runs the same test
suite, and the client starts (see the status at the end of this section).
Ubuntu 24.04 is the reference, with GCC 13 or Clang 18 and the system SDL2:

```bash
sudo apt-get install build-essential clang cmake ninja-build perl \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
    libjpeg-dev libfreetype-dev fonts-dejavu-core fonts-noto-cjk
```

The fonts are not optional: nothing under `Data/` is a font, and
`Client/TextSystem/TextBackendSDL.cpp` looks for Noto CJK and DejaVu under
`/usr/share/fonts`. Without one of them the client draws no text.

Three presets do the configure (`CMakePresets.json`), and a fourth builds
the packet-read fuzzers; each writes under
`build/presets/<preset>/`:

| Preset | Compiler | Flags |
|---|---|---|
| `linux` | GCC | Debug |
| `linux-clang` | Clang | Debug |
| `linux-asan` | GCC | `-fsanitize=address,undefined` |
| `linux-fuzz` | Clang | `linux-clang` with ASan, UBSan and the libFuzzer targets; needs the `libclang-rt` package (CLAUDE.md, *Fuzzing the packet readers*) |

```bash
cmake --preset linux
cmake --build --preset linux
ctest --test-dir build/presets/linux --output-on-failure
```

`tools/ci/verify-linux.sh <preset>` (any preset but `linux-fuzz`) runs
those three steps the way the
Linux workflow (`.github/workflows/linux.yml`) does, with UBSan reports made
fatal under `linux-asan`. On a Windows machine or a Mac with Docker
(OrbStack, which this was verified with, or Docker Desktop),
`tools/linux/Dockerfile` is the same environment as an image (on an x86_64
host; see below for Apple Silicon):

```bash
docker build -t darkeden-linux tools/linux
docker run --rm -v "$PWD:/src" -v darkeden-build:/src/build darkeden-linux \
    tools/ci/verify-linux.sh linux
```

On Apple Silicon the image is arm64, so it is not the CI environment: plain
`char` is unsigned there, and the warning profile differs. The configure, the
build and every test pass, but the script's last step fails with `missing
warning baseline for linux-aarch64`: `tools/ci/warning-baselines.json`
records the x86_64 runner's warnings only. To get the
x86_64 profile and its warning baseline, build and run the image as x86_64,
with its own build volume so it does not reuse the arm64 CMake tree:

```bash
docker build --platform linux/amd64 -t darkeden-linux-amd64 tools/linux
docker run --rm --platform linux/amd64 -v "$PWD:/src" -v darkeden-build-amd64:/src/build \
    darkeden-linux-amd64 tools/ci/verify-linux.sh linux
```

This was run once on an Apple Silicon Mac (2026-09-29, Docker under OrbStack):
the `linux` preset configured, built with `--clean-first`, passed its tests
(1243 tests, 0 failed) and the x86_64 warning check, in under 30 minutes
emulated. It is the x86_64 profile, not the CI runner itself: `linux.yml`
runs on `ubuntu-24.04` directly, not in this image.

### Status of the Linux client

The port's plan and its sizing are `docs/linux-macos-port-assessment-2026-09-07.md`.
What is verified today, in the order its acceptance criteria list them:

- **Builds:** every library, `unit_tests` and `DarkEden`, with GCC and Clang,
  and the GCC tree under ASan and UBSan with nothing reported. The suite is the
  Windows suite.
- **Runs, partly:** launched from `build/presets/linux/bin` with the `Data/`
  tree beside the executable (see *Point the build at the data*; the resolver
  in `basic/DataPath.h` takes care of the tables' backslashes and letter case),
  the client loads every data table, reaches the main menu and exits cleanly
  on `SDL_QUIT`. That has been exercised headless (`SDL_VIDEODRIVER=dummy`,
  where the quit came from SIGTERM), not on a display. Login, character select, a zone
  and chat against the docker server are **not verified** off Windows.
- **macOS:** see the next section.

The Korean IME is SDL text input on every platform, sound is SDL_mixer, and the
launcher arguments and `UserSet/Display.ini` go through the same code as on
Windows; none of that has been watched on a Linux display yet.

## Build on macOS

The same tree, the same tests, Apple Clang. CI builds and tests it on
GitHub's arm64 and Intel runners (`.github/workflows/macos.yml`, macOS 15),
and it has been built and tested on an Apple Silicon Mac (macOS 27.0, Apple
Clang 21, CMake 4.4: every target, all 20 ctest tests passing on 2026-09-30,
among them the three packet-read fuzz replay tests, `fuzz_replay_client_stream`,
`fuzz_replay_client_login_stream` and `fuzz_replay_client_datagram`, and their
corpus steps). Nothing has
been watched on a Mac's display yet. Dependencies come from Homebrew, on
Apple Silicon or Intel:

```bash
xcode-select --install
brew install cmake ninja sdl2 sdl2_image sdl2_ttf sdl2_mixer jpeg-turbo
```

Homebrew's `sdl2` is now an alias of `sdl2-compat`, the SDL2 API over SDL3;
that is what the build above linked. The linker warns that Homebrew's
libraries were built for a newer macOS than the preset's 13.0 deployment
target, which is harmless for a build run on the machine that made it.

No font package: `Client/TextSystem/TextBackendSDL.cpp` uses the system's own
(Apple SD Gothic Neo, Arial Unicode, Hiragino Sans GB, Helvetica). Two presets,
and a third for the packet-read fuzzers,
each under `build/presets/<preset>/`; CMake finds Homebrew through the
preset's prefix path, or through `HOMEBREW_PREFIX` when configuring by hand:

| Preset | Compiler | Flags |
|---|---|---|
| `macos` | Apple Clang | Debug |
| `macos-asan` | Apple Clang | `-fsanitize=address,undefined` |
| `macos-fuzz` | Apple Clang | `macos-asan` plus the libFuzzer targets, linking Homebrew `llvm@21`'s libFuzzer (CLAUDE.md, *Fuzzing the packet readers*) |

```bash
cmake --preset macos
cmake --build --preset macos
ctest --test-dir build/presets/macos --output-on-failure
```

`tools/ci/verify-linux.sh macos` runs those steps the way the workflow does,
then checks the warning budget in `tools/ci/warning-baselines.json`. The
project's own code builds without warnings; what the budget counts is the
vendored code under `third_party/` and `build/_deps/` and the linker. It is
recorded from the CI runners' compilers, so a different compiler version
can count differently: under Apple Clang 21 the build and the tests pass,
but the budget step reports the linker's deployment-target warnings above
and a few more vendored-code diagnostics than the runner's Apple Clang.
The executable is `build/presets/macos/bin/DarkEden`, run with the `Data/`
tree beside it as on Linux. The macOS equivalent of the junctions and the
copy in *Point the build at the data* is two symbolic links and a one-time
copy. These commands were written from the code, not run against the game
data. `src` must be an absolute path: a relative one is resolved against
`$dest`, and the link then points at nothing. `-sfn` replaces a link from an
earlier run; plain `ln -s` on an existing link to a directory follows it and
creates a link back to itself inside the game data.

```bash
dest=build/presets/macos/bin
src=/path/to/your/unpacked/darkeden
ln -sfn "$src/Data"    "$dest/Data"
ln -sfn "$src/UserSet" "$dest/UserSet"
cp -R tools/i18n/ui-text/Data/. "$src/Data/"  # the English UI text, once
```

Then launch it (the launcher arguments in *Launch* work the same way):

```bash
./build/presets/macos/bin/DarkEden
```

Launched from Finder or as a bundle
(`-DDARKEDEN_MACOS_BUNDLE=ON` builds `bin/DarkEden.app`), the client looks
for `Data/Info/FileDef.inf` under its working directory, its executable's
directory and the directory the bundle sits in, runs from the first that
has it, and says on stderr which directories it tried when none has it.
The bundle does not copy the data, and the game writes beside its data
(`UserSet/`, `Log/`), so the data does not go inside the bundle: put
`Data/` beside `DarkEden.app`. A bundle downloaded with a quarantine flag
is run by macOS from a random read-only copy (App Translocation), where
"beside the bundle" is nowhere; build it locally or clear the flag with
`xattr -d com.apple.quarantine`.

The window asks for `SDL_WINDOW_ALLOW_HIGHDPI`, so on a Retina display the
game frame is scaled to the native pixel grid and mouse points are mapped
through the point-to-pixel ratio (`tests/unit/test_present_geometry.cpp`
pins the mapping; no test machine has such a display). Whether macOS
honours the flag for a bare executable, which has no `Info.plist` to say
`NSHighResolutionCapable`, or magnifies it as it does an app without the
key, has not been seen; the bundle declares the key. The xBRZ filter's
factor is chosen from the pixel size, so the same display costs more CPU
per frame than a non-Retina one of the same point size.
