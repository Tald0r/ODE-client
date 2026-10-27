# Reviews

* [Code health review, 2026-08-29](code-health-review-2026-08-29.md) - 197 findings from eight parallel reviewers reading the whole client tree, ranked by severity, with the fix status of each.
* [Code-health reconciliation, 2026-09-23](code-health-reconciliation-2026-09-23.md) - The current disposition of the review's 197 findings, repaired, removed or verified, and what the historical dated summaries no longer describe.
* [Sprite asset audit, 2026-09-23](sprite-asset-audit-2026-09-23.md) - Every installed sprite file read through the production decoders under Windows ASan, 193 files and 345,531 records, as parser evidence rather than a rendering run; its inventory is [references/sprite-asset-inventory-2026-09-23.tsv](references/sprite-asset-inventory-2026-09-23.tsv).
* [Display-text ingress audit, 2026-09-23](text-ingress-audit-2026-09-23.md) - Where resource and network text is converted to UTF-8 before it is published, and the tests that own each boundary.

# Records

* [Client restructuring plan](RESTRUCTURING.md) - The extraction plan that split the executable into testable libraries, complete as of 2026-09-09, with every task's owner, the ratchet table and what the review rounds settled.
* [Next work plan](NEXT.md) - The ranked, self-contained tasks left after the 2026-10-25 merges, each written so an agent with no other context can take it, plus the owner decisions no agent may make.
