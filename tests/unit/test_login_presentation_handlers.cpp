#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "DialogPresentationHost.h"
#include "LoginPresentationHost.h"
#include "ClientPlayer.h"
#include "MStorage.h"
#include "PCOustersInfo.h"
#include "PCSlayerInfo.h"
#include "PCVampireInfo.h"
#include "UserInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Cpackets/CLGetPCList.h"
#include "Lpackets/LCCreatePCOK.h"
#include "Lpackets/LCDeletePCOK.h"
#include "Lpackets/LCPCList.h"
#include "Lpackets/LCQueryResultCharacterName.h"
#include "Lpackets/LCQueryResultPlayerID.h"
#include "Lpackets/LCSelectPCError.h"
#include "Lpackets/LCVersionCheckError.h"
#include <cstdint>
#include <string>
#include <vector>

// Login-server replies about characters, IDs and versions present what they
// receive and change login modes through LoginPresentation::Host, and pop
// message boxes through DialogPresentation::Host. The character list also
// fills the player's slots and empties the storage box, and a created or
// deleted character asks the server for the list again.
namespace {

std::vector<std::string> calls;
bool selectingCharacter = false;

std::string Text(const char* text) { return text ? text : "(null)"; }
std::string Message(int id) { return "popup|" + std::string((*g_pGameStringTable)[id].GetString()); }

const DialogPresentation::Host dialog{
	.PopupFreeMessageDlg = [](const char* message) { calls.push_back("popup|" + Text(message)); },
};

const LoginPresentation::Host host{
	.NewCharacterCreateOk = []() { calls.push_back("createok"); },
	.DeleteCharacterOK = []() { calls.push_back("deleteok"); },
	.AlreadyExistIDMessage = []() { calls.push_back("idtaken"); },
	.NoAlreadyExistIDMessage = []() { calls.push_back("idfree"); },
	.StartCharacterManager = []() { calls.push_back("charmanager"); },
	.SetSlayerCharacter = [](int slot, PCSlayerInfo* p) { calls.push_back("slayer|" + std::to_string(slot) + "|" + p->getName()); },
	.SetVampireCharacter = [](int slot, PCVampireInfo* p) { calls.push_back("vampire|" + std::to_string(slot) + "|" + p->getName()); },
	.SetOustersCharacter = [](int slot, PCOustersInfo* p) { calls.push_back("ousters|" + std::to_string(slot) + "|" + p->getName()); },
	.RunAskGoBilingPage = [](int n) { calls.push_back("billing|" + std::to_string(n)); },
	.PopupExitMessageDlg = [](const char* message) { calls.push_back("exitpopup|" + Text(message)); },
	.Sleep = [](std::uint32_t milliseconds) { calls.push_back("sleep|" + std::to_string(milliseconds)); },
	.EnterSelectPCMode = []() { calls.push_back("enterselectpc"); },
	.QueueWaitPCListMode = []() { calls.push_back("queuepclist"); },
	.QueueLoginWrongMode = []() { calls.push_back("queueloginwrong"); },
	.IsSelectPCMode = []() { calls.push_back("selectpc?"); return selectingCharacter; },
};

struct World : GameModelWorld
{
	UserInformation user;
	UserInformation* oldUser = g_pUserInformation;
	MStorage* oldStorage = g_pStorage;
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const LoginPresentation::Host* oldHost = LoginPresentation::SetHost(&host);
	World()
	{
		InitGameStringTable();
		g_pUserInformation = &user;
		g_pStorage = nullptr;
		calls.clear();
		selectingCharacter = false;
	}
	~World()
	{
		LoginPresentation::SetHost(oldHost);
		DialogPresentation::SetHost(oldDialog);
		delete g_pStorage;
		g_pStorage = oldStorage;
		g_pUserInformation = oldUser;
	}
};

// The login connection's player, its output kept unsent for inspection.
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

void Preloaded(Packet& packet, const std::vector<unsigned char>& bytes)
{
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	Preloaded(packet, SocketOutputStreamTestAccess::Bytes(encoded));
}

void ReceiveIDQuery(bool taken)
{
	// The reply has no setters: a five-letter ID, then whether it is taken.
	LCQueryResultPlayerID packet;
	Preloaded(packet, {5, 'K', 'a', 'r', 'i', 'n', static_cast<unsigned char>(taken ? 1 : 0)});
	CHECK_EQ(taken, packet.isExist());
	LCQueryResultPlayerIDHandler::execute(&packet, nullptr);
}

void ReceiveNameQuery(const char* name, bool taken)
{
	LCQueryResultCharacterName original;
	original.setCharacterName(name);
	original.setExist(taken);
	LCQueryResultCharacterName packet;
	Decode(packet, original);
	LCQueryResultCharacterNameHandler::execute(&packet, nullptr);
}

void ReceiveSelectError(BYTE code)
{
	LCSelectPCError original;
	original.setCode(code);
	LCSelectPCError packet;
	Decode(packet, original);
	LCSelectPCErrorHandler::execute(&packet, nullptr);
}

PCSlayerInfo* Slayer(const char* name, HairStyle hair)
{
	PCSlayerInfo* info = new PCSlayerInfo;
	info->setName(name);
	info->setHairStyle(hair);
	return info;
}

PCVampireInfo* Vampire(const char* name)
{
	PCVampireInfo* info = new PCVampireInfo;
	info->setName(name);
	return info;
}

PCOustersInfo* Ousters(const char* name)
{
	PCOustersInfo* info = new PCOustersInfo;
	info->setName(name);
	return info;
}

// Every host entry is asked for at least once: sixteen calls with a host.
void RunEveryHandler()
{
	LoginPlayer created, deleted;
	LCCreatePCOK createOK;
	LCCreatePCOKHandler::execute(&createOK, &created);
	LCDeletePCOK deleteOK;
	LCDeletePCOKHandler::execute(&deleteOK, &deleted);
	CHECK(created.getPlayerStatus() == CPS_AFTER_SENDING_CL_GET_PC_LIST);
	CHECK(deleted.getPlayerStatus() == CPS_AFTER_SENDING_CL_GET_PC_LIST);
	ReceiveIDQuery(true);
	ReceiveIDQuery(false);
	ReceiveNameQuery("Nobody", true);
	ReceiveSelectError(SELECT_PC_CANNOT_PLAY);
	LCVersionCheckError version;
	LCVersionCheckErrorHandler::execute(&version, nullptr);
	LCPCList list;
	list.setPCInfo(SLOT1, Slayer("Ariel", HAIR_STYLE2));
	list.setPCInfo(SLOT2, Vampire("Vlad"));
	list.setPCInfo(SLOT3, Ousters("Ilse"));
	g_pStorage = new MStorage;
	LCPCListHandler::execute(&list, nullptr);
	CHECK(g_pStorage == nullptr);
	CHECK(g_pUserInformation->Character[2] == "Ilse");
}

} // namespace

