//----------------------------------------------------------------------
// game_info_files.h - the game information's files, as tests write them
//----------------------------------------------------------------------
//
// InitInfomation (GameInitInfo.cpp) loads 35 table files and two
// optional ones. Writer::SaveTables writes all of them into a directory,
// each with its own count of entries or a record of its own, so that
// every load can be seen; the files a test names are its own business
// (FileDef's keys, the client configuration, the options, the regen
// towers).
//
//----------------------------------------------------------------------

#pragma once

#include "CUsingColorSetTable.h"
#include "ExperienceTable.h"
#include "SkillDef.h"	// FameInfo.h's SKILLDOMAIN
#include "FameInfo.h"
#include "MActionInfoTable.h"
#include "MChatManager.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MHelpStringTable.h"
#include "MInteractionObjectTable.h"
#include "MItemTable.h"
#include "MLevelNameTable.h"
#include "MMusicTable.h"
#include "MNPCScriptTable.h"
#include "MNPCTable.h"
#include "MShopTemplateTable.h"
#include "MSkillManager.h"
#include "MSoundTable.h"
#include "MZoneTable.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace GameInfoFiles {

// The files InitInfomation opens with FileOpenBinary, in its order.
inline const std::vector<std::string> kTables = {
	"FILE_INFO_USINGCOLORSET", "FILE_INFO_CHAT", "FILE_INFO_SMART_HELP_STRING", "FILE_INFO_LEVEL_NAME",
	"FILE_INFO_MONSTER_NAME", "FILE_INFO_SOUND", "FILE_INFO_MUSIC", "FILE_INFO_ZONE", "FILE_INFO_ITEM",
	"FILE_INFO_INTERACTIONOBJECT", "FILE_INFO_CREATURESPRITE", "FILE_INFO_CREATURE", "FILE_INFO_CREATURESPRITE_MAPPER",
	"FILE_INFO_SHOPTEMPLATE", "FILE_INFO_NPC", "FILE_INFO_NPC_SCRIPT_INFO", "FILE_INFO_EFFECTSPRITETYPE",
	"FILE_INFO_ACTIONEFFECTSPRITETYPE", "FILE_INFO_EFFECTSTATUS", "FILE_INFO_SKILL", "FILE_INFO_SKILL_INFO",
	"FILE_INFO_RANK_BONUS", "FILE_INFO_FAME_LIMIT", "FILE_INFO_STR_EXP", "FILE_INFO_DEX_EXP", "FILE_INFO_INT_EXP",
	"FILE_INFO_VAMPIRE_EXP", "FILE_INFO_OUSTERS_EXP", "FILE_INFO_SLAYER_RANK_EXP", "FILE_INFO_VAMPIRE_RANK_EXP",
	"FILE_INFO_OUSTERS_RANK_EXP", "FILE_PET_EXP_INFO", "FILE_ADVANCE_EXP_INFO", "FILE_INFO_ACTION",
	"FILE_INFO_MONSTER_KILL_QUEST",
};

// The experience files in InitInfomation's order, the rows each holds and
// the table each loads into.
struct ExperienceFile { const char* key; int rows; ExpTable ExperienceTable::*table; };
inline const ExperienceFile kExperience[] = {
	{"FILE_INFO_STR_EXP", 1, &ExperienceTable::m_STRExp},
	{"FILE_INFO_DEX_EXP", 2, &ExperienceTable::m_DEXExp},
	{"FILE_INFO_INT_EXP", 3, &ExperienceTable::m_INTExp},
	{"FILE_INFO_VAMPIRE_EXP", 4, &ExperienceTable::m_VampireExp},
	{"FILE_INFO_OUSTERS_EXP", 5, &ExperienceTable::m_OustersExp},
	{"FILE_INFO_SLAYER_RANK_EXP", 6, &ExperienceTable::m_SlayerRankExp},
	{"FILE_INFO_VAMPIRE_RANK_EXP", 7, &ExperienceTable::m_VampireRankExp},
	{"FILE_INFO_OUSTERS_RANK_EXP", 8, &ExperienceTable::m_OustersRankExp},
	{"FILE_PET_EXP_INFO", 9, &ExperienceTable::m_PetExp},
	{"FILE_ADVANCE_EXP_INFO", 10, &ExperienceTable::m_advanceSkillExp},
};

