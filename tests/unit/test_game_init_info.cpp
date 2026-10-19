#include "test_framework.h"
#include "GameInitInfo.h"
#include "GameInitInfoHost.h"
#include "Properties.h"
#include "Exception.h"
#include "ClientConfig.h"
#include "UserOption.h"
#include "UIDialog.h"
#include "MChatManager.h"
#include "MHelpStringTable.h"
#include "MHelpDisplayer.h"
#include "MLevelNameTable.h"
#include "MonsterNameTable.h"
#include "ExperienceTable.h"
#include "MNPCScriptTable.h"
#include "ModifyStatusManager.h"
#include "KeyAccelerator.h"
#include "AcceleratorManager.h"
#include "ProfileManager.h"
#include "MGuildMarkManager.h"
#include "MGuildInfoMapper.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "MJusticeAttackManager.h"
#include "RankBonusTable.h"
#include "MWarManager.h"
#include "SkillDef.h"
#include "FameInfo.h"
#include "MTimeItemManager.h"
#include "MMonsterKillQuestInfo.h"
#include "ShrineInfoManager.h"
#include "MEffectSpriteTypeTable.h"
#include "MActionInfoTable.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MEffectGeneratorTable.h"
#include "MEffectStatusTable.h"
#include "MInteractionObjectTable.h"
#include "MItemOptionTable.h"
#include "MItemTable.h"
#include "MMusicTable.h"
#include "MNPCTable.h"
#include "MShopTemplateTable.h"
#include "MSkillManager.h"
#include "MSoundTable.h"
#include "CUsingColorSetTable.h"
#include "MZoneTable.h"
#include "DXLib/CSoundPartManager.h"
#include "AcceleratorDef.h"
#include "InputCodes.h"
#include "SpriteLib/SpriteLibBackend.h"
#include "game_info_files.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// The start-up's loading of the game information (task 4.252: the
// executable's until then, gameclient's since). A test points FileDef's
// properties at files of its own, in a scratch directory, and runs the
// loading as the start-up does.
namespace {

const char* kDir = "game_init_info_fixture";

using GameInfoFiles::kTables;
using GameInfoFiles::ExperienceFile;
using GameInfoFiles::kExperience;
using GameInfoFiles::kSkill;
const GameInfoFiles::Writer kFiles{kDir};

std::vector<std::string> failures;
// The host's other calls, in order.
std::vector<std::string> calls;

std::string Fade(signed char start, signed char end, signed char step, BYTE red, BYTE green, BYTE blue, WORD delay)
{
	std::string text = "fade";
	for (int value : {int(start), int(end), int(step), int(red), int(green), int(blue), int(delay)}) text += " " + std::to_string(value);
	return text;
}

MonotonicClock::TimePoint now;
MonotonicClock::TimePoint Clock() { return now; }

// What the loading's Inits leave in their objects' protected tables, read
// through member pointers named in a derived class.
struct DialogPeek : UIDialog
{
	static char** PCTalk(UIDialog& dialog) { return (dialog.*(&DialogPeek::m_ppDlgMessage))[MESSAGE_PCTALK]; }
};
struct GeneratorPeek : MEffectGeneratorTable
{
	static int Count(MEffectGeneratorTable& table)
	{
		int count = 0;
		for (MEffectGenerator* pGenerator : table.*(&GeneratorPeek::m_pEffectGenerator))
			if (pGenerator != nullptr) count++;
		return count;
	}
};

std::string Path(const std::string& key) { return std::string(kDir) + "/" + key; }


// The client's FileDef pointed at the scratch directory, every table
// file there and empty, the regen towers' file one tower long; the
// host's calls recorded. Everything the loading made is freed after.
struct Fixture
{
	Properties definitions;
	Properties* oldDefinitions = g_pFileDef;
	GameInitInfoHost::Host host;
	const GameInitInfoHost::Host* oldHost = nullptr;

