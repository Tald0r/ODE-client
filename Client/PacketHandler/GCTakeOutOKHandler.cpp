//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTakeOutOKHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTakeOutOK.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCTakeOutOKHandler::execute ( GCTakeOutOK * pGCTakeOutOK , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_TAKE_OUT_OK].GetString() );

	DialogPresentation::RemoveItemFromItemShop( pGCTakeOutOK->getObjectID() );
		
	__END_CATCH
}