TEST(LoginPresentationHandlers, ACreatedOrDeletedCharacterAsksForTheListAgain)
{
	World world;
	CLGetPCList request;
	const std::vector<unsigned char> expected = Encoded(request);
	CHECK(!expected.empty());

	LoginPlayer player;
	player.setPlayerStatus(CPS_AFTER_SENDING_CL_CREATE_PC);
	LCCreatePCOK created;
	LCCreatePCOKHandler::execute(&created, &player);
	CHECK(calls == std::vector<std::string>{"createok"});
	CHECK(player.getPlayerStatus() == CPS_AFTER_SENDING_CL_GET_PC_LIST);
	CHECK(player.Sent() == expected);

	calls.clear();
	LoginPlayer other;
	LCDeletePCOK deleted;
	LCDeletePCOKHandler::execute(&deleted, &other);
	CHECK(calls == std::vector<std::string>({"deleteok", "queuepclist"}));
	CHECK(other.getPlayerStatus() == CPS_AFTER_SENDING_CL_GET_PC_LIST);
	CHECK(other.Sent() == expected);
}

TEST(LoginPresentationHandlers, AnIDQueryReportsWhetherTheIDIsTaken)
{
	World world;
	ReceiveIDQuery(true);
	ReceiveIDQuery(false);
	CHECK(calls == std::vector<std::string>({"idtaken", "idfree"}));
}

TEST(LoginPresentationHandlers, ANameQueryAnswersOnlyTheNameAskedAboutWhileChoosingACharacter)
{
	World world;
	selectingCharacter = true;
	world.user.WhisperID = "Nyx";
	ReceiveNameQuery("Nyx", true);
	CHECK(world.user.WhisperID.GetString() == nullptr);
	world.user.WhisperID = "Nyx";
	ReceiveNameQuery("Nyx", false);
	CHECK(world.user.WhisperID.GetString() == nullptr);
	CHECK(calls == std::vector<std::string>({"selectpc?", Message(STRING_ERROR_ALREADY_REGISTER_ID),
		"selectpc?", Message(STRING_MESSAGE_CAN_REGISTER_NAME)}));

	// Another name, or any name outside character selection, is ignored and
	// the asked-about name is kept.
	calls.clear();
	world.user.WhisperID = "Nyx";
	ReceiveNameQuery("Pax", true);
	selectingCharacter = false;
	ReceiveNameQuery("Nyx", true);
	CHECK(world.user.WhisperID == "Nyx");
	CHECK(calls == std::vector<std::string>({"selectpc?", "selectpc?"}));
}

