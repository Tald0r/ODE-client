#pragma once

namespace LoginLists {

// UI publication follows successful model application; the corresponding login
// mode follows publication. Missing callbacks are skipped independently.
struct Host
{
	void (*PublishWorlds)() = nullptr;
	void (*SelectWorldMode)() = nullptr;
	void (*PublishServers)() = nullptr;
	void (*SelectServerMode)() = nullptr;
};

// Borrowed until replaced; callbacks may replace or remove the host.
const Host* SetHost(const Host* host);
void WorldListApplied();
void ServerListApplied();

}
