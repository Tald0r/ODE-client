#pragma once

class ClientPlayer;

// The connection to the game server (task 4.237: GameMain's until then).
// The executable connects it; a test sets its own.
extern ClientPlayer*	g_pSocket;
