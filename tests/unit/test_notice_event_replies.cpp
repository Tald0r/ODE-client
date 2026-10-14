#include "test_framework.h"
#include "gamemodel_world.h"
#include "ClientMode.h"
#include "CMessageArray.h"
#include "GameMessages.h"
#include "GuildWarInfo.h"
#include "MEventQueue.h"
#include "MGameStringTable.h"
#include "MItem.h"
#include "MPriceManager.h"
#include "MWarManager.h"
#include "MZoneTable.h"
#include "NoticePresentationHost.h"
#include "PlayerStateHost.h"
#include "SafeFormat.h"
#include "UserInformation.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCNoticeEvent.h"
#include <string>
#include <vector>

// The server's event notices. They queue and remove timed events (the tax,
// the master's combat, a kick-out, ground attacks, the premium half price,
// the flag war's items, Gilles de Rais's ending, the race war's effect),
// write the system and notice logs, end wars (a war over in the zone the
// player is in restores its safety), and show notice windows, popups and
// a few game windows. A login sets the head price, sends the crash report
// and warns of premium days running out; the crown's price is set.
namespace {

std::vector<std::string> calls;
Race race = RACE_SLAYER;
int zoneID = 60;

std::string N(long long value) { return std::to_string(value); }
std::string Text(int id) { const char* s = (*g_pGameStringTable)[id].GetString(); return s ? s : ""; }

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.GetRace = []() { return race; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return true; };
	host.GetID = []() { return zoneID; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.AddEvent = [](MEvent& event) {
		std::string strings;
		for (int id : event.m_StringsID) strings += "|" + N(id);
		calls.push_back("event|" + N(event.eventID) + "|" + N(event.eventType) + "|" + N(event.eventFlag) + "|" + N(event.eventDelay) + "|"
			+ N(event.parameter1) + "," + N(event.parameter2) + "," + N(event.parameter3) + "|" + N(event.showTime) + "," + N(event.totalTime)
			+ strings);
	};
	host.RemoveEvent = [](int eventID) { calls.push_back("remove|" + N(eventID)); };
	return host;
}();

const NoticePresentation::Host notices = []() {
	NoticePresentation::Host host{};
	host.RunNotice = [](std::uint32_t kind, std::uint32_t parameter) { calls.push_back("notice|" + N(kind) + "|" + N(parameter)); };
	host.DeleteNotice = [](std::uint32_t kind) { calls.push_back("notice gone|" + N(kind)); };
	host.RunWelcomeMessage = []() { calls.push_back("welcome"); };
	host.LotteryResult = [](bool bSuccess) { calls.push_back(std::string("lottery|") + (bSuccess ? "won" : "lost")); };
	host.RunHorn = [](int zone) { calls.push_back("horn|" + N(zone)); };
	host.MasterLairMessage = [](std::uint8_t type, short value, std::uint16_t zone) {
		calls.push_back("lair|" + N(type) + "|" + N(value) + "|" + N(zone));
	};
	host.PopupMessage = [](int id) { calls.push_back("popup|" + N(id)); };
	host.PopupText = [](const char* text, bool bLarge) { calls.push_back(std::string("popup text|") + (bLarge ? "large|" : "normal|") + text); };
	host.RunMinigame = [](int game) { calls.push_back("minigame|" + N(game)); };
	host.CloseCTFStatusWindow = []() { calls.push_back("ctf closed"); };
	host.SendCrashReport = []() { calls.push_back("crash report"); };
	host.SetFlagTo = [](bool bOn) { calls.push_back(std::string("flags|") + (bOn ? "on" : "off")); };
	host.StartQuestEnding = []() { calls.push_back("quest ending"); };
	return host;
}();

std::vector<std::string> Lines(CMessageArray& log)
{
	std::vector<std::string> lines;
	for (int i = 0; i < log.GetSize(); i++)
		if (log[i] != nullptr && log[i][0] != '\0') lines.push_back(log[i]);
	return lines;
}

struct World : GameModelWorld
{
	CMessageArray systemLog, noticeLog;
	MWarManager wars;
	CZoneTable zones;
	MPriceManager prices;
	CMessageArray* oldSystem = g_pSystemMessage;
	CMessageArray* oldNotice = g_pNoticeMessage;
	MWarManager* oldWars = g_pWarManager;
	CZoneTable* oldZones = g_pZoneTable;
	MPriceManager* oldPrices = g_pPriceManager;
	const bool oldSafe = g_bZoneSafe;
	const int oldPremium = g_LeftPremiumDays;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const NoticePresentation::Host* oldNotices = NoticePresentation::SetHost(&notices);
	const MWarHost* oldWarHost = MWarManager::SetHost(nullptr);
	World()
	{
		InitGameStringTable();
		calls.clear();
		race = RACE_SLAYER;
		zoneID = 60;
		systemLog.Init(8, 512);
		// Room for every line a test writes, so a line too many is counted.
		noticeLog.Init(16, 512);
		g_pSystemMessage = &systemLog;
		g_pNoticeMessage = &noticeLog;
		g_pWarManager = &wars;
		ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
		pInfo->ID = 60;
		pInfo->Safety = true;
		CHECK(zones.Add(pInfo));
		g_pZoneTable = &zones;
		g_pPriceManager = &prices;
		g_bZoneSafe = false;
		g_LeftPremiumDays = 0xFFFE;
	}
	~World()
	{
		MWarManager::SetHost(oldWarHost);
		NoticePresentation::SetHost(oldNotices);
		WorldEvents::SetHost(oldEvents);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_LeftPremiumDays = oldPremium;
		g_bZoneSafe = oldSafe;
		g_pPriceManager = oldPrices;
		g_pZoneTable = oldZones;
		g_pWarManager = oldWars;
		g_pNoticeMessage = oldNotice;
		g_pSystemMessage = oldSystem;
	}
};

void Receive(int code, unsigned parameter = 0)
{
	GCNoticeEvent packet;
	packet.setCode(static_cast<WORD>(code));
	packet.setParameter(parameter);
	GCNoticeEventHandler::execute(&packet, nullptr);
}

std::string Event(int id, int type, DWORD flag, int delay, DWORD p1 = 0, DWORD p2 = 0, DWORD p3 = 0, int show = -1, int total = 0, int string = -1)
{
	MEvent defaults;
	return "event|" + N(id) + "|" + N(type) + "|" + N(flag) + "|" + N(delay) + "|" + N(p1) + "," + N(p2) + "," + N(p3) + "|"
		+ N(show == -1 ? defaults.showTime : show) + "," + N(total == 0 ? defaults.totalTime : total) + (string >= 0 ? "|" + N(string) : "");
}

} // namespace

