#pragma once

#include <cstdint>

namespace BonusSkills {

struct PlayerState
{
	int race = -1;
	int level = 0;
	std::int64_t statSum = 0;
};

// The executable supplies the current player only when both the player and
// available-skill set exist. Missing services skip presentation; an unavailable
// player leaves incoming bonus packets and the existing bonus flags unchanged.
struct Host
{
	bool (*ReadPlayer)(PlayerState& player) = nullptr;
	void (*RefreshAvailableSkills)() = nullptr;
};

// Borrowed until replaced. Every action reads the current host again.
const Host* SetHost(const Host* host);
bool ReadPlayer(PlayerState& player);
void RefreshAvailableSkills();

}
