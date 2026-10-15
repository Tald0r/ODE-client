//---------------------------------------------------------------------------
// MHelpManager.h
//---------------------------------------------------------------------------
// Help events: ExecuteHelpEvent (HelpEvent.h, gamemodel's since task 4.222)
// adds a help mail in game mode, through the host the game installs. This
// header adds the __BEGIN_HELP_EVENT macros. The decision tree
// that would choose help by conditions (MHelpManager, its nodes, and
// MCompareManager's conditions) was created at start-up but never loaded or
// run, and task 4.221 retired it.
//---------------------------------------------------------------------------
#ifndef __MHELPMANAGER_H__
#define	__MHELPMANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "HelpEvent.h"
#include "Client_PCH.h"
	#include "UserOption.h"

#ifdef OUTPUT_DEBUG
	#define __BEGIN_HELP_EVENT 		if (g_pUserOption!=NULL && g_pUserOption->UseHelpEvent) {
	#define __END_HELP_EVENT		}
	#define __USE_HELP_EVENT
#else
	#define __BEGIN_HELP_EVENT 		((void)0);
	#define __END_HELP_EVENT 		((void)0);
#endif

#endif

