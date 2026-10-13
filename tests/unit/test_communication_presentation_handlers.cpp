#include "test_framework.h"
#include "packet_stream_access.h"
#include "CommunicationPresentationHost.h"
#include "MGameStringTable.h"
#include "MStringArray.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAddressListVerify.h"
#include "Gpackets/GCFriendChatting.h"
#include "Gpackets/GCSMSAddressList.h"
#include "Gpackets/GCWhisperFailed.h"
#include <cstdint>
#include <string>
#include <vector>

// Friend, SMS address-book and whisper-failure replies present what they
// receive through CommunicationPresentation::Host. The friend reply picks
// the presentation by command; the address-book reply checks the request
// the player made (TempInformation) and always clears it afterwards.
namespace {

std::vector<std::string> calls;

std::string Text(const char* text) { return text ? text : "(null)"; }
std::string Address(const void* pointer) { return std::to_string(reinterpret_cast<std::uintptr_t>(pointer)); }

#define FRIEND_ENTRY(name) [](GCFriendChatting* p) { calls.push_back(std::string(#name "|") + Address(p)); }

const CommunicationPresentation::Host host{
	.AddChatToHistory = [](const char* message, const char* sender, int chatType, std::uint32_t color) {
		calls.push_back("chat|" + Text(message) + "|" + Text(sender) + "|" + std::to_string(chatType) + "|" + std::to_string(color));
	},
	.PopupMessage = [](int gameStringID) { calls.push_back("popup|" + std::to_string(gameStringID)); },
	.SetSMSList = [](void* pAddressList) {
		const auto& list = *static_cast<std::vector<AddressUnit*>*>(pAddressList);
		std::string text = "smslist|" + std::to_string(list.size());
		for (const AddressUnit* unit : list)
			text += "|" + std::to_string(unit->ElementID) + "," + unit->CharacterName + "," + unit->CustomName + "," + unit->Number;
		calls.push_back(text);
	},
	.AddSMSList = [](std::uint32_t element, char* name, char* id, char* num) {
		calls.push_back("addsms|" + std::to_string(element) + "|" + Text(name) + "|" + Text(id) + "|" + Text(num));
	},
	.DeleteSMSList = [](std::uint32_t id) { calls.push_back("deletesms|" + std::to_string(id)); },
	.SMS_Set_Charge = [](int charge) { calls.push_back("charge|" + std::to_string(charge)); },
	.SMS_ERROR = [](std::uint32_t parameter) { calls.push_back("smserror|" + std::to_string(parameter)); },
	.FriendChatting_UpdateFriendList = FRIEND_ENTRY(update),
	.FriendChatting_Message = FRIEND_ENTRY(message),
	.RunFriendRequest = FRIEND_ENTRY(request),
	.RunFriendRefuse = FRIEND_ENTRY(refuse),
	.RunFriendWait = FRIEND_ENTRY(wait),
	.RunFriendOK = FRIEND_ENTRY(ok),
	.RunFriendExist = FRIEND_ENTRY(exist),
	.RunFriendBlack = FRIEND_ENTRY(black),
	.RunFriendDelete = FRIEND_ENTRY(delete),
};

#undef FRIEND_ENTRY

struct World
{
	MStringArray table;
	MStringArray* oldTable = g_pGameStringTable;
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const CommunicationPresentation::Host* oldHost = CommunicationPresentation::SetHost(&host);
	World()
	{
		g_pGameStringTable = &table;
		InitGameStringTable();
		g_pTempInformation = &temp;
		calls.clear();
	}
	~World()
	{
		CommunicationPresentation::SetHost(oldHost);
		g_pTempInformation = oldTemp;
		g_pGameStringTable = oldTable;
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

void ReceiveVerify(BYTE code, DWORD parameter)
{
	GCAddressListVerify original;
	original.setCode(code);
	original.setParameter(parameter);
	GCAddressListVerify packet;
	Decode(packet, original);
	GCAddressListVerifyHandler::execute(&packet, nullptr);
}

// The handler reads the friend reply's pointer only, so a decoded packet
// with a name and message is enough to tell the presentations apart.
void ReceiveFriend(DWORD command, GCFriendChatting& packet)
{
	GCFriendChatting original;
	original.setCommand(command);
	original.setPlayerName("Vlad");
	original.setMessage("hello");
	Decode(packet, original);
	GCFriendChattingHandler::execute(&packet, nullptr);
}

void AskToAdd(World& world)
{
	world.temp.SetMode(TempInformation::MODE_SMS_ADD_ADDRESS);
	world.temp.StrValue1 = "slayer";
	world.temp.StrValue2 = "Friend";
	world.temp.StrValue3 = "0101234567";
}

void AskToDelete(World& world, intptr_t element)
{
	world.temp.SetMode(TempInformation::MODE_SMS_DELETE_ADDRESS);
	world.temp.Value1 = element;
}

// One reply for each of the sixteen host entries.
void RunEveryHandler(World& world)
{
	GCWhisperFailed whisper;
	GCWhisperFailedHandler::execute(&whisper, nullptr);
	GCSMSAddressList list;
	AddressUnit unit{1, "slayer", "Friend", "0101234567"};
	list.getAddresses().push_back(&unit);
	GCSMSAddressListHandler::execute(&list, nullptr);
	list.getAddresses().clear();
	AskToAdd(world);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_ADD_OK, 1);
	AskToDelete(world, 1);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_DELETE_OK, 0);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_ADD_FAIL, 0);
	ReceiveVerify(GCAddressListVerify::SMS_CHARGE_OK, 5);
	ReceiveVerify(GCAddressListVerify::SMS_SEND_FAIL, 9);
	for (DWORD command : {GC_UPDATE, GC_MESSAGE, GC_ADD_FRIEND_REQUEST, GC_ADD_FRIEND_REFUSE, GC_ADD_FRIEND_WAIT,
		GC_ADD_FRIEND_OK, GC_ADD_FRIEND_EXIST, GC_ADD_FRIEND_BLACK, GC_FRIEND_DELETE})
	{
		GCFriendChatting packet;
		ReceiveFriend(command, packet);
	}
}

} // namespace

