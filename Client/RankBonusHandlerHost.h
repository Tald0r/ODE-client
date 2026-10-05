#pragma once

// Rank-bonus packets update model tables before refreshing the live player.
// The executable installs a host for that refresh. Without a host or callback,
// the table updates still run and the player refresh is skipped.
namespace RankBonusHandlers {

struct Host
{
	void (*CheckRegen)() = nullptr;
};

// The caller owns the host and keeps it alive until it is replaced.
const Host* SetHost(const Host* host);
void CheckRegen();

}
