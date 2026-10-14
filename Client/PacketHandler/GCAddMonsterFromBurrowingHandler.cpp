//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddMonsterFromBurrowingHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddMonsterFromBurrowing.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddMonsterFromBurrowingHandler::execute ( GCAddMonsterFromBurrowing * pPacket , Player * pPlayer )

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
		//AfxMessageBox( pPacket->toString().c_str() );
		MCreature* pCreature = ZoneState::GetCreature(pPacket->getObjectID());

		// 2004, 9, 1, sobeit add start - in a siege, a trap is not shown to the attackers.
		WORD wMonsterType = pPacket->getMonsterType();
		if( wMonsterType == 738 ||
			wMonsterType == 739 ||
			wMonsterType == 740 ||
			wMonsterType == 741 )
		{
			EffectInfo* pTempEffectInfo = pPacket->getEffectInfo();
			
			if(NULL != pTempEffectInfo)
			{
				if(pTempEffectInfo->getListNum()>0)
				{
					std::list<WORD> TemList = pTempEffectInfo->GetList();
					
					std::list<WORD>::iterator itr = TemList.begin();
					bool bCheck = true;
					while(itr != TemList.end())
					{
						if(bCheck)
						{
							EFFECTSTATUS	status		= (EFFECTSTATUS)*itr;
							
							if(status == EFFECTSTATUS_HIDE_TO_ATTACKER)
							{
								if(PlayerState::Creature() != NULL && (PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_1) || 
									PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_2) || 
									PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3) || 
									PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_4) || 
									PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_5)) )
									return;
							}
						}
						bCheck = !bCheck;

						itr ++ ;
					}
				}			

			}
		}
		// 2004, 9, 1, sobeit add end - in a siege, a trap is not shown to the attackers.
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
		//--------------------------------------------------
		// A creature already there
		//--------------------------------------------------
		else
		{
			//CreatureState::SetCreatureType( pCreature, 0 );
			CreatureState::SetCreatureType( pCreature, pPacket->getMonsterType() );
			CreatureState::SetGroundCreature( pCreature );
			CreatureState::SetID( pCreature, pPacket->getObjectID() );
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

			//------------------------------------------------------------
			// Load it if it is not loaded.
			//------------------------------------------------------------
//			LoadCreatureType( pPacket->getMonsterType() );			
		}

		//--------------------------------------------------
		// Attach the effects..
		//--------------------------------------------------
		if (pCreature!=NULL)
		{
			WorldEvents::SetEffectInfo( pCreature, pPacket->getEffectInfo(), 0 );		
			// 2004, 8, 31, sobeit add start - an effect on a trap
			switch(CreatureState::GetCreatureType( pCreature ))
			{
				case 739: // a trap - land mine
					WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_LAND_MINE_LIGHT,CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,CreatureState::GetDirection( pCreature ),	CreatureState::GetID( pCreature ),	
								CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 0xffff, NULL, false, 0, 0);			
					break;
				case 738: // a trap - pleasure station
					WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_PLEASUER_STATION_LIGHT,CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,CreatureState::GetDirection( pCreature ),	CreatureState::GetID( pCreature ),	
								CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 0xffff, NULL, false, 0, 0);			
					break;
			}
			// 2004, 8, 31, sobeit add end -
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
