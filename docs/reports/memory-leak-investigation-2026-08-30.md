---
type: Report
title: "In-game memory leak investigation"
description: "The ~200 MB/min in-game leak root-caused to _CrtSetDbgFlag's delay-free flag running in every Debug build, fixed and verified in-game."
tags: [report, build]
---

# In-game memory leak -- investigation handoff

**Date:** 2026-08-30
**Branch:** `fix/cp949-text-conversion`
**Status:** **RESOLVED — root cause found, fixed, and verified in-game.**

**Verification (2026-08-30, final run):** with the fix, the 64..127B bucket
held at n≈374,100 / 27.6 MB across every 5 s window over 819 frames, and
`privMB` settled flat at 196.1 (the early climb was load-in transients — new
sprites and 16..32KB texture blocks as the scene populated; the final windows
were byte-for-byte identical). Startup footprint also dropped ~45 MB, since
boot churn no longer leaves corpses. All diagnostic instrumentation has been
reverted; the tree now carries only the `Client.cpp` fix, the
`VS_UI_GameCommon` chat-help fix, the CLAUDE.md correction, and this document.

One minor observation from the verification run, not pursued: the mouse
pointer's blit path re-creates RLE scanline buffers in small recurring bursts
(`spritectl_sprite_set_scanline_rle` via `C_VS_UI_MOUSE_POINTER::Show`,
n=16..144 per window). Counters stay bounded, so it is lazy cursor-frame
loading rather than a leak — at most a micro-performance note.

## Resolution (read this first)

The leak was never an unpaired allocation. Upstream startup code at
`Client.cpp:3426-3438`, guarded `#ifdef _DEBUG`, sets
**`_CRTDBG_DELAY_FREE_MEM_DF`** on the CRT debug heap. That flag makes every
`free()` in the process *retain* the block — dead-filled with `0xDD`, marked
`_FREE_BLOCK`, still busy on the Windows heap — instead of releasing it. The
in-game frame loop churns ~2,000 small alloc/free cycles per frame (mostly
`std::_Container_proxy` at 16 bytes plus short strings), so the heap grew by
~190 KB of retained corpses per frame, ~200 MB/min. The menu is "flat" only
because it barely churns.

The block was believed dead because of the repo doctrine that `_DEBUG` is
never defined. **That doctrine is wrong on Windows:** `CMakeLists.txt:16` only
refrains from *adding* `_DEBUG`; MSVC defines it automatically under `/MDd`,
so every `#ifdef _DEBUG` block in the codebase is live in Debug builds. (This
also means the `_DEBUG`-guarded bounds checks CLAUDE.md calls inert are in
fact live — CLAUDE.md needs the correction.)

**Fix applied:** the flag-setting block in `Client.cpp` is removed (a comment
explains why). The same latent pattern exists in `VS_UI/WinMain.cpp:3194-3197`
— not linked into DarkEden.exe, left for a follow-up.

