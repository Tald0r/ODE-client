#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "GuildPresentationHost.h"
#include "LoginPresentationHost.h"
#include "PlayerStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Types/PlayerTypes.h"
#include "Gpackets/GCActiveGuildList.h"
#include "Gpackets/GCModifyGuildMemberInfo.h"
#include "Gpackets/GCWaitGuildList.h"
#include "Lpackets/LCCreatePCError.h"
#include "Lpackets/LCDeletePCError.h"
#include <cstdint>
#include <string>
#include <vector>

// The guild lists end the NPC question they answer and present the list;
// a guild-membership change updates the player's guild and presents it; and
// a refused character creation or deletion shows its error and goes back to
// character selection. The guild, player and login services are recorded.
namespace {

std::vector<std::string> calls;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
const void* lastList = nullptr;

std::string N(long long value) { return std::to_string(value); }

GuildPresentation::Host GuildHost()
{
	GuildPresentation::Host host{};
	host.ShowActiveGuildList = [](GCActiveGuildList* p) { lastList = p; calls.push_back("activelist"); };
	host.ShowWaitGuildList = [](GCWaitGuildList* p) { lastList = p; calls.push_back("waitlist"); };
	host.SetGuild = [](std::uint16_t guildID, std::uint8_t rank, const std::string& name) {
		calls.push_back("setguild|" + N(guildID) + "|" + N(rank) + "|" + name + "|" + g_pUserInformation->GuildName.GetString());
	};
	return host;
}
const GuildPresentation::Host guild = GuildHost();

const PlayerState::Host player{
	.GetWaitVerify = []() { calls.push_back("wait?"); return waitVerify; },
	.SetWaitVerifyNULL = []() { calls.push_back("clearwait"); waitVerify = PlayerState::WAIT_VERIFY_NULL; },
	.SetGuildNumber = [](int guildNumber) { calls.push_back("guildnumber|" + N(guildNumber)); },
};

LoginPresentation::Host LoginHost()
{
	LoginPresentation::Host host{};
	host.PopupErrorMessage = [](int errorID) { calls.push_back("error|" + N(errorID)); };
	host.QueueWaitSelectPCMode = []() { calls.push_back("queueselectpc"); };
	return host;
}
const LoginPresentation::Host login = LoginHost();

struct World : GameModelWorld
{
	const GuildPresentation::Host* oldGuild = GuildPresentation::SetHost(&guild);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const LoginPresentation::Host* oldLogin = LoginPresentation::SetHost(&login);
	World()
	{
		calls.clear();
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		lastList = nullptr;
	}
	~World()
	{
		LoginPresentation::SetHost(oldLogin);
		PlayerState::SetHost(oldPlayer);
		GuildPresentation::SetHost(oldGuild);
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

void ReceiveMembership(GuildID_t id, GuildMemberRank_t rank, const std::string& name)
{
	GCModifyGuildMemberInfo original;
	original.setGuildID(id);
	original.setGuildMemberRank(rank);
	original.setGuildName(name);
	GCModifyGuildMemberInfo packet;
	Decode(packet, original);
	GCModifyGuildMemberInfoHandler::execute(&packet, nullptr);
}

template<class PacketType, class Handler>
void ReceiveError(BYTE errorID, Handler execute)
{
	PacketType original;
	original.setErrorID(errorID);
	PacketType packet;
	Decode(packet, original);
	execute(&packet, nullptr);
}

// Every handler once, an NPC question pending for the first list (the second
// finds it already ended): eleven calls with every host.
void RunEveryHandler()
{
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	GCActiveGuildList active;
	GCActiveGuildListHandler::execute(&active, nullptr);
	GCWaitGuildList waiting;
	GCWaitGuildListHandler::execute(&waiting, nullptr);
	ReceiveMembership(42, 3, "Wolves");
	ReceiveError<LCCreatePCError>(ALREADY_REGISTER_ID, LCCreatePCErrorHandler::execute);
	ReceiveError<LCDeletePCError>(NOT_FOUND_PLAYER, LCDeletePCErrorHandler::execute);
}

} // namespace

TEST(GuildAndCharacterErrorHandlers, AGuildListEndsAPendingNPCQuestionThenIsPresented)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	GCActiveGuildList active;
	GCActiveGuildListHandler::execute(&active, nullptr);
	CHECK(lastList == &active);
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	GCWaitGuildList waiting;
	GCWaitGuildListHandler::execute(&waiting, nullptr);
	CHECK(lastList == &waiting);
	CHECK(calls == std::vector<std::string>({"wait?", "clearwait", "activelist", "wait?", "clearwait", "waitlist"}));

	// Waiting on anything else, the question is left alone.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	GCActiveGuildListHandler::execute(&active, nullptr);
	GCWaitGuildListHandler::execute(&waiting, nullptr);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_TRADE);
	CHECK(calls == std::vector<std::string>({"wait?", "activelist", "wait?", "waitlist"}));
}

TEST(GuildAndCharacterErrorHandlers, AMembershipChangeRecordsTheGuildBeforePresentingIt)
{
	World world;
	ReceiveMembership(42, 3, "Wolves");
	CHECK(std::string(g_pUserInformation->GuildName.GetString()) == "Wolves");
	CHECK_EQ(3, static_cast<int>(g_pUserInformation->GuildGrade));
	// The user information already names the guild when it is presented.
	CHECK(calls == std::vector<std::string>({"guildnumber|42", "setguild|42|3|Wolves|Wolves"}));
}

TEST(GuildAndCharacterErrorHandlers, ARefusedCharacterShowsItsErrorThenReturnsToSelection)
{
	World world;
	ReceiveError<LCCreatePCError>(ALREADY_REGISTER_ID, LCCreatePCErrorHandler::execute);
	ReceiveError<LCDeletePCError>(NOT_FOUND_PLAYER, LCDeletePCErrorHandler::execute);
	CHECK(calls == std::vector<std::string>({"error|" + N(ALREADY_REGISTER_ID), "queueselectpc",
		"error|" + N(NOT_FOUND_PLAYER), "queueselectpc"}));
}

TEST(GuildAndCharacterErrorHandlers, MissingHostsAndEntriesSkipOnlyTheServices)
{
	World world;
	GuildPresentation::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	LoginPresentation::SetHost(nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	// The user information is still updated.
	CHECK(std::string(g_pUserInformation->GuildName.GetString()) == "Wolves");
	const GuildPresentation::Host emptyGuild{};
	const PlayerState::Host emptyPlayer{};
	const LoginPresentation::Host emptyLogin{};
	CHECK(GuildPresentation::SetHost(&emptyGuild) == nullptr);
	CHECK(PlayerState::SetHost(&emptyPlayer) == nullptr);
	CHECK(LoginPresentation::SetHost(&emptyLogin) == nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(GuildPresentation::SetHost(&guild) == &emptyGuild);
	CHECK(PlayerState::SetHost(&player) == &emptyPlayer);
	CHECK(LoginPresentation::SetHost(&login) == &emptyLogin);
	RunEveryHandler();
	CHECK_EQ(11u, calls.size());
}
