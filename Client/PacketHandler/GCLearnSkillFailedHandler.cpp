//////////////////////////////////////////////////////////////////////
//
// Filename    : GCLearnSkillFailed1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCLearnSkillFailed.h"
#include "MGameStringTable.h"
#include "DialogPresentationHost.h"
#include "TempInformation.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCLearnSkillFailedHandler::execute ( GCLearnSkillFailed * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	// pPacket->getSkillType();
	
	// pPacket->getDescription();
	g_pTempInformation->SetMode(TempInformation::MODE_NULL);

	if (pPacket->getDesc()==10)
	{
		DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_SKILL_NOT_SUPPORT].GetString());
	}
	else
	{
		switch (pPacket->getDesc())
		{
			case 0 :
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_SKILL_DIFFER_DOMAIN].GetString());
			break;

			case 1 :
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_SKILL_EXCEED_LEVEL].GetString());
			break;

			default :
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_SKILL_CANNOT_LEARN].GetString());
			
		}
	}
	

	__END_CATCH
}
