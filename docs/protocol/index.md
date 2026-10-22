# Overview

* [Protocol overview](overview.md) - Packet classes and factories live under Client/Packet and compile as packetwire; handler bodies live under Client/PacketHandler, bound to ids in PacketHandlerRegistry.cpp, and are executable-side except the seventeen gamemodel compiles.

# Guides

* [Fuzzing the packet readers](fuzzing.md) - Three libFuzzer targets drive the game stream, the login stream and the peer datagram readers with hostile bytes under ASan and UBSan; every native tree replays their seed corpus and the fixed crashes under tests/fuzz/regressions, and each fix also carries a unit test.

# Rules

* [Format strings](format-strings.md) - A format string loaded from a data file goes through SafeFormat::Format, which checks the entry's conversions against the arguments the call site passes; R7 counts the lookup spelled at the format argument, R8 every printf-family call whose format is not a literal, and format_arity audits the sites it finds and pins how many.
* [Packet input safety](packet-input-safety.md) - Unvalidated network input is the top open risk, instrumented two ways: ratchet R3 holds server-supplied lengths into copies at zero, and the packet_indices ctest reads every packet-indexed subscript by value and fails closed on a container it does not know.
* [Wire layout](wire-layout.md) - Every factory's id, name and max body size is pinned in tests/wire-layout.txt and every packet's bytes in tests/golden/*.hex; a layout change is a protocol change shipped identically in the server, re-recorded deliberately, never to make a test pass.

# Reports

* [Packet read failure stabilization](packet-read-failure-stabilization-2026-09-05.md) - Why exact body consumption alone did not refuse a malformed body, and the stabilization that followed PR #86.
