//////////////////////////////////////////////////////////////////////
//
// Filename    : LCLoginErrorHandler.cpp
// Written By  : Reiot
// Description : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Lpackets/LCLoginError.h"
#include "ClientMode.h"
#include "LoginPresentationHost.h"

extern void		PopupErrorMessage(ErrorID errorID);

extern BOOL		g_bNeedUpdate;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void LCLoginErrorHandler::execute ( LCLoginError * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	//cout << "Access Denied... (" << pPacket->toString() << ")" << endl;
	//throw ProtocolException("Login Failed!");
	// Debug Message

	// A client quitting to be updated ignores it. Otherwise the login is
	// entered again, with the error's message.
	if (!g_bNeedUpdate)
	{
		g_ModeNext = MODE_LOGIN_WRONG;
		LoginPresentation::PopupErrorMessage( pPacket->getErrorID() );
	}

		
	__END_CATCH
}
