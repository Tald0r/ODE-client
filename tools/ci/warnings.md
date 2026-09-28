# Compiler warning budgets

`project_warnings` supplies `/W3 /w14311 /w14312 /w14668` on MSVC and
`-Wall -Wextra -Wundef` on GCC/Clang to every compiled target (forwarded with
`/clang:` when using clang-cl). It does not
enable `-Werror`. The `warning_policy` CTest checks the generated transitive
options for all targets in both C and C++ contexts; `warning_budget_parser`
exercises the log parser and its rejection paths.

The verification scripts use `--clean-first`, then compare the full build log
with `warning-baselines.json`. Each preset/architecture has its own budget.
`warnings.json` in the uploaded diagnostics records both emitted warning lines
and distinct diagnostics grouped by compiler warning ID. Repeated includes and
MSBuild summaries count once per source coordinate and ID; linker/driver messages
without a coordinate are distinguished by message. Untagged Unix linker warnings
are grouped as `LD`; source warnings without an ID are `UNTAGGED`, distinguished
by coordinate and message. A count is a debt metric,
not a promise that every individual warning is unchanged: replacing one warning
with another of the same ID can leave the count unchanged.

MSVC's `LNK4217` and `LNK4286` describe the same local symbol import, with
optional caller-function detail. They share the `LNK-LOCAL-IMPORT` budget,
counted by symbol, defining object and importing object. Changing the emitted
variant does not change the debt; an additional importing object still does.

An increased count fails. A decrease also fails until the baseline is tightened
in that change. An unrecognised warning syntax, failed/no-compilation log, or absent
profile fails instead of silently returning zero. The parser cannot establish
from diagnostic text that every translation unit was compiled; the verification
script's clean build is the owner of that requirement.

After fixing warnings, build the complete preset and record its measured count:

```sh
perl tools/ci/check-warnings.pl \
  --log build/verification/macos/build.log --profile macos-arm64 \
  --baseline tools/ci/warning-baselines.json \
  --report build/verification/macos/warnings.json --record
```

Use the actual log and profile for the run: Unix profiles append `uname -m` to
the preset (`macos-arm64`, `linux-x86_64`, etc.); Windows uses
`windows-debug-x64`, `windows-release-x64`, or `windows-asan-x64`.
`--record` is an explicit maintenance command and is never invoked by CI.
Review the counts before committing them. A toolchain upgrade that changes
diagnostics needs a measured rebaseline and an explanation, rather than an
automatic increase. Do not record an incremental build or a failed build.

The 2026-09-22 packed-resource reader adds the pinned `darkeden_unrar` target
under the same warning policy. Its complete clean CI builds establish the
new dependency's initial diagnostic population, with each architecture
measured independently. GCC adds 188 dependency sites and Linux Clang adds
194; Apple Clang adds 199 on x86_64 and 200 on arm64. The archive listing
implementation also removes one project `-Wunused-parameter` site, so the
net Unix increases are one smaller. New warning locations are in the bundled
source, including its shared headers; no project-source growth is hidden.

Windows adds ten upstream C4996 sites and SDK `winioctl.h` C4668 diagnostics.
The hosted SDK emits 16 new C4668 sites; the local SDK emits 29. The recorded
Windows budgets use the hosted CI toolchain's complete clean builds, rather
than the larger local measurement. No compiler warnings or sanitizers are
disabled for the dependency; later changes retain the same comparison
against the recorded counts.

The native GPU renderer removes the unused `flags` parameter diagnostic in
`SpriteLibBackendSDL.cpp` and the signed comparison in `VS_UI_DESC.cpp`.
The Unix parameter budgets also include the prior exchange-filter change's
removed parameter warning. Two exchange UI test aggregates now explicitly
initialize `sellerFilter`, resolving the missing-field warnings inherited
from that change. Windows loses one C4244 site after using an explicit cast
for the already-clipped RLE row bound and removing the old light-buffer pitch
conversion. The budgets tighten these removals without increasing any category.

The browser/WebSocket port adds pinned IXWebSocket v12.0.1 to native builds under
the same warning and sanitizer policy. Complete clean CI logs establish its
initial warning population: GCC adds 14 sites and Linux Clang adds 22, all within
the dependency. Apple Clang adds 34, including 14 deprecation diagnostics for
its Secure Transport backend. Compression-disabled stubs account for most unused
parameter/field diagnostics. These are measured dependency additions; warnings
remain enabled and no project-source warning growth is folded into the budgets.
Windows adds 25 upstream C4244 sites and six C4267 sites. Four new project
`getenv` deprecation sites found during the first clean build were replaced by
an owned configuration reader using `_dupenv_s` on Windows; the C4996 budget
therefore stays unchanged.

The 2026-09-27 cleanup (`docs/compiler-warnings-2026-09-27.md`) cleared every
warning GCC and Clang report in the project's own code, so the Unix budgets
now count only vendored code and the linker. All ten profiles were re-recorded
from the complete clean CI builds of that branch: macOS 4,048 -> 243 (arm64)
and 4,047 -> 242 (x86_64), the same for the ASan presets; Linux GCC 3,998 ->
205, GCC with sanitizers 3,997 -> 205, Clang 4,076 -> 221; Windows Debug and
ASan 1,775 -> 1,254 and Release 1,547 -> 1,254. On Windows the C4297, C4101
and C4018 budgets fall to zero; what is left there is mostly the C4267/C4244
narrowing conversions and C4996 CRT deprecations that GCC and Clang do not
raise under `-Wall -Wextra`. No category grew in any profile.

The MSVC follow-up in the same document cleared the 1,106 project warnings
only MSVC reports (C4267/C4244/C4312 conversions, C4996 deprecations through
`basic/CrtCompat.h`, C4273 and the `LNK-LOCAL-IMPORT` pairs, C4005, C4116,
C4477 and the C4146 fix). The three Windows profiles were re-recorded from
the complete clean builds of workflow_dispatch run 36360661797: Debug, ASan
and Release 1,254 -> 57. What is left is the dependency population recorded
above - IXWebSocket's 25 C4244 and six C4267, `third_party/`'s ten C4996 and
the SDK's 16 C4668 - so no Windows budget counts project code any more. The
Unix budgets are unchanged.
