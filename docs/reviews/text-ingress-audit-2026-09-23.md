---
type: Review
title: "Display-text ingress audit, 2026-09-23"
description: "Where resource and network text is converted to UTF-8 before it is published, and the tests that own each boundary."
tags: [review, text]
---

# Display-text ingress audit (2026-09-23)

Display text is UTF-8. Resource readers convert the selected or declared page
before publishing strings. `TextService::NormalizeText` preserves valid UTF-8
and emits U+FFFD for each malformed byte; it never chooses another code page.
Its bounded cache is therefore independent of the resource encoding setting.

## Production boundaries

| Input | Conversion boundary / evidence |
|---|---|
| Binary string tables, NPC resources, saved mail and event names | `MString::LoadFromFile` uses `TextEncoding::GetResourceEncoding`; saves explicitly encode back. `test_mstring_files.cpp` owns record, conversion and failure contracts. |
| Loose or packed descriptions, help, chat tips, popups and notice templates | `CRarFile::OpenText` decodes through `ResourceText` once; UTF-8 line clipping preserves scalar boundaries. `test_resource_reader.cpp`, `test_help_messages.cpp`, `test_descriptor_text.cpp` and `test_mail_template.cpp` own reader/parser contracts. |
| Quest and tutorial XML | Raw `CRarFile::Open` feeds `XMLParser::parse`, which calls `ResourceText::Decode(..., true)` before constructing a tree. Declarations and BOMs override the pack page. `test_xml.cpp` owns parsing and UTF-8 save/load contracts. |
| Master commands and shrine data | Their loaders decode resource input before tokenization; `test_master_commands.cpp` and `test_regen_tower.cpp` own those contracts. |
| Source literals | MSVC `/utf-8`, the `source_encoding` CTest and `test_source_encoding.cpp` pin UTF-8 bytes, including BOM and non-BOM sources. |
| SDL text input and editor output | SDL text enters the UTF-8 editor path. `test_line_editor.cpp` and the editor storage/rendering tests cover scalar decoding, capacity and owned output. |
| Server display strings | The paired server database connection selects `utf8mb4`; database-backed NPC strings retain that encoding. Packet readers preserve bytes and framing. The renderer repairs malformed UTF-8 without changing wire layouts or writes. |

The server check used local `bound2/opendarkeden-server` revision
`009238283bda480caa53efc7426117ca29d7cfcc`: `docs/TOOLCHAIN.md`,
`src/server/database/Connection.cpp`, `ScriptManager` and the GQuest elements.
Some server quest XML contains legacy bytes, but its script/message elements
send quest/condition/element indices through `GCExecuteElement`; the client
looks up display text in its own decoded XML. That path does not send those
legacy XML strings as display payloads. Other server distributions must provide
UTF-8 display strings under this contract.

## Reader classification

Comment-aware scans of Client and VS_UI found 54 scalar line-reader sites.
Their remaining raw inputs are numeric language/configuration fields, network
endpoints, diagnostic metadata, option numbers and ignored labels, or offline
tools. The debug-only master-name reader performs a privilege comparison.
The tutorial's old briefing/book line loops are block-commented.

The raw binary-reader review separates sprite/map/audio/patch bytes, archive
member names and persisted identity keys from displayed text. `MString` and
help input have explicit decoding boundaries. Client log cleanup reads an ASCII
end marker. Skin resources contain ASCII section names and coordinates; Slayer
portal data contains binary coordinates. Neither needs text conversion. The
actual quest/computer/ghost XML consumers use the XML decoder above.

This is a source-path and library audit, not a game-rendering run. Dead native
GDI text code remains excluded by the supported SDL builds.

## Installed XML evidence

The production archive adapter and XML parser were linked from the Windows ASan
libraries and exercised offline against `TutorialEtc.rpk`:

| Member | Bytes | Declared encoding | Result |
|---|---:|---|---|
| `Computer.xml` | 1,984 | UTF-8 | Decodes and parses |
| `SimpleGQuest.xml` | 16,964 | EUC-KR | Decodes and parses |
| `EventGQuest.xml` | 19,782 | EUC-KR | Decodes and parses |
| `ghostPos.xml` | 4,093 | UTF-8 | Decodes; malformed XML is rejected |

`ghostPos.xml` has an extraneous comma in its first position element
(`x='72', y='6'`). It contains numerical positions rather than display text.
`Add_GDR_Ghost` finds no parsed position list and returns safely; those optional
ghost positions are unavailable with this asset. No installed resource was
modified. The earlier 1,733-member archive audit proved byte extraction, not XML
validity; this table records the narrower parser evidence and its exception.

## Renderer validation

Two test-first cases reproduced 132 failed checks against encoding guessing.
The renderer tests now cover every isolated high byte, overlong/surrogate/
out-of-range scalars, truncated tails, embedded NULs, preserved valid suffixes,
idempotence and independence from resource-page changes. Explicit CP949 resource
decoding still produces the expected Korean UTF-8 before rendering. The older
four output guards now assert replacement characters and complete valid text.

Full Windows Debug `/RTC1`, Debug ASan and Release builds and all 11 CTests pass:
1,011 unit tests, 81 UI tests and six user-option tests. Review was performed by
the implementing agent; this does not claim independent reviewer sign-off.
