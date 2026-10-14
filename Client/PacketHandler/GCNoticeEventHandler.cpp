//--------------------------------------------------------------------------------
//
// Filename    : GCNoticeEventHandler.cpp
// Written By  : elca, Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCNoticeEvent.h"
#include "ClientMode.h"
#include "CMessageArray.h"
#include "GameMessages.h"
#include "MEventQueue.h"
#include "MGameStringTable.h"
#include "MPriceManager.h"
#include "MWarManager.h"
#include "MZoneTable.h"
#include "NoticePresentationHost.h"
#include "PlayerStateHost.h"
#include "SafeFormat.h"
#include "UserInformation.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCNoticeEventHandler::execute ( GCNoticeEvent * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
	__BEGIN_DEBUG

//	char szBuffer[256];

//	wsprintf(szBuffer,"%d : 0x%x",pPacket->getCode(), pPacket->getParameter() );
//	_MinTrace( szBuffer );
		

	MEvent event;

	if(NULL == pPacket) return;

	
	switch(pPacket->getCode())
	{
	case NOTICE_EVENT_SHOP_TAX_CHANGE :
		event.eventID = EVENTID_TAX_CHANGE;
		event.eventType = EVENTTYPE_ZONE;
		event.eventDelay = -1;
		event.parameter1 = pPacket->getParameter();
		WorldEvents::AddEvent(event);
		break;
		
	case NOTICE_EVENT_MASTER_COMBAT_TIME:
		event.eventID = EVENTID_COMBAT_MASTER;
		event.eventType = EVENTTYPE_ZONE;
		event.eventFlag = EVENTFLAG_SHOW_DELAY;
		event.eventDelay = pPacket->getParameter()*1000;
		WorldEvents::AddEvent(event);
		break;
		
	case NOTICE_EVENT_MASTER_COMBAT_END:
		WorldEvents::RemoveEvent(EVENTID_COMBAT_MASTER);
		break;
		
	case NOTICE_EVENT_KICK_OUT_FROM_ZONE:
		event.eventID = EVENTID_KICK_OUT_FROM_ZONE;
		event.eventType = EVENTTYPE_ZONE;
		event.eventFlag = EVENTFLAG_SHOW_DELAY | EVENTFLAG_SHAKE_SCREEN | EVENTFLAG_SHOW_DELAY_STRING;
		event.eventDelay = pPacket->getParameter()*1000;
		event.parameter3 = 5;
		event.m_StringsID.push_back(STRING_MESSAGE_DISMISS_AFTER_SECOND);
		WorldEvents::AddEvent(event);
		WorldEvents::RemoveEvent(EVENTID_COMBAT_MASTER);
		break;
		
	case NOTICE_EVENT_CONTINUAL_GROUND_ATTACK:
		event.eventID = EVENTID_CONTINUAL_GROUND_ATTACK;
		event.eventType = EVENTTYPE_ZONE;
		event.eventFlag = EVENTFLAG_FADE_SCREEN | EVENTFLAG_SHAKE_SCREEN;
		event.parameter2 = 30 << 16;
		event.parameter3 = 5;
		event.eventDelay = pPacket->getParameter()*1000;
		WorldEvents::AddEvent(event);
		break;
		
	case NOTICE_EVENT_CONTINUAL_GROUND_ATTACK_END:
		WorldEvents::RemoveEvent(EVENTID_CONTINUAL_GROUND_ATTACK);
		break;
		
	case NOTICE_EVENT_PREMIUM_HALF_START:			// premium users get potions/serums at half price
		event.eventID = EVENTID_PREMIUM_HALF;
		event.eventType = EVENTTYPE_ZONE;
		event.eventDelay = -1;
		WorldEvents::AddEvent(event);
		if(PlayerState::GetRace()==RACE_SLAYER)
			g_pSystemMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER].GetString() );
		else
			g_pSystemMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE].GetString() );
		//			g_pPlayer->AddEffectStatus(EFFECTSTATUS_SHARP_SHIELD_1, 0xffff);
		break;
		
	case NOTICE_EVENT_PREMIUM_HALF_END:				// the event ends
		WorldEvents::RemoveEvent(EVENTID_PREMIUM_HALF);
		if(PlayerState::GetRace()==RACE_SLAYER)
			g_pSystemMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER_END].GetString() );
		else
			g_pSystemMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE_END].GetString() );
		//			g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_SHARP_SHIELD_1);
		break;

	case NOTICE_EVENT_WAR_OVER :					// the war ends
		if( g_pWarManager->IsExist( ZoneState::GetID()) )
		{
			// while the player's zone is at war, read its safety again (upstream's
			// comment said 'off the war list', the opposite of the check)
			g_bZoneSafe = g_pZoneTable->Get(ZoneState::GetID())->Safety;
		}
		g_pWarManager->RemoveWar(pPacket->getParameter());
		break;

	case NOTICE_EVENT_RACE_WAR_OVER:
		if(g_pWarManager != NULL)
			g_pWarManager->ClearRaceWar();
