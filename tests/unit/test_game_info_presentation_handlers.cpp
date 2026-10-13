#include "test_framework.h"
#include "packet_stream_access.h"
#include "GameInfoPresentationHost.h"
#include "NPCInfo.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCMiniGameScores.h"
#include "Gpackets/GCNPCInfo.h"
#include "Gpackets/GCNotifyWin.h"
#include "Gpackets/GCTeachSkillInfo.h"
#include <cstdint>
#include <string>
#include <vector>

// NPC map, skill teaching, mini-game and prize-notice replies present what
// they receive through GameInfoPresentation::Host. The NPC list is cleared,
// then each decoded NPC is presented and freed; skill teaching also opens
// the skill-learn request.
namespace {

std::vector<std::string> calls;
int modeAtSkillTree = -1;

std::string Text(const char* text) { return text ? text : "(null)"; }
std::string Address(const void* pointer) { return std::to_string(reinterpret_cast<std::uintptr_t>(pointer)); }

const GameInfoPresentation::Host host{
	.ClearNPC = []() { calls.push_back("clear"); },
	.SetNPCInfo = [](const char* name, int npcID, int x, int y) {
		calls.push_back("npc|" + Text(name) + "|" + std::to_string(npcID) + "|" + std::to_string(x) + "|" + std::to_string(y));
	},
	.RunSkillTree = [](int domain, int maxLevel) {
		modeAtSkillTree = g_pTempInformation ? static_cast<int>(g_pTempInformation->GetMode()) : -1;
		calls.push_back("skilltree|" + std::to_string(domain) + "|" + std::to_string(maxLevel));
	},
	.MiniGameScores = [](GCMiniGameScores* p) { calls.push_back("scores|" + Address(p)); },
	.RunImageNotice = [](const char* name, std::uint32_t id) { calls.push_back("notice|" + Text(name) + "|" + std::to_string(id)); },
};

struct World
{
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const GameInfoPresentation::Host* oldHost = GameInfoPresentation::SetHost(&host);
	World()
	{
		g_pTempInformation = &temp;
		calls.clear();
		modeAtSkillTree = -1;
	}
	~World()
	{
		GameInfoPresentation::SetHost(oldHost);
		g_pTempInformation = oldTemp;
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

NPCInfo* MakeNPC(const std::string& name, NPCID_t id, ZoneCoord_t x, ZoneCoord_t y)
{
	NPCInfo* info = new NPCInfo;
	info->setName(name);
	info->setNPCID(id);
	info->setX(x);
	info->setY(y);
	return info;
}

// The decoded packet owns its NPCs until the handler pops and frees them.
void ReceiveNPCs(GCNPCInfo& packet)
{
	GCNPCInfo original;
	original.addNPCInfo(MakeNPC("Gruber", 100, 34, 56));
	original.addNPCInfo(MakeNPC("Vanda", 271, 120, 7));
	Decode(packet, original);
	GCNPCInfoHandler::execute(&packet, nullptr);
}

void ReceiveSkillTeacher(SkillDomainType_t domain)
{
	GCTeachSkillInfo original;
	original.setDomainType(domain);
	original.setTargetLevel(25);
	GCTeachSkillInfo packet;
	Decode(packet, original);
	GCTeachSkillInfoHandler::execute(&packet, nullptr);
}

void ReceiveNotice()
{
	GCNotifyWin original;
	original.setName("Lucky");
	original.setGiftID(3001);
	GCNotifyWin packet;
	Decode(packet, original);
	GCNotifyWinHandler::execute(&packet, nullptr);
}

// One reply for each of the five host entries; six calls with a host. The
// NPCs are popped and freed whatever is presented.
void RunEveryHandler()
{
	GCNPCInfo npcs;
	ReceiveNPCs(npcs);
	CHECK(npcs.popNPCInfo() == nullptr);
	ReceiveSkillTeacher(2);
	GCMiniGameScores scores;
	GCMiniGameScoresHandler::execute(&scores, nullptr);
	ReceiveNotice();
}

} // namespace

TEST(GameInfoPresentationHandlers, TheNPCListIsClearedThenEachNPCPresentedAndFreed)
{
	World world;
	GCNPCInfo packet;
	ReceiveNPCs(packet);
	CHECK_EQ(3u, calls.size());
	CHECK(calls.size() == 3 && calls[0] == "clear");
	CHECK(calls.size() == 3 && calls[1] == "npc|Gruber|100|34|56");
	CHECK(calls.size() == 3 && calls[2] == "npc|Vanda|271|120|7");
	// Every NPC was popped, so the packet's destructor frees nothing twice.
	CHECK(packet.popNPCInfo() == nullptr);
}

TEST(GameInfoPresentationHandlers, ASkillTeacherOpensTheSkillTreeThenTheLearnRequest)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_SHOP_BUY);
	ReceiveSkillTeacher(3);
	CHECK_EQ(1u, calls.size());
	// The level cap is the fixed 100, not the packet's target level.
	CHECK(!calls.empty() && calls[0] == "skilltree|3|100");
	CHECK_EQ(static_cast<int>(TempInformation::MODE_SHOP_BUY), modeAtSkillTree);
	CHECK(world.temp.GetMode() == TempInformation::MODE_SKILL_LEARN);
	CHECK_EQ(3, static_cast<long long>(world.temp.Value1));
	CHECK_EQ(100, static_cast<long long>(world.temp.Value2));
}

TEST(GameInfoPresentationHandlers, ScoresPassThroughAndANoticeForwardsItsNameAndGift)
{
	World world;
	GCMiniGameScores scores;
	GCMiniGameScoresHandler::execute(&scores, nullptr);
	ReceiveNotice();
	CHECK_EQ(2u, calls.size());
	CHECK(calls.size() == 2 && calls[0] == "scores|" + Address(&scores));
	CHECK(calls.size() == 2 && calls[1] == "notice|Lucky|3001");
}

TEST(GameInfoPresentationHandlers, MissingHostsAndEntriesSkipPresentationOnly)
{
	World world;
	GameInfoPresentation::SetHost(nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(world.temp.GetMode() == TempInformation::MODE_SKILL_LEARN);
	const GameInfoPresentation::Host empty{};
	CHECK(GameInfoPresentation::SetHost(&empty) == nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(GameInfoPresentation::SetHost(&host) == &empty);
	RunEveryHandler();
	CHECK_EQ(6u, calls.size());
}
