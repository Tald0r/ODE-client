//---------------------------------------------------------------------------
// HelpEvent.h
//---------------------------------------------------------------------------
// A help event adds a help mail, in game mode only, through the UI's mailbox
// the game installs (task 4.222).
//---------------------------------------------------------------------------
#ifndef __HELPEVENT_H__
#define	__HELPEVENT_H__

#include "MHelpDef.h"

// The UI's mailbox. None installed: no mail.
struct MHelpHost
{
	void (*AddHelpMail)(HELP_EVENT he) = nullptr;
};

void			SetHelpHost(const MHelpHost* pHost);

//-----------------------------------------------------------------------------
// Execute Help Event
//-----------------------------------------------------------------------------
extern void		ExecuteHelpEvent(HELP_EVENT he);

#endif