//		UI_SetBloodBibleStatusTimer(30);
//		UI_CloseBloodBibleStatus();
		NoticePresentation::DeleteNotice(6);
		break;

	case NOTICE_EVENT_WELCOME_MESSAGE :
		NoticePresentation::RunWelcomeMessage();
		break;
		
	case NOTICE_EVENT_MEET_GRANDMA :					// talking to the grandmother
		break;

	case NOTICE_EVENT_MEET_FISHSHOP_MASTER :			// talking to the fishmonger
		break;			

	case NOTICE_EVENT_START_QUEST_ENDING :			// the quest's ending starts
		{
			NoticePresentation::StartQuestEnding();
		}
		break;

	case NOTICE_EVENT_RESULT_LOTTERY :
		NoticePresentation::LotteryResult(  pPacket->getParameter() > 0 );
		break;
		
	case NOTICE_EVENT_RUN_HORN:
		NoticePresentation::RunHorn(ZoneState::GetID());
		break;

	case NOTICE_EVENT_MASTER_LAIR_OPEN :
	case NOTICE_EVENT_MASTER_LAIR_CLOSED :
	case NOTICE_EVENT_MASTER_LAIR_COUNT :
		// the time in the high word, the zone ID in the low
		NoticePresentation::MasterLairMessage(BYTE(pPacket->getCode() - NOTICE_EVENT_MASTER_LAIR_OPEN), HIWORD(pPacket->getParameter()), LOWORD( pPacket->getParameter() ) );
		break;
	
	case NOTICE_EVENT_CONTRACT_GNOMES_HORN :          // you must go to Sioram and sign a contract to use it.
		NoticePresentation::PopupMessage( STRING_MESSAGE_CONTRACT_GNOMES_HORN );
		break;
    
	case NOTICE_EVENT_NOT_ENOUGH_MONEY :              // not enough money.
		NoticePresentation::PopupMessage( STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY );
		break;		
	
	case NOTICE_EVENT_MINI_GAME:
		NoticePresentation::RunMinigame(pPacket->getParameter());
		break;
	
	case NOTICE_EVENT_GET_RIFINIUM :					// you got rifinium.
		NoticePresentation::PopupMessage(STRING_MESSAGE_GET_RIFINIUM);
		break;
	
	case NOTICE_EVENT_INVENTORY_FULL :				// not enough room in the inventory.
		NoticePresentation::PopupMessage(UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST);
		break;
		
	case NOTICE_EVENT_FLAG_WAR_READY :                // Capture the Flag! starts in 5 minutes.
		g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_FLAG_WAR_READY].GetString() );
		NoticePresentation::RunNotice(2, pPacket->getParameter());
		break;

    case NOTICE_EVENT_FLAG_WAR_START :                // Capture the Flag! has started.
		g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_FLAG_WAR_START].GetString() );		
		break;

	case NOTICE_EVENT_FLAG_WAR_FINISH:               // Capture the Flag! ended with %s winning. Items burst out in 3 minutes.
		{
			int raceString[3] = {STRING_MESSAGE_SLAYER, STRING_MESSAGE_VAMPIRE, STRING_MESSAGE_OUSTERS};
			g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_FLAG_WAR_FINISH].GetString() );
			g_pNoticeMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_EVENT_FLAG_WAR_WINNER), (*g_pGameStringTable)[raceString[min(2,max(0,HIWORD(pPacket->getParameter())))]].GetString() ,  LOWORD(pPacket->getParameter()));
			g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_FLAG_WILL_POUR_ITEM_AFTER_3MIN].GetString() );
			
			MEvent event;
			event.eventID = EVENTID_POUR_ITEM;
			event.eventDelay = 180000;
			event.eventFlag = EVENTFLAG_SHOW_DELAY_STRING;
			event.eventType = EVENTTYPE_ZONE;
			event.m_StringsID.push_back(STRING_MESSAGE_POUR_ITEM_AFTER_SECOND);

			WorldEvents::AddEvent(event);
		}
		NoticePresentation::CloseCTFStatusWindow();
		NoticePresentation::DeleteNotice(2);
		break;

	case NOTICE_EVENT_FLAG_POURED_ITEM :
		g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_FLAG_POURED_ITEM].GetString() );
		break;

	case NOTICE_EVENT_ENTER_BEGINNER_ZONE :
		NoticePresentation::RunNotice(1, pPacket->getParameter());
		break;

	case NOTICE_EVENT_LOGIN_JUST_NOW:
		if( g_pNoticeMessage != NULL && g_pGameStringTable != NULL && g_LeftPremiumDays != 0xFFFE && g_LeftPremiumDays >= 0 && g_LeftPremiumDays <=5 )
		{
			g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_1].GetString() );
			g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_2].GetString() );
			g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_3].GetString() );
		}
		// set the head price's multiplier
		g_pUserInformation->HeadPrice = (int)pPacket->getParameter();
		NoticePresentation::SendCrashReport();
		break;

	case NOTICE_EVENT_LEVEL_WAR_ARRANGED :            // the level war starts soon.
