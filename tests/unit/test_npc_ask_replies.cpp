#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "DialogPresentationHost.h"
#include "MNPCScriptTable.h"
#include "PlayerStateHost.h"
#include "ScriptParameter.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "SystemAvailabilities.h"
#include "TalkBox.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCNPCAsk.h"
#include "Gpackets/GCNPCAskDynamic.h"
#include "Gpackets/GCNPCAskVariable.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// An NPC's question answers the player's wait on it: the PC talk box is
// cleared and filled with the question's subject, the NPC, its creature
// type, the script and each answer the system availabilities allow
// (with its index), then popped up. The question comes from the NPC's
// script (GCNPCAsk; four NPC IDs need no creature in the zone and are
// their own creature type), whole in the packet (GCNPCAskDynamic, with
// at least one answer), or from the script with variables filled in
// (GCNPCAskVariable). A question from an NPC that is not in the zone
// only ends the wait.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
bool zoneExists = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;

struct FakeNPC
{
	int type;
};
FakeNPC guard{250}, merchant{251};
int me;

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		return id == 1003 ? reinterpret_cast<MCreature*>(&guard) : id == 1004 ? reinterpret_cast<MCreature*>(&merchant) : nullptr;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature* p) { return reinterpret_cast<FakeNPC*>(p)->type; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupPCTalkDlg = []() { calls.push_back("talk box shown"); };
	return host;
}();

struct World : GameModelWorld
{
	MNPCScriptTable scripts;
	PCTalkBox talkBox;
	SystemAvailabilitiesManager availabilities;
	MNPCScriptTable* oldScripts = g_pNPCScriptTable;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	SystemAvailabilitiesManager* oldAvailabilities = g_pSystemAvailableManager;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		calls.clear();
		hasPlayer = true;
		zoneExists = true;
		waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
		g_pNPCScriptTable = &scripts;
		g_pPCTalkBox = &talkBox;
		g_pSystemAvailableManager = &availabilities;
		Script(77, {"Hello, %(name)"}, {"Go to %(place)", "Wait", "Leave"});
		Stale();
	}
	// What an earlier question left.
	void Stale()
	{
		talkBox.Release();
		talkBox.SetType(PCTalkBox::SELECT_QUEST);
		talkBox.AddString("stale");
		talkBox.m_AnswerIDMap = {9};
	}
	~World()
	{
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pSystemAvailableManager = oldAvailabilities;
		g_pPCTalkBox = oldTalkBox;
		g_pNPCScriptTable = oldScripts;
	}
	void Script(unsigned int id, std::initializer_list<const char*> subjects, std::initializer_list<const char*> answers)
	{
		auto row = std::make_unique<NPC_SCRIPT>();
		row->SubjectTable.Init(static_cast<int>(subjects.size()));
		row->ContentTable.Init(static_cast<int>(answers.size()));
		int index = 0;
		for (const char* subject : subjects) row->SubjectTable.Set(index++, subject);
		index = 0;
		for (const char* answer : answers) row->ContentTable.Set(index++, answer);
		CHECK(scripts.AddData(id, row.release()));
	}
	// The guild system closed, which refuses script 77's second answer.
	void GuildClosed()
	{
		std::istringstream filter("*4 1\n77 2\n");
		CHECK(availabilities.LoadFromStream(filter));
		availabilities.SetFlag(~(1u << 4));
		CHECK(!availabilities.ScriptFiltering(77, 1));
		CHECK(availabilities.ScriptFiltering(77, 0) && availabilities.ScriptFiltering(77, 2));
	}
	std::vector<std::string> Answers() const
	{
		std::vector<std::string> answers;
		// An empty MString holds NULL.
		for (const MString* pString : talkBox) answers.push_back(pString->GetString() != nullptr ? pString->GetString() : "<null>");
		return answers;
	}
	// The talk box as the question left it: subject, NPC, type and script.
	void Shows(const char* subject, unsigned int npcID, unsigned int creatureType, unsigned int scriptID)
	{
		CHECK(talkBox.GetType() == PCTalkBox::NORMAL);
		CHECK(std::string(subject) == talkBox.GetContent());
		CHECK_EQ(npcID, talkBox.GetNPCID());
		CHECK_EQ(creatureType, talkBox.GetCreatureType());
		CHECK_EQ(scriptID, talkBox.GetScriptID());
	}
	bool Untouched() const { return Answers() == std::vector<std::string>{"stale"} && talkBox.m_AnswerIDMap == std::vector<int>{9}; }
};

