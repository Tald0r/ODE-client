#include "test_framework.h"
#include "MParty.h"
#include "ClientConfig.h"
#include <memory>
#include <string>
#include <vector>

namespace {
struct HostScope {
	const MPartyHost* previous;
	explicit HostScope(const MPartyHost* host) : previous(MParty::SetHost(host)) {}
	~HostScope() { MParty::SetHost(previous); }
};
struct ConfigScope {
	ClientConfig* previous = g_pClientConfig;
	~ConfigScope() { g_pClientConfig = previous; }
};
std::vector<std::string> calls;
MonotonicClock::TimePoint frame;
bool JoinID(TYPE_OBJECTID id, MString& name)
{
	calls.push_back("join-id:" + std::to_string(id));
	if (id != 17) return false;
	name = "Resolved";
	return true;
}
TYPE_OBJECTID JoinName(const char* name)
{
	calls.push_back(std::string("join-name:") + name);
	return 23;
}
void LeaveID(TYPE_OBJECTID id) { calls.push_back("leave-id:" + std::to_string(id)); }
void LeaveName(const char* name) { calls.push_back(std::string("leave-name:") + name); }
MonotonicClock::TimePoint Clock() { return frame; }
const MPartyHost host{
	.JoinByID = JoinID,
	.JoinByName = JoinName,
	.LeaveByID = LeaveID,
	.LeaveByName = LeaveName,
	.CurrentTime = Clock,
};
std::unique_ptr<PARTY_INFO> Member(const char* name, TYPE_OBJECTID id)
{
	auto row = std::make_unique<PARTY_INFO>();
	row->Name = name;
	row->ID = id;
	row->IP = "127.0.0.1";
	return row;
}
}

TEST(PartyModel, OwnsCompleteRecordsWithoutAHost)
{
	HostScope scope(nullptr);
	MParty party;
	CHECK_EQ(0, party.GetSize());
	CHECK(party.IsAcceptInvite());
	party.RefuseInvite();
	CHECK(!party.IsAcceptInvite());
	party.AcceptInvite();
	CHECK(party.IsAcceptInvite());
	CHECK(!party.AddMember(nullptr));
	auto empty = Member(nullptr, OBJECTID_NULL);
	CHECK(!party.AddMember(empty.get()));
	auto unresolved = Member(nullptr, 99);
	CHECK(!party.AddMember(unresolved.get()));
	auto complete = Member("Known", 12);
	auto* saved = complete.get();
	CHECK(party.AddMember(complete.get()));
	complete.release();
	CHECK_EQ(1, party.GetSize());
	CHECK(party.GetMemberInfo(0) == saved);
	CHECK(party.GetMemberInfo("Known") == saved);
	CHECK(party.GetMemberInfoByIP("127.0.0.1") == saved);
	CHECK(party.HasMember("Known"));
	CHECK(!party.HasMember("Absent"));
	CHECK(party.GetMemberInfo(-1) == nullptr);
	party.UnSetPlayerParty();
	CHECK(party.RemoveMember(12));
	CHECK_EQ(0, party.GetSize());
	CHECK(!party.RemoveMember(12));
	CHECK(party.IsKickAvailableTime());
}

TEST(PartyModel, ResolvesMissingIdentityAndForwardsOnlyTheOriginalActions)
{
	HostScope scope(&host);
	calls.clear();
	MParty party;
	auto byID = Member(nullptr, 17);
	CHECK(party.AddMember(byID.get()));
	byID.release();
	CHECK(std::string(party.GetMemberInfo(0)->Name.GetString()) == "Resolved");
	auto byName = Member("Named", OBJECTID_NULL);
	CHECK(party.AddMember(byName.get()));
	byName.release();
	CHECK_EQ(23, party.GetMemberInfo(1)->ID);
	auto complete = Member("Complete", 30);
	CHECK(party.AddMember(complete.get()));
	complete.release();
	CHECK_EQ(2, calls.size()); // Fully populated records do not mark a creature.
	auto missing = Member(nullptr, 404);
	CHECK(!party.AddMember(missing.get()));
	CHECK_EQ(3, party.GetSize());
	party.UnSetPlayerParty();
	CHECK(party.RemoveMember("Named"));
	CHECK(party.RemoveMember(17));
	const std::vector<std::string> expected{
		"join-id:17", "join-name:Named", "join-id:404",
		"leave-name:Resolved", "leave-name:Named", "leave-name:Complete",
		"leave-name:Named", "leave-id:17"
	};
	CHECK(calls == expected);
	party.Release();
	CHECK_EQ(0, party.GetSize());
	CHECK(calls == expected); // Release has never changed live creature flags.
}