TEST(NoticeEventReplies, EventsAreQueuedAndRemoved)
{
	World world;
	MEvent defaults;
	Receive(NOTICE_EVENT_SHOP_TAX_CHANGE, 12);
	Receive(NOTICE_EVENT_MASTER_COMBAT_TIME, 3);
	Receive(NOTICE_EVENT_MASTER_COMBAT_END);
	Receive(NOTICE_EVENT_KICK_OUT_FROM_ZONE, 4);
	Receive(NOTICE_EVENT_CONTINUAL_GROUND_ATTACK, 5);
	Receive(NOTICE_EVENT_CONTINUAL_GROUND_ATTACK_END);
	Receive(NOTICE_EVENT_GDR_LAIR_ENDING_1);
	Receive(NOTICE_EVENT_RACE_WAR_IN_5);
	CHECK(calls == (std::vector<std::string>{
		Event(EVENTID_TAX_CHANGE, EVENTTYPE_ZONE, defaults.eventFlag, -1, 12, defaults.parameter2, defaults.parameter3),
		Event(EVENTID_COMBAT_MASTER, EVENTTYPE_ZONE, EVENTFLAG_SHOW_DELAY, 3000, defaults.parameter1, defaults.parameter2, defaults.parameter3),
		"remove|" + N(EVENTID_COMBAT_MASTER),
		Event(EVENTID_KICK_OUT_FROM_ZONE, EVENTTYPE_ZONE, EVENTFLAG_SHOW_DELAY | EVENTFLAG_SHAKE_SCREEN | EVENTFLAG_SHOW_DELAY_STRING, 4000,
			defaults.parameter1, defaults.parameter2, 5, -1, 0, STRING_MESSAGE_DISMISS_AFTER_SECOND),
		"remove|" + N(EVENTID_COMBAT_MASTER),
		Event(EVENTID_CONTINUAL_GROUND_ATTACK, EVENTTYPE_ZONE, EVENTFLAG_FADE_SCREEN | EVENTFLAG_SHAKE_SCREEN, 5000, defaults.parameter1, 30 << 16, 5),
		"remove|" + N(EVENTID_CONTINUAL_GROUND_ATTACK),
		Event(EVENTID_GDR_PRESENT, EVENTTYPE_ZONE, EVENTFLAG_SHAKE_SCREEN, 5000, defaults.parameter1, defaults.parameter2, 2),
		Event(EVENTID_WAR_EFFECT, EVENTTYPE_NULL, EVENTFLAG_SHAKE_SCREEN | EVENTFLAG_FADE_SCREEN, -1, defaults.parameter1, RGB(-30, -30, 0), 1, 2000, 30000)}));
}

