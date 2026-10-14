//--------------------------------------------------------------------------------
//
// Filename    : GCMorphVampire2Handler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMorphVampire2.h"
#include "CreatureTypeSelection.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCMorphVampire2Handler::execute ( GCMorphVampire2 * pPacket , Player * pPlayer )

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
		const PCVampireInfo3& vi = pPacket->getSlayerInfo();

		MCreature* pCreature = ZoneState::GetCreature(vi.getObjectID());

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

			CreatureState::SetName( pCreature, vi.getName().c_str() );

			//pCreature->SetCreatureType( 0 );
			Shape_t shape	= vi.getShape();
			bool bMale		= vi.getSex()==MALE;
			ItemType_t coatType = vi.getCoatType();
			CreatureState::SetCreatureType( pCreature, GetVampireCreatureType( shape, bMale, coatType ) );

			CreatureState::SetMale( pCreature, bMale );

			CreatureState::SetGroundCreature( pCreature );
			CreatureState::SetID( pCreature, vi.getObjectID() );
			//pCreature->SetAction(ACTION_MOVE);
			CreatureState::SetPosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetServerPosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetDirection( pCreature, vi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, vi.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			// colours
			CreatureState::SetBodyColor1( pCreature, vi.getSkinColor() );
			CreatureState::SetBodyColor2( pCreature, vi.getCoatColor() );

			// speed
			CreatureState::SetWeaponSpeed( pCreature, vi.getAttackSpeed() );
			//--------------------------------------------------
			// [ TEST CODE ]
			//--------------------------------------------------
			// set the coat's colour
			//--------------------------------------------------
			/*
			if (pCreature->IsMale())
			{
				CreatureState::SetBodyColor2( pCreature, 91 );
			}
			else
			{
				CreatureState::SetBodyColor2( pCreature, 38 );
			}
			*/


			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, vi.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, vi.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, vi.getAlignment() );
			}

			//si.getName()
			// colour information
			// for now
			CreatureState::SetGuildNumber( pCreature, 2 );

			if (!ZoneState::AddCreature( pCreature ))
			{
				ZoneState::DeleteCreature( pCreature );
				pCreature = NULL;
			}
		}
		//--------------------------------------------------
		// A creature already there: change it
		//--------------------------------------------------
		else
		{
			CreatureState::SetName( pCreature, vi.getName().c_str() );

			// for now
			CreatureState::SetGuildNumber( pCreature, 2 );

			//pCreature->SetCreatureType( 0 );
			Shape_t shape	= vi.getShape();
			bool bMale		= vi.getSex()==MALE;
			ItemType_t coatType = vi.getCoatType();
			CreatureState::SetCreatureType( pCreature, GetVampireCreatureType( shape, bMale, coatType ) );

			CreatureState::SetMale( pCreature, bMale );

			CreatureState::SetGroundCreature( pCreature );
			//pCreature->SetID(vi.getObjectID());
			
			CreatureState::MovePosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetServerPosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetDirection( pCreature, vi.getDir() );
			//pCreature->SetCurrentDirection( vi.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			// colours
			CreatureState::SetBodyColor1( pCreature, vi.getSkinColor() );
			CreatureState::SetBodyColor2( pCreature, vi.getCoatColor() );

			// speed
			CreatureState::SetWeaponSpeed( pCreature, vi.getAttackSpeed() );

			//--------------------------------------------------
			// [ TEST CODE ]
			//--------------------------------------------------
			// set the coat's colour
			//--------------------------------------------------
			/*
			if (pCreature->IsMale())
			{
				CreatureState::SetBodyColor2( pCreature, 91 );
			}
			else
			{
				CreatureState::SetBodyColor2( pCreature, 38 );
			}
			*/

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, vi.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, vi.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, vi.getAlignment() );
			}			
			
		}
	}


	__END_CATCH
}
