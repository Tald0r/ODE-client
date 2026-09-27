//////////////////////////////////////////////////////////////////////////////
// Filename    : GCAddVampirePortalHandler.cpp
// Written By  : excel96
// Description :
// 뱀파이어 포탈은 현재 이펙트의 일종으로서 구현되는데, 이 패킷은
// 클라이언트에게 바닥에다 뱀파이어 포탈 이펙트를 붙이라는 패킷이다.
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "Gpackets/GCAddVampirePortal.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GCAddVampirePortalHandler::execute ( GCAddVampirePortal * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
		


	int serverID = pPacket->getObjectID();

	// getOwnerID() returns std::string by value; bind it so the pointer handed
	// to AddVampirePortal below is not into a destroyed temporary.
	const std::string ownerID = pPacket->getOwnerID();
	const char* ownerName = ownerID.c_str();
	TYPE_SECTORPOSITION sX = pPacket->getX();
	TYPE_SECTORPOSITION sY = pPacket->getY();

	DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

	int zoneID = pPacket->getTargetZoneID();
	TYPE_SECTORPOSITION zoneX = pPacket->getTargetX();
	TYPE_SECTORPOSITION zoneY = pPacket->getTargetY();

	// 오래된거면 mainNode부터 실행해야되지..
	bool bStartFromMainNode = (pPacket->getCreateFlag()==0);

	AddVampirePortal( serverID, 
						ownerName, 
						sX, sY,	
						delayFrame,	
						zoneID,	zoneX, zoneY,
						bStartFromMainNode );


	__END_CATCH
}
