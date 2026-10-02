//----------------------------------------------------------------------
// Per-world account settings and the last selected character slot.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "PCConfigTable.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace {

constexpr int PLAYER_CONFIG_VERSION = 2;
constexpr size_t LIMIT_PLAYER_CONFIG = 20;
static_assert(sizeof(int) == 4 && sizeof(DWORD) == 4 && sizeof(BYTE) == 1);

// Even a placeholder consumes bytes. Bound counts before allocating or
// iterating, and restore the stream to the first record afterwards.
bool ReadCount(std::ifstream& file, int& count, std::streamoff minimumBytes)
{
	count = 0;
	if (!file.read(reinterpret_cast<char*>(&count), 4) || count < 0)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	const std::streamoff current = file.tellg();
	if (current < 0)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	file.seekg(0, std::ios::end);
	const std::streamoff end = file.tellg();
	if (!file.good() || end < current)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	file.seekg(current, std::ios::beg);
	if (!file.good() || count > (end - current) / minimumBytes)
	{
		file.setstate(std::ios::failbit);
		return false;
	}
	return true;
}

std::vector<PlayerConfig*> SavedAccounts(const PlayerConfigTable& table)
{
	std::vector<PlayerConfig*> accounts;
	accounts.reserve(table.size());
	for (const auto& entry : table)
		accounts.push_back(entry.second);
	if (accounts.size() > LIMIT_PLAYER_CONFIG)
	{
		// Map order breaks ties as before. Null placeholders rank after accounts.
		std::stable_sort(accounts.begin(), accounts.end(), [](const auto* left, const auto* right) {
			if (left == nullptr) return false;
			if (right == nullptr) return true;
			return left->GetRecentCount() < right->GetRecentCount();
		});
		accounts.resize(LIMIT_PLAYER_CONFIG);
	}
	return accounts;
}

bool CanSave(const std::vector<PlayerConfig*>& accounts)
{
	for (const auto* config : accounts)
	{
		if (config == nullptr) continue;
		const auto& name = config->GetPlayerID();
		if (name.empty() || name.size() > 255 || name.find('\0') != std::string::npos
			|| config->GetLastSlot() < 0 || config->GetLastSlot() >= 3)
			return false;
	}
	return true;
}

} // namespace

WorldPlayerConfigTable* g_pWorldPlayerConfigTable = NULL;

PlayerConfig::PlayerConfig()
{
	m_LastSlot = 0;
	m_RecentCount = 0;
}

PlayerConfig::~PlayerConfig()
{
}

void PlayerConfig::SetLastSlot(int slot)
{
	if (slot < 0 || slot >= 3)
		return;
	m_LastSlot = static_cast<BYTE>(slot);
	m_RecentCount = 0;
}

void PlayerConfig::SaveToFile(std::ofstream& file)
{
	// An old account must never wrap around and become the most recent one.
	const DWORD recent = m_RecentCount == (std::numeric_limits<DWORD>::max)()
		? m_RecentCount : m_RecentCount + 1;
	file.write(reinterpret_cast<const char*>(&m_LastSlot), 1);
	file.write(reinterpret_cast<const char*>(&recent), 4);
	if (file.good())
		m_RecentCount = recent;
}

void PlayerConfig::LoadFromFile(std::ifstream& file)
{
	BYTE slot = 0;
	DWORD recent = 0;
	if (!file.read(reinterpret_cast<char*>(&slot), 1)
		|| !file.read(reinterpret_cast<char*>(&recent), 4) || slot >= 3)
	{
		file.setstate(std::ios::failbit);
		return;
	}
	m_LastSlot = slot;
	m_RecentCount = recent;
}

PlayerConfigTable::PlayerConfigTable()
{
}

PlayerConfigTable::~PlayerConfigTable()
{
	Release();
}

void PlayerConfigTable::Release()
{
	for (auto& entry : *this)
		delete entry.second;
	clear();
}

void PlayerConfigTable::AddPlayerConfig(PlayerConfig* pConfig)
{
	if (pConfig == NULL || pConfig->GetPlayerID().empty())
		return;
	const auto found = find(pConfig->GetPlayerID());
	if (found != end())
	{
		if (found->second == pConfig)
			return;
		delete found->second;
		found->second = pConfig;
	}
	else
	{
		emplace(pConfig->GetPlayerID(), pConfig);
	}
}

PlayerConfig* PlayerConfigTable::GetPlayerConfig(const char* pPlayerID) const
{
	if (pPlayerID == NULL)
		return NULL;
	const auto found = find(pPlayerID);
	return found == end() ? NULL : found->second;
}

