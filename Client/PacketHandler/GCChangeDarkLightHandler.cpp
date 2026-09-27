//--------------------------------------------------------------------------------
//
// Filename    : GCChangeDarkLightHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "MTopView.h"
#include "Gpackets/GCChangeDarkLight.h"
#include "UserInformation.h"

	#include "ClientPlayer.h"

#include "ClientDef.h"

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

	// 화면의 어둡기
	g_pTopView->SetDarkBits( darkLevel );		
	
	// Player시야 빛의 크기
	g_pPlayer->SetTimeLightSight( lightSight );		

	


	__END_CATCH
}
