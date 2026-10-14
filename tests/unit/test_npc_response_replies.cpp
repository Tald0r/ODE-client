#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "BonusSkillHost.h"
#include "ClientPlayer.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MMoneyManager.h"
#include "MSkillManager.h"
#include "MSlayerGear.h"
#include "MStorage.h"
#include "NPCPresentationHost.h"
#include "PlayerStateHost.h"
#include "SafeFormat.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "SoundDef.h"
#include "TempInformation.h"
#include "UserInformation.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Cpackets/CGStashList.h"
#include "Gpackets/GCNPCResponse.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// An NPC's answer. A player waiting on the NPC's answer stops waiting.
// By the code it then:
// - opens a window (repair, waypoints, silvering, the tutorial's, a
//   guild's registration, the castle's tax and its rate, the advancement
//   swap, the campaign and event item dialogs, a couple's name, the
//   lottery, the skill downgrade), or the stash (asking the server for its
//   list when there is none yet);
// - pops up a message, plain, with the player's or the guild's name, by
//   the player's race, by a couple's or a quest's reason;
// - closes a talk, a repair, a silver coating or a stash purchase, with
//   their mode, money, sounds and items;
// - heals with a sound by race, takes balls from the inventory, and
//   updates the castle's tax balance.
namespace {

std::vector<std::string> calls;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
Race race = RACE_SLAYER;
bool ousters = false;
MItemManager* playerGear = nullptr;
int me;

std::string N(long long value) { return std::to_string(value); }
std::string Text(int id) { const char* s = (*g_pGameStringTable)[id].GetString(); return s ? s : ""; }
std::string Popup(int id) { return "popup|" + Text(id); }

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	bool m_Charge = false;
	Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id, TYPE_ITEMTYPE type = 0) : m_Class(itemClass)
	{
		SetID(id);
		SetItemType(type);
	}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsPileItem() const override { return m_Class == ITEM_CLASS_EVENT_STAR; }
	TYPE_ITEM_NUMBER GetMaxNumber() const override { return m_Charge ? 5 : 50; }
	bool IsChargeItem() const override { return m_Charge; }
	int GetMaxDurability() const override { return 100; }
	int GetMinDamage() const override { return 40; }
	int GetMaxDamage() const override { return 90; }
};

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return reinterpret_cast<MCreature*>(&me); };
	host.GetID = []() { return TYPE_OBJECTID(7); };
	host.GetRace = []() { return race; };
	host.GetGear = []() { return playerGear; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	host.SetWaitVerify = [](PlayerState::WAIT_VERIFY verify) { calls.push_back("waits|" + N(verify)); waitVerify = verify; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsOusters = [](MCreature*) { return ousters; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupFreeMessageDlg = [](const char* text) { calls.push_back(std::string("popup|") + (text ? text : "")); };
	host.ShowPCTalkDlg = []() { calls.push_back("talk shown"); };
	host.ClosePCTalkDlg = []() { calls.push_back("talk closed"); };
	host.UnSetLockInputPCTalk = []() { calls.push_back("talk unlocked"); };
	host.RunStorage = []() { calls.push_back("stash opened"); };
	host.SetStorage = [](MStorage* pStorage) { calls.push_back(pStorage == g_pStorage ? "stash set" : "stash set|other"); };
	host.UnlockItemTrade = []() { calls.push_back("trade unlocked"); };
	return host;
}();

const NPCPresentation::Host npc = []() {
	NPCPresentation::Host host{};
	host.OpenInventoryToRepair = []() { calls.push_back("repair window"); };
	host.RunSelectWayPoint = []() { calls.push_back("waypoints"); };
	host.OpenInventoryToSilvering = []() { calls.push_back("silvering window"); };
	host.RunTutorialExit = []() { calls.push_back("tutorial exit"); };
	host.RunComputer = []() { calls.push_back("computer"); };
	host.RunBriefing = []() { calls.push_back("briefing"); };
	host.RunBookcase = []() { calls.push_back("bookcase"); };
	host.SetCharInfoSilverDamage = [](int minDamage, int maxDamage) { calls.push_back("silver damage|" + N(minDamage) + "-" + N(maxDamage)); };
	host.ShowGuildRegist = [](std::uint32_t fee) { calls.push_back("guild regist|" + N(fee)); };
	host.OpenBringFee = [](std::uint32_t fee) { calls.push_back("bring fee|" + N(fee)); };
	host.SetTotalFee = [](std::uint32_t fee) { calls.push_back("total fee|" + N(fee)); };
	host.RunInputNameWindow = [](int id) { calls.push_back("name input|" + N(id)); };
	host.CloseInputNameWindow = []() { calls.push_back("name input closed"); };
	host.InitQuestStatus = []() { calls.push_back("quest cleared"); };
	host.RunLottery = [](int step) { calls.push_back("lottery|" + N(step)); };
	host.DownSkill = []() { calls.push_back("down skill"); };
	host.RunModifyTax = []() { calls.push_back("tax rate"); };
	host.ShowSwapAdvancementItem = []() { calls.push_back("swap advancement"); };
	host.RunCampaignHelp = [](int value) { calls.push_back("campaign|" + N(value)); };
	host.RunConfirmGetEventItem = [](int value) { calls.push_back("confirm event item|" + N(value)); };
	host.PopupComebackNotice = [](int which) { calls.push_back("comeback|" + N(which)); };
	host.PopupMessage = [](int id) { calls.push_back("string popup|" + N(id)); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

const BonusSkills::Host skills = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { calls.push_back("skills refreshed"); };
	return host;
}();

// The game connection, its output kept unsent for inspection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

constexpr TYPE_SOUNDID kInventorySound = 55;

struct World : GameModelWorld
{
	TempInformation temp;
	MMoneyManager money;
	MInventory inventory;
	MSlayerGear gear;
	GamePlayer connection;
	TempInformation* oldTemp = g_pTempInformation;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldGear = g_pSlayerGear;
	MStorage* oldStorage = g_pStorage;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const NPCPresentation::Host* oldNpc = NPCPresentation::SetHost(&npc);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const BonusSkills::Host* oldSkills = BonusSkills::SetHost(&skills);
	World()
	{
		InitGameStringTable();
		calls.clear();
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		race = RACE_SLAYER;
		ousters = false;
		for (ITEM_CLASS itemClass : {ITEM_CLASS_SWORD, ITEM_CLASS_COAT, ITEM_CLASS_EVENT_STAR, ITEM_CLASS_SLAYER_PORTAL_ITEM})
		{
			g_pItemTable->InitClass(itemClass, 8);
			for (int type = 0; type < 8; type++)
			{
				ITEMTABLE_INFO& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
				row.SetGrid(1, 1);
				row.InventorySoundID = kInventorySound + itemClass;
				row.SilverMax = 300 + type;
			}
		}
		inventory.Init(4, 4);
		gear.Init();
		g_pTempInformation = &temp;
		g_pMoneyManager = &money;
		g_pInventory = &inventory;
		g_pSlayerGear = &gear;
		g_pStorage = nullptr;
		playerGear = &gear;
		g_pUserInformation->CharacterID = "Kim";
		g_pUserInformation->GuildName = "Wolves";
	}
	~World()
	{
		BonusSkills::SetHost(oldSkills);
		WorldEvents::SetHost(oldEvents);
		NPCPresentation::SetHost(oldNpc);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		delete g_pStorage;
		g_pStorage = oldStorage;
		g_pSlayerGear = oldGear;
		g_pInventory = oldInventory;
		g_pMoneyManager = oldMoney;
		g_pTempInformation = oldTemp;
		inventory.Release();
		playerGear = nullptr;
	}
	void Receive(int code, unsigned parameter = 0)
	{
		GCNPCResponse packet;
		packet.setCode(static_cast<WORD>(code));
		packet.setParameter(parameter);
		GCNPCResponseHandler::execute(&packet, &connection);
	}
	// An item in the gear's slot, put there directly.
	Ware* Wearing(ITEM_CLASS itemClass, TYPE_OBJECTID id, int slot)
	{
		Ware* pItem = new Ware(itemClass, id);
		pItem->SetCurrentDurability(10);
		CHECK(static_cast<MSlotItemManager&>(gear).AddItem(pItem, static_cast<BYTE>(slot)));
		return pItem;
	}
};

std::string Formatted(int id, const char* name)
{
	char text[512];
	SafeFormat::Format(text, GetGameString(id), name);
	return std::string("popup|") + text;
}

} // namespace

TEST(NPCResponseReplies, APlayerWaitingOnTheNPCStopsWaiting)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	world.Receive(NPC_RESPONSE_INTERFACE_REPAIR);
	CHECK(calls == (std::vector<std::string>{"verified", "repair window"}));
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	world.Receive(NPC_RESPONSE_INTERFACE_REPAIR);
	CHECK(calls == std::vector<std::string>{"repair window"});
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_TRADE);
	// Quitting the dialogue lets the player act again and closes the talk.
	calls.clear();
	world.Receive(NPC_RESPONSE_QUIT_DIALOGUE);
	CHECK(calls == (std::vector<std::string>{"talk unlocked", "verified", "talk closed"}));
}

