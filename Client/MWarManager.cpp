#include "MWarManager.h"
#include "MZoneTable.h"
#include "UserInformation.h"
#include "GuildWarInfo.h"
#include "RaceWarInfo.h"
#include "LevelWarInfo.h"
#include <algorithm>
#include <memory>
#include <set>

MWarManager* g_pWarManager = nullptr;
const MWarHost* MWarManager::s_pHost = nullptr;

const MWarHost* MWarManager::SetHost(const MWarHost* host)
{
	const MWarHost* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MWarManager::ReadZone(ZoneID_t& id)
{
	return s_pHost && s_pHost->ReadZone && s_pHost->ReadZone(id);
}

void MWarManager::RaceWarNotice(DWORD startTime)
{
	if (s_pHost && s_pHost->RaceWarNotice) s_pHost->RaceWarNotice(startTime);
}

void MWarManager::RaceWarStarted()
{
	if (s_pHost && s_pHost->RaceWarStarted) s_pHost->RaceWarStarted();
}

void MWarManager::RaceWarEnded()
{
	if (s_pHost && s_pHost->RaceWarEnded) s_pHost->RaceWarEnded();
}

MWarManager::MWarManager() = default;

MWarManager::~MWarManager()
{
	ClearWar();
}

bool MWarManager::Owns(const WarInfo* info) const
{
	return std::any_of(m_WarInfo.begin(), m_WarInfo.end(),
		[info](const auto& entry) { return entry.second == info; });
}

void MWarManager::ReleaseUnreferenced(WarInfo* info)
{
	if (info && !Owns(info)) delete info;
}

void MWarManager::Store(ZoneID_t id, WarInfo* info)
{
	auto [entry, inserted] = m_WarInfo.try_emplace(id, info);
	if (!inserted)
	{
		WarInfo* previous = entry->second;
		entry->second = info;
		ReleaseUnreferenced(previous);
	}
}

void MWarManager::UpdateRow(ZoneID_t id, const WarInfo& info)
{
	if (!g_pUserInformation) return;
	auto& rows = g_pUserInformation->WarInfo;
	const auto deadline = MonotonicClock::NowInSeconds()
		+ std::chrono::seconds(static_cast<std::chrono::seconds::rep>(info.getRemainTime()));
	const auto existing = std::find_if(rows.begin(), rows.end(), [&](const WAR_INFO& row) {
		return row.zone_id == id && row.war_type == info.getWarType();
	});
	if (existing != rows.end())
	{
		existing->left_time = deadline;
		return;
	}

	WAR_INFO row{};
	row.zone_id = id;
	row.war_type = info.getWarType();
	row.left_time = deadline;
	const auto* zone = g_pZoneTable ? g_pZoneTable->Get(id) : nullptr;
	const char* name = zone ? zone->Name.GetString() : nullptr;
	if (name) row.zone_name = name;
	if (const auto* guild = dynamic_cast<const GuildWarInfo*>(&info))
	{
		row.attack_guild_name = guild->getAttackGuildName();
		row.defense_guild_name = guild->getDefenseGuildName();
	}
	rows.push_back(std::move(row));
}

void MWarManager::SetWar(WarInfo* info)
{
	if (!info) return;
	// The packet relinquishes ownership before calling us. Keep an unregistered
	// record owned even if it has no zones, only supplies a row, or is rejected.
	std::unique_ptr<WarInfo> incoming(Owns(info) ? nullptr : info);
	switch (info->getWarType())
	{
	case WAR_RACE:
		{
			auto* race = dynamic_cast<RaceWarInfo*>(info);
			if (!race) return;
			RaceWarNotice(race->getStartTime());
			auto& ids = race->getCastleIDs();
			while (!ids.IsEmpty())
			{
				const ZoneID_t id = ids.popValue();
				Store(id, race);
				incoming.release();
				UpdateRow(id, *race);
			}
			RaceWarStarted();
		}
		break;
	case WAR_GUILD:
		{
			auto* guild = dynamic_cast<GuildWarInfo*>(info);
			if (!guild) return;
			Store(guild->getCastleID(), guild);
			incoming.release();
			UpdateRow(guild->getCastleID(), *guild);
		}
		break;
	case WAR_LEVEL:
		{
			auto* level = dynamic_cast<LevelWarInfo*>(info);
			ZoneID_t zone = 0;
			if (level && ReadZone(zone)) UpdateRow(zone, *level);
		}
		break;
	default:
		break;
	}
}

void MWarManager::RemoveWar(ZoneID_t id)
{
	const auto entry = m_WarInfo.find(id);
	if (entry != m_WarInfo.end())
	{
		WarInfo* removed = entry->second;
		m_WarInfo.erase(entry);
		ReleaseUnreferenced(removed);
	}
	if (g_pUserInformation)
		std::erase_if(g_pUserInformation->WarInfo,
			[id](const WAR_INFO& row) { return row.zone_id == id; });
}

WarInfo* MWarManager::GetWarInfo(ZoneID_t id)
{
	switch (id)
	{
	case 1211: case 1212: id = 1201; break;
	case 1221: case 1222: id = 1202; break;
	case 1231: case 1232: id = 1203; break;
	case 1241: case 1242: id = 1204; break;
	case 1251: case 1252: id = 1205; break;
	case 1261: case 1262: id = 1206; break;
	}
	const auto entry = m_WarInfo.find(id);
	return entry == m_WarInfo.end() ? nullptr : entry->second;
}

void MWarManager::ClearWar()
{
	std::set<WarInfo*> records;
	for (const auto& entry : m_WarInfo) records.insert(entry.second);
	m_WarInfo.clear();
	for (auto* record : records) delete record;
	if (g_pUserInformation) g_pUserInformation->WarInfo.clear();
}

void MWarManager::ClearRaceWar()
{
	std::set<WarInfo*> records;
	for (auto entry = m_WarInfo.begin(); entry != m_WarInfo.end(); )
	{
		if (entry->second->getWarType() == WAR_RACE)
		{
			records.insert(entry->second);
			entry = m_WarInfo.erase(entry);
		}
		else
			++entry;
	}
	for (auto* record : records) delete record;
	if (g_pUserInformation)
		std::erase_if(g_pUserInformation->WarInfo,
			[](const WAR_INFO& row) { return row.war_type == WAR_RACE; });
	RaceWarEnded();
}

bool MWarManager::IsExist(ZoneID_t id)
{
	return GetWarInfo(id) != nullptr;
}
