#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "DirectionSelection.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "MVampireGear.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "UserInformation.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCSkillToObjectOK6.h"
#include "Gpackets/GCUntransformOK.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// An unseen caster's skill lands on the player: it starts from the cell
// the server names, facing the player, as the packet's skill's result, and
// its queue adds to the player the pending effect status (or else the
// skill's own) and the skill's (or its step's) action. The player turns
// back into a vampire: the drawn type becomes the coat's (or the plain
// vampire's, or an operator's), the player waits a second, stands at the
// server's cell facing its direction, and a werewolf takes back its skin
// and coat colours.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
MStatus playerStatus;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	int type = 0;
	bool male = true;
	int competence = 3;
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
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
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
	host.GetID = []() { return me.id; };
	host.Status = []() { return hasPlayer ? &playerStatus : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() {
		calls.push_back("verified");
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	};
	host.SetDelay = [](std::uint32_t milliseconds) { calls.push_back("wait|" + N(milliseconds)); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.IsMale = [](MCreature* p) { return Fake(p).male; };
	host.GetCompetence = [](MCreature* p) { return Fake(p).competence; };
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "server|" + N(sX) + "," + N(sY)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		Record(p, "move|" + N(x) + "," + N(y));
		return true;
	};
	host.SetDirection = [](MCreature* p, int direction) { Record(p, "dir|" + N(direction)); };
	host.SetCurrentDirection = [](MCreature* p, int direction) { Record(p, "facing|" + N(direction)); };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t set) { Record(p, "skin|" + N(set)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t set) { Record(p, "coat|" + N(set)); };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.SetBloodyZenith = [](MCreature* p, int delay) { Record(p, "zenith|" + N(delay)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY) + "|" + N(grade));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	return host;
}();

// A creature-type change is named, not run (it reaches the creature table
// and the skill bar); any other queue runs at once.
const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(startZ == 0 && targetZ == 0 && sX == 0 && sY == 0);
		calls.push_back("cell|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|" + N(targetID)
			+ "@" + N(targetX) + "," + N(targetY) + "|" + N(delayFrame) + (bStartFromMainNode ? "|main" : "|resume"));
		CHECK(pResult != nullptr);
		if (pResult == nullptr) return;
		bool named = false;
		auto node = pResult->GetIterator();
		for (int i = 0; i < pResult->GetSize(); i++, ++node)
		{
			if ((*node)->GetType() != ACTIONRESULTNODE_CHANGE_CREATURE_TYPE) continue;
			const auto* change = static_cast<const MActionResultNodeChangeCreatureType*>(*node);
			calls.push_back("becomes|" + N(change->m_TargetID) + "|" + N(change->m_CreatureType));
			named = true;
		}
		if (!named) pResult->Execute();
		delete pResult;
	};
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
constexpr TYPE_ACTIONINFO kSkill = SKILL_DOUBLE_IMPACT;
constexpr TYPE_ACTIONINFO kStep = SKILL_TRIPLE_SLASHER;
constexpr int kDuration = 30;

struct Coat : MItem
{
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_VAMPIRE_COAT; }
	bool IsGearItem() const override { return true; }
	bool IsGearSlotVampireCoat() const override { return true; }
	int GetMaxDurability() const override { return 100; }
};

struct World : GameModelWorld
{
	FakeCreature ogre;
	MActionInfoTable actions;
	MVampireGear vampireGear;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MVampireGear* oldVampireGear = g_pVampireGear;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		me = {7, "Self", 30, 31, CREATURETYPE_BAT};
		ogre = {1001, "Ogre", 10, 11};
		creatures = {{ogre.id, &ogre}, {me.id, &me}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
		playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL);
		calls.clear();
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_COAT, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 0).AddonMaleFrameID = 61;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 0).AddonFemaleFrameID = 62;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 0).Race = FLAG_RACE_VAMPIRE;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 0).SetGrid(1, 1);
		vampireGear.Init();
		g_pVampireGear = &vampireGear;
		g_pUserInformation->SkinColor = 211;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pVampireGear = oldVampireGear;
		g_pActionInfoTable = oldActions;
		creatures.clear();
	}
	MActionInfo& Row(TYPE_ACTIONINFO action) { return testfw::MutableRow(actions, action); }
	// Puts on a coat: a quest coat draws in the quest colour, a plain one
	// in the option table's first row's.
	void WearCoat(bool quest = true)
	{
		Coat* coat = new Coat;
		coat->SetID(12);
		coat->SetCurrentDurability(100);
		coat->SetQuestFlag(quest);
		CHECK(vampireGear.AddItem(coat, MVampireGear::GEAR_VAMPIRE_COAT));
	}
	static long long HP() { return static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)); }
};

