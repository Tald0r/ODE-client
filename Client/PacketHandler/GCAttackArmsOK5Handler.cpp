//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackArmsOK5Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackArmsOK5.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "MSlayerGear.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackArmsOK5Handler::execute ( GCAttackArmsOK5 * pPacket , Player * pPlayer )

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
		
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d, Skill=%d", pPacket->getObjectID(), pPacket->getSkillType() );

		if( pPacket->getSkillType() == SKILL_JABBING_VEIN || pPacket->getSkillType() == SKILL_MOLE_SHOT ||
					pPacket->getSkillType() == SKILL_TRIDENT || pPacket->getSkillType() == SKILL_QUICK_FIRE ||
					pPacket->getSkillType() == SKILL_ULTIMATE_BLOW || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
		{
			CreatureState::PacketSpecialActionResult(pTargetCreature, static_cast<TYPE_ACTIONINFO>(pPacket->getSkillType()+g_pActionInfoTable->GetMinResultActionInfo()),
					CreatureState::GetID(pTargetCreature),
					CreatureState::GetX(pTargetCreature),
					CreatureState::GetY(pTargetCreature)
			);
		} else
		{
			CreatureState::PacketSpecialActionResult(pTargetCreature, RESULT_SKILL_ATTACK_GUN_AR,				//pPacket->getSkillType()+g_ActionInfoTable.GetMinResultActionInfo()
					CreatureState::GetID(pTargetCreature),
					CreatureState::GetX(pTargetCreature),
					CreatureState::GetY(pTargetCreature)
			);
		}
		
		return;
	}
	//------------------------------------------------------
	// Only the attacker is here: show it attacking.
	//------------------------------------------------------
	else if (pTargetCreature==NULL)
	{
		
		DEBUG_ADD_FORMAT("There's no such creature : TargetID=%d, Skill=%d", pPacket->getTargetObjectID(), SKILL_ATTACK_MELEE);

		if( pPacket->getSkillType() == SKILL_JABBING_VEIN || pPacket->getSkillType() == SKILL_MOLE_SHOT ||
					pPacket->getSkillType() == SKILL_TRIDENT || pPacket->getSkillType() == SKILL_ULTIMATE_BLOW)
		{
			CreatureState::PacketSpecialActionToNobody(pCreature, pPacket->getSkillType(), 
							CreatureState::GetX(pCreature),
							CreatureState::GetY(pCreature)						
			);
		} else
		{
			//------------------------------------------------------
			// The creature acts.
			//------------------------------------------------------
			CreatureState::PacketSpecialActionToNobody(pCreature, CreatureState::GetBasicActionInfo(pCreature), 
							CreatureState::GetX(pCreature),
							CreatureState::GetY(pCreature)						
			);
		}
		
		return;
	}


	//------------------------------------------------------
	// The result: the other character being hit.
	//------------------------------------------------------
	MActionResult* pResult = NULL;

	TYPE_ACTIONINFO actionInfo = pPacket->getSkillType();
	
	if (pPacket->getSkillSuccess())
	{
		pResult = new MActionResult;

		if( actionInfo == SKILL_JABBING_VEIN || actionInfo == SKILL_MOLE_SHOT || actionInfo == SKILL_TRIDENT ||
			actionInfo == SKILL_QUICK_FIRE  || actionInfo == SKILL_ULTIMATE_BLOW || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
		{
			pResult->Add( new MActionResultNodeActionInfo( 
										actionInfo,
										pPacket->getObjectID(), 
										pPacket->getTargetObjectID(), 
										CreatureState::GetX(pTargetCreature),
										CreatureState::GetY(pTargetCreature)								
								 ) 
						);
		} else
		{
			pResult->Add( new MActionResultNodeActionInfo( 
										CreatureState::GetBasicActionInfo(pCreature),	// gun attack
										pPacket->getObjectID(), 
										pPacket->getTargetObjectID(), 
										CreatureState::GetX(pTargetCreature),
										CreatureState::GetY(pTargetCreature)								
								 ) 
						);
		}
	}

	//------------------------------------------------------
	// The acting creature faces its target.
	//------------------------------------------------------
	CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature) );

	//------------------------------------------------------
	// The creature acts.
	//------------------------------------------------------

	if( actionInfo == SKILL_JABBING_VEIN || actionInfo == SKILL_MOLE_SHOT || actionInfo == SKILL_TRIDENT ||
		actionInfo == SKILL_QUICK_FIRE || actionInfo == SKILL_ULTIMATE_BLOW || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
	{
		CreatureState::PacketSpecialActionToOther(pCreature, // gun attack
						actionInfo,
						pPacket->getTargetObjectID(), 
						pResult
		);
	} else
	{
		CreatureState::PacketSpecialActionToOther(pCreature, // gun attack
						CreatureState::GetBasicActionInfo(pCreature)	, 
						pPacket->getTargetObjectID(), 
						pResult
		);
	}



	__END_CATCH
}
