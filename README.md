# opendarkeden-client

The OpenDarkEden client: the isometric horror MMORPG of slayers, vampires and ousters, a Korean
original from roughly 2000-2010, ported from Win32 + DirectX to SDL2. C++20 and CMake. Windows
with Visual Studio 2022 is the main platform; Linux, macOS and a WebAssembly build build and test
too.

This repository holds the source code only; the game data is a separate download (see [Run](#run)).
This guide takes you from a clone to a running client. The rules behind the code, the test
strategy and the design notes live in the knowledge bundle under `docs/`: start at
[docs/index.md](docs/index.md).

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
The first install can compile libraries from source and take a while.

| Package | Used for |
| --- | --- |
| `sdl2`, `sdl2-image[libjpeg-turbo]`, `sdl2-ttf` | rendering, sprites, help JPEGs, text (required) |
| `sdl2-mixer` | sound and music |
| `openssl` | verified TLS for optional native WebSocket connections |
| `libiconv` | shared resource and legacy text conversion in `basic/TextEncoding.cpp` |

Existing classic-mode build trees keep their old dependency setup; use the
presets below to validate the pinned manifest without changing those trees.
Existing classic-mode installs need SDL_image's JPEG feature for help artwork,
`vcpkg install sdl2-image[libjpeg-turbo]:x64-windows --classic --recurse`, and
OpenSSL, `vcpkg install openssl:x64-windows --classic`.

vcpkg can live anywhere — just point `-DCMAKE_TOOLCHAIN_FILE` at wherever you
put it in the next step. `vcpkg integrate install` is optional.

The [browser and WebSocket guide](docs/browser/browser-client.md) covers the
WebAssembly / WebGL 2 build, asset packaging, gateway configuration and browser
tests.

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

Other ways to build:

- The optional GNU Make wrapper requires a POSIX shell (Git Bash on Windows).
  Its build and test targets select Debug/Release explicitly, including with the
  Visual Studio generator. Pass initial configure options through `CMAKE_ARGS`,
  for example `make debug CMAKE_ARGS=-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`.
  `NPROCS` overrides the detected parallel job count. The direct CMake commands
  above remain the Windows setup path.
- Ninja and Makefile generators write `compile_commands.json` into the selected
  build directory. Point clangd or another editor at that directory; a local copy
  or symlink at the repository root is ignored. Visual Studio generators do not
  produce this database.
- `BUILD_ENGINE=OFF` omits the map viewer, whose loader requires the optional
  sprite engine. The effect viewer uses the client libraries and remains available.
  Its Bash build/run helper accepts `DARKEDEN_DIR` (the directory containing
  `Data/`, default `DarkEden/` beside this README), `BUILD_DIR` and `NPROCS`.

### Presets and the complete test suite

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

To run the same complete verification as CI, including checking that every
required test is registered:

```powershell
./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release
./tools/ci/verify-windows.ps1 -ConfigurePreset windows-asan -BuildPresets windows-asan
```

It builds every configured target, runs the whole suite and checks the warning
budget; logs go under `build/verification/`. It needs no game data and does not
launch the game. [Build commands](docs/build/build-commands.md) has the other
presets (`windows-release`, `windows-asan`, the `windows-clang` ISO C++ check),
what to do when PowerShell refuses to run scripts, and local preset overrides.
CI runs these scripts on pushes to `master` and on manual dispatch, never on a
pull request ([CI](docs/build/ci.md)).

### 5. AddressSanitizer (optional)

MSVC has its own AddressSanitizer, wired up behind `USE_ASAN`. It reports heap
and stack overflows, use-after-free and double frees at the instruction that
causes them.

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

Configure stops immediately with instructions if the component is missing. The
build drops `/RTC1`, turns off incremental linking and copies the sanitizer's
runtime DLL beside the executables; expect roughly 2x the run time. It catches
invalid accesses, not leaks.

### Which Debug build to use

There are two, and neither subsumes the other: `build/vs2022` has `/RTC1`
(uninitialised locals, stack frame damage), `build/vs2022-asan` has ASan (heap
and stack overflows, use-after-free, double free). Use the ordinary tree day to
day, and switch to the sanitized one when you have a memory error to chase — a
wild pointer, a crash inside the allocator, damage that shows up far from its
cause. Configure prints which checks are on; exactly one of the two lines should
appear. [Build commands](docs/build/build-commands.md#the-two-windows-debug-trees)
has the full table and the lines to look for.

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

**The client exits at once, with no message and no log**

A DLL beside the executable that the client does not recognise stops it before
it starts logging; a DLL left behind by an earlier build counts too.
[Running the client](docs/operations/running-the-client.md) explains the check.

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
- **xBRZ smoothing**: switches instantly; uncheck it for the original presentation.

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

### Resource text encoding

The shipped Korean pack is CP949, the default. For a converted or regional
pack, set its actual encoding in `Data/Info/FileDef.inf`, for example:

```text
RESOURCE_TEXT_ENCODING : UTF-8
```

Accepted values are `UTF-8`, `CP949`, `EUC-KR`, `GBK`, `GB2312`, and `BIG5`
(case insensitive). The setting applies to the whole pack and does not change
network text; an unknown name fails initialization.
[Resource text encoding](docs/operations/resource-text-encoding.md) has how XML
files, BOMs, damaged bytes and saves behave.

### Rendering

The client draws sprites, tiles, glyphs, fills and scaled copies with SDL's
accelerated renderer when the device supports render targets, preferring SDL's
OpenGL renderer for the shader effects; no graphics SDK or extra DLL is needed.
Set `DARKEDEN_SPRITE_RENDERER=software` before launching to compare against the
original CPU renderer. An explicit `SDL_RENDER_DRIVER` is honoured, and a device
without acceleration keeps the software path.
[Native GPU sprite composition](docs/rendering/native-gpu-renderer.md) covers
the GPU path, the xBRZ filter and its fallbacks, the GPU tests and a repeatable
benchmark.

## Build on Linux

Windows is the live platform; Linux builds every target, runs the same test
suite, and the client starts and reaches the main menu
([what is verified](docs/build/linux-and-macos.md#status-on-linux)).
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
| `linux-fuzz` | Clang | `linux-clang` with ASan, UBSan and the libFuzzer targets; needs the `libclang-rt` package ([fuzzing](docs/protocol/fuzzing.md)) |

```bash
cmake --preset linux
cmake --build --preset linux
ctest --test-dir build/presets/linux --output-on-failure
```

`tools/ci/verify-linux.sh <preset>` (any preset but `linux-fuzz`) runs those
three steps the way the Linux workflow does. On a Windows machine or a Mac with
Docker, `tools/linux/Dockerfile` is the same environment as an image;
[the Linux image](docs/build/linux-and-macos.md#the-linux-image) has the
commands, and the x86_64 run an Apple Silicon Mac needs for the warning check.

The executable is `build/presets/linux/bin/DarkEden`, run with the `Data/`
tree beside it as in [Point the build at the data](#point-the-build-at-the-data).

## Build on macOS

The same tree, the same tests, Apple Clang, on Apple Silicon or Intel. CI
builds and tests it on GitHub's arm64 and Intel runners
([what is verified](docs/build/linux-and-macos.md#status-on-macos)).
Dependencies come from Homebrew:

```bash
xcode-select --install
brew install cmake ninja sdl2 sdl2_image sdl2_ttf sdl2_mixer jpeg-turbo
```

No font package is needed: the client uses the system's own fonts. Two presets,
and a third for the packet-read fuzzers, each under `build/presets/<preset>/`;
CMake finds Homebrew through the preset's prefix path, or through
`HOMEBREW_PREFIX` when configuring by hand:

| Preset | Compiler | Flags |
|---|---|---|
| `macos` | Apple Clang | Debug |
| `macos-asan` | Apple Clang | `-fsanitize=address,undefined` |
| `macos-fuzz` | Apple Clang | `macos-asan` plus the libFuzzer targets, linking Homebrew `llvm@21`'s libFuzzer ([fuzzing](docs/protocol/fuzzing.md)) |

```bash
cmake --preset macos
cmake --build --preset macos
ctest --test-dir build/presets/macos --output-on-failure
```

`tools/ci/verify-linux.sh macos` runs those steps the way the workflow does,
then checks the warning budget in `tools/ci/warning-baselines.json`, which is
recorded from the CI runners' compilers, so another compiler version can count
differently: under Apple Clang 21 the build and the tests pass while the budget
step reports more warnings than CI's ([the macOS notes](docs/build/linux-and-macos.md#status-on-macos)).

The executable is `build/presets/macos/bin/DarkEden`, run with the `Data/`
tree beside it as on Linux. The macOS equivalent of the junctions and the copy
in *Point the build at the data* is two symbolic links and a one-time copy
(written from the code, not yet run against the game data). `src` must be an
absolute path: a relative one is resolved against `$dest`, and the link then
points at nothing. `-sfn` replaces a link from an earlier run; plain `ln -s` on
an existing link to a directory follows it and creates a link back to itself
inside the game data.

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

To launch from Finder, configure with `-DDARKEDEN_MACOS_BUNDLE=ON`, which builds
`bin/DarkEden.app`, and put `Data/` beside `DarkEden.app`, not inside it.
[The macOS notes](docs/build/linux-and-macos.md#status-on-macos) cover how the
bundle finds its data, quarantined downloads and Retina displays.
