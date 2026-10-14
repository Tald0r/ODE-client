#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CommunicationPresentationHost.h"
#include "MChatManager.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MSkillManager.h"
#include "MStatus.h"
#include "MZoneTable.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "UserInformation.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCGlobalChat.h"
#include "Gpackets/GCSay.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

// A creature's words show over its head and, for a player character, in
// the history. Words the player refuses show as "......" unless an
// operator said them. Curses are filtered from a player character's words
// unless an operator said them, the player is one, or the player is a
// ghost. Another race's words are masked in a zone that masks by race,
// under its hallucination name, unless the player can read them; a
// hallucinating player sees them masked too. A shout to the zone names its
// sender before the first space and goes to the history the same way; its
// race mask lands on the copied sender name, so the words arrive unmasked
// (upstream's, pinned here).
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
std::set<EFFECTSTATUS> playerEffects;
Race playerRace = RACE_SLAYER;
MStatus playerStatus;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	int type = 0;
	int race = RACE_SLAYER;
	bool slayer = false;
	bool ousters = false;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
FakeCreature me;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetID = []() { return 21; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.Status = []() { return hasPlayer ? &playerStatus : nullptr; };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return playerEffects.count(status) != 0; };
	host.GetRace = []() { return playerRace; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetName = [](MCreature* p) { return Fake(p).name.c_str(); };
	host.GetHalluName = [](MCreature*) { return "Stranger"; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.IsSlayer = [](MCreature* p) { return Fake(p).slayer; };
	host.IsOusters = [](MCreature* p) { return Fake(p).ousters; };
	host.GetRace = [](MCreature* p) { return Fake(p).race; };
	host.SetChatString = [](MCreature* p, const char* str, std::uint32_t color) {
		calls.push_back(Fake(p).name + ".says|" + str + "|" + N(color));
	};
	return host;
}();

const CommunicationPresentation::Host chat = []() {
	CommunicationPresentation::Host host{};
	host.AddChatToHistory = [](const char* message, const char* sender, int chatType, std::uint32_t color) {
		calls.push_back(std::string("history|") + sender + "|" + message + "|" + N(chatType) + "|" + N(color));
	};
	return host;
}();

const char* const kCurseFile = "chat_replies_words.txt";
constexpr std::uint32_t kColor = 0x123456;

struct World : GameModelWorld
{
	FakeCreature slayer, vampire, monster, gilles, master;
	MChatManager filter;
	CZoneTable zones;
	MSkillSet skills;
	MChatManager* oldFilter = g_pChatManager;
	CZoneTable* oldZones = g_pZoneTable;
	MSkillSet* oldSkills = g_pSkillAvailable;
	const MChatHost* oldFilterHost = MChatManager::SetHost(nullptr);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CommunicationPresentation::Host* oldChat = CommunicationPresentation::SetHost(&chat);
	ZONETABLE_INFO* info = new ZONETABLE_INFO;
	World()
	{
		InitGameStringTable();
		me = {7, "Self", CREATURETYPE_SLAYER_MALE, RACE_SLAYER, true};
		slayer = {1001, "Nyx", CREATURETYPE_SLAYER_MALE, RACE_SLAYER, true};
		vampire = {1002, "Vlad", CREATURETYPE_VAMPIRE_MALE1, RACE_VAMPIRE};
		monster = {1003, "Ogre", 5, RACE_VAMPIRE};
		gilles = {1004, "Gilles", 717, RACE_VAMPIRE};
		master = {1005, "GM Rook", CREATURETYPE_SLAYER_MALE, RACE_SLAYER, true};
		creatures = {{slayer.id, &slayer}, {vampire.id, &vampire}, {monster.id, &monster}, {gilles.id, &gilles}, {master.id, &master}};
		zoneExists = true;
		hasPlayer = true;
		playerEffects.clear();
		playerRace = RACE_SLAYER;
		playerStatus = MStatus();
		calls.clear();
		{
			std::ofstream words(kCurseFile, std::ios::binary | std::ios::trunc);
			words << "darn\n";
		}
		filter.LoadFromFileCurse(kCurseFile);
		g_pChatManager = &filter;
		info->ID = 21;
		CHECK(zones.Add(info));
		g_pZoneTable = &zones;
		g_pSkillAvailable = &skills;
	}
	~World()
	{
		CommunicationPresentation::SetHost(oldChat);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		MChatManager::SetHost(oldFilterHost);
		g_pSkillAvailable = oldSkills;
		g_pZoneTable = oldZones;
		g_pChatManager = oldFilter;
		creatures.clear();
		std::remove(kCurseFile);
	}
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceiveSay(ObjectID_t id, const std::string& message)
{
	GCSay original;
	original.setObjectID(id);
	original.setColor(kColor);
	original.setMessage(message);
	GCSay packet;
	Decode(packet, original);
	GCSayHandler::execute(&packet, nullptr);
}

void ReceiveShout(const std::string& message, Race race = RACE_SLAYER)
{
	GCGlobalChat original;
	original.setColor(kColor);
	original.setMessage(message);
	original.setRace(static_cast<Race_t>(race));
	GCGlobalChat packet;
	Decode(packet, original);
	GCGlobalChatHandler::execute(&packet, nullptr);
}

std::string Says(const char* who, const std::string& words) { return std::string(who) + ".says|" + words + "|" + N(kColor); }
std::string History(const char* who, const std::string& words, int type = 0)
{
	return std::string("history|") + who + "|" + words + "|" + N(type) + "|" + N(kColor);
}

const std::string kLong = "abcdefghijklmnopqrstuvwxyzabcdefghijklmn";

// The words a creature said, as recorded over its head.
std::string Said(const std::string& record)
{
	const size_t first = record.find('|');
	const size_t last = record.rfind('|');
	return record.substr(first + 1, last - first - 1);
}

} // namespace

TEST(ChatReplies, APlayerCharactersWordsAreFilteredAndKeptInTheHistory)
{
	World world;
	ReceiveSay(1001, "darn it");
	CHECK(calls == std::vector<std::string>({Says("Nyx", "xxxx it"), History("Nyx", "xxxx it")}));
	// A monster's words are neither filtered nor kept.
	calls.clear();
	ReceiveSay(1003, "darn it");
	CHECK(calls == std::vector<std::string>({Says("Ogre", "darn it")}));
	// Gilles de Rais speaks as an operator, over his head only.
	calls.clear();
	ReceiveSay(1004, "darn it");
	CHECK(calls == std::vector<std::string>({Says("Gilles", "darn it")}));
}

TEST(ChatReplies, AnOperatorAMasterPlayerOrAGhostSkipsTheFilter)
{
	World world;
	ReceiveSay(1005, "darn it");
	g_pUserInformation->IsMaster = TRUE;
	ReceiveSay(1001, "darn it");
	g_pUserInformation->IsMaster = FALSE;
	playerEffects.insert(EFFECTSTATUS_GHOST);
	ReceiveSay(1001, "darn it");
	CHECK(calls == std::vector<std::string>({Says("GM Rook", "darn it"), History("GM Rook", "darn it"),
		Says("Nyx", "darn it"), History("Nyx", "darn it"), Says("Nyx", "darn it"), History("Nyx", "darn it")}));
}

TEST(ChatReplies, RefusedWordsShowAsDotsUnlessAnOperatorSaidThem)
{
	World world;
	world.filter.SetIgnoreMode();
	ReceiveSay(1001, "darn it");
	ReceiveSay(1005, "darn it");
	ReceiveSay(1004, "darn it");
	CHECK(calls == std::vector<std::string>({Says("Nyx", "......"), Says("GM Rook", "darn it"), History("GM Rook", "darn it"),
		Says("Gilles", "darn it")}));
	calls.clear();
	CHECK(world.filter.AddID("Nyx"));
	ReceiveSay(1001, "darn it");
	CHECK(calls == std::vector<std::string>({Says("Nyx", "xxxx it"), History("Nyx", "xxxx it")}));
}

TEST(ChatReplies, AnotherRacesWordsAreMaskedUnderItsHallucinationName)
{
	World world;
	srand(1);
	ReceiveSay(1002, kLong);
	CHECK_EQ(static_cast<size_t>(2), calls.size());
	const std::string masked = calls.size() == 2 ? Said(calls[0]) : std::string();
	CHECK_EQ(kLong.size(), masked.size());
	CHECK(masked != kLong);
	CHECK(calls.size() == 2 && calls[1] == History("Stranger", masked));
	// The player reads them in a zone that does not mask, with the Hillel
	// skill, under translation, or as an operator.
	const auto readable = [&]() {
		calls.clear();
		ReceiveSay(1002, kLong);
		return calls == std::vector<std::string>({Says("Vlad", kLong), History("Vlad", kLong)});
	};
	world.info->ChatMaskByRace = false;
	CHECK(readable());
	world.info->ChatMaskByRace = true;
	CHECK(world.skills.AddSkill(SKILL_HOLYLAND_BLOOD_BIBLE_HILLEL));
	CHECK(readable());
	CHECK(world.skills.DisableSkill(SKILL_HOLYLAND_BLOOD_BIBLE_HILLEL));
	playerEffects.insert(EFFECTSTATUS_TRANSLATION);
	CHECK(readable());
	playerEffects.clear();
	me.type = CREATURETYPE_SLAYER_OPERATOR;
	CHECK(readable());
	// A master player reads the words but sees the real name.
	me.type = CREATURETYPE_SLAYER_MALE;
	g_pUserInformation->IsMaster = TRUE;
	CHECK(readable());
	g_pUserInformation->IsMaster = FALSE;
}

TEST(ChatReplies, AHallucinatingPlayerSeesEvenItsOwnRaceMasked)
{
	World world;
	srand(2);
	playerEffects.insert(EFFECTSTATUS_HALLUCINATION);
	ReceiveSay(1001, kLong);
	CHECK_EQ(static_cast<size_t>(2), calls.size());
	const std::string masked = calls.size() == 2 ? Said(calls[0]) : std::string();
	CHECK(masked.size() == kLong.size() && masked != kLong);
	CHECK(calls.size() == 2 && calls[1] == History("Nyx", masked));
	// Even a monster's words, which are never filtered.
	calls.clear();
	ReceiveSay(1003, kLong);
	CHECK_EQ(static_cast<size_t>(1), calls.size());
	const std::string monster = calls.size() == 1 ? Said(calls[0]) : std::string();
	CHECK(monster.size() == kLong.size() && monster != kLong);
}

TEST(ChatReplies, TheSayGuards)
{
	World world;
	ReceiveSay(4242, "hello");
	zoneExists = false;
	ReceiveSay(1001, "hello");
	zoneExists = true;
	hasPlayer = false;
	ReceiveSay(1001, "hello");
	CHECK(calls.empty());
}

TEST(ChatReplies, AShoutNamesItsSenderBeforeTheFirstSpace)
{
	World world;
	ReceiveShout("Nyx darn it");
	CHECK(calls == std::vector<std::string>{History("Nyx", "xxxx it", 1)});
	// No words after the name, or no name: nothing.
	calls.clear();
	ReceiveShout("Nyx ");
	ReceiveShout("Nyx");
	CHECK(calls.empty());
	// A refused sender is dropped unless the name is the operator's.
	world.filter.SetIgnoreMode();
	ReceiveShout("Nyx darn it");
	ReceiveShout("GM darn it");
	CHECK(calls == std::vector<std::string>{History("GM", "darn it", 1)});
}

TEST(ChatReplies, AShoutSkipsTheFilterForAMasterPlayerOrAGhost)
{
	World world;
	g_pUserInformation->IsMaster = TRUE;
	ReceiveShout("Nyx darn it");
	g_pUserInformation->IsMaster = FALSE;
	playerEffects.insert(EFFECTSTATUS_GHOST);
	ReceiveShout("Nyx darn it");
	CHECK(calls == std::vector<std::string>(2, History("Nyx", "darn it", 1)));
}

TEST(ChatReplies, AnotherRacesShoutArrivesUnmaskedSinceTheMaskLandsOnTheName)
{
	World world;
	srand(3);
	ReceiveShout("Vlad " + kLong, RACE_VAMPIRE);
	CHECK(calls == std::vector<std::string>{History("Vlad", kLong, 1)});
}

TEST(ChatReplies, TheShoutGuards)
{
	World world;
	zoneExists = false;
	ReceiveShout("Nyx hello");
	zoneExists = true;
	hasPlayer = false;
	ReceiveShout("Nyx hello");
	CHECK(calls.empty());
}

TEST(ChatReplies, WithoutAHostACreatureIsANamelessSlayerOfNoRaceFlag)
{
	World world;
	CreatureState::SetHost(nullptr);
	MCreature* const someone = Handle(world.vampire);
	CHECK(std::string(CreatureState::GetHalluName(someone)).empty());
	CHECK(!CreatureState::IsSlayer(someone));
	CHECK(!CreatureState::IsOusters(someone));
	CHECK_EQ(static_cast<int>(RACE_SLAYER), CreatureState::GetRace(someone));
	CreatureState::SetChatString(someone, "hello", kColor);
	CHECK(calls.empty());
}