//		UI_RunLevelWarArranged( pPacket->getParameter() );
		NoticePresentation::RunNotice(3, pPacket->getParameter());
		break;

	case NOTICE_EVENT_LEVEL_WAR_STARTED :             // the level war has started.
//		UI_RunLevelWarStarted( pPacket->getParameter() );
		NoticePresentation::RunNotice(4, pPacket->getParameter());
		NoticePresentation::DeleteNotice(3);
		break;
		
	case NOTICE_EVENT_RACE_WAR_SOON:
		NoticePresentation::RunNotice(5, pPacket->getParameter());
		break;
		
	case NOTICE_EVENT_LEVEL_WAR_OVER:
		g_pWarManager->RemoveWar(ZoneState::GetID());
		break;
		
	case NOTICE_EVENT_NETMARBLE_CARD_FULL:
		g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_NETMARBLE_1].GetString() );
		g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_NETMARBLE_2].GetString() );
		g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_NETMARBLE_3].GetString() );
		g_pNoticeMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_EVENT_NETMARBLE_4].GetString() );
		break;
		
	case NOTICE_EVENT_HOLYDAY:
		NoticePresentation::SetFlagTo( pPacket->getParameter() != 0 );
		break;
		
	case NOTICE_EVENT_CAN_PET_QUEST :
		NoticePresentation::RunNotice(7, 0);
		break;
	case  NOTICE_EVENT_SEND_SMS:
		NoticePresentation::RunNotice(8, 0);
		break;
	case NOTICE_EVENT_GDR_LAIR_ENDING_1: // Gilles de Rais's first ending - the screen shakes for 5 seconds, then darkens for 10
		event.eventID = EVENTID_GDR_PRESENT;
		event.eventType = EVENTTYPE_ZONE;
		event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
		event.parameter3 = 2;
		event.eventDelay =5000; // 5 seconds
		WorldEvents::AddEvent(event);
		//SetFadeStart(31, -1, 1, 0,0,0, 4);
		break;
	case NOTICE_EVENT_GOLD_MEDALS:
		{
			char szBuf[128];
			SafeFormat::Format(szBuf, GetGameString(UI_STRING_NOTICE_EVENT_GOLD_MEDALS), pPacket->getParameter());
			g_pSystemMessage->Add( szBuf );
		}
		break;
	
	case NOTICE_EVENT_CROWN_PRICE:
		{
			int Value = pPacket->getParameter();
			g_pPriceManager->SetEventItemPrice( Value );
			char szBuf[128];
			SafeFormat::Format(szBuf, GetGameString(UI_STRING_CHANGE_EVENTITEM_PRICE), Value);
			g_pSystemMessage->Add( szBuf );
		}
		break;
	case NOTICE_EVENT_GIVE_PRESENT_1:                // the level 1 present box was given.
			//g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_1].GetString());
			NoticePresentation::PopupText((*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_1].GetString(), true);
			//NoticePresentation::PopupMessage(STRING_MESSAGE_DAUM_EVENT_1);
		break;
    case NOTICE_EVENT_GIVE_PRESENT_2:                // the level 2 present box was given.
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), 5,(*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_BOX2].GetString());
			//g_pUIDialog->PopupFreeMessageDlg(szBuf);
			NoticePresentation::PopupText(szBuf, true);
		}
		break;
    case NOTICE_EVENT_GIVE_PRESENT_3:                // the level 3 present box was given.
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), 10,(*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_BOX3].GetString());
			NoticePresentation::PopupText(szBuf, true);
		}
		break;
    case NOTICE_EVENT_GIVE_PRESENT_4:                // the level 4 present box was given.
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), 15,(*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_BOX4].GetString());
			NoticePresentation::PopupText(szBuf, true);
		}
		break;
    case NOTICE_EVENT_GIVE_PRESENT_5:                // the level 5 present box was given.
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), 20,(*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_BOX5].GetString());
			NoticePresentation::PopupText(szBuf, true);
		}
		break;
    case NOTICE_EVENT_GIVE_PRESENT_6:                // the level 6 present box was given
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), 25,(*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_BOX6].GetString());
			NoticePresentation::PopupText(szBuf, true);
		}
		break;
    case NOTICE_EVENT_GIVE_PRESENT_7:                // the level 7 present box was given.
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), 30,(*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_BOX7].GetString());
			NoticePresentation::PopupText(szBuf, true);
		}
		break;
    case NOTICE_EVENT_GIVE_PRESENT_8:                // the level 8 present box was given.
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), 35,(*g_pGameStringTable)[STRING_MESSAGE_DAUM_EVENT_BOX8].GetString());
			NoticePresentation::PopupText(szBuf, true);
		}
		break;
    case NOTICE_EVENT_CAN_OPEN_PRESENT_8:            // the level 8 present box can be opened.
		{
			char szBuf[1024];
			SafeFormat::Format(szBuf, GetGameString(STRING_MESSAGE_DAUM_EVENT_3), 40);
			NoticePresentation::PopupText(szBuf, false);
		}
		break;
		
	case NOTICE_EVENT_RACE_WAR_IN_5:
		{
			MEvent event;
			event.eventID = EVENTID_WAR_EFFECT;
			event.eventType = EVENTTYPE_NULL;
			event.eventFlag = EVENTFLAG_SHAKE_SCREEN | EVENTFLAG_FADE_SCREEN;
			event.parameter2 = RGB( -30, -30, 0 );
			event.parameter3 = 1;
			event.eventDelay = -1;
			event.showTime = 2000;
			event.totalTime = 30000;
			WorldEvents::AddEvent(event);
		}
		break;
	case NOTICE_EVENT_RACE_WAR_STARTED_IN_OTHER_SERVER:
		NoticePresentation::PopupMessage( STRING_MESSAGE_RACE_WAR_STARTED_IN_OTHER_SERVER );
		break;
	case NOTICE_EVENT_CANNOT_FIND_STORE:             // the seller cannot be found.
		NoticePresentation::PopupMessage( UI_STRING_MESSAGE_CANNOT_FIND_STORE );
		break;
    case NOTICE_EVENT_STORE_CLOSED:                  // the shop is closed already.
		NoticePresentation::PopupMessage( UI_STRING_MESSAGE_STORE_CLOSED );
		break;
    case NOTICE_EVENT_ITEM_NOT_FOUND:                // the item is sold or withdrawn by the seller already.
		NoticePresentation::PopupMessage( UI_STRING_MESSAGE_ITEM_NOT_FOUND );
		break;
    case NOTICE_EVENT_TOO_MUCH_MONEY:                // the seller has too much money to buy it.
		NoticePresentation::PopupMessage( UI_STRING_MESSAGE_TOO_MUCH_MONEY );
		break;
	case NOTICE_EVENT_NO_INVENTORY_SPACE:           // no free space in the inventory.
		NoticePresentation::PopupMessage( UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST );
		break;
    case NOTICE_EVENT_ALREADY_DISPLAYED:             // the item is on display already.
		NoticePresentation::PopupMessage( UI_STRING_MESSAGE_ALREADY_DISPLAYED );
		break;
    case NOTICE_EVENT_CANNOT_SELL:                   // the item cannot be sold.
		NoticePresentation::PopupMessage( STRING_MESSAGE_CANNOT_SELL );
		break;

	}
	
	__END_DEBUG
		__END_CATCH
}
