//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSelectRankBonusFailed1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSelectRankBonusFailed.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "TempInformation.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSelectRankBonusFailedHandler::execute ( GCSelectRankBonusFailed * pGCSelectRankBonusFailed , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pGCSelectRankBonusFailed;
	(void)pPlayer;

		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_SKILL_NOT_SUPPORT].GetString());
		DialogPresentation::EmptyMove();
		
	__END_CATCH
}
