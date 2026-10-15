// The Netmarble launch check (task 4.220, gamemodel's since then).
#include "Client_PCH.h"
#include "Netmarble.h"
#include <cstring>

// Read by GameUI's UI_RunConnect for a Netmarble login. Nothing wrote it
// at run time even before the parse below was retired (its source was a
// failing stub), so it is always empty.
std::string g_CpCookie;

namespace {
const NetmarbleHost* s_pHost = NULL;
}

void
SetNetmarbleHost(const NetmarbleHost* pHost)
{
	s_pHost = pHost;
}

//-----------------------------------------------------------------------
// ParsingNetmarble
//-----------------------------------------------------------------------
bool
ParsingNetmarble(const char* pCommandLine)
{
	const char *pString = strchr(pCommandLine, ' ');
	if(pString == NULL)
	{
		return false;
	}

	if (strlen(pString + 1) >= 4096) return false;

	// Netmarble's launcher handed the server, port and cookies over the
	// clipboard, read by NMCrypt's GetNMClipData. This build no longer
	// links NMCrypt (its .lib went with bb70900b): GetNMClipData was a
	// stub that failed, so a Netmarble launch always ended here, and task
	// 4.207 retired the parse behind it.
	if (s_pHost != NULL && s_pHost->RefuseLaunch != NULL)
	{
		s_pHost->RefuseLaunch();
	}
	return false;
}
