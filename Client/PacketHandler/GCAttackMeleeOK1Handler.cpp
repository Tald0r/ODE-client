//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackMeleeOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackMeleeOK1.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackMeleeOK1Handler::execute ( GCAttackMeleeOK1 * pPacket , Player * pPlayer )

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

	//------------------------------------------------------------------
	// Apply the status change.
	//------------------------------------------------------------------
	if (MStatus* pPlayerStatus = PlayerState::Status())
		AffectModifyInfo(pPlayerStatus, pPacket);

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
		if (pCreature != NULL)
		{
			// Show the result of the melee attack.
			// Hit when the action ends.
			if (pCreature!=NULL)
			{
				MActionResult* pResult = new MActionResult;
			
				pResult->Add( new MActionResultNodeActionInfo( 
											CreatureState::GetBasicActionInfo(pPlayerCreature),
											PlayerState::GetID(), 
											CreatureState::GetID(pCreature), 
											CreatureState::GetX(pCreature),
											CreatureState::GetY(pCreature)
											 ) 
							);

				//g_pPlayer->PacketAddActionResult( effectID, pResult );
				PlayerState::PacketAddActionResult(0, pResult );
			}
		}
	}	
		


	__END_CATCH
}
