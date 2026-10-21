---
type: Rule
title: "Host interfaces"
description: "A library reaches the executable only through a designated host; a reader guards the pointer and re-reads it every call, a missing host answers with the value a missing config would give, and an installer is designated-initialised."
tags: [architecture, testing]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Host interfaces

**A library never calls the executable; it asks a host.** Runtime dependencies cross named host
interfaces (`Client/Packet/WireHost.h`, `MItemHost` in `Client/MItem.h`, `MChatHost`,
`GameUI::Host` and the rest), and the executable installs its adapters in `Client/GameInit.cpp`
(`X::SetHost`). Inspect both the host and its installer when changing a callback, and follow the
callers that run before declaring a path live or dead
([measure the value](../process/measure-the-value.md)).

**A host entry answers without a host with the value a missing config would give, never
zero.** A test binary, or a host whose entries are NULL, gets the `ClientConfig` constructor's
defaults; a missing clock means no delay; a missing player means a skipped refresh rather than a
crash. Each such case was recorded as a behaviour delta where the old code dereferenced unguarded.
Host readers are private statics on the class (`MItem::Clock()`), guard the function pointer, and
are re-read on every call, never cached.

**A host installer is designated-initialised.** When `WireHost` reached 24 entries, fourteen
shared a signature with another, and the executable that installs them is never linked into a
test, so a positional slip compiled and passed the whole suite; the fourth packetwire slice
recorded the order as "checked by reading". C++20 designators make an entry in the wrong slot, or
one that does not exist, a compile error. What they cannot check is the body an entry points at.