TEST(NoticeEventReplies, ThePremiumHalfPriceIsAnnouncedByRace)
{
	World world;
	MEvent defaults;
	Receive(NOTICE_EVENT_PREMIUM_HALF_START);
	Receive(NOTICE_EVENT_PREMIUM_HALF_END);
	race = RACE_VAMPIRE;
	Receive(NOTICE_EVENT_PREMIUM_HALF_START);
	race = RACE_OUSTERS;
	Receive(NOTICE_EVENT_PREMIUM_HALF_END);
	const std::string start = Event(EVENTID_PREMIUM_HALF, EVENTTYPE_ZONE, defaults.eventFlag, -1, defaults.parameter1, defaults.parameter2, defaults.parameter3);
	const std::string end = "remove|" + N(EVENTID_PREMIUM_HALF);
	CHECK(calls == (std::vector<std::string>{start, end, start, end}));
	CHECK(Lines(world.systemLog) == (std::vector<std::string>{Text(STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER),
		Text(STRING_MESSAGE_EVENT_PREMIUM_HALF_SLAYER_END), Text(STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE),
		Text(STRING_MESSAGE_EVENT_PREMIUM_HALF_VAMPIRE_END)}));
}

TEST(NoticeEventReplies, AWarOverInThePlayersZoneRestoresItsSafety)
{
	World world;
	int destroyed = 0;
	GuildWarInfo* pWar = new GuildWarInfo;
	pWar->setCastleID(60);
	world.wars.SetWar(pWar);
	(void)destroyed;
	Receive(NOTICE_EVENT_WAR_OVER, 60);
	CHECK(g_bZoneSafe);
	CHECK(!world.wars.IsExist(60));
	// Another zone's war: the safety stays.
	g_bZoneSafe = false;
	pWar = new GuildWarInfo;
	pWar->setCastleID(61);
	world.wars.SetWar(pWar);
	Receive(NOTICE_EVENT_WAR_OVER, 61);
	CHECK(!g_bZoneSafe && !world.wars.IsExist(61));
	// A level war over ends the war of the player's zone.
	pWar = new GuildWarInfo;
	pWar->setCastleID(60);
	world.wars.SetWar(pWar);
	Receive(NOTICE_EVENT_LEVEL_WAR_OVER);
	CHECK(!world.wars.IsExist(60));
	// A race war over clears the race wars and closes the race war notice.
	Receive(NOTICE_EVENT_RACE_WAR_OVER);
	CHECK(calls == std::vector<std::string>{"notice gone|6"});
	g_pWarManager = nullptr;
	Receive(NOTICE_EVENT_RACE_WAR_OVER);
	CHECK(calls == (std::vector<std::string>{"notice gone|6", "notice gone|6"}));
}

TEST(NoticeEventReplies, WindowsAndPopupsAreShown)
{
	World world;
	zoneID = 1007;
	Receive(NOTICE_EVENT_WELCOME_MESSAGE);
	Receive(NOTICE_EVENT_RESULT_LOTTERY, 1);
	Receive(NOTICE_EVENT_RESULT_LOTTERY, 0);
	Receive(NOTICE_EVENT_RUN_HORN);
	Receive(NOTICE_EVENT_MASTER_LAIR_OPEN, (30u << 16) | 1104);
	Receive(NOTICE_EVENT_MASTER_LAIR_COUNT, (5u << 16) | 1105);
	Receive(NOTICE_EVENT_MINI_GAME, 3);
	Receive(NOTICE_EVENT_START_QUEST_ENDING);
	Receive(NOTICE_EVENT_HOLYDAY, 1);
	Receive(NOTICE_EVENT_HOLYDAY, 0);
	Receive(NOTICE_EVENT_MEET_GRANDMA);
	Receive(NOTICE_EVENT_MEET_FISHSHOP_MASTER);
	CHECK(calls == (std::vector<std::string>{"welcome", "lottery|won", "lottery|lost", "horn|1007", "lair|0|30|1104",
		"lair|" + N(NOTICE_EVENT_MASTER_LAIR_COUNT - NOTICE_EVENT_MASTER_LAIR_OPEN) + "|5|1105", "minigame|3", "quest ending", "flags|on",
		"flags|off"}));
	calls.clear();
	const std::pair<int, int> popups[] = {{NOTICE_EVENT_CONTRACT_GNOMES_HORN, STRING_MESSAGE_CONTRACT_GNOMES_HORN},
		{NOTICE_EVENT_NOT_ENOUGH_MONEY, STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY}, {NOTICE_EVENT_GET_RIFINIUM, STRING_MESSAGE_GET_RIFINIUM},
		{NOTICE_EVENT_INVENTORY_FULL, UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST},
		{NOTICE_EVENT_RACE_WAR_STARTED_IN_OTHER_SERVER, STRING_MESSAGE_RACE_WAR_STARTED_IN_OTHER_SERVER},
		{NOTICE_EVENT_CANNOT_FIND_STORE, UI_STRING_MESSAGE_CANNOT_FIND_STORE}, {NOTICE_EVENT_STORE_CLOSED, UI_STRING_MESSAGE_STORE_CLOSED},
		{NOTICE_EVENT_ITEM_NOT_FOUND, UI_STRING_MESSAGE_ITEM_NOT_FOUND}, {NOTICE_EVENT_TOO_MUCH_MONEY, UI_STRING_MESSAGE_TOO_MUCH_MONEY},
		{NOTICE_EVENT_NO_INVENTORY_SPACE, UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST},
		{NOTICE_EVENT_ALREADY_DISPLAYED, UI_STRING_MESSAGE_ALREADY_DISPLAYED}, {NOTICE_EVENT_CANNOT_SELL, STRING_MESSAGE_CANNOT_SELL}};
	std::vector<std::string> expected;
	for (const auto& [code, string] : popups)
	{
		Receive(code);
		expected.push_back("popup|" + N(string));
	}
	CHECK(calls == expected);
}

