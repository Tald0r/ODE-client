# de-core (vendored from the server)

`domain/` is a byte-identical copy of part of the server's de-core library,
`src/domain` in [bound2/opendarkeden-server](https://github.com/bound2/opendarkeden-server):
the rules the client and the server both compute, implemented once, there.

Last synced from server commit: 831a8edc7f673c71d35c1f9b5630249412b86e0b

**Never edit anything under `domain/`, and never reformat it.** Change the
rule on the server, then resync:

```bash
perl tools/decore/sync.pl ../opendarkeden-server
```

Always pass the server root; no default is right for every pair of
checkouts. The script copies the subset, deletes whatever left it, rewrites
`MANIFEST` and the commit line above. The subset is the server's
`DECORE_VENDORED_SOURCES` (`src/domain/CMakeLists.txt`), the domain
headers those include, and the parity vectors in `src/domain/vectors/`.
When a sync adds a `.cpp`, add it to the explicit source list in
`CMakeLists.txt` here; `decore_vendored` fails until you do.

What keeps the copy honest:

| Check | Catches |
|---|---|
| `decore_vendored` ctest (`sync.pl --verify-manifest`) | a hand edit, an unlisted file, a `.cpp` the target does not build |
| `decore_tests` ctest, and under node in `web.yml` | a toolchain that computes a different number from the same source |
| `arch_includes` rule DC1 | an include other than an existing `"domain/X.h"`, `<algorithm>` or `<cmath>` |
| `decore-upstream` job in `linux.yml` (`sync.pl --check`) | the server changed the subset and this copy was not resynced |
| the server's `tests/tools/decore_client_diff.sh <client-root>` | the same, from the server side, before a server change merges |

The vector files (`domain/vectors/*.tsv`) are shared input and expected
rows. The server records them; `decore_tests` asserts every row on every
client toolchain. A mismatch is a toolchain difference to investigate, never
a row to re-record here.

`CMakeLists.txt`, `README.md` and `MANIFEST` belong to the client. The
build flags `-ffp-contract=off -fno-fast-math` (`/clang:-ffp-contract=off`
under clang-cl; MSVC's default `/fp:precise` does not contract) keep the
floating-point arithmetic the same as the server's build. The files are
LF on every checkout (`.gitattributes`), so the hashes hold on Windows too.

The price and durability differences from the server that remain after
slices 1 and 2 are listed under "Shop prices" in
`docs/compiler-warnings-2026-09-27.md` ("Found while fixing"). Those of
the equip requirement (slice 4), such as the advancement-class check the
server applies and de-core does not hold, and those of Will of Life and
the slayer skill range (slice 6), such as the party size every caller
passes as 0 and the skill data the client loads from `SkillInfo.inf`,
are in `docs/RESTRUCTURING.md` task 4.12.
