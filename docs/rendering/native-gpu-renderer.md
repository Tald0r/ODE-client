---
type: Design
title: "Native GPU sprite composition"
description: "SpriteGpu composes sprites, glyphs, fills and scaled copies on SDL's accelerated device, with the CPU rasterizer kept as the fallback."
tags: [rendering]
---

# Native GPU sprite composition

The native client installs `SpriteGpu` on SDL's existing accelerated device.
The existing sprite API and CPU rasterizer remain available. This change moves
ordinary RGB/RLE/index/alpha sprite and glyph composition, fills, scaled
sprites and surface copies onto texture render targets. It does not change
the game protocol, assets, animation timing or world draw order, and it does
not add a browser build.

## Operation

`CSDLGraphics::Init` installs the device. GPU drawing is enabled by default
when accelerated render targets are supported. `DARKEDEN_SPRITE_RENDERER=software`
selects the original rasterizer before startup. Missing capabilities and
allocation failures use the CPU path.

The native window initialization now prefers SDL's `opengl` device, which
provides the programmable effect path. An explicit `SDL_RENDER_DRIVER` is
honored. If automatic OpenGL creation fails, SDL's normal accelerated/software
selection is retried. OpenGL functions are loaded through SDL; no new DLL or
graphics SDK is needed. The wrapped Win32 window is marked OpenGL-compatible
before `SDL_CreateWindowFrom`, and is exercised by a separate renderer test.

Sprite textures are decoded/uploaded lazily and cached within a 128 MiB pixel
budget. Destruction, reload and RLE row replacement invalidate their textures.
At the budget, one sorted pass evicts the least recently used sprite textures
down to 7/8 of it; evicting one at a time scanned the whole cache for each,
which stalled the frames that cast large spell effects. Surface render
targets and upload textures have a separate combined 128 MiB pixel budget;
surfaces beyond that budget use software. Driver allocation overhead is
additional. All renderer operations run on the SDL render thread.

Commands retain their original order. SDL batches consecutive compatible
commands; switching targets, synchronizing pixels, and presentation finish
the relevant work. RLE transparency comes from run coverage, so an opaque
black pixel remains opaque. The current RGB565 RLE path's alpha behavior is
preserved. GPU alpha blending uses the device's channel arithmetic and can
differ from the software renderer's per-operation RGB565 truncation.

Each surface records whether CPU or GPU pixels are current. Scoped backend
pixel access synchronizes GPU contents before CPU effects run. Raw pointer
access (`GetSurfaceInfo`, `GetDDSD`, `GetSurfacePointer`, or explicit
`Lock(nullptr, &pitch)`) keeps that surface on the CPU until `Unlock`, including
across presentation. A no-argument `Lock()` now returns a boolean drawing-state
flag; it does not expose pixels or force a readback. This distinction avoids
reading back the framebuffer for every legacy lock around a sprite draw.

At presentation, modified persistent offscreen surfaces are checkpointed to
CPU memory for device-reset recovery. Unchanged surfaces need no checkpoint.
The intermediate game framebuffer and the world tile cache are explicitly
transient, so they skip this readback. Both framebuffers are redrawn after
reset, and the input event hook invalidates the tile cache, which is redrawn
whole. The tile cache changes on every scroll while walking, and reading it
back each time stalled the GPU (a hitch every few steps). Cached sprite
textures are recreated lazily.
Normal device replacement synchronizes surfaces before destroying resources.

## Programmable effects

`SpriteGpuEffects` implements RGB565 gamma, channel tinting, darkness,
grayscale, color-set lookup, palette screen blending, and per-pixel palette
alpha in GLSL. The effects preserve the CPU routines' integer channel math,
including the six-bit green channel and alpha's 0..32 range. Transparent run
coverage is separate from palette index and color, so opaque black stays
opaque. Palette indices/alpha upload once per sprite; the 256-entry color
lookup uploads when its values change, including mutations through references.
Palette caches share the sprite memory budget and are invalidated on release,
replacement, reset, and renderer destruction.

Destination-dependent effects copy only the affected rectangle to a GPU
scratch target before sampling it; they do not read the framebuffer into CPU
memory. Scratch targets count toward the surface memory budget. Overlapping
self-copies use the same mechanism and now remain on the GPU too.

