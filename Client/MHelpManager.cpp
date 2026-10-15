//---------------------------------------------------------------------------
// MHelpManager.cpp
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MHelpManager.h"
	#include "ClientDef.h"
#include "VS_UI.h"

//-----------------------------------------------------------------------------
// Execute Help Event
//-----------------------------------------------------------------------------
void		
ExecuteHelpEvent(HELP_EVENT he)
{
		if (g_Mode==MODE_GAME)
		{
			gC_vs_ui.AddHelpMail(he);//, g_pPlayer->IsInSafeSector());
		}
}
