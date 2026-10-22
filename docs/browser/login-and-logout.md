---
type: Runbook
title: "Browser login and logout"
description: "Where to look when the browser client cannot connect, log in, log out or exit: the launcher's configuration, the WebSocket transport, the logout and reconnect handlers and the Emscripten loop, and what each test proves."
tags: [browser, login, logout, diagnostics]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Browser login and logout

# Configuration

`web/launcher.mjs` reads `web/client-config.json` and sets `DARKEDEN_WEBSOCKET_URL`,
`DARKEDEN_LOGIN_HOST` and `DARKEDEN_LOGIN_PORT` in the module's environment before the first
socket. The login endpoint and the game endpoints a server advertises must match the gateway's
configured routes, and an HTTPS page requires `wss://`. The gateway's listener, origin and proxy
configuration belongs to the server deployment, not to the client's packet format
([browser client](browser-client.md), *Assets and server configuration*).

# A connection failure

Inspect the launcher status and the browser console, then `Client/LoginEndpoint.cpp` and
`Client/Packet/WebSocketTransport.cpp`. The transport reads the gateway from the environment,
builds the URL with the target host and port (`WebSocketURL`, which refuses a gateway that is not
`ws://` or `wss://`, carries a query, fragment or whitespace, or is over 2048 bytes), marks the
close and error callbacks failed, and reports a closed or timed-out connection as a
`ConnectException`. An open WebSocket alone does not show that login succeeded; continue into the
login packet handlers and the mode transitions in `Client/GameMain.cpp`.

# In-game logout

Start at the caller of `ExecuteLogout` in `Client/UIMessageManager.cpp`: it sends `CGLogout`,
processes the output, sets the socket status `CPS_WAITING_FOR_GC_RECONNECT_LOGIN` and switches to
`MODE_WAIT_RECONNECT_LOGIN` (`UIMessages::SetMode`; the mode's handling is `GameMain.cpp`'s
`SetMode`). Follow the waiting update loop (`CWaitUIUpdate`) and the reconnect handlers before
attributing a failure to the transport or to cleanup order. These are inspection points, not a
record of a logout regression run.

# Browser exit

Process exit has another path: `BrowserFrame` in `Client/ClientMain.cpp` cancels the Emscripten
main loop and finishes the client's cleanup (`ReleaseAllObjects`); the launcher's `onExit` flushes
settings and shows its reload message. Settings persistence and a closed WebSocket are not
evidence of server-side account cleanup.

# What the tests prove

The [browser verification guide](browser-client.md#verification) has the asset-free transport
fixture and the browser diagnostics. Its login, world and relogin connection probes use no account
and no database, so passing them does not establish an authenticated session. Diagnose an
authenticated session with both the client flow above and the server's login handlers; packet
checks are [wire layout](../protocol/wire-layout.md).
