# Knowledge Bundle Update Log

## 2026-10-23

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
