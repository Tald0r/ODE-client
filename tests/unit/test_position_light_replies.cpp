#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientPlayer.h"
#include "LoginPresentationHost.h"
#include "PlayerStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "UserInformation.h"
#include "WorldEventsHost.h"
#include "Gpackets/GCChangeDarkLight.h"
#include "Gpackets/GCSetPosition.h"
#include <string>
#include <vector>

// The game server's first word places the player: the connection takes
// the cell and is ready for normal play, the player is placed there
// facing the direction, and the game starts. The hour's dark and light
// set how dark the view draws and how far the player's light reaches; an
// operator always sees the full light.
namespace {

std::vector<std::string> calls;

std::string N(long long value) { return std::to_string(value); }

const LoginPresentation::Host login = []() {
	LoginPresentation::Host host{};
	host.InitPlayer = [](int x, int y, int direction) { calls.push_back("player at|" + N(x) + "," + N(y) + "," + N(direction)); };
	host.EnterGameMode = []() { calls.push_back("game"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.SetDarkBits = [](int darkBits) { calls.push_back("dark|" + N(darkBits)); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.SetTimeLightSight = [](int sight) { calls.push_back("light|" + N(sight)); };
	return host;
}();

// The game connection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
};

struct World : GameModelWorld
{
	GamePlayer connection;
	const LoginPresentation::Host* oldLogin = LoginPresentation::SetHost(&login);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		calls.clear();
		g_pUserInformation->IsMaster = FALSE;
	}
	~World()
	{
		PlayerState::SetHost(oldPlayer);
		WorldEvents::SetHost(oldEvents);
		LoginPresentation::SetHost(oldLogin);
	}
};

void ReceivePosition(Player* pConnection, int x, int y, int direction)
{
	GCSetPosition packet;
	packet.setX(static_cast<Coord_t>(x));
	packet.setY(static_cast<Coord_t>(y));
	packet.setDir(static_cast<Dir_t>(direction));
	GCSetPositionHandler::execute(&packet, pConnection);
}

void ReceiveDarkLight(int dark, int light)
{
	GCChangeDarkLight packet;
	packet.setDarkLevel(static_cast<DarkLevel_t>(dark));
	packet.setLightLevel(static_cast<LightLevel_t>(light));
	GCChangeDarkLightHandler::execute(&packet, nullptr);
}

} // namespace

TEST(PositionLightReplies, TheFirstPositionPlacesThePlayerAndStartsTheGame)
{
	World world;
	world.connection.setPlayerStatus(CPS_AFTER_SENDING_CG_CONNECT);
	ReceivePosition(&world.connection, 120, 85, 3);
	CHECK(calls == std::vector<std::string>({"player at|120,85,3", "game"}));
	CHECK_EQ(120, static_cast<int>(world.connection.getX()));
	CHECK_EQ(85, static_cast<int>(world.connection.getY()));
	CHECK(world.connection.getPlayerStatus() == CPS_NORMAL);
}

TEST(PositionLightReplies, WithoutAHostThePlayerIsNotPlacedNorTheGameStarted)
{
	World world;
	LoginPresentation::SetHost(nullptr);
	ReceivePosition(&world.connection, 120, 85, 3);
	const LoginPresentation::Host bare{};
	LoginPresentation::SetHost(&bare);
	ReceivePosition(&world.connection, 121, 86, 4);
	CHECK(calls.empty());
	CHECK_EQ(121, static_cast<int>(world.connection.getX()));
	CHECK(world.connection.getPlayerStatus() == CPS_NORMAL);
}

TEST(PositionLightReplies, TheHoursDarkAndLightReachTheViewAndThePlayer)
{
	World world;
	ReceiveDarkLight(9, 4);
	CHECK(calls == std::vector<std::string>({"dark|9", "light|4"}));
	// An operator always sees the full light.
	calls.clear();
	g_pUserInformation->IsMaster = TRUE;
	ReceiveDarkLight(9, 4);
	CHECK(calls == std::vector<std::string>({"dark|0", "light|13"}));
	// Without the hosts, nothing.
	calls.clear();
	WorldEvents::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	ReceiveDarkLight(9, 4);
	const WorldEvents::Host bareEvents{};
	const PlayerState::Host barePlayer{};
	WorldEvents::SetHost(&bareEvents);
	PlayerState::SetHost(&barePlayer);
	ReceiveDarkLight(9, 4);
	CHECK(calls.empty());
}