TEST(CommunicationPresentationHandlers, AFailedWhisperIsReportedInWhiteOnChatLineFive)
{
	World world;
	GCWhisperFailed packet;
	GCWhisperFailedHandler::execute(&packet, nullptr);
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "chat|" + std::string((*g_pGameStringTable)[STRING_MESSAGE_WHISPER_FAILED].GetString())
		+ "|(null)|5|" + std::to_string(0xFFFFFFu));
}

TEST(CommunicationPresentationHandlers, AnAddressListIsPresentedOnlyWhenItHasEntries)
{
	World world;
	GCSMSAddressList empty;
	GCSMSAddressListHandler::execute(&empty, nullptr);
	CHECK(calls.empty());

	GCSMSAddressList original;
	AddressUnit first{7, "slayer", "Friend", "0101234567"};
	AddressUnit second{8, "vampire", "Rival", "0107654321"};
	original.getAddresses().push_back(&first);
	original.getAddresses().push_back(&second);
	GCSMSAddressList packet;
	Decode(packet, original);
	original.getAddresses().clear();
	GCSMSAddressListHandler::execute(&packet, nullptr);
	CHECK_EQ(1u, calls.size());
	CHECK(!calls.empty() && calls[0] == "smslist|2|7,slayer,Friend,0101234567|8,vampire,Rival,0107654321");
	// The executable's SMS list adopts these units; the recording host does not.
	for (AddressUnit* unit : packet.getAddresses()) delete unit;
	packet.getAddresses().clear();
}

TEST(CommunicationPresentationHandlers, AddAndDeleteConfirmationsNeedTheMatchingRequest)
{
	World world;
	AskToAdd(world);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_ADD_OK, 42);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_ADD_OK, 43);
	AskToDelete(world, 42);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_DELETE_OK, 0);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	AskToAdd(world);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_DELETE_OK, 0);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK_EQ(2u, calls.size());
	CHECK(calls.size() == 2 && calls[0] == "addsms|42|slayer|Friend|0101234567");
	CHECK(calls.size() == 2 && calls[1] == "deletesms|42");
}

