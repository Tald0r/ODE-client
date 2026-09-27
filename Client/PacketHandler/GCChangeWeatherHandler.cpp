//--------------------------------------------------------------------------------
//
// Filename    : GCChangeWeatherHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCChangeWeather.h"

	#include "ClientPlayer.h"

#include "ClientDef.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCChangeWeatherHandler::execute ( GCChangeWeather * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	
	SetWeather( pPacket->getWeather(), pPacket->getWeatherLevel() );	


	__END_CATCH
}
