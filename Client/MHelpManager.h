//---------------------------------------------------------------------------
// MHelpManager.h
//---------------------------------------------------------------------------
// Help events: ExecuteHelpEvent adds a help mail in game mode. The decision
// tree that would choose help by conditions (MHelpManager, its nodes, and
// MCompareManager's conditions) was created at start-up but never loaded or
// run, and task 4.221 retired it.
//---------------------------------------------------------------------------
#ifndef __MHELPMANAGER_H__
#define	__MHELPMANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "CTypeTable.h"
#include "MHelpDef.h"
#include "Client_PCH.h"
#include <fstream>
#include <list>
	#include "UserOption.h"

//-----------------------------------------------------------------------------
// Execute Help Event
//-----------------------------------------------------------------------------
extern void		ExecuteHelpEvent(HELP_EVENT he);

#ifdef OUTPUT_DEBUG
	#define __BEGIN_HELP_EVENT 		if (g_pUserOption!=NULL && g_pUserOption->UseHelpEvent) {
	#define __END_HELP_EVENT		}
	#define __USE_HELP_EVENT
#else
	#define __BEGIN_HELP_EVENT 		((void)0);
	#define __END_HELP_EVENT 		((void)0);
#endif

#endif

