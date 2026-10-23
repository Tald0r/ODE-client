---
type: Reference
title: "Resource text encoding"
description: "Resource records decode to UTF-8 from the pack's configured code page and encode back to it on save; XML and BOM-marked text override the pack default, and the XML parser's limits reject a hostile document without touching the loaded tree."
tags: [operations, text]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# Resource text encoding

`MString` resource records decode to UTF-8 when loaded and encode back to the same configured code
page when saved. The shipped Korean pack defaults to CP949. A converted or regional pack sets its
actual encoding in `Data/Info/FileDef.inf` ([README](../../README.md#resource-text-encoding)):

```text
RESOURCE_TEXT_ENCODING : UTF-8
```

- **Values**: `UTF-8`, `CP949`, `EUC-KR`, `GBK`, `GB2312` and `BIG5`, case insensitive. The
  setting applies to the whole pack and is read before the resource tables load
  (`Client/GameInit.cpp`); it does not change network text. An unknown name fails initialization.
- **Damaged bytes** display as replacement characters while later text stays readable. A save
  fails if the text cannot be represented in the selected encoding or its encoded record exceeds
  65,536 bytes.
- **XML resources** honour their `encoding` declaration or a UTF-8 BOM before the pack default.
  Their trees store UTF-8, and an XML save declares UTF-8 and escapes markup. The parser rejects
  truncated documents, unsupported DTDs, files over 16 MiB, nesting beyond 64 levels, and more than
  100,000 elements and attributes per document (`Client/SXml/SXml.h`, `VS_UI/XmlReader.cpp`); a
  rejected document leaves the existing tree intact.
- **Chat tips, popup files and help messages** follow the same policy, with a UTF-8 BOM overriding
  the pack default. A help-message reload keeps the previous collection if a document is
  incomplete or invalid; a save writes UTF-8 with a BOM, so it reads back under any configured pack
  encoding.

Where each kind of resource and network text is converted, and the test that owns each boundary,
is the [display-text ingress audit](../reviews/text-ingress-audit-2026-09-23.md).
