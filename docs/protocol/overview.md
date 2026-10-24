---
type: Design
title: "Protocol overview"
description: "Packet classes and factories live under Client/Packet and compile as packetwire; handler bodies live under Client/PacketHandler, bound to ids in PacketHandlerRegistry.cpp, and all compile into gamemodel."
tags: [protocol, architecture]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# Protocol overview

# Layout

- `Client/Packet/` is the wire layer, compiled once as [packetwire](../architecture/packetwire.md):
  every packet class in every direction (`Gpackets/` is server to client), the factories, the
  validator tables, the sockets, streams and the encrypter. A packet's `read()`, `write()` and
  `getPacketMaxSize()` are its [wire layout](wire-layout.md).
- `Client/PacketHandler/` holds the handler bodies, all bound to their ids in
  `Client/PacketHandlerRegistry.cpp` and all compiled into
  [gamemodel](../architecture/gamemodel.md), so a test binary links them; each reaches the UI and
  the live game through a named host.
- The executable installs the wire host services (`Client/Packet/WireHost.h`, in
  `Client/GameInit.cpp`) and dispatches the handlers.

# Tracing a failure

Keep parsing, model mutation and UI or runtime effects distinct. A packet's bytes reach a parser
only through the gates `ClientPlayer::processCommand` runs (`tests/fuzz/client_stream_reader.h`
lists them with their line numbers; keep the two in step); a parsed packet reaches a handler; the
handler reaches the model and, through hosts, the UI. The instruments differ by layer: the
[fuzzers](fuzzing.md) drive the readers and, through the receive loop, the handlers and the
model, [packet input safety](packet-input-safety.md) counts what
handlers do with server-supplied lengths and indices, and the model handlers have unit tests.

# The server's copy

Every packet class is hand-maintained in both repos. The server's `tests/golden` and
`tests/wire-layout.txt` are produced from its own classes; `diff -r` of the two golden
directories and `server/tests/tools/wire_inventory_diff.sh` are the cross-repo checks, and a
difference there is a protocol bug in one repo or the other ([wire layout](wire-layout.md)). The
formulas the two share are vendored, not re-implemented ([de-core](../architecture/decore.md)).