void PlayerConfigTable::SaveToFile(std::ofstream& file)
{
	const auto accounts = SavedAccounts(*this);
	if (!CanSave(accounts))
	{
		file.setstate(std::ios::failbit);
		return;
	}
	const int count = static_cast<int>(accounts.size());
	file.write(reinterpret_cast<const char*>(&count), 4);
	for (auto* config : accounts)
	{
		if (!file.good()) return;
		if (config == NULL)
		{
			const BYTE length = 0;
			file.write(reinterpret_cast<const char*>(&length), 1);
			continue;
		}
		const auto& name = config->GetPlayerID();
		const BYTE length = static_cast<BYTE>(name.size());
		file.write(reinterpret_cast<const char*>(&length), 1);
		file.write(name.data(), static_cast<std::streamsize>(name.size()));
		config->SaveToFile(file);
	}
}

void PlayerConfigTable::LoadFromFile(std::ifstream& file)
{
	Release();
	int count = 0;
	if (!ReadCount(file, count, 1))
		return;
	PlayerConfigTable parsed;
	for (int i = 0; i < count; ++i)
	{
		BYTE length = 0;
		if (!file.read(reinterpret_cast<char*>(&length), 1))
			return;
		if (length == 0)
			continue;
		std::string name(length, '\0');
		if (!file.read(name.data(), length) || name.find('\0') != std::string::npos)
		{
			file.setstate(std::ios::failbit);
			return;
		}
		auto config = std::make_unique<PlayerConfig>();
		config->LoadFromFile(file);
		if (!file.good()) return;
		config->SetPlayerID(name);
		parsed.AddPlayerConfig(config.get());
		config.release();
	}
	// Publish only a complete table. Any failed read destroys the staged rows.
	swap(parsed);
}

WorldPlayerConfigTable::WorldPlayerConfigTable()
{
}

WorldPlayerConfigTable::~WorldPlayerConfigTable()
{
	Release();
}

void WorldPlayerConfigTable::Release()
{
	for (auto& entry : *this)
		delete entry.second;
	clear();
}

void WorldPlayerConfigTable::AddPlayerConfigTable(int worldID, PlayerConfigTable* pTable)
{
	if (pTable == NULL)
		return;
	const auto found = find(worldID);
	if (found != end())
	{
		if (found->second == pTable)
			return;
		delete found->second;
		found->second = pTable;
	}
	else
	{
		emplace(worldID, pTable);
	}
}

PlayerConfigTable* WorldPlayerConfigTable::GetPlayerConfigTable(int worldID) const
{
	const auto found = find(worldID);
	return found == end() ? NULL : found->second;
}

void WorldPlayerConfigTable::SaveToFile(const char* pFilename)
{
	if (pFilename == NULL || size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
		return;
	// Validate nested records before opening (and truncating) the existing file.
	for (const auto& [worldID, table] : *this)
	{
		if (table != NULL && (worldID == -1 || !CanSave(SavedAccounts(*table))))
			return;
	}
	std::ofstream file(pFilename, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) return;
	file.write(reinterpret_cast<const char*>(&PLAYER_CONFIG_VERSION), 4);
	const int count = static_cast<int>(size());
	file.write(reinterpret_cast<const char*>(&count), 4);
	for (const auto& [worldID, table] : *this)
	{
		if (!file.good()) return;
		const int storedID = table == NULL ? -1 : worldID;
		file.write(reinterpret_cast<const char*>(&storedID), 4);
		if (table != NULL)
			table->SaveToFile(file);
	}
}

void WorldPlayerConfigTable::LoadFromFile(const char* pFilename)
{
	Release();
	if (pFilename == NULL)
		return;
	std::ifstream file(pFilename, std::ios::binary);
	if (!file.is_open()) return;
	int version = 0;
	if (!file.read(reinterpret_cast<char*>(&version), 4) || version != PLAYER_CONFIG_VERSION)
		return;
	int count = 0;
	if (!ReadCount(file, count, 4))
		return;
	WorldPlayerConfigTable parsed;
	for (int i = 0; i < count; ++i)
	{
		int worldID = 0;
		if (!file.read(reinterpret_cast<char*>(&worldID), 4))
			return;
		if (worldID == -1)
			continue;
		auto table = std::make_unique<PlayerConfigTable>();
		table->LoadFromFile(file);
		if (!file.good()) return;
		parsed.AddPlayerConfigTable(worldID, table.get());
		table.release();
	}
	swap(parsed);
}
