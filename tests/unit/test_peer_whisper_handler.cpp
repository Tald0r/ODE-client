#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CMessageArray.h"
#include "ClientMode.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "RequestServerPlayer.h"
#include "RequestUserManager.h"
#include "UserInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Rpackets/CRWhisper.h"
#include "Rpackets/RCRequestVerify.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Another client whispers over its own connection, which closes after
// every whisper. A whisper for this character, in this world, while in or
// entering the game, records the sender's address, then, if the player
// accepts the sender, shows each message as a whisper (type 2) and makes
// the sender the whisper target. Curses are filtered unless an operator
// said it, or the player is a master or a ghost; an operator's words also
// go to the notice log. Another race's words are partly masked (in
// non-debug builds), less so the more INT the player has. Any other
// whisper is answered "whisper failed".
namespace {

std::vector<std::string> calls;
Race playerRace = RACE_SLAYER;
bool ghost = false;
bool op = false;
bool hasPlayer = true;
int playerHandle = 0;
MStatus status;

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.AddChatToHistory = [](const char* message, const char* sender, int chatType, std::uint32_t color) {
		calls.push_back(std::string(sender) + "|" + std::to_string(chatType) + "|" + std::to_string(color) + "|" + message);
	};
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&playerHandle) : nullptr; };
	host.Status = []() { return &status; };
	host.GetRace = []() { return playerRace; };
	host.HasEffectStatus = [](EFFECTSTATUS effect) { return ghost && effect == EFFECTSTATUS_GHOST; };
	host.IsOperator = []() { return op; };
	return host;
}();

