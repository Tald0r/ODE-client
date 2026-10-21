# Guides

* [Testing overview](overview.md) - What the test binaries and script ctests cover, how to configure and run them in a plain and an ASan tree, and the measured baseline the suite last read.
* [The test framework](test-framework.md) - tests/framework/test_framework.h is a minimal self-registering framework, not GoogleTest: TEST, CHECK and CHECK_EQ are the whole API, tests/unit/*.cpp is globbed, and RunAll returns the failure count.

# Rules

* [Executable-only code](executable-only-code.md) - Code compiled straight into DarkEden cannot be linked by the unit binary, so it is verified with builds and the automated checks; live-server verification is optional and never a merge or completion gate.
* [Library fixes are test-first](library-fixes-test-first.md) - A library fix starts with a failing test that asserts the observable contract, not the crash, then runs green in both a plain and an ASan tree; a fix that could not be reproduced is called a regression guard, and substantial remediation gets an adversarial review.
* [Link proofs](link-proofs.md) - A link proof takes the address of non-virtual members, because a pointer to a virtual member is a vtable index that links with the file removed; verify a proof by taking the file back out.
* [Ratchets](ratchets.md) - tests/ratchet/ratchets.sh holds every shrink-only baseline inline and fails when a count rises or drops unrecorded; each ratchet's comment says what it cannot see, a ratchet at zero is a claim about the ratchet, and a checker pins its own denominator.
* [Uninitialised members are reproduced, not guarded](uninitialised-members.md) - /RTC1 fills stack locals with 0xCC and the CRT fills the heap with 0xCD, but neither flags an uninitialised member, so a test reads the fill pattern out of a fresh object rather than waiting for a crash.
