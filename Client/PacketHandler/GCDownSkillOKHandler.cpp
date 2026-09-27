//////////////////////////////////////////////////////////////////////
//
// Filename    : GCDownSkillOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCDownSkillOK.h"
#include "ClientDef.h"
#include "UIFunction.h"
#include "MSkillManager.h"
#include "MGameStringTable.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCDownSkillOKHandler::execute ( GCDownSkillOK * pGCDownSkillOK , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;

	SkillType_t skillID = pGCDownSkillOK->getSkillType();	
	
	int curLevel = (*g_pSkillInfoTable)[skillID].GetExpLevel();
	curLevel --;

	if (auto* entry = g_pSkillInfoTable->GetMutable(skillID)) {
		entry->SetExpLevel( curLevel );
	}
	// 2004, 11, 9, sobeit add start - 레벨이 0까지 다운되면 다시 배울수 있다고 세팅해야함 
	if(0 == curLevel)
	{
		if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_OUSTERS)) {
			entry->AddNextSkillForce((ACTIONINFO)skillID);
		}
	}
	// 2004, 11, 9, sobeit add end
	UI_PopupMessage( STRING_MESSAGE_SUCCESS_CHANGE );

	__END_CATCH
}
