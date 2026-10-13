//----------------------------------------------------------------------
//
// Filename    : LCCreatePCErrorHandler.cpp
// Written By  : Reiot
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCCreatePCError.h"
#include "LoginPresentationHost.h"


//----------------------------------------------------------------------
// 
//----------------------------------------------------------------------
void LCCreatePCErrorHandler::execute ( LCCreatePCError * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	//cout << "Fail to create PC... (" << pPacket->toString() << ")" << endl;
	//throw ProtocolException("CreatePC Failed!");

	//UI_NewCharacterCreateFailed( 0 );

	LoginPresentation::PopupErrorMessage( (ErrorID)pPacket->getErrorID() );


	LoginPresentation::QueueWaitSelectPCMode();

		
	__END_CATCH
}
