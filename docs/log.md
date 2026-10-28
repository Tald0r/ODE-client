# Knowledge Bundle Update Log

## 2026-10-28

* **Update**: [packet input safety](protocol/packet-input-safety.md) writes up the walk of the
  view's name pointers and the three things it found open (HP bars overflowing an `int`, a fake
  creature's `NULL` name in `strncmp`, an item name's uninitialised box), fixed with rows in the
  [code-health review](reviews/code-health-review-2026-08-29.md); the
  [next work plan](reviews/NEXT.md) moves task 5 (c) to *Done*.

## 2026-10-27

* **Update**: [packet input safety](protocol/packet-input-safety.md) writes up the descriptor
  renderers' walk and the three server values it found reaching a `NULL` name (a bomb's unknown
  skill, an unknown item type's names, a pet under level ten), fixed with rows in the
  [code-health review](reviews/code-health-review-2026-08-29.md); the
  [next work plan](reviews/NEXT.md) moves task 5 (b) to *Done*.
* **Update**: [packet input safety](protocol/packet-input-safety.md) writes up the text system's
  handling of a server string as followed and clean; the [next work plan](reviews/NEXT.md) narrows
  task 5 to the routes left.
* **Update**: the [native GPU renderer](rendering/native-gpu-renderer.md) page says the gradation
  table sums three five-bit channels, as the original did; the [next work plan](reviews/NEXT.md)
  moves its task 9 to *Done*.
* **Update**: the [native GPU renderer](rendering/native-gpu-renderer.md) page says how
  `BltIndexSpriteColor` and `BltIndexSpriteBrightness` draw now that they are no stubs, and that a
  black pixel of the three decoded effect draws is opaque; the [next work plan](reviews/NEXT.md)
  moves its task 6 to *Done*.
* **Update**: the [next work plan](reviews/NEXT.md) moves its task 8 to *Done*; the
  [restructuring plan](reviews/RESTRUCTURING.md)'s 4.93 follow-up on the SMS list's units says it
  is fixed.
* **Update**: the [native GPU renderer](rendering/native-gpu-renderer.md) page says the palette
  gradation effect, the darker filter and the 1:5:5:5 conversions take a six-bit green the right
  way, and that none is drawn; the [next work plan](reviews/NEXT.md) moves its task 7 to *Done* and
  adds task 9, the gradation table's two green scales.
* **Update**: the `Assert`-only bounds row of the [code-health review](reviews/code-health-review-2026-08-29.md)
  is fixed; the [next work plan](reviews/NEXT.md) moves its task 3 to *Done* with the follow-ups it
  leaves; [fuzzing](protocol/fuzzing.md) says where the game target's failed `Assert`s were.
* **Update**: the login-list leak row of the [code-health review](reviews/code-health-review-2026-08-29.md)
  is fixed with every site of its shape; the [next work plan](reviews/NEXT.md) moves its task 2 to
  *Done* and adds task 8 for the packets that never free a member their read allocates; the
  [restructuring plan](reviews/RESTRUCTURING.md)'s follow-ups for `GCPetStashList`,
  `GCNicknameList` and `GCGQuestStatusModify` say they are fixed.
* **Update**: the two enum rows of the [code-health review](reviews/code-health-review-2026-08-29.md),
  *The same cast where UBSan does not look* and the PC infos' sex, are fixed, with the eight sites
  two sweeps found; the [next work plan](reviews/NEXT.md) moves its task 1 to *Done* and widens task
  2 to the other sites that leak an entry whose read throws; [fuzzing](protocol/fuzzing.md) says a
  cast value that is never loaded is seen by no sanitizer.

## 2026-10-26

* **Creation**: [next work plan](reviews/NEXT.md), the ranked tasks left in the client after
  the 2026-10-25 merges, each self-contained enough for an agent without this history to take it;
  `CLAUDE.md`'s current focus links it.

## 2026-10-25

* **Update**: [packet input safety](protocol/packet-input-safety.md) records the walk of server
  values into the UI, what it found guarded and what it did not follow; the
  [code-health review](reviews/code-health-review-2026-08-29.md) gains the creature direction row it
  found, fixed; `CLAUDE.md`'s current focus says the walk is done.
* **Update**: the `GCUpdateInfoHandler` row of the [code-health review](reviews/code-health-review-2026-08-29.md)
  is fixed, and [packet input safety](protocol/packet-input-safety.md) and `CLAUDE.md`'s current
  focus no longer call it open.
* **Update**: the [native GPU renderer](rendering/native-gpu-renderer.md) page says how
  `BltIndexSpriteColorSet` draws now that it is no stub, and what a black decode does.
* **Update**: the blood bible row of the [code-health review](reviews/code-health-review-2026-08-29.md)
  is fixed with the three other server zone lookups it names, and a new open row records
  `GCUpdateInfoHandler`'s. [Packet input safety](protocol/packet-input-safety.md) says the four sites
  the audit met are fixed and the walk is next, and `CLAUDE.md`'s current focus follows.
