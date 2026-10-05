#pragma once

class RegenTowerInfoManager;

namespace RegenZoneStatus {

struct Host
{
	// Borrow the current UI-owned table for this packet; nullptr skips updates.
	RegenTowerInfoManager* (*Table)() = nullptr;
};

// The caller owns the host and keeps it alive until it is replaced.
const Host* SetHost(const Host* host);

}
