//----------------------------------------------------------------------
// test_main.cpp
//----------------------------------------------------------------------
//
// Entry point for the unit test binary. Returns non-zero when any test
// fails so CTest reports the run as failed.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

namespace {

//----------------------------------------------------------------------
// Keep failures non-interactive.
//
// Tests that cover memory handling can trip the debug heap or an assert.
// By default the MSVC runtime answers those with a modal dialog, which
// would hang an unattended run instead of failing it. Route the reports
// to stderr so the process aborts with a diagnostic and a non-zero exit
// code.
//----------------------------------------------------------------------
void	MakeFailuresNonInteractive()
{
#if defined(_MSC_VER)
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
}

//----------------------------------------------------------------------
// One video driver for the whole run.
//
// SDL's video, once started, stays up for the rest of the run, and the
// first test to start it picks the driver. Unset, that is the desktop's
// (cocoa on a macOS runner), whose windows open under the real cursor
// and move SDL's mouse state, so a later test asking for the dummy
// driver silently gets the desktop's instead. Off Windows the run takes
// the dummy driver unless one is set; on Windows the dummy driver cannot
// wrap the client's window, so the tests there keep the desktop's.
//----------------------------------------------------------------------
void	UseOneVideoDriver()
{
#if !defined(_WIN32)
	setenv("SDL_VIDEODRIVER", "dummy", 0);
#endif
}

} // namespace

int	main()
{
	MakeFailuresNonInteractive();
	UseOneVideoDriver();

	return ::testfw::RunAll();
}
