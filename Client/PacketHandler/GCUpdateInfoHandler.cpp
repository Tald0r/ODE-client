//----------------------------------------------------------------------
//
// Filename    : GCUpdateInfoHandler.cpp
// Written By  : Reiot
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUpdateInfo.h"
#include "BonusSkillHost.h"
#include "CMessageArray.h"
#include "COrderedList.h"
#include "CServerInformation.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "DebugLog.h"
#include "FrameClock.h"
#include "GameInfoPresentationHost.h"
#include "GameMessages.h"
#include "MCreatureTable.h"
#include "MEffectStatusDef.h"
#include "MGameStringTable.h"
#include "MGameTime.h"
#include "MGuildInfoMapper.h"
#include "MItem.h"
#include "MQuickSlot.h"
#include "MSkillManager.h"
#include "MStorage.h"
#include "MWeather.h"
#include "MZoneTable.h"
#include "MonotonicClock.h"
#include "MorphState.h"
#include "PCOustersInfo2.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PlayerStateHost.h"
#include "TalkBox.h"
#include "UserInformation.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneLoadingHost.h"
#include "ZoneStateHost.h"


DWORD	g_PreviousCreatureType = -1;
// g_MorphCreatureType is MorphState.cpp's.

//----------------------------------------------------------------------
// When the client receives GCUpdateInfo from the game server, it stores
// the packet's data in the client; once the data is loaded, it sends
// CGReady to the game server.
//----------------------------------------------------------------------
void GCUpdateInfoHandler::execute ( GCUpdateInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY


	#ifndef __DEBUG_OUTPUT__ 
		DEBUG_ADD("Execute UpdateInfo");
	#endif

	// Drop the event manager's zone-bound events.
	ZoneLoading::RemoveZoneEvents();

	//-----------------------------------------------------------
	// Other parts read the mode, so set it.
	//-----------------------------------------------------------
	CLIENT_MODE	previousMode = g_Mode;
	g_Mode = MODE_WAIT_UPDATEINFO;

	//-----------------------------------------------------------
	// Show the current server's status.
	//-----------------------------------------------------------
	if (g_pServerInformation!=NULL)
	{
		g_pServerInformation->SetServerGroupStatus( pPacket->getServerStat() );
	}

	//-----------------------------------------------------------
	// AddonSPK loads in parts..
	// here every part not loaded yet must load. 2001.8.20
	//-----------------------------------------------------------
	ZoneLoading::LoadAllAddonSprites();

	int i;

	//-----------------------------------------------------------
	// Play no sound.
	//-----------------------------------------------------------
	WorldEvents::MuteSound( true );

	//-----------------------------------------------------------
	// The time the update info's handling starts.
	//-----------------------------------------------------------
	const MonotonicClock::TimePoint startTime = MonotonicClock::Now();
	
	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	//--------------------------------------------------
	// Start loading the data.
	//--------------------------------------------------
	pClientPlayer->setPlayerStatus( CPS_WAITING_FOR_LOADING );

	
	//--------------------------------------------------
	//
	//			 Set the player's details
	//
	//--------------------------------------------------
	Race wasInterfaceRace = ZoneLoading::InterfaceRace();
	int spActionInfo = ACTIONINFO_NULL;
	g_PreviousCreatureType = -1;

	if (PlayerState::Creature()!=NULL)
	{	
		if (g_MorphCreatureType!=0)
		{
			g_PreviousCreatureType = g_MorphCreatureType;
			g_MorphCreatureType = 0;
		}
		else
		{
			g_PreviousCreatureType = CreatureState::GetCreatureType( PlayerState::Creature() );
		}

		//--------------------------------------------------
		// Keep the skill in use before.
		//--------------------------------------------------
		spActionInfo = PlayerState::GetSpecialActionInfo();
		ZoneState::RemoveCreature( PlayerState::GetID() );
	}	
	// Delete the player and make a new one.
	PlayerState::Recreate();

	//--------------------------------------------------
	//
	//		Reset the gear
	//
	//--------------------------------------------------
	DEBUG_ADD( "Remove All Addon" );
	
	for (i=0; i<ADDON_MAX; i++)
	{
		CreatureState::RemoveAddon( PlayerState::Creature(), i );
	}			

	PlayerState::SetBasicAttack( SKILL_ATTACK_MELEE, 1 );
		


	//--------------------------------------------------
	//
	//	Reset the skill info table
	//
	//--------------------------------------------------
	DEBUG_ADD( "SkillInfoTable Init" );
	g_pSkillInfoTable->Init();

	//--------------------------------------------------
	//
	// Reset the skill tree
	//
	//--------------------------------------------------
	DEBUG_ADD( "SkillManager Init" );
	ZoneLoading::InitSkillTree();

	g_PreviousCreatureType = -1;
	g_MorphCreatureType = 0;

	EffectInfo *pEInfo = new EffectInfo;	
	*pEInfo = *pPacket->getEffectInfo();
	
	if( pEInfo != NULL )
	{
		for( int i=0;i<pEInfo->getListNum();i++)
		{
			EFFECTSTATUS	status = (EFFECTSTATUS) pEInfo->popFrontListElement();
			pEInfo->popFrontListElement();
			
			if( status == EFFECTSTATUS_TRANSFORM_TO_BAT )
			{
				g_PreviousCreatureType = CREATURETYPE_BAT;
			} else
			if( status == EFFECTSTATUS_TRANSFORM_TO_WOLF )
			{
				g_PreviousCreatureType = CREATURETYPE_WOLF;
			} else
			if( status == EFFECTSTATUS_TRANSFORM_TO_WERWOLF )
			{
				g_PreviousCreatureType = CREATURETYPE_WER_WOLF;
			}
		}
		delete pEInfo;
	}

	g_pGuildInfoMapper->ClearGuildName();

	//--------------------------------------------------
	//
	//			 Store the player's details.
	//
	//--------------------------------------------------		
	switch (pPacket->getPCInfo()->getPCType())
	{
		//--------------------------------------------------
		// A slayer
		//--------------------------------------------------
		case PC_SLAYER :
		{
			PlayerState::SetPCSlayerInfo( (PCSlayerInfo2*)pPacket->getPCInfo() );

			if (wasInterfaceRace != RACE_SLAYER)
			{
				ZoneLoading::ChangeInterfaceRace(RACE_SLAYER);
			}
		}
		break;

		//--------------------------------------------------
		// A vampire
		//--------------------------------------------------
		case PC_VAMPIRE :
		{
			PlayerState::SetPCVampireInfo( (PCVampireInfo2*)pPacket->getPCInfo() );

			if (wasInterfaceRace != RACE_VAMPIRE)
			{
				ZoneLoading::ChangeInterfaceRace(RACE_VAMPIRE);
			}
		}
		break;

		//--------------------------------------------------
		// An ouster
		//--------------------------------------------------
		case PC_OUSTERS :
		{
			PlayerState::SetPCOustersInfo( (PCOustersInfo2*)pPacket->getPCInfo() );
			
			if (wasInterfaceRace != RACE_OUSTERS)
			{
				ZoneLoading::ChangeInterfaceRace(RACE_OUSTERS);
			}
		}
		break;
	}	
	// 2004, 6, 4 sobeit add start - sms charge
	ZoneLoading::SetSMSCharge( pPacket->GetSMSCharge() );
	// 2004, 6, 4 sobeit add end - sms charge
	
	// 2004, 6, 15 sobeit add end - nick name
	NicknameInfo* TempNick	= pPacket->getNicknameInfo();
	const char* szNickName;
	// getNickname() returns std::string by value, so binding its c_str()
	// would dangle at the end of that statement; the copy has to outlive the
	// SetNickName call below.
	std::string strCustomNick;
	if(TempNick != NULL)
	{
		if(TempNick->getNicknameType() == NicknameInfo::NICK_NONE)
		{
			szNickName = "";
		}
		else
		if(TempNick->getNicknameType() == NicknameInfo::NICK_CUSTOM_FORCED ||
			TempNick->getNicknameType() == NicknameInfo::NICK_CUSTOM)
		{
			strCustomNick = TempNick->getNickname();
			szNickName = strCustomNick.c_str();
		}
		else // when a nickname index is supplied
		{
			// The index is a raw WORD off the wire. Clamping matters more
			// than it looks: out of range, CTypeTable now returns a default
			// MString whose GetString() is NULL, and SetNickName assigns
			// that straight into a std::string. The siblings in
			// GCModifyNicknameHandler and the three GCAdd*Handlers clamp the
			// same way.
			DWORD TempIndex = TempNick->getNicknameIndex();
			if(TempIndex >= static_cast<DWORD>(g_pNickNameStringTable->GetSize()))
				TempIndex = 0;
			szNickName = (*g_pNickNameStringTable)[TempIndex].GetString();
		}

		if(szNickName == NULL)
			szNickName = "";

		CreatureState::SetNickName( PlayerState::Creature(), TempNick->getNicknameType(), (char*)szNickName );
	}
	// 2004, 6, 15 sobeit add end - nick name

	// 2004, 9, 15, sobeit add start - non pk
	g_pUserInformation->IsNonPK = (pPacket->isNonPK())?true:false;
	// 2004, 9, 15, sobeit add end - non pk
	
	// 2004, 10, 18, sobeit add start - the union guild
	g_pUserInformation->dwUnionID = pPacket->getGuildUnionID();
	g_pUserInformation->bUnionGrade = pPacket->getGuildUnionUserType();
	CreatureState::SetUnionGuildID( PlayerState::Creature(), g_pUserInformation->dwUnionID );
	// 2004, 10, 18, sobeit add end
	
	// 2004, 12, 2, sobeit add start - Powerjjang points
	ZoneLoading::SetPowerjjangPoint( pPacket->GetPowerjjang_Point() );
	// 2004, 12, 2, sobeit add end

	//--------------------------------------------------
	// Set the actionInfo chosen before
	//--------------------------------------------------
	PlayerState::SetSpecialActionInfo( spActionInfo );

	//--------------------------------------------------
	// Set the attack mode
	//--------------------------------------------------
	PlayerState::SetAttackModeNormal();



	//-----------------------------------------------------------
	// Drop the stash.
	//-----------------------------------------------------------
	if (g_pStorage!=NULL)
	{
		delete g_pStorage;
		g_pStorage = NULL;		
	}

	//-----------------------------------------------------------
	// Tidy up.
	//-----------------------------------------------------------
	ZoneLoading::CloseWindows();

	if (g_pPCTalkBox!=NULL)
	{
		g_pPCTalkBox->Release();
	}
		

	//--------------------------------------------------
	// Drop the quick slot. Needed? - -;
	//--------------------------------------------------	
	g_pQuickSlot = NULL;
	g_pArmsBand1 = NULL;
	g_pArmsBand2 = NULL;

				
	ZoneLoading::StartProgress( pPacket->getZoneID() );

	ZoneLoading::DrawProgress(0);

	DEBUG_ADD("Init the top view");
	
	//------------------------------
	// Initialise it unless it is.
	//------------------------------
	ZoneLoading::InitTopView();

	ZoneLoading::DrawProgress( 15 );
	
	//--------------------------------------------------
	//
	// Set the zone's surroundings.
	//
	//--------------------------------------------------
	DEBUG_ADD("[UpdateInfo] Set Weather");
	
	//--------------------------------------------------
	// When moving zones changes the weather..
	// clear the weather entirely.
	//--------------------------------------------------
	if ((g_pWeather->GetWeatherType()==MWeather::WEATHER_SNOW
		&& pPacket->getWeather()!=WEATHER_SNOWY)
		
		|| (g_pWeather->GetWeatherType()==MWeather::WEATHER_RAIN
		&& pPacket->getWeather()!=WEATHER_RAINY)	)
	{
		g_pWeather->Release();
	}
	
	WorldEvents::SetWeather(pPacket->getWeather(), pPacket->getWeatherLevel());

	DEBUG_ADD_FORMAT( "[UpdateInfo] Set Dark/Light : %d / %d", pPacket->getDarkLevel(), pPacket->getLightLevel());
	
	int darkLevel = pPacket->getDarkLevel();
	int lightSight = pPacket->getLightLevel();

	if (g_pUserInformation->IsMaster)
	{
		darkLevel = 0;
		lightSight = 13;
	}

	// the screen's darkness
	WorldEvents::SetDarkBits( darkLevel );
	
	// the light around the player
	PlayerState::SetTimeLightSight( lightSight );

	
	//--------------------------------------------------
	//
	//		The game's time
	//
	//--------------------------------------------------
	DEBUG_ADD( "Set Game Time" );
	
	GameTime gt = pPacket->getGameTime();
	
	// the starting time
	g_pGameTime->SetStartTime(g_FrameNow,
							gt.getYear(),
							gt.getMonth(),
							gt.getDay(),
							gt.getHour(),
							gt.getMinute(),
							gt.getSecond()
							);
  
	// the time ratio
	g_pGameTime->SetTimeRatio( 24 );


	//--------------------------------------------------
	//
	//	 Store the gear's details.
	//
	//--------------------------------------------------
	// 2004, 10 ,21, sobeit Modify start - the Blood Bible's gear items
	PlayerState::SetGearInfo( pPacket->getGearInfo(), pPacket->getBloodBibleSignInfo() );
	// 2004, 10 ,21, sobeit Modify end - the Blood Bible's gear items
	//--------------------------------------------------
	// Calculate the status..
	//--------------------------------------------------
	PlayerState::CalculateStatus();


	//--------------------------------------------------
	//
	//			Reset the inventory
	//
	//--------------------------------------------------
	PlayerState::SetInventoryInfo( pPacket->getInventoryInfo() );


	//--------------------------------------------------
	//
	// Other item details == the item held at the mouse
	//
	//--------------------------------------------------
	PlayerState::SetExtraInfo( pPacket->getExtraInfo() );


	//--------------------------------------------------
	//
	// The motorcycle
	//
	//--------------------------------------------------
	if (pPacket->hasMotorcycle())
	{
		//--------------------------------------------
		// make the motorcycle
		//--------------------------------------------
		MItem*	pMotor = new MMotorcycle;	
		pMotor->SetID( 0 );
		pMotor->SetItemType( 0 ); 
						
		//--------------------------------------------
		// the items in the motorcycle..
		//--------------------------------------------
		RideMotorcycleInfo* pMotorInfo = pPacket->getRideMotorcycleInfo();

		if (pMotorInfo!=NULL)
		{
			pMotor->SetID( pMotorInfo->getObjectID() );
			pMotor->SetItemType( pMotorInfo->getItemType() );
			pMotor->SetItemOptionList( pMotorInfo->getOptionType() );

			int num = pMotorInfo->getListNum();

			for (i=0; i<num; i++)
			{
				RideMotorcycleSlotInfo* pMotorSlotInfo = pMotorInfo->popFrontListElement();

				if (pMotorSlotInfo!=NULL)
				{
					// read the slot and add it to pMotor.
					// Motor is an MGridItemManager.

					delete pMotorSlotInfo;
				}
			}
		}

		//--------------------------------------------
		// put the player on the motorcycle.
		//--------------------------------------------
		PlayerState::SetAddonItem( pMotor );
		
		// -_-;;		
		// later the player should own the motorcycle. -_-;;
		// deleting it drops its item details and all T_T;
		delete pMotor;	
	}

	ZoneLoading::DrawProgress(17);

	//--------------------------------------------------
	// Check again which skills can be used now.
	//--------------------------------------------------
	BonusSkills::RefreshAvailableSkills();

	int numNPC = pPacket->getNPCCount();
	int numMonster = pPacket->getMonsterCount();
	
	int total = numNPC+numMonster;

	if (total==0) total = 1;

	int progress = 40;
	int progressInc = 30 / total;

	COrderedList<int>	listUse;

	//--------------------------------------------------
	// NPC Load
	//--------------------------------------------------
	for (i=0; i<numNPC; i++)
	{
		int spriteType = pPacket->getNPCType( i );

		// the SpriteType of a creature this zone uses
		listUse.Add( spriteType );

		progress += progressInc;
		ZoneLoading::DrawProgress(progress);
	}

	//--------------------------------------------------
	// Monster Load
	//--------------------------------------------------
	for (i=0; i<numMonster; i++)
	{	
		int spriteType = pPacket->getMonsterType( i );

		if (ZoneLoading::PreloadsMonsters())
			ZoneLoading::LoadCreature( spriteType );

		// the SpriteType of a creature this zone uses
		listUse.Add( spriteType );

		progress += progressInc;
		ZoneLoading::DrawProgress(progress);
	}

	//--------------------------------------------------
	// Release the creatures this zone does not use.
	//	
	// Only when moving from a large zone to a large zone!
	//--------------------------------------------------
	if (g_bZonePlayerInLarge && g_nZoneLarge!=ZONEID_NULL && ZoneLoading::HasLargeZone())
	{
		ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( pPacket->getZoneID() );

		if (pZoneInfo->Property & FLAG_ZONESIZE_SMALL)
		{
			ZoneLoading::AddMonsterSpriteTypes( listUse );
		}
		//--------------------------------------------------
		// moving to a large zone
		//--------------------------------------------------	
		else
		{
			ZoneLoading::ReleaseCreatureSpritesExcept( listUse );
		}
	}	
	else
	{
		ZoneLoading::AddMonsterSpriteTypes( listUse );
	}

	if (progress<70)
	{
		ZoneLoading::DrawProgress( 70 );
	}

	//--------------------------------------------------
	// Zone Loading...			
	//--------------------------------------------------
	// From the game.. move zones.
	DEBUG_ADD( "Load Zone" );
	
	//--------------------------------------------------
	// the zone position, for now
	//--------------------------------------------------
	PlayerState::SetXY( pPacket->getZoneX(), pPacket->getZoneY() );

	if (previousMode == MODE_GAME)
	{
		ZoneLoading::MoveZone( pPacket->getZoneID() );
	}
	// Otherwise.. just load the zone.
	else
	{
		ZoneLoading::LoadZone( pPacket->getZoneID() );
	}
	
	// Packet Encrypt
	ZoneLoading::SetEncryptCode();

	//-----------------------------------------------------------
	//
	// The NPCs' positions in the zone
	//
	//-----------------------------------------------------------
	while (1)
	{
		NPCInfo* pInfo = pPacket->popNPCInfo();

		if (pInfo==NULL)
		{
			break;
		}

		// NPCInfo::getName() returns std::string by value; bind it so pName is
		// not into a temporary that died on this line. The zone check below may
		// still repoint pName at the creature table.
		const std::string npcName = pInfo->getName();
		const char* pName = npcName.c_str();
		int npcID = pInfo->getNPCID();
		int x = pInfo->getX();
		int y = pInfo->getY();

		if(ZoneState::Exists())
		{
			int zoneID	= ZoneState::GetID();
			if(zoneID< 1500 || zoneID >1506)
				pName = (*g_pCreatureTable)[npcID].Name;
		}else
			pName = (*g_pCreatureTable)[npcID].Name;

		GameInfoPresentation::SetNPCInfo( pName, npcID, x, y );

		delete pInfo;		
	}

	//--------------------------------------------------
	//
	//	Set the effect statuses on the player now.
	//
	//--------------------------------------------------
	// [!] Must come after the zone loads.
	//--------------------------------------------------
	
	//--------------------------------------------------
	// Work out the current time and frame again.
	//--------------------------------------------------
	ZoneLoading::StampFrameClock();
	
	// the time handling the update info took
	// ...as frames
	int delayedFrame = ZoneLoading::MillisecondsToFrames( (DWORD)(MonotonicClock::Now() - startTime).count() );

	// advance the game's frame
	ZoneLoading::AdvanceFrame( delayedFrame );
	ZoneLoading::SetMyBatColorSet( 0xFFFF );

	switch (pPacket->getPCInfo()->getPCType())
	{
		//--------------------------------------------------
		// A vampire
		//--------------------------------------------------
		case PC_VAMPIRE :
		{
			PCVampireInfo2 *pInfo = dynamic_cast< PCVampireInfo2* >( pPacket->getPCInfo() );
			if( pInfo->getBatColor() != 0 )
				ZoneLoading::SetMyBatColorSet( pInfo->getBatColor() );
		}
		break;
		default:
			break;
	}



	WorldEvents::SetEffectInfo( PlayerState::Creature(), pPacket->getEffectInfo(), delayedFrame );

	
	ZoneLoading::DrawProgress(100);
	ZoneLoading::DrawProgress(100);

	ZoneLoading::EndProgress();


	//-----------------------------------------------------------
	// Let sound play again.
	//-----------------------------------------------------------
	WorldEvents::MuteSound( false );

	// shown once more for the blood drain check -.-
	ZoneLoading::ShowPlayerHP();
	
	//--------------------------------------------------
	// Clear the messages
	//--------------------------------------------------
	g_pSystemMessage->Clear();
	g_pGameMessage->Clear();
	g_pNoticeMessage->Clear();

	//--------------------------------------------------
	// Wait for the player's position.
	//--------------------------------------------------
	ZoneLoading::EnterWaitSetPositionMode();


	__END_CATCH
}
