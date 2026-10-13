//--------------------------------------------------------------------------------
//
// Filename    : LCQueryResultCharacterNameHandler.cpp
// Written By  : Reiot
// Description : 
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCQueryResultCharacterName.h"
#include "UserInformation.h"
#include "MGameStringTable.h"
#include "DialogPresentationHost.h"
#include "LoginPresentationHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void LCQueryResultCharacterNameHandler::execute ( LCQueryResultCharacterName * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// 캐릭 생성중이고
	// 캐릭터 아이디 맞는 경우..
	if (LoginPresentation::IsSelectPCMode()
		&& g_pUserInformation->WhisperID==pPacket->getCharacterName().c_str())
	{
		if (pPacket->isExist())
		{
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_ERROR_ALREADY_REGISTER_ID].GetString() );
		}
		else
		{
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CAN_REGISTER_NAME].GetString() );
		}

		g_pUserInformation->WhisperID.Release();
	}
	
		
	__END_CATCH
}
