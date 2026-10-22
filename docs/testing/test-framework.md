---
type: Guide
title: "The test framework"
description: "tests/framework/test_framework.h is a minimal self-registering framework, not GoogleTest: TEST, CHECK and CHECK_EQ are the whole API, tests/unit/*.cpp is globbed, and RunAll returns the failure count."
tags: [testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# The test framework

`tests/framework/test_framework.h` is a minimal self-registering framework, not GoogleTest. Tests
register during static initialisation and `tests/unit/*.cpp` is globbed, so adding a test means
adding a file; there is no runner to edit.

```cpp
TEST(TArray, LoadFromFileRejectsCountLargerThanTheFile)
{
	CHECK_EQ(false, arr.LoadFromFile(truncated));
}
```

`CHECK(expr)` and `CHECK_EQ(expected, actual)` are the whole API. `RunAll()` returns the failure
count, so the exit code is 0 only when the suite is clean. A test names what it checks in its
suite and name, as above, and asserts the observable contract
([library fixes are test-first](library-fixes-test-first.md)).
