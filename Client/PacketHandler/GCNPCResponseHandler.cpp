//////////////////////////////////////////////////////////////////////
//
// Filename    : GCNPCResponseHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files

#include "Client_PCH.h"
#include "Gpackets/GCNPCResponse.h"
#include "BonusSkillHost.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "NPCPresentationHost.h"
#include "PlayerStateHost.h"
#include "RaceType.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "MStorage.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MMoneyManager.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "TempInformation.h"
#include "MSkillManager.h"
#include "SoundDef.h"
#include "MItemFinder.h"
#include "UserInformation.h"
#include "MSkillManager.h"

	#include "ClientPlayer.h"
	#include "Cpackets/CGStashList.h"


void GCNPCResponseHandler::execute ( GCNPCResponse * pPacket , Player * pPlayer )
	 


{
	__BEGIN_TRY


	char sz_temp[512];
	//------------------------------------------------------
	// verified
	//------------------------------------------------------
	if ( PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NPC_ASK )
	{
		PlayerState::SetWaitVerifyNULL();

		DEBUG_ADD("[Verified] NPC Ask Answer OK");
	}

	int value = pPacket->getParameter();

	switch (pPacket->getCode())
	{		
		//------------------------------------------------------
		//
		//				Quit Dialog
		//
		//------------------------------------------------------
		case NPC_RESPONSE_QUIT_DIALOGUE :
			//if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_NPC_ASK)
			{
				DialogPresentation::UnSetLockInputPCTalk();

				PlayerState::SetWaitVerifyNULL();

				// meaning: not talking any more.
				DialogPresentation::ClosePCTalkDlg();
			}
		break;

		//------------------------------------------------------
		//
		//					open the repair
		//
		//------------------------------------------------------			
		case NPC_RESPONSE_INTERFACE_REPAIR :
			NPCPresentation::OpenInventoryToRepair();
		break;


		//------------------------------------------------------
		//
		//					open the repair
		//
		//------------------------------------------------------			
		case NPC_RESPONSE_INTERFACE_HELICOPTER :
			NPCPresentation::RunSelectWayPoint();
		break;		

		//------------------------------------------------------			
		//
		//					open the stash
		//
		//------------------------------------------------------			
		case NPC_RESPONSE_INTERFACE_STASHOPEN :
			//----------------------------------------------------------------------
			// without the stash's contents, ask for its list.
			//----------------------------------------------------------------------
			if (g_pStorage==NULL)
			{	
				ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

				CGStashList	_CGStashList;

				_CGStashList.setObjectID( PlayerState::GetID() );
				
				pClientPlayer->sendPacket( &_CGStashList );
			}
			//----------------------------------------------------------------------
			// with the contents already, open it at once.
			//----------------------------------------------------------------------
			else
			{
				DialogPresentation::RunStorage();
				DialogPresentation::SetStorage(g_pStorage);
			}
		break;
	
		
		//--------------------------------------------------------------
		//
		//  Repair OK
		//
		//--------------------------------------------------------------
		case NPC_RESPONSE_REPAIR_OK :
			//--------------------------------------------------------------
			// is the item-selling packet the right one for repairs?
			//--------------------------------------------------------------
			if (g_pTempInformation->GetMode() == TempInformation::MODE_SHOP_REPAIR)
			{
				//MItem* pCheckItem = (MItem*)g_pTempInformation->pValue;		
				
				MItem* pModifyItem = (MItem*)g_pTempInformation->pValue;		//NULL;

				BOOL	bInInventory	= static_cast<BOOL>(g_pTempInformation->Value1);
				BOOL	bInGear			= static_cast<BOOL>(g_pTempInformation->Value2);
				
				//--------------------------------------------------------------
				// repair an inventory item
				//--------------------------------------------------------------		

				//--------------------------------------------------------------
				// repairing everything
				//--------------------------------------------------------------
				if (pModifyItem==NULL && !bInInventory && bInGear)
				{
					MPlayerGear* pGear = static_cast<MPlayerGear*>(PlayerState::GetGear());	// the player's gear is one

					// repair everything
					pGear->SetBegin();

					MItem* pLastItem = NULL;

					while (pGear->IsNotEnd())
					{
						MItem* pItem = pGear->Get();

						if (pItem!=NULL)
						{
							pItem->SetCurrentDurability( pItem->GetMaxDurability() );

							pLastItem = pItem;
						}

						pGear->Next();
					}

					// check the status
					pGear->CheckItemStatusAll();

					//--------------------------------------------------------------
					// sound the repair
					//--------------------------------------------------------------
					if (pLastItem!=NULL)
					{	
						WorldEvents::PlaySound( pLastItem->GetInventorySoundID() );
					}


					//--------------------------------------------------------------
					// change the money.
					//--------------------------------------------------------------
					if (!g_pMoneyManager->SetMoney( value ))
					{
						DEBUG_ADD_FORMAT_ERR("[Error] Can't Set Money=%d, Price=%d", g_pMoneyManager->GetMoney(), value);
					}

					DEBUG_ADD("RepairAll OK");
				}
				//--------------------------------------------------------------
				// repairing one
				//--------------------------------------------------------------
				else if (//pCheckItem!=NULL && 
						pModifyItem!=NULL)
				{
					// verify the ID.
					if (1)//pCheckItem->GetID()==itemID && pModifyItem->GetID()==itemID)
					{
						//--------------------------------------------------------------
						// durability to the max..
						//--------------------------------------------------------------
						if (pModifyItem->IsChargeItem())
						{
							pModifyItem->SetNumber( pModifyItem->GetMaxNumber() );

							if (g_pSkillAvailable!=NULL)
							{
								BonusSkills::RefreshAvailableSkills();
							}	
						}
						else
						{
							pModifyItem->SetCurrentDurability( pModifyItem->GetMaxDurability() );
						}

						//--------------------------------------------------------------
						// in the gear
						//--------------------------------------------------------------
						if (bInGear)
						{
							MPlayerGear* pGear = static_cast<MPlayerGear*>(PlayerState::GetGear());	// the player's gear is one
							
							pGear->CheckItemStatusAll();				
						}

						//--------------------------------------------------------------
						// sound the repair
						//--------------------------------------------------------------
						WorldEvents::PlaySound( pModifyItem->GetInventorySoundID() );

						//--------------------------------------------------------------
						// change the money.
						//--------------------------------------------------------------
						if (!g_pMoneyManager->SetMoney( value ))
						{
							DEBUG_ADD_FORMAT_ERR("[Error] Can't Set Money=%d, Price=%d", g_pMoneyManager->GetMoney(), value);
						}
					}
					//else
					//{
					//	DEBUG_ADD_FORMAT("[Error] Different ID. Packet(%d)!=ClientTemp(%d)", itemID, pModifyItem->GetID());
					//}
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Item is NULL");
				}

				// clear the mode.
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				// let trading go on again.
				DialogPresentation::UnlockItemTrade();
			}
			else
			{
				DEBUG_ADD_ERR("[Error] Repair? -_-; No Temp Information!");
			}
		break;

		//------------------------------------------------------			
		//
		//					cannot repair
		//
		//------------------------------------------------------			
		case NPC_RESPONSE_REPAIR_FAIL_ITEM_NOT_EXIST :
		case NPC_RESPONSE_REPAIR_FAIL_ITEM_TYPE :
		case NPC_RESPONSE_REPAIR_FAIL_MONEY :
			// clear the mode.
			g_pTempInformation->SetMode(TempInformation::MODE_NULL);

			// let trading go on again.
			DialogPresentation::UnlockItemTrade();

			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_REPAIR].GetString() );

		break;

		//------------------------------------------------------			
		//
		//					buying a stash
		//
		//------------------------------------------------------			
		case NPC_RESPONSE_STASH_SELL_OK :
			if (g_pTempInformation->GetMode() == TempInformation::MODE_STORAGE_BUY_WAIT)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				// let the player choose again.
				DialogPresentation::ShowPCTalkDlg();

				// drop the stash there is, if any.
				// then.. the next time the stash is chosen.. it is made with one more.
				if (g_pStorage!=NULL)
				{
					delete g_pStorage;
					g_pStorage = NULL;
				}

				// take the stash's price.
				g_pMoneyManager->UseMoney( static_cast<int>(g_pTempInformation->Value1) );

				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_STORAGE_BUY].GetString());
			}
		break;

		//------------------------------------------------------			
		//
		//					buying a stash failed
		//
		//------------------------------------------------------			
		case NPC_RESPONSE_STASH_SELL_FAIL_MAX :
		case NPC_RESPONSE_STASH_SELL_FAIL_MONEY :
			if (g_pTempInformation->GetMode() == TempInformation::MODE_STORAGE_BUY_WAIT)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				// let the player choose again.
				DialogPresentation::ShowPCTalkDlg();

				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_MORE].GetString());
			}
		break;

		//------------------------------------------------------
		//
		//					silver coating succeeded
		//
		//------------------------------------------------------
		case NPC_RESPONSE_SILVER_COATING_OK :
			//--------------------------------------------------------------
			// is it silvering?
			//--------------------------------------------------------------
			if (g_pTempInformation->GetMode() == TempInformation::MODE_SHOP_SILVERING)
			{
				//MItem* pCheckItem = (MItem*)g_pTempInformation->pValue;		
				
				MItem* pModifyItem = (MItem*)g_pTempInformation->pValue;		//NULL;

				if (//pCheckItem!=NULL && 
					pModifyItem!=NULL)
				{
					// verify the ID.
					if (1)//pCheckItem->GetID()==itemID && pModifyItem->GetID()==itemID)
					{
						//--------------------------------------------------------------
						// durability to the max..
						//--------------------------------------------------------------
						pModifyItem->SetSilver( pModifyItem->GetSilverMax() );
						const MItem* pWeapon = g_pSlayerGear->GetItem( MSlayerGear::GEAR_SLAYER_RIGHTHAND );
						if(pModifyItem->IsAffectStatus() && pModifyItem == pWeapon)
						{
							NPCPresentation::SetCharInfoSilverDamage(pWeapon->GetMinDamage() / 10, pWeapon->GetMaxDamage() / 10);
						}

						//--------------------------------------------------------------
						// sound the repair
						//--------------------------------------------------------------
						WorldEvents::PlaySound( pModifyItem->GetInventorySoundID() );

						//--------------------------------------------------------------
						// change the money.
						//--------------------------------------------------------------
						if (!g_pMoneyManager->SetMoney( value ))
						{
							DEBUG_ADD_FORMAT_ERR("[Error] Can't Set Money=%d, Price=%d", g_pMoneyManager->GetMoney(), value);
						}
					}
					//else
					//{
					//	DEBUG_ADD_FORMAT("[Error] Different ID. Packet(%d)!=ClientTemp(%d)", itemID, pModifyItem->GetID());
					//}
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Item is NULL");
				}

				// clear the mode.
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				// let trading go on again.
				DialogPresentation::UnlockItemTrade();
			}
			else
			{
				DEBUG_ADD_ERR("[Error] Silvering? -_-; No Temp Information!");
			}
		break;

		//------------------------------------------------------
		//
		//					silver coating failed
		//
		//------------------------------------------------------
		case NPC_RESPONSE_SILVER_COATING_FAIL_ITEM_NOT_EXIST :
		case NPC_RESPONSE_SILVER_COATING_FAIL_ITEM_TYPE :
		case NPC_RESPONSE_SILVER_COATING_FAIL_MONEY :
			// clear the mode.
			g_pTempInformation->SetMode(TempInformation::MODE_NULL);

			// let trading go on again.
			DialogPresentation::UnlockItemTrade();

			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_SILVERING].GetString() );
		break;

		
		//------------------------------------------------------
		//
		//				open the silver coating UI
		//
		//------------------------------------------------------
		case NPC_RESPONSE_INTERFACE_SILVER_COATING :
			NPCPresentation::OpenInventoryToSilvering();
		break;

		//------------------------------------------------------
		//
		//				creating a guild
		//
		//------------------------------------------------------
		case NPC_RESPONSE_INTERFACE_CREATE_GUILD :
		break;

		//------------------------------------------------------
		//
		//				deleting a guild
		//
		//------------------------------------------------------
		case NPC_RESPONSE_INTERFACE_DESTROY_GUILD :
		break;
	
		//------------------------------------------------------
		//
		//				Newbie Item
		//
		//-----------------------------------------------------
		case NPC_RESPONSE_INTERFACE_NEWBIE_ITEM :
			NPCPresentation::RunTutorialExit();
		break;

		//------------------------------------------------------
		//
		//				Tutorial Computer
		//
		//------------------------------------------------------
		case NPC_RESPONSE_INTERFACE_TUTORIAL_COMPUTER :
			NPCPresentation::RunComputer();
		break;

		//------------------------------------------------------
		//
		//				Tutorial Briefing
		//
		//------------------------------------------------------
		case NPC_RESPONSE_INTERFACE_TUTORIAL_BRIEFING :
			NPCPresentation::RunBriefing();
		break;

		//------------------------------------------------------
		//
		//				Tutorial Bookcase
		//
		//------------------------------------------------------
		case NPC_RESPONSE_INTERFACE_TUTORIAL_BOOKCASE :
			NPCPresentation::RunBookcase();
		break;

		//------------------------------------------------------
		//
		//				when healed
		//
		//------------------------------------------------------
		case NPC_RESPONSE_HEAL :
			if( PlayerState::GetRace()==RACE_SLAYER )
				WorldEvents::PlaySound( SOUND_ITEM_USE_HPOTION );
			else if ((PlayerState::Creature()!=NULL && CreatureState::IsOusters( PlayerState::Creature() )) )
				WorldEvents::PlaySound( SOUND_OUSTERS_PUPA );
		break;

		//------------------------------------------------------
		//
		//				when donating
		//
		//------------------------------------------------------
		case NPC_RESPONSE_DONATION_OK :
			//--------------------------------------------------------------
			// change the money.
			//--------------------------------------------------------------
			if (!g_pMoneyManager->SetMoney( value ))
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Can't Set Money=%d, Price=%d", g_pMoneyManager->GetMoney(), value);
			}

			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_DONATION_OK].GetString() );
		break;

		//------------------------------------------------------
		//
		//				donating failed
		//
		//------------------------------------------------------
		case NPC_RESPONSE_DONATION_FAIL_MONEY :
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_DONATION_FAIL].GetString() );
		break;

		/////////////////////////////////////////////////////////
		// buying with soccer balls
		/////////////////////////////////////////////////////////
		case NPC_RESPONSE_DECREASE_BALL:
		// take as many as needed from the inventory.
		{
			int remainNum = pPacket->getParameter();
				
			// see how many there are.
			MItemClassTypeFinder ballFinder(ITEM_CLASS_EVENT_STAR, 6);	// the ball is type 6 :)
				
			// remove that many from the inventory.
			while (remainNum > 0)
			{
				MItem* pBallItem = g_pInventory->FindItemGridOrder( ballFinder );
				
				// no stars - -;
				if (pBallItem==NULL)
				{
					DEBUG_ADD_ERR("[Error] Not Enough Ball -_-");
					break;
				}
				
				int itemNum = pBallItem->GetNumber();
				
				if (itemNum > remainNum)
				{
					pBallItem->SetNumber( itemNum - remainNum );
					remainNum = 0;
				}
				else
				{
					// the same or fewer
					remainNum -= itemNum;
					
					MItem* pRemovedItem = g_pInventory->RemoveItem( 
						pBallItem->GetGridX(), 
						pBallItem->GetGridY() );
					
					if (pRemovedItem!=NULL)
					{
						delete pRemovedItem;
					}
					else
					{
						DEBUG_ADD_FORMAT_ERR("[Error] Can't remove ball(%d, %d)", pBallItem->GetGridX(), pBallItem->GetGridY());
					}
				}
			}
		}
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_ALREADY_JOIN:
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_TEAM_REGIST_FAIL_ALREADY_JOIN), g_pUserInformation->GuildName.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_QUIT_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_REGIST_FAIL_QUIT_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_CANCEL_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_REGIST_FAIL_CANCEL_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_LEVEL:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_REGIST_FAIL_LEVEL].GetString() );
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_MONEY:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_REGIST_FAIL_MONEY].GetString() );
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_FAME:
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_TEAM_REGIST_FAIL_FAME), g_pUserInformation->CharacterID.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_NAME:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_REGIST_FAIL_NAME].GetString() );
		break;

		case NPC_RESPONSE_TEAM_REGIST_FAIL_DENY:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_REGIST_FAIL_DENY].GetString() );
		break;


		case NPC_RESPONSE_TEAM_STARTING_FAIL_ALREADY_JOIN:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_STARTING_FAIL_ALREADY_JOIN].GetString() );
		break;

		case NPC_RESPONSE_TEAM_STARTING_FAIL_QUIT_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_STARTING_FAIL_QUIT_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_TEAM_STARTING_FAIL_CANCEL_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_STARTING_FAIL_CANCEL_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_TEAM_STARTING_FAIL_LEVEL:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_STARTING_FAIL_LEVEL].GetString() );
		break;

		case NPC_RESPONSE_TEAM_STARTING_FAIL_MONEY:
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_TEAM_STARTING_FAIL_MONEY), g_pUserInformation->CharacterID.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
		break;

		case NPC_RESPONSE_TEAM_STARTING_FAIL_FAME:
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_TEAM_STARTING_FAIL_FAME), g_pUserInformation->CharacterID.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
		break;

		case NPC_RESPONSE_TEAM_STARTING_FAIL_DENY:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TEAM_STARTING_FAIL_DENY].GetString() );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_ALREADY_JOIN:
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_CLAN_REGIST_FAIL_ALREADY_JOIN), g_pUserInformation->GuildName.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_QUIT_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_REGIST_FAIL_QUIT_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_CANCEL_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_REGIST_FAIL_CANCEL_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_LEVEL:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_REGIST_FAIL_LEVEL].GetString() );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_MONEY:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_REGIST_FAIL_MONEY].GetString() );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_FAME:
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_CLAN_REGIST_FAIL_FAME), g_pUserInformation->CharacterID.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_NAME:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_REGIST_FAIL_NAME].GetString() );
		break;

		case NPC_RESPONSE_CLAN_REGIST_FAIL_DENY:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_REGIST_FAIL_DENY].GetString() );
		break;


		case NPC_RESPONSE_CLAN_STARTING_FAIL_ALREADY_JOIN:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_STARTING_FAIL_ALREADY_JOIN].GetString() );
		break;

		case NPC_RESPONSE_CLAN_STARTING_FAIL_QUIT_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_STARTING_FAIL_QUIT_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_CLAN_STARTING_FAIL_CANCEL_TIMEOUT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_STARTING_FAIL_CANCEL_TIMEOUT].GetString() );
		break;

		case NPC_RESPONSE_CLAN_STARTING_FAIL_LEVEL:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_STARTING_FAIL_LEVEL].GetString() );
		break;

		case NPC_RESPONSE_CLAN_STARTING_FAIL_MONEY:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_STARTING_FAIL_MONEY].GetString() );
		break;

		case NPC_RESPONSE_CLAN_STARTING_FAIL_FAME:
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_CLAN_STARTING_FAIL_FAME), g_pUserInformation->CharacterID.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
		break;

		case NPC_RESPONSE_CLAN_STARTING_FAIL_DENY:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLAN_STARTING_FAIL_DENY].GetString() );
		break;


		case NPC_RESPONSE_GUILD_SHOW_REGIST:
			NPCPresentation::ShowGuildRegist(pPacket->getParameter());
		break;
	
		case NPC_RESPONSE_GUILD_SHOW_STARTING_JOIN:
//			UI_ShowGuildStartJoin(pPacket->getParameter());
		break;

		case NPC_RESPONSE_GUILD_SHOW_JOIN:
//			UI_ShowGuildJoin(pPacket->getParameter());
		break;

		case NPC_RESPONSE_GUILD_SHOW_QUIT:
//			UI_ShowGuildQuit();
		break;

		case NPC_RESPONSE_GUILD_ERROR:
		break;

		case NPC_RESPONSE_TRADE_GIFT_BOX_OK:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_GIFT_BOX_OK].GetString() );
		break;

		case NPC_RESPONSE_TRADE_GIFT_BOX_NO_ITEM:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_GIFT_BOX_NO_ITEM].GetString() );
		break;

		case NPC_RESPONSE_TRADE_GIFT_BOX_ALREADY_TRADE:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_GIFT_BOX_ALREADY_TRADE].GetString() );
		break;

		case NPC_RESPONSE_TRADE_GIFT_BOX_ERROR:
		break;

		case NPC_RESPONSE_REWARD_OK:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_REWARD_OK].GetString() );
		break;

		case NPC_RESPONSE_REWARD_FAIL:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_REWARD_FAIL].GetString() );
		break;

		case NPC_RESPONSE_NO_EMPTY_SLOT:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NO_EMPTY_SLOT].GetString() );
		break;

		case NPC_RESPONSE_SHOW_TAX_BALANCE :
			NPCPresentation::OpenBringFee(pPacket->getParameter());
			break;

		case NPC_RESPONSE_WITHDRAW_TAX_OK :
			NPCPresentation::SetTotalFee(pPacket->getParameter());
			if(g_pTempInformation->GetMode() == TempInformation::MODE_WAIT_BRING_FEE)
			{
				// the server sends it.. void -_-
				//g_pMoneyManager->AddMoney(g_pTempInformation->Value1);
				// verified
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_SUCCESS_BRING_FEE].GetString());
			break;

		case NPC_RESPONSE_WITHDRAW_TAX_FAIL :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_FAIL_BRING_FEE].GetString());
			
			// verified -_-
			if(g_pTempInformation->GetMode() == TempInformation::MODE_WAIT_BRING_FEE)
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

			break;			
		
		case NPC_RESPONSE_NO_GUILD :
			if(PlayerState::GetRace()==RACE_SLAYER)
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_NO_TEAM].GetString());
			else
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_NO_CLAN].GetString());
			break;
		
		case NPC_RESPONSE_NOT_GUILD_MASTER:
			if(PlayerState::GetRace()==RACE_SLAYER)
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_NOT_TEAM_MASTER].GetString());
			else
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_NOT_CLAN_MASTER].GetString());
			break;
		
		case NPC_RESPONSE_HAS_NO_CASTLE:
			if(PlayerState::GetRace()==RACE_SLAYER)
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_TEAM_HAS_NO_CASTLE].GetString());
			else
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CLAN_HAS_NO_CASTLE].GetString());
			break;
		
		case NPC_RESPONSE_NOT_YOUR_CASTLE:
			if(PlayerState::GetRace()==RACE_SLAYER)
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_TEAM_NOT_YOUR_CASTLE].GetString());
			else
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CLAN_NOT_YOUR_CASTLE].GetString());
			break;
		
		case NPC_RESPONSE_NOT_ENOUGH_MONEY :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY].GetString());
			break;
		
		case NPC_RESPONSE_WAR_SCHEDULE_FULL :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_WAR_SCHEDULE_FULL].GetString());
			break;
		
		case NPC_RESPONSE_WAR_ALREADY_REGISTERED :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_WAR_ALREADY_REGISTERED].GetString());
			break;
		
		case NPC_RESPONSE_WAR_REGISTRATION_OK :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_WAR_REGISTRATION_OK].GetString());
			break;
		
		case NPC_RESPONSE_ALREADY_HAS_CASTLE :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_ALREADY_HAS_CASTLE].GetString());
			break;
		
		case NPC_RESPONSE_WAR_UNAVAILABLE :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_WAR_UNAVAILABLE].GetString());
			break;

		case NPC_RESPONSE_RACE_WAR_JOIN_FAILED :
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_RACE_WAR_JOIN_FAILED), g_pUserInformation->CharacterID.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
			break;

		case NPC_RESPONSE_RACE_WAR_JOIN_OK :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_RACE_WAR_JOIN_OK].GetString());
			break;

		case NPC_RESPONSE_RACE_WAR_GO_FIRST_SERVER :
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_RACE_WAR_GO_FIRST_SERVER].GetString());
			break;

		case NPC_RESPONSE_GIVE_EVENT_ITEM_FAIL_NOW:
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL_NOW].GetString());
		break;
    		
		case NPC_RESPONSE_GIVE_EVENT_ITEM_FAIL:
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL].GetString());
		break;

		case NPC_RESPONSE_GIVE_EVENT_ITEM_OK:
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_GIVE_EVENT_ITEM_OK].GetString());
		break;

		case NPC_RESPONSE_GIVE_PREMIUM_USER_ONLY:
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_GIVE_PREMIUM_USER_ONLY].GetString());
		break;

		// about couples
		case NPC_RESPONSE_WAIT_FOR_MEET_COUPLE:
			NPCPresentation::RunInputNameWindow(0);
		break;

		case NPC_RESPONSE_COUPLE_MEET_SUCCESS:
			NPCPresentation::CloseInputNameWindow();
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_COUPLE_MEET_SUCCESS].GetString());
			
		break;

		case NPC_RESPONSE_COUPLE_CANNOT_MEET:
			NPCPresentation::CloseInputNameWindow();
			if(pPacket->getParameter() != 0 && pPacket->getParameter() < COUPLE_MESSAGE_MAX)
				// the enum starts at 1, so -1. Keep it in MGameStringTable's order!!!
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_COUPLE_NOT_EVENT_TERM+pPacket->getParameter()-1].GetString());
			else
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_COUPLE_CANNOT_MEET].GetString());
			
		break;

		case NPC_RESPONSE_MEET_WAIT_TIME_EXPIRED:
			NPCPresentation::CloseInputNameWindow();
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_MEET_WAIT_TIME_EXPIRED].GetString());
		break;

		case NPC_RESPONSE_WAIT_FOR_APART_COUPLE:
			NPCPresentation::RunInputNameWindow(1);
		break;

		case NPC_RESPONSE_COUPLE_APART_SUCCESS:
			NPCPresentation::CloseInputNameWindow();
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_COUPLE_APART_SUCCESS].GetString());
			BonusSkills::RefreshAvailableSkills();
		break;

		case NPC_RESPONSE_NOT_COUPLE:
			NPCPresentation::CloseInputNameWindow();
			if(pPacket->getParameter() != 0 && pPacket->getParameter() < COUPLE_MESSAGE_MAX)
				// the enum starts at 1, so -1. Keep it in MGameStringTable's order!!!
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_COUPLE_NOT_EVENT_TERM+pPacket->getParameter()-1].GetString());
			else
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_NOT_COUPLE].GetString());
		break;

		case NPC_RESPONSE_APART_WAIT_TIME_EXPIRED:
			NPCPresentation::CloseInputNameWindow();
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_MEET_WAIT_TIME_EXPIRED].GetString());
		break;

		case NPC_RESPONSE_APART_COUPLE_FORCE :
			NPCPresentation::RunInputNameWindow(2);
			break;
		case NPC_RESPONSE_QUEST :
			switch( (QuestMessage) pPacket->getParameter() )
			{
			case START_SUCCESS :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_START_MONSTER_KILL_QUEST].GetString() );
				break;
			case START_FAIL_QUEST_NUM_EXCEEDED :				
			case START_FAIL_DUPLICATED_QUEST_ID :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_FAIL_ALREADY_START_MONSTER_KILL_QUEST].GetString() );
				break;
			case START_FAIL_PC :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_FAIL_MONSTER_KILL_QUEST_BY_STATUS].GetString() );
				break;
			case COMPLETE_SUCCESS :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_SUCCESS_MONSTER_KILL_QUEST].GetString() );
				//g_pEventManager->RemoveEvent( EVENTID_MONSTER_KILL_QUEST );
				NPCPresentation::InitQuestStatus();
				break;
			case COMPLETE_FAIL_NOT_COMPLETE :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_NOT_YET_COMPLETE_MONSTER_KILL_QUEST].GetString() );
				break;
			case COMPLETE_FAIL_NO_INVENTORY_SPACE :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST].GetString() );
				break;
			case COMPLETE_FAIL_NOT_IN_QUEST :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_NOT_IN_QUEST].GetString() );
				break;
			case COMPLETE_FAIL_TIME_EXPIRED :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_FAIL_QUEST_EXPIRED_TIME].GetString() );
				break;
			case START_FAIL_CANNOT_APPLY_QUEST :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_APPLY_QUEST].GetString() );
				break;
			case CANCEL_SUCCESS :
				//				g_pEventManager->RemoveEvent( EVENTID_MONSTER_KILL_QUEST );				
				NPCPresentation::InitQuestStatus();
				// the NPC script must show, so lock it again.
				PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_NPC_ASK );
				break;
			case CANCEL_NOT_IN_QUEST :
				PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_NPC_ASK );
				break;
			case COMPLETE_FAIL_INVALID_NPC :
				// no reward from this NPC.
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_FAIL_INVALID_NPC].GetString() );
				break;
			case FAIL_BUG :
				DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_FAIL_BUG].GetString() );
				break;
			}			
			break;
		case NPC_RESPONSE_LOTTERY :
			{
				int step = -1;
				if( pPacket->getParameter() <= 4 )
					step = pPacket->getParameter() + 1;
				
				if(step != -1 )
				{
					NPCPresentation::RunLottery( step );
				}
			}
			break;
		case NPC_RESPONSE_CANNOT_BUY :			
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_TAKE_OUT_ITEM_FROM_SHOP].GetString() );
			break;
		case NPC_RESPONSE_CLEAR_RANK_BONUS_OK :
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CLEAR_RANK_BONUS_OK].GetString() );
			break;
		case NPC_RESPONSE_NO_RANK_BONUS :
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NO_RANK_BONUS].GetString() );
			break;
		case NPC_RESPONSE_ALREADY_CLEAR_RANK_BONUS :
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_ALREADY_CLEAR_RANK_BONUS].GetString() );
			break;
		case NPC_RESPONSE_GNOME_CONTRACT_OK :
			DialogPresentation::ClosePCTalkDlg();
			NPCPresentation::PopupMessage( STRING_MESSAGE_CONTRACT_GNOMES_HORN_OK );
			break;
		case NPC_RESPONSE_DOWN_SKILL :
			NPCPresentation::DownSkill();
			break;
			
		case NPC_RESPONSE_GUILD_REGIST_FAIL_ALREADY_JOIN :
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_GUILD_REGIST_FAIL_ALREADY_JOIN), g_pUserInformation->GuildName.GetString());
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
			break;
		case NPC_RESPONSE_GUILD_REGIST_FAIL_QUIT_TIMEOUT :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_REGIST_FAIL_QUIT_TIMEOUT );
			break;
		case NPC_RESPONSE_GUILD_REGIST_FAIL_CANCEL_TIMEOUT :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_REGIST_FAIL_CANCEL_TIMEOUT );
			break;
		case NPC_RESPONSE_GUILD_REGIST_FAIL_LEVEL :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_REGIST_FAIL_LEVEL );
			break;
		case NPC_RESPONSE_GUILD_REGIST_FAIL_MONEY :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_REGIST_FAIL_MONEY );
			break;
		case NPC_RESPONSE_GUILD_REGIST_FAIL_FAME :
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_GUILD_REGIST_FAIL_FAME), g_pUserInformation->CharacterID.GetString() );
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
			break;
		case NPC_RESPONSE_GUILD_REGIST_FAIL_NAME :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_REGIST_FAIL_NAME );
			break;
		case NPC_RESPONSE_GUILD_REGIST_FAIL_DENY :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_REGIST_FAIL_DENY );
			break;			
		case NPC_RESPONSE_GUILD_STARTING_FAIL_ALREADY_JOIN :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_STARTING_FAIL_ALREADY_JOIN );
			break;
		case NPC_RESPONSE_GUILD_STARTING_FAIL_QUIT_TIMEOUT :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_STARTING_FAIL_QUIT_TIMEOUT );
			break;
		case NPC_RESPONSE_GUILD_STARTING_FAIL_CANCEL_TIMEOUT :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_STARTING_FAIL_CANCEL_TIMEOUT );
			break;
		case NPC_RESPONSE_GUILD_STARTING_FAIL_LEVEL :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_STARTING_FAIL_LEVEL );
			break;
		case NPC_RESPONSE_GUILD_STARTING_FAIL_MONEY :
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_GUILD_STARTING_FAIL_MONEY), g_pUserInformation->CharacterID.GetString() );
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
			break;
		case NPC_RESPONSE_GUILD_STARTING_FAIL_FAME :
			SafeFormat::Format(sz_temp, GetGameString(STRING_MESSAGE_GUILD_STARTING_FAIL_FAME), g_pUserInformation->CharacterID.GetString() );
			DialogPresentation::PopupFreeMessageDlg( sz_temp );
			break;
		case NPC_RESPONSE_GUILD_STARTING_FAIL_DENY :
			NPCPresentation::PopupMessage( STRING_MESSAGE_GUILD_REGIST_FAIL_DENY );
			break;
		case NPC_RESPONSE_TOO_MANY_GUILD_REGISTERED:	// 120
			NPCPresentation::PopupMessage( STRING_MESSAGE_TOO_MANY_GUILD_REGISTERED );
			break;
		case NPC_RESPONSE_REINFORCE_DENYED:	// 121
			NPCPresentation::PopupMessage( STRING_MESSAGE_REINFORCE_DENYED );
			break;
		case NPC_RESPONSE_ALREADY_REINFORCE_ACCEPTED:	// 122
			NPCPresentation::PopupMessage( STRING_MESSAGE_ALREADY_REINFORCE_ACCEPTED );
			break;
		case NPC_RESPONSE_NO_WAR_REGISTERED:
			NPCPresentation::PopupMessage( STRING_MESSAGE_NO_WAR_REGISTERED );
			break;
		case NPC_RESPONSE_CANNOT_ACCEPT:
			NPCPresentation::PopupMessage( STRING_MESSAGE_CANNOT_ACCEPT );
			break;
		case NPC_RESPONSE_ACCEPT_OK:
			NPCPresentation::PopupMessage( STRING_MESSAGE_ACCEPT_OK );
			break;
		case NPC_RESPONSE_CANNOT_DENY:
			NPCPresentation::PopupMessage( STRING_MESSAGE_CANNOT_DENY );
			break;
		case NPC_RESPONSE_DENY_OK:
			NPCPresentation::PopupMessage( STRING_MESSAGE_DENY_OK );
			break;
			
		case NPC_RESPONSE_SHOW_TAX_RATIO:
			NPCPresentation::RunModifyTax();
			break;
		case NPC_RESPONSE_MODIFY_TAX_RATIO_OK:
			NPCPresentation::PopupMessage( UI_STRING_MESSAGE_MODIFY_TAX_OK );
			break;
		case NPC_RESPONSE_MODIFY_TAX_RATIO_FAIL:
			NPCPresentation::PopupMessage( UI_STRING_MESSAGE_MODIFY_TAX_FAIL );
			break;
		case NPC_RESPONSE_SWAP_ADVANCEMENT_ITEM:
			NPCPresentation::ShowSwapAdvancementItem();
			break;
		case NPC_RESPONSE_NOT_ADVANCED:
			NPCPresentation::PopupMessage( UI_STRING_MESSAGE_SWAP_ERROR );
			break;
		case NPC_RESPONSE_SHOW_DONATION_DAILOG:
		//	value
			NPCPresentation::RunCampaignHelp(value);
			break;
		case NPC_RESPONSE_SHOW_DONATION_COMPLETE_DAILOG:
			NPCPresentation::PopupMessage( UI_STRING_MESSAGE_CAMPAIGN_HELP_THANKS );
			break;
			
		case NPC_RESPONSE_SHOW_CONFIRM_GET_EVENT_ITEM_DIALOG:
			NPCPresentation::RunConfirmGetEventItem(value);
			break;

		case NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG:
			switch(value)
			{
			case YOU_CAN_GET_EVENT_200412_COMBACK_ITEM:
				NPCPresentation::PopupComebackNotice( 0 );
				//UI_PopupMessage( UI_STRING_MESSAGE_GET_EVENT_ITEM_COMEBACK_1 );
				break;
			case YOU_CAN_GET_EVENT_200412_COMBACK_PREMIUM_ITEM:
				NPCPresentation::PopupComebackNotice( 1 );
				//UI_PopupMessage( UI_STRING_MESSAGE_GET_EVENT_ITEM_COMEBACK_2 );
				break;
			case YOU_CAN_GET_EVENT_200412_COMBACK_RECOMMEND_ITEM:
				NPCPresentation::PopupComebackNotice( 2 );
				//UI_PopupMessage( UI_STRING_MESSAGE_GET_EVENT_ITEM_COMEBACK_3 );
				break;
			case YOU_GET_EVENT_ITEM:
				NPCPresentation::PopupMessage( UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_OK );
				break;
			case NOT_ENOUGH_INVENTORY_SPACE:
				NPCPresentation::PopupMessage( UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST );
				break;
			case ALEADY_GET_EVENT_ITEM:
				NPCPresentation::PopupMessage( UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_ALREADY );
				break;
			case FAIL_GET_EVENT_ITEM:
				NPCPresentation::PopupMessage( UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_FAIL );
				break;
			case YOU_ARE_NOT_EVENT_USER:
				NPCPresentation::PopupMessage( UI_STRING_MESSAGE_GET_EVENT_ITEM_NOT_EVENT_USER );
				break;

			}
			break;

	}

	__END_CATCH

}




























