#include "test_framework.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "SkillDowngradeHost.h"
#include "MSkillManager.h"
#include "MGameStringTable.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "Gpackets/GCDownSkillOK.h"
#include "Gpackets/GCDownSkillFailed.h"
#include <limits>
#include <vector>

namespace {
std::vector<int> messages, levels;
std::vector<MSkillDomain::SKILLSTATUS> statuses;
const ACTIONINFO skillID = SKILL_FLOURISH;
const SkillDowngrade::Host host{
	.PopupMessage = [](int id) {
		messages.push_back(id);
		levels.push_back(g_pSkillInfoTable ? (*g_pSkillInfoTable)[skillID].GetExpLevel() : -1);
		statuses.push_back(g_pSkillManager ? (*g_pSkillManager)[SKILLDOMAIN_OUSTERS].GetSkillStatus(skillID) : MSkillDomain::SKILLSTATUS_NULL);
	},
};
struct World
{
	MSkillInfoTable table;
	MSkillManager manager;
	MSkillInfoTable* oldTable = g_pSkillInfoTable;
	MSkillManager* oldManager = g_pSkillManager;
	MSkillSet* oldAvailable = g_pSkillAvailable;
	const SkillDowngrade::Host* oldHost = SkillDowngrade::SetHost(&host);
	World()
	{
		g_pSkillInfoTable = &table; g_pSkillManager = &manager; g_pSkillAvailable = nullptr;
		manager.CTypeTable<MSkillDomain>::Init(MAX_SKILLDOMAIN);
		auto& row = Row(); row.Set(0, "skill", 0, 0, 0, "skill"); row.SetSkillStep(SKILL_STEP_OUSTERS_COMBAT);
		row.CanDelete = 1; row.SetExpLevel(3); row.SetSkillExp(123); row.SetEnable(true);
		auto& domain = Domain(); domain.SetRootSkill(skillID); domain.SetNewSkill(); CHECK(domain.LearnSkill(skillID));
		row.SetDelayTime(2345);
		ResetMessages();
	}
	~World()
	{
		SkillDowngrade::SetHost(oldHost); g_pSkillAvailable = oldAvailable; g_pSkillManager = oldManager; g_pSkillInfoTable = oldTable;
	}
	SKILLINFO_NODE& Row() { return testfw::MutableRow(table, skillID); }
	MSkillDomain& Domain() { return testfw::MutableRow(manager, SKILLDOMAIN_OUSTERS); }
	void ResetMessages() { messages.clear(); levels.clear(); statuses.clear(); }
};

template<class PacketType>
void Read(PacketType& packet, const std::vector<unsigned char>& bytes)
{
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream); CHECK(stream.isEmpty()); CHECK_EQ(bytes.size(), packet.getPacketSize());
}
void Down(int id = skillID)
{
	GCDownSkillOK packet;
	Read(packet, {static_cast<unsigned char>(id), static_cast<unsigned char>(id >> 8)});
	CHECK_EQ(id, packet.getSkillType()); GCDownSkillOKHandler::execute(&packet, nullptr);
	CHECK_EQ(id, packet.getSkillType());
}
void Failed(int description)
{
	GCDownSkillFailed packet;
	Read(packet, {static_cast<unsigned char>(skillID), static_cast<unsigned char>(skillID >> 8), static_cast<unsigned char>(description)});
	CHECK_EQ(skillID, packet.getSkillType()); CHECK_EQ(description, packet.getDesc());
	GCDownSkillFailedHandler::execute(&packet, nullptr);
	CHECK_EQ(description, packet.getDesc());
}
}

