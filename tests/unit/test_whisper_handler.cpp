#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "UserInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCWhisper.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// A whisper from someone the player accepts goes to the chat history as a
// whisper (type 2) and makes the sender the whisper target. Curses are
// filtered unless an operator said it, or the player is one or a ghost; an
// operator's words also go to the notice log. Another race's whisper is
// partly masked (in non-debug builds), less so the more INT the player has.
namespace {

std::vector<std::string> calls;
Race playerRace = RACE_SLAYER;
bool ghost = false;
bool op = false;
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
	host.Status = []() { return &status; };
	host.GetRace = []() { return playerRace; };
	host.HasEffectStatus = [](EFFECTSTATUS effect) { return ghost && effect == EFFECTSTATUS_GHOST; };
	host.IsOperator = []() { return op; };
	return host;
}();

struct World : GameModelWorld
{
	MChatManager chat;
	UserInformation user;
	CMessageArray notice;
	std::filesystem::path words = std::filesystem::temp_directory_path() / ("de_whisper_curses_"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
	MChatManager* oldChat = g_pChatManager;
	UserInformation* oldUser = g_pUserInformation;
	CMessageArray* oldNotice = g_pNoticeMessage;
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		InitGameStringTable();
		std::ofstream(words) << "darn\n";
		chat.LoadFromFileCurse(words.string().c_str());
		notice.Init(4, 256);
		g_pChatManager = &chat;
		g_pUserInformation = &user;
		g_pNoticeMessage = &notice;
		calls.clear();
		playerRace = RACE_SLAYER;
		ghost = false;
		op = false;
		status.SetStatus(MODIFY_CURRENT_INT, 0);
		std::srand(1);
	}
	~World()
	{
		PlayerState::SetHost(oldPlayer);
		CommunicationPresentation::SetHost(oldCommunication);
		g_pNoticeMessage = oldNotice;
		g_pUserInformation = oldUser;
		g_pChatManager = oldChat;
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

void Receive(const std::string& name, const std::string& message, Race race = RACE_SLAYER, uint color = 7)
{
	GCWhisper original;
	original.setName(name);
	original.setMessage(message);
	original.setRace(race);
	original.setColor(color);
	GCWhisper packet;
	Decode(packet, original);
	GCWhisperHandler::execute(&packet, nullptr);
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

TEST(WhisperHandler, AnAcceptedWhisperIsFilteredShownAndMakesTheSenderTheTarget)
{
	World world;
	CHECK(world.Filtered(kCursing) != kCursing);
	Receive("Vanda", kCursing, RACE_SLAYER, 9);
	CHECK(calls == std::vector<std::string>{"Vanda|2|9|" + world.Filtered(kCursing)});
	CHECK(world.Target() == "Vanda ");
	CHECK(world.Notice().empty());
}

TEST(WhisperHandler, AWhisperFromSomeoneIgnoredIsDropped)
{
	World world;
	world.chat.AddID("Vanda");
	Receive("Vanda", kCursing);
	CHECK(calls.empty());
	CHECK(world.Target() == "(none)");
}

TEST(WhisperHandler, AnOperatorsWhisperShowsWholeEvenIgnoredAndGoesToTheNotices)
{
	World world;
	world.chat.AddID("GMNyx");
	Receive("GMNyx", kCursing, RACE_VAMPIRE);
	CHECK(calls == std::vector<std::string>{"GMNyx|2|7|" + kCursing});
	CHECK(world.Notice() == "GMNyx>" + kCursing);
	CHECK(world.Target() == "GMNyx ");
}

TEST(WhisperHandler, AnOperatorMasterOrGhostPlayerSeesTheWhisperWhole)
{
	World world;
	op = true;
	Receive("Vanda", kCursing, RACE_VAMPIRE);
	op = false;
	world.user.IsMaster = TRUE;
	Receive("Vanda", kCursing, RACE_VAMPIRE);
	world.user.IsMaster = FALSE;
	ghost = true;
	Receive("Vanda", kCursing, RACE_VAMPIRE);
	CHECK(calls == std::vector<std::string>(3, "Vanda|2|7|" + kCursing));
	CHECK(world.Notice().empty());
}

TEST(WhisperHandler, AnotherRacesWhisperIsMaskedLessTheMoreINTThePlayerHas)
{
	World world;
	const std::string line = LongLine();
	Receive("Vanda", line, RACE_SLAYER);
	Receive("Vanda", line, RACE_VAMPIRE);
	status.SetStatus(MODIFY_CURRENT_INT, 150);
	Receive("Vanda", line, RACE_VAMPIRE);
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

TEST(WhisperHandler, MissingServicesStillSetTheTarget)
{
	World world;
	CommunicationPresentation::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	Receive("Vanda", kCursing);
	CHECK(calls.empty());
	CHECK(world.Target() == "Vanda ");
}
