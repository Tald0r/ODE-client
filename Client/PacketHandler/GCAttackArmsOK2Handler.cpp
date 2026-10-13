//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackArmsOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackArmsOK2.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackArmsOK2Handler::execute ( GCAttackArmsOK2 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		

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
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d, Skill=%d", pPacket->getObjectID(), SKILL_ATTACK_MELEE);				
		
		if( pPacket->getSkillType() == SKILL_JABBING_VEIN || pPacket->getSkillType() == SKILL_MOLE_SHOT ||
					pPacket->getSkillType() == SKILL_TRIDENT || pPacket->getSkillType() == SKILL_QUICK_FIRE ||
					pPacket->getSkillType() == SKILL_ULTIMATE_BLOW || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
		{
			TYPE_ACTIONINFO resultActionInfo = static_cast<TYPE_ACTIONINFO>(pPacket->getSkillType() + g_pActionInfoTable->GetMinResultActionInfo());
			// Show it hit at once.
			CreatureState::PacketSpecialActionResult(pPlayerCreature, resultActionInfo,	// the basic attack, a gun? 
									PlayerState::GetID(), 
									CreatureState::GetX(pPlayerCreature),
									CreatureState::GetY(pPlayerCreature)								
					);
		} else
		{
			// Show it hit at once.
			CreatureState::PacketSpecialActionResult(pPlayerCreature, RESULT_SKILL_ATTACK_GUN_AR,	// the basic attack, a gun? 
									PlayerState::GetID(), 
									CreatureState::GetX(pPlayerCreature),
									CreatureState::GetY(pPlayerCreature)								
					);
		}
	}
	else
	{
		TYPE_ACTIONINFO actionInfo = pPacket->getSkillType();
		//------------------------------------------------------
		// The acting creature faces the player.
		//------------------------------------------------------
		CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature) );

		//------------------------------------------------------
		// The creature attacks the player.
		//------------------------------------------------------
		//g_pPlayer->PacketSpecialActionResult( SKILL_ATTACK_MELEE + g_ActionInfoTable.GetMinResultActionInfo() );

		if( actionInfo == SKILL_JABBING_VEIN || actionInfo == SKILL_TRIDENT || actionInfo == SKILL_MOLE_SHOT ||
			actionInfo == SKILL_QUICK_FIRE || actionInfo == SKILL_ULTIMATE_BLOW || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
		{
			MActionResult* pResult = new MActionResult;
			pResult->Add( new MActionResultNodeActionInfo( 
										actionInfo,	// the basic attack, a gun? 
										pPacket->getObjectID(), 
										PlayerState::GetID(), 
										CreatureState::GetX(pPlayerCreature),
										CreatureState::GetY(pPlayerCreature)								
										) 
						);
			//------------------------------------------------------
			// The creature acts.
			//------------------------------------------------------
			CreatureState::PacketSpecialActionToOther(pCreature, actionInfo,	// the basic attack, a gun? 
							PlayerState::GetID(), 
							pResult
			);
		} else
		{
			MActionResult* pResult = new MActionResult;
			pResult->Add( new MActionResultNodeActionInfo( 
										CreatureState::GetBasicActionInfo(pCreature),	// the basic attack, a gun? 
										pPacket->getObjectID(), 
										PlayerState::GetID(), 
										CreatureState::GetX(pPlayerCreature),
										CreatureState::GetY(pPlayerCreature)								
										) 
						);
			//------------------------------------------------------
			// The creature acts.
			//------------------------------------------------------
			CreatureState::PacketSpecialActionToOther(pCreature, CreatureState::GetBasicActionInfo(pCreature),	// the basic attack, a gun? 
							PlayerState::GetID(), 
							pResult
			);
		}
	}

	//------------------------------------------------------------------
	// Apply the status change.
	//------------------------------------------------------------------
	if (MStatus* pPlayerStatus = PlayerState::Status())
		AffectModifyInfo(pPlayerStatus, pPacket);


	//------------------------------------------------------------------
	// Update what the UI shows.
	// (Setting it beats comparing first.)
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );
	


	__END_CATCH
}
