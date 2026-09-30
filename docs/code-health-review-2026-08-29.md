# OpenDarkEden Client -- Code Health Review

**Date:** 2026-08-29  
**Method:** 8 parallel Claude Opus reviewers (xhigh reasoning), one per subsystem, reading the actual source. Each reported evidenced defects with `file:line` citations, an area health summary, and a letter grade.  
**Scope:** Full client tree (`Client/`, `VS_UI/`, `basic/`, build system). Static review only -- no dynamic analysis or fuzzing was run.

> ⚠ **Note:** Findings were produced by automated reviewers. `file:line` references were verified against the source at review time, but each fix should be confirmed against current code before acting. Ranking reflects reviewer-assigned severity.

---

## Executive Summary

A subsystem-by-subsystem review surfaced **197 findings**. Every area graded **D**. The dominant theme is **memory safety at the network trust boundary**: attacker-controlled bytes from the server reach array indices, function-pointer tables, and fixed-size buffers with little or no validation across nearly every subsystem. Compounding this, range checks in core lookup tables are gated behind `_DEBUG`, which the documented CMake build never defines -- so the shipping and `make debug-asan` builds run *unchecked*.

### Findings by severity

| Severity | Count |
|---|---|
| 🔴 Critical | 28 |
| 🟠 High | 60 |
| 🟡 Medium | 81 |
| ⚪ Low | 28 |
| **Total** | **197** |

### Findings by category

| Category | Count |
|---|---|
| memory-safety | 77 |
| correctness | 44 |
| build | 16 |
| maintainability | 14 |
| portability | 11 |
| undefined-behavior | 9 |
| security | 9 |
| dead-code | 7 |
| resource-leak | 3 |
| resource-management | 1 |
| resource-exhaustion | 1 |
| integer-overflow | 1 |
| file-parsing | 1 |
| protocol-design | 1 |
| performance | 1 |
| undefined-behaviour | 1 |

### Area grades

| Area | Grade | Findings | Health summary (first sentence) |
|---|---|---|---|
| Input, Audio & Media | D | 22 | This area is the least-finished part of the SDL migration and is effectively non-functional today: the SDL_mixer backend in DXLibBackendSDL.cpp is gated behind `#ifdef SDL_MIXER_MAJOR_VERSION`, a macro that only SDL_mixer.h itself defines, so every sound/music entry point compiles to the "not available" stub; the three `*_Adapter.cpp` files that would replace the stubs cannot compile at all (constructors still named after the pre-rename classes, duplicate global definitions); and even if both were fixed, nothing on Windows ever calls SDL_Init(SDL_INIT_AUDIO), so Mix_OpenAudio would fail. |
| Rendering & Sprites | D | 25 | This area is the weakest link in the client's memory safety. |
| Networking & Protocol | D | 27 | This is the client's primary attack surface and it is in poor shape. |
| Core Game Loop & State | D | 24 | This area is the client's trust boundary with the server, and it is largely unguarded. |
| Text & Strings | D | 23 | This area is the weakest part of the client I looked at. |
| UI Framework | D | 25 | This is the least safe area of the client I looked at. |
| Foundation Libraries | D | 27 | This layer is the weakest part of the tree because everything above it depends on it. |
| Build, Portability & Hygiene | D | 24 | The build works on exactly one path — Windows + VS2022 + vcpkg — and only because the case-insensitive filesystem and MSVC's tolerance paper over a lot. |

---

## Remediation Status

**2026-09-23 reconciliation complete:** all 197 finding headings have current
dispositions, including duplicate findings, removals and corrected original
claims. The [reconciliation](code-health-reconciliation-2026-09-23.md) maps
their regression owners and records the cross-cutting follow-ups. Historical
totals, grades and intermediate open notes below describe earlier trees; the
latest closure notes take precedence. Live-server verification is optional.

**Updated 2026-09-10, later:** the Medium *Networking & Protocol* finding below — wire bytes copied raw into `bool` members — is fixed on `feature/cpp20-macaddress-span`, centrally in the two stream reads, taking the total to 88 fixed and Medium to 19 fixed / 62 open. Its red run showed a bool holding 0x02 answering `if (a)` and `a == true` differently under MSVC; no consumer in the tree compares a wire bool with `== true`, so in play this was a sanitizer trap and an invalid value propagating into UI structs, not an inverted branch.

**Updated 2026-09-10:** the Medium *Networking & Protocol* finding below — `Datagram`'s bounds checks wrapping in unsigned arithmetic, with the write bound an `Assert` that Release compiles away — is fixed on `feature/cpp20-framing-header`, taking the total to 87 fixed and Medium to 18 fixed / 63 open. Its test is a reproduction rather than a guard: on the unfixed code the wrapping read did not fail the test, it crashed the test process. The same branch found and fixed a defect this review had not seen, recorded under *Found by reading*: the datagram sent one byte of uninitialised heap memory behind every UDP packet, in the pad slot both peers count and neither reads.

**Updated 2026-09-06:** the Medium *Networking & Protocol* finding below — `flush()` discarding the unsent remainder of a partial send — is fixed on `fix/flush-partial-send`, taking the total to 86 fixed and Medium to 17 fixed / 64 open. It is the same defect the adversarial review of the header-rollback fix had reported the same day as a new one, so it is recorded twice, under *Found by reading* as well as in its own entry; both now point at the same commit. It is also the first *Networking & Protocol* fix that a test binary could reach at the socket, which took one keyword: `SocketImpl::send` is virtual so a test can script a partial send, since a real one needs a congested peer.

**Updated 2026-09-04, later:** two Medium findings in *Rendering & Sprites* — `BltAlphaSpritePal` refusing partially visible sprites, and the SDL effect function tables never populated — are fixed in `6fd3f34`, taking the total to 85 fixed and Medium to 16 fixed / 65 open. They turned out to be two of the three defects behind a runtime symptom rather than cosmetic ones: every screen-blend skill effect drew nothing. That symptom and the third defect are recorded under *Runtime defects* and *Found by reading* below.

**Updated 2026-09-04:** C19 — data-file strings used as printf formats — is fixed by `docs/RESTRUCTURING.md` task 5.4's fifth slice, taking the total to 83 fixed and **Critical to 28 fixed / 0 open**. Read its entry before relying on that: the same finding was marked fixed earlier the same day on one ratchet reading zero, and had to be retracted within the hour.

**Updated 2026-09-03:** the Medium dead-code finding below — roughly 400KB of stub and duplicate source excluded from the build — is fixed by `docs/RESTRUCTURING.md` task 5.2's first and fourth slices, taking the total to 82 fixed and Medium to 14 fixed / 67 open.

**Updated 2026-09-02:** the silent `list(REMOVE_ITEM)` High finding below is fixed by `docs/RESTRUCTURING.md` task 4.0 (`restructuring/vsui-single-compile`), and the `_LIB` ODR High finding that followed it by the same task's second slice (`restructuring/vsui-lib-define-public`), taking the total to 81 fixed and High to 38 fixed / 19 open.

**Updated 2026-09-01 (High-severity pass).** 79 of the 197 findings have been fixed: 11 on branch `harden/library-code-fixes` ([PR #1](https://github.com/bound2/client/pull/1)), 2 on `harden/packet-index-bounds` ([PR #4](https://github.com/bound2/client/pull/4)), 11 on `harden/network-input`, 4 by the earlier SDL_mixer wiring commit `c0670ae` (recorded retroactively during the audio pass), 17 on `harden/audio-media`, 8 on `harden/text-format`, 2 on `harden/pointer-truncation`, and 24 on `harden/high-severity-batch1`. Fixed findings carry a ✅ marker in the sections below, naming the commit and the tests covering them; three carry ⚠️ instead, meaning the finding was resolved without a code change — already fixed elsewhere, made unreachable by another fix, or only partly closed with the remainder stated.

By severity: **Critical 28 fixed / 0 open** (C19 closed on 2026-09-04 by task 5.4's fifth slice. It was marked closed once earlier the same day on the strength of ratchet R7 reaching 0, and reopened within the hour when that slice's review found ~25 live sites R7 cannot see. The closure now rests on five separate measurements, listed in its entry, of which R7 is only one — a ratchet at zero is a claim about the ratchet), **High 38 fixed, 3 noted, 19 open** (36/21 until the two build findings closed on 2026-09-02), **Medium 14 fixed / 67 open** (this line said 13/68 until 2026-09-04, contradicting the 2026-09-03 update three paragraphs above, which is the number the running total was built from), **Low 3 fixed / 25 open**.

**Every finding the review rated Critical is now closed** (C19 last, on 2026-09-04, and read its entry for what the closure rests on — it was also *wrongly* closed earlier that day). That does not mean the client is free of critical defects: the pointer-truncation pass found a whole family of the same class that the review never recorded, described under *Open, not in this review* below.

The test-driven work is confined to code compiled into a static library, since that is the only code a test binary can link against. A second phase has since fixed nine defects in the `DarkEden` executable itself, listed under Runtime defects below; those were found by running the client rather than by this review, and none of them are among the 197 findings.

A third phase (2026-08-31, branch `harden/network-input`) took on the network attack surface this review rated most serious: the shop/stash index bounds (C7, C8, done earlier in `ed4f872`), the chat/guild/system-message string bounds (C9, C10/C16, C11/C17), the peer file-transfer filename (C12), the NewItem function-pointer table (C13), the 21-byte chat rows (C14, C15/C18) and the tooltip use-after-free (C25). None of these are reachable from a test binary, so they are build-verified regression guards. The branch was put through the same eight-angle adversarial review as phase one, which found ten real defects in the fixes themselves (including a filename guard that would have broken every legitimate profile transfer, and a missed overflow sixty lines from a fixed one) — all repaired in `3bc340e`. After merging master's wire max-size reconcile, a second review (two Opus xhigh reviewers, `1200625`) found more: write-side guards that tested the BYTE-narrowed length, a guild-name cap of 20 that would disconnect on legitimate 30-byte names, a filename validator that confined escape but not scope (a peer could still drop a DLL beside the executable), and config clamp floors of 1 that hung or corrupted the chat rows. The remaining Gpackets parsers beyond these findings are still unaudited, and the data-file format-string sites (C19/C20/C22) are untouched.

A fourth phase (2026-08-31, branch `harden/audio-media`) closed out the Input, Audio & Media area's sound findings. Four of them — the duplicate-buffer double free (the area's first critical), the uncompilable adapters, the self-defeating SDL_mixer include guard, and the recycled-channel confusion — turned out to have been fixed already by `c0670ae`, the commit that made sound play at all; they are now recorded as such. The branch itself fixed the rest: the PlaySound stack overflows and the format-after-Release logging, the zone-sound NULL dereference, the opening-screen modal error box, the WavePackFileManager mmio parsing, the latent CMP3/MMusic MCI defects, the CPartManager bounds/sentinel/rollover defects and the non-virtual Release() leak (test-first — the template is testable, see `tests/unit/test_part_manager.cpp`), and the compat MMCKINFO shadow. It also deleted the five stale root-level DXLib header copies (two of which were live ODR violations against the compiled dxlib layouts of g_SDLMusic and g_SDLInput), deleted both copies of the orphaned MP3/Huffman decoder — resolving the two huffman bounds findings by removal — and *believed* it had renamed COGGSTREAM.CPP so case-sensitive configures work. It had not: the rename was verified missing at HEAD during the sixth phase and is only now done. See that finding's entry. Of the area's 22 findings, only the two input Lows (the SDL input constructor and the mouse-wheel accumulator — input findings, not sound) remain open.

A fifth phase (2026-09-01, branch `harden/text-format`) took on the data-file format strings the third phase had left untouched, plus two unrelated criticals in the same sweep. C27 (MemoryPool releasing `::operator new` chunks with `free`) and C28 (the APICheck probe that could `ExitProcess` at random from the frame loop) are fixed by pairing the allocator and by deleting the probe outright. C20/C26 (the unbounded `vsprintf` into a shared static in `CMessageArray`) is fixed together with its two siblings, both of which are separate High findings in their own right: `MString::Format`, which had the same shape one layer down, and `SendBugReport`, whose four call sites passed a caught exception's text — which can embed packet-derived bytes — as the *format* argument. C22 (the guild-quest mission title, server text used as a format) and C24 (the fixed-offset NUL write in the tooltip renderer) are fixed at their call sites.

C19 is **narrowed, not closed**, and the distinction is the point of the phase. The load-time sanitizer added for it (`SanitizeGameStringTable`) cannot check that an entry's specifier count matches its call site, because arity is unknowable without the call site — so the actual fix for the exploit the finding describes is the conversion of all 100 zero-argument `wsprintf(buf, tableEntry)` sites to bounded `snprintf(buf, sizeof(buf), "%s", GetGameString(id))`, where the data is an argument rather than a format. The ~140 sites that do pass arguments still take their format from `String.inf` and are covered only by the gate. Two further limits are recorded in the code itself rather than left for a reader to discover: the gate does not run in the default English build at all, because `InitGameStringTable` reallocates the table from source literals immediately afterwards and discards the scrubbed file data (it protects `LANGUAGE != 3`), and its width cap bounds argument-driven expansion, not the literal text of an entry.

That branch was reviewed by two Opus reviewers who were told the authoring model's own review was not to be trusted, and they earned it. One compiled the sanitizer into a standalone harness and measured it against the real MSVC runtime rather than reasoning about it, which is how the first revision's policy came apart: `"%512d"` passed the check and emitted 512 bytes, five `"%500d"` in one entry passed and emitted 2500 because the width test never composed, and `"%f"` needs no width at all and emitted 308 bytes — all into destinations of 50 to 128 bytes, which is precisely what the check's own comment claimed to prevent. The cap became 32 with a whole-entry budget, floating-point and wide (`%S`, `%ls`) conversions are now rejected, and the comparison was off by one (`>` accepted exactly 512). The same pass found that the `CMessageArray` fix carried a comment asserting a guarantee it did not deliver, that the C24 rewrite had introduced a one-line tooltip overdraw by making the renderer disagree with the sizing pass beside it, and that the two `vsprintf` siblings existed at all. All repaired before the branch was committed.

A sixth phase (2026-09-01, branch `harden/high-severity-batch1`) took the High tier, the first pass since the Critical list was exhausted. Four areas were worked in parallel on disjoint file sets: the networking handlers (the NULL datagram socket, the dangling `c_str()` family, the `GCShopListMysterious` index, the `GCWhisper`/`RCSay` off-by-one, and the dead wire encryption, deleted outright so the absence of transport security is honest); the core game loop (`MItem::GetName`'s heap overflow, three uninitialised locals, the two creature-delete paths that freed an object a sector still pointed at, and the unvalidated `m_ppSector` subscripts, now routed through a bounds-checked `SectorAt` accessor); text and strings (the `WideCharToMultiByte` shim, `CToken`'s double free, `ReduceString2`, a twelve-site `snprintf` sweep, and the UTF-8 edit-buffer decoder); and the foundation libraries (`platform_event_wait`'s unreachable branch, the two one-byte overflows in `platform_get_executable_dir`, and `CMessageArray::Release`). `CTypeTable`'s bounds check — filed twice, once correctly and once as a CMake finding — is now unconditional in Release as well.

**The most useful output of this phase was not the fixes but the corrections.** Ten findings were wrong or stale in a way that mattered, and one previously recorded as fixed had not been. `GCShopListMysterious`'s accessors were already guarded three lines above the lines the review cites; `ConvertEncoding` had been fixed in `43458c1`; the `RequestFileManager` index -1 write was made unreachable by the C12 rework; `AddCreature` is reachable with one FAKE move type, not four, and not by the path named; `ReduceString2`'s stated failure scenario matches no caller in the tree while a genuine out-of-bounds *read* beside it went unmentioned; `platform_get_executable_dir`'s "live" callers are both dead; all three `CMessageArray` re-init sites are dead; and the `_DEBUG` premise behind the `CTypeTable` build finding is simply false on MSVC. Two review recommendations were refused with cause: allocating `m_Filename` only after a successful open would convert a leak into a NULL dereference at an unguarded `strcpy`, and hoisting `AddEffect`'s boundary test would skip rejections the chase-effect path depends on. Fixing a finding here means checking it first; roughly a third did not survive contact with the current code.

Two things this phase makes reachable and one it could not close. Adding a reject path to `MZone::AddCreature` turned a previously unreachable NULL dereference in `PacketFunction.cpp` into a live one — fixed in the same change, and the same un-masking pattern the `SendMessage` family produced, so it should now be the default expectation whenever a function gains a rejection. Making `CTypeTable`'s check unconditional moves Release from reading past the array to a deterministic NULL dereference at call sites that copy from the result without testing; that is the right trade and it matches the Debug build that is actually run, but ~600 call sites were not audited and it is the change most likely to surface in play. And the include-case finding is **swept, not closed**: it was written as "five `#include` directives" and measured at **412 occurrences across 174 files**, of which 408 plus 36 backslash separators are now rewritten by script under a `lc(old) == lc(new)` invariant — but no compiler on this machine can verify any of it, because Windows resolves either spelling. Linux still needs the CI job the finding asks for.

**The sixth phase's own adversarial review was the most productive one yet, and most of what it found were defects in the fixes rather than in the original code.** Four reviewers ran on the committed branch: two on the code, one on the 191-file mechanical commit, and one whose only job was to check the commit messages and this document against the tree — that last one added specifically because of the `COGGSTREAM` precedent, where a fix recorded as landed had not. All four earned their place.

The most important finding was that the phase's own call-site audit was wrong. It certified "all 15 live `AddCreature` sites are correct" while teaching the lesson that a new reject path un-masks latent bugs. There are 24 live sites and **three** mishandled the rejection — including the byte-identical twin of the very bug the entry celebrated fixing, 109 lines above it in the same file, and one in `GCAddWolfHandler` that dereferences the nulled pointer 55 lines later from *outside* both arms of the if/else. The audit had scanned a fixed window after each guard, which finds the easy case and misses exactly the two that matter. Review also found the same one-byte chat overflow in a third handler the sweep never looked at, two more dangling `c_str()` bindings of an accessor nobody had thought to search for, and a nickname index that this phase's own `CTypeTable` change had just turned from an out-of-bounds read into a NULL dereference. All are fixed.

Two of the phase's fixes were themselves wrong. The mechanical sweep **broke** a working include in `tools/engine/sprite/`, because its invariant forbids redirecting an include but not rewriting a correct one into a path that resolves nowhere — the root list never modelled that sub-project. And the `CMakeLists.txt` entry `Client/MNpcTable.cpp`, a wrong-case duplicate of the line below it, survived inside the commit that claimed no CMake change was needed. Both fixed; the second is a reminder that "no change needed" is a claim like any other and wants checking.

Three claims were retracted outright, which is the part of this worth carrying forward. The warning that the deleted encryption code "was also wrong" about a ring-buffer offset was **false** — an `Assert( m_Tail == 0 )` three lines above proves the two expressions identical, and the claim was repeated from a reviewer without reading the surrounding lines. The CRLF verification offered as evidence could not have detected the damage it ruled out, because `core.autocrlf=true` normalises both sides before the diff. And the unaudited exposure of the `CTypeTable` change was stated as "roughly 600 call sites" when `(*g_pGameStringTable)[` alone is **4,085** — a figure borrowed from an unrelated finding, understating by seven times the risk of the change this document itself calls the one most likely to surface in play. Several counts were off (13 renames for 14, 46 duplicated translation units for 43, twelve `snprintf` conversions for thirteen) and are corrected in place.

The audio branch was put through the same adversarial review as the earlier phases (four Opus reviewers, one per commit group). It confirmed the buffer fixes, the mmio semantics (mmioRead does not clamp to the descended chunk, so canonical 16-byte PCM fmt chunks still load), the template guards, the CKINFO rename in every real inclusion order, and the SDL refcount pairing against the installed SDL2_mixer 2.8.2 sources — and found four real problems that were repaired before the branch was finalized: making Release() virtual silently broke the four dead-but-compiled sprite cache managers (reworked as the per-slot hook), renaming only COGGSTREAM.CPP would have broken case-sensitive builds at the header include (both files renamed), the sound-init error path leaked its own subsystem refcount on audio-less machines, and MIX_INIT_MP3 would have logged a spurious warning on every init since the vcpkg mixer has no MP3 decoder. The review left three recorded latents: the `strrchr(Filename.GetString(), '\\')` trio in Client/MPlayer.cpp (6526/6884/7481), a NULL-dereference the day force feedback (`CImm::m_pDevice`) is ever revived after a WAV load failure; the rejected-SetData return being indistinguishable from a no-eviction store (see the CPartManager entry); and `dxlib_sound_release` tearing the mixer down while the music slot's bookkeeping lives elsewhere — currently unreachable in the real shutdown order.

A seventh phase (2026-09-03, branch `harden/checked-format`) returned to C19, the one Critical still open, and replaced its estimate with a measurement: **293** call sites hand a `String.inf` entry to `printf` as its format, counted by ratchet R7 rather than guessed. The fix for the argument-passing half is `basic/SafeFormat.h`, which checks the entry's conversions against the arguments the call site actually passed — the check the load-time gate cannot make, because arity is unknowable without the call site. 31 sites are converted, all of `Client/PacketHandler`. It is also the first fix for this finding that a test binary can reach: `basic` is a library, so the formatter has 39 unit tests where every earlier C19 change was a build-verified regression guard. Two measurement lessons came out of it and are recorded in `ratchets.sh`: `grep` treats `VS_UI`'s CP949 sources as binary and truncates them (113 reported against a true 192 without `-a`), and four call sites put the destination and the format on different lines, so a line-based count would have scored converting them as a no-op.

### Fixed

| Finding | Severity | Commits |
|---|---|---|
| `CSpritePalBase::LoadFromFile` scanline pointer table (C3) | 🔴 Critical | `b7c4eb3` |
| `CAlphaSpritePal::Blt` RLE walk and palette indexing (C4) | 🔴 Critical | `764e9ab`, `c444a83`, `a17aa39` |
| `CSprite555::LoadFromFile` RLE decode (C5) | 🔴 Critical | `cfc6cd6`, `964c043` |
| `CFilter::LoadFromFile` file-driven free (C6) | 🔴 Critical | `90d6794` |
| `TArray::LoadFromFile` unvalidated element count | 🟠 High | `e8f86be` |
| `CTypePack::Get` / `CTypePack2::Get` range check | 🟠 High | `355ef3f` |
| `TArray` copy constructor and self-assignment | 🟡 Medium | `a2b3c8a`, `1a3b32d` |
| `TArray::operator+=` size truncation | 🟡 Medium | `4f8435a` |
| `MPalette` copy constructor and self-assignment | 🟡 Medium | `d78dc21` |
| `CTypePack` iterators that never advance | 🟡 Medium | `6f457e0` |
| `ColorDraw::Convert565to555` discards blue | ⚪ Low | `65a2413` |
| `GCShopList::read` unvalidated rack index (C7) | 🔴 Critical | `ed4f872` |
| `GCStashList::read` unvalidated rack/index pair (C8) | 🔴 Critical | `ed4f872` |
| `GCPartySay` unbounded name/message + handler strcpy (C9) | 🔴 Critical | `a6cc969`, `3bc340e` |
| `GCGuildChat` unbounded guild name + handler sprintf (C10/C16) | 🔴 Critical | `a6cc969` |
| `GCSystemMessage` dead length guard + static char[128] (C11/C17) | 🔴 Critical | `a6cc969`, `3bc340e` |
| Peer-supplied filename used for local file create/rename/delete (C12) | 🔴 Critical | `76a1185`, `3bc340e` |
| `MItem::NewItem` unvalidated function-pointer table index (C13) | 🔴 Critical | `7b81ba4`, `3bc340e` |
| 21-byte chat rows: "Dear."/"From." copies and newline wrap (C14, C15/C18) | 🔴 Critical | `c3e9937`, `3bc340e` |
| Tooltip descriptor use-after-free on inventory delete (C25) | 🔴 Critical | `f0b8ae6` |
| `dxlib_sound_duplicate` shared Mix_Chunk double free (C1) | 🔴 Critical | `c0670ae` |
| SDL audio adapters uncompilable, duplicate globals | 🟠 High | `c0670ae` |
| SDL_mixer include guard compiled the backend to a stub | 🟠 High | `c0670ae` |
| Recycled mixer channel acted on the wrong sound | 🟡 Medium | `c0670ae` |
| `PlaySound` unbounded strcpy of Sound.inf filenames | 🔴 Critical | `4ff97c4` |
| `PlaySound` formats the Filename it just released | 🟡 Medium | `4ff97c4` |
| `MZoneSoundManager::UpdateSound` NULL dereference | 🟠 High | `4ff97c4` |
| Opening-screen modal MessageBox for the stubbed MPG | 🟠 High | `4ff97c4` |
| `WavePackFileInfo::SaveToFileData` unchecked mmio parsing | 🟠 High | `4ff97c4` |
| `CMP3` pointer-sized error buffer, uninitialized Play result | 🟡 Medium | `4ff97c4` |
| `MMusic::Play` unbounded sprintf | 🟡 Medium | `4ff97c4` |
| `CPartManager` unbounded index accessors, sentinel collision | 🟡 Medium | `6c2e37e` |
| `CSoundPartManager::Release` hidden by non-virtual base | 🟡 Medium | `6c2e37e` |
| LRU counter-rollover normalization | ⚪ Low | `6c2e37e` |
| Compat MMCKINFO shadowing the real struct layout | 🟡 Medium | `d021544` |
| Duplicate CDirectMusic.h / CDirectInput.h ODR violations | 🟠 High | `66d8637` |
| Orphaned MP3/Huffman decoder, duplicated and unbuilt | 🟡 Medium | `66d8637` |
| `huffman_decoder` unbounded tree walk (both copies' findings) | 🟡 Medium, ⚪ Low | `66d8637` |
| `COGGSTREAM.CPP` case mismatch on case-sensitive configure | 🟠 High | `66d8637` (partial -- the rename did not land, see its entry), completed on `harden/high-severity-batch1` |
| SDL audio subsystem and Mix_Init never called explicitly | 🟠 High | `452e854` |
| `CMessageArray::AddFormat`/`AddFormatVL` unbounded vsprintf into a shared static (C20/C26) | 🔴 Critical | `0e9d247` |
| `MString::Format` unbounded vsprintf into a process-wide static | 🟠 High | `0e9d247` |
| `SendBugReport` unbounded vsprintf, and four call sites passing exception text as the format | 🟠 High | `0e9d247` |
| `MemoryPool` chunks allocated with `::operator new`, released with `free` (C27) | 🔴 Critical | `8b349e7` |
| APICheck probe truncating FARPROCs and calling ExitProcess at random (C28) | 🔴 Critical | `c2f65b7` |
| Guild-quest mission title used as a printf format string (C22) | 🔴 Critical | `31f5f2f` |
| `_Multiline_Info_Show` fixed-offset NUL write past the caller's buffer (C24) | 🔴 Critical | `31f5f2f` |
| Data-file strings as printf formats: 315 call sites checked at the point of use (C19) | 🔴 Critical | `31f5f2f`, then `docs/RESTRUCTURING.md` task 5.4 |
| Gear tooltip's second item passed through a 32-bit `long` and dereferenced (C21) | 🔴 Critical | `c0df91c` |
| SDL text pointer passed through a 32-bit `long` (C23, dead path, removed) | 🔴 Critical | `2a531a9` |
| `BltAlphaSpritePal` refusing partially visible sprites | 🟡 Medium | `6fd3f34` |
| SDL effect function tables never populated (the `EFFECT_SCREEN` entry; the rest deliberately left, see its entry) | 🟡 Medium | `6fd3f34` |
| `flush()` discarding the unsent remainder of a partial send | 🟡 Medium | `fix/flush-partial-send` |
| `Datagram` bounds checks that wrap, and a write bound that was an `Assert` | 🟡 Medium | `fix:` commit on `feature/cpp20-framing-header` |
| Wire bytes copied raw into `bool` members | 🟡 Medium | `fix:` commit on `feature/cpp20-macaddress-span` |

### Fixed, not in this review: the `SendMessage` pointer-truncation family

Found while fixing C21, and **not among the 197 findings** — the review caught the
`DescriptorManager` instance of this defect but missed the same pattern in a
second, larger mechanism.

`C_VS_UI_BASE::SendMessage(DWORD message, int left = 0, int right = 0, void* void_ptr = NULL)`
(`VS_UI/src/header/VS_UI_Base.h:319`) takes its two payload slots as `int`. Nine
call sites cast a pointer into one of them, and the receiving handlers cast it
back:

- `VS_UI/src/VS_UI_Game.cpp:525` sends an `MItem*`; `UIMessageManager::Execute_UI_RUN_NAMING_CHANGE`
  (`Client/UIMessageManager.cpp:11036`) recovers it with `MItem* pItem = (MItem*)left;`.
  A second `MItem*` recovery sits at `Client/UIMessageManager.cpp:9784`.
- Seven more in `VS_UI/src/vs_ui_gamecommon2.cpp` (1562, 1570, 9910, 12107, 12512,
  13024, 13825) and two in `VS_UI/src/VS_UI_ExtraDialog.cpp` (1280, 2020) push
  `MItem*` or `std::string::c_str()` results through `int`.

On MSVC x64 `int` is 32 bits, so every one of these truncates a heap pointer and
sign-extends it back. This is the same defect as C21 with the same consequence,
and at least the item-rename path is reachable in ordinary play. The `(int)(intptr_t)`
casts present at these sites silence the truncation warning without preserving the
value — the same false-reassurance C21 called out. `git blame` puts them at
`a6e21d2`, the SDL port: the original `(int)ptr` stopped compiling on x64 and was
made to compile again rather than made correct, so every one of these features has
been crash-on-use since.

> ✅ **Fixed** in `12bde38` (branch `harden/sendmessage-truncation`). The payload
> path is `intptr_t` end to end — `MESSAGE`, `SendMessage`/`_SendMessage`, the three
> function-pointer typedefs, `UI_ResultReceiver`, `UIMessageManager::Execute`,
> `UI_MESSAGE_FUNCTION` and every handler signature — plus `TempInformation::Value1..4`,
> which is the parking slot two packet handlers cast back to `char*`. Unlike C23's
> virtual `long extra`, this bus dispatches through a function-pointer table, so a
> handler left at the old width is a compile error rather than a silent non-override;
> that is what makes the widening verifiable, and it caught two handlers written with
> a signature variant the first pass missed.

**The dangerous part was making these paths reachable.** Three memory-safety bugs sat
behind them that the truncation had been masking, all found by adversarial review and
fixed in the same commit — without them, this "fix" would have replaced a guaranteed
crash with silent corruption:

- `C_VS_UI_SMS_MESSAGE::AddSendList` copied a **server-supplied** phone number into
  `char[16]` with `wsprintf`, which bounds output at 1024 bytes rather than at the
  destination. `GCSMSAddressList` reads that field with a BYTE length prefix, so a
  hostile or buggy server had ~240 bytes of stack past the buffer — the review's own
  top open risk, unvalidated network input, in a newly-reachable path.
- `Execute_UI_CHANGE_CUSTOM_NAMING` `strcpy`'d the nickname into `char[22]`, where the
  editor's `SetByteLimit(22)` counts *characters* and the DBCS conversion emits up to
  two bytes each — 22 Korean characters are 44.
- The SMS and nickname flows parked `c_str()` pointers in `TempInformation` across the
  server round trip, with nothing keeping the owning dialog alive; closing it, or the
  ESC path through `ClosePopupWindow`, deletes it. `TempInformation` owns `std::string`
  copies now.

The 2026-09-18 queued-text follow-up closes the borrowed-string lifetime gap:
text messages own their payload through callback completion, including reentrant
dispatch. General raw payloads retain their caller-owned lifetime contract. The
tests and source audit are recorded under *Found by reading* below.

### Also fixed, not in this review

Found while fixing the above:

- **`CFilter::IsInit` was inverted** (`5939d9e`) — it and `IsNotInit` were both `m_ppFilter == NULL`. Nothing calls it today, but `CTypePack::Get` drives lazy loading off exactly this predicate.
- **`CSpritePalBase.h` was not self-contained** (`b7debd8`) — it named the stream types without including `<fstream>`, compiling only because every translation unit reached it through the PCH.
- **The sibling 555 loaders** `CAlphaSprite555` and `CIndexSprite555` (`964c043`) carried the same unbounded RLE decode as `CSprite555`.
- **The clipping blit variants.** More than twenty routines across `CAlphaSpritePal` and `CSpritePal` walk the same run length data as `Blt` with the same absence of a bound. Rather than patching each one's bespoke clipping arithmetic, `a17aa39` validates the shape of every scanline once at load time, which covers all of them.

### Test infrastructure

This repository previously had no way to run a unit test: `enable_testing()` was never called, the `Makefile`'s `test` target was a stub, and `BUILD_TESTS` was referenced in the configuration summary without being defined. Commits `60d5d8e` and `f36b8bb` add a minimal C++11 self-registering framework, wire it into CTest, implement `make test`, and add `make test-asan`. 62 tests now run; each fix above was written test-first.

### Adversarial review of this work

The remediation was itself put through an adversarial review (single Opus reviewer, xhigh effort), which returned a verdict of **significant problems** and found three real defects that the fixes had introduced or missed. All three are repaired in `6ee3e76`:

- **`CTypePack2::ReleasePart(int,int)` was never fixed.** `6f457e0` clamped the `CTypePack` overload and left the `CTypePack2` one untouched — the one the sprite packs actually instantiate and `MTopView` calls. The Remediation Status entry claiming otherwise was false and has been corrected.
- **The 555 loaders desynchronised the stream.** Their rejection paths returned part way through a sprite, but sprites are stored back to back and the pack loaders ignore the return value and rely on the position having advanced. One malformed sprite turned every later sprite in the pack into garbage. `CSprite555` now reads the whole sprite before validating any of it.
- **`CSprite555` latched itself permanently.** It sets `m_bLoading` on entry and `CSprite::Release()` never clears it, so an early return left the object unable to load anything again — escalating, through `CTypePack2`'s lazy-load retry, to disabling lazy loading for the whole pack.

The reviewer independently verified the bound arithmetic in `CSprite555`, `CAlphaSprite555`, `CIndexSprite555` and `CAlphaSpritePal::Blt` as correct and not over-strict, confirmed `CAlphaSpritePal::SetPixel` really does emit two bytes per pixel, and could not construct any input to either in-tree encoder that the new scanline validation rejects.

### Runtime defects

Ten defects in the `DarkEden` executable, found by running the client against a live server rather than by this review. None are among the 197 findings, and none were reachable from a test binary. Each one blocked login, ended the session, or produced visibly wrong behaviour in play.

| Defect | Symptom | Commit |
|---|---|---|
| `WSAStartup` never called | every login attempt reported itself as an immediate disconnect | `e3dd23e` |
| Counting `Lock`/`Unlock` semantics on `CSpriteSurface`, plus a `GetSurfacePointer` that took a lock nobody released | one leaked lock per frame: the UI's `assert(!IsLock())` guards fired, and SDL refused the `g_pLast` to `g_pBack` frame flip | `d0d3417` |
| `std::vector` iterator used after `erase`, never advanced | debug-build crash on shutdown, and whenever the server sent a nickname list | `b1bc394` |
| Speed-hack check counting each frame as 72ms of elapsed time | above ~23 fps the client set `MODE_QUIT` on itself about 23 seconds into the game | `50306d9` |
| `__LINE__` passed to a `%s` conversion | crash inside `vsnprintf` the first time a turn carried more packets than `MAX_PROCESS_PACKET`, which is what entering the world does | `6bd26fa` |
| `bool` read on a switch path that never assigns it | `/RTC1` failure on hovering the info panel's grade tabs | `aac4b76` |
| A percentage believed from a stale `ClientConfig.inf` record | every creature bleeding permanently at full health | `7660574` |
| Quest XML parsed without checking that the file opened | null pointer walked as a character buffer | `d31bf57` |
| Effect start position set only on the "start at user" and "start at target" branches | `/RTC1` failure on casting Meteor: `MAGIC_METEOR`, `RESULT_MAGIC_METEOR` and `SKILL_ERUPTION` are flagged sky-only in `Action.inf`, so `x`/`y` reached the effect generator uninitialised | `6b57bfa` |
| `CSpriteSurface::BltSpritePalEffect` an empty stub since the SDL port, over an effect table that was all NULL | every `BLT_SCREEN` skill effect drew nothing: the character swung and no lightning, slash or bolt appeared. Reported as "sword skill animations are broken" — Thunder Spark, Lightning Hand, Thunder Bolt, Thunder Storm and Wide Lightning among them. `Action.inf` and `EffectSpriteType.inf` put **118 of the 209 skills with effects** on this path, through 606 of the 2631 effect sprite types; the newer alpha-blend effects (Infinity Thunderbolt, for one) were the ones that showed. Confirmed fixed in play on 2026-09-04 | `6fd3f34` |
### Found by reading, not by running

✅ **Help-mail eligibility fixed** on `fix/review-help-eligibility` (2026-09-21). `C_VS_UI_MAILBOX::AddHelpMail` compared the Slayer attribute sum against `m_iAttrLow` for both bounds, admitting only the exact minimum instead of the configured inclusive range. It also indexed race arrays after a switch with no invalid-race rejection. The decision now lives in the already linkable `MHelpMessage::IsEligible`, with four regression guards for both boundaries, race-specific level/attribute selection, the existing -1 disabled-interval convention, invalid races and reversed ranges. The mailbox widens attribute addition before comparison. This was found by reading; no live UI reproduction is claimed.

✅ **Settings ID persistence fixed** on `build/review-warning-policy` (2026-09-21). A sporadic settings-test failure led to a reproducible defect: `BackupID[11]` initialized only its first byte, while save/load transferred 15 bytes directly through it, reading or overwriting padding and the next member. A stray `0x1A` could also make Windows text input stop before the remaining options. `UserOption.cpp` first moved unchanged into `VS_UI`, so its test now links the production library object. The fix initializes the entire ID, uses an explicitly padded 15-byte disk field, bounds and terminates the loaded ID, and rejects a short field. The existing file layout and text-mode convention are preserved. The new tests cover initialized storage, adjacent-member leakage, legacy padding/CRLF, unterminated IDs and truncated fields; the first two reproduced 13 failed checks before the fix. This is an ID-field fix, not a replacement for the remaining legacy settings parser.

✅ **Queued UI text ownership fixed** on `fix/review-ui-text-ownership` (2026-09-18). Chat and both file-dialog result paths now enqueue owned text instead of a literal/borrowed `c_str()` that the callback could free or outlive. File-dialog multi-selection had passed a local string destroyed before dispatch. The receiver uses value-owned messages and consumes each one before calling application code: tests reproduced replay after a throwing callback and an ASan double-free under reentrant dispatch. Six UI tests cover FIFO, exception/reentrant dispatch, literals, local/temporary/empty text, raw payload compatibility and outer text lifetime during nested dispatch. The focused Linux ASan/UBSan run passes six tests and ten checks; full Windows Debug/ASan builds and all nine CTests pass. Raw `SendMessage` payloads retain their existing caller-managed lifetime contract. R17 falls from 950 to 944.

✅ **Editor bounds and alias follow-up fixed** on `fix/review-editor-bounds` (2026-09-18). Character limits are clamped to the editor's fixed capacity; insertion rejects null, negative and oversized requests using remaining-capacity arithmetic before touching either buffer. Input is copied before shifting so insertion from the editor's own text preserves the requested slice. Cursor movement widens addition before clamping. Five new tests cover these boundaries and preserve existing behavior when reducing a limit below the current text length. Before the fix, capacity, cursor and alias assertions failed, and an `INT_MAX` insertion reproduced an ASan write access violation in `memmove`. Full Debug/ASan builds and all nine CTests pass in both trees; the focused Linux ASan/UBSan run passes 17 tests and 80 checks. The legacy `SetByteLimit` name still means characters; wire-byte limits remain a separate concern.

✅ **MString assignment follow-up fixed** on `fix/review-mstring-aliasing` (2026-09-18). Both assignment overloads copied from freed or uninitialized storage on self-assignment; the pointer overload also failed when given a suffix of its own buffer. Replacement storage is now allocated and filled before the previous value is released, with an explicit object-self-assignment guard. `Init()` rejects negative capacity before changing the value and widens the terminator addition before allocation. Six tests cover aliases, independent copies, null/empty contracts, initialization and invalid capacity; the alias test reproduced an ASan heap-use-after-free in the original assignment. Full Debug/ASan builds and all nine CTests pass; R17 falls from 952 to 950.

✅ **File-dialog follow-up fixed** on `fix/review-file-dialog-paths` (2026-09-18). `Start()` now splits filters without a fixed suffix buffer or a pre-guard `strlen`; directory state owns strings instead of `MAX_PATH` arrays, and navigation no longer copies into `char[200]`. Root-aware helpers normalize paths, build selected filenames, and keep parent traversal inside drive/native/network roots. Startup initializes the current entry even with no drive letters (the POSIX case), a missing current directory, or a UNC current directory. Refresh clears stale selections, and empty directories remain selectable. The unused, independently unsafe `GetFileNameInString` helpers were deleted. Ten tests in `test_file_dialog_paths.cpp` cover the actual extracted helpers: null filters crashed before the fix, and parent-of-drive-root navigation reproduced an ASan stack-buffer-underflow. A review test also caught and corrected extended UNC root handling. Long filters/paths, empty suffixes, dot names, and startup fallbacks are regression guards. Full Windows Debug/ASan builds and all nine CTests pass; R17 falls from 958 to 952.

Defects of the same weight as the ten above, kept out of that table because they do not meet its definition: each was found by reading during a remediation pass, and none has been observed in play.

| Defect | Symptom | Commit |
|---|---|---|
| Six rows of `C_VS_UI_ASK_DIALOG::m_sz_question_msg` never assigned, and read as format strings | any of the six friend dialogs runs `strlen()` over an indeterminate pointer and hands it to `sprintf` as a format. Upstream commented the `ASK_FRIEND_*` assignments — and their string ids — out of `InitString()` while leaving the six cases that read them live; `m_sz_question_msg` is an ordinary member array, so nothing else gives those rows a value. Reachable from a `GCFriendChatting` packet, which makes it server-triggered rather than merely latent — but a friend dialog is a corner of the UI, which is the likeliest reason nobody hit it | task 5.4's fifth slice |
| `GCNPCSayDynamic`'s message `strcpy`'d into `char[256]` | `GCNPCSayDynamic::read` accepts a message of up to **2048** bytes; the handler copied it into a 256-byte stack buffer and passed it to `MCreature::SetChatString`. 1792 bytes of stack past the end, at a server's discretion, on the ordinary NPC-dialogue path | the packet-tree copy pass |
| The four PCS handlers index `UserInformation` with an unbounded wire slot | `SlotID_t` is a `BYTE` and no `read()` bounds it, against `PCSUserName[3]` and `OtherPCSNumber[3]`. `MString::operator=` reads `m_pString` out of whatever lies at that offset and `delete[]`s it, so `GCPhoneConnected`, `GCPhoneDisconnected`, `GCPhoneSay` and `GCRing` gave a server an **arbitrary free and an arbitrary write** into the heap | the packet-index pass |
| `GCRemoveFromGear` reads `addonSlot[slotID]` past the end of a stack array | `MSlayerGear::RemoveItem` bounds `slotID` to `m_Size` — `MAX_GEAR_SLAYER` (27), or 28 for the other two races — while the `addonSlot` arrays hold 15 and 16 entries. Unequipping a ZAP, a PDA, a shoulder or a blood bible read up to **twelve ints** past the array and handed the result to `RemoveAddon`. **No hostile server needed**: this fires in ordinary play | the packet-index pass |
| The screen blend's green table 32 wide under a 6-bit green | `memcpyPalEffectScreen` indexes `s_EffectScreenTableG[d][s]` with `ColorDraw::Green`, which on this backend decodes the full 6-bit field of a 5:6:5 pixel; the table was declared `[32][32]`, so any green of 32 or more read past the row and, from the last rows, past the table. Latent only because the table was never filled and the blit that used it was a stub — the fix for those two would have made it live. The green table is 64 wide now, filled with the 6-bit form of the same formula, and `tests/unit/test_spritesurface_pal_blit.cpp` pins its entry for a green of 63 | `6fd3f34` |
| `DrawAttachEffect` screen-blends an attached effect's frame, and its pair frame, twice | both `BLT_SCREEN` sites in the attach path (`MTopView.cpp` ~17370 and ~17457) draw the sprite unconditionally and then again in an `else` branch that always runs, because `s_bUse3DLight` is a constant `false`. Screen is not idempotent, so every attached glow rendered brighter than its art, washing toward white; the two sibling sites had the second draw commented out. Unobservable while `BltSpritePalEffect` was a stub — the adversarial review of `6fd3f34` found it, before the first build with the blit had been played | `6fd3f34`'s review round |
| `PacketIDSet::deletePacketID` inverts its own membership test | the guard reads `if ( itr != m_PacketIDSet.end() ) throw NoSuchElementException();`, so an id the set really holds is *refused* and an id it does not hold falls through to `m_PacketIDSet.erase( end() )` — undefined on `std::set`, which MSVC's debug iterators turn into an abort and Release into a corrupted tree. Both halves of the function are wrong, which is why nothing masked it. **Latent: no caller.** Nothing in the tree calls it; `PacketValidator` only ever fills a set through `addPacketID` and never takes an id back out, so this is a trap for the next caller rather than a live fault. `tests/unit/test_packet_id_set.cpp` now covers both halves | fixed 2026-09-06 (`fix/packet-id-set-delete`) |
| `GCLearnSkillReady` reads `SKILLDOMAIN_NAME[domainType]` from an unbounded `BYTE` | `SkillDomainType_t` is a `BYTE`, `read()` does not bound it, and `SKILLDOMAIN_NAME` is a plain `int[MAX_SKILLDOMAIN]`. The out-of-range read was then handed to the string table as an id. (`(*g_pSkillManager)[domainType]` on the line above survives it — `MSkillManager` is a `CTypeTable`, which range-checks in every build) | the packet-index pass |
| `StringStream::operator<<(char)` injects a NUL after every streamed character | `Client/Packet/StringStream.cpp` builds a `std::string(2, '\0')` for a `char` and fills only its first byte, and the `uchar` overload does the same. `Throwable::toString()` streams a `'\n'` between the message and the stack trace, so every `SendBugReport("%s", t.toString().c_str())` (`GameMain.cpp`, `ClientCommunicationManager.cpp`) sends the server a `*bug_report` cut off before the stack trace it exists to carry; `assertion_failed.log` receives a bare newline per failed `Assert`, because the message starts with a streamed `eos`; and the `LOG_ERROR` and `strstr` consumers of `toString().c_str()` see only the first line. Found while pinning the assertion text for the second source-location slice, whose tests spelled the NUL out rather than hiding it. Both overloads now push a one-byte string; `tests/unit/test_stringstream_chars.cpp` pins the operators and the three texts built out of them, and the source-location tests lost their `NEWLINE_AND_NUL`. What the server sees is bounded by the packet now rather than short of it: `SendBugReport`'s own cut (`WireHost.cpp`) was 100 bytes, which left no room for a frame after a ~55-byte exception message, and is **116** — `CGSay::MAX_MESSAGE_SIZE` (128) less the 12 bytes of the `*bug_report ` prefix, which is the whole headroom the packet allows. `tests/unit/test_bug_report_message.cpp` sends through a capturing `Player` and pins the boundary, the cut and that no report outgrows the `CGSay` that carries it | fixed 2026-09-05 (`fix/stringstream-nul`); the cut raised 2026-09-06 (`fix/bug-report-cut`) |
| `StringStream::toString()` appends to the buffer it already holds | `toString()` skips the rebuild when nothing was inserted since the last call, but when something was it appends the whole list to the previous result instead of clearing it first, so `ss << 'a'; ss.toString(); ss << 'b'; ss.toString()` yields `"aab"`. Latent: every caller in the tree calls it once, or twice with nothing between. Found by the adversarial review of the NUL fix above | fixed 2026-09-05 (`fix/stringstream-nul`) |
| The profile-picture dialog lists at most one picture, through a shadowed loop counter | `C_VS_UI_FILE_DIALOG::RefreshFileList`'s file branch wrote `for(int i = 0; ...)`, which shadowed the `int i` the suffix filter above it had left at `m_filter.size()`; the `if(i == m_vs_file_list.size())` after the loop therefore compared the FILTER count against the list size. Entries arrive in ascending order, so the loop almost never breaks for a file, and a file was appended only when the two happened to agree. The one live caller asks for `".bmp;.jpg"` — a count of **two** — so a picture was listed only when exactly one subdirectory sorted before it, a directory typically showed one picture (sometimes a handful, where the byte-wise insert order disagreed with the case-folded listing order), and a directory with no subdirectory showed **no pictures at all**; the same stale test after a successful insert could put a file in twice, though only when the function was driven out of the dialog's own arrival order. Beside it the filter `strcpy`'d each suffix into a `char[20]` out of the `char[30]` `Start()` builds, and compared `strlen(suffix)` bytes whatever the name's length, so a name shorter than the filter indexed at a wrapped `size() - j - 1`. Found by reading on 2026-09-05 during the `FindFirstFile` migration; never observed in play. Fixed through the "moved, then fixed" path — `basic/FileDialogListing.{h,cpp}`, pinned by `tests/unit/test_file_dialog_listing.cpp` | the file-dialog listing slice |
| `SocketOutputStream::write(const Packet*)` leaves the framing header in the ring when the body write throws | the packet id, `getPacketSize()` and the sequence byte go into the output ring, and the sequence counter is bumped, before `pPacket->write(*this)` is called; nothing rolled any of it back. A body that refuses its input - `CGSkillToNamed::write()` over its 20-byte name cap (`595e994`), or the stream's own span core on an oversized span - therefore left a **header announcing a body that never followed**, so the peer would frame the NEXT packet's bytes as that body and every packet after it, and the counter had spent a sequence number on a packet that never went out. What keeps it out of play is that seventeen packet `write()` bodies throw after the header is in the ring - the chat family among them (`CGSay`, `CGWhisper`, `CGGlobalChat`, driven from the chat box), the guild packets, `CLCreatePC`, `CLQueryCharacterName` and `CGSkillToNamed` - and `CGSay` has already emitted its colour byte when it throws, so the residue there was a header and a partial body. The callers drop the connection on an `InvalidProtocolException`, which is what kept the residue out of play; a caller that logged and continued would desynchronise the session silently instead. Found by the adversarial review of the second span/typed-scalar slice on 2026-09-06, which `595e994` had already named as out of scope. `write(const Packet*)` now saves the ring's length and the sequence byte before the header and restores both on any exception - the length rather than the tail index, because a body large enough to resize the ring moves the retained bytes to offset zero. `tests/unit/test_output_stream_rollback.cpp` pins the empty ring, an earlier packet's bytes, the resize path, the sequence counter, a wrapped ring with and without a resize, and the unchanged non-throwing frame, over the plain and the encrypt stream | fixed 2026-09-06 (`fix/output-stream-header-rollback`) |
| `SocketOutputStream::flush()` discards the unsent remainder of a partial send | `flush()` catches `NonBlockingIOException` from a partial `send()`, swallows it, and then resets `m_Head` and `m_Tail` to zero unconditionally, so whatever the socket did not take is thrown away rather than kept for the next flush; the peer sees a frame cut mid-packet. Found by the adversarial review of the header-rollback fix on 2026-09-06. **The same defect is the Medium finding at *flush() discards unsent buffered bytes* below**, which is one of the 197 - the reviewer found it independently, and it is recorded in both places. Nothing had to be recovered to fix it: `send()` returns the count it accepted, short counts included, and `SocketAPI::send_ex` throws `NonBlockingIOException` only when the underlying `send()` returned `SOCKET_ERROR`, so the throwing call transferred nothing and the loops' `m_Head += nSent` already names the first unsent byte. The reset is now conditional on the ring being empty. `tests/unit/test_output_stream_flush.cpp` pins it through a scripted `SocketImpl` (its `send()` is virtual for that, and the seam is documented at the declaration) | fixed 2026-09-06 (`fix/flush-partial-send`) |
| Length-prefixed string writes cap the length after narrowing it to a `BYTE` | the shape `CGSkillToNamed` was fixed for on 2026-09-06 recurs in **27 more string fields across 23 packets** under `Client/Packet/Cpackets`, which is every one of them: `CGSay`, `CGGlobalChat`, `CGGuildChat`, `CGRangerSay`, `CGPhoneSay`, `CGWhisper` (name and message), `CGAppointSubmaster`, `CGModifyGuildMember`, `CGSelectGuildMember`, `CGRegistGuild` (name and intro), `CGJoinGuild`, `CGModifyGuildIntro`, `CGModifyGuildMemberIntro`, `CGUseMessageItemFromInventory`, `CGRequestPowerPoint`, `CGConnect`, `CGPortCheck`, `CLCreatePC`, `CLDeletePC` (name and SSN), `CLLogin` (id and password), `CLQueryCharacterName`, `CLQueryPlayerID`, `CLSelectPC`. Each did `BYTE szX = m_X.size();`, tested THAT against the cap, then wrote `m_X` whole - so 256 + 1 .. 256 + cap narrowed below the cap, passed, and went out whole behind a length byte holding the low eight bits. **The chat half needs no hostile anything.** The chat box bounds its input with `LineEditor::m_Limit`, which `C_VS_UI_GAME_COMMON` sets to 100 - and that counts CHARACTERS (the editor holds UTF-32) while the packet counts BYTES of the UTF-8 `GetString()` hands over, so 100 Korean characters are 300 bytes and 100 four-byte ones are 400. `Execute_UI_CHAT_RETURN` clamps nothing between the two. A long Korean chat line therefore sent 300 bytes behind a length byte claiming 44, and the whole range 43..85 characters (129..255 bytes) was already being refused outright. Four of the caps (`> 255` and `> 256` tested on a `BYTE`) could never fire at all, so those writers had no working cap of any kind. `CLRegisterPlayer` has the same eleven narrowings and is deliberately NOT in the list: every one of its setters truncates to the same maximum its write tests, so no over-cap string can reach it. A refusal on these paths is a process exit, not a dropped message: nothing between the chat box and the socket catches the `InvalidProtocolException`, so it reaches `main()`'s catch in `Client/SDLMain.cpp` and the client returns 1 - already true for 43 to 85 Korean characters before this fix, and now for 86 and up as well, until the text is bounded upstream (the slice that follows). Found by the adversarial review of the header-rollback fix on 2026-09-06 | fixed 2026-09-06 (`fix/string-write-caps`) |
| The `BYTE`-prefixed string writers with no cap at all narrow the same way | `CGPartySay`, `CGPartyLeave`, `CGStoreSign`, `CGModifyNickname`, `CGAddSMSAddress`, `CGTypeStringList` (its element strings and its element COUNT) and `CGSMSSend` write `BYTE size = m_X.size();` and then the whole string with no cap between them, so the length byte and the body disagree above 255 with nothing to refuse it. `CGPartySay` is the live one: it is the party-chat path of the same 100-character chat box, and its factory advertises a 128-byte maximum the packet itself does not enforce. `CGStoreSign` is next: `C_VS_UI_PERSNALSHOP_MESSAGE` gives its editor `SetByteLimit(80)`, again in characters, so 80 Korean characters are 240 bytes - and `Execute_UI_STORE_SIGN` (`Client/UIMessageManager.cpp`) `strcpy`s that into a `char str[250]` before the packet ever sees it, which four-byte characters overrun on their own. Left open by the cap pass on 2026-09-06 because closing it means INVENTING a cap for each - the fix that pass applied moves an existing cap, and there is none here to move. Found by reading during that pass. Closed the next day with a cap read off the packet itself: each writer now tests the string's own size BEFORE narrowing, then writes the length byte and a `std::span` bounded by it, and every one of them keeps the zero-length behaviour it had. The adversarial review of that fix found an **eighth** writer of the same shape, `CGRequestIP`, and it is capped in the same commit: name **10**, the `szBYTE + 10` its `getPacketMaxSize()` spells, sent from `Client/ProfileManager.cpp` with a name out of the server's own list (`WhisperManager.cpp` has the only other call and it sits behind `if( 0 && ... )`). The caps, and where each comes from: `CGPartySay` message **128**, `CGPartyLeave` target name **10**, `CGRequestIP` name **10**, `CGStoreSign` sign **80**, `CGModifyNickname` nickname **22** (`MAX_NICKNAME_SIZE`), `CGAddSMSAddress` character name **20** / custom name **40** / number **11**, `CGTypeStringList` element string **50** (`MAX_STRING_LENGTH`) and element count **20** (`MAX_STRING_NUM`), `CGSMSSend` number and caller number **11** (`MAX_NUMBER_LENGTH`), message **39** and number count **5**. All but **two** are the allowance the packet's own `getPacketMaxSize()` spells, which is what the receiving side's frame check enforces before `read()` is ever called. The **first** deviation is `CGSMSSend`'s message, capped a byte under the allowance at `read()`'s own `Assert(size < MAX_MESSAGE_LENGTH)`, because nothing legitimate sits in the gap. The **second** is `CGSMSSend`'s caller number: `getPacketMaxSize()` spends only `MAX_RECEVIER_NUM` (**5**) on that slot - `szBYTE + (szBYTE + MAX_NUMBER_LENGTH)*MAX_RECEVIER_NUM + szBYTE + MAX_RECEVIER_NUM + szBYTE + MAX_MESSAGE_LENGTH`, 108 bytes - while the field holds a phone number, so it is capped at **11** like the recipient numbers and is the one cap *looser* than the allowance. Under these caps a maximal five-recipient send is 113 bytes against that declared 108, **five** over, and the frame check refuses it whatever `write()` accepts; the per-field shortfall on the caller-number slot alone is six. That arithmetic is upstream's and correcting it would change the wire inventory. `CGSMSSend`'s number lengths and its list count were also deliberately NOT taken from its reader: a full number is one of `C_VS_UI_SMS_MESSAGE`'s three-digit prefixes plus that dialog's eight-digit editor - exactly the eleven bytes the packet sizes each recipient field for, and one more than `Assert(strSize < MAX_NUMBER_LENGTH)` accepts - and `Execute_UI_SEND_SMS_MESSAGE` (`Client/UIMessageManager.cpp`, ~10758) builds up to five recipients, so copying the reader would have refused what the client legitimately sends. `CGTypeStringList::read` also refuses a zero-length element, which `write()` still emits; that disagreement is older and wider than the length byte and is left alone. `CGPartySay` carried this cap once before: `3bc340e` added it and `1200625` removed it again, one ground being that a throw in `write()` left the framing header in the ring - which the header-rollback fix above answered on 2026-09-06. **What the caps do on refusal, and why this must land with `fix/chat-input-bounds`:** no `write()` throw is caught anywhere between the UI callback and the socket, so on the four user-typed paths an over-cap input that the server used to refuse now unwinds out of the callback to `main()`. Those four, cap against editor: nickname **22 bytes** against a 22-CHARACTER editor; shop sign **80 bytes** against an 80-character one; party chat **128 bytes** against a 100-character box; SMS message **39 bytes** against an 80-character box (`SetByteLimit` stores a character count, not a byte count, so that box admits ~160 bytes of DBCS). The UI-side clamps that keep those caps unreachable are the companion branch `fix/chat-input-bounds` - party chat and shop sign there already, nickname and SMS added to it - so this change is correct on the wire on its own and must ship with that one. `tests/unit/test_uncapped_string_writes.cpp` drives every field over cap+1, 255, 256, 257, 256+cap and 300 bytes and both counts over max+1, 256 and 300, asserting the refusal and that the ring holds exactly the fields ahead; then the cap itself accepted at exactly `getPacketSize()` bytes; then the round-trip through `read()` where `read()` takes the same length | fixed 2026-09-06 (`fix/uncapped-string-writes`) |
| `Execute_UI_CHAT_RETURN` `strcpy`s the chat line into a 128-byte heap allocation | the same 100-character / 128-byte mismatch as the row above, one layer earlier and far worse: the handler did `char* strOrg = new char[128]; strcpy( strOrg, strUI );` on the editor's UTF-8 text before any packet saw it. 43 Korean characters is already 129 bytes, 100 are 300, and whisper mode prepends `"/<id> "` to the line in `C_VS_UI_GAME_COMMON` before sending it here, so **an ordinary long chat line wrote up to ~285 bytes past a 128-byte heap block** (~285 with a whisper prefix, which is the largest; 273 for 100 four-byte characters without one, 173 for 100 Korean ones), on the plain zone-chat path, with no server involved and nothing hostile anywhere. The master-command branch re-enters the same function with whole lines of a `.txt` that `fgets()` read into `char[512]`, so that path could overrun it by 384. Everything the branch below builds from that buffer inherited the same slack - four more `char[128]` stack copies of the text, and a `str[prefix+60] = NUL` whose guard tested `strlen(str) > 60` rather than `> prefix+60` and so wrote past the text (and, once `RemoveStringSpace` had walked `str` forward, past the buffer) for any line between the two. Found by reading during the string-cap slice's own follow-up on 2026-09-06, which had recorded the character/byte mismatch without looking at what the handler did with the text. The text is cut to `CGSay::MAX_MESSAGE_SIZE` bytes on a UTF-8 boundary first, the working copy is a fixed array one byte larger than the old allocation - so no read that the old block absorbed becomes out of bounds - the four stack copies are sized from the same budget, and the `str[prefix+60]` guard now tests the index it writes. **No test path**: nothing links `Client/UIMessageManager.cpp` | fixed 2026-09-06 (`fix/chat-input-bounds`) |
| `Execute_UI_STORE_SIGN` `strcpy`s the personal shop sign into a `char[250]` | `char str[250]; memset(str,0,250); strcpy(str, (char*)void_ptr);` - an unbounded copy of a UI string into a fixed stack array, with nothing in the function establishing that it fits. **Not an overflow today, and the claim that it was is corrected in the row below**: the sign is converted by `g_Convert_DBCS_Ascii2SingleByte`, at most two bytes per character over an 80-character editor limit, so at most 160 bytes arrive. It is a bound that lives two files away in a conversion function and a widget limit, either of which can change without anyone looking here - and the codebase is moving the other way, since the chat box beside it already hands over UTF-8 (four bytes a character, which would put 320 into the 250). What is live on the same line is the packet: `CGStoreSignFactory::getPacketMaxSize()` advertises szBYTE + 80, `CGStoreSign::write()` has no cap of its own, and 41 Korean characters already make an 82-byte sign, so an ordinary shop sign is refused on arrival as "too large packet SIZE" and drops the connection. The copy is a `std::string` now and the text is cut to 80 bytes before the packet is built. The cut is a plain one - the sign's bytes are ASCII mixed with raw UTF-16 units and nothing marks which is which, so the chat path's UTF-8 boundary rule does not apply and an over-long sign can lose half its last glyph. Found by reading on 2026-09-06 by the string-cap slice's implementer, following its own list of uncapped writers into their callers. **No test path**: nothing links `Client/UIMessageManager.cpp` | fixed 2026-09-06 (`fix/chat-input-bounds`) |
| `GameMain.cpp` hands a string literal to the chat handler, which `delete[]`s it | Fixed on `fix/review-ui-text-ownership` (2026-09-18). All three queued chat sends now use `SendTextMessage`, which owns a copy until callback completion. The handler borrows its input and no longer deletes it; synchronous master-command recursion borrows its local line. Chat text also uses local strings before enqueue, so mute/early-return paths do not leak raw allocations. The game-coupled callers use the UI/executable exemption and source ownership audit; the actual receiver is covered by six UI tests, including owned literals/temporary strings, FIFO, exception and reentrant dispatch. Full Debug/ASan builds and all nine CTests pass in both trees. | fixed, regression guard; live-server verification waived |
| The chat sender reads its line after enqueue | **Original use-after-free claim withdrawn:** `Base::SendMessage` enqueues; it does not synchronously call the handler, so the post-send whisper update was not reading freed storage. The ownership follow-up above nevertheless removes the ambiguous raw allocation contract: the sender reads its local `std::string` and the queue owns a separate copy. | corrected and guarded on `fix/review-ui-text-ownership`, 2026-09-18 |
| `C_VS_UI_QUEST_STATUS::ShowDesc` and `C_VS_UI_LOTTERY_CARD::ShowDesc` copy up to 2 KB of description into `char[512]`, and three siblings | each loop iteration copied 2047 bytes of the text, or all of the remainder, into a 512-byte stack buffer before cutting one line out of it, so any quest or lottery description over 511 bytes smashed the stack. Latent when found because `Data/ui/txt` was still packed and descriptions were absent; packed reading is restored in the 2026-09-22 follow-up below. Found by Apple Clang's `-Warray-bounds` in the macOS port, 2026-09-08 - MSVC, GCC 13 and Clang 18 never flagged it. The adversarial review of that fix found the same loop three more times in the file, where the length is not a constant and no compiler sees it: `C_VS_UI_BRING_FEE::Show` and `C_VS_UI_INPUT_NAME::Show` `strcpy` a string-table entry into `char[512]`, and the 2048-byte loop below them `strcpy`'d a remainder of exactly 2048 bytes one byte past its end; and that in all of them `sz_string2[cut_pos] = NULL` could still land past the buffer when the window was all spaces. All six windows are bounded by their buffer and the cut is clamped to what the window holds | fixed on `port/macos` |
| `MShopShelf::NewShelf` and the three `RemoveItem(GEAR_*)` bounds checks ran after an undefined enum conversion | the shop-list and remove-from-gear handlers cast the wire byte to the enum and then called the function that range-checks it; converting an out-of-range value to an enum with no fixed underlying type is undefined before the check can run, and Clang's `-fsanitize=enum` (on in Apple Clang's and Linux Clang's `-fsanitize=undefined`, not GCC's) aborts on the load. The four functions take `int` now and the handlers pass the raw byte. Found by the `macos-asan` preset on its first run, 2026-09-08 | fixed on `port/macos` |
| `C_VS_UI_BRING_FEE::Show` evaluates `money_buf[100]` as a statement | a leftover declaration turned into an expression statement that reads one past a `char[100]`; harmless in practice, undefined by the letter. Same finder, same commit | fixed on `port/macos` |
| Item-title palette lookup reads gradation 31 from a 30-entry row | Fixed as a regression guard on `fix/review-item-visual-guards` (2026-09-18). The legacy `[base+13][31]` address is expressed as the valid `[base+14][1]` cell for the current table, preserving the previously read color without a cross-row subscript. Invalid configuration values are rejected before addition/indexing and use the normal yellow title color. This preserves the current palette choice; it does not claim to recover a different original-client artistic intent. | fixed; full Debug/ASan builds and all nine CTests, UI exemption; live-server verification waived |
| `Datagram::write(const DatagramPacket*)` sends one byte of heap memory behind every UDP packet | the datagram is sized at `szPacketHeader + body` - the id, the size, the body and the one-byte slot the stream\x27s sequence byte occupies - and the code writes every byte but that one, from a buffer a bare `new char[]` returned. Both peers count the byte in the length and neither reads it, but `DatagramSocket::send` puts the whole buffer on the wire, so every `CGPortCheck` to the login server and every `RC*` packet to a peer carried whatever the heap held there: `0xCD` under the debug heap, a byte of freed client memory in Release. The server\x27s `Datagram` zero-fills its buffer and documents the pad as travelling as zero; the client now matches it. Found while comparing the two `Datagram::write`s for the framing-header slice; reproduced in the test binary, where the pad read back as `0xCD` on the unfixed code | `fix/` commit on `feature/cpp20-framing-header`, 2026-09-10 |
| Shrine-position loading reads unchecked counts and partial rows; minimaps index rectangles directly from zone ids | `RegenTowerInfo::num` was uninitialized; unchecked `sscanf` calls could leave header/row locals unset or partly replace a record. The table now loads in `gamemodel` behind a line reader, checks complete bounded integers, requires unique complete rows, and publishes atomically. Invalid reloads preserve positions and owners; an explicit zero count clears the table. Counts/ids fit the one-byte request field, zones are 71..73, and coordinates fit the 128 by 256 minimaps. Both rendering and hit testing reject invalid records before selecting their rectangle. `test_regen_tower.cpp` reproduces default/partial-load failures and guards malformed text, overflow, budgets, domain edges and reloads; UI integration is a regression guard | fixed on `fix/review-shrine-input`, 2026-09-22 |
| Sprite-pack loaders accept incomplete indexes and silently lose later sprites after repeated rejected lazy reads | Both `CTypePack` templates now validate complete matching counts and every offset before replacing state. Unique attempt tracking keeps a rejected row from exhausting the lazy-load counter; another indexed row can still load after stream failure. Eager/indexed reads propagate decoder and stream failure and log rejection context. Five test-first cases reproduced 25 failed checks, including a real sprite loader returning success after a failed read; reload, eviction and stream-recovery guards cover both templates. | fixed on `fix/review-sprite-pack-input`, 2026-09-22 |
| Direct pixel-sprite loading accepts incomplete headers, unchecked rows and leaves long-row pixels unread | `CSprite565` could report success after a failed read and skipped row bodies longer than 8192 words; `CSprite555` checked encoded bounds but not decoded width. Both now share checked, staged row loading and the existing scanline validator. Installed `Effect.spk` rows with a one-column width discrepancy retain their pixels by expanding reported geometry by one; larger overruns and width overflow are rejected. Complete malformed records retain the next packed-record cursor, failures clear state, maximum representable rows load, and 555 conversion leaves padding unchanged. Three test-first cases reproduced 58 failures and a compatibility case exposed four more; every truncated body, exception/reset, packed cursor, maximum-row and conversion guards cover the real classes. | fixed on `fix/review-pixel-sprite-loading`, 2026-09-22 |
| Direct shadow-sprite loading accepts incomplete reads and unchecked row counts; releasing an empty sprite leaves stale state | `CShadowSprite` now stages complete rows, checks the encoded pair count and cumulative pixel width, and publishes only validated data. Complete malformed records consume every row before rejection; truncation or exceptions leave an empty object. Release resets state even without allocated pixels. Three test-first cases reproduced 44 failed checks; three further guards cover every truncated prefix, reloads, packed cursors, exceptions, padding, maximum rows and actual drawing. | fixed on `fix/review-shadow-sprite-input`, 2026-09-22 |
| Shadow-sprite assignment reads an uninitialized destination pointer, self-assignment clears the source, and implicit copying aliases owned rows/cache | Explicit deep copying validates retained row spans, preserves padding and initialized empty records, handles self-assignment, and creates independent backend caches. Loaded and all three generated forms retain allocation bounds. Two test-first cases reproduced six failed checks; regression guards cover ordinary copying, source destruction, cached sprites and mutated row counts. | fixed on `fix/review-shadow-sprite-ownership`, 2026-09-22 |
| The SDL shadow adapter trusts mutable row counts and narrows its raster stride | ASan reproduced a heap-buffer overflow when a changed row count reached `CShadowSprite::Blt` through the real surface adapter. The adapter now checks retained allocation spans and decoded width before allocation/drawing, rejects empty or unrepresentable geometry, and decodes with a `size_t` row stride. Tests cover first creation, dirty-cache replacement, empty/oversized geometry and valid 40,000-pixel rows. The existing black raster representation is preserved. | fixed on `fix/review-shadow-sprite-adapter`, 2026-09-22 |
| Effect-shadow initialization depends on an unsupported index even though its records are readable | The installed `Effect.sspki` has an unrecognized wrapper and the raw index reader rejects it. `MTopView::InitSprites` now uses the existing checked eager pack path for this small resource, as the effect viewer already does, and propagates failure. An offline ASan run through the actual pack loader accepts all three installed records. The unit guard covers eager loading with a rejected companion index; the game-global call is a source/build regression guard. | fixed on `fix/review-shadow-sprite-adapter`, 2026-09-23 |
| The character list's slayer outlook has four weapon bits for 19 weapons | `PCSlayerInfo`, the slayer record of `LCPCList`, packs the weapon into bits 11-14 of its outlook DWORD, with the shield at 15-16, while `WeaponType` runs to `WEAPON_MACE1` (18). `getWeaponType` masked four bits and `setWeaponType` wrote the value unmasked, so a mace's fifth bit fell into the shield field and read back as a sword; the server's live encoder clamped instead (a cross1 sent as a cross, a mace and mace1 as no weapon). The character-select slot therefore previewed a mace slayer's to-hit as bare hands or a sword's, and drew no mace. Found by reading on 2026-09-28 in the to-hit preview slice (`ad559efb`, "Found, not changed"). Fixed on the wire in both repos with an extension code at bits 17-18, past the shield, which a client that knows only four bits drops (server PR #285, `c3b563a8`); the outlook's other setters now cut their value to the field, which matters on the client, whose `HelmetType` and `ShieldType` run past their two-bit fields (`SHIELD4` would have landed on the code). `tests/unit/test_slayer_outlook.cpp` ports the server's outlook tests, `test_packet_goldens.cpp` pins the server's two `LCPCList` goldens, and `test_status_manager.cpp` the mace's preview. Left open: `GameUI.cpp`'s slot indexes its three-entry helmet, shield, face and hair tables with getters that return 0..3 and its four-entry coat and trouser tables with 0..7. The live server sends at most 2 and 3 there (its `Shape.cpp`), so only a server outside its own encoder reaches past them | fixed 2026-09-29 (`fix/slayer-weapon-bits`, `2e0f39c3`) |
| `Execute_UI_MY_STORE_INFO` sends a `GCMyStoreInfo` whose store is `NULL` | the handler (`Client/UIMessageManager.cpp`) default-constructs the packet, whose `m_pInfo` starts `NULL`, calls `getStoreInfo()` and discards the result, then `sendPacket`s it; `getPacketSize()` and `write()` both dereference `m_pInfo`. **Latent: nothing posts `UI_MY_STORE_INFO`** - the message is registered and has no sender anywhere in `Client/` or `VS_UI/`. Found by reading on 2026-09-29 while checking the store-count fix below for other users of `StoreInfo` | open |
| `PCVampireInfo` and `PCOustersInfo` cast their sex byte to `Sex` unchecked | `Sex` has two values (`FEMALE`, `MALE`), and both readers of the character list store a wire byte in it with no range check, so a byte past 1 is an invalid enum value, undefined behaviour that Clang's UBSan stops at when it is loaded. `read()` never loads it back, so the login fuzz target cannot see it. `GameUI.cpp`'s character-select slot compares `getSex() == FEMALE` and draws any other value as male; `PCVampireInfo::toString()` and `PCOustersInfo::toString()` both index the two-entry `Sex2String` with it, a read past the array. `PCVampireInfo::toString()` is compiled only with `__DEBUG_OUTPUT__`, which no preset defines. `PCOustersInfo::toString()` is compiled in every build, unguarded unlike its two siblings, but its only caller is `LCPCList::toString()`, compiled only with `__DEBUG_OUTPUT__` (so is `PCInfo`'s virtual `toString()`), so no build without the macro reaches it. The slayer record packs its sex into one bit of its outlook and cannot hold another value. Found by reading on 2026-09-29 while fixing the `LCPCList` slot below (`7c81f583` names it out of scope) | open |
| `LCServerList::read` and `LCWorldList::read` leak the entry whose read throws | each loop allocates a `ServerGroupInfo` or `WorldInfo` with `new`, reads it, and only then `push_back`s it, so when the entry's read throws (a name length past the frame's body, for instance) the half-read entry is owned by nothing and leaks; the entries already stored are freed by the packet's destructor. A memory leak per rejected frame, not memory corruption. A hostile game server reaches it as well as the login server: `PacketValidator` gives the game connection's `CPS_NORMAL` the `PIST_ANY` set, so every registered parser, the `LC` ones included, reads there, and the two `PIST_IGNORE_EXCEPT` statuses (`CPS_WAITING_FOR_GC_SET_POSITION`, `CPS_WAITING_FOR_GC_RECONNECT_LOGIN`) read an id outside their set without executing it. No seed carries an entry (neither packet has a golden, so its seeds are an empty and a zero-filled frame, whose count reads 0), and the fuzz recipes run with `detect_leaks=0`, so neither the replay ctest nor the fuzzer reports it. Both login fuzz runs, like a 10-minute game-target run of 2026-09-29, printed libFuzzer's "disabled leak detection after every mutation" notice: more than a thousand inputs ended with more allocations than frees. That notice names no allocation, fires on both targets, and was not traced to this leak or to anything else. `LCPCList::read` already holds each info in a `unique_ptr` until it is stored (`7c81f583`), the shape a fix would copy. Found by reading on 2026-09-29 in the `LC` read sweep of the login fuzz target (`7c81f583` names it out of scope) | open, a leak |
| `Datagram::read` calls `read(Datagram&)` through a packet that is not a `DatagramPacket` | the UDP receive path (`Client/Packet/Datagram.cpp`, called by `ClientCommunicationManager::Update`) read the header, checked only that the id was below `PACKET_MAX`, the size cap and the datagram length, then did `pPacket = (DatagramPacket*)g_pPacketFactoryManager->createPacket(packetID)` and `pPacket->read(*this)`. The factory registers every packet the client receives on any connection - every `GC` and `LC` packet, the four `CR` packets and three of the seven `RC` packets, all plain `Packet`s that travel over TCP (the `CR`/`RC` ones on the request-server connection between clients, `RequestServerPlayer`, port 9650, Windows-only) - and only four classes are `DatagramPacket`s: `RCCharacterInfo`, `RCPositionInfo`, `RCSay` and `RCStatusHP`. For any other id the C-cast is a `static_cast` to a type the object is not, and the virtual call loads a slot past the end of that class's vtable: `DatagramPacket` appends `read(Datagram&)` after `Packet`'s eight entries, and a `GC` class declares no virtual of its own, so the call jumps to whatever pointer follows the vtable in the image. `Update` checked the validator's `CPS_CLIENT_COMMUNICATION_NORMAL` set (those four ids) only after `read()` had returned. **Who reaches it**: the socket is `DatagramSocket(Wire::ClientCommunicationUDPPort())`, bound in `ClientCommunicationManager`'s constructor, which `InitGame` runs at start-up before any login, on every native build (a WebSocket build opens none). It binds `INADDR_ANY` with `SO_REUSEADDR` on `CLIENT_COMMUNICATION_UDP_PORT`, 9858 unless the config file says otherwise, and `Update` reads it every 330 ms from the game loop and the two wait loops (`UpdateSocketInput`, called by `CGameUpdate`, `CWaitPacketUpdate` and `CWaitUIUpdate`). No datagram is authenticated: anyone who can deliver UDP to the host chooses the id - the local network always; the Internet when the host has a public address, a forwarded port or UPnP; and, since the client sends its own `RC` datagrams to other players from the same socket, whoever a NAT with endpoint-independent filtering lets back in once that mapping exists (the code facts are the bind and the read loop; which of the network cases applies is the environment's). Found by reading on 2026-09-30, while preparing the datagram fuzz target, and confirmed test-first: `Datagram.ReadRefusesAnIdThatIsNotADatagramPacket` sends a `GCSay` frame with a zero-filled 16-byte body through the unfixed read. In the plain `macos` tree the test process died with a bus error (exit 138, `SIGBUS`); in the `macos-asan` tree ASan stopped it at `Datagram.cpp:191`, the `pPacket->read(*this)` line, with a `global-buffer-overflow` "READ of size 8 ... 0 bytes after global variable 'vtable for GCSay' ... of size 80" - the load of the ninth slot of an eight-slot vtable. The server's copy of the function has refused `!isDatagram(packetID)` before creating the packet all along (`src/Core/DatagramFactoryRead.cpp`). The fix refuses, before any packet exists, every id the validator's `CPS_CLIENT_COMMUNICATION_NORMAL` set does not hold - that set is the one source of what the client accepts over UDP, and a missing validator refuses everything - with an `InvalidProtocolException`; converts the created packet with `dynamic_cast`, refusing a non-`DatagramPacket`, so a set that some day admits one cannot bring the confusion back (RTTI is on in every toolchain that compiles `packetwire`, which already `dynamic_cast`s in `ClientPlayer.cpp`); and holds the packet in a `unique_ptr` until its `read()` succeeds, so a refusal or a throwing read leaves the caller's pointer `NULL` rather than handing back a half-read packet (`Update`'s `catch` deleted it, so this was not a leak). `TheAcceptedIdsAreExactlyTheRegisteredDatagramPackets` pins the two sides against each other: over every id, the created packet is a `DatagramPacket` exactly when the set accepts it, four ids in all. A sweep for the same shape found no other: the TCP receive loops (`ClientPlayer`, `RequestServerPlayer`, `Player`) call `Packet::read(SocketInputStream&)`, which every packet has (a `DatagramPacket` throws there), and the dispatch table's `static_cast<Cls*>` is keyed on the object's own `getPacketID()`, with factory ids proven unique by the wire-layout test and double registration refused; the three `(GCAddItemToZone*)` casts in handlers are upcasts | fixed 2026-09-30 (`fix/datagram-packet-type`) |
| A hostile datagram made the client post a bug report to its server | `SendBugReport` (`Client/Packet/WireHost.cpp`) formats a `"*bug_report ..."` `CGSay` and sends it on `Wire::BugReportTarget()`, the executable's `g_pSocket` - the player's own connection, to the login server or the game server, whichever it is at the time. Two places on the UDP receive path called it once per datagram: `Datagram::read`'s size check, through `PacketDiagnostics::reportBug`, for a size field over the packet's maximum, and `ClientCommunicationManager::Update`'s `catch`, for an `InvalidProtocolException` whose text held `"(datagram)"` - only "invalid packet id(datagram)", an id at or past `PACKET_MAX`, since the match is case-sensitive and the size check says "(DataGram)". **Reach**: anyone who can send the UDP port a datagram (the row above says who), with no session and no authentication, chose when the client spoke on the player's session: `Update` reads at most `MAX_PROCESS_PACKET` (11) datagrams every 330 ms, so up to about 33 chat lines a second for as long as the flood lasts, the rest queuing and dropping in the socket buffer. What the servers do with a report line, or with a `CGSay` on the login connection, was not checked (the server is out of scope). Found by reading on 2026-09-30 with the row above. Both calls are gone: a refused datagram is logged and dropped. `ClientCommunicationManager.AHostileDatagramSendsNoBugReport` (`tests/unit/test_client_communication.cpp`) binds the real manager on a free loopback port, sends four past-`PACKET_MAX` datagrams, three over-size `RCPositionInfo`s, a `GCSay` and one valid `RCPositionInfo`, and counts the host's `BugReportTarget` queries: 7 on the unfixed code, 0 now, with the valid one dispatched as the positive control; `Datagram.ASizeFieldOverTheMaximumIsRefusedWithoutAReport` pins the size check through the `PacketDiagnostics` hook (1 report before, 0 after). **Left open, a log write per datagram**: every datagram still writes log lines at `INFO` - two from `DatagramSocket::receive`, `"[CCM-Update] something"`, and the refusal's text from `Update` - which `InitGame` sends to the console in every build and, in a `_DEBUG` build, also to `DarkEden-debug.log` beside the executable (overwritten at each launch, unbounded during a run). The same cap bounds it at about 33 datagrams a second, on the order of a hundred lines a second; a disk-filling concern only for a Debug build left running under a flood | fixed 2026-09-30 (`fix/datagram-packet-type`); the log lines open |

### Found by fuzzing

This review was static, and so were the passes after it, until a libFuzzer
target over the client's receive path landed on 2026-09-29
(`tests/fuzz/fuzz_client_stream.cpp`; CLAUDE.md, *Fuzzing the packet
readers*, says how to run it). It feeds an encrypt code and a raw TCP stream
through the gates of `ClientPlayer::processCommand` into the `read()` of every
packet the client's factory registers, frame-bounded as in production, under
ASan and UBSan. A survey
probe on `deac9b56` found three crash inputs. The harness on
`feat/packet-fuzzing` replayed them: two defects, and the third input was the
first defect again. Its first run from the seed corpus found the first defect
through a third packet. With both fixed, a 15-minute single-process run
(macOS 27, Apple Clang 21, Homebrew llvm@21's libFuzzer; 482,712 inputs,
coverage from 11,215 to 19,051 points) and a 10-minute run from its corpus
with length control off, so inputs up to the 32 KB ring came early (99,849
inputs, coverage to 19,491), found no crash, sanitizer report, timeout or
out-of-memory, and reached the same five `Assert` sites below. A two-minute
run of the `linux-fuzz` preset (Clang 18, arm64 Linux; 209,796 inputs) found
nothing either. Every fixed
input is a file under `tests/fuzz/regressions/client_stream`, which the
`fuzz_replay_client_stream` ctest replays on every platform.

A second target, `tests/fuzz/fuzz_client_login_stream.cpp`, followed the same
day on `feat/fuzz-login-stream`: the login server's connection, which is the
same `ClientPlayer` read through the same gates, once in each of the seven
statuses the player holds there (`CPS_AFTER_SENDING_CL_LOGIN` to
`CPS_AFTER_SENDING_CL_SELECT_PC`), so a frame reaches a parser only when that
status's validator set accepts its id, as in production. The `LC` parsers
were not new to fuzzing: the game target's `CPS_NORMAL` accepts every
registered id, so they were reachable there, and its corpus holds `LC`
frames that mutation found, `LCPCList` and the two leaking list readers
among them. What it lacked were `LC` seeds and the login statuses' gating,
which the login target adds. Its first run from
the 32 `LC` seeds crashed within a second in `LCPCList::read`, the defect
client PR #291 had listed as found and not fixed (the row below). With that
fixed, a 15-minute single-process run with the same toolchain (3,025,254
inputs, coverage from 1,797 to 2,259 points, the last new point at input
739,926, about three minutes in) and a 10-minute run from its corpus with
length control off (1,831,754 inputs, no new edge: `cov` stayed at 2,259
while libFuzzer's feature count rose from 7,638 to 7,960) found no crash,
sanitizer report, timeout or out-of-memory, and failed no `Assert`. That
last is so by inspection as well: no `LC` reader, and none of the
`PCSlayerInfo`, `PCVampireInfo`, `PCOustersInfo`, `ServerGroupInfo` and
`WorldInfo` readers under them, contains an `Assert`. The target is small
beside the game one (seven statuses accepting 17 ids between them), so its
coverage flattens within minutes. Its fixed input is under
`tests/fuzz/regressions/client_login_stream`, which the
`fuzz_replay_client_login_stream` ctest replays on every platform. On the
unfixed code that replay passes in the plain `macos` tree and stops in the
`macos-asan` tree, where Clang's UBSan halts at the invalid `Slot` load,
before any store; no run with ASan alone was made, so what ASan would say
of the store is not known. The unit tests are the guard in every tree. The
game target reaches the same read: the same input, replayed through
`fuzz_replay_client_stream` in the `macos-asan` tree on the unfixed code,
stops at the same `Slot` load (checked 2026-09-30). A sweep of the other
`LC` reads for the same shape found no wire value indexing a fixed array:
their lists grow and their strings are `std::string`s. It found a leak
instead, recorded under *Found by reading*: `LCServerList` and `LCWorldList`
lose a list entry whose read throws.
The `LC` handlers, which index the string table and the character-select
arrays, are executable-side and not part of that sweep.

What the targets do not reach: the handlers under `Client/PacketHandler`,
which are executable-side (a crash in `read()` is found; a crash in what a
handler does with an accepted value is not); the UDP datagram path, which is
the next target; and the `toString()` methods, which `ClientPlayer`'s
receive loop calls only in a `__DEBUG_OUTPUT__` build (most of them are
compiled only under it; `PCOustersInfo::toString()` is not, see its row
under *Found by reading*).

| Defect | Symptom | Commit |
|---|---|---|
| `NicknameInfo::read` aborts the client on an unknown nickname type | its type switch ended in `default: assert(false);`, the libc assert, which aborts in every build without `NDEBUG`. The type is a wire byte in `GCModifyNickname`, `GCAddNickname`, `GCAddSlayer`, `GCAddVampire`, `GCAddOusters`, `GCNicknameList` and `GCUpdateInfo`, so one byte from a hostile or corrupt server ended any Debug client. The fuzzer found it through `GCModifyNickname` (types 0xff and 0xb3) and `GCAddNickname` (0x80). The default case throws `InvalidProtocolException`, as the server's copy of the class does. `getSize()` and `write()` keep the same assert, which no received value can reach any more. Pinned by `tests/unit/test_packet_fuzz_findings.cpp` and three regression inputs; before the fix, the unit test process aborted | fixed 2026-09-29 (`c705ea09`) |
| `StoreInfo::read` writes past its 20-item vector | the item count is a wire byte, up to 255, and the loop read that many `StoreItemInfo` records into `m_Items`, which the constructor sizes to `MAX_ITEM_NUM` (20): a heap overflow of attacker-chosen length through `GCMyStoreInfo` or `GCOtherStoreInfo`, on the ordinary personal-shop path. ASan reported the write in `StoreItemInfo::read`. A count past `m_Items.size()` is refused before any item is read. The server's de-kernel copy has the identical loop, masked only because `GamePlayer::processCommand` skips the two store packets; a separate server change fixes it there. Pinned by `tests/unit/test_packet_fuzz_findings.cpp` and one regression input | fixed 2026-09-29 (`0c782d10`) |
| `LCPCList::read` stores a character past its three-entry list | each character's slot is a wire byte, up to 255, which `PCSlayerInfo::read`, `PCVampireInfo::read` and `PCOustersInfo::read` cast to `Slot` unchecked, and `LCPCList::read` stored the info's pointer at `m_pPCInfos[getSlot()]`, an array of `SLOT_MAX` (3): a slot of 3 wrote one entry past it, and a slot of 4 or more, no `Slot` value at all (Clang's UBSan stops at the load), wrote up to 2 KB past the heap-allocated packet. The login server sends `LCPCList` after every login and character change, and a hostile game server reaches the same read, since `CPS_NORMAL` accepts every registered id (on the unfixed code the regression input stopped the game target's replay too; it is kept only with the login target's regressions). The login target found it through a slayer slot byte of 188. Each reader now refuses a slot of `SLOT_MAX` or more with `InvalidProtocolException` before the cast; `PCSlayerInfo::read` also lost a `catch (Throwable&)` around its body that would have swallowed the refusal, as it swallowed its two name-length refusals, and `LCPCList::read` owns each info until it stores it, so a refused read no longer leaks it. Pinned by five tests in `tests/unit/test_packet_fuzz_findings.cpp` (three failed on the unfixed code, with 18, 2 and 2 failed checks) and one regression input | fixed 2026-09-29 (`7c81f583`) |
| `Assert`-only bounds that hostile input reaches | in its 15-minute run the game target failed about a thousand `Assert`s (1,013) at five sites: `CGSMSSend::read`'s four length and count checks and `GCTimeLimitItemInfo::read`'s count check. In a build without `NDEBUG` an `Assert` throws, which the receive loop treats as a rejected packet; Release compiles them out, and then the values are accepted. Each one guards a growing container (a `std::list`, a `std::string`, a `std::map`), and the frame's size cap bounds what the loop can read, so none is a memory error; recorded as a class rather than fixed one by one. `CGSMSSend` is reached at all because `PacketFactoryManager` registers a factory for this client-to-server packet and `CPS_NORMAL` accepts every registered id, so the client parses it and only then refuses it for lack of a handler | open, not a memory error |

### Gear-addon follow-up from the packet-index pass

✅ **Fixed as a regression guard** on `fix/review-item-visual-guards` (2026-09-18). `GCRemoveFromGearHandler` now maps vampire coat, left-hand and right-hand enum values directly to their visual addons; other slots have no addon. This replaces the positional table whose stale weapon-slot entries both selected `ADDON_NULL`.

The same audit found that all three races tried to redirect a two-hand removal by looking at gear slots *after* `RemoveItem` had cleared both aliases. The handler now uses the returned item's two-hand property to select the right-hand addon. The gear implementations and existing `SlayerGear.ATwoHandedWeaponTakesBothHands` test establish that removal clears both aliases; the live addon calls remain under the packet-handler exemption. Full Windows Debug/ASan builds and all nine CTests pass. The packet-index audit now counts 113 subscripts, including 12 raw-container sites (down from 114/13 because the vampire lookup no longer indexes an array); its baseline is tightened in the same change. Live-server verification is waived, and no runtime reproduction is claimed.

### Caveats

**Partial sprite-load result follow-up (2026-09-23):** both generic pack
variants previously returned true from every range/set preload, even when
individual lazy records failed. They now retain and report those failures,
validate IDs/ranges and continue loading valid following rows. Empty requests
and the absent-sprite sentinel remain valid; an all-ID set no longer becomes
an empty iteration when its size narrows to 16 bits. Three test-first cases
reproduced 24 failed checks; a fourth covers valid empty pixel sprites in both
formats and the two-type pack. Addon/creature callers no longer publish loaded
flags after rejection. Both zone-loading branches now check tile/image preload
results and use the existing quit/priority-reset path. Game wiring is a
source/build regression guard, with library tests owning the loader contract.

**Guild-mark reader follow-up (2026-09-23):** the custom loader used
`2 + spriteID * sizeof(long)` for a file containing four-byte offsets, selecting
the wrong entry on LP64 systems. It now uses the shared checked index reader,
validates that both sprite IDs exist and seeks each one separately. Scoped
owners retain the large/small pair until both decodes succeed and cache
publication completes. Rejected pairs are logged and negatively cached without
partial art; repeated failed-cache lookups return false. Shared index and pixel
tests own decoding. The game-cache integration is a source/build regression
guard; no live-client reproduction is claimed. All 4,758 installed guild-mark
records pass the index helper and both pixel decoders under ASan (9,516 calls).

**Ending failure cleanup (2026-09-23):** the advancement ending now checks its
eager load, requires all seven sprites and validates the event's adjacent pair
before index addition. The three event setters select pairs 1/2, 3/4 and 5/6;
the scrolling path also uses sprite 0. On missing or rejected resources, both
advancement and Ousters endings reset their timer, remove the event and release
partial storage before returning to normal background drawing. This also clears
the ending's input-denial flags instead of leaving a failed event active.
These executable changes are source/build regression guards. All seven installed
`AdvancementQuestEnding.spk` records pass the production eager pack reader in
both formats under ASan (14 decoder calls); the eleven Ousters records were
validated in the earlier pack audit.

**File-index table follow-up (2026-09-23):** `CFileIndexTable` checks complete
little-endian counts and signed 32-bit offsets before replacing live storage.
Offsets below the pack header or above `INT32_MAX` are rejected; repeated and
out-of-order offsets remain valid. Complete malformed tables retain the next
record's cursor. Owned vectors prevent shallow-copy lifetime bugs; checked
lookup preserves its output on a missing ID, and direct invalid access throws.
Nine real-library tests cover truncated prefixes, invalid offsets, empty and
maximum counts, following records, copies, release, bounds and stream exceptions.
Two test-first cases reproduced 38 failed checks. The title and disconnect
callers now check lookup and decoding, report failures and avoid publishing or
mutating a shared fallback sprite. These executable integrations are regression
guards. All six records of the installed `UI.spk` pass the changed index reader
and both pixel decoders under ASan (12 decoder calls).

**Asset-inventory expansion (2026-09-23):** the earlier UI denominator counted
constant `new C_SPRITE_PACK(...)` calls but missed member `.Open(...)` calls.
A comment- and string-aware source map now covers 151 distinct installed UI
packs from 209 constant-path call sites; all 3,213 indexed records pass both pixel
decoders under ASan (6,426 calls). The earlier 152-path / 6,578-call total counted
`ArrowTile.spk` twice through case aliases; the distinct-file rerun corrects it.
This total includes the earlier 75 files, rather than
adding to them. Three referenced friend-window packs (`FrdWinSlayer.spk`,
`FrdWinVampire.spk`, `FrdWinOusters.spk`) are absent from this installation at six
call sites and have no asset-validation claim. The eager `Etc.spk`, `Weather.spk`
and `OustersFin.spk` packs also pass the production pack reader in both formats:
47, 24 and 11 records respectively (164 calls). A source scan excluding comments
and string literals finds no active `CSpritePackList`, `CSpritePalPackList` or
`CAlphaSpritePackList` callers outside SpriteLib in Client, VS_UI or tools; their
legacy APIs are not assigned installed asset formats. Dynamic profile filenames
and unused wrapper/list APIs are classified in the final
[sprite audit](sprite-asset-audit-2026-09-23.md); generated profile samples
and absent files have no installed-asset validation claim.

**Game pack failure propagation (2026-09-23):** `MTopView::InitSprites` now checks
all 26 active lazy sprite/palette opens, allowing initialization/restoration to
report rejected indexes. It checks the eager screen palette, miscellaneous and
weather loaders too; failed eager loads clear partial storage, including the
already checked effect-shadow path. Empty miscellaneous/weather packs fail,
and the Ousters ending requires all eleven sprite slots before rendering.
Clearing failed packs keeps the size-based retry path reachable. Startup and
display restoration now stop through their existing cleanup/quit paths when
view initialization fails. The tile renderer is discarded before its borrowed
surface is replaced, so an early pack rejection cannot leave a dangling renderer
or leak it on retry. These are executable regression guards, reviewed against
the initialization callers, renderer ownership and the ending's sprite IDs,
not runtime reproductions. Existing library tests own
the loaders. Offline ASan checks also accept the installed effect palettes in
both formats: 1,957 indexed `Effect.ppk` records and 606 eager `EffectScreen.ppk`
records, for 5,126 decoder calls. Lazy record failures still report at first use;
this change does not eagerly decode the large game packs during startup.

**Specialized alpha-pack follow-up (2026-09-23):** `CAlphaSpritePack` now owns
typed 555/565 arrays, including concrete indexing and destruction, and disables
shallow copying. Full reloads publish only complete packs; partial and indexed
loads validate IDs/ranges/offsets and report decode failure. Complete malformed
records retain the following-record cursor; successful earlier partial rows
remain loaded, while rejected decoded rows are empty. Reused index streams read
their header from the beginning. Twelve real-library tests cover both formats,
reloads, malformed input, exceptions, maximum counts, packed cursors and the two
save paths; three test-first cases reproduced 54 failed checks. This is a latent
library API fix: current game effect-alpha resources use `CAlphaSpritePalPack`,
and the specialized pack has no active game construction. It does not enlarge
the installed-asset audit's format coverage.

**Light-filter pack follow-up (2026-09-23):** the live `CFilterPack` loader
validates counts against the remaining input and stages complete filters before
replacing existing rows. Failed reads preserve the previous pack, and explicit
empty loads or initialization clear it. Seven library tests cover every
truncated prefix, invalid counts/dimensions, a malformed later row, maximum
counts, stream exceptions and following records; two test-first cases reproduced
17 failed checks. Filter row-pointer arrays are zero-initialized so interrupted
allocation can be released safely; allocator failure itself was not injected.
The game reports and propagates rejected or empty light resources, and skips
empty packs in both drawing paths. These game integrations are regression
guards. The production ASan pack loader accepts all 12 installed `Light2D.ftp`
records with no trailing bytes. Subsequent specialized-caller and UI coverage
is recorded in the [sprite audit](sprite-asset-audit-2026-09-23.md).

**Portal-resource follow-up (2026-09-21):** the Slayer portal's binary reader
was separated from its game-coupled dialog, then ASan reproduced its null
dereference with the final coordinate truncated by one byte. `SlayerPortalData`
now checks the six-map little-endian format, nonempty counts capped at 65536 per
map and bounded by remaining bytes, unsigned-16 zone IDs, nonnegative screen
positions and destination coordinates below 256. Extra bytes are rejected;
failure preserves both the prior data and reader cursor. Seven real-library
tests cover every truncated prefix, bad fields/counts, the count limit, missing
data, repeated loads and unaligned input. The dialog checks opens, logs rejected
files, skips positions outside its map sprite and retains filtered zones for
navigation. UI regression guards also replace the all-filtered navigation
fallback's bitwise `&` with modulo and check a zone-name lookup before use.
Packed resource reading is restored by the 2026-09-22 follow-up below.

- **555 rejection cursor follow-up resolved (2026-09-21).** `CAlphaSprite555` already uses the shared alpha loader, whose packed-record cursor test covers both formats. `CIndexSprite555` and `CIndexSprite565` now share a checked loader too: both header words and complete rows must read successfully; encoded counts, cumulative pixel width and palette gradations are validated before publication. Complete malformed records consume all their rows before rejection, preserving the next packed sprite; truncated input retains stream failure and an empty sprite. Only fixed colors convert to 555, while palette words and padding survive. `Release` also resets empty-header state. Four new real-library tests reproduced 151 failed checks before the fix, then passed under ASan. The existing alpha cursor test supplies evidence for its older fix. The generic pack callers now log and propagate rejection; the completed [sprite audit](sprite-asset-audit-2026-09-23.md) records formats and specialized callers.
- **The 555 fixes are latent in this build.** `ColorDraw::Is565()` returns a hardcoded `true`, and the 555 sprite variants are only constructed on the false branch, so the `CSprite555` family and `Convert565to555` fixes have no runtime effect today. They matter if a 5:5:5 surface is ever supported again.
- **Source-mapped real-art audit completed (2026-09-23).** The production ASan
  readers accepted 193 distinct installed files, 345,531 declared records and
  531,564 decoder calls. The [audit and inventory](sprite-asset-audit-2026-09-23.md)
  identify formats, indexed/eager paths, the corrected UI case-alias count,
  absent friend-window packs, generated profiles and unused legacy APIs.
  Unreferenced trailing bytes remain outside the claim. This is parser/index
  evidence, not a game-rendering run; the initial extension-only scan is not
  evidence of asset defects.

- **Packed resource reading and listing restored (2026-09-22).** The port's extracted-file-only `CRarFile` left quest data, help and tutorial resources inside password-protected RPK archives unreadable. It now keeps loose files as overrides and falls back to `basic/RarArchive`, backed by pinned static UnRAR 7.3.1. `GetList` returns owned regular-member names from the selected archive, with optional wildcard filtering. Decompression goes to bounded memory, never to filesystem extraction; volumes, links, unsafe member paths, corrupt data and oversized members fail without publishing partial bytes. Limits cover member bytes, dictionary size, cumulative decompression, header count and listed-name storage. The reader preserves its raw-byte and once-decoded text contracts, including the 16 MiB text-input cap and UTF-8 line clipping.

  `test_resource_reader.cpp` reproduced twelve failed checks for missing packed reads, encrypted data/headers and listing. `test_rar_archive.cpp` adds regression guards for every truncated prefix of a first member, damaged CRCs, impossible metadata, per-read limits, rejected links/paths, solid encrypted RAR4/RAR5, Unicode member names and UTF-8 archive paths. An offline audit through the production archive adapter, including Linux UBSan with the portable-access patch, read all **1,733 members / 761,686 bytes across eight installed Info/UI packs**, with zero failures, including `SimpleGQuest.xml` and `EventGQuest.xml`. This proves the installed archives decode; it does not claim a game-rendering run. Plain-text ingress and renderer fallback retirement are now covered by the [display-text audit](text-ingress-audit-2026-09-23.md), which separately records XML parsing and the malformed optional ghost-position asset.

- **The scanline validation enforces an upper bound, not an exact one.** The encoder normally emits exactly `width` pixels per row, but its per-row segment count is stored in a byte, so a row needing more than 255 segments truncates and decodes to fewer.
- **Reproduction limits remain explicit.** The original out-of-range `CTypePack::Get` test was a guard, not a reproduced fault. Partial preload behavior is now observable through the real indexed fixtures in `test_ctypepack_indexed.cpp`; its range/set follow-up reproduced 24 failed checks.
- **`USE_ASAN` now covers MSVC as well as GCC and Clang.** `/fsanitize=address` is wired up in `CMakeLists.txt`, along with the flag surgery it requires: `/RTC1` and incremental linking are both incompatible and are removed, and the sanitizer runtime DLL is copied beside the executables so a run outside the debugger can find it. It needs the *C++ AddressSanitizer* individual component, which the C++ workload does not install; configure fails with instructions if it is absent. See the AddressSanitizer section of `README.md`.

  Packet parsers and extracted game helpers now link into real-library tests,
  which run under ASan. Remaining game-global integrations use the named
  executable exemptions, source review and full builds. Enabling ASan alone
  is not evidence of exercising a path; live-server verification stays optional.
- **Enabling it costs `/RTC1`.** MSVC rejects the runtime checks alongside the sanitizer, and `/RTC1` is what caught the uninitialised `bool` in the runtime defect list above. The two builds are complementary rather than one superseding the other, and `README.md` now sets out which checks live in which.

  `/RTC1` itself was only ever present because CMake happens to include it in the built-in Debug flags — nothing in this tree asked for it, so a toolchain file, preset or CI script that set `CMAKE_CXX_FLAGS_DEBUG` would have removed the check without a word. It is now requested explicitly, and configure reports which of the two mutually exclusive check sets is active.

---

## Cross-Cutting Themes

1. **Unvalidated network input is the #1 risk.** Packet handlers pass server-supplied lengths, coordinates, item classes, and indices straight into array subscripts, `strcpy`/`strcat`, and function-pointer tables. A hostile *or merely buggy* server can corrupt the client heap or achieve code execution. This pattern recurs in `MItem`, `MZone`, `MCreature`, the packet layer, sprite/pak loaders, and text handling.
2. **Safety checks compiled out of the real build.** Multiple table/bounds checks are wrapped in `#ifdef _DEBUG`, and `CMakeLists.txt` never defines `_DEBUG` -- even for `make debug-asan`. The guards exist but do nothing in practice.
3. **Fixed-size buffers fed by variable-length data.** 21-byte chat rows, stack `char` buffers, and format targets receive unbounded server strings via `strcpy`/`strcat`/`sprintf`.
4. **Dead / duplicate source in the tree.** Hundreds of KB of stubbed or shadow implementations sit alongside live code, plus committed generated/IDE files and a large non-English asset-note directory -- real maintenance drag and a correctness trap when the wrong file is edited.
5. **64-bit and portability debt.** Pointer-to-int casts, `DWORD`/`long` size assumptions, and inconsistent `_WIN32`/`__WIN32__` guards remain outside the abstraction layer.

---

## All Critical Findings (Priority Fix List)

Every critical finding, grouped for a focused first pass. Full detail (including medium/low) is in the per-area sections below.

### C1. dxlib_sound_duplicate() copies the Mix_Chunk pointer instead of the chunk, and dxlib_sound_free() frees it unconditionally, so freeing any duplicate destroys the original's audio data.

**Area:** Input, Audio & Media  |  **Category:** memory-safety  |  **Location:** `Client/DXLib/DXLibBackendSDL.cpp:692`

`dxlib_sound_duplicate()` (DXLibBackendSDL.cpp:685-699) allocates a new `dxlib_sound_buffer` and sets `duplicate->chunk = sound->chunk;` (line 692) — both wrappers now own the same `Mix_Chunk*`. `dxlib_sound_free()` (line 617-626) then calls `Mix_FreeChunk(sound->chunk)` with no reference counting. The duplicate path is exercised constantly: `CSDLAudio::Play(buffer, loop, bDuplicate)` calls `DuplicateSoundBuffer(buffer, true)` (CDirectSound_Adapter.cpp:221) whenever the sound is already playing, GameMain.cpp:3714/3761 pass `g_bGoodFPS` as bDuplicate, and `CGameUpdate.cpp:5996` calls `g_SDLAudio.ReleaseTerminatedDuplicateBuffer()` every frame, which does `dxlib_sound_free(wrapper->sound); delete wrapper;` (CDirectSound_Adapter.cpp:249-253). MZoneSoundManager.cpp:184/201 additionally hold long-lived duplicates created with bAutoRelease=false whose chunk is owned by the LRU-cached master, and GameMain.cpp:2143 calls ReleaseDuplicateBuffer() on every zone load.

**Failure scenario:** Player triggers the same sound twice in quick succession. Frame N: Play() sees the master chunk already playing, creates an auto-release duplicate sharing chunk C, plays it. Frame N+k: the duplicate finishes; ReleaseTerminatedDuplicateBuffer() calls Mix_FreeChunk(C) and frees the wrapper. The master wrapper still cached in g_pSoundManager now holds a dangling `chunk`. The next PlaySound for that sound ID hits the GetData() branch (GameMain.cpp:3730) and passes the freed chunk to Mix_PlayChannel — use-after-free in the mixer thread. Releasing the master later double-frees C.

**Recommendation:** Either give dxlib_sound_buffer an ownership flag (`owns_chunk`) that duplicates clear, or reference-count the Mix_Chunk. dxlib_sound_free() must only call Mix_FreeChunk when it is the last/owning wrapper.

> ✅ **Fixed** in `c0670ae` (the commit that made sound play at all). The decoded data lives in a reference-counted `dxlib_chunk_ref` shared by every duplicate; Mix_FreeChunk runs only when the last reference goes.

### C2. PlaySound copies a sound filename of up to 64 KB from Sound.inf into a 256-byte stack buffer with an unbounded strcpy.

**Area:** Input, Audio & Media  |  **Category:** memory-safety  |  **Location:** `Client/GameMain.cpp:3651`

`char strFilename[256]; strcpy(strFilename, pFilename);` at GameMain.cpp:3650-3651, repeated at 3839-3840, 3912-3913, 3964-3965, 4038-4039 and 3772-3773. `pFilename` is `(*g_pSoundTable)[soundID].Filename`, an `MString` loaded from `Data/Info/Sound.inf` via `SOUNDTABLE_INFO::LoadFromFile` -> `MString::LoadFromFile` (Client/MString.cpp:199-244), whose only length check accepts anything up to `MAX_STRING_LENGTH = 65536`. There is no length check before the copy — only a NULL check.

**Failure scenario:** A Sound.inf entry (shipped game data, patch payload, or a tampered install) with a filename longer than 255 bytes causes strcpy to write past `strFilename` on the stack of PlaySound, corrupting saved registers/return address. Because PlaySound is driven by gameplay events, this is remotely influenceable by whatever content the server tells the client to play.

**Recommendation:** Replace with a bounded copy (`strncpy` + explicit NUL, or `snprintf`) sized to the buffer, or pass the MString's char* straight to LoadWav — the temporary copy serves no purpose.

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`). The three live sites (the others sit inside commented-out ForceSound blocks) are truncating bounded copies; an over-long Sound.inf entry now fails to load and gets logged instead of smashing the stack. Regression guard, executable-only code.

### C3. CSpritePalBase::LoadFromFile builds the per-scanline pixel pointer table from unvalidated file offsets, so m_pPixels[] entries can point arbitrarily outside the allocation.

**Area:** Rendering & Sprites  |  **Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSpritePalBase.cpp:62`

> ✅ **Fixed** in `b7c4eb3`. The declared size is checked against the file, and the running scanline offset is kept inside the pixel data. Covered by `tests/unit/test_cspritepalbase.cpp`, which asserts the pointer invariant directly rather than waiting for a fault.

m_Size is a DWORD read verbatim from the file (line 53) and used unchecked as the allocation size: `m_pData = new BYTE[m_Size+sizeof(BYTE*)*m_Height]` (line 62). No read-failure check, no cap. Then lines 69-79 read an m_Height-entry WORD array from the file and accumulate it into a running pointer: `m_pPixels[i] = tempData; tempData += indexArray[i];` with no check that the accumulated offset stays inside m_Size. Every m_pPixels[i] beyond the point where the offsets exceed m_Size points into unrelated heap. Those pointers are dereferenced later by CAlphaSpritePal::Blt (see separate finding), which reads RLE opcodes from them and writes the decoded pixels into the locked backbuffer. Also note the addition `m_Size + sizeof(BYTE*)*m_Height` can wrap on a 32-bit build, yielding a tiny allocation for a huge declared size.

**Recommendation:** Cap m_Size against a sane maximum and against the remaining file length; verify each file.read() succeeded; and validate that the running sum of indexArray[] never exceeds m_Size before storing each m_pPixels[i], bailing to SetEmptySprite() on violation.

### C4. CAlphaSpritePal::Blt walks file-derived RLE data with zero bounds checks and writes decoded pixels straight into the locked surface with no width clamp.

**Area:** Rendering & Sprites  |  **Category:** memory-safety  |  **Location:** `Client/SpriteLib/CAlphaSpritePal.cpp:461`

> ✅ **Fixed** in `764e9ab` (Blt's own bound), `c444a83` (the unchecked MPalette::operator[] this finding also names) and `a17aa39` (scanline validation at load time, which covers the twenty-odd clipping and pixel format blit variants that share this defect). Covered by `tests/unit/test_calphaspritepal.cpp`, using a sentinel filled guard band beyond the sprite width.

Lines 480-502: `count = *pPixels++` then per segment `pDestTemp += *pPixels++` (transparent run) and `colorCount = *pPixels++`, followed by `memcpyAlpha(pDestTemp, pPixels, colorCount, pal)` and `pPixels += (colorCount<<1)`. None of count, the transparent run, or colorCount is validated against m_Width or against the size of the m_pPixels[i] buffer. This is called from CSpriteSurface::BltAlphaSpritePal (Client/SpriteLib/CSpriteSurface_Adapter.cpp:657) with pDest pointing into the freshly locked backbuffer, so an oversized transparent run or colorCount writes past the end of the destination scanline and past the end of the surface allocation. memcpyAlpha additionally indexes the palette with `pal[*pSource]` (CAlphaSpritePal.cpp:265) where MPalette::operator[] is an unchecked `m_pColor[n]` over an m_Size-entry array (Client/SpriteLib/MPalette.h:30), so any pixel byte >= m_Size reads out of bounds too.

**Recommendation:** Clamp the running destination x against m_Width and the surface width, validate colorCount and the transparent run against the remaining width, and bounds-check the palette index against MPalette::GetSize() before dereferencing.

### C5. CSprite555::LoadFromFile decodes the RLE structure in place with no bound against the buffer it just allocated, producing an out-of-bounds heap read and write.

**Area:** Rendering & Sprites  |  **Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSprite555.cpp:182`

> ✅ **Fixed** in `cfc6cd6`, with the same bound applied to CAlphaSprite555 and CIndexSprite555 in `964c043`. Covered by `tests/unit/test_csprite555.cpp` and `tests/unit/test_sprite555_siblings.cpp`.

Line 181-183 read a WORD `len` from the file, allocate `new WORD[len]` and read len WORDs into it. Lines 185-204 then walk that buffer as RLE: `count = m_Pixels[i][0]`, and for each of `count` segments `colorCount = m_Pixels[i][index+1]`, then `m_Pixels[i][index] = ColorDraw::Convert565to555(m_Pixels[i][index])` for colorCount iterations. `index` is never compared against `len`. A file whose count/colorCount fields overstate the actual data walks index past the allocation and writes converted values there. If len is 0, `new WORD[0]` still returns a valid pointer and `m_Pixels[i][0]` is already out of bounds. Its sibling CSprite565::LoadFromFile (Client/SpriteLib/CSprite565.cpp:154) does have a `len > 0 && len <= 8192` guard, so this is the format-variant duplication hazard biting: the check was added to one of the pair only.

**Recommendation:** Apply the same length sanity check as CSprite565, and additionally bound `index` against `len` inside the segment walk in both files; on violation zero the line and stop parsing it.

### C6. CFilter::LoadFromFile overwrites m_Width/m_Height from the file before Init() calls Release(), so Release() frees the old pointer array using the new, file-controlled height.

**Area:** Rendering & Sprites  |  **Category:** memory-safety  |  **Location:** `Client/SpriteLib/CFilter.cpp:200`

> ✅ **Fixed** in `90d6794`. The header is read into locals and validated against the bytes the file actually carries before anything is released or allocated, so a rejected file leaves the existing filter intact. Covered by `tests/unit/test_cfilter.cpp`; the reported failure reproduced as an access violation.

LoadFromFile reads directly into the members: `file.read((char*)&m_Width, 2); file.read((char*)&m_Height, 2);` (lines 200-201), then calls `Init(m_Width, m_Height)` (line 208). Init immediately calls Release() (CFilter.cpp:37), and Release loops `for (int i=0; i<m_Height; i++) delete [] m_ppFilter[i];` (CFilter.cpp:59-60) using the just-overwritten m_Height. Reloading a filter whose file declares a larger height than the currently held one runs `delete[]` over indeterminate pointers past the end of the old m_ppFilter array — an arbitrary free driven by file content. This is the one Release-ordering bug of its kind in the area: CSprite565/555, CAlphaSprite565, CIndexSprite565 and CShadowSprite all correctly call Release() before reading the header.

**Recommendation:** Read the header into locals and only assign m_Width/m_Height after Init() has released the old buffers, matching the ordering used by the sprite loaders.

### C7. GCShopList::read uses a raw network-supplied BYTE as an index into a 20-element array with no bounds check, writing a full struct out of bounds.

**Area:** Networking & Protocol  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCShopList.cpp:65`

`iStream.read(index)` (line 63) reads an unvalidated BYTE (0-255), then line 65 does `_SHOPLISTITEM& item = m_pBuffer[index];`. `m_pBuffer` is declared `SHOPLISTITEM m_pBuffer[SHOP_RACK_INDEX_MAX]` (GCShopList.h:84) and `SHOP_RACK_INDEX_MAX` is 20 (Client/Packet/Types/ShopTypes.h:31). Lines 67-85 then write objectID, itemClass, itemType, durability, silver, grade, enchantLevel and `bExist` through that reference, and line 77 calls `item.optionType.push_back(optionType)` — invoking std::vector member functions on whatever memory lies past the array. The outer `packetSize > getPacketMaxSize()` check in ClientPlayer.cpp:256 does not help: a packet of legal declared size can carry index=255.

**Failure scenario:** A malicious or compromised game server sends PACKET_GC_SHOP_LIST with nTotal=1 and index=0xFF. The client writes ~40 bytes of attacker-chosen data roughly 235 elements past the end of m_pBuffer, then calls std::vector::push_back on the out-of-bounds 'optionType' member, dereferencing attacker-influenced pointers. This is a controlled heap write leading to likely arbitrary code execution.

**Recommendation:** Reject the packet when `index >= SHOP_RACK_INDEX_MAX` (throw InvalidProtocolException) before touching m_pBuffer, exactly as the packet-ID bound is enforced in PacketFactoryManager::createPacket.

> ✅ **Fixed** in `ed4f872` (PR #4). The index is rejected with InvalidProtocolException before the array is touched. Regression guard, executable-only code with no test path.

### C8. GCStashList::read indexes four parallel 3x20 arrays with two unvalidated network-supplied BYTEs.

**Area:** Networking & Protocol  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCStashList.cpp:92`

Lines 90-91 read `rack` and `index` as raw BYTEs from the stream with no validation. Line 92 (`m_pItems[rack][index]`), line 114 (`iStream.read(m_SubItemsCount[rack][index])`), line 119 (`m_pSubItems[rack][index].push_back(pSubItemInfo)`) and line 122 (`m_bExist[rack][index] = true`) all index arrays dimensioned `[STASH_RACK_MAX][STASH_INDEX_MAX]` (GCStashList.h:94-97), i.e. [3][20] per Client/Packet/Types/ItemTypes.h:224,230. With rack and index each up to 255 the effective offset reaches 255*20+255 elements past the base. Line 119 additionally invokes `std::list::push_back` on an out-of-bounds list object.

**Failure scenario:** Server sends PACKET_GC_STASH_LIST with nTotal=1, rack=0xFF, index=0xFF. The client writes a STASHITEM struct and a bool tens of kilobytes past the arrays, then calls std::list::push_back through an out-of-bounds list head, corrupting the heap with attacker-controlled pointers.

**Recommendation:** Validate `rack < STASH_RACK_MAX && index < STASH_INDEX_MAX` immediately after reading them and throw InvalidProtocolException otherwise. The same guard is needed on the accessors at lines 318, 331 and 363.

> ✅ **Fixed** in `ed4f872` (PR #4). read() rejects out-of-range rack/index with InvalidProtocolException, and the Assert-only accessors gained release-build range checks. The setStashItem site is `#ifdef __GAME_SERVER__` and does not compile into the client; it remains a live issue in the server repository.

### C9. GCPartySay reads name and message with no length validation, and the handler strcpy's both into 128-byte stack buffers.

**Area:** Networking & Protocol  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCPartySayHandler.cpp:37`

GCPartySay::read (Client/Packet/Gpackets/GCPartySay.cpp:22-27) reads a BYTE `szName`, then `iStream.read(m_Name, szName)`, then reuses the same variable for the message: `iStream.read(szName); iStream.read(m_Message, szName)`. There is no `> N` bound on either — unlike sibling chat packets such as GCSay and GCWhisper. Both strings can therefore be 255 bytes. GCPartySayHandler.cpp:35-38 then declares `char str[128]; char strName[128];` and does `strcpy(str, pPacket->getMessage().c_str()); strcpy(strName, pPacket->getName().c_str());`. Note that SocketInputStream::read(std::string&,uint) truncates at the first embedded NUL (SocketInputStream.cpp:196-199), so an attacker simply sends 255 non-zero bytes to keep the full length. The declared packetSize cap (GCPartySay.h:96 returns 154) does not constrain the sub-reads, which draw from the shared stream buffer beyond the packet boundary.

**Failure scenario:** Server sends PACKET_GC_PARTY_SAY with a 255-byte non-NUL name and message. strcpy writes 256 bytes into a 128-byte stack array, smashing the saved return address / stack cookie region with attacker-controlled bytes.

**Recommendation:** Add explicit `szName > 20` / `szMessage > 128` checks in GCPartySay::read (matching GCSay.cpp:32 and GCWhisper.cpp:29,43), and replace the strcpy calls with a bounded copy or use std::string directly.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`). read() enforces the declared 20/128 layout like GCSay and GCWhisper, and the handler copies are snprintf-bounded. CGPartySay::write gained the matching 128 cap in `3bc340e` so this client cannot trip the new guard on other clients.

### C10. An unbounded server-supplied guild name is sprintf'd into a 128-byte stack buffer.

**Area:** Networking & Protocol  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCGuildChatHandler.cpp:58`

GCGuildChat::read (Client/Packet/Gpackets/GCGuildChat.cpp:27-29) reads `BYTE szGName` and does `if (szGName != 0) iStream.read(m_SendGuildName, szGName)` with no upper bound — the sender name (line 38) and message (line 51) are bounded, but the guild name is not. GCGuildChatHandler.cpp:57-58 then does `char szName[128]; sprintf(szName, "[%s]%s", pPacket->getSendGuildName().c_str(), pPacket->getSender().c_str());`. Worst case output is 255 (guild) + 10 (sender) + 3 (brackets) + NUL = 269 bytes into a 128-byte buffer.

**Failure scenario:** Server sends PACKET_GC_GUILD_CHAT with m_Type != 0 (union chat), a 255-byte guild name and a 10-byte sender. sprintf overflows szName by ~141 bytes of attacker-controlled data on the stack.

**Recommendation:** Bound szGName in GCGuildChat::read (the write path at line 71 should get a matching check), and switch the handler to snprintf or std::string concatenation.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`). The guild name is capped at 20 bytes -- the declared layout bound, matching the server repo's header -- on both read and write, and both handler copies are snprintf-bounded.

### C11. A 255-byte-capable server message is strcpy'd into 128-byte static buffers in two places.

**Area:** Networking & Protocol  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCSystemMessageHandler.cpp:100`

GCSystemMessage::read (Client/Packet/Gpackets/GCSystemMessage.cpp:28) guards with `if (szMessage > 256)`, but szMessage is a BYTE whose maximum value is 255 — the check can never fire, so messages up to 255 bytes are accepted. GCSystemMessageHandler.cpp:27 declares `static char previous1[128]` and line 100 does `strcpy(previous1, pPacket->getMessage().c_str())`; line 108 declares `static char previous[128]` and line 161 does `strcpy(previous, pPacket->getMessage().c_str())`. Both overflow by up to 128 bytes into adjacent static storage. Separately, line 119-120 allocates `new char[strlen(message)+20]` and sprintf's a format string pulled from the game string table into it, assuming the format never expands by more than 20 characters.

**Failure scenario:** Server sends PACKET_GC_SYSTEM_MESSAGE with type SYSTEM_MESSAGE_PLAYER and a 255-byte message. strcpy writes 256 bytes into previous1[128], corrupting 128 bytes of the .bss/.data segment following it.

**Recommendation:** Replace both static char arrays with std::string, or use a bounded copy. Also fix the dead `> 256` check to a real limit such as `> 127`.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`). Both statics are std::string; the dead guard is replaced by a comment (a 255-byte message is legal and every sink was verified bounded) and the `+20` allocation is sized from the format string. `3bc340e` additionally fixed the dangling `c_str()`-of-temporary pointers in the RANGER_CHAT and PLAYER cases of the same handler.

### C12. A filename received verbatim from a remote peer is used to create, truncate, rename and delete files on the local disk with no path validation.

**Area:** Networking & Protocol  |  **Category:** security  |  **Location:** `Client/RequestFileManager.cpp:91`

RCRequestedFileInfo::read (Client/Packet/Rpackets/RCRequestedFile.cpp:40-50) reads a filename off the wire; its only guard is `if (szFilename > 255)` on a BYTE, which is dead code. RCRequestedFileHandler.cpp passes `pFileInfo->getFilename().c_str()` straight into `new ReceiveFileInfo(...)`, which stores it as m_Filename (RequestFileManager.cpp:50). StartReceive then opens it for writing with ios::trunc at line 91, and EndReceive calls `rename(m_FilenameTemp, m_Filename)` at lines 129/137/153/160 and `remove(m_Filename)` / `remove(m_FilenameTemp)` at lines 135/143/158/165. No component of the path is sanitized — no rejection of '..', absolute paths, or drive letters. The peer here is another game client whose IP the client learned from GCRequestedIP, i.e. fully untrusted.

**Failure scenario:** A malicious peer answers a profile request with RCRequestedFile carrying filename "..\\..\\DarkEden.exe" (or any path under the user's profile). The client truncates and overwrites that file with peer-supplied bytes, then renames the temp file over it — arbitrary file write, and via remove(), arbitrary file deletion.

**Recommendation:** Reject any filename containing a path separator, '..', a drive letter or a leading separator, and force all received files into a fixed download subdirectory. Validate before constructing ReceiveFileInfo.

> ✅ **Fixed** in `76a1185`, reworked in `3bc340e`, and hardened again in `1200625` (branch `harden/network-input`). The blanket separator ban in the first attempt broke legitimate transfers (the wire value is the sender's relative profile path); the second attempt allowed interior separators but a second adversarial review showed it still blocked only escape, not scope -- a peer could write a benign relative name such as `zz.dll` beside the executable. The handler now ignores the peer's directory entirely: it keeps only the leaf name, requires a `.spk`/`.spki` extension (plus the Win32 device-name and trailing-dot checks), and re-roots the file under the client's own `DIR_PROFILE`, so the peer controls the filename but never the location. The reject path frees the transfer state before disconnecting.

### C13. MItem::NewItem indexes a static function-pointer table with an unvalidated item class taken directly from network packets, then calls through the result.

**Area:** Core Game Loop & State  |  **Category:** memory-safety  |  **Location:** `Client/MItem.cpp:325`

`return (MItem*)(*s_NewItemClassTable[itemClass])();` performs no range check. The table is declared `static FUNCTION_NEWITEMCLASS s_NewItemClassTable[MAX_ITEM_CLASS]` (Client/MItem.h:497) with MAX_ITEM_CLASS == 90 (Client/ItemClassDef.h:137). Callers pass the packet field straight through: Client/Packet/Gpackets/GCCreateItemHandler.cpp:69 `MItem::NewItem((enum ITEM_CLASS)pPacket->getItemClass())`, GCChangeShapeHandler.cpp:33, GCAddStoreItemHandler.cpp:59/141/179, GCMakeItemOKHandler.cpp:73, GCMyStoreInfoHandler.cpp:108. `getItemClass()` returns a raw BYTE (Client/Packet/Gpackets/GCCreateItem.h:52), so the attacker-controlled range is 0-255 against a 90-entry array. This is an out-of-bounds read of a code pointer followed by an indirect call through it.

**Failure scenario:** Server (or an on-path attacker) sends GCCreateItem with m_ItemClass = 200. NewItem reads s_NewItemClassTable[200], 880 bytes past the end of the array into whatever static data follows, and calls that address as a function. Depending on what lies there this is a crash or arbitrary code execution in the client process.

**Recommendation:** Add `if (itemClass < 0 || itemClass >= MAX_ITEM_CLASS || s_NewItemClassTable[itemClass] == NULL) return NULL;` at the top of NewItem, and audit every caller to handle a NULL return (several currently dereference it immediately).

> ✅ **Fixed** in `7b81ba4` (branch `harden/network-input`), with the VS_UI call sites the first audit missed guarded in `3bc340e`. NewItem validates the class and the table entry and returns NULL; every variable-class caller under Client/ and VS_UI/ handles the NULL return. Regression guard, executable-only code.

### C14. MCreature::SetChatString does an unbounded strcpy+strcat of a server-supplied chat string into a 21-byte heap buffer.

**Area:** Core Game Loop & State  |  **Category:** memory-safety  |  **Location:** `Client/MCreature.cpp:5016`

`strcpy(m_ChatString[m_ChatStringCurrent], "Dear. "); strcat(m_ChatString[m_ChatStringCurrent], str);` at lines 5016-5017. Each row of m_ChatString is allocated as `new char[g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1]` (Client/MCreature.cpp:678), and MAX_CHATSTRINGLENGTH_PLUS1 defaults to 21 (Client/ClientConfig.cpp:117). "Dear. " already consumes 6 bytes, so any `str` longer than 14 bytes overflows the heap block. `str` is the chat message: Client/Packet/Gpackets/GCSayHandler.cpp:206 passes a buffer filled by `strcpy(str, pPacket->getMessage().c_str())` at GCSayHandler.cpp:112. The path is gated on GetCreatureType()==482 (the Christmas-tree NPC), which is itself a server-controlled field.

**Failure scenario:** A creature whose type the server reports as 482 says a 60-character line. strcat writes 66 bytes into a 21-byte heap allocation, corrupting adjacent heap metadata and the neighbouring m_ChatString rows.

**Recommendation:** Replace with a length-checked copy bounded by MAX_CHATSTRINGLENGTH_PLUS1-1 (snprintf into the row), and apply the same treatment to the sibling sites at lines 5086, 5130-5133 and 4900.

> ✅ **Fixed** in `c3e9937` and `3bc340e` (branch `harden/network-input`). The "Dear." copy and the tree "From." block -- which the first pass missed -- are both snprintf-bounded to the row size.

### C15. The chat line-wrapping loop in SetChatString/SetPersnalString copies an entire newline-delimited segment into a 21-byte row without bounding it.

**Area:** Core Game Loop & State  |  **Category:** memory-safety  |  **Location:** `Client/MCreature.cpp:5099`

The loop normally cuts at `endIndex = startIndex + MAX_CHATSTRING_LENGTH` (20). But when a '\n' is present the code overrides that: `*find = '\0'; endIndex = strlen(str+startIndex)+startIndex;` (lines 5074-5075, and identically at 4890 in SetPersnalString). endIndex is then the offset of the newline, which is unbounded. Since that is still < len, control reaches the else branch and the manual byte loop at 5099-5102 copies `endIndex - startIndex` bytes into `m_ChatString[m_ChatStringCurrent]`, a 21-byte allocation. SetPersnalString's copy of this logic at 4886-4892 is not even gated on creature type — it runs for every personal-shop sign, fed from the network at Client/Packet/Gpackets/GCAddSlayerHandler.cpp:113 and GCAddOustersHandler.cpp:123/203 as `SetPersnalString((char*)pPacket->getStoreOutlook().getSign().c_str(), ...)`.

**Failure scenario:** Another player sets a personal-shop sign of "AAAA...(200 chars)...\nx". On receiving GCAddSlayer the client copies 200 bytes into a 21-byte heap row. Note the `(char*)...c_str()` cast also means `*find = '\0'` mutates the packet's std::string buffer in place.

**Recommendation:** Clamp endIndex to `startIndex + MAX_CHATSTRING_LENGTH` after the newline adjustment, and stop const_casting away c_str() — copy the packet string into a local bounded buffer first.

> ✅ **Fixed** in `c3e9937`, restructured in `3bc340e` (branch `harden/network-input`): the newline cut is only honoured when the line fits in a row, measured before the in-place `'\0'` write so an over-long line no longer mutates the buffer at all. The `(char*)c_str()` const_cast at the call sites remains.

### C16. Server-controlled guild name of up to 255 bytes is sprintf'd into a 128-byte stack buffer, with no length cap anywhere in the parse path.

**Area:** Text & Strings  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCGuildChatHandler.cpp:58`

`GCGuildChat::read()` (Client/Packet/Gpackets/GCGuildChat.cpp:27-29) reads a BYTE length `szGName` and then `iStream.read(m_SendGuildName, szGName)` with **no upper-bound check at all** — unlike `m_Sender` (capped at 10, line 38) and `m_Message` (capped at 128, line 50). The handler then does `char szName[128]; sprintf(szName, "[%s]%s", pPacket->getSendGuildName().c_str(), pPacket->getSender().c_str());`. Worst case output is 1 + 255 + 1 + 10 + 1 = 268 bytes into a 128-byte stack buffer.

**Failure scenario:** A malicious or compromised game server sends a GCGuildChat packet with m_Type != 0 and a 255-byte guild name. The handler writes 268 bytes into szName[128], smashing 140 bytes of stack including the saved return address — remote code execution in the client.

**Recommendation:** Add an upper-bound check on szGName in GCGuildChat::read() mirroring the szSender/szMessage checks, and replace the sprintf with snprintf(szName, sizeof(szName), ...).

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`) -- the same defect as C10; see its entry.

### C17. strcpy of a server message of up to 255 bytes into a static char[128]; the packet's length guard is dead code because the length field is a BYTE.

**Area:** Text & Strings  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCSystemMessageHandler.cpp:161`

`static char previous[128]` (line 108) and `static char previous1[128]` (line 27) are filled with `strcpy(previous, pPacket->getMessage().c_str())` at lines 161 and 100. `GCSystemMessage::read()` (Client/Packet/Gpackets/GCSystemMessage.cpp:21-31) declares `BYTE szMessage` and guards with `if (szMessage > 256) throw` — a BYTE can never exceed 255, so this comparison is always false and the guard is dead code. The message can therefore be 255 bytes.

**Failure scenario:** Server sends a GCSystemMessage with a 255-byte body. strcpy writes 256 bytes into the 128-byte static buffer, corrupting 128 bytes of adjacent .data/.bss — including whatever globals the linker placed next to it. Repeatable at will by the server.

**Recommendation:** Change the guard to a real bound (e.g. `> 127`) or, better, cap the copy: use a std::string member or snprintf/strncpy with explicit truncation. Audit the other packet read() methods for the same dead `BYTE > 256` pattern.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`) -- the same defect as C11; see its entry. The dead-guard audit of other read() methods has not been done.

### C18. MCreature::SetPersnalString copies an unbounded newline-delimited segment into a 21-byte heap row.

**Area:** Text & Strings  |  **Category:** memory-safety  |  **Location:** `Client/MCreature.cpp:4917`

`m_ChatString[i] = new char[g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1]` (MCreature.cpp:678), which defaults to 21 (ClientConfig.cpp:117). In SetPersnalString the loop sets `endIndex = startIndex + MAX_CHATSTRING_LENGTH` (20), but if the string contains a '\n' (MCreature.cpp:4885-4891) it *overwrites* endIndex with `strlen(str+startIndex)+startIndex` — the distance to that newline, which is unbounded. Because `*find = '\0'` was already written while `len` still holds the original full length, `endIndex >= len` is false whenever the newline is not the last character, so control falls into the else branch and the manual copy loop at lines 4911-4917 writes `endIndex - startIndex` bytes into the 21-byte row. `SetChatString` has the identical pattern at MCreature.cpp:5066-5100, gated only on `GetCreatureType() == 482 || 650`, which is itself server-supplied.

**Failure scenario:** A personal-shop message or creature chat string of the form "AAAA...(200 bytes)...\nB" reaches SetPersnalString. endIndex becomes 200, the copy loop writes 200 bytes plus a NUL into a 21-byte heap allocation, corrupting the heap.

**Recommendation:** Clamp endIndex to startIndex + MAX_CHATSTRING_LENGTH after the newline handling, and recompute `len` after the in-place '\0' write. Also validate at load time that MAX_CHATSTRINGLENGTH_PLUS1 == MAX_CHATSTRING_LENGTH + 1 — both are read independently from the config file (ClientConfig.cpp:505-507) with no consistency check.

> ✅ **Fixed** in `c3e9937` and `3bc340e` (branch `harden/network-input`) -- the same wrap-loop defect as C15; see its entry. ClientConfig now clamps the primaries to sane geometry and recomputes the derived +1/-1 values instead of trusting the record.

### C19. Every printf format string in the client comes from a binary data file, and is passed to sprintf/vsprintf as the format argument in roughly 600 places.

**Area:** Text & Strings  |  **Category:** security  |  **Location:** `Client/GameInit.cpp:1604`

`(*g_pGameStringTable).LoadFromFile(gameStringTableTable2)` replaces the entire string table with contents read from the file named by `FILE_INFO_STRING`. Those entries are then used directly as the format argument: e.g. `sprintf(szBuf, (*g_pGameStringTable)[UI_STRING_NOTICE_EVENT_GOLD_MEDALS].GetString(), pPacket->getParameter())` (Client/Packet/Gpackets/GCNoticeEventHandler.cpp:295), `sprintf(pMsg, (*g_pGameStringTable)[UI_STRING_MESSAGE_SYSTEM].GetString(), message)` (GCSystemMessageHandler.cpp:120), and 83 call sites of the zero-argument form `wsprintf(sz_buf, (*g_pGameStringTable)[X].GetString());` (e.g. VS_UI/src/VS_UI_Description.cpp:271-310 into `char sz_buf[50]` declared at line 215). The compile-time literals in MGameStringTable.cpp are irrelevant here — see the separate finding that InitGameStringTable() is never called.

**Failure scenario:** Anyone who can write the Strings data file (a malicious patch/CDN, a repacked client distribution, or simply a user tricked into installing modified game data) puts "%n" or a long "%s" run into a table entry. sprintf then performs an arbitrary write or dereferences stack garbage as a char*. The zero-argument `wsprintf(buf, tableString)` form is exploitable with any conversion specifier at all, since there are no varargs to consume.

**Recommendation:** Never pass externally-sourced strings as the format argument. Introduce a checked formatter that validates the specifier sequence of a table entry against the argument types at the call site (or at minimum a table-load-time validator rejecting %n and counting/typing specifiers), and convert the zero-argument cases to snprintf(buf, size, "%s", tableString).

> ✅ **Fixed** across five slices of `docs/RESTRUCTURING.md` task 5.4 (`ef81654`, `5918ce4`, `e76e1db`, `b375b78`, and the fifth on branch `harden/format-indirect`). **315 call sites** are converted to the checked formatter (321 when it closed; the eleventh clocks slice, 2026-09-17, deleted a dead quest caption in `MTopView` holding six).
>
> **Read the evidence, not the zero.** This entry was marked Fixed once before, on 2026-09-04, on the strength of ratchet R7 reaching 0 — and that was wrong, because R7 only counts a table lookup *spelled at the format argument*. 24 live sites reached a format indirectly, through a static array or a local the entry had been copied into first, and R7 cannot see any of them. So the claim here rests on five measurements rather than one, each named so it can be re-run and disagreed with:
>
> 1. **R7 = 0** — no table lookup spelled at a format argument, across the `sprintf` family (now including `fprintf`), the counted family, `AddFormat`, the offset-append form, and `.Format`.
> 2. **R8 = 43** — the new ratchet, which asks the question R7 cannot: at a printf-family call, is the format a string literal? 43 calls across `Client`, `VS_UI` and `basic`, headers included, take a format that is not, and all 43 were read: 28 vararg forwarders where the format is the function's own parameter, 6 inside `SafeFormat`'s own `Emit` (where the format is a specification it built and validated itself), 3 literals behind `TEXT()`/`_T()`, and 6 declarations rather than calls. **This first read 13**, and 13 was not the population — the review round found that R8 was missing the `AddFormat` family, bare `printf`, `basic/` and every header. Nothing in the tree changed between the two numbers; the conclusion held, but it had not been measured over the population it claimed.
> 3. **The forwarders' own callers**, swept for a non-literal first argument: **zero**.
> 4. **The family list came from the tree, not from memory** — every identifier ending in `printf` in `Client`, `VS_UI` and `basic`. That is how `fprintf` (550 calls) and `vswprintf` got into both ratchets, neither of which the first draft of R8 listed. The enumeration command itself had to be fixed: as first written it required an identifier character *before* `printf` and so could never have found the 86 bare `printf(` calls it was offered as the authority for.
> 5. **Parenthesised destinations**, which both ratchets skip so a joined stream cannot run across two statements: 16 sites, every one with a literal format.
>
> What is still blind, written down rather than left to be discovered: a printf behind a macro; a destination expression containing parentheses, which both patterns exclude so a joined stream cannot run across two statements; the comment stripping in both ratchets, which is not string-literal aware; and a *new* printf-shaped method other than `MString::Format`. A finding in any of those falsifies this entry, and the way to look is a sweep over the format argument — never another spelling of the lookup. The review round of the fifth slice found three such holes at once, so treat that list as the ones known today rather than the ones there are.
>
> **The fifth slice (2026-09-04) took the 24.** Twenty-one in `VS_UI/src/VS_UI_ExtraDialog.cpp` now go through `AllocAskMessage`, which allocates and formats in one place so the bound cannot drift from the destination; three in `VS_UI_GameCommon.cpp` take the array overload directly. Three more that no sweep in the first four slices could have found were caught while checking R8's own blind spots: `Client/GameUI.cpp` passed a table entry to `MString::Format`, an ordinary varargs printf reached as a *method*. `MString` is in `gamemodel`, so its checked sibling `MString::FormatChecked` is library code with **five tests** — the first part of this finding's fix that a test binary can reach at the call-site end rather than only inside the formatter.
>
> The worst of them, and what it looked like **before** that slice — `VS_UI/src/VS_UI_ExtraDialog.cpp` filled `m_sz_question_msg[][]` from the table in `InitString()` and then did
>
> ```cpp
> m_sz_question_msg_temp[0] = new char [strlen(m_sz_question_msg[type][0])+1];
> sprintf(m_sz_question_msg_temp[0], m_sz_question_msg[type][0]);
> ```
>
> — a data-file format, **no varargs at all**, into a heap block sized from the format string. A `%s` in that entry read a stack word as a `char*` and copied it, unbounded, into a buffer sized for something else. 21 sites in that file, 12 of them passing nothing; 3 more in `VS_UI_GameCommon.cpp` (`info_vampire_title_string[num]`, `info_ousters_title_string[num]`, `chingho_name[0]`); and one in `Client/PacketHandler/GCSystemMessageHandler.cpp`, which assigned the entry to a local named `pFormat` first — in a directory the first slice reported finished. That last one went with the fourth slice, which is what makes the claim about `Client/PacketHandler` true rather than merely unmeasured; the rest went with the fifth.
>
> **What a conversion buys**, at every one of the 315: a conversion with no argument left, an argument of the wrong type, a `%n`, or an argument-supplied width is printed as text rather than performed, and every destination is bounded. Neither half of the defence can tell that an entry *means* what its call site passes — a mistranslated entry still prints wrongly. What is gone is printing something the caller never owned.
>
> Two further limits: `tests/tools/check_format_arity.pl` audits arity against the built-in English table — what the default build formats with, since `InitGameStringTable` installs it over the file data — but no `String.inf` ships here, so `LANGUAGE != 3` entries cannot be checked at all and rest on the run-time refusal. And of 384 conversion positions only 120 get a *type* comparison, because an argument whose kind cannot be told is deliberately not a failure. (Those two numbers were carried over as "386" and "117" under the word *unchanged* when the fifth slice added seven sites that moved both.)
>
> The five slices and their review rounds are in `docs/RESTRUCTURING.md` task 5.4. What follows is the original narrowing, kept because it is what the numbers above were measured against.
>
> ⚠ **Narrowed, not fixed**, in `31f5f2f` (branch `harden/text-format`). All 100 zero-argument call sites — the exploitable half, where a bare `%s` reads a stack word as a `char*` with no varargs to consume — are now `snprintf(dest, sizeof(dest), "%s", GetGameString(id))`: bounded, and the data is an argument rather than a format. `GetGameString` also range-clamps the id and answers `""` for one it cannot resolve, which matters because `CTypeTable::operator[]` returns a default-constructed `MString` out of range and that `MString`'s `GetString()` is NULL. (This sentence originally said the bounds check itself compiled away outside Debug. That was already false when written: `e65ab7a`, the same day, made all three `CTypeTable` accessors check in every build. Corrected 2026-09-03 after the same stale claim was carried into the task 5.4 work and caught there.) A load-time gate (`SanitizeGameStringTable`) rejects entries carrying `%n`, an argument-supplied width, a width or precision of 32 or more, a whole-entry width budget over 256, a floating-point conversion, or a wide conversion that would retype a `char*` as `wchar_t*`. **The ~140 call sites that pass arguments still take their format from `String.inf`** and are covered only by that gate, which cannot check arity. Note also that the gate has no effect in the default English build, where `InitGameStringTable` reallocates the table from source literals immediately after and discards the scrubbed data; it protects `LANGUAGE != 3`.
>
> **Second pass, 2026-09-03** (branch `harden/checked-format`, `docs/RESTRUCTURING.md` task 5.4). The argument-passing half now has a fix rather than a gate, and a number rather than an estimate. The remaining population is **293 call sites**, counted by ratchet **R7**; "~140" and the "roughly 600" in this finding's title were both estimates, and the second was of a different thing (all `sprintf` in the client).
>
> The fix is `basic/SafeFormat.h` — a formatter that checks the entry's conversions against the arguments the call site really passed, which is possible at the call site and impossible at load time. A conversion consumes the next argument only if that argument's type can satisfy it; one with no argument left, an argument of the wrong type, a `%n`, or an argument-supplied width is copied out as text instead of performed, and the destination is bounded by its own size. This is the first fix for this finding with a test path: **39 tests in `tests/unit/test_safe_format.cpp`**, where everything before it was a build-verified regression guard.
>
> **All 293 are converted** — every `sprintf`/`wsprintf` in `Client/PacketHandler` fed by the table. `GCBloodBibleListHandler` also `sprintf`ed a `char[192]` plus a `"%3d "` prefix into another `char[192]`, four bytes short, which the bounded formatter closes. Its table lookups are indexed by a packet field and go through `GetGameString()` now — not because the index was unchecked, as the first draft of this note claimed, but because an out-of-range `CTypeTable` lookup yields a default-constructed `MString` whose `GetString()` is NULL, and a NULL reaching a `%s` is undefined rather than empty. The second slice took all 198 `VS_UI` sites the same way, a third took the 27 `AddFormat` sites through a new `CMessageArray::AddSafeFormat`, a fourth took the last 37 ordinary `sprintf` sites and drove R7 to 0, and a fifth took the 24 R7 cannot see plus 3 more it found while auditing its own blind spots. The slices are audited by `tests/tools/check_format_arity.pl`, a ctest that compares every converted site's arguments against the built-in English table and fails when an entry asks for more than the call site passes. It reports **295 sites, 283 checked, 0 failures** (301 and 289 before the caption deletion) — a smaller number than the 315 converted call sites, and deliberately not the same measurement: it counts text, and `AllocAskMessage` is one textual site standing in for twenty-one dialog rows. The sites it cannot check index the table with a computed id or take the entry as a parameter. That audit is a floor on coverage, not the closure argument.

### C20. CMessageArray::AddFormat and AddFormatVL vsprintf into a fixed static 4096-byte buffer with a data-file-supplied format string.

**Area:** Text & Strings  |  **Category:** memory-safety  |  **Location:** `Client/CMessageArray.cpp:333`

Both `AddFormat` (line 320) and `AddFormatVL` (line 259) declare `static char Buffer[4096]` and call `vsprintf(Buffer, format, vl)` with no bound. Callers pass table strings as the format, e.g. `g_pSystemMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_PET_DIE_WARNING].GetString(), petName.c_str(), szTemp)` (Client/ModifyStatusManager.cpp:1314) and `g_pNoticeMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_EVENT_FLAG_WAR_WINNER].GetString(), ...)` (GCNoticeEventHandler.cpp:211). The buffer is also `static`, so it is shared across all CMessageArray instances and is not reentrancy- or thread-safe despite the __BEGIN_LOCK/__END_LOCK macros expanding to `((void)0)` in every non-OUTPUT_DEBUG build (lines 51-52).

**Failure scenario:** A table entry with a wide field width ("%9000d") or a long %s substitution overflows the 4096-byte static buffer, corrupting adjacent globals. Because the same buffer backs every message array, a nested AddFormat (e.g. from a logging path) silently clobbers an in-flight message.

**Recommendation:** Replace vsprintf with vsnprintf(Buffer, sizeof(Buffer), ...), make the buffer a local (or per-instance) rather than static, and route the format string through the validated-format path from the previous finding.

> ✅ **Fixed** in `0e9d247` (branch `harden/text-format`). Both functions use vsnprintf into a function-local buffer, and the return value is captured so that the negative encoding-error path — where the contents are unspecified — empties the row instead of letting strlen walk uninitialised stack into the log file and the visible chat row. Regression guard, executable-only code.

### C21. An MItem* is passed through a 32-bit int parameter and dereferenced on the other side, guaranteeing a wild-pointer dereference on 64-bit builds.

**Area:** UI Framework  |  **Category:** memory-safety  |  **Location:** `VS_UI/src/VS_UI_GameCommon.cpp:2887`

`g_descriptor_manager.Set(DID_ITEM, ..., (void *)p_selected_item, 0, (int)(intptr_t)pAddedItem)` squeezes an `MItem*` into the `long right` parameter of `DescriptorManager::Set` (VS_UI/src/VS_UI_Descriptor.cpp:47). `_Item_Description_Show` recovers it with `MItem* p_AddItem = (MItem*)right;` (VS_UI/src/VS_UI_Description.cpp:66) and dereferences it at VS_UI/src/VS_UI_Description.cpp:1109 (`p_AddItem->GetItemClass()`) and :1116. On MSVC x64 (LLP64) `long`/`int` are 32 bits, so the upper half of the heap pointer is discarded and then sign-extended back. The explicit `(intptr_t)` cast that was added only silences the truncation warning; it does not preserve the value. The repo's recent commits target a VS2022 build, whose default platform is x64.

**Failure scenario:** Player hovers a gear slot that holds a Core Zap item while a compatible item is equipped. `pAddedItem` (a heap MItem*, e.g. 0x000001F4_A2B10040) is truncated to 0xA2B10040, stored in `right`, sign-extended to 0xFFFFFFFFA2B10040 and dereferenced in the tooltip renderer -> access violation on the next frame, every time.

**Recommendation:** Change `DescriptorManager::Set`/`RectCalculationFinished`/the `fp_show` signature to take a typed second payload pointer (e.g. `void* void_ptr2`) instead of overloading the `long right` field, and update the DID_ITEM path. Audit the other `Set(...)` call sites for the same pattern.

> ✅ **Fixed** in `c0df91c` (branch `harden/pointer-truncation`). `DescriptorManager` gained a typed secondary payload pointer and an accessor, passed as a trailing defaulted parameter on `Set()` so all 201 existing call sites compile untouched and the shared `fp_show`/`fp_rect_calculator` typedefs — implemented by dozens of unrelated renderers — stay as they are. The audit the recommendation asks for found this to be the only one of those 201 sites putting a pointer in `left`/`right`, but **two** consumers rather than the one named: `_Item_Description_Calculator` read the same truncated pointer to size the tooltip, so fixing only `_Item_Description_Show` would have left the crash and desynchronised the box from its contents. `Unset()` now tests both payloads — it is the item-destruction hook, and the gear tooltip is the one place where the two payloads are different objects, so matching only the primary left the secondary dangling for the next `Show()`; that use-after-free was found by the adversarial review, not by the original pass. Regression guard; the crash was not reproduced.
>
> Recorded latent: the secondary pointer lives outside `FP_SHOW_PARAM`, so it stays consistent with the descriptor being drawn only because `Set()` early-returns while one is showing. That guard is now commented as load-bearing. Anyone relaxing it — letting a new hover replace a live descriptor is the obvious future change — must move the pointer into `FP_SHOW_PARAM` at the same time.

### C22. A runtime-built string containing server-supplied quest text is used as the format string of sprintf, giving a remote format-string vulnerability.

**Area:** UI Framework  |  **Category:** security  |  **Location:** `VS_UI/src/vs_ui_gamecommon2.cpp:15911`

Line 15893 builds `szString` with `sprintf(szString, <table format>, i+1, TempInfo->szMissionTitle.c_str())` — `szMissionTitle` comes from the guild-quest packet. Line 15911 then does `sprintf(szString2, szString, TempInfo->m_StrArg.c_str(), TempValue);`, i.e. the string that just embedded server text is itself the format string. Any `%` sequence in the mission title is interpreted as a conversion specifier. `szString`/`szString2` are `char[512]` stack buffers (lines 15884-15885), so `%s`/`%n`/width specifiers give stack disclosure, out-of-bounds reads, and on non-hardened CRTs arbitrary writes via `%n`.

**Failure scenario:** A malicious or compromised server sends a guild mission whose title is `%s%s%s%s%n`. Opening the quest window makes the client walk arbitrary stack words as char* and then write through one of them.

**Recommendation:** Never pass runtime data as a format string. Split the two-stage substitution: format the fixed table template once with all arguments, or use a placeholder-replacement helper (`std::string` find/replace) for the mission title and arg. Apply the same rule to the ~230 `sprintf(buf, (*g_pGameStringTable)[...].GetString(), ...)` sites, whose format strings come from an on-disk data file.

> ✅ **Fixed** in `31f5f2f` (branch `harden/text-format`). The server-supplied mission title is expanded by a bounded helper that substitutes only `%s` and `%d` and copies every other byte — a second `%s`, `%n`, a width form — literally, so no conversion in packet text reaches a printf. The remaining format on that path is the table entry itself, which is C19's problem rather than this one. The ~230 argument-passing table sites are **not** fixed; see the C19 entry for what was and was not done there.

### C23. The SDL text-input event pointer is passed through a `long`, truncating it on 64-bit Windows and crashing on every keystroke into a text field.

**Area:** UI Framework  |  **Category:** memory-safety  |  **Location:** `Client/CWaitUIUpdate.cpp:188`

`gC_vs_ui.KeyboardControl(WM_TEXTINPUT, 0, (long)text)` casts a `const char*` to `long`. `C_VS_UI::KeyboardControl(UINT, UINT, long extra)` (VS_UI/src/header/VS_UI.h:238) forwards it to `WindowManager::KeyboardControl` (VS_UI/src/widget/u_window.cpp:1424-1429), and the receiving editors cast it back: `const char* text = (const char*)extra;` at VS_UI/src/widget/U_edit.cpp:301 and VS_UI/src/VS_UI_Title.cpp:4648. On MSVC x64 `long` is 32 bits, so the SDL event buffer pointer loses its high half. On macOS/Linux LP64 `long` is 64 bits and the same code happens to work, which is why the bug is invisible in the platform the port was developed on.

**Failure scenario:** On the Windows x64 build, typing any character with a chat box or the login ID field focused delivers a truncated pointer to `LineEditor::HandleTextInput`, which then runs `utf8_to_utf32` over an unmapped address -> access violation.

**Recommendation:** Widen the `extra` parameter to `intptr_t`/`LPARAM` through `C_VS_UI::KeyboardControl`, `WindowManager::KeyboardControl`, `Window::KeyboardControl` and `LineEditor::KeyboardControl`, or (better) route text input exclusively through `InputFocusManager::HandleTextInput(const char*)`, which already takes a proper pointer and is used by the SDL backend.

> ✅ **Fixed by removal** in `2a531a9` (branch `harden/pointer-truncation`), and **the severity stated above is wrong**. Text entry works in this build. `g_textinput_callback` and `g_textediting_callback` have exactly four references in the tree — two definitions and two assignments — and are never invoked, so the callback that would have reached the truncating sender was dead, and with it the only site in the client that constructs a `WM_TEXTINPUT` message. The live path is the one the recommendation prefers: the SDL backend calls `InputFocusManager::HandleTextInput()` directly and the pointer stays a `const char*` all the way to the focused `LineEditor`. The dead plumbing, the three callback implementations and the `WM_TEXTINPUT` receivers that recover a pointer from `extra` are therefore deleted rather than repaired — a truncating sender wired to a callback nobody calls is a landmine for whoever reconnects it, and the receivers cannot be made safe while the parameter is a `long`. Widening `extra` across ~90 declarations was deliberately not done: a missed override silently becomes an overload that never fires, which is a worse failure than the one being fixed, and nothing is left to carry.

### C24. _Multiline_Info_Show writes a NUL terminator at a fixed offset into the caller's buffer before checking that the remaining string is that long, walking past the end of the buffer.

**Area:** UI Framework  |  **Category:** memory-safety  |  **Location:** `VS_UI/src/VS_UI_Description.cpp:3696`

In the loop at lines 3688-3708, `char_temp = cur[CurrentPos - check]; cur[CurrentPos - check] = '\0';` executes before the `if(strlen(cur) < CurrentPos-check) break;` guard on line 3702. `cur` advances by ~`CurrentPos` (36) bytes per iteration and the loop runs `strlen/right + 1` times, so the final iteration writes at an offset beyond the string's end. The only caller is VS_UI/src/vs_ui_gamecommon2.cpp:15919 passing `szMissionPopupString`, a `static char[512]` (line 15853), with `right=36`. Line 3686 additionally does `strcpy(sz_temp, cur)` into a `char sz_temp[4048]` (line 3682) that is never read afterwards — a dead, unbounded copy. The function also mutates the caller's buffer in place while notionally just rendering it.

**Failure scenario:** A quest mission line of ~500 characters gives LineCount=14; `cur` reaches offset 504 and the code writes `cur[36]` = byte 540 of a 512-byte static buffer, corrupting whatever follows it in .bss, then reads it back into `char_temp` and restores it at line 3705.

**Recommendation:** Move the length check before the write, bound the loop by the actual remaining length rather than a precomputed LineCount, delete the dead `sz_temp` copy, and make the function take a `const char*` plus its buffer size instead of mutating the input.

> ✅ **Fixed** in `31f5f2f` (branch `harden/text-format`). The loop checks the remaining length before writing the terminator and is bounded by the string rather than by a precomputed count, so the NUL always lands on a real character; the dead `sz_temp` copy is gone. The signature still mutates in place — restoring each byte before the next line — because the caller set is fixed. The rewrite initially introduced a one-line overdraw, since `_Multiline_Info_Calculator` beside it still sized the box with `strlen/right + 1` while the renderer consumes one byte less per line whenever a multi-byte character straddles the cut; the calculator now runs the same walk, so the two agree by construction. That sibling's unconditional 36-byte `memcpy` out of the caller's string is bounded as well (latent — the only caller guarantees the length).

### C25. An inventory item is deleted without clearing the tooltip descriptor that may still hold its raw pointer, leaving a use-after-free the renderer dereferences next frame.

**Area:** UI Framework  |  **Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCDeleteInventoryItemHandler.cpp:64`

`DescriptorManager` stores the hovered item as an untyped `void* m_fp_show_param.void_ptr` (VS_UI/src/VS_UI_Descriptor.cpp:144) and dereferences it every frame in `Show()` (VS_UI/src/VS_UI_Descriptor.cpp:108), which reaches `_Item_Description_Show`'s `MItem* p_item = (MItem*)void_ptr;` (VS_UI/src/VS_UI_Description.cpp:61) and immediately calls `p_item->GetName()` (line 217). Invalidation is manual and opt-in via `UI_RemoveDescriptor` (Client/GameUI.cpp:632). Sibling handlers do call it — GCShopSellOKHandler.cpp:70/83/114/169, GCRemoveFromGearHandler.cpp:143/258/370, GCReloadOKHandler.cpp:155 — but GCDeleteInventoryItemHandler.cpp:64 does `delete pItem;` with no such call. There are ~260 `g_descriptor_manager.Set(...)` sites against ~15 `Unset` sites overall.

**Failure scenario:** Player hovers an inventory item (tooltip shown) at the moment the server sends GCDeleteInventoryItem for that item (consumed by a timer, removed by a GM, quest turn-in). The MItem is freed; the next `DescriptorManager::Show()` calls `GetName()` on freed memory -> crash or reads from a reallocated object.

**Recommendation:** Add `UI_RemoveDescriptor((void*)pItem);` before the `delete` here, and audit every `delete`/`SAFE_DELETE` of an MItem. Longer term, have DescriptorManager hold the item ID rather than a raw pointer, and re-resolve it in `Show()`.

> ✅ **Fixed** in `f0b8ae6` (branch `harden/network-input`) for this handler, matching the shop/gear/reload handlers. The broader lifetime requirement is now covered by the 2026-09-22 item-destruction follow-up under C25; ID-only resolution is superseded by exact-object invalidation. The original handler fix was a regression guard, not a reproduced timing window.

> ✅ **Deletion-lifetime residual closed (2026-09-22):** `MItem`'s virtual destructor now notifies its host, and the executable invalidates both primary and secondary tooltip pointers through the existing `UI_RemoveDescriptor` path. This covers direct deletes, container release and stack lifetimes without requiring each caller to remember UI cleanup. Shutdown releases the UI and item owners before clearing the host. ID-only resolution is superseded by destruction invalidation: locally created magazines can share ID zero, while destruction identifies the exact object. `test_item_lifetime.cpp` guards stack/base-pointer destruction, container cleanup, address reuse and absent hosts; these are destruction-contract regression guards, not a reproduction of the original rendering timing window. The executable's UI wiring is checked by source audit and full builds.

### C26. AddFormat/AddFormatVL run unbounded vsprintf into a fixed 4096-byte static buffer, with the format string loaded from a data file and the arguments supplied by the server.

**Area:** Foundation Libraries  |  **Category:** memory-safety  |  **Location:** `Client/CMessageArray.cpp:333`

Client/CMessageArray.cpp:330-333 declares `static char Buffer[4096];` and calls `vsprintf(Buffer, format, vl);` with no length limit. AddFormatVL does the same at lines 265-268. The callers make this remotely reachable: Client/Packet/Gpackets/GCPartyInviteHandler.cpp:242 calls `g_pGameMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_SOMEONE_JOINED_PARTY].GetString(), pCreature->GetName())` and Client/ModifyStatusManager.cpp:1314 passes `petName.c_str()` — the format string comes from the on-disk game string table while the %s arguments come off the wire. Two distinct problems compound: (a) a long server-supplied name overruns the 4096-byte static buffer, corrupting adjacent .bss globals; (b) because the format string is data, a string-table entry whose conversion specifiers do not match the arguments actually passed (e.g. an extra %s or %n) is a classic format-string bug reading arbitrary stack. `static` also makes both functions non-reentrant.

**Failure scenario:** A server (or a modified Data/Info string table) sends a party-join notification whose creature name is longer than ~4KB, or a string-table entry gains an extra %s relative to the single argument GCPartyInviteHandler passes. vsprintf writes past Buffer[4095] into neighbouring globals, or dereferences a garbage stack value as a char*, crashing or corrupting state.

**Recommendation:** Replace both vsprintf calls with vsnprintf(Buffer, sizeof(Buffer), ...) and make Buffer a local (or per-instance) rather than static. Longer term, stop using data-file strings as printf format strings — validate the specifier list at load time, or switch to an indexed substitution scheme.

> ✅ **Fixed** in `0e9d247` (branch `harden/text-format`), the same change as C20 — this is the Foundation Libraries reviewer's report of the same two functions. The load-time specifier validation the "longer term" note asks for is the C19 gate, added in `31f5f2f`.

### C27. MemoryPool allocates its blocks with ::operator new but releases them with free(), which is undefined behaviour.

**Area:** Foundation Libraries  |  **Category:** memory-safety  |  **Location:** `Client/MemoryPool.cpp:50`

Client/MemoryPool.cpp:72 allocates each pool chunk via `CBlock *pPool = (CBlock*)( ::operator new( sizeof(CBlock) + ( m_BlockSize * m_BlockCount )) );` while the destructor at Client/MemoryPool.cpp:47-53 releases the same chunks with `free( m_pCurrentBlock );`. Mixing the C++ allocation function with the C deallocator is undefined; on MSVC the two can route through different heaps/bookkeeping. This is live code: g_CreatureMemoryPool, g_CreatureWearMemoryPool, g_NPCCreatureMemoryPool and g_FakeCreatureMemoryPool (Client/MemoryPool.cpp:30-33) are namespace-scope objects whose destructors run at process exit, and they back operator new for MCreature (Client/MCreature.h:115-123), MCreatureWear, MNPC and MFakeCreature. Secondary defect in the same function: `if( pPool == NULL ) return NULL;` at line 74 is dead, because ::operator new throws std::bad_alloc rather than returning null, so out-of-memory escapes as an uncaught exception.

**Failure scenario:** On shutdown, ~MemoryPool passes an ::operator new pointer to free(). Under a debug CRT or a heap with allocator-specific headers this trips a heap-validation assertion or corrupts the heap; under ASan it is reported as alloc-dealloc-mismatch.

**Recommendation:** Use ::operator delete(m_pCurrentBlock) in the destructor to match the allocation, and drop the dead NULL check (or use the nothrow form of operator new if the null path is wanted).

> ✅ **Fixed** in `8b349e7` (branch `harden/text-format`). The destructor — the class's only release path, since `Free()` merely pushes onto an intrusive free list inside the chunks — now uses `::operator delete`, and the dead NULL check is dropped rather than switched to the nothrow form: these pools back a throwing `operator new`, which must not hand a null block to a new-expression. Regression guard.

### C28. The legacy anti-cheat probe runs every frame, reads the wrong byte, and can terminate the process at random.

**Area:** Build, Portability & Hygiene  |  **Category:** correctness  |  **Location:** `Client/APICheck.cpp:126`

`_APICheck.CheckApi()` is called from inside the `while (TRUE)` message loop at Client/Client.cpp:4155 (initialised at Client.cpp:3996), so this whole routine executes once per frame on Windows. Line 126 does `g_ppProceAddress[i] = (DWORD)GetProcAddress(LoadLibrary(g_szCheckDLL[i*2]), ...)` for winmm/User32/kernel32. Three problems compound: (1) `g_ppProceAddress` is declared `DWORD[3]` (APICheck.h:42), so a 64-bit `FARPROC` is truncated to 32 bits on x64 — the stored value is not the address at all; (2) line 127 `memcpy(&code, &g_ppProceAddress[i], 1)` copies the low byte of the *stored address value*, not the opcode byte at that address, so the intended hook detection at line 128 (`code == 0xB9 || code == 0xE9`) can never detect anything; (3) when that low byte does happen to equal 0xB9 or 0xE9, line 130 calls `::ExitProcess(0)` — the game vanishes at startup with no message, no log, and no reproducibility, since the value depends on where the OS loaded the DLL. Additionally each frame calls `LoadLibrary` three times with no matching `FreeLibrary`, taking the loader lock 180x/second and leaking module reference counts for the life of the process. Lines 98-113 compile a hardcoded 37-byte x86 opcode signature of the 32-bit `send` prologue that cannot match on x64, and on a false match pops `MessageBox(0, "", "", MB_OK)` (an empty dialog) before `ExitProcess(0)`.

**Recommendation:** Delete APICheck entirely — it is 2006-era WPE-blocking security theatre that cannot work on x64 and provides no protection against any modern tool. Remove `Client/APICheck.cpp`/`.h`, the `APICheck _APICheck;` global at Client/Client.cpp:6, and the call sites at Client.cpp:3996 and 4155. If some form of tamper check is genuinely wanted later, it belongs behind an explicit opt-in build option, not unconditionally in the frame loop.

> ✅ **Fixed** in `c2f65b7` (branch `harden/text-format`). Both sources, the global, and both call sites are gone, along with the stale entries in the committed legacy `Client.vcxproj.filters`. `APICheck.h` declared nothing but the class — no macro or typedef anything else used — so nothing had to be rehomed. Because `CMakeLists.txt` globs sources without `CONFIGURE_DEPENDS`, an existing build tree keeps compiling the deleted file until it is reconfigured.

---

## Detailed Findings by Area

## Input, Audio & Media

**Grade:** D  |  **Findings:** 22

**Scope:** Input, audio, media — Client/DXLib (CDirectInput/CDirectSound/CDirectSoundStream/CDirectMusic SDL backends, Huffman/MP3 decoder), CMP3.cpp, COGGSTREAM.CPP, CAvi.cpp, CSoundPartManager and the Client-side sound/music/streaming code

**Health assessment:** This area is the least-finished part of the SDL migration and is effectively non-functional today: the SDL_mixer backend in DXLibBackendSDL.cpp is gated behind `#ifdef SDL_MIXER_MAJOR_VERSION`, a macro that only SDL_mixer.h itself defines, so every sound/music entry point compiles to the "not available" stub; the three `*_Adapter.cpp` files that would replace the stubs cannot compile at all (constructors still named after the pre-rename classes, duplicate global definitions); and even if both were fixed, nothing on Windows ever calls SDL_Init(SDL_INIT_AUDIO), so Mix_OpenAudio would fail. Underneath that dead scaffolding sit real defects that will bite the moment audio is turned on — most seriously a duplicate-buffer design in which `dxlib_sound_duplicate()` shares a `Mix_Chunk*` that `dxlib_sound_free()` unconditionally frees, giving a guaranteed double-free/use-after-free on the per-frame `ReleaseTerminatedDuplicateBuffer()` path. Live, currently-reachable code is also affected: three data-driven unbounded `strcpy` calls into 256-byte stack buffers in PlaySound, an unguarded NULL dereference in MZoneSoundManager, a modal error box on every entry to the opening screen, and a CMake source list that cannot configure on a case-sensitive filesystem. Around 180 KB of MP3/Huffman decoder source exists in two duplicated copies that no CMakeLists actually compiles, while build comments assert the opposite.

#### 🔴 Critical -- dxlib_sound_duplicate() copies the Mix_Chunk pointer instead of the chunk, and dxlib_sound_free() frees it unconditionally, so freeing any duplicate destroys the original's audio data.

**Category:** memory-safety  |  **Location:** `Client/DXLib/DXLibBackendSDL.cpp:692`

`dxlib_sound_duplicate()` (DXLibBackendSDL.cpp:685-699) allocates a new `dxlib_sound_buffer` and sets `duplicate->chunk = sound->chunk;` (line 692) — both wrappers now own the same `Mix_Chunk*`. `dxlib_sound_free()` (line 617-626) then calls `Mix_FreeChunk(sound->chunk)` with no reference counting. The duplicate path is exercised constantly: `CSDLAudio::Play(buffer, loop, bDuplicate)` calls `DuplicateSoundBuffer(buffer, true)` (CDirectSound_Adapter.cpp:221) whenever the sound is already playing, GameMain.cpp:3714/3761 pass `g_bGoodFPS` as bDuplicate, and `CGameUpdate.cpp:5996` calls `g_SDLAudio.ReleaseTerminatedDuplicateBuffer()` every frame, which does `dxlib_sound_free(wrapper->sound); delete wrapper;` (CDirectSound_Adapter.cpp:249-253). MZoneSoundManager.cpp:184/201 additionally hold long-lived duplicates created with bAutoRelease=false whose chunk is owned by the LRU-cached master, and GameMain.cpp:2143 calls ReleaseDuplicateBuffer() on every zone load.

**Failure scenario:** Player triggers the same sound twice in quick succession. Frame N: Play() sees the master chunk already playing, creates an auto-release duplicate sharing chunk C, plays it. Frame N+k: the duplicate finishes; ReleaseTerminatedDuplicateBuffer() calls Mix_FreeChunk(C) and frees the wrapper. The master wrapper still cached in g_pSoundManager now holds a dangling `chunk`. The next PlaySound for that sound ID hits the GetData() branch (GameMain.cpp:3730) and passes the freed chunk to Mix_PlayChannel — use-after-free in the mixer thread. Releasing the master later double-frees C.

**Recommendation:** Either give dxlib_sound_buffer an ownership flag (`owns_chunk`) that duplicates clear, or reference-count the Mix_Chunk. dxlib_sound_free() must only call Mix_FreeChunk when it is the last/owning wrapper.

> ✅ **Fixed** in `c0670ae` (the commit that made sound play at all). The decoded data lives in a reference-counted `dxlib_chunk_ref` shared by every duplicate; Mix_FreeChunk runs only when the last reference goes.

#### 🔴 Critical -- PlaySound copies a sound filename of up to 64 KB from Sound.inf into a 256-byte stack buffer with an unbounded strcpy.

**Category:** memory-safety  |  **Location:** `Client/GameMain.cpp:3651`

`char strFilename[256]; strcpy(strFilename, pFilename);` at GameMain.cpp:3650-3651, repeated at 3839-3840, 3912-3913, 3964-3965, 4038-4039 and 3772-3773. `pFilename` is `(*g_pSoundTable)[soundID].Filename`, an `MString` loaded from `Data/Info/Sound.inf` via `SOUNDTABLE_INFO::LoadFromFile` -> `MString::LoadFromFile` (Client/MString.cpp:199-244), whose only length check accepts anything up to `MAX_STRING_LENGTH = 65536`. There is no length check before the copy — only a NULL check.

**Failure scenario:** A Sound.inf entry (shipped game data, patch payload, or a tampered install) with a filename longer than 255 bytes causes strcpy to write past `strFilename` on the stack of PlaySound, corrupting saved registers/return address. Because PlaySound is driven by gameplay events, this is remotely influenceable by whatever content the server tells the client to play.

**Recommendation:** Replace with a bounded copy (`strncpy` + explicit NUL, or `snprintf`) sized to the buffer, or pass the MString's char* straight to LoadWav — the temporary copy serves no purpose.

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`). The three live sites (the others sit inside commented-out ForceSound blocks) are truncating bounded copies; an over-long Sound.inf entry now fails to load and gets logged instead of smashing the stack. Regression guard, executable-only code.

#### 🟠 High -- Two copies of CDirectMusic.h share one include guard but typedef MUSIC_TIME differently (long long vs long), giving CSDLMusic a different sizeof/layout in different translation units for a shared global.

**Category:** correctness  |  **Location:** `Client/CDirectMusic.h:21`

Client/CDirectMusic.h:21 has `typedef long long MUSIC_TIME;`, Client/DXLib/CDirectMusic.h:21 has `typedef long MUSIC_TIME;`. Both use the guard `__CSDLMUSIC_H__` (line 5 of each), so whichever is reached first in a TU wins and the other is silently skipped. `MUSIC_TIME m_mtStart; MUSIC_TIME m_mtOffset;` are class data members (Client/DXLib/CDirectMusic.h:92-93) followed by m_rtStart, m_rtOffset, m_bSoftwareSynth, m_bInit, m_bLoad, m_bPlay, m_OriginalTempo, m_CurrentTempo — so on 64-bit MSVC (long=4, long long=8) every member after m_mtOffset sits 8 bytes apart between the two versions. There is a single shared global `extern CSDLMusic g_SDLMusic;`. Resolution depends purely on include path/order: Client/DXLib/DXLib.h:18 (included by Client/Client.h:33) resolves to the DXLib copy, Client/DXLib.h:20 (included directly by MTopView.cpp:43, MTopViewDraw.cpp:33, DrawCreature*.cpp:33) resolves to the Client root copy.

**Failure scenario:** GameInit.cpp writes g_SDLMusic.m_bInit through one layout while Client.cpp:1125 reads g_SDLMusic.IsPlay()/m_bPlay at a different offset, so state checks read adjacent members. Any writer through the larger layout also writes 8 bytes past the object as the smaller layout sees it. This is a textbook ODR violation that the linker cannot diagnose.

**Recommendation:** Delete the duplicate Client/CDirectSound.h, Client/CDirectMusic.h, Client/CDirectSoundStream.h, Client/CDirectInput.h and keep only the Client/DXLib copies (dxlib's include dir is already PUBLIC), or at minimum make the two MUSIC_TIME typedefs identical and give the copies distinct guards so the divergence is caught.

> ✅ **Fixed** in `66d8637` (branch `harden/audio-media`). All five root copies (Client/DXLib.h included) are deleted; both VS_UI and DarkEden already had Client/DXLib on their include paths, so every bare include resolves to the single real copy. The root CDirectInput.h had additionally drifted behind the compiled CSDLInput layout (missing the m_*_held members), so this was two live ODR violations, not one.

#### 🟠 High -- CAVI::OpenMPG is a permanent stub returning FALSE, so entering the opening screen always pops a blocking modal 'Not Found test.mpg' MessageBox.

**Category:** correctness  |  **Location:** `Client/COpeningUpdate.cpp:50`

CAvi.cpp:21 stubs `CAVI::OpenMPG(...) { return FALSE; }` on all platforms. COpeningUpdate::PlayMPG (COpeningUpdate.cpp:50-56) treats a FALSE return as 'file missing' and calls `MessageBox(g_hWnd, str, "Error!", MB_OK)` before returning. GameMain.cpp:699 calls `g_pCOpeningUpdate->PlayMPG("test.mpg")` unconditionally in the `case MODE_OPENING:` branch of SetMode(). The same function also does `sprintf(str, "Not Found %s", filename)` into `char str[256]` (line 52-53) with no length bound on `filename`.

**Failure scenario:** Every time the client enters MODE_OPENING it blocks on a modal Windows message box that the user must dismiss before the game proceeds — a guaranteed startup/flow interruption on a code path the game always takes. The CAvi.cpp header comment (lines 18-20) acknowledges this but the caller was never updated.

**Recommendation:** Make PlayMPG a no-op (or log-only) when the AVI/MPG backend is stubbed, rather than surfacing a modal error for a feature that is deliberately unimplemented; and bound the sprintf.

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`). PlayMPG logs a bounded message (snprintf + DEBUG_ADD) and returns; entering the opening screen no longer blocks on a click-through.

#### 🟠 High -- All three SDL audio adapter files reference class names that no longer exist, so turning SDL2_mixer on (the only way to get audio) breaks the build with compile and duplicate-symbol errors.

**Category:** build  |  **Location:** `Client/DXLib/CDirectSound_Adapter.cpp:35`

After the CDirectSound->CSDLAudio / CDirectSoundStream->CSDLStream rename, the adapters were not updated: CDirectSound_Adapter.cpp:35 declares `CSDLAudio::CDirectSound()` and :45 `CSDLAudio::~CDirectSound()` (the class is CSDLAudio — no such constructor name), and :16 declares `CDirectSound g_SDLAudio;` outside the `#ifdef DXLIB_BACKEND_SDL` guard, a type that does not exist. CDirectSoundStream_Adapter.cpp:22 and :42 have the same defect (`CSDLStream::CDirectSoundStream()`), plus it assigns members `m_dwBufferSize`/`m_dwNotifySize`/`m_dwNextWriteOffset`/`m_dwProgress`/`m_dwLastPos`/`m_bFoundEnd` that the current CDirectSoundStream.h does not declare. Independently, Client/DXLib/CMakeLists.txt compiles the stub CDirectSound.cpp (which defines `CSDLAudio g_SDLAudio;` at line 14) and CDirectMusic.cpp (`CSDLMusic g_SDLMusic;` at line 15) *unconditionally* (lines 58-60), while adding the adapters that define the same globals when HAVE_SDL2_MIXER is true (lines 70-76) — a guaranteed duplicate-symbol link error even if the compile errors were fixed.

**Failure scenario:** A contributor installs SDL2_mixer (vcpkg/brew) expecting audio to start working. `find_package(SDL2_mixer)` now succeeds, HAVE_SDL2_MIXER becomes TRUE, the adapters enter the build, and the project stops compiling entirely. The 'working' configuration is the one where SDL2_mixer is absent — i.e. the project only builds while audio is guaranteed dead.

**Recommendation:** Fix the constructor/destructor names and the `CDirectSound g_SDLAudio` declaration, move the global definitions so exactly one TU owns each, and make CMakeLists exclude the stub CDirectSound.cpp/CDirectMusic.cpp/CDirectSoundStream.cpp when the adapters are compiled.

> ✅ **Fixed** in `c0670ae`. The three adapters were rewritten against the renamed classes, and Client/DXLib/CMakeLists.txt now compiles exactly one set — the adapters when SDL2_mixer is found, the stubs otherwise — so no global is defined twice.

#### 🟠 High -- The SDL_mixer include is guarded by SDL_MIXER_MAJOR_VERSION, a macro that only SDL_mixer.h itself defines, so the entire audio backend always compiles to the no-op stub.

**Category:** build  |  **Location:** `Client/DXLib/DXLibBackendSDL.cpp:29`

Lines 29-31 read `#ifdef SDL_MIXER_MAJOR_VERSION` / `#include <SDL_mixer.h>` / `#endif`. `SDL_MIXER_MAJOR_VERSION` is defined *by* SDL_mixer.h, so the condition can never be true before the include. The same macro then gates the real sound implementation (line 531), the real music implementation (line 722) and the capability flags (line 906); a grep across the whole tree shows nothing else defines it. Client/DXLib/CMakeLists.txt:44 defines `-DHAVE_SDL2_MIXER`, but no source file ever tests `HAVE_SDL2_MIXER`. Result: `dxlib_sound_init` returns 1, `dxlib_sound_load_wav` returns NULL, `dxlib_music_*` are no-ops (lines 704-714, 838-849), and `dxlib_get_capabilities()` reports neither SOUND nor MUSIC.

**Failure scenario:** The game builds and runs with zero sound and zero music, with no diagnostic beyond CSDLAudio::Init() returning false. Because the stub CDirectSound.cpp is what actually gets linked (see the adapter finding), even the plumbing above it is inert.

**Recommendation:** Include <SDL_mixer.h> unconditionally under `#ifdef HAVE_SDL2_MIXER` (the macro CMake actually defines) and switch all three `#ifdef SDL_MIXER_MAJOR_VERSION` blocks to `#ifdef HAVE_SDL2_MIXER`.

> ✅ **Fixed** in `c0670ae`. The include is gated on `HAVE_SDL2_MIXER`; the later `SDL_MIXER_MAJOR_VERSION` blocks are then true because the header has actually been included, so the real implementations compile.

#### 🟠 High -- SDL's audio subsystem is never initialized on Windows and Mix_Init() is never called, so even a fixed SDL_mixer build would fail to open audio or decode MP3/OGG.

**Category:** portability  |  **Location:** `Client/DXLib/DXLibBackendSDL.cpp:336`

`dxlib_input_init` does `SDL_Init(0)` (line 336) — no subsystems. SpriteLibBackendSDL.cpp:41 does `SDL_Init(SDL_INIT_VIDEO)`. basic/PlatformSDL.cpp:483 does `SDL_Init(0)`. The only `SDL_INIT_AUDIO` in the tree is Client/SDLMain.cpp:341, and SDLMain.cpp is entirely wrapped in `#ifndef PLATFORM_WINDOWS` (line 34, closed at 570) — the Windows entry point is Client.cpp's WinMain. Separately, `dxlib_sound_init` (line 543-557) calls Mix_OpenAudio directly and never calls `Mix_Init(MIX_INIT_MP3|MIX_INIT_OGG)`, yet `dxlib_get_capabilities()` (line 907) advertises DXLIB_CAP_MP3 and DXLIB_CAP_OGG.

**Failure scenario:** After someone fixes the SDL_mixer include guard, Mix_OpenAudio on Windows fails with 'Audio subsystem is not initialized' because SDL_INIT_AUDIO was never requested; the failure surfaces only as CSDLAudio::Init() returning false, which InitSound (GameInit.cpp:960) turns into a silent 'return FALSE'. On platforms where audio does open, Mix_LoadMUS on an .mp3/.ogg fails because the corresponding decoder was never loaded.

**Recommendation:** Add SDL_INIT_AUDIO to the subsystem init on the Windows path (Client.cpp/GameInit.cpp) or have dxlib_sound_init call SDL_InitSubSystem(SDL_INIT_AUDIO) itself, and call Mix_Init with the required decoder flags before Mix_OpenAudio.

> ✅ **Fixed** in `452e854` (branch `harden/audio-media`). In practice audio worked anyway — SDL2_mixer's Mix_OpenAudio initializes the subsystem itself and loads decoders on demand — but dxlib_sound_init now calls SDL_InitSubSystem(SDL_INIT_AUDIO) and Mix_Init(MIX_INIT_OGG) explicitly rather than lean on that, and a missing decoder is logged at init. Only OGG is requested: the mixer music path is OGG-only and the vcpkg SDL2_mixer is built without MP3, so MIX_INIT_MP3 would warn on every init for a decoder nothing uses. The Mix_OpenAudio failure path undoes the subsystem init so retries cannot grow the refcount.

#### 🟠 High -- MZoneSoundManager::UpdateSound dereferences the result of GetData() without a NULL check in one of three sibling branches.

**Category:** correctness  |  **Location:** `Client/MZoneSoundManager.cpp:461`

At line 459-464: `ZONESOUND_NODE* pSound = GetData( zoneSoundID ); if (pSound->IsLoop()) { pSound->SetContinueLoop(); }`. `CTypeMap<ZONESOUND_NODE>::GetData()` (Client/CTypeMap.h:~108) returns NULL when the id is not in the map. The two sibling branches in the same if/else chain do guard: line 410 `if (pSound==NULL)` creates the node, line 452 `if (pSound!=NULL)` before Stop(). Only this branch — taken when `!pInfo->IsShowTime() && pInfo->IsShowHour()`, i.e. the zone sound is inside its active hour but not yet due to replay — is unguarded, and it is exactly the branch that runs before any node has ever been created for that id.

**Failure scenario:** Player walks into a sector whose SECTORSOUND_INFO references a zone sound that is currently inside its show-hour window but whose next show time has not arrived. UpdateSound() calls GetData(zoneSoundID), gets NULL because no ZONESOUND_NODE was ever allocated for it, and calls IsLoop() on a null pointer — immediate crash on zone entry.

**Recommendation:** Add `if (pSound != NULL)` around the IsLoop()/SetContinueLoop() call, matching the sibling branches.

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`), exactly as recommended. Regression guard, executable-only code.

#### 🟠 High -- WavePackFileInfo::SaveToFileData does an unbounded strcpy into a 256-byte stack buffer, leaks the mmio handle on the format-mismatch path, and allocates using an unvalidated chunk size from the WAV file.

**Category:** memory-safety  |  **Location:** `Client/WavePackFileManager.cpp:42`

Line 41-42: `char filename[256]; strcpy(filename, m_Filename.c_str());` with no length check on an MString-derived path. Line 64: `mmioRead(wavefile, (char*)&wavefmt, wavefmtSize)` return value is ignored, so `wavefmt` can be partially uninitialized before line 65 tests `wavefmt.wFormatTag`. Line 65-68: when the format is not WAVE_FORMAT_PCM the function `return false;` without calling `mmioClose(wavefile, 0)` — the HMMIO opened at line 43 leaks. Line 73-77: `mmioDescend(..., MMIO_FINDCHUNK)` result is unchecked, then `DWORD cksize = child.cksize; char* pBuffer = new char[cksize];` with no upper bound; if the data chunk is absent, `child.cksize` is whatever mmioDescend left behind. This file is compiled on Windows (it is only excluded under `if(NOT WIN32)` at CMakeLists.txt:709).

**Failure scenario:** A WAV whose path exceeds 255 characters overflows `filename` on the stack. A non-PCM WAV in the pack leaks an mmio file handle per call. A truncated/corrupt WAV yields a garbage cksize and either a huge allocation (std::bad_alloc / OOM) or an mmioRead into a buffer smaller than the declared size.

**Recommendation:** Use a bounded copy for the path, check every mmio* return value, close the handle on all exit paths (RAII or a single cleanup label), and sanity-bound cksize against the actual file size before allocating.

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`). Every mmio return is checked, every exit path closes the handle, the path copy is length-checked, and the data chunk size is rejected when zero or larger than the parent RIFF chunk. Regression guard, executable-only code.

#### 🟠 High -- CMake configure fails on any case-sensitive filesystem: the source list names Client/COGGSTREAM.cpp but the tracked file is Client/COGGSTREAM.CPP.

**Category:** portability  |  **Location:** `CMakeLists.txt:593`

CMakeLists.txt:593 does `list(APPEND CLIENT_MAIN_SOURCES Client/COGGSTREAM.cpp)`, but `git ls-files` shows the tracked name is `Client/COGGSTREAM.CPP` (uppercase extension). The surrounding `file(GLOB CLIENT_MAIN_SOURCES Client/*.cpp)` at line 583 also will not match `.CPP` on a case-sensitive filesystem, so the explicit append is the only thing that would pull the file in — and it names a path that does not exist there. On Windows (and case-insensitive macOS) both the glob and the explicit entry resolve to the same file under two different path strings.

**Failure scenario:** A contributor runs `cmake ..` on Linux (a platform the README and CLAUDE.md both claim support for) and gets `Cannot find source file: Client/COGGSTREAM.cpp` before any compilation starts. On Windows the same file is listed twice under differing case, which risks LNK2005 duplicate symbols for COGGSTREAM if CMake generates two object files.

**Recommendation:** Rename the file to Client/COGGSTREAM.cpp (git mv, case-only rename) and drop the redundant explicit list(APPEND), letting the glob pick it up.

> ⚠️ **Recorded as fixed in `66d8637`, but the rename never landed.** Verified at HEAD on 2026-09-01: `git ls-files` still reported `Client/COGGSTREAM.CPP` and `Client/COGGSTREAM.H`, while all three include sites (`CGameUpdate.cpp:51`, `SoundSetting.h:9`, and the file's own line 1) spelled it `"COGGSTREAM.h"`. Only the include-spelling half and the `list(APPEND)` removal actually landed. That combination is *worse* on Linux than what the finding described, not better: dropping the explicit append removed the loud `Cannot find source file` configure error and left the case-sensitive glob silently skipping the file, so the failure moved from configure time to a link-time cascade of unresolved `COGGSTREAM::` symbols.
>
> ✅ **Fixed for real** on branch `harden/high-severity-batch1`. Both files are renamed to `COGGSTREAM.cpp` / `COGGSTREAM.h` with a two-step `git mv` (a case-only rename is a no-op in one step on a case-insensitive filesystem, which is the most likely reason the original attempt was believed to have worked). The lesson for this document: a rename claimed in a commit message is not evidence, because on Windows the build stays green either way — only `git ls-files` is.

#### 🟡 Medium -- The compat MMCKINFO in AudioTypes.h has different fields and a different layout from the real Windows MMCKINFO, and both definitions are live in the same program.

**Category:** maintainability  |  **Location:** `basic/AudioTypes.h:88`

AudioTypes.h:87-93 defines `typedef struct _MMCKINFO { FOURCC ckid; FOURCC fccType; DWORD dwDataOffset; DWORD dwSize; } MMCKINFO;`. The real <mmsystem.h> MMCKINFO is `{ FOURCC ckid; DWORD cksize; FOURCC fccType; DWORD dwDataOffset; DWORD dwFlags; }` — five members, different order, and the size field is `cksize` not `dwSize`. The `#if !defined(_INC_MMSYSTEM) && !defined(_MMISCAPI_H_)` guard only prevents a same-TU redefinition; it does nothing about the cross-TU case. Client/WavePackFileManager.cpp:13-15 deliberately includes the real <MMSystem.h> and uses `child.cksize` (line 75), while Client/DXLib/CDirectSoundStream.h/.cpp declare `WaveReadFile(..., MMCKINFO* pckIn, ...)` against the compat struct.

**Failure scenario:** Any future code that passes an MMCKINFO across the boundary between a real-mmsystem TU and a compat TU reads the wrong fields at the wrong offsets. Today CSDLStream::WaveReadFile is a stub so nothing crosses, which is the only reason this is not already corrupting data.

**Recommendation:** Give the compat struct a distinct name (e.g. DXLIB_CKINFO) so the two can never be confused, or drop the compat definition entirely now that no code parses RIFF chunks outside WavePackFileManager.cpp.

> ✅ **Fixed** in `d021544` (branch `harden/audio-media`). The compat struct is now DXLIB_CKINFO, used only by the CSDLStream stubs; the FOURCC/HMMIO typedefs keep the include-guard check since those must still defer to the real headers.

#### 🟡 Medium -- CMP3::GetErrorString passes sizeof(pointer) as the destination buffer length to mciGetErrorString, and CMP3::Play can return an uninitialized DWORD.

**Category:** memory-safety  |  **Location:** `Client/CMP3.cpp:528`

Line 528: `mciGetErrorString(dwErrCode, lpszErrString, sizeof(lpszErrString));` — `lpszErrString` is an `LPSTR` parameter, so sizeof yields 4 or 8, not the buffer capacity; the API is told the caller's buffer is pointer-sized. In `CMP3::Play` (lines 372-395), `DWORD dwResult;` is declared uninitialized at line 374 and only assigned inside `if (dwStatus != MCI_MODE_NOT_READY)` (line 385), yet line 394 unconditionally does `return dwResult;`. This branch of the file is behind `#if defined(PLATFORM_WINDOWS) && defined(__USE_MP3__)` (line 285) and __USE_MP3__ is currently never defined, so both are latent.

**Failure scenario:** If __USE_MP3__ is ever enabled: GetErrorString silently truncates every MCI error message to a few bytes (or, on the other reading, misreports capacity to the API); Play() called while the device is MCI_MODE_NOT_READY returns an indeterminate value that callers test as a success/failure code — undefined behavior per the standard.

**Recommendation:** Pass an explicit length parameter to GetErrorString, and initialize dwResult (e.g. to a defined error code) at declaration.

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`). GetErrorString takes the capacity from the caller (there were no existing callers to update), and Play initializes dwResult to MCIERR_DEVICE_NOT_READY. Still latent behind __USE_MP3__.

#### 🟡 Medium -- CPartManager's index-based accessors perform no bounds checking on the caller-supplied index used to address m_pPartIndex.

**Category:** memory-safety  |  **Location:** `Client/CPartManager.h:325`

`IsDataNULL(index)`/`IsDataNotNULL(index)` (lines 100-101) read `m_pPartIndex[index]`; `SetData` writes `m_pPartIndex[index] = newPartIndex` (lines 325 and 384); `GetData` reads `m_pPartIndex[index]` (line 443). None compare `index` against `m_nIndex`. The array is allocated as `new PartIndexType[m_nIndex]` at line 220 where m_nIndex comes from `g_pSoundTable->GetSize()`. Callers rely entirely on an external guard: GameMain.cpp:3612 and 3806 check `soundID >= (*g_pSoundTable).GetSize()`, and MZoneSoundManager.cpp:90 checks the same — but MZoneSoundManager.cpp:152/175 then index with `m_SoundID`, and ZONESOUND_NODE::m_SoundID is taken from a zone data file via ZONESOUND_INFO::LoadFromFile (MZoneSound.cpp:42). Additionally, PartIndexType is BYTE so `m_PartIndexNULL = (PartIndexType)0xFFFFFFFF` is 0xFF (line 181), capping usable parts at 255 with no check in Init(); GetLRU() dereferences `*m_listLRU.rbegin()` on a possibly empty list at line 550 if m_nPart is 0.

**Failure scenario:** A zone file whose ZONESOUND_INFO.SoundID exceeds the loaded Sound.inf size reaches ZONESOUND_NODE::Play with a check only against g_pSoundTable size (line 90) but reaches g_pSoundManager->IsDataNULL(m_SoundID) at line 152, indexing m_pPartIndex out of bounds if the two tables ever disagree. If Init() is ever called with maxPart >= 256, BYTE truncation makes m_nPart 0 and GetLRU dereferences an empty list.

**Recommendation:** Bounds-check index against m_nIndex in IsDataNULL/SetData/GetData (returning a defined result on overflow), clamp maxPart to the PartIndexType sentinel in Init(), and guard GetLRU against an empty LRU list.

> ✅ **Fixed** in `6c2e37e` (branch `harden/audio-media`), test-first: CPartManager.h is a self-contained template, so `tests/unit/test_part_manager.cpp` pins the rejected out-of-range indices, the sentinel clamps (a 255-part BYTE pool becomes 254, not 0xFF), and the empty-list guard.

#### 🟡 Medium -- CSoundPartManager::Release() hides a non-virtual base Release(), so the base class's own Init() path frees the cache array without releasing the sound buffers it holds.

**Category:** memory-safety  |  **Location:** `Client/CSoundPartManager.cpp:12`

`CPartManager<...>::Release()` is not virtual (Client/CPartManager.h:95) and neither is `~CPartManager()` (line 89). `CSoundPartManager::Release()` (CSoundPartManager.cpp:11-34) is the only version that calls `g_SDLAudio.Stop()/Release()` on each cached LPDIRECTSOUNDBUFFER before delegating. But `CPartManager::Init()` calls `Release()` at CPartManager.h:206 — statically bound to the base version — and then `delete [] m_pData` (line 270) discards every cached buffer handle without freeing the underlying Mix_Chunks. The same applies to any deletion through a `CPartManager<WORD,BYTE,LPDIRECTSOUNDBUFFER>*`.

**Failure scenario:** InitSound() (GameInit.cpp:986) calls `g_pSoundManager->Init(g_pSoundTable->GetSize(), g_pClientConfig->MAX_SOUNDPART)`. Any code path that reaches Init() on a manager still holding loaded sounds — a re-init without an intervening UnInitSound(), or a future caller that resizes the cache — leaks up to MAX_SOUNDPART (100) Mix_Chunks with no way to recover them.

**Recommendation:** Make CPartManager::Release() and ~CPartManager() virtual, or add a protected virtual `OnEvictData(DataType&)` hook that the base calls before freeing so subclasses can release resources regardless of which Release() is entered.

> ✅ **Fixed** in `6c2e37e` (branch `harden/audio-media`), via the hook variant of this recommendation: a protected virtual `OnReleaseData(DataType&)` that the base Release() calls per slot, which CSoundPartManager overrides to free each cached buffer. Release() itself deliberately stays non-virtual — the adversarial review of the branch caught that making it virtual (the first draft) silently broke CShadowPartManager and the three texture managers, whose shadowing Release() frees dimension tables their Clear() needs a base re-Init to preserve. `tests/unit/test_part_manager.cpp` asserts a re-Init reaches the hook for every stored slot.

#### 🟡 Medium -- About 180 KB of MP3/Huffman decoder source exists in two duplicated copies that no CMakeLists compiles, while root CMakeLists comments assert they are built into dxlib.

**Category:** dead-code  |  **Location:** `Client/DXLib/CMakeLists.txt:51`

`DXLIB_SOURCES` (Client/DXLib/CMakeLists.txt:51-67) lists only DXLibBackendSDL.cpp, CDirectDraw*.cpp, CDirectSound.cpp, CDirectMusic.cpp, CDirectSoundStream.cpp and CDirectInput_Adapter.cpp. It does not list mp3.cpp, huffman.cpp, subdecoder.cpp, synfilt.cpp, reader.cpp, header.cpp, soundbuf.cpp or BIT_RES.CPP — nor does any other CMakeLists in the tree (verified by grep). Meanwhile the root CMakeLists.txt:640-669 excludes the Client-root copies of exactly those files with the justification that they are 'already compiled into the dxlib library that DarkEden links', which is false. `git ls-files` confirms both copies are tracked: Client/mp3.cpp + Client/DXLib/mp3.cpp, Client/huffman.cpp + Client/DXLib/huffman.cpp, and so on for reader/synfilt/subdecoder/soundbuf/header/BIT_RES. CDirectInput.cpp (the DirectInput implementation, 26 KB) is likewise absent from DXLIB_SOURCES.

**Failure scenario:** A contributor asked to fix or port MP3 playback edits Client/DXLib/mp3.cpp, rebuilds, sees no change, and has no way to tell from the build files that the source is orphaned — the CMake comments actively point the wrong way. Bugs like the huffman bounds defect sit unfixed and undetected because nothing compiles them.

**Recommendation:** Delete one copy of each duplicated decoder file and either add the surviving copies to DXLIB_SOURCES or remove them outright; correct the now-inaccurate exclusion comments at root CMakeLists.txt:640-669.

> ✅ **Fixed** in `66d8637` (branch `harden/audio-media`). Both copies are deleted outright — nothing outside the family referenced its symbols, and MP3/OGG decoding comes from SDL2_mixer — and the false CMake comments went with them.

#### 🟡 Medium -- The SDL sound backend caches a mixer channel index that SDL_mixer recycles, so stop/volume/halt operations act on whatever sound now owns that channel.

**Category:** correctness  |  **Location:** `Client/DXLib/DXLibBackendSDL.cpp:632`

`dxlib_sound_play` stores `sound->channel = Mix_PlayChannel(-1, ...)` and sets `sound->playing = 1` (lines 632-633). Nothing ever clears `playing`/`channel` when playback ends naturally — only the explicit `dxlib_sound_stop` (lines 638-648) does. Consequently: `dxlib_sound_free` (line 620-622) calls `Mix_HaltChannel(sound->channel)` whenever `playing` is still set, `dxlib_sound_set_volume` (line 661-663) calls `Mix_Volume(sound->channel, ...)`, and `dxlib_sound_is_playing` (line 652) returns true if *anything* is playing on that channel. SDL_mixer hands out the lowest free channel from a pool of 32 (Mix_AllocateChannels at line 553), so indices are recycled aggressively in a game that fires many short sounds.

**Failure scenario:** Sound A plays on channel 5 and finishes; `playing` stays 1 and `channel` stays 5. Sound B is then assigned channel 5. A call to g_SDLAudio.Stop(A) or the free path in ReleaseTerminatedDuplicateBuffer halts sound B mid-playback; g_SDLAudio.SubVolumeFromMax(A, ...) silences B; IsPlay(A) reports true because B is audible. Audio cuts out and volume/panning apply to the wrong sounds.

**Recommendation:** Register a Mix_ChannelFinished callback that clears the owning wrapper's channel/playing state, and verify `Mix_GetChunk(channel) == sound->chunk` before acting on a cached channel index.

> ✅ **Fixed** in `c0670ae`. A `g_channel_owner` table records which buffer most recently started on each channel, and every stop/volume/pan/is-playing operation first verifies the buffer still owns its remembered channel.

#### 🟡 Medium -- huffman_decoder's tree-walk bound uses ht->treelen (table 0, whose treelen is 0) instead of h->treelen, disabling the bounds check and allowing out-of-range reads of the Huffman value tables.

**Category:** memory-safety  |  **Location:** `Client/DXLib/huffman.cpp:427`

The decode loop at lines 410-427 ends with `} while (level || ((unsigned int)point < ht->treelen) );`. `ht` is the global `struct huffcodetab ht[HTN]` array, so `ht->treelen` is `ht[0].treelen`, which is 0 (line 353) — the second condition is always false and the loop is bounded only by `level` shifting out of a 32-bit HUFFBITS, i.e. up to 32 iterations. Inside the loop, `point` advances via `while (h->val[point][1] >= MXOFF) point += h->val[point][1]; point += h->val[point][1];` (lines 419-420) and the symmetric `[0]` branch (423-424) with no range test against `h->treelen`. Tables are as small as 7 entries (ValTab1, line 16) while entry values reach 250. The error path that would conceal a bad code is commented out (lines 431-435), so on failure `*x`/`*y` are returned uninitialized to the caller. Note this file is currently orphaned (see the dead-decoder finding), so the bug is latent rather than live.

**Failure scenario:** A malformed or truncated MP3 frame drives 32 unchecked `point +=` steps on a 7- or 17-entry table; `h->val[point]` reads hundreds of entries past the end of the static array, and the caller in subdecoder.cpp then consumes uninitialized *x/*y as quantized sample values.

**Recommendation:** Restore the reference-decoder bound (`point < h->treelen`), add an explicit range check before every `h->val[point]` access, and initialize *x/*y (or return early) on the error path.

> ✅ **Resolved by removal** in `66d8637` (branch `harden/audio-media`): the orphaned decoder — both copies — is deleted rather than patched (see the dead-decoder finding above).

#### 🟡 Medium -- On WAV load failure PlaySound releases the sound table's Filename MString and then immediately formats that now-NULL pointer with %s.

**Category:** correctness  |  **Location:** `Client/GameMain.cpp:3661`

At GameMain.cpp:3661-3664: `(*g_pSoundTable)[soundID].Filename.Release();` followed by `DEBUG_ADD_FORMAT("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, (*g_pSoundTable)[soundID].Filename );`. `MString::Release()` (Client/MString.cpp:86-94) does `delete [] m_pString; m_pString = NULL;`, and the MString-to-`const char*` conversion operator (MString.h:37) returns that NULL. The same order appears at 3849-3852 and 3974-3977. DEBUG_ADD_FORMAT is a real vararg function in debug builds (DebugInfo.h:70) and `((void)0)` otherwise (DebugInfo.h:88).

**Failure scenario:** In a debug build, the first failure to load any WAV passes a NULL pointer to a %s conversion. MSVC's CRT prints '(null)' but this is not portable — glibc/other CRTs and any custom formatter in DEBUG_ADD_FORMAT will dereference it and crash on the exact code path that is supposed to report the error.

**Recommendation:** Capture the filename into a local (or emit the message) before calling Filename.Release().

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`). The log line formats the bounded local copy and runs before Release() at all three sites.

#### 🟡 Medium -- MMusic::Play formats an unbounded filename into a 256-byte stack buffer with sprintf.

**Category:** memory-safety  |  **Location:** `Client/MMusic.cpp:161`

Line 147 declares `char buffer[256];` and line 161 does `sprintf(buffer, "open %s type sequencer alias Midi", filename);` where `filename` is the LPCSTR argument, with only a NULL check at line 155. Callers pass `(*g_pMusicTable)[...].Filename`, an MString loaded from a data file. The whole branch sits behind `#if defined(PLATFORM_WINDOWS) && defined(__USE_REAL_MIDI__)` (line 16), which is never defined today, so this is latent — but the file is otherwise live and this is the code the stub is meant to be replaced by.

**Failure scenario:** With MIDI re-enabled, a music table entry with a path longer than ~230 characters overflows `buffer` on MMusic::Play's stack.

**Recommendation:** Use snprintf with sizeof(buffer), or build the MCI command string with a std::string.

> ✅ **Fixed** in `4ff97c4` (branch `harden/audio-media`), with snprintf. Still latent behind __USE_REAL_MIDI__.

#### ⚪ Low -- The LRU counter-rollover normalization subtracts the minimum inside the loop after that entry has already been zeroed, so only entries before the minimum are corrected.

**Category:** correctness  |  **Location:** `Client/CPartManager.h:513`

Lines 512-516: `int leastTime = m_pLastTime[leastTimeIndex];` (computed but never used) then `for (int i=0; i<m_nPart; i++) { m_pLastTime[i] -= m_pLastTime[leastTimeIndex]; }`. On the iteration where `i == leastTimeIndex`, that element becomes 0, so every subsequent iteration subtracts 0 and leaves those timestamps unnormalized. The loop is guarded by `if (m_Counter==0xFFFFFFFF)` at line 490.

**Failure scenario:** After ~4 billion GetData() calls the rollover path runs and leaves the timestamp array in a mixed state, so m_Counter is reset to a value that can be below existing timestamps and LRU eviction picks the wrong entries. Practically unreachable in a game session, but the dead `leastTime` local shows the intent was the opposite.

**Recommendation:** Use the captured `leastTime` local inside the loop instead of re-reading m_pLastTime[leastTimeIndex].

> ✅ **Fixed** in `6c2e37e` (branch `harden/audio-media`), exactly as recommended; `tests/unit/test_part_manager.cpp` drives the counter to the rollover and asserts every timestamp is normalized.

#### ⚪ Low -- The SDL input constructor leaves m_bSwapMouseButtons uninitialized, and CSDLInput::s_KeyName has no definition in the SDL build.

**Category:** correctness  |  **Location:** `Client/DXLib/CDirectInput_Adapter.cpp:29`

> ✅ **Fixed (2026-09-18):** removed the unused swap flag, unreferenced key-name API/table declaration and unbuilt DirectInput implementation. `KeyDown` rejects indices outside its 256 entries. `dxlib` now links into `unit_tests` through an application host instead of game-global/UI references; its SDL backend selection is public to consumers. The key-index test reproduced reading mouse state through index 256 before the guard.

`CSDLInput::CSDLInput()` (lines 29-47) initializes every member except `BOOL m_bSwapMouseButtons` (declared CDirectInput.h:36), leaving it indeterminate. Separately, `static const char* s_KeyName[256]` is declared at CDirectInput.h:40 and used by the inline `GetKeyName(DWORD dik) { return s_KeyName[dik]; }` (line 118), but its only definition is CDirectInput.cpp:20, a file wrapped entirely in `#ifdef PLATFORM_WINDOWS` (line 7) and not listed in DXLIB_SOURCES at all. The comment at CDirectInput_Adapter.cpp:20 claims 'it's defined in the header', which is not true. Both GetKeyName and KeyDown also index with an unchecked DWORD.

**Failure scenario:** Reading m_bSwapMouseButtons is UB (it is unused on the SDL path today, so this is latent). The first caller of CSDLInput::GetKeyName — e.g. a key-binding UI — fails to link with an unresolved external for s_KeyName, with a source comment pointing the reader in the wrong direction.

**Recommendation:** Initialize m_bSwapMouseButtons = FALSE in the adapter constructor, move the s_KeyName table into a file that is actually compiled (or delete GetKeyName if unused), fix the misleading comment, and bounds-check the dik index in GetKeyName/KeyDown.

#### ⚪ Low -- The mouse wheel accumulator is never reset and is out of sync with CSDLInput's copy, producing spurious wheel events after SetMouseMoveLimit.

**Category:** correctness  |  **Location:** `Client/DXLib/DXLibBackendSDL.cpp:444`

> ✅ **Fixed (2026-09-18):** wheel reads consume a signed pending delta, with widened/saturating accumulation. Adapter callbacks use that delta directly; clears, mode changes and release discard old pending input. Multiple SDL pumps before consumption preserve movement, including the non-Windows outer-loop pump. Tests reproduced the phantom mode-change event and old cumulative values; regressions cover consecutive frames, repeated reads, multiple pumps, pending input at mode changes and extreme deltas.

`g_mouse_wheel += event.wheel.y;` (line 444) accumulates for the lifetime of the process and is never cleared; `dxlib_input_get_mouse_wheel()` (line 497) just returns the running total. `CSDLInput::UpdateInput` compares `old_z` to the freshly read total and fires WHEELUP/WHEELDOWN on any difference (CDirectInput_Adapter.cpp:141-153). But `CSDLInput::SetMouseMoveLimit` sets `m_mouse_z = 0` (CDirectInput_Adapter.cpp:236) without touching the backend accumulator.

**Failure scenario:** After a resolution/mode change calls SetMouseMoveLimit, the next UpdateInput sees old_z = 0 and m_mouse_z = the historical accumulated total, and fires a single bogus WHEELUP (or WHEELDOWN) event — an unexpected zoom/scroll immediately after a mode switch.

**Recommendation:** Either reset g_mouse_wheel via a backend call in SetMouseMoveLimit, or make dxlib_input_get_mouse_wheel return and clear a per-frame delta instead of a lifetime total.

---

## Rendering & Sprites

**Grade:** D  |  **Findings:** 25

**Scope:** Rendering — Client/SpriteLib (SDL backend, sprite/palette formats), CSDLGraphicsFlip, C*PartManager*, sprite/pak file loading

**Health assessment:** This area is the weakest link in the client's memory safety. Every sprite/palette/filter parser (CSpritePalBase, CSprite555, CFilter, CAlphaSprite*, CShadowSprite, spritectl_load_sprite_from_file) reads counts, lengths and offsets straight out of .spk/.ppk data and then walks RLE structures with no bound tied to the allocation; several produce out-of-bounds writes, one directly into the locked backbuffer, so a corrupt or hostile data file is a heap-corruption primitive rather than a graceful failure. Hardening has been applied unevenly: CSprite565 got a length sanity check while its 555 twin did not; get_backend_sprite validates its RLE walk while get_backend_alpha_sprite does not. The 555/565/Pal/Index/Shadow/Alpha format duplication means every fix must be made five times and in practice is not. On top of that the SDL migration left structural debt with real cost: four texture part managers each take two surface locks and release one, CSpriteSurface::GetSurfaceInfo hands out a pixel pointer after unlocking, ~40 Blt* entry points are TODO stubs or silently redirect to a plain blit, the effect function tables are never populated so all sprite effects are dead while live call sites remain, and CSpriteSurface.h carries two hand-synced parallel class declarations gated on a macro set in only one place. Note that D3DLib/ no longer exists in the tree despite still being documented in CLAUDE.md.

#### 🔴 Critical -- CAlphaSpritePal::Blt walks file-derived RLE data with zero bounds checks and writes decoded pixels straight into the locked surface with no width clamp.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CAlphaSpritePal.cpp:461`

> ✅ **Fixed** in `764e9ab` (Blt's own bound), `c444a83` (the unchecked MPalette::operator[] this finding also names) and `a17aa39` (scanline validation at load time, which covers the twenty-odd clipping and pixel format blit variants that share this defect). Covered by `tests/unit/test_calphaspritepal.cpp`, using a sentinel filled guard band beyond the sprite width.

Lines 480-502: `count = *pPixels++` then per segment `pDestTemp += *pPixels++` (transparent run) and `colorCount = *pPixels++`, followed by `memcpyAlpha(pDestTemp, pPixels, colorCount, pal)` and `pPixels += (colorCount<<1)`. None of count, the transparent run, or colorCount is validated against m_Width or against the size of the m_pPixels[i] buffer. This is called from CSpriteSurface::BltAlphaSpritePal (Client/SpriteLib/CSpriteSurface_Adapter.cpp:657) with pDest pointing into the freshly locked backbuffer, so an oversized transparent run or colorCount writes past the end of the destination scanline and past the end of the surface allocation. memcpyAlpha additionally indexes the palette with `pal[*pSource]` (CAlphaSpritePal.cpp:265) where MPalette::operator[] is an unchecked `m_pColor[n]` over an m_Size-entry array (Client/SpriteLib/MPalette.h:30), so any pixel byte >= m_Size reads out of bounds too.

**Recommendation:** Clamp the running destination x against m_Width and the surface width, validate colorCount and the transparent run against the remaining width, and bounds-check the palette index against MPalette::GetSize() before dereferencing.

#### 🔴 Critical -- CFilter::LoadFromFile overwrites m_Width/m_Height from the file before Init() calls Release(), so Release() frees the old pointer array using the new, file-controlled height.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CFilter.cpp:200`

> ✅ **Fixed** in `90d6794`. The header is read into locals and validated against the bytes the file actually carries before anything is released or allocated, so a rejected file leaves the existing filter intact. Covered by `tests/unit/test_cfilter.cpp`; the reported failure reproduced as an access violation.

LoadFromFile reads directly into the members: `file.read((char*)&m_Width, 2); file.read((char*)&m_Height, 2);` (lines 200-201), then calls `Init(m_Width, m_Height)` (line 208). Init immediately calls Release() (CFilter.cpp:37), and Release loops `for (int i=0; i<m_Height; i++) delete [] m_ppFilter[i];` (CFilter.cpp:59-60) using the just-overwritten m_Height. Reloading a filter whose file declares a larger height than the currently held one runs `delete[]` over indeterminate pointers past the end of the old m_ppFilter array — an arbitrary free driven by file content. This is the one Release-ordering bug of its kind in the area: CSprite565/555, CAlphaSprite565, CIndexSprite565 and CShadowSprite all correctly call Release() before reading the header.

**Recommendation:** Read the header into locals and only assign m_Width/m_Height after Init() has released the old buffers, matching the ordering used by the sprite loaders.

#### 🔴 Critical -- CSprite555::LoadFromFile decodes the RLE structure in place with no bound against the buffer it just allocated, producing an out-of-bounds heap read and write.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSprite555.cpp:182`

> ✅ **Fixed** in `cfc6cd6`, with the same bound applied to CAlphaSprite555 and CIndexSprite555 in `964c043`. Covered by `tests/unit/test_csprite555.cpp` and `tests/unit/test_sprite555_siblings.cpp`.

Line 181-183 read a WORD `len` from the file, allocate `new WORD[len]` and read len WORDs into it. Lines 185-204 then walk that buffer as RLE: `count = m_Pixels[i][0]`, and for each of `count` segments `colorCount = m_Pixels[i][index+1]`, then `m_Pixels[i][index] = ColorDraw::Convert565to555(m_Pixels[i][index])` for colorCount iterations. `index` is never compared against `len`. A file whose count/colorCount fields overstate the actual data walks index past the allocation and writes converted values there. If len is 0, `new WORD[0]` still returns a valid pointer and `m_Pixels[i][0]` is already out of bounds. Its sibling CSprite565::LoadFromFile (Client/SpriteLib/CSprite565.cpp:154) does have a `len > 0 && len <= 8192` guard, so this is the format-variant duplication hazard biting: the check was added to one of the pair only.

**Recommendation:** Apply the same length sanity check as CSprite565, and additionally bound `index` against `len` inside the segment walk in both files; on violation zero the line and stop parsing it.

#### 🔴 Critical -- CSpritePalBase::LoadFromFile builds the per-scanline pixel pointer table from unvalidated file offsets, so m_pPixels[] entries can point arbitrarily outside the allocation.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSpritePalBase.cpp:62`

> ✅ **Fixed** in `b7c4eb3`. The declared size is checked against the file, and the running scanline offset is kept inside the pixel data. Covered by `tests/unit/test_cspritepalbase.cpp`, which asserts the pointer invariant directly rather than waiting for a fault.

m_Size is a DWORD read verbatim from the file (line 53) and used unchecked as the allocation size: `m_pData = new BYTE[m_Size+sizeof(BYTE*)*m_Height]` (line 62). No read-failure check, no cap. Then lines 69-79 read an m_Height-entry WORD array from the file and accumulate it into a running pointer: `m_pPixels[i] = tempData; tempData += indexArray[i];` with no check that the accumulated offset stays inside m_Size. Every m_pPixels[i] beyond the point where the offsets exceed m_Size points into unrelated heap. Those pointers are dereferenced later by CAlphaSpritePal::Blt (see separate finding), which reads RLE opcodes from them and writes the decoded pixels into the locked backbuffer. Also note the addition `m_Size + sizeof(BYTE*)*m_Height` can wrap on a 32-bit build, yielding a tiny allocation for a huge declared size.

**Recommendation:** Cap m_Size against a sane maximum and against the remaining file length; verify each file.read() succeeded; and validate that the running sum of indexArray[] never exceeds m_Size before storing each m_pPixels[i], bailing to SetEmptySprite() on violation.

#### 🟠 High -- All four texture part managers acquire two surface locks (Lock plus GetSurfacePointer) but release only one, permanently leaving every created texture surface locked.

**Category:** resource-leak  |  **Location:** `Client/CTexturePartManager.cpp:409`

> ✅ **Closed in the 2026-09-17 follow-up.** The counting-lock implementation described here had already been replaced with an idempotent DirectDraw compatibility flag: `GetSurfacePointer()` does not acquire a lock, and `Unlock()` clears the flag. `tests/unit/test_spritesurface_bounds.cpp` now pins repeated `Lock()`, pointer access, one `Unlock()`, and release/reinitialization; the follow-up also clears the flag on `Release()`. No double SDL lock remains at the cited manager call sites.

CTexturePartManager::GetTexture calls `pTextureSurface->Lock()` at line 393, which increments m_lock_count and calls spritectl_lock_surface (Client/SpriteLib/CSpriteSurface_SDL.cpp:739-760). It then calls `pTextureSurface->GetSurfacePointer()` at line 409, which locks *again* and increments m_lock_count again (CSpriteSurface_SDL.cpp:718-737 — the function even prints 'GetSurfacePointer() is deprecated and leaks locks!' under _DEBUG). Only one `Unlock()` follows, at line 467. The surface therefore stays locked for its whole lifetime; SDL refuses to blit to or from a locked surface, and spritectl_blt_sprite's fallback path has to hand-unwind dest->locked in a loop to work around it (SpriteLibBackendSDL.cpp:643-652). The identical Lock+GetSurfacePointer+single-Unlock pattern appears in Client/CSpriteTexturePartManager.cpp:395/411, Client/CShadowPartManager.cpp:458/516, and Client/CNormalSpriteTexturePartManager.cpp:385/401. The GetSurfacePointer() return value is also not null-checked before the memset loop at line 417.

**Recommendation:** Use the pointer already returned by Lock() instead of calling GetSurfacePointer(), or add the matching Unlock(); null-check the pixel pointer before memset. Consider deleting GetSurfacePointer() outright now that its own implementation documents it as broken.

#### 🟠 High -- CTexturePartManager::GetTexture indexes m_pWidth/m_pHeight and m_EffectAlphaPPK with unvalidated sprite and palette ids.

**Category:** memory-safety  |  **Location:** `Client/CTexturePartManager.cpp:475`

> ✅ **Closed (2026-09-17):** removed the unused `CTexturePartManager`, `CSpriteTexturePartManager`, `CNormalSpriteTexturePartManager` and `CShadowPartManager` implementations. All object declarations were commented out; the only remaining cache call sites were behind the never-defined `__3D_IMAGE_OBJECT__`. Removed those branches while retaining their active sprite-drawing alternatives, and removed the type-size diagnostics and includes. `CPartManager` bounds and LRU behavior were already fixed and have regression tests. Source-reference audit and full Debug/ASan builds plus CTest verify this dead-code removal; no runtime defect reproduction is claimed.

m_pWidth and m_pHeight are allocated with `allSize = m_ASPK.GetSize()` entries (lines 97-98), but GetTexture reads `m_pWidth[id]` at line 475 and writes `m_pWidth[id] = width; m_pHeight[id] = height;` at lines 512-513 with no check that id < allSize. The `index` parameter is likewise passed straight to `m_EffectAlphaPPK[index]` at line 460, which resolves to the unchecked CTypePack::Get above. The same unchecked-index pattern reaches the base class: CPartManager::IsDataNotNULL / GetData / SetData all do `m_pPartIndex[index]` with no bound against m_nIndex (Client/CPartManager.h:101, 443, 325). Separately, `spWidth = pSprite->GetWidth()` at line 362 is executed before the `if (pSprite == NULL || ...)` guard at line 368.

**Recommendation:** Validate id against the m_pWidth/m_pHeight allocation size and index against m_EffectAlphaPPK.GetSize() at the top of GetTexture; add an index assertion inside CPartManager's accessors; move the null guard before the first dereference.

#### 🟠 High -- get_backend_alpha_sprite walks CAlphaSprite RLE data with no validation at all, unlike the CSprite path immediately above it which is fully validated.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSpriteSurface_Adapter.cpp:204`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-alpha-sprites`). Alpha sprites now retain each row's allocation length through both file formats, copy assignment and generated pixels. Both loaders read and validate complete rows against encoded length and decoded width before publishing a sprite; temporary ownership also makes truncated/allocation-failure cleanup safe. The adapter validates each stored span before decoding, including dirty cached sprites. Full malformed rows are consumed before rejection so the following sprite in a pack stays aligned. `tests/unit/test_alpha_sprite_bounds.cpp` covers byte-truncated files, width/length violations, packed-file alignment, mutated rows, generated and copied sprites, padding and self-assignment.

Lines 204-232 decompress each scanline: `int count = *pPixels++;` then per run `int transCount = *pPixels++; int colorCount = *pPixels++;` and, per colored pixel, two more `*pPixels++` reads. Nothing bounds count, transCount or colorCount, and nothing bounds the cumulative pointer advance against the size of the buffer CAlphaSprite565::LoadFromFile allocated (Client/SpriteLib/CAlphaSprite565.cpp:109, `new WORD[len]` with len straight from the file and no sanity check either). Only the destination write is guarded (`if (x < width)`). By contrast get_backend_sprite at lines 103-143 in this same file validates count, colorCount and rle_size before use — the hardening pass simply skipped the alpha variant.

**Recommendation:** Mirror the validation block from get_backend_sprite into get_backend_alpha_sprite (and the index/shadow paths), tracking consumed WORDs against the source line length and bailing to SPRITECTL_INVALID_SPRITE on inconsistency.

#### 🟠 High -- GetSurfaceInfo locks the surface, captures the pixel pointer, unlocks, and returns the pointer; every caller then writes through an unlocked surface.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSpriteSurface_SDL.cpp:479`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-surfaces`). The two blits and gamma drawing hold scoped SDL locks for their complete pixel access. Legacy borrowed-pointer accessors now return pixels only for uncompressed software surfaces, which are what `Init()` creates; they reject RLE surfaces instead of returning invalidated pixels. `GetDDSD()` uses the existing per-object description. `tests/unit/test_spritesurface_bounds.cpp` covers independent descriptions, actual RLE compression/decompression, blitting and gamma drawing, plus balanced locks.

Lines 481-486 do `spritectl_lock_surface(...); info->p_surface = sdl_info.pixels; ...; spritectl_unlock_surface(...)`. The returned p_surface is then used for direct pixel writes by Blt (line 562), BltNoColorkey (line 311) and GammaBox565 (line 697), all outside any lock. This violates the SDL contract: SDL_UnlockSurface on an RLE-accelerated surface re-encodes the pixel data and invalidates the pointer, and the pointer is not guaranteed stable for any surface where SDL_MUSTLOCK is true. It happens to work today only because these are plain SDL_CreateRGBSurface software surfaces. Relatedly, GetDDSD() at line 499 returns the address of a function-local `static S_SURFACEINFO`, so holding two GetDDSD() results (source and destination) silently aliases the same struct.

**Recommendation:** Have GetSurfaceInfo keep the lock and require a matching release, or convert the three call sites to use Lock()/Unlock() directly. Replace GetDDSD()'s static buffer with a caller-provided struct.

#### 🟠 High -- CSpriteSurface::Blt and BltNoColorkey clamp only the right/bottom edges, so negative destination or source coordinates index before the start of the pixel buffer.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSpriteSurface_SDL.cpp:549`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-surfaces`). Both methods use one implementation that intersects source bounds and the destination SDL clip rectangle before forming any pixel pointer. Wide arithmetic handles extreme signed coordinates, and overlapping self-copies preserve the original pixels. `tests/unit/test_spritesurface_bounds.cpp` exhausts small rectangles across every edge and covers empty/reversed rectangles, extreme coordinates, destination clips, pitch and self-overlap.

Lines 549-555 clamp src_w/src_h when `src_x + src_w > src_info.width` and when `pPoint->x + src_w > dst_info.width`, but neither src_x/src_y nor pPoint->x/pPoint->y is ever clamped to >= 0. The row pointers at lines 570-571 and 576-577 are then computed as `dst_pixels + (pPoint->y + y) * dst_pitch_words + pPoint->x` and `src_pixels + (src_y + y) * src_pitch_words + src_x`. With pPoint->x = -10 the memcpy destination starts 20 bytes before the surface allocation; with a negative pPoint->y it starts a whole pitch-multiple before it. BltNoColorkey at lines 298-333 is a verbatim copy of the same logic with the same omission. The clipping helper that would normally catch this, ClippingRectToPoint at line 400, is a stub that unconditionally returns true.

**Recommendation:** Clamp src_x, src_y, pPoint->x and pPoint->y to >= 0 (adjusting src_w/src_h and the source origin accordingly) before computing row pointers, and factor the shared body of Blt/BltNoColorkey into one function so the fix cannot drift again.

#### 🟠 High -- CTypePack::Get and CTypePack2::Get index m_pData and m_file_index without a usable bounds check on n.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CTypePack.h:141`

> ✅ **Fixed** in `355ef3f`. Both now range check before touching m_pData, and the late duplicate check inside CTypePack2's running-load branch is removed. Out of range access returns a shared empty element, since Get returns a reference and cannot report failure. Covered by `tests/unit/test_ctypepack.cpp`.

CTypePack::Get (lines 141-160) does `m_pData[n].IsInit()`, `m_file_index[n]` and `return m_pData[n]` with no comparison against m_Size at all, and no null check on m_pData (which stays NULL when Init() early-returns for size 0 at line 124). CTypePack2::Get does have a check, but it is placed at line 559 — *after* the out-of-bounds dereference `!m_pData[n].IsInit()` at line 549 has already happened, so it cannot prevent the fault it was written for. These are the accessors every sprite/palette lookup goes through: e.g. CTexturePartManager::GetTexture does `CAlphaSpritePal* pSprite = &m_ASPK[id];` (Client/CTexturePartManager.cpp:332) with an id that came from map/creature data.

**Recommendation:** Move the bounds and null checks to the top of both Get() overloads before any dereference of m_pData, and return a shared empty element on violation.

#### 🟠 High -- spritectl_load_sprite_from_file's RLE decode loop reads scanline_rle[y][rle_index++] without ever comparing rle_index to scanline_lengths[y].

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/SpriteLibBackendSDL.cpp:1286`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-rle`). The file loader validates each complete scanline against both its allocation length and decoded width before decoding. It rejects truncated headers/data and overflowing runs, and releases all temporary allocations on failure. `tests/unit/test_sprite_backend_bounds.cpp` covers every byte-truncated prefix, malformed later headers, excessive transparent/color runs, and a valid multi-segment sprite.

Lines 1273-1299 decode each scanline: `int count = scanline_rle[y][rle_index++];` then per segment `int trans_count = scanline_rle[y][rle_index++]; int color_count = scanline_rle[y][rle_index++];` and `row[x] = scanline_rle[y][rle_index++]` in the inner loop. The buffer was allocated with exactly scanline_lengths[y] WORDs (line 1257) but rle_index is never bounded by it — only `x < width` is checked, which constrains the writes but not the reads. A .spk whose segment counts exceed the stored scanline length reads arbitrarily far past the heap allocation. The sibling function spritectl_blt_sprite_rle at line 452 does at least attempt a header check, so the inconsistency is visible within the same file.

**Recommendation:** Bound rle_index against scanline_lengths[y] before each of the four reads and abandon the scanline on violation, the same way the blit path attempts to.

#### 🟠 High -- spritectl_blt_sprite_rle's header-only validation stops holding once the first segment consumes pixel data, allowing out-of-bounds reads in later segments.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/SpriteLibBackendSDL.cpp:452`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-rle`). A shared bounded scanline validator checks every visible row before the first destination write. The scanline setter also validates before replacing existing data and rejects lengths that cannot fit its 16-bit length field. Clipping arithmetic is widened before subtracting extreme coordinates and SDL lock failures abort drawing. Tests cover a malformed later row without partial drawing, rejected replacements preserving the old row, clipped valid rendering and extreme origins (`tests/unit/test_sprite_backend_bounds.cpp`).

Line 452 checks `if (rle_index + seg_count * 2 > rle_data_size)`, which would be sufficient only if rle_index advanced by exactly 2 per segment. But rle_index also advances by color_count in the pixel loop (line 531) and by `rle_index += color_count` in the skip path (line 471). After the first segment with a nonzero color_count, the header guarantee is void, and the unguarded `int trans_count = rle_data[rle_index++]; int color_count = rle_data[rle_index++];` at lines 463-464 can read past the end of the scanline buffer. The inner loop's guard at line 478 breaks out of the *pixel* loop but then falls back into the segment loop, which immediately performs those two unguarded reads.

**Recommendation:** Re-check `rle_index + 2 <= rle_data_size` at the top of the segment loop and bound `rle_index += color_count` at line 471 against rle_data_size; break out of the whole scanline rather than just the inner loop on overrun.

#### 🟡 Medium -- CSprite::Release dereferences m_Pixels without a NULL guard, unlike the equivalent Release in CShadowSprite.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/CSprite.cpp:121`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-release`). `CSprite::Release()` checks the row array before traversing it, and its constructors of row arrays plus the plain/indexed loaders value-initialize every pointer. Alpha loaders already publish fully owned rows transactionally in the preceding alpha-sprite fix. `tests/unit/test_sprite_release.cpp` reproduces the null-row-array state after an allocation failure and guards repeated cleanup of a partially allocated sprite. The test synthesizes those states; it does not inject allocator failures.

Lines 121-124 run `for (int i=0; i<m_Height; i++) delete [] m_Pixels[i]; delete [] m_Pixels;` with no `if (m_Pixels != NULL)` test. CShadowSprite::Release (Client/SpriteLib/CShadowSprite.cpp:70-83) guards the identical loop. Any state where m_Height is nonzero while m_Pixels is NULL crashes here — reachable if `new WORD*[m_Height]` throws in CSprite565::LoadFromFile (line 133) after m_Height was already assigned at line 112, since m_Height is only cleared on the next successful Release. The loaders also leave `new WORD*[m_Height]` uninitialized and fill it element by element, so an allocation failure partway through the loop leaves indeterminate pointers that a later Release will delete[].

**Recommendation:** Add the NULL guard and value-initialize the pointer array (`new WORD*[m_Height]()`) in all five LoadFromFile variants so a partial load cannot leave indeterminate pointers behind.

#### 🟡 Medium -- CSpriteSurface is declared twice in one header as two independent ~270-line class bodies selected by SPRITESURFACE_STANDALONE, an ODR landmine across targets.

**Category:** maintainability  |  **Location:** `Client/SpriteLib/CSpriteSurface.h:73`

> ✅ **Fixed in the 2026-09-17 follow-up** (`refactor/review-single-sprite-surface`). The unused DirectDraw-derived definition and all layout-selecting guards in `CSpriteSurface.h` are removed. Every consumer sees the same SDL class, and CMake no longer exports `SPRITESURFACE_STANDALONE`. `CSDLGraphicsFlip.cpp` stays in the executable because it connects the executable-owned `g_pBack` to the two rendering libraries; its old macro-layout rationale is removed. This is declaration/dead-code cleanup, verified by full Debug/ASan builds and the existing surface, alpha and palette tests.

Lines 73-341 define the standalone/SDL CSpriteSurface, and lines 350-579 define `class CSpriteSurface : public CDirectDrawSurface` for the legacy path; the two carry separate copies of the FUNCTION_EFFECT enum (lines 192 and 503), separate static tables (lines 249-250 and 572-573) and different member layouts. SPRITESURFACE_STANDALONE is set in exactly one place, `target_compile_definitions(SpriteLib PUBLIC ...)` in Client/SpriteLib/CMakeLists.txt:173-177; the DarkEden target itself defines only SPRITELIB_BACKEND_SDL (CMakeLists.txt:833-837). Any translation unit that pulls in this header without inheriting SpriteLib's PUBLIC interface gets the other layout, and the resulting mismatch is an ODR violation that links cleanly. Client/CSDLGraphicsFlip.cpp exists solely as a workaround for exactly this hazard — its header comment explains that dxlib is built without SPRITELIB_BACKEND_SDL and therefore cannot safely include CSpriteSurface.h.

**Recommendation:** Delete the dead CDirectDrawSurface-derived branch now that D3DLib/ is gone, leaving a single class definition; that also removes the need for the CSDLGraphicsFlip.cpp split.

#### 🟡 Medium -- BltAlphaSpritePal refuses to draw any sprite that is only partially on screen, so palette sprites vanish entirely at screen edges.

**Category:** correctness  |  **Location:** `Client/SpriteLib/CSpriteSurface_Adapter.cpp:624`

> ✅ **Fixed** in `6fd3f34`. The refusal is gone: `BltAlphaSpritePal` clips through `CAlphaSpritePal`'s existing `BltClip*` variants, chosen by a new `CSpriteSurface::ClipSpriteToSurface`. The coupling this finding describes was already broken by the C4 fixes — `CSpritePalBase::LoadFromFile` validates every scanline and `CAlphaSpritePal::Blt` bounds its walk — so the clamp no longer depends on the caller. Covered by `tests/unit/test_spritesurface_pal_blit.cpp`, which drives the static `BltAlphaSpritePalTo` against a plain array and checks every pixel outside the expected footprint untouched. The symptom in play was a tall bolt from the sky vanishing near the top edge: the 2026-09-03 log shows a 99x195 sprite at y=-104 dropped by this check. One thing to know when reading the clip variants: `CAlphaSpritePal::Blt` carries its own `pDataEnd` / width bound and the four `BltClip*` siblings do not, nor do the five `CSpritePal::BltEffect*` routines the screen path now reaches. They are safe because every sprite reaches them through `CSpritePalBase::LoadFromFile`, whose scanline validation is the layer that covers them (C4's third commit); a sprite built any other way would not be covered.

Lines 624-634 bail out with a warning whenever `pPoint->x < 0 || pPoint->y < 0 || pPoint->x + spriteWidth > surfaceWidth || pPoint->y + spriteHeight > surfaceHeight`, with the comment 'TODO: implement proper partial clipping'. Because CAlphaSpritePal::Blt itself does no clamping at all (see the critical finding above), this is currently the only thing preventing an out-of-bounds surface write — so the safety and the visual bug are coupled: fixing the pop-out requires fixing the clamp first. CAlphaSpritePal already provides BltClipLeft/BltClipRight/BltClipWidth/BltClipHeight (CAlphaSpritePal.cpp:517, 649, 746, 935) that are simply never called from the SDL adapter.

**Recommendation:** Wire the existing BltClip* variants into BltAlphaSpritePal for the partial cases, and add the width clamp inside CAlphaSpritePal::Blt so correctness does not depend on the caller's screening.

#### 🟡 Medium -- The SDL backend never populates the effect function tables, so SetEffect/SetPalEffect always install NULL and every sprite effect silently degrades to a plain copy.

**Category:** dead-code  |  **Location:** `Client/SpriteLib/CSpriteSurface_SDL.cpp:35`

> ✅ **Selection residual closed (2026-09-23):** the supported non-palette
> grayscale/gradation and palette screen effects are registered. Both selectors
> now reject table-end indices before reading a function pointer and issue an
> SDL rendering warning for unavailable selections. Warnings are bounded to one
> per effect and family, with a single invalid-ID bucket, preserving the existing
> copy fallback without a per-frame log flood. This follows the recommendation's
> explicit-diagnostic option for unported effects; unsafe 5-bit palette routines
> remain disabled on 565 surfaces. Two real-library tests reproduce an ASan
> global-buffer-overflow in `SetEffect(MAX_EFFECT)` and two missing-diagnostic
> checks, then cover repeated selection, fallback pixels and supported entries.
> The following paragraph describes the narrower original screen-effect fix.

> ✅ **Fixed** in `6fd3f34`, for the one entry any call site selects. `EFFECT_SCREEN` is registered in the palette table and `InitEffectTable` fills the screen tables; every `DRAW_NORMALSPRITEPAL_EFFECT` in `MTopView.cpp` passes `EFFECT_SCREEN`, and nothing else calls `SetPalEffect`. The other twelve palette routines stay at NULL on purpose: they assume three 5-bit channels, and `ColorDraw::Green` on this backend returns the 6-bit field — `memcpyPalEffectColorDodge` divides by `(32 - green)`. The non-palette table stays empty because its routines were never compiled for SDL (the header declares `memcpyEffectDarker` and siblings; no SDL translation unit defines them), which is what makes the `EFFECT_WIPE_OUT` call site this finding names a plain copy still. This was not merely dead code: it was the second of three defects behind every screen-blend skill effect drawing nothing — see *Runtime defects*. Filling the tables also exposed the third, the 32-wide green table under *Found by reading*.

s_pMemcpyEffectFunctionTable and s_pMemcpyPalEffectFunctionTable are defined as `{0}` at lines 35 and 37, and InitEffectTable() at lines 361-364 is an empty body. SetEffect (line 795) and SetPalEffect (line 800) therefore always assign NULL, and memcpyEffect (line 806) falls through to `dest[i] = src[i]`. Meanwhile the effect implementations exist and are compiled (Client/SpriteLib/CSpriteSurface_Effects.cpp defines memcpyPalEffectDarker, memcpyPalEffectGrayScale, memcpyPalEffectLighten, memcpyPalEffectDarken and more), and live call sites still invoke the API — e.g. Client/DrawCreatureEffect.cpp:882 calls `CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_WIPE_OUT)`. A contributor reading either side reasonably concludes the feature works.

**Recommendation:** Either populate the tables in InitEffectTable() from the functions already compiled in CSpriteSurface_Effects.cpp, or make SetEffect/SetPalEffect assert/log loudly so the gap is visible at the call sites.

#### 🟡 Medium -- DrawRect's RGB565-to-RGB555 conversion ORs three overlapping bit fields, bleeding green's low bit into blue's high bit.

**Category:** correctness  |  **Location:** `Client/SpriteLib/CSpriteSurface_SDL.cpp:180`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-surfaces`). The green mask is `0x07C0`; the format branch now tests bytes per pixel, since SDL reports RGB555 as 15 bits per pixel and previously bypassed that branch entirely. `tests/unit/test_spritesurface_bounds.cpp` checks low-green, saturated-green, white, red and blue on a real RGB555 surface.

Lines 180-182 compute `pixel = ((rgb565 & 0xF800) >> 1) | ((rgb565 & 0x0600) >> 1) | ((rgb565 & 0x07E0) >> 1) | (rgb565 & 0x001F);`. The three shifted terms are 0x7C00, 0x0300 and 0x03F0 — the last extends one bit below the 555 green field (0x03E0) into bit 4, which belongs to blue. Any pixel whose green LSB is set therefore corrupts blue's top bit, and the redundant 0x0600 term contributes nothing. The correct mask is `((rgb565 & 0x07C0) >> 1)`. Note the file also has an unconditional `if (rgb555_count < 3 && ...)` fprintf branch inside this hot path (lines 183-187).

**Recommendation:** Replace the three-term expression with `((rgb565 & 0xF800) >> 1) | ((rgb565 & 0x07C0) >> 1) | (rgb565 & 0x001F)`, and route the conversion through the existing spritectl_565_to_rgb/SDL_MapRGB path so there is only one implementation.

#### 🟡 Medium -- CTypePack2::m_bSecond is never initialized in the constructor but is read by Release() to choose which array type to delete[].

**Category:** undefined-behavior  |  **Location:** `Client/SpriteLib/CTypePack.h:514`

> ✅ **Already fixed in `6ee3e76b`; status reconciled 2026-09-17.** `CTypePack2` initializes `m_bSecond` to `false` in its constructor and assigns the selected pixel format before allocating either array type in `Init()`. The old claim below no longer describes the current constructor.

The constructor at lines 474-483 initializes m_pData, m_Size, m_bRunningLoad, m_nLoadData, m_file_index and m_file, but not m_bSecond. Release() at lines 514-517 reads it to pick between `delete [] ((Type2*)m_pData)` and `delete [] ((Type1*)m_pData)` — a read of an indeterminate value, and one that selects between two different array-delete semantics. Today this is masked by the `m_pData != NULL` guard at line 510 (m_pData is NULL on the first Release), but the flag is load-bearing for correctness of the delete and should not be indeterminate; the comment at line 512-513 acknowledges how delicate the type pairing is.

**Recommendation:** Initialize m_bSecond in the constructor initializer list, ideally to ColorDraw::Is565() so it matches what Init() will allocate.

#### 🟡 Medium -- LoadFromFilePart(SSM) and ReleasePart(list) never advance their iterator, so they process the first element GetSize() times.

**Category:** correctness  |  **Location:** `Client/SpriteLib/CTypePack.h:327`

> ✅ **Fixed** in `6f457e0`, completed in `6ee3e76`. Both iterators advance and list entries are range checked. `6f457e0` clamped `ReleasePart(int,int)` to m_Size on `CTypePack` but missed the identical overload on `CTypePack2` — which is the template the sprite packs actually instantiate and the one `MTopView` calls — so the unbounded write survived until an adversarial review caught it. The original `CSpriteSetManager` change had no running-load fixture. The current `test_ctypepack_indexed.cpp` exercises both variants through real running indexes, including mixed/repeated failures and all 65,536 set IDs.

CTypePack::LoadFromFilePart(const CSpriteSetManager&) at lines 329-334 obtains `iID = SSM.GetIterator()` and loops `for (int t=0; t<SSM.GetSize(); t++) { if(*iID != 0xFFFF) Get(*iID); }` — iID is never incremented. CTypePack::ReleasePart(COrderedList) at lines 352-357 has the identical defect, as do both CTypePack2 copies at lines 818-823 and 841-846. The effect is that a partial preload or partial release touches only the first sprite in the set and silently leaves every other requested sprite unloaded (or unreleased), which manifests as missing graphics or unbounded memory growth rather than a crash.

**Recommendation:** Increment iID at the end of each loop body, or convert these to range-based loops over the underlying container.

#### 🟡 Medium -- MPalette owns a raw WORD* and defines a destructor but no copy constructor, and its operator= frees the destination before reading the source.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/MPalette.h:12`

> ✅ **Fixed** in `d78dc21`. Copy constructor added and operator= returns early on self-assignment rather than releasing first. Covered by `tests/unit/test_mpalette.cpp`.

MPalette declares `WORD* m_pColor` (MPalette.h:43) and a destructor that deletes it (MPalette.cpp:11-14), but no copy constructor — so any copy-construction shallow-copies the pointer and the two objects double-free it. The user-written operator= (MPalette.cpp:35-45) calls Release() first and then does `memcpy(m_pColor, pal.m_pColor, m_Size*2)`, so a self-assignment `p = p` copies from the block it just freed; it also memcpys from pal.m_pColor without checking it is non-NULL, which it is for any default-constructed or empty palette. Palettes are handed around as `MPalette&` through the Blt* API (e.g. CSpriteSurface_Adapter.cpp:657) and stored in CTypePack containers, so a copy is one refactor away.

**Recommendation:** Add a copy constructor (or delete it explicitly), guard operator= against self-assignment, and null-check pal.m_pColor before memcpy.

#### 🟡 Medium -- spritectl_create_sprite accepts any width/height/data_size combination, and the consumers then read width*height elements from a buffer sized only data_size.

**Category:** memory-safety  |  **Location:** `Client/SpriteLib/SpriteLibBackendSDL.cpp:270`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-rle`). Raw sprite creation accepts only positive dimensions, supported formats, a pixel count representable by its integer consumers, and a buffer large enough for every pixel at the format's byte width. `tests/unit/test_sprite_backend_bounds.cpp` tests short buffers, zero/negative/overflowing dimensions, unknown formats, and valid RGB565/RGB555/RGBA32 data.

spritectl_create_sprite (lines 270-305) copies data_size bytes but never checks data_size against width*height*bytes-per-pixel, nor that width/height are positive. spritectl_blt_sprite then reads `sprite->width * sprite->height` uint16 elements out of sprite->pixels via spritectl_convert_565_to_rgba (lines 693-698), and for the RGBA32 case takes `pixel_src = (const uint32_t*)sprite->pixels` and memcpys width*height*4 bytes (lines 710, 736-741) — both over-read whenever data_size is smaller. Related in the same file: `pixel_count = width * height` at line 1240 is evaluated in int after integer promotion of two uint16 values, so a 65535x65535 header overflows signed int (UB) before reaching size_t.

**Recommendation:** Validate width > 0, height > 0 and data_size >= (size_t)width*height*bpp in spritectl_create_sprite and reject otherwise; compute pixel_count as `(size_t)width * (size_t)height`.

#### ⚪ Low -- The LRU counter-wrap normalization subtracts a value that becomes zero partway through the loop, so half the timestamps are left un-normalized.

**Category:** correctness  |  **Location:** `Client/CPartManager.h:513`

> ✅ **Already fixed by the audio/media pass; status reconciled 2026-09-17.** `CPartManager::GetData` snapshots `leastTime` before modifying any timestamps. `CPartManager.RolloverNormalizesEveryTimestamp` in `tests/unit/test_part_manager.cpp` pins every slot and the resulting counter. This duplicates the rollover defect already recorded as fixed in the Input, Audio & Media finding.

Lines 512-516: `int leastTime = m_pLastTime[leastTimeIndex]; for (int i=0; i<m_nPart; i++) m_pLastTime[i] -= m_pLastTime[leastTimeIndex];`. The loop reads m_pLastTime[leastTimeIndex] fresh on every iteration, so once i reaches leastTimeIndex that entry becomes 0 and every subsequent entry has 0 subtracted from it. The local `leastTime` that holds the correct value is computed and then never used. After a wrap the LRU ordering is corrupted for all entries after leastTimeIndex, causing the wrong texture to be evicted. Reached only when m_Counter hits 0xFFFFFFFF, so impact is low, but the fix is one token.

**Recommendation:** Use the already-computed `leastTime` local in the subtraction loop.

#### ⚪ Low -- Several source comments are irrecoverable mojibake rather than readable Korean or English.

**Category:** maintainability  |  **Location:** `Client/SpriteLib/CSpriteSurface_Effects.cpp:56`

> ✅ **Fixed in the 2026-09-17 follow-up** (`refactor/review-single-sprite-surface`). The corrupted effect comments and filter-member comment are rewritten in English from their implementations. The effect notes distinguish the registered RGB565 screen blend from unregistered legacy routines, including their 5-bit channel assumptions and raw palette-index copies. A non-comment-line comparison confirms this documentation cleanup changes no code.

Comments such as line 56 ('source --> dest �� pixels��ŭ \u3designĪ�ȿ�� ó���� \u3d���.') and Client/SpriteLib/CSprite.h:328 ('// Filter 점쓹옆점쓹옆') have been through a double encoding conversion and no longer decode to anything in any charset, so the original explanation of these pixel-blending routines is lost. CLAUDE.md states this fork should be English-only and that comments should be converted where possible; these particular ones cannot be converted, only rewritten from the code.

**Recommendation:** Rewrite the mojibake comments in English from the code's actual behaviour rather than attempting recovery, prioritizing the blending functions where the semantics are non-obvious.

#### ⚪ Low -- FillSurface indexes the surface linearly by width*height, ignoring pitch, so it under-fills any surface with row padding.

**Category:** correctness  |  **Location:** `Client/SpriteLib/CSpriteSurface_SDL.cpp:601`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-surfaces`). Fill walks each row by pitch under a scoped SDL lock. The test uses a three-pixel-wide surface with padding, verifies every visible pixel and verifies that the padding remains unchanged (`tests/unit/test_spritesurface_bounds.cpp`).

Lines 601-607 do `WORD* pixels = (WORD*)info.pixels; int pixel_count = info.width * info.height; for (i...) pixels[i] = color;`. SDL aligns 16bpp surface pitch to 4 bytes, so an odd-width surface has pitch = width*2 + 2 and the linear walk drifts one pixel left per row, leaving the right edge unfilled and smearing the fill diagonally. GammaBox565 in the same file (line 697) correctly steps by info.pitch, so the two are inconsistent.

**Recommendation:** Walk row by row using info.pitch, as GammaBox565 already does.

#### ⚪ Low -- spritectl_shutdown calls SDL_Quit(), tearing down the video subsystem that DXLib's window and renderer also depend on.

**Category:** maintainability  |  **Location:** `Client/SpriteLib/SpriteLibBackendSDL.cpp:68`

> ✅ **Fixed in the 2026-09-17 follow-up** (`fix/review-sprite-surfaces`). Initialization and shutdown now pair `SDL_InitSubSystem(SDL_INIT_VIDEO)` with `SDL_QuitSubSystem(SDL_INIT_VIDEO)`. The test holds separate video and timer references and verifies both survive SpriteLib shutdown (`tests/unit/test_spritesurface_bounds.cpp`).

spritectl_init calls SDL_Init(SDL_INIT_VIDEO) at line 41 and spritectl_shutdown calls SDL_Quit() at line 68. SDL_Quit shuts down every subsystem, not just the one this module initialized, so whichever of SpriteLib or DXLib shuts down first invalidates the other's window/renderer — CSDLGraphics::Flip (Client/CSDLGraphicsFlip.cpp:26) dereferences m_pSDLRenderer with only a NULL check, not a validity check. spritectl_destroy_surface (line 182) also frees the SDL_Surface without checking surface->locked, which given the part-manager lock leak above means locked surfaces are routinely freed.

**Recommendation:** Use SDL_QuitSubSystem(SDL_INIT_VIDEO) paired with the SDL_InitSubSystem this module actually performed, and leave SDL_Quit to whichever component owns application shutdown.

---

## Networking & Protocol

**Grade:** D  |  **Findings:** 27

**Scope:** Networking and protocol — Client/Packet/ (packet classes, factory, socket streams) and Client/ socket/connection handling

**Health assessment:** This is the client's primary attack surface and it is in poor shape. The framing layer is sound in outline — a bounded packet-ID table, a per-ID max-size cap, and a completeness check before dispatch — but that safety net stops at the packet header. Nothing bounds a packet's own `read()` to its declared size, so every length prefix and array index inside a packet body is effectively unvalidated unless the individual packet class remembers to check it, and many do not. The result is a set of directly reachable remote memory-corruption bugs from a malicious or compromised game server: two unchecked network-supplied array indices that write structs (including `std::vector`/`std::list` operations) far outside fixed arrays, at least four `strcpy`/`sprintf` of unbounded server strings into 128-byte buffers, and a peer-supplied filename used verbatim for `open`/`rename`/`remove` on the local disk. Supporting infrastructure adds more: the "encryption" is dead code that returns before doing anything, credentials cross the wire in cleartext, `flush()` silently discards unsent bytes, and three of the four packet-dispatch loops parse an uninitialized header buffer because they ignore `peek()`'s return value. Validation quality varies wildly packet-to-packet — some classes check every length carefully, neighbours check nothing, and several checks are dead (`BYTE > 255`), which suggests the checks were copy-pasted rather than reasoned about.

#### 🔴 Critical -- An unbounded server-supplied guild name is sprintf'd into a 128-byte stack buffer.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCGuildChatHandler.cpp:58`

GCGuildChat::read (Client/Packet/Gpackets/GCGuildChat.cpp:27-29) reads `BYTE szGName` and does `if (szGName != 0) iStream.read(m_SendGuildName, szGName)` with no upper bound — the sender name (line 38) and message (line 51) are bounded, but the guild name is not. GCGuildChatHandler.cpp:57-58 then does `char szName[128]; sprintf(szName, "[%s]%s", pPacket->getSendGuildName().c_str(), pPacket->getSender().c_str());`. Worst case output is 255 (guild) + 10 (sender) + 3 (brackets) + NUL = 269 bytes into a 128-byte buffer.

**Failure scenario:** Server sends PACKET_GC_GUILD_CHAT with m_Type != 0 (union chat), a 255-byte guild name and a 10-byte sender. sprintf overflows szName by ~141 bytes of attacker-controlled data on the stack.

**Recommendation:** Bound szGName in GCGuildChat::read (the write path at line 71 should get a matching check), and switch the handler to snprintf or std::string concatenation.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`). The guild name is capped at 20 bytes -- the declared layout bound, matching the server repo's header -- on both read and write, and both handler copies are snprintf-bounded.

#### 🔴 Critical -- GCPartySay reads name and message with no length validation, and the handler strcpy's both into 128-byte stack buffers.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCPartySayHandler.cpp:37`

GCPartySay::read (Client/Packet/Gpackets/GCPartySay.cpp:22-27) reads a BYTE `szName`, then `iStream.read(m_Name, szName)`, then reuses the same variable for the message: `iStream.read(szName); iStream.read(m_Message, szName)`. There is no `> N` bound on either — unlike sibling chat packets such as GCSay and GCWhisper. Both strings can therefore be 255 bytes. GCPartySayHandler.cpp:35-38 then declares `char str[128]; char strName[128];` and does `strcpy(str, pPacket->getMessage().c_str()); strcpy(strName, pPacket->getName().c_str());`. Note that SocketInputStream::read(std::string&,uint) truncates at the first embedded NUL (SocketInputStream.cpp:196-199), so an attacker simply sends 255 non-zero bytes to keep the full length. The declared packetSize cap (GCPartySay.h:96 returns 154) does not constrain the sub-reads, which draw from the shared stream buffer beyond the packet boundary.

**Failure scenario:** Server sends PACKET_GC_PARTY_SAY with a 255-byte non-NUL name and message. strcpy writes 256 bytes into a 128-byte stack array, smashing the saved return address / stack cookie region with attacker-controlled bytes.

**Recommendation:** Add explicit `szName > 20` / `szMessage > 128` checks in GCPartySay::read (matching GCSay.cpp:32 and GCWhisper.cpp:29,43), and replace the strcpy calls with a bounded copy or use std::string directly.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`). read() enforces the declared 20/128 layout like GCSay and GCWhisper, and the handler copies are snprintf-bounded. CGPartySay::write gained the matching 128 cap in `3bc340e` so this client cannot trip the new guard on other clients.

#### 🔴 Critical -- GCShopList::read uses a raw network-supplied BYTE as an index into a 20-element array with no bounds check, writing a full struct out of bounds.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCShopList.cpp:65`

`iStream.read(index)` (line 63) reads an unvalidated BYTE (0-255), then line 65 does `_SHOPLISTITEM& item = m_pBuffer[index];`. `m_pBuffer` is declared `SHOPLISTITEM m_pBuffer[SHOP_RACK_INDEX_MAX]` (GCShopList.h:84) and `SHOP_RACK_INDEX_MAX` is 20 (Client/Packet/Types/ShopTypes.h:31). Lines 67-85 then write objectID, itemClass, itemType, durability, silver, grade, enchantLevel and `bExist` through that reference, and line 77 calls `item.optionType.push_back(optionType)` — invoking std::vector member functions on whatever memory lies past the array. The outer `packetSize > getPacketMaxSize()` check in ClientPlayer.cpp:256 does not help: a packet of legal declared size can carry index=255.

**Failure scenario:** A malicious or compromised game server sends PACKET_GC_SHOP_LIST with nTotal=1 and index=0xFF. The client writes ~40 bytes of attacker-chosen data roughly 235 elements past the end of m_pBuffer, then calls std::vector::push_back on the out-of-bounds 'optionType' member, dereferencing attacker-influenced pointers. This is a controlled heap write leading to likely arbitrary code execution.

**Recommendation:** Reject the packet when `index >= SHOP_RACK_INDEX_MAX` (throw InvalidProtocolException) before touching m_pBuffer, exactly as the packet-ID bound is enforced in PacketFactoryManager::createPacket.

> ✅ **Fixed** in `ed4f872` (PR #4). The index is rejected with InvalidProtocolException before the array is touched. Regression guard, executable-only code with no test path.

#### 🔴 Critical -- GCStashList::read indexes four parallel 3x20 arrays with two unvalidated network-supplied BYTEs.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCStashList.cpp:92`

Lines 90-91 read `rack` and `index` as raw BYTEs from the stream with no validation. Line 92 (`m_pItems[rack][index]`), line 114 (`iStream.read(m_SubItemsCount[rack][index])`), line 119 (`m_pSubItems[rack][index].push_back(pSubItemInfo)`) and line 122 (`m_bExist[rack][index] = true`) all index arrays dimensioned `[STASH_RACK_MAX][STASH_INDEX_MAX]` (GCStashList.h:94-97), i.e. [3][20] per Client/Packet/Types/ItemTypes.h:224,230. With rack and index each up to 255 the effective offset reaches 255*20+255 elements past the base. Line 119 additionally invokes `std::list::push_back` on an out-of-bounds list object.

**Failure scenario:** Server sends PACKET_GC_STASH_LIST with nTotal=1, rack=0xFF, index=0xFF. The client writes a STASHITEM struct and a bool tens of kilobytes past the arrays, then calls std::list::push_back through an out-of-bounds list head, corrupting the heap with attacker-controlled pointers.

**Recommendation:** Validate `rack < STASH_RACK_MAX && index < STASH_INDEX_MAX` immediately after reading them and throw InvalidProtocolException otherwise. The same guard is needed on the accessors at lines 318, 331 and 363.

> ✅ **Fixed** in `ed4f872` (PR #4). read() rejects out-of-range rack/index with InvalidProtocolException, and the Assert-only accessors gained release-build range checks. The setStashItem site is `#ifdef __GAME_SERVER__` and does not compile into the client; it remains a live issue in the server repository.

#### 🔴 Critical -- A 255-byte-capable server message is strcpy'd into 128-byte static buffers in two places.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCSystemMessageHandler.cpp:100`

GCSystemMessage::read (Client/Packet/Gpackets/GCSystemMessage.cpp:28) guards with `if (szMessage > 256)`, but szMessage is a BYTE whose maximum value is 255 — the check can never fire, so messages up to 255 bytes are accepted. GCSystemMessageHandler.cpp:27 declares `static char previous1[128]` and line 100 does `strcpy(previous1, pPacket->getMessage().c_str())`; line 108 declares `static char previous[128]` and line 161 does `strcpy(previous, pPacket->getMessage().c_str())`. Both overflow by up to 128 bytes into adjacent static storage. Separately, line 119-120 allocates `new char[strlen(message)+20]` and sprintf's a format string pulled from the game string table into it, assuming the format never expands by more than 20 characters.

**Failure scenario:** Server sends PACKET_GC_SYSTEM_MESSAGE with type SYSTEM_MESSAGE_PLAYER and a 255-byte message. strcpy writes 256 bytes into previous1[128], corrupting 128 bytes of the .bss/.data segment following it.

**Recommendation:** Replace both static char arrays with std::string, or use a bounded copy. Also fix the dead `> 256` check to a real limit such as `> 127`.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`). Both statics are std::string; the dead guard is replaced by a comment (a 255-byte message is legal and every sink was verified bounded) and the `+20` allocation is sized from the format string. `3bc340e` additionally fixed the dangling `c_str()`-of-temporary pointers in the RANGER_CHAT and PLAYER cases of the same handler.

#### 🔴 Critical -- A filename received verbatim from a remote peer is used to create, truncate, rename and delete files on the local disk with no path validation.

**Category:** security  |  **Location:** `Client/RequestFileManager.cpp:91`

RCRequestedFileInfo::read (Client/Packet/Rpackets/RCRequestedFile.cpp:40-50) reads a filename off the wire; its only guard is `if (szFilename > 255)` on a BYTE, which is dead code. RCRequestedFileHandler.cpp passes `pFileInfo->getFilename().c_str()` straight into `new ReceiveFileInfo(...)`, which stores it as m_Filename (RequestFileManager.cpp:50). StartReceive then opens it for writing with ios::trunc at line 91, and EndReceive calls `rename(m_FilenameTemp, m_Filename)` at lines 129/137/153/160 and `remove(m_Filename)` / `remove(m_FilenameTemp)` at lines 135/143/158/165. No component of the path is sanitized — no rejection of '..', absolute paths, or drive letters. The peer here is another game client whose IP the client learned from GCRequestedIP, i.e. fully untrusted.

**Failure scenario:** A malicious peer answers a profile request with RCRequestedFile carrying filename "..\\..\\DarkEden.exe" (or any path under the user's profile). The client truncates and overwrites that file with peer-supplied bytes, then renames the temp file over it — arbitrary file write, and via remove(), arbitrary file deletion.

**Recommendation:** Reject any filename containing a path separator, '..', a drive letter or a leading separator, and force all received files into a fixed download subdirectory. Validate before constructing ReceiveFileInfo.

> ✅ **Fixed** in `76a1185`, reworked in `3bc340e`, and hardened again in `1200625` (branch `harden/network-input`). The blanket separator ban in the first attempt broke legitimate transfers (the wire value is the sender's relative profile path); the second attempt allowed interior separators but a second adversarial review showed it still blocked only escape, not scope -- a peer could write a benign relative name such as `zz.dll` beside the executable. The handler now ignores the peer's directory entirely: it keeps only the leaf name, requires a `.spk`/`.spki` extension (plus the Win32 device-name and trailing-dot checks), and re-roots the file under the client's own `DIR_PROFILE`, so the peer controls the filename but never the location. The reject path frees the transfer state before disconnecting.

#### 🟠 High -- m_pDatagramSocket is deliberately left NULL when UDP socket creation fails, but every user dereferences it unconditionally.

**Category:** correctness  |  **Location:** `Client/Packet/ClientCommunicationManager.cpp:114`

The constructor catches socket-creation failure and sets `m_pDatagramSocket = NULL` at line 42, with a comment saying it will 'continue without P2P communication'. Nothing then checks it: sendDatagram dereferences it at line 76, sendPacket at line 114, and Update() at line 156 — and Update() is called every frame from the main loop.

**Failure scenario:** A second instance of the client is already bound to CLIENT_COMMUNICATION_UDP_PORT (or the port is otherwise unavailable). The constructor swallows the BindException and sets the pointer to NULL; the very next frame Update() calls m_pDatagramSocket->receive() and the client crashes on startup with a null dereference.

**Recommendation:** Guard all three call sites with `if (m_pDatagramSocket == NULL) return;`, or add an `IsAvailable()` accessor the caller must consult.

> ✅ **Fixed** on branch `harden/high-severity-batch1`. All three sites return early. The pointer is private with only three users in one file, so an `IsAvailable()` accessor would have added API for no caller. `sendDatagram`/`sendPacket` log one line per dropped send; `Update()` is deliberately silent because it runs every frame and would otherwise write a log line forever. P2P (whisper-by-IP, profile transfer) is now quietly unavailable when the bind fails, which is what the constructor's comment always claimed. Regression guard, executable-only code.

#### 🟠 High -- The declared packetSize is validated but never used to bound a packet's own read(), so any packet body can consume arbitrary amounts of the stream.

**Category:** protocol-design  |  **Location:** `Client/Packet/ClientPlayer.cpp:265`

> ✅ **Already fixed; reconciled 2026-09-17:** framed reads now set `m_FrameRemaining`, constrain every body operation and restore the next-frame boundary after rejection. A swallowed inner read error also rejects the outer frame (`c5ff2e5e`). `test_packetwire_parsers.cpp` covers over-consumption, under-consumption, fragmented bodies, every body operation, swallowed errors and standard exceptions; `test_player_base.cpp` proves malformed bodies never dispatch or leak. These regressions continue to pass with the receive-buffer limits below.

processCommand checks `packetSize > getPacketMaxSize(packetID)` (line 256) and `length() < szPacketHeader + packetSize` (line 265), then calls `m_pInputStream->read(pPacket)` (line 280), which skips the header and hands control to the packet's virtual read(). SocketInputStream::read bounds each call only against `length()` — the total bytes buffered from the socket — not against the current packet's declared size. A packet class that reads a length prefix of 255 when the declared body was 20 bytes simply consumes 235 bytes belonging to subsequent packets, all of them attacker-supplied. This is why the per-packet length checks in GCPartySay, GCGuildChat and GCPartyLeave are load-bearing and why their absence is directly exploitable. It also means a packet whose read() over-runs leaves the stream head mid-packet, permanently desynchronising the connection.

**Failure scenario:** Server sends GCPartySay declaring the legal maximum body size, but with a name-length prefix of 255. The client reads 255 bytes across the packet boundary into m_Name, then the handler strcpy's it into char[128]. The size cap that appears to protect the parse provides no protection at all.

**Recommendation:** Give SocketInputStream a per-packet limit (e.g. a scoped sub-stream or a read cursor bounded to szPacketHeader+packetSize) so that any read() overrunning its own packet raises InvalidProtocolException, and skip to the next packet boundary on failure instead of leaving the head mid-stream.

#### 🟠 High -- Handlers store c_str() of a by-value std::string accessor, leaving a dangling pointer used for the rest of the function.

**Category:** correctness  |  **Location:** `Client/Packet/Gpackets/GCPartyLeaveHandler.cpp:38`

`GCPartyLeave::getExpeller()` and `getExpellee()` return `std::string` by value (Client/Packet/Gpackets/GCPartyLeave.h:34,37). Lines 38-39 of the handler capture `pPacket->getExpeller().c_str()` and `pPacket->getExpellee().c_str()` into `const char*` locals; both temporaries are destroyed at the end of their full-expressions, so pExpeller/pExpellee dangle for the ~100 lines that follow (dereferenced at lines 56, 61, 74, 78, 94, 96, 100, 118, 132). The same pattern appears at GCAddVampirePortalHandler.cpp:25, GCRequestedIPHandler.cpp:37, and GCSystemMessageHandler.cpp:56 and :65 — the last two are used at lines 59 and 98 after the temporary has died. GCSystemMessageHandler.cpp:110-113 carries a comment explicitly noting this class of bug was fixed in one spot, so the pattern is known but was not swept.

**Failure scenario:** On a party-leave packet the small-string-optimised temporary goes out of scope; subsequent strcmp/sprintf reads (line 132: `sprintf(str, fmt, pExpeller, pExpellee)`) read freed or reused stack/heap memory, producing garbage names or a crash. Under ASan this is a reported use-after-free. Because GCPartyLeave::read (lines 21-27) applies no length bound to either name, the same sprintf into `char str[256]` (line 41) is also an overflow candidate.

**Recommendation:** Bind the accessor results to `const std::string` locals (as GCSystemMessageHandler.cpp:112 now does) or change the accessors to return `const std::string&`. Add length bounds to GCPartyLeave::read.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, with two corrections to the finding. Both `GCSystemMessageHandler` sites it names were **already fixed** in `3bc340e`. Against that, the first sweep found three the review did not name — `GCNPCInfoHandler.cpp:50`, `GCUpdateInfoHandler.cpp:793` and `GCModifyNicknameHandler.cpp:43` — and adversarial review then found **three more the sweep itself had missed**: `GCUpdateInfoHandler.cpp:319` (the same accessor and the same `SetNickName` sink as the site 474 lines below that the sweep *did* fix), `CRWhisperHandler.cpp:72`, and `RequestServerPlayerManager.cpp:192`, the last two binding `Socket::getHost()`, a by-value accessor the sweep never looked for. Eight sites in total. All are bound to named locals rather than changing the accessors to return a reference, because those headers are the wire-layout inventory's input and share their shape with the server repo.
>
> The lesson for the next sweep of this pattern: searching for the *known* by-value accessors finds the sites you already know about. What actually enumerates it is going the other way — list every accessor in the packet and info headers that returns `std::string` by value, then find every place its `c_str()` is stored rather than consumed in the same full-expression.
>
> One of those sites carried a second defect that this batch's own `CTypeTable` change had just made dangerous: `GCUpdateInfoHandler.cpp` indexed `g_pNickNameStringTable` with a raw wire `WORD` and no clamp, where its four siblings all clamp against `GetSize()`. Out of range now yields a default `MString` whose `GetString()` is NULL, and that NULL flowed straight into a `std::string` assignment. Clamped, with a NULL guard behind it.
>
> `GCPartyLeave::read` gained the length bound in the same change. The limit is derived, not chosen: `GCPartyLeaveFactory::getPacketMaxSize()` is `szBYTE*2 + 20` and the server's copy computes the same value (the text differs; "byte-identical" was overstated), and `ClientPlayer::processCommand` already rejects anything declaring more — so the two names together cannot legitimately exceed 20 bytes, and the guard enforces that as a *combined* budget rather than a per-name round number. It therefore cannot reject anything the existing size check accepts.
>
> Left as-is: `GCPartyLeaveHandler.cpp:63`'s `pExpeller==NULL` test is vacuous and always was, since `c_str()` never returns NULL. Noted rather than changed, because removing it alters control flow for no safety gain.

#### 🟠 High -- GCShopListMysterious::read indexes a 20-element array with an unvalidated network BYTE, the same defect as GCShopList.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCShopListMysterious.cpp:63`

Lines 63-67: `iStream.read(index); iStream.read(m_pBuffer[index].itemClass); iStream.read(m_pBuffer[index].itemType); m_pBuffer[index].bExist = true;`. `index` is a BYTE read straight off the wire; `m_pBuffer` is `SHOPLISTITEM_MYSTERIOUS m_pBuffer[SHOP_RACK_INDEX_MAX]` (GCShopListMysterious.h:77) with SHOP_RACK_INDEX_MAX == 20. No bounds check exists at the read site or at the accessors on lines 200 and 216.

**Failure scenario:** Server sends PACKET_GC_SHOP_LIST_MYSTERIOUS with index=0xFF; the client writes itemClass, itemType and a bool at an offset ~235 elements past the end of the member array, corrupting whatever follows the packet object.

**Recommendation:** Add `if (index >= SHOP_RACK_INDEX_MAX) throw InvalidProtocolException(...)` before the writes.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, following `ed4f872`'s house pattern for the `GCShopList` twin. **The finding is wrong about the accessors.** It states "No bounds check exists at the read site or at the accessors on lines 200 and 216" — both accessors *are* guarded — three lines above the first cited line and six above the second. The reviewer quoted the use lines and missed the guards. Only the read site needed fixing. (`setShopItem` was `#ifndef __GAME_CLIENT__` and never compiled into the client, the same situation `ed4f872` recorded for `GCStashList`; task 5.2's ninth slice deleted it with the rest of the server halves on 2026-09-13.)
>
> Recorded latent, present in both this file and `GCShopList.cpp:260,273`: those accessors `throw` a string literal from functions declared `throw()`. MSVC treats `throw()` as `__declspec(nothrow)`, so an escaping exception is undefined behaviour rather than a clean `std::unexpected`. Left alone to keep the two files identical, but it is a real defect in both.

#### 🟠 High -- Chat handlers strcpy a 128-byte-maximum server string into a 128-byte buffer, overflowing by the NUL terminator.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCWhisperHandler.cpp:36`

GCWhisper::read (Client/Packet/Gpackets/GCWhisper.cpp:43) rejects only `szMessage > 128`, so a 128-byte message is accepted. GCWhisperHandler.cpp:34 declares `char str[128]` and line 36 does `strcpy(str, pPacket->getMessage().c_str())` — 128 characters plus the terminator is 129 bytes. The identical pattern appears in Client/Packet/Rpackets/RCSayHandler.cpp:37-39 (`char str[128]`, RCSay message capped at 128 in RCSay.cpp:43). Line 98 of GCWhisperHandler additionally does `sprintf(strWhisperID, "%s ", getName().c_str())` into `char strWhisperID[128]`, appending a space to a name of up to 10 bytes (safe there, but the same idiom elsewhere is not).

**Failure scenario:** Server sends a whisper whose message is exactly 128 non-NUL bytes. strcpy writes str[128] = '\0', one byte past the array — a classic off-by-one that on MSVC lands on the stack cookie or an adjacent local.

**Recommendation:** Either tighten the packet check to `> 127`, or size the buffers 129+ / use bounded copies. Applying the same fix to RCSayHandler.cpp is required.

> ✅ **Fixed** on branch `harden/high-severity-batch1` by the second option, deliberately. Tightening `read()` to `> 127` would reject a message the factory's own `getPacketMaxSize()` budgets 128 bytes for and that this repo's `write()` will happily produce — the same shape as the guild-name cap of 20 that `1200625` had to undo after it started disconnecting legitimate sessions. All three handlers now use `char str[128 + 1]` with `snprintf`, and the name buffers are sized from each packet's own name cap (10 for `GCWhisper` and `CRWhisper`, 20 for `RCSay`).
>
> **Three, not the two the finding names.** `CRWhisperHandler.cpp` — the peer-to-peer whisper handler sitting in the same directory as `RCSayHandler`, whose `CRWhisper::read` applies the identical `> 128` cap — was missed by the first pass and found by adversarial review. It is the same one-byte overflow, on data arriving from another client over UDP rather than from the server.
>
> The name buffers shrinking from 128 is the load-bearing part, so it was traced rather than assumed: the only writer of each is the `snprintf` beside it, and the consumers are `strncmp`, `IsAcceptID(const char*)`, `CMessageArray::AddFormat` (varargs, read-only) and `UI_AddChatToHistory`. That last one is declared `char*` and carries a comment saying it must not be const, so its whole chain was followed — `UI_AddChatToHistory` → `C_VS_UI::AddChatToHistory` → `C_VS_UI_GAME::AddChatToHistory` are all pure forwarders, and the terminal `C_VS_UI_CHATTING::AddChatToHistory` takes `const char*`. Nothing writes back through either pointer.

#### 🟠 High -- The wire encryption is dead code — both EncryptData implementations return before the XOR loop, so all traffic including login credentials is cleartext.

**Category:** security  |  **Location:** `Client/Packet/SocketInputStream.cpp:769`

SocketInputStream::EncryptData (line 766) begins with `return EncryptKey;` at line 769, before the `*(buf+i) ^= 0xCC` loop at 771 and the hash-table XOR at 776. SocketOutputStream::EncryptData (Client/Packet/SocketOutputStream.cpp:333) has the identical unconditional `return EncryptKey;` at line 336. Every caller in fill() and flush() is therefore a no-op. The alternative Encrypter class (Client/Packet/Encrypter.h) is a single-byte XOR keyed on zoneID/serverID (ClientPlayer.cpp:493-501) and is only applied to individual fields via readEncrypt/writeEncrypt, not to the login stream. CLLogin::write (Client/Packet/Cpackets/CLLogin.cpp:74-83) writes the account password through the plain SocketOutputStream.

**Failure scenario:** Anyone on the network path between the client and the login server reads account IDs and passwords directly off the wire, and can trivially forge or modify any packet in either direction — which is what makes every parsing defect in this report remotely reachable by a network attacker, not just by a hostile server operator.

**Recommendation:** Either delete the dead EncryptData functions so the absence of transport security is honest, or move the connection to TLS. At minimum stop sending the password in the clear.

> ✅ **Resolved by the first option** on branch `harden/high-severity-batch1`: both `EncryptData` bodies, all ten no-op call sites in `fill()`/`flush()`, and the `m_EncryptKey`/`m_HashTable` members are deleted. `setKey(WORD, BYTE*)` is kept with its exact signature as an explicit no-op, because `Player::setKey`/`delKey` call it from a file outside that change's scope; the header comment now states plainly that the stream carries cleartext including the login password.
>
> **Nothing changed on the wire.** There was never transport encryption on this link and there is none now — the deletion only stops the code from implying otherwise. `CLLogin::write` still sends the account password through the plain `SocketOutputStream`. Real encryption is a protocol change requiring the server repo. The separate `Encrypter` path (`SocketEncryptInputStream`/`SocketEncryptOutputStream`, keyed per-field on zone/server) is untouched and still live; it never referenced the removed members.
>
> ~~Worth knowing before anyone re-enables this from git history: the deleted code passed `&m_Buffer[m_Tail]` after receiving into `&m_Buffer[0]`.~~ **That warning was wrong and is retracted.** The branch runs only when `nReceived == nFree` with `nFree = m_BufferLen - m_Tail`, so `m_Tail` is provably 0 there — and the deleted code *asserted* it three lines above (`Assert( m_Tail == 0 );`). `&m_Buffer[m_Tail]` was `&m_Buffer[0]`. The claim came from a reviewer, was repeated here without checking the surrounding lines, and is exactly the kind of plausible-sounding detail this document should not carry.
>
> ✅ **Follow-up closed (2026-09-22):** removed `Player`'s unused 512-byte hash table, `setKey`/`delKey`, the stream no-op setters and the two reconnect cleanup calls. `CGConnectSetKey` packets still carry their existing fields and retain their explicit no-op local dispatch; the separate per-field `Encrypter` remains live. Existing player receive-loop tests and packet golden tests own the surviving behavior. Tests solely about the deleted table were retired with it.

#### 🟠 High -- SendBugReport vsprintf's a caller-supplied runtime string into a fixed 256-byte stack buffer, and two callers pass an exception message as the format string.

**Category:** memory-safety  |  **Location:** `Client/PacketFunction.cpp:4968`

SendBugReport (line 4958) declares `char Buffer[256]` at line 4965 and calls `vsprintf(Buffer, bug, vl)` at line 4968 with no length bound. The truncation at lines 4981-4982 (`if (len >= 100) Buffer[100] = '\0';`) runs after the overflow has already occurred. Client/Packet/ClientCommunicationManager.cpp:129 and :205 call `SendBugReport(t.toString().c_str())` — passing a runtime-constructed string as the printf format. Throwable::toString (Client/Packet/Exception.h:80-87) concatenates the message with getStackTrace(), which in non-NDEBUG builds contains one absolute `__FILE__:__LINE__` entry per __END_CATCH frame; on this checkout those paths are 60+ characters each, so exceeding 256 bytes takes only a few frames.

**Failure scenario:** A datagram triggers an InvalidProtocolException deep in the parse path. The __END_CATCH chain appends several absolute source paths to the stack trace; ClientCommunicationManager passes the >256-byte result as a format string to SendBugReport, and vsprintf overruns Buffer[256] on the stack. Any '%' sequence in the message is additionally interpreted as a conversion specifier.

**Recommendation:** Use vsnprintf with sizeof(Buffer), and change the two ClientCommunicationManager call sites to `SendBugReport("%s", t.toString().c_str())`.

> ✅ **Fixed** in `0e9d247` (branch `harden/text-format`). The formatting is bounded and returns early on an encoding error, so the pre-existing 100-character truncation is protective rather than cosmetic now that it no longer runs after the overflow. **Four** call sites passed the exception text as the format, not two — `GameMain.cpp:286` and `:437` do it as well — and all four now pass it as an argument to a literal `"%s"`. The inverted datagram guard above them (a separate Low, below) is untouched; it matters here only in that it does let exceptions through, so this fix is load-bearing whichever way that guard is eventually corrected.

#### 🟠 High -- ReceiveFileInfo::StartReceive writes to index -1 of a std::string when the peer-supplied filename contains no dot.

**Category:** memory-safety  |  **Location:** `Client/RequestFileManager.cpp:81`

Line 79: `int dot = m_FilenameTemp.rfind(".");`. `std::string::rfind` returns `std::string::npos` ((size_t)-1) when the character is absent; assigning that to `int` yields -1. Line 81 then executes `m_FilenameTemp[dot] = '-';` i.e. `operator[](-1)` — converted back to size_t this is an enormous index, which is undefined behaviour and in practice a write one byte before the string's buffer (or a wild write). The filename is entirely peer-controlled (see RCRequestedFile::read).

**Failure scenario:** A peer sends a filename with no '.' character, e.g. "profile". rfind returns npos, dot becomes -1, and the assignment writes outside the std::string allocation, corrupting the heap allocator's metadata.

**Recommendation:** Use `size_t dot = m_FilenameTemp.rfind('.'); if (dot != std::string::npos) m_FilenameTemp[dot] = '-';` and handle the no-extension case explicitly.

> ✅ **Closed; reconciled 2026-09-17:** `a2b14c33` removed `ReceiveFileInfo` and the rest of the unused outbound peer file-transfer side. Neither the dotless-name write nor the class that contained it remains. The inbound request service and file sender remain separate and unchanged.

#### 🟡 Medium -- Datagram bounds checks compute m_InputOffset + len in unsigned arithmetic that can wrap, bypassing the check.

**Category:** integer-overflow  |  **Location:** `Client/Packet/Datagram.cpp:62`

`Datagram::read(char*, uint)` line 62 tests `if (m_InputOffset + len > m_Length)` and `Datagram::read(std::string&, uint)` line 83 tests the same expression; both operands are uint. When `len` is large enough that `m_InputOffset + len` wraps past UINT_MAX the comparison succeeds and the following `memcpy(buf, &m_Data[m_InputOffset], len)` / `str.assign(&m_Data[m_InputOffset], len)` reads far outside the datagram buffer. `len` reaches these functions from packet read() implementations, several of which pass wire-derived lengths (e.g. RCSay.cpp:33,46 pass BYTEs; other DatagramPacket classes could pass wider types). Related: Datagram::write (lines 180, 205) uses Assert for its bounds check, and Assert compiles to `((void)0)` under NDEBUG (Client/Packet/PacketAssert.h:30-31), so release builds have no write bound at all.

**Failure scenario:** Any current or future datagram packet that reads a 32-bit length prefix and passes it to Datagram::read gets an unchecked out-of-bounds heap read of up to 4 GB, crashing or leaking adjacent heap memory into a string.

**Recommendation:** Rewrite as `if (len > m_Length || m_InputOffset > m_Length - len)`, and replace the Assert-based bounds checks in write() with real runtime checks that survive NDEBUG.

> ✅ **Fixed** on branch `feature/cpp20-framing-header` (2026-09-10), in the `fix:` commit after the slice that gave `Datagram` the span and typed-scalar layer the streams have. The two read checks are `len > m_Length - m_InputOffset`, which cannot wrap because the offset only advances past the check; the write check is the same expression on `m_OutputOffset`, a runtime `Error` in every build where the `Assert` threw only in Debug, and `write(const std::string&)` dropped its own `Assert` and goes through the checked core. Test path: lib + test, `tests/unit/test_datagram_frame.cpp`. **Both tests are reproductions, not guards:** on the unfixed code the read test did not fail, it took `unit_tests` down with a segmentation fault at the first wrapping length, which is the crash the finding predicted, and the write test's two wrapping lengths slip past the old `Assert` the same way (never observed, since the read test crashes first). What no test shows is the Release half: the suite is a Debug build, where the `Assert` threw too. The adversarial review then tightened the fix twice: the first wrap-proof form relied on an offset-never-past-length invariant nothing enforces, and is now the unconditionally safe form this recommendation gives; and `Datagram::write(const DatagramPacket*)` now refuses a body that disagrees with its declared size in either direction, since the bound alone let a body one byte over eat the pad slot behind it.

#### 🟡 Medium -- Packets read raw wire bytes directly into bool members, producing invalid bool representations.

**Category:** undefined-behaviour  |  **Location:** `Client/Packet/Gpackets/GCAddMonsterCorpse.cpp:32`

SocketInputStream.h:65 implements `read(bool& buf)` as `read((char*)&buf, szbool)` — a raw byte copy into the bool object. `szbool` is `sizeof(bool)` (Client/Packet/Types/SystemTypes.h:73), which is implementation-defined. Call sites include GCAddMonsterCorpse.cpp:32 (`iStream.read(m_bhasHead)`) and GCAttackArmsOK1.cpp:60 (`iStream.read(m_bSuccess)`), among others in GCAttackArmsOK4/OK5, GCFriendChatting, GCKnocksTargetBackOK1/4/5 and GCOtherStoreInfo. A server sending 0x02 stores a bool holding neither 0 nor 1, which is undefined behaviour in C++; branching on it is unpredictable and `-fsanitize=bool` traps.

**Failure scenario:** Server sends a monster-corpse packet with the hasHead byte set to 0xFF. The debug-asan build the project README recommends (`make debug-asan`) aborts with 'load of value 255, which is not a valid value for type bool'; optimised builds may take both branches of subsequent tests inconsistently.

**Recommendation:** Read these fields as BYTE and normalise with `!= 0` before assigning to a bool, or make SocketInputStream::read(bool&) do that normalisation centrally.

> ✅ **Fixed** on branch `feature/cpp20-macaddress-span` (2026-09-10), the second way: `SocketInputStream::read(bool&)` takes the byte through `readWire` as a `BYTE` and stores `b != 0`, and `SocketEncryptInputStream::readEncrypt(bool&)` reads through it and then applies the encrypter's flip, so every packet that reads a bool is covered at once and no packet file changed. Test path: lib + test, `tests/unit/test_wire_bool_char.cpp`: written first, and red on the unfixed code (25 of its 32 checks) in a way that shows what an invalid bool does under MSVC - one holding 0x02, 0x7F, 0x80 or 0xFF takes the true branch of `if (a)` and is **unequal to `true`** at the same time. No consumer in the tree compares a wire bool with `== true` (every one is an `if` or a `!`), so in play this was the sanitizer trap the finding names plus an invalid value propagating into UI structs, not an inverted branch. The bytes 0 and 1 are pinned unchanged under ten encrypt codes on both sides of the flip. The encrypting bool read has no production caller (no `readEncrypt`/`writeEncrypt` argument in the tree is a bool), and `Encrypter::convert(bool)` itself still preserves whatever representation it is handed, which is why the normalisation sits at the stream read and not inside it.

#### 🟡 Medium -- The exchange feature's server-to-client packet has no registered factory and its read/write/size are mutually inconsistent, so any server response kills the connection.

**Category:** correctness  |  **Location:** `Client/Packet/Gpackets/GCExchangeList.cpp:25`

> ✅ **Already fixed; reconciled 2026-09-17:** `49263379` reconciled the exchange wire layouts with the server and registered both reply factories. `GCExchangeList` now reads and writes the same page fields, count and bounded listing records, and reports their actual wire size. Factory/inventory tests and the exchange round-trip, maximum-string and shared-golden tests remain passing.

GCExchangeListFactory is declared (GCExchangeList.h:54) but never passed to addFactory — PacketFactoryManager.cpp:1198-1199 registers only CGExchangeListFactory and CGExchangeBuyFactory. ClientPlayer::processCommand calls getPacketMaxSize(packetID) at line 256, which throws InvalidProtocolException("packet factory [...] not exist") for any unregistered ID, tearing down the session. Independently, the packet is internally inconsistent: read() (lines 28-30) consumes three `int` members (12 bytes, per GCExchangeList.h:44-46), write() (lines 42-48) emits those 12 bytes plus a 2-byte count, and getPacketSize() (line 56) reports `szBYTE+szBYTE+szBYTE+szWORD` == 5. `m_pListings` is set to NULL in the constructor and never used or freed.

**Failure scenario:** The server replies to CGExchangeList with PACKET_GC_EXCHANGE_LIST. The client cannot find a factory for it, throws InvalidProtocolException out of processCommand, and disconnects — so the exchange feature can never work end to end.

**Recommendation:** Register GCExchangeListFactory (and the other declared GC_EXCHANGE_* IDs) or remove the enum entries; then make read(), write() and getPacketSize() agree on the wire layout.

#### 🟡 Medium -- sscanf writes 4-byte ints into 2-byte SYSTEMTIME WORD fields, corrupting the surrounding struct.

**Category:** memory-safety  |  **Location:** `Client/Packet/Lpackets/LCReconnectHandler.cpp:117`

> ✅ **Closed (2026-09-17):** removed the obsolete `OUTPUT_DEBUG` reconnect timing-file report, including its mismatched `sscanf`, persisted records and aggregate output. No build defines this diagnostic macro; active reconnect/send operations and their existing log messages remain. This is dead-code removal verified by builds, not a runtime reproduction.

Lines 117-121 call `sscanf(readTemp, "%4d/%2d/%2d %2d:%2d:%2d\t%8d\t%8d\n", &ts.st.wYear, &ts.st.wMonth, &ts.st.wDay, &ts.st.wHour, &ts.st.wMinute, &ts.st.wSecond, &ts.reconnectTickCount, &ts.sendCGConnectTickCount)`. The `%d` conversion requires an `int*`, but wYear/wMonth/wDay/wHour/wMinute/wSecond are `WORD` (16-bit) members of SYSTEMTIME. Each conversion writes 4 bytes into a 2-byte slot, overwriting the adjacent field. The field-width prefixes limit the parsed digits, not the size of the store. `reconnectTickCount`/`sendCGConnectTickCount` are DWORD and read with `%d` (signed) — a further mismatch.

**Failure scenario:** Log\ConnectTime.txt exists with a well-formed timestamp line. Each of the six %d conversions clobbers two bytes past its target, so the parsed SYSTEMTIME is garbage and the write past wSecond spills into reconnectTickCount. On a struct laid out differently this smashes neighbouring stack data.

**Recommendation:** Parse into local `int` variables and assign to the WORD fields, or use %hd. Also note the hardcoded backslash path 'Log\\ConnectTime.txt' breaks the non-Windows targets.

#### 🟡 Medium -- Three of the four packet-dispatch loops ignore peek()'s boolean result and parse an uninitialized header buffer.

**Category:** correctness  |  **Location:** `Client/Packet/Player.cpp:159`

> ✅ **Already fixed; reconciled 2026-09-17:** all three remaining dispatch loops (`Player`, `ClientPlayer`, `RequestServerPlayer`) branch on a failed `peek()` before reading the header. The outbound `RequestClientPlayer` was removed in `a2b14c33`. `PlayerReceiveLoop.FragmentationAndMalformedBodiesNeverDispatchOrLeak` verifies that a partial wrapped header remains untouched and causes neither allocation nor dispatch; the two specialized loops were also checked at their header reads.

SocketInputStream::peek returns false (rather than throwing) when the buffer holds fewer than the requested bytes — the InsufficientDataException throw is commented out at SocketInputStream.cpp:262. Player.cpp:159, Client/Packet/RequestClientPlayer.cpp:144 and Client/Packet/RequestServerPlayer.cpp:123 all call `m_pInputStream->peek(header, szPacketHeader);` and discard the result, then memcpy packetID and packetSize out of `char header[szPacketHeader]` which peek left untouched. Both packetID and packetSize are declared without initialisers (Player.cpp:146-147, RequestClientPlayer.cpp:124-125). Only ClientPlayer.cpp:140 checks `== false` and breaks.

**Failure scenario:** On any frame where fewer than 7 bytes have arrived — routine with partial TCP reads — the loop reads uninitialized stack memory as a packet ID and size. On the first iteration this is genuinely indeterminate; on later iterations it re-parses the previous packet's stale header. MSVC /RTCu and MemorySanitizer both flag this, and the resulting control flow depends on stack garbage.

**Recommendation:** Mirror ClientPlayer.cpp:140: `if (m_pInputStream->peek(header, szPacketHeader) == false) break;` in all three loops, and zero-initialise packetID/packetSize.

#### 🟡 Medium -- fill() calls recv() with a zero-length buffer when the input ring buffer is full, and recv_ex turns the resulting 0 return into a spurious disconnect.

**Category:** correctness  |  **Location:** `Client/Packet/SocketInputStream.cpp:421`

> ✅ **Fixed (2026-09-17):** `fill()` checks a full ring's backlog before growing and never receives into an empty slice. It preserves bytes received before a later would-block, including across the wrap. `recv_ex(..., 0, ...)` is a no-op. Scripted transport tests reproduce the original failures and cover all 832 combinations of small-ring head, queued length and incoming length, plus partial reads and would-block after progress. Both Windows Debug and ASan suites pass.

When m_Head == 0 and m_Tail == m_BufferLen-1 the buffer is exactly full and line 421 computes `nFree = m_BufferLen - m_Tail - 1` == 0, which is then passed to receiveWithDebug/recv. `SocketAPI::recv_ex` (SocketAPI.cpp:844-845) treats any return of 0 as `throw ConnectException("connect closed.")` — but recv() with len==0 legitimately returns 0 on an open socket. The same zero-length case arises at line 493 (`nFree = m_Head - 1` with m_Head == 1) and line 536 (`nFree = m_Head - m_Tail - 1` when the buffer is full). The resize-on-full path only triggers when `m_pSocket->available() > 0` at that instant, so a momentarily full buffer is reachable.

**Failure scenario:** During a burst of server traffic the 32 KB input buffer fills exactly while the socket's receive queue happens to be empty. The next fill() calls recv with length 0, recv_ex throws ConnectException, and the client reports a lost connection to a perfectly healthy server.

**Recommendation:** Return early from fill() when nFree == 0 (after attempting a resize), and make recv_ex not treat a 0 return as a close when the requested length was 0.

#### 🟡 Medium -- The input buffer grows without bound from server-controlled data and never shrinks; the shrink guard is a dead unsigned comparison.

**Category:** resource-management  |  **Location:** `Client/Packet/SocketInputStream.cpp:656`

> ✅ **Fixed (2026-09-17):** input capacity is capped at 16 MiB; a backlog that would exceed the cap raises `InvalidProtocolException` before allocation or another receive. The wire inventory asserts that at least four copies of every declared maximum frame fit. An emptied grown ring returns to its construction capacity on the next fill. Both stream resize functions calculate the new capacity in signed 64-bit arithmetic and preserve the ring's empty sentinel slot; invalid sizes leave bytes and capacity intact. The unused, separately broken `fill_RAW` path is removed. Tests cover exact-limit growth, excessive/overflowing backlogs, reclamation, wrapped data and rejected shrink/underflow sizes; six initial tests failed before the fix. Full Debug/ASan builds and CTest verify it without a live server.

fill() calls `resize(available + 1)` at lines 445, 508 and 556 whenever the ring buffer fills and the socket still has data, where `available` comes from ioctl(FIONREAD) on the socket — i.e. from how much the peer sent. resize (line 649) computes `uint newBufferLen = m_BufferLen + size;` and allocates that; nothing ever reduces the buffer afterwards, and no ceiling is enforced. The guard at line 663, `if (newBufferLen < 0 || newBufferLen < len)`, tests an unsigned value for `< 0`, which is always false — a compiler-warning-level dead check that leaves only half the intended protection. The same dead comparison exists at Client/Packet/SocketOutputStream.cpp:267.

**Failure scenario:** A hostile server floods the connection faster than the client's frame loop drains it (the loop processes only MAX_PROCESS_PACKET packets per frame, ClientPlayer.cpp:131). Each fill() grows the 32 KB buffer by the socket backlog; the buffer ratchets up to hundreds of megabytes and the client is OOM-killed or thrashes.

**Recommendation:** Cap the input buffer at a sane multiple of the largest legal packet and drop the connection when it would be exceeded; delete or fix the `newBufferLen < 0` checks.

#### 🟡 Medium -- flush() discards unsent buffered bytes when the socket would block, truncating packets mid-body.

**Category:** correctness  |  **Location:** `Client/Packet/SocketOutputStream.cpp:242`

The send loops at lines 189-194, 209-214 and 225-230 call `m_Socket->send(...)`, which throws NonBlockingIOException from SocketAPI::send_ex when the kernel buffer is full. That exception is caught and swallowed at line 239, and line 242 then executes `m_Head = m_Tail = 0;` unconditionally — resetting the ring buffer and dropping everything not yet written to the socket. The bytes already handed to send() have gone out, so the server receives a partial packet.

**Failure scenario:** Under load or on a congested link the socket send buffer fills mid-packet. The client sends the first N bytes of a packet, drops the remainder, and continues writing the next packet on top. The server's parser reads the truncated packet's header, takes the following packet's bytes as its body, and the connection desynchronises or is dropped as a protocol violation.

**Recommendation:** On NonBlockingIOException, leave m_Head/m_Tail pointing at the unsent remainder so the next flush() resumes; only reset the buffer when nLeft reaches 0.

> ✅ **Fixed on 2026-09-06** (`fix/flush-partial-send`), which is the recommendation above, taken as written. The one thing worth stating plainly is that no byte count had to be recovered from the exception: `send()` returns the number it accepted — a short count is an ordinary return, not a failure — and `SocketAPI::send_ex` (SocketAPI.cpp:672, 720-721) raises `NonBlockingIOException` only when the underlying `send()` returned `SOCKET_ERROR`, so the call that throws transferred nothing and every byte that did go out is already in `m_Head`, which all three send loops advance by each returned count (true of the shipped build: `SocketImpl.cpp`'s `__USE_ENCRY__` arm returns `len` whatever the kernel accepted, and nothing defines `__USE_ENCRY__`). The growth this leaves under sustained backpressure is the worst shape: `resize()` leaves exactly one spare byte, so with the peer blocked every `write()` reallocates and copies the whole live run; geometric growth and a cap are the follow-up. `flush()` therefore only had to stop throwing the rest away: the catch is unchanged, and `m_Head = m_Tail = 0` is now guarded by `m_Head == m_Tail`, which keeps the normalisation of a drained ring and drops nothing from a partial one. `tests/unit/test_output_stream_flush.cpp` pins the remainder, the head and tail, the peer's byte stream across two flushes, both segments of a wrapped ring, a would-block that sent nothing, writes queued behind a remainder, and a real framed packet cut mid-body — driven through a `SocketImpl` whose `send()` is scripted with one cap per call, which is why that method is now virtual (the seam is documented at its declaration in `SocketImpl.h`). The finding's sibling at SocketInputStream.cpp:421 (the zero-length `recv`) and the unbounded input-buffer growth above it are untouched. One consequence is worth naming rather than leaving to be discovered: keeping the remainder means a peer that stays blocked while the client keeps writing now grows the output ring — `write()` resizes when it fills, and nothing caps it — where before the ring was emptied on every flush by throwing the data away. That is the trade the recommendation asks for and the one every correct implementation makes, but it gives the output stream the same unbounded-growth shape the input stream's Medium finding above describes, and neither has a cap yet.

> ✅ **Output-growth residual fixed (2026-09-23):** `SocketOutputStream` enforces
> a 16 MiB allocation ceiling, including its empty sentinel, at construction,
> explicit resize and every write. Automatic growth reserves space geometrically
> instead of reallocating for each small write. Over-budget writes raise the
> existing protocol exception without changing queued bytes; the packet rollback
> path also restores sequence state when a header or body hits the limit.
> Four test-first cases reproduced 22 failed checks and cover exact capacity,
> wrapped rings, blocked/partial sends, resumed delivery and rejected frames.
> `test_wire_layout.cpp` verifies that four copies of every declared maximum
> frame fit. Storage remains bounded and reusable after draining; it is not
> returned to the original capacity. The input-buffer residual above was closed
> on 2026-09-17. The older paragraph records the state after the original fix.

#### 🟡 Medium -- Dynamic exception specifications are violated throughout, which calls std::terminate on GCC/Clang while MSVC silently ignores it.

**Category:** portability  |  **Location:** `Client/RequestFileManager.cpp:419`

> ✅ **Already fixed; reconciled 2026-09-17:** the C++20 migration removed dynamic exception specifications across the project. Ratchets R9/R10 are zero, strict C++20 builds pass on Windows, Linux and macOS, and the framed-read tests exercise exceptions through the packet layer. Destructors that can throw are explicitly `noexcept(false)`.

`RequestFileManager::ReceiveMyRequest(...) throw (ConnectException)` (lines 418-419) calls `pRequestClientPlayer->readInputStream(...)`, which reaches SocketInputStream::read and can throw InvalidProtocolException (SocketInputStream.cpp:90, when len==0) or InsufficientDataException (line 98). Neither derives from ConnectException — the hierarchy in Client/Packet/Exception.h puts InvalidProtocolException/InsufficientDataException under ProtocolException:IOException while ConnectException sits under SocketException:IOException (lines 324, 367, 381). Under C++11 (the standard the project sets, CMakeLists.txt:13) violating a dynamic exception specification calls std::unexpected() and then std::terminate(). MSVC ignores throw-specs entirely, so this only manifests on the Linux/macOS targets the SDL migration is aiming at. Dynamic exception specifications are also removed outright in C++17, so any standard bump breaks the entire Packet layer.

**Failure scenario:** During a peer file transfer the input stream momentarily has 0 bytes for the current chunk; readInputStream throws InvalidProtocolException("len==0"); on a Clang/GCC build the runtime calls std::terminate and the client aborts instead of recovering. On MSVC the same code recovers normally, so the bug is invisible on the primary dev platform.

**Recommendation:** Strip the dynamic exception specifications across Client/Packet (they are already deprecated), replacing genuine no-throw guarantees with `noexcept`. Do this before any move to C++17.

#### ⚪ Low -- InternetReadFile's return value is ignored and its byte count is read from an uninitialized DWORD.

**Category:** correctness  |  **Location:** `Client/MInternetConnection.cpp:246`

> ✅ **Closed (2026-09-17):** removed the unused `MInternetConnection`/`MInternetFile` classes, their type-size diagnostics, non-Windows exclusion and sole WinINet link dependency. A source-reference audit found no object construction or call site. Correction to the original finding: the file was excluded only off Windows; removing it reduces Windows executable membership by one. No live downloader behavior changes.

Line 246 declares `DWORD nRead;` with no initialiser; line 248 calls `InternetReadFile(m_hFile, pBuffer, BUFFER_SIZE, &nRead)` and discards the BOOL result; lines 249 and 258 then use nRead for `m_nReceived += nRead` and `m_LocalFile.write(&pBuffer, nRead)`. If the call fails without writing the out-parameter, nRead holds stack garbage and the write reads far past the 4096-byte pBuffer. Separately, line 62 passes the literal `TEXT("pAppName")` to InternetOpen instead of the pAppName parameter, so the user-agent is always the string "pAppName".

**Failure scenario:** The HTTP connection drops mid-download; InternetReadFile returns FALSE and (on some providers) leaves nRead untouched. The client writes an arbitrary garbage-length slice of stack memory into the local patch file.

**Recommendation:** Initialise nRead to 0 and return FALSE when InternetReadFile fails; pass the real pAppName. This file is currently excluded from the CMake build (CMakeLists.txt:694), which is worth confirming is intentional given the SDL cross-platform goal.

#### ⚪ Low -- An operator-precedence mistake makes the bug-report condition read as (!ptr) == NULL, and it appears twice.

**Category:** correctness  |  **Location:** `Client/Packet/ClientCommunicationManager.cpp:128`

> ✅ **Fixed (2026-09-17):** both UDP-path conditions explicitly compare the substring result with `NULL` using `!=`, preserving their existing behavior without the misleading negation. The stream-path counterpart is addressed separately in the `GameMain.cpp:281` finding.

Line 128 reads `if( !strstr( t.toString().c_str(), "(datagram)" ) == NULL )` and line 204 repeats it verbatim. `!strstr(...)` evaluates first to a bool, which is then compared against NULL — so the condition is true precisely when the substring IS present, the opposite of what the negation reads as. Comparing a bool to NULL is itself a diagnosable construct on modern compilers.

**Failure scenario:** A maintainer reading the code concludes the bug report is suppressed for datagram-related exceptions when in fact it is only sent for those, and 'fixes' it in the wrong direction.

**Recommendation:** Write the intent explicitly: `if (strstr(...) != NULL)` (or `== NULL`), at both sites.

#### ⚪ Low -- addFactory indexes the factory table with an unchecked getPacketID(), unlike every other accessor in the class.

**Category:** correctness  |  **Location:** `Client/Packet/PacketFactoryManager.cpp:1218`

> ✅ **Fixed (2026-09-17):** `addFactory` rejects null and out-of-range factories before indexing, reads the ID once and preserves existing entries on rejection. Ownership transfers only on success. New tests reproduced the invalid-ID rejection failures and null dereference; duplicate registration is also covered. Removed the hand-numbered enum comments without changing any of the 504 packet enum declarations. Full Debug/ASan builds and suites pass.

createPacket (line 1251), getPacketMaxSize (line 1275) and getPacketName (line 1300) all guard with `packetID >= m_Size`. addFactory does not: line 1218 reads `m_Factories[pFactory->getPacketID()]` and line 1233 writes to the same slot with no bound. m_Factories is allocated with m_Size == Packet::PACKET_MAX entries (line 587). Note also that the ID enum in Packet.h is the wire protocol, and its trailing comments are already stale — PACKET_CG_ENCODE_KEY is commented '483' but the two 'add by viva' entries after it shift the Exchange block, whose comments restart at '484'.

**Failure scenario:** Someone adds a packet class whose getPacketID() returns a value not present in the enum (a typo, or an ID copied from the server repo after the two enums drift apart). addFactory writes a pointer out of bounds at startup with no diagnostic, corrupting whatever follows the array.

**Recommendation:** Add the same `>= m_Size` guard to addFactory, and fix or delete the numeric comments in the Packet::PACKET_* enum so they cannot mislead anyone matching client and server IDs.

#### ⚪ Low -- An unconditional printf fires on every outgoing packet in the send hot path.

**Category:** maintainability  |  **Location:** `Client/Packet/SocketOutputStream.cpp:150`

> ✅ **Fixed (2026-09-17):** both datagram header prints now use the existing `__DEBUG_OUTPUT__` guard. The TCP send print was already guarded; the unused `Packet::writeHeaderNBody` helper and its misleading queued-length diagnostic are removed. All production TCP framing goes through `SocketOutputStream::write(const Packet*)`, covered by framing/rollback and golden tests.

SocketOutputStream::write(const Packet*) contains a bare `printf("%s:%d SocketOutputStream::write packetID: %d, packetSZ: %d sequence %d\n", __FILE__, __LINE__, packetID, packetSize, m_Sequence-1);` at lines 150-152, outside any #ifdef. Every packet the client sends — movement, attacks, chat — writes a line to stdout. Client/Packet/Packet.h:606 has a matching unguarded `std::cout` in writeHeaderNBody, and Client/Packet/Datagram.cpp:125 and :138 print each datagram's ID and size.

**Failure scenario:** In normal play the client emits thousands of stdout lines per minute; on a console-attached build this measurably stalls the frame loop and buries any genuine diagnostic output.

**Recommendation:** Wrap all four in __DEBUG_OUTPUT__ or route them through DEBUG_ADD_FORMAT like the rest of the layer.

---

## Core Game Loop & State

**Grade:** D  |  **Findings:** 24

**Scope:** Core game loop and state (Client/GameMain.cpp, CGameUpdate/COpeningUpdate, ActionFunctions, and the M* gameplay classes: MZone, MCreature, MPlayer, MItem, MEffect*/effect generators)

**Health assessment:** This area is the client's trust boundary with the server, and it is largely unguarded. Network-supplied bytes reach a function-pointer table index (MItem::NewItem), fixed 21-byte heap chat buffers via strcpy/strcat, and raw 2D sector-array subscripts in at least six places in MZone — one of which had its bounds check explicitly commented out. Several hot paths read uninitialized locals (MZone::AddCreature's bAdd, MZone::LoadFromFile's pImageObject and size, MStopZoneCrossEffectGenerator's bOK), and MZone::UpdateAllCreature deletes a creature while acknowledging in a log line that it is still referenced by a sector. The one place a real memory-safety fix was attempted, MZone::AddEffect, was instead papered over with ~120 lines of pointer-address heuristics, a hardcoded ASAN heap address range, and a try/catch that cannot catch a segfault — while introducing new leaks on its early-return paths. Compounding this, CTypeTable's range checks are gated on _DEBUG, which CMakeLists.txt:16 deliberately never defines, so every table lookup in the documented `make debug-asan` build is unchecked. Structurally, ~400KB of stub/duplicate source (GameFunctions.cpp, GameHelpers.cpp, ActionFunctions.cpp, MissingGlobals.cpp, MitemTableinit2.cpp) sits in the tree excluded from the build and shadowing the live implementations. The code does run and some newer call sites do validate correctly (MZone::RemoveCreature, MCreature::AddEffectStatus), which is why this is not a failing grade — but the density of remotely reachable memory-safety defects is high enough that any hostile or merely buggy server can corrupt the client's heap.

#### 🔴 Critical -- The chat line-wrapping loop in SetChatString/SetPersnalString copies an entire newline-delimited segment into a 21-byte row without bounding it.

**Category:** memory-safety  |  **Location:** `Client/MCreature.cpp:5099`

The loop normally cuts at `endIndex = startIndex + MAX_CHATSTRING_LENGTH` (20). But when a '\n' is present the code overrides that: `*find = '\0'; endIndex = strlen(str+startIndex)+startIndex;` (lines 5074-5075, and identically at 4890 in SetPersnalString). endIndex is then the offset of the newline, which is unbounded. Since that is still < len, control reaches the else branch and the manual byte loop at 5099-5102 copies `endIndex - startIndex` bytes into `m_ChatString[m_ChatStringCurrent]`, a 21-byte allocation. SetPersnalString's copy of this logic at 4886-4892 is not even gated on creature type — it runs for every personal-shop sign, fed from the network at Client/Packet/Gpackets/GCAddSlayerHandler.cpp:113 and GCAddOustersHandler.cpp:123/203 as `SetPersnalString((char*)pPacket->getStoreOutlook().getSign().c_str(), ...)`.

**Failure scenario:** Another player sets a personal-shop sign of "AAAA...(200 chars)...\nx". On receiving GCAddSlayer the client copies 200 bytes into a 21-byte heap row. Note the `(char*)...c_str()` cast also means `*find = '\0'` mutates the packet's std::string buffer in place.

**Recommendation:** Clamp endIndex to `startIndex + MAX_CHATSTRING_LENGTH` after the newline adjustment, and stop const_casting away c_str() — copy the packet string into a local bounded buffer first.

> ✅ **Fixed** in `c3e9937`, restructured in `3bc340e` (branch `harden/network-input`): the newline cut is only honoured when the line fits in a row, measured before the in-place `'\0'` write so an over-long line no longer mutates the buffer at all. The `(char*)c_str()` const_cast at the call sites remains.

#### 🔴 Critical -- MCreature::SetChatString does an unbounded strcpy+strcat of a server-supplied chat string into a 21-byte heap buffer.

**Category:** memory-safety  |  **Location:** `Client/MCreature.cpp:5016`

`strcpy(m_ChatString[m_ChatStringCurrent], "Dear. "); strcat(m_ChatString[m_ChatStringCurrent], str);` at lines 5016-5017. Each row of m_ChatString is allocated as `new char[g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1]` (Client/MCreature.cpp:678), and MAX_CHATSTRINGLENGTH_PLUS1 defaults to 21 (Client/ClientConfig.cpp:117). "Dear. " already consumes 6 bytes, so any `str` longer than 14 bytes overflows the heap block. `str` is the chat message: Client/Packet/Gpackets/GCSayHandler.cpp:206 passes a buffer filled by `strcpy(str, pPacket->getMessage().c_str())` at GCSayHandler.cpp:112. The path is gated on GetCreatureType()==482 (the Christmas-tree NPC), which is itself a server-controlled field.

**Failure scenario:** A creature whose type the server reports as 482 says a 60-character line. strcat writes 66 bytes into a 21-byte heap allocation, corrupting adjacent heap metadata and the neighbouring m_ChatString rows.

**Recommendation:** Replace with a length-checked copy bounded by MAX_CHATSTRINGLENGTH_PLUS1-1 (snprintf into the row), and apply the same treatment to the sibling sites at lines 5086, 5130-5133 and 4900.

> ✅ **Fixed** in `c3e9937` and `3bc340e` (branch `harden/network-input`). The "Dear." copy and the tree "From." block -- which the first pass missed -- are both snprintf-bounded to the row size.

#### 🔴 Critical -- MItem::NewItem indexes a static function-pointer table with an unvalidated item class taken directly from network packets, then calls through the result.

**Category:** memory-safety  |  **Location:** `Client/MItem.cpp:325`

`return (MItem*)(*s_NewItemClassTable[itemClass])();` performs no range check. The table is declared `static FUNCTION_NEWITEMCLASS s_NewItemClassTable[MAX_ITEM_CLASS]` (Client/MItem.h:497) with MAX_ITEM_CLASS == 90 (Client/ItemClassDef.h:137). Callers pass the packet field straight through: Client/Packet/Gpackets/GCCreateItemHandler.cpp:69 `MItem::NewItem((enum ITEM_CLASS)pPacket->getItemClass())`, GCChangeShapeHandler.cpp:33, GCAddStoreItemHandler.cpp:59/141/179, GCMakeItemOKHandler.cpp:73, GCMyStoreInfoHandler.cpp:108. `getItemClass()` returns a raw BYTE (Client/Packet/Gpackets/GCCreateItem.h:52), so the attacker-controlled range is 0-255 against a 90-entry array. This is an out-of-bounds read of a code pointer followed by an indirect call through it.

**Failure scenario:** Server (or an on-path attacker) sends GCCreateItem with m_ItemClass = 200. NewItem reads s_NewItemClassTable[200], 880 bytes past the end of the array into whatever static data follows, and calls that address as a function. Depending on what lies there this is a crash or arbitrary code execution in the client process.

**Recommendation:** Add `if (itemClass < 0 || itemClass >= MAX_ITEM_CLASS || s_NewItemClassTable[itemClass] == NULL) return NULL;` at the top of NewItem, and audit every caller to handle a NULL return (several currently dereference it immediately).

> ✅ **Fixed** in `7b81ba4` (branch `harden/network-input`), with the VS_UI call sites the first audit missed guarded in `3bc340e`. NewItem validates the class and the table entry and returns NULL; every variable-class caller under Client/ and VS_UI/ handles the NULL return. Regression guard, executable-only code.

#### 🟠 High -- MItem::GetName computes a write offset as strlen()-4 and copies an arbitrary-length game-string over it, into a buffer sized for only 9 extra bytes.

**Category:** memory-safety  |  **Location:** `Client/MItem.cpp:435`

Lines 432-439: `m_pName = new char[strlen(HName)+1+5];` then `strcpy(m_pName, HName);` then `char *psz_temp = &m_pName[strlen(m_pName)-4];` then `strcpy(psz_temp, (*g_pGameStringTable)[STRING_MESSAGE_SOUL_STONE].GetString());`. The allocation has len+6 bytes; writing S+1 bytes at offset len-4 needs len-4+S+1 <= len+6, i.e. S <= 9. The English replacement "Soul Stone" is already 10 characters, and the Korean/Chinese equivalents are longer still in UTF-8. Separately, if strlen(HName) < 4 the offset `&m_pName[strlen-4]` is a pointer before the allocation. The guard `if(psz_temp != NULL)` at 436 is vacuous — the result of pointer arithmetic on a valid pointer is never NULL. MItem::GetEName has the mirror problem at 468-478, copying an item-table name into a `static char sz_temp[256]` with strcpy and then overwriting from a strstr offset.

**Failure scenario:** Teen mode (GoreLevel == false) with any ITEM_CLASS_SKULL item and a SOUL_STONE string longer than 9 bytes: a guaranteed heap overflow on every call, sized by the localized string. With a skull item whose HName is 3 characters, the write also starts before the buffer.

**Recommendation:** Size the allocation from strlen(HName)+strlen(replacement)+1, guard `strlen(m_pName) >= 4` before computing the offset, and drop the meaningless NULL test. Return a std::string or a caller-supplied buffer instead of the static in GetEName.

> ✅ **Fixed** on branch `harden/high-severity-batch1`. `GetName` sizes the allocation from both strings, requires `strlen(HName) >= 4`, builds the result with `memcpy` + `strcpy` at the computed offset, and the vacuous NULL test is gone. Both source strings are now NULL-checked, which matters because they come from data files and an `MString` for an entry the file never supplied holds NULL.
>
> `GetEName` keeps its `static char sz_temp[256]` — returning a `std::string` would change the signature and every caller — but both writes are clamped, the second by the room actually left after the `strstr` match. A localised name over 255 bytes is now truncated instead of overflowing.
>
> Behaviour change: on the reject path `GetName` returns the item table's `HName` and leaves `m_pName` NULL, so it recomputes each call. That is the same shape as the existing non-skull path, with no leak and no ownership change. Regression guard, executable-only code.

#### 🟠 High -- MStopZoneCrossEffectGenerator::Generate reads and returns the uninitialized local bOK whenever the first AddEffect fails.

**Category:** undefined-behavior  |  **Location:** `Client/MStopZoneCrossEffectGenerator.cpp:26`

`bool bOK, bAdd;` at line 26. bOK is only assigned inside `if (bAdd)` after the first `g_pZone->AddEffect(pEffect)` at line 66. If that AddEffect returns false — which is the common case for duplicate frame IDs, out-of-zone positions, or any of the ~10 early-return paths in MZone::AddEffect — bOK is never written. It is then read at line 144 (`if (!bOK)`), written conditionally at 148, and returned at the end of the function. Callers of Generate branch on that return value.

**Failure scenario:** A cross-shaped stop-zone effect is cast on a tile that already holds an identical effect. AddEffect returns false, bOK is indeterminate; the loop takes an arbitrary branch for effect-target linking and the function returns garbage to the effect-generator dispatcher.

**Recommendation:** Initialize `bool bOK = false; bool bAdd = false;`. This is the only generator in the M*EffectGenerator family with this declaration form, so it is a one-line fix.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, and **the supporting claim is wrong**. Four siblings declare `bool bOK;` uninitialized too — `MStopZoneEmptyCrossEffectGenerator`, `MStopZoneRandomEffectGenerator`, `MStopZoneXEffectGenerator` and `MStopZoneRhombusEffectGenerator`. None of them has the bug: each assigns `bOK = g_pZone->AddEffect(...)` unconditionally before any read, whereas this one assigns only inside `if (bAdd)`. The fix is still one line, but "the only generator with this declaration form" is not the reason.
>
> Visible behaviour change, and the reason this one deserves a runtime look: `MEffectGeneratorTable.cpp` documents the return as whether `pEffectTarget` was linked into an effect. A garbage-true made the dispatcher skip `SetResultTime()` and copy the target instead of linking the original. A deterministic false now fires the result immediately and links the original on the first successful tile — the designed behaviour, but stop-zone-cross and SAND_CROSS result *timing* changes with it.

#### 🟠 High -- MZone::LoadFromFile calls pImageObject->LoadFromFile through a pointer left uninitialized when a map file contains an unknown object-type byte.

**Category:** memory-safety  |  **Location:** `Client/MZone.cpp:1103`

`MImageObject *pImageObject;` is declared uninitialized outside the loop at 1048. The switch at 1060 dispatches on a byte read straight from the map file (`file.read((char*)&ObjectType, 1)` at 1058) and has cases only for TYPE_IMAGEOBJECT, TYPE_SHADOWOBJECT, TYPE_ANIMATIONOBJECT, TYPE_SHADOWANIMATIONOBJECT and TYPE_INTERACTIONOBJECT — no default. Line 1103 then unconditionally calls `pImageObject->LoadFromFile(file);` and 1110 adds it to the map. On the first iteration the pointer is indeterminate; on later iterations it is the previous object, which then gets loaded over and inserted twice.

**Failure scenario:** A corrupted or hand-edited .map file has an object-type byte of 7. On the first object this is a call through an indeterminate pointer; on a later object the previous MImageObject is re-parsed and AddImageObject'd a second time under a new ID, so MZone::Release deletes it once and leaves the other map entry dangling.

**Recommendation:** Initialize pImageObject to NULL inside the loop, add a default: case that logs and aborts the load (return false), and guard the LoadFromFile/AddImageObject calls on non-NULL.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, exactly as recommended. Aborting is the only correct option rather than skipping the record: only the object itself knows its length, so the loader cannot resynchronise past an unknown type byte. Both callers already treat a `false` return as fatal (`SetMode(MODE_QUIT)`), so this is an existing, handled path rather than a new one.

#### 🟠 High -- MZone::UpdateAllCreature deletes a creature even when it failed to remove it from its sector, leaving a dangling MCreature* in the sector.

**Category:** memory-safety  |  **Location:** `Client/MZone.cpp:2198`

Lines 2180-2204 compute `bool removed = true;`, set it to false when both `m_ppSector[y][x].RemoveCreature(id)` and `m_ppSector[serverY][serverX].RemoveCreature(id)` fail (logging "Can't RemoveCreature!" at 2189), and then `delete pCreature;` unconditionally at 2198. `removed` is never read again — it is a dead variable. The sibling function MZone::RemoveCreature (lines 3142-3199) computes the same flag and correctly guards `delete pCreature;` with `if (removed)`, which shows the omission here is unintentional. MZone::KeepObjectInSight has the same defect: the removal result is discarded at 2701-2707 and `delete pCreature;` runs at 2759. Both sites also subscript m_ppSector without bounds checks.

**Failure scenario:** A creature's client and server coordinates have drifted apart (the normal case this code exists to handle) so neither sector holds it. The object is freed, but MSector still holds the pointer; the next MZone::GetCreatureBySector or draw pass dereferences freed memory.

**Recommendation:** Guard the delete with `if (removed)` exactly as RemoveCreature does, or do a full scan of the zone for the stale pointer before freeing. Remove the dead `removed` variable if the guard is intentionally omitted, so the intent is not ambiguous.

> ✅ **Fixed** on branch `harden/high-severity-batch1`. Both the `delete` and the `m_mapCreature.erase` are now under `if (removed)`, matching `MZone::RemoveCreature`. `KeepObjectInSight` turned out to have no `removed` variable at all — the review described it as "discarded", but the live code simply ignored the result — so one was added.
>
> The cost is worth stating: when no sector gives the pointer up, the creature now stays owned by `m_mapCreature` (and is collected by `Release()`) instead of being freed while a sector still points at it. An invisible creature can therefore linger and re-log "Can't RemoveCreature!" each frame. That is exactly `RemoveCreature`'s existing semantics, and a leak is preferable to a dangling pointer the draw pass dereferences. Iterator flow was checked in both functions: neither can loop forever.

#### 🟠 High -- MZone::AddCreature indexes the 2D sector array with unvalidated creature coordinates taken straight from packets.

**Category:** memory-safety  |  **Location:** `Client/MZone.cpp:2990`

Lines 2984-3018 do `int x = pCreature->GetX(); int y = pCreature->GetY();` then `m_ppSector[y][x].AddUndergroundCreature(...)` / `AddGroundCreature(...)` / `AddFlyingCreature(...)` with no comparison against m_Width/m_Height. The only guard, at 2904, rejects SECTORPOSITION_NULL. Positions arrive verbatim from packets — e.g. Client/Packet/Gpackets/GCAddMonsterHandler.cpp:124 `pCreature->SetPosition(pPacket->getX(), pPacket->getY())` followed by `g_pZone->AddCreature(pCreature)` at line 148; the same shape appears in GCAddSlayerHandler, GCAddVampireHandler, GCAddOustersHandler, GCAddNPCHandler and others. That the guard is understood to be needed is visible at Client/MZone.cpp:420, where the equivalent check `if (x>=0 && y>=0 && x<m_Width && y<m_Height)` was commented out but the braces left behind.

**Failure scenario:** Server sends GCAddMonster with x=60000, y=60000 for a 256x256 zone. m_ppSector[60000] reads a pointer ~480KB past the row-pointer array and then writes an MCreature* through it.

**Recommendation:** Reject out-of-range coordinates in AddCreature (return false) and restore the commented-out check at MZone.cpp:420; better, add a private `MSector* SectorAt(x,y)` accessor returning NULL out of range and route every m_ppSector subscript through it.

> ✅ **Fixed** on branch `harden/high-severity-batch1` via the recommended accessor: `MZone::SectorAt(x, y)` plus a const overload return NULL when `m_ppSector` is NULL or the coordinate is off-map. The commented-out check at `MZone.cpp:420` was real, and that site also indexed `[serverY][serverX]`, which the old commented-out check never covered; both subscripts there now go through the accessor.
>
> **It was deliberately not routed through every `m_ppSector` subscript.** 145 raw subscripts remain against 12 `SectorAt` uses. Many of the rest index legitimately — immediately after their own bounds test, or from a loop already bounded by `m_Width`/`m_Height` — and a mechanical rewrite of all of them that nobody can run is a worse risk than the one being fixed. (An earlier version of this entry said "167 subscripts, six sites converted"; 167 was the count *before* the conversion and "six" matched nothing measured.) Note `GetX()`/`GetY()` return `unsigned short`, so the `x < 0` half of the guard is dead for creature coordinates and live only for computed ones.
>
> Adversarial review then found three more subscripts in this file that should have been converted and were not, all now done: `RemoveCreature` — which this change's own comments cite as the model for the guarded delete, while itself subscripting raw on both the client and server coordinate pairs — plus `AddItem`, the direct sibling of `AddCreature` whose item coordinates arrive from `GCAddItemToZone` exactly as creature coordinates arrive from `GCAddMonster`, and the item loop in `ReleaseObject`, ninety lines below the creature loop in the same function that *was* converted.
>
> **`AddCreature` now returns `false` in two new situations, and the call-site audit that first accompanied this entry was wrong — twice over.** It claimed "all 15 live sites are correct". There are **24** live sites (25 counting one inside a comment block), and **three** mishandled the new rejection. All three are fixed:
>
> - `Client/PacketFunction.cpp` monster type **795** — nulls `pCreature`, dereferences it two lines later.
> - `Client/PacketFunction.cpp` monster type **793** — the byte-identical twin, 109 lines above the first, missed because the first was found by reading forward a few lines rather than the whole function.
> - `Client/Packet/Gpackets/GCAddWolfHandler.cpp` — nulls `pCreature` in the "new creature" arm, then dereferences it ~55 lines later at a point *outside* both arms of the if/else, so no amount of reading near the guard would have found it.
>
> The method that missed them is worth recording, because it is the reusable lesson: scanning a fixed window after the guard finds the easy case and misses the two that matter. What finds them is comparing **indentation** — a use of the pointer at lower indentation than the guard is a use after the enclosing block closed — or simply reading each enclosing function to its end.
>
> One further claim, made by a reviewer rather than the author, is itself wrong and is recorded so it does not get "fixed": `MZone.cpp`'s `AddCreature(g_pPlayer)` discards the return value, but the player **cannot** be rejected by the new guard — `CLASS_PLAYER` takes a branch that sets `bAdd = true` and never reaches the `SectorAt` test. That call ignoring its result is pre-existing behaviour, unaffected by this change.
>
> That a fix makes a previously unreachable NULL dereference reachable is now the second batch running in which this happened. It should be the default expectation whenever a function gains a reject path, and the audit should be mechanical rather than by eye.

#### 🟠 High -- MZone::AddEffect reads m_ppSector[y][x] before the zone-bounds check that appears 70 lines later.

**Category:** memory-safety  |  **Location:** `Client/MZone.cpp:4118`

`if(bDarkNess) { const MSector& sector = m_ppSector[y][x]; if(sector.HasDarknessForbidden() == true) ... }` at 4116-4124. x and y come from `pNewEffect->GetX()/GetY()` at 3989-3990 and are not validated until line 4191 (`if (x<0 || x>=m_Width || y<0 || y>=m_Height)`). The very next block, at 4126-4142, does the check correctly (`if( !( x < 0 || x >= GetWidth() || y < 0 || y >= GetHeight() ) )`), so the first block is a straightforward omission. The zone-boundary path at 4191 explicitly exists because effects legitimately arrive outside the zone (chase effects are allowed through at 4197).

**Failure scenario:** A darkness effect is generated at a target position outside the current zone — the code at 4191 anticipates exactly this — and the read at 4118 dereferences m_ppSector past its bounds, then branches on whatever HasDarknessForbidden() finds in unrelated memory.

**Recommendation:** Move the zone-boundary test at 4191 to immediately after x/y are read at 3990, before any sector access.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, but **not** by the recommended move. Hoisting the boundary test would have skipped the darkness and mercy-ground rejections for the chase effects the code deliberately lets through at ~4197 — the very behaviour the finding notes exists on purpose. The darkness read instead goes through `SectorAt` and skips its test when off-map, matching the shape of the adjacent block that already guards itself.

#### 🟠 High -- MZone::AddCreature reads the uninitialized local bAdd when a creature's move type falls outside the three cases the switch handles.

**Category:** undefined-behavior  |  **Location:** `Client/MZone.cpp:2968`

`bool bAdd;` is declared uninitialized at 2968. The switch at 2987 covers only CREATURE_UNDERGROUND, CREATURE_GROUND and CREATURE_FLYING, with no default. MCreature::MOVE_TYPE (Client/MCreature.h:57-63) also defines CREATURE_FAKE_UNDERGROUND, CREATURE_FAKE_GROUND, CREATURE_FAKE_FLYING and CREATURE_FAKE_NO_BLOCK — MCreature.h:201's IsFakeCreature() enumerates all four. `if (bAdd)` at 3021 then reads an indeterminate value.

**Failure scenario:** A creature with a FAKE_* move type (Client/GameMain.cpp:5877 sets CREATURE_FAKE_UNDERGROUND on ghosts) reaches AddCreature. bAdd is garbage; if it happens to be non-zero the creature is inserted into m_mapCreature without ever being placed in a sector, so later removal fails and — via the UpdateAllCreature defect above — it is freed while still mapped.

**Recommendation:** Initialize `bool bAdd = false;` and add an explicit default: case that logs and returns false.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, and **the finding is wrong twice over — it is a latent-UB guard, not a live bug.**
>
> First, only `CREATURE_FAKE_UNDERGROUND` can reach the switch at all. The block above it calls `SetFlyingCreature()`/`SetGroundCreature()` for anything that is not `IsUndergroundCreature()`, which rewrites `CREATURE_FAKE_NO_BLOCK`, `CREATURE_FAKE_GROUND` and `CREATURE_FAKE_FLYING` to `CREATURE_GROUND`/`CREATURE_FLYING` first. The review's "four FAKE_* values" is one.
>
> Second, the cited live path does not exist. The ghost site calls **`AddFakeCreature`**, which inserts into `m_mapFakeCreature` and never touches a sector or `bAdd`, and `SetMoveType(CREATURE_FAKE_UNDERGROUND)` is called only there and only after the creature is already in that map. No live path reaches `AddCreature` with a FAKE move type. The initialiser and the `default:` case are still correct, but nothing was reproduced and the chained consequence the finding describes — a creature freed while still mapped — cannot occur through this route.

#### 🟠 High -- CTypeTable's index bounds checks are compiled only under _DEBUG, which the build system deliberately never defines — so every table lookup is unchecked in all configurations including debug-asan.

**Category:** build  |  **Location:** `CMakeLists.txt:16`

Client/CTypeTable.h:41-65 wraps the range test in all three accessors (`const operator[]`, `operator[]`, `Get`) in `#ifdef _DEBUG`. CMakeLists.txt:16 states "Note: We do NOT define _DEBUG here because it conflicts with DebugInfo.h", and no add_compile_definitions adds it. Makefile:43/59 build with -DCMAKE_BUILD_TYPE=Debug, which on Clang/GCC does not define _DEBUG either. The documented developer command in CLAUDE.md is `make debug-asan`. The affected tables are indexed with server-supplied values throughout this area — e.g. Client/MZone.cpp:2958 `(*g_pCreatureTable)[pCreature->GetCreatureType()]` where the type came from GCAddMonsterHandler.cpp:120, and Client/MCreature.cpp:2356 `(*g_pEffectStatusTable)[status]`.

**Failure scenario:** A packet carries a creature type beyond the loaded creature table. In MSVC Debug the guard silently returns a dummy; in every build the project actually ships and tests, the same input reads past the end of m_pTypeInfo.

**Recommendation:** Make the check unconditional (it is a single comparison on a hot-but-not-critical path), or gate it on a project-owned macro that the CMake Debug and asan configurations do define.

> ✅ **Fixed** on branch `harden/high-severity-batch1` — but **this finding's premise is false and its filing is wrong**, which matters because it is filed as a *build* finding against `CMakeLists.txt:16`.
>
> It claims the build system "deliberately never defines `_DEBUG`", so "every table lookup is unchecked in all configurations". Not so: MSVC defines `_DEBUG` automatically under `/MDd`, and CMake only declines to *add* it — see the Traps section of `CLAUDE.md`. The check has been running in both Debug trees all along and is dead only in Release. The real defect is narrower than described, the correct location is `Client/CTypeTable.h`, the category is memory-safety rather than build, and **no CMake change was needed or made**. The duplicate write-up in Text & Strings states it correctly.
>
> The second half of the finding — that `LoadFromFile` reads `numSize` as a raw `int` from a data file and hands it to `Init` unvalidated — was untouched by any of that and was entirely real. All three accessors now check unconditionally and also reject a NULL table; `Init` rejects `size <= 0` (a negative made `new Type[]` undefined) and publishes `m_Size` only after the allocation succeeds; both `LoadFromFile` variants validate the file-supplied count against the bytes actually left in the file. `LoadFromFile_NickNameString`'s `int numSize;` was *also* uninitialized, which neither write-up mentions.
>
> **The behaviour change to weigh at runtime, with the exposure measured rather than guessed.** Out of range now returns a default-constructed entry in Release too, and for `MStringArray` that is an `MString` whose `GetString()` is NULL — so Release moves from *reading past the array* (garbage pointer, sometimes survivable) to a *deterministic NULL dereference* at call sites that copy from the result without testing. That is the right trade: it makes Release behave like the Debug build that is actually run, and a NULL dereference beats a wild pointer.
>
> The unaudited exposure was first written here as "roughly 600 call sites". That figure was wrong — borrowed from C19's unrelated `sprintf` count. Measured: **`(*g_pGameStringTable)[` alone appears 4,085 times**, and all `(*g_pXTable)[` forms together **5,755**. `g_pGameStringTable` is `MStringArray : CTypeTable<MString>` — precisely the instantiation whose out-of-range result now yields a NULL `GetString()`. These historical counts identify a source/caller audit concern; live-server verification is optional and is not a merge or completion gate.
>
> ✅ **Count/stream residual fixed (2026-09-22):** every count is checked against the remaining file bytes before replacing or allocating table storage; failed prefixes, failed streams and unmeasurable input are rejected with failure state preserved. Nickname records include their index in the minimum byte bound, require a complete index before lookup, and report failed entry reads. Both loops stop on stream failure. Valid completed rows remain available after a later malformed body; this is not an atomic whole-table loader. `tests/unit/test_type_table_input.cpp` owns short counts, the former 65,536-entry trust boundary, stream states, nickname indices/bodies and valid record boundaries.
>
> ✅ **Accessor residual fixed (2026-09-22):** `operator[]` and `Get` expose const rows and an immutable default; writes use `GetMutable` (null for an invalid index) or `Set` (false without assignment). Const internal access preserves constness, including nested tables. Production writers and fixture setup use checked mutation, while readers use const references and iterators. `tests/unit/test_type_table_access.cpp` owns cross-table fallback isolation, invalid writes, valid row ownership and initialized storage. The ten experience loaders validate counts and complete rows before checked publication; `test_experience_input.cpp` covers malformed input and following-record boundaries. This closes mutable fallback poisoning, not every caller's handling of a default row or null string.
>
> ✅ **Skill-domain experience input fixed (2026-09-22):** the separate `MSkillDomain` reader no longer publishes partially read experience values. Its manager validates counts against the remaining 16-byte records before loading, rejects invalid domains with a failed stream, and stops after read failure. Tests first reproduced 16 partial-row overwrites and five count/domain rejection failures; `test_skill_core.cpp` owns these cases plus truncated prefixes and an empty section followed by another record. Complete earlier records remain published if a later record fails.

#### 🟡 Medium -- Format strings for the in-game clock are taken from a loadable string table and printed into a fixed 80-byte stack buffer.

**Category:** security  |  **Location:** `Client/CGameUpdate.cpp:6443`

> ✅ **Already fixed; reconciled 2026-09-17:** both clock/date sites use `SafeFormat::Format` with the actual array destination and typed arguments. The checked formatter bounds output and rejects incompatible format conversions; its library tests and the format-arity checker cover those contracts.

`char str[80];` at 6435, then `sprintf(str, (*g_pGameStringTable)[STRING_DRAW_GAME_TIME].GetString(), hour, minute, second);` at 6443 and the same pattern for STRING_DRAW_GAME_DATE at 6457. g_pGameStringTable is loaded from a game data file (Client/MGameStringTable.cpp), so both the specifier list and the output length are controlled by that file, not by the code. Neither the argument count nor the resulting length is bounded.

**Failure scenario:** A localizer (or a tampered data file) writes "%s %s %s" for the time string. sprintf reads three ints as pointers and dereferences them; a long literal prefix instead overflows str[80] on the stack. This runs every frame in the main draw loop.

**Recommendation:** Switch to snprintf with sizeof(str), and validate at load time that these table entries contain only the expected specifiers.

#### 🟡 Medium -- Roughly 400KB of stub and duplicate source sits in the tree explicitly excluded from the build, silently shadowing the live implementations.

**Category:** dead-code  |  **Location:** `Client/GameFunctions.cpp:36`

CMakeLists.txt:609-618 removes Client/MissingGlobals.cpp, Client/GameHelpers.cpp, Client/GameFunctions.cpp, Client/GamePacketFunctions.cpp (45KB), Client/ActionFunctions.cpp and Client/MitemTableinit2.cpp (302KB) from CLIENT_MAIN_SOURCES because they duplicate symbols. GameFunctions.cpp defines empty-bodied `SetWeather(int,int)` (line 36), `LoadZone(int)` (109), `LoadZoneInfo(int)` (117), `LoadCreature(int)` (101) and `InitPlayer(int,int,int)` (20) with signatures identical to the real definitions in GameMain.cpp at 4300, 2117, 2902, 1952 and 3260. ActionFunctions.cpp:19 is an empty stub of ExecuteActionInfoFromMainNode, whose real 150-line body is at Client/PacketFunction.cpp:1545; counting Client/SDLMain.cpp:90 and Client/GameHelpers.cpp:193 there are five definitions of that one function in the tree.

**Failure scenario:** A contributor greps for SetWeather, finds GameFunctions.cpp:36, edits the empty stub, and observes no behaviour change — or adds the file back to the build and gets either a duplicate-symbol link failure or silently no-op weather, zone loading and skill execution.

**Recommendation:** Delete these files (git history preserves them) or move them under a clearly-named unbuilt directory. At minimum add a header banner to each stating it is excluded from the build and naming the live implementation.

> ✅ **Fixed** in `b4665e0` (branch `restructuring/dead-exclusion-graveyard`, `docs/RESTRUCTURING.md` task 5.2 fourth slice), completing what `MitemTableinit2.cpp`'s deletion started in the task's first slice. All six files this finding names are deleted, with `GlobalVariables.cpp` and a stale copy of the `GCNotifyWin` packet class and its handler alongside them, and the `list(REMOVE_ITEM)` block that excluded them one by one is gone. Verified before deleting: none of the eight appears as a `ClCompile` entry in any of the 70 generated project files across the four build trees, so no target compiled them; and every one of the 171 function definitions they carried is also defined by a file the build does compile (mostly `Client/PacketFunction.cpp`, `Client/GameMain.cpp` and `Client/RenderingFunctions.cpp`) — none was unique, so the deletion cannot change the linked program. Executable-side code with no test path; the four build trees are the check.

#### 🟡 Medium -- Exception text derived from malformed server packets is passed as the format string to LOG_ERROR/DEBUG_ADD.

**Category:** security  |  **Location:** `Client/GameMain.cpp:285`

> ✅ **Fixed (2026-09-17):** both direct `LOG_ERROR` calls now pass exception text through a literal `%s`. The `DEBUG_ADD` path already uses a literal `%s` in its macro. Server-derived percent signs are data at all three sites. Executable-side regression guard verified by full Debug/ASan builds and existing automated checks.

`LOG_ERROR( t.toString().c_str() );` at lines 285 and 287, and `DEBUG_ADD(t.toString().c_str())` at 436. LOG_ERROR expands to `log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)` (Client/DebugLog.h:71), so the first argument is consumed as a printf format with no variadic arguments supplied. The handler is the catch block for InvalidProtocolException raised by packet parsing, and the exception text embeds the offending packet's description.

**Failure scenario:** A malformed packet whose type or payload string contains "%s" causes vsnprintf to read a non-existent vararg — an information disclosure from the stack at best, a crash at worst; "%n" is a write primitive on platforms that still honour it.

**Recommendation:** Use `LOG_ERROR("%s", t.toString().c_str())` at all three sites; the DEBUG_ADD macro already does this correctly, which is why the two-argument form exists.

#### 🟡 Medium -- An MString object is passed through a variadic %s, and the string it names has already been freed one line earlier.

**Category:** undefined-behavior  |  **Location:** `Client/GameMain.cpp:3663`

> ✅ **Fixed (2026-09-17):** all three WAV-load failure paths log their bounded C-string filename and preserve the sound-table entry for later retries. The previous variadic `MString` use had already been replaced; the remaining destructive `Filename.Release()` calls are now gone. Same diagnostic on all platforms. Executable-side regression guard; no runtime reproduction is claimed.

Lines 3661-3663: `(*g_pSoundTable)[soundID].Filename.Release();` followed by `DEBUG_ADD_FORMAT("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, (*g_pSoundTable)[soundID].Filename );`. Filename is an MString (Client/MSoundTable.h:44). MString has a virtual destructor and a user-declared copy constructor (Client/MString.h:16-19), so it is not trivially copyable and passing it through `...` is undefined; MSVC warns C4840 and pushes the object bytes, Clang emits a call that traps. Independently, Release() (Client/MString.cpp:86-93) has already `delete[]`'d and NULLed m_pString, so even the intended value is gone.

**Failure scenario:** Any WAV that fails to load logs garbage or crashes, and the sound-table entry is permanently destroyed so the sound can never be retried for the rest of the session.

**Recommendation:** Capture the name before Release() and pass `.GetString()`: `const char* fn = (*g_pSoundTable)[soundID].Filename.GetString(); ... Release(); DEBUG_ADD_FORMAT(..., fn);` — or better, do not destroy the table entry on a transient load failure.

#### 🟡 Medium -- AddEffectStatus re-assigns the effect sprite type from a data-table field after the bounds check, then uses the new value to index two arrays.

**Category:** memory-safety  |  **Location:** `Client/MCreature.cpp:2385`

> ✅ **Fixed (2026-09-17):** both add and remove status paths recheck the remapped female sprite type before any table/attachment-array access, and reject a missing attachment array. The indices are unsigned `TYPE_EFFECTSPRITETYPE` values. Executable-side regression guard verified by full builds and source audit.

Line 2375 validates `if (type >= g_pEffectSpriteTypeTable->GetSize()) return false;`. Line 2385 then overwrites it: `type = (*g_pEffectSpriteTypeTable)[type].FemaleEffectSpriteType;` with no re-validation. The new value is used at 2396 to index the same table (`(*g_pEffectSpriteTypeTable)[type].FrameID`), at 2408 to read `m_bAttachEffect[type]`, and at 2513 and 2556 to write `m_bAttachEffect[type] = true`. m_bAttachEffect is allocated as `new bool[(*g_pEffectSpriteTypeTable).GetSize()]` (Client/MCreature.cpp:701), so an out-of-range FemaleEffectSpriteType is an out-of-bounds heap write of one byte.

**Failure scenario:** An effect-sprite table entry whose FemaleEffectSpriteType field exceeds the table size (a stale or hand-edited data file) causes a female creature gaining that status to write true past the end of m_bAttachEffect.

**Recommendation:** Re-run the `type >= GetSize()` check after the reassignment at 2385, or validate FemaleEffectSpriteType once at table load time.

#### 🟡 Medium -- Five sites call list::empty() and discard the result where the intent is clearly clear() — the pathfinding direction list is never cleared.

**Category:** correctness  |  **Location:** `Client/MPlayer.cpp:10402`

> ✅ **Fixed (2026-09-17):** the four standalone `empty()` calls in `MPlayer` and the one in `MFakeCreature` are now `clear()`. Movement-error recovery, blockage and destination resets discard their stale queued directions. The remaining `empty()` uses are queries in conditions. Executable-side regression guard verified by full builds and source audit.

`m_listDirection.empty();` as a standalone statement, directly under the comment "길찾기 제거" (remove pathfinding). empty() is a const query; the list is untouched. The same mistake appears at MPlayer.cpp:6018, 10518 and 10805, and at Client/MFakeCreature.cpp:2003. At 10402 the surrounding code is the MoveOK-after-MoveError recovery path, which resets m_sX/m_sY, m_MoveCount and the destination fields — clearing the queued direction list is plainly part of that reset.

**Failure scenario:** The server rejects a move and then sends MoveOK. The player is snapped to the server position, but the stale direction queue survives, so on the next ActionMove the character walks the old path from the new position and desynchronises again.

**Recommendation:** Replace all five with clear(). A -Wunused-result / [[nodiscard]] pass would have caught these; consider enabling -Wunused-value on this target.

#### 🟡 Medium -- MZone::LoadFromFile never checks stream state, so a truncated map file leaves the object count uninitialized and drives an unbounded allocation loop.

**Category:** file-parsing  |  **Location:** `Client/MZone.cpp:1013`

> ✅ **Fixed (2026-09-18):** map records/header I/O now live in `gamemodel`, and `ZoneMapData` parses the complete file before `MZone` changes live state. It checks every structural read, available record bytes, duplicate IDs, object tags and sector positions. Resource budgets are explicit: 64 MiB input, 1,024 tiles per side, 262,144 sectors/objects and 1,048,576 stored positions. Invalid input leaves the previous map intact; records are owned until the zone accepts them. Live row allocation is also exception-safe. Regression tests cover all five object kinds, the existing BYTE/WORD header layouts, every truncation in a representative map, hostile counts/types/positions and inclusive dimension/sector/position/file limits. The extracted record tests also pin polymorphic destruction and the interaction type's actual two-byte field. These are automated regression guards, not a claimed reproduction of the original runaway allocation.

`int size;` is declared uninitialized at 1013 and filled by `file.read((char *)&size, 4);` at 1034. If the stream has already entered a failed state — which a truncated or short map file guarantees, since none of the ~10 preceding read() calls are checked — read() leaves the destination untouched and `size` stays indeterminate. The loop at 1051 then runs `for (i=0; i<size; i++)` allocating a new MImageObject per iteration. The same absence of validation applies to m_Width/m_Height at 940-941: only zero is rejected (944), so a 65535x65535 header drives `new MSector*[65535]` plus 65535 row allocations in Init() (272-278).

**Failure scenario:** A truncated .map file (partial download, disk error) yields a garbage `size` in the tens of millions; the client allocates until it exhausts memory, with each object's LoadFromFile silently reading nothing.

**Recommendation:** Check `file.good()` after each structural read and bail out; sanity-cap m_Width/m_Height and the object count against plausible maxima before allocating.

#### 🟡 Medium -- MZone::AddEffect contains ~120 lines of pointer-validity heuristics — including a hardcoded ASAN heap address range and a try/catch around a raw dereference — instead of a fix for the underlying use-after-free.

**Category:** maintainability  |  **Location:** `Client/MZone.cpp:3949`

> ✅ **Fixed (2026-09-17):** removed the address-range tests, exception swallowing around dereferences, poison probes, canary reads and shadow-pointer machinery (including its initialization hooks). The remaining checks concern actual state: a supplied owned effect, an available loaded table, valid indices and zone coordinates. Ownership is explicit below. This does not claim reproduction or diagnosis of an earlier suspected use-after-free; real invalid accesses remain visible to ASan instead of being hidden by address guesses. Full Debug/ASan builds and CTest pass.

Lines 3947-4081 include: `if (ptr_value < 0x1000)` wild-pointer tests (3950, 4028); `try { x = pNewEffect->GetX(); } catch (...)` at 3988-4000, which cannot catch the SIGSEGV a dangling dereference actually raises on Clang/GCC; `volatile bool table_valid = true;` at 4053 that is never set false, making the branch at 4077 unconditional; a canary read `test_frame_id = (*g_pEffectSpriteTypeTable)[0].FrameID;` at 4079 whose result is discarded; and at 4066-4073 `if (table_addr >= 0x632000000000ULL && table_addr <= 0x6320000FFFFFFULL)` with the comment "This looks like the SDL surface region that gets freed!" — an ASAN-specific magic address baked into shipping logic. Comments at 3960 and 4018 ("ROOT CAUSE FIX") show the real defect was never located.

**Failure scenario:** The heuristics give no protection on a normal build (freed heap is usually still mapped and rarely at those addresses) while making the function 500 lines long and hiding the genuine bounds bug at 4118 in the noise. A contributor reading this cannot tell which checks are load-bearing.

**Recommendation:** Delete the address-range, try/catch and canary code; if there is a real UAF on MEffect, reproduce it under ASAN and fix the lifetime at the owning site (the generators and m_listWaitEffect) rather than at the consumer.

#### 🟡 Medium -- MZone::AddEffect has an inconsistent ownership contract: four early-return-false paths leak the effect while eleven others delete it, and all callers assume the callee frees.

**Category:** resource-leak  |  **Location:** `Client/MZone.cpp:3984`

> ✅ **Fixed (2026-09-17):** `AddEffect` owns its argument through a local `unique_ptr` until one of three successful container insertions transfers it to the wait list or effect map. Every rejection and pre-transfer exception now destroys the effect. A ready wait-list node is erased before calling `AddEffect`, so failure cannot leave a freed pointer queued. The header documents the ownership contract. Audited all return/transfer paths and callers; executable-side regression guard verified by full Debug/ASan builds and CTest, without a claimed runtime reproduction.

AddEffect deletes pNewEffect before returning false at lines 4009, 4022, 4030, 4039, 4071, 4121, 4138, 4167, 4173, 4215 and 4426, but returns false without deleting at 3944, 3952, 3984 and 3999 — the last two with the comment "Don't delete the effect". Callers uniformly rely on the delete-on-failure contract: Client/MAttackCreatureEffectGenerator.cpp:88-94 does `if (g_pZone->AddEffect(pEffect)) {...} else return false;` with no delete, and the same shape appears in MAttackZoneEffectGenerator.cpp:158, MFallingEffectGenerator.cpp:63, MRippleZoneEffectGenerator.cpp:84 and roughly twenty other generators.

**Failure scenario:** g_pEffectSpriteTypeTable is momentarily NULL during a zone transition, or the ASAN poison check at 3980 fires. AddEffect returns false without freeing; the generator returns false without freeing. Effects are created many times per second, so this leaks continuously for as long as the condition holds.

**Recommendation:** Make AddEffect take ownership unconditionally: delete pNewEffect on every false return, and document that in the header.

#### 🟡 Medium -- ExecuteActionInfoFromMainNode leaks the caller-owned MActionResult when the action-info id is out of range.

**Category:** resource-leak  |  **Location:** `Client/PacketFunction.cpp:1574`

> ✅ **Fixed (2026-09-17):** an invalid action ID or absent action table deletes the incoming `MActionResult` before returning. Valid requests retain their existing transfer to `MEffectTarget`, or execute-and-delete behavior when no effect phases exist. Executable-side regression guard verified by full builds and ownership-path audit.

The function owns pActionResult: it deletes it on the ACTIONINFO_NULL path (1565-1567) and executes-then-deletes it on the normal path (1687-1693). The bounds-check path added at 1574-1577 (`if( nActionInfo >= g_pActionInfoTable->GetSize() ) { DEBUG_ADD_FORMAT(...); return; }`) returns without deleting. nActionInfo originates in action-result packets, so an unrecognised or malformed action id leaks the whole MActionResult node tree on every occurrence.

**Failure scenario:** A server running a newer skill table sends an action id the client does not know. Every such packet leaks an MActionResult and its child nodes; a sustained mismatch leaks steadily for the whole session.

**Recommendation:** Add `if (pActionResult != NULL) delete pActionResult;` before the return at 1577, matching the ACTIONINFO_NULL path.

#### ⚪ Low -- CheckTime() returns immediately, making 125 lines of speed-hack detection unreachable, and ReleaseGameObject has a branch that can never be taken.

**Category:** dead-code  |  **Location:** `Client/GameMain.cpp:461`

> ✅ **Closed (2026-09-17):** removed `CheckTime`, its call/declaration and five timing variables used only by its unreachable body. Removed the impossible second zone-removal branch after `ReleaseGameObject` has cleared `g_pZone`; the earlier `RemovePlayer` still runs. R14 decreases from four to two calls, both log-file naming. Dead-code removal verified by full builds and ratchets.

CheckTime() opens with a bare `return;` at line 461, so everything from 463 to 587 — the timeGetTime/GetTickCount drift comparison and the window-title process scan — is dead. The function is still called from the main loop. Separately, ReleaseGameObject sets `g_pZone = NULL;` at line 2030 and then tests `if (g_pZone!=NULL) { g_pZone->RemoveCreature(g_pPlayer->GetID()); }` at 2080-2083, which can never execute; the removal happens to be harmless only because RemovePlayer() already ran at 2028.

**Failure scenario:** No runtime failure, but both blocks read as live logic. A contributor investigating the anti-cheat path, or the player-teardown ordering, will spend time on code that never runs — and the unreachable-code warning is buried among thousands of others.

**Recommendation:** Delete CheckTime()'s body and its call site if the check is intentionally retired, or move the disable behind a named flag. Remove the impossible branch at 2080-2083.

#### ⚪ Low -- Add_GDR_Ghost formats a size_t with %d, and reuses a 64-byte buffer whose earlier contents it depends on.

**Category:** undefined-behavior  |  **Location:** `Client/GameMain.cpp:5852`

> ✅ **Fixed (2026-09-17):** ghost position names use `%zu`; the zone, file data, XML nodes/coordinates and new creature are checked before use. Each record requires both coordinates inside the current zone, so a missing attribute cannot reuse the previous ghost's value. The file wrapper releases its buffer on every return. Executable-side regression guard verified by full builds; no runtime reproduction is claimed.

`for( size_t GhostCount = 0; GhostCount < GhostMax; GhostCount++ ) { sprintf(szTempBuffer, "Position%d", GhostCount+1); ... }` at 5850-5853. GhostCount+1 is size_t (8 bytes on x64 Windows and Linux) passed to a %d that reads 4 — undefined and, on some ABIs, misaligning the rest of the varargs. The function also has no NULL check on `parser.parse((char *)GhostFile.GetFilePointer(), &computerTree)` at 5843, and does not check the result of `NewFakeCreature(...)` at 5868 before passing it to AddFakeCreature.

**Failure scenario:** On the 64-bit build the ghost position lookup at 5853 searches for a garbled node name, so no ghosts spawn in the Gilles de Rais lair maps (zones 1412/1413) and the failure is silent.

**Recommendation:** Cast explicitly: `sprintf(szTempBuffer, "Position%d", (int)(GhostCount+1));` — or use %zu. Add the two missing NULL checks.

#### ⚪ Low -- An operator-precedence error inverts the datagram test guarding SendBugReport.

**Category:** correctness  |  **Location:** `Client/GameMain.cpp:281`

> ✅ **Fixed (2026-09-17):** the two stream error paths explicitly report `InvalidProtocolException` messages without the datagram marker (`strstr(...) == NULL`), and both nested conditions have braces. This restores the stream-reporting intent; the UDP manager retains its explicit positive marker check. Executable-side regression guard, verified by full builds; no runtime reproduction is claimed.

`if( !strstr( t.toString().c_str(), "(datagram)" ) == NULL )` at line 281 and again at 432. `!strstr(...)` yields a bool, so the expression is `(!p) == 0`, i.e. `p != NULL` — the bug report is sent only when the exception text does contain "(datagram)". The surrounding code and the stray `!` indicate the author meant the opposite (report non-datagram protocol errors). Line 432 additionally has no braces on the outer if, so the two conditions silently chain.

**Failure scenario:** Stream protocol violations — the ones worth diagnosing — are never reported, while every datagram hiccup is. The bug is invisible because the code compiles cleanly.

**Recommendation:** Write the condition as `if (strstr(t.toString().c_str(), "(datagram)") == NULL)` if the intent is to skip datagram errors, and add the missing braces at 431-433.

---

## Text & Strings

**Grade:** D  |  **Findings:** 23

**Scope:** Text and strings — Client/TextSystem, string/encoding utilities (MString, CMessageArray, CToken, basic/Platform.h shims), the game string table, and format-string usage across Client/ and VS_UI/

**Health assessment:** This area is the weakest part of the client I looked at. Every user-visible string in the game is formatted through unbounded `sprintf`/`vsprintf`/`wsprintf` into fixed stack, static, or heap buffers, and the *format strings themselves* come from a binary data file (`g_pGameStringTable` is populated exclusively by `CTypeTable::LoadFromFile`) — a textbook CWE-134 exposure repeated in roughly 600 call sites. At least four network-reachable buffer overflows are concretely reachable from a hostile or compromised game server (guild-chat name, system message, creature chat/personal-shop string, quest title), and one of them exists because a `BYTE > 256` length guard is dead code. The SDL text migration added its own defects: an unbounded glyph cache keyed on server-supplied colours, a UTF-8 decoder in the IME editor that reads past the terminator, an encoding converter that treats `SDL_ICONV_EILSEQ` as success, a `WideCharToMultiByte` shim that is not a conversion at all and whose return value drives a stack write, and a `g_PossibleStringCut` reimplementation whose second parameter silently changed meaning from "character index" to "pixel width" while every caller kept passing an index. Compounding all of it, `MGameStringTable.cpp` — 1630 lines of literals, many already irreversibly mojibake'd — is dead code, because its only caller (`VS_UI/WinMain.cpp`) is excluded from every CMake configuration, so contributors editing it change nothing at runtime.

#### 🔴 Critical -- CMessageArray::AddFormat and AddFormatVL vsprintf into a fixed static 4096-byte buffer with a data-file-supplied format string.

**Category:** memory-safety  |  **Location:** `Client/CMessageArray.cpp:333`

Both `AddFormat` (line 320) and `AddFormatVL` (line 259) declare `static char Buffer[4096]` and call `vsprintf(Buffer, format, vl)` with no bound. Callers pass table strings as the format, e.g. `g_pSystemMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_PET_DIE_WARNING].GetString(), petName.c_str(), szTemp)` (Client/ModifyStatusManager.cpp:1314) and `g_pNoticeMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_EVENT_FLAG_WAR_WINNER].GetString(), ...)` (GCNoticeEventHandler.cpp:211). The buffer is also `static`, so it is shared across all CMessageArray instances and is not reentrancy- or thread-safe despite the __BEGIN_LOCK/__END_LOCK macros expanding to `((void)0)` in every non-OUTPUT_DEBUG build (lines 51-52).

**Failure scenario:** A table entry with a wide field width ("%9000d") or a long %s substitution overflows the 4096-byte static buffer, corrupting adjacent globals. Because the same buffer backs every message array, a nested AddFormat (e.g. from a logging path) silently clobbers an in-flight message.

**Recommendation:** Replace vsprintf with vsnprintf(Buffer, sizeof(Buffer), ...), make the buffer a local (or per-instance) rather than static, and route the format string through the validated-format path from the previous finding.

> ✅ **Fixed** in `0e9d247` (branch `harden/text-format`). Both functions use vsnprintf into a function-local buffer, and the return value is captured so that the negative encoding-error path — where the contents are unspecified — empties the row instead of letting strlen walk uninitialised stack into the log file and the visible chat row. Regression guard, executable-only code.

#### 🔴 Critical -- Every printf format string in the client comes from a binary data file, and is passed to sprintf/vsprintf as the format argument in roughly 600 places.

**Category:** security  |  **Location:** `Client/GameInit.cpp:1604`

`(*g_pGameStringTable).LoadFromFile(gameStringTableTable2)` replaces the entire string table with contents read from the file named by `FILE_INFO_STRING`. Those entries are then used directly as the format argument: e.g. `sprintf(szBuf, (*g_pGameStringTable)[UI_STRING_NOTICE_EVENT_GOLD_MEDALS].GetString(), pPacket->getParameter())` (Client/Packet/Gpackets/GCNoticeEventHandler.cpp:295), `sprintf(pMsg, (*g_pGameStringTable)[UI_STRING_MESSAGE_SYSTEM].GetString(), message)` (GCSystemMessageHandler.cpp:120), and 83 call sites of the zero-argument form `wsprintf(sz_buf, (*g_pGameStringTable)[X].GetString());` (e.g. VS_UI/src/VS_UI_Description.cpp:271-310 into `char sz_buf[50]` declared at line 215). The compile-time literals in MGameStringTable.cpp are irrelevant here — see the separate finding that InitGameStringTable() is never called.

**Failure scenario:** Anyone who can write the Strings data file (a malicious patch/CDN, a repacked client distribution, or simply a user tricked into installing modified game data) puts "%n" or a long "%s" run into a table entry. sprintf then performs an arbitrary write or dereferences stack garbage as a char*. The zero-argument `wsprintf(buf, tableString)` form is exploitable with any conversion specifier at all, since there are no varargs to consume.

**Recommendation:** Never pass externally-sourced strings as the format argument. Introduce a checked formatter that validates the specifier sequence of a table entry against the argument types at the call site (or at minimum a table-load-time validator rejecting %n and counting/typing specifiers), and convert the zero-argument cases to snprintf(buf, size, "%s", tableString).

> ✅ **Fixed** across five slices of `docs/RESTRUCTURING.md` task 5.4 (`ef81654`, `5918ce4`, `e76e1db`, `b375b78`, and the fifth on branch `harden/format-indirect`). **315 call sites** are converted to the checked formatter (321 when it closed; the eleventh clocks slice, 2026-09-17, deleted a dead quest caption in `MTopView` holding six).
>
> **Read the evidence, not the zero.** This entry was marked Fixed once before, on 2026-09-04, on the strength of ratchet R7 reaching 0 — and that was wrong, because R7 only counts a table lookup *spelled at the format argument*. 24 live sites reached a format indirectly, through a static array or a local the entry had been copied into first, and R7 cannot see any of them. So the claim here rests on five measurements rather than one, each named so it can be re-run and disagreed with:
>
> 1. **R7 = 0** — no table lookup spelled at a format argument, across the `sprintf` family (now including `fprintf`), the counted family, `AddFormat`, the offset-append form, and `.Format`.
> 2. **R8 = 43** — the new ratchet, which asks the question R7 cannot: at a printf-family call, is the format a string literal? 43 calls across `Client`, `VS_UI` and `basic`, headers included, take a format that is not, and all 43 were read: 28 vararg forwarders where the format is the function's own parameter, 6 inside `SafeFormat`'s own `Emit` (where the format is a specification it built and validated itself), 3 literals behind `TEXT()`/`_T()`, and 6 declarations rather than calls. **This first read 13**, and 13 was not the population — the review round found that R8 was missing the `AddFormat` family, bare `printf`, `basic/` and every header. Nothing in the tree changed between the two numbers; the conclusion held, but it had not been measured over the population it claimed.
> 3. **The forwarders' own callers**, swept for a non-literal first argument: **zero**.
> 4. **The family list came from the tree, not from memory** — every identifier ending in `printf` in `Client`, `VS_UI` and `basic`. That is how `fprintf` (550 calls) and `vswprintf` got into both ratchets, neither of which the first draft of R8 listed. The enumeration command itself had to be fixed: as first written it required an identifier character *before* `printf` and so could never have found the 86 bare `printf(` calls it was offered as the authority for.
> 5. **Parenthesised destinations**, which both ratchets skip so a joined stream cannot run across two statements: 16 sites, every one with a literal format.
>
> What is still blind, written down rather than left to be discovered: a printf behind a macro; a destination expression containing parentheses, which both patterns exclude so a joined stream cannot run across two statements; the comment stripping in both ratchets, which is not string-literal aware; and a *new* printf-shaped method other than `MString::Format`. A finding in any of those falsifies this entry, and the way to look is a sweep over the format argument — never another spelling of the lookup. The review round of the fifth slice found three such holes at once, so treat that list as the ones known today rather than the ones there are.
>
> **The fifth slice (2026-09-04) took the 24.** Twenty-one in `VS_UI/src/VS_UI_ExtraDialog.cpp` now go through `AllocAskMessage`, which allocates and formats in one place so the bound cannot drift from the destination; three in `VS_UI_GameCommon.cpp` take the array overload directly. Three more that no sweep in the first four slices could have found were caught while checking R8's own blind spots: `Client/GameUI.cpp` passed a table entry to `MString::Format`, an ordinary varargs printf reached as a *method*. `MString` is in `gamemodel`, so its checked sibling `MString::FormatChecked` is library code with **five tests** — the first part of this finding's fix that a test binary can reach at the call-site end rather than only inside the formatter.
>
> The worst of them, and what it looked like **before** that slice — `VS_UI/src/VS_UI_ExtraDialog.cpp` filled `m_sz_question_msg[][]` from the table in `InitString()` and then did
>
> ```cpp
> m_sz_question_msg_temp[0] = new char [strlen(m_sz_question_msg[type][0])+1];
> sprintf(m_sz_question_msg_temp[0], m_sz_question_msg[type][0]);
> ```
>
> — a data-file format, **no varargs at all**, into a heap block sized from the format string. A `%s` in that entry read a stack word as a `char*` and copied it, unbounded, into a buffer sized for something else. 21 sites in that file, 12 of them passing nothing; 3 more in `VS_UI_GameCommon.cpp` (`info_vampire_title_string[num]`, `info_ousters_title_string[num]`, `chingho_name[0]`); and one in `Client/PacketHandler/GCSystemMessageHandler.cpp`, which assigned the entry to a local named `pFormat` first — in a directory the first slice reported finished. That last one went with the fourth slice, which is what makes the claim about `Client/PacketHandler` true rather than merely unmeasured; the rest went with the fifth.
>
> **What a conversion buys**, at every one of the 315: a conversion with no argument left, an argument of the wrong type, a `%n`, or an argument-supplied width is printed as text rather than performed, and every destination is bounded. Neither half of the defence can tell that an entry *means* what its call site passes — a mistranslated entry still prints wrongly. What is gone is printing something the caller never owned.
>
> Two further limits: `tests/tools/check_format_arity.pl` audits arity against the built-in English table — what the default build formats with, since `InitGameStringTable` installs it over the file data — but no `String.inf` ships here, so `LANGUAGE != 3` entries cannot be checked at all and rest on the run-time refusal. And of 384 conversion positions only 120 get a *type* comparison, because an argument whose kind cannot be told is deliberately not a failure. (Those two numbers were carried over as "386" and "117" under the word *unchanged* when the fifth slice added seven sites that moved both.)
>
> The five slices and their review rounds are in `docs/RESTRUCTURING.md` task 5.4. What follows is the original narrowing, kept because it is what the numbers above were measured against.
>
> ⚠ **Narrowed, not fixed**, in `31f5f2f` (branch `harden/text-format`). All 100 zero-argument call sites — the exploitable half, where a bare `%s` reads a stack word as a `char*` with no varargs to consume — are now `snprintf(dest, sizeof(dest), "%s", GetGameString(id))`: bounded, and the data is an argument rather than a format. `GetGameString` also range-clamps the id and answers `""` for one it cannot resolve, which matters because `CTypeTable::operator[]` returns a default-constructed `MString` out of range and that `MString`'s `GetString()` is NULL. (This sentence originally said the bounds check itself compiled away outside Debug. That was already false when written: `e65ab7a`, the same day, made all three `CTypeTable` accessors check in every build. Corrected 2026-09-03 after the same stale claim was carried into the task 5.4 work and caught there.) A load-time gate (`SanitizeGameStringTable`) rejects entries carrying `%n`, an argument-supplied width, a width or precision of 32 or more, a whole-entry width budget over 256, a floating-point conversion, or a wide conversion that would retype a `char*` as `wchar_t*`. **The ~140 call sites that pass arguments still take their format from `String.inf`** and are covered only by that gate, which cannot check arity. Note also that the gate has no effect in the default English build, where `InitGameStringTable` reallocates the table from source literals immediately after and discards the scrubbed data; it protects `LANGUAGE != 3`.

#### 🔴 Critical -- MCreature::SetPersnalString copies an unbounded newline-delimited segment into a 21-byte heap row.

**Category:** memory-safety  |  **Location:** `Client/MCreature.cpp:4917`

`m_ChatString[i] = new char[g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1]` (MCreature.cpp:678), which defaults to 21 (ClientConfig.cpp:117). In SetPersnalString the loop sets `endIndex = startIndex + MAX_CHATSTRING_LENGTH` (20), but if the string contains a '\n' (MCreature.cpp:4885-4891) it *overwrites* endIndex with `strlen(str+startIndex)+startIndex` — the distance to that newline, which is unbounded. Because `*find = '\0'` was already written while `len` still holds the original full length, `endIndex >= len` is false whenever the newline is not the last character, so control falls into the else branch and the manual copy loop at lines 4911-4917 writes `endIndex - startIndex` bytes into the 21-byte row. `SetChatString` has the identical pattern at MCreature.cpp:5066-5100, gated only on `GetCreatureType() == 482 || 650`, which is itself server-supplied.

**Failure scenario:** A personal-shop message or creature chat string of the form "AAAA...(200 bytes)...\nB" reaches SetPersnalString. endIndex becomes 200, the copy loop writes 200 bytes plus a NUL into a 21-byte heap allocation, corrupting the heap.

**Recommendation:** Clamp endIndex to startIndex + MAX_CHATSTRING_LENGTH after the newline handling, and recompute `len` after the in-place '\0' write. Also validate at load time that MAX_CHATSTRINGLENGTH_PLUS1 == MAX_CHATSTRING_LENGTH + 1 — both are read independently from the config file (ClientConfig.cpp:505-507) with no consistency check.

> ✅ **Fixed** in `c3e9937` and `3bc340e` (branch `harden/network-input`) -- the same wrap-loop defect as C15; see its entry. ClientConfig now clamps the primaries to sane geometry and recomputes the derived +1/-1 values instead of trusting the record.

#### 🔴 Critical -- Server-controlled guild name of up to 255 bytes is sprintf'd into a 128-byte stack buffer, with no length cap anywhere in the parse path.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCGuildChatHandler.cpp:58`

`GCGuildChat::read()` (Client/Packet/Gpackets/GCGuildChat.cpp:27-29) reads a BYTE length `szGName` and then `iStream.read(m_SendGuildName, szGName)` with **no upper-bound check at all** — unlike `m_Sender` (capped at 10, line 38) and `m_Message` (capped at 128, line 50). The handler then does `char szName[128]; sprintf(szName, "[%s]%s", pPacket->getSendGuildName().c_str(), pPacket->getSender().c_str());`. Worst case output is 1 + 255 + 1 + 10 + 1 = 268 bytes into a 128-byte stack buffer.

**Failure scenario:** A malicious or compromised game server sends a GCGuildChat packet with m_Type != 0 and a 255-byte guild name. The handler writes 268 bytes into szName[128], smashing 140 bytes of stack including the saved return address — remote code execution in the client.

**Recommendation:** Add an upper-bound check on szGName in GCGuildChat::read() mirroring the szSender/szMessage checks, and replace the sprintf with snprintf(szName, sizeof(szName), ...).

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`) -- the same defect as C10; see its entry.

#### 🔴 Critical -- strcpy of a server message of up to 255 bytes into a static char[128]; the packet's length guard is dead code because the length field is a BYTE.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCSystemMessageHandler.cpp:161`

`static char previous[128]` (line 108) and `static char previous1[128]` (line 27) are filled with `strcpy(previous, pPacket->getMessage().c_str())` at lines 161 and 100. `GCSystemMessage::read()` (Client/Packet/Gpackets/GCSystemMessage.cpp:21-31) declares `BYTE szMessage` and guards with `if (szMessage > 256) throw` — a BYTE can never exceed 255, so this comparison is always false and the guard is dead code. The message can therefore be 255 bytes.

**Failure scenario:** Server sends a GCSystemMessage with a 255-byte body. strcpy writes 256 bytes into the 128-byte static buffer, corrupting 128 bytes of adjacent .data/.bss — including whatever globals the linker placed next to it. Repeatable at will by the server.

**Recommendation:** Change the guard to a real bound (e.g. `> 127`) or, better, cap the copy: use a std::string member or snprintf/strncpy with explicit truncation. Audit the other packet read() methods for the same dead `BYTE > 256` pattern.

> ✅ **Fixed** in `a6cc969` (branch `harden/network-input`) -- the same defect as C11; see its entry. The dead-guard audit of other read() methods has not been done.

#### 🟠 High -- The non-Windows WideCharToMultiByte shim is not a conversion, returns an unclamped length, and writes out of bounds when cbMultiByte is 0 — and its return value drives a stack write in SXml.

**Category:** portability  |  **Location:** `basic/Platform.h:729`

The shim (lines 729-747) ignores CodePage entirely and does `lpMultiByteStr[i] = (char)(lpWideCharStr[i] & 0xFF)` — Latin-1 truncation, not UTF-8 — despite being called with CP_UTF8. It returns `cchWideChar` unclamped by cbMultiByte, whereas the real Win32 API returns 0 on ERROR_INSUFFICIENT_BUFFER. Line 745 writes `lpMultiByteStr[cbMultiByte - 1]` when cchWideChar >= cbMultiByte, so cbMultiByte == 0 (the Win32 size-query idiom) writes at index -1. Client/SXml/SXml.cpp:83-97 relies on the return value: `szTemp[nCopied] = NULL;` on a `char szTemp[5120]`. Client/SXml/*.cpp is globbed into the build unconditionally at CMakeLists.txt:587.

**Failure scenario:** On Linux/macOS, an XML node longer than 5120 wide characters makes the shim return e.g. 10000; SXml.cpp:97 then writes a NUL at szTemp[10000], 4880 bytes past a 5120-byte stack buffer. Separately, every Korean or Chinese character passing through this shim is silently replaced by its low byte.

**Recommendation:** Implement a real UTF-16 to UTF-8 conversion (or route through SDL_iconv), clamp and return the bytes actually written, honour the cbMultiByte == 0 size-query contract, and have SXml.cpp check for a 0 return before indexing.

> ✅ **Fixed** on branch `harden/high-severity-batch1`. The shim now decodes through a single wide-codepoint walk that handles both 16-bit and 32-bit `wchar_t` (surrogates pair only where they actually appear, U+FFFD for unpaired or out-of-range) and encodes in two passes over that walk — measure, then write — so the returned count is exactly the bytes written. The Win32 contract holds: `cbMultiByte == 0` is a size query, a too-small destination returns 0, and `cchWideChar == -1` counts the terminator.
>
> **The finding was incomplete in one way that matters.** It presents the `SXml` write as a Linux-only consequence, but `WideCharToMultiByte` on Windows returns *up to* `cbMultiByte` bytes, so a conversion that exactly fills the 5120-byte buffer makes `szTemp[nCopied]` write at `szTemp[5120]` — one past a stack array, on a path that **is** compiled here. Holding back one byte of capacity removes it.
>
> **Duplicate and encoding follow-up (2026-09-21):** the corrected client implementation moved byte-for-byte into `VS_UI`, replacing the older duplicate, before further changes. All platforms now use the same library object and declaration. Its wide-string conversion queries the required UTF-8 size and uses owned storage, removing both the 5,120-byte ceiling and the Windows OEM-code-page conversion. Real-library tests cover null/invalid counts, the former exact-fill boundary, 50,000 characters and supplementary Unicode. The old production callers of this wide helper remain commented out, so this is a latent-path fix; the tests directly exercise it. Hosted platform jobs cover the POSIX shim as well as native Windows.

#### 🟠 High -- CToken::Release frees m_pString without nulling it, producing a double free and a dangling m_pCurrent.

**Category:** memory-safety  |  **Location:** `Client/CToken.cpp:37`

`Release()` (lines 36-41) does `delete [] m_pString` but never sets `m_pString = NULL`, and never touches `m_pCurrent`. `SetString` (line 47) calls Release() first; when `str == NULL` it takes neither branch, leaving both pointers dangling into freed memory. `~CToken` (line 21) then calls Release() again on the already-freed pointer.

**Failure scenario:** `CToken t("a b c"); t.SetString(NULL);` — the destructor double-frees m_pString. Even without the destructor, a subsequent `GetToken()` reads and writes (`*pFound = '\0'`, line 88) through m_pCurrent into freed heap memory.

**Recommendation:** Set m_pString = NULL and m_pCurrent = NULL inside Release().

> ✅ **Fixed** on branch `harden/high-severity-batch1`, plus the same defect by another route: the class owns a raw `char[]` and hands out interior pointers, so a copy would share the allocation and both destructors would free it. Copy construction and assignment are now `= delete`, which makes that unbuildable rather than latent.
>
> Reachability the finding does not state: `CToken` has three constructing uses in the tree, all locals in `UIMessageManager.cpp` (plus a `sizeof` in `SizeOfObjects.cpp`), none of which calls `SetString` twice or copies. The double free was latent, not live.

> **Reset ownership follow-up (2026-09-21):** `CToken.cpp` moved byte-identically
> into `gamemodel`, where the real implementation now has token/lifetime tests.
> ASan then reproduced a separate heap-use-after-free: `SetString(GetToken())`
> released the owned buffer before measuring/copying its borrowed token.
> Replacement allocates and copies before releasing the old string. Five tests
> cover normal parsing, repeated null/empty resets, borrowed first/interior
> tokens, suffixes, long repeated resets and the empty terminator. Current game
> callers still construct locals without resetting them; this remains a latent
> API defect, reproduced in the library test rather than a live-server report.

#### 🟠 High -- CTypeTable::operator[] bounds-checks only under _DEBUG, while m_Size is taken unvalidated from a data file and callers index by compile-time constants.

**Category:** memory-safety  |  **Location:** `Client/CTypeTable.h:49`

The bounds check in `operator[]`, the const overload, and `Get()` (lines 42-68) is wrapped in `#ifdef _DEBUG`, so release builds do a raw `return m_pTypeInfo[type];`. `LoadFromFile` (line 183) reads `numSize` as a raw int from the file and passes it straight to `Init(numSize)`, which does `m_pTypeInfo = new Type[m_Size]` (line 127) with no range validation — a negative value is undefined behaviour / std::bad_array_new_length, and a small value silently shrinks the table. Callers then index with constants up to MAX_GAME_STRING (~2019, MGameStringTable.h:2019). Client/MTopView.cpp:9646 and :9658 illustrate the trap: they guard with `< MAX_GAME_STRING` (a compile-time constant) rather than against `GetSize()` (the runtime, file-derived size).

**Failure scenario:** A truncated or older Strings file yields numSize = 800. `(*g_pGameStringTable)[1500].GetString()` reads 700 MString objects past the end of the array, returns whatever the garbage m_pString field holds, and MTopView.cpp:9647 strcpy's from that wild pointer.

**Recommendation:** Make the bounds check unconditional (return a shared empty object out of range), validate numSize in LoadFromFile against a sane maximum and against the table's expected size, and change MTopView's guard to compare against GetSize().

> ✅ **Fixed** on branch `harden/high-severity-batch1`. This is the same defect as the *build*-filed duplicate in Core Game Loop & State, and it was fixed once covering both — **this write-up is the accurate one**; the other misattributes the cause to CMake. See that entry for the full note, including the Release-side behaviour change. `MTopView`'s two guards now compare against the runtime `GetSize()` instead of the compile-time `MAX_GAME_STRING`, which also closes a missing lower bound.

#### 🟠 High -- MString::Format vsprintf's into a 1024-byte buffer shared statically by every MString in the process.

**Category:** memory-safety  |  **Location:** `Client/MString.cpp:174`

`char MString::s_pBuffer[MAX_BUFFER_LENGTH]` (MString.cpp:29, MAX_BUFFER_LENGTH == 1024 per MString.h:10) is written by `vsprintf(s_pBuffer, format, vl)` with no bound. Callers feed it data-file format strings plus server data, e.g. `msg.Format((*g_pGameStringTable)[STRING_MESSAGE_OPEN_LAIR].GetString(), g_pZoneTable->Get(ZoneID)->Name.GetString())` (Client/GameUI.cpp:4441-4447).

**Failure scenario:** Any Format() call whose expansion exceeds 1024 bytes writes past s_pBuffer into adjacent static storage. Separately, two MString::Format calls interleaved (recursion through a logging or drawing path) corrupt each other's results because the buffer is process-global.

**Recommendation:** Use vsnprintf with sizeof(s_pBuffer), and make the scratch buffer function-local.

> ✅ **Fixed** in `0e9d247` (branch `harden/text-format`). The scratch buffer is function-local and written with vsnprintf, so the cross-call corruption goes with the overflow. `s_pBuffer` had no other reference in the tree, so the static member is deleted outright; being a static, it occupied no object storage and no layout changed.

#### 🟠 High -- ConvertEncoding treats SDL_ICONV_EILSEQ/EINVAL/E2BIG as success, so partial and mis-guessed conversions are accepted as valid text.

> ✅ **Encoding contract completed (2026-09-23):** the renderer now accepts UTF-8
> and repairs malformed bytes with U+FFFD, preserving valid text on both sides.
> It no longer tries CP949/EUC-KR/Chinese pages. Legacy resources decode at their
> selected or declared boundary. The [ingress audit](text-ingress-audit-2026-09-23.md)
> traces resources, XML, source literals, editor input and paired-server strings;
> it also records the production ASan parser results for installed XML and the
> malformed numerical ghost-position asset. Two test-first renderer cases
> reproduced 132 failed checks. The current tests cover replacement, retained
> suffixes, scalar errors, embedded NULs, idempotence and explicit resource
> decoding, with all 11 CTests passing in Debug, ASan and Release. The older
> fallback discussion below describes the superseded implementation.

**Category:** correctness  |  **Location:** `Client/TextSystem/TextService.cpp:77`

`SDL_iconv` returns SDL_ICONV_ERROR (-1), SDL_ICONV_E2BIG (-2), SDL_ICONV_EILSEQ (-3) or SDL_ICONV_EINVAL (-4), but line 77 only tests `res == static_cast<size_t>(-1)`. An invalid byte sequence (-3) or a truncated tail (-4) therefore falls through to line 80-81 and returns the *partially* converted prefix. `NormalizeText` (line 100-104) accepts the first non-empty result, and its candidate order is `{CP949, EUC-KR, GBK, GB2312, BIG5}` — Korean first. Because GBK and CP949 share the same double-byte lead-byte range, GBK resource text almost always decodes "successfully" as CP949.

**Failure scenario:** The client's resource strings are documented as GBK (MString.cpp:230). A GBK string is fed to NormalizeText, CP949 partially decodes it, EILSEQ is ignored, and the user sees truncated Korean mojibake instead of the intended Chinese text. This is consistent with the mojibake already frozen into MGameStringTable.cpp (e.g. line 44, line 106).

**Recommendation:** Reject any res other than a non-negative count (or at minimum reject EILSEQ/EINVAL), require that all input bytes were consumed, and stop guessing: record the resource file's declared encoding in the file/config and convert once at load rather than re-guessing per draw call.

> ⚠️ **Already fixed** in `43458c1`, before this batch looked at it — no code change was made or needed. `ConvertEncoding` reads `if (res == (size_t)-1 || inBytes != 0)`, and the `inBytes != 0` half is exactly the "require that all input bytes were consumed" clause. The finding's stated mechanism is obsolete in a second way too: that commit moved the function off `SDL_iconv` onto real `iconv`, so `SDL_ICONV_E2BIG`/`EILSEQ`/`EINVAL` are no longer the return values at all — POSIX `iconv` reports all three as `-1` with `errno` and a non-zero `inbytesleft`.
>
> `tests/unit/test_text_service.cpp` was added anyway (4 tests, 7 checks), because the two halves of that condition look redundant side by side and the obvious tidy-up of dropping one silently reintroduces truncation. It covers a truncated tail, an unmappable byte mid-string, a positive control so the rejections cannot pass vacuously, and the fallback contract.
>
> That last test is the important one. Tightening this condition looks like it risks blanking text on screen; it does not, and the test pins down why — when no candidate accepts the bytes, `NormalizeText` returns *the input*, not an empty string, and the renderer draws unrecognised bytes as U+FFFD. So the worst case is replacement glyphs, which is already the behaviour for unrecognised text. "Return empty on failure" is the tempting simplification that would actually erase a line.
>
> **Resource loading fixed; direct renderer inputs remain open (2026-09-21):** `MString` now decodes once at load using `RESOURCE_TEXT_ENCODING` from `FileDef.inf`, with CP949 as the shipped-pack default and explicit UTF-8/EUC-KR/GBK/GB2312/BIG5 alternatives. Saves encode back to that declared page and reject unrepresentable text or an encoded record above the reader's 65,536-byte limit before writing a prefix. Damaged loaded bytes become U+FFFD while later text and record alignment survive. The shared `basic/TextEncoding` converter checks expansion before reading, owns iconv through RAII, refuses partial/irreversible strict conversions and is also used by the renderer. Non-ASCII file round trips, ambiguous legacy bytes, embedded NULs, invalid scalars and capacity guards are covered by `test_mstring_files.cpp` and `test_text_encoding.cpp`; the first file tests reproduced seven failed checks. The old GBK comment was not evidence of the shipped pack's encoding: known NPC records are CP949. Legacy text fed directly to the renderer still uses its existing candidate policy; those ingress paths must migrate before that fallback is removed. XML resources now also decode their declared page once before parsing, with UTF-8 BOM/declaration precedence over the pack default. Consolidating the XML implementation exposed a 2,100-byte attribute write into a 1,024-byte stack buffer under ASan; the parser now uses bounded views and owned strings, rejects incomplete/mismatched documents before publishing, and preserves appended quest documents and parent pointers. Tests cover large non-ASCII attributes/text, every truncated prefix of a document, markup/character references, repeated quest names, depth/node limits and escaped UTF-8 save/load round trips.

> **Plain-text ingress follow-up (2026-09-21):** common and race-specific chat-tip files and welcome/event popup files now use `CRarFile::OpenText`, so they reach UI storage as UTF-8 under the configured page/BOM policy. The popups test the open result instead of checking `IsSet` before opening anything, which previously kept these file-backed messages empty. Their executable-coupled UI paths are regression guards verified by full builds and automated checks; `test_resource_reader.cpp` exercises the actual shared decoder, line clipping and failed-open contracts. Description/help/mail/command loaders and renderer fallback retirement remain open.

> **Help-message loader follow-up (2026-09-21):** the production implementation moved unchanged into `VS_UI`, then a real-library stream test reproduced a 100-byte over-read on a valid short level row under ASan. One bounded parser now serves stream and extracted-resource inputs, validates six level fields and three race pages, and publishes complete collections only. Missing or malformed reloads preserve existing messages. Resource bytes decode once under the pack policy/BOM; saves write UTF-8 with a BOM and retain the distinct attribute maximum instead of writing the minimum twice. Default singleton loading opens the resource before reading and never calls back into `Instance` during initialization. Unused duplicate counters/mutators and undefined message-file APIs were removed; lookup indices use checked access. Tests cover truncation, invalid counts/rows, long CP949 fields, embedded NULs, reloads, constructor loading and save/load bounds/text round trips. Description, mail-template and command-file ingress still need migration.


> **Mail/command ingress follow-up (2026-09-21):** notice mail templates now open through `OpenText` before reading, then validate four complete owned rows and dimensions bounded by the actual viewport. The fresh reader's old `IsSet` precheck had prevented loading; fixed 4096-byte field copies and unchecked dimensions are removed. Popup rendering consumes only rows that fit above its buttons. Command files now use scoped file handles and whole-file resource decoding, with strict conversion failure instead of replacement for bytes that would become actions. A complete ordered command plan is validated before dispatch: cycles, missing files, excessive depth/visited rows/bytes and commands above the existing 128-byte packet allowance reject the whole plan. Repeated nonrecursive includes retain order, while local `*mc N` selectors are consumed rather than forwarded. Four mail tests and six command tests cover complete rows, geometry, ownership, failure publication, nested/repeated files, limits, declared pages/BOMs, malformed bytes and real file handles. The first strict-command assertion reproduced two failures in the initial new helper and was then corrected. These UI integrations remain regression guards; no game commands were sent during tests. Remaining ingress auditing and renderer fallback retirement are still open.

#### 🟠 High -- ReduceString2 writes at pStr[maxWidth-3..maxWidth] without ever verifying the string is that long — a regression introduced when the function was ported.

**Category:** memory-safety  |  **Location:** `Client/UIUtilityFunctions.cpp:58`

The counting loop (lines 41-54) stops at `len < maxWidth`, so `len` is the *actual* string length when the string is shorter than maxWidth. The condition at line 56 is then `if (len > maxWidth - 3)`, which is true for any string whose length lies in (maxWidth-3, maxWidth] — i.e. strings shorter than maxWidth — and the body writes pStr[maxWidth-3], [maxWidth-2], [maxWidth-1] and pStr[maxWidth]. The superseded implementation in VS_UI/src/hangul/FL2.cpp:425 guarded the whole body with `if (lt > len)`; that guard was dropped in the port. `ReduceString` (line 15) has a related hole: it only rejects `maxWidth <= 0`, so maxWidth of 1 or 2 writes at pStr[-2]/pStr[-1].

**Failure scenario:** `ReduceString2(szString, 38)` (VS_UI/src/vs_ui_gamecommon2.cpp:15455) on a 36-byte title held in a buffer sized exactly 37 bytes writes at indices 35..38 — two bytes past the end of the allocation.

**Recommendation:** Restore the `strlen(pStr) > maxWidth` precondition in ReduceString2 (as ReduceString and ReduceString3 have), and take an explicit buffer-capacity parameter rather than inferring it from maxWidth.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, with **three corrections to the finding**.
>
> 1. **Its failure scenario is not reachable.** At every call site in the tree the destination is far larger than `maxWidth` (`szString[64]`/`[256]`/`[512]` with 32/36/38, `sz_temp[200]` with 55, `name[300]` with 38), so `pStr[maxWidth]` lands inside the buffer everywhere. The "36-byte title in a 37-byte buffer" corresponds to no actual caller. The out-of-bounds *write* is a latent trap.
> 2. **It missed the defect that does fire today.** The counting loop advanced `p += 2` on any high-bit byte, so a string ending on an unpaired DBCS lead byte steps over the terminator and keeps reading past the buffer — and the callers fill those buffers with `sprintf("%s", ...)`, which cuts multi-byte text wherever the buffer happens to end, which is precisely how an unpaired lead byte gets there. That is a live out-of-bounds *read*, and `strlen` replaces the loop.
> 3. **`ReduceString3` is not a defect** — its `len <= maxWidth` early return and `cutLen > 0` test keep every index at or before the terminator. Unchanged.
>
> `ReduceString` and `ReduceString2` also gained a `maxWidth < 3` branch that cuts without the marker instead of indexing at `pStr[-2]`. The capacity parameter was **not** added: it would mean changing `Fl2.h` and every call site, several outside that change's scope. So the guarantee is narrower than the recommendation asks for and is stated in a banner comment — these functions take a `char*` with no capacity and can only promise that every byte they touch lies at or before the existing terminator.
>
> **On-screen change:** `VS_UI_ExtraDialog.cpp:1513` calls `ReduceString2(sz_temp, 55)` with no length guard, so item-description titles of 53–55 bytes are today rendered as `<52 chars>...` with the ellipsis stamped past the string; they will now render in full, which is correct for a 55-wide field. The three call sites in `vs_ui_gamecommon2.cpp` all guard with `strlen > N` first and are unaffected.

> **UTF-8 follow-up (2026-09-21):** all three reducers now use a shared scalar-
> boundary prefix operation and one byte-budget contract. `ReduceString3` also
> honors positive limits of one to three bytes; its older no-op behavior was
> storage-safe but did not satisfy truncation. Existing exact-allocation and
> guard-byte tests still pin the storage guarantee, and new two-, three- and
> four-byte character fixtures reproduce and prevent malformed UTF-8 output.

#### 🟠 High -- Quest titles and nicknames are sprintf'd into 64-byte stack buffers with no length limit; the length check is applied only afterwards.

**Category:** memory-safety  |  **Location:** `VS_UI/src/vs_ui_gamecommon2.cpp:15453`

`char szString[64];` (line 15452) followed by `sprintf(szString, "%s", m_szTitle.c_str());` (line 15453) — the `if (strlen(szString) > 40) ReduceString2(...)` on the next line runs after the overflow has already occurred. `m_szTitle` is a std::string assigned from the quest description path at line 15727 with no length bound. The same pattern appears at line 13224/13253 (`char szString[64]` then `wsprintf(szString, "%s", (TempInfo->getNickname()).c_str())`) and at line 15006 with a 256-byte buffer.

**Failure scenario:** A quest title or player nickname longer than 63 bytes — trivially produced by multi-byte CJK text, where 21 characters already exceed the buffer — overflows the stack frame of the draw routine before any truncation happens.

**Recommendation:** Use snprintf with sizeof(szString), or copy into a std::string and truncate before formatting.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, and the sweep found more than the three sites the review sampled. The rule applied across `vs_ui_gamecommon2.cpp`: every `sprintf`/`wsprintf`/`strcpy` whose destination is a fixed-size array and whose source is a runtime-variable-length string becomes `snprintf(dst, sizeof(dst), ...)` — thirteen sites in all.
>
> **Three further defects turned up during that sweep and are fixed with it:**
> - An *entirely unchecked* `BYTE` index into a 7-element `const char*` array of quest-status labels, followed by `sprintf("%s", ...)` through whatever pointer that yielded. The sibling loop in the mission list does bound its index; this one never did.
> - A `memcpy` into a 20480-byte help-text buffer using an offset derived from the text itself, now clamped.
> - A `strlen(TempNum) > 0` test guarding a `TempNum+3` split and a `TempNum[2]` read that need three characters, now `>= 3` — matching the sibling that `12bde38` had already hardened for exactly this.
>
> **Deliberately left in this file:** the `"%d %s"` / `"%s +%d"` family fed by game-string-table entries and the `strcat` chains beside them. Those are the C19/C20/C22 data-file-format class, and bounding the `sprintf` without also bounding the adjacent `strcat(sz_buf, "%")` would leave a one-byte overflow reachable exactly on truncation — a fix that makes things worse. Also left: `char pPartName[20]` filled by `strcpy` from a data table whose index is itself unchecked, same class.
>
> ✅ **Option-text residual fixed (2026-09-22):** the seven live 20-byte name copies use checked, owned names; direct pet and option-list name lookups use the same API. The table arrays are private, absent/invalid names yield an empty string, and race-specific MP/HP/EP substitution preserves the first-match behavior. Literal percentage formats and data-file-format suffix appends are bounded through their final write. Table loading validates complete counts, rows and part IDs before replacing existing data, and clears old names on smaller valid reloads. `test_gamemodel_tables.cpp` covers every truncated prefix, invalid counts/parts, later-row failure, following records and long UTF-8 names; initial tests reproduced 165 failed checks. UI integrations are regression guards.
>
> The `VS_UI_ExtraDialog.cpp` two-string message now uses the bounded formatter (2026-09-21). The separate title `strcpy` into `char[300]` was already removed by `7fccb4f3` (2026-09-18): the file dialog renders `GetGameString` directly, and its adjacent path labels use owned strings guarded by `test_file_dialog_labels.cpp`.

#### 🟠 High -- utf8_to_utf32 dereferences continuation bytes without checking the NUL terminator, reading past the end of the input buffer.

**Category:** memory-safety  |  **Location:** `VS_UI/src/widget/U_edit.cpp:44`

The decoder tests only the lead byte and then unconditionally consumes 1-3 more bytes with `*s++` (lines 44, 46-48, 50-53). For a truncated sequence the loop walks straight over the terminating NUL and keeps going — the outer `while (*s && n < cap)` then continues reading whatever follows the buffer in memory until it happens to hit a zero byte. Reached from `HandleTextInput` (line 195, SDL_TEXTINPUT), `HandleTextEditing` (line 209, SDL_TEXTEDITING / IME composition), and `AddString` (line 260), which is called with arbitrary strings via `LineEditorVisual::AddString`.

**Failure scenario:** An SDL_TEXTEDITING event whose UTF-8 payload ends mid-sequence (e.g. a lone 0xE0 as the final byte, which IMEs can emit when a composition string is chunked) causes the decoder to read past the SDL-owned buffer, producing garbage codepoints and potentially faulting on an unmapped page.

**Recommendation:** Check for the NUL terminator before consuming each continuation byte and bail out of the sequence (emit U+FFFD) if the string ends early; also validate that continuation bytes have the 0b10xxxxxx prefix.

> ✅ **Fixed** on branch `harden/high-severity-batch1`. Every continuation byte is tested for the `0b10xxxxxx` prefix before it is read, so a NUL ends the sequence instead of being consumed, and `s` itself is NULL-guarded because `HandleTextEditing` passes SDL's pointer straight through. Decoding of well-formed input is byte-identical.
>
> **Reachability, corrected:** all three entry points are live, but not by the route the finding's line numbers describe. `2a531a9` (finding C23) deleted only the `WM_TEXTINPUT` plumbing that carried a pointer through a `long`; the SDL path is untouched, reaching `HandleTextInput`/`HandleTextEditing` from `DXLibBackendSDL.cpp`.
>
> **One deliberate divergence:** malformed input is dropped rather than emitting U+FFFD, which is what the original did for an unusable lead byte. This decoder fills an *edit buffer* whose contents are echoed to the user and sent to the server, and a replacement character silently typed into someone's chat line is worse than a byte that never arrives. `TextService::Utf8Decode`, which is a *rendering* decoder, correctly does emit U+FFFD — the two want opposite answers.

#### 🟡 Medium -- The wsprintf shim turns a length-capped Win32 API into an unbounded vsprintf at 646 call sites.

> ✅ **Fixed (2026-09-21):** delete the POSIX shim and replace the remaining 355 narrow calls with `SafeFormat::Format`, whose array overload derives the actual destination capacity. The one pointer slice supplies its remaining capacity explicitly. The wide trace command uses capacity-bounded `swprintf` and explicit termination; unused CMP3 string-formatting overloads are removed. The caller audit removes 21 `void*` casts that would hide string arguments from the typed formatter. Existing `SafeFormat` bounds/type tests own the formatter; UI calls are regression guards under the named exemption, with full builds and source checks. R18 rejects raw identifiers, including aliases, across C/C++ sources and headers.

**Category:** portability  |  **Location:** `basic/Platform.h:1864`

On non-Windows, `wsprintf` is defined as a static inline that calls `vsprintf(buf, fmt, args)` with no bound. The Win32 wsprintfA it replaces caps its output at 1024 bytes; the shim removes that cap entirely. There are 646 `wsprintf(` call sites across Client/ and VS_UI/, many writing into buffers of 20-64 bytes (e.g. VS_UI/src/VS_UI_Description.cpp:271-310 into `char sz_buf[50]`, VS_UI/src/vs_ui_gamecommon2.cpp:13253 into `char szString[64]`). Code originally written under the assumption of a 1024-byte ceiling now has none.

**Failure scenario:** Any of the ~646 call sites whose expansion exceeds its destination now overflows on Linux/macOS where it merely truncated on Windows — a class of latent bug that the port silently unmasked all at once.

**Recommendation:** Do not provide a wsprintf shim. Mechanically rewrite the call sites to snprintf with sizeof(dest); if a transitional shim is unavoidable, make it a macro that resolves the destination size via sizeof and calls snprintf.

#### 🟡 Medium -- CMessageArray::Release leaves m_Filename dangling, leaks it when the log file fails to open, and Add/operator[]/Clear have no initialization guards.

**Category:** memory-safety  |  **Location:** `Client/CMessageArray.cpp:158`

> ✅ **Fixed (2026-09-18):** move `CMessageArray` into `basic` before fixing it. Reads reject invalid indices and absent storage; add/format/clear/advance operations tolerate an empty or released ring. Invalid dimensions leave an empty object, and allocation failure cleans temporary owners while preserving the old ring. Reinitialization safely accepts its own filename. Filename release was already corrected; tests now cover failed/successful opens, repeated release, reinitialization and bounded writes through every formatting path. Mutable access no longer lets callers replace the owned row pointer, and copying the owner is disabled.

Release() deletes m_Filename (line 160) only when m_bLog is true and never nulls it, so GetFilename() (CMessageArray.h:50) and the OUTPUT_FILE_LOG reopen path (line 195) use a freed pointer. Conversely, if PLATFORM_OPEN fails in Init (line 114-119), m_bLog stays false and the m_Filename allocation from line 111 is leaked and then overwritten on the next Init. Separately, Add() (line 203), Clear() (line 450) and operator[] (line 429) dereference m_ppMessage with no NULL check, and operator[] computes `BYTE gap = m_Max - i` (line 424) — for i > m_Max the BYTE wraps to ~255 and line 429 indexes with a negative offset.

**Failure scenario:** Init() is called with a log filename on a read-only directory: _open fails, m_bLog stays false, and every subsequent Init leaks the filename buffer. If the open succeeds, the second Release() (destructor after an explicit Release, or Init-then-destructor) reads m_Filename after free.

**Recommendation:** Null m_Filename after delete, free it unconditionally rather than only under m_bLog, and add `if (m_ppMessage == NULL) return;` plus an explicit `i < 0 || i >= m_Max` range check to Add/Clear/operator[].

#### 🟡 Medium -- 1630 lines of string-table literals are unreachable — their only caller is excluded from every build configuration.

> ✅ **Verified resolved (2026-09-18)**: `GameInit.cpp` calls `InitGameStringTable()` on the live English-language path. `MGameStringTable.cpp` is compiled into `gamemodel`; the language-selection and English-counter tests in `test_gamemodel_tables.cpp` exercise the real table. The dead-only-caller claim describes the old build.

**Category:** dead-code  |  **Location:** `Client/MGameStringTable.cpp:18`

`InitGameStringTable()` is defined here and called from exactly one place: VS_UI/WinMain.cpp:3229. CMakeLists.txt excludes VS_UI/WinMain.cpp on non-Windows (line 179, `EXCLUDE REGEX ".*WinMain.*"`) and on Windows (line 209, `EXCLUDE REGEX "VS_UI/WinMain\\.cpp"`), so it is in no build. The table is therefore populated exclusively by the file load at Client/GameInit.cpp:1604. The file is also the tree's worst encoding casualty: it mixes correctly-encoded Chinese with irreversibly mojibake'd CP949-read-as-GBK text in the same statements (compare line 102 "生命力(HP)最大值变成了%d。" with line 106 "제좆(STR)긴냥죄%d。" and line 44 "꼇콘뗬陵。").

**Failure scenario:** A contributor spends an afternoon fixing the mojibake in MGameStringTable.cpp, rebuilds, and nothing changes — the strings all come from the binary data file. The next contributor repeats the exercise.

**Recommendation:** Either delete the file and document that the string table lives in the data file, or wire InitGameStringTable() in as a fallback that runs before LoadFromFile so the literals are a real default. Do not leave it in the ambiguous state.

#### 🟡 Medium -- MString::LoadFromFile never checks that the read succeeded, and serializes a size_t through a 4-byte field.

**Category:** correctness  |  **Location:** `Client/MString.cpp:226`

> ✅ **Fixed (2026-09-17):** binary I/O uses an explicit four-byte little-endian `uint32_t` length. The reader rejects over-64-KiB lengths with stream failure, checks the entire payload before encoding conversion, and retains the previous string on read/allocation/conversion failure; RAII frees temporary buffers even when streams throw. The writer refuses lengths outside the file format before emitting bytes. Five tests cover all truncation boundaries, hostile lengths, empty/sequential records, a 64-KiB byte-exact round trip and throwing streams; three reproduced the old failures.

Line 226 does `file.read((char*)pTemp, m_Length)` with no subsequent `file.gcount()` or stream-state check, then line 227 NUL-terminates at pTemp[m_Length] and passes the buffer on as a string. On a truncated file the tail of pTemp is uninitialized heap. Separately, `m_Length` is declared `size_t` (MString.h:66) but SaveToFile writes only 4 bytes of it (line 186) and LoadFromFile reads only 4 bytes into it (line 207) — on 64-bit the upper half of the member is left at whatever it previously held, and the on-disk format is implicitly little-endian.

**Failure scenario:** A Strings file truncated by an interrupted patch download yields MString objects whose content is uninitialized heap memory, which is then rendered on screen and, per the format-string finding, used as a printf format.

**Recommendation:** Check file.gcount() == m_Length after the read and fall back to the empty string on short reads; serialize an explicit uint32_t local rather than reading into the size_t member.

#### 🟡 Medium -- g_PossibleStringCut was reimplemented with a different meaning for its second parameter while every caller kept passing the old one.

> ✅ **Resolved by caller migration and API retirement (2026-09-21):** all compiled callers now use owned UTF-8 rows or bounded scalar prefixes through `TextWrap`, `DescriptorText`, `HelpLayout` and the string reducers. The mismatched pixel-width implementation and shared declaration are removed, so compiled code cannot accidentally call the old API. Existing library tests own scalar boundaries, row consumption, layout and capacity contracts; executable UI wiring remains a regression guard. Remaining textual mentions are in comments or the native GDI `FL2.cpp` excluded by the forced SDL build on every supported platform. Resource ingress and renderer encoding policy remain separate work under the encoding finding.

> **UTF-8 prefix foundation (2026-09-21):** `TextSystem::Utf8PrefixBytes` in
> `basic/TextUtf8.h` now returns a bounded prefix using the shared scalar
> decoder. The three `ReduceString` variants use it and reserve space for their
> ellipsis; limits below three bytes clip without splitting a character.
> Two real-library tests reproduced 54 failed output/validity checks in the old
> reducers, including `ReduceString3` ignoring limits of one to three bytes.
> Decoded `CRarFile` line reads share the prefix helper. The pixel-width predicate
> and its one-byte-backoff callers still need migration, so this finding remains
> open; the helper does not guess encodings or repair malformed input.

> **Rich-help layout follow-up (2026-09-21):** `basic/HelpLayout` now owns complete UTF-8 text runs, colors, image placements and displayed row counts. Eleven library tests cover long/blank lines, scalar progress, append mode, partial/full-row images, invalid attributes and bounded atomic output. The help window uses those rows for drawing and scrolling; it no longer reparses wrapped text as markup or copies through fixed scratch buffers. Images load through an independently tested owning `HelpImageCache`, with correct RGB565 channels, missing/corrupt-file rejection, duplicate reuse and dimension/pixel/entry limits. Five cache tests include a real JPEG; they exposed that the Windows SDL_image package needed its JPEG feature, now required by the manifest. Image clipping preserves the existing destination clip, and centered images use the same row origin as text. The uncalled substitution API and its dead copy are gone (R17: 570 to 569). These game-global integrations are regression guards. The final compiled cut-predicate caller is removed; definition/declaration retirement and the remaining resource-ingress work are still open.

**Category:** correctness  |  **Location:** `Client/RenderingFunctions.cpp:313`

The original in VS_UI/src/hangul/FL2.cpp:38 walked the string counting DBCS lead bytes and answered "can I cut at character *position*". The replacement is `int width = g_GetStringWidth(pStr, NULL); return width <= maxWidth;` — it measures the pixel width of the *entire* string and compares it to the argument. Callers were not updated: Client/MCreature.cpp:4905 and :5091 pass `endIndex`, a byte index into the chat string; VS_UI/src/VS_UI_DESC.cpp:148,164,402,665 and VS_UI/src/VS_UI_GameCommon.cpp:4990 likewise pass character offsets. The two functions have identical signatures, so nothing warns.

**Failure scenario:** MCreature.cpp:4905 asks whether it may cut at byte 20 of a chat line. The new implementation measures the whole line (say 160 pixels), compares 160 <= 20, returns false, and the caller does `endIndex--` unconditionally — so chat wrapping now cuts one byte earlier on every line regardless of multi-byte boundaries, splitting UTF-8 sequences. It also runs a full MeasureText (including NormalizeText and per-glyph metrics) inside a per-line wrap loop.

**Recommendation:** Restore the original semantics under the original name (a UTF-8-aware "is index a character boundary" predicate), and give the pixel-width variant a distinct name.

#### 🟡 Medium -- The glyph cache is unbounded and keyed on a server-controllable colour, so text rendering has no memory ceiling.

**Category:** resource-exhaustion  |  **Location:** `Client/TextSystem/TextBackendSDL.cpp:285`

> ✅ **Fixed (2026-09-18):** the SDL glyph cache is bounded by 4,096 entries and 8 MiB of retained sprite pixels by default, with least-recently-used eviction and owning sprite handles. A glyph larger than the configured pixel budget is refused. Opacity is applied during blitting; cached sprites retain full coverage so alpha does not contaminate later draws. Five tests exercise color churn, byte and entry limits, hot-entry reuse, recreated pixels/metrics, zero budgets and actual transparent/faded/opaque draws. The first three tests reproduced the original defects. The limits cover this glyph cache, not SDL_ttf's internal font allocations.

`m_glyphs` (line 343) is an unordered_map keyed by {fontId, codepoint, packed RGB} (lines 207-214) with no eviction anywhere — entries are only destroyed in the destructor (lines 46-53). Each entry owns a full RGBA sprite created by `spritectl_create_sprite` (line 273). The colour component comes from `TextStyle::color`, which for chat text originates in `pPacket->getColor()` — see Client/Packet/Gpackets/GCGuildChatHandler.cpp:59 and GCSayHandler.cpp:225 passing the packet colour into UI_AddChatToHistory. Additionally the cache key packs only R/G/B while the render at line 234 uses `color.a` too, so a glyph first cached at one alpha is reused at every other alpha; and `DrawGlyph` (line 289-300) discards its `alpha` argument outright (`(void)alpha;` then passes a hardcoded 255), so text alpha/fade is simply not implemented.

**Failure scenario:** A hostile server sends chat messages cycling the 24-bit colour field with CJK glyphs. Every (glyph, colour) pair allocates a new never-freed RGBA sprite; a few thousand messages exhaust memory. Benignly, any UI that fades text out renders it fully opaque instead.

**Recommendation:** Bound the cache (LRU with a byte budget), or render glyphs white and modulate the colour at blit time so the key collapses to (fontId, codepoint). Include alpha in the key or implement the alpha parameter in DrawGlyph.

#### 🟡 Medium -- NormalizeText runs an encoding sniff (and for non-UTF-8 text a full iconv open/convert/close plus a 4x allocation) on every draw and every measure, every frame.

> ✅ **Fixed repeated work and resource load-time conversion (2026-09-21):** a bounded normalization cache shares results across real measurement, wrapping and drawing calls, including rejected inputs. It owns complete input keys (embedded NULs included), retains at most 1,024 entries and 1 MiB of string capacity per thread, and bypasses oversized inputs. A separate 4,096-entry glyph-metric cache shares successful and failed measurements across colors; a bitmap rendered first supplies fallback dimensions without another white-surface render. LRU tests cover both budgets, key ownership, font separation, eviction and disabled caches. Counters reproduce the repeated work before the fix and prove reuse afterward. The resource-codec follow-up now converts `MString` records once at load, using their declared page. The remaining direct-input renderer fallback belongs to the still-open encoding finding above.

**Category:** performance  |  **Location:** `Client/TextSystem/TextService.cpp:370`

`DrawLine` (line 370) and `WrapText` (line 285, reached from MeasureText at line 267) both call `NormalizeText` on their input. For text that is not valid UTF-8 — which is exactly the case for resource strings loaded from the GBK data file, since ConvertGBKToUTF8 is a no-op copy outside PLATFORM_MACOS (MString.cpp:334-341) — this performs an SDL_iconv_open/SDL_iconv/SDL_iconv_close plus a `std::string` of `input.size()*4+4` bytes, per string, per call. `g_PrintColorStrShadow` (RenderingFunctions.cpp:212-232) calls g_Print twice plus g_GetStringWidth, so a single shadowed label costs three normalizations per frame. Separately, `GetGlyphMetrics` (TextBackendSDL.cpp:175-188) falls back to rendering a whole SDL_Surface and immediately freeing it whenever TTF_GlyphMetrics fails, with no caching.

**Failure scenario:** A UI screen drawing a few hundred labels from the resource table performs several hundred iconv_open/close cycles and heap allocations per frame, on the render thread.

**Recommendation:** Normalize once at load time (in MString::LoadFromFile) rather than at draw time, and cache the normalized form. Cache the fallback glyph metrics alongside the glyph in m_glyphs.

#### ⚪ Low -- POSIX open() is called with O_CREAT but no mode argument, so the log file's permissions come from stack garbage.

**Category:** portability  |  **Location:** `Client/CMessageArray.cpp:114`

> ✅ **Already fixed; verified 2026-09-18:** every create/reopen passes `LogFileMode` (`_S_IREAD | _S_IWRITE` on Windows, `S_IRUSR | S_IWUSR` on POSIX). The message-ring extraction preserves those modes, and its POSIX regression test rejects group/other and set-ID permission bits on a newly created log. Reopen failure also disables subsequent log writes.

`PLATFORM_OPEN` maps to `open` on non-Windows (line 31) and the call passes only two arguments: `m_LogFile = PLATFORM_OPEN(filename, _O_WRONLY | _O_TEXT | _O_CREAT | _O_TRUNC);`. open() with O_CREAT requires a third `mode_t` argument; omitting it is undefined behaviour and in practice takes whatever happens to be in the register/stack slot. The same defect appears at lines 195 and 248 in the OUTPUT_FILE_LOG reopen paths. The Windows `_open` mapping (line 26) has the same requirement for _O_CREAT.

**Failure scenario:** On Linux the debug log file is created with an arbitrary permission bits value — potentially world-writable, or with the setuid bit — depending on stack contents at the call.

**Recommendation:** Pass an explicit mode (e.g. 0644) as the third argument at all three call sites.

#### ⚪ Low -- TextBackendSDL.cpp is the only non-UTF-8 source file in the repository, containing a block of CP949 comment bytes.

**Category:** correctness  |  **Location:** `Client/TextSystem/TextBackendSDL.cpp:98`

> ✅ **Already fixed; verified 2026-09-18:** the current file decodes strictly as UTF-8 and contains only ASCII. Its Windows font fallback comments are English. No source conversion remains necessary for this finding.

`file -b` reports "ISO-8859 text" for this file alone out of every .c/.cpp/.h tracked by git; all 1900+ other sources are ASCII or UTF-8. The offending region is the comment block at lines 98-108, which is CP949-encoded Korean explaining the Windows font fallback list. Under MSVC without /utf-8 this produces C4819 warnings and is interpreted per the active code page, and any CP949 trail byte equal to 0x5C at end-of-line would splice the following line into the comment.

**Failure scenario:** A contributor on a non-Korean Windows locale opens the file, their editor re-saves it in yet another encoding, and the comment degrades further; MSVC emits C4819 on every build.

**Recommendation:** Re-encode the file as UTF-8 and translate the comment to English, per the project's stated English-only policy in CLAUDE.md.

#### ⚪ Low -- The UTF-8 validator accepts overlong 3-byte forms and UTF-16 surrogates, and the decoder never validates continuation bytes.

**Category:** correctness  |  **Location:** `Client/TextSystem/TextService.cpp:36`

> ✅ **Fixed (2026-09-18):** validation and rendering now share the scalar decoder in `TextUtf8.h`. It rejects bad continuation bytes, overlong forms, surrogates and values above U+10FFFF; malformed input consumes one byte as U+FFFD so following ASCII survives. Three tests exercise scalar boundaries, exact-length truncated allocations and real text measurement. Two tests failed against the original functions before the fix. Legacy encoding selection is still tracked separately in finding 108.

`IsValidUtf8` (lines 21-55) rejects overlong 2-byte forms (`c < 0xC2`, line 33) and out-of-range 4-byte leads (`c > 0xF4`, line 39), but applies no equivalent check for 3-byte sequences — it accepts 0xE0 0x80 0x80 (overlong NUL) and 0xED 0xA0 0x80 (a lone surrogate). `Utf8Decode` (lines 109-162) checks only the remaining byte count, never that continuation bytes carry the 0b10xxxxxx prefix, so invalid input silently yields wrong codepoints rather than U+FFFD. These codepoints are then handed to TTF_RenderUTF8_Blended via EncodeUtf8 (TextBackendSDL.cpp:310).

**Failure scenario:** Malformed resource or chat text containing a surrogate half passes validation, is re-encoded by EncodeUtf8 into the same invalid sequence, and is passed to SDL_ttf — which may return NULL or render a replacement box, and pollutes the glyph cache with a permanent entry for the bogus codepoint.

**Recommendation:** Reject overlong 3-byte forms (lead 0xE0 requires a continuation >= 0xA0) and the surrogate range U+D800-DFFF, and validate continuation-byte prefixes in Utf8Decode.

---

## UI Framework

**Grade:** D  |  **Findings:** 25

**Scope:** UI — VS_UI/ (widgets, skin manager, input editors, hangul IME, Imm/) plus Client/GameUI* and the VS_UI dialog/window classes (shop, storage, exchange, skill tree, descriptor/tooltip system)

**Health assessment:** This is the least safe area of the client I looked at. The widget layer is a 2000-era raw-pointer framework (SimpleDataList of `Window*`/`Button*`, void*-typed tooltip payloads, fixed stack buffers everywhere) that has been partially re-plumbed for SDL2 without the invariants being re-established. Three classes of defect dominate: (1) 64-bit pointer truncation — object pointers are still smuggled through `long`/`int` parameters (`(long)text`, `(int)(intptr_t)pAddedItem`), which was harmless on VC6/x86 and is a guaranteed wild-pointer dereference on the VS2022 x64 build the repo just started targeting; (2) unbounded string handling on server- and file-supplied data, including a genuine format-string vulnerability where a server-provided quest title becomes the format argument of `sprintf`; (3) uninitialized members read in constructors and default constructors (`m_image_index`, `m_button_width`, `Button::m_pC_exec_handler`), one of which silently mis-assigns every button's sprite/menu index in the game. On top of that, the SDL port replaced `g_PossibleStringCut` and `ReduceString*` with functions that have different semantics from the originals their ~40 call sites were written against, and `LineEditorVisual::SetAbsWidth()` writes a member nothing reads. There is real structural value here (the Window/ButtonGroup/Descriptor separation is coherent), but the memory-safety baseline is well below what a maintained codebase needs, and ASan on the chat/tooltip/file-dialog paths would light up immediately.

#### 🔴 Critical -- The SDL text-input event pointer is passed through a `long`, truncating it on 64-bit Windows and crashing on every keystroke into a text field.

**Category:** memory-safety  |  **Location:** `Client/CWaitUIUpdate.cpp:188`

`gC_vs_ui.KeyboardControl(WM_TEXTINPUT, 0, (long)text)` casts a `const char*` to `long`. `C_VS_UI::KeyboardControl(UINT, UINT, long extra)` (VS_UI/src/header/VS_UI.h:238) forwards it to `WindowManager::KeyboardControl` (VS_UI/src/widget/u_window.cpp:1424-1429), and the receiving editors cast it back: `const char* text = (const char*)extra;` at VS_UI/src/widget/U_edit.cpp:301 and VS_UI/src/VS_UI_Title.cpp:4648. On MSVC x64 `long` is 32 bits, so the SDL event buffer pointer loses its high half. On macOS/Linux LP64 `long` is 64 bits and the same code happens to work, which is why the bug is invisible in the platform the port was developed on.

**Failure scenario:** On the Windows x64 build, typing any character with a chat box or the login ID field focused delivers a truncated pointer to `LineEditor::HandleTextInput`, which then runs `utf8_to_utf32` over an unmapped address -> access violation.

**Recommendation:** Widen the `extra` parameter to `intptr_t`/`LPARAM` through `C_VS_UI::KeyboardControl`, `WindowManager::KeyboardControl`, `Window::KeyboardControl` and `LineEditor::KeyboardControl`, or (better) route text input exclusively through `InputFocusManager::HandleTextInput(const char*)`, which already takes a proper pointer and is used by the SDL backend.

> ✅ **Fixed by removal** in `2a531a9` (branch `harden/pointer-truncation`), and **the severity stated above is wrong**. Text entry works in this build. `g_textinput_callback` and `g_textediting_callback` have exactly four references in the tree — two definitions and two assignments — and are never invoked, so the callback that would have reached the truncating sender was dead, and with it the only site in the client that constructs a `WM_TEXTINPUT` message. The live path is the one the recommendation prefers: the SDL backend calls `InputFocusManager::HandleTextInput()` directly and the pointer stays a `const char*` all the way to the focused `LineEditor`. The dead plumbing, the three callback implementations and the `WM_TEXTINPUT` receivers that recover a pointer from `extra` are therefore deleted rather than repaired — a truncating sender wired to a callback nobody calls is a landmine for whoever reconnects it, and the receivers cannot be made safe while the parameter is a `long`. Widening `extra` across ~90 declarations was deliberately not done: a missed override silently becomes an overload that never fires, which is a worse failure than the one being fixed, and nothing is left to carry.

#### 🔴 Critical -- An inventory item is deleted without clearing the tooltip descriptor that may still hold its raw pointer, leaving a use-after-free the renderer dereferences next frame.

**Category:** memory-safety  |  **Location:** `Client/Packet/Gpackets/GCDeleteInventoryItemHandler.cpp:64`

`DescriptorManager` stores the hovered item as an untyped `void* m_fp_show_param.void_ptr` (VS_UI/src/VS_UI_Descriptor.cpp:144) and dereferences it every frame in `Show()` (VS_UI/src/VS_UI_Descriptor.cpp:108), which reaches `_Item_Description_Show`'s `MItem* p_item = (MItem*)void_ptr;` (VS_UI/src/VS_UI_Description.cpp:61) and immediately calls `p_item->GetName()` (line 217). Invalidation is manual and opt-in via `UI_RemoveDescriptor` (Client/GameUI.cpp:632). Sibling handlers do call it — GCShopSellOKHandler.cpp:70/83/114/169, GCRemoveFromGearHandler.cpp:143/258/370, GCReloadOKHandler.cpp:155 — but GCDeleteInventoryItemHandler.cpp:64 does `delete pItem;` with no such call. There are ~260 `g_descriptor_manager.Set(...)` sites against ~15 `Unset` sites overall.

**Failure scenario:** Player hovers an inventory item (tooltip shown) at the moment the server sends GCDeleteInventoryItem for that item (consumed by a timer, removed by a GM, quest turn-in). The MItem is freed; the next `DescriptorManager::Show()` calls `GetName()` on freed memory -> crash or reads from a reallocated object.

**Recommendation:** Add `UI_RemoveDescriptor((void*)pItem);` before the `delete` here, and audit every `delete`/`SAFE_DELETE` of an MItem. Longer term, have DescriptorManager hold the item ID rather than a raw pointer, and re-resolve it in `Show()`.

> ✅ **Fixed** in `f0b8ae6` (branch `harden/network-input`) for this handler, matching the shop/gear/reload handlers. The broader lifetime requirement is now covered by the 2026-09-22 item-destruction follow-up under C25; ID-only resolution is superseded by exact-object invalidation. The original handler fix was a regression guard, not a reproduced timing window.

#### 🔴 Critical -- _Multiline_Info_Show writes a NUL terminator at a fixed offset into the caller's buffer before checking that the remaining string is that long, walking past the end of the buffer.

> **Current implementation (2026-09-21):** the multiline tooltip callbacks use owned `TextWrap` rows and never modify the supplied string. Sizing measures those same complete rows, including the widest row. The paragraph below records the earlier in-place repair; the wrapping findings track the remaining callers.

**Category:** memory-safety  |  **Location:** `VS_UI/src/VS_UI_Description.cpp:3696`

In the loop at lines 3688-3708, `char_temp = cur[CurrentPos - check]; cur[CurrentPos - check] = '\0';` executes before the `if(strlen(cur) < CurrentPos-check) break;` guard on line 3702. `cur` advances by ~`CurrentPos` (36) bytes per iteration and the loop runs `strlen/right + 1` times, so the final iteration writes at an offset beyond the string's end. The only caller is VS_UI/src/vs_ui_gamecommon2.cpp:15919 passing `szMissionPopupString`, a `static char[512]` (line 15853), with `right=36`. Line 3686 additionally does `strcpy(sz_temp, cur)` into a `char sz_temp[4048]` (line 3682) that is never read afterwards — a dead, unbounded copy. The function also mutates the caller's buffer in place while notionally just rendering it.

**Failure scenario:** A quest mission line of ~500 characters gives LineCount=14; `cur` reaches offset 504 and the code writes `cur[36]` = byte 540 of a 512-byte static buffer, corrupting whatever follows it in .bss, then reads it back into `char_temp` and restores it at line 3705.

**Recommendation:** Move the length check before the write, bound the loop by the actual remaining length rather than a precomputed LineCount, delete the dead `sz_temp` copy, and make the function take a `const char*` plus its buffer size instead of mutating the input.

> ✅ **Fixed** in `31f5f2f` (branch `harden/text-format`). The loop checks the remaining length before writing the terminator and is bounded by the string rather than by a precomputed count, so the NUL always lands on a real character; the dead `sz_temp` copy is gone. The signature still mutates in place — restoring each byte before the next line — because the caller set is fixed. The rewrite initially introduced a one-line overdraw, since `_Multiline_Info_Calculator` beside it still sized the box with `strlen/right + 1` while the renderer consumes one byte less per line whenever a multi-byte character straddles the cut; the calculator now runs the same walk, so the two agree by construction. That sibling's unconditional 36-byte `memcpy` out of the caller's string is bounded as well (latent — the only caller guarantees the length).

#### 🔴 Critical -- An MItem* is passed through a 32-bit int parameter and dereferenced on the other side, guaranteeing a wild-pointer dereference on 64-bit builds.

**Category:** memory-safety  |  **Location:** `VS_UI/src/VS_UI_GameCommon.cpp:2887`

`g_descriptor_manager.Set(DID_ITEM, ..., (void *)p_selected_item, 0, (int)(intptr_t)pAddedItem)` squeezes an `MItem*` into the `long right` parameter of `DescriptorManager::Set` (VS_UI/src/VS_UI_Descriptor.cpp:47). `_Item_Description_Show` recovers it with `MItem* p_AddItem = (MItem*)right;` (VS_UI/src/VS_UI_Description.cpp:66) and dereferences it at VS_UI/src/VS_UI_Description.cpp:1109 (`p_AddItem->GetItemClass()`) and :1116. On MSVC x64 (LLP64) `long`/`int` are 32 bits, so the upper half of the heap pointer is discarded and then sign-extended back. The explicit `(intptr_t)` cast that was added only silences the truncation warning; it does not preserve the value. The repo's recent commits target a VS2022 build, whose default platform is x64.

**Failure scenario:** Player hovers a gear slot that holds a Core Zap item while a compatible item is equipped. `pAddedItem` (a heap MItem*, e.g. 0x000001F4_A2B10040) is truncated to 0xA2B10040, stored in `right`, sign-extended to 0xFFFFFFFFA2B10040 and dereferenced in the tooltip renderer -> access violation on the next frame, every time.

**Recommendation:** Change `DescriptorManager::Set`/`RectCalculationFinished`/the `fp_show` signature to take a typed second payload pointer (e.g. `void* void_ptr2`) instead of overloading the `long right` field, and update the DID_ITEM path. Audit the other `Set(...)` call sites for the same pattern.

> ✅ **Fixed** in `c0df91c` (branch `harden/pointer-truncation`). `DescriptorManager` gained a typed secondary payload pointer and an accessor, passed as a trailing defaulted parameter on `Set()` so all 201 existing call sites compile untouched and the shared `fp_show`/`fp_rect_calculator` typedefs — implemented by dozens of unrelated renderers — stay as they are. The audit the recommendation asks for found this to be the only one of those 201 sites putting a pointer in `left`/`right`, but **two** consumers rather than the one named: `_Item_Description_Calculator` read the same truncated pointer to size the tooltip, so fixing only `_Item_Description_Show` would have left the crash and desynchronised the box from its contents. `Unset()` now tests both payloads — it is the item-destruction hook, and the gear tooltip is the one place where the two payloads are different objects, so matching only the primary left the secondary dangling for the next `Show()`; that use-after-free was found by the adversarial review, not by the original pass. Regression guard; the crash was not reproduced.
>
> Recorded latent: the secondary pointer lives outside `FP_SHOW_PARAM`, so it stays consistent with the descriptor being drawn only because `Set()` early-returns while one is showing. That guard is now commented as load-bearing. Anyone relaxing it — letting a new hover replace a live descriptor is the obvious future change — must move the pointer into `FP_SHOW_PARAM` at the same time.

#### 🔴 Critical -- A runtime-built string containing server-supplied quest text is used as the format string of sprintf, giving a remote format-string vulnerability.

**Category:** security  |  **Location:** `VS_UI/src/vs_ui_gamecommon2.cpp:15911`

Line 15893 builds `szString` with `sprintf(szString, <table format>, i+1, TempInfo->szMissionTitle.c_str())` — `szMissionTitle` comes from the guild-quest packet. Line 15911 then does `sprintf(szString2, szString, TempInfo->m_StrArg.c_str(), TempValue);`, i.e. the string that just embedded server text is itself the format string. Any `%` sequence in the mission title is interpreted as a conversion specifier. `szString`/`szString2` are `char[512]` stack buffers (lines 15884-15885), so `%s`/`%n`/width specifiers give stack disclosure, out-of-bounds reads, and on non-hardened CRTs arbitrary writes via `%n`.

**Failure scenario:** A malicious or compromised server sends a guild mission whose title is `%s%s%s%s%n`. Opening the quest window makes the client walk arbitrary stack words as char* and then write through one of them.

**Recommendation:** Never pass runtime data as a format string. Split the two-stage substitution: format the fixed table template once with all arguments, or use a placeholder-replacement helper (`std::string` find/replace) for the mission title and arg. Apply the same rule to the ~230 `sprintf(buf, (*g_pGameStringTable)[...].GetString(), ...)` sites, whose format strings come from an on-disk data file.

> ✅ **Fixed** in `31f5f2f` (branch `harden/text-format`). The server-supplied mission title is expanded by a bounded helper that substitutes only `%s` and `%d` and copies every other byte — a second `%s`, `%n`, a width form — literally, so no conversion in packet text reaches a printf. The remaining format on that path is the table entry itself, which is C19's problem rather than this one. The ~230 argument-passing table sites are **not** fixed; see the C19 entry for what was and was not done there.

#### 🟠 High -- C_VS_UI_EVENT_BUTTON's constructor tests the uninitialized member m_image_index instead of the parameter, so every default-image button gets a garbage or -1 sprite/menu index.

> ✅ **Fixed (2026-09-18)**: the event-button constructor selects its image from the parameter and initializes the member directly. Two tests against the real UI class reproduce both paths with prefilled storage: the missing-image case selected -1, and the explicit-image case selected the button ID. Both now select the requested value, and animation reset preserves it. The interval timer already initializes its clock state.

**Category:** undefined-behavior  |  **Location:** `VS_UI/src/header/VS_UI_widget.h:103`

Lines 97-107: the constructor body is `Init(); m_dw_millisec = millisec; if (m_image_index == -1) m_image_index = id; else m_image_index = image_index;`. `Init()` (lines 109-113) sets only `m_bl_start` and `m_alpha` — `m_image_index` is never initialized before the test, so this reads an indeterminate value (UB) and the intended "no image index given -> use the button id" fallback never fires for real. With an indeterminate non-(-1) value the else branch stores `image_index`, which for the default argument is `-1`. `m_image_index` is then used directly as a sprite index (`Blt(x, y, p_button->m_image_index)`, VS_UI/src/VS_UI_Dialog.cpp:511; VS_UI/src/VS_UI_Exchange.cpp:649; VS_UI/src/VS_UI_ELEVATOR.cpp:149) and as an array subscript (`m_p_menu[p_button->m_image_index]`, VS_UI/src/VS_UI_Dialog.cpp:569/571/576).

**Failure scenario:** Any button constructed with the default `image_index=-1` (a large fraction of the game's buttons) renders with sprite index -1 and, in the menu dialog path, indexes `m_p_menu[-1]` — an out-of-bounds read of a std::vector<std::string> member followed by `.c_str()` on garbage.

**Recommendation:** Test the parameter, not the member: `m_image_index = (image_index == -1) ? (int)id : image_index;` and initialize `m_image_index` and `m_dw_prev_tickcount` in `Init()`.

#### 🟠 High -- sscanf with unbounded %s writes file-controlled text into two 40-byte stack buffers from a 255-byte input line.

> ✅ **Fixed (2026-09-18)**: skin headers use owned string tokens, and point/rectangle rows use checked numeric extraction before appending. Tests linking the real SkinManager and CRarFile reproduced the long-key stack overwrite and malformed rows containing uninitialized coordinates. A failed reload preserves the previous skin; null/out-of-range numeric inputs have regression guards, and whitespace/comment rows are skipped. Seven parser tests exercise the real extracted-file path and coordinate readers.

**Category:** memory-safety  |  **Location:** `VS_UI/src/SkinManager.cpp:82`

`char szType[40],szKey[40]; sscanf( szLine+1, "%s %s", szKey, szType);` — `szLine` is `char szLine[256]` filled by `rarfile.GetString(szLine, 256)` (line 73), so either token can be up to ~254 bytes. `%s` has no width limit, so a long token overruns `szKey` (and then `szType`) on the stack. The skin data is loaded from a RAR resource pack (`LoadInformation`, lines 52-97), i.e. a redistributable file a user may install from a third party. The `sscanf` return value is also unchecked at lines 27, 37, 82, 112 and 134, so short lines leave POINT/RECT members uninitialized before they are pushed into `m_PointList`/`m_RectList`.

**Failure scenario:** A skin .info file containing a `*` line with a 200-character key overflows `szKey[40]` and overwrites the saved return address of `SkinManager::LoadInformation`.

**Recommendation:** Use width-limited conversions (`%39s`) everywhere, check the `sscanf` return count before using the parsed values, and prefer a tokenizer over sscanf for this parser.

#### 🟠 High -- Item-class labels from the game string table are passed as the format string to wsprintf into a 50-byte buffer with neither a bounds limit nor a format guard.

> ✅ **Verified resolved (2026-09-18)**: item-class labels use bounded `snprintf(..., "%s", GetGameString(...))` in `VS_UI_Description.cpp`. The related title-screen messages likewise use bounded literal copies or `SafeFormat::Format`. Existing formatter tests and the R7/R8 and format-arity checks cover the shared formatting path; no new runtime reproduction is claimed.

**Category:** security  |  **Location:** `VS_UI/src/VS_UI_Description.cpp:271`

`char sz_buf[50];` (line 215). Lines 271-318 call `wsprintf(sz_buf, (*g_pGameStringTable)[UI_STRING_MESSAGE_ITEM_CLASS_*].GetString());` — the data-file string is the *format* argument with no variadic arguments supplied. Any `%` in that string is a conversion specifier reading nonexistent varargs, and `wsprintf` performs no bounds checking against the 50-byte destination. The same table-string-as-format pattern recurs throughout this file (lines 371, 405, 411, 417, 594, 605, 1116, ...) and in VS_UI/src/VS_UI_Title.cpp:2638-2651 / 3634-3647.

**Failure scenario:** A localized or edited GameStringTable entry containing `%s` (trivially introduced when translating, since these are user-facing labels) makes `wsprintf` read a garbage pointer off the stack and copy it into a 50-byte buffer — stack disclosure plus overflow.

**Recommendation:** Use `wsprintf(sz_buf, "%s", table.GetString())` for the no-argument cases and a bounded `snprintf` everywhere; validate at load time that each table entry's specifier list matches what the call site supplies.

#### 🟠 High -- The item tooltip builds its name strings with an unbounded strcat chain from server-supplied text into two 100-byte stack buffers.

> ✅ **Verified resolved (2026-09-18)**: both tooltip rendering and size measurement call `BuildItemTooltipNames`, which delegates to the dynamically sized `UISafeText::FinishItemTooltipNames`. `test_ui_safe_text.cpp` covers long/multibyte names and complete versus incomplete grade tokens.

**Category:** memory-safety  |  **Location:** `VS_UI/src/VS_UI_Description.cpp:217`

`char sz_name[NAME_STRING_LEN]; char sz_ename[NAME_STRING_LEN];` with `NAME_STRING_LEN = 100` (line 35, lines 119-120). Lines 128-227 then append, with plain `strcat`: a quest prefix from the string table, `p_item->GetName()` / `GetEName()`, `pPetItem->GetPetOptionName()` / `GetPetOptionEName()` (lines 166/169, both server-supplied std::strings), and a two-byte grade suffix — none of it length-checked. Line 223 additionally reads `szGrade + p_item->GetGrade()*2` for 2 bytes, an offset of up to 20 into a string-table entry whose length is never verified.

**Failure scenario:** A pet item whose option name is set by the server to a long string, or an item name longer than ~90 bytes, overflows `sz_name`/`sz_ename` on the stack while rendering the tooltip. The same construction is repeated at lines 2733-2734 for the second description path.

**Recommendation:** Build these with std::string (or strncat with a running remaining-length), and bounds-check `szGrade`'s length before the `strncat` at line 223.

#### 🟠 High -- The file dialog strcpy's a filesystem path plus filter list into a 300-byte stack buffer with no length check.

> ✅ **Fixed** on branch `fix/review-file-dialog-labels` (2026-09-18). Path/filter composition and 38-byte display shortening moved into `basic/FileDialogListing` before the fix, and all five folder/drive/hover label sites use the same helpers. The shortening buffer is now an owned string; only an actual trailing `*.*` pattern is removed, and filter separators are inserted without erasing from possibly empty strings. The dialog also renders its two string-table titles directly, eliminating its remaining 300-byte buffer. `tests/unit/test_file_dialog_labels.cpp` has five tests: empty-path composition threw `invalid string position` before the fix, and long path/filter composition then caused a stack-buffer termination (exit `0xC0000409`). The long-filename case is an additional regression guard. Normal search patterns, empty filter lists, and the legacy 38-byte output are pinned. Full Debug/ASan builds and all nine CTests pass; R17 falls from 963 to 958 across the move and fix. Directory navigation/storage and scroll positions are separate paths, not covered by this label fix.

**Category:** memory-safety  |  **Location:** `VS_UI/src/VS_UI_ExtraDialog.cpp:2858`

`C_VS_UI_FILE_DIALOG::Show()` declares `char name[300]` (line 2793) and then does `strcpy(name, title.c_str())` (line 2858), where `title` is built from `mp_open_current_directory[mi_open_drive_index]` — a `new char[MAX_PATH]` buffer (line 2512), i.e. up to 259 characters — concatenated with every entry of `m_filter` plus `;` separators (line 2856). The same unbounded `strcpy` into `name[300]` occurs at line 2880 (`m_vs_file_list[...]`, filesystem-supplied names) and line 2914 (drive path + filters). Line 2855 (`title.erase(strlen(...)-2, 2)`) and lines 2857/2911/2913 (`erase(size()-1, 1)`) additionally underflow `size_t` and throw `std::out_of_range` when the string is shorter than the amount erased.

**Failure scenario:** User navigates the profile-picture file dialog into a deeply nested directory (path ~250 chars) with three filters configured; `title` exceeds 300 bytes and `strcpy` overruns `name` on the stack.

**Recommendation:** Replace all three `strcpy(name, ...)` with a bounded copy (or keep the std::string and pass `.c_str()` to the print functions directly), and guard the `erase` calls with size checks.

#### 🟠 High -- strncpy into a 1024-byte cursor buffer with a byte length that can reach 4096, overflowing the stack buffer.

> ✅ **Verified resolved (2026-09-18)**: cursor measurement uses `UISafeText::Utf8Prefix` and `std::string` in the editor's platform branches. Existing tests cover long supplementary-character text, character boundaries and truncated tails, without a fixed 1,024-byte cursor buffer.

**Category:** memory-safety  |  **Location:** `VS_UI/src/widget/U_edit.cpp:538`

`char cursorBuffer[1024];` (line 525, and again at line 567 in the non-macOS branch). `bytePos` is computed by walking UTF-8 bytes until `m_Editor.m_CursorPos` characters have been passed — `m_CursorPos` can be up to `LineEditor::MAX_TEXT-1` = 1023 characters, and `GetBuffer()` returns up to 4 bytes per character (its own buffer is `char[MAX_TEXT*4+1]`, U_edit.cpp:240). `strncpy(cursorBuffer, fullText, bytePos); cursorBuffer[bytePos] = '\0';` therefore writes up to 4097 bytes into a 1024-byte stack array. Both the PLATFORM_MACOS branch (lines 525-539) and the Windows branch (lines 567-581) have the identical defect.

**Failure scenario:** User pastes or types ~300 CJK characters (3 bytes each) into a chat/nickname field and presses End; on the next frame with the caret visible, `bytePos` is ~900... push it to 400 characters and bytePos exceeds 1024, smashing the stack frame of `LineEditorVisual::Show()`.

**Recommendation:** Clamp `bytePos` to `sizeof(cursorBuffer)-1` before the copy, or better, measure the prefix width directly from the UTF-32 buffer without materialising a UTF-8 copy.

#### 🟠 High -- Password mode writes the NUL terminator at an unbounded index into a 1024-byte stack buffer.

> ✅ **Verified resolved (2026-09-18)**: password masks are dynamically sized by `UISafeText::MakePasswordMask`; the editor no longer indexes a fixed stack buffer. Existing tests cover the largest editor UTF-8 result and null/empty input. One mask glyph per input byte is intentionally preserved as the established display contract; changing that count is a separate behavior choice.

**Category:** memory-safety  |  **Location:** `VS_UI/src/widget/U_edit.cpp:495`

Lines 489-496: `char displayBuffer[1024]; int len = strlen(textToDisplay);` the asterisk-fill loop is correctly bounded by `i < (int)sizeof(displayBuffer) - 1`, but the terminator write `displayBuffer[len] = '\0';` uses the unclamped `len`. `textToDisplay` comes from `m_Editor.GetBuffer()`, whose static buffer is `MAX_TEXT*4+1` = 4097 bytes (line 240), so `len` can be up to 4096.

**Failure scenario:** A password field whose byte limit was not tightened (or any editor put into password mode) holding >1024 bytes of UTF-8 causes a single 0x00 write up to 3072 bytes past `displayBuffer`, corrupting the caller's stack frame and return address region.

**Recommendation:** Clamp: `int n = min(len, (int)sizeof(displayBuffer)-1); ... displayBuffer[n] = '\0';` — and count characters, not bytes, so the asterisk count matches the visible character count for multi-byte input.

#### 🟠 High -- The UTF-8 decoder reads past the string terminator on truncated sequences and has unsequenced side effects on the same pointer, which is undefined behavior.

> ✅ **Verified resolved (2026-09-18)**: the editor uses `UISafeText::DecodeNext`/`Utf8ToUtf32`, with sequenced continuation reads, terminator checks and a null-input guard. `test_ui_safe_text.cpp` covers truncated sequences, malformed input and small output capacities. This closes the out-of-bounds/unsequenced-read finding, not a claim that every legacy editor decoding rule was redesigned.

**Category:** undefined-behavior  |  **Location:** `VS_UI/src/widget/U_edit.cpp:44`

In `utf8_to_utf32` (lines 35-60) the multi-byte branches consume continuation bytes without checking for the NUL terminator: `c = ((b & 0x1F) << 6) | (*s++ & 0x3F);` — if `b` is a lead byte at the end of the string, the NUL is consumed as a continuation byte and the `while (*s ...)` loop continues reading past the buffer. Separately, lines 46-48 and 50-53 modify `s` two and three times within a single `|` expression (`((*s++ & 0x3F) << 6) | (*s++ & 0x3F)`); the operands of `|` are unsequenced in C++, so multiple unsequenced modifications of `s` are undefined behavior, and the byte-to-position assignment is compiler-dependent. Callers include `HandleTextInput` (line 195), `HandleTextEditing` (line 209, which also does not NULL-check `text`) and `AddString` (line 260).

**Failure scenario:** An SDL_TEXTEDITING/TEXTINPUT payload ending in a truncated multi-byte sequence (common with IME composition being cut mid-character, or any malformed UTF-8 reaching AddString from server data) makes the decoder run off the end of the SDL event buffer; and even on well-formed input, different compilers/optimization levels decode 3- and 4-byte sequences differently.

**Recommendation:** Rewrite as a table- or explicit-step decoder that checks for `\0` and for `0x80` continuation bits before each consumption, with each `s++` on its own statement. Add a NULL guard to `HandleTextEditing`.

#### 🟡 Medium -- The SDL replacement for g_PossibleStringCut has different semantics from the original its ~40 call sites were written against.

> ✅ **Resolved by caller migration and API retirement (2026-09-21):** all compiled callers now use owned UTF-8 rows or bounded scalar prefixes through `TextWrap`, `DescriptorText`, `HelpLayout` and the string reducers. The mismatched pixel-width implementation and shared declaration are removed, so compiled code cannot accidentally call the old API. Existing library tests own scalar boundaries, row consumption, layout and capacity contracts; executable UI wiring remains a regression guard. Remaining textual mentions are in comments or the native GDI `FL2.cpp` excluded by the forced SDL build on every supported platform. Resource ingress and renderer encoding policy remain separate work under the encoding finding.

> **Tooltip follow-up (2026-09-21):** `_Multiline_Info_Calculator` and `_Multiline_Info_Show` now share owned rows from `basic/TextWrap`, normalize the complete input before splitting and measure the widest rendered row. No temporary NUL is written into the caller's text. Five library tests cover complete UTF-8 scalars, progress at narrow columns, spaces, explicit line breaks, empty input and bounded/read-only input lifetimes. The UI wiring is a regression guard; the other live wrapping callers and the old predicate remain open under this finding and its duplicate above.

> **Creature follow-up (2026-09-21):** chat and personal-shop signs normalize the complete display text before wrapping. `TextWrap` preserves their seam spaces and ordinary-chat newline policy; two additional tests cover these policies and every supported byte column (4–255). `AppendChatRow` clips all rows, including Christmas-tree recipient/sender decorations, to complete scalars within the allocated capacity. Personal-shop signs no longer mutate borrowed input. These game-global paths are build/source regression guards; no packet bytes or wire encoding contract changed. The remaining UI callers still need migration.

> **Dialog follow-up (2026-09-21):** seven team/dialog/billing/quest/lottery wrappers now use owned rows with explicit leading-space and newline policies. The team introduction uses exact per-row consumption for its narrower first row. Fixed 512/2048-byte scratch copies and the two whole-remainder `strcpy` calls are gone, and layout guards zero glyph widths and nonpositive columns. The personal-shop wrapper with an always-empty input was removed. Five further library guards cover consumption, ownership, changing columns, escaped markers crossing a boundary, text beyond the old scratch limits and many short rows under a large column. The row scan stops at the first line break before decoding the prefix. These UI integrations are regression guards; subsequent descriptor work is recorded below.
>
> **Descriptor follow-up (2026-09-21):** both descriptor entry points use bounded, owned UTF-8 layout. File input opens through the declared resource decoder; string input retains literal tag characters. Empty titles, missing colons, long lines and image/continuation indents no longer reach scratch arrays, invalid pointer subtraction or unsigned width underflow. Eight library guards cover layout, title/image offsets, row/byte limits and checked indices. The game-global integration validates pack/sprite indices, clips images against the actual surface and descriptor viewport, guards pages/scrolling and publishes candidate rows and file-created packs together; explicit titles and caller-supplied icon packs are preserved. The unused original-token substitution path is removed after a full-tree write/caller inventory. The core-zap pre-read also uses a complete decoded line and checked tag. These UI changes are regression guards, not reproduced runtime crashes. R17 falls from 584 to 570. Rich-help wrapping and the old predicate remain open.
>
> **Indexed pack dependency (2026-09-21):** `CTypePack::LoadFromFileData` and its `CTypePack2` counterpart ignored short index reads, negative/out-of-file offsets and element decoder failure. Two direct unit tests reproduced 46 failed checks across both implementations. Their shared index reader now validates the selected entry before invoking the decoder, and both overloads return the decoder result. This covers indexed single-entry loading; whole-pack and lazy loaders are separate paths.

> **Rich-help attribute follow-up (2026-09-21):** all nine live layout/rendering lookups use the owned `basic/HelpMarkup` parser. It matches complete names outside values and reads only within the supplied span; missing or unterminated quotes return an empty string. The old nullable pointer and shared 512-byte buffer are removed. Five library regression guards cover both quote styles, the legacy optional equals sign, name collisions, truncated/exact spans, long Unicode values and independent result lifetimes. This is an attribute-reader fix, not a claim that the whole help parser is safe: image lookup, geometry, substitution and wrapping still need migration.

**Category:** correctness  |  **Location:** `Client/RenderingFunctions.cpp:313`

The original (VS_UI/src/hangul/FL2.cpp:38-82, now excluded from the build) answered "is byte offset `position` a safe place to cut this DBCS string" by walking lead/trail byte pairs. The replacement is `bool g_PossibleStringCut(const char* pStr, int maxWidth) { return g_GetStringWidth(pStr, NULL) <= maxWidth; }` — "does the whole string fit in maxWidth pixels". Every caller still passes a byte/character index: `g_PossibleStringCut(str, 5)` in the old ReduceString, `g_PossibleStringCut(cur, CurrentPos)` with CurrentPos=36 in VS_UI/src/VS_UI_Description.cpp:3691, and Client/MCreature.cpp:4905/5091. The boolean feeds directly into cut-offset arithmetic (`check = 0` or `1`).

**Failure scenario:** Multi-byte names and mission text are cut mid-character (mojibake in the tooltip/quest/creature-name renderers), and the `check` adjustment that the multiline renderer's index math depends on is now essentially arbitrary — which is what makes the off-by-N in _Multiline_Info_Show reachable.

**Recommendation:** Reimplement the function as a UTF-8/DBCS character-boundary test over the byte index (the name and every call site expect that), and give the pixel-fitting query a different name if anything actually needs it.

#### 🟡 Medium -- The rewritten ReduceString family writes at index maxWidth without knowing the destination buffer's size, and can write past the end of a short string.

> ✅ **Already repaired; regression coverage added** on branch `refactor/review-string-reduction` (2026-09-18). This duplicates the High `ReduceString2` finding and its `harden/high-severity-batch1` repair above: all three functions return before writing when the string is no longer than the byte limit, and the first two handle limits below three without negative subscripts. `ReduceString3` already had the required bounds. The implementations now live in `basic/StringReduction.cpp`, with their bodies unchanged, so `tests/unit/test_string_reduction.cpp` links the production functions directly. Four regression guards cover exact-size short allocations, writes at every small length/limit pair, non-positive/extreme limits, and an unpaired high byte at the terminator. These are guards for the existing repair, not reproductions of a new defect. The API requires a writable NUL-terminated string and never grows it; character-boundary semantics remain tracked separately under the two `g_PossibleStringCut` findings.

**Category:** memory-safety  |  **Location:** `Client/UIUtilityFunctions.cpp:56`

`ReduceString` (line 25-28), `ReduceString2` (lines 58-61) and `ReduceString3` (lines 79-82) all write `pStr[maxWidth-3..maxWidth]` (or `cutLen..cutLen+3`) after validating only `maxWidth > 0`. `ReduceString2` is the worst: its `len` is a byte count that stops at the NUL, so a 34-byte string with `maxWidth=36` satisfies `len > maxWidth-3` and writes indices 33-36 — three bytes past the string's terminator. The callers pass raw stack arrays (VS_UI/src/VS_UI_ExtraDialog.cpp:1513, 2859, 2881, 2915; VS_UI/src/vs_ui_gamecommon2.cpp:15008, 15455, 15922, 17009) whose capacity these functions never learn.

**Failure scenario:** Any caller that passes a buffer sized to the string rather than to `maxWidth+1` gets a 1-4 byte overflow; in the current tree the buffers happen to be 300/512 bytes, so it corrupts adjacent stack data silently rather than crashing.

**Recommendation:** Take the destination capacity as a parameter (or return a std::string), and make the DBCS scan in ReduceString2 stop before writing into a trail byte.

#### 🟡 Medium -- The Chinese IME composition handler reads an unbounded IMM composition string into a 128-byte member buffer and terminates past its end.

> ✅ **Removed** on branch `cleanup/review-legacy-ime` (2026-09-18). Deleted the excluded `VS_UI/src/hangul/Ci.cpp` and its obsolete CMake exclusion, as the recommendation permits. Its unchecked `ImmGetCompositionString` copy can no longer be revived by changing the source filter. `Ci_macOS.cpp` already supplies the compatibility class definitions on every supported SDL build, while SDL text events use the editor path. This removes dormant code rather than changing live input behavior; the separate Windows editor activation finding remains open. Full Debug/ASan builds and all nine CTests pass.

**Category:** memory-safety  |  **Location:** `VS_UI/src/hangul/Ci.cpp:293`

`m_composing_string` is `char[128]` (VS_UI/src/hangul/Ci.h:43). Lines 290-296: `if ((len = ImmGetCompositionString(m_hIMC, GCS_RESULTSTR, NULL, 0)) > 0) { ImmGetCompositionString(m_hIMC, GCS_RESULTSTR, m_composing_string, len); m_composing_string[len] = NULL; ... }` — `len` is the composition string's byte length as reported by the IME and is never compared against 128, so both the read-into and the terminator write can go out of bounds. This file is currently excluded from the build (CMakeLists.txt:194) in favour of the Ci_macOS.cpp stubs, so it is not live today, but it is the reference implementation anyone re-enabling Windows IME support will start from.

**Failure scenario:** With the Windows IME path restored, composing a phrase longer than 127 bytes in a Chinese IME overwrites the CI object past `m_composing_string` — which, given the member layout, runs off the end of the heap allocation.

**Recommendation:** Clamp to `min(len, (int)sizeof(m_composing_string)-1)` before both the read and the terminator write. Either fix it in place or delete the file so it cannot be revived as-is.

#### 🟡 Medium -- C_VS_UI_SCROLL_BAR's default constructor leaves m_button_width uninitialized, and it is read on every mouse move over a horizontal scroll bar.

> ✅ **Fixed as a regression guard** on `fix/review-widget-draw-guards` (2026-09-18). The default constructor now initializes button width from the same sprite as the parameterized constructor. The report's horizontal failure scenario was not reproduced: the default constructor sets `m_bHeight = true`, and `SetSize` does not change orientation. The missing initialization is nevertheless corrected. This constructor still reaches the game's global sprite resource and remains within the UI exemption; full Debug/ASan builds and all nine CTests pass, without a live-server gate.

**Category:** undefined-behavior  |  **Location:** `VS_UI/src/header/VS_UI_widget.h:432`

The parameterized constructor sets `m_button_width = spk->GetWidth(...)` at line 406, but the default constructor (lines 413-436) initialises `m_button_height` and `m_tag_height` and omits `m_button_width` entirely. `MouseControl` reads it in the horizontal branch at lines 714 and 724 (`_x < x && _x > x-m_button_width`, `_x < x+w+m_button_width`).

**Failure scenario:** A default-constructed scroll bar configured later via `SetSize()` computes its up/down button hit rectangles from indeterminate stack/heap garbage — the arrow buttons respond in the wrong screen region or never respond, non-deterministically per run.

**Recommendation:** Set `m_button_width` (and `m_pos`/`m_spk` consistently) in the default constructor, ideally by factoring both constructors onto a shared init helper.

#### 🟡 Medium -- ScrollUp/ScrollDown have no lower clamp, so a scroll bar whose pos_max is zero or negative produces a negative scroll position that leaks into list index arithmetic.

> ✅ **Fixed** on branch `fix/review-scrollbar-range` (2026-09-18). The actual scrollbar position calculations moved into `basic/ScrollRange.h` before the fix. Position state is private, every update clamps both ends using widened arithmetic, and non-positive ranges stay at position zero. Forty container-size call sites now use checked item/position counts, retaining their existing visible-row offsets while checking before unsigned subtraction or narrowing. Both UI constructors use the checked range setter; all thumb drawing uses the same state without temporarily reversing it. `tests/unit/test_scroll_range.cpp` has nine tests covering normal and reverse buttons, empty/negative ranges, extreme deltas and counts, pixel geometry, and drawing/dragging agreement. Before the fix, the range tests returned negative indices (including -21 and -2), large deltas wrapped to -4, and reverse dragging selected the opposite endpoint. Full Windows Debug/ASan builds and all nine CTests pass. The default constructor's separate uninitialized button width remains tracked by its own finding.

**Category:** correctness  |  **Location:** `VS_UI/src/header/VS_UI_widget.h:823`

`ScrollDown` is `m_pos = min(m_pos_max-1, m_pos+pos)` (line 823) and the reverse branch of `ScrollUp` is the same (line 807) — neither applies the `max(0, ...)` clamp that `SetScrollPos` does (line 829). `m_pos_max` reaches non-positive values easily because callers compute it from unsigned sizes: `SetPosMax(m_vs_file_list.size()-12)` (VS_UI/src/VS_UI_ExtraDialog.cpp:2537, 2583, 2635) and `SetPosMax(...->size()-7)` (VS_UI/src/VS_UI_GameCommon.cpp:12388, 12401, 13556, 13563, 13570, 13577, 13584) underflow `size_t` when the list is shorter than the subtrahend and narrow to a negative `int`. The default constructor also sets `m_pos_max = -1` (line 419).

**Failure scenario:** Open the file dialog on a directory with fewer than 12 entries: `size()-12` underflows, `m_pos_max` becomes negative, one click on the down arrow sets `m_pos` to a negative value, and `GetScrollPos()` returns it to `m_vs_file_list[i + GetScrollPos()]` (VS_UI/src/VS_UI_ExtraDialog.cpp:2840, 2880) and `m_button_y_list[GetScrollPos()]` (VS_UI/src/VS_UI_Dialog.cpp:391).

**Recommendation:** Clamp both ends in ScrollUp/ScrollDown (`m_pos = max(0, min(m_pos_max-1, ...))`), make `SetPosMax` reject/clamp non-positive values, and fix the call sites to compute `max(0, (int)size() - N)` before the subtraction.

#### 🟡 Medium -- m_p_menu is indexed by the button's image index without the null/bounds check that the sibling branch three lines below applies.

> ✅ **Fixed as a regression guard** on `fix/review-widget-draw-guards` (2026-09-18). A shared guard rejects a missing menu and negative or out-of-range row indices before layout or either pressed/unpressed drawing branch can index it. Null button pointers are rejected at entry. This renderer still reaches live game surfaces and resources and remains within the UI exemption; the source-path audit confirms the guard precedes every menu access, and full Debug/ASan builds plus all nine CTests pass. No live-server verification is required.

**Category:** memory-safety  |  **Location:** `VS_UI/src/VS_UI_Dialog.cpp:569`

Lines 569, 571 and 576 do `m_p_menu[p_button->m_image_index].sz_menu_str...` with no guard, and lines 597/599 repeat it. The equivalent access at line 605 is guarded: `if(m_p_menu != NULL && p_button->m_image_index < m_menu_count && ...)`. The index comes from `m_image_index`, which the constructor mis-assigns (see the C_VS_UI_EVENT_BUTTON finding) and which for menu buttons is set to the menu row index at VS_UI/src/VS_UI_Dialog.cpp:1179.

**Failure scenario:** A menu dialog whose `m_p_menu` was freed/rebuilt (line 1152 deletes `m_button_y_list`, and the menu array is rebuilt around it) or a button whose `m_image_index` is stale relative to the current `m_menu_count` dereferences past the end of the menu array while drawing, reading a bogus std::vector and calling `.c_str()` on it.

**Recommendation:** Hoist the guard from line 605 to cover all five accesses, or resolve the menu entry once at the top of the block and bail out if it is out of range.

#### 🟡 Medium -- Two different platform macros gate the same SDL-vs-Win32 decision, so the Windows build takes the legacy IME path whose implementation was removed from the build.

> ✅ **Fixed** on branch `fix/review-editor-sdl-focus` (2026-09-18). InputFocusManager owns acquisition and SDL text-input activation on every supported platform. Switching editors deactivates the previous owner; releasing an inactive editor cannot stop the current one, and destroying the focused editor stops text input. The duplicate login edit dispatcher and inert Window-to-native-IME branch are removed: SDL text, composition and editing keys arrive through the existing DXInput host once. Rendering uses the same field-font, width and horizontal-scroll path everywhere, through the TextService-backed compatibility printer. Tests reproduced Windows acquisition, focus-switch and destruction defects plus incorrect preedit cancellation/double commits. Full Debug/ASan builds and all nine CTests pass; focused Linux ASan/UBSan tests pass with SDL's dummy video driver.

**Category:** maintainability  |  **Location:** `VS_UI/src/VS_UI_Title.cpp:4644`

`C_VS_UI_LOGIN::KeyboardControl` branches on `#ifndef PLATFORM_WINDOWS` (line 4644) to route SDL text input, while `LineEditorVisual` branches on `#ifdef PLATFORM_MACOS` for the same purpose (VS_UI/src/widget/U_edit.cpp:363, 390, 499). `PLATFORM_MACOS` is defined by CMakeLists.txt only under `if(NOT WIN32)` (line 364-366) and `PLATFORM_WINDOWS` is defined by basic/Platform.h:37 on Windows, so on the Windows SDL build both files take their 'legacy Windows' branch: `LineEditorVisual::Acquire()` never calls `SDL_StartTextInput()`, `Show()` uses the legacy `g_Print` path instead of TextService, and the login screen delegates to `Window::KeyboardControl` -> the CI IME classes, whose real implementations (VS_UI/src/hangul/Ci.cpp, FL2.cpp) are excluded from the build by CMakeLists.txt:193-196 and replaced by the no-op stubs in Ci_macOS.cpp.

**Failure scenario:** A contributor fixes text input on macOS and it silently stays broken (or diverges) on Windows, because the same logical condition is spelled two different ways and neither name means what the build actually selects.

**Recommendation:** Introduce a single `USE_SDL_INPUT`-style macro driven by the CMake option (which is already forced ON for all platforms) and replace every `PLATFORM_MACOS`/`!PLATFORM_WINDOWS` use that is really about the SDL backend, then delete the dead Win32 branches.

#### 🟡 Medium -- Button's default constructor leaves the exec handler pointer, id and click option uninitialized while Run() guards only against null.

> ✅ **Fixed (2026-09-18)**: the default button initializes its handler to null, ID to zero, and click mode to release. Tests link the real UI implementation: prefilled storage reproduced the garbage ID and missing callback, and clicking before handler installation crashed. Three tests now cover default settings, handler-free clicks, and explicit press-mode behavior.

**Category:** undefined-behavior  |  **Location:** `VS_UI/src/widget/u_button.h:105`

`Button::Button()` (VS_UI/src/widget/u_button.cpp:170-173) has an empty body; `m_pC_exec_handler`, `m_id` and `m_click_option` (u_button.h:105-107) are left indeterminate. `Button::Run()` (u_button.cpp:190-194) then does `if (m_pC_exec_handler) m_pC_exec_handler->Run(m_id);` — a non-null garbage pointer passes the guard and results in a virtual call through an arbitrary address. `MouseControl` also compares the indeterminate `m_click_option` at u_button.cpp:283 and 302. The header explicitly advertises this two-phase pattern in its usage comment (u_button.h:115-120), and `SetExecHandler` has no callers in the tree, so any future use of pattern (2) hits this immediately.

**Failure scenario:** A widget default-constructs a Button and receives a click before `SetExecHandler` runs (or the author follows the documented pattern and omits it): `Run()` dispatches a virtual call through uninitialized stack memory.

**Recommendation:** Initialize the three members in the default constructor (`m_pC_exec_handler = NULL; m_id = 0; m_click_option = RUN_WHEN_PUSHUP;`), or delete the default constructor since nothing uses it.

#### 🟡 Medium -- SetAbsWidth writes a member nothing reads, while the width actually used for layout is hardcoded to 100 pixels forever.

> ✅ **Fixed** on branch `fix/review-editor-sdl-focus` (2026-09-18). The unused second width and abandoned font-atlas members are deleted. `SetAbsWidth` maintains one nonnegative width used by rendering and `ReachSizeOfBox`; the latter retains its existing twelve-pixels-per-character estimate. Both platforms use the configured field font and horizontal scrolling in `Show`. A direct editor test reproduced the ignored width and covers negative widths. Full Debug/ASan builds and all nine CTests pass.

**Category:** correctness  |  **Location:** `VS_UI/src/widget/U_edit.cpp:418`

`m_AbsWidth` is written only by `SetAbsWidth` (line 420) and never read anywhere in the tree. The width the editor actually uses — `m_MaxWidth` — is set to 100 in the constructor (line 353) and never written again, yet it is the wrap width passed to `TextService::DrawLine` (line 517) and the threshold in `ReachSizeOfBox()` (line 448). Every caller configures the field via `SetAbsWidth`: chat at CHAT_INPUT_WIDTH (VS_UI/src/VS_UI_GameCommon.cpp:5941, 6491), nickname at 140, SMS at 90, etc. (VS_UI/src/vs_ui_gamecommon2.cpp:1424, 9767, 11582, 13581, 17176).

**Failure scenario:** Every text field in the game wraps or clips its content at 100 pixels regardless of the configured widget width, and `ReachSizeOfBox()` reports 'full' at the wrong length for every field.

**Recommendation:** Make `SetAbsWidth` set both (or collapse the two members into one) and delete whichever is redundant; the two-field split appears to be a porting artifact.

#### 🟡 Medium -- Window's destructor does not unregister from WindowManager, so the manager keeps raw pointers to destroyed windows.

> ✅ **Fixed as a regression guard** on `fix/review-window-lifetime` (2026-09-18). Registration records its actual manager, and the base destructor detaches from that manager without callbacks into already-destroyed derived fields or attached editors. Cleanup removes both display-list entries and every cached window pointer; surviving keyboard/topmost selection is recomputed only when needed. A private list replaces the allocating `SimpleDataList::Delete` path so destructor cleanup cannot allocate or be bypassed through inherited `Add`/`Delete`. Manager destruction clears surviving windows' owner links, explicit unregister is idempotent and still performs live disappearance, and copying either registration participant is disabled. Initial mouse coordinates are now defined. The report's constructor description was inaccurate: the geometry constructor did register; the default constructor still relies on explicit subclass registration. These game-coupled window/input paths use the documented UI exemption, source lifetime audit, full Debug/ASan builds and all nine CTests in both trees; no live-server gate is required.

**Category:** memory-safety  |  **Location:** `VS_UI/src/widget/u_window.cpp:66`

`Window::~Window()` (lines 62-73) has its `gpC_window_manager->Unregister(this)` call commented out, and the constructors' matching `Register(this)` calls are commented out too (lines 36-43, 46-55). Registration is instead done ad hoc by subclasses calling `g_RegisterWindow`/`g_UnregisterWindow` (VS_UI/src/VS_UI_Dialog.cpp:242 and ~30 other sites). Any Window subclass that is destroyed without an explicit `g_UnregisterWindow` leaves a dangling `Window*` in `WindowManager`'s SimpleDataList and potentially in `m_show_list`, which `MouseControl`/`Show`/`SetMouseMoveFocusedWindow` (lines 1070-1122) then dereference. `Unregister` (lines 1564-1574) also clears only `m_pC_pushed_window`; `m_pC_mouse_click_window` is never cleared anywhere.

**Failure scenario:** A dynamically created dialog is deleted on a path that forgets `g_UnregisterWindow` (or throws before reaching it); the next mouse-move iterates the show list and calls `IsPixel()` through the freed vtable pointer.

**Recommendation:** Restore automatic registration/unregistration in `Window`'s constructor/destructor (guarding for a null manager), or at minimum make `~Window` call `gpC_window_manager->Unregister(this)` unconditionally and have `Unregister` null every cached `Window*` member including `m_pC_mouse_click_window`.

#### ⚪ Low -- SetScrollPixel divides by (h - m_tag_height) with no guard against the two being equal.

> ✅ **Fixed** with `fix/review-scrollbar-range` (2026-09-18). The shared pixel mapper returns position zero when the thumb fills/exceeds the bar, when thumb size is invalid, or when the range cannot scroll. Valid coordinates are widened and clamped before multiplication; reverse dragging agrees with reverse thumb drawing. The equal-size test terminated the unfixed unit binary with integer divide-by-zero (`0xC0000094`); the repaired suite also covers shorter/negative extents and extreme coordinates. Both UI orientations delegate their actual geometry to this mapper. `tests/unit/test_scroll_range.cpp`, full Debug/ASan builds, and all nine CTests verify the fix.

**Category:** correctness  |  **Location:** `VS_UI/src/header/VS_UI_widget.h:835`

`SetScrollPos((_pixel-y-m_tag_height/2)*m_pos_max/(h-m_tag_height));` (line 835, and the horizontal variant using `w-m_tag_height` at line 837). `h`/`w` come from the caller-supplied `Rect` (constructor line 377, `SetSize` line 453) and `m_tag_height` from the SB_TAG sprite's height (line 408), so nothing prevents them from matching. `Show()` at lines 524/616/662 performs the analogous division but at least guards it with `if(m_pos_max > 1)`; `SetScrollPixel` has no guard at all.

**Failure scenario:** A scroll bar laid out with a height equal to the tag sprite height (or a skin pack whose SB_TAG sprite grows to the bar height) divides by zero on the first drag of the tag — an immediate integer-divide-by-zero fault on x86.

**Recommendation:** Guard the denominator: if `h - m_tag_height <= 0`, set the position to 0 and return.

#### ⚪ Low -- GetBuffer and GetStringWide return pointers to function-local static buffers, so two editors' text can never be held at once.

> ✅ **Fixed** on branch `fix/review-editor-text-storage` (2026-09-18). Both getters return owned strings; the legacy `GetString()` view has storage belonging to its editor instead of a process-wide buffer. Wide-string callers consume their temporary's pointer in the same full expression. UTF-16 output holds every supplementary pair through U+10FFFF, and invalid scalar values become replacement characters in both encodings. `Show()` first moved unchanged into its own translation unit so the real editor can link into `ui_tests` without game rendering globals. Tests reproduced cross-editor overwrites, invalidation of saved results and the maximum-scalar/truncation defects; ordinary editing and invalid-scalar cases are regression guards. Full Debug/ASan builds and all nine CTests pass.

**Category:** maintainability  |  **Location:** `VS_UI/src/widget/U_edit.cpp:240`

`LineEditor::GetBuffer()` returns `static char utf8_buffer[MAX_TEXT*4+1]` (line 240) and `LineEditorVisual::GetStringWide()` returns `static char_t wide_buffer[LineEditor::MAX_TEXT]` (line 454). Both are declared `const` member functions that mutate shared state. `LineEditorVisual::Show()` already relies on subtle re-entrancy here, calling `GetBuffer()` at line 486 and again at line 568 while the first result is still nominally in scope.

**Failure scenario:** Any expression that compares or concatenates two editors' contents — `strcmp(a.GetString(), b.GetString())`, or passing `GetString()` into a function that itself calls `GetString()` on another editor — silently sees the same buffer twice. It is also not thread-safe and not re-entrant.

**Recommendation:** Return a `std::string` by value (or fill a caller-supplied buffer). The callers in VS_UI/src/VS_UI_GameCommon.cpp:4449-4453 already copy the result immediately, so the change is mechanical.

---

## Foundation Libraries

**Grade:** D  |  **Findings:** 27

**Scope:** Foundation libraries — basic/ (Platform/PlatformSDL, BasicException, BasicMemory, Timer2, Directory, ColorDraw, GL_import), Client/framelib/ (TArray, CFrame, CFramePack, CFrameSet), cross-cutting containers/utilities in Client/ (COrderedList.h, CDataTable.h, CPositionList.h, CMessageArray, CToken, MemoryPool), and the huffman/bit-reserve compression code (MZLib/DEUtil/VolumeLib do not exist in this tree)

**Health assessment:** This layer is the weakest part of the tree because everything above it depends on it. The shared containers (TArray, CDataTable, CPositionList, CToken, CMessageArray) all manage raw owning pointers with no copy constructor, no bounds checking, and no validation of sizes read from data files — TArray::LoadFromFile is on a live runtime path (EffectResourceContainer) parsing .efpk files. CMessageArray, the logging facility used from packet handlers, formats network-supplied strings through unbounded vsprintf into a fixed 4096-byte buffer and leaves m_Filename dangling after Release(). MemoryPool, which backs operator new for every creature object, allocates with ::operator new and frees with free(). basic/Platform.h is a 2038-line Win32 shim that silently neuters assert(), macro-defines min/max over std::, and contains stub functions with off-by-one and negative-index writes; PlatformSDL.cpp's event primitives have unreachable branches that make manual-reset events behave incorrectly, and platform_get_executable_dir has two separate one-byte buffer overflows. Five #include directives use the wrong filename case, so the "Linux should work" claim in CLAUDE.md cannot be true. Nothing here is unfixable — most items are small, local, and testable — but the density of real defects in code that multiplies across the whole client is high.

#### 🔴 Critical -- AddFormat/AddFormatVL run unbounded vsprintf into a fixed 4096-byte static buffer, with the format string loaded from a data file and the arguments supplied by the server.

**Category:** memory-safety  |  **Location:** `Client/CMessageArray.cpp:333`

Client/CMessageArray.cpp:330-333 declares `static char Buffer[4096];` and calls `vsprintf(Buffer, format, vl);` with no length limit. AddFormatVL does the same at lines 265-268. The callers make this remotely reachable: Client/Packet/Gpackets/GCPartyInviteHandler.cpp:242 calls `g_pGameMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_SOMEONE_JOINED_PARTY].GetString(), pCreature->GetName())` and Client/ModifyStatusManager.cpp:1314 passes `petName.c_str()` — the format string comes from the on-disk game string table while the %s arguments come off the wire. Two distinct problems compound: (a) a long server-supplied name overruns the 4096-byte static buffer, corrupting adjacent .bss globals; (b) because the format string is data, a string-table entry whose conversion specifiers do not match the arguments actually passed (e.g. an extra %s or %n) is a classic format-string bug reading arbitrary stack. `static` also makes both functions non-reentrant.

**Failure scenario:** A server (or a modified Data/Info string table) sends a party-join notification whose creature name is longer than ~4KB, or a string-table entry gains an extra %s relative to the single argument GCPartyInviteHandler passes. vsprintf writes past Buffer[4095] into neighbouring globals, or dereferences a garbage stack value as a char*, crashing or corrupting state.

**Recommendation:** Replace both vsprintf calls with vsnprintf(Buffer, sizeof(Buffer), ...) and make Buffer a local (or per-instance) rather than static. Longer term, stop using data-file strings as printf format strings — validate the specifier list at load time, or switch to an indexed substitution scheme.

> ✅ **Fixed** in `0e9d247` (branch `harden/text-format`), the same change as C20 — this is the Foundation Libraries reviewer's report of the same two functions. The load-time specifier validation the "longer term" note asks for is the C19 gate, added in `31f5f2f`.

#### 🔴 Critical -- MemoryPool allocates its blocks with ::operator new but releases them with free(), which is undefined behaviour.

**Category:** memory-safety  |  **Location:** `Client/MemoryPool.cpp:50`

Client/MemoryPool.cpp:72 allocates each pool chunk via `CBlock *pPool = (CBlock*)( ::operator new( sizeof(CBlock) + ( m_BlockSize * m_BlockCount )) );` while the destructor at Client/MemoryPool.cpp:47-53 releases the same chunks with `free( m_pCurrentBlock );`. Mixing the C++ allocation function with the C deallocator is undefined; on MSVC the two can route through different heaps/bookkeeping. This is live code: g_CreatureMemoryPool, g_CreatureWearMemoryPool, g_NPCCreatureMemoryPool and g_FakeCreatureMemoryPool (Client/MemoryPool.cpp:30-33) are namespace-scope objects whose destructors run at process exit, and they back operator new for MCreature (Client/MCreature.h:115-123), MCreatureWear, MNPC and MFakeCreature. Secondary defect in the same function: `if( pPool == NULL ) return NULL;` at line 74 is dead, because ::operator new throws std::bad_alloc rather than returning null, so out-of-memory escapes as an uncaught exception.

**Failure scenario:** On shutdown, ~MemoryPool passes an ::operator new pointer to free(). Under a debug CRT or a heap with allocator-specific headers this trips a heap-validation assertion or corrupts the heap; under ASan it is reported as alloc-dealloc-mismatch.

**Recommendation:** Use ::operator delete(m_pCurrentBlock) in the destructor to match the allocation, and drop the dead NULL check (or use the nothrow form of operator new if the null path is wanted).

> ✅ **Fixed** in `8b349e7` (branch `harden/text-format`). The destructor — the class's only release path, since `Free()` merely pushes onto an intrusive free list inside the chunks — now uses `::operator delete`, and the dead NULL check is dropped rather than switched to the nothrow form: these pools back a throwing `operator new`, which must not hand a null block to a new-expression. Regression guard.

#### 🟠 High -- Five #include directives in basic/ use the wrong filename case, so the library cannot compile on a case-sensitive filesystem despite the documented Linux support.

> ✅ **Verified resolved (2026-09-18)**: the named live include directives now match the tracked filenames. The complete client and tests pass on case-sensitive Linux under GCC, Clang and GCC ASan/UBSan ([CI run](https://github.com/bound2/opendarkeden-client/actions/runs/35276197859)). Historical commented include lines are not compiled.

**Category:** build  |  **Location:** `basic/Basics.h:15`

git ls-files shows the tracked names as basic/Basics.h, basic/i_signal.h, basic/timer2.h and basic/2d.h, but the includes spell them differently: basic/Basics.h:15 `#include "I_signal.h"` (file is i_signal.h); basic/timer2.h:14 and basic/IMG.h:14 `#include "BasicS.h"` (file is Basics.h); basic/Timer2.cpp:11 `#include "Timer2.h"` (file is timer2.h); basic/TGA.h:14 and basic/GL_import.h:4 `#include "2D.h"` (file is 2d.h). Four VS_UI headers repeat the "BasicS.h" spelling (VS_UI/src/header/VS_UI_Base.h:12, VS_UI/src/header/VS_UI_util.h:18, VS_UI/src/widget/SimpleDataList.h:12, VS_UI/src/hangul/Ci.h:17), while sibling headers in the same directory use the correct "Basics.h" (VS_UI/src/widget/u_button.h:15). basic/Timer2.cpp is unconditionally in BASIC_SOURCES (basic/CMakeLists.txt:37-42), so this is not dead code. CLAUDE.md states "Linux (should work)" — it cannot, and the inconsistent spelling within the same directory means this was never caught because development happens on case-insensitive Windows/macOS.

**Failure scenario:** A contributor runs `make debug-asan` on Linux. Compilation of basic/Timer2.cpp fails immediately with "fatal error: Timer2.h: No such file or directory", and the same for every TU that reaches Basics.h.

**Recommendation:** Normalise all of these to the tracked on-disk names (Basics.h, i_signal.h, timer2.h, 2d.h) in one pass, then add a CI job that builds on Linux so the class of bug cannot reappear.

> ⚠️ **Swept on branch `harden/high-severity-batch1`, not closed. The finding understates the scale by roughly two orders of magnitude, and Linux still does not build.** Its headline says "Five #include directives"; its own body lists ten. Roughly **440 wrong-case includes across ~180 files** were rewritten, plus 37 that used Windows backslash separators (which break on Linux whatever their case). `client_PCH.h` alone accounted for about 65 sites against a tracked `Client_PCH.h`.
>
> **The total is deliberately given as a range, because it depends on a modelling choice rather than on counting.** Two independent measurements gave 412 and 474. Both are "correct" — they differ in which `-I` roots each treats as reachable from a given file, and an include only counts as broken if no *reachable* root resolves it case-sensitively. Anyone re-measuring should expect a different number again and should derive roots from the generated project files rather than from `CMakeLists.txt` prose.
>
> The rewrite ran under two invariants that make a change this size auditable: it happens **only** when `lc(old) == lc(new)`, so it can change case and nothing else; and the substitution is byte-level in binary mode, confined to the bytes inside the quotes.
>
> **Three corrections to how this was first written up, each of which matters more than the counts:**
>
> 1. **The invariant is weaker than it was claimed to be, and it let a regression through.** `lc(old) == lc(new)` forbids *redirecting* an include to a different file; it does **not** forbid rewriting a correct include into one that resolves nowhere. That is exactly what happened to `tools/engine/sprite/tests/test_animation.c`, whose working `"types.h"` became a broken `"Types.h"` because the script's root list never modelled the sprite engine's own `include/` directory and so "corrected" it toward `Client/Packet/Types.h`. One file, reverted — but the lesson is that a mechanical sweep must be scoped to targets whose include paths it actually models.
> 2. **The CRLF verification cited was worthless.** The claim was that `git diff --numstat` equalling `git diff --ignore-cr-at-eol --numstat` proves line endings survived. It proves nothing here: `core.autocrlf=true`, so blobs store LF and git normalises the worktree side before diffing — stripping *every* CR from a committed file produces no diff from either command. The outcome is fine (a per-file census of CRLF, lone-LF, final-newline and UTF-8 BOM counts across all 179 content-changed blobs found zero anomalies, and 46 of those files carry a BOM), but the check originally offered as evidence could not have detected the damage it was supposed to rule out.
> 3. **The script had two systematic blind spots**, neither of which was stated: `../`-relative includes, which it refuses because resolving `..` changes the component count and breaks its own invariant; and a UTF-8 BOM on line 1, which defeats the `^\s*#` anchor — so two files had every include rewritten *except* the first one. Six live case-broken includes survived on that account and are now fixed by hand.
>
> **Residual reconciled (2026-09-23):** the nine commented backslash includes are removed, including obsolete `ex/` and MP3 paths. The deletion changes only complete `//` lines. Full hosted Linux and macOS compilers now verify the active include paths; the earlier Windows-only verification limitation is superseded.

#### 🟠 High -- platform_event_wait contains an unreachable branch and a hardcoded true, so manual-reset events are never honoured and already-signalled auto-reset events never clear.

**Category:** correctness  |  **Location:** `basic/PlatformSDL.cpp:218`

basic/PlatformSDL.cpp:212-244. The fast path at lines 218-225 reads `if (event->signaled) { if (!event->signaled) { event->signaled = 0; } ... return 0; }` — the inner condition is the negation of the outer one and can never be true, so an already-signalled auto-reset event is never cleared and every subsequent wait returns immediately. The slow path at line 237 reads `if (!0) { /* Auto-reset if manual_reset == 0 */ event->signaled = 0; }` — a literal that is always true, so the event is always auto-reset regardless of intent. The root cause is that `struct platform_event_s` (lines 65-69) has only mutex/cond/signaled members: the `manual_reset` argument to platform_event_create (line 194) is accepted and then discarded. Separately, the wait uses `if` rather than `while` around SDL_CondWait, so a spurious wakeup is reported as a completed wait.

**Failure scenario:** Code that creates a manual-reset event to broadcast "loading finished" to several waiters gets auto-reset behaviour: the first waiter through the slow path clears the flag and the rest block forever. Conversely a waiter that hits the fast path on an auto-reset event never consumes the signal, so its loop spins.

**Recommendation:** Store manual_reset in platform_event_s, clear `signaled` on the fast path when manual_reset is 0, replace `if (!0)` with `if (!event->manual_reset)`, and wrap the condition wait in a while loop that re-checks `signaled`.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, going one step past the recommendation in two places because following it exactly would leave the finding's own failure scenario broken.
>
> - **`platform_event_signal` now broadcasts for manual-reset events.** Storing `manual_reset` fixes the flag, but `SDL_CondSignal` still wakes exactly one waiter — so the finding's "first waiter through clears the flag and the rest block forever" becomes "the flag stays set and the rest block forever". Only a broadcast actually releases them.
> - **The timeout path uses a deadline rather than a bare `while`.** Naively wrapping `SDL_CondWaitTimeout` in a loop re-arms the full timeout on every spurious wakeup, so a 100 ms wait could block indefinitely — trading one bug for another. The remaining time is computed as an unsigned subtraction compared as signed, so it survives the ~49-day `SDL_GetTicks()` wrap.
>
> Behaviour change: an SDL failure on the infinite path now returns 1 instead of being silently ignored, and `timeout == 0` polls correctly instead of entering the wait.
>
> **Unverifiable on this machine, and the build proves nothing about it.** These functions live inside `#else /* !PLATFORM_WINDOWS */`; on Windows `platform_event_t` is a `HANDLE` and there is no definition here at all. The only in-tree callers are Win32-compatibility shims. Confirmed by reading the guard structure, not by compiling.

#### 🟠 High -- platform_get_executable_dir has two separate one-byte buffer overflows: readlink's null terminator and the trailing '/' append.

**Category:** memory-safety  |  **Location:** `basic/PlatformSDL.cpp:322`

In basic/PlatformSDL.cpp:309-337: (1) line 320-322 does `ssize_t count = readlink("/proc/self/exe", path, sizeof(path)); if (count < 0) return 1; path[count] = '\0';` — readlink may return exactly sizeof(path) (PATH_MAX), in which case `path[PATH_MAX]` writes one byte past the end of the local array. readlink does not null-terminate, so the size argument must be sizeof(path)-1. (2) line 331-335 checks `size_t len = strlen(dir); if (len + 1 > size) return 1;` and then writes `strcpy(buffer, dir); strcat(buffer, "/");` — that produces len+2 bytes including the terminator, so when len+1 == size the guard passes and the caller's buffer is overrun by one byte. Both are on a live path: basic/Directory.cpp:30-33 calls this with a 512-byte stack buffer inside C_DIRECTORY's constructor, and basic/PlatformSDL.cpp:379-380 calls it with a PATH_MAX stack buffer in get_config_file_path().

**Failure scenario:** The game is installed at a path whose executable directory is exactly 511 characters (or /proc/self/exe resolves to exactly PATH_MAX bytes). C_DIRECTORY's constructor smashes one byte of adjacent stack, or the readlink path writes a NUL one past the 4096-byte local array — either is a stack corruption with a deterministic, attacker-influenceable trigger (install path).

**Recommendation:** Use `readlink("/proc/self/exe", path, sizeof(path) - 1)` and change the length guard to `if (len + 2 > size) return 1;` (or build the string with snprintf(buffer, size, "%s/", dir) and check the return value).

> ✅ **Fixed** on branch `harden/high-severity-batch1`, exactly as recommended — but **the finding is wrong that these are live paths.** It states "Both are on a live path: `basic/Directory.cpp:30-33` … and `basic/PlatformSDL.cpp:379-380`". `Directory.cpp` does compile on Windows, but `platform_get_executable_dir` has **no Windows definition at all**; that object links today only because nothing outside `basic/` references it, which this review records separately as a Medium. The second caller sits inside the same `#ifndef PLATFORM_WINDOWS` block. So on Windows this is unreachable code calling an undefined function, and on Linux the overflows are real but the file does not compile anyway.
>
> **A third defect in the same file, not in this review and not fixable by inspection alone:** `dirname()` is used on the Linux path but `<libgen.h>` was included only for macOS and Emscripten. glibc declares `dirname` nowhere else, so this is an undeclared-identifier hard error in C++ — another reason the "Linux should work" claim has never held. Fixed with the above.

#### 🟠 High -- CMessageArray::Release deletes m_Filename without nulling it and skips the delete entirely when the log file failed to open, producing both a dangling pointer and a leak.

**Category:** memory-safety  |  **Location:** `Client/CMessageArray.cpp:158`

Client/CMessageArray.cpp:153-162 guards the whole file-log teardown with `if (m_bLog)`, then does `delete [] m_Filename;` without setting m_Filename = NULL. Two consequences. (a) Dangling pointer: after Release(), CMessageArray.h:50 `const char* GetFilename() const { return m_Filename; }` returns freed memory, and Init() only reassigns m_Filename when the new `filename` argument is non-NULL (CMessageArray.cpp:108-112) — so `Init(max, len, "a.log"); Init(max, len);` leaves GetFilename() pointing at a freed block. (b) Leak: Init() at lines 111-119 allocates m_Filename *before* attempting PLATFORM_OPEN, and only sets m_bLog = true if the open succeeded; if the open fails, m_Filename is allocated but m_bLog stays false, so Release() never frees it. Client/CGameUpdate.cpp:2191, Client/Client.cpp:2872 and Client/Client.cpp:3408 all call g_pDebugMessage->Init(...) with a log filename, so re-initialisation is a real code path.

**Failure scenario:** g_pDebugMessage->Init(MAX_DEBUGMESSAGE, 256, logFile) is called at Client.cpp:2872 and again at Client.cpp:3408. The second Init calls Release(), which frees m_Filename; any subsequent GetFilename() call reads freed heap. If the log file could not be opened the first time, the allocation simply leaks on every re-init.

**Recommendation:** Set `m_Filename = NULL` immediately after the delete, move the delete outside the `if (m_bLog)` guard so it runs whenever the pointer is non-null, and only allocate m_Filename after the file has actually opened.

> ✅ **Fixed** on branch `harden/high-severity-batch1` — the first two clauses only. **The third recommendation was deliberately refused: following it would convert a leak into a crash.** Allocating `m_Filename` only after a successful open makes `GetFilename()` return NULL after a failed open, and `Client.cpp:2901` does `strcpy(logFile, g_pDebugMessage->GetFilename())` with no NULL check. Moving the `delete[]` out of the `m_bLog` guard fixes the leak without that risk.
>
> **All three "real code path" call sites the finding cites are dead**, so this is a regression guard rather than a reproduction: `CGameUpdate.cpp:2234` sits inside a `/* */` block opened at 2227, and **both** `Client.cpp` sites (2914, 3468) are under `#ifdef OUTPUT_DEBUG`, which `DebugInfo.h` comments out and CMake never defines. The only live `Init()` calls pass no filename, so `m_Filename` is never allocated in the current build and both the leak and the dangling pointer are latent.
>
> Two details in the first version of this entry were wrong and are corrected above: it attributed one site to `__METROTECH_TEST__`, and it claimed all three were `delete`/`new` object replacement rather than re-`Init()` on a live object. The `CGameUpdate.cpp` line *is* exactly the `Init()`-on-a-live-object shape the finding describes — it is simply commented out. The conclusion (all three dead) survives; the reasoning given for it did not.
>
> Recorded separately: `Client/Client.cpp:2901` is an unbounded `strcpy` of a filename into a 128-byte stack buffer with no NULL check — dead today, but it is what makes the refused recommendation dangerous.

#### 🟠 High -- TArray::LoadFromFile reads the element count straight from a game data file into the member and allocates from it with no validation or stream-state check; nested instantiations multiply the effect.

**Category:** security  |  **Location:** `Client/framelib/TArray.h:221`

> ✅ **Fixed** in `e8f86be`. The count goes into a local, the stream state is checked, and the count is rejected when it exceeds the bytes remaining in the file. Applied to both copies of the template.

Client/framelib/TArray.h:218-232 does `file.read((char*)&m_Size, s_SIZEOF_SizeType); if (m_Size==0) return false; Init(m_Size); for (SizeType i=0; i<m_Size; i++) m_pData[i].LoadFromFile(file);` — the count is never range-checked, the stream state is never inspected, and nothing bounds the total. Because the types are nested (Client/framelib/CFrame.h:152-168 defines FRAME_ARRAY = TArray<CFrame,WORD>, DIRECTION_FRAME_ARRAY = TArray<FRAME_ARRAY,BYTE>, ACTION_FRAME_ARRAY = TArray<DIRECTION_FRAME_ARRAY,BYTE>), each inner element reads its own count, so a file declaring 255 actions x 255 directions x 65535 frames drives an unbounded allocation cascade. This is a live runtime path, not tool code: Client/EffectResourceContainer.cpp:69, :85, :101 and :113 call LoadFromFile on CEffectFramePack (which inherits this method through CFramePack, Client/framelib/CFramePack.h:25) against .efpk files, and these data files are delivered through the patcher (Client/CGameUpdate.cpp, Client/AppendPatchInfo.cpp).

**Failure scenario:** A truncated or tampered .efpk sets an inner WORD count to 65535 across many outer entries. The client either exhausts memory and dies on an uncaught std::bad_alloc, or (if SizeType were ever signed) reaches `new DataType[negative]` and throws std::bad_array_new_length. Either way the process terminates without a diagnostic.

**Recommendation:** Check `file.good()` after each read, reject counts above a per-type sane ceiling, and wrap the allocation so failure returns false instead of propagating an exception. The same fix applies verbatim to the duplicate at Client/SpriteLib/TArray.h.

#### 🟡 Medium -- basic/Directory.cpp is compiled on every platform but calls platform_get_executable_dir(), which PlatformSDL.cpp only defines on non-Windows.

**Category:** build  |  **Location:** `basic/Directory.cpp:31`

basic/CMakeLists.txt:36-42 lists Directory.cpp in BASIC_SOURCES unconditionally. C_DIRECTORY's constructor (basic/Directory.cpp:31) calls `platform_get_executable_dir(dir, sizeof(dir))`, and the definition in basic/PlatformSDL.cpp:308-337 is wrapped in `#ifndef PLATFORM_WINDOWS` with a comment asserting "nothing in the current Windows build calls either" — which Directory.cpp contradicts. It links today only because nothing outside basic/ references gC_directory or any C_DIRECTORY member, so the linker never pulls Directory.obj out of the static library. The moment any Windows code uses gC_directory the build fails with an unresolved external. The declaration is also unconditional in basic/Platform.h:1101, so there is no compile-time signal.

**Failure scenario:** A contributor uses gC_directory.GetProgramDirectory() to locate the DarkEden data folder on Windows. Directory.obj is pulled in and the link fails with LNK2019 for platform_get_executable_dir, in a file that appears to have nothing to do with the change.

**Recommendation:** Add a Windows implementation of platform_get_executable_dir (GetModuleFileNameA plus a path trim) so the function is defined everywhere it is declared, or guard Directory.cpp out of the Windows build and make that explicit in basic/CMakeLists.txt.

> ✅ **Fixed** on `feature/cpp20-bounded-format-1` (2026-09-17, the first bounded-formatting slice): `basic/Directory.{h,cpp}` are deleted rather than guarded. Nothing referenced `C_DIRECTORY` or `gC_directory`, and the class could not have linked on Windows had anything done so; `basic/PS.h` loses the include.

#### 🟡 Medium -- Platform.h defines min/max as function-like macros on non-Windows from a header that reaches nearly every translation unit, breaking std::min/std::max and numeric_limits.

> ✅ **Verified resolved (2026-09-18)**: non-Windows `min`/`max` are C++ templates outside the C-linkage block in `Platform.h`, preserving mixed arithmetic types without preprocessor substitution. The full Linux builds and tests compile this implementation.

**Category:** maintainability  |  **Location:** `basic/Platform.h:1847`

basic/Platform.h:1845-1856 defines `#define max(a, b) (((a) > (b)) ? (a) : (b))` and the matching min inside `#ifndef PLATFORM_WINDOWS`. Platform.h is included by basic/Typedef.h:17, which sits at the base of essentially every include chain in the project, so on Linux and macOS every subsequent header sees these macros. Any use of std::min/std::max, std::numeric_limits<T>::max(), or a member function named min/max in a header included afterwards fails to compile or expands into nonsense. The same header also macro-defines a number of very common identifiers unconditionally on that platform: TRANSPARENT and OPAQUE (lines 339-340), IN / OUT / OPTIONAL (lines 454-462), and `#define stricmp strcasecmp` (line 549) and `#define CloseHandle(handle)` to nothing (line 557). Note that Windows itself is normally compiled with NOMINMAX for exactly this reason.

**Failure scenario:** A contributor adds `#include <algorithm>` and a `std::max(a, b)` call to any client header on Linux. The macro rewrites it to `std::(((a) > (b)) ? (a) : (b))` and compilation fails with an error that does not point at Platform.h.

**Recommendation:** Remove the min/max macros and use std::min/std::max (or clearly-named DE_MIN/DE_MAX) at the handful of call sites that need them; rename or scope the other single-word macros.

#### 🟡 Medium -- The non-Windows wsprintf stub uses unbounded vsprintf, replacing a Win32 function whose documented contract caps output at 1024 characters.

> ✅ **Fixed (2026-09-21):** delete the POSIX shim and replace the remaining 355 narrow calls with `SafeFormat::Format`, whose array overload derives the actual destination capacity. The one pointer slice supplies its remaining capacity explicitly. The wide trace command uses capacity-bounded `swprintf` and explicit termination; unused CMP3 string-formatting overloads are removed. The caller audit removes 21 `void*` casts that would hide string arguments from the typed formatter. Existing `SafeFormat` bounds/type tests own the formatter; UI calls are regression guards under the named exemption, with full builds and source checks. R18 rejects raw identifiers, including aliases, across C/C++ sources and headers.

**Category:** memory-safety  |  **Location:** `basic/Platform.h:1871`

basic/Platform.h:1864-1877 defines `static inline int wsprintf(char* buf, const char* fmt, ...)` whose body is `int result = vsprintf(buf, fmt, args);` — with the deprecation warning explicitly suppressed by a #pragma. Real Win32 wsprintf truncates at 1024 characters, so callers ported from Windows are entitled to assume a bounded write into a 1024-byte buffer. This stub removes that bound entirely, converting every such call site on Linux/macOS into an unbounded write. Because the function is `static inline` in a header reached through Typedef.h, it silently shadows any correct implementation.

**Failure scenario:** Windows code that does `char buf[1024]; wsprintf(buf, "%s", someLongString);` is safe on Windows by the API contract but overflows buf on Linux/macOS the moment someLongString exceeds 1023 characters.

**Recommendation:** Implement the stub with vsnprintf against an explicit 1024-byte cap to match the Win32 contract, or delete it and convert call sites to snprintf with the real buffer size.

#### 🟡 Medium -- The WideCharToMultiByte stub writes to index -1 when cbMultiByte is 0 and never null-checks its output pointer.

> ✅ **Verified resolved (2026-09-18)**: `Platform.h` now measures the required UTF-8 size first, returns it without writing for a zero-capacity query, and rejects a null or undersized destination before encoding. Invalid negative sizes are also rejected. This is verification of the existing implementation, not a newly reproduced crash.

**Category:** memory-safety  |  **Location:** `basic/Platform.h:745`

basic/Platform.h:729-747. The copy loop is `for (int i = 0; i < cchWideChar && i < cbMultiByte - 1; i++)` and the terminator is written as `lpMultiByteStr[cchWideChar < cbMultiByte ? cchWideChar : cbMultiByte - 1] = '\0';`. Both use `cbMultiByte - 1`, and the standard Win32 idiom for this API is to call it once with cbMultiByte == 0 (and lpMultiByteStr == NULL) to query the required buffer size. With cbMultiByte == 0 the loop is skipped but the terminator statement evaluates `cbMultiByte - 1` as -1 and writes `lpMultiByteStr[-1]`; with lpMultiByteStr also NULL that is a null-pointer write at offset -1. There is no null check on either pointer argument. The stub also does not perform the UTF-16 to UTF-8 conversion its comment claims — line 743 copies only the low byte of each wide character, which corrupts every non-ASCII character in a client whose whole point is Korean text.

**Failure scenario:** Any caller following the standard two-call Win32 pattern (query size, allocate, convert) crashes or corrupts memory on the first call when built for Linux/macOS.

**Recommendation:** Return the required byte count and write nothing when cbMultiByte is 0 or lpMultiByteStr is NULL, and implement a real UTF-16 to UTF-8 conversion (or delegate to SDL_iconv) instead of byte truncation.

#### 🟡 Medium -- Platform.h defines assert() as a no-op evaluation on non-Windows, silently disabling every assertion in translation units that include it before <assert.h>.

> ✅ **Verified resolved (2026-09-18)**: `Platform.h` includes the real `<assert.h>` and no longer defines an assertion replacement. Non-Windows Debug assertions retain standard behavior, including normal `NDEBUG` control. The duplicate build finding below has the same resolution.

**Category:** correctness  |  **Location:** `basic/Platform.h:27`

basic/Platform.h:25-29 contains `#ifndef PLATFORM_WINDOWS / #ifndef assert / #define assert(e) ((void)(e)) / #endif / #endif`. Platform.h is pulled in by basic/Typedef.h:17, which is included essentially everywhere, so on Linux/macOS any TU that reaches Platform.h before <assert.h> gets assertions compiled out even in debug builds. Worse, the behaviour is include-order dependent: <assert.h> unconditionally #undefs and redefines assert, so a TU that happens to include it afterwards gets the real macro. The same macro therefore means different things in different TUs, which is a hazard for any inline function in a header that uses assert. This interacts badly with basic/BasicException.cpp:44-49, whose debug path is `assert(false)` — in a debug non-Windows build where the neutered macro won, `_Error(MEM_ALLOC)` prints to stderr and then returns, letting the caller proceed with the null pointer it was reporting.

**Failure scenario:** On Linux, CheckMemAlloc(p) (basic/BasicException.h:16) fires after a failed allocation, g_BasicException prints "Memory allocation failed", assert(false) expands to ((void)(false)), the function returns, and the caller immediately dereferences the null pointer.

**Recommendation:** Delete the assert shim from Platform.h and include <assert.h> where assertions are used. If a project-specific assertion is wanted, give it a distinct name (DE_ASSERT) rather than shadowing the standard macro.

#### 🟡 Medium -- platform_config_set_string appends a duplicate key on every call and never rewrites the existing one, while the getter returns the first (stale) match.

**Category:** correctness  |  **Location:** `basic/PlatformSDL.cpp:458`

> ✅ **Fixed (2026-09-18):** `platform_config_*` delegates to the real `basic/ConfigFile` implementation with an explicit path, so tests exercise it without changing the executable's settings. Updates replace the first matching key and remove old duplicates while preserving unrelated lines. Checked reads and writes, exclusive temporary files and atomic replacement preserve the existing file on failure. Files/results are capped at 1 MiB; malformed names, line injection, embedded NULs and invalid output arguments are rejected. Two tests reproduced stale values and duplicate accumulation; additional regressions cover long records, short/query buffers, limits and a refused Windows replacement. POSIX uses the same parsing/writing implementation and its native rename operation.

basic/PlatformSDL.cpp:422-462 reads the whole config file into a buffer, writes it back verbatim, and then appends `fprintf(file, "%s.%s=%s\n", key, value, data);` — the existing line for that key is never located or removed. platform_config_get_string (lines 389-420) scans with fgets and `break`s on the first line matching the search key (lines 402-415), which is the oldest entry. The net effect is that setting a configuration value has no observable effect after the first time, and the file grows by one line per write forever. Two smaller defects in the same functions: the `fread(content, 1, fileSize, file)` at line 438 does not check its return value before treating the buffer as a null-terminated string, and there is no null check on the `size` out-parameter that line 409 dereferences.

**Failure scenario:** The user changes a setting that routes through platform_config_set_string. The new value is appended after the old one; the next platform_config_get_string returns the old value, so the setting appears not to stick, and DarkEden.conf accumulates duplicate lines indefinitely.

**Recommendation:** Parse the file into key/value pairs, replace the matching entry (or append if absent), and write the whole set back; check the fread return value and the size pointer.

#### 🟡 Medium -- The Windows platform_thread_create casts a __cdecl thread function to LPTHREAD_START_ROUTINE (__stdcall), a calling-convention mismatch on 32-bit builds.

**Category:** correctness  |  **Location:** `basic/PlatformSDL.cpp:110`

> ✅ **Fixed (2026-09-18):** Windows passes a real `DWORD WINAPI` trampoline to `CreateThread`, with no function-pointer cast. Both backends retain ownership of callback data until thread creation succeeds and release it on failure; a started thread owns it through callback return. Null callbacks are rejected. Three real-thread tests guard parameter delivery, concurrent starts, the Windows exit code and invalid handles. These are regression guards: the current x64 environment cannot reproduce the original x86 stack-convention fault, and OS thread-creation failure is not injected. The recommendation to return SDL's callback exit status from `platform_thread_wait` is deliberately declined: `Platform.h` defines that result as wait success/failure, matching the Windows implementation, not the callback's exit code.

basic/PlatformSDL.cpp:109-111 does `return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, param, 0, NULL);` where `func` has type `platform_thread_func_t`, declared at basic/Platform.h:957 as `typedef DWORD (*platform_thread_func_t)(void* param);` — no calling convention, so __cdecl under the project's default settings. LPTHREAD_START_ROUTINE is `DWORD WINAPI (*)(LPVOID)`, i.e. __stdcall. On x86 Windows the callee cleans the stack under __stdcall and the caller under __cdecl, so the thread returning through the wrong convention leaves the stack pointer wrong. On x64 the two conventions are identical so the cast is harmless there, which is exactly why this can sit undetected until someone produces a Win32 build. Adjacent, less severe: the non-Windows platform_thread_create at lines 152-158 leaks its heap-allocated ThreadWrapperData when SDL_CreateThread returns NULL, and platform_thread_wait at lines 160-165 discards the status SDL_WaitThread writes and unconditionally returns 0.

**Failure scenario:** The project is configured for a Win32 (x86) target. Any thread started through platform_thread_create corrupts the stack on return, producing a crash or silent misbehaviour far from the thread function itself.

**Recommendation:** Declare platform_thread_func_t with an explicit calling convention that matches LPTHREAD_START_ROUTINE on Windows (or wrap func in a small __stdcall trampoline), free the wrapper when SDL_CreateThread fails, and propagate the wait status.

#### 🟡 Medium -- C_TIMER2 never reuses deleted timer slots — m_id_generator only increases — so the timer queue reallocs upward forever in a long session.

> ✅ **Fixed (2026-09-18):** the manager keeps only live timers in an ordered map, created lazily inside `Add` and released when the last timer is deleted. IDs remain unique for the manager lifetime, so stale handles cannot affect later timers; exhausted IDs return `INVALID_TID` instead of wrapping. `Execute` finds the next live ID after each callback, preserving creation order and safe add/delete behavior without scanning deleted history. Four tests cover 2,048 add/delete cycles with a surviving timer, stale handles, self-deletion/replacement and ID exhaustion. Two tests reproduced retained entries before the fix; the existing timing and callback tests remain the behavioral contract. Allocation failure still returns `INVALID_TID`; the global constructor allocates nothing.

**Category:** correctness  |  **Location:** `basic/Timer2.cpp:73`

basic/Timer2.cpp:73 `timer_id_t tid = m_id_generator++;` uses the monotonically increasing generator directly as the array index, and the growth check at line 50 is `if (m_id_generator >= m_timer_queue.size)`, expanding by 8 each time (line 53). Delete (lines 89-109) marks the slot `pUnit->tid = INVALID_TID` but never decrements m_id_generator and never records the slot as reusable, so an add/delete cycle consumes a fresh slot every time. Execute() (lines 119-141) then scans all m_id_generator entries every frame, skipping the dead ones, so the per-frame cost grows too. This is live: VS_UI/src/hangul/Ci.cpp:38, VS_UI/src/Vs_ui.cpp:3778 and VS_UI/src/VS_UI_Title.cpp:2973 all call gC_timer2.Add() for cursor-blink and animation timers.

**Failure scenario:** A UI element that creates a blink timer on focus and deletes it on blur is focused and blurred repeatedly during a long play session. The timer array reallocs to thousands of entries, almost all INVALID_TID, and Execute() walks all of them every frame.

**Recommendation:** Maintain a free list of released slot indices (or scan for the first INVALID_TID slot) in Add(), and track a separate high-water mark for Execute()'s loop bound rather than reusing m_id_generator for both roles.

#### 🟡 Medium -- CDataTable::LoadFromFile allocates from an unvalidated 32-bit count read from file and raw-dumps whole C++ objects to and from disk.

> ✅ **Fixed by removal (2026-09-18)**: `CDataTable.h` has no include or instantiation anywhere in the tracked C++ tree. Removed the unused raw-object serializer and its commented size-report entry. No live file format or loader changes; the reported failure scenario was not reachable in the current client.

**Category:** security  |  **Location:** `Client/CDataTable.h:153`

Client/CDataTable.h:148-170 reads `int numSize` with `file.read((char*)&numSize, 4)` and, when it differs from m_Size, calls Init(numSize). Init (lines 87-102) does `m_Size = size; m_pTypeInfo = new Type [m_Size];` with no upper bound and no rejection of negative values — a negative count converts to an enormous size_t and throws std::bad_array_new_length; a large positive count throws std::bad_alloc. Neither is caught. Two further defects in the same class: (a) Init returns early on `if (size==0) return;` at line 92 *before* calling Release(), so Init(0) leaves the previous buffer allocated and m_Size stale; (b) if `new Type[m_Size]` throws, m_Size has already been assigned while m_pTypeInfo stays NULL, and Release() only resets m_Size inside `if (m_pTypeInfo != NULL)` (line 112-119), leaving the object claiming a non-zero size over a null pointer that operator[] (line 36-38) will happily dereference. Finally, SaveToFile/LoadFromFile at lines 139 and 168 memcpy raw `Type` objects to and from disk; for any Type containing a pointer or vtable this serialises addresses and reads them back as live pointers.

**Failure scenario:** A corrupt or patched data table file carries 0xFFFFFFFF as its count. LoadFromFile calls Init, new Type[4294967295] throws, and the exception propagates out of a loader with no handler, terminating the client. Alternatively the count is plausible but Type holds a pointer, and the restored garbage address is dereferenced on first use.

**Recommendation:** Validate the count against a per-table maximum before allocating, move the size==0 early return after Release(), reset m_Size in Release() unconditionally, and replace the raw-object dump with explicit per-field serialisation.

#### 🟡 Medium -- 48 files directly under Client/ include "../../basic/Platform.h", one directory level too deep, and only resolve by accident through a subdirectory include path.

> ✅ **Fixed (2026-09-18)**: corrected all 43 remaining top-level `Client/` includes from `../../basic/Platform.h` to `../basic/Platform.h`. The paths now resolve relative to their source files. The deeper library paths remain relative to their actual locations; the duplicate TArray headers now forward to the shared basic header.

**Category:** build  |  **Location:** `Client/CPositionList.h:21`

Client/CPositionList.h:21 has `#include "../../basic/Platform.h"`, but CPositionList.h lives directly in Client/, so the relative path points at <parent-of-repo>/basic/Platform.h. The correct form is used by its sibling Client/COrderedList.h:19 (`#include "../basic/Platform.h"`). Counting across Client/'s top level: 48 files use the wrong depth (including Client/Client.cpp:49, Client/MemoryPool.cpp:21, Client/DebugLog.cpp:12, Client/CrashReport.cpp:17) against 19 that use the correct one. These compile only because the quoted-include lookup falls back to the -I search path and some entry such as Client/SpriteLib makes "../../basic" resolve to the right place from *there*. Files one level deeper (Client/framelib/TArray.h:24, Client/framelib/CFrame.h:27) legitimately use ../../, which is why the two spellings coexist unnoticed.

**Failure scenario:** An include-directory list is tidied up — a subdirectory -I entry is dropped as redundant — and 48 files stop compiling at once with a missing-header error whose path does not obviously correspond to any real location.

**Recommendation:** Normalise all Client/ top-level files to "../basic/Platform.h" (or, better, rely on the basic target's PUBLIC include directory and write <Platform.h> everywhere), so the includes state a truth rather than depending on search-path luck.

#### 🟡 Medium -- CToken owns a raw char* with no copy constructor or assignment operator, and Release() frees without nulling, so SetString(NULL) after a real string double-frees.

> ✅ **Verified resolved (2026-09-18; library coverage added 2026-09-21)**: `CToken` deletes its copy constructor/assignment and `Release()` deletes then nulls both owned/current pointers. Repeated release after `SetString(NULL)` cannot free the old allocation again. The implementation now belongs to `gamemodel`, and `test_token.cpp` links it directly. The self-aliasing reset defect was reproduced under ASan and fixed separately as described in the text/string finding above.

**Category:** memory-safety  |  **Location:** `Client/CToken.cpp:39`

Client/CToken.cpp:36-41 `Release()` does `if (m_pString!=NULL) delete [] m_pString;` and never assigns NULL. SetString (lines 46-60) calls Release() and then only reallocates `if (str!=NULL)`. So `CToken t("a,b"); t.SetString(NULL);` leaves m_pString pointing at freed memory, and the destructor's Release() (line 23) deletes it a second time. Separately, Client/CToken.h:14-53 declares a destructor that frees m_pString but no copy constructor and no copy assignment operator, so the compiler-generated shallow copies produce the same double free — passing a CToken by value or storing one in a container is enough. Today's call sites (Client/UIMessageManager.cpp:2597, :2678, :2857) happen to construct locals and never copy or reset them, so the bug is latent, but nothing in the class prevents it.

**Failure scenario:** Any future caller that reuses a CToken by calling SetString(NULL) to clear it, or copies one into a std::vector, gets a double free on the second destruction.

**Recommendation:** Set m_pString = NULL and m_pCurrent = NULL at the end of Release(), and either declare the copy constructor/assignment operator (implementing a deep copy) or delete them.

#### 🟡 Medium -- TArray::operator+= computes the combined size in the narrow SizeType, so BYTE/WORD instantiations overflow the allocation while the copy loops write the full untruncated count.

**Category:** memory-safety  |  **Location:** `Client/framelib/TArray.h:152`

> ✅ **Fixed** in `4f8435a`. The count is computed in a wider type and the append is refused when the result cannot be represented in SizeType, leaving the target unchanged. Applied to both copies of the template.

Client/framelib/TArray.h:150-190: `SizeType newSize = m_Size + array.m_Size;` then `DataType* pTempData = new DataType [newSize];` followed by two loops that copy m_Size and array.m_Size elements respectively. The addition promotes to int and is then narrowed back to SizeType on assignment, so for the BYTE instantiations at Client/framelib/CFrame.h:155 and :158 (DIRECTION_FRAME_ARRAY, ACTION_FRAME_ARRAY) a 200 + 100 append allocates 44 elements and writes 300; for the WORD instantiation at CFrame.h:152 a 40000 + 40000 append allocates 14464 and writes 80000. There is no overflow check anywhere. No caller currently uses operator+= on these types, so the defect is latent — but it is a public method on the array type used throughout framelib, and the identical bug is duplicated at Client/SpriteLib/TArray.h:151.

**Failure scenario:** A tool or future feature merges two direction arrays whose combined length exceeds 255 (or two frame arrays exceeding 65535). new DataType[truncated] returns a small buffer and the copy loops write far past its end — a heap overflow with fully attacker-influenceable content if the arrays came from a data file.

**Recommendation:** Compute the sum in a wider type (size_t), reject or clamp when it exceeds the maximum value representable by SizeType, and only then allocate.

#### 🟡 Medium -- TArray manages a raw owning pointer with a destructor but no copy constructor, so any copy-construction double-frees; operator= is also not self-assignment safe.

**Category:** memory-safety  |  **Location:** `Client/framelib/TArray.h:56`

> ✅ **Fixed** in `a2b3c8a` (copy constructor) and `1a3b32d` (self-assignment). Applied to both copies of the template, in `framelib` and `SpriteLib`, so they do not diverge. Following consolidation, `tests/unit/test_tarray.cpp` covers the shared implementation and both compatibility headers; the redundant SpriteLib-only test file was retired.

Client/framelib/TArray.h declares `~TArray()` (line 99) which deletes m_pData, and a user-defined `void operator = (const TArray&)` (line 58), but no copy constructor. The implicitly generated copy constructor performs a memberwise copy of m_Size and the raw m_pData pointer, so two TArray objects end up owning the same buffer and both delete it. Because the assignment operator is user-declared the compiler does not warn. Additionally, operator= at lines 239-249 begins with `Init( array.m_Size );`, and Init calls Release() — for self-assignment (`a = a`) this frees the source buffer and then copies the freshly default-constructed elements onto themselves, silently wiping the array. The class is used pervasively: CFramePack derives from it (Client/framelib/CFramePack.h:25) and the FRAME_ARRAY / DIRECTION_FRAME_ARRAY / ACTION_FRAME_ARRAY typedefs (Client/framelib/CFrame.h:152-168) are nested instantiations of it.

**Failure scenario:** Returning a FRAME_ARRAY by value, passing one by value, or inserting one into a std::vector copy-constructs it; both the original and the copy delete m_pData, giving a double free.

**Recommendation:** Add a deep-copying copy constructor (or delete it explicitly), add a self-assignment guard `if (this == &array) return;` at the top of operator=, and have operator= return TArray& for conventional semantics. Mirror the change in Client/SpriteLib/TArray.h.

#### 🟡 Medium -- TArray, huffman.cpp and BIT_RES.CPP each exist as two divergent copies of the same global-namespace code, and the CMakeLists comment justifying the MP3 exclusions is factually wrong.

> ✅ **Fixed (2026-09-18)**: `basic/TArray.h` is the single implementation, using the already-tested framelib body. Both former locations are compatibility includes. The existing TArray suite includes the shared header and both public paths; two redundant SpriteLib-only ownership tests are removed because they exercised the same body twice. The MP3 duplicates and misleading exclusions had already been deleted, as recorded below.

**Category:** maintainability  |  **Location:** `Client/framelib/TArray.h:1`

Client/framelib/TArray.h and Client/SpriteLib/TArray.h define the same global template `TArray<DataType, SizeType>` with divergent bodies — the framelib copy has `using namespace std;` at line 27 and the SpriteLib copy does not, their include paths differ (line 24 vs line 24), and their operator+= loop variable scoping differs. Every bug fixed in one silently persists in the other (all the TArray defects reported here apply to both). Client/huffman.cpp and Client/DXLib/huffman.cpp are byte-identical apart from their includes, as are Client/BIT_RES.CPP and Client/DXLib/BIT_RES.CPP; both huffman copies define the global `ht[HTN]` table. Compounding this, CMakeLists.txt:640-670 excludes Client/mp3.cpp, reader.cpp, synfilt.cpp, subdecoder.cpp, header.cpp and soundbuf.cpp on the stated grounds that they are "already compiled into the dxlib library that DarkEden links" — but Client/DXLib/CMakeLists.txt:51-67 (DXLIB_SOURCES) lists none of those files. The result is that the entire MP3 decoder is dead in both trees, while Client/huffman.cpp and Client/BIT_RES.CPP are still globbed in and compiled into the executable with no caller.

**Failure scenario:** A contributor fixes a TArray bounds bug in framelib and ships it; the SpriteLib sprite-index path still carries the bug. Or a contributor re-enables MP3 playback on the strength of the CMakeLists comment and discovers at link time that none of the decoder objects exist.

**Recommendation:** Collapse each pair to a single shared header/source (one TArray.h in a common include directory, one huffman/bit-reserve pair), then correct the CMakeLists comment to say the MP3 decoder is currently unbuilt rather than claiming it lives in dxlib.

> ⚠ **Partly resolved** in `66d8637` (branch `harden/audio-media`): the huffman/BIT_RES pairs, the rest of the MP3 decoder, and the false CMakeLists comments are deleted (see the dead-decoder finding in Input, Audio & Media). The two divergent TArray copies remain open.

#### 🟡 Medium -- MemoryPool::Alloc ignores the requested allocation size and the class operator new overrides discard their size_t parameter, so any new subclass silently gets a block sized for its base.

> ✅ **Fixed** on branch `fix/review-memory-pool` (2026-09-18). The allocator's six function bodies first moved unchanged into `basic`; its four creature-specific instances stay executable-side. All four class allocation overrides now forward the requested size and provide aligned allocation/deallocation overloads. Nominal objects use aligned chunks with checked stride/count arithmetic and exact slot-state tracking; larger or over-aligned objects use separately owned allocations with the matching aligned delete. Null frees are harmless, and foreign, interior and already released pointers are rejected before touching storage. Ten direct tests cover mixed sizes, alignment, overflow, membership, repeated release and reuse. Three of the tests reproduced the previous allocator's misalignment, neighbor-membership error and duplicate allocation after a double free; requested-size cases are regression guards for the new API. Full Debug/ASan builds and all nine CTests pass, as does the focused Linux ASan/UBSan suite.

**Category:** memory-safety  |  **Location:** `Client/MemoryPool.cpp:56`

Client/MemoryPool.cpp:56-94 `void* MemoryPool::Alloc()` takes no size parameter and always carves a fixed m_BlockSize block. The class-level overrides all discard the size the compiler passes them: Client/MCreature.h:115-118 `void* operator new( size_t size ) { return g_CreatureMemoryPool.Alloc(); }`, and identically Client/MCreatureWear.h:40-43, Client/MNPC.h:20-23, Client/MFakeCreature.h:133-136. The pools are constructed with sizeof(MCreature), sizeof(MCreatureWear), sizeof(MNPC) and sizeof(MFakeCreature) respectively (Client/MemoryPool.cpp:30-33). The hierarchy is currently covered — MCreature has a virtual destructor (Client/MCreature.h:113) so the deleting destructor routes to the right operator delete, and MPlayer (Client/MPlayer.h:232-240) was explicitly special-cased to use ::operator new — but any future class deriving from MCreatureWear that forgets to add its own pair inherits MCreatureWear::operator new and receives a buffer sized for the base with no diagnostic. Related: MemoryPool::Free (lines 96-105) performs no membership check and no double-free detection; it simply threads the block onto the free list, so a double delete makes two subsequent Alloc() calls return the same address.

**Failure scenario:** A contributor adds `class MSummonedPet : public MCreatureWear` with extra members and no operator new override. Every `new MSummonedPet` returns sizeof(MCreatureWear) bytes; constructing the derived members writes past the end of the pool block into the next object.

**Recommendation:** Give Alloc a size_t parameter and assert (or fall back to ::operator new) when size > m_BlockSize; have Free call IsPtrInPool in debug builds before threading a block onto the free list.

#### ⚪ Low -- ColorDraw::Convert565to555 discards the blue channel entirely instead of preserving it.

**Category:** correctness  |  **Location:** `basic/ColorDraw.h:67`

> ✅ **Fixed** in `65a2413`. Blue is now carried across untouched; the mask was losing it. Covered by `tests/unit/test_colordraw.cpp`, including a lossless round trip through Convert555to565.

basic/ColorDraw.h:65-68 implements the 5:6:5 to 5:5:5 conversion as `return (pixel & 0xFFE0) >> 1;`. Masking to 0xFFE0 keeps bits 15..5 and the shift moves them to 14..4, so the low five bits of the result come from the original green channel's upper bits (original bits 9..5) and the original blue channel (bits 4..0) is dropped completely. The correct expression preserves blue separately, e.g. `((pixel & 0xFFC0) >> 1) | (pixel & 0x1F)`. The sibling Convert555to565 at line 62 does handle blue correctly (`((pixel & 0x7FE0) << 1) | (pixel & 0x001F)`), which makes the asymmetry look accidental rather than intended. These are static inline helpers in a header included via basic/Platform.h's dependency chain, so any pixel path that converts down to 555 is affected.

**Failure scenario:** Any surface converted from the SDL 5:6:5 backend format down to 5:5:5 renders with blue replaced by a shifted copy of green — reddish/greenish tinting rather than an obvious failure, which is why it can survive unnoticed.

**Recommendation:** Change the body to `((pixel & 0xFFC0) >> 1) | (pixel & 0x1F)` and add a round-trip unit test against Convert555to565.

#### ⚪ Low -- GL_import.h declares roughly twenty functions and eight function pointers that have no implementation anywhere in the tree; the pointers are initialised to NULL and never assigned.

> ✅ **Fixed (2026-09-18)**: removed the unbuilt GL import source and header, the unimplemented TGA/IMG image interfaces and their unused picture-info type. VS_UI no longer includes the import header. Removed the abandoned GL drawing comments and the disabled standalone UI debug block; a token comparison preserves the live statements in the six files whose edits are comment-only. The working SDL `SetSurfaceInfo` helper remains. The separate no-op rendering functions in `Client/RenderingFunctions.cpp` are not DLL imports and remain outside this removal.

**Category:** dead-code  |  **Location:** `basic/GL_import.h:10`

basic/GL_import.h:10-51 declares FillRect, SetSurfaceInfo, Get_ColorkeyColor, GL_RGB, InitializeGL, Convert24RGBto16, rectangle, line, TransparentBlt16, getPixel16, TestTga and GetTgaPicInfo, plus the function pointers Bltz, CkBltz, filledRect, cls, putPixel, getPixel, vertline and horzline. A repo-wide search finds no definition for any of the functions, and basic/GL_import.cpp:15-22 defines the eight pointers as NULL under `#ifndef PLATFORM_WINDOWS` with no assignment anywhere — on Windows they are only declared, as __declspec(dllimport) from a DLL that no longer exists. The current call sites in VS_UI/src/VS_UI_Description.cpp (lines 90, 99, 1934, 1943, and others) are all commented out. basic/TGA.h:52-98 is the same situation: class Tga declares Load, Bltz, CkBltz and GetSurfaceInfo with no .cpp anywhere (git ls-files shows only basic/TGA.h), and its one caller at VS_UI/src/VS_UI_GlobalResource.cpp:187 is inside a commented-out function. basic/Platform.h:1996-1998 already acknowledges part of this in a comment.

**Failure scenario:** A contributor uncomments one of the filledRect or TransparentBlt16 calls in VS_UI_Description.cpp to restore a UI effect. On Linux the call goes through a NULL function pointer and crashes immediately; on Windows it fails to link against a DLL import that does not exist.

**Recommendation:** Delete basic/GL_import.h, basic/GL_import.cpp and basic/TGA.h along with the commented-out call sites, or, if the drawing helpers are still wanted, implement them over the SDL surface backend that already provides equivalent operations.

#### ⚪ Low -- CMessageArray::operator[] truncates the ring-buffer offset into a BYTE and performs no bounds check on its index argument.

**Category:** correctness  |  **Location:** `Client/CMessageArray.cpp:424`

> ✅ **Fixed (2026-09-18):** ring offsets remain integers, use overflow-safe wrap arithmetic and reject indices outside `[0, GetSize())`. A 300-row ring filled with 650 numbered messages reproduced wrong ordering before the fix; the end-index test reproduced returning a live row instead of rejecting the index. Both now pass alongside empty/released-ring and extreme-index regression guards in `test_message_array.cpp`.

Client/CMessageArray.cpp:424 computes `BYTE gap = m_Max - i;` where m_Max is an int member. For any array larger than 255 entries the subtraction wraps modulo 256 and the subsequent branch selection at lines 426-435 returns an unrelated message. The index `i` is never validated against [0, m_Max), and m_ppMessage is never checked for NULL, so calling operator[] before Init() dereferences a null pointer. The computed indices happen to stay inside [0, m_Max) given the branch conditions, so this is a wrong-data bug rather than an out-of-bounds one. It is latent today because every instantiation is small — Client/Client.h:84-88 defines MAX_DEBUGMESSAGE as 25 and the other four as 5, and Client/GameInit.cpp:1611-1628 uses those — but nothing in the class documents or enforces the 255 ceiling.

**Failure scenario:** Someone raises MAX_DEBUGMESSAGE to 300 to keep more log history. The debug overlay silently starts showing the wrong lines in the wrong order, with no crash to point at the cause.

**Recommendation:** Change gap to int, add `if (i < 0 || i >= m_Max || m_ppMessage == NULL) return "";` at the top, and either document the ring-buffer indexing or replace it with a straightforward modulo expression.

#### ⚪ Low -- CPositionList::SaveToFile truncates the element count to 16 bits and LoadFromFile restores a list without re-establishing the sorted invariant Add/Remove depend on.

**Category:** correctness  |  **Location:** `Client/CPositionList.h:262`

> ✅ **Fixed (2026-09-17):** the generic lists now live in `basic`, with forwarding headers for existing consumers. Position writes reject more than 65,535 entries before emitting bytes, preserving the WORD-count format; reads check every field into a temporary list, then sort/deduplicate and publish only complete input. Both containers expose `GetEnd()`, and position comparisons are const-correct for C++20. Five tests cover unsorted/duplicate input, every truncation boundary, the count limit and overflow, successive records, and end iterators. Three tests reproduced the old failures before the fix.

Client/CPositionList.h:262 does `WORD size = m_listPosition.size();` and writes two bytes — a list longer than 65535 entries is written with a wrong count, silently corrupting the file. LoadFromFile (lines 295-324) reads the WORD count and then loops `file.read` into a single reused `node` without ever checking the stream state, so a truncated file pushes the previous node's values repeatedly. More importantly, it push_backs whatever order the file supplies, while Add (lines 183-201) and Remove (lines 227-244) both rely on the list being sorted ascending — Remove returns false as soon as it sees an element greater than the target (line 238). A file whose entries are not in sorted order therefore makes Remove silently fail to remove present entries. Same structure applies to COrderedList (Client/COrderedList.h:114-180), which additionally exposes GetIterator() at line 62 with no matching end accessor, so callers such as Client/Client.cpp:2425 must iterate exactly GetSize() times and any mismatch walks off the list.

**Failure scenario:** A hand-edited or older-format position file lists coordinates out of order. Loading succeeds, but subsequent Remove(x, y) calls for entries that are actually present return false, so stale positions accumulate.

**Recommendation:** Write the count as a 32-bit value (or bound the list), check file.good() inside the read loop, and route loaded nodes through Add() so the sorted/unique invariant is re-established. Add a GetEnd() accessor to both COrderedList and CPositionList.

#### ⚪ Low -- huffman_decoder bounds its tree walk against ht->treelen (table 0, which is 0) instead of h->treelen, and ValTab24 contains an offset equal to MXOFF that jumps past the end of the table.

**Category:** memory-safety  |  **Location:** `Client/huffman.cpp:432`

Client/huffman.cpp:415-432 walks the decoder tree with `point` and terminates on `} while (level || ((unsigned int)point < ht->treelen) );` — `ht` is the global table array from line 357, so `ht->treelen` is ht[0].treelen, which is 0 for the dummy table. The intended bound is `h->treelen`. The only remaining limit is `level` shifting right, and nothing checks `point` against the size of `h->val` before `h->val[point][0]` is read at line 416. Concretely: ValTab24 is declared `[512][2]` (line 287) and its entry at index 310 is `{85,250}` (line 319). MXOFF is 250 (Client/huffman.h:14), so the chaining loop at line 424, `while (h->val[point][1] >= MXOFF) point += h->val[point][1];`, takes 250 >= 250 and sets point to 560 — 49 entries past the end of a 512-entry array — and then keeps reading. The identical code and table exist at Client/DXLib/huffman.cpp:314 and :427. Severity is held at low only because the decoder is currently unreachable: its caller subdecoder.cpp is excluded from the client build (CMakeLists.txt:663) and Client/DXLib/subdecoder.cpp is not in DXLIB_SOURCES, so nothing calls huffman_decoder even though Client/huffman.cpp is still compiled into the executable.

**Failure scenario:** If MP3 playback is re-enabled, an .mp3 whose bitstream reaches ValTab24 index 310 with a set bit drives point to 560 and the decoder reads past the end of the static table — an out-of-bounds read whose extent is controlled by whatever bytes follow the array.

**Recommendation:** Change the loop condition to use h->treelen, and add an explicit `if (point >= (int)h->treelen) return 1;` guard inside the chaining loops at lines 424 and 428 before indexing. If MP3 support is not coming back, delete both copies rather than leaving them compiled.

> ✅ **Resolved by removal** in `66d8637` (branch `harden/audio-media`). MP3 support is not coming back — SDL2_mixer decodes MP3/OGG — so both copies of the whole decoder family are deleted rather than patched.

---

## Build, Portability & Hygiene

**Grade:** D  |  **Findings:** 24

**Scope:** Build system, portability & repository hygiene (CMakeLists.txt, Makefile, compile flags, platform ifdef hygiene, copy-protection remnants, tools/, dead files, 64-bit cleanliness)

**Health assessment:** The build works on exactly one path — Windows + VS2022 + vcpkg — and only because the case-insensitive filesystem and MSVC's tolerance paper over a lot. Underneath, the CMake logic is not doing what its own comments say: the `list(REMOVE_ITEM)` that is supposed to stop ~35 Client translation units being compiled twice is a silent no-op (relative vs. absolute paths), several `list(FILTER)` exclusions are anchored `^Client/` against absolute paths and never match, and the resulting double-compilation combines with per-target macro divergence (`_LIB`, `__WIN32__`, `__WINDOWS__` on one target but not the other) to produce genuine ODR violations on inline functions that ship in the same link. Portability claims in CLAUDE.md/README are not met: Linux cannot even configure, because six sources are tracked as `.CPP` while CMake names two of them lowercase. The safety net is absent — `USE_ASAN` is a documented no-op on the documented toolchain, and no target except `basic` sets a single warning flag, in a codebase full of `sprintf`/`strcpy`/pointer-truncation. Worst of all, a legacy anti-cheat probe is still wired into the main frame loop, where it leaks module handles every frame and can call `ExitProcess(0)` on a coin flip. Repository hygiene compounds the cost: a 3.4 MB `compile_commands.json` full of another developer's macOS paths, a 320 KB mojibake `filelist.txt`, 97 Korean-named files that the build file itself cites as documentation, and duplicated doc trees.

#### 🔴 Critical -- The legacy anti-cheat probe runs every frame, reads the wrong byte, and can terminate the process at random.

**Category:** correctness  |  **Location:** `Client/APICheck.cpp:126`

`_APICheck.CheckApi()` is called from inside the `while (TRUE)` message loop at Client/Client.cpp:4155 (initialised at Client.cpp:3996), so this whole routine executes once per frame on Windows. Line 126 does `g_ppProceAddress[i] = (DWORD)GetProcAddress(LoadLibrary(g_szCheckDLL[i*2]), ...)` for winmm/User32/kernel32. Three problems compound: (1) `g_ppProceAddress` is declared `DWORD[3]` (APICheck.h:42), so a 64-bit `FARPROC` is truncated to 32 bits on x64 — the stored value is not the address at all; (2) line 127 `memcpy(&code, &g_ppProceAddress[i], 1)` copies the low byte of the *stored address value*, not the opcode byte at that address, so the intended hook detection at line 128 (`code == 0xB9 || code == 0xE9`) can never detect anything; (3) when that low byte does happen to equal 0xB9 or 0xE9, line 130 calls `::ExitProcess(0)` — the game vanishes at startup with no message, no log, and no reproducibility, since the value depends on where the OS loaded the DLL. Additionally each frame calls `LoadLibrary` three times with no matching `FreeLibrary`, taking the loader lock 180x/second and leaking module reference counts for the life of the process. Lines 98-113 compile a hardcoded 37-byte x86 opcode signature of the 32-bit `send` prologue that cannot match on x64, and on a false match pops `MessageBox(0, "", "", MB_OK)` (an empty dialog) before `ExitProcess(0)`.

**Recommendation:** Delete APICheck entirely — it is 2006-era WPE-blocking security theatre that cannot work on x64 and provides no protection against any modern tool. Remove `Client/APICheck.cpp`/`.h`, the `APICheck _APICheck;` global at Client/Client.cpp:6, and the call sites at Client.cpp:3996 and 4155. If some form of tamper check is genuinely wanted later, it belongs behind an explicit opt-in build option, not unconditionally in the frame loop.

> ✅ **Fixed** in `c2f65b7` (branch `harden/text-format`). Both sources, the global, and both call sites are gone, along with the stale entries in the committed legacy `Client.vcxproj.filters`. `APICheck.h` declared nothing but the class — no macro or typedef anything else used — so nothing had to be rehomed. Because `CMakeLists.txt` globs sources without `CONFIGURE_DEPENDS`, an existing build tree keeps compiling the deleted file until it is reconfigured.

#### 🟠 High -- PLATFORM_USE_SDL and DXLIB_USE_SDL_BACKEND are set with directory-scoped add_definitions(), so libraries and their consumers see different versions of the same headers.

> ✅ **Fixed (2026-09-18):** `basic` now publishes `PLATFORM_USE_SDL` through its CMake link interface, completing the earlier `dxlib` change for `DXLIB_USE_SDL_BACKEND`. Ordinary consumers receive the same backend selection as each library. `test_build_contract.cpp` compiles without source-specific defines and checks both interfaces; its SDL assertion failed before this change.

**Category:** build  |  **Location:** `basic/CMakeLists.txt:16`

basic/CMakeLists.txt:16 uses `add_definitions(-DPLATFORM_USE_SDL)` and Client/DXLib/CMakeLists.txt:16 uses `add_definitions(-DDXLIB_USE_SDL_BACKEND)`. `add_definitions()` applies to the current directory and below only — it does not propagate to consumers the way `target_compile_definitions(... PUBLIC ...)` does. Both macros are load-bearing inside public headers. basic/Platform.h:63-70 gates `#include <SDL.h>` (`<SDL2/SDL.h>` until the macOS port; `basic/Platform.h` says why) on `PLATFORM_USE_SDL`, so on Windows the TUs in `basic/` see the SDL declarations and every other TU in the project (which reaches Platform.h through Typedef.h and Client_PCH.h) does not. Client/DXLib/DXLibBackend.h:27-31 picks `DXLIB_BACKEND_WINDOWS` when `DXLIB_USE_SDL_BACKEND` is absent and `DXLIB_BACKEND_SDL` when present, so on Windows the dxlib library compiles its adapters under `DXLIB_BACKEND_SDL` (CDirectInput_Adapter.cpp:26, CDirectSound_Adapter.cpp:22, CDirectMusic_Adapter.cpp:22, CDirectSoundStream_Adapter.cpp:19 are all wrapped in `#ifdef DXLIB_BACKEND_SDL`) while every caller in Client/ and VS_UI/ compiles the same header believing the backend is native Windows. The top-level FORCEd `USE_SDL_BACKEND` (CMakeLists.txt:54) is a *different* macro and does not fix this.

**Recommendation:** Replace both `add_definitions()` calls with `target_compile_definitions(<tgt> PUBLIC ...)` so the macro travels with the target through `target_link_libraries`. Since USE_SDL_BACKEND is FORCEd ON for all platforms at CMakeLists.txt:54, the cleanest end state is to delete the `#ifdef` branching from Platform.h and DXLibBackend.h entirely and keep only the SDL path.

#### 🟠 High -- APICheck dereferences LoadLibrary/GetProcAddress results without any NULL check, in code that runs every frame.

**Category:** memory-safety  |  **Location:** `Client/APICheck.cpp:31`

`APICheck::GetWsAddr()` (lines 31-35) calls `m_hws32 = LoadLibrary("WS2_32.dll")` and immediately `GetProcAddress(m_hws32, "send")` / `GetProcAddress(m_hws32, "recv")` with no check on either result, then does `memcpy(&m_bSaveSend[0], m_hsend, 5)` and `memcpy(&m_bSaveRecv[0], m_hrecv, 5)`. If WS2_32.dll fails to load or either export is not found, these are 5-byte reads from address 0 — an immediate access violation during startup (`init()` is called from Client/Client.cpp:3996). The same unguarded pattern repeats in `CheckApi()` at lines 53 and 73 (`memcpy(&btemp[0], m_hsend, 1)`), which run once per frame from Client.cpp:4155, so a single failed resolve turns into a crash on the very next frame rather than a graceful startup error. Note lines 96-98 *do* guard with `if (m_hsend != NULL)` before a 37-byte read, which shows the author was aware of the risk and applied it inconsistently. The casts at lines 32/34 (`(FARPROC&)m_hsend = GetProcAddress(...)`) are also type-punned reference casts through incompatible function-pointer types, which is undefined behaviour independent of the NULL issue.

**Recommendation:** Removing APICheck (see the critical finding above) resolves this. If any part is retained, check every `LoadLibrary`/`GetProcAddress` result before use, drop the `(FARPROC&)` reference casts in favour of an explicit `reinterpret_cast` of the returned value, and pair every `LoadLibrary` with a `FreeLibrary`.

> ✅ **Fixed** in `c2f65b7` (branch `harden/text-format`), by the first branch of its own recommendation: `Client/APICheck.cpp` and `Client/APICheck.h` were deleted outright along with the global, the `init()` call and the per-frame `CheckApi()` call in `Client/Client.cpp`. Nothing in the tree references `APICheck` today. The unguarded `LoadLibrary`/`GetProcAddress` results, the type-punned `(FARPROC&)` casts and the never-paired `FreeLibrary` all went with it; no part was retained, so none of the fallback advice applies.

#### 🟠 High -- USE_ASAN/USE_TSAN/USE_UBSAN are silently ignored on MSVC, so the documented primary dev command produces an unsanitized build.

> ✅ **Verified resolved (2026-09-18)**: CMake enables MSVC AddressSanitizer explicitly, checks its incompatible Debug runtime flags, and rejects unsupported MSVC TSan/UBSan requests at configure time. Separate full `/RTC1` and `/fsanitize=address` Windows builds and CTest runs verify the actual configurations; an ASan option no longer silently produces an ordinary MSVC build.

**Category:** build  |  **Location:** `CMakeLists.txt:28`

The sanitizer block (lines 27-51) is entirely wrapped in `if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")`. MSVC reports `MSVC`, so on the Windows/VS2022 toolchain that README.md:9 and :84 document as *the* build, `-DUSE_ASAN=ON` sets no compile flags, no linker flags, and — because the `message(STATUS ...)` at line 49 is inside the same guard — prints nothing at all. There is no `else()` branch warning the user. CLAUDE.md states "For development, the most commonly used one is `make debug-asan`", and Makefile:57-60 implements that as `-DUSE_ASAN=ON`. A contributor on Windows therefore believes they are running under AddressSanitizer while running a plain Debug build, and concludes the codebase is memory-clean when ASan simply never ran. MSVC has supported `/fsanitize=address` since VS2019 16.9, so this is a gap in the CMake logic, not a toolchain limitation. Two secondary issues in the same block: enabling USE_ASAN and USE_TSAN together concatenates `-fsanitize=address -fsanitize=thread`, which the compiler rejects; and USE_UBSAN omits `-fno-sanitize-recover`, so UBSan findings are logged and execution continues, which is easy to miss in a game's console spam.

**Recommendation:** Add an MSVC branch mapping USE_ASAN to `/fsanitize=address` (and note that MSVC ASan is incompatible with `/RTC1`, which CMake's default Debug flags include, so strip it). Add an `else()` that emits `message(WARNING "USE_ASAN requested but not supported for ${CMAKE_CXX_COMPILER_ID} - building without sanitizers")` so the no-op is never silent. Reject USE_ASAN+USE_TSAN together with `message(FATAL_ERROR)`.

#### 🟠 High -- __WIN32__ and __WINDOWS__ are defined only on the DarkEden target, so shared Packet headers mean different things in VS_UI than in the executable.

> ✅ **Fixed (2026-09-18):** `packetwire` now publishes the Windows wire macros through its CMake link interface. They reach `gamemodel`, `VS_UI`, the executable and tests transitively. Removed the duplicate target definitions and manual test-source macro list; only actual fixture paths remain source-specific. The ordinary consumer in `test_build_contract.cpp` failed its Windows-definition assertion before the fix, and generated projects are checked for the inherited definitions.

**Category:** portability  |  **Location:** `CMakeLists.txt:856`

Line 856 adds `__WIN32__ __WINDOWS__` to DarkEden only. The long comment at lines 843-855 correctly explains why they are needed — `Client/Packet/Exception.h`, `Assert1.h` and `SocketAPI.cpp` branch on bare `#if __WINDOWS__` / `#elif __WINDOWS__`, where an undefined macro reads as 0 and silently compiles neither path — but the fix was applied to one target out of six. The headers in question are pulled into VS_UI too, via the Client sources listed in VS_UI_CLIENT_SOURCES. Concretely, in VS_UI TUs on Windows: Client/Packet/Assert1.h:23-31 matches none of its branches (`NDEBUG` unset in Debug, `__LINUX__`/`__WIN_CONSOLE__`/`__WIN32__`/`__MFC__` all unset), leaving `Assert` completely undefined; Client/Packet/Types/SystemTypes.h:61-67 declares neither `separatorChar` nor `separator` because it tests `__LINUX__`, `PLATFORM_MACOS` and `__WINDOWS__` and none is set; Client/Packet/Datagram.h:16-22 includes neither `<sys/socket.h>` nor `<winsock.h>`. Confirmed against the generated projects: DarkEden.vcxproj carries `__WIN32__;__WINDOWS__`, VS_UI.vcxproj does not. Today this survives only because no VS_UI-compiled source happens to use `Assert()` or `separator`; the first one that does gets an error whose cause is three files away.

**Recommendation:** Hoist the platform macros to a single place that every target inherits — either an INTERFACE library that all targets link, or `add_compile_definitions()` at the top level guarded by `if(WIN32)`. Then add `-Wundef` / `/w14668` so a bare `#if __WINDOWS__` against an undefined macro is reported rather than silently taken as 0.

#### 🟠 High -- list(REMOVE_ITEM) meant to de-duplicate ~35 Client sources is a silent no-op, so they are compiled into both VS_UI.lib and DarkEden.exe.

**Category:** build  |  **Location:** `CMakeLists.txt:603`

Line 602's comment reads "Remove files that are already compiled in VS_UI library", and line 604 passes `${VS_UI_CLIENT_SOURCES}` to `list(REMOVE_ITEM CLIENT_MAIN_SOURCES ...)`. But `VS_UI_CLIENT_SOURCES` (lines 213-283) holds *relative* paths like `Client/MItem.cpp`, while `CLIENT_MAIN_SOURCES` was produced by `file(GLOB ...)` at line 583, which yields *absolute* paths. `REMOVE_ITEM` is an exact string match, so nothing is removed. The author clearly knew this — every explicit removal immediately below (lines 606-620) is written with a `${CMAKE_CURRENT_SOURCE_DIR}/` prefix — but the bulk removal was never updated. Verified against the generated build tree: `build/vs2022/DarkEden.vcxproj` and `build/vs2022/VS_UI.vcxproj` each contain MItem.cpp, MZone.cpp, MPlayer.cpp, MInventory.cpp, MStorage.cpp, MParty.cpp and MShop.cpp. Consequences: ~35 large translation units are compiled twice on every clean build, and — worse — the two copies are compiled with *different* preprocessor state (see the `_LIB` finding), so the archive members and the EXE objects are not interchangeable. Today the linker resolves from the EXE objects and never pulls the lib members, but any change that causes a VS_UI-internal reference to pull one in produces LNK2005 duplicate-symbol errors that will look inexplicable.

**Recommendation:** Prefix the entries of VS_UI_CLIENT_SOURCES with `${CMAKE_CURRENT_SOURCE_DIR}/` (or build a second absolute-path list for the removal), then rebuild clean and confirm each of those files appears in exactly one .vcxproj. A `foreach`/`list(APPEND)` loop that absolutises the list in one place is less error-prone than editing 35 lines.

> ✅ **Fixed 2026-09-02** on `restructuring/vsui-single-compile` (`docs/RESTRUCTURING.md` task 4.0), the other way round from the recommendation: since the executable's own objects were the ones the linker used all along (its objects are always linked; library members only resolve what is still undefined), the `VS_UI_CLIENT_SOURCES` list was deleted and the 36 files compile once, into the executable. A linker map before and after — symbol, defining object, order and address — is identical apart from timestamps. The `_LIB` ODR finding below is unchanged by this and stays open.
>
> **Still open, but re-measured 2026-09-01 against the generated tree.** The count is **43**, not ~35: `DarkEden.vcxproj` lists 1044 translation units and `VS_UI.vcxproj` 97, and 43 **full paths** appear in both — the `MItem`/`MZone`/`MPlayer` group the finding named, plus `Client.cpp`, `MGameStringTable.cpp` and `MCreatureTable.cpp`.
>
> A first pass at this measurement said 46 by comparing **basenames**, which is wrong in a way worth recording because it is easy to repeat: three basenames collide between *different* files — `Client_PCH.cpp`, `MitemTableInit.cpp` and `SXml.cpp` each exist once under `Client/` and once under `VS_UI/`. Those are two distinct sources, not one compiled twice, and folding them in inflates exactly the number the finding is about. Compare full paths.
>
> The mechanism is as the finding describes (relative paths in `VS_UI_CLIENT_SOURCES` versus absolute ones from `file(GLOB)`, so `list(REMOVE_ITEM)` matches nothing). This is the precondition for the `_LIB` ODR finding below, so the two want fixing together and in that order.

#### 🟠 High -- _LIB is defined PRIVATE on VS_UI but not on DarkEden, giving inline functions in VS_UI headers two different bodies in the same link (ODR violation).

**Category:** correctness  |  **Location:** `CMakeLists.txt:371`

> ✅ **Fixed 2026-09-02** on `restructuring/vsui-lib-define-public` (`docs/RESTRUCTURING.md` task 4.0, second slice): the definition is `PUBLIC` on the `VS_UI` target, so the executable compiles its 72 VS_UI-including translation units (50 under `Client/`, 22 packet handlers) with `_LIB` too and sees the library layout. The recommendation's first option; moving the `#ifndef _LIB` blocks out of the headers would remove the macro's power to fork layouts at all and stays open as the better long-term shape.

Line 371 sets `target_compile_definitions(VS_UI PRIVATE _LIB)`; the DarkEden target never gets it. Confirmed in the generated projects: VS_UI.vcxproj defines `_LIB;SPRITELIB_BACKEND_SDL;SPRITESURFACE_STANDALONE` while DarkEden.vcxproj defines `USE_SDL_BACKEND;SPRITELIB_BACKEND_SDL;__GAME_CLIENT__=1;__WIN32__;__WINDOWS__;SPRITESURFACE_STANDALONE` — no `_LIB`. `_LIB` is not an internal detail; it gates code inside shared *headers*: VS_UI/src/header/VS_UI_widget.h:300-320 wraps the body of the inline member `ButtonGroup::MouseControl` so that the `g_GetCtrlPushState()` drag-handling branch and the function-local `static bool press; static int gapx, gapy;` exist only when `_LIB` is undefined. VS_UI/src/header/VS_UI_Base.h:324-326 similarly adds/removes the inline member `Base::GetMessageSize()`. Because the duplicate-compilation bug above puts Client/MPlayer.cpp, MZone.cpp, MSkillManager.cpp etc. in *both* targets, and DarkEden-only files such as Client/GameUI.cpp and Client/MTopView.cpp include these same headers, one link contains two different definitions of the same inline function. That is a hard ODR violation: the linker keeps one COMDAT arbitrarily and every caller silently gets it, so which UI drag behaviour you ship depends on link order rather than on any source-level decision.

**Recommendation:** Stop letting `_LIB` change header content. Either define it uniformly for every target that includes VS_UI headers (`PUBLIC`/`INTERFACE` on the VS_UI target rather than `PRIVATE`), or — better — move the `#ifndef _LIB` blocks out of headers and into the .cpp files so the header has a single definition regardless of who compiles it. Fixing the REMOVE_ITEM bug first shrinks the blast radius but does not eliminate it, since DarkEden-only TUs still include these headers.

#### 🟠 High -- The Linux build cannot configure: six sources are tracked with a .CPP extension while CMake names two of them lowercase and globs for *.cpp.

**Category:** portability  |  **Location:** `CMakeLists.txt:277`

Six tracked sources use an uppercase extension: Client/BIT_RES.CPP, Client/COGGSTREAM.CPP, Client/MAttachZoneAroundEffectGenerator.CPP, Client/MBloodyBreakerEffectGenerator.CPP, Client/MTimeItemManager.CPP, Client/MWarManager.CPP, plus VS_UI/src/VS_UI_TITLE_SHOWCHAR.CPP. CMakeLists.txt names two of them explicitly with a lowercase extension — line 277 `Client/MTimeItemManager.cpp` and line 593 `Client/COGGSTREAM.cpp` — and every other one is picked up only by `file(GLOB ... Client/*.cpp)` (line 583) or `file(GLOB_RECURSE VS_UI/src/*.cpp)` (line 171). CMake's globbing is case-insensitive on Windows and macOS but case-sensitive on Linux. So on Windows/macOS everything resolves (confirmed: the checked-in macOS compile_commands.json contains all six .CPP files plus the lowercase MTimeItemManager entry), while on Linux the two explicit entries become a hard configure error ("Cannot find source file") and the remaining five are silently dropped from the build, producing a cascade of unresolved externals. CLAUDE.md states "Linux (should work)" and README/CLAUDE.md present the CMake build as cross-platform; neither is true today.

**Recommendation:** `git mv` all seven files to a lowercase `.cpp` extension (use a two-step rename through a temporary name so Git on case-insensitive filesystems records it), then update lines 277 and 593. Adding a Linux configure job to CI would keep this class of breakage from recurring — it is invisible from Windows and macOS.

> ✅ **Fixed** on branch `harden/high-severity-batch1`, and the finding **understated the problem**. `BIT_RES.CPP` was already gone (deleted with the orphaned MP3 decoder in `66d8637`), leaving six of the seven — but a full `git ls-files` sweep for `\.(CPP|H)$`, which the review did not do, turned up seven *more* case-broken headers the finding never named:
>
> - `Client/MWarManager.H` was included under **both** spellings — `"MWarManager.H"` from `CGameUpdate.cpp:53`, `GameInit.cpp:85` and `SizeOfObjects.cpp:113`, and `"MWarManager.h"` elsewhere — so no single on-disk name could satisfy every includer.
> - All five vendored libjpeg headers under `Client/JpegLib/` are tracked uppercase (`JPEGLIB.H`, `JCONFIG.H`, `JERROR.H`, `JMORECFG.H`, `JPEGINT.H`) while every include of them, including their own internal cross-includes at `JPEGLIB.H:24,26,1092,1093`, is lowercase. `Client/UtilityFunction.cpp:594` reaches them through a live `extern "C"` block, and got the directory case wrong as well (`"jpegLib/jpeglib.h"` against a tracked `JpegLib/`).
>
> **Fourteen** files are renamed to their lowercase spelling and the include sites corrected; `Client/Client.vcxproj.filters` is updated to match. `git ls-tree -r` at the branch tip reports **zero** uppercase `.CPP`/`.H`/`.C` files, where the parent commit had exactly fourteen. (An earlier version of this entry said thirteen and claimed `VS_UI/src/widget/PI.H` was deliberately left behind; it was renamed with the rest.)
>
> One further hole of the same class was missed here and found by review: `CMakeLists.txt` listed `Client/MNpcTable.cpp` immediately above the correctly-cased `Client/MNPCTable.cpp`. On Windows CMake folds the two spellings and emits one entry, so the tree builds; on Linux the first is a hard `Cannot find source file` — the exact configure error this work exists to eliminate, surviving inside the commit that claimed no `CMakeLists.txt` change was needed. The duplicate is removed, and every explicitly-listed source is now verified against `git ls-files` case-sensitively.
>
> Two things this fix does **not** establish. The review's speculation that Windows lists these files twice and risks LNK2005 is wrong — each appears exactly once in the generated projects. The *reason* first given for that was also wrong, though: it was not CMake de-duplicating a glob against an explicit entry, since `66d8637` had already removed the only such entry. The uniqueness is trivial rather than earned. And **no build on this machine can verify any of it**: the Windows filesystem resolves either case, so the tree is green before and after. The evidence is `git ls-files` and inspection of every include site, not a compiler. Only a Linux configure would prove it, which is why the finding's CI recommendation still stands.

#### 🟡 Medium -- .gitignore ignores `Makefile` — the repo's own tracked build entry point — and `*.cmake`, which would swallow any future CMake module.

> ✅ **Fixed (2026-09-18)**: removed the blanket `Makefile` and `*.cmake` ignore rules and their exception list. Only generated CMake filenames/build directories remain ignored. `git check-ignore --no-index` confirms hand-written Makefiles/modules are visible.

**Category:** build  |  **Location:** `.gitignore:49`

Line 49 is a bare `Makefile` pattern. Because Git ignore rules without a slash match at every directory level, this matches the repository's own hand-written, tracked `Makefile` (confirmed: `git check-ignore --no-index -v Makefile` reports `.gitignore:49:Makefile`). It survives only because it is already in the index — the moment anyone runs `git rm --cached Makefile`, or a contributor adds a `tools/Makefile` or `emscripten/Makefile`, it silently will not be added and `git status` will not mention it. Line 50's `*.cmake` has the same shape: it would ignore a `cmake/FindSDL2.cmake` or a `cmake/Toolchain.cmake` module, which is the natural next step for a project already doing manual ATL discovery at CMakeLists.txt:298-321. Line 51's `!CMakeLists.txt` negation is a no-op — `CMakeLists.txt` does not end in `.cmake` and was never matched by line 50, so it gives a false impression that the negation is protecting something. All of this is redundant anyway: line 25's `[Bb]uild/` already excludes generated CMake trees, which is where these files actually appear.

**Recommendation:** Delete lines 45-52 (the "CMake Generated Files" block) entirely — `[Bb]uild/` covers the real case. If per-file rules are wanted, scope them to the build tree (`build/**/Makefile`, `build/**/*.cmake`) so they cannot reach hand-written files.

#### 🟡 Medium -- Platform.h defines assert() as a no-op on all non-Windows platforms, and whether the real assert survives depends on include order.

> ✅ **Verified resolved (2026-09-18)**: duplicate of the non-Windows assertion finding in the platform section. `Platform.h` uses `<assert.h>` with no project replacement; Debug builds retain the standard assertion behavior.

**Category:** correctness  |  **Location:** `basic/Platform.h:27`

Lines 25-29 read `#ifndef PLATFORM_WINDOWS / #ifndef assert / #define assert(e) ((void)(e)) / #endif / #endif`. This makes every `assert()` in the codebase a silent no-op on Linux and macOS — a failing assertion evaluates its expression and continues into the state it was supposed to catch. Worse, the behaviour is not even consistent within a build: `<assert.h>` always `#undef`s and redefines `assert`, so a TU that includes `<assert.h>` *after* Platform.h gets the real aborting assert, while a TU that never includes it gets the stub. So the same `assert(pFoo != NULL)` aborts in one object file and falls through in another, which makes any debugging session that relies on assertions actively misleading. Since the ASan/UBSan story is already broken on the primary Windows toolchain (see the USE_ASAN finding), asserts are one of the few remaining runtime checks, and they are disabled precisely on the platforms where the sanitizers *do* work.

**Recommendation:** Delete lines 25-29 and `#include <assert.h>` instead. If the intent was to disable asserts in release builds, that is what `NDEBUG` is for, and CMake already sets it for Release configurations.

#### 🟡 Medium -- Platform.h opens extern "C" at line 21 and includes windows.h, SDL.h and pthread.h inside it, across 2000 lines, in a header reached by nearly every TU.

> ✅ **Fixed** on branch `fix/review-platform-linkage` (2026-09-18). The linkage block now encloses the platform API declarations, with no dependency includes inside it; the earlier `platform_show_error` forward declaration explicitly keeps C linkage. The source audit moved 12 include directives out of the block and verified that all 30 `PlatformSDL.cpp` API definitions still have their declarations inside it. `test_platform_linkage.cpp` links a separately compiled C consumer (`tests/support/platform_c_consumer.c`) against the same implementation and checks its type layout and calls alongside C++; this is a regression guard for ABI preservation, not a reproduction with today's dependency versions. Full Debug/ASan builds and all nine CTests pass.

**Category:** portability  |  **Location:** `basic/Platform.h:21`

Line 21 opens `extern "C" {` and it is not closed until the end of the 2037-line file. Inside that block the header includes `<TargetConditionals.h>` (line 41), `<SDL2/SDL.h>` (lines 65/67/69), `<windows.h>` (lines 79 and again at 892) and `<pthread.h>` (line 212). Giving C language linkage to whatever those headers declare is ill-formed the moment any of them reaches a C++ construct — an overload set, a template, a default argument. It survives on MSVC today mostly by luck: windows.h opens its own `extern "C"` blocks, and the standard headers SDL pulls in are usually already included (and thus include-guarded out) by the time Platform.h runs. Change the include order, add SDL_mixer, or move to a toolchain whose `<stdlib.h>` pulls `<cstdlib>` and it stops compiling with errors that point nowhere useful. Separately, dragging the whole Win32 header into every TU brings the A/W macro set with it, which silently renames project identifiers: `Client/UtilityFunction.h:46` declares `unsigned long GetDiskFreeSpace(const char*)`, which the preprocessor rewrites to `GetDiskFreeSpaceA` and quietly adds to the Win32 overload set alongside `BOOL GetDiskFreeSpaceA(LPCSTR, LPDWORD, LPDWORD, LPDWORD, LPDWORD)`. It compiles because the arities differ, but a future one-argument call to `::GetDiskFreeSpaceA` would resolve to the project's own function instead of the API. `VS_UI/src/header/VS_UI_Base.h:320-321` has the same shape with `SendMessage`/`DispatchMessage`.

**Recommendation:** Move all `#include` directives above the `extern "C" {` at line 21 so only this header's own C declarations are inside it. Longer term, split Platform.h: a small C-linkage type/function header, and a separate implementation header that pulls in windows.h/SDL only where it is actually needed, so a 2000-line Win32+SDL include is not on the critical path of every one of ~1000 translation units.

#### 🟡 Medium -- PLATFORM_MACOS is defined for every non-Windows platform, so Linux builds compile the macOS code paths.

> ✅ **Verified resolved (2026-09-18)**: `Platform.h` derives Windows, Linux and macOS from the compiler's platform macros; shared Unix behavior uses `PLATFORM_POSIX`. CMake no longer defines a fake macOS platform for Linux, and ratchet R13 prevents those build-defined platform macros from returning.

**Category:** portability  |  **Location:** `CMakeLists.txt:841`

Four sites define `PLATFORM_MACOS` guarded only by `if(NOT WIN32)`: line 156 (TextSystem), lines 364 and 366 (VS_UI, both PRIVATE and PUBLIC), and line 841 (DarkEden). Client/DXLib/CMakeLists.txt:32-34 is the one place that gets it right, guarding on `if(APPLE)`. The result is that a Linux build has both `PLATFORM_LINUX` (auto-detected by basic/Platform.h:38 from `__linux__`) and `PLATFORM_MACOS` (forced by CMake) defined simultaneously, and every `#ifdef PLATFORM_MACOS` in the codebase takes the macOS branch on Linux. This is not cosmetic: Client/Packet/Types/SystemTypes.h:61 selects the path separator with `#if defined(__LINUX__) || defined(PLATFORM_MACOS)`, and Client/CGameUpdate.cpp:2803 gates an anti-cheat error check on `#ifndef PLATFORM_MACOS`, so Linux silently inherits macOS-specific behaviour. It also makes the macro name actively misleading to anyone reading the source, which is a maintenance tax on every future port decision.

**Recommendation:** Introduce a proper platform selector: `if(APPLE) ... PLATFORM_MACOS elseif(UNIX) ... PLATFORM_LINUX endif()` at all four sites, or better, delete the CMake-side definitions entirely and rely on basic/Platform.h:35-48, which already derives the correct macro from the compiler's own predefined `__APPLE__`/`__linux__`.

#### 🟡 Medium -- MSVC is never given /utf-8 while source encoding is mixed, so identical Korean literals produce different bytes depending on whether a file has a BOM.

> ✅ **Fixed (2026-09-18)**: MSVC receives `/utf-8` for every C/C++ target. A unit test reproduced the mismatched narrow-literal bytes: both the universal-character escape and BOM-header literal became `?`, while the non-BOM literal retained UTF-8. All three now match the same explicit UTF-8 bytes. The strict source audit found one remaining invalid byte, a degree sign in an xBRZ comment, now written in ASCII; TextBackendSDL is already valid UTF-8. A `source_encoding` CTest validates existing tracked and new C/C++ files, accepting UTF-8 with or without a BOM and failing on invalid bytes or a failed or unexpectedly small enumeration.

**Category:** portability  |  **Location:** `CMakeLists.txt:13`

Of the tracked C++ sources, 94 carry a UTF-8 BOM and 2200 do not; only one file (Client/TextSystem/TextBackendSDL.cpp) is not valid UTF-8 at all. No CMakeLists sets `/utf-8`, `/source-charset` or `/execution-charset`. MSVC's rule is that a BOM'd file is decoded as UTF-8 and its narrow literals are then converted to the ANSI execution charset — on a non-Korean Windows locale, Korean characters become `?`. A file *without* a BOM is decoded using the ANSI codepage, so its raw UTF-8 bytes pass through unchanged and reach the game's own text renderer intact. The two halves of the codebase therefore disagree: Client/Client.cpp has a BOM and contains live Korean literals such as `DEBUG_CMD(MIN_CLRSCR, "시작")` at line 4147, which get lossily converted, while the same string in a non-BOM file survives. This is silent — the compiler emits C4819 at most, and warnings are off everywhere (see previous finding). It also means the encoding of a string literal depends on an invisible property of the file rather than on anything in the source.

**Recommendation:** Add `/utf-8` for MSVC on every target (it sets both source and execution charset to UTF-8), normalise Client/TextSystem/TextBackendSDL.cpp to UTF-8, and add a `.gitattributes` entry marking `*.cpp`/`*.h` as `text working-tree-encoding=UTF-8` so new files stay consistent. Stripping the 94 BOMs afterwards makes the tree uniform.

#### 🟡 Medium -- No warning configuration on any target that holds the risky code; only `basic` sets warning flags, and half of that is in a dead branch.

**Category:** build  |  **Location:** `CMakeLists.txt:833`

> ✅ **Fixed** on `build/review-warning-policy` (2026-09-21). `project_warnings` supplies `/W3 /w14311 /w14312 /w14668` or `-Wall -Wextra -Wundef` to every compiled target; clang-cl forwards the latter through `/clang:` because its driver silently ignores the MSVC warning-number switches. `warning_policy` checks the generated options for all 22 targets in both C and C++ contexts. The 17-case parser guard covers MSVC/Clang/GCC and untagged linker diagnostics, repeated warnings, malformed/missing budgets, growth and unrecorded decreases. Both verification scripts require clean builds, upload reports, and enforce measured budgets for all ten CI preset/architecture combinations. The metric counts distinct diagnostic locations by warning ID, not every repeated include; swapping warnings of the same kind can preserve the count, as documented in `tools/ci/warnings.md`. Full Windows Debug/ASan/Release builds and all 11 CTests pass; the initial hosted builds also passed all 11 tests on their platforms before exposing the missing budgets/diagnostic variants now recorded. Final-head CI remains a merge gate. The original assertion that MSVC warnings were off was incorrect: CMake already supplied `/W3`. MSVC ASan is also active in the current dedicated build tree.

A repo-wide search for `add_compile_options`/`target_compile_options`/warning flags across all six CMakeLists finds exactly three hits: basic/CMakeLists.txt:79 (`/W3`, inside `if(WIN32 AND NOT USE_SDL_BACKEND)` — dead, since USE_SDL_BACKEND is FORCEd ON at CMakeLists.txt:54), basic/CMakeLists.txt:85-88 (`-Wall -Wno-unknown-pragmas`, GNU/Clang only), and tools/engine/sprite/CMakeLists.txt:47 (`/std:clatest`, unrelated). The targets that actually contain the hazardous code — `DarkEden`, `VS_UI`, `SpriteLib`, `dxlib`, `framelib`, `TextSystem` — set no warning level at all, on any compiler. For a ~2000s C++ codebase mid-port, full of raw `sprintf`/`strcpy`, `DWORD`-holding-a-pointer casts and 32-bit arithmetic, this removes the cheapest defence available. Two specific warnings would have caught findings in this very report: `-Wundef` / `/w14668` flags the `#if __WINDOWS__` misfires described above, and MSVC's C4311 (`pointer truncation from 'FARPROC' to 'DWORD'`) flags Client/APICheck.cpp:126 directly. With warnings off and ASan a no-op on MSVC, nothing in the build is looking for these.

**Recommendation:** Add a shared interface target carrying `-Wall -Wextra` (GNU/Clang) and `/W3 /w14311 /w14312 /w14668` (MSVC) and link it into every target. Do not start with `-Werror` — the existing warning backlog will be large — but capture the count in CI so it ratchets down instead of up.

#### 🟡 Medium -- Three non-Windows source exclusions are anchored ^Client/ but match against absolute paths, so they never fire.

> ✅ **Verified resolved (2026-09-18)**: the ineffective anchored exclusion block is gone. The obsolete root handlers were deleted; `ClientFunction.cpp` is compiled on all supported platforms. Current full Linux/macOS builds verify the resulting source set instead of relying on those filters.

**Category:** build  |  **Location:** `CMakeLists.txt:696`

Inside the `if(NOT WIN32)` block, line 696 filters `"^Client/[^/]+Handler\\.cpp"`, line 698 filters `"^Client/Client\\.cpp"` and line 699 filters `"^Client/ClientFunction\\.cpp"`. All three are anchored to the start of the string with `^`, but `CLIENT_MAIN_SOURCES` came from `file(GLOB)` at line 583 and contains absolute paths (`/Users/.../client/Client/Client.cpp`), so the anchor can never match. Confirmed empirically: the checked-in macOS-generated compile_commands.json still contains `/Users/genius/project/opendarkeden/client/Client/Client.cpp` despite line 698 claiming to exclude it. Line 713 repeats the Client.cpp exclusion *without* the anchor, which is the only reason that particular one has any effect at all — meaning the file is protected by an accidental duplicate rather than by the rule that names it. The `^Client/[^/]+Handler\.cpp` rule at line 696 has no unanchored twin and is simply dead: its stated intent (exclude root-level Handler files but keep the ones under Packet/) is not being enforced on any platform.

**Recommendation:** Drop the `^` anchors and match on a path separator instead — e.g. `"/Client/[^/]+Handler\\.cpp$"` — matching the style already used correctly at lines 650, 651, 660-662, 669 and 676. Then remove the now-redundant duplicate at line 713.

#### 🟡 Medium -- Every file(GLOB) lacks CONFIGURE_DEPENDS, so adding or deleting a source does not trigger a reconfigure.

> ✅ **Fixed (2026-09-18)**: all four source globs (client, UI, framelib and unit tests) now use `CONFIGURE_DEPENDS`; the one-level packet glob is spelled `*`, not a misleading `**`. A temporary framelib source was added and removed without running configure manually, and both changes regenerated the project. MSBuild had already loaded the old target on removal, so that direct target build required a second invocation after regeneration; the regenerated source list was correct.

**Category:** build  |  **Location:** `CMakeLists.txt:583`

CMakeLists.txt:171-174 (`file(GLOB_RECURSE VS_UI_SRC_SOURCES ...)`), :583-589 (`file(GLOB CLIENT_MAIN_SOURCES ...)`) and Client/framelib/CMakeLists.txt:6 all glob without `CONFIGURE_DEPENDS`. CMake evaluates these once at configure time and bakes the result into the generated projects. A contributor who adds a new `Client/Foo.cpp` gets a build that succeeds while silently never compiling it, then a link error about a symbol whose definition is sitting right there in the tree; a contributor who deletes one gets a build referencing a file that no longer exists. In a codebase where the recommended workflow is `make debug-asan` (which reuses `build/debug-asan` across runs, Makefile:57-60) rather than a clean configure, this hits often. The risk is amplified here because the glob list is then filtered by fourteen `list(FILTER ... EXCLUDE REGEX ...)` calls (lines 626-713) whose correctness already depends on exactly what the glob returned. Line 589's `Client/Packet/**/*.cpp` is also misleading: `**` is not a recursive wildcard in `file(GLOB)` — CMake treats it the same as `*`, matching one directory level only.

**Recommendation:** Add `CONFIGURE_DEPENDS` to all three globs. Given how much conditional filtering is layered on top, converting the Client and VS_UI source sets to explicit lists would be a larger but more honest fix — it would have made the REMOVE_ITEM and case-sensitivity bugs above impossible.

#### 🟡 Medium -- cmake_minimum_required(VERSION 3.10) understates the real requirement; the file uses commands that need 3.13.

> ✅ **Verified resolved (2026-09-18)**: the root project and test project require CMake 3.21, matching the documented Visual Studio 2022 generator requirement. Older 3.20 declarations in individual subdirectories cannot lower the root requirement.

**Category:** build  |  **Location:** `CMakeLists.txt:1`

Line 1 declares 3.10, but line 77 calls `add_link_options()` and line 752 calls `target_link_directories()`, both introduced in CMake 3.13. README.md:43 tells contributors to install "CMake 3.20+" and CLAUDE.md's build requirements say "CMake 3.20+". A contributor on 3.10-3.12 passes the version gate and then fails with `Unknown CMake command "target_link_directories"` partway through configure, which reads as a broken CMakeLists rather than an out-of-date CMake. The understated minimum also silences policies the project would benefit from: at 3.10 compatibility, CMP0077 (option() honouring normal variables) and CMP0079 stay at OLD, which matters here because the file mixes `option()` in subdirectories (basic/CMakeLists.txt:13, Client/DXLib/CMakeLists.txt:13) with a FORCEd cache set at line 54. Note also that all five sub-CMakeLists repeat `cmake_minimum_required(VERSION 3.10)`, which is redundant and drifts independently.

**Recommendation:** Raise the top-level minimum to match reality and the docs — `cmake_minimum_required(VERSION 3.20)` — and delete the redundant per-subdirectory calls so there is one number to keep in sync with README.md:43.

#### 🟡 Medium -- map_viewer and effect_viewer link the `sprite` target unconditionally, but that target only exists when BUILD_ENGINE is ON.

> ✅ **Fixed (2026-09-18)**: `map_viewer` is created only with `BUILD_ENGINE=ON`; its loader really uses the engine's `zone.c`. Removed the unused `sprite` link from `effect_viewer`, which remains available with the engine off. A Windows `BUILD_ENGINE=OFF` build reproduced LNK1104 before the change; afterwards the map viewer is absent from the solution and the effect viewer builds successfully.

**Category:** build  |  **Location:** `CMakeLists.txt:511`

`sprite` is defined by `add_subdirectory(tools/engine/sprite)` at line 163, which is inside `if(BUILD_ENGINE)` (line 162). `BUILD_ENGINE` is a user-settable option declared at line 56. But `target_link_libraries(map_viewer PRIVATE sprite ...)` (line 512) and `target_link_libraries(effect_viewer PRIVATE sprite ...)` (line 548) are both outside any `BUILD_ENGINE` guard, as are the `add_executable` calls at lines 497 and 529. Configuring with `-DBUILD_ENGINE=OFF` therefore leaves `sprite` undefined as a CMake target; CMake falls back to treating it as a raw library name and emits `-lsprite` / `sprite.lib`, producing a link failure (`cannot open file 'sprite.lib'`) rather than a clear configure-time message. map_viewer additionally adds `tools/engine/sprite/include` to its include path at line 508, which likewise stops existing.

**Recommendation:** Wrap the map_viewer and effect_viewer targets in `if(BUILD_ENGINE)`, or add `if(NOT BUILD_ENGINE) message(FATAL_ERROR "map_viewer/effect_viewer require BUILD_ENGINE=ON") endif()` so the failure is reported at configure time with the actual cause.

#### 🟡 Medium -- A 3.4 MB generated compile_commands.json full of another developer's macOS absolute paths is tracked in git and not gitignored.

> ✅ **Fixed (2026-09-18)**: the stale compilation database was already untracked; `/compile_commands.json` is now explicitly ignored. README explains how to use the build-directory database generated by Ninja/Makefile generators and that Visual Studio generators do not produce one.

**Category:** maintainability  |  **Location:** `compile_commands.json:2`

The tracked root `compile_commands.json` has `"directory": "/Users/genius/project/opendarkeden/client"`, invokes `/Library/Developer/CommandLineTools/usr/bin/c++`, and passes `-DPLATFORM_MACOS` with `-I/Users/genius/project/opendarkeden/client/...` include paths. None of that exists on any other machine. Meanwhile CMakeLists.txt:20 sets `CMAKE_EXPORT_COMPILE_COMMANDS ON`, so a real, correct database is generated into the build tree on every configure. `git check-ignore --no-index compile_commands.json` reports it is not ignored, so it will keep being re-committed. The practical cost is direct: clangd, ccls and every editor that auto-discovers a compilation database at the repo root will read this file first, resolve nothing, and give every contributor broken navigation and phantom diagnostics across a 2500-file codebase. The 3.4 MB also re-diffs noisily on any commit that touches it. Note the same stale artifact is what let me confirm several of the other findings — it is a useful forensic record, but it does not belong at the repo root.

**Recommendation:** `git rm --cached compile_commands.json`, add it to .gitignore, and document in README.md that the database is generated at `build/<dir>/compile_commands.json` (a symlink or a `file(CREATE_LINK)` in CMake can put it at the root for editors without tracking it).

#### 🟡 Medium -- The Makefile's entire Emscripten workflow points at an emscripten/ directory that does not exist, and `make clean` invokes it.

> ✅ **Fixed by removal (2026-09-18)**: deleted the four dead web targets, SDK variables, help entries and cleanup chain, plus the root CMake Emscripten SDL aliases/link options and iconv bypasses. The repository has no Emscripten application to support these paths.

**Category:** dead-code  |  **Location:** `Makefile:188`

Makefile:18 sets `BUILD_DIR_WEB = emscripten/build`; the `web` target at line 203 runs `emcmake cmake $(CLIENT_DIR)/emscripten`; `web-desktop` at line 179 runs `$(MAKE) -C emscripten test_desktop.sh`; `web-clean` at lines 225-226 removes `emscripten/demo_test`. No `emscripten/` directory exists in the repo or in git. The `clean` target at line 104 unconditionally chains `$(MAKE) web-clean`, so plain `make clean` runs `rm -rf emscripten/build` and `rm -f emscripten/demo_test` against nothing — harmless today only because both are `rm -f`-style. CMakeLists.txt carries the matching dead weight: lines 73-88 build a whole `if(EMSCRIPTEN)` branch with four IMPORTED interface targets and SDL2 aliases, and line 792 has an `if(NOT EMSCRIPTEN)` guard around iconv linking. Meanwhile the help text at Makefile:249-253 and 287-290 advertises four web targets to contributors, and lines 212-216 print specific output filenames and a "Total size: ~1.1MB" claim for a build that cannot run. This is roughly 60 lines of Makefile and 20 lines of CMake that will mislead anyone who tries them.

**Recommendation:** Either restore the `emscripten/` directory or delete the four web targets from the Makefile (including the `$(MAKE) web-clean` chain in `clean` at line 104 and the help text), plus the EMSCRIPTEN branches in CMakeLists.txt. Leaving advertised-but-broken targets in `make help` costs every new contributor the time it takes to discover they do not work.

#### ⚪ Low -- Client_PCH.h is not a precompiled header, puts `using namespace std;` into every TU, and redefines a macro the build already passes on the command line.

> ✅ **Fixed (2026-09-21):** `DarkEden` and `VS_UI` each compile `Client/Client_PCH.h` as a private, C++-only PCH using their own definitions and options. CMake generates the producer, replacing the two empty handwritten `.cpp` files. The VS_UI wrapper's warning pragmas remain local to its existing includers. The shared header already uses selected `std` declarations instead of a namespace directive; the retired client/server macro stays covered by R15. Generated build rules and the full compiler/test matrix own PCH production and consumption, while R1 counts application sources without the generated producer.

**Category:** maintainability  |  **Location:** `Client/Client_PCH.h:30`

Despite the name and the header comment ("Minimal precompiled header for CMake builds"), no CMakeLists calls `target_precompile_headers` or sets MSVC `/Yc`/`/Yu`. It is a plain header included at the top of essentially every Client TU, and it pulls in basic/Platform.h — 2037 lines that themselves include windows.h and SDL — so that cost is paid ~1000 times per build rather than once. Line 30 places `using namespace std;` at global scope in that same universally-included header, which drops the entire std namespace into every translation unit; in a codebase this old that is a live collision risk against common identifiers (`count`, `left`, `right`, `distance`, `swap`, `byte`) and it makes any future move to a newer standard harder than it needs to be. Line 16 does `#define __GAME_CLIENT__` while CMakeLists.txt:836 already passes `__GAME_CLIENT__=1` to the DarkEden target, so every DarkEden TU hits a C4005 macro redefinition — currently invisible because warnings are off, and it means the macro has value `1` in some TUs and empty in others.

**Recommendation:** Wire the header up with `target_precompile_headers(DarkEden PRIVATE Client/Client_PCH.h)` (and the same for VS_UI) so it earns its name; move `using namespace std;` out of the header into the .cpp files that need it; and pick one place to define `__GAME_CLIENT__` — either the CMake definition at line 836 or the `#define` at line 16, not both. (✅ **The macro part is closed**: task 5.2's ninth and tenth slices, 2026-09-13, evaluated every conditional on `__GAME_CLIENT__` out of the tree and removed both definitions, so nothing defines or reads it; ratchet R15 keeps it that way. The remaining clauses are resolved by the current status above.)

#### ⚪ Low -- 97 Korean-named files are tracked under 참고자료/, and CMakeLists.txt cites one of them as normative build documentation.

> ✅ **Verified resolved (2026-09-18)**: the historical Korean-named archive is no longer tracked, and CMake's WinMain rationale is inline. Removed the stale archive row from `CLAUDE.md`; the current build documentation no longer points to that tree.

**Category:** maintainability  |  **Location:** `CMakeLists.txt:207`

The `참고자료/` tree holds 97 tracked files with fully Korean paths (e.g. `참고자료/분석관련/07. VS2019 마이그레이션 가이드.md`, `참고자료/작업지시/build_after 4(DEBUG 출력 설정).md`). CMakeLists.txt:207 points at one of them as the justification for excluding VS_UI/WinMain.cpp: "See 참고자료/커밋로그/2026-08-21_VS_UI_WinMain_cpp_죽은_진입점_제외.md". Non-ASCII paths are a practical hazard on Windows: `git` renders them as octal escapes under the default `core.quotepath`, `cmd.exe` and some CI checkout steps mangle them under a non-UTF-8 codepage, and archive extraction (`git archive`, GitHub's zip download) can produce unopenable names. Having the build file itself depend on such a path for its rationale means a contributor debugging the WinMain exclusion may be unable to open the referenced document. This also sits against the project's own stated direction — CLAUDE.md says "only **English** should be used" for comments, and the repo's memory notes record that this fork is English-only.

**Recommendation:** Move the design rationale that CMakeLists.txt actually depends on into the CMake comment itself or into an ASCII-named file under `Documents/`, so the build file has no non-ASCII path dependency. Whether to keep the wider 참고자료/ archive is a call for the maintainer, but renaming the paths to ASCII (content can stay Korean) removes the tooling risk at no information cost.

#### ⚪ Low -- A 320 KB mojibake `tree /f` dump and the batch file that regenerates it are tracked in git, already stale.

> ✅ **Verified resolved (2026-09-18)**: `git ls-files Documents filelist.txt make_tree.bat` is empty. Both the generated tree dump and its generator, as well as the duplicated historical documentation tree, were already removed.

**Category:** maintainability  |  **Location:** `filelist.txt:1`

`filelist.txt` is 327 KB of CP949-encoded Windows `tree /f` output, tracked in git and not ignored. Its first lines identify volume `2TB_Nvme2` and root `H:.`, and its listing includes `plan.md` at the repo root — a file that no longer exists. Every line renders as mojibake in any UTF-8 tool. `make_tree.bat` (a single line: `tree /f > filelist.txt`) is tracked alongside it. The file duplicates information `git ls-files` gives accurately and for free, goes stale on every commit that adds or removes a file, and adds 320 KB to every clone. Related hygiene in the same category: `Documents/` tracks ten `*_kr.md` documents plus a `Documents/backup/` directory holding 14 files that are the same documents under their original names — two copies of the project's migration notes, with no indication which is current.

**Recommendation:** `git rm filelist.txt make_tree.bat`. Reconcile `Documents/` and `Documents/backup/` down to one copy of each document; a `git log` on the deleted path recovers anything needed later.

#### ⚪ Low -- The Makefile is single-threaded on Windows, and helper scripts are macOS-only, despite Windows being the documented build platform.

> ✅ **Fixed (2026-09-18)**: the Makefile detects Windows processor counts or uses `nproc`/`sysctl`, respects `NPROCS`, and passes explicit build/test configurations. Test targets build both registered test executables. The effect-viewer Bash helper uses the same portable job selection, an overridable in-repository data path, and Debug/single-config executable locations. Removed the identical CMake branch. GNU Make dry runs checked Unix and Windows job selection plus Debug/Release/test commands; the helper passes Bash syntax checking. No viewer or game was launched.

**Category:** build  |  **Location:** `Makefile:31`

Makefile:31 sets `NPROCS ?= 1` and only overrides it for `uname -s` values `Linux` (line 33) and `Darwin` (line 36). Under Git Bash on Windows `uname -s` returns `MINGW64_NT-10.0-...`, so `NPROCS` stays 1 and `cmake --build ... -j1` compiles ~1000 translation units serially — on the platform README.md:9 names as the primary build. `build_and_run_effect_viewer.sh:31` hardcodes `sysctl -n hw.ncpu`, which exists only on macOS/BSD, and line 9 points `DATA_DIR` at `../DarkEden` while CLAUDE.md documents the data directory as `DarkEden/`. Two smaller build-file inconsistencies in the same vein: CMakeLists.txt:563-571 branches on `if(CMAKE_BUILD_TYPE STREQUAL "debug-asan")` where both branches are byte-identical and the Makefile never sets that value (it passes `Debug` at line 59); and `CMAKE_BUILD_TYPE` is ignored entirely by the Visual Studio multi-config generator that README.md:84 prescribes, so `make debug` and `make release` select no configuration at all on the primary platform.

**Recommendation:** Default `NPROCS` to `$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)` so it works everywhere; replace the `sysctl` call in build_and_run_effect_viewer.sh the same way. Delete the dead identical branch at CMakeLists.txt:563-571. Either make the Makefile pass `--config` for multi-config generators or state plainly in README.md that the Makefile is for single-config (Ninja/Unix Makefiles) builds only.

---

## Methodology & Limitations

Eight independent Opus reviewers ran in parallel at xhigh reasoning effort, each assigned one subsystem and instructed to read real source, cite `file:line`, and report only evidenced defects (not style nits or the mere presence of legacy patterns). Reviewers did not coordinate, so a small number of cross-cutting issues (e.g. the `_DEBUG` gating) may appear in more than one area from different angles. This was **static review only**: no compilation, sanitizer run, fuzzing, or dynamic reproduction was performed. Treat severities as reviewer-assigned triage, and confirm each `file:line` against the current tree before implementing a fix.
