#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "VsUiPresentationHost.h"
#include "QuestStatusInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCGQuestStatusInfo.h"
#include <string>
#include <vector>

// The full quest list rebuilds the quest window: the window is closed if
// it runs and opened again (loading its quest XML), each quest becomes a
// UI record (the quest's ID and status, and each mission's condition,
// index, status and arguments, untimed) handed to it in the list's order,
// and the XML is released. The window keeps a record it takes; one it
// refuses, or one with no window to go to, is freed.
namespace {

std::vector<std::string> calls;
bool running = false;
bool takes = true;
std::vector<UI_GQuestInfo*> kept;

std::string N(long long value) { return std::to_string(value); }

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.IsRunningQuestManager = []() { return running; };
	host.CloseQuestManager = []() { calls.push_back("closed"); };
	host.RunQuestManager = []() { calls.push_back("opened"); };
	host.SetQuestManagerInfo = [](UI_GQuestInfo* pInfo) {
		std::string call = "quest|" + N(pInfo->dwQuestID) + "|" + N(pInfo->bStatus);
		for (const UI_GMissionInfo* mission : pInfo->vMissionList)
			call += "|" + N(mission->bCondition) + "/" + N(mission->bIndex) + "/" + N(mission->bStatus) + "/"
				+ mission->m_StrArg + "/" + N(mission->m_NumArg) + "/" + N(mission->bTimeLimited);
		calls.push_back(call);
		if (takes) kept.push_back(pInfo);
		return takes;
	};
	host.ReleaseQuestXML = []() { calls.push_back("released"); };
	return host;
}();

struct World : GameModelWorld
{
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	World()
	{
		calls.clear();
		running = false;
		takes = true;
	}
	~World()
	{
		// The quest window owns what it took.
		for (UI_GQuestInfo* info : kept)
		{
			for (UI_GMissionInfo* mission : info->vMissionList) delete mission;
			delete info;
		}
		kept.clear();
		VsUiPresentation::SetHost(oldUi);
	}
};

MissionInfo Mission(BYTE condition, WORD index, BYTE status, const char* text, DWORD number)
{
	MissionInfo mission;
	mission.m_Condition = condition;
	mission.m_Index = index;
	mission.m_Status = status;
	mission.m_StrArg = text;
	mission.m_NumArg = number;
	return mission;
}

struct Quest
{
	DWORD id;
	BYTE status;
	std::vector<MissionInfo> missions;
};

// QuestStatusInfo has no setters, so the list's bytes are written here: the
// quest count, then per quest its ID, its status, the mission count and the
// missions.
void Receive(const std::vector<Quest>& quests)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(static_cast<BYTE>(quests.size()));
	for (const Quest& quest : quests)
	{
		encoded.write(quest.id);
		encoded.write(quest.status);
		encoded.write(static_cast<BYTE>(quest.missions.size()));
		for (const MissionInfo& mission : quest.missions) mission.write(encoded);
	}
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 4096);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCGQuestStatusInfo packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCGQuestStatusInfoHandler::execute(&packet, nullptr);
}

} // namespace

TEST(QuestListReplies, EachQuestGoesToTheReopenedWindowInOrder)
{
	World world;
	Receive({{4001, QuestStatusInfo::DOING, {Mission(1, 2, 3, "Bring nine bones", 9), Mission(0, 7, 1, "", 0)}},
		{4002, QuestStatusInfo::SUCCESS, {}}});
	CHECK(calls == std::vector<std::string>({"opened", "quest|4001|2|1/2/3/Bring nine bones/9/0|0/7/1//0/0",
		"quest|4002|" + N(QuestStatusInfo::SUCCESS), "released"}));
	CHECK_EQ(2u, kept.size());
	// A running window is closed first.
	calls.clear();
	running = true;
	Receive({});
	CHECK(calls == std::vector<std::string>({"closed", "opened", "released"}));
}

TEST(QuestListReplies, ARefusedRecordOrOneWithNoWindowIsFreed)
{
	World world;
	takes = false;
	Receive({{4003, QuestStatusInfo::FAIL, {Mission(1, 1, 1, "a", 1), Mission(2, 2, 2, "b", 2), Mission(3, 3, 3, "c", 3)}},
		{4004, QuestStatusInfo::FAIL, {}}});
	CHECK_EQ(4u, calls.size());
	VsUiPresentation::SetHost(nullptr);
	Receive({{4005, QuestStatusInfo::DOING, {Mission(1, 1, 1, "a", 1), Mission(1, 2, 1, "b", 1)}}});
	const VsUiPresentation::Host empty{};
	VsUiPresentation::SetHost(&empty);
	CHECK(!VsUiPresentation::IsRunningQuestManager());
	CHECK(!VsUiPresentation::SetQuestManagerInfo(nullptr));
	Receive({{4006, QuestStatusInfo::DOING, {Mission(1, 1, 1, "a", 1)}}});
	CHECK_EQ(4u, calls.size());
	// Nothing was kept. ASan sees a refused record freed twice; one never
	// freed only LeakSanitizer (the Linux ASan job) sees.
	CHECK(kept.empty());
}