TEST(CommunicationPresentationHandlers, FailuresShowTheServerErrorOrTheirOwnMessage)
{
	World world;
	const BYTE codes[] = {GCAddressListVerify::ADDRESS_LIST_ADD_FAIL, GCAddressListVerify::ADDRESS_LIST_DELETE_FAIL,
		GCAddressListVerify::SMS_SEND_FAIL};
	const int messages[] = {UI_STRING_MESSAGE_SMS_ADD_FAIL, UI_STRING_MESSAGE_SMS_DELETE_FAIL, UI_STRING_MESSAGE_SMS_SEND_FAIL};
	for (int i = 0; i < 3; ++i)
	{
		calls.clear();
		AskToAdd(world);
		ReceiveVerify(codes[i], 0);
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
		AskToDelete(world, 3);
		ReceiveVerify(codes[i], GCAddressListVerify::SMS_SEND_FAIL_NOT_ENOUGH_CHARGE);
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
		CHECK_EQ(2u, calls.size());
		CHECK(calls.size() == 2 && calls[0] == "popup|" + std::to_string(messages[i]));
		CHECK(calls.size() == 2 && calls[1] == "smserror|" + std::to_string(GCAddressListVerify::SMS_SEND_FAIL_NOT_ENOUGH_CHARGE));
	}
}

TEST(CommunicationPresentationHandlers, SendAndChargeRepliesUpdateTheChargeAndUnknownCodesOnlyClearTheRequest)
{
	World world;
	AskToAdd(world);
	ReceiveVerify(GCAddressListVerify::SMS_SEND_OK, 12);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	AskToDelete(world, 3);
	ReceiveVerify(GCAddressListVerify::SMS_CHARGE_OK, 30);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	AskToAdd(world);
	ReceiveVerify(GCAddressListVerify::ADDRESS_LIST_VERIFY_MAX, 1);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	ReceiveVerify(0xFF, 1);
	CHECK_EQ(3u, calls.size());
	CHECK(calls.size() == 3 && calls[0] == "charge|12");
	CHECK(calls.size() == 3 && calls[1] == "popup|" + std::to_string(UI_STRING_MESSAGE_SMS_SEND_OK));
	CHECK(calls.size() == 3 && calls[2] == "charge|30");
}

TEST(CommunicationPresentationHandlers, FriendRepliesPickThePresentationForTheirCommand)
{
	World world;
	const struct { DWORD command; const char* entry; } cases[] = {
		{GC_UPDATE, "update"}, {GC_MESSAGE, "message"}, {GC_ADD_FRIEND_REQUEST, "request"},
		{GC_ADD_FRIEND_REFUSE, "refuse"}, {GC_ADD_FRIEND_WAIT, "wait"}, {GC_ADD_FRIEND_OK, "ok"},
		{GC_ADD_FRIEND_EXIST, "exist"}, {GC_ADD_FRIEND_BLACK, "black"}, {GC_FRIEND_DELETE, "delete"},
	};
	for (const auto& c : cases)
	{
		calls.clear();
		GCFriendChatting packet;
		ReceiveFriend(c.command, packet);
		CHECK_EQ(1u, calls.size());
		CHECK(!calls.empty() && calls[0] == std::string(c.entry) + "|" + Address(&packet));
	}
	calls.clear();
	for (DWORD command : {static_cast<DWORD>(GC_ADD_FRIEND_ERROR), static_cast<DWORD>(GC_NO_ONLINE),
		static_cast<DWORD>(CG_FRIEND_DELETE), static_cast<DWORD>(MAX_GC), DWORD(0xFFFFFFFF)})
	{
		GCFriendChatting packet;
		ReceiveFriend(command, packet);
	}
	CHECK(calls.empty());
}

TEST(CommunicationPresentationHandlers, MissingHostsAndEntriesSkipPresentationOnly)
{
	World world;
	CommunicationPresentation::SetHost(nullptr);
	RunEveryHandler(world);
	CHECK(calls.empty());
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	const CommunicationPresentation::Host empty{};
	CHECK(CommunicationPresentation::SetHost(&empty) == nullptr);
	RunEveryHandler(world);
	CHECK(calls.empty());
	CHECK(CommunicationPresentation::SetHost(&host) == &empty);
	RunEveryHandler(world);
	CHECK_EQ(16u, calls.size());
}
