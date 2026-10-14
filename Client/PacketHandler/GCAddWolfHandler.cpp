//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddWolfHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddWolf.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddWolfHandler::execute ( GCAddWolf * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	
	int wolfCreatureType = 186;

	if( pPacket->getItemType() == 39 )			// turning into a werewolf
		wolfCreatureType = CREATURETYPE_WER_WOLF;
	
	//------------------------------------------------------
	// The zone is not created yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
	}	
	//------------------------------------------------------
	// Normal.. 
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature(pPacket->getObjectID());

		//--------------------------------------------------
		// A new creature: add it
		//--------------------------------------------------
		if (pCreature==NULL)
		{
			pCreature = ZoneState::NewCreatureWear();
			if (pCreature == NULL)
			{
				return;
			}
//			pCreature->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, vi.getAdvancementLevel() );

			
			
			CreatureState::SetName( pCreature, pPacket->getName().c_str() );


			//CreatureState::SetCreatureType( pCreature, 0 );
			CreatureState::SetCreatureType( pCreature, wolfCreatureType );
			CreatureState::SetGroundCreature( pCreature );
			CreatureState::SetID( pCreature, pPacket->getObjectID() );
			//CreatureState::SetAction( pCreature, ACTION_MOVE);
			CreatureState::SetPosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetDirection( pCreature, pPacket->getDir() );
			CreatureState::SetCurrentDirection( pCreature, pPacket->getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, pPacket->getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
			}

			//pPacket->getName()
			// the colour information
			// for now
			CreatureState::SetGuildNumber( pCreature, pPacket->getGuildID() );

			if (!ZoneState::AddCreature( pCreature ))
			{
				ZoneState::DeleteCreature( pCreature );
				pCreature = NULL;
			}
		}
		//--------------------------------------------------
		// A creature already there
		//--------------------------------------------------
		else
		{
			CreatureState::SetCreatureType( pCreature, wolfCreatureType );

			// for now
			CreatureState::SetGuildNumber( pCreature, pPacket->getGuildID() );

			CreatureState::SetGroundCreature( pCreature );
			
			//CreatureState::SetAction( pCreature, ACTION_MOVE);
			CreatureState::MovePosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetDirection( pCreature, pPacket->getDir() );
			CreatureState::SetCurrentDirection( pCreature, pPacket->getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );			

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, pPacket->getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
			}

			//--------------------------------------------------
			// the result: it turns into a wolf
			//--------------------------------------------------
			MActionResult* pResult = new MActionResult;

			pResult->Add( new MActionResultNodeChangeCreatureType( CreatureState::GetID( pCreature ), wolfCreatureType ) );

			//--------------------------------------------------
			// turning into a wolf 
			//--------------------------------------------------								
			WorldEvents::ExecuteActionInfoFromMainNode(
				RESULT_MAGIC_TRANSFORM_TO_WOLF,										// the skill used
			
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,
				CreatureState::GetDirection( pCreature ),														// the direction used
				
				OBJECTID_NULL,												// the target
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 
				
				0,													// the skill's (remaining) duration		
				
				pResult, //NULL,
				
				false, 0, 0);			// the skill starts from the beginning.

			//pCreature->SetDelay( 1000 );
		}	

		// This runs after both arms of the if/else above, and the "new
		// creature" arm nulls pCreature when the zone refuses it (see
		// ZoneState::AddCreature).
		if( pCreature != NULL && wolfCreatureType == CREATURETYPE_WER_WOLF )
			CreatureState::SetBodyColor1( pCreature, pPacket->getColor() );
	}

	// [Help] when a vampire appears
//	__BEGIN_HELP_EVENT
//		//ExecuteHelpEvent( HE_CREATURE_APPEAR_VAMPIRE );
//	__END_HELP_EVENT


	__END_CATCH
}
