//////////////////////////////////////////////////////////////////////
//
// Filename    : GCKnockBackHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCKnockBack.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCKnockBackHandler::execute ( GCKnockBack * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
	

	int objectID = pPacket->getObjectID();
	int ox = pPacket->getOriginX();
	int oy = pPacket->getOriginY();
	int nx = pPacket->getTargetX();
	int ny = pPacket->getTargetY();	
	
	MCreature* pCreature = ZoneState::GetCreature( objectID );

	if (pCreature!=NULL)
	{
		// 일반 creature인 경우는 시작좌표를 일단 설정해준다.
		if (CreatureState::GetID(pCreature)!=PlayerState::GetID())
		{
			CreatureState::SetStop(pCreature);		
			
			CreatureState::MovePosition(pCreature, ox, oy );
			CreatureState::SetServerPosition(pCreature, ox, oy );
		}

		// (nx, ny)로 KnockBack된다.
		CreatureState::KnockBackPosition(pCreature, nx, ny );
	}
	else
	{
		// 그런 creature가 없을 경우
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d", pPacket->getObjectID());
	}


	__END_CATCH
}
