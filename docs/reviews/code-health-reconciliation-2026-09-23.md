---
type: Review
title: "Code-health reconciliation, 2026-09-23"
description: "The current disposition of the review's 197 findings, repaired, removed or verified, and what the historical dated summaries no longer describe."
tags: [review]
---

# Code-health reconciliation (2026-09-23)

The 197 findings in the [original review](code-health-review-2026-08-29.md)
have current dispositions: repaired code, removed unused code, or verified
existing behavior with the original claim corrected. Duplicate findings remain
in the 197-entry denominator. This is not a claim of 197 distinct new fixes,
nor a claim that the client has no remaining defects.

The review's dated summaries, grades, line numbers and intermediate "open"
paragraphs describe earlier trees. The latest closure notes and the evidence
below supersede those historical states. Reconciliation included the runtime,
source-reading and caveat sections, not just a count of check marks. That pass
found the still-unbounded output queue and incomplete effect-selection
diagnostics, leading to the output-budget and checked-selection fixes.

## Finding coverage and continuing owners

The ranges below number the `####` finding headings in document order. They
are separate from the review's C1-C28 critical-finding aliases.

| Area / findings | Current mechanisms and regression owners |
|---|---|
| Input, Audio & Media, 1-22 | SDL mixer chunk references/channel ownership and paired subsystem initialization; bounded audio paths; removed duplicate headers/decoder. `test_part_manager.cpp` owns bounds, release hooks and rollover; `test_input_adapter.cpp` owns input indices and consumed wheel deltas. Native MCI paths remain latent. Audio/game-global integrations use source review and builds. |
| Rendering & Sprites, 23-47 | Checked pixel, index, shadow, palette, alpha and filter readers; retained row bounds; single SDL surface definition; scoped locks, clipping and pitch-aware writes. Sprite/backend/surface tests own these contracts. Removed unused texture managers. Pack and caller follow-ups plus the installed-art denominator are in the [sprite audit](sprite-asset-audit-2026-09-23.md). |
| Networking & Protocol, 48-74 | Framed reads, bounded indices/strings, normalized wire booleans, checked factories, matching exchange layouts, partial-send retention and bounded input/output rings. Packet parser, factory, golden, stream, datagram and player tests own the library behavior. Removed unused peer-receive and WinINet paths. Handler effects retain the executable exemption. |
| Core Game Loop & State, 75-98 | Checked item factories/names, sector access and effect indices; explicit effect/action ownership; complete `ZoneMapData` parsing before live publication; checked table counts and immutable fallback rows. Zone-map, table-access/input, experience and skill tests own parsing/storage. Live sector, movement, effect and lifecycle changes use the named exemptions and source ownership review. |
| Text & Strings, 99-121 | Typed bounded formatting; UTF-8 resource boundaries and renderer contract; owned wrapping/reduction; checked `MString`, token and message-ring lifetimes; bounded normalization/glyph caches. Safe-format, string, encoding, wrapping, resource and text-service tests own these contracts; the [ingress audit](text-ingress-audit-2026-09-23.md) records source paths and installed XML results. |
| UI Framework, 122-146 | Typed/owned message payloads, item-destruction tooltip invalidation, owned labels/descriptions/help, tested skin parsing, checked scroll geometry, initialized buttons, unified SDL editor focus/storage and manager-aware window destruction. UI tests link the reachable production classes; file-dialog, scroll, safe-text and layout helpers have library tests. Rendering/global wiring remains explicitly exempt. |
| Foundation Libraries, 147-173 | One shared `TArray`, size/alignment-aware memory pools, checked persistent configuration, reusable timer storage, thread trampolines, isolated C linkage, bounded platform conversion and sorted transactional position-list reads. Dedicated array, pool, config, timer, thread, linkage and list tests own behavior. Unused directory, raw-object serializer, GL imports and decoder copies are removed. |
| Build, Portability & Hygiene, 174-197 | Public backend/wire definitions, single source membership, actual private PCHs, source-glob regeneration, UTF-8 compilation, supported sanitizer checks, all-target warning policy/budgets, portable helpers and removed obsolete build paths. Build-contract tests, generated-project checks, ratchets and full Windows/Linux/macOS builds own these rules. |