* **Update**: the two guild-rank rows of the [code-health review](reviews/code-health-review-2026-08-29.md)
  are fixed, and [packet input safety](protocol/packet-input-safety.md) says so.

## 2026-10-24

* **Update**: the mail-date row of the [code-health review](reviews/code-health-review-2026-08-29.md)
  is fixed, and [packet input safety](protocol/packet-input-safety.md) says so.
* **Update**: [packet input safety](protocol/packet-input-safety.md) records the audit of server
  strings into fixed buffers. It found no site, and it names server values used as UI indices as
  the next item. The [code-health review](reviews/code-health-review-2026-08-29.md)'s *Found by
  reading* lists the four sites the audit met on the way, and `CLAUDE.md`'s current focus follows.

## 2026-10-23

* **Update**: the review of the handler fuzz targets. [Fuzzing](protocol/fuzzing.md) says the
  world now holds the English game strings, resets what replies leave behind, and aborts a run that
  disconnects its socket; it lists the handler targets in its build line and corrects its claim
  about the UDP manager's NULL tests. The [code-health review](reviews/code-health-review-2026-08-29.md)
  records the seven values found, the harness gaps the review and the later run found (the
  game strings, the nickname table, the per-run reset), and four casts UBSan cannot see as open; the [index](index.md) and [the knowledge bundle](knowledge-bundle.md) name the
  handler fuzzers.
* **Update**: the [code-health review](reviews/code-health-review-2026-08-29.md)'s *Found by
  fuzzing* records the handler targets' first runs and their findings, and
  [fuzzing](protocol/fuzzing.md) summarises them.
* **Update**: [fuzzing](protocol/fuzzing.md) is now *Fuzzing the packet readers and handlers*:
  two libFuzzer targets run the client's receive loop into the packet handlers, in the world
  `tests/fuzz/handler_world.h` builds, with how a run is reset, what counts as a finding and how a
  harness gap is told apart from a handler bug; [testing overview](testing/overview.md) lists
  their replay ctests, and `CLAUDE.md`'s fuzzing lines follow.
* **Update**: the review of the `CLAUDE.md` rewrite. The path-mismatch comment in
  `CMakeLists.txt` now points at the [restructuring plan](reviews/RESTRUCTURING.md)'s task 4.0,
  not the LNK4217 note; the three code comments that cited `CLAUDE.md`'s *Traps* point at
  [client traps](architecture/client-traps.md); the headless Linux run, the `<SDL.h>` rule and
  the 2026-09-30 macOS ctest run are owned by [Linux and macOS](build/linux-and-macos.md);
  the line-ending rule says endings vary by file ([source style](process/source-style.md)); and
  the README's link-and-copy data recipe moves from its macOS section to its Linux one.
