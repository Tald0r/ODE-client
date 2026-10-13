//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTakeOutFailHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTakeOutFail.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCTakeOutFailHandler::execute ( GCTakeOutFail * pGCTakeOutFail , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pGCTakeOutFail;
	(void)pPlayer;

	DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_TAKE_OUT_FAIL].GetString() );

	__END_CATCH
}
