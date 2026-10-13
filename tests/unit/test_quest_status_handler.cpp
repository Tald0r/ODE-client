#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "VsUiPresentationHost.h"
#include "QuestStatusInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCGQuestStatusModify.h"
#include <string>
#include <vector>

// A quest's new status becomes a UI record (the quest's ID and status, and
// each mission's condition, index, status and arguments, untimed) handed to
// the quest window with the packet's kind less two. The window keeps a
// record it takes; one it refuses, or one with no window to go to, is freed.
namespace {

std::vector<std::string> calls;
bool takes = true;
std::vector<UI_GQuestInfo*> kept;

std::string N(long long value) { return std::to_string(value); }

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.ModifyQuestManagerInfo = [](UI_GQuestInfo* pInfo, int type) {
		std::string call = "quest|" + N(pInfo->dwQuestID) + "|" + N(pInfo->bStatus) + "|" + N(type);
		for (const UI_GMissionInfo* mission : pInfo->vMissionList)
			call += "|" + N(mission->bCondition) + "/" + N(mission->bIndex) + "/" + N(mission->bStatus) + "/"
				+ mission->m_StrArg + "/" + N(mission->m_NumArg) + "/" + N(mission->bTimeLimited);
		calls.push_back(call);
		if (takes) kept.push_back(pInfo);
		return takes;
	};
	return host;
}();

struct World : GameModelWorld
{
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	World()
	{
		calls.clear();
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

// QuestStatusInfo has no setters, so its bytes are written here: the kind,
// then the quest ID, its status, the mission count and the missions.
void Receive(BYTE kind, DWORD questID, BYTE status, const std::vector<MissionInfo>& missions)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(kind);
	encoded.write(questID);
	encoded.write(status);
	encoded.write(static_cast<BYTE>(missions.size()));
	for (const MissionInfo& mission : missions) mission.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 2048);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCGQuestStatusModify packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCGQuestStatusModifyHandler::execute(&packet, nullptr);
	// The packet never frees what its read allocates (a follow-up).
	delete packet.getInfo();
}

} // namespace

TEST(QuestStatusHandler, AQuestsStatusAndMissionsGoToTheQuestWindow)
{
	World world;
	Receive(3, 4001, QuestStatusInfo::DOING, {Mission(1, 2, 3, "Bring nine bones", 9), Mission(0, 7, 1, "", 0)});
	CHECK(calls == std::vector<std::string>{"quest|4001|2|1|1/2/3/Bring nine bones/9/0|0/7/1//0/0"});
	CHECK_EQ(1u, kept.size());
}

TEST(QuestStatusHandler, ARefusedRecordOrOneWithNoWindowIsFreed)
{
	World world;
	takes = false;
	Receive(2, 4002, QuestStatusInfo::SUCCESS, {Mission(1, 1, 1, "a", 1), Mission(2, 2, 2, "b", 2), Mission(3, 3, 3, "c", 3)});
	Receive(2, 4003, QuestStatusInfo::FAIL, {});
	CHECK_EQ(2u, calls.size());
	VsUiPresentation::SetHost(nullptr);
	Receive(4, 4004, QuestStatusInfo::DOING, {Mission(1, 1, 1, "a", 1)});
	const VsUiPresentation::Host empty{};
	VsUiPresentation::SetHost(&empty);
	Receive(4, 4005, QuestStatusInfo::DOING, {Mission(1, 1, 1, "a", 1), Mission(1, 2, 1, "b", 1)});
	CHECK_EQ(2u, calls.size());
	// Nothing was kept; ASan sees whether the refused records were freed
	// once each.
	CHECK(kept.empty());
}
