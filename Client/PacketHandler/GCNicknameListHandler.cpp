//////////////////////////////////////////////////////////////////////
//
// Filename    : GCNicknameListHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCNicknameList.h"
#include "VS_UI.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCNicknameListHandler::execute ( GCNicknameList * pGCNicknameList , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
//		__BEGIN_DEBUG_EX
		

	gC_vs_ui.SetNickNameList((void*)&pGCNicknameList->getAddresses());
//	std::vector<NICKNAMEINFO*> g_NickNameList;
	//cout << pGCNicknameList->toString() << endl;
	


//	__END_DEBUG_EX 
	__END_CATCH
}
