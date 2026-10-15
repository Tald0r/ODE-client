//---------------------------------------------------------------------------
// HelpEvent.cpp
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "HelpEvent.h"
#include "ClientMode.h"

namespace {
const MHelpHost* s_pHost = NULL;
}

void
SetHelpHost(const MHelpHost* pHost)
{
	s_pHost = pHost;
}

//-----------------------------------------------------------------------------
// Execute Help Event
//-----------------------------------------------------------------------------
void		
ExecuteHelpEvent(HELP_EVENT he)
{
		if (g_Mode==MODE_GAME && s_pHost != NULL && s_pHost->AddHelpMail != NULL)
		{
			s_pHost->AddHelpMail(he);
		}
}
