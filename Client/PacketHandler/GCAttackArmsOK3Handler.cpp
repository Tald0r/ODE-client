//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackArmsOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackArmsOK3.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "MSlayerGear.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackArmsOK3Handler::execute ( GCAttackArmsOK3 * pPacket , Player * pPlayer )

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
	
	if (pCreature==NULL)
	{
		// No such creature.
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d, Skill=%d", pPacket->getObjectID(), pPacket->getSkillType());				
		
		return;
	}

	//------------------------------------------------------
	// The acting creature faces its target.
	//------------------------------------------------------
	CreatureState::SetDirectionToPosition(pCreature, pPacket->getTargetX(), pPacket->getTargetY() );

	//------------------------------------------------------
	// The creature acts.
	// The target is not in the zone.
	//------------------------------------------------------
	if( pPacket->getSkillType() == SKILL_JABBING_VEIN || pPacket->getSkillType() == SKILL_MOLE_SHOT ||
					pPacket->getSkillType() == SKILL_TRIDENT || pPacket->getSkillType() == SKILL_QUICK_FIRE ||
					pPacket->getSkillType() == SKILL_ULTIMATE_BLOW || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
	{
		CreatureState::PacketSpecialActionToNobody(pCreature, pPacket->getSkillType()	,	// gun attack
						pPacket->getTargetX(),
						pPacket->getTargetY()						
		);
	} else
	{
		CreatureState::PacketSpecialActionToNobody(pCreature, CreatureState::GetBasicActionInfo(pCreature)	,	// gun attack
						pPacket->getTargetX(),
						pPacket->getTargetY()						
		);
	}




	__END_CATCH
}
