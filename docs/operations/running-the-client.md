---
type: Runbook
title: "Running the client"
description: "DarkEden.exe runs from the Visual Studio debugger and is not launched by an agent unless asked; an unlisted DLL beside it exits -1 before main, resources are read loose first and then from the .rpk, and off Windows the headless run reaches the main menu."
tags: [operations]
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-23T12:03:53Z }
---

# Running the client

**Don't launch `DarkEden.exe` yourself** unless asked. It is normally run from the Visual Studio
debugger, and launching it separately steals the stack trace and locks the build output. When a
startup failure has to be bisected, ask: running it is sometimes the only way to see the exit
code. The whitelist's decision is unit-tested since task 4.265 (`FindUnlistedDll`,
`tests/unit/test_client_main_1.cpp`), but no test sees which DLLs a real build directory holds.
Live-server verification is optional and never a merge gate
([executable-only code](../testing/executable-only-code.md)). [README.md](../../README.md)'s
*Run* section owns the data layout, the display options and how to connect to a server; the
[resource text encoding](resource-text-encoding.md) and the
[renderer settings](../rendering/native-gpu-renderer.md#the-xbrz-setting) have their own concepts.

**A silent `-1` exit with no log written is the DLL whitelist** until proven otherwise.
`Client/ClientMain.cpp` walks `*.dll` in the working directory against a hardcoded list and
returns `-1` on the first name it does not recognise, before `InitGame()` sets up logging. A
library that deploys a new DLL beside the executable breaks startup until the DLL is listed, and
the DLL stays in the output directory after the build change is reverted
([client traps](../architecture/client-traps.md)).

**Resources are read loose first, then from the archive.** `CRarFile` reads a loose override
beside its `.rpk`, then falls back to the password-protected archive, into bounded memory and
never to disk. Keep the UnRAR notices beside distributed binaries (`third_party/unrar/README.md`).

**Off Windows**, the client has reached the main menu headless on Linux and nothing past it is
verified; [Linux and macOS](../build/linux-and-macos.md#status-on-linux) has what is verified
there, the app bundle's data lookup and Retina scaling. The browser build has its own loop and
exit path ([browser client](../browser/browser-client.md), [login and logout](../browser/login-and-logout.md)).
