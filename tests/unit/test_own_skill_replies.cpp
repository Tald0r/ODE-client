#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "ClientConfig.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "MActionInfoTable.h"
#include "MEventQueue.h"
#include "MGameStringTable.h"
#include "MItem.h"
#include "MSkillManager.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "RankBonusHandlerHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCSkillToSelfOK1.h"
#include <string>
#include <vector>

// The player's own skill succeeded: the wait on it ends, the player keeps
// the result (its step when graded) for the converted duration and shows
// it where it stands, the packet changes the player's status, and the
// effect status the packet names, or else the skill's own, is added for
// that duration. Some skills do more: Eternity resurrects a slayer after
// five seconds (and closes the resurrect window); Concealment rounds its
// duration to its animation; Soul Chain starts its delay now; Will of
// Life checks the player's regeneration and waits as long as its level
// says. An ouster's untransform ends there (it is the answer to getting
// off a sylph), and so does a turret's.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
bool ousters = false;
bool slayer = true;
bool turret = false;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
MStatus playerStatus;
MonotonicClock::TimePoint now;
MItemHost itemHost{};

struct FakeCreature
{
	TYPE_OBJECTID id;
	TYPE_SECTORPOSITION x, y;
};
FakeCreature me{7, 30, 31};

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle() { return reinterpret_cast<MCreature*>(&me); }

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle() : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	host.Status = []() { return &playerStatus; };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return turret && status == EFFECTSTATUS_INSTALL_TURRET; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsOusters = [](MCreature*) { return ousters; };
	host.IsSlayer = [](MCreature*) { return slayer; };
	host.GetID = [](MCreature* p) { return reinterpret_cast<FakeCreature*>(p)->id; };
	host.GetX = [](MCreature* p) { return reinterpret_cast<FakeCreature*>(p)->x; };
	host.GetY = [](MCreature* p) { return reinterpret_cast<FakeCreature*>(p)->y; };
	host.SetEffectDelayFrame = [](MCreature*, TYPE_ACTIONINFO ai, std::uint32_t frames) { calls.push_back("delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature*, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t) {
		calls.push_back("result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY));
	};
	host.AddEffectStatus = [](MCreature*, EFFECTSTATUS status, std::uint32_t delayFrame) {
		calls.push_back("effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetUndergroundCreature = [](MCreature*) { calls.push_back("burrow"); };
	return host;
}();

// The result node finds the player through the zone.
const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return true; };
	host.GetCreature = [](TYPE_OBJECTID id) { return id == me.id ? Handle() : nullptr; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.AddEvent = [](MEvent& event) {
		std::string strings;
		for (int id : event.m_StringsID) strings += "|" + N(id);
		calls.push_back("event|" + N(event.eventID) + "|" + N(event.eventDelay) + "|" + N(event.eventFlag) + "|" + N(event.eventType) + strings);
	};
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.CloseRequestResurrectWindow = []() { calls.push_back("resurrect window closed"); };
	return host;
}();

const RankBonusHandlers::Host regen = []() {
	RankBonusHandlers::Host host{};
	host.CheckRegen = []() { calls.push_back("regen checked"); };
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.GetCharInfoLevel = []() { return 50; };
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;

struct World : GameModelWorld
{
	MActionInfoTable actions;
	MSkillInfoTable skills;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MSkillInfoTable* oldSkills = g_pSkillInfoTable;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const RankBonusHandlers::Host* oldRegen = RankBonusHandlers::SetHost(&regen);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	World()
	{
		calls.clear();
		hasPlayer = true;
		ousters = false;
		slayer = true;
		turret = false;
		waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL);
		// A duration is then its frame count.
		g_pClientConfig->FPS = 10;
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
		g_pSkillInfoTable = &skills;
		now = MonotonicClock::Now();
		itemHost.pCurrentTime = &now;
		MItem::SetHost(&itemHost);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		GameInfoPresentation::SetHost(oldInfo);
		RankBonusHandlers::SetHost(oldRegen);
		DialogPresentation::SetHost(oldDialog);
		ZoneState::SetHost(oldZone);
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		g_pSkillInfoTable = oldSkills;
		g_pActionInfoTable = oldActions;
	}
};

void Receive(int skill, int duration, int grade = 0, int hp = -1, int effectStat = -1)
{
	GCSkillToSelfOK1 packet;
	packet.setSkillType(static_cast<SkillType_t>(skill));
	packet.setDuration(static_cast<Duration_t>(duration));
	packet.setGrade(static_cast<BYTE>(grade));
	if (hp >= 0) packet.addShortData(MODIFY_CURRENT_HP, static_cast<WORD>(hp));
	if (effectStat >= 0) packet.addLongData(MODIFY_EFFECT_STAT, static_cast<DWORD>(effectStat));
	GCSkillToSelfOK1Handler::execute(&packet, nullptr);
}

// What every success shows: the delay on the result, and the result at
// the player's cell.
std::vector<std::string> Shown(int skill, int frames)
{
	return {"delay|" + N(skill + kMin) + "|" + N(frames), "result|" + N(skill + kMin) + "|7|30|31"};
}

std::vector<std::string> Plus(std::vector<std::string> first, const std::vector<std::string>& more)
{
	first.insert(first.end(), more.begin(), more.end());
	return first;
}

} // namespace

TEST(OwnSkillReplies, ASuccessEndsTheWaitShowsTheResultAndChangesThePlayer)
{
	World world;
	auto& row = testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT);
	row.SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	row.SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	Receive(SKILL_DOUBLE_IMPACT, 50, 0, 120);
	CHECK(calls == Plus(Plus({"verified"}, Shown(SKILL_DOUBLE_IMPACT, 50)),
		{"effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|50", "burrow"}));
	CHECK_EQ(120, static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)));
	// Not waited on: the rest still happens.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	Receive(SKILL_DOUBLE_IMPACT, 50);
	CHECK(calls == Plus(Shown(SKILL_DOUBLE_IMPACT, 50), {"effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|50", "burrow"}));
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_TRADE);
}

