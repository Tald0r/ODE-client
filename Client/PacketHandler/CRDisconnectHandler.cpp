//////////////////////////////////////////////////////////////////////
//
// Filename    : CRDisconnectHandler.cc
// Written By  : crazydog
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Rpackets/CRDisconnect.h"
#include "RequestServerPlayer.h"
#include "WorldEventsHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void CRDisconnectHandler::execute ( CRDisconnect * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;
		

	RequestServerPlayer* pRequestServerPlayer = dynamic_cast<RequestServerPlayer*>( pPlayer );

	if (pRequestServerPlayer!=NULL)
	{
		// 접속된 사용자를 접속해제 시킨다.
		WorldEvents::RequestDisconnect( pRequestServerPlayer->getName().c_str() );
	}


	__END_CATCH
}
