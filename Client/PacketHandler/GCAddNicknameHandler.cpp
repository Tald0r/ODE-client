//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddNicknameHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddNickname.h"
#include "VsUiPresentationHost.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddNicknameHandler::execute ( GCAddNickname * pGCAddNickname , Player * pPlayer )

{
	__BEGIN_TRY/* __BEGIN_DEBUG_EX*/
	(void)pPlayer;
		

		VsUiPresentation::AddNickNameList(&pGCAddNickname->getNicknameInfo());
	//cout << pGCAddNickname->toString() << endl;
	
/*#elif __WINDOWS__*/


	/*__END_DEBUG_EX */__END_CATCH
}