TEST(SkillDowngradeHandlers, ValidDecrementAndRelearningPrecedeSuccessPresentation)
{
	World world;
	Down(); CHECK_EQ(2, world.Row().GetExpLevel()); CHECK(levels == std::vector<int>{2});
	CHECK(statuses == std::vector<MSkillDomain::SKILLSTATUS>{MSkillDomain::SKILLSTATUS_LEARNED});
	CHECK(messages == std::vector<int>{STRING_MESSAGE_SUCCESS_CHANGE});
	world.ResetMessages(); world.Row().SetExpLevel(1); Down();
	CHECK_EQ(0, world.Row().GetExpLevel()); CHECK(levels == std::vector<int>{0});
	CHECK(statuses == std::vector<MSkillDomain::SKILLSTATUS>{MSkillDomain::SKILLSTATUS_NEXT});
	CHECK(messages == std::vector<int>{STRING_MESSAGE_SUCCESS_CHANGE});
	CHECK_EQ(123, world.Row().GetSkillExp()); CHECK_EQ(2345, world.Row().GetDelayTime()); CHECK(world.Row().IsEnable());
	// A repeated successful reply after reaching zero cannot downgrade again.
	world.ResetMessages(); Down(); CHECK_EQ(0, world.Row().GetExpLevel()); CHECK(messages.empty());
	CHECK_EQ(MSkillDomain::SKILLSTATUS_NEXT, world.Domain().GetSkillStatus(skillID));
}

TEST(SkillDowngradeHandlers, RelearningPreservesCanDeleteAndDomainMembershipPolicy)
{
	World world; world.Row().SetExpLevel(1); world.Row().CanDelete = 0; Down();
	CHECK_EQ(0, world.Row().GetExpLevel()); CHECK_EQ(MSkillDomain::SKILLSTATUS_LEARNED, world.Domain().GetSkillStatus(skillID));
	world.ResetMessages(); world.Domain().Clear(); world.Row().CanDelete = 1; world.Row().SetExpLevel(1); Down();
	CHECK_EQ(0, world.Row().GetExpLevel()); CHECK_EQ(MSkillDomain::SKILLSTATUS_NULL, world.Domain().GetSkillStatus(skillID));
	CHECK(messages == std::vector<int>{STRING_MESSAGE_SUCCESS_CHANGE});
}

TEST(SkillDowngradeHandlers, EveryFailureDescriptionUsesExistingMessageWithoutChangingSkills)
{
	World world;
	const int known[] = {STRING_ERROR_ETC_ERROR, STRING_MESSAGE_NOT_OUSTERS, STRING_MESSAGE_TOO_LOW_SKILL_LEVEL,
		STRING_MESSAGE_TOO_HIGH_SKILL_LEVEL, STRING_MESSAGE_NOT_ENOUGH_MONEY_FOR_DOWN_SKILL, STRING_MESSAGE_INVALID_SKILL,
		STRING_MESSAGE_NOT_LEARNED_SKILL, STRING_MESSAGE_CANNOT_SKILLTREE_DELETE};
	for (int desc = 0; desc < 256; ++desc)
	{
		world.ResetMessages(); Failed(desc);
		int expected = STRING_ERROR_ETC_ERROR;
		if (desc < 8) expected = known[desc];
		CHECK_EQ(1, messages.size()); CHECK_EQ(expected, messages.front());
		CHECK_EQ(3, world.Row().GetExpLevel()); CHECK_EQ(MSkillDomain::SKILLSTATUS_LEARNED, world.Domain().GetSkillStatus(skillID));
	}
}

TEST(SkillDowngradeHandlers, InvalidSkillIDsRejectWithoutSuccessOrNeighbourMutation)
{
	World world;
	for (const int id : {world.table.GetSize(), 65535})
	{
		world.ResetMessages(); Down(id); CHECK(messages.empty()); CHECK_EQ(3, world.Row().GetExpLevel());
		CHECK_EQ(MSkillDomain::SKILLSTATUS_LEARNED, world.Domain().GetSkillStatus(skillID));
	}
}

