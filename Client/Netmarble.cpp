//#define NETMARBLE_DEBUG
#include "Client_PCH.h"
#include "CrtCompat.h"
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include "MString.h"

struct NETMARBLE_INFO
{
	MString ID;
	MString Password;
	int WorldID;
	int ServerID;
	bool bGore;
};


// Read by GameUI's UI_RunConnect for a Netmarble login. Nothing wrote it
// at run time even before the parse below was retired (its source was a
// failing stub), so it is always empty.
std::string g_CpCookie;

// For Netmarble
//-----------------------------------------------------------------------
// ParsingNetmarble
//-----------------------------------------------------------------------
// False when the command line has no space, or what follows the first
// space is 4096 bytes or more; otherwise it tells the player to launch
// from Netmarble's site and exits.
//-----------------------------------------------------------------------
bool
ParsingNetmarble(const char* pCommandLine, NETMARBLE_INFO &info)
{
#ifdef NETMARBLE_DEBUG
	MessageBox(NULL,pCommandLine,"CommandLine",MB_OK);
#endif
	(void)info;

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
	MessageBox( NULL, "넷마블 홈페이지를 통해서 다시 실행해주세요.","NetmarbleDarkEden",MB_OK);
	exit(0);
}
