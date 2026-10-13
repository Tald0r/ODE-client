//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackMeleeOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackMeleeOK3.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackMeleeOK3Handler::execute ( GCAttackMeleeOK3 * pPacket , Player * pPlayer )

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
	
	if (pCreature==NULL && pTargetCreature==NULL)
	{
		return;
	}

	//------------------------------------------------------
	// Only the target is here: show it being hit.
	//------------------------------------------------------
	if (pCreature==NULL)
	{
		
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d, Skill=%d", pPacket->getObjectID(), SKILL_ATTACK_MELEE);

		CreatureState::PacketSpecialActionResult(pTargetCreature, RESULT_SKILL_ATTACK_MELEE,				//pPacket->getSkillType()+g_ActionInfoTable.GetMinResultActionInfo()
				CreatureState::GetID(pTargetCreature),
				CreatureState::GetX(pTargetCreature),
				CreatureState::GetY(pTargetCreature)
		);
		
		return;
	}
	//------------------------------------------------------
	// Only the attacker is here: show it attacking.
	//------------------------------------------------------
	else if (pTargetCreature==NULL)
	{
		
		DEBUG_ADD_FORMAT("There's no such creature : TargetID=%d, Skill=%d", pPacket->getTargetObjectID(), SKILL_ATTACK_MELEE);

		//------------------------------------------------------
		// The creature acts.
		//------------------------------------------------------
		CreatureState::PacketSpecialActionToNobody(pCreature, CreatureState::GetBasicActionInfo(pCreature), 
						CreatureState::GetX(pCreature),
						CreatureState::GetY(pCreature)						
		);

		// A result added to PacketSpecialActionToNobody could show
		// blood spattering too,
		// but without the target creature
		// the server would first have to send its exact cell.		
		
		return;
	}

	//------------------------------------------------------
	// The result: the other character being hit.
	//------------------------------------------------------
	MActionResult* pResult = new MActionResult;
	pResult->Add( new MActionResultNodeActionInfo( 
								CreatureState::GetBasicActionInfo(pCreature),
								pPacket->getObjectID(), 
								pPacket->getTargetObjectID(), 
								CreatureState::GetX(pTargetCreature),
								CreatureState::GetY(pTargetCreature)
								 ) 
				);

	//------------------------------------------------------
	// The acting creature faces its target.
	//------------------------------------------------------
	CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature) );

	//------------------------------------------------------
	// The creature acts.
	//------------------------------------------------------
	CreatureState::PacketSpecialActionToOther(pCreature, CreatureState::GetBasicActionInfo(pCreature), 
					pPacket->getTargetObjectID(), 
					pResult
	);


	__END_CATCH
}
