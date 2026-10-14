//////////////////////////////////////////////////////////////////////
//
// Filename    : GCStashSellHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCStashSell.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "PlayerStateHost.h"
#include "MGameStringTable.h"
#include "TempInformation.h"


void GCStashSellHandler::execute ( GCStashSell * pPacket , Player * pPlayer )
	 


{
	__BEGIN_TRY
	(void)pPlayer;
	__BEGIN_DEBUG

	// The player; without one there is nothing to do.
	if (PlayerState::Creature() == NULL)
	{
		return;
	}
	


	if ( PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NPC_ASK )
	{
		PlayerState::SetWaitVerifyNULL();
	
		DEBUG_ADD("[Verified] NPC Ask Answer OK");
	
		//------------------------------------------------------
		// A price of 0 means no more storage can be bought.
		//------------------------------------------------------
		if (pPacket->getPrice()==0)
		{
			// Let the player choose something else..
			DialogPresentation::ShowPCTalkDlg();

			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_MORE].GetString());
		}
		//------------------------------------------------------
		// Buy the storage at the price, or not?
		//------------------------------------------------------
		else
		{
			//------------------------------------------------------
			// Buy the storage at the price, or not?
			//------------------------------------------------------
			DialogPresentation::RunStorageBuy( pPacket->getPrice() );

			//------------------------------------------------------
			// Keep the price.
			//------------------------------------------------------
			g_pTempInformation->SetMode(TempInformation::MODE_STORAGE_BUY);
			g_pTempInformation->Value1	= pPacket->getPrice();
		}
	}


	__END_DEBUG
	__END_CATCH
}
