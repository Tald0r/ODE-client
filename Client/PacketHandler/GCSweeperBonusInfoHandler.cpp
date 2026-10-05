// Apply the complete ordered list of level-war owners to the skill model.
#include "Client_PCH.h"
#include "Gpackets/GCSweeperBonusInfo.h"
#include "BonusSkillHost.h"
#include "MSkillManager.h"

#include <algorithm>
#include <array>
#include <memory>

namespace {
bool Eligible(const BonusSkills::PlayerState& player, size_t index)
{
	// Each bracket grants three consecutive bonus skills.
	const int slayerMinimum[] = {1, 151, 211, 261};
	const int slayerMaximum[] = {150, 210, 260, 300};
	const int otherMinimum[] = {1, 31, 51, 71};
	const int otherMaximum[] = {30, 50, 70, 90};
	const size_t bracket = index / 3;
	if (bracket >= 4) return false;
	if (player.race == RACE_SLAYER)
		return player.statSum >= slayerMinimum[bracket] && player.statSum <= slayerMaximum[bracket];
	if (player.race == RACE_VAMPIRE || player.race == RACE_OUSTERS)
		return player.level >= otherMinimum[bracket] && player.level <= otherMaximum[bracket];
	return false;
}
}

void GCSweeperBonusInfoHandler::execute(GCSweeperBonusInfo* packet, Player* source)
{
	(void)source;
	BonusSkills::PlayerState player;
	if (!BonusSkills::ReadPlayer(player)) return;

	std::array<bool, SWEEPER_BONUS_MAX> next{};
	size_t index = 0;
	while (auto* row = packet->popFrontSweeperBonusInfoList())
	{
		std::unique_ptr<SweeperBonusInfo> owned(row);
		// Live framing caps this list at twelve. Keep direct callers bounded too.
		if (index < next.size()) next[index] = player.race == row->getRace() && Eligible(player, index);
		++index;
	}
	std::copy(next.begin(), next.end(), g_abSweeperBonusSkills);
	BonusSkills::RefreshAvailableSkills();
}