The shader adapter flushes SDL commands before direct OpenGL calls, and restores
the GL program, texture units, viewport, clipping, and other modified state
before SDL resumes. This uses SDL's supported
[texture binding](https://wiki.libsdl.org/SDL2/SDL_GL_BindTexture) and
[command flushing](https://wiki.libsdl.org/SDL2/SDL_RenderFlush) interfaces.

## Frame composition and xBRZ

Clipped descriptions, progress bars and cooldown sprites now use the surface
API without borrowing framebuffer memory. Minimap markers, chat tails and
scratch-card erasure use GPU fills. Minimap region tinting samples a GPU
snapshot. Scratch progress uses a separate coverage mask, seeded once when the
cover image loads; querying progress no longer reads rendered pixels.

World lighting submits the existing light grid as one shader batch. It preserves
the RGB565 channel arithmetic, uneven cell sizes at 800x600, and 1024x768 cells.
The game continues to calculate light levels and sprite positions on the CPU.

The GPU xBRZ implementation adapts the vendored RGB algorithm in two passes:
source-resolution corner classification, then 2x/3x/4x output blending. Input
conversion uses the local SDL RGB565 expansion table. Double precision is used
near decision boundaries to preserve the CPU algorithm's distance comparisons;
this requires `GL_ARB_gpu_shader_fp64`. Most distance calculations use floats.
This is a native OpenGL implementation, not yet a WebGL backend.

The combined xBRZ intermediate texture budget is 64 MiB. Unchanged surface
revisions reuse the filtered texture. Drawing, explicit CPU writes, resize,
setting changes, renderer replacement and reset invalidate the cache. The saved
xBRZ preference, aspect ratio, letterbox, mouse mapping and fractional scaling
are preserved. Turning the setting off releases the intermediate textures.

## The xBRZ setting

The player turns the filter on and off under **Options > Graphics > xBRZ smoothing**, immediately
below the FPS option. It is enabled by default, including with older settings files that predate
it (`VS_UI/src/UserOption.cpp`). A change applies at once and is saved with the other options when
the menu closes; unchecking it restores the original presentation, with no restart or environment
variable. Fullscreen and resolution, in the same tab, take effect after a restart.

The filter runs on the completed frame, world, effects and UI together, at an integer scale of 2x
to 4x, and SDL then fits the result to the window. It uses the existing assets and preserves game
coordinates, mouse mapping, aspect ratio and letterboxing; it adds no widescreen world visibility
and replaces no artwork. Because it filters after composition, character edges can differ from
the individual transparent sprites in the offline comparison gallery. Moving scenes filter every
new frame, so compare frame time with the toggle on and off on the machine in question. Without the
shader path, or when a GPU allocation fails, the CPU filter runs on up to four worker threads
(`Client/SpriteLib/FrameUpscaler.cpp`); if both paths fail, the original presentation is
restored. The CPU fallback is compiled with `/O2` in every configuration, Debug included
(`third_party/xbrz/CMakeLists.txt`), and the pinned xBRZ source and its upstream license are under
`third_party/xbrz/`.

The startup log names the rendering device, and shutdown logs the texture draws, uploads and
readbacks (`Sprite composition: ... draws, ... sprite uploads, ... surface uploads, ... readbacks`).

## Compatibility and remaining CPU work

Unsupported devices, texture allocation failures and explicit software mode
retain the CPU renderer. Devices without programmable effects use CPU effects;
without the xBRZ shader capability, they use the existing multithreaded CPU
filter. Unregistered custom effect callbacks and malformed palette alpha values
also retain their existing fallback.

The active Windows game/UI framebuffer drawing callers were audited and migrated.
Resource loading, guild/profile sprite extraction, screenshots and the initial
scratch-card coverage seed still use explicit pixel access. Persistent offscreen
surfaces modified during a frame are read back at presentation for reset recovery;
unchanged cached surfaces and the presented framebuffer require no checkpoint.
Thus a scene with modified offscreen caches can still report readbacks. CPU
fallbacks, resource work and reset recovery are distinct from GPU composition.
The older non-Windows colored-panel helper retains its compatibility pixel path.

Existing SDL-port stubs retain their previous behavior. This migration does not
implement missing legacy DirectDraw effects or change game protocol/asset formats.
RGBA glyph blending retains the device's arithmetic; subchannel rounding can
differ from the software RGB565 renderer, as described above.

## Completion checklist

- [x] Native cached GPU sprites, fills, scaling, and surface copies.
- [x] GPU gamma and the currently registered source and palette effects.
- [x] CPU/GPU ownership, resource budgets, cache invalidation, and reset recovery.
- [x] Replace active Windows framebuffer drawing callers in the game/UI.
- [x] Move xBRZ presentation onto the GPU while honoring the saved setting.
- [x] Verify complete mixed rendering without CPU composition/readbacks,
  rerun native/ASan checks, and measure the final renderer.

## Verification

The regular CTest suite includes `sprite_gpu_tests`, which exercises texture
rendering using SDL's software device without requiring a display. It covers
opaque black and transparent RLE holes, clipping, overlapping draws, texture
reuse/invalidation, mixed CPU/GPU access, scaling, surface copies, alpha,
xBRZ, device detach and render-target reset recovery.

The same executable can require a real accelerated device in a hidden window:

```powershell
cmake --build build/vs2022 --config Debug --target sprite_gpu_tests
./build/vs2022/bin/Debug/sprite_gpu_tests.exe --accelerated
```

It prints the device name and fails if acceleration is unavailable. The
ASan build supports the same invocation. Neither invocation starts the game
or requires game assets or a server.

Require the programmable backend (failure to create it is a test failure):

```powershell
./build/vs2022/bin/Debug/sprite_gpu_tests.exe --shaders
./build/vs2022/bin/Debug/sprite_gpu_tests.exe --foreign-shaders # Windows native HWND
./build/vs2022-asan/bin/Debug/sprite_gpu_tests.exe --foreign-shaders
```

The shader checks compare all 65,536 RGB565 inputs against the CPU gamma and
source-effect routines, compare palette screen/alpha output against CPU blits,
and cover palette mutation, cache reload, clipping, transparent holes, reset,
and ordinary SDL drawing after shaders. Counters assert zero readbacks during
the GPU operations; subsequent test readbacks inspect the result.

The xBRZ oracle checks compare about 29.7 million output pixels across 2x/3x/4x:
binary neighborhoods, RGB565 random inputs, palette noise, one-pixel boundaries,
and complete 800x600 mixed scenes. The mixed scenes combine cached terrain,
moving sprites, palette screen/alpha effects, lighting, clipped UI and glyphs.
Their steady frames assert zero sprite uploads, surface uploads and composition
readbacks. Offscreen reset checkpoint behavior is tested separately.

For repeatable Release benchmarks:

```powershell
cmake --build build/vs2022 --config Release --target sprite_gpu_tests
./build/vs2022/bin/Release/sprite_gpu_tests.exe --benchmark-mixed
./build/vs2022/bin/Release/sprite_gpu_tests.exe --benchmark-xbrz
./build/vs2022/bin/Release/sprite_gpu_tests.exe --benchmark
```

Local OpenGL measurements on 2026-09-24:

| Workload | CPU ms/frame | GPU ms/frame |
|---|---:|---:|
| Mixed 800x600 scene, composition only | 1.033 | 3.192 |
| Same scene with 2x xBRZ | 11.236 | 3.977 |
| Dense palette noise, xBRZ 2x filter | 7.187 | 1.295 |
| Dense palette noise, xBRZ 3x filter | 7.561 | 2.370 |
| Dense palette noise, xBRZ 4x filter | 7.887 | 1.864 |

The mixed benchmark uses 120 changing frames with 160 sprites/effects, a 64x64
light grid, UI and glyphs. GPU timing includes a one-pixel read to wait for
completed work; this measurement-only read is outside composition counters.
Steady GPU frames upload no sprites or surfaces and perform no composition
readbacks. The filter-only benchmark uses 60 changing frames and four CPU
workers. These are synthetic timings, not game FPS measurements. GPU composition
alone is slower in this workload; xBRZ provides the measured overall improvement.
The `--benchmark` mode retains the original RLE-only comparison with a full-frame
readback, so its timing is not directly comparable to the other two modes.

Final verification: the complete native Debug build and all 12 CTest entries
passed. The 27 renderer tests passed with 3,175,922 checks on OpenGL in a wrapped
Win32 window, in both Debug and ASan. Applicable tests also passed on SDL software
and Direct3D devices. ASan unit tests passed (1,049 tests / 1,406,292 checks) and
UI tests passed (90 tests / 6,066 checks), including scratch coverage. The full
game was not launched; live scene appearance and whole-game frame times remain
unmeasured.
