//-----------------------------------------------------------------------------
// UIMessageManager.cpp
//-----------------------------------------------------------------------------
// The UI's results turned into packets and model changes (UIMessageManager.h),
// gameclient's since task 4.258. Something done to the quick slot may find it
// gone: a timing problem in the UI, still unfixed.
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"
#include "DisplaySettings.h"
#include "DataPath.h"
#ifdef PLATFORM_WINDOWS
#include <io.h>
#include <process.h>
#endif
#include <stdio.h>
#include "MPlayer.h"
#include "MZone.h"
#include "ZoneFlags.h"
#include "FrameClock.h"
#include "InputState.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "HelpEvent.h"
#include "GameInitInfo.h"
#include "DXLib/CSoundPartManager.h"
#include "Packet/WireHost.h"
#include "UIMessageManager.h"
#include "WorldEventsHost.h"
#include "LoginPresentationHost.h"
#include "PacketDispatcher.h"
#include "UIFunction.h"
#include "VS_UI.h" 
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_Message.h"
#include "TalkBox.h"
#include "MShopShelf.h"
#include "MPriceManager.h"
#include "MMoneyManager.h"
#include "UIDialog.h"
#include "SkillDef.h"
#include "ClientFunction.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "MasterCommands.h"
#include "MItemOptionTable.h"
#include "CToken.h"
#include "UserOption.h"
#include "MWeather.h"
#include "MStatusManager.h"
#include "MChatManager.h"
#include "PacketDef.h"
#include "UtilityFunction.h"
#include "ClientConfig.h"
#include "MMusic.h"
#include "MMusicTable.h"
#include "SoundDef.h"
#include "CServerInformation.h"
#include "MParty.h"
#include "MItemFinder.h"
#include "GameSocket.h"
#include "MZoneTable.h"
//#include "MFileDef.h"
#include "Properties.h"
#include "UserInformation.h"
#include "TempInformation.h"
#include "ProfileManager.h"

#include "Packet/Cpackets/CGRequestNewbieItem.h"
#include "Packet/Cpackets/CGSilverCoating.h"
#include "Packet/Cpackets/CLChangeServer.h"
#include "Packet/Cpackets/CLGetServerList.h"
#include "Packet/Cpackets/CGSelectPortal.h"
#include "Packet/Cpackets/CGSelectWayPoint.h"
#include "Packet/Cpackets/CGPartyInvite.h"
#include "Packet/Cpackets/CGPartyLeave.h"
#include "Packet/Cpackets/CGResurrect.h"
#include "Packet/Cpackets/CLSelectWorld.h"
#include "Packet/Cpackets/CLSelectServer.h"
#include "Packet/Cpackets/CLQueryCharacterName.h"
#include "Packet/Cpackets/CGRelicToObject.h"
#include "Packet/Cpackets/CGSelectGuild.h"
#include "Packet/Cpackets/CGSelectGuildMember.h"
#include "Packet/Cpackets/CGRequestGuildMemberList.h"
#include "Packet/Cpackets/CGTryJoinGuild.h"
#include "Packet/Cpackets/CGJoinGuild.h"
#include "Packet/Cpackets/CGRegistGuild.h"
#include "Packet/Cpackets/CGModifyGuildMember.h"
#include "Packet/Cpackets/CGAddItemToItem.h"
#include "Packet/Cpackets/CGModifyGuildIntro.h"
#include "Packet/Cpackets/CGModifyGuildMemberIntro.h"
#include "Packet/Cpackets/CGSkillToNamed.h"
#include "Packet/Cpackets/CGSelectRankBonus.h"
#include "Packet/Cpackets/CGWithdrawTax.h"
#include "Packet/Cpackets/CGTypeStringList.h"
#include "Packet/Cpackets/CGLotterySelect.h"
#include "Packet/Cpackets/CGTakeOutGood.h"
#include "Packet/Cpackets/CGMixItem.h"					// -_- I heard it shows up like this in Visual Assist
#include "Packet/Cpackets/CGDownSkill.h"
#include "Packet/Gpackets/GCMiniGameScores.h"
#include "Packet/Cpackets/CGSubmitScore.h"
#include "Packet/Cpackets/CGAddItemToCodeSheet.h"
#include "Packet/Cpackets/CGFailQuest.h"
#include "Packet/Cpackets/CGSelectRegenZone.h"
#include "Packet/Cpackets/CGTameMonster.h"
#include "Packet/Cpackets/CGPetGamble.h"
#include "Packet/Cpackets/CGUseMessageItemFromInventory.h"
#include "Packet/Cpackets/CGPartySay.h"
#include "Packet/Cpackets/CGDepositPet.h"
#include "Packet/Cpackets/CGWithdrawPet.h"
#include "Packet/Cpackets/CGSMSSend.h"
#include "Packet/Cpackets/CGSMSAddressList.h"
#include "Packet/Cpackets/CGDeleteSMSAddress.h"
#include "Packet/Cpackets/CGAddSMSAddress.h"
#include "Packet/Cpackets/CGSelectNickname.h"
#include "Packet/Cpackets/CGModifyNickname.h"
#include "Packet/Cpackets/CGGQuestAccept.h"
#include "Packet/Cpackets/CGGQuestCancel.h"
#include "Packet/Cpackets/CGUseItemFromGQuestInventory.h"

#include "RequestServerPlayerManager.h"
#include "RequestUserManager.h"
#include "ClientCommunicationManager.h"
#include "Packet/Cpackets/CGWhisper.h"
#include "Packet/Rpackets/RCSay.h"
#include "Packet/Cpackets/CGGuildChat.h"
#include "RankBonusTable.h"
#include "Profiler.h"

#include "DebugLog.h"
#include "MEventManager.h"
#include "MOustersGear.h"
#include "DXLib/COGGSTREAM.h"
#include "ShrineInfoManager.h"

#include "MTimeItemManager.h"
#include "MFakeCreature.h"

#include "Packet/Cpackets/CGRequestUnion.h"
#include "Packet/Cpackets/CGQuitUnion.h"
#include "Packet/Cpackets/CGExpelGuild.h"
#include "Packet/Cpackets/CGRequestGuildList.h"
#include "Packet/Cpackets/CGRequestUnionInfo.h"
#include "Packet/Cpackets/CGAcceptUnion.h"
#include "Packet/Cpackets/CGDenyUnion.h"
#include "Packet/Cpackets/CGQuitUnionAccept.h"
#include "Packet/Cpackets/CGQuitUnionDeny.h"
#include "Packet/Cpackets/CGRangerSay.h"
#include "Packet/Cpackets/CGModifyTaxRatio.h"
#include "Packet/Cpackets/CGAppointSubmaster.h"
//
#include "Packet/Cpackets/CGRequestUnion.h"
#include "Packet/Cpackets/CGQuitUnion.h"
#include "Packet/Cpackets/CGExpelGuild.h"
#include "Packet/Cpackets/CGRequestGuildList.h"
#include "Packet/Cpackets/CGRequestUnionInfo.h"
#include "Packet/Cpackets/CGAcceptUnion.h"
#include "Packet/Cpackets/CGDenyUnion.h"
#include "Packet/Cpackets/CGQuitUnionAccept.h"
#include "Packet/Cpackets/CGQuitUnionDeny.h"
#include "Packet/Cpackets/CGRangerSay.h"
#include "Packet/Cpackets/CGModifyTaxRatio.h"
#include "Packet/Cpackets/CGAppointSubmaster.h"

#include "Packet/Cpackets/CGDisplayItem.h"
#include "Packet/Cpackets/CGUndisplayItem.h"
#include "Packet/Cpackets/CGStoreOpen.h"
#include "Packet/Cpackets/CGStoreClose.h"
#include "Packet/Cpackets/CGStoreSign.h"
#include "Packet/Cpackets/CGRequestStoreInfo.h"
#include "Packet/Gpackets/GCMyStoreInfo.h"
#include "Packet/Gpackets/GCOtherStoreInfo.h"
#include "Packet/Cpackets/CGBuyStoreItem.h"
#include "Packet/Gpackets/GCRemoveStoreItem.h"
#include "Packet/Gpackets/GCAddStoreItem.h"
//

#include "Packet/Cpackets/CGUsePowerPoint.h"
#include "Packet/Cpackets/CGRequestPowerPoint.h"
#include "Packet/Cpackets/CGDonationMoney.h"
#include "Packet/Cpackets/CGGetEventItem.h"
#include "Packet/Cpackets/CGUseMessageItemFromInventory.h"

//add by viva
#include "Packet/Cpackets/CGConnectSetKey.h"
#include "Packet/Gpackets/GCFriendChatting.h"
//end

//-----------------------------------------------------------------------------
// Special characters used in Chat
//-----------------------------------------------------------------------------
#define	SYMBOL_GLOBALCHAT			'!'			// zone chat
#define	SYMBOL_WHISPER				'/'			// whisper
#define	SYMBOL_COMMAND				'@'			// special command
#define SYMBOL_MASTER_COMMAND		'*'			// special command for operators

#define	STRING_IGNORE				"거부"		// 대화 거부
#define	STRING_IGNORE_ENG			"ignore"	// refuse conversation
#define	STRING_ACCEPT				"허용"		// 대화 허용
#define	STRING_ACCEPT_ENG			"accept"	// allow conversation
#define	STRING_ACCEPT_CURSE			"뭐라고?"	// 나쁜말 보기
#define	STRING_ACCEPT_CURSE_ENG		"what?"		// view bad language
#define	STRING_FILTER_CURSE			"바른말"	// 바른말만 보기
#define	STRING_FILTER_CURSE_ENG		"filter"	// view only clean language

int g_C2G = 0;

//-----------------------------------------------------------------------------
// extern
//-----------------------------------------------------------------------------
#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
	extern int g_ShowImageObjectID;
#endif

#ifdef OUTPUT_DEBUG
	extern MString g_ProfileInfoName;
#endif

bool							g_bEventQuestEnding = false;
extern BOOL g_MyFull;
extern RECT g_GameRect;
//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------
UIMessageManager*	g_pUIMessageManager = NULL;

extern BOOL g_bEnable3DHAL;
extern BOOL g_bSetHotKey;
extern bool	g_bTestMode;
extern bool g_bHolyLand;
extern bool g_bZoneSafe;

//-----------------------------------------------------------------------------
// The host GameInit installs (UIMessageManager.h); none until it does.
//-----------------------------------------------------------------------------
namespace UIMessages {

static const Host* s_Host = NULL;

const Host*
SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

static BOOL InitSocket()
{
	return s_Host != NULL && s_Host->InitSocket != NULL ? s_Host->InitSocket() : FALSE;
}

static void LoadingAddonSPK(bool bLoading)
{
	if (s_Host != NULL && s_Host->LoadingAddonSPK != NULL) s_Host->LoadingAddonSPK(bLoading);
}

static void ReleaseGameObject()
{
	if (s_Host != NULL && s_Host->ReleaseGameObject != NULL) s_Host->ReleaseGameObject();
}

static void StopLoadingThread()
{
	if (s_Host != NULL && s_Host->StopLoadingThread != NULL) s_Host->StopLoadingThread();
}

static void SaveLastSelectedCharacter(int slot)
{
	if (s_Host != NULL && s_Host->SaveLastSelectedCharacter != NULL) s_Host->SaveLastSelectedCharacter(slot);
}

static void SetMode(CLIENT_MODE mode)
{
	if (s_Host != NULL && s_Host->SetMode != NULL) s_Host->SetMode(mode);
}

static TYPE_OBJECTID GetSelectedCreature()
{
	return s_Host != NULL && s_Host->GetSelectedCreature != NULL ? s_Host->GetSelectedCreature() : OBJECTID_NULL;
}

static TYPE_OBJECTID GetSelectedItemID()
{
	return s_Host != NULL && s_Host->GetSelectedItemID != NULL ? s_Host->GetSelectedItemID() : OBJECTID_NULL;
}

}

//-----------------------------------------------------------------------------
// Execute Logout
//-----------------------------------------------------------------------------
void
ExecuteLogout()
{
	//--------------------------------------------------
	// if alive
	//--------------------------------------------------
	if (g_Mode==MODE_GAME)
	{
		DEBUG_ADD("ExecuteLogout");

		UI_SaveUserOption();

		// Stop the weather
		WorldEvents::SetWeather(WEATHER_CLEAR, 0);

		// If dead, send a resurrection packet and log out.
		// But.. it has no real meaning when actually processed.. - -;
		if (!g_pPlayer->IsAlive())
		{
				CGResurrect _CGResurrect;
				
				g_pSocket->sendPacket( &_CGResurrect );				

			gC_vs_ui.FinishRequestDie();
			gC_vs_ui.FinishRequestResurrect();
			gC_vs_ui.CloseRequestShrineMinimap();
		}

		if (1)//g_pPlayer->IsAlive())
		{
			//--------------------------------------------------
			// Stop the music.
			//--------------------------------------------------
			if (g_pUserOption->PlayWaveMusic)
			{
//				g_pSDLStream->Stop();
				DEBUG_ADD("MP3 STOP7");
				if( g_SDLAudio.IsInit() )
					g_pOGG->streamClose();
				DEBUG_ADD("MP3 STOP7 OK");
			}
			else
			{
				g_Music.Stop();
			}

			//--------------------------------------------------
			// End the loading thread..
			//--------------------------------------------------
			UIMessages::StopLoadingThread();

			//
			// Send logout packet to Client and Server.
			//
			//--------------------------------------------------
			// Send a CGLogout packet to the game server.
			//--------------------------------------------------
				UI_SaveHotKeyToServer();

				CGLogout cgLogout;
			
				g_pSocket->sendPacket( &cgLogout );
				//g_pSocket->setPlayerStatus( CPS_END_SESSION );

				g_pSocket->processOutput();

				g_pSocket->setPlayerStatus( CPS_WAITING_FOR_GC_RECONNECT_LOGIN );	

				
				//g_pSocket->disconnect();

				// 2001.6.12 : do not release; reconnect.
				//ReleaseSocket();

			/*
			if (g_pZone != NULL)
			{
				g_pZone->RemovePlayer();
				g_pPlayer->SetStop();
				g_pPlayer->SetAction( ACTION_STAND );
				//g_pZone->UnSetLight(g_pPlayer->GetX(), g_pPlayer->GetY(), g_pPlayer->GetLightSight());	// remove sight
			}
			*/

			// Stop the weather..
			//SetWeather(WEATHER_CLEAR, 0);
			UIMessages::ReleaseGameObject();

			// 2001.6.12: reconnect
			//SetMode( MODE_MAINMENU );	
			UIMessages::SetMode( MODE_WAIT_RECONNECT_LOGIN );

			//------------------------------------------------------
			// [ TEST CODE ]
			// Is it OK to initialize sound here??
			//------------------------------------------------------
			//InitSound();
			g_CurrentFrame		= 0;

			g_bSetHotKey	= FALSE;

			//------------------------------------------------------
			// ui dialog
			//------------------------------------------------------
			g_pUIDialog->HidePCTalkDlg();
			g_pUIDialog->CloseMessageDlg();


			//------------------------------------------------------
			// allow all conversation
			//------------------------------------------------------
			g_pChatManager->ClearID();
			g_pChatManager->SetAcceptMode();

//			gC_vs_ui.ServerDisconnectMessage();
//			gC_vs_ui.CloseAllDialog();

			// Start Title screen UI
			//gC_vs_ui.EndTitle();
			gC_vs_ui.StartTitle();	
		
			gC_vs_ui.DropItem();

			g_bUIInput = FALSE;

			// Clear the whisper target.
			g_pUserInformation->WhisperID.Release();

			//----------------------------------------------
			// remove message
			//----------------------------------------------
			g_pSystemMessage->Clear();
			g_pGameMessage->Clear();
			g_pNoticeMessage->Clear();

			//------------------------------------------------------
			// start music
			//------------------------------------------------------
			if (g_pUserOption->PlayWaveMusic)
			{
//				g_pSDLStream->Stop();			
				DEBUG_ADD("MP3 STOP8");
				if( g_SDLAudio.IsInit() )
					g_pOGG->streamClose();
				DEBUG_ADD("MP3 STOP8 OK");

				if (g_pUserOption->PlayMusic)
				{
					int musicID = g_pClientConfig->MUSIC_THEME;

					if (musicID!=MUSICID_NULL)
					{
						if( g_oggfile != NULL )
							fclose(g_oggfile);
						
						g_oggfile = NULL;

						if( g_SDLAudio.IsInit() )
						{
							g_oggfile = Basic::OpenFile( Basic::NormalizeDataPath( (*g_pMusicTable)[ musicID ].FilenameWav.GetString() ).c_str(), "rb");
							if( g_oggfile != NULL )
							{
								g_pOGG->streamLoad( g_oggfile, NULL );
								g_pOGG->streamPlay( SOUND_PLAY_REPEAT );
								int volume = (g_pUserOption->VolumeMusic - 15) * 250;
								g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
							}
						}
					}
				}
			}
			else
			{
				g_Music.Stop();			

				if (g_pUserOption->PlayMusic)
				{
					int musicID = g_pClientConfig->MUSIC_THEME;

					if (musicID!=MUSICID_NULL)
					{
						g_Music.Play( (*g_pMusicTable)[ musicID ].Filename );
					}
				}
			}
		}
		else
		{
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_LOGOUT_DIED].GetString() );
		}
	}
	else
	{
		DEBUG_ADD("Logout Not ModeGame");
	}
}

//-----------------------------------------------------------------------------
// Play TitleMusic
//-----------------------------------------------------------------------------
void
PlayTitleMusic()
{
	//----------------------------------------------------------------
	// WAV output
	//----------------------------------------------------------------
	if (g_pUserOption->PlayWaveMusic)
	{
		g_Music.Stop();

		if (g_pUserOption->PlayMusic)
		{
			int musicID = g_pClientConfig->MUSIC_THEME;

			if (musicID!=MUSICID_NULL)
			{
				if( g_oggfile != NULL )
					fclose(g_oggfile);

				g_oggfile = NULL;

				if( g_SDLAudio.IsInit() )
				{
					g_oggfile = Basic::OpenFile( Basic::NormalizeDataPath( (*g_pMusicTable)[ musicID ].FilenameWav.GetString() ).c_str(), "rb");
					if( g_oggfile != NULL )
					{					
						int volume = (g_pUserOption->VolumeMusic - 15) * 250;

						g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
						g_pOGG->streamLoad( g_oggfile,NULL );
						g_pOGG->streamPlay( SOUND_PLAY_REPEAT );					
					}
				}
			}									
		}
		else
		{
//			g_pSDLStream->Stop();
			DEBUG_ADD("MP3 STOP9");
			if( g_SDLAudio.IsInit() )
				g_pOGG->streamClose();
			DEBUG_ADD("MP3 STOP9 OK");
		}
	}
	//----------------------------------------------------------------
	// MID output
	//----------------------------------------------------------------
	else
	{
//		g_pSDLStream->Stop();
		DEBUG_ADD("MP3 STOP10");
		if( g_SDLAudio.IsInit() )
			g_pOGG->streamClose();
		DEBUG_ADD("MP3 STOP10 OK");

		if (g_pUserOption->PlayMusic)
		{
			if (g_Music.IsPlay())
			{
				g_Music.Resume();
			}
			else //if (g_Mode==MODE_GAME)
			{	
				WORD volume = g_pUserOption->VolumeMusic*0x1000 + 0x0FFF;

				if (g_Music.IsPause())
				{
					g_Music.SetVolume( volume );					
					g_Music.Resume();
				}
				else
				{
					g_Music.Pause();
					g_Music.SetVolume( volume );
					g_Music.Resume();					

					int musicID = g_pClientConfig->MUSIC_THEME;

					if (musicID!=MUSICID_NULL)
					{
						g_Music.Play( (*g_pMusicTable)[ musicID ].Filename );
					}
				}					
			}				
		}
		else
		{
			if (g_Music.IsPlay())
			{
				g_Music.Pause();
			}
			else
			{
				g_Music.Stop();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// PlayGameMusic
//-----------------------------------------------------------------------------
void
PlayGameMusic()
{
	//----------------------------------------------------------------
	// WAV output
	//----------------------------------------------------------------
	if (g_pUserOption->PlayWaveMusic)
	{
		g_Music.Stop();

		if (g_pUserOption->PlayMusic)
		{
			if( g_SDLAudio.IsInit() )
			{
				int volume = (g_pUserOption->VolumeMusic - 15) * 250;

				g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
			}

			WorldEvents::PlayMusicCurrentZone();
		}
		else
		{					
//			g_pSDLStream->Stop();				
			DEBUG_ADD("MP3 STOP11");
			if( g_SDLAudio.IsInit() )
				g_pOGG->streamClose();
			DEBUG_ADD("MP3 STOP11 OK");
		}
	}
	//----------------------------------------------------------------
	// MID output
	//----------------------------------------------------------------
	else
	{
//		g_pSDLStream->Stop();
		DEBUG_ADD("MP3 STOP12");
		if( g_SDLAudio.IsInit() )
			g_pOGG->streamClose();
		
		DEBUG_ADD("MP3 STOP12 OK");

		if (g_pUserOption->PlayMusic)
		{
			DEBUG_ADD("MP3 STOP12 OK2");
			if (g_Music.IsPlay())
			{
				DEBUG_ADD("MP3 STOP12 RESUME");
				g_Music.Resume();
				DEBUG_ADD("MP3 STOP12 RESUME OK");
			}
			else //if (g_Mode==MODE_GAME)
			{				
				DEBUG_ADD("MP3 STOP12 pp");
				WORD volume = g_pUserOption->VolumeMusic*0x1000 + 0x0FFF;
				DEBUG_ADD("MP3 STOP12 pp1");
				g_Music.SetVolume( volume );
				DEBUG_ADD("MP3 STOP12 pp2");
				
				WorldEvents::PlayMusicCurrentZone();
				DEBUG_ADD("MP3 STOP12 PlayMusicCurrentZone OK");
			}				
		}
		else
		{
			if (g_Music.IsPlay())
			{
				g_Music.Pause();
			}
			else
			{
				g_Music.Stop();
			}
		}
	}
	DEBUG_ADD("PlayGameMusic OK");
}

//-----------------------------------------------------------------------------
//
// constructor / destructor
//
//-----------------------------------------------------------------------------
UIMessageManager::UIMessageManager()
{
	for (int i=0; i<MAX_UI_MESSAGE; i++)
	{
		m_UIMessageFunction[i] = NULL;
	}
}

UIMessageManager::~UIMessageManager()
{
	// just for testing.. - -;
	for (int i=0; i<MAX_UI_MESSAGE; i++)
	{
		m_UIMessageFunction[i] = NULL;
	}
}

//-----------------------------------------------------------------------------
//
// member functions
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
void
UIMessageManager::Init()
{	
	m_UIMessageFunction[UI_STORAGE_BUY] = Execute_UI_STORAGE_BUY;
	m_UIMessageFunction[UI_SELECT_STORAGE_SLOT] = Execute_UI_SELECT_STORAGE_SLOT;
	m_UIMessageFunction[UI_RUNNING_GAMEMENU] = Execute_UI_RUNNING_GAMEMENU;
	m_UIMessageFunction[UI_FINISH_LEVELUP_BUTTON] = Execute_UI_FINISH_LEVELUP_BUTTON;
	m_UIMessageFunction[UI_CLICK_BONUS_POINT] = Execute_UI_CLICK_BONUS_POINT;
	m_UIMessageFunction[UI_INFO] = Execute_UI_INFO;
//	m_UIMessageFunction[UI_MINIMAP_TOGGLE] = Execute_UI_MINIMAP_TOGGLE;
	m_UIMessageFunction[UI_DROP_MONEY] = Execute_UI_DROP_MONEY;
	m_UIMessageFunction[UI_EXCHANGE_REQUEST_CANCEL] = Execute_UI_EXCHANGE_REQUEST_CANCEL;
	m_UIMessageFunction[UI_EXCHANGE_ACCEPT] = Execute_UI_EXCHANGE_ACCEPT;
	m_UIMessageFunction[UI_EXCHANGE_MONEY] = Execute_UI_EXCHANGE_MONEY;
	m_UIMessageFunction[UI_WITHDRAW_MONEY] = Execute_UI_WITHDRAW_MONEY;
	m_UIMessageFunction[UI_ITEM_SELECT_EXCHANGE] = Execute_UI_ITEM_SELECT_EXCHANGE;
	m_UIMessageFunction[UI_DEPOSIT_MONEY] = Execute_UI_DEPOSIT_MONEY;
	m_UIMessageFunction[UI_CHECK_EXIST_ID] = Execute_UI_CHECK_EXIST_ID;
	m_UIMessageFunction[UI_RUN_NEWUSER_REGISTRATION] = Execute_UI_RUN_NEWUSER_REGISTRATION;
	m_UIMessageFunction[UI_INFO_CLOSE] = Execute_UI_INFO_CLOSE;
	m_UIMessageFunction[UI_GAMEMENU_CONTINUE] = Execute_UI_GAMEMENU_CONTINUE;

	//
	// PDS
	//
//	m_UIMessageFunction[UI_CHANGE_PCS_CONNECTED_SLOT] = Execute_UI_CHANGE_PCS_CONNECTED_SLOT;		// Clicked another person who is already connected.
//	m_UIMessageFunction[UI_PLEASE_PCS_CONNECT_ME] = Execute_UI_PLEASE_PCS_CONNECT_ME;			// Clicked a wait room that has a person in it.
	//m_UIMessageFunction[UI_PCS_CONNECTOR_GRANTED] = Execute_UI_PCS_CONNECTOR_GRANTED;			// Accepted the connection with the connector in the wait room.
//	m_UIMessageFunction[UI_QUIT_PCS_ONLINE_MODE] = Execute_UI_QUIT_PCS_ONLINE_MODE;			// Pressed the quit button in PCS online mode.
//	m_UIMessageFunction[UI_END_PCS] = Execute_UI_END_PCS;								// Pressed end while sending.
//	m_UIMessageFunction[UI_SEND_PCS_NUMBER] = Execute_UI_SEND_PCS_NUMBER;
//	m_UIMessageFunction[UI_PDS_CLOSED] = Execute_UI_PDS_CLOSED;							// The slayer pds was closed. ! The current connection is kept.

//	m_UIMessageFunction[UI_PLEASE_SET_SLAYER_VALUE] = Execute_UI_PLEASE_SET_SLAYER_VALUE;		// Ran the gauge menu from the slayer pds.

//	m_UIMessageFunction[UI_LEARN_SLAYER_SKILL] = Execute_UI_LEARN_SLAYER_SKILL;				// Selected a learnable skill in the skill tree.
//	m_UIMessageFunction[UI_CLOSE_SKILL_VIEW] = Execute_UI_CLOSE_SKILL_VIEW;

	m_UIMessageFunction[UI_BACKGROUND_MOUSE_FOCUS] = Execute_UI_BACKGROUND_MOUSE_FOCUS;			// Delivered to Client when the UI Window disappears while the mouse pointer has stayed on it without moving.
	m_UIMessageFunction[UI_REMOVE_BACKGROUND_MOUSE_FOCUS] = Execute_UI_REMOVE_BACKGROUND_MOUSE_FOCUS;// When a topmost Window such as the Game Menu is shown, it is better to remove the background focus.

	//
	// shop Message
	//
	m_UIMessageFunction[UI_BUY_ITEM] = Execute_UI_BUY_ITEM;

	m_UIMessageFunction[UI_SELL_ITEM] = Execute_UI_SELL_ITEM;
	m_UIMessageFunction[UI_SELL_ALL_ITEM] = Execute_UI_SELL_ALL_ITEM;
	m_UIMessageFunction[UI_REPAIR_ITEM] = Execute_UI_REPAIR_ITEM;
	m_UIMessageFunction[UI_SILVERING_ITEM] = Execute_UI_SILVERING_ITEM;
	m_UIMessageFunction[UI_ITEM_SELL_FINISHED] = Execute_UI_ITEM_SELL_FINISHED;
	m_UIMessageFunction[UI_ITEM_REPAIR_FINISHED] = Execute_UI_ITEM_REPAIR_FINISHED;
	m_UIMessageFunction[UI_ITEM_SILVERING_FINISHED] = Execute_UI_ITEM_SILVERING_FINISHED;

	m_UIMessageFunction[UI_CLOSE_SHOP] = Execute_UI_CLOSE_SHOP;
	m_UIMessageFunction[UI_CLOSE_STORAGE] = Execute_UI_CLOSE_STORAGE;
	m_UIMessageFunction[UI_CLOSE_EXCHANGE] = Execute_UI_CLOSE_EXCHANGE;
	m_UIMessageFunction[UI_CANCEL_EXCHANGE] = Execute_UI_CANCEL_EXCHANGE;
	m_UIMessageFunction[UI_OK_EXCHANGE] = Execute_UI_OK_EXCHANGE;

	m_UIMessageFunction[UI_CHARACTER_MANAGER_FINISHED] = Execute_UI_CHARACTER_MANAGER_FINISHED;	// Notified when going from the Characters Window back to the initial screen.
	m_UIMessageFunction[UI_TERMINATION] = Execute_UI_TERMINATION;						// termination
	m_UIMessageFunction[UI_LOGIN] = Execute_UI_LOGIN;								// user login request
	m_UIMessageFunction[UI_LOGOUT] = Execute_UI_LOGOUT;								
	//m_UIMessageFunction[UI_NEW_USER_REGISTRATION] = Execute_UI_NEW_USER_REGISTRATION;			// new user registration request
	m_UIMessageFunction[UI_CONNECT] = Execute_UI_CONNECT;							   // Game connection.
	m_UIMessageFunction[UI_CHAT_RETURN] = Execute_UI_CHAT_RETURN;						// chat window input.
//	m_UIMessageFunction[UI_CHAT_SELECT_NAME] = Execute_UI_CHAT_SELECT_NAME;						// select name in chat window.
	m_UIMessageFunction[UI_SELECT_SKILL] = Execute_UI_SELECT_SKILL;						// Skill selection.
	m_UIMessageFunction[UI_CANCEL_SELECT_SKILL] = Execute_UI_CANCEL_SELECT_SKILL;				// Skill selection was cancelled.

	//
	// Item message
	//

	// drop
	m_UIMessageFunction[UI_ITEM_DROP_TO_CLIENT] = Execute_UI_ITEM_DROP_TO_CLIENT;				// Dropped the Item on the ground.
	m_UIMessageFunction[UI_ITEM_DROP_TO_INVENTORY] = Execute_UI_ITEM_DROP_TO_INVENTORY; 		// Put into Inventory.
	//m_UIMessageFunction[UI_ITEM_DROP_TO_TRADEGRID] = Execute_UI_ITEM_DROP_TO_TRADEGRID;			// Put onto the trade window grid.
	m_UIMessageFunction[UI_ITEM_DROP_TO_GEAR] = Execute_UI_ITEM_DROP_TO_GEAR;				// Put onto Gear.
	m_UIMessageFunction[UI_ITEM_DROP_TO_QUICKSLOT] = Execute_UI_ITEM_DROP_TO_QUICKSLOT;			// Put into Quick Slot.

	// pick up
	// Picking up in Client is Client -> UI.
	//m_UIMessageFunction[UI_ITEM_PICKUP_FROM_TRADEGRID] = Execute_UI_ITEM_PICKUP_FROM_TRADEGRID;
	m_UIMessageFunction[UI_ITEM_PICKUP_FROM_INVENTORY] = Execute_UI_ITEM_PICKUP_FROM_INVENTORY;
	m_UIMessageFunction[UI_ITEM_PICKUP_FROM_GEAR] = Execute_UI_ITEM_PICKUP_FROM_GEAR;
	m_UIMessageFunction[UI_ITEM_PICKUP_FROM_QUICKSLOT] = Execute_UI_ITEM_PICKUP_FROM_QUICKSLOT;

	// insert to other Item
	//m_UIMessageFunction[UI_ITEM_INSERT_FROM_TRADEGRID] = Execute_UI_ITEM_INSERT_FROM_TRADEGRID;
	m_UIMessageFunction[UI_ITEM_INSERT_FROM_INVENTORY] = Execute_UI_ITEM_INSERT_FROM_INVENTORY;
	m_UIMessageFunction[UI_ITEM_INSERT_FROM_GEAR] = Execute_UI_ITEM_INSERT_FROM_GEAR;
	m_UIMessageFunction[UI_ITEM_INSERT_FROM_QUICKSLOT] = Execute_UI_ITEM_INSERT_FROM_QUICKSLOT;  // when stacking - not supported yet -

	// use
	m_UIMessageFunction[UI_ITEM_USE] = Execute_UI_ITEM_USE;							// Use from Inventory.
	m_UIMessageFunction[UI_ITEM_USE_QUICKSLOT] = Execute_UI_ITEM_USE_QUICKSLOT;				// Use from Quick Item Slot.

	//
	// Character management
	//
	m_UIMessageFunction[UI_NEW_CHARACTER] = Execute_UI_NEW_CHARACTER;
	m_UIMessageFunction[UI_DELETE_CHARACTER] = Execute_UI_DELETE_CHARACTER;

	//m_UIMessageFunction[UI_CLOSE_BBS] = Execute_UI_CLOSE_BBS;

//	m_UIMessageFunction[UI_CHANGE_GAME_OPTION] = Execute_UI_CHANGE_GAME_OPTION;
//	m_UIMessageFunction[UI_CLOSE_GAME_OPTION] = Execute_UI_CLOSE_GAME_OPTION;
	m_UIMessageFunction[UI_CHANGE_OPTION] = Execute_UI_CHANGE_OPTION;
	m_UIMessageFunction[UI_CLOSE_OPTION] = Execute_UI_CLOSE_OPTION;
	m_UIMessageFunction[UI_CLOSE_BOOKCASE] = Execute_UI_CLOSE_BOOKCASE;
	m_UIMessageFunction[UI_CLOSE_BRIEFING] = Execute_UI_CLOSE_BRIEFING;
	m_UIMessageFunction[UI_CLOSE_COMPUTER] = Execute_UI_CLOSE_COMPUTER;
	m_UIMessageFunction[UI_CLOSE_TUTORIAL_EXIT] = Execute_UI_CLOSE_TUTORIAL_EXIT;

	m_UIMessageFunction[UI_CLOSE_DESC_DIALOG] = Execute_UI_CLOSE_DESC_DIALOG;
	m_UIMessageFunction[UI_CLOSE_ELEVATOR] = Execute_UI_CLOSE_ELEVATOR;
	m_UIMessageFunction[UI_SELECT_ELEVATOR] = Execute_UI_SELECT_ELEVATOR;

	//m_UIMessageFunction[UI_SELECT_SERVER] = Execute_UI_SELECT_SERVER;

	//m_UIMessageFunction[UI_REQUEST_SERVER_LIST] = Execute_UI_REQUEST_SERVER_LIST;

	m_UIMessageFunction[UI_ITEM_TO_QUICKITEMSLOT] = Execute_UI_ITEM_TO_QUICKITEMSLOT;

	m_UIMessageFunction[UI_CLOSE_SLAYER_PORTAL] = Execute_UI_CLOSE_SLAYER_PORTAL;
	m_UIMessageFunction[UI_SLAYER_PORTAL] = Execute_UI_SLAYER_PORTAL;

	m_UIMessageFunction[UI_FINISH_REQUEST_PARTY_BUTTON] = Execute_UI_FINISH_REQUEST_PARTY_BUTTON;
	m_UIMessageFunction[UI_PARTY_REQUEST_CANCEL] = Execute_UI_PARTY_REQUEST_CANCEL;
	m_UIMessageFunction[UI_PARTY_ACCEPT] = Execute_UI_PARTY_ACCEPT;

	m_UIMessageFunction[UI_CLOSE_PARTY_MANAGER] = Execute_UI_CLOSE_PARTY_MANAGER;
	m_UIMessageFunction[UI_AWAY_PARTY] = Execute_UI_AWAY_PARTY;

	m_UIMessageFunction[UI_FINISH_REQUEST_DIE_BUTTON] = Execute_UI_FINISH_REQUEST_DIE_BUTTON;

	m_UIMessageFunction[UI_CLOSE_TEAM_LIST] = Execute_UI_CLOSE_TEAM_LIST;
	m_UIMessageFunction[UI_CLOSE_TEAM_INFO] = Execute_UI_CLOSE_TEAM_INFO;
	m_UIMessageFunction[UI_CLOSE_TEAM_MEMBER_INFO] = Execute_UI_CLOSE_TEAM_MEMBER_INFO;
	m_UIMessageFunction[UI_CLOSE_TEAM_REGIST] = Execute_UI_CLOSE_TEAM_REGIST;
	m_UIMessageFunction[UI_CLOSE_TEAM_MEMBER_LIST] = Execute_UI_CLOSE_TEAM_MEMBER_LIST;
	
	m_UIMessageFunction[UI_REQUEST_GUILD_INFO] = Execute_UI_REQUEST_GUILD_INFO;
	m_UIMessageFunction[UI_REQUEST_GUILD_MEMBER_LIST] = Execute_UI_REQUEST_GUILD_MEMBER_LIST;
	m_UIMessageFunction[UI_MODIFY_GUILD_MEMBER] = Execute_UI_MODIFY_GUILD_MEMBER;

	m_UIMessageFunction[UI_SELECT_TEAM_MEMBER_LIST] = Execute_UI_SELECT_TEAM_MEMBER_LIST;		// void_ptr = TEAM_NAME
	m_UIMessageFunction[UI_SELECT_READY_TEAM_LIST] = Execute_UI_SELECT_READY_TEAM_LIST;		// void_ptr = TEAM_NAME
	m_UIMessageFunction[UI_SELECT_REGIST_TEAM_LIST] = Execute_UI_SELECT_REGIST_TEAM_LIST;		// void_ptr = TEAM_NAME
//add by viva
	m_UIMessageFunction[UI_CLOSE_FRIEND_CHATTING_INFO] = Execute_UI_CLOSE_FRIEND_CHATTING_INFO;
	m_UIMessageFunction[UI_OPEN_FRIEND_CHATTING_INFO] = Execute_UI_OPEN_FRIEND_CHATTING_INFO;
	//friend message
	m_UIMessageFunction[UI_FRIEND_CHATTING_SEND_MESSAGE] = Execute_UI_FRIEND_CHATTING_SEND_MESSAGE;
	m_UIMessageFunction[UI_FRIEND_CHATTING_UPDATE] = Execute_UI_FRIEND_CHATTING_UPDATE;
	m_UIMessageFunction[UI_FRIEND_CHATTING_ADD_FRIEND] = Execute_UI_FRIEND_CHATTING_ADD_FRIEND;
	//ask_friend_request
	m_UIMessageFunction[UI_FRIEND_REQUEST_ACCEPT] = Execute_UI_FRIEND_REQUEST_ACCEPT;
	//ask_friend_ask_close
	m_UIMessageFunction[UI_FRIEND_ASK_CLOSE] = Execute_UI_FRIEND_ASK_CLOSE;
	//ask_friend_delete_request
	m_UIMessageFunction[UI_FRIEND_DELETE_ASK] = Execute_UI_FRIEND_DELETE_ASK;
	m_UIMessageFunction[UI_FRIEND_DELETE_ACCEPT] = Execute_UI_FRIEND_DELETE_ACCEPT;
//end
	m_UIMessageFunction[UI_JOIN_READY_TEAM] = Execute_UI_JOIN_READY_TEAM;				// void_ptr = TEAM_NAME
	m_UIMessageFunction[UI_JOIN_REGIST_TEAM] = Execute_UI_JOIN_REGIST_TEAM;			// void_ptr = TEAM_NAME

	m_UIMessageFunction[UI_REGIST_GUILD_MEMBER] = Execute_UI_REGIST_GUILD_MEMBER;			// void_ptr = introduction max:150byte, remember to close the window!
	m_UIMessageFunction[UI_REGIST_GUILD_TEAM] = Execute_UI_REGIST_GUILD_TEAM;			// left = TEAM_NAME, void_ptr = introduction max:150byte, remember to close the window!

	m_UIMessageFunction[UI_CLOSE_FILE_DIALOG] = Execute_UI_CLOSE_FILE_DIALOG;


	m_UIMessageFunction[UI_CONNECT_SERVER] = Execute_UI_CONNECT_SERVER;
	m_UIMessageFunction[UI_CLOSE_SERVER_SELECT] = Execute_UI_CLOSE_SERVER_SELECT;

	m_UIMessageFunction[UI_NEWCHARACTER_CHECK] = Execute_UI_NEWCHARACTER_CHECK;
	
//	m_UIMessageFunction[UI_SELECT_EXPLOSIVE] = Execute_UI_SELECT_EXPLOSIVE;
//	m_UIMessageFunction[UI_CLOSE_SELECT_EXPLOSIVE] = Execute_UI_CLOSE_SELECT_EXPLOSIVE;

	m_UIMessageFunction[UI_ENCHANT_ACCEPT] = Execute_UI_ENCHANT_ACCEPT;
	m_UIMessageFunction[UI_ENCHANT_CANCEL] = Execute_UI_ENCHANT_CANCEL;

	m_UIMessageFunction[UI_MESSAGE_BOX] = Execute_UI_MESSAGE_BOX;

	m_UIMessageFunction[UI_CLOSE_OTHER_INFO] = Execute_UI_CLOSE_OTHER_INFO;

	m_UIMessageFunction[UI_MODIFY_TEAM_INFO] = Execute_UI_MODIFY_TEAM_INFO;
	m_UIMessageFunction[UI_MODIFY_GUILD_MEMBER_INTRO] = Execute_UI_MODIFY_GUILD_MEMBER_INTRO;

	m_UIMessageFunction[UI_SEND_NAME_FOR_SOUL_CHAIN] = Execute_UI_SEND_NAME_FOR_SOUL_CHAIN;
	m_UIMessageFunction[UI_CLOSE_TRACE_WINDOW] = Execute_UI_CLOSE_TRACE_WINDOW;

	m_UIMessageFunction[UI_RUN_CONNECT] = Execute_UI_RUN_CONNECT;

	m_UIMessageFunction[UI_SELECT_GRADE_SKILL] = Excute_UI_SELECT_GRADE_SKILL;

	m_UIMessageFunction[UI_USE_XMAS_TREE] = Excute_UI_USE_XMAS_TREE;
	m_UIMessageFunction[UI_CLOSE_XMAS_CARD_WINDOW] = Excute_UI_CLOSE_XMAS_CARD_WINDOW;

	m_UIMessageFunction[UI_SEND_BRING_FEE] = Excute_UI_SEND_BRING_FEE;
	m_UIMessageFunction[UI_CLOSE_BRING_FEE_WINDOW] = Excute_UI_CLOSE_BRING_FEE_WINDOW;

	m_UIMessageFunction[UI_CLOSE_WAR_LIST_WINDOW] = Excute_UI_CLOSE_WAR_LIST;

	m_UIMessageFunction[UI_CLOSE_BLOOD_BIBLE_STATUS] = Execute_UI_CLOSE_BLOOD_BIBLE_STATUS;
	m_UIMessageFunction[UI_SEND_NAME_FOR_COUPLE] = Execute_UI_SEND_NAME_FOR_COUPLE;
	m_UIMessageFunction[UI_CLOSE_INPUT_NAME] = Execute_UI_CLOSE_INPUT_NAME_WINDOW;

	m_UIMessageFunction[UI_ITEM_USE_GEAR] = Execute_UI_ITEM_USE_GEAR;
	m_UIMessageFunction[UI_GO_BILING_PAGE] = Execute_GO_BILING_PAGE;
	m_UIMessageFunction[UI_CLOSE_POPUP_MESSAGE] = Execute_UI_CLOSE_POPUP_MESSAGE;
	m_UIMessageFunction[UI_CLOSE_QUEST_STATUS] = Execute_UI_CLOSE_QUEST_STATUS;

	m_UIMessageFunction[UI_CLOSE_LOTTERY_CARD] = Execute_UI_CLOSE_LOTTERY_CARD;
	m_UIMessageFunction[UI_LOTTERY_CARD_STATUS] = Execute_UI_LOTTERY_CARD_STATUS;
	m_UIMessageFunction[UI_FINISH_SCRATCH_LOTTERY]	= Execute_UI_FINISH_SCRATCH_LOTTERY;
	m_UIMessageFunction[UI_CLOSE_IMAGE_NOTICE] = Execute_UI_CLOSE_IMAGE_NOTICE;

	m_UIMessageFunction[UI_CLOSE_ITEM_LIST]	= Execute_UI_CLOSE_ITEM_LIST;
	m_UIMessageFunction[UI_SELECT_ITEM_FROM_SHOP] = Execute_UI_SELECT_ITEM_FROM_SHOP;
	m_UIMessageFunction[UI_CLOSE_BULLETIN_BOARD_WINDOW] = Execute_UI_CLOSE_BULLETIN_BOARD;

	m_UIMessageFunction[UI_TRANS_ITEM_CANCEL] = Execute_UI_TRANS_ITEM_CANCEL;
	m_UIMessageFunction[UI_TRANS_ITEM_ACCEPT] = Execute_UI_TRANS_ITEM_ACCEPT;
	m_UIMessageFunction[UI_FINISH_REQUEST_RESURRECT_BUTTON] = Execute_UI_REQUEST_RESURRECT;

	m_UIMessageFunction[UI_CLOSE_MIXING_FORGE] = Execute_UI_CLOSE_MIXING_FORGE;
	m_UIMessageFunction[UI_USE_MIXING_FORGE] = Execute_UI_MIXING_FORGE;

	m_UIMessageFunction[UI_CLOSE_REMOVE_OPTION] = Execute_UI_CLOSE_REMOVE_OPTION;
	m_UIMessageFunction[UI_SEND_REMOVE_OPTION] = Execute_UI_SEND_REMOVE_OPTION;

	m_UIMessageFunction[UI_CLOSE_OUSTERS_SKILL_INFO] = Execute_UI_CLOSE_OUSTERS_SKILL_INFO;
	m_UIMessageFunction[UI_OUSTERS_LEARN_SKILL] = Execute_UI_LEARN_OUSTERS_SKILL;

	m_UIMessageFunction[UI_RUN_LEVELUP] = Execute_UI_RUN_LEVELUP;
	m_UIMessageFunction[UI_CLOSE_HORN] = Execute_UI_CLOSE_HORN;
	m_UIMessageFunction[UI_OUSTERS_DOWN_SKILL] = Execute_UI_OUSTERS_DOWN_SKILL;	
	
	m_UIMessageFunction[UI_CLEAR_ALL_STAGE] = Execute_UI_CLEAR_ALL_STAGE;
	m_UIMessageFunction[UI_CLOSE_FINDING_MINE] = Execute_UI_CLOSE_FINDING_MINE;
	m_UIMessageFunction[UI_CLOSE_NEMONEMO] = Execute_UI_CLOSE_NEMONEMO;
	m_UIMessageFunction[UI_CLOSE_PUSHPUSH] = Execute_UI_CLOSE_PUSHPUSH;
	m_UIMessageFunction[UI_CLOSE_CRAZY_MINE] = Execute_UI_CLOSE_CRAZY_MINE;
	m_UIMessageFunction[UI_CLOSE_ARROW_TILE] = Execute_UI_CLOSE_ARROW_TILE;
	m_UIMessageFunction[UI_CLEAR_STAGE] = Execute_UI_CLEAR_STAGE;
	m_UIMessageFunction[UI_REQUEST_FORCE_DIE] = Execute_UI_FORCE_DIE;
	m_UIMessageFunction[UI_ADD_ITEM_TO_CODE_SHEET] = Execute_UI_ADD_ITEM_TO_CODE_SHEET;
	m_UIMessageFunction[UI_SEND_BUG_REPORT] = Execute_UI_SEND_BUG_REPORT;
	m_UIMessageFunction[UI_GO_TO_BEGINNER_ZONE] = Execute_UI_GO_BEGINNER_ZONE;
	m_UIMessageFunction[UI_POPUP_MESSAGE_OK] = Execute_UI_POPUP_MESSAGE_OK;

	m_UIMessageFunction[UI_CLOSE_SHRINE_MINIMAP] = Execute_UI_CLOSE_SHRINE_MINIMAP;
	m_UIMessageFunction[UI_WARP_REGEN_TOWER] = Execute_UI_WARP_TO_REGEN_TOWER;
	m_UIMessageFunction[UI_CLOSE_MAILBOX] = Execute_UI_CLOSE_MAILBOX;
	m_UIMessageFunction[UI_CLOSE_PET_INFO] = Execute_UI_CLOSE_PET_INFO;
	m_UIMessageFunction[UI_PET_GAMBLE] = Execute_UI_PET_GAMBLE;
	m_UIMessageFunction[UI_CLOSE_USE_PET_FOOD] = Execute_UI_CLOSE_USE_PET_FOOD;

	// 2004, 5, 11 sobeit add start - pet storage
	m_UIMessageFunction[UI_CLOSE_PETSTORAGE] = Execute_UI_CLOSE_PETSTORAGE;
	m_UIMessageFunction[UI_CLOSE_KEEP_PETITEM] = Execute_UI_CLOSE_KEEP_PETITEM;
	m_UIMessageFunction[UI_CLOSE_GET_KEEP_PETITEM] = Execute_UI_CLOSE_GET_KEEP_PETITEM;
	m_UIMessageFunction[UI_KEEP_PETITEM] = Execute_UI_KEEP_PETITEM;
	m_UIMessageFunction[UI_GET_KEEP_PETITEM] = Execute_UI_GET_KEEP_PETITEM;
	// 2004, 5, 11 sobeit add end - pet storage

	// 2004, 5, 28 sobeit add start - sms related
	m_UIMessageFunction[UI_CLOSE_SMS_MESSAGE] = Execute_UI_CLOSE_SMS_MESSAGE;
	m_UIMessageFunction[UI_SEND_SMS_MESSAGE] = Execute_UI_SEND_SMS_MESSAGE;
	m_UIMessageFunction[UI_SMS_OPEN_LIST] = Execute_UI_SMS_OPEN_LIST;
	m_UIMessageFunction[UI_SMS_RECORD] = Execute_UI_SMS_RECORD;
	m_UIMessageFunction[UI_SMS_DELETE] = Execute_UI_SMS_DELETE;
	m_UIMessageFunction[UI_CLOSE_SMS_LIST] = Execute_UI_CLOSE_SMS_LIST;
	m_UIMessageFunction[UI_CLOSE_SMS_RECORD] = Execute_UI_CLOSE_SMS_RECORD;
	m_UIMessageFunction[UI_SMS_ADD_SEND_LIST] = Execute_UI_SMS_ADD_SEND_LIST;
	// 2004, 5, 28 sobeit add end

	// 2004, 6, 15 sobeit add start - naming related
	m_UIMessageFunction[UI_CLOSE_NAMING] = Execute_UI_CLOSE_NAMING;
	m_UIMessageFunction[UI_CHANGE_CUSTOM_NAMING] = Execute_UI_CHANGE_CUSTOM_NAMING;
	m_UIMessageFunction[UI_SELECT_NAMING] = Execute_UI_SELECT_NAMING;
	m_UIMessageFunction[UI_CLOSE_NAMING_CHANGE] = Execute_UI_CLOSE_NAMING_CHANGE;
	m_UIMessageFunction[UI_RUN_NAMING_CHANGE] = Execute_UI_RUN_NAMING_CHANGE;
	// 2004, 6, 15 sobeit add end

	// 2004, 7, 12 sobeit add start - quest related
	m_UIMessageFunction[UI_CLOSE_QUEST_MANAGER] = Execute_UI_CLOSE_QUEST_MANAGER;
	m_UIMessageFunction[UI_CLOSE_QUEST_LIST] = Execute_UI_CLOSE_QUEST_LIST;
	m_UIMessageFunction[UI_CLOSE_QUEST_DETAIL] = Execute_UI_CLOSE_QUEST_DETAIL;
	m_UIMessageFunction[UI_CLOSE_QUEST_MISSION] = Execute_UI_CLOSE_QUEST_MISSION;
	m_UIMessageFunction[UI_CLOSE_QUEST_ITEM] = Execute_UI_CLOSE_QUEST_ITEM;
	m_UIMessageFunction[UI_CLOSE_QUEST_ICON] = Execute_UI_CLOSE_QUEST_ICON;
	m_UIMessageFunction[UI_GQUEST_ACCEPT] = Execute_UI_GQUEST_ACCEPT;
	m_UIMessageFunction[UI_GQUEST_GIVEUP] = Execute_UI_GQUEST_GIVEUP;

	// 2004, 7, 12 sobeit add end - quest related

	m_UIMessageFunction[UI_ITEM_USE_GQUEST_ITEM] = Execute_UI_ITEM_USE_GQUEST_ITEM;

	// 2004,10.9 csm guild related
	m_UIMessageFunction[UI_REQUEST_UNION] =	 Execute_UI_ITEM_USE_REQUEST_UNION;
	m_UIMessageFunction[UI_REQUEST_UNION_QUIT] =	 Execute_UI_ITEM_USE_QUIT;
	m_UIMessageFunction[UI_REQUEST_UNION_EXPERGUILD] =	 Execute_UI_ITEM_USE_EXPER;
	m_UIMessageFunction[UI_REQUEST_UNION_REQUEST_INFO] =   Execute_UI_ITEM_USE_UNION_INFO;
	m_UIMessageFunction[UI_REQUEST_UNION_REQUEST_GUILD_LIST] =		Execute_UI_ITEM_USE_REQUEST_GUILD_LIST;
	m_UIMessageFunction[UI_REQUEST_UNION_ACCEPT] =		Execute_UI_ITEM_USE_UNION_ACCEPT;
	m_UIMessageFunction[UI_REQUEST_UNION_DENY] =	Execute_UI_ITEM_USE_UNION_DENY;
	m_UIMessageFunction[UI_REQUEST_UNION_QUIT_ACCEPT] =		Execute_UI_ITEM_USE_UNION_QUIT_ACCEPT;
	m_UIMessageFunction[UI_REQUEST_UNION_QUIT_DENY] =	Execute_UI_ITEM_USE_UNION_QUIT_DENY;

	m_UIMessageFunction[UI_RECALL_BY_NAME] =	 Execute_UI_RECALL_BY_NAME;

	m_UIMessageFunction[UI_MODIFY_TAX] =	 Execute_UI_UI_MODIFY_TAX;

	m_UIMessageFunction[UI_APPOINT_SUBMASTER] =	 Execute_UI_APPOINT_SUBMASTER;


	m_UIMessageFunction[UI_DISPLAY_ITEM] =	             Execute_UI_DISPLAY_ITEM;        
    m_UIMessageFunction[UI_UNDISPLAY_ITEM] =	         Execute_UI_UNDISPLAY_ITEM;
    m_UIMessageFunction[UI_STORE_SIGN] =	             Execute_UI_STORE_SIGN;
    m_UIMessageFunction[UI_STORE_OPEN] =				 Execute_UI_STORE_OPEN;
    m_UIMessageFunction[UI_STORE_CLOSE] =				 Execute_UI_STORE_CLOSE;
	m_UIMessageFunction[UI_REQUEST_STORE_INFO] =		 Execute_UI_REQUEST_STORE_INFO;
	m_UIMessageFunction[UI_MY_STORE_INFO] =				 Execute_UI_MY_STORE_INFO;
    m_UIMessageFunction[UI_OTHER_STORE_INFO] =			Execute_UI_OTHER_STORE_INFO;
    m_UIMessageFunction[UI_BUY_STORE_ITEM] =			Execute_UI_BUY_STORE_ITEM;
    m_UIMessageFunction[UI_REMOVE_STORE_ITEM] =			Execute_UI_REMOVE_STORE_ITEM;
    m_UIMessageFunction[UI_ADD_STORE_ITEM] =			Execute_UI_ADD_STORE_ITEM;
	m_UIMessageFunction[UI_CLOSE_PERSNALSHOP] =			Execute_UI_CLOSE_PERSNALSHOP;
	
	m_UIMessageFunction[UI_CLOSE_POWER_JJANG]			=	 Execute_UI_CLOSE_POWER_JJANG;
	m_UIMessageFunction[UI_REQUEST_POWER_JJANG_POINT]	=	 Execute_UI_REQUEST_POWER_JJANG_POINT;
	m_UIMessageFunction[UI_POWER_JJANG_GAMBLE]			=	 Execute_UI_POWER_JJANG_GAMBLE;

	m_UIMessageFunction[UI_CLOSE_SWAPADVANCEMENTITEM]			=	 Execute_UI_CLOSE_SWAPADVANCEMENTITEM;
	m_UIMessageFunction[UI_SWAPADVANCEMENTITEM]			=	 Execute_UI_SWAPADVANCEMENTITEM;

	m_UIMessageFunction[UI_LEARN_ADVANCE_SKILL]			=	 Execute_UI_LEARN_ADVANCE_SKILL;
	
	
	m_UIMessageFunction[UI_CAMPAIGN_HELP] =	 Execute_UI_CAMPAIGN_HELP;

	m_UIMessageFunction[UI_RUN_NEXT_GQUEST_EXCUTE_ELEMENT] =	 Execute_UI_RUN_NEXT_GQUEST_EXCUTE_ELEMENT;

	m_UIMessageFunction[UI_GQUEST_SET_ACTION] =	 Execute_UI_GQUEST_SET_ACTION;
	m_UIMessageFunction[UI_GQUEST_ENDING_EVENT] =	 Execute_UI_GQUEST_ENDING_EVENT;

	m_UIMessageFunction[UI_REQUEST_EVENT_ITEM] =	 Execute_UI_REQUEST_EVENT_ITEM;

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
		m_UIMessageFunction[UI_CLOSE_INVENTORY_SUB]			=	 Execute_UI_CLOSE_INVENTORY_SUB;
		m_UIMessageFunction[UI_ITEM_DROP_TO_INVENTORY_SUB]		=	 Execute_UI_ITEM_DROP_TO_INVENTORY_SUB;
		m_UIMessageFunction[UI_ITEM_PICKUP_FROM_INVENTORY_SUB]	=	 Execute_UI_ITEM_PICKUP_FROM_INVENTORY_SUB;

		m_UIMessageFunction[UI_ITEM_USE_SUBINVENTORY]	=	 Execute_UI_ITEM_USE_SUBINVENTORY;
	#endif
}

//-----------------------------------------------------------------------------
// Execute
//-----------------------------------------------------------------------------
// Call the appropriate handler function linked to the message.
//-----------------------------------------------------------------------------
void			
UIMessageManager::Execute(DWORD message, intptr_t left, intptr_t right, void* void_ptr)
{
	// The message indexes the handler table, in every mode.
	if (message >= MAX_UI_MESSAGE)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] UI message past the table. msg=%d", message);

		return;
	}

	// Basically, if MODE_GAME,
	// there are values that must not be NULL..
	if (g_Mode==MODE_GAME
		&& (g_pZone==NULL
			|| g_pPlayer==NULL
			|| g_pInventory==NULL
			|| (g_pPlayer->IsSlayer() && g_pSlayerGear==NULL)
			|| (g_pPlayer->IsVampire() && g_pVampireGear==NULL)
			|| (g_pPlayer->IsOusters() && g_pOustersGear==NULL)
			|| g_pTempInformation==NULL
			)
		)
	{
		DEBUG_ADD_ERR("[Error] UI message. game object pointer NULL");

		return;
	}
		
	if (m_UIMessageFunction[message]==NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] NO UI_MessageFunction. msg=%d", message);
	}
	else
	{
		(*m_UIMessageFunction[message])(left, right, void_ptr);

		DEBUG_ADD("[UI] Execute OK");
	}
}


//-----------------------------------------------------------------------------
//
// Create a new character
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_NEW_CHARACTER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	DEBUG_ADD("UI_NEW_CHARACTER");
	if (g_Mode!=MODE_WAIT_SELECTPC)
	{
		DEBUG_ADD("Not Mode MODE_WAIT_SELECTPC");
		return;
	}

	//
	// ((C_VS_UI::NEW_CHARACTER *)void_ptr) = 
	//	char *	sz_name;
	//	bool		bl_female;
	//	int		STR;
	//	int		DEX;
	//	int		INT;
	//	int		slot;
	//	
	//	int		face;
	//	int		skin_color;
	//	int		hair_color;
	//	int		coat_color;
	//	int		trouser_color;
	//
	NEW_CHARACTER* pChar = (NEW_CHARACTER *)void_ptr;

	// The slot indexes the three characters, the face the three hair styles.
	if (pChar->slot < SLOT1 || pChar->slot >= SLOT_MAX
		|| pChar->face < HAIR_STYLE1 || pChar->face > HAIR_STYLE3)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] UI_NEW_CHARACTER slot=%d face=%d", pChar->slot, pChar->face);
		return;
	}
	
	BOOL bAllOK = TRUE;

	//---------------------------------------------
	// check name length
	//---------------------------------------------
	int len = static_cast<int>(strlen(pChar->sz_name));	
	
	if (static_cast<uint>(len)<PlayerInfo::minIDLength || static_cast<uint>(len)>PlayerInfo::maxIDLength)
	{
		char strTemp[128];
		SafeFormat::Format(strTemp, GetGameString(STRING_USER_REGISTER_ID_LENGTH), PlayerInfo::minIDLength, PlayerInfo::maxIDLength);
		g_pUIDialog->PopupFreeMessageDlg( strTemp );
		bAllOK = FALSE;
	}

	if (bAllOK)
	{
		if (!IsValidID(pChar->sz_name, "-_"))
		{
			//---------------------------------------------
			// if the ID is invalid
			//---------------------------------------------						
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
			bAllOK = FALSE;
		}
		else
		{
			char strName[80];
			SafeFormat::Copy(strName, pChar->sz_name);

			// if it contains bad words, it is not allowed
			if (g_pChatManager->RemoveCurse(strName))
			{
				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
				bAllOK = FALSE;
			}
			else
			{
				MChatManager badNameManager;
				badNameManager.LoadFromFile( g_pFileDef->getProperty("FILE_INFO_CHAT_2").c_str());

				if (badNameManager.RemoveCurse(strName))
				{
					g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
					bAllOK = FALSE;
				}
			}
		}
	}

	//---------------------------------------------
	// If all the data is valid...
	//---------------------------------------------
	if (bAllOK)
	{

			//char wansungName[16];

			//---------------------------------------------
			// Johab (combining) --> Wansung (precomposed)
			//---------------------------------------------
			//UI_JohapToWansung( pChar->sz_name, wansungName );

			CLCreatePC	_CLCreatePC;
			_CLCreatePC.setName ( pChar->sz_name );//wansungName );
			
			g_pUserInformation->Character[pChar->slot] = pChar->sz_name;
			//g_pUserInformation->CharacterW[pChar->slot] = wansungName;

			HairStyle hairStyle[3] =
			{
				HAIR_STYLE1,
				HAIR_STYLE2,
				HAIR_STYLE3
			};

			// check color
			pChar->hair_color = max(0, min(pChar->hair_color, MAX_COLORSET-1));
			pChar->skin_color = max(0, min(pChar->skin_color, MAX_COLORSET-1));
			
			_CLCreatePC.setSlot ( pChar->slot==0? SLOT1 : pChar->slot==1? SLOT2 : SLOT3 );
			_CLCreatePC.setRace ( pChar->race );
			_CLCreatePC.setSex ( pChar->bl_female? FEMALE : MALE );
			_CLCreatePC.setHairStyle ( hairStyle[pChar->face] );
			_CLCreatePC.setHairColor ( pChar->hair_color );					
			_CLCreatePC.setSkinColor ( pChar->skin_color );
			_CLCreatePC.setShirtColor ( pChar->coat_color, MAIN_COLOR );
			_CLCreatePC.setShirtColor ( pChar->coat_color, SUB_COLOR );
			_CLCreatePC.setJeansColor ( pChar->trouser_color, MAIN_COLOR );
			_CLCreatePC.setJeansColor ( pChar->trouser_color, SUB_COLOR );
			_CLCreatePC.setSTR( pChar->STR );
			_CLCreatePC.setDEX( pChar->DEX );
			_CLCreatePC.setINT( pChar->INT );

			g_pSocket->sendPacket( &_CLCreatePC );

			g_pSocket->setPlayerStatus( CPS_AFTER_SENDING_CL_CREATE_PC );	

			

		UIMessages::SetMode(MODE_WAIT_CREATEPCOK);
	}
}


//-----------------------------------------------------------------------------
//
// RegisterNewUser
//
// Registers a new account with the login server using the ID/password typed
// into the login window. The original design had a full registration form
// (C_VS_UI_NEWUSER: name, national ID, address, ...) that never shipped, so
// every field the CLRegisterPlayer wire format still requires is filled with a
// placeholder; the server only acts on the ID and password.
//
// Mirrors Execute_UI_LOGIN: version check, encryption key, then the request.
// Takes ownership of the two strings in *pLogin.
//
//-----------------------------------------------------------------------------
static void
RegisterNewUser(LOGIN* pLogin)
{
	if (pLogin == NULL)
	{
		DEBUG_ADD("[RegisterNewUser] no LOGIN");
		return;
	}

	bool bOK = false;

	// A string missing: the other is still freed below.
	if (pLogin->sz_id == NULL || pLogin->sz_password == NULL)
	{
		DEBUG_ADD("[RegisterNewUser] no ID/password");
	}
	else if (g_Mode != MODE_MAINMENU)
	{
		DEBUG_ADD("[RegisterNewUser] Not Mode MODE_MAINMENU");
	}
	else
	{
		const int idLength = static_cast<int>(strlen(pLogin->sz_id));
		const int passwordLength = static_cast<int>(strlen(pLogin->sz_password));
		char strTemp[128];

		DEBUG_ADD("[RegisterNewUser] registration requested");

		//--------------------------------------------------
		// ID: length, then ASCII letters/digits/underscore only.
		// IsValidID() is not used here: it also forbids a leading
		// digit, which every existing account in the shipped DB has.
		//--------------------------------------------------
		bool bIDCharsOK = true;
		for (const char* p = pLogin->sz_id; *p != '\0'; ++p)
		{
			const char ch = *p;
			if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_'))
			{
				bIDCharsOK = false;
				break;
			}
		}

		if (idLength < (int)PlayerInfo::minIDLength || idLength > (int)PlayerInfo::maxIDLength)
		{
			DEBUG_ADD("[RegisterNewUser] rejected: ID length");
			SafeFormat::Format(strTemp, "The ID must be %d to %d characters.", PlayerInfo::minIDLength, PlayerInfo::maxIDLength);
			g_ShowMessage( strTemp );
		}
		else if (!bIDCharsOK)
		{
			DEBUG_ADD("[RegisterNewUser] rejected: ID characters");
			g_ShowMessage( "The ID may only contain letters, digits and '_'." );
		}
		//--------------------------------------------------
		// Password: length, character set
		//--------------------------------------------------
		else if (passwordLength < (int)PlayerInfo::minPasswordLength || passwordLength > (int)PlayerInfo::maxPasswordLength)
		{
			DEBUG_ADD("[RegisterNewUser] rejected: password length");
			SafeFormat::Format(strTemp, "The password must be %d to %d characters.", PlayerInfo::minPasswordLength, PlayerInfo::maxPasswordLength);
			g_ShowMessage( strTemp );
		}
		else if (!IsValidPassword(pLogin->sz_password))
		{
			DEBUG_ADD("[RegisterNewUser] rejected: password characters");
			g_ShowMessage( "Do not use special characters in the password." );
		}
		else
		{
			bOK = true;
		}
	}

	if (bOK)
	{
		char strName[128];
		SafeFormat::Copy(strName, pLogin->sz_id);

		// An ID containing a curse word is not allowed.
		if (g_pChatManager->RemoveCurse(strName))
		{
			g_ShowMessage( "Invalid ID." );
			bOK = false;
		}
	}

	if (bOK && !UIMessages::InitSocket())
	{
		DEBUG_ADD_ERR("[RegisterNewUser] Can't init Socket");
		bOK = false;
	}

	if (bOK)
	{
		//--------------------------------------------------
		// CLVersionCheck - same guard as Execute_UI_LOGIN
		//--------------------------------------------------
		#if !defined(_DEBUG) && !defined(OUTPUT_DEBUG)
			CLVersionCheck _CLVersionCheck;
			_CLVersionCheck.setVersion( g_pUserInformation->GameVersion );
			g_pSocket->sendPacket( &_CLVersionCheck );
		#endif

		//--------------------------------------------------
		// Encryption key - the login server expects it before
		// anything else on a fresh connection.
		//--------------------------------------------------
		CGConnectSetKey cgConnectSetKey;
		cgConnectSetKey.setEncryptKey(rand());
		cgConnectSetKey.setHashKey(rand());
		g_pSocket->sendPacket(&cgConnectSetKey);
		LoginPresentation::FlushSocketOutput();
		// Registered as an explicit no-op (see PacketHandlerRegistry.cpp) -
		// the linked handler was always the empty stub.
		PacketDispatcher::dispatch(&cgConnectSetKey, g_pSocket);
		Sleep(500);

		//--------------------------------------------------
		// CLRegisterPlayer
		//--------------------------------------------------
		CLRegisterPlayer _CLRegisterPlayer;

		_CLRegisterPlayer.setID( pLogin->sz_id );
		_CLRegisterPlayer.setPassword( pLogin->sz_password );

		// The wire format rejects empty strings, so every profile field
		// the client no longer collects carries a placeholder.
		_CLRegisterPlayer.setName( pLogin->sz_id );
		_CLRegisterPlayer.setSex( MALE );
		_CLRegisterPlayer.setSSN( "000000-0000000" );
		_CLRegisterPlayer.setTelephone( "NULL" );
		_CLRegisterPlayer.setCellular( "NULL" );
		_CLRegisterPlayer.setZipCode( "NULL" );
		_CLRegisterPlayer.setAddress( "NULL" );
		_CLRegisterPlayer.setNation( KOREA );
		_CLRegisterPlayer.setEmail( "NULL" );
		_CLRegisterPlayer.setHomepage( "NULL" );
		_CLRegisterPlayer.setProfile( "NULL" );
		_CLRegisterPlayer.setPublic( false );

		g_pSocket->setPlayerStatus( CPS_AFTER_SENDING_CL_REGISTER_PLAYER );
		g_pSocket->sendPacket( &_CLRegisterPlayer );
		DEBUG_ADD("[RegisterNewUser] CLRegisterPlayer sent");

		// Remember the ID; LCRegisterPlayerOK backs it up as the login ID.
		g_pUserInformation->UserID = pLogin->sz_id;

		gC_vs_ui.ClearAllCharacter();

		UIMessages::SetMode( MODE_WAIT_REGISTERPLAYEROK );
	}

	// C_VS_UI_LOGIN::SendNewUserToClient() malloc()s these.
	free(pLogin->sz_id);
	free(pLogin->sz_password);
	pLogin->sz_id = NULL;
	pLogin->sz_password = NULL;
}

//-----------------------------------------------------------------------------
//
// Run NewUser Registration
//
// void_ptr is a LOGIN* from C_VS_UI_LOGIN::SendNewUserToClient() (may be NULL
// when the message is raised without input).
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_RUN_NEWUSER_REGISTRATION(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	DEBUG_ADD("[UI] UI_RUN_NEWUSER_REGISTRATION");

	LOGIN* pLogin = (LOGIN*)void_ptr;

	switch (g_pClientConfig->NEW_USER_REGISTERATION_MODE)
	{
		//-----------------------------------------------------------
		// CLIENT - register from inside the client
		//-----------------------------------------------------------
		case ClientConfig::NUR_CLIENT :
		{
			RegisterNewUser(pLogin);
			return;
		}

		//-----------------------------------------------------------
		// HOMEPAGE - open the registration web page and quit
		//-----------------------------------------------------------
		case ClientConfig::NUR_HOMEPAGE :
		{
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_HOMEPAGE].GetString() );			

			// Quit..
			UIMessages::SetMode( MODE_QUIT );

#ifdef PLATFORM_WINDOWS
			char str[256];

			if (const UINT length = GetWindowsDirectoryA(str, static_cast<UINT>(sizeof(str)));
				length == 0 || length > sizeof(str) - sizeof("\\Explorer.exe")) break;
			SafeFormat::Append(str, "\\Explorer.exe");

			// CSDLGraphics::GetDD() always returns nullptr (stub - see
			// reference-docs/work-needed stub.md 1-1), and IDirectDraw is only
			// forward-declared, so ->RestoreDisplayMode() never compiled.
			// Same category as the WinMain.cpp/VS_UI_Title.cpp call sites
			// already disabled there; just missed here.
#ifdef __YHDK2__
			_spawnl(_P_NOWAIT, str, "Explorer.exe", "http://www.yhdk2.cn", NULL);
#else
			_spawnl(_P_NOWAIT, str, "Explorer.exe", "http://www.ttdk2.com", NULL);
#endif
#endif

			//_spawnl(_P_NOWAIT, str, "Explorer.exe", "http://www.ttdk2.cn", NULL);
		}
		break;

		//-----------------------------------------------------------
		// MESSAGE_HOMEPAGE - a message telling the user to register on the homepage
		//-----------------------------------------------------------
		case ClientConfig::NUR_MESSAGE_HOMEPAGE :	
		{
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_HOMEPAGE].GetString() );			
		}
		break;
		
		//-----------------------------------------------------------
		// DENY - new user registration not possible
		//-----------------------------------------------------------
		case ClientConfig::NUR_DENY :				
		{
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_DENY].GetString() );
		}
		break;
	}

	// Not registering from the client: release the typed strings unused.
	if (pLogin != NULL)
	{
		free(pLogin->sz_id);
		free(pLogin->sz_password);
		pLogin->sz_id = NULL;
		pLogin->sz_password = NULL;
	}
}

//-----------------------------------------------------------------------------
//
// Char Info Close
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_INFO_CLOSE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_INFO_CLOSE");
	
	if (g_Mode!=MODE_WAIT_SELECTPC
		&& g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_WAIT_SELECTPC");
		return;
	}
	
	gC_vs_ui.CloseInfo();
}

//-----------------------------------------------------------------------------
//
// Check Exist ID
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CHECK_EXIST_ID(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	//
	// left = id
	//
	DEBUG_ADD("[UI] UI_CHECK_EXIST_ID");
	
	const char* pName = (const char*)void_ptr;

	//--------------------------------------------------
	// ID check
	//--------------------------------------------------
	if (IsValidID(pName, NULL))
	{
		int len = static_cast<int>(strlen(pName));	
		
		if (static_cast<uint>(len)<PlayerInfo::minIDLength || static_cast<uint>(len)>PlayerInfo::maxIDLength)
		{
			char strTemp[128];
			SafeFormat::Format(strTemp, "ID는 %d~%d자입니다", PlayerInfo::minIDLength, PlayerInfo::maxIDLength);
			g_pUIDialog->PopupFreeMessageDlg( strTemp );						
		}
		else
		{
			//--------------------------------------------------
			// Initialise the socket
			//--------------------------------------------------
			if (!UIMessages::InitSocket())
			{						
				//InitFail("[Error] Can't init Socket");
				DeleteNewArray( pName );
				return;
			}

				CLQueryPlayerID _CLQueryPlayerID;
				//_CLDeletePC.setName( g_pUserInformation->CharacterW[pChar->slot].GetString() );
				_CLQueryPlayerID.setPlayerID( pName );

				g_pSocket->sendPacket( &_CLQueryPlayerID );

				g_pSocket->setPlayerStatus( CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID );


			//gC_vs_ui.AleadyExistIdMessage(); // not usable
			//gC_vs_ui.NoAleadyExistIdMessage(); // usable
		}
	}
	else
	{
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
	}

	// Not sure whether it is right to delete this.. hmm *_*;
	DeleteNewArray( pName );
}


//-----------------------------------------------------------------------------
//
//		Delete character
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_DELETE_CHARACTER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	DEBUG_ADD("[UI] UI_DELETE_CHARACTER");
	
	if (g_Mode!=MODE_WAIT_SELECTPC)
	{
		DEBUG_ADD("Not Mode MODE_WAIT_SELECTPC");
		return;
	}
	
	DELETE_CHARACTER *pChar = (DELETE_CHARACTER*)void_ptr;

	// The slot indexes the three characters. An empty one has nothing to
	// delete: its name is NULL, and the packet cannot carry an empty name.
	if (pChar->slot < SLOT1 || pChar->slot >= SLOT_MAX
		|| g_pUserInformation->Character[pChar->slot].GetString() == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] UI_DELETE_CHARACTER slot=%d", pChar->slot);
		return;
	}

	// The delete dialog already confirmed the request by making the user type
	// the character's name. The SSN field the packet still carries is
	// vestigial: the server's check has been retired, so send a placeholder.
	CLDeletePC _CLDeletePC;
	_CLDeletePC.setName( g_pUserInformation->Character[pChar->slot].GetString() );
	_CLDeletePC.setSlot( pChar->slot==0? SLOT1 : pChar->slot==1? SLOT2 : SLOT3 );
	_CLDeletePC.setSSN( "1" );

	g_pSocket->sendPacket( &_CLDeletePC );

	g_pSocket->setPlayerStatus( CPS_AFTER_SENDING_CL_DELETE_PC );

	//--------------------------------------------
	// Remember which slot is being deleted.
	//--------------------------------------------
	g_pUserInformation->Slot = pChar->slot;

	UIMessages::SetMode( MODE_WAIT_DELETEPCOK );
}

	
//-----------------------------------------------------------------------------
//
//		Exit program
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_TERMINATION(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_TERMINATION");
	
	if (g_Mode!=MODE_MAINMENU)		
	{
		DEBUG_ADD("Not Mode MODE_MAINMENU");
		return;
	}

	UIMessages::SetMode( MODE_QUIT );
}

//-----------------------------------------------------------------------------
//
// New user registration
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// Login
//
//-----------------------------------------------------------------------------
extern BYTE g_macAddress[6];

void
UIMessageManager::Execute_UI_LOGIN(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	DEBUG_ADD("[UI] UI_LOGIN");

	g_dwSeqNumL = rand()%1024;
	g_dwSeqNumG = 0;

	UI_SaveUserOption();

	//
	// ((C_VS_UI::LOGIN *)void_ptr) = id, password
	//
	// ! Must not store the string ptr.
	//
	LOGIN*	login = (LOGIN*)void_ptr;

	if (g_Mode!=MODE_MAINMENU)
	{
		DeleteNewArray(login->sz_id);
		DeleteNewArray(login->sz_password);

		DEBUG_ADD("Not Mode MODE_MAINMENU");
		return;
	}	

	// Load part of the slayer pictures - 2001.8.20
	UIMessages::LoadingAddonSPK( false );

	if (login->sz_id!=NULL && login->sz_password!=NULL)
	{
		// CLLogin::write throws for an empty ID or one past 30 (2048 in
		// Netmarble's layout) and, outside that layout, for an empty
		// password or one past 20: refuse them before anything is sent.
		const bool bNetmarble = g_pUserInformation->IsNetmarble;
		const size_t idLength = strlen(login->sz_id);
		const size_t passwordLength = strlen(login->sz_password);

		if (idLength > 0 && idLength <= (bNetmarble ? 2048u : 30u))
		{
			// remember ID
			//UI_BackupLoginID(((LOGIN *)void_ptr)->sz_id);
			
			if (IsValidPassword(login->sz_password)
				&& (bNetmarble || (passwordLength > 0 && passwordLength <= 20)))
			{
				{
					DEBUG_ADD("[Execute_UI_LOGIN]");
					// Everything below sends through g_pSocket.
					if (!UIMessages::InitSocket() || g_pSocket == NULL)
					{
						//InitFail("[Error] Can't init Socket");
						DEBUG_ADD_ERR("[Execute_UI_LOGIN] Can't init Socket");
						DeleteNewArray(login->sz_id);
						DeleteNewArray(login->sz_password);
						DEBUG_ADD_ERR("[Execute_UI_LOGIN] Can't init Socket2");
						return;
					} 

					//char wansungID[16];

					//---------------------------------------------
					// Johab (combining) --> Wansung (precomposed)
					//---------------------------------------------
					//UI_JohapToWansung( login->sz_id, wansungID );
	
					//--------------------------------------------------
					// After connecting to the login server,
					// the first packet to send is the CLLogin packet.
					// But,
					// the Version check packet comes first. - -;
					//--------------------------------------------------
					//--------------------------------------------------
					// CLVersionCheck
					//--------------------------------------------------
					#if !defined(_DEBUG) && !defined(OUTPUT_DEBUG)
						int version = g_pUserInformation->GameVersion;
//						std::ifstream versionFile;//(FILE_INFO_ACTION, ios::binary);
//						if (!FileOpenBinary(FILE_INFO_VERSION, versionFile))
//						{
//							DeleteNewArray(login->sz_id);
//							DeleteNewArray(login->sz_password);
//							return;
//						}
//						versionFile.read((char*)&version, 4);
//						versionFile.close();

						CLVersionCheck _CLVersionCheck;
						_CLVersionCheck.setVersion( version );

						g_pSocket->sendPacket( &_CLVersionCheck );

						
					#endif
					//add by viva
					CGConnectSetKey cgConnectSetKey;
					cgConnectSetKey.setEncryptKey(rand());
					cgConnectSetKey.setHashKey(rand());
					g_pSocket->sendPacket(&cgConnectSetKey);
					LoginPresentation::FlushSocketOutput();
					// See PacketHandlerRegistry.cpp: registered explicit no-op.
					PacketDispatcher::dispatch(&cgConnectSetKey, g_pSocket);
					Sleep(500);
					//--------------------------------------------------
					// CLLogin
					//--------------------------------------------------
					CLLogin clLogin;
					// The packet no longer reads g_pUserInformation itself
					// (it compiles into packetwire); same null-guarded
					// condition its write() used to evaluate.
					clLogin.setNetmarble( g_pUserInformation != NULL && g_pUserInformation->IsNetmarble );

					//clLogin.setID("Reiot");
					//clLogin.setPassword("fpdldhxm");

					// for Netmarble
					std::string temp_id;
					DEBUG_ADD("[Execute_UI_LOGIN] #");
					#ifdef __METROTECH_TEST__
						temp_id = "#";
					#endif
					
					//if(g_pUserInformation->IsNetmarble)
					//	temp_id += "@";
					
					temp_id += login->sz_id;
										
					clLogin.setID( temp_id );//wansungID);
					clLogin.setPassword(login->sz_password);			

					// 2004,03,16 sobeit add start - mac address
//					BYTE MacAddress[6];
//					memset(MacAddress, 0,sizeof(BYTE)*6);
//					memcpy(MacAddress, g_macAddress, sizeof(BYTE)*6);
//					GetMacAddressFromNetBIOS(MacAddress);
					clLogin.setMacAddress(g_macAddress);
					// 2004,03,16 sobeit add end - mac address
					clLogin.SetLoginMode(g_pUserInformation->IsAutoLogIn);

					g_pSocket->sendPacket( &clLogin );
					g_pSocket->setPlayerStatus( CPS_AFTER_SENDING_CL_LOGIN );
					DEBUG_ADD("[Execute_UI_LOGIN] SendPacket OK");

					//----------------------------------------------------
					// Store it in UserInformation.
					//----------------------------------------------------
					g_pUserInformation->UserID = login->sz_id;
					//g_pUserInformation->UserIDW = wansungID;
				}
				
				DEBUG_ADD("[Execute_UI_LOGIN] ClearAllCharacter");
				gC_vs_ui.ClearAllCharacter();
				DEBUG_ADD("[Execute_UI_LOGIN] ClearAllCharacter OK");
				if(false == g_pUserInformation->IsAutoLogIn) 
					UIMessages::SetMode( MODE_WAIT_LOGINOK );
				DEBUG_ADD("[Execute_UI_LOGIN] SetMode OK");
			}
			else
			{
				// if the password is wrong
				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_PASSWORD_SPECIAL].GetString() );
			}
		}
		//---------------------------------------------
		// if the ID is invalid
		//---------------------------------------------
		else
		{
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
		}
	}
	

	// (!) Originally the character selection window should be shown.

	//gC_vs_ui.StartGame();
	//gC_vs_ui.StartCharacterManager();

	DeleteNewArray(login->sz_id);
	DeleteNewArray(login->sz_password);
	DEBUG_ADD("[Execute_UI_LOGIN] OK");
}

//-----------------------------------------------------------------------------
//
// When returning from the character selection window to the main menu
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CHARACTER_MANAGER_FINISHED(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CHARACTER_MANAGER_FINISHED");

	if (g_Mode!=MODE_WAIT_SELECTPC)
	{
		DEBUG_ADD("g_Mode not MODE_WAIT_SELECTPC");

		return;
	}

	// for Netmarble
	if(g_pUserInformation->IsNetmarble)
	{
		UIMessages::SetMode( MODE_MAINMENU );
		return;
	}

	//
	// Send logout packet to Client and Server.
	//
	//--------------------------------------------------
	// Send a CLLogout packet to the Login server.
	//--------------------------------------------------
	/*
	#ifdef	CONNECT_SERVER		
		
		// save the hot keys.
		//UI_SaveHotKeyToServer();

		CLLogout clLogout;
	
		g_pSocket->sendPacket( &clLogout );
		//g_pSocket->setPlayerStatus( CPS_END_SESSION );
		//g_pSocket->disconnect();

		ReleaseSocket();
	#endif					


	UIMessages::SetMode( MODE_MAINMENU );
	*/

	UI_SetServerList();

	UIMessages::SetMode( MODE_WAIT_SELECT_SERVER );
}

//-----------------------------------------------------------------------------
//
// UI_CONNECT - when a character was selected
//
//-----------------------------------------------------------------------------
// When about to start the game...
void
UIMessageManager::Execute_UI_CONNECT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CONNECT");
	
	if (g_Mode!=MODE_WAIT_SELECTPC)
	{
		DEBUG_ADD("Not MODE_WAIT_SELECTPC");
		return;
	}

	// left indexes the three characters. An empty slot has no one to
	// choose: its name is NULL, and the packet cannot carry an empty name.
	if (left < SLOT1 || left >= SLOT_MAX
		|| g_pUserInformation->Character[left].GetString() == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] UI_CONNECT slot=%d", static_cast<int>(left));
		return;
	}

	//------------------------------------------------------------
	// stop music - 2001.8.20
	//------------------------------------------------------------
	if (g_pUserOption!=NULL)
	{
		if (g_pUserOption->PlayWaveMusic)
		{
//			if (g_pSDLStream!=NULL)
//			{
//				g_pSDLStream->Stop();
//			}
			if (g_pOGG != NULL)
			{
				DEBUG_ADD("MP3 STOP13");
				if( g_SDLAudio.IsInit() )
					g_pOGG->streamClose();
				DEBUG_ADD("MP3 STOP13 OK");
			}
		}
		else
		{
			g_Music.Stop();
		}
	}

	//------------------------------------------------------------
	// Print that it needs to connect.
	//------------------------------------------------------------
	if (g_pUIDialog!=NULL)
	{
		//g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_LOGOUT_DIED].GetString() );
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CONNECTING_SERVER].GetString(), -1, -1, 0 );

//		if (true)
//		{
//			DEBUG_ADD("UI show1");
//			gC_vs_ui.Show();
//			//gC_vs_ui.DrawMousePointer();		
//			DEBUG_ADD("UI show1 OK");
//		}
//		else
		{
			POINT point;
			
			DEBUG_ADD("UI show2");
			gC_vs_ui.Show();
			DEBUG_ADD("UI show2 OK");

			//gC_vs_ui.DrawMousePointer();

			//-----------------------------------------------------------------
			// Copy Last to Back - only when not 3D HAL..
			//-----------------------------------------------------------------		
			point.x = 0;
			point.y = 0;
			RECT rect = { 0, 0, g_GameRect.right, g_GameRect.bottom };
			g_pBack->BltNoColorkey( &point, g_pLast, &rect );	
		}
		CSDLGraphics::Flip();

		// Just draw it on the screen and erase it.. lol
		DEBUG_ADD("close msg dlg");
		g_pUIDialog->CloseMessageDlg();
	}

	DEBUG_ADD("LoadingAddonSPK");

	//------------------------------------------------------------
	// Test 2001.8.20
	//------------------------------------------------------------
	// Right after the character is chosen
	// Load part of the slayer pictures - 2001.8.20
	UIMessages::LoadingAddonSPK( false );
	

	DEBUG_ADD_FORMAT("CLSelectPC(%d)", left);

	CLSelectPC clSelectPC;
	// To use it in CGConnect, it must be saved here.
	//clSelectPC.setPCName( (const char *)g_pUserInformation->CharacterW[left] );//"sigi");
	clSelectPC.setPCName( (const char *)g_pUserInformation->Character[left] );//"sigi");
	clSelectPC.setPCType( (PCType)(g_pUserInformation->Race[left]));

	DEBUG_ADD_FORMAT("socket Set PC type");

	g_pSocket->setPCType(clSelectPC.getPCType());
	g_pSocket->setPCName(clSelectPC.getPCName());

	DEBUG_ADD_FORMAT("user Information");

	g_pUserInformation->CharacterID = g_pUserInformation->Character[left];
	g_pUserInformation->FaceStyle = g_pUserInformation->FaceStyleSlot[left];	
	// An empty master name (a NULL string) is the prefix of no name.
	const MString& masterName = (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME];
	g_pUserInformation->IsMaster = 
		(masterName.GetLength() > 0 && strncmp( g_pUserInformation->CharacterID, masterName.GetString(), masterName.GetLength() ) == 0)
//		(strstr(g_pUserInformation->CharacterID, "운영자")!=NULL)
		|| g_pInventory->FindItem(ITEM_CLASS_ETC, 0);
//		|| g_pPlayer->GetCreatureType()==CREATURETYPE_SLAYER_OPERATOR
//		|| g_pPlayer->GetCreatureType()==CREATURETYPE_VAMPIRE_OPERATOR;
//		g_pPlayer does not exist so it crashes; I thought about checking...but nah~

	#ifdef OUTPUT_DEBUG
		DEBUG_ADD("MasterCheck");
		std::ifstream file("master.txt", );

		char str[256];
		while (!file.eof())
		{
			file.getline(str, 256);
			
			int n = file.gcount();

			if (n <= 0)
				break;
			
			str[n] = '\0';

			g_pUserInformation->IsMaster |= (strstr(g_pUserInformation->CharacterID, str)!=NULL);
		}
		
		file.close();
		DEBUG_ADD("MasterCheck OK");
	#endif
	
	//g_pUserInformation->CharacterIDW = g_pUserInformation->CharacterW[left];
	DEBUG_ADD("send CLSelectPC");
	g_pSocket->sendPacket( &clSelectPC );
	g_pSocket->setPlayerStatus( CPS_AFTER_SENDING_CL_SELECT_PC );

	// Send it at once.
	LoginPresentation::FlushSocketOutput();

	UIMessages::SaveLastSelectedCharacter( static_cast<int>(left) );

	DEBUG_ADD("set mode MODE_WAIT_RECONNECT");
	UIMessages::SetMode(MODE_WAIT_RECONNECT);
	//------------------------------
	// game UI
	//------------------------------
	//gC_vs_ui.StartGame();
					
	//
	// Skills must be added after StartGame() has run.
	//
	//gC_vs_ui.AddSlayerSkill(SKILL_FLASH_SLASHER);
}


static const size_t CHAT_MESSAGE_MAX_BYTES = CGSay::MAX_MESSAGE_SIZE;

//-----------------------------------------------------------------------------
//
// Truncate UTF-8 text to a byte budget, on a character boundary
//
//-----------------------------------------------------------------------------
static void
TruncateUtf8ToBytes(std::string& strText, size_t nMaxBytes)
{
	if (strText.size() <= nMaxBytes)
	{
		return;
	}

	// Back off over continuation bytes so the cut lands on a character.
	size_t nCut = nMaxBytes;

	while (nCut > 0 && ((unsigned char)strText[nCut] & 0xC0) == 0x80)
	{
		nCut--;
	}

	if (nCut == 0)
	{
		nCut = nMaxBytes;
	}

	strText.resize(nCut);
}

//-----------------------------------------------------------------------------
//
// When Enter is pressed in Chat
//
//-----------------------------------------------------------------------------
// chat window input
void
UIMessageManager::Execute_UI_CHAT_RETURN(intptr_t left, intptr_t right, void* void_ptr)
{
	//
	// If left = true it was entered from pcs, if false it is normal chat.
	//
	// ! To find the slot number when entered from pcs, run the following function.
	//   => gC_vs_ui.GetSendPossibleSlot();
	//      Valid numbers are 0, 1, 2; if NOT_SELECTED, it is no slot at all.
	//
	// void_ptr = input string
	//	right = color
	//

	DEBUG_ADD("[UI] UI_CHAT_RETURN");
	if (void_ptr == nullptr)
		return;

	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	
	DEBUG_ADD_FORMAT("[Original] %s", (char*)void_ptr );
	
	const char* chatString = static_cast<const char*>(void_ptr);
//	g_pChatManager->RemoveCurse(chatString);
	//-------------------------------------------------------------
	// if party chat
	//-------------------------------------------------------------
	if (chatString!=NULL 
		&& chatString[0]!=SYMBOL_WHISPER	// if it is not a whisper..
		//&& chatString[0]!=SYMBOL_MASTER_COMMAND
		&& left==CLD_PARTY)
	{
		if (g_pParty!=NULL )
		{
				std::string strPartySay( chatString );
				TruncateUtf8ToBytes( strPartySay, CHAT_MESSAGE_MAX_BYTES );

				CGPartySay _CGPartySay;
				_CGPartySay.setColor(static_cast<uint>(right));
				_CGPartySay.setMessage(strPartySay);
				g_pSocket->sendPacket( &_CGPartySay );

		}
	}
	// guild chat
	else if (chatString!=NULL 
		&& chatString[0]!=SYMBOL_WHISPER	// if it is not a whisper..
		&& chatString[0]!=SYMBOL_MASTER_COMMAND
		&& (left==CLD_GUILD || left == CLD_UNION))
	{
		// CGGuildChat::write refuses an empty message.
		if(g_pUserInformation->GuildName.GetLength() > 0 && chatString[0] != '\0')
		{
			CGGuildChat _CGGuildChat;
			// 2004, 11, 11, sobeit add start
			if(left==CLD_GUILD)
				_CGGuildChat.SetType(0);
			else
				_CGGuildChat.SetType(1);
			// 2004, 11, 11, sobeit add end
			std::string strGuildChat( chatString );
			TruncateUtf8ToBytes( strGuildChat, CHAT_MESSAGE_MAX_BYTES );

			_CGGuildChat.setMessage( strGuildChat );
			_CGGuildChat.setColor( static_cast<uint>(right) );
			
			g_pSocket->sendPacket( &_CGGuildChat );
		}
	}
	//-------------------------------------------------------------
	// Chatting is possible only when the Player is alive.. it used to be, but..   --;
	//-------------------------------------------------------------
	else //if (g_pPlayer->IsAlive())
	{
		BOOL bZoneChat = (left==CLD_ZONECHAT);
	
		/*	
		#ifdef __GAME_CLIENT__
			if (g_pSystemMessage!=NULL)
			{
				// Service.. - -;;
				g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_CHAT_BE_GOOD].GetString());
			}
		#endif
		*/

		//int pcsChat = left;

		//-------------------------------------------------------------
		//
		// PCS chat
		//
		//-------------------------------------------------------------
		//if (pcsChat)
		//{
		//}
		//-------------------------------------------------------------
		//
		// normal chat
		//
		//-------------------------------------------------------------
		//else
		{
			//
			// Send to the chat history Window... let's just send it right away for now.
			//
			//break;

			// send message to the server
			//g_Socket.Send(g_String);
			const char* strUI = chatString;

			std::string strChat( strUI );
			TruncateUtf8ToBytes( strChat, CHAT_MESSAGE_MAX_BYTES );

			char strOrg[CHAT_MESSAGE_MAX_BYTES + 1];
			char* str = strOrg;

			memset( strOrg, 0, sizeof(strOrg) );
			memcpy( strOrg, strChat.c_str(), strChat.size() );

			if (str!=NULL && str[0]!='\0')
			{
				RemoveStringSpace( str );							

				DEBUG_ADD_FORMAT("[After RemoveSpace] %s", str );
					
				if (str!=NULL && str[0]!='\0')
				{
					// 2004, 10, 25, sobeit add start - Dragon Eye related
					if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_DRAGON_EYES))
					{
						if(0 == strncmp(str, (*g_pGameStringTable)[UI_STRING_MESSAGE_RANGER_SAY].GetString(),(*g_pGameStringTable)[UI_STRING_MESSAGE_RANGER_SAY].GetLength()))
						{
							char TempBuffer[CHAT_MESSAGE_MAX_BYTES + 1];
							SafeFormat::Copy(TempBuffer, str+(*g_pGameStringTable)[UI_STRING_MESSAGE_RANGER_SAY].GetLength());
							// The prefix alone says nothing: CGRangerSay::write refuses an empty message.
							if (TempBuffer[0] == '\0')
							{
								return;
							}
							CGRangerSay _CGRangerSay;
							_CGRangerSay.setMessage(TempBuffer);

							g_pSocket->sendPacket( &_CGRangerSay );
										
							// add to the Player's Chat pWansungStringing
							g_pPlayer->SetChatString( str );//+1 );

							// add to history
							char temp[CHAT_MESSAGE_MAX_BYTES + 1];
							SafeFormat::Copy(temp, str );//+1);
							//sprintf(temp, "[%s] %s", g_pUserInformation->CharacterID.GetString(), str+1);
							//UI_AddChatToHistory( temp );
							UI_AddChatToHistory( temp, g_pUserInformation->CharacterID.GetString(), CLD_ZONECHAT, static_cast<DWORD>(right) );
							return;
						}
					}
					// 自动使用全局聊天
					//UI_STRING_MESSAGE_PLAYER_SAY
					if(0 == strncmp(str, (*g_pGameStringTable)[UI_STRING_MESSAGE_PLAYER_SAY].GetString(),(*g_pGameStringTable)[UI_STRING_MESSAGE_PLAYER_SAY].GetLength()))
					{
						// 查找物品
						// 查找绿色
						MItem* pItem = g_pInventory->FindItem(ITEM_CLASS_EFFECT_ITEM,10);
						if (pItem == NULL)
						{
							// 查找蓝色
							pItem = g_pInventory->FindItem(ITEM_CLASS_EFFECT_ITEM,11);
							if (pItem == NULL)
							{
								// 查找黄色
								pItem = g_pInventory->FindItem(ITEM_CLASS_EFFECT_ITEM,12);
							}
						}
						if (pItem!=NULL)
						{
							char TempBuffer[128];

							// The terminator goes at prefix+60, so the guard is on that index.
							const size_t nSayPrefix = (*g_pGameStringTable)[UI_STRING_MESSAGE_PLAYER_SAY].GetLength();

							if (strlen(str)>nSayPrefix+60)
							{
								str[nSayPrefix+60]='\0';
							}

							SafeFormat::Copy(TempBuffer, str+nSayPrefix);
							string msg="";
							msg = g_pUserInformation->CharacterID.GetString();
							msg +=">";
							msg +=TempBuffer;
							CGUseMessageItemFromInventory _CGUseMessageItemFromInventory;
							_CGUseMessageItemFromInventory.setObjectID( pItem->GetID() );
							_CGUseMessageItemFromInventory.setX( pItem->GetGridX() );
							_CGUseMessageItemFromInventory.setY( pItem->GetGridY() );
							_CGUseMessageItemFromInventory.setMessage( msg );
							g_pSocket->sendPacket( &_CGUseMessageItemFromInventory );
							g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
						}else
						{
							g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[UI_STRING_NO_ITEM_MESSAGE].GetString() );
						}
						return;
					}
					// 2004, 10, 25, sobeit add end - Dragon Eye related
					if (bZoneChat && str[0]!=SYMBOL_WHISPER
						&& str[0]!=SYMBOL_COMMAND && str[0]!=SYMBOL_MASTER_COMMAND)			
					{
						// system message test
						//------------------------------------------------------------
						//
						// If '!' is typed, it is global chat.
						//
						//------------------------------------------------------------
						//case SYMBOL_GLOBALCHAT :	//if (str[0]=='!')//pWansungString[0]=='/')
						{
							//if (strlen(str) > 1 )	//pWansungString) > 1)
							{		
								//---------------------------------------------------------
								// Cannot speak while hidden.
								// Cannot speak when a wolf or a bat.
								//---------------------------------------------------------
								if (g_pPlayer->IsUndergroundCreature()
									|| g_pPlayer->IsInCasket()
									|| g_pPlayer->GetCreatureType()==CREATURETYPE_BAT
									|| g_pPlayer->GetCreatureType()==CREATURETYPE_WOLF
									|| g_pPlayer->GetCreatureType()==CREATURETYPE_WER_WOLF)
								{
									// add to history
									//char temp[128];
									//sprintf(temp, "[%s] .....", g_pUserInformation->CharacterID.GetString());
									//UI_AddChatToHistory( temp );								
									UI_AddChatToHistory( ".....", g_pUserInformation->CharacterID.GetString(), CLD_NORMAL, static_cast<DWORD>(right) );
								}
								//---------------------------------------------------------
								// global say
								//---------------------------------------------------------
								else
								{
									//---------------------------------------------------------
									// Cannot speak if the delay has not finished.
									//---------------------------------------------------------
									if (1)
									{
										CGGlobalChat _CGGlobalChat;
										_CGGlobalChat.setMessage( str );//+ 1 );	//pWansungString+1 );
										_CGGlobalChat.setColor( static_cast<uint>(right) );
										g_pSocket->sendPacket( &_CGGlobalChat );

										
										// add to the Player's Chat pWansungStringing
										g_pPlayer->SetChatString( str, static_cast<COLORREF>(right) );//+1 );

										// add to history
										char temp[CHAT_MESSAGE_MAX_BYTES + 1];
										SafeFormat::Copy(temp, str );//+1);
										//sprintf(temp, "[%s] %s", g_pUserInformation->CharacterID.GetString(), str+1);
										//UI_AddChatToHistory( temp );
										UI_AddChatToHistory( temp, g_pUserInformation->CharacterID.GetString(), CLD_ZONECHAT, static_cast<DWORD>(right) );

										// [Help] when shouting
//										__BEGIN_HELP_EVENT
////											ExecuteHelpEvent( HE_CHAT_SHOUT );	
//										__END_HELP_EVENT
									}
									//else
									//{
										// Show that shouting is not possible right now
									//	UI_AddChatToHistory( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_GLOBAL_SAY].GetString(), NULL, CLD_INFO );
									//}
								}
							}
						}
						//break;

					}					
					else // Hmm.. this part of the code is ugly.. - -;;
					switch ( str[0] )
					{
						//------------------------------
						// Johab (combining) --> Wansung (precomposed)
						//------------------------------
						//char* pWansungString = new char [strlen(str)+1];

						//UI_JohapToWansung( str, pWansungString );

						
						//------------------------------------------------------------
						//
						// If '/' is typed, it is a whisper.
						//
						//------------------------------------------------------------
						case SYMBOL_WHISPER :						
						{
							// In the case of a Vampire
							//else if (//g_pPlayer->IsVampire() && 
									//str[0]=='/')

							if (strlen(str) > 1 )
							{
								// Add the rest except '/'..
								CToken strToken(str+1);

								// It consists of [name]+[ ]+[message].
								const char* pName = strToken.GetToken();
								const char* pMessage = strToken.GetEnd();										
								
								if (pName!=NULL && pMessage!=NULL)
								{									
									int nameLen = static_cast<int>(strlen(pName));
									
									//------------------------------------------------------
									// if the ID length is invalid
									//------------------------------------------------------
									if (static_cast<uint>(nameLen)<PlayerInfo::minIDLength || static_cast<uint>(nameLen)>PlayerInfo::maxIDLength)
									{
										char strTemp[256];
										SafeFormat::Format(strTemp, GetGameString(STRING_USER_REGISTER_ID_LENGTH), 
													PlayerInfo::minIDLength,
													PlayerInfo::maxIDLength);

										UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );
									}
									//------------------------------------------------------
									// A valid whisper...?
									//------------------------------------------------------
									else
									{
										if (g_pUserInformation->CharacterID==pName)
										{
											// whispering to oneself
											UI_AddChatToHistory( (*g_pGameStringTable)[STRING_MESSAGE_WHISPER_SELF].GetString(), NULL, CLD_INFO, static_cast<DWORD>(right) );
										}
										else
										{
											// set whisper target : ID + ' '
											char strWhisperID[128];
											SafeFormat::Format(strWhisperID, "%s ", pName);
											g_pUserInformation->WhisperID = strWhisperID;

											// Whispers go through the game server. The
											// peer-to-peer whisper (WhisperManager, a queue
											// and a direct connection to the other client)
											// was compiled out upstream and is deleted
											// (docs/RESTRUCTURING.md task 5.2, seventh slice).
											CGWhisper _CGWhisper;
											_CGWhisper.setName( pName );
											_CGWhisper.setMessage( pMessage );
											_CGWhisper.setColor( static_cast<uint>(right) );

											g_pSocket->sendPacket( &_CGWhisper );

											
											char strMessage[CHAT_MESSAGE_MAX_BYTES + 1];
											char strName[128];
											//sprintf(temp, "[%s] <%s> %s", g_pUserInformation->CharacterID.GetString(), pName, pMessage);
											//UI_AddChatToHistory( temp );
											SafeFormat::Copy(strMessage, pMessage);
											// Displayed in the form "[me] to whom> what".
											SafeFormat::Format(strName, "[%s] %s", g_pUserInformation->CharacterID.GetString(), pName);
											UI_AddChatToHistory( strMessage, strName, CLD_WHISPER, static_cast<DWORD>(right) );

											// [Help] when whispering
//											__BEGIN_HELP_EVENT
////												ExecuteHelpEvent( HE_CHAT_WHISPER );	
//											__END_HELP_EVENT
										}
									}
								}
							}
						}
						break;

						//------------------------------------------------------------
						//
						// If '@' is typed, it is a special command.
						//
						//------------------------------------------------------------
						case SYMBOL_COMMAND :
						//else if (//g_pPlayer->IsVampire() && 
						//			str[0]=='@')
						{
							if (strlen(str) > 1 )
							{
								// Add the rest except '@'..
								CToken strToken(str+1);

								// It consists of [command]+[ ]+[content].
								const char* pCommand = strToken.GetToken();
								const char* pData = strToken.GetEnd();

								char pLwrCommand[CHAT_MESSAGE_MAX_BYTES + 1];
								SafeFormat::Copy(pLwrCommand, pCommand);
#ifdef PLATFORM_WINDOWS
								_strlwr_s(pLwrCommand, sizeof(pLwrCommand));
#else
								// Simple lowercase conversion for macOS; tolower takes an
								// unsigned char's value (a Korean command's bytes are negative chars).
								for (int i = 0; pLwrCommand[i]; i++) {
									pLwrCommand[i] = static_cast<char>(tolower(static_cast<unsigned char>(pLwrCommand[i])));
								}
#endif


								DEBUG_ADD_FORMAT("[Command] %s %s", pCommand, pData);
								
								//-------------------------------------------------------
								// refuse conversation
								//-------------------------------------------------------
								if (strcmp(pCommand, STRING_IGNORE)==0
									|| strcmp(pLwrCommand, STRING_IGNORE_ENG)==0)
								{
									//-------------------------------------------------------
									// Refuse conversation from everyone.
									//-------------------------------------------------------
									if (pData==NULL)
									{
										g_pChatManager->ClearID();
										g_pChatManager->SetIgnoreMode();	

										// Add own ID.
										g_pChatManager->AddID( g_pUserInformation->CharacterID.GetString() );

										char strTemp[128];
										SafeFormat::Format(strTemp, GetGameString(STRING_MESSAGE_CHAT_IGNORE_ALL), pData);
										UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );

										// [Help] refuse conversation
//										__BEGIN_HELP_EVENT
////											ExecuteHelpEvent( HE_CHAT_REJECT );	
//										__END_HELP_EVENT
									}
									//-------------------------------------------------------
									// Refuse conversation from one person.
									//-------------------------------------------------------
									else
									{
										int nameLen = static_cast<int>(strlen( pData ));

										//------------------------------------------------------
										// if the ID length is invalid
										//------------------------------------------------------
										if (static_cast<uint>(nameLen)<PlayerInfo::minIDLength || static_cast<uint>(nameLen)>PlayerInfo::maxIDLength)
										{
											char strTemp[256];
											SafeFormat::Format(strTemp, GetGameString(STRING_USER_REGISTER_ID_LENGTH), 
													PlayerInfo::minIDLength,
													PlayerInfo::maxIDLength);

											UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );
										}
										//------------------------------------------------------
										// normal
										//------------------------------------------------------
										else
										{
											if (g_pChatManager->IsAcceptMode())
											{
												g_pChatManager->AddID( pData );
											}
											else
											{
												g_pChatManager->RemoveID( pData );
											}

											char strTemp[128];
											SafeFormat::Format(strTemp, GetGameString(STRING_MESSAGE_CHAT_IGNORE), pData);
											UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );

											// [Help] refuse conversation, one person
//											__BEGIN_HELP_EVENT
////												ExecuteHelpEvent( HE_CHAT_REJECT_USER );	
//											__END_HELP_EVENT
										}
									}
								}
								//-------------------------------------------------------
								// allow conversation
								//-------------------------------------------------------
								else if (strcmp(pCommand, STRING_ACCEPT)==0
										|| strcmp(pLwrCommand, STRING_ACCEPT_ENG)==0)
								{
									//-------------------------------------------------------
									// Allow conversation from everyone.
									//-------------------------------------------------------
									if (pData==NULL)
									{
										g_pChatManager->ClearID();
										g_pChatManager->SetAcceptMode();	

										char strTemp[128];
										SafeFormat::Format(strTemp, GetGameString(STRING_MESSAGE_CHAT_ACCEPT_ALL), pData);
										UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );
									}
									//-------------------------------------------------------
									// Allow conversation from one person.
									//-------------------------------------------------------
									else
									{
										int nameLen = static_cast<int>(strlen( pData ));

										//------------------------------------------------------
										// if the ID length is invalid
										//------------------------------------------------------
										if (static_cast<uint>(nameLen)<PlayerInfo::minIDLength || static_cast<uint>(nameLen)>PlayerInfo::maxIDLength)
										{
											char strTemp[256];
											SafeFormat::Format(strTemp, GetGameString(STRING_USER_REGISTER_ID_LENGTH), 
													PlayerInfo::minIDLength,
													PlayerInfo::maxIDLength);

											UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );
										}
										//------------------------------------------------------
										// normal
										//------------------------------------------------------
										else
										{
											if (g_pChatManager->IsAcceptMode())
											{
												g_pChatManager->RemoveID( pData );
											}
											else
											{	
												g_pChatManager->AddID( pData );
											}

											char strTemp[128];
											SafeFormat::Format(strTemp, GetGameString(STRING_MESSAGE_CHAT_ACCEPT), pData);
											UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );
										}
									}
								}
								//-------------------------------------------------------
								// view bad language too
								//-------------------------------------------------------
								else if (strcmp(pCommand, STRING_ACCEPT_CURSE)==0
										|| strcmp(pLwrCommand, STRING_ACCEPT_CURSE_ENG)==0)
								{
									g_pUserOption->FilteringCurse = FALSE;

									char strTemp[128];
									SafeFormat::Format(strTemp, GetGameString(STRING_MESSAGE_CHAT_ACCEPT_CURSE), pData);
									UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );
								}
								//-------------------------------------------------------
								// view only clean language
								//-------------------------------------------------------
								else if (strcmp(pCommand, STRING_FILTER_CURSE)==0
										|| strcmp(pLwrCommand, STRING_FILTER_CURSE_ENG)==0)
								{
									g_pUserOption->FilteringCurse = TRUE;

									char strTemp[128];
									SafeFormat::Format(strTemp, GetGameString(STRING_MESSAGE_CHAT_FILTER_CURSE), pData);
									UI_AddChatToHistory( strTemp, NULL, CLD_INFO, static_cast<DWORD>(right) );
								}

								#if defined(OUTPUT_DEBUG) //&& defined(_DEBUG)
									//-------------------------------------------------------
									// connect
									//-------------------------------------------------------
									else if (strcmp(pCommand, "connect")==0
											|| strcmp(pLwrCommand, "connect")==0)
									{
										CToken strToken(pData);

										// [IP]+[]+[Name]
										const char* pIP = strToken.GetToken();
										const char* pName = strToken.GetEnd();

										//RequestConnect( pIP, pName );
										if (g_pParty!=NULL)
										{
											PARTY_INFO* pInfo = g_pParty->GetMemberInfo( pName );

											if (pInfo!=NULL)
											{
												pInfo->IP = pIP;
											}
										}
									}
									//-------------------------------------------------------
									// disconnect
									//-------------------------------------------------------
									else if (strcmp(pCommand, "disconnect")==0
											|| strcmp(pLwrCommand, "disconnect")==0)
									{
										const char* pName = pData;
										//RequestDisconnect( pData );
										if (g_pParty!=NULL)
										{
											PARTY_INFO* pInfo = g_pParty->GetMemberInfo( pName );

											if (pInfo!=NULL)
											{
												pInfo->IP = "0.0.0.0";
											}
										}
									}
									//-------------------------------------------------------
									// speak
									//-------------------------------------------------------
									else if (strcmp(pCommand, "say")==0
											|| strcmp(pLwrCommand, "say")==0)
									{
										if (g_pParty!=NULL
											&& g_pClientCommunicationManager!=NULL)
										{
											RCSay _RCSay;
											_RCSay.setName( g_pUserInformation->CharacterID.GetString() );
											_RCSay.setMessage( pData );
											_RCSay.setColor( right );
										
											for (int i=0; i<g_pParty->GetSize(); i++)
											{
												PARTY_INFO* pInfo = g_pParty->GetMemberInfo( i );
												
												if (pInfo!=NULL)										
												{
													int port = g_pClientConfig->CLIENT_COMMUNICATION_UDP_PORT;

													RequestUserInfo* pUserInfo = g_pRequestUserManager->GetUserInfo( pInfo->Name.GetString() );

													if (pUserInfo!=NULL)
													{
														if (pUserInfo->UDPPort!=0)
															port = pUserInfo->UDPPort;
													}

													g_pClientCommunicationManager->sendPacket( 
															pInfo->IP, 
															port, 
															&_RCSay );
												}
											}	
										}
									}
									//-------------------------------------------------------
									// set number of npcs
									//-------------------------------------------------------
									else if (strcmp(pCommand, "npc")==0
											|| strcmp(pLwrCommand, "npc")==0)
									{
										g_MaxNPC = atoi( pData );
									}									
									//-------------------------------------------------------
									// set ProfilerInfoName
									//-------------------------------------------------------
									else if (strcmp(pCommand, "profileName")==0
											|| strcmp(pLwrCommand, "profilename")==0)
									{
										if (g_pProfiler!=NULL)
										{
											g_ProfileInfoName = pData;
										}
									}
									//-------------------------------------------------------
									// set ProfilerInfoName
									//-------------------------------------------------------
									else if (strcmp(pCommand, "profileClear")==0
											|| strcmp(pLwrCommand, "profileclear")==0)
									{
										if (g_pProfiler!=NULL)
										{
											g_pProfiler->Release();
										}
									}
									//-------------------------------------------------------
									// set gamma
									//-------------------------------------------------------
									else if (strcmp(pCommand, "gamma")==0
											|| strcmp(pLwrCommand, "gamma")==0)
									{
										const char* pValue = pData;

										int value = atoi( pValue );

										if (value==999)
										{
											CSDLGraphics::RestoreGammaRamp();
										}
										else
										{
											CSDLGraphics::SetGammaRamp( value );
										}
									}
									
								#endif
							}
						}
						break;

						case SYMBOL_MASTER_COMMAND:
						{
							unsigned commandNumber = 0;
							static std::string	strTempCommand;

							char *pMessage = str;
							static bool bInvincible = false;

							if(strcmp(str, "*iddqd") == 0 || strcmp(str, "*Tnrrkt") == 0 || strcmp(str, "*쑥갓") == 0)
							{
								if(bInvincible == true)
									strTempCommand = "*command invincible off";
								else
									strTempCommand = "*command invincible on";
								bInvincible = !bInvincible;

								pMessage = const_cast<char*>(strTempCommand.c_str());
							}
							else if(strcmp(str, "*gaonashi") == 0 || strcmp(str, "*gaonasi") == 0 || strcmp(str, "*rkdhsktl") == 0 || strcmp(str, "*가오나시") == 0 || strcmp(str,"*ghost") == 0)
							{
								if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_GHOST))
									strTempCommand = "*command ghost off";
								else
									strTempCommand = "*command ghost on";
								pMessage = const_cast<char*>(strTempCommand.c_str());
							}
							else if(strcmp(str, "*goes") == 0 )
							{
								strTempCommand = "*warp 8000 75 77";
								pMessage = const_cast<char*>(strTempCommand.c_str());
							}
							else if (MasterCommands::Invocation(str, commandNumber))
							{
								MasterCommands::Limits limits;
								limits.maxCommandBytes = CHAT_MESSAGE_MAX_BYTES;
								std::vector<std::string> commands;
								if (MasterCommands::Load(commandNumber, ".", commands, limits)) {
									for (auto& command : commands)
										Execute_UI_CHAT_RETURN(left, right, command.data());
								} else {
									DEBUG_ADD_WAR("[Master Command] Rejected missing, malformed, cyclic or excessive command files.");
								}
								// This is a local macro selector; only its validated leaf commands dispatch.
								break;
							}
							else if(strlen(str) >= 5 && strncmp(str, "*C2G", 4) == 0 )
							{
								if( strcmp(str+5, "on") == 0 )
								{
									g_C2G = 1;
								}
								else
								{
									g_C2G--;
								}
//								for( int grid_y = 0; grid_y < g_pInventory->GetHeight(); grid_y++ )
//								for( int grid_x = 0; grid_x < g_pInventory->GetWidth(); grid_x++ )
//								{
//									C_VS_UI_INVENTORY::AutoMove( grid_x, grid_y );
//									gpC_base->DispatchMessage();
//								}
							}
							else if( g_C2G > 0 && strncmp(str, "*create", 7 ) == 0 )
							{
								g_C2G++;
							}

//							{
//								std::string aaaa[4];
//								aaaa[0] = "*set war_active off";
//								aaaa[1] = "*command removeRaceWar";
//								aaaa[2] = "*set war_active on";
//								aaaa[3] = "*set war_active off";
//
//								CGSay _CGSay;
//
//								_CGSay.setColor( right );
//
//								for(int i=0;i<4;i++)
//								{
//									_CGSay.setMessage( aaaa[i].c_str() );									
//									g_pSocket->sendPacket( &_CGSay );
//								}
//							}

							bool bSay = true;
#ifndef __METROTECH_TEST
							if(g_pUserInformation->bCompetence == true && g_pUserInformation->bCompetenceShape != true)
							{
								// A zone the table lacks is no competence zone.
								const ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( g_pZone->GetID() );
								if(pZoneInfo != NULL && pZoneInfo->CompetenceZone == true)
								{
								}
								else
								{
									if(strstr(pMessage, "*command ghost off") != NULL)
										bSay = false;
								}
							}
#endif

							if(bSay == true)
							{
								CGSay _CGSay;
								_CGSay.setMessage( pMessage );	//pWansungString );
								_CGSay.setColor( static_cast<uint>(right) );
								g_pSocket->sendPacket( &_CGSay );
								
//								UI_AddChatToHistory( pMessage, g_pUserInformation->CharacterID.GetString(), CLD_NORMAL, right );
							}
							break;
						}

						//------------------------------------------------------------
						//
						//	normal conversation
						//
						//------------------------------------------------------------
						default :
						{
							//------------------------------
							// Cannot speak while hidden.
							// Cannot speak when a wolf or a bat.
							//------------------------------
							if (g_pPlayer->IsUndergroundCreature()
								|| g_pPlayer->IsInCasket()
								|| g_pPlayer->GetCreatureType()==CREATURETYPE_BAT
								|| g_pPlayer->GetCreatureType()==CREATURETYPE_WOLF
								|| g_pPlayer->GetCreatureType()==CREATURETYPE_WER_WOLF)
							{
								// add to history
								//char temp[128];
								//sprintf(temp, "[%s] .....", g_pUserInformation->CharacterID.GetString());
								//UI_AddChatToHistory( temp );
								UI_AddChatToHistory( ".....", g_pUserInformation->CharacterID.GetString(), CLD_NORMAL, static_cast<DWORD>(right) );
							}
							else
							{
								CGSay _CGSay;
								_CGSay.setMessage( str );	//pWansungString );
								_CGSay.setColor( static_cast<uint>(right) );
								g_pSocket->sendPacket( &_CGSay );

									
								// add to the Player's Chat pWansungStringing
								g_pPlayer->SetChatString( str, static_cast<COLORREF>(right) );

								// add to history
								//char temp[256];
								//sprintf(temp, "%s> %s", g_pUserInformation->CharacterID.GetString(), str);
								//UI_AddChatToHistory( temp );
								UI_AddChatToHistory( str, g_pUserInformation->CharacterID.GetString(), CLD_NORMAL, static_cast<DWORD>(right) );
							}
						}	
						
						//delete [] str;
						//delete [] pWansungString;
					}
				}
			}
		}
	}

	// The queued message (or the synchronous caller) owns the input string.

//	__BEGIN_HELP_EVENT
		ExecuteHelpEvent( HELP_EVENT_CHAT );
//	__END_HELP_EVENT

}


//-----------------------------------------------------------------------------
//
// Logout
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_LOGOUT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_LOGOUT");

	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	
	if (g_pPlayer->IsWaitVerify())
	{
		DEBUG_ADD_FORMAT("WaitVerify %d", g_pPlayer->GetWaitVerify());
		return;
	}
	
	//-------------------------------------------------------------------
	// If the Logout time is not set, set it and,
	// if it is set, print the remaining time.
	//-------------------------------------------------------------------

	//ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( zoneID );
	
	//-------------------------------------------------------------------
	// In a safe zone, log out immediately.
	// when alone..
	//-------------------------------------------------------------------
	if (g_bZoneSafe //pZoneInfo!=NULL	&& pZoneInfo->Safety
		// if it is a safe zone tile..
		|| g_pPlayer->IsInSafeSector()
		|| g_pZone->GetID() == 8000				// 捞亥飘版扁厘 愚搁 官肺 辆丰
		// if some seconds have passed since death...
//		|| g_pPlayer->IsDead() && g_pPlayer->GetDeadDelayLast() < 4)
		//|| g_pZone->GetCreatureNumber()==1)
#ifdef __METROTECH_TEST__
		|| 1
#endif
		)
	{
		ExecuteLogout();
	}
	//-------------------------------------------------------------------
	// Otherwise log out after 5 seconds.
	//-------------------------------------------------------------------
	else if (g_pPlayer!=NULL && g_pPlayer->IsAlive())
	{		
		if (!g_pUserInformation->IsLogoutScheduled())
		{
			// force the logout five seconds on
			
			#ifdef _DEBUG
				g_pUserInformation->LogoutTime = g_FrameNow + MonotonicClock::Millis(2000);
			#else
				g_pUserInformation->LogoutTime = g_FrameNow + MonotonicClock::Millis(5000);
			#endif

			MEvent event;
			event.eventID = EVENTID_LOGOUT;
			event.eventDelay = 5000;
			event.eventFlag = EVENTFLAG_SHOW_DELAY_STRING;
			event.eventType = EVENTTYPE_ZONE;
			event.m_StringsID.push_back(STRING_MESSAGE_LOGOUT_AFTER_SECOND);

			g_pEventManager->AddEvent(event);

			g_pPlayer->SetWaitVerify(MPlayer::WAIT_VERIFY_LOGOUT);
//			g_pSystemMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_LOGOUT_AFTER_SECOND].GetString(), 5);
		}
		else
		{
			DWORD sec = g_pUserInformation->SecondsToLogout(g_FrameNow);

			if (sec > 0)
			{
				MEvent event;
				event.eventID = EVENTID_LOGOUT;
				event.eventDelay = sec*1000;
				event.eventFlag = EVENTFLAG_SHOW_DELAY_STRING;
				event.eventType = EVENTTYPE_ZONE;
				event.m_StringsID.push_back(STRING_MESSAGE_LOGOUT_AFTER_SECOND);
				
				g_pEventManager->AddEvent(event);
				g_pPlayer->SetWaitVerify(MPlayer::WAIT_VERIFY_LOGOUT);

//				g_pSystemMessage->AddFormat((*g_pGameStringTable)[STRING_MESSAGE_LOGOUT_AFTER_SECOND].GetString(), sec);
			}
		}

		gC_vs_ui.HotKey_ESC();
	}
	//-------------------------------------------------------------------
	// if dead..
	//-------------------------------------------------------------------
	else
	{
		g_pSystemMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_LOGOUT_DIED].GetString() );

		gC_vs_ui.HotKey_ESC();
	}	
}


//-----------------------------------------------------------------------------
//
// When a Skill was selected
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_SELECT_SKILL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD_FORMAT("[UI] UI_SELECT_SKILL(%d)", left);

	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	//
	// dw_left = Skill ID
	//
	if (g_pPlayer!=NULL)
	{
		g_pPlayer->SetSpecialActionInfo( static_cast<TYPE_ACTIONINFO>(left) );
		
		// because actionCount can go wrong during repeated actions,
		// this should be done.. but skills feel sluggish, so for now.. - -;
		//g_pPlayer->UnSetRepeatAction();
	}

	// [Help] when the Skill icon changes
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_SKILL_ICON_CHANGE );
//	__END_HELP_EVENT
}

//-----------------------------------------------------------------------------
//
// Skill selection cancelled
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CANCEL_SELECT_SKILL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CANCEL_SELECT_SKILL");

	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	//
	// Skill selection cancelled.
	//
	if (g_pPlayer!=NULL)
	{
		g_pPlayer->SetSpecialActionInfoNULL();
	}
}

// Check whether the desired corpse is near the given creature.
bool IsExistCorpseFromPlayer(MCreature* OriginCreature, int creature_type)
{
	int sx, sy;

	sx = OriginCreature->GetX();
	sy = OriginCreature->GetY();

	for(int i=-1;i<=1;i++)
	{
		for(int j=-1;j<=1;j++)
		{
			if( sx+i < 0 || sy + j < 0 || sx+i >= g_pZone->GetWidth() || sy+j >= g_pZone->GetHeight() )
				continue;

			const MSector& sector = g_pZone->GetSector( sx+i, sy+j );
			MItem* pItem = sector.GetItem();
			
			if( pItem != NULL && pItem->GetItemClass() == ITEM_CLASS_CORPSE)
			{
				MCreature* pCreature = (dynamic_cast<MCorpse*>(pItem))->GetCreature();
				if( pCreature != NULL && pCreature->GetCreatureType() == creature_type )
					return true;
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
//
// When dropping an Item from the mouse to the Zone,
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_DROP_TO_CLIENT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ITEM_DROP_TO_CLIENT");

	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}

	MItem* pItem = UI_GetMouseItem();//(MItem*)void_ptr;		

	// Hardcoding for Capture The Flag
	// Check 3 tiles around me.
	// Check if it is not a flag item.

	if(!( pItem != NULL && pItem->GetItemClass() == ITEM_CLASS_EVENT_ITEM && pItem->GetItemType() == 27 ) )
	{
		if( IsExistCorpseFromPlayer( dynamic_cast<MCreature*>( g_pPlayer ), 670 ) )
		{
			g_pSystemMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_DROP_ITEM_BY_FLAG].GetString() );
			return;
		}
	}
				
	if (!g_bWatchMode
		// nothing may be dropped during a trade,
		&& !UI_IsRunningExchange()
		// nor for a while after the trade window opened
		&& g_pUserInformation->IsItemDropEnabled(g_FrameNow))
	{
		//void_ptr = MItem *

		if( pItem != NULL && 
				(
					gC_vs_ui.IsRunningRemoveOptionFromRareItem() && pItem->GetItemClass() == ITEM_CLASS_MIXING_ITEM 
				)
			)
//			pItem != NULL && pItem->GetItemClass() == ITEM_CLASS_QUEST_ITEM && pItem->GetItemType() == 4 )
			return;
				
		if (pItem != NULL && (pItem->GetItemClass() == ITEM_CLASS_RELIC ||
			pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE || 
			pItem->GetItemClass() == ITEM_CLASS_CASTLE_SYMBOL ||
			(pItem->GetItemClass() == ITEM_CLASS_EVENT_ITEM && (pItem->GetItemType() == 27 || pItem->GetItemType() == 31)) ||
			pItem->GetItemClass() == ITEM_CLASS_SWEEPER || pItem->GetItemClass() == ITEM_CLASS_PET_FOOD
			))
		{
			// A relic clicked on its shelf ??-_-;
			int CreatureID = UIMessages::GetSelectedCreature();

			MCreature *pCreature = NULL;
			bool		bCorpse = false;

			if(CreatureID == -1)
			{
				CreatureID = UIMessages::GetSelectedItemID();
				MItem *selectedItem = g_pZone->GetItem(CreatureID);
				if(selectedItem != NULL)
				{
					if(selectedItem->GetItemClass() == ITEM_CLASS_CORPSE)
					{
						MCorpse *pCorpse = (MCorpse *)selectedItem;
						pCreature = pCorpse->GetCreature();
						bCorpse = true;
					}
				}
			}
			else
			{
//				((g_pPlayer->IsSlayer() && pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 373)
//				|| (g_pPlayer->IsVampire() && pCreature->GetCreatureType() >= 374 && pCreature->GetCreatureType() <= 376))
				pCreature = g_pZone->GetCreature(CreatureID);
			}

			if(g_pPlayer->IsItemCheckBufferNULL() && pCreature != NULL)
			{
				if(bCorpse)	// Put the item into the corpse
				{
					if((pCreature->GetCreatureType() >= 371 && pCreature->GetCreatureType() <= 376) || 
						(pCreature->GetCreatureType() >= 560 && pCreature->GetCreatureType() <= 563) ||
						(pCreature->GetCreatureType() >= 526 && pCreature->GetCreatureType() <= 549) ||
						(pCreature->GetCreatureType() == 670	&& pItem->GetItemClass() == ITEM_CLASS_EVENT_ITEM) || // if it is a flag
						(pCreature->GetCreatureType() == 672 && pItem->GetItemClass() == ITEM_CLASS_SWEEPER)
					)
					{
						if(abs(g_pPlayer->GetX()-pCreature->GetX()) < 3 && abs(g_pPlayer->GetY()-pCreature->GetY()) < 3)
						{
							CGRelicToObject _CGRelicToObject;
							_CGRelicToObject.setItemObjectID(pItem->GetID());
							_CGRelicToObject.setObjectID(CreatureID);
							g_pSocket->sendPacket( &_CGRelicToObject );
							g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE );
						}
						else
						{
							g_pSystemMessage->Add((*g_pGameStringTable)[UI_STRING_MESSAGE_TOO_FAR].GetString());
						}
						return;
					}
				}
				else	// Put the item into the monster
				{
					if(pItem->GetItemClass() == ITEM_CLASS_PET_FOOD &&
						pCreature->GetCreatureType() == 687 && pItem->GetNumber() == 1)	// Put in the creature type later
					{
						if(abs(g_pPlayer->GetX()-pCreature->GetX()) < 3 && abs(g_pPlayer->GetY()-pCreature->GetY()) < 3)
						{
							CGTameMonster _CGTameMonster;
							_CGTameMonster.setObjectID(CreatureID);
							g_pSocket->sendPacket( &_CGTameMonster );
							g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_CREATURE );
						}
						else
						{
							g_pSystemMessage->Add((*g_pGameStringTable)[UI_STRING_MESSAGE_TOO_FAR].GetString());
						}
						return;
					}
					// when it is a Spiral of Life item for a quest
					else if(pItem->GetItemClass() == ITEM_CLASS_EVENT_ITEM && pItem->GetItemType() == 31 &&
						(pCreature->GetCreatureType() == 793 || pCreature->GetCreatureType() == 794 || pCreature->GetCreatureType() == 795))
					{
						CGRelicToObject _CGRelicToObject;
						_CGRelicToObject.setItemObjectID(pItem->GetID());
						_CGRelicToObject.setObjectID(CreatureID);
						g_pSocket->sendPacket( &_CGRelicToObject );
						g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE );
						pCreature->RemoveCauseCriticalWoundsEffect();

					}
				}
			}

		}

		//-----------------------------------------------------------------
		// if there is nothing to verify
		//-----------------------------------------------------------------
		if (pItem!=NULL 
			&& !pItem->IsQuestItem()
			&& g_pPlayer->IsItemCheckBufferNULL()
			// A red Event GiftBox item cannot be thrown away.
			&& !(pItem->GetItemClass()==ITEM_CLASS_EVENT_GIFT_BOX&& pItem->GetItemType()==1)
			&& !(pItem->GetItemClass()==ITEM_CLASS_COUPLE_RING)
			&& !(pItem->GetItemClass()==ITEM_CLASS_VAMPIRE_COUPLE_RING 
			&& (pItem->GetItemClass() == ITEM_CLASS_CODE_SHEET && !gC_vs_ui.IsRunningQuestInventory() ) )
			&& !(pItem->GetItemClass() == ITEM_CLASS_LUCKY_BAG && pItem->GetItemType() == 3)
			&& !(pItem->GetItemClass() == ITEM_CLASS_EVENT_ITEM && (pItem->GetItemType() == 28||pItem->GetItemType() == 31))
				
			)
		{
			//-----------------------------------------------------------------------
			// Drop only when not trading.
			//-----------------------------------------------------------------------
			if (g_pTradeManager==NULL)
			{
					//---------------------------------------------------
					// if there is no need to verify with the item..
					//---------------------------------------------------
					//if (g_pPlayer->IsItemCheckBufferNULL())
					{
						//---------------------------------------------------
						// Send a packet to the Server saying the item is dropped.
						// What actually drops..
						// is when the server adds the item to the zone.
						//---------------------------------------------------
						CGAddMouseToZone _CGAddMouseToZone;
						_CGAddMouseToZone.setObjectID( pItem->GetID() );

						g_pSocket->sendPacket( &_CGAddMouseToZone );

						
						//---------------------------------------------------
						// Shouldn't this part be done.. after receiving AddNewItem..
						// The timing issue is ambiguous.
						//---------------------------------------------------
						// remove the item from the mouse
						//gC_vs_ui.DropItem();

						// store the item in the buffer. (create a new item)
						g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE );

						// delete the item
						//delete pItem;
					}

			}
			//-----------------------------------------------------------------------
			// Do not drop items while trading.
			//-----------------------------------------------------------------------
			else
			{
				DEBUG_ADD("Cannot Drop Item to Zone while Trading.");
			}
		}
		//-----------------------------------------------------------------
		// if there is another item that needs verification
		//-----------------------------------------------------------------
		else
		{
			DEBUG_ADD_ERR( "[Error] There is another item in ItemCheckBuffer or Mouse NULL");
		}
	}
	else
	{
		DEBUG_ADD( "Can't Drop");
	}
}


//-----------------------------------------------------------------------------
//
// When the mouse's Item is added to the Inventory
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_DROP_TO_INVENTORY(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_ITEM_DROP_TO_INVENTORY");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	//
	// left = grid_x, right = grid_y
	// void_ptr = MItem *		// added item
	//							// if bAcceptMyTrade, it is the inventory's item.
	//
	MItem* pItem = (MItem*)void_ptr;		// the item in the inventory (may be NULL)
	MItem* pMouseItem = UI_GetMouseItem();	// the current mouse item

	// Swap pMouseItem with pItem..
	// or pMouseItem just goes in...
	
	BOOL bAcceptMyTrade = (g_pTradeManager!=NULL && g_pTradeManager->IsAcceptMyTrade());
	BOOL bAcceptOtherTrade = (g_pTradeManager!=NULL && g_pTradeManager->IsAcceptOtherTrade());

	//-----------------------------------------------------------------
	// if there is nothing to verify
	//-----------------------------------------------------------------
	if (g_pPlayer->IsItemCheckBufferNULL() && pMouseItem!=NULL
		&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{		
		BOOL bSendPacketTradeRemove = FALSE;	// remove the existing one
		BOOL bSendPacket = TRUE;				// mouse --> inventory
		BOOL bSendPacketTradeAdd = FALSE;		// set the added one as the one to trade
		
		TYPE_OBJECTID removeItemID = OBJECTID_NULL;
		TYPE_OBJECTID toInventoryItemID = (pMouseItem==NULL)? OBJECTID_NULL : pMouseItem->GetID();

		//---------------------------------------------------
		// In the trade state it must be verified.
		//---------------------------------------------------
		if (bAcceptMyTrade)
		{
			if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL
				&& g_pPlayer->IsItemCheckBufferNULL())
			{	
				g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY);
				g_pTempInformation->Value1 = pMouseItem->GetID();
				g_pTempInformation->Value2 = left;
				g_pTempInformation->Value3 = right;
				g_pTempInformation->pValue = pMouseItem;

				g_pPlayer->SetItemCheckBuffer( pMouseItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_INVENTORY );

				//toInventoryItemID = pMouseItem->GetID();

				//-------------------------------------------------------------
				// If the item in the inventory was selected for trade, remove it.
				//-------------------------------------------------------------
				if (pItem!=NULL && pItem->IsTrade())
				{
					removeItemID = pItem->GetID();				
					
					bSendPacketTradeRemove = TRUE;
				}

				//bSendPacketTradeAdd = TRUE;
			}
			else
			{
				bSendPacket = FALSE;
			}
		}	
		//---------------------------------------------------
		// if the item placed was selected for trade
		//---------------------------------------------------
		else if (gC_vs_ui.IsRunningExchange())
		{
			//---------------------------------------------------
			// Remove the item that was there from the trade..
			//---------------------------------------------------
			if (pItem!=NULL && pItem->IsTrade())
			{
				removeItemID = pItem->GetID();

				bSendPacketTradeRemove = TRUE;
			}

			//---------------------------------------------------
			// the item selected in the trade
			//---------------------------------------------------
			if (pMouseItem->IsTrade())
			{
				bSendPacketTradeAdd = TRUE;

				// if the other person has pressed OK..
				if (bAcceptOtherTrade)
				{
					g_pTradeManager->RefuseOtherTrade();
				}
			}
			//---------------------------------------------------
			// Otherwise.. manually send an OK cancel.
			//---------------------------------------------------
			else
			{
				/*
				if (bAcceptMyTrade)
				#ifdef CONNECT_SERVER
					CGTradeFinish _CGTradeFinish;
					_CGTradeFinish.setTargetObjectID( g_pTradeManager->GetOtherID() );
					_CGTradeFinish.setCode( CG_TRADE_FINISH_RECONSIDER );

					g_pSocket->sendPacket( &_CGTradeFinish );
				#endif
				*/
			}
		}

		//---------------------------------------------------------
		// SendPacket - Trade Remove Item
		//---------------------------------------------------------
		if (bSendPacketTradeRemove)
		{
				CGTradeRemoveItem _CGTradeRemoveItem;
				_CGTradeRemoveItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
				_CGTradeRemoveItem.setItemObjectID( removeItemID );

				g_pSocket->sendPacket( &_CGTradeRemoveItem );

				g_pTradeManager->SetNextAcceptTime();
		}

		//---------------------------------------------------
		// if a packet needs to be sent
		//---------------------------------------------------
		if (bSendPacket)
		{

				//---------------------------------------------------
				// Tell the server that the item that was on the mouse
				// was added to the Inventory, by sending a packet.
				// The grid coordinates of pItem are its coordinates in the inventory.
				//---------------------------------------------------
				CGAddMouseToInventory _CGAddMouseToInventory;
				_CGAddMouseToInventory.setObjectID( toInventoryItemID );
				_CGAddMouseToInventory.setInvenX( static_cast<CoordInven_t>(left) );//pItem->GetGridX() );
				_CGAddMouseToInventory.setInvenY( static_cast<CoordInven_t>(right) );//pItem->GetGridY() );

				g_pSocket->sendPacket( &_CGAddMouseToInventory );

		}

		//---------------------------------------------------
		// item selected in the trade...
		//---------------------------------------------------
		if (bSendPacketTradeAdd)
		{
				CGTradeAddItem _CGTradeAddItem;
				_CGTradeAddItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
				_CGTradeAddItem.setItemObjectID( pMouseItem->GetID() );

				g_pSocket->sendPacket( &_CGTradeAddItem );
				
		}

		//---------------------------------------------------
		// Actually process it.
		//---------------------------------------------------
		if (!bAcceptMyTrade)
		{
			MItem* pOldItem = NULL;
			if (g_pInventory->ReplaceItem(pMouseItem,		// item to add
												static_cast<BYTE>(left), static_cast<BYTE>(right),	// position to add
												pOldItem))		// item that was there originally
			{
				if (pOldItem != NULL) // was it replaced?
				{				
					UI_PickUpItem( pOldItem );
				}
				else
				{
					UI_DropItem();
				}
			}

			// Hardcoding for the Christmas tree
			if(pMouseItem->GetItemClass() == ITEM_CLASS_EVENT_TREE 
				&& pMouseItem->GetItemType() != 12
				&& pMouseItem->GetItemType() != 25
				&& pMouseItem->GetItemType() != 26
				&& pMouseItem->GetItemType() != 27
				&& pMouseItem->GetItemType() != 28
				&& pMouseItem->GetItemType() != 41
				)
			{
				int mx = 0, my = 0;
				
				
				if(pMouseItem->GetItemType() < 12)
				{
					my = pMouseItem->GetItemType()/3;
					mx = pMouseItem->GetItemType()%3;
				}
				else if( pMouseItem->GetItemType() < 25 )
				{
					my = (pMouseItem->GetItemType()-13)/3;
					mx = (pMouseItem->GetItemType()-13)%3;
				} 
				else if( pMouseItem->GetItemType() < 41 )
				{
					my = (pMouseItem->GetItemType()-29)/3;
					mx = (pMouseItem->GetItemType()-29)%3;
				}
				const int pointX = pMouseItem->GetGridX() - mx;
				const int pointY = pMouseItem->GetGridY() - my;
				
				bool bTree = true;

				for(int y = 0; y < 4; y++)
				{
					for(int x = 0; x < 3; x++)
					{
						MItem *pPartOfTree = g_pInventory->GetItem(x+pointX, y+pointY);
						if(pPartOfTree != NULL && 
							pPartOfTree->GetItemClass() == ITEM_CLASS_EVENT_TREE &&
							(pPartOfTree->GetItemType() == y*3+x ||
							pPartOfTree->GetItemType() == y*3+x + 13 ||
							pPartOfTree->GetItemType() == y*3+x + 29) )
						{
						}
						else
						{
							bTree = false;
						}
					}
				}

				if(bTree)
					g_pPlayer->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_TREE_MERGE);
			}
		}
	}
	//-----------------------------------------------------------------
	// if there is another item that needs verification
	//-----------------------------------------------------------------
	else
	{
		DEBUG_ADD_ERR( "[Error] There is another item in ItemCheckBuffer or Mouse NULL");

		if(!g_pPlayer->IsItemCheckBufferNULL())
			DEBUG_ADD_FORMAT("[ITEMDROP] !g_pPlayer->IsItemCheckBufferNULL() %d", g_pPlayer->GetItemCheckBufferStatus());
		if(pMouseItem == NULL)
			DEBUG_ADD("[ITEMDROP] pMouseItem == NULL)");
		if(g_pTempInformation->GetMode()!=TempInformation::MODE_NULL)
			DEBUG_ADD_FORMAT("[ITEMPDROP] g_pTempInformation->Mode!=TempInformation::MODE_NULL %d", g_pTempInformation->Mode);
	}

	// This code should be deleted. *_*;
	//gC_vs_ui.DropItem();
}

//-----------------------------------------------------------------------------
//
// When an Item is put into the QuickSlot
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_DROP_TO_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ITEM_DROP_TO_QUICKSLOT");

	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	
	MOustersArmsBand* pQuickSlot = NULL;

	int slot = static_cast<int>(left);
	
	if( g_pPlayer->IsOusters() )
	{
		if( g_pArmsBand1 == NULL )
			pQuickSlot = g_pArmsBand2;
		else
		{
			if( g_pArmsBand1->GetPocketNumber() > slot )
				pQuickSlot = g_pArmsBand1;
			else				
			{
				pQuickSlot = g_pArmsBand2;
				slot -= g_pArmsBand1->GetPocketNumber();
			}
		}

		if( pQuickSlot == NULL )
			return;
	}

	//
	// left = grid_x
	// void_ptr = MItem *
	//
	//---------------------------------------------------
	// The UI currently adds it right away..
	// or swaps it right away.
	// So the two have to be read swapped. - -;
	// but changed so that the client handles it, by larosel
	//---------------------------------------------------
	//---------------------------------------------------
	// No belt to drop into: a slayer without one, or a vampire, for whom
	// neither branch above set anything up.
	//---------------------------------------------------
	if (g_pPlayer->IsSlayer() ? g_pQuickSlot == NULL : pQuickSlot == NULL)
	{
		DEBUG_ADD_ERR("[Error] UI_ITEM_DROP_TO_QUICKSLOT without a quick slot");
		return;
	}

	MItem* pMouseItem = gpC_mouse_pointer->GetPickUpItem();//g_pQuickSlot->GetItem( left );

	//---------------------------------------------------
	// Nothing on the mouse: the message is stale. The UI checks the mouse
	// before queueing, but messages are dispatched one per frame, so a
	// second click (a double-click delivers DOWN and DBLCLK, queueing two
	// drops) or a server packet that cleared the mouse in between
	// (GCDeleteObject, GCDeleteInventoryItem, a trade verify) arrives
	// here with nothing to drop. ReplaceItem() tolerates a NULL item;
	// the packet built below dereferenced it. Same guard as
	// Execute_UI_ITEM_DROP_TO_INVENTORY.
	//---------------------------------------------------
	if (pMouseItem == NULL)
	{
		DEBUG_ADD_ERR("[Error] UI_ITEM_DROP_TO_QUICKSLOT with no item on the mouse");
		return;
	}

	MItem* pSlotItem = NULL ;// = (MItem*)g_pQuickSlot->GetItem( left );;//gpC_mouse_pointer->GetPickUpItem();
	if( g_pPlayer->IsSlayer() )
		pSlotItem = (MItem*)g_pQuickSlot->GetItem( static_cast<BYTE>(left) );
	else
		pSlotItem = (MItem*)pQuickSlot->GetItem( slot );
	
	bool	Replace = FALSE;
	
	if( g_pPlayer->IsSlayer() )
		Replace = g_pQuickSlot->ReplaceItem( pMouseItem, static_cast<BYTE>(left), pSlotItem );
	else
		Replace = pQuickSlot->ReplaceItem( pMouseItem, slot, pSlotItem );

	if (Replace)
	{
		if (pSlotItem) // was it replaced?
		{
			gpC_mouse_pointer->PickUpItem((MItem *)pSlotItem);
		}
		else
		{
			// not replace, just drop at empty grid.
			gpC_mouse_pointer->DropItem();
		}
	}
	
	//-----------------------------------------------------------------
	// if there is nothing to verify
	//-----------------------------------------------------------------
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)//g_pPlayer->IsItemCheckBufferNULL())
	{
		// Just always send the packet.
		if (1)//g_pQuickSlot->GetItem( left )==NULL)
		{

				//MItem* pItem = (MItem*)void_ptr;

				//---------------------------------------------------
				// Tell the server that the item that was on the mouse
				// was added to the Inventory, by sending a packet.
				//---------------------------------------------------
				CGAddMouseToQuickSlot _CGAddMouseToQuickSlot;
				_CGAddMouseToQuickSlot.setObjectID( pMouseItem->GetID() );
				
				if( g_pPlayer->IsSlayer() )
					_CGAddMouseToQuickSlot.setSlotID( static_cast<SlotID_t>(left) );//pItem->GetItemSlot() );
				else
				{
					if( pQuickSlot == g_pArmsBand2 )
						_CGAddMouseToQuickSlot.setSlotID( slot + 3 );
					else
						_CGAddMouseToQuickSlot.setSlotID( slot );
				}
				
				g_pSocket->sendPacket( &_CGAddMouseToQuickSlot );
		
				//---------------------------------------------------
				// Must not do it, because the UI does it..
				//---------------------------------------------------
				// Add it to the QuickSlot right away.
				//g_pQuickSlot->AddItem( pItem, left );
				
				//UI_DropItem();

			PlaySound( pMouseItem->GetInventorySoundID() );
		}
		//---------------------------------------------------
		// if something is already there..
		//---------------------------------------------------
		else
		{						
			DEBUG_ADD_FORMAT_ERR("[Error] Can't add item to quickslot, already exist slot=%d", left);
		}
	}
	//-----------------------------------------------------------------
	// if there is another item that needs verification
	//-----------------------------------------------------------------
	else
	{
		DEBUG_ADD_ERR( "[Error] There is another item in ItemCheckBuffer");
	}
}


//-----------------------------------------------------------------------------
//
// When an Item is worn on Gear
//
//-----------------------------------------------------------------------------				
void
UIMessageManager::Execute_UI_ITEM_DROP_TO_GEAR(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ADD_ITEM_TO_GEAR");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}

	// left is cast to the race's gear enum below: refuse a slot past its
	// last (MAX_GEAR_* is the count, no slot).
	const intptr_t lastSlot = g_pPlayer->GetRace()==RACE_SLAYER ? intptr_t(MSlayerGear::MAX_GEAR_SLAYER)
		: g_pPlayer->GetRace()==RACE_VAMPIRE ? intptr_t(MVampireGear::MAX_GEAR_VAMPIRE)
		: intptr_t(MOustersGear::MAX_GEAR_OUSTERS);
	if (left < 0 || left >= lastSlot)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] UI_ITEM_DROP_TO_GEAR slot=%d", static_cast<int>(left));
		return;
	}
	//
	// left = slot enum
	// void_ptr = MItem * the item to be swapped (may be NULL.)
	//

	if (!g_bWatchMode)
	{
		//-----------------------------------------------------------------
		// if there is nothing to verify
		//-----------------------------------------------------------------
		if (g_pPlayer->IsItemCheckBufferNULL()
			&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
		{
			MItem* pMouseItem = (MItem*)gpC_mouse_pointer->GetPickUpItem();	// item being held
			
			//-----------------------------------------------------------------
			// Something must be held on the mouse.
			//-----------------------------------------------------------------
			if (pMouseItem!=NULL)
			{
				//-----------------------------------------------------------------
				// If the situation allows wearing..
				//-----------------------------------------------------------------
				if (g_pPlayer->SetAddonItem( pMouseItem ))
				{				
					MItem* pRemovedItem = NULL;

					// mouse --> gear
					switch(g_pPlayer->GetRace())
					{
					case RACE_SLAYER:
						g_pSlayerGear->ReplaceItem( pMouseItem, (MSlayerGear::GEAR_SLAYER)left, pRemovedItem );
						
						//-----------------------------------------------------------
						// When a belt is put down or picked up, the quick item must be reset.
						//-----------------------------------------------------------
						if ((MSlayerGear::GEAR_SLAYER)left == MSlayerGear::GEAR_SLAYER_BELT)
						{
							gC_vs_ui.ResetSlayerQuickItemSize();
						}
						break;

					case RACE_VAMPIRE:
						g_pVampireGear->ReplaceItem( pMouseItem, (MVampireGear::GEAR_VAMPIRE)left, pRemovedItem );
						break;

					case RACE_OUSTERS:
						g_pOustersGear->ReplaceItem( pMouseItem, (MOustersGear::GEAR_OUSTERS)left, pRemovedItem );
						break;

					default:
						break;
					}

					UI_DropItem();

					//-----------------------------------------------------------
					// if swapped
					//-----------------------------------------------------------
					if (pRemovedItem!=NULL)
					{
						UI_PickUpItem( pRemovedItem );
					}								

					//----------------------------------------------------
					// Skill check
					//----------------------------------------------------
					g_pSkillAvailable->SetAvailableSkills();

						//---------------------------------------------------
						// Tell the server that item1 that was on the mouse
						// was added to the Inventory, by sending a packet.
						//---------------------------------------------------
						CGAddMouseToGear _CGAddMouseToGear;
						_CGAddMouseToGear.setObjectID( pMouseItem->GetID() );
						_CGAddMouseToGear.setSlotID( pMouseItem->GetItemSlot() );//pItem->GetItemSlot() );
						
						g_pSocket->sendPacket( &_CGAddMouseToGear );

						DEBUG_ADD_FORMAT("[UI] CGAddMouseToGear id=%d class=%d type=%d dropSlot=%d sentSlot=%d twoHand=%d",
							pMouseItem->GetID(), (int)pMouseItem->GetItemClass(), (int)pMouseItem->GetItemType(),
							left, (int)pMouseItem->GetItemSlot(), pMouseItem->IsGearSlotTwoHand() ? 1 : 0);
						

					//----------------------------------------------------
					// Help according to the worn item
					//----------------------------------------------------
//					__BEGIN_HELP_EVENT
//						ITEM_CLASS itemClass = pMouseItem->GetItemClass();
//
//						//----------------------------------------------------
//						// gun
//						//----------------------------------------------------
//						if (pMouseItem->IsGunItem())
//						{
//							// [Help] when a gun is worn
////							ExecuteHelpEvent( HE_ITEM_WEAR_GUN );
//						}
//						//----------------------------------------------------
//						// belt
//						//----------------------------------------------------
//						else if (itemClass==ITEM_CLASS_BELT)
//						{
//							// [Help] when a belt is worn
////							ExecuteHelpEvent( HE_ITEM_WEAR_BELT );
//						}
//						//----------------------------------------------------
//						// sword
//						//----------------------------------------------------
//						else if (itemClass==ITEM_CLASS_SWORD)
//						{
//							// [Help] when a sword is worn
////							ExecuteHelpEvent( HE_ITEM_WEAR_SWORD );
//						}
//						//----------------------------------------------------
//						// blade
//						//----------------------------------------------------
//						else if (itemClass==ITEM_CLASS_BLADE)
//						{
//							// [Help] when a blade is worn
////							ExecuteHelpEvent( HE_ITEM_WEAR_BLADE );
//						}
//						//----------------------------------------------------
//						// cross
//						//----------------------------------------------------
//						else if (itemClass==ITEM_CLASS_CROSS)
//						{
//							// [Help] when a cross is worn
////							ExecuteHelpEvent( HE_ITEM_WEAR_CROSS );
//						}
//						//----------------------------------------------------
//						// mace
//						//----------------------------------------------------
//						else if (itemClass==ITEM_CLASS_MACE)
//						{
//							// [Help] when a mace is worn
////							ExecuteHelpEvent( HE_ITEM_WEAR_MACE );
//						}				
//					__END_HELP_EVENT
				}		
			}
			else
			{
				DEBUG_ADD_ERR( "[Error] Mouse Item is NULL" );
			}
		}
		//-----------------------------------------------------------------
		// if there is another item that needs verification
		//-----------------------------------------------------------------
		else
		{
			DEBUG_ADD_ERR( "[Error] There is another item in ItemCheckBuffer");
		}
	}
	else
	{
		DEBUG_ADD( "[Watch Mode]");
	}

	//-------------------------------------------------
	// Only possible when the Player is standing still..
	//-------------------------------------------------
	//if (g_pPlayer->IsStop())
	{
		//-----------------------------------------
		// the player equips pItem
		//-----------------------------------------
		//g_pPlayer->SetAddonItem( pItem );

		//-----------------------------------------
		// Equip the Item on Gear in the UI.
		//-----------------------------------------
		//gC_vs_ui.DropItem();
	}
	//else
	{
	//	(*g_pSlayerGear).AddItem( pItem, (enum MSlayerGear::GEAR_SLAYER)left );
	}
}


//-----------------------------------------------------------------------------
//
// When an item in the QuickSlot is picked up with the mouse
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_PICKUP_FROM_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	DEBUG_ADD("[UI] UI_ITEM_PICKUP_FROM_QUICKSLOT");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}

	//
	// left = slot enum
	// void_ptr = MItem *
	//
	if (void_ptr == NULL)	// no item to lift
		return;

	MOustersArmsBand* pQuickSlot = NULL;
		
	if( g_pPlayer->IsOusters() )
	{
		if( g_pArmsBand1 == NULL )
			pQuickSlot = g_pArmsBand2;
		else
		{
			if( g_pArmsBand1->GetPocketNumber() > left )
				pQuickSlot = g_pArmsBand1;
			else
				pQuickSlot = g_pArmsBand2;
		}
		
		if( pQuickSlot == NULL )
			return;
	}
	else if (g_pPlayer->IsSlayer() && g_pQuickSlot == NULL)
		return;	// no belt, no quick slot to lift from
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		MItem* pItem = (MItem*)void_ptr;

			//---------------------------------------------------
			// Tell the server that the item that was in the QuickSlot
			// was added to the Inventory, by sending a packet.
			//---------------------------------------------------
			CGAddQuickSlotToMouse _CGAddQuickSlotToMouse;
			_CGAddQuickSlotToMouse.setObjectID( pItem->GetID() );
			if( pQuickSlot != NULL)
			{
				if( pQuickSlot == g_pArmsBand2 )
					_CGAddQuickSlotToMouse.setSlotID( pItem->GetItemSlot() + 3 );
				else
					_CGAddQuickSlotToMouse.setSlotID( pItem->GetItemSlot() );
			} else
				_CGAddQuickSlotToMouse.setSlotID( pItem->GetItemSlot() );
			
			g_pSocket->sendPacket( &_CGAddQuickSlotToMouse );
			

		// Remove it from the QuickSlot right away and make the mouse hold it.
		
		if( g_pPlayer->IsSlayer() )
			g_pQuickSlot->RemoveItem( pItem->GetID() );
		else
		{
			if( pQuickSlot != NULL )
				pQuickSlot->RemoveItem( pItem->GetID() );
		}

		UI_PickUpItem(pItem);
	}
}


//-----------------------------------------------------------------------------
//
// When an item in the Inventory is picked up with the mouse
//
//-----------------------------------------------------------------------------				
void
UIMessageManager::Execute_UI_ITEM_PICKUP_FROM_INVENTORY(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_ITEM_PICKUP_FROM_INVENTORY");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	//
	// left = grid x
	// right = grid y
	// void_ptr = MItem *
	//
	//if (g_pTempInformation->Mode==TempInformation::MODE_NULL)
	{
		MItem* pItem = (MItem*)void_ptr;
		if (pItem == NULL)
			return;

		BOOL bSendPacket = TRUE;
		
		BOOL bSendPacketTradeRemove = FALSE;

		BOOL bAcceptMyTrade = (g_pTradeManager!=NULL && g_pTradeManager->IsAcceptMyTrade());
		BOOL bAcceptOtherTrade = (g_pTradeManager!=NULL && g_pTradeManager->IsAcceptOtherTrade());

		//---------------------------------------------------------
		// Verification is required when OK was pressed during a trade.
		//---------------------------------------------------------
		if (bAcceptMyTrade)
		{
			if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL
				&& g_pPlayer->IsItemCheckBufferNULL())
			{	
				g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY);
				g_pTempInformation->Value1 = pItem->GetID();
				g_pTempInformation->Value2 = left;
				g_pTempInformation->Value3 = right;
				g_pTempInformation->pValue = pItem;

				g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_PICKUP_FROM_INVENTORY );

				if (pItem->IsTrade())
				{
					bSendPacketTradeRemove = TRUE;
				}
			}
			else
			{
				bSendPacket = FALSE;
			}
		}
		//---------------------------------------------------------
		// When lifting an item that was checked for trade,
		//---------------------------------------------------------
		else if (gC_vs_ui.IsRunningExchange())
		{
			//---------------------------------------------------
			// If it was a selected item.. remove it
			//---------------------------------------------------
			if (pItem->IsTrade())
			{
				UI_PickUpItem( pItem );

				bSendPacketTradeRemove = TRUE;
				
				// if the other person has pressed OK..
				if (bAcceptOtherTrade)
				{
					g_pTradeManager->RefuseOtherTrade();
				}
			}
			//---------------------------------------------------
			// Otherwise.. manually send an OK cancel.
			//---------------------------------------------------
			else
			{
				UI_PickUpItem( pItem );

				/*
				if (bAcceptMyTrade)
				{
					#ifdef CONNECT_SERVER
						CGTradeFinish _CGTradeFinish;
						_CGTradeFinish.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeFinish.setCode( CG_TRADE_FINISH_RECONSIDER );

						g_pSocket->sendPacket( &_CGTradeFinish );					
					#endif
				}
				*/
			}
		}
		else
		{
			UI_PickUpItem( pItem );
		}

		//---------------------------------------------------------
		// SendPacket - Trade Remove Item
		//---------------------------------------------------------
		if (bSendPacketTradeRemove)
		{
				CGTradeRemoveItem _CGTradeRemoveItem;
				_CGTradeRemoveItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
				_CGTradeRemoveItem.setItemObjectID( pItem->GetID() );

				g_pSocket->sendPacket( &_CGTradeRemoveItem );

				g_pTradeManager->SetNextAcceptTime();
		}

		//---------------------------------------------------------
		// SendPacket - Inventory to Mouse
		//---------------------------------------------------------
		if (bSendPacket)
		{
				//---------------------------------------------------
				// Tell the server that the item that was in the Inventory
				// was added to the Inventory, by sending a packet.
				//---------------------------------------------------
				CGAddInventoryToMouse _CGAddInventoryToMouse;
				_CGAddInventoryToMouse.setObjectID( pItem->GetID() );
				_CGAddInventoryToMouse.setX( pItem->GetGridX() );
				_CGAddInventoryToMouse.setY( pItem->GetGridY() );
				
				g_pSocket->sendPacket( &_CGAddInventoryToMouse );				
		}
	}
}


//-----------------------------------------------------------------------------
//
// When an item worn on Gear is picked up with the mouse
//
//-----------------------------------------------------------------------------				
void
UIMessageManager::Execute_UI_ITEM_PICKUP_FROM_GEAR(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	DEBUG_ADD("[UI] UI_REMOVE_ITEM_FROM_GEAR");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	//
	// left = slot enum
	// void_ptr = MItem *
	//

	if (!g_bWatchMode)
	{
		int itemSlot = static_cast<int>(left);
		MItem* pItem = (MItem*)void_ptr;	// the item in the gear

		if (pItem!=NULL)
		{
			//-------------------------------------------------
			// Only when the Player is standing still.
			//-------------------------------------------------
			//if (g_pPlayer->IsStop())
			{
				//---------------------------------------------
				// Unequip the AddonItem.
				//---------------------------------------------
				if (g_pPlayer->RemoveAddonItem( pItem ))
				{
					MItem* pRemovedItem = NULL;

					//---------------------------------------------
					// Remove it from the gear.
					//---------------------------------------------
					switch(g_pPlayer->GetRace())
					{
					case RACE_SLAYER:
						pRemovedItem = g_pSlayerGear->RemoveItem( (MSlayerGear::GEAR_SLAYER)itemSlot );
						
						//-----------------------------------------------------------
						// When a belt is put down or picked up, the quick item must be reset.
						//-----------------------------------------------------------
						if (pRemovedItem!=NULL 
							&& pRemovedItem->GetItemSlot() == MSlayerGear::GEAR_SLAYER_BELT)
						{
							gC_vs_ui.ResetSlayerQuickItemSize();
						}					
						break;

					case RACE_VAMPIRE:
						pRemovedItem = g_pVampireGear->RemoveItem( (MVampireGear::GEAR_VAMPIRE)itemSlot );
						break;

					case RACE_OUSTERS:
						pRemovedItem = g_pOustersGear->RemoveItem( (MOustersGear::GEAR_OUSTERS)itemSlot );
						break;

					default:
						break;
					}


					//---------------------------------------------
					// Make the UI pick up the Gear's Item.
					//---------------------------------------------
					if (pRemovedItem!=NULL)
					{
						UI_PickUpItem( pRemovedItem );
						
							//---------------------------------------------------
							// Tell the server that the item that was in the Inventory
							// was added to the Inventory, by sending a packet.
							//---------------------------------------------------
							CGAddGearToMouse _CGAddGearToMouse;
							_CGAddGearToMouse.setObjectID( pRemovedItem->GetID() );
							_CGAddGearToMouse.setSlotID( pRemovedItem->GetItemSlot() );
							
							g_pSocket->sendPacket( &_CGAddGearToMouse );							
					}

					//----------------------------------------------------
					// Skill check
					//----------------------------------------------------
					g_pSkillAvailable->SetAvailableSkills();
				}			
			}
			//else
			{
			//	(*g_pSlayerGear).AddItem( pItem, (enum MSlayerGear::GEAR_SLAYER)left );
			}
		}
	}
	else
	{
		DEBUG_ADD( "[Watch Mode]");		
	}
}

//-----------------------------------------------------------------------------
//
// When an Item is used from the Inventory
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_USE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ITEM_USE");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	//
	// void_ptr = MItem *
	//
	MItem* pItem = g_pInventory->GetItemToModify( static_cast<TYPE_OBJECTID>(left) );//(MItem*)void_ptr;

	if (pItem!=NULL && g_pPlayer->IsItemCheckBufferNULL())
	{
		// Let's use only items of our own race -_-;
		if((g_pPlayer->IsSlayer() && pItem->IsSlayerItem()) ||
			(g_pPlayer->IsVampire() && pItem->IsVampireItem()) ||
			(g_pPlayer->IsOusters() && pItem->IsOustersItem()))
		{
			pItem->UseInventory();
		}
	}
	//----------------------------------------------------
	// item check buffer not null
	//----------------------------------------------------
	else
	{
		#ifdef OUTPUT_DEBUG
			if (pItem==NULL)
			{
				DEBUG_ADD("[Wait] Item is NULL");
			}
			else 
			{
				DEBUG_ADD("[Wait] Item Check Buffer is Not NULL");
			}
		#endif
	}
}

//-----------------------------------------------------------------------------
//
// When an Item is used from the QuickSlot
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_USE_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ITEM_USE_QUICKSLOT");

	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	//
	// left = slot
	// right = by FunctionKey
	//
	//MItem* pItem = (MItem*)void_ptr;
	if (g_pPlayer!=NULL && ((g_pQuickSlot!=NULL && g_pPlayer->IsSlayer()) || 
		(g_pPlayer->IsOusters() && (g_pArmsBand1 != NULL || g_pArmsBand2 != NULL)) ) )
	{
		MSlotItemManager *pQuickSlot = g_pQuickSlot;

		int slot = static_cast<int>(left);
//		const MItem* pSlotItem = g_pQuickSlot->GetItem( left );
		MItem * pSlotItem = NULL;
		
		if( g_pPlayer->IsOusters() )
		{
			if( g_pArmsBand1 == NULL )
				pQuickSlot = g_pArmsBand2;
			else
			{
				if( g_pArmsBand1->GetPocketNumber() > slot )
					pQuickSlot = g_pArmsBand1;
				else
				{
					pQuickSlot = g_pArmsBand2;
					slot -= g_pArmsBand1->GetPocketNumber();
				}
			}
		}

		if( pQuickSlot == NULL )
			return;

		pSlotItem = pQuickSlot->GetItem( slot );

		if (pSlotItem!=NULL)
		{
			MItem *pItem = NULL;
			
			pItem = pQuickSlot->GetItemToModify( pSlotItem->GetID() );

			if (g_pPlayer->IsItemCheckBufferNULL())
			{
//				__BEGIN_HELP_EVENT
//					if (bByFunctionKey)
//					{
//						// [Help] was it an item used via a functionKey?
////						ExecuteHelpEvent( HE_PRESSED_FUNCTION_KEY_FOR_QUICKITEM );
//					}
//				__END_HELP_EVENT

				// Let's use only items of our own race -_-;
				if((g_pPlayer->IsSlayer() && pItem->IsSlayerItem()) ||
					(g_pPlayer->IsVampire() && pItem->IsVampireItem()) ||
					(g_pPlayer->IsOusters() && pItem->IsOustersItem()))
				{
					pItem->UseQuickItem();
				}
			}
			//----------------------------------------------------
			// item check buffer not null
			//----------------------------------------------------
			else
			{
				DEBUG_ADD("[Wait] Item Check Buffer is Not NULL");
			}
		}
		else
		{
			DEBUG_ADD_FORMAT("No usable item in slot=%d", left);
		}
	}
	else
	{
		DEBUG_ADD("Quickslot use ignored: no quickslot available");
	}

}

//-----------------------------------------------------------------------------
//
// When adding the mouse's Item to an Inventory Item
//
//-----------------------------------------------------------------------------
// The stacking part is processed without verification.
// If it exceeds the max count, only change the count,
// otherwise.. what was held disappears.
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_INSERT_FROM_INVENTORY(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_ITEM_INSERT_FROM_INVENTORY");

	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}

	// gpC_mouse_pointer->GetPickUpItem()		// the one being held
	//void_ptr = MItem *		// where it goes in
	// Inventory coordinates (left,right)
	MItem* pItem = (MItem*)void_ptr;
	MItem* pMouseItem = gpC_mouse_pointer->GetPickUpItem();

	if (pMouseItem==NULL)
	{
		DEBUG_ADD_ERR("[Error] Mouse Item is NULL");
		return;
	}

	if (pItem==NULL)
	{
		DEBUG_ADD_ERR("[Error] InvenItem is NULL");
		return;
	}
	// add by Coffee 2006.11.4  修正任务物品不能叠加
	if (pItem->IsQuestItem())
	{
		return;
	}

	TYPE_OBJECTID mouseItemID = ((pMouseItem==NULL)?OBJECTID_NULL : pMouseItem->GetID());

	if (pItem->IsPileItem())
	{
		if (pItem->GetItemClass()==pMouseItem->GetItemClass()
			&& pItem->GetItemType()==pMouseItem->GetItemType()
			&& pItem->GetNumber() < pItem->GetMaxNumber())
		{
			BOOL bSendPacket = TRUE;

			BOOL bAcceptMyTrade = (g_pTradeManager!=NULL && g_pTradeManager->IsAcceptMyTrade());
			BOOL bAcceptOtherTrade = (g_pTradeManager!=NULL && g_pTradeManager->IsAcceptOtherTrade());

			//---------------------------------------------------------
			// Verification is required when OK was pressed during a trade.
			//---------------------------------------------------------
			if (bAcceptMyTrade)
			{	
				// In this state the UI does not insert it directly.
				// pItem is the item being held.
				if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL
					&& g_pPlayer->IsItemCheckBufferNULL())
				{	
					g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_INSERT_TO_INVENTORY);
					g_pTempInformation->Value1 = pItem->GetID();
					g_pTempInformation->Value2 = left;
					g_pTempInformation->Value3 = right;
					g_pTempInformation->pValue = pItem;

					g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY );
				}
				else
				{
					bSendPacket = FALSE;
				}
			}
			else
			{
				//g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY );
				MItem* pMouseItem = gpC_mouse_pointer->GetPickUpItem();

				//----------------------------------------------------
				// Add pMouseItem to pItem.
				//----------------------------------------------------
				int total = pMouseItem->GetNumber() + pItem->GetNumber();
				//yckou
				if(!bAcceptOtherTrade && (pItem->GetItemClass() == ITEM_CLASS_MONEY))
				{
					if(total>99)
					{
						pMouseItem->SetNumber( total - 99 );
						pItem->SetNumber( 99 );
					}
					else
					{
						// if all of it can be added to pItem
						pItem->SetNumber( total );
						UI_DropItem();
						
						delete pMouseItem;
					}
				}
				else
				//end yckou
				
				if ( static_cast<TYPE_ITEM_NUMBER>(total) > pItem->GetMaxNumber())
				{
					// if it exceeds the limit
					pMouseItem->SetNumber( total - pItem->GetMaxNumber() );
					pItem->SetNumber( pItem->GetMaxNumber() );
				}
				else
				{
					// if all of it can be added to pItem
					pItem->SetNumber( total );
					UI_DropItem();

					delete pMouseItem;
				}

				//----------------------------------------------------
				// if it is an item selected during the trade
				//----------------------------------------------------
				// Remove and Add - -;
				if (gC_vs_ui.IsRunningExchange() && pItem->IsTrade())
				{
						//----------------------------------------------------
						// remove what was placed
						//----------------------------------------------------
						CGTradeRemoveItem _CGTradeRemoveItem;
						_CGTradeRemoveItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeRemoveItem.setItemObjectID( pItem->GetID() );

						g_pSocket->sendPacket( &_CGTradeRemoveItem );

						g_pTradeManager->SetNextAcceptTime();

						
						//----------------------------------------------------
						// add item --> increase count
						//----------------------------------------------------
						CGAddMouseToInventory _CGAddMouseToInventory;
						_CGAddMouseToInventory.setObjectID( mouseItemID );
						_CGAddMouseToInventory.setInvenX( pItem->GetGridX() );
						_CGAddMouseToInventory.setInvenY( pItem->GetGridY() );								

						g_pSocket->sendPacket( &_CGAddMouseToInventory );

						bSendPacket = FALSE;

						
						//----------------------------------------------------
						// add the item to the trade window again
						//----------------------------------------------------
						CGTradeAddItem _CGTradeAddItem;
						_CGTradeAddItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeAddItem.setItemObjectID( pItem->GetID() );

						g_pSocket->sendPacket( &_CGTradeAddItem );
						

					// if the other person has pressed OK..
					if (bAcceptOtherTrade)
					{
						g_pTradeManager->RefuseOtherTrade();
					}
				}

				PlaySound( pItem->GetInventorySoundID() );
			}

			//----------------------------------------------------
			// When connected to the Server
			//----------------------------------------------------
				if (bSendPacket)
				{
					CGAddMouseToInventory _CGAddMouseToInventory;
					_CGAddMouseToInventory.setObjectID( mouseItemID );
					_CGAddMouseToInventory.setInvenX( pItem->GetGridX() );
					_CGAddMouseToInventory.setInvenY( pItem->GetGridY() );								

					g_pSocket->sendPacket( &_CGAddMouseToInventory );

					
					//----------------------------------------------------
					// Handling according to each Item class
					//----------------------------------------------------
					//if (pItem->GetItemClass()==ITEM_CLASS_MONEY)
					//{
					//}
				}
				
		}
	}
}

//-----------------------------------------------------------------------------
//
// When adding the mouse's Item to a Gear Item
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_INSERT_FROM_GEAR(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	DEBUG_ADD("[UI] UI_ITEM_INSERT_FROM_GEAR");

	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}

	// gpC_mouse_pointer->GetPickUpItem()		// the one being held
	//void_ptr = MItem *		// where it goes in
	// Gear Slot(left)

	if (!g_bWatchMode
		&& g_pPlayer->IsItemCheckBufferNULL())
	{
		MItem* pItem = (MItem*)void_ptr;
		MItem* pMouseItem = gpC_mouse_pointer->GetPickUpItem();
		if (pItem == NULL || pMouseItem == NULL)
			return;

		//----------------------------------------------------
		// Magazine
		//----------------------------------------------------
		if (pItem->GetItemClass()==ITEM_CLASS_MAGAZINE)
		{
			//----------------------------------------------------
			// When connected to the Server
			//----------------------------------------------------
				//---------------------------------------------------
				// Tell the server that the item that was on the mouse
				// was added to the Inventory, by sending a packet.
				//---------------------------------------------------
				CGAddMouseToGear _CGAddMouseToGear;
				_CGAddMouseToGear.setObjectID( pMouseItem->GetID() );
				_CGAddMouseToGear.setSlotID( pItem->GetItemSlot() );
				
				g_pSocket->sendPacket( &_CGAddMouseToGear );

				
				// Must wait for verification.
				g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_GEAR );

				// Once verified, m_p_current_pickup_item must be added to pItem.
		}
		//----------------------------------------------------
		// Otherwise.
		//----------------------------------------------------
		else
		{
			// unused.. - -;
		}
	}
	else
	{
		DEBUG_ADD( "[Watch Mode]");
	}
}


//-----------------------------------------------------------------------------
//
// When adding the mouse's Item to a QuickSlot Item
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_INSERT_FROM_QUICKSLOT(intptr_t left, intptr_t right, void* void_ptr)
{				
	(void)right;
	DEBUG_ADD("[UI] UI_ITEM_INSERT_FROM_QUICKSLOT");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}

	
	if (g_pPlayer->IsItemCheckBufferNULL())
	{
		// gpC_mouse_pointer->GetPickUpItem()		// the one being held
		//void_ptr = MItem *		// where it goes in
		// QuickSlot coordinates (left)
		MItem* pItem = (MItem*)void_ptr;
		MItem* pMouseItem = (MItem*)gpC_mouse_pointer->GetPickUpItem();
		if (pItem == NULL || pMouseItem == NULL)
			return;

		if (pItem->GetItemClass()==pMouseItem->GetItemClass()
			&& pItem->GetItemType()==pMouseItem->GetItemType())
		{
			//----------------------------------------------------
			// When connected to the Server
			//----------------------------------------------------
				CGAddMouseToQuickSlot _CGAddMouseToQuickSlot;
				_CGAddMouseToQuickSlot.setObjectID( pMouseItem->GetID() );
				if ( g_pPlayer->IsOusters() )
				{
					int slotID = static_cast<int>(left);
					
					if( g_pArmsBand1 != NULL &&g_pArmsBand1->GetPocketNumber() > slotID )
						_CGAddMouseToQuickSlot.setSlotID( slotID );
					else
					{
						_CGAddMouseToQuickSlot.setSlotID( pItem->GetItemSlot() + 3);
					}
				}
				else
					_CGAddMouseToQuickSlot.setSlotID( pItem->GetItemSlot() );

				g_pSocket->sendPacket( &_CGAddMouseToQuickSlot );

				
				// Must wait in order to be verified.
				// Once verified, m_p_current_pickup_item must be added to pItem.
				//g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY );
				//MItem* pMouseItem = (MItem*)gpC_mouse_pointer->GetPickUpItem();

				//----------------------------------------------------
				// Add pMouseItem to pItem.
				//----------------------------------------------------
				int total = pMouseItem->GetNumber() + pItem->GetNumber();
				if ( static_cast<TYPE_ITEM_NUMBER>(total) > pItem->GetMaxNumber() )
				{
					// if it exceeds the limit
					pMouseItem->SetNumber( total - pItem->GetMaxNumber() );
					pItem->SetNumber( pItem->GetMaxNumber() );
				}
				else
				{
					// if all of it can be added to pItem
					pItem->SetNumber( total );
					UI_DropItem();

					delete pMouseItem;
				}

				PlaySound( pItem->GetInventorySoundID() );

				//----------------------------------------------------
				// Handling according to each Item class
				//----------------------------------------------------
				//if (pItem->GetItemClass()==ITEM_CLASS_MONEY)
				//{
				//}
				
		}
	}
}


//-----------------------------------------------------------------------------
//
// When closing the shop,
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_SHOP(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_SHOP");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseShop();

		// remove the shop
		gC_vs_ui.SetShop( NULL );


		// Make it possible to select something again?
		g_pUIDialog->ShowPCTalkDlg();
	}
}

//-----------------------------------------------------------------------------
//
// The player picked an item to buy from the shop.
//
//-----------------------------------------------------------------------------
//
// void_ptr = MShop
// left = slot
// right = item count
//
void
UIMessageManager::Execute_UI_BUY_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{				
	DEBUG_ADD("[UI] UI_BUY_ITEM");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	// Only when no other request is waiting for the server's answer, and
	// with the NPC's talk box, which names the seller
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL
		&& g_pPCTalkBox != NULL)
	{
		int index = static_cast<int>(left);					// the shelf slot of the item to buy
		int number = static_cast<int>(right);					// how many to buy
		MShop* pShop = (MShop*)void_ptr;	// the shop
		int npcID = (*g_pPCTalkBox).GetNPCID();					

		if (pShop!=NULL)
		{					
			MShopShelf* pShopShelf = pShop->GetCurrentShelf();
			
			if (pShopShelf!=NULL)
			{
				MItem* pItem = pShopShelf->GetItem( index );
				
				if (pItem!=NULL)
				{
					POINT point;
					
					int oldNumber = pItem->GetNumber();					

					//-------------------------------------------------
					// Whether the player can pay for it.
					//-------------------------------------------------							
					bool bBuyPossible = false;
					//GAME_STRINGID buyImpossibleMessage = STRING_MESSAGE_CANNOT_BUY;
					int buyImpossibleMessage = STRING_MESSAGE_CANNOT_BUY;

					switch (pShop->GetShopType())
					{
						//-------------------------------------------------
						// A shop that sells for money
						//-------------------------------------------------
						case MShop::SHOP_NORMAL :
						{
							// The total with the castle tax, which past the server's
							// int range is 2^31, more than any purse.
							const unsigned price = g_pPriceManager->GetPurchasePrice(pItem, number);
							const int money = (*g_pMoneyManager).GetMoney();

							bBuyPossible = (money >= 0 && price <= (unsigned)money);

							buyImpossibleMessage = STRING_MESSAGE_CANNOT_BUY_NO_MONEY;
						}
						break;

						//-------------------------------------------------
						// An event shop that sells for stars
						//-------------------------------------------------
						case MShop::SHOP_EVENT_STAR :
						{
							STAR_ITEM_PRICE starPrice;

							g_pPriceManager->GetItemPrice( pItem, starPrice );

							if (starPrice.type!=-1 && starPrice.number!=0)
							{
								// How many stars of the price's colour the player has.
								MItemClassTypeNumberFinder starFinder(ITEM_CLASS_EVENT_STAR, 
																		starPrice.type);


								((MItemManager*)g_pInventory)->FindItem( starFinder );

								// At least as many as the price.
								bBuyPossible = (starFinder.GetTotalNumber() >= starPrice.number);

								buyImpossibleMessage = STRING_MESSAGE_CANNOT_BUY_NO_STAR;
							}							
							
						}
						break;
					}

					//-------------------------------------------------
					// The player can pay
					//-------------------------------------------------
					if (bBuyPossible)
					{								
						//-------------------------------------------------
						// Where the item goes in the inventory
						//-------------------------------------------------
						pItem->SetNumber( number );

						if (g_pInventory->GetFitPosition(pItem, point))
						{								
								CGShopRequestBuy _CGShopRequestBuy;
								_CGShopRequestBuy.setObjectID( npcID );

								_CGShopRequestBuy.setShopType( pShop->GetCurrent() );
								_CGShopRequestBuy.setShopIndex( index );

								_CGShopRequestBuy.setX(static_cast<Coord_t>(point.x));
								_CGShopRequestBuy.setY(static_cast<Coord_t>(point.y));

								_CGShopRequestBuy.setItemNum( number );

								g_pSocket->sendPacket( &_CGShopRequestBuy );

								
								//-------------------------------------------------
								// Remember the request, to check the server's answer against
								//-------------------------------------------------
								(*g_pTempInformation).Mode	= TempInformation::MODE_SHOP_BUY;
								(*g_pTempInformation).Value1 = pShop->GetCurrent();
								(*g_pTempInformation).Value2 = index;
								(*g_pTempInformation).Value3 = point.x;
								(*g_pTempInformation).Value4 = point.y;
								(*g_pTempInformation).pValue = (void*)pShop;

								//-------------------------------------------------
								// No other item can be moved until the answer comes
								//-------------------------------------------------
								UI_LockItemTrade();
						}
						else
						{
							// The inventory is full, so the item cannot be bought.
							g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_NO_SPACE ].GetString());
							
							// With no room in the inventory, the help suggests buying a storage box.
							ExecuteHelpEvent( HELP_EVENT_STORAGE_BUY );
						}

						pItem->SetNumber( oldNumber );

					}
					//-------------------------------------------------
					// Not enough money or stars
					//-------------------------------------------------					
					else
					{
						g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[buyImpossibleMessage].GetString());
					}
				}
				else
				{
					// No item in that shelf slot
					DEBUG_ADD_FORMAT_ERR("[Error] There is NO Item in index=%d", index);
				}
			}
			else
			{
				// The shop has no current shelf
				DEBUG_ADD_FORMAT_ERR("[Error] There is NO Shelf type=%d", (int)pShop->GetCurrent());
			}
		}
		else
		{
			// No shop
			DEBUG_ADD_FORMAT_ERR("[Error] There is NO Shop. npc id=%d", npcID);
		}
	}
}

//-----------------------------------------------------------------------------
//
// When closing the sell dialog
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_SELL_FINISHED(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ITEM_SELL_FINISHED");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	if (g_pTempInformation!=NULL
		&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.FinishItemSelling();

		// Make it possible to select something again?
		g_pUIDialog->ShowPCTalkDlg();
	}
}

//-----------------------------------------------------------------------------
//
// When the item to sell was selected,
//
//-----------------------------------------------------------------------------
//
// gridX   //void_ptr = Mitem*
// gridY
//
void
UIMessageManager::Execute_UI_SELL_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{				
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_SELL_ITEM");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	//MItem* pItem = (MItem*)void_ptr;
	// if there is nothing to verify
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		const MItem* pItem = g_pInventory->GetItem( static_cast<BYTE>(left), static_cast<BYTE>(right) );

		if (pItem!=NULL && g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
		{
			// An Event GiftBox item cannot be put down.
			
			if (//pItem->GetItemClass()!=ITEM_CLASS_EVENT_GIFT_BOX &&
				!pItem->IsUniqueItem() || (g_pTimeItemManager->IsExist( pItem->GetID())))
			{
				//-----------------------------------------------------
				// Send a packet to sell the item.
				//-----------------------------------------------------
					CGShopRequestSell	_CGShopRequestSell;

					_CGShopRequestSell.setObjectID( (*g_pPCTalkBox).GetNPCID() );		// NPC ID	
					_CGShopRequestSell.setOpCode( SHOP_REQUEST_SELL_NORMAL );
					_CGShopRequestSell.setItemObjectID( pItem->GetID() );	// Item Object ID

					g_pSocket->sendPacket( &_CGShopRequestSell );
			
						
					//-------------------------------------------------
					// Set Temp Information for verification
					//-------------------------------------------------
					(*g_pTempInformation).Mode	= TempInformation::MODE_SHOP_SELL;
					(*g_pTempInformation).pValue = (void*)pItem;

					//-------------------------------------------------
					// so that other items cannot be accessed..
					//-------------------------------------------------
					UI_LockItemTrade();
			}
			else
			{
				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_SELL ].GetString());
			}
		}
		else
		{
			DEBUG_ADD_ERR("[Error] Item Is NULL");
		}
	}
}


//-----------------------------------------------------------------------------
//
// When the item to sell was selected,
//
//-----------------------------------------------------------------------------
//
// gridX   //void_ptr = Mitem*
// gridY
//
void
UIMessageManager::Execute_UI_SELL_ALL_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{				
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_SELL_ALL_ITEM");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	//MItem* pItem = (MItem*)void_ptr;
	// if there is nothing to verify
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		//-----------------------------------------------------
		// Send a packet to sell the item.
		//-----------------------------------------------------
			CGShopRequestSell	_CGShopRequestSell;

			_CGShopRequestSell.setObjectID( (*g_pPCTalkBox).GetNPCID() );		// NPC ID	
			_CGShopRequestSell.setOpCode( SHOP_REQUEST_SELL_ALL_SKULL );
			_CGShopRequestSell.setItemObjectID( 0 );	// Item Object ID

			g_pSocket->sendPacket( &_CGShopRequestSell );
	
				
			//-------------------------------------------------
			// Set Temp Information for verification
			//-------------------------------------------------
			(*g_pTempInformation).Mode	= TempInformation::MODE_SHOP_SELL_ALL_SKULL;
			
			//-------------------------------------------------
			// so that other items cannot be accessed..
			//-------------------------------------------------
			UI_LockItemTrade();
	}
}

//-----------------------------------------------------------------------------
//
//	Mouse Focus
// 
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_REMOVE_BACKGROUND_MOUSE_FOCUS(intptr_t left, intptr_t right, void* void_ptr)
{	
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		return;
	}

	g_bUIInput = TRUE;
}

void
UIMessageManager::Execute_UI_BACKGROUND_MOUSE_FOCUS(intptr_t left, intptr_t right, void* void_ptr)
{	
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		return;
	}

	//
	// (left, right) = mouse (x, y)
	//
	g_bUIInput = FALSE;
}
/*
//-----------------------------------------------------------------------------
//
// When closing the Skill View,
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_SKILL_VIEW(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_CLOSE_SKILL_VIEW");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	gC_vs_ui.CloseSkillView();

	// Make it possible to select something again?
	//g_pUIDialog->ShowPCTalkDlg();

	// remove mode
	//(*g_pTempInformation).Mode = TempInformation::MODE_NULL;
}


//-----------------------------------------------------------------------------
//
//	When it is said that a Skill is learned,
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_LEARN_SLAYER_SKILL(intptr_t left, intptr_t right, void* void_ptr)
{
	//
	// left = ACTIONINFO
	//
	DEBUG_ADD("[UI] UI_LEARN_SLAYER_SKILL");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	if ((*g_pTempInformation).Mode == TempInformation::MODE_SKILL_LEARN)
	{
		(*g_pTempInformation).Value3 = left;

		#ifdef	CONNECT_SERVER
			CGLearnSkill _CGLearnSkill;
			
			//_CGLearnSkill.setObjectID( (*g_pPCTalkBox).GetNPCID() );
			_CGLearnSkill.setSkillType( left );
			_CGLearnSkill.setSkillDomainType( (*g_pTempInformation).Value1 );

			g_pSocket->sendPacket( &_CGLearnSkill );

			
		#endif
	}
}
*/
//-----------------------------------------------------------------------------
//
// When closing the PDS,
//
//-----------------------------------------------------------------------------
/*
void
UIMessageManager::Execute_UI_PDS_CLOSED(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_PDS_CLOSED");
	
	gC_vs_ui.AcquirePDSClosedMessage();
}
*/

//-----------------------------------------------------------------------------
//
// Set the values before viewing Slayer info..
//
//-----------------------------------------------------------------------------
/*
void
UIMessageManager::Execute_UI_PLEASE_SET_SLAYER_VALUE(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_PLEASE_SET_SLAYER_VALUE");
	
	//C_VS_UI_SLAYER_PDS::m_str		= g_pPlayer->GetSTR();
	//C_VS_UI_SLAYER_PDS::m_dex		= g_pPlayer->GetDEX();
	//C_VS_UI_SLAYER_PDS::m_int		= g_pPlayer->GetINT();
	//C_VS_UI_SLAYER_PDS::m_hp		= g_pPlayer->GetHP();
	//C_VS_UI_SLAYER_PDS::m_mp		= g_pPlayer->GetMP();
	//C_VS_UI_SLAYER_PDS::m_th		= g_pPlayer->GetTOHIT();
	//C_VS_UI_SLAYER_PDS::m_ac		= g_pPlayer->GetAC();
	//C_VS_UI_SLAYER_PDS::m_cc		= g_pPlayer->GetCARRYWEIGHT();
	//C_VS_UI_SLAYER_PDS::m_str_max	= g_pPlayer->GetSTR(); // (!)
	//C_VS_UI_SLAYER_PDS::m_dex_max	= g_pPlayer->GetDEX(); // (!)
	//C_VS_UI_SLAYER_PDS::m_int_max	= g_pPlayer->GetINT();
	//C_VS_UI_SLAYER_PDS::m_HP_MAX	= g_pPlayer->GetMAX_HP();
	//C_VS_UI_SLAYER_PDS::m_MP_MAX	= g_pPlayer->GetMAX_MP();
	//C_VS_UI_SLAYER_PDS::m_th_max	= g_pPlayer->GetTOHIT(); // (!)
	//C_VS_UI_SLAYER_PDS::m_ac_max	= g_pPlayer->GetAC();	 // (!)
	//C_VS_UI_SLAYER_PDS::m_cc_max	= g_pPlayer->GetCARRYWEIGHT(); // (!)	
}
*/

//-----------------------------------------------------------------------------
//
// When a PCS number is received --> to call another person..
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// When connected
//
//-----------------------------------------------------------------------------
/*
case UI_PCS_CONNECTOR_GRANTED:
//
// Connect with the person in the wait room.
//
// left = pcs number
//
DEBUG_ADD("[UI] UI_PCS_CONNECTOR_GRANTED");

UI_OnLinePCS();
break;
*/

//-----------------------------------------------------------------------------
//
// When ending during a PCS connection..
//
//-----------------------------------------------------------------------------
// Sent the PCS number in the PCS window and pressed END while connecting.
/*
void
UIMessageManager::Execute_UI_END_PCS(intptr_t left, intptr_t right, void* void_ptr) 
{
	DEBUG_ADD("[UI] UI_END_PCS");
	
	//------------------------------------------------
	// Disconnect all connections.
	//------------------------------------------------
	#ifdef	CONNECT_SERVER			
		CGPhoneDisconnect _CGPhoneDisconnect;
			
		for (int i=0; i<3; i++)
		{
			if (g_pUserInformation->OtherPCSNumber[i] != 0)
			{
				_CGPhoneDisconnect.setSlotID( i );

				g_pSocket->sendPacket( &_CGPhoneDisconnect );

			}
		}
	#endif
}
*/

//-----------------------------------------------------------------------------
//
// When ending PCS while Online
//
//-----------------------------------------------------------------------------
/*
void
UIMessageManager::Execute_UI_QUIT_PCS_ONLINE_MODE(intptr_t left, intptr_t right, void* void_ptr)
{
	//
	// Pressed the quit button in PCS online mode.
	//
	DEBUG_ADD("[UI] UI_QUIT_PCS_ONLINE_MODE");
	
	//------------------------------------------------
	// Disconnect all connections.
	//------------------------------------------------
	#ifdef	CONNECT_SERVER			
		CGPhoneDisconnect _CGPhoneDisconnect;
			
		for (int i=0; i<3; i++)
		{
			if (g_pUserInformation->OtherPCSNumber[i] != 0)
			{
				_CGPhoneDisconnect.setSlotID( i );

				g_pSocket->sendPacket( &_CGPhoneDisconnect );

			}
		}
	#endif

	UI_AcquireQuitPCSOnlineModeMessage();
}
*/

//-----------------------------------------------------------------------------
//
// Switch to another person.
//
//-----------------------------------------------------------------------------
/*
void
UIMessageManager::Execute_UI_CHANGE_PCS_CONNECTED_SLOT(intptr_t left, intptr_t right, void* void_ptr)
{
	//
	// Switch to another connected person.
	//
	// left = pcs number
	// right = slot 
	//
	DEBUG_ADD("[UI] UI_CHANGE_PCS_CONNECTED_SLOT");
	
	int slot = right;

	gC_vs_ui.GrantPCSWaitRoomToDuplex( slot );
}
*/

//-----------------------------------------------------------------------------
//
// When ending PCS while Online
//
//-----------------------------------------------------------------------------
// Wait room click! (not yet connected - duplex)
/*
void
UIMessageManager::Execute_UI_PLEASE_PCS_CONNECT_ME(intptr_t left, intptr_t right, void* void_ptr)
{
	//
	// Clicked a wait room that has a person in it. (to send a message)
	//
	// left = pcs number
	// right = slot 
	//
	DEBUG_ADD("[UI] UI_PLEASE_PCS_CONNECT_ME");
	
	int slot = right;

	// Connect with the clicked slot.
	gC_vs_ui.GrantPCSWaitRoomToDuplex( slot );
}
*/

//-----------------------------------------------------------------------------
//
// GameMenu Continue
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_GAMEMENU_CONTINUE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_GAMEMENU_CONTINUE");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	gC_vs_ui.CloseGameMenu();
}

//-----------------------------------------------------------------------------
//
// Minimap Toggle
//
//-----------------------------------------------------------------------------
/*
void
UIMessageManager::Execute_UI_MINIMAP_TOGGLE(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_MINIMAP_TOGGLE");
	
	g_pUserOption->DrawMinimap = !g_pUserOption->DrawMinimap;
}
*/

//-----------------------------------------------------------------------------
//
// Drop Money
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_DROP_MONEY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	return;
	//
	// Drop money from the inventory.
	//
	// left = money
	//
	DEBUG_ADD("[UI] UI_DROP_MONEY");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	//-----------------------------------------------------------------------------
	// The money must not be 0.
	//-----------------------------------------------------------------------------
	if (left > 0)
	{
		if (g_pMoneyManager!=NULL)
		{
				CGDropMoney _CGDropMoney;
				_CGDropMoney.setAmount( static_cast<Gold_t>(left) );

				g_pSocket->sendPacket( &_CGDropMoney );

				

			//-----------------------------------------------
			// reduce the money
			//-----------------------------------------------
			g_pMoneyManager->UseMoney( static_cast<int>(left) );
		}
		else
		{
			DEBUG_ADD_ERR("[Error] MoneyManager is NULL");
		}
	}
}

//-----------------------------------------------------------------------------
//
// Click Bonus Point - when raising bonus points,
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLICK_BONUS_POINT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	//
	// Pressed the button to raise the vampire bonus points.
	//
	// Searching g_char_slot_ingame's bonus points tells how many are left.
	//
	// left = { 0:STR, 1:DEX, 2:INT }
	//
	DEBUG_ADD("[UI] UI_CLICK_BONUS_POINT");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	const int bonusPart[] =
	{
		INC_STR,
		INC_DEX,
		INC_INT
	};

	// left picks one of the three; anything else would read past the table
	if (left < 0 || left >= static_cast<intptr_t>(std::size(bonusPart)))
		return;

	// to remember it...
	if ((*g_pTempInformation).Mode==TempInformation::MODE_NULL)
	{
		int bonusPoint = g_pPlayer->GetBonusPoint();

		//----------------------------------------------
		// when it is a vampire and has bonuspoint,
		//----------------------------------------------
		// 2005, 1, 18, sobeit modify start - promoted Slayers have bonus points.
//		if (!g_pPlayer->IsSlayer()// || g_pPlayer->IsOusters()
//			&& bonusPoint > 0)
		if(bonusPoint > 0)
		// 2005, 1, 18, sobeit modify end - promoted Slayers have bonus points.
		{
			
				CGUseBonusPoint _CGUseBonusPoint;

				_CGUseBonusPoint.setWhich( bonusPart[left] );

				g_pSocket->sendPacket( &_CGUseBonusPoint );

				
				//----------------------------------------------
				// remember the selected part
				//----------------------------------------------
				(*g_pTempInformation).Mode = TempInformation::MODE_BONUSPOINT_USE;
				(*g_pTempInformation).Value1 = bonusPart[left];				
		
				
		}
	}
}


//-----------------------------------------------------------------------------
//
// When the CharInfo button was pressed,
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	//
	// Pressed the character info button. Must show one's own info!
	//
//	DEBUG_ADD("[UI] UI_CHARINFO");
//	
//	
//	if (g_Mode!=MODE_GAME)
//	{
//		DEBUG_ADD("Not Mode MODE_GAME");
//		return;
//	}
//
//
//	if (gC_vs_ui.IsRunningElevator())
//	{
//		DEBUG_ADD("[Execute_UI_CHARINFO] before RunCharInfo-Elevator Running");
//	}
//
//	//----------------------------------------------
//	// Close it if it is open.
//	//----------------------------------------------
//	if (gC_vs_ui.IsRunningCharInfo())
//	{
//		gC_vs_ui.CloseCharInfo();
//	}
//	//----------------------------------------------
//	// Show it.
//	//----------------------------------------------
//	else
//	{
//		// set slot
//
//		// g_char_slot_ingame must keep being set during the game.
//		/*
//		g_char_slot_ingame.sz_name = "yaho";
//		g_char_slot_ingame.bl_vampire = true;//false;
//		g_char_slot_ingame.MP_MAX = 100;
//		g_char_slot_ingame.HP_MAX = 100;
//		g_char_slot_ingame.HP = 100;
//		g_char_slot_ingame.MP = 100;
//		g_char_slot_ingame.bl_female = false;
//
//		g_char_slot_ingame.bonus_point = 5;
//		*/
//		g_char_slot_ingame.sz_name = g_pUserInformation->CharacterID.GetString();
//		g_char_slot_ingame.bl_vampire = g_pPlayer->IsVampire();
//		g_char_slot_ingame.bl_female = !g_pPlayer->IsMale();
//		
//		
//		if (g_pPlayer->IsSlayer())
//		{
//			g_char_slot_ingame.level = 1;//g_pPlayer->GetLEVEL();
//
//			//g_char_slot_ingame.skin_color = g_pPlayer->GetAddonInfo(ADDON_COAT).ColorSet1;
//			//g_char_slot_ingame.hair_color = g_pPlayer->GetAddonInfo(ADDON_HAIR).ColorSet1;
//			g_char_slot_ingame.hair_color = g_pUserInformation->HairColor;
//			g_char_slot_ingame.skin_color = g_pUserInformation->SkinColor;
//		}
//		else
//		{
//			g_char_slot_ingame.level = g_pPlayer->GetVampExp();
//
//			//g_char_slot_ingame.skin_color = g_pPlayer->GetBodyColor1();//pCreatureWear->GetAddonInfo(ADDON_COAT).ColorSet1;
//			//g_char_slot_ingame.hair_color = pCreatureWear->GetAddonInfo(ADDON_HAIR).ColorSet1;
//			g_char_slot_ingame.hair_color = g_pUserInformation->HairColor;
//			g_char_slot_ingame.skin_color = g_pUserInformation->SkinColor;
//		}
//		
//		if (g_pPlayer->IsMale())
//		{
//			switch (g_pUserInformation->FaceStyle)
//			{
//				case HAIR_STYLE1 :
//					g_char_slot_ingame.man_info.face = M_FACE1;
//				break;
//				case HAIR_STYLE2 :
//					g_char_slot_ingame.man_info.face = M_FACE2;
//				break;
//				case HAIR_STYLE3 :
//					g_char_slot_ingame.man_info.face = M_FACE3;
//				break;
//			}
//		}
//		else
//		{
//			switch (g_pUserInformation->FaceStyle)
//			{
//				case HAIR_STYLE1 :
//					g_char_slot_ingame.woman_info.face = W_FACE1;
//				break;
//				case HAIR_STYLE2 :
//					g_char_slot_ingame.woman_info.face = W_FACE2;
//				break;
//				case HAIR_STYLE3 :
//					g_char_slot_ingame.woman_info.face = W_FACE3;
//				break;
//			}
//		}
//
//		g_char_slot_ingame.level = g_pPlayer->GetLEVEL();
//		g_char_slot_ingame.alignment = (ALIGNMENT)ConvertAlignment( g_pPlayer->GetAlignment() );
//		g_char_slot_ingame.EXP_CUR	= g_pPlayer->GetVampExp();
//
//		g_char_slot_ingame.STR_CUR =g_pPlayer->GetSTR();
//		g_char_slot_ingame.DEX_CUR =g_pPlayer->GetDEX();
//		g_char_slot_ingame.INT_CUR =g_pPlayer->GetINT();
//		g_char_slot_ingame.STR_MAX =g_pPlayer->GetMAX_STR();
//		g_char_slot_ingame.DEX_MAX =g_pPlayer->GetMAX_DEX();
//		g_char_slot_ingame.INT_MAXX =g_pPlayer->GetMAX_INT();
//		g_char_slot_ingame.STR_EXP_CUR =g_pPlayer->GetSTR_EXP();
//		g_char_slot_ingame.DEX_EXP_CUR =g_pPlayer->GetDEX_EXP();
//		g_char_slot_ingame.INT_EXP_CUR =g_pPlayer->GetINT_EXP();

		// test code
//		g_char_slot_ingame.STR_EXP_NEXT_LEVEL	= 10000;
//		g_char_slot_ingame.DEX_EXP_NEXT_LEVEL	= 10000;
		//g_char_slot_ingame.INT_EXP_NEXT_LEVEL	= 10000;
//		g_char_slot_ingame.STR_EXP_CUR_LEVEL	= 5000;
//		g_char_slot_ingame.DEX_EXP_CUR_LEVEL	= 5000;
//		g_char_slot_ingame.INT_EXP_CUR_LEVEL	= 5000;


		//g_char_slot_ingame.DAM =g_pPlayer->GetDAM();
		//g_char_slot_ingame.AC =g_pPlayer->GetAC();
		//g_char_slot_ingame.TOHIT =g_pPlayer->GetTOHIT();
//		g_char_slot_ingame.HP =g_pPlayer->GetHP();
//		g_char_slot_ingame.MP =g_pPlayer->GetMP();
//		g_char_slot_ingame.HP_MAX =g_pPlayer->GetMAX_HP();
//		g_char_slot_ingame.MP_MAX =g_pPlayer->GetMAX_MP();
//		g_char_slot_ingame.SILVER_HP = g_pPlayer->GetSilverDamage();
//		//g_char_slot_ingame.CC =g_pPlayer->GetCARRYWEIGHT();
//		g_char_slot_ingame.FAME =g_pPlayer->GetFAME();
//		g_char_slot_ingame.NOTERITY =g_pPlayer->GetNotoriety();
//
//		DEBUG_ADD("[UI] CalculateStatus");
//		
//		g_pPlayer->CalculateStatus();
//
//		DEBUG_ADD("[UI] RunCharInfo");
//		
//
//		gC_vs_ui.RunInfo();
//	}
//
//	if (gC_vs_ui.IsRunningElevator())
//	{
//		DEBUG_ADD("[Execute_UI_CHARINFO] after RunCharInfo-Elevator Running");
//	}
//
//	DEBUG_ADD("[UI] UI_CHARINFO OK");
}

//-----------------------------------------------------------------------------
//
// When the item to repair was selected,
//
//-----------------------------------------------------------------------------
//
// void_ptr = Mitem*
//
void
UIMessageManager::Execute_UI_REPAIR_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{				
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_REPAIR_ITEM");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	// if there is nothing to verify
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		MItem* pItem = NULL;// = (MItem*)void_ptr;
		int itemID = static_cast<int>(left);

		//-------------------------------------------------------------
		// If itemID is 0, repair everything
		//-------------------------------------------------------------
		if (itemID==0)
		{
			//-----------------------------------------------------
			// Send a packet to repair the item.
			//-----------------------------------------------------
				CGRequestRepair	_CGRequestRepair;

				_CGRequestRepair.setObjectID( 0 );
				
				g_pSocket->sendPacket( &_CGRequestRepair );
			
				//-------------------------------------------------
				// Set Temp Information for verification
				//-------------------------------------------------
				(*g_pTempInformation).Mode		= TempInformation::MODE_SHOP_REPAIR;
				(*g_pTempInformation).Value1	= false;	// is it an item in the inventory?
				(*g_pTempInformation).Value2	= true;		// is it an item in the gear?
				(*g_pTempInformation).pValue	= NULL;

				//-------------------------------------------------
				// so that other items cannot be accessed..
				//-------------------------------------------------
				UI_LockItemTrade();
		}
		//-------------------------------------------------------------
		// repair just one item
		//-------------------------------------------------------------
		else
		{
			BOOL bInInventory = FALSE;
			BOOL bInGear = FALSE;

			//------------------------------------------------------------------------------
			// look in the inventory
			//------------------------------------------------------------------------------
			pItem = g_pInventory->GetItemToModify( itemID );

			if (pItem==NULL)
			{
				//------------------------------------------------------------------------------
				// look in the gear
				//------------------------------------------------------------------------------
				switch(g_pPlayer->GetRace())
				{
				case RACE_SLAYER:
					pItem = g_pSlayerGear->GetItemToModify( itemID );
					
					if (pItem!=NULL)
					{
						bInGear = TRUE;
					}
					break;

				case RACE_VAMPIRE:
					pItem = g_pVampireGear->GetItemToModify( itemID );
					
					if (pItem!=NULL)
					{
						bInGear = TRUE;
					}
					break;

				case RACE_OUSTERS:
					pItem = g_pOustersGear->GetItemToModify( itemID );
					
					if (pItem!=NULL)
					{
						bInGear = TRUE;
					}
					break;

				default:
					break;
				}
			}
			else
			{
				bInInventory = TRUE;
			}

			//-------------------------------------------------
			// If it is in the inventory or gear, repair it.
			//-------------------------------------------------
			if (bInInventory || bInGear)
			{
				//-------------------------------------------------
				// Check whether it is a repairable item.
				// A key means motorcycle repair.
				//-------------------------------------------------
				if (pItem->GetItemClass() != ITEM_CLASS_VAMPIRE_AMULET
					|| !pItem->IsUniqueItem()
					|| !pItem->IsQuestItem()
					|| pItem->GetMaxDurability() != -1
					|| pItem->GetItemClass()==ITEM_CLASS_KEY
					|| pItem->IsChargeItem())
				{
					//-------------------------------------------------
					// Check whether there is enough money.
					//-------------------------------------------------
					int price = g_pPriceManager->GetItemPrice(pItem, MPriceManager::REPAIR);
					int money = g_pMoneyManager->GetMoney();

					//-------------------------------------------------					
					// If the price is 0, no repair is needed.
					// However, key is used for motorcycle repair.
					//-------------------------------------------------					
					if (price!=0 || pItem->GetItemClass()==ITEM_CLASS_KEY)
					{
						//-------------------------------------------------
						// if there is enough money
						//-------------------------------------------------
						if (money!=0 && price <= money)
						{														
							//-----------------------------------------------------
							// Send a packet to repair the item.
							//-----------------------------------------------------
								CGRequestRepair	_CGRequestRepair;

								_CGRequestRepair.setObjectID( pItem->GetID() );
								
								g_pSocket->sendPacket( &_CGRequestRepair );
							
								//-------------------------------------------------
								// Set Temp Information for verification
								//-------------------------------------------------
								(*g_pTempInformation).Mode		= TempInformation::MODE_SHOP_REPAIR;
								(*g_pTempInformation).Value1	= bInInventory;	// is it an item in the inventory?
								(*g_pTempInformation).Value2	= bInGear;		// is it an item in the gear?
								(*g_pTempInformation).pValue	= (void*)pItem;

								//-------------------------------------------------
								// so that other items cannot be accessed..
								//-------------------------------------------------
								UI_LockItemTrade();
						}
						//-------------------------------------------------
						// if there is not enough money
						//-------------------------------------------------					
						else
						{
							g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_NO_MONEY ].GetString());
						}
					}
					//-------------------------------------------------
					// if no repair is needed
					//-------------------------------------------------
					else
					{
						g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_REPAIR].GetString());
					}
				}
				//-------------------------------------------------
				// if no repair is needed
				//-------------------------------------------------
				else
				{
					g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_REPAIR].GetString());
				}
			}		
		}
	}
}

//-----------------------------------------------------------------------------
//
// When a Silvering item was selected,
//
//-----------------------------------------------------------------------------
//
// void_ptr = Mitem*
//
void
UIMessageManager::Execute_UI_SILVERING_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{				
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_SILVERING_ITEM");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	// if there is nothing to verify
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		MItem* pItem = NULL;// = (MItem*)void_ptr;
		int itemID = static_cast<int>(left);

		BOOL bInInventory = FALSE;
		BOOL bInGear = FALSE;

		//------------------------------------------------------------------------------
		// look in the inventory
		//------------------------------------------------------------------------------
		pItem = g_pInventory->GetItemToModify( itemID );

		if (pItem==NULL)
		{
			//------------------------------------------------------------------------------
			// look in the gear
			//------------------------------------------------------------------------------
			switch(g_pPlayer->GetRace())
			{
			case RACE_SLAYER:
				pItem = g_pSlayerGear->GetItemToModify( itemID );
				
				if (pItem!=NULL)
				{
					bInGear = TRUE;
				}
				break;

			case RACE_VAMPIRE:
				pItem = g_pVampireGear->GetItemToModify( itemID );
				
				if (pItem!=NULL)
				{
					bInGear = TRUE;
				}
				break;

			case RACE_OUSTERS:
				pItem = g_pOustersGear->GetItemToModify( itemID );
				
				if (pItem!=NULL)
				{
					bInGear = TRUE;
				}
				break;

			default:
				break;
			}
		}
		else
		{
			bInInventory = TRUE;
		}

		//-------------------------------------------------
		// If it is in the inventory or gear, silver it..
		//-------------------------------------------------
		if (bInInventory || bInGear)
		{
			//-------------------------------------------------
			// Check whether it is an item that can be Silvered.
			//-------------------------------------------------
			if (pItem->GetSilverMax() > 0)
			{
				//-------------------------------------------------
				// Check whether there is enough money.
				//-------------------------------------------------
				int price = (*g_pPriceManager).GetItemPrice(pItem, MPriceManager::SILVERING);
				int money = (*g_pMoneyManager).GetMoney();

				//-------------------------------------------------					
				// If the price is 0, no silvering is needed.
				//-------------------------------------------------					
				if (price!=0)
				{
					//-------------------------------------------------
					// if there is enough money
					//-------------------------------------------------
					if (money!=0 && price <= money)
					{														
						//-----------------------------------------------------
						// Send a packet to silver the item.
						//-----------------------------------------------------
							CGSilverCoating	_CGSilverCoating;

							_CGSilverCoating.setObjectID( pItem->GetID() );
							
							g_pSocket->sendPacket( &_CGSilverCoating );
						
							//-------------------------------------------------
							// Set Temp Information for verification
							//-------------------------------------------------
							(*g_pTempInformation).Mode		= TempInformation::MODE_SHOP_SILVERING;
							(*g_pTempInformation).Value1	= bInInventory;	// is it an item in the inventory?
							(*g_pTempInformation).Value2	= bInGear;		// is it an item in the gear?
							(*g_pTempInformation).pValue	= (void*)pItem;

							//-------------------------------------------------
							// so that other items cannot be accessed..
							//-------------------------------------------------
							UI_LockItemTrade();
					}
					//-------------------------------------------------
					// if there is not enough money
					//-------------------------------------------------					
					else
					{
						g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_NO_MONEY ].GetString());
					}
				}
				//-------------------------------------------------
				// if no Silvering is needed
				//-------------------------------------------------
				else
				{
					g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_SILVERING].GetString());
				}
			}
			//-------------------------------------------------
			// if no repair is needed
			//-------------------------------------------------
			else
			{
				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_SILVERING].GetString());
			}
		}		
	}
}

//-----------------------------------------------------------------------------
//
// Stop repairing
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_REPAIR_FINISHED(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ITEM_REPAIR_FINISHED");
	
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation!=NULL
		&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.FinishItemRepairing();

		// Make it possible to select something again?
		g_pUIDialog->ShowPCTalkDlg();
	}
}

//-----------------------------------------------------------------------------
//
// Stop silvering
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_SILVERING_FINISHED(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_ITEM_SILVERING_FINISHED");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	if (g_pTempInformation!=NULL
		&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.FinishItemSilvering();

		// Make it possible to select something again?
		g_pUIDialog->ShowPCTalkDlg();
	}
}

//-----------------------------------------------------------------------------
//
// stats
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_FINISH_LEVELUP_BUTTON(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_FINISH_LEVELUP_BUTTON");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	gC_vs_ui.FinishLevelUp();
}

//-----------------------------------------------------------------------------
//
// When the game menu is shown, close everything else.
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_RUNNING_GAMEMENU(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_RUNNING_GAMEMENU");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
//	g_pUIDialog->CloseHelpDlg();
	g_pUIDialog->CloseMessageDlg();
	g_pUIDialog->ClosePCTalkDlg();
}

//-----------------------------------------------------------------------------
//
// Buy storage
//
//-----------------------------------------------------------------------------
// left = buy (TRUE), do not buy (FALSE)
void
UIMessageManager::Execute_UI_STORAGE_BUY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_STORAGE_BUY");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	BOOL buy = (BOOL)left;

	//-----------------------------------------------------------------------------
	// State of trying to buy a storage..
	//-----------------------------------------------------------------------------
	if (buy)
	{
		if (g_pTempInformation->GetMode()==TempInformation::MODE_STORAGE_BUY)
		{		
			//--------------------------------------------------------
			// check money
			//--------------------------------------------------------
			if (g_pMoneyManager->GetMoney() >= g_pTempInformation->Value1)
			{
					CGStashRequestBuy _CGStashRequestBuy;
					
					g_pSocket->sendPacket( &_CGStashRequestBuy );					

				// set mode
				g_pTempInformation->SetMode(TempInformation::MODE_STORAGE_BUY_WAIT);

			}
			//--------------------------------------------------------
			// if there is not enough money
			//--------------------------------------------------------
			else
			{
				// Make it possible to select something again?
				g_pUIDialog->ShowPCTalkDlg();

				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_NO_MONEY ].GetString());
				
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}
		}
		else
		{
			DEBUG_ADD_ERR("[Error] Mode is Not MODE_STORAGE_BUY");
		}

		// When buying, until the result packet is received..
		// the NPC dialog is not shown.
	}
	//-----------------------------------------------------------------------------
	// Do not buy storage.
	//-----------------------------------------------------------------------------
	else
	{
		// Make it possible to select something again?
		g_pUIDialog->ShowPCTalkDlg();

		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
	}					
}

//-----------------------------------------------------------------------------
//
// A storage slot was selected.
//
//-----------------------------------------------------------------------------
// left = tab (0, 1, 2)
// right = slot
//
void
UIMessageManager::Execute_UI_SELECT_STORAGE_SLOT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_SELECT_STORAGE_SLOT");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if(g_pStorage == NULL)
		return;

	g_pStorage->SetCurrent( static_cast<unsigned int>(left) );	// for checking

	int slot = static_cast<int>(right);

	MItem* pMouseItem = (MItem*)gpC_mouse_pointer->GetPickUpItem();
	const MItem* pStorageItem = g_pStorage->GetItem( slot );

	//-----------------------------------------------------------------------------
	// if trying to pick something up from the storage
	//-----------------------------------------------------------------------------
	if (pMouseItem==NULL)
	{
		//---------------------------------------------------
		// If the selected slot has an item, pick it up.
		//---------------------------------------------------
		if (pStorageItem!=NULL)
		{
			MItem* pRemovedItem = g_pStorage->RemoveItem( slot );

			if (pRemovedItem!=NULL)
			{
				 UI_PickUpItem( (MItem*)pStorageItem );

					CGStashToMouse _CGStashToMouse;
					_CGStashToMouse.setObjectID( pStorageItem->GetID() );
					_CGStashToMouse.setRack( g_pStorage->GetCurrent() );
					_CGStashToMouse.setIndex( slot );									

					g_pSocket->sendPacket( &_CGStashToMouse );					
			}
		}
	}
	//-----------------------------------------------------------------------------
	// if trying to put the held item into the storage
	//-----------------------------------------------------------------------------
	else 
	{
		BOOL bSendPacket = TRUE;

		TYPE_OBJECTID mouseItemID = pMouseItem->GetID();		
	
		// An Event GiftBox item cannot be put down.
		if (pMouseItem->GetItemClass()!=ITEM_CLASS_EVENT_GIFT_BOX
			&& pMouseItem->GetItemClass()!=ITEM_CLASS_VAMPIRE_AMULET
			&& pMouseItem->GetItemClass()!=ITEM_CLASS_COUPLE_RING
			&& pMouseItem->GetItemClass()!=ITEM_CLASS_VAMPIRE_COUPLE_RING
			&& !pMouseItem->IsQuestItem()
			&& pMouseItem->IsNormalItem()
			&& !((pMouseItem->GetItemClass() == ITEM_CLASS_EVENT_TREE) && pMouseItem->GetItemType() >= 13  )
			&& pMouseItem->GetItemClass() != ITEM_CLASS_EVENT_ITEM
			&& pMouseItem->GetItemClass() != ITEM_CLASS_CODE_SHEET
			&& !(pMouseItem->GetItemClass() == ITEM_CLASS_LUCKY_BAG && pMouseItem->GetItemType() == 3)
			)
		{
			//---------------------------------------------------
			// If nothing is in that spot, put the item in.
			//---------------------------------------------------
			if (pStorageItem==NULL)
			{
				// A slot past the rack reads as empty but takes nothing: the item
				// stays held and the server is not told.
				if (g_pStorage->SetItem( slot, pMouseItem ))
					UI_DropItem();	// take the item off the mouse
				else
					bSendPacket = FALSE;
			}
			//---------------------------------------------------
			// if something is there...
			//---------------------------------------------------
			else
			{
				//---------------------------------------------------
				// if it is a stackable item
				//---------------------------------------------------
				if (pStorageItem->IsPileItem()
					&& pStorageItem->GetItemClass()==pMouseItem->GetItemClass()
					&& pStorageItem->GetItemType()==pMouseItem->GetItemType())
				{
					//---------------------------------------------------
					// if more can be stacked
					//---------------------------------------------------
					if (pStorageItem->GetNumber() < pStorageItem->GetMaxNumber())
					{
						PlaySound( pMouseItem->GetInventorySoundID() );
						
						MItem* pModifyStorageItem = g_pStorage->RemoveItem( slot );

						//----------------------------------------------------
						// Add pMouseItem to pStorageItem.
						//----------------------------------------------------
						int total = pMouseItem->GetNumber() + pStorageItem->GetNumber();
						if ( static_cast<TYPE_ITEM_NUMBER>(total) > pStorageItem->GetMaxNumber() )
						{
							// if it exceeds the limit
							pMouseItem->SetNumber( total - pStorageItem->GetMaxNumber() );
							pModifyStorageItem->SetNumber( pStorageItem->GetMaxNumber() );
						}
						else
						{
							// if all of it can be added to pItem
							pModifyStorageItem->SetNumber( total );
							UI_DropItem();

							delete pMouseItem;
						}	

						//----------------------------------------------------
						// set the swapped one again
						//----------------------------------------------------
						g_pStorage->SetItem( slot, pModifyStorageItem );					
						
					}
					//---------------------------------------------------
					// If no more can be stacked.. just leave it.
					//---------------------------------------------------				
					else
					{
						bSendPacket = FALSE;
					}
				}
				//---------------------------------------------------				
				// If it is a non-stackable item --> swap.
				//---------------------------------------------------				
				else
				{				
					MItem* pTempItem = pMouseItem;
					
					UI_PickUpItem( (MItem*)pStorageItem );

					g_pStorage->RemoveItem( slot );	// If this fails.. - -;

					g_pStorage->SetItem( slot, pTempItem );
				}
			}

				if (bSendPacket)
				{		
					CGMouseToStash _CGMouseToStash;
					_CGMouseToStash.setObjectID( mouseItemID );
					_CGMouseToStash.setRack( g_pStorage->GetCurrent() );
					_CGMouseToStash.setIndex( slot );									

					g_pSocket->sendPacket( &_CGMouseToStash );				
				}
		}
		else
		{
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_STORAGE].GetString());
		}


	}
}

//-----------------------------------------------------------------------------
//
// When saving money into the storage,
//
//-----------------------------------------------------------------------------
// left = money
//
void
UIMessageManager::Execute_UI_DEPOSIT_MONEY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_DEPOSIT_MONEY");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	//-----------------------------------------------------------------------------
	// The money must not be 0.
	//-----------------------------------------------------------------------------
	if (left > 0)
	{
		if (g_pMoneyManager!=NULL && g_pStorage!=NULL)
		{
				CGStashDeposit _CGStashDeposit;
				_CGStashDeposit.setAmount( static_cast<Gold_t>(left) );

				g_pSocket->sendPacket( &_CGStashDeposit );				

			//-----------------------------------------------
			// Move the money into the storage.
			//-----------------------------------------------
			g_pMoneyManager->UseMoney( static_cast<int>(left) );
			g_pStorage->GetMoneyManager()->AddMoney( static_cast<int>(left) );
		}
		else
		{
			DEBUG_ADD_ERR("[Error] MoneyManager or Storage is NULL");
		}
	}
}

//-----------------------------------------------------------------------------
//
// When withdrawing money from the storage,
//
//-----------------------------------------------------------------------------
// left = money
//
void
UIMessageManager::Execute_UI_WITHDRAW_MONEY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_WITHDRAW_MONEY");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	//-----------------------------------------------------------------------------
	// The money must not be 0.
	//-----------------------------------------------------------------------------
	if (left > 0)
	{
		if (g_pMoneyManager!=NULL && g_pStorage!=NULL)
		{
				CGStashWithdraw _CGStashWithdraw;
				_CGStashWithdraw.setAmount( static_cast<Gold_t>(left) );

				g_pSocket->sendPacket( &_CGStashWithdraw );				

			//-----------------------------------------------
			// Move the storage's money to the player.
			//-----------------------------------------------
			g_pStorage->GetMoneyManager()->UseMoney( static_cast<int>(left) );
			g_pMoneyManager->AddMoney( static_cast<int>(left) );						
		}
		else
		{
			DEBUG_ADD_ERR("[Error] MoneyManager or Storage is NULL");
		}
	}
}

//-----------------------------------------------------------------------------
//
//	When closing the storage
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_STORAGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_STORAGE");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	UI_CloseStorage();		

	// Make it possible to select something again?
	g_pUIDialog->ShowPCTalkDlg();
}

//-----------------------------------------------------------------------------
//
// Put money into the trade window.
//
//-----------------------------------------------------------------------------
// left = bGive - is it money given to the other person?
// right = money
//
void
UIMessageManager::Execute_UI_EXCHANGE_MONEY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_EXCHANGE_MONEY");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	if (g_pTradeManager!=NULL)
	{
		BOOL bGive = static_cast<BOOL>(left);
		int money = static_cast<int>(right);
			
		BOOL bAcceptMyTrade = g_pTradeManager->IsAcceptMyTrade();

		int code;		

		//---------------------------------------------------------
		// Verification is required when OK was pressed during a trade.
		//---------------------------------------------------------
		if (bAcceptMyTrade)
		{	
			if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL)
			{
				if (bGive)
				{
					g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_ADD_MONEY);
				}
				else
				{
					g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_REMOVE_MONEY);

					g_pTradeManager->SetNextAcceptTime();
				}

				g_pTempInformation->Value1	= money;
			}
		}
		//---------------------------------------------------------
		// the normal case where OK is not pressed..
		//---------------------------------------------------------
		else
		{
			/*
			//-----------------------------------------------------------
			// If something changes... cancel OK
			//-----------------------------------------------------------
			g_pTradeManager->RefuseOtherTrade();
			g_pTradeManager->RefuseMyTrade();
		
			//-----------------------------------------------------------
			// client money movement
			//-----------------------------------------------------------
			if (bGive)
			{
				//if (!g_pMoneyManager->GetOtherMoneyManager->CanAddMoney(money))
				//{
				//	money = g_pMoneyManager->GetOtherMoneyManager->GetMaxAddMoney();
				//}

				g_pMoneyManager->UseMoney( money );
				g_pTradeManager->GetMyMoneyManager()->AddMoney( money );
			}
			else
			{
				g_pTradeManager->GetMyMoneyManager()->UseMoney( money );
				g_pMoneyManager->AddMoney( money );	
			}
			*/

			if (!bGive)
			{
				g_pTradeManager->SetNextAcceptTime();
			}
		}	

		//-----------------------------------------------------------
		// client money movement
		//-----------------------------------------------------------
		if (bGive)
		{
			code = CG_TRADE_MONEY_INCREASE;
		}
		else
		{
			code = CG_TRADE_MONEY_DECREASE;
		}
		
		//-----------------------------------------------------------
		// notify the server
		//-----------------------------------------------------------
			CGTradeMoney _CGTradeMoney;
			_CGTradeMoney.setTargetObjectID( g_pTradeManager->GetOtherID() );
			_CGTradeMoney.setAmount( money );
			_CGTradeMoney.setCode( code );				

			g_pSocket->sendPacket( &_CGTradeMoney );			

//			g_pTradeManager->SetNextAcceptTime();
	}
	else
	{
		DEBUG_ADD_ERR( "[Error] TradeManager is NULL");
	}


}

//-----------------------------------------------------------------------------
//
// When an item in the trade window is selected
//
//-----------------------------------------------------------------------------
// gridXY = (left,right)
// item to pick up = (MItem*)void_ptr
/*
void
UIMessageManager::Execute_UI_ITEM_PICKUP_FROM_TRADEGRID(intptr_t left, intptr_t right, void* void_ptr)
{
	//
	// hi/low dw_left is the grid (x, y) value.
	//        dw_right is the item's screen coordinates.
	//
	DEBUG_ADD("[UI] UI_ITEM_PICKUP_FROM_TRADEGRID");
	
	int		gridX = left;
	int		gridY = right;
	MItem*	pItem = (MItem *)void_ptr;

	UI_PickUpItem( pItem );
}
*/

//-----------------------------------------------------------------------------
//
// When added to an item in the trade window,
//
//-----------------------------------------------------------------------------
// gridXY = (left, right)
// item that was placed = (MItem*)void_ptr
/*
void
UIMessageManager::Execute_UI_ITEM_INSERT_FROM_TRADEGRID(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_ITEM_INSERT_FROM_TRADEGRID");
	
	int		gridX = left;
	int		gridY = right;
	MItem*	pItem = (MItem *)void_ptr;
	MItem*	pMouseItem = (MItem*)gpC_mouse_pointer->GetPickUpItem();
}
*/

//-----------------------------------------------------------------------------
//
// When placing an item in the trade window,
//
//-----------------------------------------------------------------------------
// gridXY = (left, right)
// item held just before placing = (MItem*)void_ptr
/*
void
UIMessageManager::Execute_UI_ITEM_DROP_TO_TRADEGRID(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_ITEM_DROP_TO_TRADEGRID");
	
	int		gridX = left;
	int		gridY = right;
	MItem*	pItem = (MItem *)void_ptr;
}
*/

//-----------------------------------------------------------------------------
//
// When placing an item in the trade window,
//
//-----------------------------------------------------------------------------
// gridXY = (left, right)
// item held just before placing = (MItem*)void_ptr
void
UIMessageManager::Execute_UI_ITEM_SELECT_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] UI_ITEM_SELECT_EXCHANGE");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTradeManager!=NULL && void_ptr!=NULL)
	{
		int		gridX = static_cast<int>(left);
		int		gridY = static_cast<int>(right);
		MItem*	pItem = (MItem *)void_ptr;

		BOOL bAcceptMyTrade = g_pTradeManager->IsAcceptMyTrade();		

		// gift box hardcoding -_-;
		if(pItem->GetItemClass() == ITEM_CLASS_EVENT_GIFT_BOX && pItem->GetItemType() > 1 && pItem->IsTrade() == false)	//
		{
			MEventGiftBoxItemFinder finder;
			MItem* pFindItem = ((MItemManager*)g_pInventory)->FindItem( finder );
			if(pFindItem != NULL)
			{
				// something has already been put up for trade
				return;
			}
		}
	

		//---------------------------------------------------------
		// Verification is required when OK was pressed during a trade.
		//---------------------------------------------------------
		if (bAcceptMyTrade)
		{	
			if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL
				&& g_pPlayer->IsItemCheckBufferNULL())
			{
				g_pTempInformation->Value1 = pItem->GetID();
				g_pTempInformation->Value2 = gridX;
				g_pTempInformation->Value3 = gridY;
				g_pTempInformation->pValue = pItem;

				//-----------------------------------------------------------
				// previously selected item --> cancel
				//-----------------------------------------------------------
				if (pItem->IsTrade())
				{
					g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_REMOVE_ITEM);

					g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_TRADE_REMOVE );

						CGTradeRemoveItem _CGTradeRemoveItem;
						_CGTradeRemoveItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeRemoveItem.setItemObjectID( pItem->GetID() );

						g_pSocket->sendPacket( &_CGTradeRemoveItem );

						g_pTradeManager->SetNextAcceptTime();

					// [Help] during trade - item cancel
//					__BEGIN_HELP_EVENT
////						ExecuteHelpEvent( HE_TRADE_ITEM_REMOVE );	
//					__END_HELP_EVENT
				}
				//-----------------------------------------------------------
				// select item
				//-----------------------------------------------------------
				else
				{
					g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM);

					g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_TRADE_ADD );

						CGTradeAddItem _CGTradeAddItem;
						_CGTradeAddItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeAddItem.setItemObjectID( pItem->GetID() );

						g_pSocket->sendPacket( &_CGTradeAddItem );
						

					// [Help] during trade - item select
//					__BEGIN_HELP_EVENT
////						ExecuteHelpEvent( HE_TRADE_ITEM_ADD );	
//					__END_HELP_EVENT
				}				
			}			
		}
		//---------------------------------------------------------
		// the normal case where OK is not pressed..
		//---------------------------------------------------------
		else
		{		
			//-----------------------------------------------------------
			// previously selected item --> cancel
			//-----------------------------------------------------------
			if (pItem->IsTrade())
			{
				//-----------------------------------------------------------
				// If something changes... cancel OK
				//-----------------------------------------------------------
				g_pTradeManager->RefuseOtherTrade();
				g_pTradeManager->RefuseMyTrade();

				pItem->UnSetTrade();		

					CGTradeRemoveItem _CGTradeRemoveItem;
					_CGTradeRemoveItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
					_CGTradeRemoveItem.setItemObjectID( pItem->GetID() );

					g_pSocket->sendPacket( &_CGTradeRemoveItem );

					g_pTradeManager->SetNextAcceptTime();

				// [Help] during trade - item cancel
//				__BEGIN_HELP_EVENT
////					ExecuteHelpEvent( HE_TRADE_ITEM_REMOVE );	
//				__END_HELP_EVENT
			}
			//-----------------------------------------------------------
			// previously unselected item --> select
			//-----------------------------------------------------------
			else
			{
				// A red gift box cannot be traded.
				if (pItem->GetItemClass()==ITEM_CLASS_EVENT_GIFT_BOX
					&& pItem->GetItemType()==1)
				{
				}
				else
				{
					// A gift box needs verification.
					if (pItem->GetItemClass()==ITEM_CLASS_EVENT_GIFT_BOX)// && pItem->GetItemClass() < 2)
					{
						g_pTempInformation->SetMode(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM);
						g_pTempInformation->pValue	= pItem;
						
						g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_TRADE_ADD );
					}
					// Otherwise just put it in.
					else
					{
						//-----------------------------------------------------------
						// If something changes... cancel OK
						//-----------------------------------------------------------
						g_pTradeManager->RefuseOtherTrade();
						g_pTradeManager->RefuseMyTrade();

						pItem->SetTrade();		
					}

						CGTradeAddItem _CGTradeAddItem;
						_CGTradeAddItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeAddItem.setItemObjectID( pItem->GetID() );

						g_pSocket->sendPacket( &_CGTradeAddItem );
						

					// [Help] during trade - item select
//					__BEGIN_HELP_EVENT
////						ExecuteHelpEvent( HE_TRADE_ITEM_ADD );	
//					__END_HELP_EVENT
				}
			}	
		}
	}
	else
	{
		DEBUG_ADD_ERR( "[Error] TradeManager is NULL");
	}
}


//-----------------------------------------------------------------------------
//
// Trade confirm
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_OK_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_OK_EXCHANGE");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	if (g_pTradeManager!=NULL
		&& g_pTradeManager->IsAcceptTime())
	{
		MItem* pMouseItem = UI_GetMouseItem();

		//--------------------------------------------------------
		// if the trade is possible
		// Holding a gift box prevents the trade.
		//--------------------------------------------------------
		if ((pMouseItem==NULL || pMouseItem->GetItemClass()!=ITEM_CLASS_EVENT_GIFT_BOX)
			&& g_pTradeManager->CanTrade())
		{
			g_pTradeManager->AcceptMyTrade();
			
				CGTradeFinish _CGTradeFinish;
				_CGTradeFinish.setTargetObjectID( g_pTradeManager->GetOtherID() );
				_CGTradeFinish.setCode( CG_TRADE_FINISH_ACCEPT );

				g_pSocket->sendPacket( &_CGTradeFinish );
				
		}
		//--------------------------------------------------------
		// if the trade is impossible
		//--------------------------------------------------------
		else
		{
			// If the trade is not possible..
			// For now.. assume it is lack of space.
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_TRADE_NO_SPACE].GetString() );
		}
	}
	else
	{
		if (g_pTradeManager == NULL)
			DEBUG_ADD_WAR("Trade acceptance requested without a trade manager");
		else
			DEBUG_ADD("Trade acceptance is not ready");
	}
}

//-----------------------------------------------------------------------------
//
// Trade cancel
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CANCEL_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CANCEL_EXCHANGE");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	BOOL bSendPacket = FALSE;

	if (g_pTradeManager!=NULL)
	{	
		BOOL bAcceptMyTrade = g_pTradeManager->IsAcceptMyTrade();

		//---------------------------------------------------------------
		// Verification is required while OK is pressed.
		//---------------------------------------------------------------
		if (bAcceptMyTrade)
		{
			if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL)
			{
				g_pTempInformation->SetMode(g_pTempInformation->MODE_TRADE_VERIFY_CANCEL);

				bSendPacket = TRUE;
			}
			else
			{
				DEBUG_ADD_ERR( "[Error] Mode is not MODE_NULL");
			}
		}
		//---------------------------------------------------------------
		// Just refuse (this can't happen anyway - -;;)
		//---------------------------------------------------------------
		else
		{		
			//---------------------------------------------------------------
			// Trade refused..
			//---------------------------------------------------------------
			g_pTradeManager->RefuseMyTrade();
			
			bSendPacket = TRUE;
		}
	}	
	else
	{
		DEBUG_ADD_ERR( "[Error] TradeManager is NULL");
	}

	//---------------------------------------------------------------
	// packet that cancels my OK
	//---------------------------------------------------------------
	if (bSendPacket)
	{
			CGTradeFinish _CGTradeFinish;
			_CGTradeFinish.setTargetObjectID( g_pTradeManager->GetOtherID() );
			_CGTradeFinish.setCode( CG_TRADE_FINISH_RECONSIDER );

			g_pSocket->sendPacket( &_CGTradeFinish );
			
	}
}

//-----------------------------------------------------------------------------
//
// Close the trade window
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_EXCHANGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_EXCHANGE");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	BOOL bAcceptMyTrade = (g_pTradeManager!=NULL && g_pTradeManager->IsAcceptMyTrade());

	BOOL bSendPacket = FALSE;

	int otherID;

	//---------------------------------------------------------------
	// Verification is required while OK is pressed.
	//---------------------------------------------------------------
	if (bAcceptMyTrade)
	{
		if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL)
		{
			g_pTempInformation->SetMode(g_pTempInformation->MODE_TRADE_VERIFY_CLOSE);
			
			otherID = g_pTradeManager->GetOtherID();

			bSendPacket = TRUE;
		}
		else
		{
			DEBUG_ADD_ERR( "[Error] Mode is not MODE_NULL");
		}
	}
	//---------------------------------------------------------------
	// Just cancel..
	//---------------------------------------------------------------
	else
	{		
		if (g_pTradeManager!=NULL)
		{			
			otherID = g_pTradeManager->GetOtherID();

			g_pTradeManager->CancelTrade();

			bSendPacket = TRUE;
		}

		//---------------------------------------------------------------
		// No trade~~ done~
		//---------------------------------------------------------------
		UI_CloseExchange();	
	}
	
	//---------------------------------------------------------------
	// packet that ends the trade
	//---------------------------------------------------------------
	if (bSendPacket)
	{
			CGTradeFinish _CGTradeFinish;
			_CGTradeFinish.setTargetObjectID( otherID );
			_CGTradeFinish.setCode( CG_TRADE_FINISH_REJECT );

			g_pSocket->sendPacket( &_CGTradeFinish );
	}
}

//-----------------------------------------------------------------------------
//
// Select a name in the chat window
//
//-----------------------------------------------------------------------------
// left = type of input
// void_ptr = character name (!=NULL)

//-----------------------------------------------------------------------------
//
// Response to the Trade? Y/N prompt
//
//-----------------------------------------------------------------------------
// left = (BOOL) whether to trade
//
void
UIMessageManager::Execute_UI_EXCHANGE_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_EXCHANGE_ACCEPT");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	BOOL accept = (BOOL)left;

	int code;

	//---------------------------------------------------------
	// Response to the Trade? Y/N prompt
	//---------------------------------------------------------
	if (g_pTempInformation->GetMode() == TempInformation::MODE_TRADE_REQUEST)
	{
		//---------------------------------------------------------
		// allow
		//---------------------------------------------------------
		if (accept)
		{
			code = CG_TRADE_PREPARE_CODE_ACCEPT;

			//---------------------------------------------------------
			// Show the trade window.
			//---------------------------------------------------------
			UI_RunExchange( static_cast<TYPE_OBJECTID>(g_pTempInformation->Value1) );	// set otherID
		}
		//---------------------------------------------------------
		// refuse
		//---------------------------------------------------------
		else
		{	
			if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_TRADE)
			{
				g_pPlayer->SetWaitVerifyNULL();
			}

			code = CG_TRADE_PREPARE_CODE_REJECT;
		}

			CGTradePrepare _CGTradePrepare;
			_CGTradePrepare.setTargetObjectID( static_cast<ObjectID_t>(g_pTempInformation->Value1) );
			_CGTradePrepare.setCode( code );				

			g_pSocket->sendPacket( &_CGTradePrepare );			
	
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);		
	}	
}

//-----------------------------------------------------------------------------
//
// Cancel the trade?
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_EXCHANGE_REQUEST_CANCEL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_EXCHANGE_REQUEST_CANCEL");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	//---------------------------------------------------------
	// Response to the Trade? Y/N prompt
	//---------------------------------------------------------
	if (g_pTempInformation->GetMode() == TempInformation::MODE_TRADE_REQUEST)
	{	
		if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_TRADE)
		{
			g_pPlayer->SetWaitVerifyNULL();
		}

			CGTradePrepare _CGTradePrepare;
			_CGTradePrepare.setTargetObjectID( static_cast<ObjectID_t>(g_pTempInformation->Value1) );
			_CGTradePrepare.setCode( CG_TRADE_PREPARE_CODE_CANCEL );				

			g_pSocket->sendPacket( &_CGTradePrepare );			
	
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		
		// Remove the Cancel trade? prompt.
		UI_CloseExchangeCancel();
	}	
}

//-----------------------------------------------------------------------------
//
// Change Title Option
//
//-----------------------------------------------------------------------------
// left = option being changed
// right = value
void
UIMessageManager::Execute_UI_CHANGE_OPTION(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CHANGE_TITLE_OPTION");
	
//	if (g_Mode!=MODE_MAINMENU)
//	{
//		DEBUG_ADD("Not Mode MODE_MAINMENU");
//		return;
//	}

	
	int value = static_cast<int>(right);

	switch (left)
	{
		//-------------------------------------------------------------------
		// USE 3D HAL
		//-------------------------------------------------------------------
//		case C_VS_UI_OPTION::CHECK_3D :
//			g_pUserOption->Use3DHAL = (BOOL)value;
//		break;

		//-------------------------------------------------------------------
		// DRAW_MINIMAP
		//-------------------------------------------------------------------
		//case C_VS_UI_OPTION::DRAW_MINIMAP :
		//	g_pUserOption->DrawMinimap = (BOOL)value;
		//break;

		//-------------------------------------------------------------------
		// DRAW_ZONE_NAME
		//-------------------------------------------------------------------
		//case C_VS_UI_OPTION::DRAW_ZONE_NAME :
		//	g_pUserOption->DrawZoneName = (BOOL)value;
		//break;

		//-------------------------------------------------------------------
		// DRAW_GAME_TIME
		//-------------------------------------------------------------------
		//case C_VS_UI_OPTION::DRAW_GAME_TIME :
		//	g_pUserOption->DrawGameTime = (BOOL)value;
		//break;

		//-------------------------------------------------------------------
		// DRAW_BLENDING_CHARACTER_SHADOW
		//-------------------------------------------------------------------
		//case C_VS_UI_OPTION::DRAW_BLENDING_CHARACTER_SHADOW :
	//		g_pUserOption->BlendingShadow = (BOOL)value;
		//break;

		//-------------------------------------------------------------------
		// PLAY_WAV
		//-------------------------------------------------------------------
		case C_VS_UI_OPTION::CHECK_WAV :
			g_pUserOption->PlayWaveMusic = (BOOL)value;

			if(g_Mode == MODE_MAINMENU)
				PlayTitleMusic();			
			else 
				PlayGameMusic();			
		break;

		//-------------------------------------------------------------------
		// BLOOD_DROP
		//-------------------------------------------------------------------
//		case C_VS_UI_OPTION::BLOOD_DROP :
//			g_pUserOption->BloodDrop = (BOOL)value;
//		break;

		//-------------------------------------------------------------------
		// PLAY_SOUND
		//-------------------------------------------------------------------
		case C_VS_UI_OPTION::CHECK_SOUND :
			g_pUserOption->PlaySound = (BOOL)value;

			if (g_pUserOption->PlaySound)
			{
				g_SDLAudio.UnSetMute();
			}
			else
			{
				if (g_pSoundManager != NULL)
				{
					g_pSoundManager->Stop();
				}
				
				g_SDLAudio.SetMute();
			}
		break;

		//-------------------------------------------------------------------
		// PLAY_MUSIC
		//-------------------------------------------------------------------
		case C_VS_UI_OPTION::CHECK_MUSIC :
			g_pUserOption->PlayMusic = (BOOL)value;

			PlayTitleMusic();			
		break;

		//-------------------------------------------------------------------
		// VOLUME_SOUND
		//-------------------------------------------------------------------
		case C_VS_UI_OPTION::CHECK_VALUE_SOUND_VOLUME :
			if (value>=0 && value<16)
			{
				g_pUserOption->VolumeSound = value;	

				LONG volume = value*SOUND_DEGREE + SOUND_MIN;

				g_SDLAudio.SetVolumeLimit( volume );	
				
				PlaySound(SOUND_SLAYER_BUTTON);
			}
		break;

		//-------------------------------------------------------------------
		// VOLUME_MUSIC 
		//-------------------------------------------------------------------
		case C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME :

			if (value>=0 && value<16)
			{
				g_pUserOption->VolumeMusic = value;
				
				if (g_pUserOption->PlayWaveMusic)
				{
//					LONG volume = value*16*257;//*SOUND_DEGREE + SOUND_MIN;

//					DEBUG_ADD_FORMAT("Change VOLUME_MUSIC = %ld", volume);
					
//					g_pSDLStream->SetVolumeLimit( volume );
					DEBUG_ADD("MP3 SetVolume4");
					if( g_SDLAudio.IsInit() && g_pOGG != NULL )
					{
						//int step = (DSBVOLUME_MIN) / 16;
						int volume = (value - 15) * 250;

						g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
					}
					DEBUG_ADD("MP3 SetVolume4 OK");
				}
				else
				{
					WORD volume = (value)*0x1000 + 0x0FFF;

					if (!g_pUserOption->PlayMusic || g_Music.IsPause())
					{
						g_Music.SetVolume( volume );
					}
					else
					{
						g_Music.Pause();
						g_Music.SetVolume( volume );
						g_Music.Resume();
					}				
				}
			}
		break;

		//-------------------------------------------------------------------
		// character HP bar transparent/opaque
		//-------------------------------------------------------------------
//		case C_VS_UI_OPTION::PARTY_HPBAR_ALPHA :
//			g_pUserOption->DrawTransHPBar = (BOOL)value;
//		break;
	}
}


//-----------------------------------------------------------------------------
//
// Close Option
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_OPTION(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (!GetDisplaySettings().Save())
		DEBUG_ADD_ERR("[Error] Could not save UserSet/Display.ini");
	DEBUG_ADD("[UI] Execute_UI_CLOSE_TITLE_OPTION");

//	if (g_Mode!=MODE_MAINMENU)
//	{
//		DEBUG_ADD("Not Mode MODE_MAINMENU");
//		return;
//	}

	
	gC_vs_ui.CloseOption();

	//---------------------------------------------------------------
	// Just save it. - -;
	//---------------------------------------------------------------
	// A file definition without the names throws: those saves are skipped
	// (from the first missing name on), not the close.
	try
	{
		g_pClientConfig->SaveToFile( g_pFileDef->getProperty("FILE_INFO_CLIENTCONFIG").c_str());
		g_pUserOption->SaveToFile( g_pFileDef->getProperty("FILE_INFO_USEROPTION").c_str());
	}
	catch (NoSuchElementException&)
	{
		DEBUG_ADD_ERR("[Error] No file name for the client configuration or the options");
	}

	if (g_Mode==MODE_MAINMENU)
	{
		// SDL Migration: 3D HAL options are legacy features not used in SDL backend
#ifdef PLATFORM_WINDOWS
		if (true)
		{
			// When 3D acceleration is on.. and it is being turned off
			if (!g_pUserOption->Use3DHAL)
			{
				g_Mode = MODE_CHANGE_OPTION;
			}
		}
		else
#endif
		{
			// When 3D acceleration is off.. and it is being turned on
			if (g_bEnable3DHAL && g_pUserOption->Use3DHAL)
			{
				g_Mode = MODE_CHANGE_OPTION;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
// Close Bookcase
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_BOOKCASE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_BOOKCASE");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	UI_CloseBookcase();
}

//-----------------------------------------------------------------------------
//
// Close Briefing
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_BRIEFING(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_BRIEFING");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	UI_CloseBriefing();
}

//-----------------------------------------------------------------------------
//
// Close Computer
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_COMPUTER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_COMPUTER");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	UI_CloseComputer();
}

//-----------------------------------------------------------------------------
//
// When leaving the Tutorial, select one weapon
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_CLOSE_TUTORIAL_EXIT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_TUTORIAL_EXIT");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	UI_CloseTutorialExit();

	int arms = static_cast<int>(left);	// 0: blade 1: sword 2: mace 3: cross 4: AR 5: TR 6: SMG 7:SG

	if (arms >= 0 && arms <=7)
	{
		ITEM_CLASS itemClass[] =
		{
			ITEM_CLASS_BLADE,
			ITEM_CLASS_SWORD,
			ITEM_CLASS_MACE,
			ITEM_CLASS_CROSS,
			ITEM_CLASS_AR,
			ITEM_CLASS_SR,
			ITEM_CLASS_SMG,
			ITEM_CLASS_SG
		};
		

			CGRequestNewbieItem _CGRequestNewbieItem;
		
			_CGRequestNewbieItem.setItemClass( itemClass[arms] );
			
			g_pSocket->sendPacket( &_CGRequestNewbieItem );
			
	}
}


//-----------------------------------------------------------------------------
//
// When closing the Desc Dialog
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_CLOSE_DESC_DIALOG(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_DESC_DIALOG");

	
	//if (g_Mode!=MODE_GAME)
	{
	//	DEBUG_ADD("Not Mode MODE_GAME");
	//	return;
	}
	
	// Close	
	gC_vs_ui.CloseDescDialog();
}

//-----------------------------------------------------------------------------
//
// When closing the Elevator
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_CLOSE_ELEVATOR(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_ELEVATOR");
	
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	// Close
	gC_vs_ui.CloseElevator();
}

//-----------------------------------------------------------------------------
//
// When selecting in the Elevator
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_SELECT_ELEVATOR(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_SELECT_ELEVATOR");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	// left 0 : 4th floor ....
	//		1 : 3
	//		2 : 2
	//		3 : 1
	//		4 : basement 1
	int selectPortal = static_cast<int>(4-left);

	if (selectPortal <= 4)
	{
		const MSector& sector = g_pZone->GetSector(g_pPlayer->GetX(), g_pPlayer->GetY());

		// a portal cell without floors goes nowhere
		if (sector.IsPortal() && sector.GetPortalSize() > 0)
		{
			int numPortal = sector.GetPortalSize();		
			
			selectPortal = min(selectPortal, numPortal - 1);	// the top floor, not the list's end

			PORTAL_LIST::const_iterator	iPortal = sector.GetPortalBegin();

			for (int i=0; i<selectPortal; i++)
			{
				iPortal++;		
			}		

			PORTAL_INFO portalInfo = *iPortal;

			int zoneID = portalInfo.ZoneID;

			// Move to zoneID.
			//-----------------------------------------------------
			// Send Packet
			//-----------------------------------------------------
				CGSelectPortal _CGSelectPortal;

				_CGSelectPortal.setZoneID( zoneID );

				g_pSocket->sendPacket( &_CGSelectPortal );
		}
	}

	gC_vs_ui.CloseElevator();
}

//-----------------------------------------------------------------------------
//
// When selecting a Server
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// When selecting a Server
//
//-----------------------------------------------------------------------------
/*
void	
UIMessageManager::Execute_UI_REQUEST_SERVER_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] Execute_UI_REQUEST_SERVER_LIST");

	
	if (g_Mode!=MODE_WAIT_SELECTPC)
	{
		DEBUG_ADD("Not Mode MODE_WAIT_SELECTPC");
		return;
	}

	
	#ifdef CONNECT_SERVER
		if (gC_vs_ui.IsCharManagerEnable())
		{
			CLGetServerList _CLGetServerList;					
			
			g_pSocket->sendPacket( &_CLGetServerList );

			gC_vs_ui.CharManagerDisable();
		}
	#endif
}
*/

//-----------------------------------------------------------------------------
//
// Inventory --> QuickSlot
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_ITEM_TO_QUICKITEMSLOT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
//	DEBUG_ADD("[UI] UI_ITEM_TO_QUICKITEMSLOT");
//	// focus_grid_x, focus_grid_y, (MItem *)p_item
//
//	
//	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
//	{
//		DEBUG_ADD("Not Mode MODE_GAME or Dead");
//		return;
//	}
//				
//	//-----------------------------------------------------------------
//	// if there is nothing to verify
//	//-----------------------------------------------------------------
//	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL
//		&& g_pPlayer->IsItemCheckBufferNULL())
//	{
//		if (gC_vs_ui.IsRunningExchange())
//		{
//			// During a trade just attach it to the Mouse.
//			Execute_UI_ITEM_PICKUP_FROM_INVENTORY(left, right, void_ptr);
//		}
//		else
//		{
//			MItem* pItem = (MItem*)void_ptr;
//
//			//---------------------------------------------------------
//			// It is a stackable item, and..
//			// check whether there is a QuickSlot
//			//---------------------------------------------------------
//			if (pItem->IsQuickItem() && g_pQuickSlot!=NULL)
//			{		
//				int itemID = pItem->GetID();
//				int itemX = pItem->GetGridX();
//				int itemY = pItem->GetGridY();
//
//				int num = g_pQuickSlot->GetSize();
//				int addTotal = -1;
//
//				int addIndex = -1;
//
//				//---------------------------------------------------------
//				// Where in the QuickSlot can it go?
//				//---------------------------------------------------------
//				for (int i=0; i<num; i++)
//				{
//					MItem* pQuickItem = g_pQuickSlot->GetItem( i );
//
//					//---------------------------------------------------------
//					// If the spot is empty, just put it in.
//					//---------------------------------------------------------
//					if (pQuickItem==NULL)
//					{						
//						addIndex = i;
//
//						MItem* pRemoveItem = g_pInventory->RemoveItem( itemX, itemY );
//
//						g_pQuickSlot->AddItem( pRemoveItem, addIndex );
//
//						PlaySound( pRemoveItem->GetInventorySoundID() );
//
//						break;
//					}
//					//---------------------------------------------------------
//					// If something is there.. check whether it can be stacked there.
//					//---------------------------------------------------------
//					else
//					{
//						//--------------------------------------------------------
//						// Verify whether it is a stackable item.
//						//--------------------------------------------------------
//						if (pQuickItem->GetItemClass()==pItem->GetItemClass()
//							&& pQuickItem->GetItemType()==pItem->GetItemType())
//						{
//							//----------------------------------------------------
//							// The summed count must not exceed max.
//							//----------------------------------------------------
//							addTotal = pQuickItem->GetNumber() + pItem->GetNumber();
//							if ( addTotal <= pQuickItem->GetMaxNumber() )
//							{
//								// Decide that it can be added at the i-th one.
//								addIndex = i;
//
//								//---------------------------------------------------
//								// Remove the Inventory's item.
//								//---------------------------------------------------
//								MItem* pRemoveItem = g_pInventory->RemoveItem( itemX, itemY );
//								if (pRemoveItem!=NULL)
//								{
//									// assert( pRemoveItem==pItem );
//									delete pRemoveItem;
//								}
//
//								pQuickItem->SetNumber( addTotal );
//
//								PlaySound( pQuickItem->GetInventorySoundID() );
//								
//								break;
//							}
//						}
//					}
//				}
//
//				if (addIndex != -1)
//				{
//					//---------------------------------------------------------
//					// SendPacket - Inventory to QuickItem
//					// Actually it is Inventory --> Mouse --> QuickItem.
//					//---------------------------------------------------------
//						//---------------------------------------------------
//						// Tell the server that the item that was in the Inventory
//						// send a packet to the server.
//						//---------------------------------------------------
//						CGAddInventoryToMouse _CGAddInventoryToMouse;
//						_CGAddInventoryToMouse.setObjectID( itemID );
//						_CGAddInventoryToMouse.setX( itemX );
//						_CGAddInventoryToMouse.setY( itemY );
//						
//						g_pSocket->sendPacket( &_CGAddInventoryToMouse );
//
//						//---------------------------------------------------
//						// Tell the server that the item that was on the mouse
//						// send a packet to the server.
//						//---------------------------------------------------
//						CGAddMouseToQuickSlot _CGAddMouseToQuickSlot;
//						_CGAddMouseToQuickSlot.setObjectID( itemID );
//						_CGAddMouseToQuickSlot.setSlotID( addIndex );
//						
//						g_pSocket->sendPacket( &_CGAddMouseToQuickSlot );
//						
//				}
//
//			}
//			//---------------------------------------------------------
//			// If it does not go into the Quick slot..
//			// just put it into Gear. -_-;
//			//---------------------------------------------------------
//			else
//			{
//				//---------------------------------------------------------
//				// For now..  hold it with the mouse..
//				//---------------------------------------------------------
//				g_pInventory->RemoveItem( pItem->GetID() );
//				Execute_UI_ITEM_PICKUP_FROM_INVENTORY(left, right, void_ptr);
//
//				int maxSlot;
//
//				//---------------------------------------------------------
//				// check whether it can go into the gear
//				//---------------------------------------------------------
//				if (g_pPlayer->IsSlayer())
//				{
//					maxSlot = MSlayerGear::MAX_GEAR_SLAYER;
//				}
//				else
//				{
//					maxSlot = MVampireGear::MAX_GEAR_VAMPIRE;
//				}
//
//				MItem* pGearItem = NULL;
//				MItem* pChangeItem = NULL;
//				int addSlot = -1;
//
//				for (int slot=0; slot<maxSlot; slot++)
//				{
//					MItem* pGearItem = NULL;
//
//					if (g_pPlayer->IsSlayer() && g_pSlayerGear->CanReplaceItem(pItem, (MSlayerGear::GEAR_SLAYER)slot, pGearItem)
//						|| g_pPlayer->IsVampire() && g_pVampireGear->CanReplaceItem(pItem, (MVampireGear::GEAR_VAMPIRE)slot, pGearItem)
//						|| g_pPlayer->IsOusters() && g_pOustersGear->CanReplaceItem(pItem, (MOustersGear::GEAR_OUSTERS)slot, pGearItem))
//					{
//						addSlot = slot;
//						pChangeItem = pGearItem;
//
//						// If the spot is empty, just put it in right away.
//						// If the spot is not empty.. look for the next one.
//						if (pGearItem==NULL)
//						{
//							break;
//						}							
//					} 
//				}
//				
//				//---------------------------------------------------------
//				// put it into the gear..
//				//---------------------------------------------------------
//				if (addSlot != -1)
//				{
//					Execute_UI_ITEM_DROP_TO_GEAR(addSlot, 0, pChangeItem);
//				}
//			}
//		}		
//	}
//	//-----------------------------------------------------------------
//	// if there is another item that needs verification
//	//-----------------------------------------------------------------
//	else
//	{
//		DEBUG_ADD( "[Error] There is another item in ItemCheckBuffer");
//	}
}

//-----------------------------------------------------------------------------
//
// Close Slayer Portal
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_CLOSE_SLAYER_PORTAL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_SLAYER_PORTAL");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


		CGSelectWayPoint _CGSelectWayPoint;
		
		_CGSelectWayPoint.setZoneID( 0 );
		_CGSelectWayPoint.setX( 0 );
		_CGSelectWayPoint.setY( 0 );

		g_pSocket->sendPacket( &_CGSelectWayPoint );

	g_pZone->RemoveHelicopter( g_pPlayer->GetID() );

	UI_CloseSelectWayPoint();
}

//-----------------------------------------------------------------------------
//
// Slayer Portal
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_SLAYER_PORTAL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_SLAYER_PORTAL");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	int zoneID = static_cast<int>(left);
	int zoneX = HIWORD(right);
	int zoneY = LOWORD(right);
	

//	if( g_pMoneyManager->GetMoney() < cost && g_pPlayer->IsOusters() )
//	{
//		UI_PopupMessage( STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY );
//		return;
//	}
	
		CGSelectWayPoint _CGSelectWayPoint;
		
		_CGSelectWayPoint.setZoneID( zoneID );
		_CGSelectWayPoint.setX( zoneX );
		_CGSelectWayPoint.setY( zoneY );

		g_pSocket->sendPacket( &_CGSelectWayPoint );


	if(g_pPlayer->IsSlayer())
	{
		g_pZone->RemoveHelicopter( g_pPlayer->GetID() );
		UI_CloseSelectWayPoint();
	}
	else if(g_pPlayer->IsOusters())
		gC_vs_ui.CloseHorn();
		
}

//-----------------------------------------------------------------------------
//
// When closing a received party request
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_FINISH_REQUEST_PARTY_BUTTON(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_FINISH_REQUEST_PARTY_BUTTON");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	BOOL bTimeout = static_cast<BOOL>(left);

	UI_ClosePartyRequest();

	if (bTimeout)
	{
			// Refuse the party invitation.
			CGPartyInvite _CGPartyInvite;
			_CGPartyInvite.setTargetObjectID( g_pTempInformation->PartyInviter );
			_CGPartyInvite.setCode( CG_PARTY_INVITE_REJECT );	

			g_pSocket->sendPacket( &_CGPartyInvite );		
	}
}

//-----------------------------------------------------------------------------
//
// Cancel a received party request
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_PARTY_REQUEST_CANCEL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_PARTY_REQUEST_CANCEL");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	
	//---------------------------------------------------------
	// Response to the Party? Y/N prompt
	//---------------------------------------------------------
	if (g_pTempInformation->GetMode() == TempInformation::MODE_PARTY_REQUEST)
	{	
		if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_PARTY)
		{
			g_pPlayer->SetWaitVerifyNULL();
		}

			CGPartyInvite _CGPartyInvite;
			_CGPartyInvite.setTargetObjectID( static_cast<ObjectID_t>(g_pTempInformation->Value1) );
			_CGPartyInvite.setCode( CG_PARTY_INVITE_CANCEL );				

			g_pSocket->sendPacket( &_CGPartyInvite );			
	
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		
		// Remove the Cancel party? prompt.
		UI_ClosePartyCancel();
	}	
}

//-----------------------------------------------------------------------------
//
// Party request response
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_PARTY_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_PARTY_ACCEPT");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	BOOL accept = (BOOL)left;

	int code;

	//---------------------------------------------------------
	// Response to the Trade? Y/N prompt
	//---------------------------------------------------------
	//if (g_pTempInformation->Mode == TempInformation::MODE_PARTY_REQUEST)
	{
		//---------------------------------------------------------
		// allow
		//---------------------------------------------------------
		if (accept)
		{
			code = CG_PARTY_INVITE_ACCEPT;
		}
		//---------------------------------------------------------
		// refuse
		//---------------------------------------------------------
		else
		{	
			//if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_TRADE)
			{
			//	g_pPlayer->SetWaitVerifyNULL();
			}

			code = CG_PARTY_INVITE_REJECT;
		}

			CGPartyInvite _CGPartyInvite;
			_CGPartyInvite.setTargetObjectID( g_pTempInformation->PartyInviter );
			_CGPartyInvite.setCode( code );				

			g_pSocket->sendPacket( &_CGPartyInvite );			
	
		//g_pTempInformation->Mode = TempInformation::MODE_NULL;		
	}
}

//-----------------------------------------------------------------------------
//
// Close Party Manager
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_CLOSE_PARTY_MANAGER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_PARTY_MANAGER");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	UI_CloseParty();
}

//-----------------------------------------------------------------------------
//
// Leave/kick from party
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_AWAY_PARTY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_AWAY_PARTY");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	// Leave party / kick
	// left :: -1 == leave, 0~4 == kick
	BOOL bLeftParty = (left==-1);

	if (g_pPlayer->IsWaitVerifyNULL())
	{
		//-----------------------------------------------------------------
		// when I leave the party
		//-----------------------------------------------------------------
		if (bLeftParty)
		{
			if (g_pParty->GetSize() > 0)
			{
					CGPartyLeave _CGPartyLeave;
				
					//_CGPartyLeave.setTargetName( g_pUserInformation->CharacterID.GetString() );
					
					g_pSocket->sendPacket( &_CGPartyLeave );
					
					g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_PARTY_LEAVE );
			}
		}
		//-----------------------------------------------------------------
		// when I kick someone else
		//-----------------------------------------------------------------
		else
		{
			if (g_pParty->IsKickAvailableTime())
			{
				int kickIndex = static_cast<int>(left);

				PARTY_INFO*	pInfo = g_pParty->GetMemberInfo( kickIndex );

				if (pInfo!=NULL)
				{
						CGPartyLeave _CGPartyLeave;
					
						_CGPartyLeave.setTargetName( pInfo->Name.GetString() );
						
						g_pSocket->sendPacket( &_CGPartyLeave );			

						g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_PARTY_LEAVE );
				}
			}
			else
			{
				g_pGameMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_NO_AUTHORITY].GetString() );
			}
		}		
	}
}


//-----------------------------------------------------------------------------
//
// Leave/kick from party
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_FINISH_REQUEST_DIE_BUTTON(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_FINISH_REQUEST_DIE_BUTTON");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}


	// Close die-_-request left == TRUE : timeout

	if (g_pPlayer->IsDead() && !g_pPlayer->IsWaitVerify())
	{
		int TempZoneID = g_pPlayer->GetResurrectZoneID();
		if(TempZoneID)
		{
			CGSelectWayPoint _CGSelectWayPoint;

			_CGSelectWayPoint.setZoneID( TempZoneID );
			_CGSelectWayPoint.setX( 0 );
			_CGSelectWayPoint.setY( 0 );

			g_pSocket->sendPacket( &_CGSelectWayPoint );

			g_pPlayer->SetResurrectZoneID(0);
		}
		else
		{
			CGResurrect _CGResurrect;
			
			g_pSocket->sendPacket( &_CGResurrect );
		}	
			// I should make a separate Verify instead of this
			// but I don't want to recompile, so for now... - -;
		g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_RESURRECT );

		gC_vs_ui.FinishRequestDie();
		gC_vs_ui.FinishRequestResurrect();
		gC_vs_ui.CloseRequestShrineMinimap();
	}
}

//-----------------------------------------------------------------------------
//
// When selecting a server
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_CONNECT_SERVER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
//	DEBUG_ADD_FORMAT("[UI] UI_CONNECT_SERVER. %s(%d)", (left? "group" : "server"), right);

	// left == true : group
	//         false : server
    //  right : id

	if(NULL == g_pSocket)
		return;
	
	BOOL bSelectGroup = static_cast<BOOL>(left);
	int	selectID = static_cast<int>(right);

	//-------------------------------------------------------
	// when World was selected
	//-------------------------------------------------------
	if (bSelectGroup)
	{
		if (g_Mode==MODE_WAIT_SELECT_WORLD)
		{
			// CLSelectWorld
				CLSelectWorld clSelectWorld;
			
				clSelectWorld.setWorldID( selectID );				
				g_pUserInformation->WorldID = selectID;

				g_pSocket->sendPacket( &clSelectWorld );				

			g_pServerInformation->SetServerGroupID( selectID );

			UIMessages::SetMode( MODE_WAIT_SERVER_LIST );
		}
		else
		{
			DEBUG_ADD("Not MODE_WAIT_SELECT_WORLD");
		}
	}
	//-------------------------------------------------------
	// when Server was selected
	//-------------------------------------------------------
	else
	{
		if (g_Mode==MODE_WAIT_SELECT_SERVER)
		{
			// CLSelectServer
				CLSelectServer clSelectServer;
			
				clSelectServer.setServerGroupID( selectID );				
				g_pUserInformation->ServerID = selectID;

				g_pSocket->sendPacket( &clSelectServer );				

				g_pSocket->setPlayerStatus( CPS_AFTER_SENDING_CL_GET_PC_LIST );

			g_pServerInformation->SetServerID( selectID );			

			UIMessages::SetMode( MODE_WAIT_PCLIST );
		}
		else
		{
			DEBUG_ADD("Not MODE_WAIT_SELECT_SERVER");
		}
	}
		
}

//-----------------------------------------------------------------------------
//
// When closing server selection
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_CLOSE_SERVER_SELECT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_SERVER_SELECT");

	// left == true : group 
	//         false : server
	BOOL bSelectGroup = static_cast<BOOL>(left);

	//------------------------------------------------------------
	// Going back from group selection returns to the initial screen
	//------------------------------------------------------------
	if (bSelectGroup)
	{
		UIMessages::SetMode( MODE_MAINMENU );
	}
	//------------------------------------------------------------
	// Going back from server selection returns to group selection..
	//------------------------------------------------------------
	else
	{
		UI_SetWorldList();

		UIMessages::SetMode( MODE_WAIT_SELECT_WORLD );
	}	
}

//-----------------------------------------------------------------------------
//
// When checking the character name
//
//-----------------------------------------------------------------------------
void	
UIMessageManager::Execute_UI_NEWCHARACTER_CHECK(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	DEBUG_ADD("[UI] UI_NEWCHARACTER_CHECK");

	char* pName = (char*)void_ptr;

	if (pName!=NULL)
	{
		if (g_pUserInformation->WhisperID.GetLength()==0)
		{
			//---------------------------------------------
			// check name length
			//---------------------------------------------
			BOOL bAllOK = TRUE;

			int len = static_cast<int>(strlen(pName));	
			
			if (static_cast<uint>(len)<PlayerInfo::minIDLength || static_cast<uint>(len)>PlayerInfo::maxIDLength)
			{
				char strTemp[128];
				SafeFormat::Format(strTemp, GetGameString(STRING_USER_REGISTER_ID_LENGTH), PlayerInfo::minIDLength, PlayerInfo::maxIDLength);
				g_pUIDialog->PopupFreeMessageDlg( strTemp );
				bAllOK = FALSE;
			}

			if (bAllOK)
			{
				if (!IsValidID(pName, "-_"))
				{
					//---------------------------------------------
					// if the ID is invalid
					//---------------------------------------------						
					g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
					bAllOK = FALSE;
				}
				else
				{
					char strName[80];
					SafeFormat::Copy(strName, pName);

					// if it contains bad words, it is not allowed
					if (g_pChatManager->RemoveCurse(strName))
					{
						g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
						bAllOK = FALSE;
					}
					else
					{
						MChatManager badNameManager;
						badNameManager.LoadFromFile( g_pFileDef->getProperty("FILE_INFO_CHAT_2").c_str());

						if (badNameManager.RemoveCurse(strName))
						{
							g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
							bAllOK = FALSE;
						}
					}
				}
			}

			if (bAllOK)
			{
					CLQueryCharacterName clQueryCharacterName;

					clQueryCharacterName.setCharacterName( pName );

					g_pSocket->sendPacket( &clQueryCharacterName );

					// temporarily.. -_-;;
					g_pUserInformation->WhisperID = pName;
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
// When selecting a mine/bomb to make
//
//-----------------------------------------------------------------------------
/*
void
UIMessageManager::Execute_UI_SELECT_EXPLOSIVE(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] Execute_UI_SELECT_EXPLOSIVE");
	
	int type = left;		// mine(2) / bomb(3)
	int itemType = right;	// type (mine:0~3, bomb:0~5)


	//--------------------------------------------------------
	// if there is nothing to verify
	//--------------------------------------------------------
	if (g_pPlayer->IsItemCheckBufferNULL()
		&& g_pTempInformation->Mode==TempInformation::MODE_NULL)
	{
		//--------------------------------------------------------
		// normal case
		//--------------------------------------------------------
		if (type==2 && itemType>=0 && itemType<6
			|| type==3 && itemType>=0 && itemType<4)
		{
			int itemClass = ((type==2)? ITEM_CLASS_MINE : ITEM_CLASS_BOMB);
				
			#ifdef CONNECT_SERVER
				CGMakeItem _CGMakeItem;
			
				_CGMakeItem.setItemClass( itemClass );
				_CGMakeItem.setItemType( itemType );

				g_pSocket->sendPacket( &_CGMakeItem );
				
			#endif	

			// remember the item
			g_pTempInformation->Mode = TempInformation::MODE_SKILL_MAKE_ITEM;
			g_pTempInformation->Value1 = itemClass;
			g_pTempInformation->Value2 = itemType;

			// block it from moving
			UI_LockItem();
			
			// Close the selection window.
			UI_CloseSelectExplosive();
		}
		//--------------------------------------------------------
		// if something went wrong
		//--------------------------------------------------------
		else
		{
			// Select what to make properly~~
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_ERROR_ETC_ERROR].GetString() );
		}
	}
	else
	{
		DEBUG_ADD("ItemCheckBuffer is not NULL. wait~~" );
	}
}
*/

//-----------------------------------------------------------------------------
//
// When closing the mine/bomb selection
//
//-----------------------------------------------------------------------------
/*
void
UIMessageManager::Execute_UI_CLOSE_SELECT_EXPLOSIVE(intptr_t left, intptr_t right, void* void_ptr)
{
	DEBUG_ADD("[UI] Execute_UI_CLOSE_SELECT_EXPLOSIVE");
	
	UI_CloseSelectExplosive();
}
*/

void
UIMessageManager::Execute_UI_CLOSE_TEAM_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_TEAM_LIST");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseTeamList();

		g_pUIDialog->UnSetLockInputPCTalk();
//		g_pUIDialog->ClosePCTalkDlg();
//		g_pPlayer->SetWaitVerifyNULL();
		// Make it possible to select something again?
		//g_pUIDialog->ShowPCTalkDlg();
	}
}

void
UIMessageManager::Execute_UI_CLOSE_TEAM_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseTeamInfo(); 
}

void
UIMessageManager::Execute_UI_CLOSE_TEAM_MEMBER_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseTeamMemberInfo();
}

void
UIMessageManager::Execute_UI_CLOSE_TEAM_REGIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_TEAM_REGIST");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		// Make it possible to select something again?
		if(!gC_vs_ui.ReturnTeamRegistMember())
			g_pUIDialog->ShowPCTalkDlg();

		gC_vs_ui.CloseTeamRegist();
	}
}

void
UIMessageManager::Execute_UI_CLOSE_TEAM_MEMBER_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] UI_CLOSE_TEAM_MEMBER_LIST");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	
	gC_vs_ui.CloseTeamMemberList();
//	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
//	{
//
//		// Make it possible to select something again?
//		g_pUIDialog->ShowPCTalkDlg();
//	}
}

void
UIMessageManager::Execute_UI_REQUEST_GUILD_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
		(void)left;
		(void)right;
		(void)void_ptr;
		CGSelectGuild  _CGSelectGuild ;
			
		_CGSelectGuild.setGuildID(g_pPlayer->GetGuildNumber());
		g_pSocket->sendPacket( &_CGSelectGuild  );				

}

	
void
UIMessageManager::Execute_UI_REQUEST_GUILD_MEMBER_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
		(void)left;
		(void)right;
		(void)void_ptr;
		CGRequestGuildMemberList  _CGRequestGuildMemberList ;
			
		g_pSocket->sendPacket( &_CGRequestGuildMemberList  );				

}

void
UIMessageManager::Execute_UI_MODIFY_GUILD_MEMBER(intptr_t left, intptr_t right, void* void_ptr)
{
		if (void_ptr == NULL)	// no name, nothing to send
			return;
		CGModifyGuildMember _CGModifyGuildMember;

		_CGModifyGuildMember.setGuildID(static_cast<GuildID_t>(left));
		_CGModifyGuildMember.setGuildMemberRank(static_cast<GuildMemberRank_t>(right));
		_CGModifyGuildMember.setName(std::string((char *)void_ptr));

		g_pSocket->sendPacket( &_CGModifyGuildMember );				

		gC_vs_ui.CloseTeamMemberInfo();
}

void
UIMessageManager::Execute_UI_SELECT_TEAM_MEMBER_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	char *pName = (char *)void_ptr;

	if(pName != NULL)
	{
			CGSelectGuildMember  _CGSelectGuildMember ;
			
			_CGSelectGuildMember.setName(std::string(pName));
			g_pSocket->sendPacket( &_CGSelectGuildMember  );				
	}
}


void
UIMessageManager::Execute_UI_SELECT_READY_TEAM_LIST(intptr_t left, intptr_t right, void* void_ptr)		// void_ptr = TEAM_NAME
{
	(void)right;
	char *pName = (char *)void_ptr;

	if(pName != NULL)
	{
			CGSelectGuild  _CGSelectGuild ;
			
			_CGSelectGuild.setGuildID(static_cast<GuildID_t>(left));
			g_pSocket->sendPacket( &_CGSelectGuild  );				
	}
}

//add by viva
void
UIMessageManager::Execute_UI_CLOSE_FRIEND_CHATTING_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	C_VS_UI_FRIEND_CHATTING_INFO* pInfo = (C_VS_UI_FRIEND_CHATTING_INFO*)void_ptr;
	gC_vs_ui.CloseFriendChattingInfo(pInfo);
}

void
UIMessageManager::Execute_UI_OPEN_FRIEND_CHATTING_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	C_VS_UI_FRIEND_INFO::FRIEND_LIST* pList = (C_VS_UI_FRIEND_INFO::FRIEND_LIST*)void_ptr;
	gC_vs_ui.OpenFriendChattingInfo(pList);
}

////////////////////////friend message////////////////////////////////////////////////////////
void UIMessageManager::Execute_UI_FRIEND_CHATTING_SEND_MESSAGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	C_VS_UI_FRIEND_CHATTING_INFO::FRIEND_SEND_MESSAGE* pMessage = (C_VS_UI_FRIEND_CHATTING_INFO::FRIEND_SEND_MESSAGE*)void_ptr;
	if (pMessage == NULL)
		return;
	GCFriendChatting gcFriend;
	gcFriend.setCommand(CG_MESSAGE);
	gcFriend.setPlayerName(pMessage->Name);
	gcFriend.setMessage(pMessage->Message);
	g_pSocket->sendPacket( &gcFriend );
	delete pMessage;
}
void UIMessageManager::Execute_UI_FRIEND_CHATTING_UPDATE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	GCFriendChatting gcFriend;
	gcFriend.setCommand(CG_UPDATE);
	g_pSocket->sendPacket( &gcFriend  );
}
void UIMessageManager::Execute_UI_FRIEND_CHATTING_ADD_FRIEND(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	if(void_ptr != NULL && gC_vs_ui.IsGameMode() && !gC_vs_ui.IsFriendWaitAskRuning())
	{
		char* pName = (char*)void_ptr;
		GCFriendChatting gcFriend;
		gcFriend.setCommand(CG_ADD_FRIEND);
		gcFriend.setPlayerName(string(pName));
		g_pSocket->sendPacket( &gcFriend );
//		Delete(pName);
	}
//	gC_vs_ui.SendAddFriend(pName);
}

//////////////////////////ask_friend_request///////////////////////////////////////////////////
void UIMessageManager::Execute_UI_FRIEND_REQUEST_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	C_VS_UI_ASK_DIALOG* pDialog = (C_VS_UI_ASK_DIALOG*)void_ptr;
	char* pName = (char*)pDialog->GetpTemporayValue();
	GCFriendChatting gcFriend;

	if(left == 0)
		gcFriend.setCommand(CG_ADD_FRIEND_AGREE);
	else if(left == 1)
		gcFriend.setCommand(CG_ADD_FRIEND_REFUSE);
	else if(left == 2)
		gcFriend.setCommand(CG_ADD_FRIEND_BLACK);

	if (pName != NULL)	// a dialog without a name answers no one
	{
		gcFriend.setPlayerName(string(pName));
		g_pSocket->sendPacket(&gcFriend);
	}
	
	DeleteNew(pDialog);
	DeleteNewArray(pName);
}
////////////////////////////////////////////ask_friend_close///////////////////////////////////
void UIMessageManager::Execute_UI_FRIEND_ASK_CLOSE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	C_VS_UI_ASK_DIALOG* pDialog = (C_VS_UI_ASK_DIALOG*)void_ptr;
	char* pName = (char*)pDialog->GetpTemporayValue();
	if(pDialog->GetAskType() == C_VS_UI_ASK_DIALOG::ASK_FRIEND_WAIT)
		gC_vs_ui.setFriendWaitAskNull();

	DeleteNew(pDialog);
	DeleteNewArray(pName);
}
//////////////////////////////////////////////ask_friend_delete_ask//////////////////////////////
void UIMessageManager::Execute_UI_FRIEND_DELETE_ASK(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	C_VS_UI_FRIEND_INFO::FRIEND_LIST* pList = (C_VS_UI_FRIEND_INFO::FRIEND_LIST*)void_ptr;
	if (pList == NULL || !gC_vs_ui.IsGameMode())	// no dialog to take and free the name
		return;
	char* pName = new char[pList->Name.size()+1];
	memcpy(pName, pList->Name.c_str(), pList->Name.size() + 1);
	gC_vs_ui.RunFriendDeleteAsk(pName);
}
////////////////////////////////////////////////ask_friend_delete_accept/////////////////////////
void UIMessageManager::Execute_UI_FRIEND_DELETE_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	C_VS_UI_ASK_DIALOG* pDialog = (C_VS_UI_ASK_DIALOG*)void_ptr;
	char* pName = (char*)pDialog->GetpTemporayValue();
	GCFriendChatting gcFriend;
	gcFriend.setCommand(CG_FRIEND_DELETE);
	if (pName != NULL)	// a dialog without a name deletes no one
	{
		gcFriend.setPlayerName(string(pName));
		g_pSocket->sendPacket(&gcFriend);
	}

	DeleteNew(pDialog);
	DeleteNewArray(pName);
}
//end

void
UIMessageManager::Execute_UI_SELECT_REGIST_TEAM_LIST(intptr_t left, intptr_t right, void* void_ptr)		// void_ptr = TEAM_NAME
{
	(void)right;
	char *pName = (char *)void_ptr;

	if(pName != NULL)
	{
			CGSelectGuild  _CGSelectGuild ;
			
			_CGSelectGuild.setGuildID(static_cast<GuildID_t>(left));
			g_pSocket->sendPacket( &_CGSelectGuild  );				
	}
}

	
void
UIMessageManager::Execute_UI_JOIN_READY_TEAM(intptr_t left, intptr_t right, void* void_ptr)				// void_ptr = TEAM_NAME
{
	(void)right;
	(void)void_ptr;
	CGTryJoinGuild _CGTryJoinGuild;
	_CGTryJoinGuild.setGuildID(static_cast<GuildID_t>(left));
	_CGTryJoinGuild.setGuildMemberRank(2);
	g_pSocket->sendPacket( &_CGTryJoinGuild );
}

void
UIMessageManager::Execute_UI_JOIN_REGIST_TEAM(intptr_t left, intptr_t right, void* void_ptr)			// void_ptr = TEAM_NAME
{
	(void)right;
	(void)void_ptr;
	CGTryJoinGuild _CGTryJoinGuild;
	_CGTryJoinGuild.setGuildID(static_cast<GuildID_t>(left));
	_CGTryJoinGuild.setGuildMemberRank(3);
	g_pSocket->sendPacket( &_CGTryJoinGuild );
}

	
void
UIMessageManager::Execute_UI_REGIST_GUILD_MEMBER(intptr_t left, intptr_t right, void* void_ptr)			// void_ptr = introduction max:150byte, remember to close the window!
{

	if(void_ptr == NULL)
	{
		// print error message
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_GUILD_REGIST_FAIL_INTRO].GetString() );
		return;
	}

	CGJoinGuild _CGJoinGuild;
	_CGJoinGuild.setGuildID(static_cast<GuildID_t>(left));
	_CGJoinGuild.setGuildMemberRank(static_cast<GuildMemberRank_t>(right));
	_CGJoinGuild.setGuildMemberIntro(std::string((char *)void_ptr));
	g_pSocket->sendPacket( &_CGJoinGuild );

	gC_vs_ui.CloseTeamRegist();
	gC_vs_ui.CloseTeamInfo();
}

void
UIMessageManager::Execute_UI_REGIST_GUILD_TEAM(intptr_t left, intptr_t right, void* void_ptr)			// left = TEAM_NAME, void_ptr = introduction max:150byte, remember to close the window!
{
	(void)right;

	if(void_ptr == NULL || (char *)left == NULL)
	{
		// print error message
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_GUILD_REGIST_FAIL_INTRO].GetString() );
		return;
	}

	CGRegistGuild _CGRegistGuild;
	_CGRegistGuild.setGuildName(std::string((char *)left));
	_CGRegistGuild.setGuildIntro(std::string((char *)void_ptr));
	g_pSocket->sendPacket( &_CGRegistGuild );

	gC_vs_ui.CloseTeamRegist();
}

	
void
UIMessageManager::Execute_UI_CLOSE_FILE_DIALOG(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	switch(left)
	{
	case C_VS_UI_FILE_DIALOG::MODE_PROFILE_SELECT:
		// A chosen profile picture (void_ptr, its file name) was loaded
		// into a CDirectDrawSurface here, scaled to 55x70 and saved as the
		// character's profile. In this port that class is a stub, so the
		// load never succeeded (see LoadImageToSurface's retirement in
		// task 4.231), and choosing a picture has done nothing. Restoring
		// it means loading into a CSpriteSurface sized to the picture
		// first (LoadImageToSurface in SpriteLib/SurfaceImage.h cannot size
		// one) and saving that.
		(void)void_ptr;
		break;
	}
	gC_vs_ui.CloseFileDialog();
}

//-----------------------------------------------------------------------------
//
// When enchanting an Inventory Item with the mouse's Item (EVENT_STAR)
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ENCHANT_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	DEBUG_ADD("[UI] UI_ENCHANT_ACCEPT");

	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}

	// gpC_mouse_pointer->GetPickUpItem()		// the one being held
	//void_ptr = MItem *		// where it goes in
	// Inventory coordinates (left,right)
	MItem* pItem = (MItem*)void_ptr;
	MItem* pMouseItem = gpC_mouse_pointer->GetPickUpItem();

	if (pMouseItem==NULL)
	{
		DEBUG_ADD_ERR("[Error] Mouse Item is NULL");
		return;
	}

	if (pItem==NULL)
	{
		DEBUG_ADD_ERR("[Error] InvenItem is NULL");
		return;
	}

	TYPE_OBJECTID mouseItemID = ((pMouseItem==NULL)?OBJECTID_NULL : pMouseItem->GetID());

	// Cannot Enchant during a trade!
	if (g_pTradeManager!=NULL)
		return;
	
	//g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY );
	
	//----------------------------------------------------
	// When connected to the Server
	//----------------------------------------------------
	if(g_pTempInformation->GetMode() == TempInformation::MODE_NULL && g_pPlayer->IsItemCheckBufferNULL())
	{
		CGAddItemToItem _CGAddItemToItem;
		_CGAddItemToItem.setObjectID( mouseItemID );
		_CGAddItemToItem.setX( pItem->GetGridX() );
		_CGAddItemToItem.setY( pItem->GetGridY() );								
		
		g_pSocket->sendPacket( &_CGAddItemToItem );
		
		g_pPlayer->SetItemCheckBuffer( pMouseItem, MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM );
		g_pTempInformation->SetMode(TempInformation::MODE_ITEM_TO_ITEM);
		g_pTempInformation->Value1 = pItem->GetGridX();
		g_pTempInformation->Value2 = pItem->GetGridY();

//		UI_LockItem();
	}
}

void
UIMessageManager::Execute_UI_ENCHANT_CANCEL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	UI_CloseEnchant();
}

void 
UIMessageManager::Execute_UI_MESSAGE_BOX(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if(left >= 0 && left < MAX_GAME_STRING)
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[static_cast<int>(left)].GetString() );
}


void 
UIMessageManager::Execute_UI_CLOSE_OTHER_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseOtherInfo();
}

void UIMessageManager::Execute_UI_MODIFY_TEAM_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	if(void_ptr != NULL)
	{
		CGModifyGuildIntro _CGModifyGuildIntro;
		_CGModifyGuildIntro.setGuildID(static_cast<GuildID_t>(left));
		_CGModifyGuildIntro.setGuildIntro(std::string((char *)void_ptr));
		g_pSocket->sendPacket( &_CGModifyGuildIntro );
	}
}

void UIMessageManager::Execute_UI_MODIFY_GUILD_MEMBER_INTRO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	if(void_ptr != NULL)
	{
		CGModifyGuildMemberIntro _CGModifyGuildMemberIntro;
		_CGModifyGuildMemberIntro.setGuildID(static_cast<GuildID_t>(left));
		_CGModifyGuildMemberIntro.setGuildMemberIntro(std::string((char *)void_ptr));
		g_pSocket->sendPacket( &_CGModifyGuildMemberIntro );
	}
}

void UIMessageManager::Execute_UI_SEND_NAME_FOR_SOUL_CHAIN(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	// if the track button is already pressed
	if(g_pPlayer->IsWaitVerify())
		return;

	if(void_ptr != NULL)
	{
		CGSkillToNamed _CGSkillToNamed;
		_CGSkillToNamed.setSkillType(SKILL_SOUL_CHAIN);
		_CGSkillToNamed.setTargetName(std::string((char *)void_ptr));
		_CGSkillToNamed.setCEffectID(0);
		g_pSocket->sendPacket( &_CGSkillToNamed );

		g_pPlayer->SetWaitVerify(MPlayer::WAIT_VERIFY_SKILL_SUCCESS);

//		(*g_pSkillInfoTable)[SKILL_SOUL_CHAIN].SetNextAvailableTime();
	}
}

void UIMessageManager::Execute_UI_CLOSE_TRACE_WINDOW(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
//	//------------------------------------------------------------------
//	// The success of the skill that the Player was waiting for was verified.
//	//------------------------------------------------------------------	
//	if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_SKILL_SUCCESS)
//	{		
//		g_pPlayer->SetWaitVerifyNULL();
//	}
//	else
//	{
//		DEBUG_ADD("[Error] Player is not WaitVerifySkillSuccess");
//	}
//
//	//------------------------------------------------------------------
//	// Release the Item Lock.
//	//------------------------------------------------------------------
//	if (g_pPlayer->GetItemCheckBufferStatus()==MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY)
//	{
//		g_pPlayer->ClearItemCheckBuffer();
//	}

	gC_vs_ui.CloseTraceWindow();
}

// Fix for Netmarble
void UIMessageManager::Execute_UI_RUN_CONNECT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	UI_RunConnect();
}

void UIMessageManager::Excute_UI_SELECT_GRADE_SKILL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if(left >= g_pRankBonusTable->GetSize() || (*g_pRankBonusTable)[static_cast<int>(left)].GetStatus() == RankBonusInfo::STATUS_CANNOT_LEARN || (*g_pRankBonusTable)[static_cast<int>(left)].GetStatus() == RankBonusInfo::STATUS_LEARNED
		|| !g_pPlayer->IsWaitVerifyNULL() || g_pTempInformation->GetMode() != TempInformation::MODE_NULL)
		return;

	CGSelectRankBonus _CGSelectRankBonus;
	_CGSelectRankBonus.setRankBonusType(static_cast<DWORD>(left));
	g_pSocket->sendPacket( &_CGSelectRankBonus );
	g_pTempInformation->SetMode(TempInformation::MODE_SKILL_LEARN);

}

void UIMessageManager::Excute_UI_USE_XMAS_TREE(intptr_t left, intptr_t right, void* void_ptr)				// left = MItem*, right = which window to close, void_ptr = message
{
	DEBUG_ADD("Excute_UI_USE_XMAS_TREE");
	MItem *pItem = (MItem *)left;

	if(pItem == NULL  || !g_pPlayer->IsItemCheckBufferNULL())
		return;

	char *sz_ptr = (char *)void_ptr;

	if(sz_ptr == NULL)
	{
		// There is an empty entry.
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_XMAS_CARD_CANNOT_USE].GetString() );
		return;
	}

	CGUseMessageItemFromInventory _CGUseMessageItemFromInventory;
	_CGUseMessageItemFromInventory.setObjectID( pItem->GetID() );
	_CGUseMessageItemFromInventory.setX( pItem->GetGridX() );
	_CGUseMessageItemFromInventory.setY( pItem->GetGridY() );
	_CGUseMessageItemFromInventory.setMessage(sz_ptr);
					
	g_pSocket->sendPacket( &_CGUseMessageItemFromInventory );
					
	g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

	if(right == 1)
		gC_vs_ui.CloseBulletinBoardWindow();
	else
		gC_vs_ui.CloseXmasCardWindow();
	
	DEBUG_ADD("Excute_UI_USE_XMAS_TREE OK");
}

void UIMessageManager::Excute_UI_CLOSE_XMAS_CARD_WINDOW(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseXmasCardWindow();
}


void UIMessageManager::Excute_UI_SEND_BRING_FEE(intptr_t left,intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	// If there is something to verify, return.
	if(g_pTempInformation->GetMode() != TempInformation::MODE_NULL)
	{
		DEBUG_ADD("Excute_UI_SEND_BRING_FEE TempInformation is not NULL");
		return;
	}
	// Fetch tax
	DEBUG_ADD("Excute_UI_SEND_BRING_FEE");
	// When fetching, leave it alone until the verification packet arrives.

	g_pTempInformation->SetMode(TempInformation::MODE_WAIT_BRING_FEE);
	g_pTempInformation->Value1 = left;

	CGWithdrawTax _CGWithdrawTax;

	_CGWithdrawTax.setGold((DWORD)left);
	g_pSocket->sendPacket( &_CGWithdrawTax );

	DEBUG_ADD("Excute_UI_SEND_BRING_FEE OK");
}

void UIMessageManager::Excute_UI_CLOSE_BRING_FEE_WINDOW(intptr_t left,intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	// if there is nothing to verify
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseBringFeeWindow();
		g_pUIDialog->ShowPCTalkDlg();
	}
}

void UIMessageManager::Excute_UI_CLOSE_WAR_LIST(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseWarListWindow();	
}

void UIMessageManager::Execute_UI_CLOSE_BLOOD_BIBLE_STATUS(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseBloodBibleStatus();	
}

void UIMessageManager::Execute_UI_SEND_NAME_FOR_COUPLE(intptr_t left, intptr_t right, void *void_ptr)
{	
	(void)right;
	if( g_pPlayer->GetWaitVerify() != MPlayer::WAIT_VERIFY_NULL 
		&& g_pTempInformation->GetMode()!=TempInformation::MODE_NULL)
	{
		g_pUIDialog->ClosePCTalkDlg();
		return;
	}

	if (void_ptr == NULL)	// no name, nothing to send or wait for
		return;

	int npcID = (*g_pPCTalkBox).GetNPCID();
	
	CGTypeStringList _CGTypeStringList;

	_CGTypeStringList.setType((CGTypeStringList::StringType) left);
	_CGTypeStringList.addString( std::string((char*) void_ptr) );	
	_CGTypeStringList.setParam( npcID );
	
	g_pSocket->sendPacket( &_CGTypeStringList );

//	gC_vs_ui.CloseInputNameWindow();
	gC_vs_ui.SetDisableInputName();

	g_pPlayer->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
	g_pTempInformation->SetMode(TempInformation::MODE_INPUT_NAME);
	g_pTempInformation->TimeValue1 = MonotonicClock::Now();
}

void UIMessageManager::Execute_UI_CLOSE_INPUT_NAME_WINDOW(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if( g_pPlayer->GetWaitVerify() != MPlayer::WAIT_VERIFY_NPC_ASK	)
	{
		g_pUIDialog->ClosePCTalkDlg();
		gC_vs_ui.CloseInputNameWindow();
		g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_NULL );
	}  else
	{
		if(g_pTempInformation->GetMode() == TempInformation::MODE_INPUT_NAME)
		{
			if( MonotonicClock::Now() - g_pTempInformation->TimeValue1 >= MonotonicClock::Millis(1000*60) )
			{
				g_pUIDialog->ClosePCTalkDlg();
				gC_vs_ui.CloseInputNameWindow();
				g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_NULL );
				g_pTempInformation->SetMode( TempInformation::MODE_NULL );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
// When an Item is used from Gear
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_USE_GEAR(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	DEBUG_ADD("[UI] UI_ITEM_USE_GEAR");

	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	//
	// left = slot
	// right = by FunctionKey
	//
	//MItem* pItem = (MItem*)void_ptr;
	if (g_pPlayer!=NULL && ((g_pPlayer->IsSlayer() && g_pSlayerGear!=NULL) || (g_pPlayer->IsVampire() && g_pVampireGear!= NULL) || (g_pPlayer->IsOusters() && g_pOustersGear!= NULL)))
	{
		MItem* pSlotItem = static_cast<MItem*>(void_ptr);

		if (pSlotItem!=NULL && pSlotItem->IsAffectStatus())
		{

			if (g_pPlayer->IsItemCheckBufferNULL())
			{
				// Let's use only items of our own race -_-;
				if((g_pPlayer->IsSlayer() && pSlotItem->IsSlayerItem()) ||
					(g_pPlayer->IsVampire() && pSlotItem->IsVampireItem()) ||
					(g_pPlayer->IsOusters() && pSlotItem->IsOustersItem()))
				{
					pSlotItem->UseQuickItem();
				}
			}
			//----------------------------------------------------
			// item check buffer not null
			//----------------------------------------------------
			else
			{
				DEBUG_ADD("[Wait] Item Check Buffer is Not NULL");
			} 
		}
		else
		{
			DEBUG_ADD_FORMAT("No usable item in slot=%d", left);
		}
	}
	else
	{
		DEBUG_ADD_ERR("[Error] There is no Gear");
	}

}

void
UIMessageManager::Execute_GO_BILING_PAGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if(left == TRUE)
	{
		// Quit..
		UIMessages::SetMode( MODE_QUIT );


	}
}

void 
UIMessageManager::Execute_UI_CLOSE_POPUP_MESSAGE(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.ClosePopupMessage();
}


void 
UIMessageManager::Execute_UI_CLOSE_QUEST_STATUS(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseQuestStatusWindow();
}

void 
UIMessageManager::Execute_UI_CLOSE_LOTTERY_CARD(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)void_ptr;
	gC_vs_ui.CloseLotteryCard();
	gC_vs_ui.SetQuestStatusInit();

	if(g_bEventQuestEnding && right == 5)
	{
		UI_CloseAllDialog();
		MEvent event;
		event.eventID = EVENTID_OUSTERS_FIN;
		event.eventType = EVENTTYPE_ZONE;
		event.eventFlag = EVENTFLAG_ONLY_EVENT_BACKGROUND | EVENTFLAG_NOT_DRAW_UI | EVENTFLAG_NOT_DRAW_CREATURE | 
			EVENTFLAG_NOT_DRAW_INFORMATION | EVENTFLAG_NOT_DRAW_CREATURE_SHADOW | EVENTFLAG_NOT_DRAW_ITEM | 
			EVENTFLAG_NOT_DRAW_MOUSE_POINTER | EVENTFLAG_NOT_DRAW_EFFECT | EVENTFLAG_DENY_INPUT | 
			EVENTFLAG_NOT_FADE_SCREEN | EVENTFLAG_NOT_PLAY_SOUND;
		event.parameter1 = 0;
		event.parameter4 = EVENTBACKGROUNDID_QUEST_2;
		g_pEventManager->AddEvent(event);			
	}
	g_bEventQuestEnding = false;	
}

void 
UIMessageManager::Execute_UI_LOTTERY_CARD_STATUS(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)void_ptr;
	// left = step, right = product ID
	
	CGLotterySelect _CGLotterySelect;
	
	_CGLotterySelect.setType( TYPE_SELECT_LOTTERY );
	_CGLotterySelect.setQuestLevel ( (DWORD) left-1 );
	_CGLotterySelect.setGiftID ( (DWORD) right );

	g_pSocket->sendPacket( &_CGLotterySelect );
}

void 
UIMessageManager::Execute_UI_FINISH_SCRATCH_LOTTERY(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	CGLotterySelect _CGLotterySelect;
	
	_CGLotterySelect.setType( TYPE_FINISH_SCRATCH );
	_CGLotterySelect.setQuestLevel ( 0 );
	_CGLotterySelect.setGiftID ( 0 );

	g_pSocket->sendPacket( &_CGLotterySelect );
	
}

void 
UIMessageManager::Execute_UI_CLOSE_IMAGE_NOTICE(intptr_t left,intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseImageNotice();
}

void 
UIMessageManager::Execute_UI_SELECT_ITEM_FROM_SHOP(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)right;
	(void)void_ptr;
	CGTakeOutGood _CGTakeOutGood;

	_CGTakeOutGood.setObjectID( (DWORD) left );

	g_pSocket->sendPacket( &_CGTakeOutGood );
}

void 
UIMessageManager::Execute_UI_CLOSE_ITEM_LIST( intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	g_pUIDialog->ClosePCTalkDlg();	
	gC_vs_ui.CloseItemListWindow();
}

void 
UIMessageManager::Execute_UI_CLOSE_BULLETIN_BOARD(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseBulletinBoardWindow();
}

void 
UIMessageManager::Execute_UI_TRANS_ITEM_CANCEL(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	UI_CloseTransItem();
}

void 
UIMessageManager::Execute_UI_TRANS_ITEM_ACCEPT(intptr_t left, intptr_t right, void *void_ptr)
{	
	(void)left;
	(void)right;
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	// gpC_mouse_pointer->GetPickUpItem()		// the one being held
	//void_ptr = MItem *		// where it goes in
	// Inventory coordinates (left,right)
	MItem* pItem = (MItem*)void_ptr;
	MItem* pMouseItem = gpC_mouse_pointer->GetPickUpItem();

	if (pMouseItem==NULL)
	{
		DEBUG_ADD_ERR("[Error] Mouse Item is NULL");
		return;
	}
	if (pItem==NULL)
	{
		DEBUG_ADD_ERR("[Error] InvenItem is NULL");
		return;
	}
	TYPE_OBJECTID mouseItemID = ((pMouseItem==NULL)?OBJECTID_NULL : pMouseItem->GetID());

	// Cannot Enchant during a trade!
	if (g_pTradeManager!=NULL)
		return;
	
	//g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY );
	
	//----------------------------------------------------
	// When connected to the Server
	//----------------------------------------------------
	if(g_pTempInformation->GetMode() == TempInformation::MODE_NULL && g_pPlayer->IsItemCheckBufferNULL())
	{
		CGAddItemToItem _CGAddItemToItem;
		_CGAddItemToItem.setObjectID( mouseItemID );
		_CGAddItemToItem.setX( pItem->GetGridX() );
		_CGAddItemToItem.setY( pItem->GetGridY() );								
		
		g_pSocket->sendPacket( &_CGAddItemToItem );
		
		g_pPlayer->SetItemCheckBuffer( pMouseItem, MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM );
		g_pTempInformation->SetMode(TempInformation::MODE_ITEM_TO_ITEM);
		g_pTempInformation->Value1 = pItem->GetGridX();
		g_pTempInformation->Value2 = pItem->GetGridY();

//		UI_LockItem();
	}
}

void 
UIMessageManager::Execute_UI_REQUEST_RESURRECT(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)right;
	(void)void_ptr;
	if( left == 0 || left == 1)
	{
		// Resurrect Scroll
		MItem* pItem = g_pInventory->FindItem( ITEM_CLASS_RESURRECT_ITEM, static_cast<TYPE_ITEMTYPE>(left) );
		if( pItem == NULL )
		{
			return;
		}

		CGUseItemFromInventory _CGUseItemFromInventory;
		_CGUseItemFromInventory.setObjectID( pItem->GetID() );
		_CGUseItemFromInventory.setX( pItem->GetGridX() );
		_CGUseItemFromInventory.setY( pItem->GetGridY() );
		
		g_pSocket->sendPacket( &_CGUseItemFromInventory );				
		g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	}
	else
	if( left == 2 )
	{
		// Eternity
		CGSkillToSelf			_CGSkillToSelf;

		_CGSkillToSelf.setSkillType( SKILL_ETERNITY );
		g_pSocket->sendPacket( &_CGSkillToSelf );
	}
	else if(left == 3)
	{
		CGSelectRegenZone _CGSelectRegenZone;
		_CGSelectRegenZone.setRegenZoneID(14);
		g_pSocket->sendPacket( &_CGSelectRegenZone );
	}
}

void 
UIMessageManager::Execute_UI_CLOSE_MIXING_FORGE(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)right;
	(void)void_ptr;
	if(left == 0 )
	{
		gC_vs_ui.CloseMixingForge();
		g_pPlayer->ClearItemCheckBuffer();
		g_pTempInformation->SetMode( TempInformation::MODE_NULL );
	} else
	{
		g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[static_cast<int>(left)].GetString() );		
	}
}

void 
UIMessageManager::Execute_UI_MIXING_FORGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	if( g_pPlayer->GetItemCheckBufferStatus() != MPlayer::ITEM_CHECK_BUFFER_USE_MIXING_ITEM ||
		g_pTempInformation->GetMode() != TempInformation::MODE_NULL )
		return;

	MItem *pFirstItem = g_pInventory->GetItemToModify( static_cast<TYPE_OBJECTID>(left) );
	MItem *pSecondItem = g_pInventory->GetItemToModify( static_cast<TYPE_OBJECTID>(right) );

	if( pFirstItem == NULL || pSecondItem == NULL )
	{
		return;
	}

	if( pFirstItem->GetItemClass() == pSecondItem->GetItemClass() &&
		pFirstItem->GetItemType() == pSecondItem->GetItemType() &&
		pFirstItem->GetItemOptionListCount() == 1 &&
		pSecondItem->GetItemOptionListCount() == 1 &&
		!pFirstItem->IsUniqueItem() && ! pSecondItem->IsUniqueItem() &&
		!pFirstItem->IsQuestItem() && ! pSecondItem->IsQuestItem() )
	{		
		MItem* pMixingItem = g_pPlayer->GetItemCheckBuffer();

		CGMixItem _CGMixItem;

		_CGMixItem.setObjectID( pMixingItem->GetID() );
		_CGMixItem.setX( pMixingItem->GetGridX() );
		_CGMixItem.setY( pMixingItem->GetGridY() );
		_CGMixItem.setTargetObjectID( 0, pFirstItem->GetID() );
		_CGMixItem.setTargetObjectID( 1, pSecondItem->GetID() );
		
		g_pSocket->sendPacket( &_CGMixItem );
		g_pPlayer->SetItemCheckBuffer( pMixingItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY );
		
		g_pTempInformation->SetMode( TempInformation::MODE_ITEM_MIXING );
		g_pTempInformation->Value1 = pFirstItem->GetGridX();
		g_pTempInformation->Value2 = pFirstItem->GetGridY();

		g_pTempInformation->Value3 = pSecondItem->GetGridX();
		g_pTempInformation->Value4 = pSecondItem->GetGridY();
		
		g_pTempInformation->pValue = (void*) pMixingItem;
	}		
}

void 
UIMessageManager::Execute_UI_CLOSE_REMOVE_OPTION(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseRemoveOptionFromRareItem();
}

void 
UIMessageManager::Execute_UI_SEND_REMOVE_OPTION(intptr_t left, intptr_t right, void *void_ptr)
{
	MItem *pMouseItem = (MItem*) void_ptr;
	MItem *pCurItem = g_pInventory->GetItemToModify( static_cast<TYPE_OBJECTID>(right) );


	if( left < 0 || left > 1 || pMouseItem == NULL || pCurItem == NULL)
	{
		g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_ERROR_ETC_ERROR].GetString());
		return;
	}

	CGMixItem _CGMixItem;
	_CGMixItem.setObjectID( pMouseItem->GetID() );
	_CGMixItem.setX( pCurItem->GetGridX() );
	_CGMixItem.setY( pCurItem->GetGridY() );
	_CGMixItem.setTargetObjectID( 0, pCurItem->GetID() );
	_CGMixItem.setTargetObjectID( 1, static_cast<ObjectID_t>(left) );
	g_pSocket->sendPacket( &_CGMixItem );
	gC_vs_ui.CloseRemoveOptionFromRareItem();

	g_pPlayer->SetItemCheckBuffer( pMouseItem, MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM );
	
	g_pTempInformation->SetMode(TempInformation::MODE_ITEM_TO_ITEM);
	g_pTempInformation->Value1 = pCurItem->GetGridX();
	g_pTempInformation->Value2 = pCurItem->GetGridY();
	g_pTempInformation->Value3 = left;
}

void 
UIMessageManager::Execute_UI_CLOSE_OUSTERS_SKILL_INFO(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseOustersSkillInfo();
}

void 
UIMessageManager::Execute_UI_LEARN_OUSTERS_SKILL(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)right;
	(void)void_ptr;
	if( g_pTempInformation->GetMode() != TempInformation::MODE_NULL )
		return;		
		
	DEBUG_ADD("[UI] UI_LEARN_OUSTERS_SKILL");
	
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	
	
	g_pTempInformation->SetMode(TempInformation::MODE_SKILL_LEARN);
	{
		(*g_pTempInformation).Value3 = left;
		
		CGLearnSkill _CGLearnSkill;
		
		//_CGLearnSkill.setObjectID( (*g_pPCTalkBox).GetNPCID() );
		_CGLearnSkill.setSkillType( static_cast<SkillType_t>(left) );
		_CGLearnSkill.setSkillDomainType( SKILLDOMAIN_OUSTERS );
		
		g_pSocket->sendPacket( &_CGLearnSkill );
		
	}
}

void 
UIMessageManager::Execute_UI_RUN_LEVELUP(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	UI_LevelUp();
}

void 
UIMessageManager::Execute_UI_CLOSE_HORN(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseHorn();
}


void 
UIMessageManager::Execute_UI_OUSTERS_DOWN_SKILL(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)right;
	(void)void_ptr;
	if( left < 0 || left >= MAX_ACTIONINFO)
		return;

	int curLevel = (*g_pSkillInfoTable)[static_cast<int>(left)].GetExpLevel();

	// 2004, 11, 8, sobeit modify start - depending on the Ouster skill, can be made even at level 0.
	if( curLevel > 30 || (curLevel == 1 && (*g_pSkillInfoTable)[static_cast<int>(left)].CanDelete == 0) )
		return;	
	// 2004, 11, 8, sobeit modify end
	
	CGDownSkill	_CGDownSkill;

	_CGDownSkill.setSkillType( (SkillType_t) left );

	g_pSocket->sendPacket( &_CGDownSkill );

	DEBUG_ADD("[UI] Execute_UI_OUSTERS_DOWN_SKILL"); 
}

void 
UIMessageManager::Execute_UI_CLEAR_ALL_STAGE(intptr_t left, intptr_t right, void *void_ptr)
{	
	(void)right;
	(void)void_ptr;
	// LEFT = GAMETYPE
	switch( left )
	{
	case 0 :				// Dug it all up!!!!
		break;
	case 1 :
		break;
	}
	gC_vs_ui.CloseArrowTile();
	gC_vs_ui.CloseCrazyMine();
}

void 
UIMessageManager::Execute_UI_CLOSE_FINDING_MINE(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseFindingMine();
}

void 
UIMessageManager::Execute_UI_CLOSE_NEMONEMO(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	//gC_vs_ui.CloseNemo
}

void 
UIMessageManager::Execute_UI_CLOSE_PUSHPUSH(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
}

void 
UIMessageManager::Execute_UI_CLOSE_CRAZY_MINE(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseCrazyMine();

	CGFailQuest _CGFailQuest;

	_CGFailQuest.setFail( false );

	g_pSocket->sendPacket( &_CGFailQuest );
	gC_vs_ui.SetQuestStatusInit();
}

void 
UIMessageManager::Execute_UI_CLOSE_ARROW_TILE(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseArrowTile();
	CGFailQuest _CGFailQuest;

	_CGFailQuest.setFail( false );

	g_pSocket->sendPacket( &_CGFailQuest );
	gC_vs_ui.SetQuestStatusInit();
}

void 
UIMessageManager::Execute_UI_CLEAR_STAGE(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)void_ptr;
	WORD	GameType = HIWORD(left);
	WORD	Stage = LOWORD(left);
	DWORD	Time = (DWORD)right;

	Time/=10;

	Time = min(DWORD(65535), Time);
	DEBUG_ADD_FORMAT("[Execute_UI_CLEAR_STAGE] GameType:%d Stage:%d Time:%d", GameType, Stage,Time);	
	
	CGSubmitScore _CGSubmitScore;

	_CGSubmitScore.setGameType( GameType == 0 ? GAME_ARROW : GAME_MINE );
	_CGSubmitScore.setLevel( static_cast<BYTE>(Stage) );
	_CGSubmitScore.setScore( WORD(Time) );
	g_pSocket->sendPacket( &_CGSubmitScore );
}

void
UIMessageManager::Execute_UI_FORCE_DIE(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)right;
	(void)void_ptr;
	// Kill me
	gC_vs_ui.CloseAllDialog();
	
	switch(left)
	{
	case 10 :						// when died while doing ArrowTile
		break;
	case 11 :						// when died while doing CrazyMine
		break;
	}
	
	// Send a kill-me packet to the server.
	CGFailQuest		_CGFailQuest;
	
	_CGFailQuest.setFail( true );
	g_pSocket->sendPacket( &_CGFailQuest );
	gC_vs_ui.SetQuestStatusInit();
}

void 
UIMessageManager::Execute_UI_ADD_ITEM_TO_CODE_SHEET(intptr_t left, intptr_t right, void *void_ptr)
{
	MItem* pItem = (MItem*)void_ptr;

	if( pItem == NULL || right < 0 || right >= 5)
		return;

	CGAddItemToCodeSheet _CGAddItemToCodeSheet;

	_CGAddItemToCodeSheet.setX( static_cast<Coord_t>(HIWORD( left )) );
	_CGAddItemToCodeSheet.setY( static_cast<Coord_t>(LOWORD( left )) );
	_CGAddItemToCodeSheet.setObjectID( pItem->GetID() );
	
	g_pSocket->sendPacket( &_CGAddItemToCodeSheet );
	g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM );	

	g_pTempInformation->SetMode(TempInformation::MODE_ITEM_CODE_SHEET);
	g_pTempInformation->Value1 = right;	
	g_pTempInformation->Value2 = left;
	g_pTempInformation->pValue = (void*)gpC_mouse_pointer->GetPickUpItem();
}


void 
UIMessageManager::Execute_UI_SEND_BUG_REPORT(intptr_t left,intptr_t right, void *void_ptr)
{
	(void)right;
	SendBugReport("[UI_BUG] %d, %s", left, reinterpret_cast<char*>( void_ptr ) );
}

void 
UIMessageManager::Execute_UI_GO_BEGINNER_ZONE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
//	Execute_UI_CLOSE_POPUP_MESSAGE(left,right,void_ptr);
//
//	CGSelectWayPoint _CGSelectWayPoint;
//
//	_CGSelectWayPoint.setZoneID( 1122 );
//	_CGSelectWayPoint.setX( 0 );
//	_CGSelectWayPoint.setY( 0 );
//
//	g_pSocket->sendPacket( &_CGSelectWayPoint );
}

void
UIMessageManager::Execute_UI_POPUP_MESSAGE_OK(intptr_t left, intptr_t right, void* void_ptr)
{
	gC_vs_ui.CloseMailBox();
	
	Execute_UI_CLOSE_POPUP_MESSAGE(left,right,void_ptr);

	DWORD recvID = (DWORD) left;
	
	int zoneID = 0;

	switch(recvID)
	{
	// beginner zone
	case 1:
		zoneID = 1122;
		break;

	case 2:
		zoneID = 1122;
		break;
		
	// Start level war
	case 4:
		zoneID = 1131;
		break;

	// Start race war
	case 6:
		zoneID = 72;
		break;

	}

	CGSelectWayPoint _CGSelectWayPoint;
	
	_CGSelectWayPoint.setZoneID( zoneID );
	_CGSelectWayPoint.setX( 0 );
	_CGSelectWayPoint.setY( 0 );
	
	g_pSocket->sendPacket( &_CGSelectWayPoint );
}

void 
UIMessageManager::Execute_UI_CLOSE_SHRINE_MINIMAP(intptr_t left, intptr_t right, void* void_ptr)
{
	// Send as Random-_-. This is the case of a forced close.

	bool bSended = false;

	for(int i = 0; i < g_pRegenTowerInfoManager->GetSize(); i++ )
	{
		const RegenTowerInfo* pInfo = &g_pRegenTowerInfoManager->Get(i);
		
		if( pInfo->owner == (int)g_pPlayer->GetRace() )
		{
			CGSelectRegenZone _CGSelectRegenZone;
			_CGSelectRegenZone.setRegenZoneID( i );
			g_pSocket->sendPacket( &_CGSelectRegenZone );
			bSended = true;
			break;
		}
	}
	gC_vs_ui.CloseRequestShrineMinimap();

	if( bSended == false )
	{
		Execute_UI_FINISH_REQUEST_DIE_BUTTON(left, right, void_ptr);
	}
	gC_vs_ui.FinishRequestDie();
	gC_vs_ui.FinishRequestResurrect();	
}

void 
UIMessageManager::Execute_UI_WARP_TO_REGEN_TOWER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if( g_pRegenTowerInfoManager == NULL || left < 0 || 
		left >= g_pRegenTowerInfoManager->GetSize() || 
		g_pRegenTowerInfoManager->Get(static_cast<int>(left)).owner != g_pPlayer->GetRace() )
		return;

	CGSelectRegenZone _CGSelectRegenZone;
	
	_CGSelectRegenZone.setRegenZoneID( static_cast<BYTE>(left) );

	g_pSocket->sendPacket( &_CGSelectRegenZone );
	gC_vs_ui.CloseRequestShrineMinimap();
	gC_vs_ui.FinishRequestDie();
	gC_vs_ui.FinishRequestResurrect();	
}

void 
UIMessageManager::Execute_UI_CLOSE_MAILBOX(intptr_t left, intptr_t right, void *void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseMailBox();
}

void
UIMessageManager::Execute_UI_CLOSE_PET_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.ClosePetInfo();
}

void	
UIMessageManager::Execute_UI_PET_GAMBLE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	CGPetGamble _CGPetGamble;
	g_pSocket->sendPacket( &_CGPetGamble );
	// Prevent removing items during gambling
	UI_LockItem();
}

void
UIMessageManager::Execute_UI_CLOSE_USE_PET_FOOD(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	gC_vs_ui.CloseUsePetFood();
}

void	 
UIMessageManager::Execute_UI_CLOSE_PETSTORAGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	gC_vs_ui.ClosePetStorage();
	
	// Stop storage
	if (g_pStorage!=NULL)
	{
		delete g_pStorage;
		g_pStorage = NULL;
	}

	// Remove the storage.
	gC_vs_ui.SetPetStorage( NULL );

//	// Make it possible to select something again?
//	g_pUIDialog->ShowPCTalkDlg();

	g_pTempInformation->SetMode(TempInformation::MODE_NULL);

}
void	 
UIMessageManager::Execute_UI_CLOSE_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseKeepPetItemDialog();
}
void	 
UIMessageManager::Execute_UI_CLOSE_GET_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	gC_vs_ui.CloseGetKeepPetItemDialog();
}

void	 
UIMessageManager::Execute_UI_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	DEBUG_ADD("[UI] Execute_UI_KEEP_PETITEM");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	if (g_pTempInformation->GetMode() != TempInformation::MODE_NULL)
		return;
	MItem* pItem = (MItem*)void_ptr;

	if(NULL == pItem)
		return;

	CGDepositPet _CGDepositPet;
	_CGDepositPet.setObjectID(pItem->GetID());
	if(-1 != right)
		_CGDepositPet.setIndex(static_cast<BYTE>(right));
	else
		return;
	g_pSocket->sendPacket( &_CGDepositPet );	

	g_pTempInformation->SetMode(TempInformation::MODE_PETITEM_MOVETO_PETSTORAGE);
	g_pTempInformation->Value1 = right;
	g_pTempInformation->Value2 = pItem->GetGridX();
	g_pTempInformation->Value3 = pItem->GetGridY();
	g_pTempInformation->pValue = pItem;
	UI_LockItem(); 
}
void	 
UIMessageManager::Execute_UI_GET_KEEP_PETITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	DEBUG_ADD("[UI] Execute_UI_GET_KEEP_PETITEM");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	if (g_pTempInformation->GetMode() != TempInformation::MODE_NULL)
		return;
	MItem* pItem = (MItem*)void_ptr;

	if(NULL == pItem)
		return;

	CGWithdrawPet _CGWithdrawPet;
	_CGWithdrawPet.setObjectID(pItem->GetID());
	if(right<20 &&right>-1)
		_CGWithdrawPet.setIndex(static_cast<BYTE>(right));
	else
		return;
	g_pSocket->sendPacket( &_CGWithdrawPet );	
	g_pTempInformation->SetMode(TempInformation::MODE_PETITEM_MOVETO_INVENTORY);
	g_pTempInformation->Value1 = right;
	g_pTempInformation->Value2 = pItem->GetGridX();
	g_pTempInformation->Value3 = pItem->GetGridY();
	g_pTempInformation->pValue = pItem;
	UI_LockItem();
}


void	 
UIMessageManager::Execute_UI_CLOSE_SMS_MESSAGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		if(gC_vs_ui.IsRunningSMSRecord())
			gC_vs_ui.CloseSMSRecord();
		if(gC_vs_ui.IsRunningSMSList())
			gC_vs_ui.CloseSMSList();
		gC_vs_ui.CloseSMSMessage(); 
		
	}

}
void
UIMessageManager::Execute_UI_SEND_SMS_MESSAGE(intptr_t left, intptr_t right, void* void_ptr)
{
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	if (g_pTempInformation->GetMode() != TempInformation::MODE_NULL)
		return;

	std::list<std::string> *TempStr = (std::list<std::string>*)(void_ptr);

	if(NULL == TempStr || TempStr->size()>5
		|| (char *)left == NULL || (char *)right == NULL)	// no caller or no text
		return ;
	
	// CGSMSSend::read() asserts the message length below MAX_MESSAGE_LENGTH.
	std::string strMessage( (char *)right );

	if (strMessage.size() >= MAX_MESSAGE_LENGTH)
	{
		strMessage.resize( MAX_MESSAGE_LENGTH - 1 );
	}

	CGSMSSend _CGSMSSend;
	_CGSMSSend.setCallerNumber(std::string((char *)left));
	_CGSMSSend.setMessage(strMessage);
	_CGSMSSend.clearString(); 
	std::list<std::string>::iterator itr = TempStr->begin();
	while(itr != TempStr->end())
	{
		string str = (*itr);
		_CGSMSSend.addString(str);
		itr++;
	}
	g_pSocket->sendPacket( &_CGSMSSend );
	
	gC_vs_ui.CloseSMSMessage();
	if(gC_vs_ui.IsRunningSMSList())
		gC_vs_ui.CloseSMSList();
	if(gC_vs_ui.IsRunningSMSRecord())
		gC_vs_ui.CloseSMSRecord();
}


void	 
UIMessageManager::Execute_UI_SMS_OPEN_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.RunSMSList();
		// sent sms address request packet
		CGSMSAddressList _CGSMSAddressList;
		g_pSocket->sendPacket( &_CGSMSAddressList );
	}
}

void	 
UIMessageManager::Execute_UI_SMS_RECORD(intptr_t left, intptr_t right, void* void_ptr)
{
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		if ((char *)left == NULL || (char *)right == NULL || void_ptr == NULL)	// a field missing
			return;

		CGAddSMSAddress	_CGAddSMSAddress;
		_CGAddSMSAddress.setCharacterName(std::string((char *)left));
		_CGAddSMSAddress.setCustomName((std::string((char *)right)));
		_CGAddSMSAddress.setNumber((std::string((char *)void_ptr)));
		g_pSocket->sendPacket( &_CGAddSMSAddress );

		// Keep copies, not the callers' pointers: the reply arrives frames later
		// and the dialog that owns these strings can be closed and deleted in
		// the meantime, so GCAddressListVerifyHandler would read freed memory.
		g_pTempInformation->SetMode(TempInformation::MODE_SMS_ADD_ADDRESS);
		g_pTempInformation->StrValue1 = (char *)left;
		g_pTempInformation->StrValue2 = (char *)right;
		g_pTempInformation->StrValue3 = (char *)void_ptr;

	}

}


void	 
UIMessageManager::Execute_UI_SMS_DELETE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	// sent registration request packet
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		CGDeleteSMSAddress	_CGDeleteSMSAddress;
		_CGDeleteSMSAddress.setElementID((DWORD)left);
		g_pSocket->sendPacket( &_CGDeleteSMSAddress );

		g_pTempInformation->SetMode(TempInformation::MODE_SMS_DELETE_ADDRESS);
		g_pTempInformation->Value1 = left;
	}
}

void	 
UIMessageManager::Execute_UI_CLOSE_SMS_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseSMSList();
	}
}

void	 
UIMessageManager::Execute_UI_CLOSE_SMS_RECORD(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseSMSRecord();
	}
}
void	 
UIMessageManager::Execute_UI_SMS_ADD_SEND_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.AddSMSSendList((char*)left);
	}
}


void	 
UIMessageManager::Execute_UI_CLOSE_NAMING(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseNaming();
	}
}

void	 
UIMessageManager::Execute_UI_SELECT_NAMING(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		CGSelectNickname	_CGSelectNickName;
		_CGSelectNickName.setNicknameID((WORD)left);
		g_pSocket->sendPacket( &_CGSelectNickName );

//		g_pTempInformation->SetMode(TempInformation::MODE_NICKNAME_CHANGE_CUSTOM);
//		g_pTempInformation->Value1 = left;
	}
}


void	 
UIMessageManager::Execute_UI_CLOSE_NAMING_CHANGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseNamingChange();
	}
}


void	 
UIMessageManager::Execute_UI_CHANGE_CUSTOM_NAMING(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	// Remove the return after the server setup is done and tested..
//	return;
	// left : name, right : item ID(0=Wolverine) void_ptr:penitem pointer
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		// The editor's SetByteLimit(22) counts characters, not bytes, and the
		// DBCS conversion emits up to two bytes per character - so 22 Korean
		// characters are 44 bytes, and even 22 ASCII ones need 23 with the
		// terminator. Copy under the size of the buffer rather than trusting
		// the limit.
		char szTemp[64];
		const char* pName = (const char*) left;

		if(pName == NULL)
			return;

		Basic::CopyBounded(szTemp, pName, sizeof(szTemp)-1);
		szTemp[sizeof(szTemp)-1] = '\0';

		if(strlen(szTemp) == 0)
			return;
//		if (!IsValidID(szTemp, "-_"))
//		{
//			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
//			bAllOK = FALSE;
//		}
//		else
//		{
//			char strName[80];
//			strcpy(strName, pChar->sz_name);
//
//			// if it contains bad words, it is not allowed
//			if (g_pChatManager->RemoveCurse(strName))
//			{
//				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_USER_REGISTER_INVALID_ID].GetString() );
//				bAllOK = FALSE;
//			}
//		}
		g_pChatManager->RemoveCurse(szTemp );

		// The nickname is the DBCS conversion's output, not UTF-8, so the cut is a plain one.
		if(strlen(szTemp) > MAX_NICKNAME_SIZE)
			szTemp[MAX_NICKNAME_SIZE] = '\0';

		// At this point it would be good to search the effect status and send the item use packet..
		MItem* pItem = NULL;	
		
		if(right != 0)
			pItem = g_pInventory->GetItemToModify( static_cast<TYPE_OBJECTID>(right) );//(MItem*)void_ptr;
		
		if(pItem!= NULL || right == 0)
		{
			CGModifyNickname	_CGModifyNickname; 
			_CGModifyNickname.setNickname(std::string((char *)szTemp));

			if(right != 0)
			{ 
				// First send a packet saying the item was used - set to not send
			//	Execute_UI_ITEM_USE(pItem->GetID(), 0, (void*)pItem);
				// Send the nickname change packet..
				_CGModifyNickname.setNicknameID(pItem->GetID());
				g_pSocket->sendPacket( &_CGModifyNickname );

				// A copy, not the editor's pointer: the reply arrives frames
				// later and the naming dialog can be deleted before then.
				g_pTempInformation->SetMode(TempInformation::MODE_NICKNAME_CHANGE_CUSTOM);
				g_pTempInformation->StrValue1 = szTemp;				// the new nickname
				g_pTempInformation->Value2 = pItem->GetItemType();	// item type

				if(25 != g_pTempInformation->Value2)  // time-limited item
					g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
			}
			else
			{
				// For a Wolverine, change without an item..
				_CGModifyNickname.setNicknameID(0);
				g_pSocket->sendPacket( &_CGModifyNickname );
				g_pTempInformation->SetMode(TempInformation::MODE_NICKNAME_CHANGE_CUSTOM);
				g_pTempInformation->StrValue1 = szTemp;				// the new nickname
				g_pTempInformation->Value2 = 0;
			}
		}
		else
			UI_PopupMessage (UI_STRING_MESSAGE_FAIL_SEARCH_ITEM);
	}
}

void	 
UIMessageManager::Execute_UI_RUN_NAMING_CHANGE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		MItem* pItem = (MItem*)left;
		if(pItem == NULL || pItem->GetItemClass() != ITEM_CLASS_EVENT_GIFT_BOX)
			return;
		// If pItem is an item that changes a character nickname
			// one-time (2 kinds) - edit existing nickname, add new nickname
			// time-limited (1 kind) - existing nickname can be edited without limit for a given time
		// If pItem is an item that changes a pet nickname, usable only when the pet is summoned
			// No item is needed for a Wolverine
		
		if(pItem->GetItemType() == 22 || pItem->GetItemType() == 25) // edit user nickname
			gC_vs_ui.RunNamingChange(pItem, (char*)g_pPlayer->GetNickName().c_str());
//		// add nickname
		else if(pItem->GetItemType() == 24) // extra naming pen // naming add item
			gC_vs_ui.RunNamingChange(pItem, (char*)(*g_pGameStringTable)[UI_STRING_MESSAGE_ADD_PLAYER_NICKNAME].GetString());
//		// time-limited item
//		else if(pItem->GetItemType() == 25) // limited naming pen // naming change time item
//			gC_vs_ui.RunNamingChange(pItem, (char*)g_pPlayer->GetNickName().c_str());
		else if(pItem->GetItemType() == 23) // edit pet nickname
		{
			MFakeCreature *pFakeCreature = (MFakeCreature *)g_pZone->GetFakeCreature(g_pPlayer->GetPetID());
			if(NULL != pFakeCreature)
			{
				MPetItem *pPetItem = pFakeCreature->GetPetItem();
				if(pPetItem != NULL)
				{
					if(2 == pPetItem->GetItemType()) // For a Wolverine, no pen item is needed
						UI_PopupMessage( UI_STRING_MESSAGE_PET_NAMING_WOLVERINE );
					else
						gC_vs_ui.RunNamingChange(pItem, (char*)pFakeCreature->GetNickName().c_str());
				}
			}
			else
				UI_PopupMessage( UI_STRING_MESSAGE_PET_NAMING_SUMMON );
		}
		//gC_vs_ui.RunNamingChange(pItem, char* szCustom);

	}
}

void	 
UIMessageManager::Execute_UI_CLOSE_QUEST_MANAGER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseQuestManager();
	}
}
void	 
UIMessageManager::Execute_UI_CLOSE_QUEST_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseQuestList();
	}
}
void	 
UIMessageManager::Execute_UI_CLOSE_QUEST_DETAIL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseQuestDetail();
	}
}
void	 
UIMessageManager::Execute_UI_CLOSE_QUEST_MISSION(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseQuestMission();
	}
}
void	 
UIMessageManager::Execute_UI_CLOSE_QUEST_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseQuestItem();
	}
}
void	 
UIMessageManager::Execute_UI_CLOSE_QUEST_ICON(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		gC_vs_ui.CloseQuestIcon(static_cast<int>(left));
	}
}
void	 
UIMessageManager::Execute_UI_GQUEST_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		CGGQuestAccept	_CGGQuestAccept;
		_CGGQuestAccept.setQuestID((DWORD)left);
		g_pSocket->sendPacket( &_CGGQuestAccept );
	}
}
void	 
UIMessageManager::Execute_UI_GQUEST_GIVEUP(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}

	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		CGGQuestCancel	_CGGQuestCancel;
		_CGGQuestCancel.setQuestID((DWORD)left);
		g_pSocket->sendPacket( &_CGGQuestCancel );
	}
}
//-----------------------------------------------------------------------------
//
// When an Item is used from the GQuest Inventory
//
//-----------------------------------------------------------------------------
void
UIMessageManager::Execute_UI_ITEM_USE_GQUEST_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_GQUEST_ITEM");
	
	if (g_Mode!=MODE_GAME || g_pPlayer->IsDead())
	{
		DEBUG_ADD("Not Mode MODE_GAME or Dead");
		return;
	}
	MItem* pItem =  (MItem*)void_ptr ;//(MItem*)void_ptr; 

	if(NULL != pItem)
	{
		CGUseItemFromGQuestInventory _CGUseItemFromGQuestInventory;
		_CGUseItemFromGQuestInventory.setIndex( static_cast<BYTE>(left) );
		
		g_pSocket->sendPacket( &_CGUseItemFromGQuestInventory );
		
		g_pPlayer->SetItemCheckBuffer( pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GQUEST_INVENTORY);
	}
}


void
UIMessageManager::Execute_UI_ITEM_USE_REQUEST_GUILD_LIST(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;

	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_REQUEST_GUILD_LIST");
	CGRequestGuildList _CGRequestGuildlist;	
	_CGRequestGuildlist.setGuildType(static_cast<GuildType_t>(left));
	g_pSocket->sendPacket(&_CGRequestGuildlist); 

}


void
UIMessageManager::Execute_UI_ITEM_USE_UNION_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;

	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_UNION_INFO");
	CGRequestUnionInfo _CGRequestUnioninfo;	
	g_pSocket->sendPacket(&_CGRequestUnioninfo); 

}


void
UIMessageManager::Execute_UI_ITEM_USE_REQUEST_UNION(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_REQUEST_UNION");
	
	if(g_pUserInformation->GuildGrade != 1) // if not the guild master
	{
		UI_PopupMessage(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_1);
		return;
	}
	else if(g_pUserInformation->dwUnionID != 0)
	{
		UI_PopupMessage(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_2);
		return;
	}
	CGRequestUnion _CGRequestUnion;	
	_CGRequestUnion.setGuildID(static_cast<GuildID_t>(left));
	g_pSocket->sendPacket(&_CGRequestUnion); 

	gC_vs_ui.CloseTeamInfoDialogBox();
}
void
UIMessageManager::Execute_UI_ITEM_USE_QUIT(intptr_t left, intptr_t right, void* void_ptr)
{	
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_REQUEST_DENY");

	if(g_pUserInformation->GuildGrade != 1) // if not the guild master
	{
		UI_PopupMessage(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_1);
		return;
	}

	CGQuitUnion   _CGQuitUnion;
	_CGQuitUnion.setGuildID(static_cast<GuildID_t>(left));
	_CGQuitUnion.setQuitMethod(static_cast<BYTE>(right));
	g_pSocket->sendPacket(&_CGQuitUnion); 

	gC_vs_ui.ClosePopupMessage();
	gC_vs_ui.CloseTeamInfoDialogBox();
}
void
UIMessageManager::Execute_UI_ITEM_USE_EXPER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_EXPER"); 
	if(g_pUserInformation->GuildGrade != 1) // if not the guild master
	{
		UI_PopupMessage(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_1);
		return;
	}
	else if(g_pUserInformation->bUnionGrade != 0)
	{
		UI_PopupMessage(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_3);
		return;
	}
	CGExpelGuild  _CGExpelGuild;
	_CGExpelGuild.setGuildID(static_cast<GuildID_t>(left));
	g_pSocket->sendPacket(&_CGExpelGuild); 

	gC_vs_ui.CloseTeamInfoDialogBox();
	gC_vs_ui.CloseTeamInfo();
	gC_vs_ui.CloseTeamList();
}

void
UIMessageManager::Execute_UI_ITEM_USE_UNION_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_UNION_ACCEPT");
	CGAcceptUnion  _CGAcceptUnion;
	_CGAcceptUnion.setGuildID(static_cast<GuildID_t>(left));
	g_pSocket->sendPacket(&_CGAcceptUnion); 

	gC_vs_ui.CloseMailBoxDialogBox();
}

void
UIMessageManager::Execute_UI_ITEM_USE_UNION_DENY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_UNION_DENY");
	CGDenyUnion  _CGDenyUnion;
	_CGDenyUnion.setGuildID(static_cast<GuildID_t>(left));
	g_pSocket->sendPacket(&_CGDenyUnion); 

	gC_vs_ui.CloseMailBoxDialogBox();

}


void
UIMessageManager::Execute_UI_ITEM_USE_UNION_QUIT_ACCEPT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_UNION_QUIT_ACCEPT");
	CGQuitUnionAccept _CGQuitUnionAccept;
	_CGQuitUnionAccept.setGuildID(static_cast<GuildID_t>(left));
	g_pSocket->sendPacket(&_CGQuitUnionAccept); 

	gC_vs_ui.CloseMailBoxDialogBox();
}

void
UIMessageManager::Execute_UI_ITEM_USE_UNION_QUIT_DENY(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_ITEM_USE_UNION_QUIT_DENY");
	CGQuitUnionDeny _CGQuitUnionDeny;
	_CGQuitUnionDeny.setGuildID(static_cast<GuildID_t>(left));
	g_pSocket->sendPacket(&_CGQuitUnionDeny); 

	gC_vs_ui.CloseMailBoxDialogBox();
}




void
UIMessageManager::Execute_UI_RECALL_BY_NAME(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_RECALL_BY_NAME");
	const char *szSelectedID = gC_vs_ui.GetTeamMember_SelectedID();
	if(szSelectedID != NULL && strlen(szSelectedID)>0)
	{
		char szBuf[128];
		// Never send a truncated player identity as a different recall target.
		if (strlen(szSelectedID) + sizeof("*recall ") > sizeof(szBuf)) return;
		SafeFormat::Format(szBuf, "*recall %s", szSelectedID );
		CGSay _CGSay;
		_CGSay.setMessage( szBuf );	//pWansungString );
		_CGSay.setColor( 0 );
		g_pSocket->sendPacket( &_CGSay );
	}
}

void
UIMessageManager::Execute_UI_UI_MODIFY_TAX(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_UI_MODIFY_TAX");

	CGModifyTaxRatio _CGModifyTaxRatio;
	_CGModifyTaxRatio.setRatio(static_cast<uint>(left));
	g_pSocket->sendPacket( &_CGModifyTaxRatio );

}


void
UIMessageManager::Execute_UI_APPOINT_SUBMASTER(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	DEBUG_ADD("[UI] Execute_UI_APPOINT_SUBMASTER");
	if (void_ptr == NULL)	// no name, nothing to send
		return;

	CGAppointSubmaster _CGAppointSubmaster;
	_CGAppointSubmaster.setGuildID(static_cast<GuildID_t>(left));
	_CGAppointSubmaster.setName(std::string((char *)void_ptr));
	g_pSocket->sendPacket( &_CGAppointSubmaster );

}

//2004.11.27
 void
UIMessageManager::Execute_UI_DISPLAY_ITEM(intptr_t left, intptr_t right, void* void_ptr)
 {
	(void)left;
	(void)right;
	  	DEBUG_ADD("[UI] UI_SELECT_PERSNALSHOP_SLOT");

	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	if(g_pStorage2 == NULL || void_ptr == NULL)
		return;	


	MItem* pItem = (MItem*)void_ptr;
	g_pStorage2->SetCurrent( 0 );	// for checking
 

	
	//TYPE_OBJECTID mouseItemID = pMouseItem->GetID();		
	
	// An Event GiftBox item cannot be put down.
	if (pItem->GetItemClass()!=ITEM_CLASS_EVENT_GIFT_BOX
		//&& pItem->GetItemClass()!=ITEM_CLASS_VAMPIRE_AMULET
//		&& pItem->GetItemClass()!=ITEM_CLASS_COUPLE_RING
//		&& pItem->GetItemClass()!=ITEM_CLASS_VAMPIRE_COUPLE_RING
//		&& !pItem->IsQuestItem()
//		&& pItem->IsNormalItem()
//		&& !((pItem->GetItemClass() == ITEM_CLASS_EVENT_TREE) && pItem->GetItemType() >= 13  )
//		&& pItem->GetItemClass() != ITEM_CLASS_EVENT_ITEM
//		&& pItem->GetItemClass() != ITEM_CLASS_CODE_SHEET
//		&& !(pItem->GetItemClass() == ITEM_CLASS_LUCKY_BAG && pItem->GetItemType() == 3)
		)
	{
		int current_storage = g_pStorage2->GetCurrent();
		const int storage_size = g_pStorage2->GetSize();
		const int storage_max = STORAGE_SLOT;
		
			
			for(int i = 0; i < storage_max; i++)
			{
				const MItem *p_slot_item = g_pStorage2->GetItem(i);
				
				// If the slot is empty, just put it in~
				if(p_slot_item == NULL)
				{
					CGDisplayItem _CGDisplayItem;
					_CGDisplayItem.setIndex(i);
					_CGDisplayItem.setItemObjectID(pItem->GetID());
					_CGDisplayItem.setPrice(pItem->GetPersnalPrice());
					//_CGDisplayItem.setXY(pMouseItem->GetGridX(),pMouseItem->GetGridY());
					_CGDisplayItem.setXY(pItem->GetGridX(),pItem->GetGridY());
					g_pSocket->sendPacket( &_CGDisplayItem ); 
					break;
				}
			}
			current_storage ++;
			if(current_storage >= storage_size)
				current_storage = 0;

		}
		else
		{
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_STORAGE].GetString());
		}
	}

 void
UIMessageManager::Execute_UI_UNDISPLAY_ITEM(intptr_t left, intptr_t right, void* void_ptr)
 {
	(void)right;

	MItem* pMouseItem = (MItem*)void_ptr;

	if(g_pStorage2 == NULL)
		return;

	if (pMouseItem!=NULL)
	{
//		if (pStorageItem!=NULL)
//		{
			MItem* pRemovedItem = g_pStorage2->RemoveItem( static_cast<unsigned int>(left) );

			if (pRemovedItem!=NULL)
			{
				 CGUndisplayItem _CGUndisplayItem;
				 _CGUndisplayItem.setIndex(static_cast<BYTE>(left));
				 _CGUndisplayItem.setItemObjectID(pRemovedItem->GetID());
				 //_CGUndisplayItem.setXY(left,right);
				 _CGUndisplayItem.setXY(pMouseItem->GetGridX(),pMouseItem->GetGridY());
				 
				 g_pSocket->sendPacket( &_CGUndisplayItem );

				 // The stall's copy is ours now; often void_ptr itself, so it goes last.
				 delete pRemovedItem;
			}
//		}
	}
	DEBUG_ADD("[UI] Execute_UI_UNDISPLAY_ITEM");
 }

// CGStoreSignFactory::getPacketMaxSize() advertises szBYTE + 80.
static const size_t STORE_SIGN_MAX_BYTES = 80;

 void
UIMessageManager::Execute_UI_STORE_SIGN(intptr_t left, intptr_t right, void* void_ptr)
 {
	(void)left;
	(void)right;
	 DEBUG_ADD("[UI] Execute_UI_STORE_SIGN");
	if (void_ptr == NULL)	// no sign, nothing to send
		return;

	DEBUG_ADD_FORMAT("[Original] %s", (char*)void_ptr );

	char * pernalshop_message = (char*)void_ptr;
    CGStoreSign _CGStoreSign;

	// Not UTF-8: the sign is ASCII bytes mixed with raw UTF-16 units, so the cut is a plain one.
	std::string strSign( pernalshop_message );

	if (strSign.size() > STORE_SIGN_MAX_BYTES)
	{
		strSign.resize( STORE_SIGN_MAX_BYTES );
	}

	g_pChatManager->RemoveCurse( strSign.data() );
	_CGStoreSign.setSign(strSign);
	g_pSocket->sendPacket( &_CGStoreSign );

	gC_vs_ui.ClosePersnalShopMessage();
 }

 void
 UIMessageManager::Execute_UI_STORE_OPEN(intptr_t left, intptr_t right, void* void_ptr)
 {
	(void)left;
	(void)right;
	(void)void_ptr;
	 	DEBUG_ADD("[UI] Execute_UI_STORE_OPEN");
		
		CGStoreOpen _CGCGStoreOpen;
		gC_vs_ui.SetPersnalShoptoUI(2);
		g_pSocket->sendPacket( &_CGCGStoreOpen );
				
 }
 
 void
UIMessageManager::Execute_UI_STORE_CLOSE(intptr_t left, intptr_t right, void* void_ptr)
 {
	(void)left;
	(void)right;
	(void)void_ptr;
	 DEBUG_ADD("[UI] Execute_UI_STORE_CLOSE");
	 
	 CGStoreClose _CGStoreClose;
	 gC_vs_ui.SetPersnalShoptoUI(3);
	 
	 g_pSocket->sendPacket( &_CGStoreClose );


 }

 void
UIMessageManager::Execute_UI_REQUEST_STORE_INFO(intptr_t left, intptr_t right, void* void_ptr)
 {
	(void)void_ptr;
	 DEBUG_ADD("[UI] Execute_UI_REQUEST_STORE_INFO");
	 
	 if(g_pPlayer->IsFlyingCreature() 
		 || g_pPlayer->IsUndergroundCreature() 
		 //|| g_pPlayer->IsGroundCreature() 
		 || g_pPlayer->IsFakeCreature() 
		 || g_pPlayer->GetMoveDevice()== 1
		 || g_pPlayer->GetMoveDevice()== 2
		 || gC_vs_ui.IsRunningSlayerPortal()
		 || gC_vs_ui.IsRunningHorn()
		)
	 {
		 DEBUG_ADD("[UI] Execute_DON'T OPEN PERSNAL SHOP Cuz your's type can't open PersnalShop./......");
	 }
	 else
	 {
		 if(right == 1)
			 gC_vs_ui.SetPersnalShoptoUI(1); 
	 

		 CGRequestStoreInfo _CGRequestStoreInfo;
		 _CGRequestStoreInfo.setOwnerObjectID(static_cast<ObjectID_t>(left));
		 g_pSocket->sendPacket( &_CGRequestStoreInfo );
	 }

 }

 void
UIMessageManager::Execute_UI_MY_STORE_INFO(intptr_t left, intptr_t right, void* void_ptr)
 {
	(void)left;
	(void)right;
	(void)void_ptr;

	 DEBUG_ADD("[UI] Execute_UI_MY_STORE_INFO"); // item information list
	 
	 GCMyStoreInfo _GCMyStoreInfo;
	 _GCMyStoreInfo.getStoreInfo();

//	 if (g_pStorage2!=NULL)
//		 delete g_pStorage2;
//	 
//	 g_pStorage2 = new MStorage;
//	 
//	 g_pStorage2->Init( left ); 
//	 gC_vs_ui.RunPersnalShop();
//	 gC_vs_ui.SetPersnalShop( g_pStorage2 );
//	 
	 
	 // write() and getPacketSize() read the StoreInfo, which nothing sets here
	 if (_GCMyStoreInfo.getStoreInfo() != NULL)
		 g_pSocket->sendPacket( &_GCMyStoreInfo );

 }

void
UIMessageManager::Execute_UI_OTHER_STORE_INFO(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	// not sure
		 DEBUG_ADD("[UI] Execute_UI_OTHER_STORE_INFO");
	 
	  GCOtherStoreInfo _GCOtherStoreInfo;
	 _GCOtherStoreInfo.setObjectID(UIMessages::GetSelectedCreature());
	  // write() and getPacketSize() read the StoreInfo, which nothing sets here
	  if (_GCOtherStoreInfo.getStoreInfo() != NULL)
		  g_pSocket->sendPacket( &_GCOtherStoreInfo );
}

void
UIMessageManager::Execute_UI_BUY_STORE_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	DEBUG_ADD("[UI] Execute_UI_BUY_STORE_ITEM");
	
	if(UIMessages::GetSelectedCreature() == 0)
		return;
	MItem* pItem = (MItem*)void_ptr;		// the item in the inventory (may be NULL)
	
	if(pItem != NULL)		
	{
		CGBuyStoreItem _CGBuyStoreItem;
		_CGBuyStoreItem.setIndex(static_cast<BYTE>(left));
		_CGBuyStoreItem.setItemObjectID(pItem->GetID());
		_CGBuyStoreItem.setOwnerObjectID(gC_vs_ui.GetOtherObjectID());
		
		g_pSocket->sendPacket( &_CGBuyStoreItem );
	}

}

void
UIMessageManager::Execute_UI_REMOVE_STORE_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	 DEBUG_ADD("[UI] Execute_UI_REMOVE_STORE_ITEM");
	 
	 GCRemoveStoreItem _GCRemoveStoreItem;
	 _GCRemoveStoreItem.setIndex(0);
	 _GCRemoveStoreItem.setOwnerObjectID(g_pPlayer->GetID());
	 g_pSocket->sendPacket( &_GCRemoveStoreItem );
}

void
UIMessageManager::Execute_UI_ADD_STORE_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;

	DEBUG_ADD("[UI] Execute_UI_ADD_STORE_ITEM");
	 
	 GCAddStoreItem _GCAddStoreItem;
	 _GCAddStoreItem.setIndex(0);
	 _GCAddStoreItem.setOwnerObjectID(g_pPlayer->GetID());
	 g_pSocket->sendPacket( &_GCAddStoreItem );
}


void
UIMessageManager::Execute_UI_CLOSE_PERSNALSHOP(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_PERSNALSHOP");
	gC_vs_ui.ClosePersnalShop();	
	
	if (g_pStorage2!=NULL)
	{
		delete g_pStorage2;
		g_pStorage2 = NULL;
	}
	 
	gC_vs_ui.SetPersnalShop( NULL );
}

void
UIMessageManager::Execute_UI_CLOSE_POWER_JJANG(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_POWER_JJANG");
	gC_vs_ui.ClosePowerjjang();
}
void
UIMessageManager::Execute_UI_REQUEST_POWER_JJANG_POINT(intptr_t left, intptr_t right, void* void_ptr)
{ 
	(void)left;
	(void)right;
	DEBUG_ADD("[UI] Execute_UI_REQUEST_POWER_JJANG_POINT");
	if (void_ptr == NULL)	// no number, nothing to send
		return;

	CGRequestPowerPoint _CGRequestPowerPoint;
	_CGRequestPowerPoint.setCellNum(std::string((char *)void_ptr));
	g_pSocket->sendPacket( &_CGRequestPowerPoint );
}
void
UIMessageManager::Execute_UI_POWER_JJANG_GAMBLE(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_POWER_JJANG_GAMBLE");

	CGUsePowerPoint _CGUsePowerPoint;
	g_pSocket->sendPacket( &_CGUsePowerPoint );
}


void
UIMessageManager::Execute_UI_CLOSE_SWAPADVANCEMENTITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CLOSE_SWAPADVANCEMENTITEM");

	gC_vs_ui.FinishSwapAdvancementItem();
}

void
UIMessageManager::Execute_UI_SWAPADVANCEMENTITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_SWAPADVANCEMENTITEM");
	
	if (g_Mode!=MODE_GAME)
	{
		DEBUG_ADD("Not Mode MODE_GAME");
		return;
	}
	
	if (g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
	{
		const MItem* pItem = g_pInventory->GetItem( static_cast<BYTE>(left), static_cast<BYTE>(right) );

		if (pItem!=NULL && g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
		{
			// An Event GiftBox item cannot be put down.
			
			if (!pItem->IsUniqueItem() && !pItem->IsQuestItem())
			{
				//-----------------------------------------------------
				// Send a packet to sell the item.
				//-----------------------------------------------------
					CGShopRequestSell	_CGShopRequestSell;

					_CGShopRequestSell.setObjectID( (*g_pPCTalkBox).GetNPCID() );		// NPC ID	
					_CGShopRequestSell.setOpCode( SHOP_REQUEST_SWAP_ADVANCEMENT_ITEM );
					_CGShopRequestSell.setItemObjectID( pItem->GetID() );	// Item Object ID

					g_pSocket->sendPacket( &_CGShopRequestSell );
			
						
					//-------------------------------------------------
					// Set Temp Information for verification
					//-------------------------------------------------
					(*g_pTempInformation).Mode	= TempInformation::MODE_SHOP_SELL;
					(*g_pTempInformation).pValue = (void*)pItem;

					//-------------------------------------------------
					// so that other items cannot be accessed..
					//-------------------------------------------------
					UI_LockItemTrade();
			}
			else
			{
				g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[UI_STRING_MESSAGE_SWAP_ADVANCEMENT_ITEM_ERROR ].GetString());
			}
		}
		else
		{
			DEBUG_ADD_ERR("[Error] Item Is NULL");
		}
	}
}

void
UIMessageManager::Execute_UI_LEARN_ADVANCE_SKILL(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_LEARN_ADVANCE_SKILL");
	CGLearnSkill _CGLearnSkill;
	
	_CGLearnSkill.setSkillType( static_cast<SkillType_t>(left) );
	_CGLearnSkill.setSkillDomainType( static_cast<SkillDomainType_t>(right) );
	
	g_pSocket->sendPacket( &_CGLearnSkill );
}

// 2005, 1, 11, sobeit add start - charity donation related
void
UIMessageManager::Execute_UI_CAMPAIGN_HELP(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_CAMPAIGN_HELP");
	
	// Send the donation packet; the dialog's amount is in units of 10,000.
	std::uint32_t gold = 0;
	if (MMoneyManager::DonationGold(left, g_pMoneyManager->GetMoney(), gold))
	{
		CGDonationMoney _CGDonationMoney;
		_CGDonationMoney.setGold(static_cast<Gold_t>(gold));
		_CGDonationMoney.setDonationType(static_cast<BYTE>(right));
		g_pSocket->sendPacket( &_CGDonationMoney );
	}
	else if (left > 0)
	{
		// More than the balance: the message the server's
		// NPC_RESPONSE_NOT_ENOUGH_MONEY reply would have shown.
		g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY].GetString());
	}
// 2005, 1, 11, sobeit add end - charity donation related
}

// 2005, 1, 20, sobeit add start - quest related
void
UIMessageManager::Execute_UI_RUN_NEXT_GQUEST_EXCUTE_ELEMENT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)left;
	(void)right;
	(void)void_ptr;
	// Execute the next element stored in the quest element List.
	DEBUG_ADD("[UI] Execute_UI_RUN_NEXT_GQUEST_EXCUTE_ELEMENT");
	gC_vs_ui.RunNextGQuestExcuteElement();
}

void
UIMessageManager::Execute_UI_GQUEST_SET_ACTION(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_GQUEST_SET_ACTION");
	// Perform a specific action when a specific element runs.
	if(left == 1) // dead
	{
		g_pPlayer->SetDead();
		g_pPlayer->SetResurrectZoneID(static_cast<int>(right));
	}
}

void
UIMessageManager::Execute_UI_GQUEST_ENDING_EVENT(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_GQUEST_ENDING_EVENT");
	// Run the related event when a specific quest is completed.
	MEvent event;
	event.eventID = EVENTID_ADVANCEMENT_QUEST_ENDING;
	event.eventType = EVENTTYPE_ZONE;
	event.eventFlag = EVENTFLAG_ONLY_EVENT_BACKGROUND | EVENTFLAG_NOT_DRAW_UI | EVENTFLAG_NOT_DRAW_CREATURE | EVENTFLAG_NOT_DRAW_INFORMATION | EVENTFLAG_NOT_DRAW_CREATURE_SHADOW | EVENTFLAG_NOT_DRAW_ITEM | EVENTFLAG_NOT_DRAW_MOUSE_POINTER | EVENTFLAG_NOT_DRAW_EFFECT | EVENTFLAG_DENY_INPUT | EVENTFLAG_NOT_FADE_SCREEN | EVENTFLAG_NOT_PLAY_SOUND;
	switch(left)
	{
	case 1: // Slayer promotion quest ending event
		event.parameter1 = 0;
		event.parameter4 = 1;
		g_pEventManager->AddEvent(event);
		break;
	case 2: // Vampire promotion quest ending event
		event.parameter1 = 0;
		event.parameter4 = 3;
		g_pEventManager->AddEvent(event);
		break;
	case 3: // Ouster promotion quest ending event
		event.parameter1 = 0;
		event.parameter4 = 5;
		g_pEventManager->AddEvent(event);
		break;
	}
	
}
// 2005, 1, 20, sobeit add end - quest related

// 2005, 1, 24, sobeit add start - item receiving event related
void
UIMessageManager::Execute_UI_REQUEST_EVENT_ITEM(intptr_t left, intptr_t right, void* void_ptr)
{
	(void)right;
	(void)void_ptr;
	DEBUG_ADD("[UI] Execute_UI_REQUEST_EVENT_ITEM");
	{
		CGGetEventItem _CGGetEventItem;
		_CGGetEventItem.setEventType(static_cast<BYTE>(left));
		g_pSocket->sendPacket( &_CGGetEventItem );
	}
}
// 2005, 1, 24, sobeit add end - item receiving event related
#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
	// 2005, 2, 25, sobeit add start - close the sub inventory.
	void
	UIMessageManager::Execute_UI_CLOSE_INVENTORY_SUB(intptr_t left, intptr_t right, void* void_ptr)
	{
		DEBUG_ADD("[UI] Execute_UI_CLOSE_INVENTORY_SUB");
		{
			gC_vs_ui.CloseSubInventory();
		}
	}

	void
	UIMessageManager::Execute_UI_ITEM_DROP_TO_INVENTORY_SUB(intptr_t left, intptr_t right, void* void_ptr)
	{
		DEBUG_ADD("[UI] Execute_UI_ITEM_DROP_TO_INVENTORY_SUB");
		
		if (g_Mode!=MODE_GAME || g_pPlayer->IsDead() || !gC_vs_ui.IsRunningSubInventory())
		{
			DEBUG_ADD("Not Mode MODE_GAME or Dead");
			return;
		}

		MItem* pMultiPackItem = gC_vs_ui.GetSubInventoryItem();

		if(NULL == pMultiPackItem)
			return;

		MItem* pMouseItem = UI_GetMouseItem();	// the current mouse item


		//-----------------------------------------------------------------
		// if there is nothing to verify
		//-----------------------------------------------------------------
		if (g_pPlayer->IsItemCheckBufferNULL() && pMouseItem!=NULL
			&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
		{		
			if(pMouseItem->GetItemClass() == ITEM_CLASS_SUB_INVENTORY)
				return;
			TYPE_OBJECTID toInventoryItemID = (pMouseItem==NULL)? OBJECTID_NULL : pMouseItem->GetID();
			
			CGAddMouseToInventory _CGAddMouseToInventory;
			_CGAddMouseToInventory.setObjectID( toInventoryItemID );
			_CGAddMouseToInventory.setInvenX( left );//pItem->GetGridX() );
			_CGAddMouseToInventory.setInvenY( right );//pItem->GetGridY() );
			_CGAddMouseToInventory.setInventoryItemObjectID(pMultiPackItem->GetID());

			g_pSocket->sendPacket( &_CGAddMouseToInventory );

			MItem* pOldItem = NULL;
			if (gC_vs_ui.ReplaceSubInventoryItem(pMouseItem,		// item to add
												left, right,	// position to add
												pOldItem))		// item that was there originally
			{
				if (pOldItem != NULL) // was it replaced?
				{				
					UI_PickUpItem( pOldItem );
				}
				else
				{
					UI_DropItem();
				}
			}
		}
		//-----------------------------------------------------------------
		// if there is another item that needs verification
		//-----------------------------------------------------------------
		else
		{
			DEBUG_ADD_ERR( "[Error] There is another item in ItemCheckBuffer or Mouse NULL");

			if(!g_pPlayer->IsItemCheckBufferNULL())
				DEBUG_ADD_FORMAT("[ITEMDROP] !g_pPlayer->IsItemCheckBufferNULL() %d", g_pPlayer->GetItemCheckBufferStatus());
			if(pMouseItem == NULL)
				DEBUG_ADD("[ITEMDROP] pMouseItem == NULL)");
			if(g_pTempInformation->GetMode()!=TempInformation::MODE_NULL)
				DEBUG_ADD_FORMAT("[ITEMPDROP] g_pTempInformation->Mode!=TempInformation::MODE_NULL %d", g_pTempInformation->Mode);
		}
	}


	void
	UIMessageManager::Execute_UI_ITEM_PICKUP_FROM_INVENTORY_SUB(intptr_t left, intptr_t right, void* void_ptr)
	{
		DEBUG_ADD("[UI] UI_ITEM_PICKUP_FROM_INVENTORY"); 
		
		if (g_Mode!=MODE_GAME || g_pPlayer->IsDead() || !gC_vs_ui.IsRunningSubInventory())
		{
			DEBUG_ADD("Not Mode MODE_GAME or Dead");
			return;
		}
		MItem* pItem = (MItem*)void_ptr;

		MItem* pMultiPackItem = gC_vs_ui.GetSubInventoryItem();

		TYPE_OBJECTID MultiPackItemID = (pMultiPackItem==NULL)? OBJECTID_NULL : pMultiPackItem->GetID();

		UI_PickUpItem( pItem );
		//---------------------------------------------------
		// Tell the server that the item that was in the Inventory
		// was added to the Inventory, by sending a packet.
		//---------------------------------------------------
		CGAddInventoryToMouse _CGAddInventoryToMouse;
		_CGAddInventoryToMouse.setObjectID( pItem->GetID() );
		_CGAddInventoryToMouse.setX( pItem->GetGridX() );
		_CGAddInventoryToMouse.setY( pItem->GetGridY() );
		_CGAddInventoryToMouse.setInventoryItemObjectID(MultiPackItemID);
		
		g_pSocket->sendPacket( &_CGAddInventoryToMouse );				
	}

	// 2005, 2, 25, sobeit add end - close the sub inventory.


	// 2005, 3, 2, sobeit add start
	void
	UIMessageManager::Execute_UI_ITEM_USE_SUBINVENTORY(intptr_t left, intptr_t right, void* void_ptr)
	{
		DEBUG_ADD("[UI] Execute_UI_ITEM_USE_SUBINVENTORY"); 
		
		if (g_Mode!=MODE_GAME || g_pPlayer->IsDead() )
		{
			DEBUG_ADD("Not Mode MODE_GAME or Dead");
			return;
		}
		MItem* pSubInventory = g_pInventory->GetItem(right);

		if(NULL == pSubInventory)
			return;

		MItem* pItem = ((MSubInventory*)pSubInventory)->GetItemToModify( left );//(MItem*)void_ptr;

		if (pItem!=NULL && g_pPlayer->IsItemCheckBufferNULL())
		{
			// Let's use only items of our own race -_-;
			if(g_pPlayer->IsSlayer() && pItem->IsSlayerItem() ||
				g_pPlayer->IsVampire() && pItem->IsVampireItem() ||
				g_pPlayer->IsOusters() && pItem->IsOustersItem())
			{
				pItem->UseInventory(pSubInventory->GetID());
			}
		}
		//----------------------------------------------------
		// item check buffer not null
		//----------------------------------------------------
		else
		{
			#ifdef OUTPUT_DEBUG
				if (pItem==NULL)
				{
					DEBUG_ADD("[Wait] Item is NULL");
				}
				else 
				{
					DEBUG_ADD("[Wait] Item Check Buffer is Not NULL");
				}
			#endif
		}
	}

	// 2005, 3, 2, sobeit add end
#endif