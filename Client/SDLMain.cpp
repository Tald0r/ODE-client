/*-----------------------------------------------------------------------------

	SDLMain.cpp

	The executable's entry points: WinMain() on Windows, main() on Linux,
	macOS and the browser. Each installs the real ClientHost and hands the
	launch argument to ClientMain (ClientMain.cpp, in the gameclient
	library), which runs on every platform - the command line, the
	working directory, the file definitions, the language, InitApp, the
	frame loop and the shutdown.

	This file used to be a hand-copied partial of that body, 572 lines
	with about twenty globals re-declared at the wrong types (bool for a
	BOOL, int for a LONG), its own window creation and its own frame
	loop that presented the back buffer beside the one CSDLGraphics::Flip
	already does. It had never had a green build to diverge from. The SDL
	window is CSDLGraphics::Init's off Windows now, and the SDL event pump
	is inside ClientMain's loop (docs/build/linux-macos-port-assessment-2026-09-07.md,
	area D).

	2025.01.18 - rewritten from Client.cpp WinMain
	2026-09-08 - reduced to the bootstrap
	2026-10-19 - WinMain moved here from Client.cpp

-----------------------------------------------------------------------------*/

#include "Client_PCH.h"

#include "ClientMain.h"

#ifdef PLATFORM_WINDOWS

#include "ClientMode.h"		// g_hInstance

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The Windows entry point. Records the instance handle and hands
//       the rest to ClientMain, which is this function's old body.
//-----------------------------------------------------------------------------
int PASCAL
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{
	(void)hPrevInstance;
	g_hInstance = hInstance;
	ClientHost::SetHost(ClientHost::SystemHost());
	return ClientMain(lpCmdLine, nCmdShow);
}

#else

#include <SDL.h>
#include <SDL_main.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <string>

int main(int argc, char* argv[])
{
	// A send() on a connection the server has closed raises SIGPIPE on
	// every POSIX system, and the default action kills the process with
	// no log line - the outcome SocketOutputStream's destructor comments
	// on and avoids by not flushing. Ignored, the send() fails with EPIPE
	// instead and SocketAPI::send_ex turns that into the ConnectException
	// the game already handles as a dropped connection. Linux could have
	// set MSG_NOSIGNAL per call; macOS has no such flag (SO_NOSIGPIPE is
	// per socket), and one line here covers both.
	signal(SIGPIPE, SIG_IGN);

	// The launch argument, as WinMain receives lpCmdLine: the first
	// argument or nothing. ClientMain folds the display settings' launch
	// command into it and may rewrite its own pointer, so it takes a
	// mutable buffer.
	std::string commandLine = (argc > 1 && argv[1] != NULL) ? argv[1] : "";

	fprintf(stderr, "Dark Eden (SDL2) starting\n");

	// The subsystems initialise what they use (SpriteLib the video, dxlib
	// the audio, TextSystem the fonts); this only fails early and loudly
	// if SDL itself cannot start, before any window or data is touched.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	ClientHost::SetHost(ClientHost::SystemHost());
	const int result = ClientMain(&commandLine[0], 1);

#ifdef __EMSCRIPTEN__
	// The browser now owns the frame callbacks and performs shutdown when the
	// game quits. Returning main keeps the page responsive between frames.
	if (ClientHasBrowserLoop()) return result;
#endif
	SDL_Quit();
	return result;
}

#endif /* PLATFORM_WINDOWS */