TEST(NoticeEventReplies, NoticeWindowsOpenAndClose)
{
	World world;
	Receive(NOTICE_EVENT_ENTER_BEGINNER_ZONE, 11);
	Receive(NOTICE_EVENT_LEVEL_WAR_ARRANGED, 12);
	Receive(NOTICE_EVENT_LEVEL_WAR_STARTED, 13);
	Receive(NOTICE_EVENT_RACE_WAR_SOON, 14);
	Receive(NOTICE_EVENT_CAN_PET_QUEST, 15);
	Receive(NOTICE_EVENT_SEND_SMS, 16);
	CHECK(calls == (std::vector<std::string>{"notice|1|11", "notice|3|12", "notice|4|13", "notice gone|3", "notice|5|14", "notice|7|0",
		"notice|8|0"}));
}

TEST(NoticeEventReplies, TheFlagWarIsAnnounced)
{
	World world;
	Receive(NOTICE_EVENT_FLAG_WAR_READY, 300);
	Receive(NOTICE_EVENT_FLAG_WAR_START);
	CHECK(calls == std::vector<std::string>{"notice|2|300"});
	calls.clear();
	// The winner's race is the parameter's high word, the ousters' past
	// it; its flags the low word.
	char winner[512];
	for (unsigned high : {1u, 7u})
	{
		Receive(NOTICE_EVENT_FLAG_WAR_FINISH, (high << 16) | (high + 2));
		CHECK(calls == (std::vector<std::string>{Event(EVENTID_POUR_ITEM, EVENTTYPE_ZONE, EVENTFLAG_SHOW_DELAY_STRING, 180000, MEvent().parameter1,
			MEvent().parameter2, MEvent().parameter3, -1, 0, STRING_MESSAGE_POUR_ITEM_AFTER_SECOND), "ctf closed", "notice gone|2"}));
		calls.clear();
	}
	Receive(NOTICE_EVENT_FLAG_POURED_ITEM);
	std::vector<std::string> said = {Text(STRING_MESSAGE_EVENT_FLAG_WAR_READY), Text(STRING_MESSAGE_EVENT_FLAG_WAR_START)};
	for (const auto& [raceString, flags] : {std::pair<int, int>{STRING_MESSAGE_VAMPIRE, 3}, {STRING_MESSAGE_OUSTERS, 9}})
	{
		SafeFormat::Format(winner, GetGameString(STRING_MESSAGE_EVENT_FLAG_WAR_WINNER), Text(raceString).c_str(), flags);
		said.insert(said.end(), {Text(STRING_MESSAGE_EVENT_FLAG_WAR_FINISH), winner, Text(STRING_MESSAGE_EVENT_FLAG_WILL_POUR_ITEM_AFTER_3MIN)});
	}
	said.push_back(Text(STRING_MESSAGE_EVENT_FLAG_POURED_ITEM));
	CHECK(Lines(world.noticeLog) == said);
	CHECK(Text(STRING_MESSAGE_VAMPIRE) != Text(STRING_MESSAGE_OUSTERS));
}