TEST(PartyModel, EmptyCallbacksRetainOffscreenMembersAndSkipLiveActions)
{
	const MPartyHost empty{};
	HostScope scope(&empty);
	MParty party;
	auto named = Member("Offscreen", OBJECTID_NULL);
	CHECK(party.AddMember(named.get()));
	named.release();
	CHECK_EQ(OBJECTID_NULL, party.GetMemberInfo(0)->ID);
	CHECK(party.IsKickAvailableTime());
	party.SetJoinTime();
	CHECK(party.GetJoinTime() == MonotonicClock::TimePoint());
	party.UnSetPlayerParty();
	CHECK(party.RemoveMember("Offscreen"));
}

TEST(PartyModel, KickDelayReadsCurrentClockAndConfigurationWithStrictBoundary)
{
	HostScope scope(&host);
	ConfigScope configScope;
	ClientConfig config;
	CHECK_EQ(config.AFTER_PARTY_KICK_DELAY, MParty::DefaultKickDelayMs);
	g_pClientConfig = nullptr;
	frame = MonotonicClock::TimePoint(MonotonicClock::Millis(4294967000LL));
	MParty party;
	party.SetJoinTime();
	const auto joined = party.GetJoinTime();
	frame = joined + MonotonicClock::Millis(MParty::DefaultKickDelayMs);
	CHECK(!party.IsKickAvailableTime());
	frame += MonotonicClock::Millis(1);
	CHECK(party.IsKickAvailableTime());
	g_pClientConfig = &config;
	config.AFTER_PARTY_KICK_DELAY = 20;
	frame = joined + MonotonicClock::Millis(20);
	CHECK(!party.IsKickAvailableTime());
	frame += MonotonicClock::Millis(1);
	CHECK(party.IsKickAvailableTime());
	config.AFTER_PARTY_KICK_DELAY = 30;
	CHECK(!party.IsKickAvailableTime());
	party.SetJoinTime(frame);
	CHECK(party.GetJoinTime() == frame);
}

TEST(PartyModel, IndexQueriesOnlyReturnOccupiedMembers)
{
	HostScope scope(nullptr);
	MParty party;
	CHECK(party.GetMemberInfo(0) == nullptr);
	CHECK(party.GetMemberInfo(5) == nullptr);
	auto first = Member("One", 1);
	CHECK(party.AddMember(first.get()));
	first.release();
	CHECK(party.GetMemberInfo(0) != nullptr);
	CHECK(party.GetMemberInfo(1) == nullptr);
	CHECK(party.GetMemberInfo(5) == nullptr);
	CHECK(party.RemoveMember(1));
	CHECK(party.GetMemberInfo(0) == nullptr);
}

TEST(PartyModel, MemberIsInSightOnlyInThePlayersZoneAndWithinOneAndAHalfSight)
{
	// Sight 13 gives a range of 13 + 6 = 19 tiles, summed over X and Y.
	CHECK(MParty::IsMemberInSight(13, 100, 100, 13, 102, 101, 13));
	CHECK(MParty::IsMemberInSight(13, 100, 100, 13, 110, 109, 13));
	CHECK(!MParty::IsMemberInSight(13, 100, 100, 13, 110, 110, 13));
	CHECK(!MParty::IsMemberInSight(13, 0, 0, 13, 100, 100, 13));
	// A member in another zone is not in sight, whatever its coordinates.
	CHECK(!MParty::IsMemberInSight(14, 100, 100, 13, 100, 100, 13));
	CHECK(!MParty::IsMemberInSight(14, 100, 100, 13, 102, 101, 13));
}