TEST(OwnSkillReplies, TheEffectStatusThePacketNamesComesFirst)
{
	World world;
	testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	Receive(SKILL_DOUBLE_IMPACT, 40, 0, -1, EFFECTSTATUS_INSTALL_TURRET);
	CHECK(calls == Plus(Plus({"verified"}, Shown(SKILL_DOUBLE_IMPACT, 40)), {"effect|" + N(EFFECTSTATUS_INSTALL_TURRET) + "|40"}));
	CHECK(playerStatus.GetEFFECT_STAT() == EFFECTSTATUS_NULL);
}

TEST(OwnSkillReplies, TheDurationIsConvertedToFrames)
{
	World world;
	// At the client's 16 frames a second, 5 seconds are 80 frames.
	g_pClientConfig->FPS = 16;
	testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	Receive(SKILL_DOUBLE_IMPACT, 50);
	CHECK(calls == Plus(Plus({"verified"}, Shown(SKILL_DOUBLE_IMPACT, 80)), {"effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|80"}));
}

TEST(OwnSkillReplies, AGradedSkillActsAsItsStep)
{
	World world;
	auto& row = testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT);
	row.SetUseActionStep();
	row.SetActionStep(1, SKILL_SINGLE_BLOW);
	testfw::MutableRow(world.actions, SKILL_SINGLE_BLOW).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	Receive(SKILL_DOUBLE_IMPACT, 20, 2);
	CHECK(calls == Plus(Plus({"verified"}, Shown(SKILL_SINGLE_BLOW, 20)), {"effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|20"}));
	// Ungraded, it acts as itself.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	Receive(SKILL_DOUBLE_IMPACT, 20, 0);
	CHECK(calls == Plus({"verified"}, Shown(SKILL_DOUBLE_IMPACT, 20)));
	// A grade on a skill without steps changes nothing.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	Receive(SKILL_SINGLE_BLOW, 20, 2);
	CHECK(calls == Plus(Plus({"verified"}, Shown(SKILL_SINGLE_BLOW, 20)), {"effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|20"}));
}

TEST(OwnSkillReplies, ASkillPastTheTableOnlyEndsTheWait)
{
	World world;
	Receive(MAX_ACTIONINFO, 20);
	CHECK(calls == std::vector<std::string>{"verified"});
}

