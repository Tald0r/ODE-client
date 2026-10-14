//--------------------------------------------------------------------------------
//
// Filename    : GCMorphSlayer2Handler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMorphSlayer2.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCMorphSlayer2Handler::execute ( GCMorphSlayer2 * pPacket , Player * pPlayer )

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
		const PCSlayerInfo3 & si = pPacket->getSlayerInfo();

		MCreature* pCreature = ZoneState::GetCreature(si.getObjectID());

		//--------------------------------------------------
		// A new creature: add it
		// It is a creature that wears gear.
		//--------------------------------------------------
		if (pCreature==NULL)
		{
			MCreature* pCreatureWear = ZoneState::NewCreatureWear();
			if (pCreatureWear == NULL)
			{
				return;
			}
	
	

			CreatureState::SetCreatureType( pCreatureWear, (si.getSex()==MALE)? CREATURETYPE_SLAYER_MALE : CREATURETYPE_SLAYER_FEMALE );

			CreatureState::SetGroundCreature( pCreatureWear );
			CreatureState::SetID( pCreatureWear, si.getObjectID() );
			//pCreatureWear->SetAction(ACTION_MOVE);
			CreatureState::SetPosition( pCreatureWear, si.getX(), si.getY() );
			CreatureState::SetServerPosition( pCreatureWear, si.getX(), si.getY() );
			CreatureState::SetDirection( pCreatureWear, si.getDir() );
			CreatureState::SetCurrentDirection( pCreatureWear, si.getDir() );
			CreatureState::SetAction( pCreatureWear, ACTION_STAND );

			// skin colour
			CreatureState::SetBodyColor1( pCreatureWear, si.getSkinColor() );
			
			if (MStatus* pStatus = CreatureState::Status( pCreatureWear ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, si.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, si.getCurrentHP() );
			}

			// the name
			CreatureState::SetName( pCreatureWear, si.getName().c_str() );

			// the colour information

			// its gear --> for now, in basic clothes.
			//----------------------------------------	
			// Put the gear on.
			//----------------------------------------	
			CreatureState::SetAddonToSlayer3( pCreatureWear, &si );

			// for now
			CreatureState::SetGuildNumber( pCreatureWear, -1 );

			
			if (!ZoneState::AddCreature( pCreatureWear ))
			{
				ZoneState::DeleteCreature( pCreatureWear );
				pCreatureWear = NULL;
				pCreature = NULL;
			}
		}
		//--------------------------------------------------
		// A creature already there: change it
		//--------------------------------------------------
		else
		{
			if (CreatureState::IsPlainCreatureWear( pCreature ))
			{
				MCreature* pCreatureWear = pCreature;

				CreatureState::SetCreatureType( pCreatureWear, (si.getSex()==MALE)? CREATURETYPE_SLAYER_MALE : CREATURETYPE_SLAYER_FEMALE );

				CreatureState::SetGroundCreature( pCreatureWear );
				//pCreatureWear->SetID(si.getObjectID());

				// for now
				CreatureState::SetGuildNumber( pCreatureWear, -1 );
				
				CreatureState::SetServerPosition( pCreatureWear, si.getX(), si.getY() );				
				CreatureState::MovePosition( pCreatureWear, si.getX(), si.getY() );
				CreatureState::SetDirection( pCreatureWear, si.getDir() );
				//pCreatureWear->SetCurrentDirection( si.getDir() );
				CreatureState::SetAction( pCreatureWear, ACTION_STAND );

				// skin colour
				CreatureState::SetBodyColor1( pCreatureWear, si.getSkinColor() );
			
				if (MStatus* pStatus = CreatureState::Status( pCreatureWear ))
				{
					pStatus->SetStatus( MODIFY_MAX_HP, si.getMaxHP() );
					pStatus->SetStatus( MODIFY_CURRENT_HP, si.getCurrentHP() );
				}

				// the name
				CreatureState::SetName( pCreatureWear, si.getName().c_str() );

				// the colour information

				//----------------------------------------	
				// Put the gear on.
				//----------------------------------------	
				CreatureState::SetAddonToSlayer3( pCreatureWear, &si );
			}
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] the Creature is Not CreatureWear. id=%d", CreatureState::GetID( pCreature ));
			}
		}

	}


	__END_CATCH
}
