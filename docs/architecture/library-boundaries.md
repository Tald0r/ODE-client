---
type: Rule
title: "Library boundaries"
description: "Only code compiled into a static library links into a test binary; membership is the four tests/arch lists CMake reads, the executable's own source is SDLMain.cpp alone, and a new target names its sources, never a glob."
tags: [architecture, testing]
enforced_by: [arch_includes, ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Library boundaries

**Only code compiled into a static library can be tested.** Game logic compiled straight into
the `DarkEden` executable, the packet handlers under `Client/PacketHandler/` among it (all but the
seventeen [gamemodel](gamemodel.md) lists), cannot be linked into a test binary. That is a
structural limit, and it is the single biggest constraint on how work gets verified here
([executable-only code](../testing/executable-only-code.md)).

The libraries, and what each holds:

| Library | What it holds |
|---|---|
| `basic` | memory, exceptions, typedefs, platform abstraction, the logging facility (`DebugLog.h`) and the checked formatter (`SafeFormat.h`), the one library every other links |
| `SpriteLib` | sprite decode and blitting, the SDL backend, the 555/565 variants |
| `dxlib` | input, sound and music behind a DirectX-shaped interface, SDL underneath |
| `framelib`, `TextSystem` | frames and collision; UTF-8 text rendering on SDL and freetype2 |
| `VS_UI` | the UI framework: widgets, dialogs, skinning, the Korean IME |
| `packetwire` | the whole wire layer ([packetwire](packetwire.md)) |
| `gamemodel` | the extracted model logic and the seventeen model-only packet handlers ([gamemodel](gamemodel.md)) |
| `gameview`, `gameclient` | the view's drawing and the client that puts the game together ([gameview and gameclient](gameview-and-gameclient.md)) |
| `decore` | the server's shared rules, vendored byte-identical ([de-core](decore.md)) |

Membership of the four extracted libraries is a file each under `tests/arch/`:
`packetwire_files.txt`, `gamemodel_files.txt`, `gameview_files.txt` and `gameclient_files.txt`,
read by CMake, by the include checker (`tests/arch/check_includes.pl`, ctest `arch_includes`) and
by the ratchet script. The executable's own source is `Client/SDLMain.cpp` alone: `main` and
`WinMain` install `ClientHost`'s `SystemHost()` and call `ClientMain`. Ratchet R1 holds the
count of translation units compiled directly into `DarkEden` at **1**
([ratchets](../testing/ratchets.md)).

**New library targets use explicit source lists, never `file(GLOB)`.** The existing globs do not
use `CONFIGURE_DEPENDS`, so a moved file silently stays in its old target until someone
reconfigures; an explicit list makes membership reviewable and is what the ratchet script parses.
After moving files, reconfigure both Debug trees.

For a change, first identify the target that owns the implementation. Library code can be linked
by unit tests; executable-only logic needs an existing exemption in the
[restructuring plan](../reviews/RESTRUCTURING.md#exemption-list-tangled-outside-the-unit-test-libraries)
or an extraction into a testable library. A header include or a zero-valued ratchet alone does not
prove that a component links independently: a [link proof](../testing/link-proofs.md) does.