// A connection from another client, closed when the test is done with it.
struct Peer : RequestServerPlayer
{
	Peer() : RequestServerPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Peer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

struct World : GameModelWorld
{
	MChatManager chat;
	UserInformation user;
	CMessageArray notice;
	RequestUserManager users;
	std::filesystem::path words = std::filesystem::temp_directory_path() / ("de_peer_whisper_curses_"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
	CLIENT_MODE oldMode = g_Mode;
	MChatManager* oldChat = g_pChatManager;
	UserInformation* oldUser = g_pUserInformation;
	CMessageArray* oldNotice = g_pNoticeMessage;
	RequestUserManager* oldUsers = g_pRequestUserManager;
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		InitGameStringTable();
		std::ofstream(words) << "darn\n";
		chat.LoadFromFileCurse(words.string().c_str());
		notice.Init(4, 256);
		user.CharacterID = "Lantern";
		user.WorldID = 3;
		g_Mode = MODE_GAME;
		g_pChatManager = &chat;
		g_pUserInformation = &user;
		g_pNoticeMessage = &notice;
		g_pRequestUserManager = &users;
		calls.clear();
		playerRace = RACE_SLAYER;
		ghost = false;
		op = false;
		hasPlayer = true;
		status.SetStatus(MODIFY_CURRENT_INT, 0);
		std::srand(1);
	}
	~World()
	{
		PlayerState::SetHost(oldPlayer);
		CommunicationPresentation::SetHost(oldCommunication);
		g_pRequestUserManager = oldUsers;
		g_pNoticeMessage = oldNotice;
		g_pUserInformation = oldUser;
		g_pChatManager = oldChat;
		g_Mode = oldMode;
		std::error_code ignored;
		std::filesystem::remove(words, ignored);
	}
	std::string Notice() { return notice[notice.GetSize() - 1]; }
	std::string Target() { return user.WhisperID.GetString() ? user.WhisperID.GetString() : "(none)"; }
	// What the curse filter alone makes of a line.
	std::string Filtered(const std::string& line)
	{
		std::vector<char> buffer(line.begin(), line.end());
		buffer.push_back('\0');
		chat.RemoveCurse(buffer.data(), true);
		return buffer.data();
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

// The "whisper failed" answer, as the peer's output stream holds it.
std::vector<unsigned char> Failed()
{
	RCRequestVerify verify;
	verify.setCode(REQUEST_VERIFY_WHISPER_FAILED);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(&verify);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

struct Line
{
	std::string text;
	DWORD color;
};

// Every whisper ends the peer's connection.
void Receive(Peer& peer, const std::string& name, const std::vector<Line>& lines, Race race = RACE_SLAYER,
	const std::string& target = "Lantern", WorldID_t worldID = 3)
{
	CRWhisper original;
	original.setName(name);
	original.setTargetName(target);
	for (const Line& line : lines)
	{
		WHISPER_MESSAGE message;
		message.msg = line.text;
		message.color = line.color;
		original.addMessage(message);
	}
	original.setRace(race);
	original.setWorldID(worldID);
	CRWhisper packet;
	Decode(packet, original);
	bool closed = false;
	try { CRWhisperHandler::execute(&packet, &peer); }
	catch (DisconnectException&) { closed = true; }
	CHECK(closed);
}

void Receive(const std::string& name, const std::string& text, Race race = RACE_SLAYER)
{
	Peer peer;
	Receive(peer, name, {{text, 7}}, race);
	CHECK(peer.Sent().empty());
}

const std::string kCursing = "you darn fool";

// A 128-character line of letters (no 'x', the curse mask) and spaces.
std::string LongLine()
{
	std::string line;
	for (int i = 0; i < 128; i++)
		line += (i % 9 == 8) ? ' ' : static_cast<char>('a' + i % 23);
	return line;
}

// How many characters the cross-race mask replaced: its characters are
// symbols, the line's are letters and spaces.
int Masked(const std::string& shown, const std::string& line)
{
	CHECK_EQ(line.size(), shown.size());
	int masked = 0;
	for (size_t i = 0; i < line.size() && i < shown.size(); i++)
	{
		if (shown[i] != line[i])
		{
			CHECK(line[i] != ' ');
			masked++;
		}
	}
	return masked;
}

std::string Shown(const std::string& call)
{
	// sender|type|color|message
	size_t at = 0;
	for (int i = 0; i < 3; i++) at = call.find('|', at) + 1;
	return call.substr(at);
}

} // namespace

TEST(PeerWhisperHandler, EachMessageIsFilteredShownAndTheSenderBecomesTheTarget)
{
	World world;
	CHECK(world.Filtered(kCursing) != kCursing);
	Peer peer;
	Receive(peer, "Nyx", {{kCursing, 9}, {"hello", 4}});
	CHECK(calls == std::vector<std::string>({"Nyx|2|9|" + world.Filtered(kCursing), "Nyx|2|4|hello"}));
	CHECK(world.Target() == "Nyx ");
	// A new sender's address is the connection's.
	CHECK(world.users.HasRequestUser("Nyx"));
	if (world.users.HasRequestUser("Nyx"))
		CHECK(world.users.GetUserInfo("Nyx")->IP == peer.getSocket()->getHost().c_str());
	CHECK(world.Notice().empty());
	CHECK(peer.Sent().empty());
}

TEST(PeerWhisperHandler, AKnownSendersAddressIsKept)
{
	World world;
	world.users.AddRequestUser("Nyx", "10.0.0.9");
	Receive("Nyx", "hello");
	CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.9");
	CHECK(calls == std::vector<std::string>{"Nyx|2|7|hello"});
}

TEST(PeerWhisperHandler, AWhisperForAnotherCharacterWorldOrModeIsAnsweredFailed)
{
	World world;
	auto Refused = [&](const std::string& target, WorldID_t worldID) {
		Peer peer;
		Receive(peer, "Nyx", {{"hello", 7}}, RACE_SLAYER, target, worldID);
		CHECK(peer.Sent() == Failed());
	};
	Refused("Vanda", 3);
	Refused("Lantern", 4);
	g_Mode = MODE_WAIT_SELECTPC;
	Refused("Lantern", 3);
	g_Mode = MODE_GAME;
	hasPlayer = false;
	Refused("Lantern", 3);
	hasPlayer = true;
	g_pChatManager = nullptr;
	Refused("Lantern", 3);
	g_pChatManager = &world.chat;
	g_pUserInformation = nullptr;
	Refused("Lantern", 3);
	UserInformation unnamed;
	unnamed.WorldID = 3;
	CHECK(unnamed.CharacterID.GetString() == nullptr);
	g_pUserInformation = &unnamed;
	Refused("Lantern", 3);
	g_pUserInformation = &world.user;
	CHECK(calls.empty());
	CHECK(world.Target() == "(none)");
	CHECK(!world.users.HasRequestUser("Nyx"));
	// While loading or waiting for the position, it is taken.
	for (CLIENT_MODE mode : {MODE_WAIT_UPDATEINFO, MODE_WAIT_SETPOSITION})
	{
		g_Mode = mode;
		Receive("Nyx", "hello");
	}
	CHECK_EQ(2u, calls.size());
}

TEST(PeerWhisperHandler, AWhisperFromSomeoneIgnoredIsDroppedButTheAddressIsKept)
{
	World world;
	world.chat.AddID("Nyx");
	Receive("Nyx", kCursing);
	CHECK(calls.empty());
	CHECK(world.Target() == "(none)");
	CHECK(world.users.HasRequestUser("Nyx"));
}

TEST(PeerWhisperHandler, AnOperatorsWhisperShowsWholeEvenIgnoredAndGoesToTheNotices)
{
	World world;
	world.chat.AddID("GMNyx");
	Receive("GMNyx", kCursing, RACE_VAMPIRE);
	CHECK(calls == std::vector<std::string>{"GMNyx|2|7|" + kCursing});
	CHECK(world.Notice() == "GMNyx>" + kCursing);
	CHECK(world.Target() == "GMNyx ");
}

TEST(PeerWhisperHandler, AMasterOrGhostPlayerSeesTheWhisperWhole)
{
	World world;
	world.user.IsMaster = TRUE;
	Receive("Nyx", kCursing, RACE_VAMPIRE);
	world.user.IsMaster = FALSE;
	ghost = true;
	Receive("Nyx", kCursing, RACE_VAMPIRE);
	CHECK(calls == std::vector<std::string>(2, "Nyx|2|7|" + kCursing));
}

TEST(PeerWhisperHandler, AnOperatorPlayerStillHasAPeersCursesFiltered)
{
	World world;
	op = true;
	Receive("Nyx", kCursing);
	CHECK(calls == std::vector<std::string>{"Nyx|2|7|" + world.Filtered(kCursing)});
}

TEST(PeerWhisperHandler, AnotherRacesWhisperIsMaskedLessTheMoreINTThePlayerHas)
{
	World world;
	const std::string line = LongLine();
	Receive("Nyx", line, RACE_SLAYER);
	Receive("Nyx", line, RACE_VAMPIRE);
	status.SetStatus(MODIFY_CURRENT_INT, 150);
	Receive("Nyx", line, RACE_VAMPIRE);
	CHECK_EQ(3u, calls.size());
	if (calls.size() != 3) return;
	// The same race reads it whole.
	CHECK(Shown(calls[0]) == line);
#ifdef _DEBUG
	// A debug build does not mask.
	CHECK(Shown(calls[1]) == line);
	CHECK(Shown(calls[2]) == line);
#else
	// A letter is masked when (rand() & 63) >= readable% * 63 / 100. With
	// no INT a slayer reads 25% (about 87 of the 114 letters masked); with
	// 150 INT, the 75% cap (about 30). The bounds sit over three standard
	// deviations out, whatever the platform's rand().
	const int dull = Masked(Shown(calls[1]), line);
	const int sharp = Masked(Shown(calls[2]), line);
	CHECK(dull > 70);
	CHECK(sharp > 15 && sharp < 50);
#endif
}

TEST(PeerWhisperHandler, AnyINTTheServerSetsReadsAsTheCap)
{
	World world;
	const std::string line = LongLine();
	status.SetStatus(MODIFY_CURRENT_INT, 0xFFFFFFFFu);
	Receive("Nyx", line, RACE_VAMPIRE);
	// Unclamped, 2^31 times 100 wrapped to 0 in the old unsigned arithmetic
	// and read at 25%.
	status.SetStatus(MODIFY_CURRENT_INT, 0x80000000u);
	Receive("Nyx", line, RACE_VAMPIRE);
	// A vampire divides INT by 300 where a slayer divides by 150, so its
	// player reaches the cap only at 150 INT: a clamp below about 100
	// would show.
	playerRace = RACE_VAMPIRE;
	Receive("Nyx", line, RACE_SLAYER);
	CHECK_EQ(3u, calls.size());
#ifndef _DEBUG
	for (const std::string& call : calls)
	{
		const int sharp = Masked(Shown(call), line);
		CHECK(sharp > 15 && sharp < 50);
	}
#endif
}

TEST(PeerWhisperHandler, WithoutAChatServiceTheTargetIsStillSet)
{
	World world;
	CommunicationPresentation::SetHost(nullptr);
	Receive("Nyx", kCursing);
	CHECK(calls.empty());
	CHECK(world.Target() == "Nyx ");
}
