---
okf_version: "0.2"
---

# OpenDarkEden client knowledge

- [Client architecture](architecture.md) - Find library boundaries, runtime composition, and the owners of architectural checks.
- [Builds and tests](builds-tests.md) - Choose the existing verification path and distinguish automated coverage from gameplay evidence.
- [Protocol and shared rules](protocol-shared-rules.md) - Locate packet contracts, cross-repository checks, and the server-owned formula source.
- [Browser login and logout](browser-login-logout.md) - Trace configuration, transport, reconnect, and browser exit without confusing their test coverage.

# Reading and maintaining this bundle

This small [Open Knowledge Format v0.2](https://github.com/GoogleCloudPlatform/open-knowledge-format/blob/main/SPEC.md)
bundle is a navigation aid. [README](../../README.md), [CLAUDE.md](../../CLAUDE.md)
and the linked implementation/test owners remain authoritative for work in the
current checkout. No generated concept claims human verification.

Concept source URLs are pinned to commit
`eabcf17f23cafebf9b60825edfc33a27e544820c`; relative links open this checkout.
`source_revision` is a bundle-specific extension recording that inspected base.
When a relevant source changes, inspect its current callers and tests, revise the
affected claims and source URLs together, and update `source_revision` and
`generated.at`. Recheck source/footnote IDs and relative links. Do not merely move
the revision or timestamp forward, or describe an unmerged change as current.
