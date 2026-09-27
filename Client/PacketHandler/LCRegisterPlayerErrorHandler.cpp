//--------------------------------------------------------------------------------
//
// Filename    : LCRegisterPlayerErrorHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCRegisterPlayerError.h"
#include "ClientDef.h"
#include "MGameStringTable.h"
#include "VS_UI_Message.h"

extern BOOL		g_bNeedUpdate;

//--------------------------------------------------------------------------------
//
// The login server refused the registration. Tell the user why and return
// to the login window (MODE_LOGIN_WRONG drops the socket and re-enters the
// main menu, exactly as a failed login does).
//
//--------------------------------------------------------------------------------
void LCRegisterPlayerErrorHandler::execute ( LCRegisterPlayerError * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	DEBUG_ADD_FORMAT("[ RegisterPlayerError ] %d", (int)pPacket->getErrorID() );

	if (!g_bNeedUpdate)
	{
		int stringID;

		switch ((ErrorID)pPacket->getErrorID())
		{
			case ALREADY_REGISTER_ID:		stringID = STRING_ERROR_ALREADY_REGISTER_ID;		break;
			case ALREADY_REGISTER_SSN:		stringID = STRING_ERROR_ALREADY_REGISTER_SSN;		break;
			case EMPTY_ID:					stringID = STRING_ERROR_EMPTY_ID;					break;
			case SMALL_ID_LENGTH:			stringID = STRING_ERROR_SMALL_ID_LENGTH;			break;
			case EMPTY_PASSWORD:			stringID = STRING_ERROR_EMPTY_PASSWORD;				break;
			case SMALL_PASSWORD_LENGTH:		stringID = STRING_ERROR_SMALL_PASSWORD_LENGTH;		break;
			case EMPTY_NAME:				stringID = STRING_ERROR_EMPTY_NAME;					break;
			case EMPTY_SSN:					stringID = STRING_ERROR_EMPTY_SSN;					break;
			default:						stringID = STRING_ERROR_ETC_ERROR;					break;
		}

		// VS_UI dialog: UIDialog's popup draws no text on the login screen.
		g_ShowMessage( (*g_pGameStringTable)[stringID].GetString() );
	}

	// Back to the login window.
	g_ModeNext = MODE_LOGIN_WRONG;


	__END_CATCH
}