The original notes sometimes name executable-only code that has since moved to
a library. Current CMake membership and tests take precedence. In particular,
packet parsers are testable in `packetwire`, message rings in `basic`, tokens in
`gamemodel`, and XML/help/settings in `VS_UI`. `test_tarray.cpp` now includes the
shared implementation and both compatibility headers; the former duplicate
`test_tarray_spritelib.cpp` was retired during consolidation.

## Cross-cutting residuals

| Earlier residual | Current disposition |
|---|---|
| Tooltip cleanup depended on selected delete sites | `MItem` destruction notifies the host; the game invalidates both tooltip payloads before storage dies. `test_item_lifetime.cpp` covers the destruction contract. An ID-only redesign is superseded by exact-object invalidation, including locally created items with ID zero. |
| A queued UI string could outlive its sender | Text sends own their payload until callback completion; UI receiver tests cover exception and reentrant dispatch. General raw payloads retain their explicit caller-owned lifetime contract. |
| Table defaults were mutable and counts below 65,536 were trusted | Checked reads and const fallback rows, with checked mutation APIs; separate experience and skill-domain loaders are covered. This does not make every arbitrary use of a default/null resource row meaningful. |
| Output backpressure retained unbounded queued bytes | A 16 MiB ring ceiling and geometric growth preserve unsent bytes while bounding memory; frame/sequence rollback survives rejection. Input already has its own 16 MiB ceiling. |
| Missing packed help/quest resources and repeated encoding guessing | Bounded production UnRAR reads/listing, explicit resource decoding, migrated text callers and a UTF-8-only renderer. Archive extraction and XML parsing have separate evidence and limits. |
| Sprite rejection desynchronized later records or disappeared at callers | Checked complete records/indexes, retained lazy failure states, diagnostics and checked startup, ending, title, guild and preload callers. See the source-mapped asset audit; lazy opening still defers record decoding. |
| Option-name copies and format suffixes remained unbounded | Checked owned option names and transactional table loading; final percentage/suffix writes are bounded. Existing formatter and option-table tests own the helpers. |
| Player key setters retained dead encryption bookkeeping | Unused hash storage and no-op setters are removed. Packet fields and the separate per-field `Encrypter` retain their wire contract. |
| PCH, compiler-case verification and warning policy were partial | CMake creates private PCHs for the executable/UI; hosted case-sensitive builds compile the full tree. Every compiled target receives checked warning options and clean-build budgets. |

## Boundaries retained deliberately

- Transport remains unencrypted, including login credentials. Removing unused
  XOR bookkeeping does not add encryption; that requires a matching server
  protocol change. No wire layout changed in the final loader/output slices.
- SDL's pixel grayscale/gradation and palette screen blend are registered.
  Other legacy effects retain the copy fallback with bounded diagnostics;
  selection checks both table bounds. Unsafe 5-bit palette routines remain
  disabled on the 565 backend, following the review's diagnostic alternative.
- Native MCI/force-feedback and excluded native GDI paths are not runtime
  coverage claims. The live SDL input/text path and the library's latent 555
  decoders have their own tests.
- Executable exemptions are source/build regression guards where stated.
  They do not claim a live rendering reproduction or independent reviewer
  sign-off. The final review was performed by the implementing agent.
- Installed resources are external data. Three friend-window packs are absent,
  generated profile samples were unavailable, and optional `ghostPos.xml` is
  malformed. The parser rejects that XML safely; no installed files were edited.
  Unreferenced sprite trailing bytes are outside the asset-validation claim.
- Ratchets and warning budgets measure their documented patterns; zero counts
  do not prove every parser, format expression or pointer lifetime safe. Named
  library tests and source/caller review supply the additional evidence.

Live-server verification remains optional and is not a completion or merge
gate. No game executable was launched during this follow-up.

## Verification

The final behavior changes pass full Windows Debug `/RTC1`, Debug ASan and
Release builds and all 11 registered CTests: 1,021 unit tests, 81 UI tests and
six settings tests, plus source/build/wire checks. Each code slice requires
passing Linux, Windows and macOS workflows at its PR head before merging.
The final documentation cleanup removes nine obsolete commented includes;
a line comparison verifies that it changes no active source statements.
