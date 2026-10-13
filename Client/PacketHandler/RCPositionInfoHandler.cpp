//----------------------------------------------------------------------
//
// Filename    : RCPositionInfoHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Rpackets/RCPositionInfo.h"
#include "CMessageArray.h"
#include "ClientMode.h"
#include "GameMessages.h"
#include "MParty.h"
#include "PlayerStateHost.h"
#include "RequestUserManager.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"


//----------------------------------------------------------------------
// 
// RCPositionInfoHander::execute()
// 
//----------------------------------------------------------------------
void RCPositionInfoHandler::execute ( RCPositionInfo * pPacket )

{
	__BEGIN_TRY

	//g_pClientCommunicationManager->sendPacket( pPacket->getHost() , pPacket->getPort() , &glIncomingConnectionOK );
	if ((g_Mode==MODE_GAME
			|| g_Mode==MODE_WAIT_UPDATEINFO			// not loading
			|| g_Mode==MODE_WAIT_SETPOSITION		// waiting for the position
			)
		&& PlayerState::Creature()!=NULL
		&& g_pParty!=NULL
		&& g_pGameMessage!=NULL
		&& g_pRequestUserManager!=NULL)
	{
		// Refresh the peer's address.
		RequestUserInfo* pUserInfo = g_pRequestUserManager->GetUserInfo( pPacket->getName().c_str() );

		if (pUserInfo!=NULL)
		{
			pUserInfo->IP = pPacket->getHost().c_str();
			pUserInfo->UDPPort = pPacket->getPort();
		}

		PARTY_INFO*	pInfo = NULL;

		// No name: find the member by address.
		if (pPacket->getName().size()==0)
		{
			pInfo = g_pParty->GetMemberInfoByIP( pPacket->getHost().c_str() );
		}
		else
		{
			pInfo = g_pParty->GetMemberInfo( pPacket->getName().c_str() );
		}
			
		// Update the member's position.
		if (pInfo!=NULL)
		{
			pInfo->zoneID = pPacket->getZoneID();
			pInfo->zoneX = pPacket->getZoneX();
			pInfo->zoneY = pPacket->getZoneY();

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

