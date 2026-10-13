#include "test_framework.h"
#include "packet_stream_access.h"
#include "QuestWarPresentationHost.h"
#include "TempInformation.h"
#include "MonotonicClock.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCBloodBibleStatus.h"
#include "Gpackets/GCFlagWarStatus.h"
#include "Gpackets/GCQuestStatus.h"
#include "Gpackets/GCSelectQuestID.h"
#include "Gpackets/GCWarScheduleList.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

// Quest and war status replies present what they receive through
// QuestWarPresentation::Host. The quest list also opens the quest
// selection request, and the flag-war reply turns its remaining seconds
// into an end time and ignores more than three hours.
namespace {

std::vector<std::string> calls;
MonotonicClock::TimePoint ctfEnd;
int modeAtQuestList = -1;

std::string Address(const void* pointer) { return std::to_string(reinterpret_cast<std::uintptr_t>(pointer)); }

const QuestWarPresentation::Host host{
	.RunQuestStatus = [](GCQuestStatus* p) { calls.push_back("queststatus|" + Address(p)); },
	.RunQuestList = [](GCSelectQuestID* p) {
		modeAtQuestList = g_pTempInformation ? static_cast<int>(g_pTempInformation->GetMode()) : -1;
		calls.push_back("questlist|" + Address(p));
	},
	.RunWarList = [](GCWarScheduleList* p) { calls.push_back("warlist|" + Address(p)); },
	.SetCTFStatus = [](const MonotonicClock::TimePoint& endTime, int flag_s, int flag_v, int flag_o) {
		ctfEnd = endTime;
		calls.push_back("ctf|" + std::to_string(flag_s) + "|" + std::to_string(flag_v) + "|" + std::to_string(flag_o));
	},
	.SetBloodBibleStatus = [](GCBloodBibleStatus* p) { calls.push_back("bible|" + Address(p)); },
};

struct World
{
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const QuestWarPresentation::Host* oldHost = QuestWarPresentation::SetHost(&host);
	World()
	{
		g_pTempInformation = &temp;
		calls.clear();
		modeAtQuestList = -1;
	}
	~World()
	{
		QuestWarPresentation::SetHost(oldHost);
		g_pTempInformation = oldTemp;
	}
};

void ReceiveFlagWar(WORD secondsLeft, BYTE slayers, BYTE vampires, BYTE ousters)
{
	GCFlagWarStatus original;
	original.setTimeRemain(secondsLeft);
	original.setFlagCount(RACE_SLAYER, slayers);
	original.setFlagCount(RACE_VAMPIRE, vampires);
	original.setFlagCount(RACE_OUSTERS, ousters);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCFlagWarStatus packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCFlagWarStatusHandler::execute(&packet, nullptr);
}

// One reply for each of the five host entries.
void RunEveryHandler()
{
	GCQuestStatus status;
	GCSelectQuestID list;
	GCWarScheduleList wars;
	GCBloodBibleStatus bible;
	GCQuestStatusHandler::execute(&status, nullptr);
	GCSelectQuestIDHandler::execute(&list, nullptr);
	GCWarScheduleListHandler::execute(&wars, nullptr);
	ReceiveFlagWar(60, 1, 1, 1);
	GCBloodBibleStatusHandler::execute(&bible, nullptr);
}

} // namespace

TEST(QuestWarPresentationHandlers, StatusAndListRepliesPassTheirPacketThrough)
{
	World world;
	GCQuestStatus status;
	GCWarScheduleList wars;
	GCBloodBibleStatus bible;
	GCQuestStatusHandler::execute(&status, nullptr);
	GCWarScheduleListHandler::execute(&wars, nullptr);
	GCBloodBibleStatusHandler::execute(&bible, nullptr);
	CHECK_EQ(3u, calls.size());
	CHECK(calls.size() == 3 && calls[0] == "queststatus|" + Address(&status));
	CHECK(calls.size() == 3 && calls[1] == "warlist|" + Address(&wars));
	CHECK(calls.size() == 3 && calls[2] == "bible|" + Address(&bible));
}

TEST(QuestWarPresentationHandlers, TheQuestListOpensTheSelectionRequestAfterPresenting)
{
	World world;
	GCSelectQuestID list;
	GCSelectQuestIDHandler::execute(&list, nullptr);
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "questlist|" + Address(&list));
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), modeAtQuestList);
	CHECK(world.temp.GetMode() == TempInformation::MODE_SELECT_QUEST);

	world.temp.SetMode(TempInformation::MODE_NULL);
	QuestWarPresentation::SetHost(nullptr);
	GCSelectQuestIDHandler::execute(&list, nullptr);
	CHECK(world.temp.GetMode() == TempInformation::MODE_SELECT_QUEST);
}

TEST(QuestWarPresentationHandlers, FlagWarStatusEndsAfterTheRemainingSeconds)
{
	World world;
	const MonotonicClock::TimePoint before = MonotonicClock::Now();
	ReceiveFlagWar(600, 3, 2, 1);
	const MonotonicClock::TimePoint after = MonotonicClock::Now();
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "ctf|3|2|1");
	CHECK(ctfEnd >= before + std::chrono::seconds(600));
	CHECK(ctfEnd <= after + std::chrono::seconds(600));
}

TEST(QuestWarPresentationHandlers, FlagWarStatusIgnoresMoreThanThreeHours)
{
	World world;
	ReceiveFlagWar(14399, 1, 0, 0);
	ReceiveFlagWar(14400, 2, 0, 0);
	ReceiveFlagWar(0xFFFF, 3, 0, 0);
	ReceiveFlagWar(0, 4, 0, 0);
	CHECK_EQ(2u, calls.size());
	CHECK(calls.size() == 2 && calls[0] == "ctf|1|0|0");
	CHECK(calls.size() == 2 && calls[1] == "ctf|4|0|0");
}

TEST(QuestWarPresentationHandlers, MissingHostsAndEntriesSkipPresentationOnly)
{
	World world;
	QuestWarPresentation::SetHost(nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	const QuestWarPresentation::Host empty{};
	CHECK(QuestWarPresentation::SetHost(&empty) == nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(QuestWarPresentation::SetHost(&host) == &empty);
	RunEveryHandler();
	CHECK_EQ(5u, calls.size());
}