* **Update**: `README.md` is trimmed to the path from a clone to a running client. Its Windows
  presets and `windows-clang` detail, the sanitized tree's build steps and the full Debug-tree
  table now live in [build commands](build/build-commands.md); the Docker recipes and the Linux and
  macOS status in [Linux and macOS](build/linux-and-macos.md); the encoding detail in
  [resource text encoding](operations/resource-text-encoding.md); the xBRZ setting in
  [native GPU renderer](rendering/native-gpu-renderer.md#the-xbrz-setting). The README links
  each; `tools/linux/Dockerfile` now names the concept that records the x86_64 run.
* **Update**: `CLAUDE.md` is rewritten as the bundle's index, one line per rule and a link to
  its concept, keeping the user's instructions as lines. References to its removed sections are
  repointed: the LNK4217 note (`CMakeLists.txt`) to
  [reading build output](build/reading-build-output.md), the golden counts
  (`tests/unit/test_packet_goldens.cpp`) to [wire layout](protocol/wire-layout.md), the baseline
  (`tools/ci/verify-linux.sh`) to [testing overview](testing/overview.md), and *Testability*,
  *Fuzzing the packet readers* and the `_DEBUG` trap, in five documents here, to
  [library boundaries](architecture/library-boundaries.md), [fuzzing](protocol/fuzzing.md) and
  [client traps](architecture/client-traps.md).
* **Creation**: [Linux and macOS](build/linux-and-macos.md) and
  [resource text encoding](operations/resource-text-encoding.md), from the README's platform
  sections and its *Resource text encoding* section, which the README now links instead of
  restating in full.
* **Move**: what `CLAUDE.md` and `README.md` stated and no concept did moves into one: the
  per-class detail of what `gamemodel` holds ([gamemodel](architecture/gamemodel.md)), the source
  layout ([library boundaries](architecture/library-boundaries.md#source-layout)), the LNK4217
  note ([reading build output](build/reading-build-output.md)), the full Debug-tree table, the
  sanitized tree's build steps, the Windows presets and `windows-clang`
  ([build commands](build/build-commands.md)), the Windows workflow's cache ([CI](build/ci.md)),
  what the last baseline measured and how ([testing overview](testing/overview.md#baseline)), the
  fuzz preset's verification ([fuzzing](protocol/fuzzing.md)) and the xBRZ setting
  ([native GPU renderer](rendering/native-gpu-renderer.md#the-xbrz-setting)).
* **Fix**: the code disagreed with concepts drafted on 2026-10-21. Every packet handler, not
  seventeen, is a `gamemodel` member, and the executable is `Client/SDLMain.cpp` alone (task
  4.265), so [gamemodel](architecture/gamemodel.md),
  [library boundaries](architecture/library-boundaries.md),
  [protocol overview](protocol/overview.md) and
  [executable-only code](testing/executable-only-code.md) say so. `Client/D3DLib/`, `VolumeLib/`,
  `DEUtil/`, `MZLib/` and `TextLib/`, which `CLAUDE.md`'s layout table listed, no longer exist.
  Every remaining source glob uses `CONFIGURE_DEPENDS`, so the reason
  [library boundaries](architecture/library-boundaries.md) and
  [move, then fix](process/move-then-fix.md) gave for explicit lists is replaced.
* **Fix**: [build/index.md](build/index.md), [build commands](build/build-commands.md),
  [reading build output](build/reading-build-output.md) and [CI](build/ci.md), which the
  restructuring below linked but `.gitignore`'s `[Bb]uild/` kept out of its commit, are written
  from `CLAUDE.md`, the workflows and `tools/ci/`; `.gitignore` re-includes `docs/build/`.

## 2026-10-21

* **Creation**: the bundle is restructured like the sidecar's: `docs/` is the bundle root with
  [index.md](index.md), [the knowledge bundle](knowledge-bundle.md) and this log; every concept
  carries frontmatter; every folder has an index; `tests/tools/check_knowledge_bundle.pl` (ctest
  `knowledge_bundle`) owns the shape. The four navigation concepts under `docs/knowledge/`
  (`architecture.md`, `builds-tests.md`, `protocol-shared-rules.md`, `browser-login-logout.md`)
  are superseded: their claims live in [library boundaries](architecture/library-boundaries.md),
  [testing overview](testing/overview.md) and [build commands](build/build-commands.md),
  [wire layout](protocol/wire-layout.md) and [de-core](architecture/decore.md), and
  [login and logout](browser/login-and-logout.md). The `source_revision` and `sources` footnote
  convention they used is dropped: concepts link files in the checkout with relative paths.
* **Creation**: the rules `CLAUDE.md` states and the lessons the restructuring plan's *What the
  review rounds settled* section records, one concept each, under `architecture/`, `build/`,
  `protocol/`, `testing/`, `process/` and `operations/`.
* **Move**: the documents that sat at the `docs/` root moved into folders and got frontmatter;
  every reference in the repo (`CLAUDE.md`, `README.md`, CMake files, scripts, the commit hook
  and comments in `Client/`, `VS_UI/`, `basic/` and `tests/`) was rewritten to the new path.
  * `docs/RESTRUCTURING.md` -> `docs/reviews/RESTRUCTURING.md`
  * `docs/code-health-review-2026-08-29.md` -> `docs/reviews/code-health-review-2026-08-29.md`
  * `docs/code-health-reconciliation-2026-09-23.md` -> `docs/reviews/code-health-reconciliation-2026-09-23.md`
  * `docs/sprite-asset-audit-2026-09-23.md` -> `docs/reviews/sprite-asset-audit-2026-09-23.md`
  * `docs/sprite-asset-inventory-2026-09-23.tsv` -> `docs/reviews/references/sprite-asset-inventory-2026-09-23.tsv`
  * `docs/text-ingress-audit-2026-09-23.md` -> `docs/reviews/text-ingress-audit-2026-09-23.md`
  * `docs/webgl-client.md` -> `docs/browser/browser-client.md`
  * `docs/knowledge/browser-login-logout.md` -> `docs/browser/login-and-logout.md` (rewritten as a runbook)
  * `docs/native-gpu-renderer.md` -> `docs/rendering/native-gpu-renderer.md`
  * `docs/frame-rate-interpolation-2026-08-30.md` -> `docs/rendering/frame-rate-interpolation-2026-08-30.md`
  * `docs/widescreen-scaling-2026-08-30.md` -> `docs/rendering/widescreen-scaling-2026-08-30.md`
  * `docs/compiler-warnings-2026-09-27.md` -> `docs/build/compiler-warnings-2026-09-27.md`
  * `docs/linux-macos-port-assessment-2026-09-07.md` -> `docs/build/linux-macos-port-assessment-2026-09-07.md`
  * `docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md` -> `docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md`
  * `docs/memory-leak-investigation-2026-08-30.md` -> `docs/reports/memory-leak-investigation-2026-08-30.md`
  * `docs/skill-repeat-diagnostics-2026-08-31.md` -> `docs/reports/skill-repeat-diagnostics-2026-08-31.md`
  * `docs/modernization-stabilization-2026-09-05.md` -> `docs/reports/modernization-stabilization-2026-09-05.md`
  * `docs/packet-read-failure-stabilization-2026-09-05.md` -> `docs/protocol/packet-read-failure-stabilization-2026-09-05.md`
