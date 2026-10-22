---
okf_version: "0.2"
---

# Guides

* [The knowledge bundle](knowledge-bundle.md) - How docs/ is organised as an Open Knowledge Format v0.2 bundle, which frontmatter keys and types it uses, and how to add or move a document.

# Folders

* [Architecture](architecture/) - Which code lives in a static library and can be tested, the host interfaces the libraries reach the executable through, the vendored de-core, and the traps that cost time to rediscover.
* [Browser](browser/) - The WebAssembly client, the WebSocket gateway transport and the browser's login and logout flows.
* [Build](build/) - The two Debug trees, how to read a build, CI on master only, the compiler-warning cleanup and the Linux and macOS port assessment.
* [History](history/) - Records of finished migrations.
* [Operations](operations/) - Running the client from the debugger, the DLL whitelist and the packed resources.
* [Process](process/) - Commit messages, source style, move-then-fix, the restructuring status lines and the lessons the review rounds settled.
* [Protocol](protocol/) - The wire layer: the pinned layout and goldens, server-supplied lengths and indices, format strings, and the packet-read fuzzers.
* [Rendering](rendering/) - The native GPU renderer, 60 fps interpolation over the logic tick and widescreen scaling.
* [Reports](reports/) - Investigations and stabilization write-ups.
* [Reviews](reviews/) - The restructuring plan, the code-health review and its reconciliation, and the sprite and text audits.
* [Testing](testing/) - What can be tested, the test framework, the test-first fix policy, executable-only code and the ratchets.

# Changes

* [Update log](log.md) - What changed in the bundle, newest first, including where each moved document went.
