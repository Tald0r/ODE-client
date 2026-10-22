---
type: Reference
title: "Client traps"
description: "The build and runtime facts that have cost time to rediscover: _DEBUG is live in every Debug build, an unlisted DLL exits before main, 5:5:5 sprites are latent, sprite rejection is silent, and grep stops at the NUL bytes in VS_UI_GameCommon.cpp."
tags: [architecture, build]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Client traps

**`_DEBUG` IS defined in MSVC Debug builds.** The top-level `CMakeLists.txt` only declines to
*add* it; MSVC defines it automatically under `/MDd`, which CMake cannot undo. The ~50
`#ifdef _DEBUG` blocks in shipped code are therefore **live in every Debug build** and dead only
in Release. Both directions have bitten: the `_DEBUG`-guarded bounds checks (e.g.
`CTypeTable::operator[]`) do run in Debug but vanish in Release, and upstream's
`_CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF)` in `Client.cpp`, long believed dead, ran on every
Debug launch and made the CRT retain every freed block, which presented as a ~200 MB/min in-game
"memory leak" (root-caused and removed 2026-08-30,
[the investigation](../reports/memory-leak-investigation-2026-08-30.md)). Judge every `_DEBUG`
guard by the build configuration, not by the old doctrine that it never fires.

**An unlisted DLL kills the client before `main` does anything.** `Client/ClientMain.cpp` walks
`*.dll` in the working directory against a hardcoded whitelist and does a bare `return -1` on the
first name it does not recognise: no message, no log line, and well before `InitGame()` sets up
logging. Adding any library that deploys a new DLL beside the executable therefore breaks startup
until that DLL is added to the list. A silent `-1` exit with no log written is this check until
proven otherwise, and the DLL stays in the output directory after you revert the build change that
brought it in, so reverting alone does not restore a working tree.

**Don't launch `DarkEden.exe` yourself** unless asked
([running the client](../operations/running-the-client.md)).

**Packed resource reading is restored.** `CRarFile` (`VS_UI/RarFile.h`) first reads a loose
override beside its `.rpk`, then falls back to the password-protected archive. The pinned static
UnRAR dependency reads into bounded memory and never extracts to disk; `GetList` returns owned
regular-member names from that archive. Keep its notices beside distributed binaries
(`third_party/unrar/README.md`).

**The 5:5:5 sprite paths are latent.** `ColorDraw::Is565()` returns a hardcoded `true`, so the
`CSprite555` family is never constructed and fixes there have no runtime effect today.

**Sprite rejection is silent.** A rejected sprite is dropped with no log line and an ignored
return value, so bad art would vanish without a signal.

**`grep` stops reading `VS_UI/src/VS_UI_GameCommon.cpp` at its first NUL byte.** The file
carries three NUL bytes, so ripgrep and `grep -I` class it as binary and either skip it or stop at
the first one; a tree-wide count that does not pass `grep -a` (or read the file in binary mode, as
`tests/tools/count_identifier.pl` does) undercounts by whatever that file holds: 296 of the tree's
~364 `wsprintf` calls, for one. Two counts in the port assessment were wrong by a factor of two
for this reason (found 2026-09-08). Force text mode for any measurement over `VS_UI/`
([measure the value](../process/measure-the-value.md)).
