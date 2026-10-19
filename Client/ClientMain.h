//----------------------------------------------------------------------
// ClientMain.h
//
// The game's entry point, shared by the two platform entry points in
// SDLMain.cpp: WinMain on Windows and main elsewhere.
// It is WinMain's old body - the command line, the working directory,
// the file definitions, the language and character input, InitApp,
// the frame loop and the shutdown - with the Win32-only steps (the
// single-instance mutex, the patcher hand-off, the DLL whitelist, the
// message pump) behind PLATFORM_WINDOWS and their SDL equivalents in
// the other branch (docs/linux-macos-port-assessment-2026-09-07.md,
// area D). SDLMain.cpp used to carry a hand-copied partial of that
// body instead, with about twenty globals re-declared at the wrong
// types.
//
// lpCmdLine is the launch argument as WinMain received it (mutable:
// the body rewrites its own pointer to the display settings' launch
// command); nCmdShow is passed through to InitApp. Returns the process
// exit code.
//----------------------------------------------------------------------
#ifndef __CLIENT_MAIN_H__
#define __CLIENT_MAIN_H__

int ClientMain(char* lpCmdLine, int nCmdShow);
#ifdef __EMSCRIPTEN__
bool ClientHasBrowserLoop();
#endif

#endif // __CLIENT_MAIN_H__
