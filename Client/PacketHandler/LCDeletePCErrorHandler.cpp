//----------------------------------------------------------------------
//
// Filename    : LCDeletePCErrorHandler.cpp
// Written By  : Reiot
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCDeletePCError.h"
#include "LoginPresentationHost.h"

//----------------------------------------------------------------------
// 캐릭터의 삭제가 실패했다는 뜻이다.
// 가만히 생각해보니, 삭제 실패는 크래킹 시도가 아닌가?? - -;
//----------------------------------------------------------------------
void LCDeletePCErrorHandler::execute ( LCDeletePCError * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	//#ifdef __DEBUG_OUTPUT__
	//cout << "Fail to delete PC... (" << pPacket->toString() << ")" << endl;
	//throw ProtocolException("DeletePC Failed!");
	
	//UI_DeleteCharacterFailed();

	LoginPresentation::PopupErrorMessage( pPacket->getErrorID() );

	LoginPresentation::QueueWaitSelectPCMode();

		
	__END_CATCH
}
