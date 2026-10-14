//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddMonsterFromTransformationHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddMonsterFromTransformation.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddMonsterFromTransformationHandler::execute ( GCAddMonsterFromTransformation * pPacket , Player * pPlayer )

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
		#ifdef __DEBUG_OUTPUT__
			DEBUG_ADD_FORMAT("[AddMonster] type=%d, id=%d, xy=(%d,%d), dir=%d", 
								pPacket->getMonsterType(),
								pPacket->getObjectID(), 
								pPacket->getX(), pPacket->getY(), 
								pPacket->getDir());
		#endif

		//AfxMessageBox( pPacket->toString().c_str() );
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


			//CreatureState::SetCreatureType( pCreature, 0 );
			CreatureState::SetCreatureType( pCreature, pPacket->getMonsterType() );
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

			// [ TEST CODE ]
			// set the name
			//char str[20];
			//sprintf(str, "ID=%d", pCreature->GetID());
			//pCreature->SetName( (*g_pCreatureTable)[pPacket->getMonsterType()].Name.GetString() );
			CreatureState::SetName( pCreature, pPacket->getMonsterName().c_str() );

			// for now..
			CreatureState::SetGuildNumber( pCreature, 1 );

			// the name
			//pCreature->SetName( pPacket->getName().toString().c_str() );

			// the colour information
			

			if (!ZoneState::AddCreature( pCreature ))
			{	
				ZoneState::DeleteCreature( pCreature );
				pCreature = NULL;
			}
			else
			{
				//------------------------------------------------------------
				// Load it if it is not loaded.
				//------------------------------------------------------------
//				LoadCreatureType( pPacket->getMonsterType() );			
			}
		}
		else
		{
			// Remove it if it is not a plain creature.
			bool reAdd = false;

			if (!CreatureState::IsPlainCreature( pCreature ))
			{
				ZoneState::RemoveCreature( CreatureState::GetID( pCreature ) );

				pCreature = ZoneState::NewCreature();
				if (pCreature == NULL)
				{
					return;
				}

				CreatureState::SetID( pCreature, pPacket->getObjectID() );

					
				//pCreature->SetName( (*g_pCreatureTable)[pPacket->getMonsterType()].Name.GetString() );
				CreatureState::SetName( pCreature, pPacket->getMonsterName().c_str() );

				CreatureState::SetPosition( pCreature, pPacket->getX(), pPacket->getY() );				

				reAdd = true;
			}
			else
			{
				CreatureState::MovePosition( pCreature, pPacket->getX(), pPacket->getY() );				
			}

			CreatureState::SetCreatureType( pCreature, pPacket->getMonsterType() );
			CreatureState::SetGroundCreature( pCreature );
			
			//CreatureState::SetAction( pCreature, ACTION_MOVE);
			CreatureState::SetDirection( pCreature, pPacket->getDir() );
			CreatureState::SetCurrentDirection( pCreature, pPacket->getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );
			CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, pPacket->getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
			}

			// for now..
			CreatureState::SetGuildNumber( pCreature, 1 );	
			
			if (reAdd)
			{
				if (!ZoneState::AddCreature( pCreature ))
				{	
					ZoneState::DeleteCreature( pCreature );
					pCreature = NULL;
				}
				else
				{
					//------------------------------------------------------------
					// Load it if it is not loaded.
					//------------------------------------------------------------
//					LoadCreatureType( pPacket->getMonsterType() );			
				}
			}
		}

		if (pCreature!=NULL)
		{
			//--------------------------------------------------
			// Attach the effects..
			//--------------------------------------------------
			WorldEvents::SetEffectInfo( pCreature, pPacket->getEffectInfo(), 0 );

			//--------------------------------------------------
			// the effect of coming out of the burrow == the effect of burrowing
			//--------------------------------------------------		
			WorldEvents::ExecuteActionInfoFromMainNode(
				RESULT_MAGIC_HIDE,										// the skill used
			
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,
				CreatureState::GetDirection( pCreature ),														// the direction used
				
				OBJECTID_NULL,												// the target
				CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 
				
				0,													// the skill's (remaining) duration		
				
				NULL,
				
				false, 0, 0);			// the skill starts from the beginning.
		}
	}

	// [Help] when a monster appears
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_CREATURE_APPEAR_MONSTER );
//	__END_HELP_EVENT

	__END_CATCH
}
