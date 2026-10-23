---
type: Rule
title: "CI"
description: "The four workflows run on master pushes and manual dispatch only, never on a pull request, so a change is verified locally and dispatched by hand; the verification scripts assert the required ctests by name."
tags: [build, process]
enforced_by: [tools/ci/verify-linux.sh, tools/ci/verify-windows.ps1]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T11:11:26Z }
---

# CI

**CI runs only on pushes to `master` and manual `workflow_dispatch`.** Pull-request triggers stay
disabled to conserve GitHub Actions minutes (user instruction, 2026-09-25). Verify a change
locally ([build commands](build-commands.md#the-full-check)) and dispatch the workflows on its
branch when it needs CI:

```bash
for w in windows linux macos web; do gh workflow run $w.yml --ref <branch>; done
```

| Workflow | Jobs |
|---|---|
| `windows.yml` | `verify-windows.ps1` over the `windows` (Debug and Release) and `windows-asan` presets |
| `linux.yml` | `verify-linux.sh` over `linux` (GCC), `linux-clang` and `linux-asan`; `decore-upstream` compares the vendored server rules with the server's master |
| `macos.yml` | `verify-linux.sh` over `macos` and `macos-asan`, each on an arm64 and an Intel runner |
| `web.yml` | the WebAssembly client and its probes, de-core's vectors in WebAssembly, the compressed assets, and the WebGL pixels and login/world handoffs against the gateway fixture ([browser client](../browser/browser-client.md)) |

Each native verify job uploads its `build/verification/` diagnostics and the browser job its
build and logs (7 days); the plain `windows`, `linux` and `macos` jobs also upload the packaged
client (30 days).

**A skipped check is not a pass.** ctest exits 0 over an empty inventory, and a perl or bash
check that could not be registered is simply absent, so `verify-linux.sh` and
`verify-windows.ps1` each hold the same list of required tests and fail when one is missing
from the tree ([testing overview](../testing/overview.md)). A new ctest that must run everywhere
goes on both lists.

**A docs-only change still needs `knowledge_bundle`.** Since nothing runs before the merge, run
`perl tests/tools/check_knowledge_bundle.pl .` before pushing one
([the knowledge bundle](../knowledge-bundle.md)) and check `git status` lists every file you
wrote: `.gitignore` ignores every `build/` folder but this one, and a page it swallowed broke
every native verify job on master on 2026-10-22.