void Ask(TYPE_OBJECTID npc, int scriptID, int npcID = 0)
{
	GCNPCAsk packet;
	packet.setObjectID(npc);
	packet.setScriptID(static_cast<ScriptID_t>(scriptID));
	packet.setNPCID(static_cast<NPCID_t>(npcID));
	GCNPCAskHandler::execute(&packet, nullptr);
}

void AskDynamic(TYPE_OBJECTID npc, const std::string& subject, std::initializer_list<const char*> answers)
{
	GCNPCAskDynamic packet;
	packet.setObjectID(npc);
	packet.setScriptID(77);
	packet.setSubject(subject);
	for (const char* answer : answers) packet.addContent(answer);
	GCNPCAskDynamicHandler::execute(&packet, nullptr);
}

// The packet has no setters (the client only reads it), so it is read
// from the bytes the server would send.
void AskVariable(TYPE_OBJECTID npc, int scriptID)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(static_cast<ObjectID_t>(npc));
	encoded.write(static_cast<ScriptID_t>(scriptID));
	encoded.write(static_cast<BYTE>(2));
	for (const auto& [name, value] : {std::pair<const char*, const char*>{"name", "Bob"}, {"place", "Eslania"}})
	{
		ScriptParameter parameter;
		parameter.setName(name);
		parameter.setValue(value);
		parameter.write(encoded);
	}
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCNPCAskVariable packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCNPCAskVariableHandler::execute(&packet, nullptr);
}

std::vector<unsigned char> Encoded(const Packet& packet)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	packet.write(encoded);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	const std::vector<unsigned char> bytes = Encoded(original);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

} // namespace

TEST(NPCAskReplies, AScriptedQuestionFillsTheTalkBox)
{
	World world;
	Ask(1003, 77);
	CHECK(calls == std::vector<std::string>({"verified", "talk box shown"}));
	world.Shows("Hello, %(name)", 1003, 250, 77);
	CHECK(world.Answers() == std::vector<std::string>({"Go to %(place)", "Wait", "Leave"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 1, 2}));
	// An answer the system availabilities refuse is left out, its index
	// with it.
	calls.clear();
	world.GuildClosed();
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	Ask(1004, 77);
	CHECK(calls == std::vector<std::string>({"verified", "talk box shown"}));
	world.Shows("Hello, %(name)", 1004, 251, 77);
	CHECK(world.Answers() == std::vector<std::string>({"Go to %(place)", "Leave"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 2}));
}

TEST(NPCAskReplies, FourNPCsNeedNoCreatureAndAreTheirOwnType)
{
	World world;
	for (int npcID : {634, 637})
	{
		waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
		Ask(4242, 77, npcID);
		world.Shows("Hello, %(name)", 4242, static_cast<unsigned int>(npcID), 77);
	}
	// Even with a creature in the zone.
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	Ask(1003, 77, 635);
	world.Shows("Hello, %(name)", 1003, 635, 77);
	CHECK(calls == std::vector<std::string>({"verified", "talk box shown", "verified", "talk box shown", "verified", "talk box shown"}));
	// Their neighbours do need one, and take its type.
	calls.clear();
	world.Stale();
	for (int npcID : {633, 638})
	{
		waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
		Ask(4242, 77, npcID);
	}
	CHECK(calls == std::vector<std::string>({"verified", "verified"}));
	CHECK(world.Untouched());
	for (int npcID : {633, 638})
	{
		waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
		Ask(1003, 77, npcID);
		world.Shows("Hello, %(name)", 1003, 250, 77);
	}
}

TEST(NPCAskReplies, AQuestionNobodyWaitsOnOrThatCannotBeShownIsIgnored)
{
	World world;
	// An NPC not in the zone only ends the wait.
	Ask(4242, 77);
	AskVariable(4242, 77);
	CHECK(calls == std::vector<std::string>{"verified"});
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	AskVariable(4242, 77);
	CHECK(calls == std::vector<std::string>({"verified", "verified"}));
	CHECK(world.Untouched());
	// Not waiting on a question: nothing.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	Ask(1003, 77);
	AskVariable(1003, 77);
	AskDynamic(1003, "Q", {"A"});
	CHECK(calls.empty());
	CHECK(world.Untouched());
	// Without a player, the zone, the scripts or the talk box: nothing, and
	// the wait stays.
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	hasPlayer = false;
	Ask(1003, 77);
	AskVariable(1003, 77);
	AskDynamic(1003, "Q", {"A"});
	hasPlayer = true;
	zoneExists = false;
	Ask(1003, 77);
	AskVariable(1003, 77);
	AskDynamic(1003, "Q", {"A"});
	zoneExists = true;
	g_pNPCScriptTable = nullptr;
	Ask(1003, 77);
	AskVariable(1003, 77);
	g_pNPCScriptTable = &world.scripts;
	g_pPCTalkBox = nullptr;
	Ask(1003, 77);
	AskVariable(1003, 77);
	g_pPCTalkBox = &world.talkBox;
	CHECK(calls.empty());
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_NPC_ASK);
	CHECK(world.Untouched());
}

