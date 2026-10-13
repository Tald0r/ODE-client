//////////////////////////////////////////////////////////////////////
//
// Filename    : GCWarListHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCWarList.h"
#include "MEventQueue.h"
#include "MWarManager.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCWarListHandler::execute ( GCWarList * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
	
	if(g_pWarManager == NULL || !ZoneState::Exists())
		return;

	while(1)
	{
		WarInfo *info = pPacket->popWarInfo();
		
		if(info == NULL)
			break;

		g_pWarManager->SetWar(info);
	}	
	
	// A race war in the current zone makes it unsafe.
	WarInfo *pInfo = g_pWarManager->GetWarInfo(static_cast<ZoneID_t>(ZoneState::GetID()));
	if(pInfo != NULL && pInfo->getWarType() == WAR_RACE)
	{
		g_bZoneSafe = false;
	}
	
	WorldEvents::PlayMusicCurrentZone();
	WorldEvents::RemoveEvent( EVENTID_WAR_EFFECT );

	__END_CATCH
}