TEST(OwnSkillReplies, AnOustersUntransformOrATurretsEndsThere)
{
	World world;
	ousters = true;
	Receive(MAGIC_UN_TRANSFORM, 20);
	// Waited on, the wait is ended twice: once as an answer, once as an
	// ouster's untransform.
	CHECK(calls == std::vector<std::string>({"verified", "verified"}));
	// Waited on or not, an ouster's wait is cleared.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	Receive(MAGIC_UN_TRANSFORM, 20);
	CHECK(calls == std::vector<std::string>{"verified"});
	calls.clear();
	ousters = false;
	turret = true;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	Receive(MAGIC_UN_TRANSFORM, 20);
	CHECK(calls == std::vector<std::string>{"verified"});
	// Anyone else's untransform goes on.
	calls.clear();
	turret = false;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	Receive(MAGIC_UN_TRANSFORM, 20);
	CHECK(calls == Plus({"verified"}, Shown(MAGIC_UN_TRANSFORM, 20)));
}

TEST(OwnSkillReplies, ASlayersEternityResurrectsLater)
{
	World world;
	Receive(SKILL_ETERNITY, 20);
	CHECK(calls == Plus({"verified", "event|" + N(EVENTID_RESURRECT) + "|5000|" + N(EVENTFLAG_SHOW_DELAY_STRING) + "|"
		+ N(EVENTTYPE_ZONE) + "|" + N(STRING_MESSAGE_RESURRECT_AFTER_SECONDS), "resurrect window closed"}, Shown(SKILL_ETERNITY, 20)));
	calls.clear();
	slayer = false;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	Receive(SKILL_ETERNITY, 20);
	CHECK(calls == Plus({"verified"}, Shown(SKILL_ETERNITY, 20)));
}

TEST(OwnSkillReplies, ConcealmentRoundsItsDurationToItsAnimation)
{
	World world;
	auto& result = testfw::MutableRow(world.actions, RESULT_SKILL_CONCEALMENT);
	result.Init(2);
	result.GetMutable(1)->Count = 8;
	const int skill = RESULT_SKILL_CONCEALMENT - kMin;
	const struct { int duration, frames; } cases[] = {{19, 16}, {21, 24}, {20, 24}, {3, 8}, {16, 16}, {8, 16}};
	for (const auto& c : cases)
	{
		calls.clear();
		waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
		Receive(skill, c.duration);
		CHECK(calls == Plus({"verified"}, Shown(skill, c.frames)));
	}
}

TEST(OwnSkillReplies, SoulChainsDelayStartsNow)
{
	World world;
	SKILLINFO_NODE* pChain = world.skills.GetMutable(SKILL_SOUL_CHAIN);
	CHECK(pChain != nullptr);
	if (pChain == nullptr) return;
	pChain->SetDelayTime(3000);
	pChain->SetAvailableTime();
	Receive(SKILL_SOUL_CHAIN, 20);
	CHECK_EQ(3000, static_cast<long long>(pChain->GetAvailableTimeLeft()));
	// Another skill's delay does not start on its success.
	SKILLINFO_NODE* pOther = world.skills.GetMutable(SKILL_DOUBLE_IMPACT);
	CHECK(pOther != nullptr);
	if (pOther == nullptr) return;
	pOther->SetDelayTime(3000);
	pOther->SetAvailableTime();
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	Receive(SKILL_DOUBLE_IMPACT, 20);
	CHECK_EQ(0, static_cast<long long>(pOther->GetAvailableTimeLeft()));
}

TEST(OwnSkillReplies, WillOfLifeChecksRegenerationAndWaitsByLevel)
{
	World world;
	SKILLINFO_NODE* pWill = world.skills.GetMutable(SKILL_WILL_OF_LIFE);
	CHECK(pWill != nullptr);
	if (pWill == nullptr) return;
	Receive(SKILL_WILL_OF_LIFE, 20);
	CHECK(calls == Plus(Plus({"verified"}, Shown(SKILL_WILL_OF_LIFE, 20)), {"regen checked"}));
	CHECK(GetWillOfLifeDelay(50) > 0);
	CHECK_EQ(static_cast<long long>(GetWillOfLifeDelay(50)), static_cast<long long>(pWill->GetAvailableTimeLeft()));
}

TEST(OwnSkillReplies, WithoutAPlayerNothingHappens)
{
	World world;
	hasPlayer = false;
	Receive(SKILL_DOUBLE_IMPACT, 20, 0, 120);
	CHECK(calls.empty());
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_SKILL_SUCCESS);
	// Without a host the window is left alone.
	DialogPresentation::SetHost(nullptr);
	DialogPresentation::CloseRequestResurrectWindow();
	CHECK(calls.empty());
}
