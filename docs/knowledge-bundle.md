---
type: Guide
title: "The knowledge bundle"
description: "How docs/ is organised as an Open Knowledge Format v0.2 bundle, which frontmatter keys and types it uses, and how to add or move a document."
tags: [process, knowledge]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# The knowledge bundle

`docs/` is a knowledge bundle in Google's Open Knowledge Format, version 0.2
([the spec](https://github.com/GoogleCloudPlatform/knowledge-catalog/blob/main/okf/SPEC.md)): a
tree of markdown files called concepts, each starting with a YAML frontmatter block. `CLAUDE.md`
is the working brief for an agent; the rules and the incidents behind them live here, one concept
each, and `CLAUDE.md` links them. `README.md` stays the human setup guide.

# Layout

| Folder | What lives there |
|---|---|
| `architecture/` | the library boundaries, the host interfaces, the vendored de-core and the traps |
| `browser/` | the WebAssembly client, its gateway transport and its login and logout flows |
| `build/` | the two Debug trees, reading build output, CI, Linux and macOS, the warning cleanup and the port assessment |
| `protocol/` | the wire layer: layout pins, input safety, format strings and the packet-read and packet-handler fuzzers |
| `rendering/` | the GPU renderer, frame interpolation and widescreen scaling designs |
| `testing/` | what can be tested, the framework, the fix policy and the ratchets |
| `process/` | commits, source style, moves, status lines and the lessons the review rounds settled |
| `operations/` | running the client and the resource text encoding |
| `reviews/` | the restructuring plan, the code-health review and reconciliation, and the audits |
| `reports/` | investigations and stabilization write-ups |
| `history/` | records of finished migrations |

Every folder has an `index.md` listing what it holds, and [`log.md`](log.md) records changes to
the bundle, newest first. A `references/` folder inside a folder holds non-markdown material
(the sprite inventory) that its concepts link to.

# Frontmatter

```yaml
---
type: Rule                         # required
title: "Wire layout"
description: "One sentence. index.md and search show it."
tags: [protocol, testing]
enforced_by: [wire_inventory_fresh, unit_tests]   # the ctest or gate that fails when the rule breaks
---
```

- **Types in use:** Rule, Design, Guide, Runbook, Reference, Report, Review, Record.
- **`enforced_by`** names the ctest, script or hook that pins a rule. A rule without it is a
  convention that only review protects.
- **`generated: { by, at }`** marks content an agent wrote (`claude-code/<model>`). The concepts
  drafted from `CLAUDE.md`, the restructuring plan and the code on 2026-10-21 carry it; the
  documents that predate the bundle do not.
- **`verified`**: a person who checks a concept against the code adds
  `verified: { by: human:<id>, at: <ISO 8601 datetime> }`. Until then the concept is unverified,
  and the code wins any disagreement.
- Wrap a title or description in double quotes when it contains a colon followed by a space, or
  a `#`.

# Adding or moving a document

1. Put it in the folder of what it is about: a protocol rule goes in `docs/protocol/`.
2. Give it frontmatter with at least `type`, `title` and `description`.
3. Add a line for it to the folder's `index.md`, in the section for its type:

   ```markdown
   * [Title](file-name.md) - the description
   ```

4. Link to other documents with relative paths. When you move a document, grep the whole repo
   for its old path (comments in `Client/`, `basic/`, `tests/` and the scripts name docs by
   path), fix every reference, and add a `log.md` entry with the old-to-new map. Line endings
   vary by file: rewrite a path with `perl -pi -e 's{old}{new}g'` on the exact files grep found,
   never with a tool that normalises line endings.

`tests/tools/check_knowledge_bundle.pl` (ctest `knowledge_bundle`, required by
`tools/ci/verify-linux.sh` and `verify-windows.ps1`) fails the suite on a concept without a
`type`, an index that misses a concept or subfolder, an index with frontmatter, a `log.md` heading
that is not an ISO day, or a broken local link in the bundle, `CLAUDE.md`, `README.md` or
`AGENTS.md`. It needs only perl, so run it before you push a docs change:

```bash
perl tests/tools/check_knowledge_bundle.pl .
```