std::string F() { return N(ConvertDurationToFrame(kDuration)); }

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

void ReceiveUnseenSkill(int skill, int grade = 0)
{
	GCSkillToObjectOK6 original;
	original.setXY(40, 41);
	original.setSkillType(static_cast<SkillType_t>(skill));
	original.setDuration(kDuration);
	original.setGrade(static_cast<BYTE>(grade));
	original.addShortData(MODIFY_CURRENT_HP, 66);
	GCSkillToObjectOK6 packet;
	Decode(packet, original);
	GCSkillToObjectOK6Handler::execute(&packet, nullptr);
}

void ReceiveUntransform()
{
	GCUntransformOK original;
	original.setX(40);
	original.setY(41);
	original.setDir(3);
	GCUntransformOK packet;
	Decode(packet, original);
	GCUntransformOKHandler::execute(&packet, nullptr);
}

std::vector<std::string> Join(std::initializer_list<std::vector<std::string>> parts)
{
	std::vector<std::string> out;
	for (const std::vector<std::string>& part : parts) out.insert(out.end(), part.begin(), part.end());
	return out;
}

// The unseen skill's start: from the packet's cell facing the player.
std::string Start(TYPE_ACTIONINFO skill)
{
	return "cell|" + N(skill + kMin) + "|40,41|dir " + N(SelectFacingDirection(40, 41, 30, 31)) + "|7@30,31|" + F() + "|resume";
}

std::vector<std::string> HitsThePlayer(TYPE_ACTIONINFO action)
{
	return {"Self.delay|" + N(action + kMin) + "|" + F(), "Self.result|" + N(action + kMin) + "|7|30|31|0"};
}

// The untransform's record after the creature-type change.
std::vector<std::string> Stands()
{
	return {"wait|1000", "Self.server|40,41", "Self.move|40,41", "Self.dir|3", "Self.facing|3"};
}

std::string Untransform() { return "cell|" + N(RESULT_MAGIC_UN_TRANSFORM) + "|40,41|dir 3|" + N(OBJECTID_NULL) + "@40,41|0|resume"; }

} // namespace