TEST(NPCResponseReplies, WindowsOpen)
{
	World world;
	const std::pair<int, std::string> windows[] = {
		{NPC_RESPONSE_INTERFACE_HELICOPTER, "waypoints"}, {NPC_RESPONSE_INTERFACE_SILVER_COATING, "silvering window"},
		{NPC_RESPONSE_INTERFACE_NEWBIE_ITEM, "tutorial exit"}, {NPC_RESPONSE_INTERFACE_TUTORIAL_COMPUTER, "computer"},
		{NPC_RESPONSE_INTERFACE_TUTORIAL_BRIEFING, "briefing"}, {NPC_RESPONSE_INTERFACE_TUTORIAL_BOOKCASE, "bookcase"},
		{NPC_RESPONSE_GUILD_SHOW_REGIST, "guild regist|42"}, {NPC_RESPONSE_SHOW_TAX_BALANCE, "bring fee|42"},
		{NPC_RESPONSE_SHOW_TAX_RATIO, "tax rate"}, {NPC_RESPONSE_SWAP_ADVANCEMENT_ITEM, "swap advancement"},
		{NPC_RESPONSE_SHOW_DONATION_DAILOG, "campaign|42"}, {NPC_RESPONSE_SHOW_CONFIRM_GET_EVENT_ITEM_DIALOG, "confirm event item|42"},
		{NPC_RESPONSE_DOWN_SKILL, "down skill"}, {NPC_RESPONSE_WAIT_FOR_MEET_COUPLE, "name input|0"},
		{NPC_RESPONSE_WAIT_FOR_APART_COUPLE, "name input|1"}, {NPC_RESPONSE_APART_COUPLE_FORCE, "name input|2"}};
	for (const auto& [code, call] : windows)
	{
		calls.clear();
		world.Receive(code, 42);
		CHECK(calls == std::vector<std::string>{call});
	}
	// Codes with nothing to show.
	calls.clear();
	for (int code : {static_cast<int>(NPC_RESPONSE_INTERFACE_CREATE_GUILD), static_cast<int>(NPC_RESPONSE_INTERFACE_DESTROY_GUILD),
		static_cast<int>(NPC_RESPONSE_GUILD_SHOW_STARTING_JOIN), static_cast<int>(NPC_RESPONSE_GUILD_SHOW_JOIN),
		static_cast<int>(NPC_RESPONSE_GUILD_SHOW_QUIT), static_cast<int>(NPC_RESPONSE_GUILD_ERROR),
		static_cast<int>(NPC_RESPONSE_TRADE_GIFT_BOX_ERROR), 9999})
		world.Receive(code, 42);
	CHECK(calls.empty());
}

