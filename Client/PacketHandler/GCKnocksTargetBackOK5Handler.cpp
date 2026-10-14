//////////////////////////////////////////////////////////////////////
//
// Filename    : GCKnocksTargetBackOK5Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCKnocksTargetBackOK5.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCKnocksTargetBackOK5Handler::execute ( GCKnocksTargetBackOK5 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	// message

	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
		return;
	}	

	//------------------------------------------------------
	// The creatures involved.
	//------------------------------------------------------
	MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );
	MCreature* pTargetCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );
	
	if (pCreature==NULL)
	{
		// No such creature.
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d, Skill=%d", pPacket->getObjectID(), pPacket->getSkillType());				
		
		return;
	}

	if (pTargetCreature==NULL)
	{
		// No such creature.
		DEBUG_ADD_FORMAT("There's no such creature : TargetID=%d, Skill=%d", pPacket->getTargetObjectID(), pPacket->getSkillType());
		
		return;
	}


	int skillType = pPacket->getSkillType();	//pCreature->GetBasicActionInfo()	

	if( skillType >= g_pActionInfoTable->GetSize() )
	{
		DEBUG_ADD_FORMAT_WAR("[GCKnocksTargetBackOK5] SkillType Error %d", skillType );
		return;
	}

	//------------------------------------------------------
	// The direction it is knocked back in.
	//------------------------------------------------------	
	unsigned short x = pPacket->getX();
	unsigned short y = pPacket->getY();

	// The packet carries the cell after the move.
	//MCreature::GetPositionToDirection(x, y, pPacket->getDir());

	//------------------------------------------------------
	// The result: the other character being hit.
	//------------------------------------------------------
	MActionResult* pResult = new MActionResult;
	pResult->Add( new MActionResultNodeActionInfo( 
								skillType, // gun attack									
								pPacket->getObjectID(), 
								pPacket->getTargetObjectID(),
								CreatureState::GetX(pTargetCreature),
								CreatureState::GetY(pTargetCreature)
								 ) 
				);

	// the target moves
	pResult->Add( new MActionResultNodeChangePosition( 
								pPacket->getTargetObjectID(),
								x, y)
				);
								
	//------------------------------------------------------
	// The acting creature faces its target.
	//------------------------------------------------------
	CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature) );

	//------------------------------------------------------
	// The creature acts.
	//------------------------------------------------------
	CreatureState::PacketSpecialActionToOther(pCreature,
					skillType	, // gun attack
					pPacket->getTargetObjectID(), 
					pResult
	);

	//------------------------------------------------------
	// To stay in sync,
	// the server cell is set at once.
	//------------------------------------------------------
	CreatureState::SetServerPosition(pTargetCreature, x, y );


	__END_CATCH
}
