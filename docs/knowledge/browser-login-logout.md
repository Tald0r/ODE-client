---
type: Playbook
title: Browser login and logout
description: Trace configuration, transport, reconnect, and browser exit without confusing their test coverage.
tags: [client, browser, login, logout, diagnostics]
generated:
  by: codex/gpt-6
  at: "2026-10-04T09:27:48Z"
source_revision: eabcf17f23cafebf9b60825edfc33a27e544820c
sources:
  - id: browser-guide
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/webgl-client.md
  - id: launcher
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/web/launcher.mjs
  - id: transport
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/Packet/WebSocketTransport.cpp
  - id: logout-flow
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/UIMessageManager.cpp
  - id: mode-flow
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/GameMain.cpp
  - id: browser-loop
    resource: https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/Client.cpp
---

# Browser login and logout

The launcher reads `client-config.json` and sets `DARKEDEN_WEBSOCKET_URL`,
`DARKEDEN_LOGIN_HOST` and `DARKEDEN_LOGIN_PORT` before the first socket. Login and
advertised game endpoints must match gateway routes; HTTPS pages require WSS.
Gateway listener/origin/proxy configuration belongs to the server deployment
rather than the client's packet format.[^launcher][^browser-guide]

For a connection failure, inspect the launcher status and browser console, then
`Client/LoginEndpoint.cpp` and `Client/Packet/WebSocketTransport.cpp`. The
transport builds a gateway URL with the target host/port, marks close/error
callbacks failed, and reports a closed or timed-out connection through
`ConnectException`. An open WebSocket alone does not show that login succeeded;
continue into the login handlers and mode transitions.[^transport][^browser-guide]

For in-game logout, inspect the caller into `ExecuteLogout` in
`Client/UIMessageManager.cpp`, its `CGLogout` send/output processing, socket status
`CPS_WAITING_FOR_GC_RECONNECT_LOGIN`, and transition to
`MODE_WAIT_RECONNECT_LOGIN`. Follow the corresponding waiting update and reconnect
handlers before attributing a failure to transport or cleanup order. This map
records inspection points, not a successful logout regression run.[^logout-flow][^mode-flow]

Browser process exit has another path: `BrowserFrame` in `Client/Client.cpp`
ends the Emscripten loop and finishes client cleanup; the launcher's `onExit`
flushes settings and displays its reload message. Settings persistence and a
WebSocket close are not evidence of server account cleanup.[^browser-loop][^launcher]

Use [the browser verification guide](../webgl-client.md#verification) for the
asset-free transport fixture and browser diagnostics. Its login/world/relogin
connection probes do not use an account or database. Diagnose an authenticated
session with both client flow and server handlers; use
[protocol contracts](protocol-shared-rules.md) for packet checks.[^browser-guide]

[^browser-guide]: [docs/webgl-client.md](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/docs/webgl-client.md)
[^launcher]: [web/launcher.mjs](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/web/launcher.mjs)
[^transport]: [Client/Packet/WebSocketTransport.cpp](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/Packet/WebSocketTransport.cpp)
[^logout-flow]: [Client/UIMessageManager.cpp](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/UIMessageManager.cpp)
[^mode-flow]: [Client/GameMain.cpp](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/GameMain.cpp)
[^browser-loop]: [Client/Client.cpp](https://github.com/bound2/opendarkeden-client/blob/eabcf17f23cafebf9b60825edfc33a27e544820c/Client/Client.cpp)