TEST(NPCResponseReplies, PlainMessagesPopUp)
{
	World world;
	const std::pair<int, int> dialogs[] = {
	{NPC_RESPONSE_DONATION_FAIL_MONEY, STRING_MESSAGE_DONATION_FAIL},
	{NPC_RESPONSE_TEAM_REGIST_FAIL_QUIT_TIMEOUT, STRING_MESSAGE_TEAM_REGIST_FAIL_QUIT_TIMEOUT},
	{NPC_RESPONSE_TEAM_REGIST_FAIL_CANCEL_TIMEOUT, STRING_MESSAGE_TEAM_REGIST_FAIL_CANCEL_TIMEOUT},
	{NPC_RESPONSE_TEAM_REGIST_FAIL_LEVEL, STRING_MESSAGE_TEAM_REGIST_FAIL_LEVEL},
	{NPC_RESPONSE_TEAM_REGIST_FAIL_MONEY, STRING_MESSAGE_TEAM_REGIST_FAIL_MONEY},
	{NPC_RESPONSE_TEAM_REGIST_FAIL_NAME, STRING_MESSAGE_TEAM_REGIST_FAIL_NAME},
	{NPC_RESPONSE_TEAM_REGIST_FAIL_DENY, STRING_MESSAGE_TEAM_REGIST_FAIL_DENY},
	{NPC_RESPONSE_TEAM_STARTING_FAIL_ALREADY_JOIN, STRING_MESSAGE_TEAM_STARTING_FAIL_ALREADY_JOIN},
	{NPC_RESPONSE_TEAM_STARTING_FAIL_QUIT_TIMEOUT, STRING_MESSAGE_TEAM_STARTING_FAIL_QUIT_TIMEOUT},
	{NPC_RESPONSE_TEAM_STARTING_FAIL_CANCEL_TIMEOUT, STRING_MESSAGE_TEAM_STARTING_FAIL_CANCEL_TIMEOUT},
	{NPC_RESPONSE_TEAM_STARTING_FAIL_LEVEL, STRING_MESSAGE_TEAM_STARTING_FAIL_LEVEL},
	{NPC_RESPONSE_TEAM_STARTING_FAIL_DENY, STRING_MESSAGE_TEAM_STARTING_FAIL_DENY},
	{NPC_RESPONSE_CLAN_REGIST_FAIL_QUIT_TIMEOUT, STRING_MESSAGE_CLAN_REGIST_FAIL_QUIT_TIMEOUT},
	{NPC_RESPONSE_CLAN_REGIST_FAIL_CANCEL_TIMEOUT, STRING_MESSAGE_CLAN_REGIST_FAIL_CANCEL_TIMEOUT},
	{NPC_RESPONSE_CLAN_REGIST_FAIL_LEVEL, STRING_MESSAGE_CLAN_REGIST_FAIL_LEVEL},
	{NPC_RESPONSE_CLAN_REGIST_FAIL_MONEY, STRING_MESSAGE_CLAN_REGIST_FAIL_MONEY},
	{NPC_RESPONSE_CLAN_REGIST_FAIL_NAME, STRING_MESSAGE_CLAN_REGIST_FAIL_NAME},
	{NPC_RESPONSE_CLAN_REGIST_FAIL_DENY, STRING_MESSAGE_CLAN_REGIST_FAIL_DENY},
	{NPC_RESPONSE_CLAN_STARTING_FAIL_ALREADY_JOIN, STRING_MESSAGE_CLAN_STARTING_FAIL_ALREADY_JOIN},
	{NPC_RESPONSE_CLAN_STARTING_FAIL_QUIT_TIMEOUT, STRING_MESSAGE_CLAN_STARTING_FAIL_QUIT_TIMEOUT},
	{NPC_RESPONSE_CLAN_STARTING_FAIL_CANCEL_TIMEOUT, STRING_MESSAGE_CLAN_STARTING_FAIL_CANCEL_TIMEOUT},
	{NPC_RESPONSE_CLAN_STARTING_FAIL_LEVEL, STRING_MESSAGE_CLAN_STARTING_FAIL_LEVEL},
	{NPC_RESPONSE_CLAN_STARTING_FAIL_MONEY, STRING_MESSAGE_CLAN_STARTING_FAIL_MONEY},
	{NPC_RESPONSE_CLAN_STARTING_FAIL_DENY, STRING_MESSAGE_CLAN_STARTING_FAIL_DENY},
	{NPC_RESPONSE_TRADE_GIFT_BOX_OK, STRING_MESSAGE_TRADE_GIFT_BOX_OK},
	{NPC_RESPONSE_TRADE_GIFT_BOX_NO_ITEM, STRING_MESSAGE_TRADE_GIFT_BOX_NO_ITEM},
	{NPC_RESPONSE_TRADE_GIFT_BOX_ALREADY_TRADE, STRING_MESSAGE_TRADE_GIFT_BOX_ALREADY_TRADE},
	{NPC_RESPONSE_REWARD_OK, STRING_MESSAGE_REWARD_OK},
	{NPC_RESPONSE_REWARD_FAIL, STRING_MESSAGE_REWARD_FAIL},
	{NPC_RESPONSE_NO_EMPTY_SLOT, STRING_MESSAGE_NO_EMPTY_SLOT},
	{NPC_RESPONSE_NOT_ENOUGH_MONEY, STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY},
	{NPC_RESPONSE_WAR_SCHEDULE_FULL, STRING_MESSAGE_WAR_SCHEDULE_FULL},
	{NPC_RESPONSE_WAR_ALREADY_REGISTERED, STRING_MESSAGE_WAR_ALREADY_REGISTERED},
	{NPC_RESPONSE_WAR_REGISTRATION_OK, STRING_MESSAGE_WAR_REGISTRATION_OK},
	{NPC_RESPONSE_ALREADY_HAS_CASTLE, STRING_MESSAGE_ALREADY_HAS_CASTLE},
	{NPC_RESPONSE_WAR_UNAVAILABLE, STRING_MESSAGE_WAR_UNAVAILABLE},
	{NPC_RESPONSE_RACE_WAR_JOIN_OK, STRING_MESSAGE_RACE_WAR_JOIN_OK},
	{NPC_RESPONSE_RACE_WAR_GO_FIRST_SERVER, STRING_MESSAGE_RACE_WAR_GO_FIRST_SERVER},
	{NPC_RESPONSE_GIVE_EVENT_ITEM_FAIL_NOW, STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL_NOW},
	{NPC_RESPONSE_GIVE_EVENT_ITEM_FAIL, STRING_MESSAGE_GIVE_EVENT_ITEM_FAIL},
	{NPC_RESPONSE_GIVE_EVENT_ITEM_OK, STRING_MESSAGE_GIVE_EVENT_ITEM_OK},
	{NPC_RESPONSE_GIVE_PREMIUM_USER_ONLY, STRING_MESSAGE_GIVE_PREMIUM_USER_ONLY},
	{NPC_RESPONSE_CANNOT_BUY, STRING_MESSAGE_CANNOT_TAKE_OUT_ITEM_FROM_SHOP},
	{NPC_RESPONSE_CLEAR_RANK_BONUS_OK, STRING_MESSAGE_CLEAR_RANK_BONUS_OK},
	{NPC_RESPONSE_NO_RANK_BONUS, STRING_MESSAGE_NO_RANK_BONUS},
	{NPC_RESPONSE_ALREADY_CLEAR_RANK_BONUS, STRING_MESSAGE_ALREADY_CLEAR_RANK_BONUS},
	};
	for (const auto& [code, string] : dialogs)
	{
		calls.clear();
		world.Receive(code);
		CHECK(calls == std::vector<std::string>{Popup(string)});
	}
	const std::pair<int, int> strings[] = {
	{NPC_RESPONSE_GUILD_REGIST_FAIL_QUIT_TIMEOUT, STRING_MESSAGE_GUILD_REGIST_FAIL_QUIT_TIMEOUT},
	{NPC_RESPONSE_GUILD_REGIST_FAIL_CANCEL_TIMEOUT, STRING_MESSAGE_GUILD_REGIST_FAIL_CANCEL_TIMEOUT},
	{NPC_RESPONSE_GUILD_REGIST_FAIL_LEVEL, STRING_MESSAGE_GUILD_REGIST_FAIL_LEVEL},
	{NPC_RESPONSE_GUILD_REGIST_FAIL_MONEY, STRING_MESSAGE_GUILD_REGIST_FAIL_MONEY},
	{NPC_RESPONSE_GUILD_REGIST_FAIL_NAME, STRING_MESSAGE_GUILD_REGIST_FAIL_NAME},
	{NPC_RESPONSE_GUILD_REGIST_FAIL_DENY, STRING_MESSAGE_GUILD_REGIST_FAIL_DENY},
	{NPC_RESPONSE_GUILD_STARTING_FAIL_ALREADY_JOIN, STRING_MESSAGE_GUILD_STARTING_FAIL_ALREADY_JOIN},
	{NPC_RESPONSE_GUILD_STARTING_FAIL_QUIT_TIMEOUT, STRING_MESSAGE_GUILD_STARTING_FAIL_QUIT_TIMEOUT},
	{NPC_RESPONSE_GUILD_STARTING_FAIL_CANCEL_TIMEOUT, STRING_MESSAGE_GUILD_STARTING_FAIL_CANCEL_TIMEOUT},
	{NPC_RESPONSE_GUILD_STARTING_FAIL_LEVEL, STRING_MESSAGE_GUILD_STARTING_FAIL_LEVEL},
	{NPC_RESPONSE_GUILD_STARTING_FAIL_DENY, STRING_MESSAGE_GUILD_REGIST_FAIL_DENY},
	{NPC_RESPONSE_TOO_MANY_GUILD_REGISTERED, STRING_MESSAGE_TOO_MANY_GUILD_REGISTERED},
	{NPC_RESPONSE_REINFORCE_DENYED, STRING_MESSAGE_REINFORCE_DENYED},
	{NPC_RESPONSE_ALREADY_REINFORCE_ACCEPTED, STRING_MESSAGE_ALREADY_REINFORCE_ACCEPTED},
	{NPC_RESPONSE_NO_WAR_REGISTERED, STRING_MESSAGE_NO_WAR_REGISTERED},
	{NPC_RESPONSE_CANNOT_ACCEPT, STRING_MESSAGE_CANNOT_ACCEPT},
	{NPC_RESPONSE_ACCEPT_OK, STRING_MESSAGE_ACCEPT_OK},
	{NPC_RESPONSE_CANNOT_DENY, STRING_MESSAGE_CANNOT_DENY},
	{NPC_RESPONSE_DENY_OK, STRING_MESSAGE_DENY_OK},
	{NPC_RESPONSE_MODIFY_TAX_RATIO_OK, UI_STRING_MESSAGE_MODIFY_TAX_OK},
	{NPC_RESPONSE_MODIFY_TAX_RATIO_FAIL, UI_STRING_MESSAGE_MODIFY_TAX_FAIL},
	{NPC_RESPONSE_NOT_ADVANCED, UI_STRING_MESSAGE_SWAP_ERROR},
	{NPC_RESPONSE_SHOW_DONATION_COMPLETE_DAILOG, UI_STRING_MESSAGE_CAMPAIGN_HELP_THANKS},
	};
	for (const auto& [code, string] : strings)
	{
		calls.clear();
		world.Receive(code);
		CHECK(calls == std::vector<std::string>{"string popup|" + N(string)});
	}
}

