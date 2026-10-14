//----------------------------------------------------------------------
//
// Filename    : LCLoginOKHandler.cpp
// Written By  : Reiot
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCLoginOK.h"


	#include "ClientPlayer.h"
	//#include "Cpackets/CLGetPCList.h"
	#include "Cpackets/CLGetWorldList.h"
	#include "UserInformation.h"
	#include "MGameStringTable.h"

#include "ClientMode.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "LoginPresentationHost.h"
//#include "ServerInformation.h"

//----------------------------------------------------------------------
//
// The login server has accepted the ID and password.
//
// Now ask the login server for the world list (it once asked for the PC list).
//
//----------------------------------------------------------------------
void LCLoginOKHandler::execute ( LCLoginOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY


	if( g_pUserInformation->UserID.GetLength() >= 15 )
	{
		LoginPresentation::BackupLoginID( "DarkEden" );
	} 
	else 
		LoginPresentation::BackupLoginID( g_pUserInformation->UserID );

	if (!g_bNeedUpdate)
	{
		// 2004, 07, 20 sobeit add start
		// The blocked-out code is the earlier one
//			ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);
//
//			CLGetWorldList clGetWorldList;
//
//			pClientPlayer->sendPacket( &clGetWorldList );
//
//			SetMode( MODE_WAIT_WORLD_LIST );

		g_pUserInformation->pLogInClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);
		// 2006.11.07 edit Coffee Start
			CLGetWorldList clGetWorldList;

			ClientPlayer * pClientPlayer = (ClientPlayer *)(g_pUserInformation->pLogInClientPlayer);
			pClientPlayer->sendPacket( &clGetWorldList );

			LoginPresentation::EnterWaitWorldListMode();
/*
		if(false == g_pUserInformation->IsAutoLogIn) 
		{
			CLGetWorldList clGetWorldList;

			ClientPlayer * pClientPlayer = (ClientPlayer *)(g_pUserInformation->pLogInClientPlayer);
			pClientPlayer->sendPacket( &clGetWorldList );

			SetMode( MODE_WAIT_WORLD_LIST );
		}	

  */
		// 2006.11.07  Coffee edit end
		//else
		//	SetMode( MODE_MAINMENU );
		// 2004, 07, 20 sobeit add end


		/*
		// The old one
		ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

		CLGetPCList clGetPCList;

		pClientPlayer->sendPacket( &clGetPCList );
		
		// Change the player's status.
		pClientPlayer->setPlayerStatus( CPS_AFTER_SENDING_CL_GET_PC_LIST );

		//------------------------------------------------------------
		// Keep the current server's information.
		//------------------------------------------------------------
		SetServerGroupName( pPacket->getGroupName().c_str() );
		SetServerGroupStatus( (int)pPacket->getStat() );
		//SetServerName( pPacket->getServerName().c_str() );

		//------------------------------------------------------------
		// The client must wait for the PC list.
		//------------------------------------------------------------	
		SetMode( MODE_WAIT_PCLIST );
		*/

		// For Netmarble
		//------------------------------------------------------------
		// Change the gore level.
		//------------------------------------------------------------
		bool bGoreLevel;
		
//		if(g_pUserInformation->IsNetmarble)
//			bGoreLevel = g_pUserInformation->bNetmarbleGoreLevel && !g_pUserOption->UseTeenVersion;
//		else
//		By tiancaiamao:
//		bGoreLevel = pPacket->isAdult() && !g_pUserOption->UseTeenVersion;
		bGoreLevel = !LoginPresentation::UseTeenVersion();

		g_pUserInformation->GoreLevel = bGoreLevel;

		LoginPresentation::SetGoreLevel( bGoreLevel );			
	}


//#if !defined(OUTPUT_DEBUG) && !defined(_DEBUG) && !defined(__DEBUG_OUTPUT__)
//	if( g_nProtectInstance == NULL )
//	{
//		SendMessage(g_hWnd, WM_NPROTECT_FORCEEXIT, 0, 0 );
//		return;
//	}
//	
//	char szBuffer[256] = {0,};
//	if( GetSystemDirectory( szBuffer, 256) == 0)
//	{
//		SendMessage( g_hWnd, WM_NPROTECT_FORCEEXIT, 0, 0 );
//		return;
//
//	}
//		
//	strcat( szBuffer, "\\NPMON.EXE");	
//	
//	bool	Result;
//	Result  = ChecknProtectCRC(g_nProtectInstance, "FindHack.exe");
//	Result |= ChecknProtectCRC(g_nProtectInstance, szBuffer );
//	
//	if(!Result)
//		SendMessage(g_hWnd, WM_NPROTECT_FORCEEXIT, 0, 0 );
//#endif

	g_bFamily = pPacket->isFamily();

	if( g_pUserInformation != NULL && !g_pUserInformation->IsNetmarble )
	{
		// 2005, 1, 24, sobeit add start
		if(0xFFFD == pPacket->getLastDays())
		{
			g_LeftPremiumDays = 7; // on an account's first login in the dormant-account item event fffd comes: a free 7-day premium starts
			DialogPresentation::PopupFreeMessageDlg( GetGameString(UI_STRING_MESSAGE_GET_EVENT_ITEM_NOTICE) );
		}
		else
		// 2005, 1, 24, sobeit add end
			g_LeftPremiumDays = pPacket->getLastDays();
	}
	else
		g_LeftPremiumDays = 0xFFFF;
	
	DEBUG_ADD_FORMAT("[LCLoginOK] g_LeftPremiumDays : %x pPacket->getLastDays() : %x g_bFamily : %s", g_LeftPremiumDays, pPacket->getLastDays(), g_bFamily?"true":"false");

#if !defined(__DEBUG_OUTPUT__) && !defined(OUTPUT_DEBUG)
	// REMOVED: nProtect SendUserIDToGameMon call (SDL migration - no longer needed)
#endif
	__END_CATCH
}