// The skill both skill files describe.
inline const int kSkill = 5;

// The files, written into a directory.
struct Writer
{
	std::string dir;
	std::string Path(const std::string& key) const { return dir + "/" + key; }

	// A table of n entries read from zero bytes (every field and count 0; a
	// default entry can hold uninitialised fields), saved as its own
	// SaveToFile writes it.
	template<class Table> void SaveSized(const std::string& key, int n) const
	{
		{
			std::ofstream zeros(Path("zeros"), std::ios::binary | std::ios::trunc);
			const std::vector<char> bytes(1 << 16);
			zeros.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
		Table table;
		table.Init(n);
		for (int i = 0; i < n; i++)
		{
			std::ifstream zeros(Path("zeros"), std::ios::binary);
			table.GetInternalPointer()[i].LoadFromFile(zeros);
		}
		std::remove(Path("zeros").c_str());
		std::ofstream out(Path(key), std::ios::binary | std::ios::trunc);
		table.SaveToFile(out);
	}

	// A map of n default records under the ids 1..n.
	template<class Map, class Record> static void SaveMap(std::ofstream& out, int n)
	{
		Map map;
		for (int id = 1; id <= n; id++) map.AddData(id, new Record);
		map.SaveToFile(out);
	}

	static void WriteInt(std::ofstream& out, int value) { out.write(reinterpret_cast<const char*>(&value), 4); }

	// The experience files: a count, then each row's level, goal and total.
	void SaveExperience(const std::string& key, int rows) const
	{
		std::ofstream out(Path(key), std::ios::binary | std::ios::trunc);
		WriteInt(out, rows);
		for (int level = 1; level <= rows; level++) { WriteInt(out, level); WriteInt(out, level * 10); WriteInt(out, level * 100); }
	}

	// Each table its own count of entries, or a record of its own, so
	// that every load can be seen.
	void SaveTables() const
	{
		SaveSized<USINGCOLORSET_TABLE>("FILE_INFO_USINGCOLORSET", 2);
		{
			MChatManager chat;
			chat.AddID("fixture");
			chat.SaveToFile(Path("FILE_INFO_CHAT").c_str());
		}
		SaveSized<MHelpStringTable>("FILE_INFO_SMART_HELP_STRING", 3);
		SaveSized<MLevelNameTable>("FILE_INFO_LEVEL_NAME", 4);
		{
			// The first, middle and last names, one after the other.
			std::ofstream out(Path("FILE_INFO_MONSTER_NAME"), std::ios::binary | std::ios::trunc);
			for (int n : {5, 6, 7})
			{
				MLevelNameTable names;
				names.Init(n);
				names.SaveToFile(out);
			}
		}
		SaveSized<SOUND_TABLE>("FILE_INFO_SOUND", 8);
		SaveSized<MUSIC_TABLE>("FILE_INFO_MUSIC", 9);
		{
			CZoneTable zones;
			ZONETABLE_INFO* pZone = new ZONETABLE_INFO;
			pZone->ID = 31;
			zones.Add(pZone);
			std::ofstream out(Path("FILE_INFO_ZONE"), std::ios::binary | std::ios::trunc);
			zones.SaveToFile(out);
		}
		SaveSized<ITEMCLASS_TABLE>("FILE_INFO_ITEM", 10);
		{
			// No part names, then eleven options of 52 zero bytes each.
			std::ofstream out(Path("FILE_INFO_ITEMOPTION"), std::ios::binary | std::ios::trunc);
			WriteInt(out, 0);
			WriteInt(out, 11);
			const std::vector<char> rows(11 * 52);
			out.write(rows.data(), static_cast<std::streamsize>(rows.size()));
		}
		SaveSized<INTERACTIONOBJECT_TABLE>("FILE_INFO_INTERACTIONOBJECT", 12);
		SaveSized<CREATURESPRITE_TABLE>("FILE_INFO_CREATURESPRITE", 13);
		SaveSized<CREATURE_TABLE>("FILE_INFO_CREATURE", 14);
		{
			CreatureSpriteTypeMapper mapper;
			mapper.Init(4);
			mapper.AddCreatureType(3, 77);
			std::ofstream out(Path("FILE_INFO_CREATURESPRITE_MAPPER"), std::ios::binary | std::ios::trunc);
			mapper.SaveToFile(out);
		}
		{
			std::ofstream out(Path("FILE_INFO_SHOPTEMPLATE"), std::ios::binary | std::ios::trunc);
			SaveMap<MShopTemplateTable, MShopTemplate>(out, 2);
		}
		{
			// Named in the data's own language; the English client's
			// overlay knows NPC 11 (Drake) and script 100's first subject.
			MNPCTable npcs;
			for (int id = 11; id <= 13; id++)
			{
				NPC_INFO* pNPC = new NPC_INFO;
				pNPC->Name = "Korean";
				npcs.AddData(id, pNPC);
			}
			std::ofstream out(Path("FILE_INFO_NPC"), std::ios::binary | std::ios::trunc);
			npcs.SaveToFile(out);
		}
		{
			MNPCScriptTable scripts;
			for (int id = 100; id <= 103; id++)
			{
				NPC_SCRIPT* pScript = new NPC_SCRIPT;
				pScript->SubjectTable.Init(1);
				pScript->SubjectTable.Set(0, "Korean");
				scripts.AddData(id, pScript);
			}
			std::ofstream out(Path("FILE_INFO_NPC_SCRIPT_INFO"), std::ios::binary | std::ios::trunc);
			scripts.SaveToFile(out);
		}
		SaveSized<EFFECTSPRITETYPE_TABLE>("FILE_INFO_EFFECTSPRITETYPE", 15);
		SaveSized<MActionEffectSpriteTypeTable>("FILE_INFO_ACTIONEFFECTSPRITETYPE", 16);
		SaveSized<EFFECTSTATUS_TABLE>("FILE_INFO_EFFECTSTATUS", 17);
		{
			// The whole table, as the skill files must be: one skill on
			// its screen position, and the server's numbers for it.
			MSkillInfoTable skills;
			skills.GetInternalPointer()[kSkill].Set(1, "Fixture", 41, 42, 7, "Korean");
			std::ofstream out(Path("FILE_INFO_SKILL"), std::ios::binary | std::ios::trunc);
			skills.SaveToFile(out);
			SKILLINFO_NODE server;
			server.Set(1, "Fixture", 0, 0, 0, "Korean");
			server.SetMP(43);
			server.SetLearnLevel(44);
			std::ofstream serverOut(Path("FILE_INFO_SKILL_INFO"), std::ios::binary | std::ios::trunc);
			WriteInt(serverOut, 1);
			WriteInt(serverOut, kSkill);
			server.SaveFromFileServerSkillInfo(serverOut);
		}
		{
			// RankBonusInfo saves nothing: a count, then each bonus's type,
			// name, level, race, points and icon.
			std::ofstream out(Path("FILE_INFO_RANK_BONUS"), std::ios::binary | std::ios::trunc);
			WriteInt(out, 18);
			for (int i = 0; i < 18; i++)
			{
				const char zero[4] = {};
				out.write(zero, 2);
				MString().SaveToFile(out);
				out.write(zero, 1);
				out.write(zero, 1);
				out.write(zero, 4);
				out.write(zero, 2);
			}
		}
		SaveSized<FameInfoTable>("FILE_INFO_FAME_LIMIT", 19);
		for (const ExperienceFile& file : kExperience) SaveExperience(file.key, file.rows);
		SaveSized<MActionInfoTable>("FILE_INFO_ACTION", 20);
		{
			// A version, then the quests, each keyed by its own id: its
			// type, id, time limit, creature, sprite, chief, goal, event,
			// level and name.
			std::ofstream out(Path("FILE_INFO_MONSTER_KILL_QUEST"), std::ios::binary | std::ios::trunc);
			WriteInt(out, 1);
			WriteInt(out, 5);
			const char zero[4] = {};
			for (int id = 1; id <= 5; id++)
			{
				out.write(zero, 1);
				WriteInt(out, id);
				for (int field = 0; field < 3; field++) WriteInt(out, 0);
				out.write(zero, 1);
				WriteInt(out, 0);
				out.write(zero, 2);
				MString().SaveToFile(out);
			}
		}
	}
};

} // namespace GameInfoFiles
