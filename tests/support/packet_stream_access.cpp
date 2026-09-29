//----------------------------------------------------------------------
// packet_stream_access.cpp
//----------------------------------------------------------------------
//
// Points SocketInputStreamTestAccess's preload hook
// (packet_stream_preload.h) at the test framework: every preload is a
// check that its bytes fit the ring, as it was when the class lived in
// packet_stream_access.h, so a preload that does not fit fails the test.
//
// A namespace-scope static in an object linked straight into the test
// binary, not an inline variable in the header: the latter's
// initialisation may be deferred until its first use, which never
// comes, and a linker that drops unreferenced COMDATs (MSVC /OPT:REF)
// can discard it with its initialiser.
//
//----------------------------------------------------------------------

#include "packet_stream_access.h"

namespace {

void	CheckPreloadFits(unsigned int len, unsigned int bufferLen)
{
	CHECK(len < bufferLen);
}

// Runs during static initialisation, before RunAll() starts a test.
struct PreloadHookInstaller
{
	PreloadHookInstaller()
	{
		SocketInputStreamTestAccess::s_pOnPreload = &CheckPreloadFits;
	}
};

const PreloadHookInstaller g_PreloadHookInstaller;

} // namespace
