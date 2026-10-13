#include "test_framework.h"
#include "packet_stream_access.h"
#include "GuildPresentationHost.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MStringArray.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCGuildChat.h"
#include "Gpackets/GCGuildMemberList.h"
#include "Gpackets/GCGuildResponse.h"
#include "Gpackets/GCOtherGuildName.h"
#include "Gpackets/GCShowGuildInfo.h"
#include "Gpackets/GCShowGuildJoin.h"
#include "Gpackets/GCShowGuildMemberInfo.h"
#include "Gpackets/GCShowUnionInfo.h"
#include "Gpackets/GCShowWaitGuildInfo.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

// The guild and union replies present what they receive through
// GuildPresentation::Host. Guild chat also filters by the chat manager's
// ID list and curse words, and the union reply maps its code to a message.
namespace {

const char* const kWordFile = "guild_presentation_test.txt";
std::vector<std::string> calls;

std::string Text(const char* text) { return text ? text : "(null)"; }
std::string Address(const void* pointer) { return std::to_string(reinterpret_cast<std::uintptr_t>(pointer)); }

const GuildPresentation::Host host{
	.AddChatToHistory = [](const char* message, const char* sender, int chatType, std::uint32_t color) {
		calls.push_back("chat|" + Text(message) + "|" + Text(sender) + "|" + std::to_string(chatType) + "|" + std::to_string(color));
	},
	.PopupMessage = [](int gameStringID) { calls.push_back("popup|" + std::to_string(gameStringID)); },
	.SetGuildName = [](int guildID, const std::string& guildName) {
		calls.push_back("name|" + std::to_string(guildID) + "|" + guildName);
	},
	.ShowGuildJoin = [](std::uint32_t joinFee, int rank, const char* date, const char* guildName, int guildID) {
		calls.push_back("join|" + std::to_string(joinFee) + "|" + std::to_string(rank) + "|" + Text(date) + "|"
			+ Text(guildName) + "|" + std::to_string(guildID));
	},
	.ShowGuildInfo = [](GCShowGuildInfo* p) { calls.push_back("info|" + Address(p)); },
	.ShowGuildMemberInfo = [](GCShowGuildMemberInfo* p) { calls.push_back("memberinfo|" + Address(p)); },
	.ShowGuildMemberList = [](GCGuildMemberList* p) { calls.push_back("memberlist|" + Address(p)); },
	.ShowUnionGuildMemberInfo = [](GCShowUnionInfo* p) { calls.push_back("union|" + Address(p)); },
	.ShowWaitGuildInfo = [](GCShowWaitGuildInfo* p) { calls.push_back("wait|" + Address(p)); },
};

struct World
{
	MStringArray table;
	MStringArray* oldTable = g_pGameStringTable;
	MChatManager chat;
	MChatManager* oldChat = g_pChatManager;
	const GuildPresentation::Host* oldHost = GuildPresentation::SetHost(&host);
	World()
	{
		g_pGameStringTable = &table;
		InitGameStringTable();
		MChatManager::SetHost(nullptr);
		g_pChatManager = &chat;
		calls.clear();
	}
	~World()
	{
		GuildPresentation::SetHost(oldHost);
		g_pChatManager = oldChat;
		g_pGameStringTable = oldTable;
		std::remove(kWordFile);
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

void ReceiveChat(BYTE type, const std::string& sender, const std::string& guild, const std::string& message)
{
	GCGuildChat original;
	original.setType(type);
	original.setSender(sender);
	original.setSendGuildName(guild);
	original.setColor(0x00ff8040);
	original.setMessage(message);
	GCGuildChat packet;
	Decode(packet, original);
	GCGuildChatHandler::execute(&packet, nullptr);
}

void ReceiveResponse(WORD code)
{
	GCGuildResponse original;
	original.setCode(code);
	original.setParameter(77);
	GCGuildResponse packet;
	Decode(packet, original);
	GCGuildResponseHandler::execute(&packet, nullptr);
}

// One reply of each kind, so every host entry is asked for once.
void RunEveryHandler()
{
	ReceiveChat(0, "Vlad", "Night", "hello");
	ReceiveResponse(OK);
	// Neither packet initialises its scalar fields, so every one is set.
	GCOtherGuildName name;
	name.setObjectID(1);
	name.setGuildID(2);
	name.setGuildName("Night");
	GCOtherGuildNameHandler::execute(&name, nullptr);
	GCShowGuildJoin join;
	join.setGuildID(2);
	join.setGuildName("Night");
	join.setGuildMemberRank(0);
	join.setJoinFee(0);
	GCShowGuildJoinHandler::execute(&join, nullptr);
	GCShowGuildInfo info;
	GCShowGuildMemberInfo memberInfo;
	GCGuildMemberList memberList;
	GCShowUnionInfo unionInfo;
	GCShowWaitGuildInfo waitInfo;
	GCShowGuildInfoHandler::execute(&info, nullptr);
	GCShowGuildMemberInfoHandler::execute(&memberInfo, nullptr);
	GCGuildMemberListHandler::execute(&memberList, nullptr);
	GCShowUnionInfoHandler::execute(&unionInfo, nullptr);
	GCShowWaitGuildInfoHandler::execute(&waitInfo, nullptr);
}

} // namespace

TEST(GuildPresentationHandlers, GuildAndUnionChatReachTheirHistoryChannels)
{
	World world;
	ReceiveChat(0, "Vlad", "Night", "meet at the gate");
	ReceiveChat(1, "Vlad", "Night", "union muster");
	CHECK_EQ(2u, calls.size());
	CHECK(calls[0] == "chat|meet at the gate|Vlad|4|16744512");
	CHECK(calls[1] == "chat|union muster|[Night]Vlad|5|16744512");
}

TEST(GuildPresentationHandlers, ChatSkipsEmptyLinesAndSendersTheListRefuses)
{
	World world;
	// The reader refuses an empty message; one that starts with NUL is the
	// empty line that reaches the handler.
	ReceiveChat(0, "Vlad", "Night", std::string("\0hidden", 7));
	world.chat.AddID("Pest");
	ReceiveChat(0, "Pest", "Night", "spam");
	world.chat.SetIgnoreMode();
	ReceiveChat(1, "Vlad", "Night", "not on the list");
	ReceiveChat(1, "Pest", "Night", "on the list");
	CHECK_EQ(1u, calls.size());
	CHECK(calls[0] == "chat|on the list|[Night]Pest|5|16744512");
}

TEST(GuildPresentationHandlers, ChatMasksCurseWordsBeforePresenting)
{
	World world;
	{
		std::ofstream words(kWordFile, std::ios::binary | std::ios::trunc);
		words << "darn\n";
	}
	world.chat.LoadFromFileCurse(kWordFile);
	ReceiveChat(0, "Vlad", "Night", "darn it");
	CHECK_EQ(1u, calls.size());
	CHECK(calls[0] == "chat|xxxx it|Vlad|4|16744512");
}

TEST(GuildPresentationHandlers, UnionRepliesMapEachCodeToItsMessage)
{
	World world;
	const int expected[] = {
		UI_STRING_MESSAGE_TOTAL_GUILD_MESSAGE_OK,
		UI_STRING_MESSAGE_TOTAL_GUILD_ALREADY_IN_UNION,
		UI_STRING_MESSAGE_TOTAL_GUILD_ALREADY_OFFER_SOMETHING,
		UI_STRING_MESSAGE_TOTAL_GUILD_TARGET_IS_NOT_MASTER,
		UI_STRING_MESSAGE_TOTAL_GUILD_NOT_IN_UNION,
		UI_STRING_MESSAGE_TOTAL_GUILD_MASTER_CANNOT_QUIT,
		UI_STRING_MESSAGE_TOTAL_GUILD_NO_TARGET_UNION,
		UI_STRING_MESSAGE_TOTAL_GUILD_NOT_YOUR_UNION,
		UI_STRING_MESSAGE_TOTAL_GUILD_SOURCE_IS_NOT_MASTER,
		UI_STRING_MESSAGE_TOTAL_UNION_PENALTY,
		UI_STRING_MESSAGE_UNION_ERROR_NO_SLOT,
		UI_STRING_MESSAGE_TOO_MANY_MEMBER,
	};
	CHECK_EQ(TOO_MANY_MEMBER + 1, static_cast<int>(sizeof(expected) / sizeof(expected[0])));
	for (int code = OK; code <= TOO_MANY_MEMBER; ++code)
	{
		calls.clear();
		ReceiveResponse(static_cast<WORD>(code));
		CHECK_EQ(1u, calls.size());
		CHECK(!calls.empty() && calls[0] == "popup|" + std::to_string(expected[code]));
	}
	calls.clear();
	ReceiveResponse(TOO_MANY_MEMBER + 1);
	ReceiveResponse(0xFFFF);
	CHECK(calls.empty());
}

TEST(GuildPresentationHandlers, NameAndJoinRepliesForwardTheirFields)
{
	World world;
	GCOtherGuildName nameOriginal;
	nameOriginal.setObjectID(9001);
	nameOriginal.setGuildID(321);
	nameOriginal.setGuildName("Night");
	GCOtherGuildName name;
	Decode(name, nameOriginal);
	GCOtherGuildNameHandler::execute(&name, nullptr);

	GCShowGuildJoin joinOriginal;
	joinOriginal.setGuildID(321);
	joinOriginal.setGuildName("Night");
	joinOriginal.setGuildMemberRank(2);
	joinOriginal.setJoinFee(500000);
	GCShowGuildJoin join;
	Decode(join, joinOriginal);
	GCShowGuildJoinHandler::execute(&join, nullptr);

	CHECK_EQ(2u, calls.size());
	CHECK(calls[0] == "name|321|Night");
	CHECK(calls[1] == "join|500000|2|(null)|Night|321");
}

TEST(GuildPresentationHandlers, InformationRepliesPassTheirPacketThrough)
{
	World world;
	GCShowGuildInfo info;
	GCShowGuildMemberInfo memberInfo;
	GCGuildMemberList memberList;
	GCShowUnionInfo unionInfo;
	GCShowWaitGuildInfo waitInfo;
	GCShowGuildInfoHandler::execute(&info, nullptr);
	GCShowGuildMemberInfoHandler::execute(&memberInfo, nullptr);
	GCGuildMemberListHandler::execute(&memberList, nullptr);
	GCShowUnionInfoHandler::execute(&unionInfo, nullptr);
	GCShowWaitGuildInfoHandler::execute(&waitInfo, nullptr);
	CHECK_EQ(5u, calls.size());
	CHECK(calls[0] == "info|" + Address(&info));
	CHECK(calls[1] == "memberinfo|" + Address(&memberInfo));
	CHECK(calls[2] == "memberlist|" + Address(&memberList));
	CHECK(calls[3] == "union|" + Address(&unionInfo));
	CHECK(calls[4] == "wait|" + Address(&waitInfo));
}

TEST(GuildPresentationHandlers, AMissingHostSkipsPresentationOnly)
{
	World world;
	GuildPresentation::SetHost(nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	GuildPresentation::SetHost(&host);
	RunEveryHandler();
	CHECK_EQ(9u, calls.size());
}

TEST(GuildPresentationHandlers, EmptyEntriesSkipAndEachCallReadsTheCurrentHost)
{
	World world;
	const GuildPresentation::Host empty{};
	CHECK(GuildPresentation::SetHost(&empty) == &host);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(GuildPresentation::SetHost(&host) == &empty);
	ReceiveResponse(NOT_ENOUGH_SLOT);
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "popup|" + std::to_string(UI_STRING_MESSAGE_UNION_ERROR_NO_SLOT));
}