	Fixture()
	{
		failures.clear();
		calls.clear();
		std::error_code error;
		std::filesystem::remove_all(kDir, error);
		std::filesystem::create_directory(kDir);
		for (const std::string& key : kTables) definitions.setProperty(key, Path(key));
		definitions.setProperty("FILE_INFO_ITEMOPTION", Path("FILE_INFO_ITEMOPTION"));
		kFiles.SaveTables();
		Write("FILE_INFO_CLIENTCONFIG", std::string());
		Write("FILE_INFO_USEROPTION", std::string());
		{
			MGuildInfoMapper guilds;
			guilds.Set(9, new GUILD_INFO("Fixture", "Leader"));
			std::ofstream out(Path("FILE_INFO_GUILD_INFO_MAPPER"), std::ios::binary | std::ios::trunc);
			guilds.SaveToFile(out);
			definitions.setProperty("FILE_INFO_GUILD_INFO_MAPPER", Path("FILE_INFO_GUILD_INFO_MAPPER"));
		}
		// Named only: the guild marks' sprite packs load when first drawn.
		for (const char* key : {"FILE_SPRITE_GUILD_MARK", "FILE_SPRITE_GRADE_MARK_SLAYER", "FILE_SPRITE_GRADE_MARK_VAMPIRE",
			"FILE_SPRITE_GRADE_MARK_OUSTERS", "FILE_SPRITE_LEVEL_MARK_PET"})
			definitions.setProperty(key, Path(key));
		definitions.setProperty("FILE_INFO_DATA", std::string(kDir) + "/absent.rpk");
		// The regen towers' file is read beside the archive (a loose override).
		Write("FILE_REGEN_TOWER_INFO", "*1\n0 72 18 21\n");
		definitions.setProperty("FILE_REGEN_TOWER_INFO", "FILE_REGEN_TOWER_INFO");
		host.InitFailMessage = [](const char* message) { failures.push_back(message); };
		host.PersonalShopUpdateTime = []() { return 4321; };
		host.InitPacketItemTable = []() { calls.push_back("packet items"); };
		host.ShowError = [](const char* message) { calls.push_back(std::string("error|") + message); };
		host.SetFadeStart = [](signed char start, signed char end, signed char step, BYTE red, BYTE green, BYTE blue, WORD delay) {
			calls.push_back(Fade(start, end, step, red, green, blue, delay));
		};
		oldHost = GameInitInfoHost::SetHost(&host);
		g_pFileDef = &definitions;
	}
	~Fixture()
	{
		FreeAll();
		g_pFileDef = oldDefinitions;
		GameInitInfoHost::SetHost(oldHost);
		std::error_code error;
		std::filesystem::remove_all(kDir, error);
	}
	void Write(const std::string& key, const std::string& bytes)
	{
		std::ofstream out(Path(key), std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		definitions.setProperty(key, Path(key));
	}
	void Remove(const std::string& key) { std::remove(Path(key).c_str()); }
	// The client's language (Language.inf's LANGUAGE line; 3 is English,
	// which the client also takes without the file).
	void Language(int language) { Write("FILE_LANGUAGE_INFO", "LANGUAGE " + std::to_string(language) + "\n"); }
	template<class T> static void Free(T*& p) { delete p; p = nullptr; }
	static void FreeAll()
	{
		Free(g_pRegenTowerInfoManager); Free(g_pQuestInfoManager); Free(g_pTimeItemManager); Free(g_pFameInfoTable);
		Free(g_pWarManager); Free(g_pKeyAccelerator); Free(g_pAcceleratorManager); Free(g_pModifyStatusManager);
		Free(g_pEffectGeneratorTable); Free(g_pUIDialog); Free(g_pProfileManager); Free(g_pGuildMarkManager);
		Free(g_pGuildInfoMapper); Free(g_pEventManager); Free(g_pEffectSpriteTypeTable);
		Free(g_pActionEffectSpriteTypeTable); Free(g_pCreatureTable); Free(g_pCreatureSpriteTypeMapper);
		Free(g_pItemTable); Free(g_pCreatureSpriteTable); Free(g_pEffectStatusTable); Free(g_pInteractionObjectTable);
		Free(g_pItemOptionTable); Free(g_pNPCTable); Free(g_pJusticeAttackManager); Free(g_pNPCScriptTable);
		Free(g_pShopTemplateTable); Free(g_pSkillInfoTable); Free(g_pRankBonusTable); Free(g_pExperienceTable);
		Free(g_pSoundTable); Free(g_pMusicTable); Free(g_pZoneTable); Free(g_pSoundManager);
		Free(g_pUsingColorSetTable); Free(g_pActionInfoTable); Free(g_pChatManager); Free(g_pHelpStringTable);
		Free(g_pHelpDisplayer); Free(g_pLevelNameTable); Free(g_pMonsterNameTable);
		Free(g_pClientConfig); Free(g_pUserOption);
	}
};

// InitInfomation; a Throwable it lets out is printed and taken as a
// failure.
bool Load()
{
	try { return InitInfomation() != FALSE; }
	catch (Throwable& t) { std::printf("      %s\n", t.toString().c_str()); return false; }
}

} // namespace

TEST(GameInitInfo, WithEveryFileThereEverythingIsMade)
{
	Fixture fixture;
	CHECK(Load());
	CHECK(failures.empty());
	// The packets' item table is made once.
	CHECK(calls == std::vector<std::string>({"packet items"}));
	// The default key bindings, the talk box's line and the effect
	// generators.
	CHECK(g_pKeyAccelerator != nullptr && g_pKeyAccelerator->GetKey(ACCEL_F1) == DIK_F1);
	CHECK(g_pUIDialog != nullptr && DialogPeek::PCTalk(*g_pUIDialog) != nullptr);
	CHECK(g_pEffectGeneratorTable != nullptr && GeneratorPeek::Count(*g_pEffectGeneratorTable) > 0);
	// Made, with nothing to load.
	CHECK(g_pKeyAccelerator != nullptr && g_pWarManager != nullptr && g_pTimeItemManager != nullptr);
	CHECK(g_pAcceleratorManager != nullptr && g_pModifyStatusManager != nullptr && g_pEffectGeneratorTable != nullptr);
	CHECK(g_pUIDialog != nullptr && g_pProfileManager != nullptr && g_pGuildInfoMapper != nullptr);
	CHECK(g_pGuildMarkManager != nullptr && g_pEventManager != nullptr && g_pJusticeAttackManager != nullptr);
	CHECK(g_pSoundManager != nullptr && g_pHelpDisplayer != nullptr);
	if (g_pRegenTowerInfoManager == nullptr || g_pQuestInfoManager == nullptr || g_pExperienceTable == nullptr
		|| g_pSkillInfoTable == nullptr || g_pChatManager == nullptr || g_pMonsterNameTable == nullptr
		|| g_pZoneTable == nullptr || g_pCreatureSpriteTypeMapper == nullptr || g_pNPCTable == nullptr
		|| g_pNPCScriptTable == nullptr || g_pShopTemplateTable == nullptr || g_pUsingColorSetTable == nullptr
		|| g_pHelpStringTable == nullptr || g_pLevelNameTable == nullptr || g_pSoundTable == nullptr
		|| g_pMusicTable == nullptr || g_pItemTable == nullptr || g_pItemOptionTable == nullptr
		|| g_pInteractionObjectTable == nullptr || g_pCreatureSpriteTable == nullptr || g_pCreatureTable == nullptr
		|| g_pEffectSpriteTypeTable == nullptr || g_pActionEffectSpriteTypeTable == nullptr
		|| g_pEffectStatusTable == nullptr || g_pRankBonusTable == nullptr || g_pFameInfoTable == nullptr
		|| g_pActionInfoTable == nullptr)
	{
		CHECK(!"a table was not made");
		return;
	}
	// Each loaded from its file.
	CHECK_EQ(2, g_pUsingColorSetTable->GetSize());
	CHECK(!g_pChatManager->IsAcceptID("fixture") || !g_pChatManager->IsAcceptID("someone else"));
	CHECK(g_pChatManager->IsAcceptID("fixture") != g_pChatManager->IsAcceptID("someone else"));
	CHECK_EQ(3, g_pHelpStringTable->GetSize());
	CHECK_EQ(4, g_pLevelNameTable->GetSize());
	CHECK_EQ(5, g_pMonsterNameTable->GetFirstNameSize());
	CHECK_EQ(6, g_pMonsterNameTable->GetMiddleNameSize());
	CHECK_EQ(7, g_pMonsterNameTable->GetLastNameSize());
	CHECK_EQ(8, g_pSoundTable->GetSize());
	CHECK_EQ(9, g_pMusicTable->GetSize());
	CHECK(g_pZoneTable->Get(31) != nullptr);
	CHECK_EQ(10, g_pItemTable->GetSize());
	CHECK_EQ(11, g_pItemOptionTable->GetSize());
	CHECK_EQ(12, g_pInteractionObjectTable->GetSize());
	CHECK_EQ(13, g_pCreatureSpriteTable->GetSize());
	CHECK_EQ(14, g_pCreatureTable->GetSize());
	CHECK_EQ(77, g_pCreatureSpriteTypeMapper->GetRandomCreatureType(3));
	CHECK_EQ(2, int(g_pShopTemplateTable->size()));
	CHECK_EQ(3, int(g_pNPCTable->size()));
	CHECK_EQ(4, int(g_pNPCScriptTable->size()));
	CHECK_EQ(15, g_pEffectSpriteTypeTable->GetSize());
	CHECK_EQ(16, g_pActionEffectSpriteTypeTable->GetSize());
	CHECK_EQ(17, g_pEffectStatusTable->GetSize());
	CHECK_EQ(41, (*g_pSkillInfoTable)[kSkill].GetX());
	CHECK_EQ(42, (*g_pSkillInfoTable)[kSkill].GetY());
	CHECK_EQ(43, (*g_pSkillInfoTable)[kSkill].GetMP());
	CHECK_EQ(44, (*g_pSkillInfoTable)[kSkill].GetLearnLevel());
	CHECK_EQ(18, g_pRankBonusTable->GetSize());
	CHECK_EQ(19, g_pFameInfoTable->GetSize());
	// A table holds a row more than its file: the zeroth.
	for (const ExperienceFile& file : kExperience)
		CHECK_EQ(file.rows + 1, ((*g_pExperienceTable).*file.table).GetSize());
	CHECK_EQ(20, g_pActionInfoTable->GetSize());
	CHECK_EQ(5, int(g_pQuestInfoManager->size()));
	CHECK_EQ(1, g_pRegenTowerInfoManager->GetSize());
	CHECK(g_pGuildInfoMapper->Get(9) != nullptr);
}

TEST(GameInitInfo, AMissingTableFileStopsTheLoading)
{
	for (const std::string& key : kTables)
	{
		Fixture fixture;
		fixture.Remove(key);
		CHECK(!Load());
		const bool said = failures.size() == 1 && failures[0] == "[Error] File Open Error : [" + Path(key) + "]";
		if (!said) std::printf("      %s: %zu failures\n", key.c_str(), failures.size());
		CHECK(said);
	}
}

TEST(GameInitInfo, TheOptionalFilesMayBeMissing)
{
	// The item options and the guild names load when they are there.
	Fixture fixture;
	fixture.Remove("FILE_INFO_ITEMOPTION");
	fixture.Remove("FILE_INFO_GUILD_INFO_MAPPER");
	CHECK(Load());
	CHECK(failures.empty());
	CHECK(g_pItemOptionTable != nullptr && g_pItemOptionTable->GetSize() == 0);
	CHECK(g_pGuildInfoMapper != nullptr && g_pGuildInfoMapper->empty());
	CHECK(g_pActionInfoTable != nullptr && g_pActionInfoTable->GetSize() == 20);
}

TEST(GameInitInfo, AMissingRegenTowerFileStopsTheLoading)
{
	// It says so in an error box of its own (the host's), before any table
	// loads.
	Fixture fixture;
	fixture.Remove("FILE_REGEN_TOWER_INFO");
	CHECK(!Load());
	CHECK(failures.empty());
	CHECK(calls == std::vector<std::string>({"error|Cannot Open RTI File"}));
	CHECK(g_pChatManager != nullptr && g_pChatManager->IsAcceptID("fixture") == g_pChatManager->IsAcceptID("someone else"));
	CHECK(g_pSoundTable != nullptr && g_pSoundTable->GetSize() == 0);
}

TEST(GameInitInfo, TheClientConfigIsLoadedAndWrittenBack)
{
	Fixture fixture;
	{
		ClientConfig config;
		config.MAX_SCREENSHOT = 123;
		config.SaveToFile(Path("FILE_INFO_CLIENTCONFIG").c_str());
	}
	CHECK(InitClientConfig());
	CHECK(g_pClientConfig != nullptr && g_pClientConfig->MAX_SCREENSHOT == 123);
	// Without a file: the defaults, written where the file belongs.
	Fixture::Free(g_pClientConfig);
	fixture.Remove("FILE_INFO_CLIENTCONFIG");
	CHECK(InitClientConfig());
	const ClientConfig defaults;
	CHECK(g_pClientConfig != nullptr && g_pClientConfig->MAX_SCREENSHOT == defaults.MAX_SCREENSHOT);
	ClientConfig written;
	written.MAX_SCREENSHOT = defaults.MAX_SCREENSHOT + 1;
	CHECK(written.LoadFromFile(Path("FILE_INFO_CLIENTCONFIG").c_str()));
	CHECK_EQ(defaults.MAX_SCREENSHOT, written.MAX_SCREENSHOT);
}

TEST(GameInitInfo, TheUserOptionsAreLoadedAndCompleted)
{
	// The options load into the key bindings InitInfomation makes.
	Fixture fixture;
	CHECK(Load());
	{
		UserOption option;
		option.PlaySound = FALSE;
		option.PlayWaveMusic = FALSE;
		option.UseXbrz = FALSE;
		option.SaveToFile(Path("FILE_INFO_USEROPTION").c_str());
	}
	const int xbrz = spritectl_get_xbrz_enabled();
	spritectl_set_xbrz_enabled(1);
	CHECK(InitUserOption());
	CHECK(g_pUserOption != nullptr);
	if (g_pUserOption != nullptr)
	{
		CHECK_EQ(FALSE, g_pUserOption->PlaySound);
		CHECK_EQ(FALSE, g_pUserOption->UseXbrz);
		// The sprites are smoothed as the options say.
		CHECK_EQ(0, spritectl_get_xbrz_enabled());
		// The music always plays from waves; the shop's update time is
		// the executable's.
		CHECK_EQ(TRUE, g_pUserOption->PlayWaveMusic);
		CHECK_EQ(4321, g_pUserOption->persnalShopupdatetime);
	}
	spritectl_set_xbrz_enabled(xbrz);
}

TEST(GameInitInfo, WithoutAHostTheDefaultsApply)
{
	Fixture fixture;
	GameInitInfoHost::SetHost(nullptr);
	CHECK(Load());
	// A real options file: the key bindings' loader reads an empty one's
	// count uninitialised (fixed with its siblings' loaders).
	UserOption().SaveToFile(Path("FILE_INFO_USEROPTION").c_str());
	const int xbrz = spritectl_get_xbrz_enabled();
	CHECK(InitUserOption());
	spritectl_set_xbrz_enabled(xbrz);
	CHECK(g_pUserOption != nullptr && g_pUserOption->persnalShopupdatetime == 60000);
	// A missing file still stops the loading, with no one to tell.
	fixture.Remove("FILE_INFO_SOUND");
	CHECK(!Load());
	CHECK(failures.empty());
}

TEST(GameInitInfo, TheHostsEntriesArePassedOn)
{
	const GameInitInfoHost::Host* old = GameInitInfoHost::SetHost(nullptr);
	calls.clear();
	failures.clear();
	// Without a host, or without an entry: nothing, and the default time.
	GameInitInfoHost::SetFadeStart(1, 2, 3, 4, 5, 6, 7);
	GameInitInfoHost::InitPacketItemTable();
	GameInitInfoHost::InitFailMessage("unheard");
	GameInitInfoHost::ShowError("unseen");
	CHECK_EQ(60000, GameInitInfoHost::PersonalShopUpdateTime());
	const GameInitInfoHost::Host empty{};
	CHECK(GameInitInfoHost::SetHost(&empty) == nullptr);
	GameInitInfoHost::SetFadeStart(1, 2, 3, 4, 5, 6, 7);
	GameInitInfoHost::InitPacketItemTable();
	GameInitInfoHost::InitFailMessage("unheard");
	CHECK_EQ(60000, GameInitInfoHost::PersonalShopUpdateTime());
	CHECK(calls.empty() && failures.empty());
	const GameInitInfoHost::Host host{
		.SetFadeStart = [](signed char start, signed char end, signed char step, BYTE red, BYTE green, BYTE blue, WORD delay) {
			calls.push_back(Fade(start, end, step, red, green, blue, delay));
		},
		.InitPacketItemTable = []() { calls.push_back("packet items"); },
		.PersonalShopUpdateTime = []() { return 7; },
		.InitFailMessage = [](const char* message) { failures.push_back(message); },
		.ShowError = [](const char* message) { calls.push_back(std::string("error|") + message); },
	};
	CHECK(GameInitInfoHost::SetHost(&host) == &empty);
	GameInitInfoHost::SetFadeStart(-1, 2, 3, 4, 5, 6, 700);
	GameInitInfoHost::InitPacketItemTable();
	GameInitInfoHost::InitFailMessage("heard");
	GameInitInfoHost::ShowError("shown");
	CHECK_EQ(7, GameInitInfoHost::PersonalShopUpdateTime());
	CHECK(calls == std::vector<std::string>({"fade -1 2 3 4 5 6 700", "packet items", "error|shown"}));
	CHECK(failures == std::vector<std::string>({"heard"}));
	CHECK(GameInitInfoHost::SetHost(old) == &host);
}

TEST(GameInitInfo, TheEventsFadeThroughTheHostAndKeepEffectsWithoutAPlayer)
{
	// The event queue InitInfomation makes: the Gilles de Rais shake's
	// end starts the dark fade, and with no player an effect's event
	// stays.
	Fixture fixture;
	MonotonicClock::ScopedTestSource source{Clock};
	now = MonotonicClock::FromMillis(1000);
	CHECK(Load());
	if (g_pEventManager == nullptr) return;
	calls.clear();
	MEvent shake;
	shake.eventID = EVENTID_GDR_PRESENT;
	shake.eventDelay = 0;
	g_pEventManager->AddEvent(shake);
	MEvent effect;
	effect.eventID = EVENTID_METEOR;
	effect.eventType = EVENTTYPE_EFFECT;
	effect.parameter1 = 5;
	g_pEventManager->AddEvent(effect);
	now += MonotonicClock::Millis(1);
	g_pEventManager->ProcessEvent();
	CHECK(!g_pEventManager->IsEvent(EVENTID_GDR_PRESENT));
	CHECK(g_pEventManager->IsEvent(EVENTID_METEOR));
	CHECK(calls == std::vector<std::string>({"fade 31 -1 1 0 0 0 4"}));
}

TEST(GameInitInfo, AReloadStartsFromNewObjects)
{
	// The debug build reloads the information (Alt+F1, CGameUpdate): what
	// the first load made is freed and made again, so nothing carries over.
	Fixture fixture;
	CHECK(Load());
	if (g_pEventManager == nullptr || g_pJusticeAttackManager == nullptr || g_pKeyAccelerator == nullptr
		|| g_pProfileManager == nullptr || g_pTimeItemManager == nullptr) return;
	MEvent event;
	event.eventID = EVENTID_METEOR;
	g_pEventManager->AddEvent(event);
	g_pJusticeAttackManager->AddCreature("Attacker");
	const WORD key = g_pKeyAccelerator->GetKey(1);
	g_pKeyAccelerator->SetAcceleratorKey(1, WORD(key + 1));
	g_pProfileManager->AddProfile("Slayer", "slayer.spk");
	g_pTimeItemManager->AddTimeItem(5, 100);
	// The optional files are gone by the reload.
	fixture.Remove("FILE_INFO_ITEMOPTION");
	fixture.Remove("FILE_INFO_GUILD_INFO_MAPPER");
	CHECK(Load());
	CHECK(g_pEventManager != nullptr && !g_pEventManager->IsEvent(EVENTID_METEOR));
	CHECK(g_pJusticeAttackManager != nullptr && !g_pJusticeAttackManager->HasCreature("Attacker"));
	CHECK(g_pKeyAccelerator != nullptr && g_pKeyAccelerator->GetKey(1) == key);
	CHECK(g_pProfileManager != nullptr && !g_pProfileManager->HasProfile("Slayer"));
	CHECK(g_pTimeItemManager != nullptr && !g_pTimeItemManager->IsExist(5));
	CHECK(g_pItemOptionTable != nullptr && g_pItemOptionTable->GetSize() == 0);
	CHECK(g_pGuildInfoMapper != nullptr && g_pGuildInfoMapper->empty());
}

TEST(GameInitInfo, TheEnglishClientShowsTheEnglishNames)
{
	// The data files hold the names in their own language; in English the
	// built-in translations are laid over the NPCs, their scripts and the
	// skills' display names.
	for (bool withFile : {false, true})
	{
		Fixture fixture;
		if (withFile) fixture.Language(3);
		CHECK(Load());
		if (g_pNPCTable == nullptr || g_pNPCScriptTable == nullptr || g_pSkillInfoTable == nullptr) return;
		CHECK(g_pNPCTable->GetData(11) != nullptr && std::string(g_pNPCTable->GetData(11)->Name.GetString()) == "Drake");
		const NPC_SCRIPT* pScript = g_pNPCScriptTable->GetData(100);
		CHECK(pScript != nullptr && std::string(pScript->SubjectTable[0].GetString()).rfind("These accessories", 0) == 0);
		CHECK(std::string((*g_pSkillInfoTable)[kSkill].GetHName()) == "Fixture");
	}
}

TEST(GameInitInfo, AnotherLanguageKeepsTheDataFilesNames)
{
	Fixture fixture;
	fixture.Language(0);
	CHECK(Load());
	if (g_pNPCTable == nullptr || g_pNPCScriptTable == nullptr || g_pSkillInfoTable == nullptr) return;
	CHECK(g_pNPCTable->GetData(11) != nullptr && std::string(g_pNPCTable->GetData(11)->Name.GetString()) == "Korean");
	const NPC_SCRIPT* pScript = g_pNPCScriptTable->GetData(100);
	CHECK(pScript != nullptr && std::string(pScript->SubjectTable[0].GetString()) == "Korean");
	CHECK(std::string((*g_pSkillInfoTable)[kSkill].GetHName()) == "Korean");
}

TEST(GameInitInfo, AnEmptyEffectSpriteTableStopsTheLoading)
{
	// No effect sprite types: the loading stops, with no message of its own.
	Fixture fixture;
	kFiles.SaveSized<EFFECTSPRITETYPE_TABLE>("FILE_INFO_EFFECTSPRITETYPE", 0);
	CHECK(!Load());
	CHECK(failures.empty());
}