**Evidence chain, in order:** `[LEAK][SIZE]` pinned the growth to 64..127B
blocks, ~1,950/frame, with `newLive` frozen (allocs balanced at our level).
`[LEAK][WHO]` (`_CrtSetAllocHook`) saw near-zero CRT malloc traffic, ruling
out every allocation path — the growth had to come from frees not freeing.
`[LEAK][BLK]` hexdumps showed each block is a debug-CRT header
(`_data_size=0x10`, rising `_request_number`, `fdfdfdfd` no-man's-land) with
`_block_use=0` and `0xDD` dead-fill: freed and retained. `dumpbin` then showed
DarkEden.exe imports `_CrtSetDbgFlag` — an import the retail no-op macro
cannot produce — from `Client.obj`, proving `Client.cpp` compiles with
`_DEBUG` and the "dead" block runs.

**Verification pending:** one in-game run with the fixed build; `[LEAK][SIZE]`
64..127B and `privMB` should hold flat. Then revert the instrumentation (see
*Reverting*) and commit the one-line-of-substance fix plus the chat-help fix.

---

**Earlier status:** Pool localised, call site not yet found. One contradiction outstanding.
**Tree state:** Instrumented. Nothing committed. See *Reverting* at the end.
**Update (2026-08-30, later):** The `[LEAK][SIZE]` run happened and **settled
the contradiction — reading 1 was correct.** Over frames 9→1872 the 64..127B
bucket grew 34.4 MB (n=464,032) → 283.2 MB (n=4,116,778) while every other
bucket stayed flat (128..255B crept +4 MB, n +73k — small secondary) and
`newLive` sat frozen at 486,389. So:

- The probe is right, the profiler's instance column is **cumulative**;
  `_Container_proxy` churns, it does not leak.
- The leak is **~1,950 blocks per frame, raw size 64..127B** (payload roughly
  12..75B under the ucrtbased debug-heap header), allocated via the malloc
  family, **never via `operator new`**, never freed.
- ~1,950/frame is far above the exception storm's rate and right at the scale
  of per-tile/per-blit work, but the sprite backend's mallocs are all
  creation-time (`sprites` flat at 340 all run). The caller is not findable by
  reading; it is now measured directly — see `[LEAK][WHO]` below.

A `[LEAK][WHO]` probe was added and run: `_CrtSetAllocHook` on ucrtbased.dll
counts every small (<=200B) malloc/calloc/realloc process-wide by caller
address. (The retail `<crtdbg.h>` hides the function behind a no-op macro, but
ucrtbased exports it for real — the earlier note that the hook is unavailable
was wrong.) **Result: the hook sees almost nothing** — whole 5 s windows report
`allocs=0` while the 64..127B bucket grows ~190,000 blocks in the same window.
In steady state the only CRT-malloc traffic is our own `operator new`
(excluded, proven balanced) plus stray singles. Therefore:

- **The leaker bypasses the CRT entirely — it is raw `HeapAlloc` /
  `RtlAllocateHeap` on `h0`.** The 64..127B raw size is the true payload, no
  debug header involved.
- Every deployed DLL, including the two suspicious-looking unsuffixed ones
  (`iconv-2.dll`, `jpeg62.dll`), links ucrtbased (checked with
  `dumpbin /dependents`), so no module's plain `malloc` can be hiding from the
  hook. iconv is also exonerated behaviourally: no per-frame `iconv_open`
  traffic showed up (the standing-still UI text is ASCII, so
  `TextService::NormalizeText` takes the `IsValidUtf8` early-out).
- No code in this repo calls `HeapAlloc`/`GlobalAlloc`/`LocalAlloc`/
  `VirtualAlloc` at all (grepped). The caller is a system or runtime DLL doing
  per-frame work on our behalf. At ~24,000 allocations/second the
  `NonBlockingIOException` storm ("thousands per second") is the right order
  of magnitude; the socket layer (`ws2_32`/`mswsock`) polling every frame is
  the other candidate.

A `[LEAK][BLK]` probe has been added and built: each 5 s dump reservoir-samples
6 busy 64..127B blocks during the heap walk, hexdumps their first 64 bytes
(hex + ASCII), and symbolizes any qword that points into a loaded module —
a vtable or code pointer names the owning type outright. By late run ~90% of
the class is leaked blocks, so the samples are almost surely leaks. Next
action: one more in-game run; read the `[LEAK][BLK]` lines.

---

## Symptom

An in-game session grows private bytes without bound. Reported at roughly
**217 MB/min**, reaching ~1 GiB after four minutes. Measured rate is
**~194 KB per frame**, and it scales with *frames*, not wall-clock, so it lives
in the per-frame update/draw path.

The leak only reproduces **logged in, standing still in a city**. The main menu
is flat. Neither the investigating agent nor any test binary can reach this code
path -- game logic compiles straight into `DarkEden.exe` and cannot be linked
into `unit_tests` (see `CLAUDE.md`, *Testability*). **Every measurement below
came from the repo owner running the client from the Visual Studio debugger and
pasting the Output window.** Any next step that needs numbers needs them too.

---

## Where the memory actually is

Instrumentation splits process memory into pools and dumps all of them every
5 s. The decisive run:

| frame | privMB | **h0 (process heap)** | newMB | sdlAlloc | textures | sprites |
|---|---|---|---|---|---|---|
| 88  | 167.1 | **138.9** | 44.7 | 264 | 1 | 382 |
| 574 | 261.7 | **233.3** | 45.0 | 268 | 1 | 441 |
| Δ | **+94.6 MB** | **+94.4 MB** | **+0.3 MB** | +4 | 0 | +59 |

**The process default heap accounts for 99.8% of the growth.** Everything else
is flat.

Combined with the pool counters, the allocation is:

- **on the Win32 process heap** (`h0`, the only heap over 1 MB in the process),
- **not `operator new`** -- `newMB` wraps the global `new`/`delete` pair and
  stayed at 45.0 MB while 94 MB appeared underneath it,
- therefore **reached via `malloc`/`calloc`/`realloc` or a direct `HeapAlloc`**.

`malloc` is the one allocator the `operator new` replacement cannot intercept.
On Windows the UCRT is shared (`ucrtbased.dll`), so every module -- the exe and
every DLL -- allocates on that same `h0`. **A `malloc` from inside SDL2d.dll,
SDL2_ttfd.dll, freetyped.dll, libpng16d.dll, zd.dll, bz2d.dll, jpeg62.dll or
iconv-2.dll lands in `h0` and is invisible to every counter in the tree.**

---

## Ruled out, with evidence

Do not re-investigate these without new evidence. Each was excluded by
measurement, not by reading.

| Hypothesis | Evidence against |
|---|---|
| Sprite / glyph cache | `sprites` plateaus (473→498) while +576 MB accrues. Glyph cache is keyed and bounded. |
| Backend surfaces | `surfaces` pinned at 5 for every run. |
| SDL textures | `textures` pinned at 1. Persistent streaming texture is reused, not recreated. |
| Texture-convert fallback path | `fallback=0` -- `SpriteLibBackendSDL.cpp:1421` never executes. |
| **D3D driver / GPU** | Run with `SDL_RENDER_DRIVER=software`; probe confirmed `renderer=software`; leak continued at the same rate. |
| SDL's own allocator | `sdlAlloc` (`SDL_GetNumAllocations()`) flat at 264-268. |
| Game containers | `MZone` / `MTopView` map and list sizes flat across every run. |
| C++ heap (`operator new`) | `newMB` flat to 0.1 MB over 729 frames; `newLive` +13. |
| `CRarFile` failure path | Returns before allocating. Was a per-frame open storm -- fixed, see below. |
| iconv / CP949 transcoding | All three sites pair `iconv_open`/`iconv_close` on every path; the two in `MString.cpp` are `PLATFORM_MACOS`-only, dead on Windows. |
| Our own `malloc` sites | All audited. The three sprite-adapter temp buffers (`CSpriteSurface_Adapter.cpp:191,281,348`) free on every path and are lazy-creation gated. Sound buffers are load-time. |
| `LeakMemoryDumper` | Gated behind `ENABLE_LEAK_TRACKER`, provably off -- it defines a replaceable global `operator delete` that would have collided with the probe's as `LNK2005`. The link is clean. |
| `NonBlockingIOException` storm | Thousands/sec, but each carries ~2 short strings that are freed on catch, and their allocations go through `operator new`, which is flat. **Still a real performance bug** -- see *Side findings*. |
| zlib in the packet path | (2026-08-30) No `inflate`/`uncompress`/MZLib caller anywhere under `Client/Packet/`. The vendored zlib in `Client/*.c` is reached only from file-load code. |
| `C_SPRITE_FRAME_ARRAY::AddSpriteFrame` grow-by-one `realloc` | (2026-08-30) `VS_UI_util.cpp:1244` fits the fingerprint perfectly (CRT `realloc` into `h0`, invisible to the `new` counter) but its only caller is commented out (`VS_UI_util.cpp:69`). Dead code. |
| Sprite backend create/destroy imbalance | (2026-08-30) Sprite pixel data is `malloc`/`calloc` (invisible to `newMB`), so a churn leak would evade the counters while `sprites` stayed flat -- but `spritectl_destroy_sprite` (`SpriteLibBackendSDL.cpp:326`) frees every buffer `create`/decode allocates: `pixels`, `rgba_pixels`, per-row `scanline_rle`, `scanline_lens`. Paired. |
| Zone sound update | (2026-08-30) `MZoneSoundManager::UpdateSound` is gated to once per 5 s while standing still, and buffers are cached per node. Wrong shape for a per-frame leak. |
| Glyph cache unbounded growth via color churn | (2026-08-30) `TextBackendSDL` keys glyphs on (font, codepoint, color) with no eviction, so per-frame color-cycling text *would* grow it -- but every glyph is a `spritectl` sprite, and `sprites` plateaus. Bounded in practice. |

---

## The open contradiction -- resolve this first

Visual Studio's Memory Usage tool, native heap profiling on, reports for a
single snapshot:

```
DarkEden.exe!std::_Container_proxy    4,219,384    67,510,144
```

That is 4.2M objects of 16 bytes. It cannot be reconciled with the probe, which
reports **494,137 live allocations in total across all types** -- and the
profiler's own allocation call stack shows `_Container_proxy` being allocated
through `DarkEden.exe!operator new() - Line 40`, i.e. through the counter.

Two readings, and they lead to opposite next steps:

1. **The profiler column is cumulative** (every allocation since process start,
   not live instances). Supporting evidence: the same snapshot lists
   `std::_Tree_node<std::pair<unsigned __int64 const, MCreature*>>` at
   **106,867** instances, but the probe reports `creature=2` in both `MZone`
   and `MTopView`, so about **4** are live. A live snapshot cannot show 106,867.
   Under this reading `_Container_proxy` is per-frame string churn, not a leak,
   and the leak is a DLL-internal `malloc`.

2. **The probe is wrong and `_Container_proxy` really is leaking.** This would
   require `operator new` to be missing ~88% of allocations while remaining
   perfectly balanced, which is hard to construct -- escaped allocations would
   make `newLive` drift, and it does not.

Reading 1 is strongly favoured, but it has **not** been settled by measurement,
and the investigating agent was already wrong once on this data (it dismissed
the young-age instances on the first screenshot before the owner scrolled and
disproved it). **Settle it before trusting either tool further.**

### The adjudicator is already written and unbuilt

`[LEAK][SIZE]` in `CGameUpdate.cpp:6257` walks the real heap and buckets **live**
blocks by power-of-two size. It has never been run -- the link failed twice with
`LNK1168` because the game held the exe.

- `16..31B` shows **~67 MB, n≈4,200,000** → the profiler is right, the probe is
  broken, `_Container_proxy` is the leak.
- `16..31B` shows **~8 MB, n≈500,000** → the probe is right, the profiler list
  is cumulative, and the growing bucket names the leaked block's size class.

Either way the growing bucket gives the allocation size, which usually
fingerprints the caller even for a DLL-internal `malloc`.

**Build it first:**

```bash
cmake --build build/vs2022 --config Debug --target DarkEden -- -m
```

The game must be closed or the link fails with `LNK1168`.

---

## Suggested next steps, in order

1. **Land the `[LEAK][SIZE]` build and get one in-game run.** Settles the
   contradiction and yields the leaked block size. *(Build landed 2026-08-30
   14:55 -- only the run remains.)*
2. **Hook `malloc`, not `new`.** This is the real gap. `_CrtSetAllocHook` is
   unavailable (`_DEBUG` is deliberately never defined -- `CMakeLists.txt:16`).
   Options: link a `malloc`/`free` interposer, or use **UMDH** or **Application
   Verifier**, which trace heap allocations by call stack process-wide and
   include DLL-internal ones. UMDH with two snapshots is probably the shortest
   path to a stack. Runbook below.
3. **Bisect by subsystem** if instrumentation stalls: the leak is per-frame and
   in-game only. Text rendering is the prime remaining suspect -- it is the main
   per-frame consumer of SDL_ttf/FreeType, both of which `malloc` into `h0`
   invisibly. Confirm by measuring with all in-game text drawing disabled.
4. Note `sdlAlloc` counts SDL **allocations**, not bytes. A single SDL buffer
   repeatedly `realloc`'d larger would hold the count flat while bytes grew.
   Considered unlikely (SDL frees its render queue per present) but not
   excluded by measurement. **The `[LEAK][SIZE]` run adjudicates this too**: a
   single regrown buffer shows up as one entry in a very large power-of-two
   bucket (`n=1`), and heap *fragmentation* from regrowing shows as committed
   (`[LEAK][HEAP]`) climbing while the busy-byte bucket total stays flat.

### UMDH runbook (step 2, when a call stack is needed)

UMDH ships with **Debugging Tools for Windows**, which is **not installed** on
this machine (checked `C:\Program Files (x86)\Windows Kits\10\Debuggers\` on
2026-08-30). Install it by re-running the Windows 10 SDK installer and ticking
only *Debugging Tools for Windows*.

1. Enable stack capture for the exe (**admin prompt**, once; slows the game a
   little):

   ```
   "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe" /i DarkEden.exe +ust
   ```

2. Point UMDH at symbols (the PDB dir plus the MS server), in the prompt that
   will run umdh:

   ```
   set _NT_SYMBOL_PATH=C:\Users\donjulio\Documents\git\client\build\vs2022\bin\Debug;srv*C:\symbols*https://msdl.microsoft.com/download/symbols
   ```

3. Start the game from the VS debugger as usual, log in, stand in the city
   until the leak is clearly running (a minute is plenty). Find the PID in
   Task Manager or the VS title bar, then take two snapshots ~60 s apart:

   ```
   umdh -p:<PID> -f:C:\temp\snap1.txt
   umdh -p:<PID> -f:C:\temp\snap2.txt
   umdh C:\temp\snap1.txt C:\temp\snap2.txt -f:C:\temp\leakdiff.txt
   ```

4. `leakdiff.txt` lists growth by allocation call stack, biggest first. The
   top entry is the leak, including DLL-internal `malloc`s -- this is the
   view every in-process counter is blind to.

5. Afterwards, turn stack capture back off:

   ```
   gflags.exe /i DarkEden.exe -ust
   ```

---

## The instrumentation in the tree

All of it is marked `TEMPORARY DIAGNOSTIC - revert before committing`.

### `Client/CGameUpdate.cpp`

| Where | What |
|---|---|
| `:16-22` | `psapi.h`, `SDL2/SDL.h`, `malloc.h` includes + `psapi.lib` pragma |
| `:34-58` | Global `operator new`/`delete` replacement. `g_leak_new_live` (block count), `g_leak_new_bytes` (via `_msize`, no header so alignment is unchanged). Interlocked. |
| `:6139` | Dump block, every 5 s, keyed off `g_CurrentTime` |
| `:6160` | `[LEAK]` -- frame, private MB, working-set MB, sprites, surfaces |
| `:6184` | `[LEAK][POOL]` -- newLive, newMB, sdlAlloc, textures, fallback, renderer |
| `:6201` | `[LEAK][HEAP]` -- `GetProcessHeaps` + `HeapWalk`, committed MB per heap ≥1 MB plus total |
| `:6257` | `[LEAK][SIZE]` -- live blocks bucketed by power-of-two size. Run 2026-08-30; named the 64..127B class. |
| after `operator delete[]` | `[LEAK][WHO]` -- `_CrtSetAllocHook` on ucrtbased, counts small allocations per caller PC (first non-CRT return address), 16k-slot lock-free table, symbolized via dbghelp at dump time. Skips our own `operator new` (proven balanced) via a thread-local flag. |
| in the 5 s dump block | `[LEAK][WHO]` install-on-first-dump + top-8 report per window. |

### `Client/SpriteLib/SpriteLibBackendSDL.cpp`

`g_sc_live_sprites`, `g_sc_live_surfaces`, `g_sc_live_textures`,
`g_sc_fallback_hits`, `g_sc_last_renderer` (`:151-161`), incremented and
decremented on the success paths at `:194, 211, 217, 322, 357, 603, 1363, 1390,
1398, 1421`.

### Container-size dumps

`MTopView::DumpContainerSizes()` (`MTopView.cpp:20316`, decl `MTopView.h:896`)
and `MZone::DumpContainerSizes()` (`MZone.cpp:6093`, decl `MZone.h:441`).

### `Client/GameInit.cpp:1461`

`log_set_file_output("darkeden_debug.log")`.

> **Output sink matters.** The probe uses `OutputDebugStringA`, which lands in
> the Visual Studio **Output** window. The project's `DEBUG_ADD_FORMAT` /
> `log_set_file_output` write to the **log file** instead. An early version of
> this probe used the latter and appeared to produce nothing. If a new counter
> seems silent, check which sink it is using before assuming it did not fire.

To read the Output window, right-click in it and untick **Exception Messages**
and **Module Load Messages**, or the exception storm buries everything.

---

## Side findings

**Keep this one.** `VS_UI/src/VS_UI_GameCommon.cpp:6029` --
`C_VS_UI_CHATTING::Process()` guarded its help-text load on
`m_v_help_check.empty()`. Half the shipped data is still packed, so
`commoningame.txt` / `<race>ingame.txt` never open, the vector never fills, and
the guard never went false -- reopening the archive **every frame, forever**.
Fixed with a `m_bl_help_load_tried` member (`VS_UI_GameCommon.h:675`, set at
`VS_UI_GameCommon.cpp:6162`). This is a
genuine fix, not a diagnostic; it is the one change in the tree worth keeping.
It was not the leak -- `CRarFile::Open` returns before allocating on failure.

**Worth a separate issue.** The `NonBlockingIOException` storm. `__END_CATCH`
(`Client/Packet/Exception.h:117`) catches `Throwable&`, appends
`__FILE__:__LINE__` to a list, and rethrows, so every non-blocking socket poll
throws and rethrows C++ exceptions -- thousands per second. `NDEBUG` is not
defined in Debug builds, so this is live. It is not the leak, but it is likely a
large part of why the client runs at ~16 fps in-game.

**Also worth an issue.** Heavy per-frame `std::string` construction, visible as
the `_Container_proxy` churn above. Same performance bucket.

---

## Corrections to earlier notes

- `Client/SDLMain.cpp` is **not built on Windows** -- `CMakeLists.txt` filters
  it, and `WinMain` in `Client/Client.cpp` is the entry point. Presenting goes
  through `CSDLGraphics::Flip()` in `Client/CSDLGraphicsFlip.cpp`. Earlier
  analysis quoted `SDLMain.cpp`'s main loop, which never compiles into the
  binary. The present sequence is equivalent, but do not read that file.
- "Main menu is flat" was used at one point to argue the leak is not in the
  present path. That is weak -- the menu also has no server connection and runs
  almost no game logic. The `renderer=software` run is the sound version of that
  experiment.

---

## Reverting

Everything except the `VS_UI_GameCommon` chat-help fix is diagnostic scaffolding
and must come out before any commit:

```bash
git checkout -- Client/CGameUpdate.cpp Client/GameInit.cpp \
  Client/MTopView.cpp Client/MTopView.h Client/MZone.cpp Client/MZone.h \
  Client/SpriteLib/SpriteLibBackendSDL.cpp
```

Keep `VS_UI/src/VS_UI_GameCommon.cpp` and
`VS_UI/src/header/VS_UI_GameCommon.h`.

> Most sources here are **pure CRLF**. `sed -i` and `awk` silently rewrite the
> whole file to LF and turn a five-line change into a whole-file diff. Check
> with `grep -c $'\r' <file>` against `wc -l`, and use an editor that preserves
> bytes.