TEST(UntransformReplies, AnUnseenSkillLandsOnThePlayerFromTheCellTheServerNames)
{
	World world;
	// The packet's cell is not the player's, so the facing differs by way.
	CHECK(SelectFacingDirection(40, 41, 30, 31) != SelectFacingDirection(30, 31, 40, 41));
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveUnseenSkill(kSkill);
	CHECK(calls == Join({{Start(kSkill), "Self.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + F()}, HitsThePlayer(kSkill)}));
	CHECK_EQ(66, World::HP());
	// The player's pending effect status rides instead of the skill's own.
	calls.clear();
	playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_CURSE_DOOM);
	ReceiveUnseenSkill(kSkill);
	CHECK(calls == Join({{Start(kSkill), "Self.effect|" + N(EFFECTSTATUS_CURSE_DOOM) + "|" + F()}, HitsThePlayer(kSkill)}));
	CHECK_EQ(static_cast<long long>(EFFECTSTATUS_NULL), static_cast<long long>(playerStatus.GetStatus(MODIFY_EFFECT_STAT)));
}

TEST(UntransformReplies, AnUnseenSkillsStepActsWhileThePacketsSkillStarts)
{
	World world;
	world.Row(kSkill).SetUseActionStep();
	world.Row(kSkill).SetActionStep(1, kStep);
	world.Row(kStep).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveUnseenSkill(kSkill, 2);
	CHECK(calls == Join({{Start(kSkill), "Self.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + F()}, HitsThePlayer(kStep)}));
	// Grade 0 is the skill itself.
	calls.clear();
	ReceiveUnseenSkill(kSkill, 0);
	CHECK(calls == Join({{Start(kSkill)}, HitsThePlayer(kSkill)}));
}

TEST(UntransformReplies, AnUnseenSkillsGuards)
{
	World world;
	// Past the table, or without a zone, only the status changes.
	ReceiveUnseenSkill(static_cast<int>(world.actions.GetSize()));
	CHECK_EQ(66, World::HP());
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = false;
	ReceiveUnseenSkill(kSkill);
	CHECK_EQ(66, World::HP());
	CHECK(calls.empty());
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = true;
	hasPlayer = false;
	ReceiveUnseenSkill(kSkill);
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
}

TEST(UntransformReplies, ThePlayerTurnsBackIntoAPlainVampireAndStandsAtTheServersCell)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveUntransform();
	CHECK(calls == Join({{"verified", Untransform(), "becomes|7|" + N(CREATURETYPE_VAMPIRE_MALE1)}, Stands()}));
	calls.clear();
	me.male = false;
	ReceiveUntransform();
	CHECK(calls == Join({{Untransform(), "becomes|7|" + N(CREATURETYPE_VAMPIRE_FEMALE1)}, Stands()}));
	// Without a player, nothing.
	calls.clear();
	hasPlayer = false;
	ReceiveUntransform();
	CHECK(calls.empty());
}

TEST(UntransformReplies, ACoatOrAnOperatorsRankNamesTheType)
{
	World world;
	world.WearCoat();
	ReceiveUntransform();
	CHECK(calls == Join({{Untransform(), "becomes|7|61"}, Stands()}));
	calls.clear();
	me.male = false;
	ReceiveUntransform();
	CHECK(calls == Join({{Untransform(), "becomes|7|62"}, Stands()}));
	calls.clear();
	me.competence = 0;
	ReceiveUntransform();
	CHECK(calls == Join({{Untransform(), "becomes|7|" + N(CREATURETYPE_VAMPIRE_OPERATOR)}, Stands()}));
}

TEST(UntransformReplies, AWerewolfTakesBackItsSkinAndCoatColours)
{
	World world;
	me.type = CREATURETYPE_WER_WOLF;
	ReceiveUntransform();
	CHECK(calls == Join({{Untransform(), "becomes|7|" + N(CREATURETYPE_VAMPIRE_MALE1)}, Stands(), {"Self.skin|211", "Self.coat|377"}}));
	calls.clear();
	world.WearCoat();
	ReceiveUntransform();
	CHECK(calls == Join({{Untransform(), "becomes|7|61"}, Stands(), {"Self.skin|211", "Self.coat|" + N(QUEST_ITEM_COLOR)}}));
}

TEST(UntransformReplies, APlainCoatGivesAWerewolfItsOptionColour)
{
	World world;
	me.type = CREATURETYPE_WER_WOLF;
	testfw::MutableRow(*g_pItemOptionTable, 0).ColorSet = 333;
	world.WearCoat(false);
	ReceiveUntransform();
	CHECK(calls == Join({{Untransform(), "becomes|7|61"}, Stands(), {"Self.skin|211", "Self.coat|333"}}));
}

TEST(UntransformReplies, AnUnseenSkillsNodeHasNoUser)
{
	World world;
	// Bloody Zenith's record would show the node's user; there is none.
	ReceiveUnseenSkill(MAGIC_BLOODY_ZENITH);
	CHECK(calls == Join({{Start(MAGIC_BLOODY_ZENITH)}, HitsThePlayer(MAGIC_BLOODY_ZENITH)}));
}

TEST(UntransformReplies, WithoutHostsAPlayerIsAPlainTypeOfNoRankAndKeepsStill)
{
	World world;
	CreatureState::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	CHECK_EQ(0, CreatureState::GetCreatureType(Handle(me)));
	// 3 is MCreature's own starting rank: not an operator (0).
	CHECK_EQ(3, CreatureState::GetCompetence(Handle(me)));
	CreatureState::SetCurrentDirection(Handle(me), 5);
	PlayerState::SetDelay(1000);
	CHECK(calls.empty());
}
