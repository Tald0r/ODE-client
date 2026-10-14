//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSayHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSay.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "UserInformation.h"
#include "MChatManager.h"
#include "MCreatureTable.h"
#include "MSkillManager.h"
#include "MZoneTable.h"	
#include "MGameStringTable.h"
#include "MStatus.h"

//////////////////////////////////////////////////////////////////////
//
// Run when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCSayHandler::execute ( GCSay * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	MStatus* const pPlayerStatus = PlayerState::Status();
	if (pPlayerCreature == NULL || pPlayerStatus == NULL)
	{
		return;
	}
	

	// Debug Message	
	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// The normal case. 
	//------------------------------------------------------
	else
	{
		//---------------------------------------------------------------
		// Show the chat over the character's head.
		//---------------------------------------------------------------
		//DEBUG_ADD("Zone-GetCreature");

		MCreature*	pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		if (pCreature==NULL)
		{
			// Ignored when there is no such character.
			DEBUG_ADD("No such Character");
		}
		else
		{			
			bool IsGildre	= CreatureState::GetCreatureType(pCreature) == 717 || 
							  CreatureState::GetCreatureType(pCreature) == 723 ; // Gilles de Rais
			bool bMasterWords = CreatureState::GetCreatureType(pCreature)==CREATURETYPE_SLAYER_OPERATOR
								|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_OPERATOR
								|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_OUSTERS_OPERATOR
								|| strncmp( CreatureState::GetName(pCreature), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetString(), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetLength() ) == 0
								|| IsGildre ;
//								|| (strstr(pCreature->GetName(), "GM")!=NULL);

			//DEBUG_ADD_FORMAT("isMasterWords=%d", bMasterWords);

			//-------------------------------------------------
			// The normal case: ordinary talk.
			//-------------------------------------------------
			if (bMasterWords 
				|| g_pChatManager->IsAcceptID( CreatureState::GetName(pCreature) ))
			{
				//DEBUG_ADD("can see");

				// Chat.
				char str[256];

				bool isPlayerVampire = CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_MALE1
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_FEMALE1
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_MALE2
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_FEMALE2
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_MALE3
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_FEMALE3
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_OPERATOR
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_BAT
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_WOLF
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_WER_WOLF
										// Added by coffee, 2006.11.24.
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_FEMALE4
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_MALE4
										//add by viva
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_MALE5
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_FEMALE5
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_MALE6
										|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_VAMPIRE_FEMALE6;
										// add end

				bool isPlayerCharacter = (isPlayerVampire 
											|| CreatureState::IsSlayer(pCreature)
											|| CreatureState::IsOusters(pCreature)
											|| CreatureState::GetCreatureType(pCreature)==CREATURETYPE_SLAYER_OPERATOR);

				//DEBUG_ADD("strcpy");
				
				//---------------------------------------------------------------
				// Wansung --> Johab
				//---------------------------------------------------------------
				//UI_WansungToJohap( pPacket->getMessage().c_str(), str );
				snprintf(str, sizeof(str), "%s", pPacket->getMessage().c_str());
				bool	bTranslation = g_pSkillAvailable->IsEnableSkill(SKILL_HOLYLAND_BLOOD_BIBLE_HILLEL) || 
					!g_pZoneTable->Get( ZoneState::GetID() )->ChatMaskByRace ||
					PlayerState::HasEffectStatus(EFFECTSTATUS_TRANSLATION ) ||
					CreatureState::GetCreatureType(pPlayerCreature) == CREATURETYPE_SLAYER_OPERATOR ||
					CreatureState::GetCreatureType(pPlayerCreature) == CREATURETYPE_VAMPIRE_OPERATOR ||
					CreatureState::GetCreatureType(pPlayerCreature) == CREATURETYPE_OUSTERS_OPERATOR;

				//--------------------------------------------------
				// Remove curses.
				// Filtered unless an operator said it or I am one.
				// --> An operator's words are all shown, and an operator sees all.
				//--------------------------------------------------
				if (!bMasterWords && !g_pUserInformation->IsMaster 
					&& !PlayerState::HasEffectStatus(EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
				&& !g_bLight
#endif
					)
				{
				
					// Remove curses only from a player.
					if (isPlayerCharacter)
					{
						DEBUG_ADD("remove curse");
						g_pChatManager->RemoveCurse( str );

						DEBUG_ADD("has effect");

						if (PlayerState::HasEffectStatus(EFFECTSTATUS_HALLUCINATION))
						{
							//DEBUG_ADD("add mask hallu");

							g_pChatManager->AddMask(str, 50);
						}

						//DEBUG_ADD("ok");
					}					 

					#ifndef _DEBUG
						//--------------------------------------------------
						// Another race.
						//--------------------------------------------------
						if (PlayerState::GetRace() != CreatureState::GetRace(pCreature)
							// Only a player's, so it cannot be heard...
							//&& isPlayerVampire 
							&& isPlayerCharacter
							&& !bTranslation 
							&& !PlayerState::HasEffectStatus(EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
							&& !g_bLight
#endif
							)
						{
							//DEBUG_ADD("add mask1");

							// INT goes up to 150, so...  
							int percent;
//							if( g_pPlayer->IsOusters() )
//								percent = 70;
//							else
								percent = min(75u, 25+pPlayerStatus->GetINT()*100/(min(2, PlayerState::GetRace()+1)*150));
							
							g_pChatManager->AddMask(str, percent);

							//DEBUG_ADD("ok");
						}
//						else if (g_pPlayer->IsVampire() && pCreature->IsSlayer() && !bTranslation)
//						{
//							//DEBUG_ADD("add mask2");
//
//							// INT goes up to 300, so...  
//							int percent = min(75, 25+g_pPlayer->GetINT()*100/300);
//							g_pChatManager->AddMask(str, percent);
//
//							//DEBUG_ADD("ok");
//						}
						// Under hallucination, chat is not shown properly either.
						else if (PlayerState::HasEffectStatus(EFFECTSTATUS_HALLUCINATION))
						{
							//DEBUG_ADD("add mask3");

							g_pChatManager->AddMask(str, 50);

							//DEBUG_ADD("ok");
						}
					#endif
				}
				
				//DEBUG_ADD("setChat");

				CreatureState::SetChatString(pCreature, str, pPacket->getColor() );

				//DEBUG_ADD("ok");

				// Add to the history.
				char strName[256];
				//sprintf(temp, "%s> %s", pCreature->GetName(), str);
				

				// Another race reads the hallucination name.
				if(!bMasterWords && !g_pUserInformation->IsMaster && PlayerState::GetRace() != CreatureState::GetRace(pCreature) && !bTranslation)
					snprintf(strName, sizeof(strName), "%s", CreatureState::GetHalluName(pCreature));
				else
					snprintf(strName, sizeof(strName), "%s", CreatureState::GetName(pCreature));

				// Only a player's words go into the chat window.
				if (isPlayerCharacter /*|| IsGildre*/) // Gilles de Rais's would go to zone chat, so it is blocked.
				{
					//DEBUG_ADD("UI add");
					CommunicationPresentation::AddChatToHistory( str, strName, 0, pPacket->getColor() );
					//DEBUG_ADD("UI add");
				}
			}
			//-------------------------------------------------
			// Not shown when chat from them is refused.
			//-------------------------------------------------
			else
			{
				//DEBUG_ADD("no see");
				CreatureState::SetChatString(pCreature, "......", pPacket->getColor() );
			}
		}
	}

	__END_CATCH
}
