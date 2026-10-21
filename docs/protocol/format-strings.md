---
type: Rule
title: "Format strings"
description: "A format string loaded from a data file goes through SafeFormat::Format, which checks the entry's conversions against the arguments the call site passes; R7 counts the lookup spelled at the format argument, R8 every printf-family call whose format is not a literal, and format_arity audits the sites it finds and pins how many."
tags: [protocol, security, testing]
enforced_by: [ratchets, format_arity]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Format strings

**Format strings loaded from data files are checked, never passed raw to sprintf.** Finding C19
of the [code-health review](../reviews/code-health-review-2026-08-29.md) is closed (2026-09-04,
the [restructuring plan](../reviews/RESTRUCTURING.md)'s task 5.4, fifth slice): **315 sites** are
converted to `SafeFormat::Format` in `basic/SafeFormat.h`, which checks a table entry's
conversions against the arguments the call site really passed, across `Client`, `VS_UI`, the
`AddFormat` family (through `CMessageArray::AddSafeFormat`) and `MString::FormatChecked`.

**Two ratchets hold it, and the pair is the point.** R7 counts a lookup *spelled at the format
argument*; R8 counts every printf-family call whose format is not a string literal, whatever it is
spelled as. `CLAUDE.md` claimed C19 closed once before, for about an hour on 2026-09-04, on R7
alone, while 24 live sites were reading the entry out of a static array or a local first,
invisible to it. **A ratchet at zero is a claim about the ratchet**
([ratchets](../testing/ratchets.md)). If you need to know the state of this finding, read the C19
entry in the review: it lists five measurements, and R7 is one of them. If you need to look for a
new site, sweep the *format argument*, never another spelling of the lookup.

`tests/tools/check_format_arity.pl` (ctest `format_arity`) audits every converted site against
the built-in English table and fails the suite when an entry asks for more arguments than its call
site passes. It also ratchets how many sites it finds *and* how many it resolves, because it has
three times passed while silently checking less than it should (a Perl list assignment, `find(1)`
under PowerShell, a `//` inside a string literal): a checker pins its own denominator.
