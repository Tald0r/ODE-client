//--------------------------------------------------------------------------------
//
// Filename    : GCNicknameVerifyHandler.cpp
// Written By  : elca, Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCNicknameVerify.h"

#include "CommunicationPresentationHost.h"
#include "MGameStringTable.h"
#include "TempInformation.h"
#include "VsUiPresentationHost.h"


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCNicknameVerifyHandler::execute ( GCNicknameVerify * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
//	__BEGIN_DEBUG_EX
//	__BEGIN_DEBUG
		
	switch(pPacket->getCode())
	{
		case NICKNAME_MODIFY_FAIL_NO_ITEM:
			CommunicationPresentation::PopupMessage( STRING_MESSAGE_CANNOT_BUY_NO_ITEM );
			break;
		case NICKNAME_MODIFY_OK:
			if (g_pTempInformation->GetMode()==TempInformation::MODE_NICKNAME_CHANGE_CUSTOM)
			{
				// StrValue1 = the name, Value2 = the item type used
				switch(g_pTempInformation->Value2)
				{
				case 22: // a nickname change
				case 25: // a nickname change (timed)
					VsUiPresentation::ChangeCustomNaming(0, g_pTempInformation->StrValue1.c_str());
					break;
				// 23, a pet's nickname, and 0, a pet's name, change through
				// GCModifyNickname; 24, a nickname added, through the list the
				// server sends.
				}
				VsUiPresentation::CloseNamingChange();
				CommunicationPresentation::PopupMessage( UI_STRING_MESSAGE_NICKNAME_CHANGE_OK );
			}
			break;
		case NICKNAME_SELECT_FAIL_NO_SUCH:
			CommunicationPresentation::PopupMessage( UI_STRING_MESSAGE_SMS_FAIL_NO_SUCH_EID );
			break;
		case NICKNAME_SELECT_OK:
			CommunicationPresentation::PopupMessage( UI_STRING_MESSAGE_NICKNAME_CHANGE_OK );
			break;
		case NICKNAME_SELECT_FAIL_FORCED:
			CommunicationPresentation::PopupMessage( UI_STRING_MESSAGE_NICKNAME_SELECT_FAIL_FORCED);
			break;
	}

	g_pTempInformation->SetMode(TempInformation::MODE_NULL);

//	__END_DEBUG
//	__END_DEBUG_EX 
	__END_CATCH
}
