//////////////////////////////////////////////////////////////////////
//
// Filename    : GCKnocksTargetBackOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCKnocksTargetBackOK1.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCKnocksTargetBackOK1Handler::execute ( GCKnocksTargetBackOK1 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		


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

		//------------------------------------------------------
		// Only a hit shows being hit.
		//------------------------------------------------------
		if (pPacket->getSkillSuccess())
		{
			// The creature is hit.
			if (pCreature != NULL)
			{			
				unsigned short x = pPacket->getX();
				unsigned short y = pPacket->getY();

				// The direction it is knocked back in.
				// The packet carries the cell after the move.
				//MCreature::GetPositionToDirection(x, y, pPacket->getDir());

				MActionResult* pResult = new MActionResult;

				pResult->Add( new MActionResultNodeChangePosition(
										pPacket->getObjectID(),
										x, y)
							);

			

				// Move the creature's cell at once.
				//pCreature->SetPosition( x, y );
				//pCreature->SetServerPosition( x, y );
				//pCreature->SetStop();

				// Show the result at once.
				//pCreature->PacketSpecialActionResult( 
				//					RESULT_SKILL_KNOCKS_TARGET_BACK//pPacket->getSkillType() + g_ActionInfoTable.GetMinResultActionInfo()
				//);			

				PlayerState::PacketAddActionResult(0 , pResult);

				//------------------------------------------------------
				// To stay in sync,
				// the server cell is set at once.
				//------------------------------------------------------
				CreatureState::SetServerPosition(pCreature, x, y );
			}
		}
		
		//------------------------------------------------------
		// Spend the bullets.
		//------------------------------------------------------
		PlayerState::SetCurrentMagazineNumber( pPacket->getBullet() );
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
