//--------------------------------------------------------------------------------
//
// Filename    : GCFastMoveHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCFastMove.h"
#include "Player.h"
#include "DebugLog.h"
#include "DirectionSelection.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCFastMoveHandler::execute ( GCFastMove * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		


	int objectID = pPacket->getObjectID();

	// (ox, oy) --> (nx, ny)
	int ox = pPacket->getFromX();
	int oy = pPacket->getFromY();
	int nx = pPacket->getToX();
	int ny = pPacket->getToY();
	// Not cast to ACTIONINFO: the wire's 16 bits run past the enum's range.
	TYPE_ACTIONINFO skillType = pPacket->getSkillType();

	MCreature* pCreature = NULL;
	
	//------------------------------------------------------------------
	// The player
	//------------------------------------------------------------------
	if (static_cast<TYPE_OBJECTID>(objectID)==PlayerState::GetID())
	{
		//------------------------------------------------------------------
		// The skill the player waited on was verified.
		//------------------------------------------------------------------
		if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_SKILL_SUCCESS)
		{		
			PlayerState::SetWaitVerifyNULL();
		}
		else
		{
			DEBUG_ADD_ERR("[Error] Player is not WaitVerifySkillSuccess");
		}

		pCreature = pPlayerCreature;
	}
	else
	{
		pCreature = ZoneState::GetCreature( objectID );
	}

	if (pCreature == NULL)
	{
		// No such character.
		DEBUG_ADD_FORMAT_ERR("[Error] There is no such Creature. id=%d", objectID);
	}
	// The creature is there..
	else
	{
		switch(skillType)
		{
		case SKILL_CHARGING_ATTACK:
				WorldEvents::ExecuteActionInfoFromMainNode(
							SKILL_CLIENT_CHARGING_ATTACK,										// the skill used
						
							ox, oy, 0,
							CreatureState::GetDirection(pCreature),														// the direction used
							
							CreatureState::GetID(pCreature),												// the target
							nx, ny, 0,
							
							17, //5*16, 
							
							NULL,
							
							false,
							
							0, 0);	

				if(pCreature == pPlayerCreature)
				{
					PlayerState::SetDelay(1000);
				}
				CreatureState::FastMovePosition( pCreature, nx, ny, true );
				CreatureState::SetGhost(pCreature, 3, 16);
			break;
			
		case SKILL_TELEPORT:
			{
				WorldEvents::ExecuteActionInfoFromMainNode(
							SKILL_CLIENT_TELEPORT,										// the skill used
						
							ox, oy, 0,
							CreatureState::GetDirection(pCreature),														// the direction used
							
							CreatureState::GetID(pCreature),												// the target
							ox, oy, 0,
							
							20, //5*16, 
							
							NULL,
							
							false,
							
							0, 0);	

				//pCreature->SetPosition(nx, ny);
				CreatureState::FastMovePosition( pCreature, nx, ny, true );
				if(pCreature == pPlayerCreature)
				{
					PlayerState::SetDelay(1250);
				}
				CreatureState::SetGhost(pCreature, 0xFF, 18);

				WorldEvents::ExecuteActionInfoFromMainNode(
							SKILL_CLIENT_TELEPORT2,										// the skill used
						
							nx, ny, 0,
							CreatureState::GetDirection(pCreature),														// the direction used
							
							CreatureState::GetID(pCreature),												// the target
							nx, ny, 0,
							
							20, //5*16, 
							
							NULL,
							
							false,
							
							0, 0);	

			}
			break;

		case SKILL_DUCKING_WALLOP:
			{
				WorldEvents::ExecuteActionInfoFromMainNode(
							SKILL_CLIENT_DUCKING_WALLOP,										// the skill used
						
							ox, oy, 0,
							CreatureState::GetDirectionToPosition(pCreature, nx, ny),														// the direction used
							
							CreatureState::GetID(pCreature),												// the target
							ox, oy, 0,
							
							10, //5*16, 
							
							NULL,
							
							false,
							
							0, 0);	

				CreatureState::FastMovePosition( pCreature, nx, ny, true );
				CreatureState::SetGhost(pCreature, 0xFF, 9);
				if(pCreature == pPlayerCreature)
				{
					PlayerState::SetDelay(565);
				}
			}
			break;
		case SKILL_BIKE_CRASH:
			{
				MItem*	pItem = MItem::NewItem( ITEM_CLASS_MOTORCYCLE );
				if(pItem != NULL)
				{
					CreatureState::AddEffectStatus(pCreature, EFFECTSTATUS_BIKE_CRASH, 0xffff);
					pItem->SetItemType( 6 );
					CreatureState::SetAddonItem( pCreature, pItem );
					delete pItem;
					int TempDir = SelectFacingDirection(CreatureState::GetX(pCreature), CreatureState::GetY(pCreature), nx, ny);
					CreatureState::SetInstallTurretDirect(pCreature, TempDir);
					CreatureState::SetDirection(pCreature, TempDir);
					CreatureState::SetCurrentDirection(pCreature, TempDir);
					CreatureState::SetInstallTurretCount(pCreature, 0);
					if(CreatureState::GetID(pCreature) == PlayerState::GetID())
						PlayerState::SetDelay(3000);
				}
				CreatureState::FastMovePosition( pCreature, nx, ny, true );
			}
			break;
		default:
			{
//							
				CreatureState::FastMovePosition( pCreature, nx, ny, true );
				
			}
		}
	}



	__END_CATCH
}
