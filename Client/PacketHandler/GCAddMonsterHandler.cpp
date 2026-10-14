//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddMonsterHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddMonster.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddMonsterHandler::execute ( GCAddMonster * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	//	cout << pPacket->toString() << endl;

	//AfxMessageBox( pPacket->toString().c_str() );
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
		WORD wMonsterType = pPacket->getMonsterType();
		// A monster in another race's shape (one in a vampire's shape needs nothing of its own)

		if(wMonsterType == 735 || wMonsterType == 793 || wMonsterType == 796 || wMonsterType == 797)
		{
			WorldEvents::AddRaceSlayerMonster(pPacket);
			
			return;
		}
		else if(wMonsterType == 737 || wMonsterType == 795 || wMonsterType == 800)
		{
			WorldEvents::AddRaceOustersMonster(pPacket);
			return;
		}
		// 2004, 9, 1, sobeit add start - in a siege, a trap is not shown to the attackers.
		else if( wMonsterType == 738 ||
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

			//pCreature->SetCreatureType( 0 );
			CreatureState::SetCreatureType( pCreature, pPacket->getMonsterType() );
			CreatureState::SetGroundCreature( pCreature );
			CreatureState::SetID( pCreature, pPacket->getObjectID() );
			//pCreature->SetAction(ACTION_MOVE);
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
			if (CreatureState::GetCreatureType( pCreature )!=pPacket->getMonsterType())
			{
				CreatureState::SetCreatureType( pCreature, pPacket->getMonsterType());
			}

			CreatureState::SetGroundCreature( pCreature );
			
			//pCreature->SetAction(ACTION_MOVE);
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

			// for now..
			CreatureState::SetGuildNumber( pCreature, 1 );
		}

		if (pCreature!=NULL)
		{
			//--------------------------------------------------
			// Attach the effects..
			//--------------------------------------------------
			bool bShowComeFromPotal = true;
			WorldEvents::SetEffectInfo( pCreature, pPacket->getEffectInfo(), 0 );
			// 2004, 6, 25, sobeit add start - Gilles de Rais's map effects
			switch(CreatureState::GetCreatureType( pCreature ))
			{
				case 724: // a vampire summoning, 1
					WorldEvents::AddGDREffect(1, true);
					break;
				case 725: // a vampire summoning, 2
					WorldEvents::AddGDREffect(2, true);
					break;
				case 739: // a trap - land mine
					WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_LAND_MINE_LIGHT,CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,CreatureState::GetDirection( pCreature ),	CreatureState::GetID( pCreature ),	
								CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 0xffff, NULL, false, 0, 0);			
					break;
				case 738: // a trap - pleasure station
					WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_PLEASUER_STATION_LIGHT,CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,CreatureState::GetDirection( pCreature ),	CreatureState::GetID( pCreature ),	
								CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 0xffff, NULL, false, 0, 0);			
					break;
				case 753:
					bShowComeFromPotal = false;
					CreatureState::SetAction( pCreature, ACTION_VAMPIRE_DRAIN);  
					break;
				case 765: // the effect on the Great Ruffian's body
					WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_GREAT_RUFFIAN_BODY,CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,CreatureState::GetDirection( pCreature ),	CreatureState::GetID( pCreature ),	
								CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 0xffff, NULL, false, 0, 0);			
					break;
				case 768: // miga
					bShowComeFromPotal = false;
					CreatureState::AddEffectStatus( pCreature, EFFECTSTATUS_SUMMON_MIGA_FIRE, 0xFFFF );
					break;
				case 786: // an officer
					CreatureState::AddEffectStatus( pCreature, EFFECTSTATUS_GRAND_MASTER_VAMPIRE, 0xFFFF );
					break;
				case 788: // 
					bShowComeFromPotal = false;
					CreatureState::AddEffectStatus( pCreature, EFFECTSTATUS_DRAGON_TORNADO, 0xFFFF );
					break;
				case 789:
					bShowComeFromPotal = false;
					CreatureState::AddEffectStatus( pCreature, EFFECTSTATUS_DRAGON_TORNADO_CHILD, 0xFFFF );
					break;
				case 793:
				case 794:
				case 795: // the sacrifices
					bShowComeFromPotal = false;
					CreatureState::AddEffectStatus( pCreature, EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS, 0xffff );
					WorldEvents::ExecuteActionInfoFromMainNode(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS,CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0,2,	CreatureState::GetID( pCreature ),	
							CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 0xffff, NULL, false, 0, 0);			
					break;
			
			}
			// 2004, 6, 25, sobeit add end - Gilles de Rais
			//--------------------------------------------------
			// 0 normally, 1 through a portal
			//--------------------------------------------------
			if (pPacket->getFromFlag()==1 && bShowComeFromPotal)
			{
//				if(pCreature->GetCreatureType() == 753)
//				{
//					pCreature->SetAction(ACTION_VAMPIRE_DRAIN);  
//				}
//				else
//				{
					WorldEvents::ComeFromPortal( pCreature );
					
					// the effect on the ground
					int x			= CreatureState::GetX( pCreature );
					int y			= CreatureState::GetY( pCreature );
					int delayFrame	= ConvertDurationToFrame( 18 );
					
					WorldEvents::ExecuteActionInfoFromMainNode(
						RESULT_MAGIC_BLOODY_TUNNEL,		// the value itself is a RESULT_ACTIONINFO.
						
						x, y, 0,
						DIRECTION_DOWN,
						
						OBJECTID_NULL,
						x, y, 0,
						
						delayFrame,	
						
						NULL,
						true, 0, 0
						);
//				}
			}
			
			
		}
	}
	
	// [Help] when a monster appears
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_CREATURE_APPEAR_MONSTER );
//	__END_HELP_EVENT


	__END_CATCH
}
