#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MGameStringTable.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCSystemMessage.h"
#include <string>
#include <vector>

// A system message goes to the system log as "[System]<message>", unless
// the player hides its kind (holy land, master lair, war). The same message
// twice in a row is added again only if it is no longer in the log. A
// ranger's chat goes to the ranger chat line, and a player message to the
// player log, as it came. The reply remembers the last message across
// calls, so each test uses messages of its own.
namespace {

std::vector<std::string> calls;
bool hideHolyLand = false;
bool hideLair = false;
bool hideWar = false;

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.SetRangerChatString = [](const char* message) { calls.push_back("ranger|" + std::string(message)); };
	host.HideHolyLandMessages = []() { return hideHolyLand; };
	host.HideLairMessages = []() { return hideLair; };
	host.HideWarMessages = []() { return hideWar; };
	return host;
}();

struct World : GameModelWorld
{
	CMessageArray systemLog;
	CMessageArray playerLog;
	CMessageArray* oldSystem = g_pSystemMessage;
	CMessageArray* oldPlayer = g_pPlayerMessage;
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	World()
	{
		InitGameStringTable();
		systemLog.Init(3, 512);
		playerLog.Init(3, 512);
		g_pSystemMessage = &systemLog;
		g_pPlayerMessage = &playerLog;
		calls.clear();
		hideHolyLand = hideLair = hideWar = false;
	}
	~World()
	{
		CommunicationPresentation::SetHost(oldCommunication);
		g_pPlayerMessage = oldPlayer;
		g_pSystemMessage = oldSystem;
	}
	// The log from oldest to newest, empty rows left out.
	static std::vector<std::string> Lines(CMessageArray& log)
	{
		std::vector<std::string> lines;
		for (int i = 0; i < log.GetSize(); i++)
			if (log[i] != nullptr && log[i][0] != '\0') lines.push_back(log[i]);
		return lines;
	}
	std::vector<std::string> System() { return Lines(systemLog); }
	std::vector<std::string> Player() { return Lines(playerLog); }
};

void Receive(SystemMessageType type, const std::string& message)
{
	GCSystemMessage original;
	original.setType(type);
	original.setMessage(message);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCSystemMessage packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCSystemMessageHandler::execute(&packet, nullptr);
}

// The wire refuses an empty message (GCSystemMessage::read), so an empty
// one only reaches the handler made by hand.
void ReceiveEmpty(SystemMessageType type)
{
	GCSystemMessage packet;
	packet.setType(type);
	packet.setMessage("");
	GCSystemMessageHandler::execute(&packet, nullptr);
}

std::string System(const std::string& message) { return "[System]" + message; }

} // namespace

TEST(SystemMessageHandler, EveryOtherKindGoesToTheSystemLogMarkedAsSystem)
{
	World world;
	Receive(SYSTEM_MESSAGE_NORMAL, "normal one");
	Receive(SYSTEM_MESSAGE_OPERATOR, "operator one");
	Receive(SYSTEM_MESSAGE_INFO, "info one");
	CHECK(world.System() == std::vector<std::string>({System("normal one"), System("operator one"), System("info one")}));
	CHECK(world.Player().empty());
	CHECK(calls.empty());
}

TEST(SystemMessageHandler, AHiddenKindIsDroppedAndAShownOneAdded)
{
	World world;
	hideHolyLand = hideLair = hideWar = true;
	Receive(SYSTEM_MESSAGE_HOLY_LAND, "holy hidden");
	Receive(SYSTEM_MESSAGE_MASTER_LAIR, "lair hidden");
	Receive(SYSTEM_MESSAGE_COMBAT, "war hidden");
	CHECK(world.System().empty());
	hideHolyLand = hideLair = hideWar = false;
	Receive(SYSTEM_MESSAGE_HOLY_LAND, "holy shown");
	Receive(SYSTEM_MESSAGE_MASTER_LAIR, "lair shown");
	Receive(SYSTEM_MESSAGE_COMBAT, "war shown");
	CHECK(world.System() == std::vector<std::string>({System("holy shown"), System("lair shown"), System("war shown")}));
	// Each option hides its own kind only.
	World other;
	hideLair = true;
	Receive(SYSTEM_MESSAGE_HOLY_LAND, "holy kept");
	Receive(SYSTEM_MESSAGE_COMBAT, "war kept");
	Receive(SYSTEM_MESSAGE_MASTER_LAIR, "lair dropped");
	CHECK(other.System() == std::vector<std::string>({System("holy kept"), System("war kept")}));
}

TEST(SystemMessageHandler, ARepeatIsAddedOnlyOnceItHasLeftTheLog)
{
	World world;
	Receive(SYSTEM_MESSAGE_NORMAL, "repeat me");
	Receive(SYSTEM_MESSAGE_NORMAL, "repeat me");
	CHECK(world.System() == std::vector<std::string>{System("repeat me")});
	// Another message in between makes it new again.
	Receive(SYSTEM_MESSAGE_NORMAL, "between");
	Receive(SYSTEM_MESSAGE_NORMAL, "repeat me");
	CHECK(world.System() == std::vector<std::string>({System("repeat me"), System("between"), System("repeat me")}));
	// Once the log (three rows) has scrolled it out, a repeat is added.
	Receive(SYSTEM_MESSAGE_NORMAL, "fill one");
	Receive(SYSTEM_MESSAGE_NORMAL, "fill two");
	Receive(SYSTEM_MESSAGE_NORMAL, "fill two");
	CHECK(world.System() == std::vector<std::string>({System("repeat me"), System("fill one"), System("fill two")}));
	Receive(SYSTEM_MESSAGE_NORMAL, "fill three");
	Receive(SYSTEM_MESSAGE_NORMAL, "fill four");
	Receive(SYSTEM_MESSAGE_NORMAL, "fill five");
	Receive(SYSTEM_MESSAGE_NORMAL, "fill five");
	CHECK(world.System() == std::vector<std::string>({System("fill three"), System("fill four"), System("fill five")}));
}

TEST(SystemMessageHandler, ARangersChatAndAPlayerMessageGoWhereTheyBelongUnmarked)
{
	World world;
	Receive(SYSTEM_MESSAGE_RANGER_CHAT, "ranger says");
	ReceiveEmpty(SYSTEM_MESSAGE_RANGER_CHAT);
	Receive(SYSTEM_MESSAGE_PLAYER, "player says");
	ReceiveEmpty(SYSTEM_MESSAGE_PLAYER);
	CHECK(calls == std::vector<std::string>{"ranger|ranger says"});
	CHECK(world.Player() == std::vector<std::string>{"player says"});
	CHECK(world.System().empty());
}

TEST(SystemMessageHandler, WithoutTheServiceNothingIsHiddenAndNoRangerChatShown)
{
	World world;
	CommunicationPresentation::SetHost(nullptr);
	Receive(SYSTEM_MESSAGE_COMBAT, "war without host");
	Receive(SYSTEM_MESSAGE_RANGER_CHAT, "ranger without host");
	CHECK(world.System() == std::vector<std::string>{System("war without host")});
	CHECK(calls.empty());
}
