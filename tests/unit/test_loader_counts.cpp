#include "test_framework.h"
#include "MCreatureTable.h"
#include "MGuildInfoMapper.h"
#include "MMonsterKillQuestInfo.h"
#include "MNPCTable.h"
#include "SkillDef.h"
#include "MSkillManager.h"
#include "MZoneTable.h"
#include "PackFileManager.h"
#include "MString.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>

// The data loaders that read a count and then that many entries. A file
// with no count (empty, or shorter than the count) loads nothing; a count
// past the end of the file loads the entries the file holds and stops
// there. Before this, the first read an uninitialised count and the
// second went on reading, and keeping, entries from an ended stream.
namespace {

const char* kPath = "loader_counts_test.bin";

void Write(const std::string& bytes)
{
	std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string Int(int value)
{
	char bytes[4];
	std::memcpy(bytes, &value, 4);
	return std::string(bytes, 4);
}

std::string Word(unsigned short value)
{
	char bytes[2];
	std::memcpy(bytes, &value, 2);
	return std::string(bytes, 2);
}

// What a save writes.
std::string Saved(const std::function<void(std::ofstream&)>& save)
{
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		save(out);
	}
	std::ifstream in(kPath, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// A record saved with one list entry and then, the same object, with two
// (a default record can hold uninitialised fields): the first byte they
// differ at is the list's count. The record with its count set to 1000,
// ending after its one entry.
std::string CountPastTheFile(const std::string& one, const std::string& two, size_t entrySize)
{
	size_t at = 0;
	while (at < one.size() && one[at] == two[at]) at++;
	std::string bytes = one.substr(0, at + 4 + entrySize);
	const int count = 1000;
	std::memcpy(&bytes[at], &count, 4);
	return bytes;
}

// Loads the file into the record and removes the file.
template<class Record> void Load(Record& record)
{
	{
		std::ifstream in(kPath, std::ios::binary);
		record.LoadFromFile(in);
	}
	std::remove(kPath);
}

struct Pack : PackFileManager<PackFileInfo>
{
	using PackFileManager<PackFileInfo>::HasInfo;
};

} // namespace

TEST(LoaderCounts, TheCreatureSpriteMapperKeepsTheSpriteTypesItsFileHolds)
{
	// Two sprite types where the file holds one, with creature 77.
	Write(Int(2) + Int(1) + Word(77));
	CreatureSpriteTypeMapper mapper;
	Load(mapper);
	CHECK_EQ(77, mapper.GetRandomCreatureType(0));
	// The one the file did not reach, and one past the table: the Blood
	// Warlock, the default.
	CHECK_EQ(28, mapper.GetRandomCreatureType(1));
	CHECK_EQ(28, mapper.GetRandomCreatureType(2));
	// A creature count past the end of the file.
	Write(Int(1) + Int(1000) + Word(78));
	Load(mapper);
	for (int i = 0; i < 20; i++) CHECK_EQ(78, mapper.GetRandomCreatureType(0));
	Write("");
	Load(mapper);
	CHECK_EQ(28, mapper.GetRandomCreatureType(0));
}

TEST(LoaderCounts, TheGuildMapperKeepsTheGuildsItsFileHolds)
{
	// Three guilds where the file holds guild 9, with sprite 77.
	Write(Int(3) + Word(9) + Word(77));
	MGuildInfoMapper guilds;
	Load(guilds);
	CHECK_EQ(1, int(guilds.size()));
	CHECK(guilds.Get(9) != nullptr && guilds.Get(9)->GetSpriteID() == 77);
	// A guild the file names twice is the later one; one the file ends
	// inside is not kept.
	Write(Int(3) + Word(9) + Word(77) + Word(9) + Word(78) + Word(10) + std::string(1, '\x01'));
	Load(guilds);
	CHECK_EQ(1, int(guilds.size()));
	CHECK(guilds.Get(9) != nullptr && guilds.Get(9)->GetSpriteID() == 78);
	CHECK(guilds.Get(10) == nullptr);
	Write("");
	Load(guilds);
	CHECK(guilds.empty());
}

TEST(LoaderCounts, TheQuestManagerKeepsTheQuestsItsFileHolds)
{
	QUEST_INFO quest;
	quest.SetID(5);
	const std::string record = Saved([&](std::ofstream& out) { quest.SaveToFile(out); });
	Write(Int(3) + record);
	MQuestInfoManager quests;
	Load(quests);
	CHECK_EQ(1, int(quests.size()));
	CHECK(quests.GetInfo(5) != nullptr);
	// It adds to what it holds, so an empty file leaves a new one empty.
	MQuestInfoManager fresh;
	Write("");
	Load(fresh);
	CHECK(fresh.empty());
}

TEST(LoaderCounts, AnNPCKeepsTheShopTemplatesItsFileHolds)
{
	NPC_INFO record;
	record.ListShopTemplateID.push_back(7);
	const std::string one = Saved([&](std::ofstream& out) { record.SaveToFile(out); });
	record.ListShopTemplateID.push_back(8);
	Write(CountPastTheFile(one, Saved([&](std::ofstream& out) { record.SaveToFile(out); }), 4));
	NPC_INFO npc;
	Load(npc);
	CHECK_EQ(1, int(npc.ListShopTemplateID.size()));
	CHECK(!npc.ListShopTemplateID.empty() && npc.ListShopTemplateID.front() == 7);
	Write("");
	Load(npc);
	CHECK(npc.ListShopTemplateID.empty());
}

TEST(LoaderCounts, AServerNPCKeepsTheShopTemplatesItsFileHolds)
{
	SERVERNPC_INFO record;
	record.ListShopTemplateID.push_back(7);
	const std::string one = Saved([&](std::ofstream& out) { record.SaveToFile(out); });
	record.ListShopTemplateID.push_back(8);
	Write(CountPastTheFile(one, Saved([&](std::ofstream& out) { record.SaveToFile(out); }), 4));
	SERVERNPC_INFO npc;
	Load(npc);
	CHECK_EQ(1, int(npc.ListShopTemplateID.size()));
	Write("");
	Load(npc);
	CHECK(npc.ListShopTemplateID.empty());
}

TEST(LoaderCounts, ASkillKeepsTheNextSkillsItsFileHolds)
{
	SKILLINFO_NODE record;
	record.Set(1, "Skill", 0, 0, 0, "Skill");
	record.AddNextSkill((ACTIONINFO)10);
	const std::string one = Saved([&](std::ofstream& out) { record.SaveToFile(out); });
	record.AddNextSkill((ACTIONINFO)11);
	Write(CountPastTheFile(one, Saved([&](std::ofstream& out) { record.SaveToFile(out); }), SIZE_ACTIONINFO));
	SKILLINFO_NODE skill;
	Load(skill);
	CHECK_EQ(1, int(skill.GetNextSkillList().size()));
}

TEST(LoaderCounts, AnOustersSkillKeepsTheSkillTypesItsFileHolds)
{
	SKILLINFO_NODE record;
	record.Set(2, "Flourish", 0, 0, 0, "Flourish");
	record.DomainType = SKILLDOMAIN_OUSTERS;
	record.SkillTypeList.push_back(5);
	const std::string one = Saved([&](std::ofstream& out) { record.SaveFromFileServerSkillInfo(out); });
	record.SkillTypeList.push_back(6);
	Write(CountPastTheFile(one, Saved([&](std::ofstream& out) { record.SaveFromFileServerSkillInfo(out); }), 4));
	SKILLINFO_NODE skill;
	{
		std::ifstream in(kPath, std::ios::binary);
		skill.LoadFromFileServerSkillInfo(in);
	}
	std::remove(kPath);
	CHECK_EQ(1, int(skill.SkillTypeList.size()));
}

TEST(LoaderCounts, TheServerSkillInformationStopsWhereItsFileDoes)
{
	SKILLINFO_NODE server;
	server.Set(1, "Fixture", 0, 0, 0, "Fixture");
	server.SetMP(43);
	const std::string record = Saved([&](std::ofstream& out) { server.SaveFromFileServerSkillInfo(out); });
	const int skill = 5;
	auto load = [](MSkillInfoTable& table) {
		std::ifstream in(kPath, std::ios::binary);
		table.LoadFromFileServerSkillInfo(in);
		std::remove(kPath);
	};
	// Three skills where the file holds one.
	{
		MSkillInfoTable table;
		Write(Int(3) + Int(skill) + record);
		load(table);
		CHECK_EQ(43, table[skill].GetMP());
	}
	// A skill the table has no slot for is read past.
	for (int unknown : {MSkillInfoTable().GetSize(), -1})
	{
		MSkillInfoTable table;
		Write(Int(2) + Int(unknown) + record + Int(skill) + record);
		load(table);
		CHECK_EQ(43, table[skill].GetMP());
	}
	{
		MSkillInfoTable table;
		const int mp = table[skill].GetMP();
		Write("");
		load(table);
		CHECK_EQ(mp, table[skill].GetMP());
	}
}

TEST(LoaderCounts, AZoneKeepsTheSoundsItsFileHolds)
{
	ZONETABLE_INFO record;
	record.SoundIDList.push_back(5);
	const std::string one = Saved([&](std::ofstream& out) { record.SaveToFile(out); });
	record.SoundIDList.push_back(6);
	Write(CountPastTheFile(one, Saved([&](std::ofstream& out) { record.SaveToFile(out); }), SIZE_SOUNDID));
	ZONETABLE_INFO zone;
	Load(zone);
	CHECK_EQ(1, int(zone.SoundIDList.size()));
}

TEST(LoaderCounts, TheZoneTableKeepsTheZonesItsFileHolds)
{
	ZONETABLE_INFO zone;
	zone.ID = 31;
	const std::string record = Saved([&](std::ofstream& out) { zone.SaveToFile(out); });
	Write(Int(3) + record);
	CZoneTable zones;
	Load(zones);
	CHECK_EQ(1, int(zones.GetZoneMap().size()));
	CHECK(zones.Get(31) != nullptr);
	Write("");
	Load(zones);
	CHECK(zones.GetZoneMap().empty());
}

TEST(LoaderCounts, APackKeepsTheFilesItsInfoFileHolds)
{
	PackFileInfo info(4, "four.wav");
	const std::string record = Saved([&](std::ofstream& out) { info.SaveToFile(out); });
	Write(Word(3) + record);
	Pack pack;
	CHECK(pack.LoadFromFileInfo(kPath));
	CHECK(pack.HasInfo(DWORD(4)));
	CHECK(!pack.HasInfo(DWORD(0)));
	Write("");
	CHECK(pack.LoadFromFileInfo(kPath));
	CHECK(!pack.HasInfo(DWORD(4)) && !pack.HasInfo(DWORD(0)));
	std::remove(kPath);
}