TEST(NPCResponseReplies, NamedMessagesCarryThePlayersOrTheGuildsName)
{
	World world;
	// Some English texts leave the name out; these say where it goes.
	for (int string : {STRING_MESSAGE_TEAM_REGIST_FAIL_ALREADY_JOIN, STRING_MESSAGE_CLAN_REGIST_FAIL_ALREADY_JOIN,
		STRING_MESSAGE_GUILD_REGIST_FAIL_ALREADY_JOIN, STRING_MESSAGE_TEAM_REGIST_FAIL_FAME, STRING_MESSAGE_TEAM_STARTING_FAIL_MONEY,
		STRING_MESSAGE_TEAM_STARTING_FAIL_FAME, STRING_MESSAGE_CLAN_REGIST_FAIL_FAME, STRING_MESSAGE_CLAN_STARTING_FAIL_FAME,
		STRING_MESSAGE_RACE_WAR_JOIN_FAILED, STRING_MESSAGE_GUILD_REGIST_FAIL_FAME, STRING_MESSAGE_GUILD_STARTING_FAIL_MONEY,
		STRING_MESSAGE_GUILD_STARTING_FAIL_FAME})
		g_pGameStringTable->Set(string, ("[" + N(string) + ": %s]").c_str());
	const std::pair<int, int> byGuild[] = {{NPC_RESPONSE_TEAM_REGIST_FAIL_ALREADY_JOIN, STRING_MESSAGE_TEAM_REGIST_FAIL_ALREADY_JOIN},
		{NPC_RESPONSE_CLAN_REGIST_FAIL_ALREADY_JOIN, STRING_MESSAGE_CLAN_REGIST_FAIL_ALREADY_JOIN},
		{NPC_RESPONSE_GUILD_REGIST_FAIL_ALREADY_JOIN, STRING_MESSAGE_GUILD_REGIST_FAIL_ALREADY_JOIN}};
	for (const auto& [code, string] : byGuild)
	{
		calls.clear();
		world.Receive(code);
		CHECK(calls == std::vector<std::string>{Formatted(string, "Wolves")});
	}
	const std::pair<int, int> byPlayer[] = {{NPC_RESPONSE_TEAM_REGIST_FAIL_FAME, STRING_MESSAGE_TEAM_REGIST_FAIL_FAME},
		{NPC_RESPONSE_TEAM_STARTING_FAIL_MONEY, STRING_MESSAGE_TEAM_STARTING_FAIL_MONEY},
		{NPC_RESPONSE_TEAM_STARTING_FAIL_FAME, STRING_MESSAGE_TEAM_STARTING_FAIL_FAME},
		{NPC_RESPONSE_CLAN_REGIST_FAIL_FAME, STRING_MESSAGE_CLAN_REGIST_FAIL_FAME},
		{NPC_RESPONSE_CLAN_STARTING_FAIL_FAME, STRING_MESSAGE_CLAN_STARTING_FAIL_FAME},
		{NPC_RESPONSE_RACE_WAR_JOIN_FAILED, STRING_MESSAGE_RACE_WAR_JOIN_FAILED},
		{NPC_RESPONSE_GUILD_REGIST_FAIL_FAME, STRING_MESSAGE_GUILD_REGIST_FAIL_FAME},
		{NPC_RESPONSE_GUILD_STARTING_FAIL_MONEY, STRING_MESSAGE_GUILD_STARTING_FAIL_MONEY},
		{NPC_RESPONSE_GUILD_STARTING_FAIL_FAME, STRING_MESSAGE_GUILD_STARTING_FAIL_FAME}};
	for (const auto& [code, string] : byPlayer)
	{
		calls.clear();
		world.Receive(code);
		CHECK(calls == std::vector<std::string>{Formatted(string, "Kim")});
	}
}

