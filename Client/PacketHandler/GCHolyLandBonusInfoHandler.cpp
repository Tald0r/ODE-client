// Apply the complete ordered list of Blood Bible owners to the skill model.
#include "Client_PCH.h"
#include "Gpackets/GCHolyLandBonusInfo.h"
#include "BonusSkillHost.h"
#include "MSkillManager.h"

#include <algorithm>
#include <array>
#include <memory>

void GCHolyLandBonusInfoHandler::execute(GCHolyLandBonusInfo* packet, Player* source)
{
	__BEGIN_TRY
	(void)source;
	BonusSkills::PlayerState player;
	if (!BonusSkills::ReadPlayer(player)) return;

	std::array<bool, HOLYLAND_BONUS_MAX> next{};
	size_t index = 0;
	while (auto* row = packet->popFrontBloodBibleBonusInfoList())
	{
		std::unique_ptr<BloodBibleBonusInfo> owned(row);
		// Live framing caps this list at twelve. Keep direct callers bounded too.
		if (index < next.size()) next[index] = player.race == row->getRace();
		++index;
	}
	std::copy(next.begin(), next.end(), g_abHolyLandBonusSkills);
	BonusSkills::RefreshAvailableSkills();
	__END_CATCH
}