TEST(LoginPresentationHandlers, AVersionMismatchQueuesTheWrongLoginModeThenExplainsOnExit)
{
	World world;
	LCVersionCheckError packet;
	LCVersionCheckErrorHandler::execute(&packet, nullptr);
	const std::string message = Message(UI_STRING_MESSAGE_CHECK_VERSION_ERROR).substr(6);
	CHECK(calls == std::vector<std::string>({"queueloginwrong", "exitpopup|" + message}));
}

TEST(LoginPresentationHandlers, ARefusedSelectionExplainsPausesThenReturnsToSelection)
{
	World world;
	const struct { BYTE code; std::vector<std::string> expected; } cases[] = {
		{SELECT_PC_CANNOT_PLAY, {"billing|0", "sleep|1000", "enterselectpc"}},
		{SELECT_PC_NOT_BILLING_CHECK, {Message(STRING_MESSAGE_SELECT_PC_NOT_BILLING_CHECK), "sleep|1000", "enterselectpc"}},
		{SELECT_PC_CANNOT_PLAY_BY_ATTR, {Message(STRING_MESSAGE_LOGIN_ERROR_NONPK), "sleep|1000", "enterselectpc"}},
		{SELECT_PC_ERROR_NULL, {"enterselectpc"}},
		{200, {"enterselectpc"}},
	};
	for (const auto& c : cases)
	{
		calls.clear();
		ReceiveSelectError(c.code);
		CHECK(calls == c.expected);
	}
}

TEST(LoginPresentationHandlers, TheCharacterListFillsEachOccupiedSlotThenEmptiesTheStorageBox)
{
	World world;
	world.user.FaceStyleSlot[1] = 7;
	world.user.FaceStyleSlot[2] = 5;
	g_pStorage = new MStorage;
	LCPCList full;
	full.setPCInfo(SLOT1, Slayer("Ariel", HAIR_STYLE3));
	full.setPCInfo(SLOT2, Vampire("Vlad"));
	full.setPCInfo(SLOT3, Ousters("Ilse"));
	LCPCListHandler::execute(&full, nullptr);
	CHECK(calls == std::vector<std::string>({"charmanager", "slayer|0|Ariel", "vampire|1|Vlad", "ousters|2|Ilse", "enterselectpc"}));
	CHECK(g_pStorage == nullptr);
	CHECK(world.user.Character[0] == "Ariel");
	CHECK(world.user.Character[1] == "Vlad");
	CHECK(world.user.Character[2] == "Ilse");
	CHECK(world.user.Race[0] == RACE_SLAYER);
	CHECK(world.user.Race[1] == RACE_VAMPIRE);
	CHECK(world.user.Race[2] == RACE_OUSTERS);
	CHECK_EQ(static_cast<int>(HAIR_STYLE3), world.user.FaceStyleSlot[0]);
	CHECK_EQ(0, world.user.FaceStyleSlot[1]);
	CHECK_EQ(0, world.user.FaceStyleSlot[2]);

	// Empty slots are skipped and keep what they held.
	calls.clear();
	LCPCList one;
	one.setPCInfo(SLOT2, Slayer("Mira", HAIR_STYLE1));
	LCPCListHandler::execute(&one, nullptr);
	CHECK(calls == std::vector<std::string>({"charmanager", "slayer|1|Mira", "enterselectpc"}));
	CHECK(world.user.Character[0] == "Ariel");
	CHECK(world.user.Character[1] == "Mira");
	CHECK(world.user.Race[1] == RACE_SLAYER);
	CHECK(world.user.Character[2] == "Ilse");
	CHECK(world.user.Race[2] == RACE_OUSTERS);
}

TEST(LoginPresentationHandlers, MissingHostsAndEntriesSkipPresentationOnly)
{
	World world;
	LoginPresentation::SetHost(nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(!LoginPresentation::IsSelectPCMode());
	const LoginPresentation::Host empty{};
	CHECK(LoginPresentation::SetHost(&empty) == nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(!LoginPresentation::IsSelectPCMode());
	CHECK(LoginPresentation::SetHost(&host) == &empty);
	RunEveryHandler();
	CHECK_EQ(16u, calls.size());
}
