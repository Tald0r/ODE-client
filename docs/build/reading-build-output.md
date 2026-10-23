---
type: Rule
title: "Reading build output"
description: "A clean Debug build has warnings only from third-party code and the Windows SDK, so a warning in project code is new and fails its preset's budget; judge a build by error C####, LNK, MSB or fatal error, never by grepping for \"error\"."
tags: [build]
enforced_by: [warning_policy, warning_budget_parser, tools/ci/check-warnings.pl]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T11:11:26Z }
---

# Reading build output

A clean Debug build is **57 distinct warnings (about 900 lines) and 0 errors** on the CI runner
(measured 2026-09-28), none of them in project code: IXWebSocket's C4244/C4267, `third_party/`'s
C4996 and the Windows SDK's C4668. The project's own code builds without warnings under MSVC,
Apple Clang, GCC and Clang ([the warning cleanup](compiler-warnings-2026-09-27.md)), so a warning
in `Client/`, `VS_UI/`, `basic/`, `tests/` or `tools/` is new and the budget fails on it. Clear a
C4996 through `basic/CrtCompat.h`'s portable spelling, never `_CRT_SECURE_NO_WARNINGS`.

**Judge a build by `error C####`, `error LNK`, `error MSB` or `fatal error`**, never by grepping
`"error"`, which matches about 2,700 identifiers such as `GCMoveErrorHandler`. Redirect a build
to a log file; piping it through `tail` buffers the output and hides all progress.

# The budget

`project_warnings` gives every target `/W3 /w14311 /w14312 /w14668` on MSVC and
`-Wall -Wextra -Wundef` on GCC and Clang, without `-Werror`; the `warning_policy` ctest checks
the options every target really compiles with. The verification scripts build from clean and
compare the whole log with `tools/ci/warning-baselines.json`, one budget per preset and
architecture, through `tools/ci/check-warnings.pl` (its parser is pinned by
`warning_budget_parser`). A count that rises fails, and so does one that drops until the baseline
is tightened in the same change. [tools/ci/warnings.md](../../tools/ci/warnings.md) has how
warnings are counted and the command that records a new baseline.

**There is no `aarch64` Linux budget.** On an Apple Silicon Mac, `verify-linux.sh` in the
`darkeden-linux` image stops at the warning step after ctest has passed, so a GCC- or Clang-only
warning passes every local check and fails Linux CI. Apply the x86_64 budget to the local log;
it gives CI's verdict:

```bash
perl tools/ci/check-warnings.pl --log build/verification/linux/build.log \
  --profile linux-x86_64 --baseline tools/ci/warning-baselines.json --report /tmp/warnings.json
```

Run it for `linux` and `linux-clang` and require no growth before pushing.
