//////////////////////////////////////////////////////////////////////
//
// Filename    : GCPetUseSkillHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCPetUseSkill.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCPetUseSkillHandler::execute ( GCPetUseSkill * pGCPetUseSkill , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
	TYPE_OBJECTID attackerID = pGCPetUseSkill->getAttacker();
	TYPE_OBJECTID targetID = pGCPetUseSkill->getTarget();

	MCreature *pAttackerCreature = ZoneState::GetCreature(attackerID);
	MCreature *pTargetCreature = ZoneState::GetCreature(targetID);

	if( pAttackerCreature == NULL )
	{
		DEBUG_ADD_FORMAT("[GCPetUseSkillHandler] no attacker (%d)-_-;", attackerID);
	}
	else
	{
		if( pTargetCreature == NULL )
		{
			DEBUG_ADD_FORMAT("[GCPetUseSkillHandler] no target (%d)-_-;", targetID);
		}
		else
		{
			CreatureState::SetAction( pTargetCreature, ACTION_DAMAGED );
		
			MCreature *pPet = ZoneState::GetFakeCreature(CreatureState::GetPetID(pAttackerCreature));
			
			if( pPet == NULL )
			{
				DEBUG_ADD_FORMAT("[GCPetUseSkillHandler] no pet (%d)-_-;", CreatureState::GetPetID(pAttackerCreature));
			}
			else
			{
//				pPet->SetAction( ACTION_ATTACK );
//				pPet->SyncTurretDirection();

				int skillID = SKILL_CLIENT_TANK_ATTACK_3;
				MPetItem* pPetItem = CreatureState::GetPetItem(pPet);
				if(pPetItem != NULL)
				{
					int petItemType = pPetItem->GetItemType();
					int petLevel = pPetItem->GetNumber();

					TYPE_SOUNDID soundID = (*g_pCreatureTable)[CreatureState::GetCreatureType(pPet)].GetActionSound( ACTION_ATTACK );

					if (soundID!=SOUNDID_NULL  )
					{
						WorldEvents::PlaySoundAt( soundID,
									false,
									CreatureState::GetX(pPet), CreatureState::GetY(pPet));
					}

					switch(petItemType)
					{
					// tank-_-;
					case 3:
						{
							WorldEvents::ExecuteActionInfoFromMainNode(
								SKILL_CLIENT_TANK_ATTACKED,										// the skill used
								
								0, 0, 0,
								DIRECTION_DOWN, // the direction used
								
								CreatureState::GetID(pTargetCreature),												// the target
								0, 0, 0, 
								
								0,													// the skill's (remaining) duration
								
								NULL,
								
								false, 0, 0);	
						}

						if(petLevel >= 30 && petLevel < 40)
							skillID = SKILL_CLIENT_TANK_ATTACK_3;
						else if(petLevel >= 40 && petLevel < 50)
							skillID = SKILL_CLIENT_TANK_ATTACK_4;
						else if(petLevel >= 50)
							skillID = SKILL_CLIENT_TANK_ATTACK_5;
						break;
					case 4:	// stirge
						if(petLevel<20)
							skillID = SKILL_CLIENT_STIRGE_BLOOD_DRAIN_1;
						else if(petLevel >= 20 && petLevel < 30)
							skillID = SKILL_CLIENT_STIRGE_BLOOD_DRAIN_2;
						else if(petLevel >= 30 && petLevel < 40)
							skillID = SKILL_CLIENT_STIRGE_BLOOD_DRAIN_3;
						else if(petLevel >= 40 && petLevel < 50)
							skillID = SKILL_CLIENT_STIRGE_BLOOD_DRAIN_4;
						else if(petLevel >= 50)
							skillID = SKILL_CLIENT_STIRGE_BLOOD_DRAIN_5;
						
						{
							WorldEvents::ExecuteActionInfoFromMainNode(
								skillID,										// the skill used
								
								0, 0, 0,
								DIRECTION_DOWN, // the direction used
								
								CreatureState::GetID(pTargetCreature),												// the target
								0, 0, 0, 
								
								0,													// the skill's (remaining) duration
								
								NULL,
								
								true, 0, 0);	
						}
						break;
					case 5:	// pixie
						if(petLevel<20)
							skillID = SKILL_CLIENT_PIXIE_ABSORB_SOUL_1;
						else if(petLevel >= 20 && petLevel < 30)
							skillID = SKILL_CLIENT_PIXIE_ABSORB_SOUL_2;
						else if(petLevel >= 30 && petLevel < 40)
							skillID = SKILL_CLIENT_PIXIE_ABSORB_SOUL_3;
						else if(petLevel >= 40 && petLevel < 50)
							skillID = SKILL_CLIENT_PIXIE_ABSORB_SOUL_4;
						else if(petLevel >= 50)
							skillID = SKILL_CLIENT_PIXIE_ABSORB_SOUL_5;

						WorldEvents::ExecuteActionInfoFromMainNode(
							skillID,										// the skill used
							
							0, 0, 0,
							DIRECTION_DOWN, // the direction used
							
							CreatureState::GetID(pTargetCreature),												// the target
							0, 0, 0, 
							
							0,													// the skill's (remaining) duration
							
							NULL,
							
							false, 0, 0);	
						break;
					}
				}

				CreatureState::SetTurretDelay( pPet, 3 );
//				pPet->SetDirectionToPosition(pTargetCreature->GetX(), pTargetCreature->GetY());
				int sx = 0, sy = 0;
				BYTE petDirect = static_cast<BYTE>(CreatureState::GetDirectionToPosition(pPet, CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature)));

				// 2004, 12, 21, sobeit add start - set the Centauro turret's position
				const int petType = CreatureState::GetCreatureType(pPet);
				if( petType == 702 || petType == 703 || petType == 704 )
					CreatureState::SetTurretFinalDirection(pPet, petDirect);
				// 2004, 12, 21, sobeit add end

				if( petType == 702 )
				{
					POINT pointGap[8] = 
					{
						{ 11, 6 },	// left
						{ 8, 2 },	// left-down
						{ 0, 0 },	// down
						{ -7, 2 },	// right-down
						{ -10, 6 },	// right
						{ -7, 10 },	// right-up
						{ 0, 12 },	// up
						{ 8, 10 },	// left-up
					};
					const int petFacing = CreatureState::GetDirection(pPet);
					sx = pointGap[petFacing].x-pointGap[petDirect].x;
					sy = pointGap[petFacing].y-pointGap[petDirect].y;
				}
				WorldEvents::ExecuteActionInfoFromMainNode(
					skillID,										// the skill used
					
					CreatureState::GetX(pPet), CreatureState::GetY(pPet), 0,
					petDirect, // the direction used
					
					CreatureState::GetID(pPet),												// the target
					CreatureState::GetX(pPet), CreatureState::GetY(pPet), 0, 
					
					0,													// the skill's (remaining) duration
					
					NULL,
					
					false, sx, sy);	
				
			}
		}
	}

	//cout << pGCPetUseSkill->toString() << endl;
	

	__END_CATCH
}
