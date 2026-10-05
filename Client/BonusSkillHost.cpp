#include "Client_PCH.h"
#include "BonusSkillHost.h"
#include "MSkillManager.h"

// Model state shared by packet application and the executable's skill filter.
bool g_abHolyLandBonusSkills[HOLYLAND_BONUS_MAX] = {};
bool g_abSweeperBonusSkills[SWEEPER_BONUS_MAX] = {};

namespace BonusSkills {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const auto* previous = s_Host;
	s_Host = host;
	return previous;
}

bool ReadPlayer(PlayerState& player)
{
	player = {};
	return s_Host && s_Host->ReadPlayer && s_Host->ReadPlayer(player);
}

void RefreshAvailableSkills()
{
	if (s_Host && s_Host->RefreshAvailableSkills) s_Host->RefreshAvailableSkills();
}

}
