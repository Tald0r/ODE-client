//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddBurrowingCreatureHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddBurrowingCreature.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddBurrowingCreatureHandler::execute ( GCAddBurrowingCreature * pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPlayer;
		


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
			pCreature = ZoneState::NewCreature();
			if (pCreature == NULL)
			{
				return;
			}


			// it will not move anyway, so it does not matter.
			CreatureState::SetCreatureType( pCreature, CREATURETYPE_VAMPIRE_MALE1 );	


			CreatureState::SetName( pCreature, pPacket->getName().c_str() );

			
			CreatureState::SetUndergroundCreature( pCreature );
			CreatureState::SetID( pCreature, pPacket->getObjectID() );
			
			CreatureState::SetPosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetDirection( pCreature, DIRECTION_DOWN );
			CreatureState::SetCurrentDirection( pCreature, DIRECTION_DOWN );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			CreatureState::SetGuildNumber( pCreature, -1 );

			MStatus* pStatus = CreatureState::Status( pCreature );
			if (pStatus != NULL)
			{
				pStatus->SetStatus( MODIFY_MAX_HP, 100 );
				pStatus->SetStatus( MODIFY_CURRENT_HP, 100 );
			}

			//si.getName()
			// the colour information

			

			if (!ZoneState::AddCreature( pCreature ))
			{
				ZoneState::DeleteCreature( pCreature );
			}
		}
		else
		{
			// Already there.. make it an underground creature.
			CreatureState::SetUndergroundCreature( pCreature );

			// set its name again - -;
			CreatureState::SetName( pCreature, pPacket->getName().c_str() );

			// set its cell
			CreatureState::MovePosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );

			CreatureState::SetGuildNumber( pCreature, -1 );
		}
	}


	__END_CATCH
}
