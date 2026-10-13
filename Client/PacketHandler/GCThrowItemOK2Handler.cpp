//////////////////////////////////////////////////////////////////////
//
// Filename    : GCThrowItemOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCThrowItemOK2.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCThrowItemOK2Handler::execute ( GCThrowItemOK2 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do (the game crashed here).
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		

	int skillType = MAGIC_THROW_HOLY_WATER;

	//int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

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
		if (pCreature == NULL)
		{
			
		}
		else
		{
			// The player is hit by someone's skill.
			// [ TEST CODE ]
			MActionResult* pResult = new MActionResult;
			pResult->Add( new MActionResultNodeActionInfo( 
									skillType, 
									CreatureState::GetID(pCreature), 
									PlayerState::GetID(), 
									CreatureState::GetX(pPlayerCreature),
									CreatureState::GetY(pPlayerCreature)
									)
						);
									//delayFrame ) );		

			// [ TEST CODE ]
			//
			// Make the result and hand it over.
			//
			// Face the player.
			CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature));

			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToOther(pCreature, skillType, 
								PlayerState::GetID(),		// the player is hit
								pResult						// result
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
