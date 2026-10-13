//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTimeLimitItemInfoHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTimeLimitItemInfo.h"
#include "MTimeItemManager.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MItem.h"
#include "MItemManager.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCTimeLimitItemInfoHandler::execute ( GCTimeLimitItemInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
		
	if( g_pTimeItemManager != NULL && !pPacket->m_TimeLimitItemInfos.empty())
	{
		GCTimeLimitItemInfo::ItemTimeLimitMap::const_iterator endItr = pPacket->m_TimeLimitItemInfos.end();
		GCTimeLimitItemInfo::ItemTimeLimitMap::const_iterator Itr = pPacket->m_TimeLimitItemInfos.begin();
		
		//MPlayerGear* pGear;
		
		MItemManager* pGear = PlayerState::GetGear();

		while( Itr != endItr )
		{
			g_pTimeItemManager->AddTimeItem( Itr->first, Itr->second );
			MItem *pItem = pGear->GetItem( Itr->first );
			if(pItem != NULL)
			{
				pItem->SetAffectStatus();			
				PlayerState::SetAddonItem( pItem );
			}
			
			Itr++;
		}

		pPacket->m_TimeLimitItemInfos.clear();
	}
		

	__END_CATCH
}
