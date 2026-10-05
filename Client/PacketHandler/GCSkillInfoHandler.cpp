#include "Client_PCH.h"
#include "Gpackets/GCSkillInfo.h"
#include "ApplySkillInfo.h"
#include "MSkillManager.h"
#include "BonusSkillHost.h"

#include <algorithm>

// A zone's skill snapshot clears sweeper bonuses after rebuilding the skill
// model, then asks the executable to refresh currently usable skills.
void GCSkillInfoHandler::execute(GCSkillInfo* packet, Player* source)
{
	__BEGIN_TRY
	(void)source;
	ApplySkillInfo(packet);
	std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, false);
	BonusSkills::RefreshAvailableSkills();
	__END_CATCH
}
