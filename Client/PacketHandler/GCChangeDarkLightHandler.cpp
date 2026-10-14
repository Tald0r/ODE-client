//--------------------------------------------------------------------------------
//
// Filename    : GCChangeDarkLightHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCChangeDarkLight.h"
#include "UserInformation.h"

	#include "ClientPlayer.h"

#include "PlayerStateHost.h"
#include "WorldEventsHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCChangeDarkLightHandler::execute ( GCChangeDarkLight * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	// message


	int darkLevel = pPacket->getDarkLevel();
	int lightSight = pPacket->getLightLevel();

	if (g_pUserInformation->IsMaster)
	{
		darkLevel = 0;
		lightSight = 13;
	}

	// How dark the view is
	WorldEvents::SetDarkBits( darkLevel );		
	
	// The size of the light around the player
	PlayerState::SetTimeLightSight( lightSight );		

	


	__END_CATCH
}
