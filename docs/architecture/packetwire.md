---
type: Design
title: "The packetwire library"
description: "Every .cpp under Client/Packet is compiled once as packetwire, the wire layer that reaches the program around it only through WireHost.h's entries the executable installs in GameInit.cpp."
tags: [architecture, protocol]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# The packetwire library

# Purpose

`packetwire` is the whole wire layer compiled once as a static library, so packet tests construct
real packets through the real factories and the fuzzers drive the real readers. It holds the
sockets (TCP and datagram), the socket streams, the `Player` base under both player classes, the
game-server player and the inbound peer player with its manager, the encrypter, the info classes,
every packet class in every direction and the factory/validator tables
([restructuring plan](../reviews/RESTRUCTURING.md) tasks 1.1, 2.4 and 5.1).

# Membership

**Every `.cpp` under `Client/Packet` is a member** since 2026-09-09; the list is
`tests/arch/packetwire_files.txt`. `tests/arch/packetwire_holdouts.txt` is empty and is where the
next exception, if one is ever needed, gets written down with what it *reaches*, not what it
includes. The include checker (`tests/arch/check_includes.pl`, ctest `arch_includes`) walks the
quote-include closure of the library and fails on a forbidden edge: a member may include only
files under `Client/Packet`, `basic` and the precompiled header (W1), and never the debug
facilities the executable defines (W2).

# The host

What the wire layer needs from the program around it goes through `Client/Packet/WireHost.h`,
11 entries the executable installs in `Client/GameInit.cpp`: packet and request-service limits,
the UDP port, the bug-report target, the encrypter's zone and server ids and seed choice, the
current time, and the other-request registry. Every entry follows
[host interfaces](host-interfaces.md): a test binary runs with no host installed and gets the
value a missing configuration would give.

# Logging and formatting

The logging facility (`basic/DebugLog.h`) is in `basic`, so every library may log;
`Client/DebugInfo.h` is the executable's front end to it and pulls in `MinTr.h`, which is why the
libraries may not include it. The checked formatter (`basic/SafeFormat.h`,
[format strings](../protocol/format-strings.md)) is in `basic` for the same reason: the call sites
that need it are in the executable, in `VS_UI` and in the packet handlers, and `basic` is the one
library all three link.
