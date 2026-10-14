#include "test_framework.h"
#include "gamemodel_world.h"
#include "CommunicationPresentationHost.h"
#include "MParty.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "UserInformation.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCPartyJoined.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

// The party the player is in, as the server lists it, replaces the old
// one: the old members leave, and each listed member but the player
// joins with its sex, face, address and what the old party knew of it.
// A member in the zone is in sight with its hit points, the player's
// zone, its cell and guild, and is marked as the player's party; one
// elsewhere is out of sight in no zone the client knows. The asking
// windows close, the party window opens when the party changed size, a
// party that was empty starts its clock, and the player's information
// goes to the party.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
bool zoneExists = true;

struct FakeCreature
{
	TYPE_OBJECTID id;
	TYPE_SECTORPOSITION serverX, serverY;
	int guild;
	MStatus status;
};
FakeCreature ana{1004, 21, 22, 5, {}};
std::map<std::string, TYPE_OBJECTID> ids;
int me;

std::string N(long long value) { return std::to_string(value); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
const MonotonicClock::TimePoint kNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000);
const MonotonicClock::TimePoint kJoined = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000);

const MPartyHost partyHost = []() {
	MPartyHost host{};
	host.LeaveByName = [](const char* name) { calls.push_back(std::string("leaves|") + name); };
	host.JoinByName = [](const char*) { return static_cast<TYPE_OBJECTID>(OBJECTID_NULL); };
	host.CurrentTime = []() { return kNow; };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.SendInfoToParty = []() { calls.push_back("player's information sent"); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreatureID = [](const char* pName, int flag) -> TYPE_OBJECTID {
		CHECK_EQ(1, flag);
		const auto found = ids.find(pName);
		return found == ids.end() ? OBJECTID_NULL : found->second;
	};
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* { return id == ana.id ? reinterpret_cast<MCreature*>(&ana) : nullptr; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.Status = [](MCreature* p) { return &Fake(p).status; };
	host.GetServerX = [](MCreature* p) { return Fake(p).serverX; };
	host.GetServerY = [](MCreature* p) { return Fake(p).serverY; };
	host.GetGuildNumber = [](MCreature* p) { return Fake(p).guild; };
	host.SetPlayerParty = [](MCreature* p) { calls.push_back("in the party|" + N(Fake(p).id)); };
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.ClosePartyCancel = []() { calls.push_back("cancel? closed"); };
	host.ClosePartyAsk = []() { calls.push_back("ask closed"); };
	host.ClosePartyRequest = []() { calls.push_back("request closed"); };
	host.RunParty = []() { calls.push_back("party window"); };
	host.GetFaceStyle = [](bool male, int hairStyle) { return hairStyle + (male ? 100 : 200); };
	return host;
}();

struct World : GameModelWorld
{
	TempInformation temp;
	MParty* oldParty = g_pParty;
	TempInformation* oldTemp = g_pTempInformation;
	int oldLarge = g_nZoneLarge;
	int oldSmall = g_nZoneSmall;
	bool oldInLarge = g_bZonePlayerInLarge;
	const MPartyHost* oldPartyHost = MParty::SetHost(&partyHost);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	World()
	{
		calls.clear();
		hasPlayer = true;
		zoneExists = true;
		ana = FakeCreature{1004, 21, 22, 5, {}};
		ana.status.SetStatus(MODIFY_CURRENT_HP, 70);
		ana.status.SetStatus(MODIFY_MAX_HP, 90);
		ids = {{"Ana", 1004}, {"Cy", 1009}};
		g_pUserInformation->CharacterID = "Me";
		g_pParty = new MParty;
		g_pTempInformation = &temp;
		g_nZoneLarge = 12;
		g_nZoneSmall = 13;
		g_bZonePlayerInLarge = true;
	}
	~World()
	{
		delete g_pParty;
		CommunicationPresentation::SetHost(oldCommunication);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		MParty::SetHost(oldPartyHost);
		g_pTempInformation = oldTemp;
		g_pParty = oldParty;
		g_bZonePlayerInLarge = oldInLarge;
		g_nZoneSmall = oldSmall;
		g_nZoneLarge = oldLarge;
	}
	// A member of the party before the list arrives.
	void Knew(const char* name)
	{
		PARTY_INFO* pInfo = new PARTY_INFO;
		pInfo->Name = name;
		pInfo->HP = 50;
		pInfo->MaxHP = 80;
		pInfo->zoneID = 7;
		pInfo->zoneX = 3;
		pInfo->zoneY = 4;
		pInfo->guildID = 9;
		CHECK(g_pParty->AddMember(pInfo));
		calls.clear();
	}
};

void Receive(std::initializer_list<PARTY_MEMBER_INFO> members)
{
	GCPartyJoined packet;
	for (const PARTY_MEMBER_INFO& member : members) packet.addMemberInfo(new PARTY_MEMBER_INFO(member));
	GCPartyJoinedHandler::execute(&packet, nullptr);
}

// 1.2.3.4 as the server writes an address: its bytes in that order.
const IP_t kAddress = []() {
	IP_t ip = 0;
	const unsigned char bytes[4] = {1, 2, 3, 4};
	std::memcpy(&ip, bytes, sizeof(ip));
	return ip;
}();

} // namespace

TEST(PartyJoinedReplies, TheListedPartyReplacesTheOldOne)
{
	World world;
	world.Knew("Bo");
	world.Knew("Ana");
	MParty* pOld = g_pParty;
	pOld->SetJoinTime(kJoined);
	Receive({{"Me", 1, 1, kAddress}, {"Ana", 1, 3, kAddress}, {"Bo", 0, 4, 0}, {"Cy", 0, 5, 0}});
	CHECK(g_pParty != nullptr && g_pParty != pOld);
	CHECK(calls == std::vector<std::string>({"leaves|Bo", "leaves|Ana", "in the party|1004",
		"cancel? closed", "ask closed", "request closed", "party window", "player's information sent"}));
	CHECK_EQ(3, g_pParty->GetSize());
	CHECK(g_pParty->GetJoinTime() == kJoined);
	CHECK(g_pParty->GetMemberInfo("Me") == nullptr);
	// In sight: what the zone knows of it, over what the old party knew.
	const PARTY_INFO* pAna = g_pParty->GetMemberInfo("Ana");
	CHECK(pAna != nullptr);
	if (pAna != nullptr)
	{
		CHECK(pAna->bMale && pAna->hairStyle == 103);
		CHECK(pAna->IP == "1.2.3.4");
		CHECK(pAna->bInSight);
		CHECK(pAna->HP == 70 && pAna->MaxHP == 90);
		CHECK(pAna->zoneID == 12 && pAna->zoneX == 21 && pAna->zoneY == 22 && pAna->guildID == 5);
	}
	// Out of the zone: what the old party knew, in no zone the client knows.
	const PARTY_INFO* pBo = g_pParty->GetMemberInfo("Bo");
	CHECK(pBo != nullptr);
	if (pBo != nullptr)
	{
		CHECK(!pBo->bMale && pBo->hairStyle == 204);
		CHECK(!pBo->bInSight);
		CHECK(pBo->HP == 50 && pBo->MaxHP == 80 && pBo->zoneX == 3 && pBo->zoneY == 4 && pBo->guildID == 9);
		CHECK_EQ(60002, pBo->zoneID);
	}
	// In the zone's list but not there: out of sight, its zone unknown.
	const PARTY_INFO* pCy = g_pParty->GetMemberInfo("Cy");
	CHECK(pCy != nullptr);
	if (pCy != nullptr)
	{
		CHECK(!pCy->bInSight);
		CHECK_EQ(0, pCy->zoneID);
		CHECK(pCy->HP == 100 && pCy->MaxHP == 100);
	}
}

TEST(PartyJoinedReplies, AMemberInSightInTheSmallZoneAndWithoutAStatus)
{
	World world;
	world.Knew("Ana");
	g_bZonePlayerInLarge = false;
	// Without the creature's status: its hit points stay as the old party
	// knew them.
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	Receive({{"Ana", 1, 3, kAddress}});
	// The local host is gone before the World's teardown.
	CreatureState::SetHost(&creature);
	const PARTY_INFO* pAna = g_pParty->GetMemberInfo("Ana");
	CHECK(pAna != nullptr);
	if (pAna == nullptr) return;
	CHECK(pAna->bInSight);
	CHECK_EQ(13, pAna->zoneID);
	CHECK(pAna->HP == 50 && pAna->MaxHP == 80);
}

TEST(PartyJoinedReplies, AFirstPartyStartsItsClockAndASameSizedOneShowsNoWindow)
{
	World world;
	Receive({{"Ana", 1, 3, kAddress}});
	CHECK(g_pParty->GetJoinTime() == kNow);
	CHECK(calls == std::vector<std::string>({"in the party|1004", "cancel? closed", "ask closed", "request closed",
		"party window", "player's information sent"}));
	// The same one member again: no window, the clock kept.
	calls.clear();
	g_pParty->SetJoinTime(kJoined);
	Receive({{"Ana", 1, 3, kAddress}});
	CHECK(calls == std::vector<std::string>({"leaves|Ana", "in the party|1004", "cancel? closed", "ask closed",
		"request closed", "player's information sent"}));
	CHECK(g_pParty->GetJoinTime() == kJoined);
}

TEST(PartyJoinedReplies, WithoutItsPiecesNothingChanges)
{
	World world;
	world.Knew("Bo");
	MParty* pParty = g_pParty;
	hasPlayer = false;
	Receive({{"Ana", 1, 3, kAddress}});
	hasPlayer = true;
	zoneExists = false;
	Receive({{"Ana", 1, 3, kAddress}});
	zoneExists = true;
	g_pTempInformation = nullptr;
	Receive({{"Ana", 1, 3, kAddress}});
	g_pTempInformation = &world.temp;
	UserInformation* pUser = g_pUserInformation;
	g_pUserInformation = nullptr;
	Receive({{"Ana", 1, 3, kAddress}});
	g_pUserInformation = pUser;
	g_pParty = nullptr;
	Receive({{"Ana", 1, 3, kAddress}});
	g_pParty = pParty;
	CHECK(calls.empty());
	CHECK(g_pParty == pParty && g_pParty->GetSize() == 1);
	// Without the hosts the windows stay, the face is the style and
	// nothing is sent.
	CommunicationPresentation::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	CommunicationPresentation::RunParty();
	CHECK_EQ(7, CommunicationPresentation::GetFaceStyle(true, 7));
	PlayerState::SendInfoToParty();
	ZoneState::SetHost(nullptr);
	CreatureState::SetHost(nullptr);
	CHECK(ZoneState::GetCreatureID("Ana", 1) == OBJECTID_NULL);
	CHECK_EQ(0, CreatureState::GetGuildNumber(reinterpret_cast<MCreature*>(&ana)));
	CreatureState::SetPlayerParty(reinterpret_cast<MCreature*>(&ana));
	CHECK(calls.empty());
}
