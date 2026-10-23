---
type: Rule
title: "Library boundaries"
description: "Only code compiled into a static library links into a test binary; membership is the four tests/arch lists CMake reads, the executable's own source is SDLMain.cpp alone, and a new target names its sources, never a glob."
tags: [architecture, testing]
enforced_by: [arch_includes, ratchets]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# Library boundaries

**Only code compiled into a static library can be tested.** Code compiled straight into the
`DarkEden` executable cannot be linked into a test binary. That structural limit was the single
biggest constraint on how work got verified here, and the [restructuring plan](../reviews/RESTRUCTURING.md)
existed to shrink it: since task 4.265 (2026-10-19) the executable's own source is
`Client/SDLMain.cpp` alone, and every packet handler is a [gamemodel](gamemodel.md) member. What
still cannot be reached from a test is verified as [executable-only code](../testing/executable-only-code.md).

The libraries, and what each holds:

| Library | What it holds |
|---|---|
| `basic` | memory, exceptions, typedefs, platform abstraction, the logging facility (`DebugLog.h`) and the checked formatter (`SafeFormat.h`), the one library every other links |
| `SpriteLib` | sprite decode and blitting, the SDL backend, the 555/565 variants |
| `dxlib` | input, sound and music behind a DirectX-shaped interface, SDL underneath |
| `framelib`, `TextSystem` | frames, frame packs and effect resources; UTF-8 text rendering on SDL and freetype2 |
| `VS_UI` | the UI framework: widgets, dialogs, skinning, the Korean IME |
| `packetwire` | the whole wire layer ([packetwire](packetwire.md)) |
| `gamemodel` | the extracted model logic, the world and every packet handler ([gamemodel](gamemodel.md)) |
| `gameview`, `gameclient` | the view's drawing and the client that puts the game together ([gameview and gameclient](gameview-and-gameclient.md)) |
| `decore` | the server's shared rules, vendored byte-identical ([de-core](decore.md)) |

Membership of the four extracted libraries is a file each under `tests/arch/`:
`packetwire_files.txt`, `gamemodel_files.txt`, `gameview_files.txt` and `gameclient_files.txt`,
read by CMake, by the include checker (`tests/arch/check_includes.pl`, ctest `arch_includes`) and
by the ratchet script. The executable's own source is `Client/SDLMain.cpp` alone: `main` and
`WinMain` install `ClientHost`'s `SystemHost()` and call `ClientMain`. Ratchet R1 holds the
count of translation units compiled directly into `DarkEden` at **1**
([ratchets](../testing/ratchets.md)).

**New library targets use explicit source lists, never `file(GLOB)`.** An explicit list makes
membership reviewable and is what the ratchet script and the include checker parse. The globs that
remain use `CONFIGURE_DEPENDS`, so a build re-globs on its own, but a direct MSBuild target can need
a retry after a source-list change. After moving files, reconfigure both Debug trees.

For a change, first identify the target that owns the implementation. Library code can be linked
by unit tests; executable-only logic needs an existing exemption in the
[restructuring plan](../reviews/RESTRUCTURING.md#exemption-list-tangled-outside-the-unit-test-libraries)
or an extraction into a testable library. A header include or a zero-valued ratchet alone does not
prove that a component links independently: a [link proof](../testing/link-proofs.md) does.

# Source layout

| Path | What |
|---|---|
| `Client/` | game logic (`GameMain`, `MZone`, `MCreature`, `MPlayer`, `MItem`, `MSkill`), compiled into `gamemodel` and `gameclient`; `SDLMain.cpp` is the executable |
| `Client/Packet/` | the wire layer, compiled once as `packetwire`; `Gpackets/` is server to client |
| `Client/PacketHandler/` | the packet handlers, all `gamemodel` members, each bound to its id in `Client/PacketHandlerRegistry.cpp` |
| `Client/SpriteLib/` | sprite decode and blitting, the SDL backend and GPU composition, the 555/565 variants |
| `Client/DXLib/` | input, sound and music behind a DirectX-shaped interface, SDL underneath |
| `Client/TextSystem/` | UTF-8 text rendering on SDL and freetype2 |
| `Client/framelib/` | frames, frame packs and effect resources |
| `VS_UI/` | the UI framework: widgets, dialogs, skinning, the Korean IME |
| `basic/` | memory, exceptions, typedefs, platform abstraction, logging, the checked formatter |
| `third_party/` | vendored code: [de-core](decore.md), UnRAR, xBRZ |
| `tests/` | the [test framework](../testing/test-framework.md), the unit tests, the goldens, the fuzz targets, the ratchets and the audit scripts |
| `tools/` | the CI verification scripts, the de-core sync, the fuzz corpus converter, the commit hook, the translation tables and the web build |
| `docs/` | the knowledge bundle ([index](../index.md)) |

`Client/D3DLib/`, `VolumeLib/`, `DEUtil/`, `MZLib/` and `TextLib/`, which older documents list, no
longer exist.