TEST(NPCResponseReplies, GuildMessagesFollowTheRace)
{
	World world;
	const std::pair<int, std::pair<int, int>> byRace[] = {{NPC_RESPONSE_NO_GUILD, {STRING_MESSAGE_NO_TEAM, STRING_MESSAGE_NO_CLAN}},
		{NPC_RESPONSE_NOT_GUILD_MASTER, {STRING_MESSAGE_NOT_TEAM_MASTER, STRING_MESSAGE_NOT_CLAN_MASTER}},
		{NPC_RESPONSE_HAS_NO_CASTLE, {STRING_MESSAGE_TEAM_HAS_NO_CASTLE, STRING_MESSAGE_CLAN_HAS_NO_CASTLE}},
		{NPC_RESPONSE_NOT_YOUR_CASTLE, {STRING_MESSAGE_TEAM_NOT_YOUR_CASTLE, STRING_MESSAGE_CLAN_NOT_YOUR_CASTLE}}};
	for (const auto& [code, strings] : byRace)
	{
		for (Race who : {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS})
		{
			calls.clear();
			race = who;
			world.Receive(code);
			CHECK(calls == std::vector<std::string>{Popup(who == RACE_SLAYER ? strings.first : strings.second)});
		}
	}
}

TEST(NPCResponseReplies, TheStashOpensOrIsAskedFor)
{
	World world;
	world.Receive(NPC_RESPONSE_INTERFACE_STASHOPEN);
	CGStashList asked;
	asked.setObjectID(7);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(&asked);
	CHECK(world.connection.Sent() == SocketOutputStreamTestAccess::Bytes(encoded));
	CHECK(calls.empty());
	g_pStorage = new MStorage;
	world.Receive(NPC_RESPONSE_INTERFACE_STASHOPEN);
	CHECK(calls == (std::vector<std::string>{"stash opened", "stash set"}));
}

