//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddBatHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddBat.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddBatHandler::execute ( GCAddBat * pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPlayer;
		


	int batCreatureType = 185;

	// _MinTrace("%d\n", pPacket->getBatColor() ); // Windows-only debug trace

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
		//AfxMessageBox( pPacket->toString().c_str() );
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

			
			//CreatureState::SetCreatureType( pCreature, 0 );
			CreatureState::SetCreatureType( pCreature, batCreatureType );


			CreatureState::SetName( pCreature, pPacket->getName().c_str() );

			// for now
			CreatureState::SetGuildNumber( pCreature, pPacket->getGuildID() );

			CreatureState::SetFlyingCreature( pCreature );

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
			
			if( pPacket->getBatColor() != 0 )
				CreatureState::SetBatColor( pCreature, pPacket->getBatColor() );
			else
				CreatureState::SetBatColor( pCreature, 0xFFFF );
			

			if (!ZoneState::AddCreature( pCreature ))
			{
				ZoneState::DeleteCreature( pCreature );
			}
		}
		//--------------------------------------------------
		// A creature already there
		//--------------------------------------------------
		else
		{
			//CreatureState::SetCreatureType( pCreature, batCreatureType );
			CreatureState::SetFlyingCreature( pCreature );
			
			// for now
			CreatureState::SetGuildNumber( pCreature, pPacket->getGuildID() );

			//CreatureState::SetAction( pCreature, ACTION_MOVE);
			CreatureState::MovePosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetDirection( pCreature, pPacket->getDir() );
			CreatureState::SetCurrentDirection( pCreature, pPacket->getDir() );
			//CreatureState::SetAction( pCreature, ACTION_STAND );			

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, pPacket->getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
			}

			if( pPacket->getBatColor() != 0 )
				CreatureState::SetBatColor( pCreature, pPacket->getBatColor() );
			else
				CreatureState::SetBatColor( pCreature, 0xFFFF );

			//--------------------------------------------------
			// the result: it turns into a bat
			//--------------------------------------------------
			MActionResult* pResult = new MActionResult;

			pResult->Add( new MActionResultNodeChangeCreatureType( CreatureState::GetID( pCreature ), batCreatureType ) );

			//--------------------------------------------------
			// turning into a bat 
			//--------------------------------------------------								
			WorldEvents::ExecuteActionInfoFromMainNode(
				RESULT_MAGIC_TRANSFORM_TO_BAT,										// the skill used
			
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,
				CreatureState::GetDirection( pCreature ),														// the direction used
				
				OBJECTID_NULL,												// the target
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 
				
				0,													// the skill's (remaining) duration		
				
				pResult, //NULL,
				
				false, 0, 0);			// the skill starts from the beginning.

			//pCreature->SetDelay( 1000 );
		}	
	}

	// [Help] when a vampire appears
//	__BEGIN_HELP_EVENT
//		//ExecuteHelpEvent( HE_CREATURE_APPEAR_VAMPIRE );
//	__END_HELP_EVENT


	__END_CATCH
}
