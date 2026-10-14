//--------------------------------------------------------------------------------
//
// Filename    : GCAddVampireFromTransformationHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddVampireFromTransformation.h"
#include "CreatureTypeSelection.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCAddVampireFromTransformationHandler::execute ( GCAddVampireFromTransformation * pPacket , Player * pPlayer )

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
		const PCVampireInfo3 & vi = pPacket->getVampireInfo ();

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
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, vi.getAdvancementLevel() );
			}
			//pCreature->SetCreatureType( 0 );
			//--------------------------------------------------
			// Set the CreatureType
			//--------------------------------------------------
			Shape_t shape	= vi.getShape();
			bool bMale		= vi.getSex()==MALE;
			ItemType_t coatType = vi.getCoatType();

			if (vi.getCompetence()==0)
			{
				CreatureState::SetCompetence( pCreature, 0 );
				CreatureState::SetCreatureType( pCreature, CREATURETYPE_VAMPIRE_OPERATOR );
				CreatureState::SetMale( pCreature, bMale );
			}
			else
			{
				CreatureState::SetCreatureType( pCreature, GetVampireCreatureType( shape, bMale, coatType ) );
			}

			CreatureState::SetMale( pCreature, bMale );

			if (CreatureState::GetCreatureType( pCreature )==CREATURETYPE_BAT)
			{
				CreatureState::SetFlyingCreature( pCreature );
			}
			else
			{
				CreatureState::SetGroundCreature( pCreature );
			}

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
			
			CreatureState::SetName( pCreature, vi.getName().c_str() );

			// for now
			CreatureState::SetGuildNumber( pCreature, vi.getGuildID() );
			

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

			//vi.getName()
			// colour information

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
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, vi.getAdvancementLevel() );
			}
			bool reAdd = false;

			// Remove it if it is not a plain wearing creature.
			if (!CreatureState::IsPlainCreatureWear( pCreature ))
			{
				ZoneState::RemoveCreature( CreatureState::GetID( pCreature ) );

				pCreature = ZoneState::NewCreatureWear();
				if (pCreature == NULL)
				{
					return;
				}

				CreatureState::SetID( pCreature, vi.getObjectID() );


				CreatureState::SetName( pCreature, vi.getName().c_str() );

				CreatureState::SetPosition( pCreature, vi.getX(), vi.getY() );

				reAdd = true;
			}
			else
			{
				CreatureState::MovePosition( pCreature, vi.getX(), vi.getY() );
			}

		
			//--------------------------------------------------
			// Set the CreatureType
			//--------------------------------------------------
			Shape_t shape	= vi.getShape();
			bool bMale		= vi.getSex()==MALE;
			ItemType_t coatType = vi.getCoatType();
			
			if (vi.getCompetence()==0)
			{
				CreatureState::SetCompetence( pCreature, 0 );
				CreatureState::SetCreatureType( pCreature, CREATURETYPE_VAMPIRE_OPERATOR );
				CreatureState::SetMale( pCreature, bMale );
			}
			else
			{
				CreatureState::SetCreatureType( pCreature, GetVampireCreatureType( shape, bMale, coatType ) );
			}

			CreatureState::SetMale( pCreature, bMale );

			if (CreatureState::GetCreatureType( pCreature )==CREATURETYPE_BAT)
			{
				CreatureState::SetFlyingCreature( pCreature );
			}
			else
			{
				CreatureState::SetGroundCreature( pCreature );
			}
			
			//pCreature->SetAction(ACTION_MOVE);			
			CreatureState::SetServerPosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetDirection( pCreature, vi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, vi.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			// colours
			CreatureState::SetBodyColor1( pCreature, vi.getSkinColor() );
			CreatureState::SetBodyColor2( pCreature, vi.getCoatColor() );

			// speed
			CreatureState::SetWeaponSpeed( pCreature, vi.getAttackSpeed() );

			// for now
			CreatureState::SetGuildNumber( pCreature, vi.getGuildID() );
			
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

			if (reAdd && !ZoneState::AddCreature( pCreature ))
			{
				ZoneState::DeleteCreature( pCreature );
				pCreature = NULL;
			}
		}

		if (pCreature!=NULL)
		{
			//--------------------------------------------------
			// Attach the effects..
			//--------------------------------------------------
			WorldEvents::SetEffectInfo( pCreature, pPacket->getEffectInfo(), 0 );

			//--------------------------------------------------
			// the effect of coming out of the transformation
			//--------------------------------------------------		
			WorldEvents::ExecuteActionInfoFromMainNode(
//				RESULT_MAGIC_HIDE,										// the skill used
				RESULT_MAGIC_UN_TRANSFORM,
			
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,
				CreatureState::GetDirection( pCreature ),														// the direction used
				
				OBJECTID_NULL,												// the target
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,
				
				0,													// the skill's (remaining) duration
				
				NULL,
				
				false, 0, 0);			// the skill starts from the beginning.
		}
	}

	// [Help] when a vampire appears
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_CREATURE_APPEAR_VAMPIRE );
//	__END_HELP_EVENT

	__END_CATCH
}
