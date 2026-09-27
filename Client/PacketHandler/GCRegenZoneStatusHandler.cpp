//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRegenZoneStatusHandler.cc
// Written By  : elca
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRegenZoneStatus.h"
#include "ShrineInfoManager.h"

//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCRegenZoneStatusHandler::execute ( GCRegenZoneStatus * pPacket , Player * pPlayer )

{
	(void)pPlayer;
	if( g_pRegenTowerInfoManager == NULL )
		return;

	int i;
	for(i = 0; i < 8 ; i++ )
	{
		if (auto* info = g_pRegenTowerInfoManager->GetMutable(i)) {
			info->owner = (int)pPacket->getStatus(i);
		}
	}

	for(;i < g_pRegenTowerInfoManager->GetSize(); i++)
	{
		auto* info = g_pRegenTowerInfoManager->GetMutable(i);
		if (info == nullptr) continue;
		if( i >= 8 && i <= 11 )
		{
			info->owner = (i&0x1) ? RACE_VAMPIRE : RACE_SLAYER;
		}
		else
			info->owner = RACE_OUSTERS;
	}
}
