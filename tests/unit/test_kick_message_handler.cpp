#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "MEventQueue.h"
#include "MGameStringTable.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCKickMessage.h"
#include <string>
#include <vector>

// A kick warning is queued as a zone event that logs the player out after
// the packet's seconds, showing its reason and the countdown: the pay time
// is over, or the free play is (worded for a slayer's level or another
// race's). Any other kind queues nothing.
namespace {

std::vector<std::string> events;
Race playerRace = RACE_SLAYER;

std::string Described(const MEvent& event)
{
	std::string text = std::to_string(event.eventID) + "|" + std::to_string(event.eventDelay) + "|"
		+ std::to_string(event.eventFlag) + "|" + std::to_string(event.eventType);
	for (int id : event.m_StringsID) text += "|" + std::to_string(id);
	return text;
}

const WorldEvents::Host world{
	.AddEvent = [](MEvent& event) { events.push_back(Described(event)); },
};

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.GetRace = []() { return playerRace; };
	return host;
}();

struct World : GameModelWorld
{
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&world);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		events.clear();
		playerRace = RACE_SLAYER;
	}
	~World()
	{
		PlayerState::SetHost(oldPlayer);
		WorldEvents::SetHost(oldWorld);
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

void Receive(BYTE type, uint seconds)
{
	GCKickMessage original;
	original.setType(type);
	original.setSeconds(seconds);
	GCKickMessage packet;
	Decode(packet, original);
	GCKickMessageHandler::execute(&packet, nullptr);
}

std::string Logout(int delay, int reason)
{
	return std::to_string(EVENTID_FORCE_LOGOUT_BY_PREMIUM) + "|" + std::to_string(delay) + "|"
		+ std::to_string(EVENTFLAG_SHOW_DELAY_STRING) + "|" + std::to_string(EVENTTYPE_ZONE) + "|"
		+ std::to_string(reason) + "|" + std::to_string(STRING_MESSAGE_LOGOUT_AFTER_SECOND);
}

} // namespace

TEST(KickMessageHandler, AnEndedPayTimeQueuesALogoutAfterThePacketsSeconds)
{
	World world;
	Receive(KICK_MESSAGE_PAY_TIMEOUT, 30);
	CHECK(events == std::vector<std::string>{Logout(30000, STRING_MESSAGE_LOGOUT_BY_PAYTIME)});
}

TEST(KickMessageHandler, AnEndedFreePlayIsWordedForASlayersLevelOrAnotherRaces)
{
	World world;
	Receive(KICK_MESSAGE_EXPIRE_FREEPLAY, 5);
	playerRace = RACE_VAMPIRE;
	Receive(KICK_MESSAGE_EXPIRE_FREEPLAY, 6);
	playerRace = RACE_OUSTERS;
	Receive(KICK_MESSAGE_EXPIRE_FREEPLAY, 7);
	CHECK(events == std::vector<std::string>({Logout(5000, STRING_MESSAGE_LOGOUT_BY_FREEPLAY_LEVEL),
		Logout(6000, STRING_MESSAGE_LOGOUT_BY_LEVEL), Logout(7000, STRING_MESSAGE_LOGOUT_BY_LEVEL)}));
}

TEST(KickMessageHandler, AnyOtherKindQueuesNothingAndNoServiceQueuesNothing)
{
	World world;
	Receive(KICK_MESSAGE_MAX, 30);
	CHECK(events.empty());
	WorldEvents::SetHost(nullptr);
	Receive(KICK_MESSAGE_PAY_TIMEOUT, 30);
	const WorldEvents::Host empty{};
	WorldEvents::SetHost(&empty);
	Receive(KICK_MESSAGE_PAY_TIMEOUT, 30);
	CHECK(events.empty());
}
