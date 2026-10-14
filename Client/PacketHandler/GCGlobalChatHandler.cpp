//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGlobalChatHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGlobalChat.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MChatManager.h"
#include "UserInformation.h"
#include "MSkillManager.h"
#include "MZoneTable.h"
#include "MGameStringTable.h"
#include "MStatus.h"

//////////////////////////////////////////////////////////////////////
//
// Run when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCGlobalChatHandler::execute ( GCGlobalChat * pPacket , Player * pPlayer )

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
		char str[256];
		char strName[256];

		//---------------------------------------------------------------
		// Wansung --> Johab
		//---------------------------------------------------------------
		//UI_WansungToJohap( pPacket->getMessage().c_str(), str );
		snprintf(str, sizeof(str), "%s", pPacket->getMessage().c_str());

		//CommunicationPresentation::AddChatToHistory( str );
		char* pLB = strchr( str, ' ' );

		if (pLB!=NULL)
		{
			if (*(pLB+1)!='\0')
			{
				// ' ' becomes NULL
				*pLB = '\0';

				snprintf(strName, sizeof(strName), "%s", str);
 
//				bool bMasterWords = (strstr(strName, "GM")!=NULL);
				bool bMasterWords = strncmp( strName, (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetString(), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetLength() ) == 0 ;

				//--------------------------------------------------
				// Is it shown to me? (An operator's words always are.)
				//--------------------------------------------------
				if (bMasterWords 
					|| g_pChatManager->IsAcceptID( strName ))
				{
					//--------------------------------------------------
					// Remove curses.
					// Filtered unless an operator said it or I am one.
					// --> An operator's words are all shown, and an operator sees all.
					//--------------------------------------------------			

					if (!bMasterWords && 
						!g_pUserInformation->IsMaster 
						&& !PlayerState::HasEffectStatus(EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
				&& !g_bLight
#endif
					)
					{
						//--------------------------------------------------
						// Remove curses.
						//--------------------------------------------------
						g_pChatManager->RemoveCurse( pLB+1 );

						#ifndef _DEBUG
						
						ZONETABLE_INFO *pZoneInfo = g_pZoneTable->Get( ZoneState::GetID() );
						if( pZoneInfo->ChatMaskByRace == true && 
							!g_pSkillAvailable->IsEnableSkill(SKILL_HOLYLAND_BLOOD_BIBLE_HILLEL) &&
							!PlayerState::HasEffectStatus(EFFECTSTATUS_TRANSLATION ))
						{
							//--------------------------------------------------
							// Another race.
							//--------------------------------------------------
							Race race = (Race)pPacket->getRace();
							if (PlayerState::GetRace() != race)
							{
								// INT goes up to 150, so...  
								int percent = min(75, 25+pPlayerStatus->GetINT()*100/(min(2, PlayerState::GetRace()+1)*150));
//								if(g_pPlayer->GetRace() == RACE_OUSTERS || race == RACE_OUSTERS)
//									percent = 70;
								
								g_pChatManager->AddMask(str, percent);
							}
//							if (g_pPlayer->IsSlayer() && race != RACE_SLAYER)
//							{
//								// INT goes up to 150, so...  
//								int percent = min(75, 25+g_pPlayer->GetINT()*100/150);
//								if(race == RACE_OUSTERS)
//									percent = 70;
//								g_pChatManager->AddMask(pLB+1, percent);
//							}
//							else if (g_pPlayer->IsVampire() && race != RACE_VAMPIRE)
//							{
//								// INT goes up to 300, so...  
//								int percent = min(75, 25+g_pPlayer->GetINT()*100/300);
//								if(race == RACE_OUSTERS)
//									percent = 70;
//								g_pChatManager->AddMask(pLB+1, percent);
//							}
//							else if (g_pPlayer->IsOusters() && race != RACE_OUSTERS)
//							{
//								// INT goes up to 300, so...  
//								//int percent = min(75, 25+g_pPlayer->GetINT()*100/300);
//								int percent = 70;
//								g_pChatManager->AddMask(pLB+1, percent);
//							}
						}

						#endif
					}

					// ZONECHAT = 1
					CommunicationPresentation::AddChatToHistory( pLB+1, strName, 1, pPacket->getColor() );

					// [Help] when shouting
//					__BEGIN_HELP_EVENT
////						ExecuteHelpEvent( HE_CHAT_SHOUTED );	
//					__END_HELP_EVENT
				}
			}
		}
	}


	__END_CATCH
}