TEST(NPCResponseReplies, ARepairMendsTheItemAndSetsTheMoney)
{
	World world;
	Ware* pItem = new Ware(ITEM_CLASS_COAT, 2001);
	pItem->SetCurrentDurability(10);
	world.inventory.AddItem(pItem, 0, 0);
	world.temp.SetMode(TempInformation::MODE_SHOP_REPAIR);
	world.temp.pValue = pItem;
	world.temp.Value1 = TRUE;
	world.temp.Value2 = FALSE;
	world.Receive(NPC_RESPONSE_REPAIR_OK, 900);
	CHECK(calls == (std::vector<std::string>{"sound|" + N(kInventorySound + ITEM_CLASS_COAT), "trade unlocked"}));
	CHECK(pItem->GetCurrentDurability() == 100 && world.money.GetMoney() == 900);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// A charge item is filled, and the skills follow.
	calls.clear();
	Ware* pCharge = new Ware(ITEM_CLASS_SLAYER_PORTAL_ITEM, 2002);
	pCharge->m_Charge = true;
	pCharge->SetNumber(1);
	world.inventory.AddItem(pCharge, 1, 0);
	MSkillSet available;
	MSkillSet* oldAvailable = g_pSkillAvailable;
	g_pSkillAvailable = &available;
	world.temp.SetMode(TempInformation::MODE_SHOP_REPAIR);
	world.temp.pValue = pCharge;
	world.Receive(NPC_RESPONSE_REPAIR_OK, 800);
	g_pSkillAvailable = oldAvailable;
	CHECK(calls == (std::vector<std::string>{"skills refreshed", "sound|" + N(kInventorySound + ITEM_CLASS_SLAYER_PORTAL_ITEM), "trade unlocked"}));
	CHECK(pCharge->GetNumber() == 5 && world.money.GetMoney() == 800);
	// Not repairing: nothing.
	calls.clear();
	world.Receive(NPC_RESPONSE_REPAIR_OK, 700);
	CHECK(calls.empty() && world.money.GetMoney() == 800);
}

TEST(NPCResponseReplies, RepairingAllMendsTheGear)
{
	World world;
	Ware* pSword = world.Wearing(ITEM_CLASS_SWORD, 2001, MSlayerGear::GEAR_SLAYER_RIGHTHAND);
	Ware* pCoat = world.Wearing(ITEM_CLASS_COAT, 2002, MSlayerGear::GEAR_SLAYER_COAT);
	world.temp.SetMode(TempInformation::MODE_SHOP_REPAIR);
	world.temp.pValue = nullptr;
	world.temp.Value1 = FALSE;
	world.temp.Value2 = TRUE;
	world.Receive(NPC_RESPONSE_REPAIR_OK, 600);
	CHECK(pSword->GetCurrentDurability() == 100 && pCoat->GetCurrentDurability() == 100);
	CHECK(world.money.GetMoney() == 600);
	CHECK(calls.size() == 2 && calls[0].rfind("sound|", 0) == 0 && calls[1] == "trade unlocked");
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(NPCResponseReplies, FailedRepairsAndSilveringsUnlockTheTrade)
{
	World world;
	for (int code : {NPC_RESPONSE_REPAIR_FAIL_ITEM_NOT_EXIST, NPC_RESPONSE_REPAIR_FAIL_ITEM_TYPE, NPC_RESPONSE_REPAIR_FAIL_MONEY})
	{
		calls.clear();
		world.temp.SetMode(TempInformation::MODE_SHOP_REPAIR);
		world.Receive(code);
		CHECK(calls == (std::vector<std::string>{"trade unlocked", Popup(STRING_MESSAGE_CANNOT_REPAIR)}));
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	}
	for (int code : {NPC_RESPONSE_SILVER_COATING_FAIL_ITEM_NOT_EXIST, NPC_RESPONSE_SILVER_COATING_FAIL_ITEM_TYPE, NPC_RESPONSE_SILVER_COATING_FAIL_MONEY})
	{
		calls.clear();
		world.temp.SetMode(TempInformation::MODE_SHOP_SILVERING);
		world.Receive(code);
		CHECK(calls == (std::vector<std::string>{"trade unlocked", Popup(STRING_MESSAGE_CANNOT_SILVERING)}));
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	}
}

TEST(NPCResponseReplies, ASilverCoatingFillsTheSilverAndTheWeaponsDamage)
{
	World world;
	Ware* pSword = world.Wearing(ITEM_CLASS_SWORD, 2001, MSlayerGear::GEAR_SLAYER_RIGHTHAND);
	pSword->SetItemType(3);
	pSword->SetAffectStatus();
	world.temp.SetMode(TempInformation::MODE_SHOP_SILVERING);
	world.temp.pValue = pSword;
	world.Receive(NPC_RESPONSE_SILVER_COATING_OK, 500);
	CHECK(calls == (std::vector<std::string>{"silver damage|4-9", "sound|" + N(kInventorySound + ITEM_CLASS_SWORD), "trade unlocked"}));
	CHECK(pSword->GetSilver() == 303 && world.money.GetMoney() == 500);
	// An item that is not the weapon held, or not usable: no damage shown.
	calls.clear();
	Ware* pOther = new Ware(ITEM_CLASS_SWORD, 2002);
	pOther->SetAffectStatus();
	world.inventory.AddItem(pOther, 0, 0);
	world.temp.SetMode(TempInformation::MODE_SHOP_SILVERING);
	world.temp.pValue = pOther;
	world.Receive(NPC_RESPONSE_SILVER_COATING_OK, 400);
	pSword->UnSetAffectStatus();
	world.temp.SetMode(TempInformation::MODE_SHOP_SILVERING);
	world.temp.pValue = pSword;
	world.Receive(NPC_RESPONSE_SILVER_COATING_OK, 300);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("silver damage", 0) == 0; }));
	CHECK(pOther->GetSilver() == 300);
	// Not silvering: nothing.
	calls.clear();
	world.Receive(NPC_RESPONSE_SILVER_COATING_OK, 200);
	CHECK(calls.empty() && world.money.GetMoney() == 300);
}