TEST(NoticeEventReplies, ALoginSetsTheHeadPriceAndWarnsOfPremiumDays)
{
	World world;
	g_LeftPremiumDays = 3;
	Receive(NOTICE_EVENT_LOGIN_JUST_NOW, 250);
	CHECK(calls == std::vector<std::string>{"crash report"});
	CHECK_EQ(250, g_pUserInformation->HeadPrice);
	CHECK(Lines(world.noticeLog) == (std::vector<std::string>{Text(STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_1),
		Text(STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_2), Text(STRING_MESSAGE_EXPIRE_PREMIUM_SERVICE_MESSAGE_3)}));
	// Six days, none counted (0xFFFE) or none left below zero: no warning.
	for (int days : {6, 0xFFFE, -1})
	{
		World quiet;
		g_LeftPremiumDays = days;
		Receive(NOTICE_EVENT_LOGIN_JUST_NOW, 1);
		CHECK(Lines(quiet.noticeLog).empty());
	}
	// Zero and five days are warned of.
	for (int days : {0, 5})
	{
		World warned;
		g_LeftPremiumDays = days;
		Receive(NOTICE_EVENT_LOGIN_JUST_NOW, 1);
		CHECK(Lines(warned.noticeLog).size() == 3);
	}
}

TEST(NoticeEventReplies, MedalsAndTheCrownsPriceAreLogged)
{
	World world;
	Receive(NOTICE_EVENT_GOLD_MEDALS, 9);
	Receive(NOTICE_EVENT_CROWN_PRICE, 4500);
	char medals[128], crown[128];
	SafeFormat::Format(medals, GetGameString(UI_STRING_NOTICE_EVENT_GOLD_MEDALS), 9);
	SafeFormat::Format(crown, GetGameString(UI_STRING_CHANGE_EVENTITEM_PRICE), 4500);
	CHECK(Lines(world.systemLog) == (std::vector<std::string>{medals, crown}));
	struct Card : MItem
	{
		ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_MOON_CARD; }
	} card;
	card.SetItemType(4);
	CHECK_EQ(4500, world.prices.GetItemPrice(&card, MPriceManager::NPC_TO_PC));
	Receive(NOTICE_EVENT_NETMARBLE_CARD_FULL);
	CHECK(Lines(world.noticeLog) == (std::vector<std::string>{Text(STRING_MESSAGE_EVENT_NETMARBLE_1), Text(STRING_MESSAGE_EVENT_NETMARBLE_2),
		Text(STRING_MESSAGE_EVENT_NETMARBLE_3), Text(STRING_MESSAGE_EVENT_NETMARBLE_4)}));
}

TEST(NoticeEventReplies, PresentBoxesArePoppedUp)
{
	World world;
	Receive(NOTICE_EVENT_GIVE_PRESENT_1);
	std::vector<std::string> expected = {"popup text|large|" + Text(STRING_MESSAGE_DAUM_EVENT_1)};
	const int boxes[] = {STRING_MESSAGE_DAUM_EVENT_BOX2, STRING_MESSAGE_DAUM_EVENT_BOX3, STRING_MESSAGE_DAUM_EVENT_BOX4, STRING_MESSAGE_DAUM_EVENT_BOX5,
		STRING_MESSAGE_DAUM_EVENT_BOX6, STRING_MESSAGE_DAUM_EVENT_BOX7, STRING_MESSAGE_DAUM_EVENT_BOX8};
	for (int level = 2; level <= 8; level++)
	{
		Receive(NOTICE_EVENT_GIVE_PRESENT_2 + level - 2);
		char text[1024];
		SafeFormat::Format(text, GetGameString(STRING_MESSAGE_DAUM_EVENT_2), (level - 1) * 5, Text(boxes[level - 2]).c_str());
		expected.push_back(std::string("popup text|large|") + text);
	}
	Receive(NOTICE_EVENT_CAN_OPEN_PRESENT_8);
	char open[1024];
	SafeFormat::Format(open, GetGameString(STRING_MESSAGE_DAUM_EVENT_3), 40);
	expected.push_back(std::string("popup text|normal|") + open);
	CHECK(calls == expected);
}

TEST(NoticeEventReplies, WithoutTheHostsNothingIsShown)
{
	World world;
	NoticePresentation::SetHost(nullptr);
	WorldEvents::SetHost(nullptr);
	Receive(NOTICE_EVENT_WELCOME_MESSAGE);
	Receive(NOTICE_EVENT_SHOP_TAX_CHANGE, 1);
	Receive(NOTICE_EVENT_GIVE_PRESENT_3);
	Receive(9999);
	CHECK(calls.empty());
}
