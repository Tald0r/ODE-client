#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "DialogPresentationHost.h"
#include "LoginPresentationHost.h"
#include "MGameStringTable.h"
#include "UserInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "Cpackets/CLGetWorldList.h"
#include "Lpackets/LCLoginOK.h"
#include "Lpackets/LCRegisterPlayerOK.h"
#include <string>
#include <vector>

// The login server's welcome, after a login or after a new account. The
// login ID is kept for the next login's ID box ("DarkEden" in place of an
// ID of 15 or more characters). Unless the client is quitting to update,
// the connection becomes the login connection, asks for the world list
// and waits for it, and blood shows unless the player chose the teen
// version. After a new account the connection also joins the logged-in
// status and the server group is kept. After a login the account's family
// flag and premium days are kept: a Netmarble account has none to show,
// and the dormant-account event's first login starts a free week with a
// notice.
namespace {

std::vector<std::string> calls;
bool teen = false;

const LoginPresentation::Host login = []() {
	LoginPresentation::Host host{};
	host.BackupLoginID = [](const char* id) { calls.push_back(std::string("keeps id|") + (id ? id : "(null)")); };
	host.EnterWaitWorldListMode = []() { calls.push_back("waits for the worlds"); };
	host.SetServerGroupName = [](const char* name) { calls.push_back(std::string("group|") + name); };
	host.SetGoreLevel = [](bool gore) { calls.push_back(gore ? "blood" : "no blood"); };
	host.UseTeenVersion = []() { return teen; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back(std::string("message|") + message); };
	return host;
}();

struct LoginPlayer : ClientPlayer
{
	LoginPlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~LoginPlayer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

std::vector<unsigned char> Encoded(const Packet& packet)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(&packet);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

std::vector<unsigned char> WorldListAsked()
{
	CLGetWorldList ask;
	return Encoded(ask);
}

struct World : GameModelWorld
{
	int oldNeedUpdate = g_bNeedUpdate;
	int oldPremiumDays = g_LeftPremiumDays;
	bool oldFamily = g_bFamily;
	const LoginPresentation::Host* oldLogin = LoginPresentation::SetHost(&login);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		InitGameStringTable();
		calls.clear();
		teen = false;
		g_bNeedUpdate = 0;
		g_LeftPremiumDays = 42;
		g_bFamily = false;
		g_pUserInformation->UserID = "Lantern";
		g_pUserInformation->GoreLevel = false;
		g_pUserInformation->IsNetmarble = false;
		g_pUserInformation->pLogInClientPlayer = nullptr;
	}
	~World()
	{
		DialogPresentation::SetHost(oldDialog);
		LoginPresentation::SetHost(oldLogin);
		g_bFamily = oldFamily;
		g_LeftPremiumDays = oldPremiumDays;
		g_bNeedUpdate = oldNeedUpdate;
	}
};

void ReceiveLogin(LoginPlayer& player, int lastDays = 12, bool family = false)
{
	LCLoginOK packet;
	packet.setLastDays(static_cast<WORD>(lastDays));
	packet.setFamily(family);
	LCLoginOKHandler::execute(&packet, &player);
}

void ReceiveRegistered(LoginPlayer& player, const char* group = "Rion")
{
	LCRegisterPlayerOK packet;
	packet.setGroupName(group);
	LCRegisterPlayerOKHandler::execute(&packet, &player);
}

} // namespace

TEST(LoginWelcomeReplies, ALoginAsksForTheWorldsAndKeepsTheAccount)
{
	World world;
	LoginPlayer player;
	const PlayerStatus before = player.getPlayerStatus();
	ReceiveLogin(player, 12, true);
	CHECK(calls == std::vector<std::string>({"keeps id|Lantern", "waits for the worlds", "blood"}));
	CHECK(g_pUserInformation->pLogInClientPlayer == &player);
	CHECK(player.Sent() == WorldListAsked());
	CHECK(player.getPlayerStatus() == before);
	CHECK(g_pUserInformation->GoreLevel);
	CHECK(g_bFamily);
	CHECK_EQ(12, g_LeftPremiumDays);
}