TEST(NPCResponseReplies, BuyingAStashCostsAndShowsTheTalkAgain)
{
	World world;
	world.money.SetMoney(1000);
	g_pStorage = new MStorage;
	world.temp.SetMode(TempInformation::MODE_STORAGE_BUY_WAIT);
	world.temp.Value1 = 300;
	world.Receive(NPC_RESPONSE_STASH_SELL_OK);
	CHECK(calls == (std::vector<std::string>{"talk shown", Popup(STRING_MESSAGE_STORAGE_BUY)}));
	CHECK(g_pStorage == nullptr && world.money.GetMoney() == 700 && world.temp.GetMode() == TempInformation::MODE_NULL);
	for (int code : {NPC_RESPONSE_STASH_SELL_FAIL_MAX, NPC_RESPONSE_STASH_SELL_FAIL_MONEY})
	{
		calls.clear();
		world.temp.SetMode(TempInformation::MODE_STORAGE_BUY_WAIT);
		world.Receive(code);
		CHECK(calls == (std::vector<std::string>{"talk shown", Popup(STRING_MESSAGE_CANNOT_BUY_MORE)}));
	}
	// Not buying: nothing.
	calls.clear();
	world.Receive(NPC_RESPONSE_STASH_SELL_OK);
	world.Receive(NPC_RESPONSE_STASH_SELL_FAIL_MAX);
	CHECK(calls.empty() && world.money.GetMoney() == 700);
}

TEST(NPCResponseReplies, HealingSoundsByRaceAndDonationsSetTheMoney)
{
	World world;
	world.Receive(NPC_RESPONSE_HEAL);
	race = RACE_OUSTERS;
	ousters = true;
	world.Receive(NPC_RESPONSE_HEAL);
	race = RACE_VAMPIRE;
	ousters = false;
	world.Receive(NPC_RESPONSE_HEAL);
	CHECK(calls == (std::vector<std::string>{"sound|" + N(SOUND_ITEM_USE_HPOTION), "sound|" + N(SOUND_OUSTERS_PUPA)}));
	calls.clear();
	world.Receive(NPC_RESPONSE_DONATION_OK, 250);
	CHECK(calls == std::vector<std::string>{Popup(STRING_MESSAGE_DONATION_OK)});
	CHECK(world.money.GetMoney() == 250);
}

TEST(NPCResponseReplies, BallsAreTakenFromTheInventory)
{
	World world;
	Ware* pFirst = new Ware(ITEM_CLASS_EVENT_STAR, 2001, 6);
	pFirst->SetNumber(3);
	world.inventory.AddItem(pFirst, 0, 0);
	Ware* pSecond = new Ware(ITEM_CLASS_EVENT_STAR, 2002, 6);
	pSecond->SetNumber(4);
	world.inventory.AddItem(pSecond, 1, 0);
	Ware* pStar = new Ware(ITEM_CLASS_EVENT_STAR, 2003, 5);
	pStar->SetNumber(9);
	world.inventory.AddItem(pStar, 2, 0);
	world.Receive(NPC_RESPONSE_DECREASE_BALL, 5);
	CHECK(world.inventory.GetItem(0, 0) == nullptr);
	CHECK(world.inventory.GetItem(1, 0) == pSecond && pSecond->GetNumber() == 2);
	CHECK(pStar->GetNumber() == 9);
	// More than there are: all go, the rest is let be.
	world.Receive(NPC_RESPONSE_DECREASE_BALL, 10);
	CHECK(world.inventory.GetItem(1, 0) == nullptr && world.inventory.GetItem(2, 0) == pStar);
	// Exactly a stack's worth takes the stack out.
	Ware* pExact = new Ware(ITEM_CLASS_EVENT_STAR, 2004, 6);
	pExact->SetNumber(3);
	world.inventory.AddItem(pExact, 3, 0);
	world.Receive(NPC_RESPONSE_DECREASE_BALL, 3);
	CHECK(world.inventory.GetItem(3, 0) == nullptr);
}

TEST(NPCResponseReplies, TheCastlesTaxIsWithdrawn)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_WAIT_BRING_FEE);
	world.Receive(NPC_RESPONSE_WITHDRAW_TAX_OK, 777);
	CHECK(calls == (std::vector<std::string>{"total fee|777", Popup(STRING_MESSAGE_SUCCESS_BRING_FEE)}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_WAIT_BRING_FEE);
	world.Receive(NPC_RESPONSE_WITHDRAW_TAX_FAIL);
	CHECK(calls == std::vector<std::string>{Popup(STRING_MESSAGE_FAIL_BRING_FEE)});
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// Another mode stays.
	world.temp.SetMode(TempInformation::MODE_SHOP_REPAIR);
	world.Receive(NPC_RESPONSE_WITHDRAW_TAX_OK, 1);
	world.Receive(NPC_RESPONSE_WITHDRAW_TAX_FAIL);
	CHECK(world.temp.GetMode() == TempInformation::MODE_SHOP_REPAIR);
}