TEST(NPCAskReplies, AWholeQuestionFillsTheTalkBox)
{
	World world;
	AskDynamic(1003, "Where to?", {"North", "South"});
	CHECK(calls == std::vector<std::string>({"verified", "talk box shown"}));
	world.Shows("Where to?", 1003, 250, 77);
	CHECK(world.Answers() == std::vector<std::string>({"North", "South"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 1}));
	// No answer, or an NPC not in the zone: only the wait ends.
	calls.clear();
	world.Stale();
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	AskDynamic(1003, "Where to?", {});
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	AskDynamic(4242, "Where to?", {"North"});
	CHECK(calls == std::vector<std::string>({"verified", "verified"}));
	CHECK(world.Untouched());
}

TEST(NPCAskReplies, AQuestionWithVariablesFillsThemIn)
{
	World world;
	AskVariable(1003, 77);
	CHECK(calls == std::vector<std::string>({"verified", "talk box shown"}));
	world.Shows("Hello, Bob", 1003, 250, 77);
	CHECK(world.Answers() == std::vector<std::string>({"Go to Eslania", "Wait", "Leave"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 1, 2}));
	calls.clear();
	world.GuildClosed();
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	AskVariable(1004, 77);
	world.Shows("Hello, Bob", 1004, 251, 77);
	CHECK(world.Answers() == std::vector<std::string>({"Go to Eslania", "Leave"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 2}));
}

TEST(NPCAskReplies, WithoutADialogHostTheTalkBoxIsNotShown)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	Ask(1003, 77);
	const DialogPresentation::Host bare{};
	DialogPresentation::SetHost(&bare);
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	Ask(1003, 77);
	CHECK(calls == std::vector<std::string>({"verified", "verified"}));
	world.Shows("Hello, %(name)", 1003, 250, 77);
}

TEST(NPCAskReplies, AWholeQuestionKeepsEachAnswerWithItsText)
{
	World world;
	world.GuildClosed();
	AskDynamic(1003, "Where to?", {"North", "South", "East"});
	CHECK(calls == std::vector<std::string>({"verified", "talk box shown"}));
	// The refused second answer goes with its text, not the third's.
	CHECK(world.Answers() == std::vector<std::string>({"North", "East"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 2}));
}

TEST(NPCAskReplies, AnEmptyAnswerKeepsItsPlace)
{
	World world;
	GCNPCAskDynamic original;
	original.setObjectID(1003);
	original.setScriptID(77);
	original.setSubject("Where to?");
	for (const char* answer : {"North", "", "East"}) original.addContent(answer);
	GCNPCAskDynamic packet;
	Decode(packet, original);
	// Read back, it says what was sent: three answers, the second empty.
	CHECK(Encoded(packet) == Encoded(original));
	CHECK_EQ(3, static_cast<int>(packet.getContentsCount()));
	// The empty answer is left out, as a refused one is, and the others
	// keep their indices.
	GCNPCAskDynamicHandler::execute(&packet, nullptr);
	CHECK(world.Answers() == std::vector<std::string>({"North", "East"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 2}));
}

TEST(NPCAskReplies, AnEmptyScriptedAnswerIsLeftOut)
{
	World world;
	// Shown, an empty answer would reach the dialog as a NULL menu string.
	world.Script(78, {"Hi"}, {"Go", "", "Leave"});
	Ask(1003, 78);
	CHECK(world.Answers() == std::vector<std::string>({"Go", "Leave"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 2}));
	world.Stale();
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	AskVariable(1003, 78);
	CHECK(world.Answers() == std::vector<std::string>({"Go", "Leave"}));
	CHECK(world.talkBox.m_AnswerIDMap == std::vector<int>({0, 2}));
}
