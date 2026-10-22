---
type: Rule
title: "Ratchets"
description: "tests/ratchet/ratchets.sh holds every shrink-only baseline inline and fails when a count rises or drops unrecorded; each ratchet's comment says what it cannot see, a ratchet at zero is a claim about the ratchet, and a checker pins its own denominator."
tags: [testing, process]
enforced_by: [ratchets]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Ratchets

**Ratchet numbers only go down.** `tests/ratchet/ratchets.sh` (ctest `ratchets`) holds the
baselines inline and fails the suite when a count **rises** or drops unrecorded, so tightening
lands in the same commit as the progress. Measurements are grep-only by design; nothing is
generated or overwritten. Re-measure with the given command before and after a change that claims
progress; a recorded growth (a split that adds an executable translation unit) moves the baseline
with the reason written beside it. The table of R1 to R18, with what each counts and the history
of every baseline move, is the [restructuring plan](../reviews/RESTRUCTURING.md#ratchets-shrink-only);
the script's comment on each ratchet is the authority on what it cannot see. Compiler warnings
have a budget of their own ([reading build output](../build/reading-build-output.md)).

**A ratchet at zero is a claim about the ratchet.** R7 reached 0 and was read as finding C19
closed while 24 live sites used an entry copied into a static array first
([format strings](../protocol/format-strings.md)). Anything stronger than "the pattern sees
nothing" needs a different instrument, and a new instrument gets the same adversarial reading as
the code; it will not get it from the person who just built it (R8 was recorded at 13, and the
population was 43).

**Two ratchets on the same finding see different things.** R3 counts `sprintf` lines but `\b`
rejects the `w` in `wsprintf`; R7 counts both. R4 greps `g_p*` and cannot see a library file
calling an executable-side *function* (which is what [link proofs](link-proofs.md) are for) or a
global under another name (`g_Mode`). Read each ratchet's comment for what it cannot see before
quoting its number.

**A checker pins its own denominator.** `tests/tools/check_format_arity.pl` passed three times
while checking nothing (a Perl list assignment, `find(1)` under PowerShell, a `//` inside a
string literal). Both the sites it finds and the sites it resolves are floors now; every way a
scan can shrink must fail rather than report a smaller number and exit 0.
