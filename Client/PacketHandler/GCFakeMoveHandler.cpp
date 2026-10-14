//--------------------------------------------------------------------------------
//
// Filename    : GCFakeMoveHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCFakeMove.h"
#include "Player.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCFakeMoveHandler::execute ( GCFakeMove * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	MCreature *pCreature = ZoneState::GetCreature(pPacket->getObjectID());
	
	if(pCreature == NULL) return;

	MCreature* pFakeCreature = ZoneState::NewFakeCreature( pCreature, CreatureState::GetX(pCreature), CreatureState::GetY(pCreature) );
	if (pFakeCreature == NULL) return;

	CreatureState::SetFakeFastMove( pFakeCreature, pPacket->getToX(), pPacket->getToY() );
//	pFakeCreature->SetAction( ACTION_MOVE );
//	pFakeCreature->SetFakePosition( pCreature->GetX(), pCreature->GetY() );
	
	//------------------------------------------------------
	// Add the fake creature to the zone
	//------------------------------------------------------
	if (!ZoneState::AddFakeCreature( pFakeCreature ))
	{
		ZoneState::DeleteFakeCreature( pFakeCreature );
	}


	__END_CATCH
}
