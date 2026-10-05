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
#include "SkillDowngradeHost.h"
#include "MSkillManager.h"
#include "MGameStringTable.h"

namespace SkillDowngrade {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const Host* previous = s_Host;
	s_Host = host;
	return previous;
}

void PopupMessage(int gameStringID)
{
	if (s_Host && s_Host->PopupMessage) s_Host->PopupMessage(gameStringID);
}
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCDownSkillOKHandler::execute ( GCDownSkillOK * pGCDownSkillOK , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	const SkillType_t skillID = pGCDownSkillOK->getSkillType();
	if (!g_pSkillInfoTable) return;
	auto* skill = g_pSkillInfoTable->GetMutable(skillID);
	// A reply cannot downgrade an absent or already unlearned skill. Check
	// before subtracting so a corrupt negative level cannot overflow either.
	if (!skill || skill->GetExpLevel() <= 0) return;
	const int curLevel = skill->GetExpLevel() - 1;
	skill->SetExpLevel(curLevel);
	// A skill downgraded to zero can be learned again.
	if (curLevel == 0 && g_pSkillManager)
	{
		if (auto* entry = g_pSkillManager->GetMutable(SKILLDOMAIN_OUSTERS)) {
			entry->AddNextSkillForce(static_cast<ACTIONINFO>(skillID));
		}
	}

	SkillDowngrade::PopupMessage( STRING_MESSAGE_SUCCESS_CHANGE );

	__END_CATCH
}