TEST(NPCResponseReplies, CouplesMeetPartOrSayWhyNot)
{
	World world;
	const std::pair<int, std::vector<std::string>> flows[] = {
		{NPC_RESPONSE_COUPLE_MEET_SUCCESS, {"name input closed", Popup(STRING_MESSAGE_COUPLE_MEET_SUCCESS)}},
		{NPC_RESPONSE_MEET_WAIT_TIME_EXPIRED, {"name input closed", Popup(STRING_MESSAGE_MEET_WAIT_TIME_EXPIRED)}},
		{NPC_RESPONSE_COUPLE_APART_SUCCESS, {"name input closed", Popup(STRING_MESSAGE_COUPLE_APART_SUCCESS), "skills refreshed"}},
		{NPC_RESPONSE_APART_WAIT_TIME_EXPIRED, {"name input closed", Popup(STRING_MESSAGE_MEET_WAIT_TIME_EXPIRED)}}};
	for (const auto& [code, expected] : flows)
	{
		calls.clear();
		world.Receive(code);
		CHECK(calls == expected);
	}
	// A reason (1 to the last) picks its message; none or past it, the
	// general one.
	for (int code : {NPC_RESPONSE_COUPLE_CANNOT_MEET, NPC_RESPONSE_NOT_COUPLE})
	{
		const int general = code == NPC_RESPONSE_COUPLE_CANNOT_MEET ? STRING_MESSAGE_COUPLE_CANNOT_MEET : STRING_MESSAGE_NOT_COUPLE;
		for (unsigned reason : {1u, static_cast<unsigned>(COUPLE_MESSAGE_MAX - 1)})
		{
			calls.clear();
			world.Receive(code, reason);
			CHECK(calls == (std::vector<std::string>{"name input closed", Popup(STRING_MESSAGE_COUPLE_NOT_EVENT_TERM + static_cast<int>(reason) - 1)}));
		}
		for (unsigned reason : {0u, static_cast<unsigned>(COUPLE_MESSAGE_MAX)})
		{
			calls.clear();
			world.Receive(code, reason);
			CHECK(calls == (std::vector<std::string>{"name input closed", Popup(general)}));
		}
	}
}

TEST(NPCResponseReplies, QuestsAnswerByTheirReason)
{
	World world;
	const std::pair<int, std::vector<std::string>> reasons[] = {
		{START_SUCCESS, {Popup(UI_STRING_MESSAGE_START_MONSTER_KILL_QUEST)}},
		{START_FAIL_QUEST_NUM_EXCEEDED, {Popup(UI_STRING_MESSAGE_FAIL_ALREADY_START_MONSTER_KILL_QUEST)}},
		{START_FAIL_DUPLICATED_QUEST_ID, {Popup(UI_STRING_MESSAGE_FAIL_ALREADY_START_MONSTER_KILL_QUEST)}},
		{START_FAIL_PC, {Popup(UI_STRING_MESSAGE_FAIL_MONSTER_KILL_QUEST_BY_STATUS)}},
		{COMPLETE_SUCCESS, {Popup(UI_STRING_MESSAGE_SUCCESS_MONSTER_KILL_QUEST), "quest cleared"}},
		{COMPLETE_FAIL_NOT_COMPLETE, {Popup(UI_STRING_MESSAGE_NOT_YET_COMPLETE_MONSTER_KILL_QUEST)}},
		{COMPLETE_FAIL_NO_INVENTORY_SPACE, {Popup(UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST)}},
		{COMPLETE_FAIL_NOT_IN_QUEST, {Popup(UI_STRING_MESSAGE_NOT_IN_QUEST)}},
		{COMPLETE_FAIL_TIME_EXPIRED, {Popup(UI_STRING_MESSAGE_FAIL_QUEST_EXPIRED_TIME)}},
		{START_FAIL_CANNOT_APPLY_QUEST, {Popup(UI_STRING_MESSAGE_CANNOT_APPLY_QUEST)}},
		{CANCEL_SUCCESS, {"quest cleared", "waits|" + N(PlayerState::WAIT_VERIFY_NPC_ASK)}},
		{CANCEL_NOT_IN_QUEST, {"waits|" + N(PlayerState::WAIT_VERIFY_NPC_ASK)}},
		{COMPLETE_FAIL_INVALID_NPC, {Popup(UI_STRING_MESSAGE_FAIL_INVALID_NPC)}},
		{FAIL_BUG, {Popup(UI_STRING_MESSAGE_FAIL_BUG)}}};
	for (const auto& [reason, expected] : reasons)
	{
		calls.clear();
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		world.Receive(NPC_RESPONSE_QUEST, static_cast<unsigned>(reason));
		CHECK(calls == expected);
	}
}

TEST(NPCResponseReplies, TheLotteryTheGnomesAndTheEventMessages)
{
	World world;
	for (unsigned step : {0u, 4u})
	{
		calls.clear();
		world.Receive(NPC_RESPONSE_LOTTERY, step);
		CHECK(calls == std::vector<std::string>{"lottery|" + N(step + 1)});
	}
	calls.clear();
	world.Receive(NPC_RESPONSE_LOTTERY, 5);
	CHECK(calls.empty());
	world.Receive(NPC_RESPONSE_GNOME_CONTRACT_OK);
	CHECK(calls == (std::vector<std::string>{"talk closed", "string popup|" + N(STRING_MESSAGE_CONTRACT_GNOMES_HORN_OK)}));
	const std::pair<int, std::string> messages[] = {{YOU_CAN_GET_EVENT_200412_COMBACK_ITEM, "comeback|0"},
		{YOU_CAN_GET_EVENT_200412_COMBACK_PREMIUM_ITEM, "comeback|1"}, {YOU_CAN_GET_EVENT_200412_COMBACK_RECOMMEND_ITEM, "comeback|2"},
		{YOU_GET_EVENT_ITEM, "string popup|" + N(UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_OK)},
		{NOT_ENOUGH_INVENTORY_SPACE, "string popup|" + N(UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST)},
		{ALEADY_GET_EVENT_ITEM, "string popup|" + N(UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_ALREADY)},
		{FAIL_GET_EVENT_ITEM, "string popup|" + N(UI_STRING_MESSAGE_GET_EVENT_ITEM_RECEIVE_FAIL)},
		{YOU_ARE_NOT_EVENT_USER, "string popup|" + N(UI_STRING_MESSAGE_GET_EVENT_ITEM_NOT_EVENT_USER)}};
	for (const auto& [value, call] : messages)
	{
		calls.clear();
		world.Receive(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG, static_cast<unsigned>(value));
		CHECK(calls == std::vector<std::string>{call});
	}
}
