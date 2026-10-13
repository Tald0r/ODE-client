//--------------------------------------------------------------------------------
//
// Filename    : GCPartyPositionHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCPartyPosition.h"
#include "CMessageArray.h"
#include "ClientMode.h"
#include "GameMessages.h"
#include "MParty.h"
#include "PlayerStateHost.h"
#include "RequestUserManager.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCPartyPositionHandler::execute ( GCPartyPosition * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
		
	if ((g_Mode==MODE_GAME
			|| g_Mode==MODE_WAIT_UPDATEINFO			// not loading
			|| g_Mode==MODE_WAIT_SETPOSITION		// waiting for the position
			)
		&& PlayerState::Creature()!=NULL
		&& g_pParty!=NULL
		&& g_pGameMessage!=NULL
		&& g_pRequestUserManager!=NULL)
	{
		PARTY_INFO*	pInfo = NULL;

		// No name: nothing to find.
		if (pPacket->getName().size()==0)
		{
			return;
		}
		pInfo = g_pParty->GetMemberInfo( pPacket->getName().c_str() );
			
		// Update the member's position.
		if (pInfo!=NULL)
		{
			pInfo->zoneID = pPacket->getZoneID();
			pInfo->zoneX = static_cast<BYTE>(pPacket->getX());
			pInfo->zoneY = static_cast<BYTE>(pPacket->getY());
			pInfo->HP	= pPacket->getHP();
			pInfo->MaxHP= pPacket->getMaxHP();

			const int currentZoneID = (g_bZonePlayerInLarge ? g_nZoneLarge : g_nZoneSmall);

			MCreature* const pPlayerCreature = PlayerState::Creature();
			pInfo->bInSight = MParty::IsMemberInSight(pInfo->zoneID, pInfo->zoneX, pInfo->zoneY,
												currentZoneID, CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature),
												PlayerState::GetSight());
			
			#if defined(_DEBUG) && defined(OUTPUT_DEBUG)
				if (g_pGameMessage!=NULL)
				{
					g_pGameMessage->AddSafeFormat("[RCMove] %s to zone:%d, (%d, %d)",
												pInfo->Name.GetString(),
												pInfo->zoneID,
												pInfo->zoneX,
												pInfo->zoneY);
				}
			#endif
		}
	}

	__END_CATCH
}
