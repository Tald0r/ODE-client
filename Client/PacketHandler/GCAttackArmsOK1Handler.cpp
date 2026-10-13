//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackArmsOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackArmsOK1.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "MSlayerGear.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackArmsOK1Handler::execute ( GCAttackArmsOK1 * pPacket , Player * pPlayer )

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

	//------------------------------------------------------------------
	// The player's skill succeeded, so
	// its result is applied.
	//------------------------------------------------------------------
	// Apply the status change.
	//------------------------------------------------------------------
	if (MStatus* pPlayerStatus = PlayerState::Status())
		AffectModifyInfo(pPlayerStatus, pPacket);

	//------------------------------------------------------------------
	// The skill the player waited on was confirmed.
	//------------------------------------------------------------------
	if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_SKILL_SUCCESS)
	{		
		PlayerState::SetWaitVerifyNULL();
	}
	else
	{
		DEBUG_ADD_ERR("[Error] Player is not WaitVerifySkillSuccess");
	}

	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// The normal case. 
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		// The creature is hit.

		//------------------------------------------------------
		// Only a hit shows being hit.
		//------------------------------------------------------
		if (pPacket->getSkillSuccess())
		{
			// Hit when the action ends.
			if (pCreature!=NULL)
			{
				MActionResult* pResult = new MActionResult;

				if( pPacket->getSkillType() == SKILL_JABBING_VEIN || pPacket->getSkillType() == SKILL_MOLE_SHOT ||
					pPacket->getSkillType() == SKILL_TRIDENT || pPacket->getSkillType() == SKILL_QUICK_FIRE || 
					pPacket->getSkillType() == SKILL_ULTIMATE_BLOW  || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
				{
					pResult->Add( new MActionResultNodeActionInfo( 
												pPacket->getSkillType(),
												PlayerState::GetID(), 
												CreatureState::GetID(pCreature), 
												CreatureState::GetX(pCreature),
												CreatureState::GetY(pCreature)
												 ) 
								);
				} else
				{			
					pResult->Add( new MActionResultNodeActionInfo( 
												CreatureState::GetBasicActionInfo(pPlayerCreature),
												PlayerState::GetID(), 
												CreatureState::GetID(pCreature), 
												CreatureState::GetX(pCreature),
												CreatureState::GetY(pCreature)
												 ) 
								);
				}

				//g_pPlayer->PacketAddActionResult( effectID, pResult );
				PlayerState::PacketAddActionResult(0, pResult );
			}
		}
					
		//------------------------------------------------------
		// Spend the bullets.
		//------------------------------------------------------
		PlayerState::SetCurrentMagazineNumber( pPacket->getBullet() );

	}	
		


	__END_CATCH
}