TEST(LoginWelcomeReplies, ALongIDIsKeptAsDarkEden)
{
	World world;
	LoginPlayer player;
	g_pUserInformation->UserID = "fourteen_chars";
	ReceiveLogin(player);
	g_pUserInformation->UserID = "fifteen_chars__";
	ReceiveLogin(player);
	ReceiveRegistered(player);
	g_pUserInformation->UserID = "fourteen_chars";
	ReceiveRegistered(player);
	CHECK(calls[0] == "keeps id|fourteen_chars");
	CHECK(calls[3] == "keeps id|DarkEden");
	CHECK(calls[6] == "keeps id|DarkEden");
	CHECK(calls[10] == "keeps id|fourteen_chars");
}

TEST(LoginWelcomeReplies, TheTeenVersionHidesTheBlood)
{
	World world;
	LoginPlayer player;
	teen = true;
	g_pUserInformation->GoreLevel = true;
	ReceiveLogin(player);
	CHECK(calls.back() == "no blood");
	CHECK(!g_pUserInformation->GoreLevel);
	g_pUserInformation->GoreLevel = true;
	calls.clear();
	ReceiveRegistered(player);
	CHECK(calls.back() == "no blood");
	CHECK(!g_pUserInformation->GoreLevel);
}

TEST(LoginWelcomeReplies, QuittingToUpdateOnlyKeepsTheIDAndTheAccount)
{
	World world;
	LoginPlayer player;
	g_bNeedUpdate = 1;
	ReceiveLogin(player, 12, true);
	ReceiveRegistered(player);
	CHECK(calls == std::vector<std::string>(2, "keeps id|Lantern"));
	CHECK(player.Sent().empty());
	CHECK(g_pUserInformation->pLogInClientPlayer == nullptr);
	CHECK(!g_pUserInformation->GoreLevel);
	CHECK(g_bFamily);
	CHECK_EQ(12, g_LeftPremiumDays);
}

TEST(LoginWelcomeReplies, TheDormantAccountEventStartsAFreeWeek)
{
	World world;
	LoginPlayer player;
	ReceiveLogin(player, 0xFFFD);
	CHECK_EQ(7, g_LeftPremiumDays);
	CHECK(calls.back() == std::string("message|") + GetGameString(UI_STRING_MESSAGE_GET_EVENT_ITEM_NOTICE));
	CHECK(!g_bFamily);
	// A Netmarble account has no premium days to show.
	calls.clear();
	g_pUserInformation->IsNetmarble = true;
	ReceiveLogin(player, 0xFFFD);
	CHECK_EQ(0xFFFF, g_LeftPremiumDays);
	CHECK(calls.back() == "blood");
}

TEST(LoginWelcomeReplies, ANewAccountLogsInAndKeepsItsServerGroup)
{
	World world;
	LoginPlayer player;
	ReceiveRegistered(player, "Rion");
	CHECK(calls == std::vector<std::string>({"keeps id|Lantern", "waits for the worlds", "group|Rion", "blood"}));
	CHECK(g_pUserInformation->pLogInClientPlayer == &player);
	CHECK(player.getPlayerStatus() == CPS_AFTER_SENDING_CL_LOGIN);
	CHECK(player.Sent() == WorldListAsked());
	CHECK(g_pUserInformation->GoreLevel);
	// The account's premium days and family flag are the login's alone.
	CHECK_EQ(42, g_LeftPremiumDays);
	CHECK(!g_bFamily);
}

TEST(LoginWelcomeReplies, WithoutTheHostTheBloodShows)
{
	World world;
	LoginPresentation::SetHost(nullptr);
	CHECK(!LoginPresentation::UseTeenVersion());
	LoginPresentation::BackupLoginID("Lantern");
	LoginPresentation::EnterWaitWorldListMode();
	LoginPresentation::SetServerGroupName("Rion");
	LoginPresentation::SetGoreLevel(true);
	LoginPlayer player;
	ReceiveRegistered(player);
	CHECK(g_pUserInformation->GoreLevel);
	CHECK(calls.empty());
}