TEST(SkillDowngradeHandlers, NonpositiveLevelsRejectWithoutDecrementOrSuccess)
{
	World world;
	for (const int level : {0, -1, (std::numeric_limits<int>::min)()})
	{
		world.ResetMessages(); world.Row().SetExpLevel(level); Down();
		CHECK_EQ(level, world.Row().GetExpLevel()); CHECK(messages.empty());
		CHECK_EQ(MSkillDomain::SKILLSTATUS_LEARNED, world.Domain().GetSkillStatus(skillID));
	}
}

TEST(SkillDowngradeHandlers, MissingOrUnallocatedSkillTablesRejectWithoutPresentation)
{
	World world; g_pSkillInfoTable = nullptr; Down(); CHECK(messages.empty()); CHECK_EQ(3, world.Row().GetExpLevel());
	g_pSkillInfoTable = &world.table; world.table.Release(); Down(); CHECK(messages.empty());
	world.table.CTypeTable<SKILLINFO_NODE>::Init(skillID); Down(); CHECK(messages.empty());
}

TEST(SkillDowngradeHandlers, MissingManagerOrDomainSkipsOnlyTheRelearningUpdate)
{
	World world;
	for (const int start : {1, 3})
	{
		g_pSkillManager = nullptr; world.ResetMessages(); world.Row().SetExpLevel(start); Down();
		CHECK_EQ(start - 1, world.Row().GetExpLevel()); CHECK(messages == std::vector<int>{STRING_MESSAGE_SUCCESS_CHANGE});
		CHECK_EQ(MSkillDomain::SKILLSTATUS_LEARNED, world.Domain().GetSkillStatus(skillID));
	}
	g_pSkillManager = &world.manager; world.manager.Release();
	world.ResetMessages(); world.Row().SetExpLevel(1); Down();
	CHECK_EQ(0, world.Row().GetExpLevel()); CHECK(messages == std::vector<int>{STRING_MESSAGE_SUCCESS_CHANGE});
	world.manager.CTypeTable<MSkillDomain>::Init(SKILLDOMAIN_OUSTERS);
	world.ResetMessages(); world.Row().SetExpLevel(1); Down();
	CHECK_EQ(0, world.Row().GetExpLevel()); CHECK(messages == std::vector<int>{STRING_MESSAGE_SUCCESS_CHANGE});
}

TEST(SkillDowngradeHandlers, MissingOrReplacedPopupServicesPreserveModelUpdates)
{
	World world; const SkillDowngrade::Host empty{};
	for (const auto* services : {static_cast<const SkillDowngrade::Host*>(nullptr), &empty})
	{
		SkillDowngrade::SetHost(services); world.Row().SetExpLevel(3); Down(); Failed(1);
		CHECK_EQ(2, world.Row().GetExpLevel()); CHECK(messages.empty());
	}
	const SkillDowngrade::Host replacing{
		.PopupMessage = [](int id) { host.PopupMessage(id); SkillDowngrade::SetHost(nullptr); },
	};
	SkillDowngrade::SetHost(&replacing); world.Row().SetExpLevel(3); Down();
	CHECK(messages == std::vector<int>{STRING_MESSAGE_SUCCESS_CHANGE}); world.ResetMessages();
	Down(); Failed(1); CHECK_EQ(1, world.Row().GetExpLevel()); CHECK(messages.empty());
	CHECK(SkillDowngrade::SetHost(&host) == nullptr); Failed(1);
	CHECK(messages == std::vector<int>{STRING_MESSAGE_NOT_OUSTERS});
}

TEST(SkillDowngradeHandlers, FailurePresentationNeedsNoModelAndMaximumPositiveLevelDecrements)
{
	World world; world.Row().SetExpLevel((std::numeric_limits<int>::max)()); Down();
	CHECK_EQ((std::numeric_limits<int>::max)() - 1, world.Row().GetExpLevel());
	world.ResetMessages(); g_pSkillInfoTable = nullptr; g_pSkillManager = nullptr; Failed(7);
	CHECK(messages == std::vector<int>{STRING_MESSAGE_CANNOT_SKILLTREE_DELETE});
}
