#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "PacketDispatcherTestAccess.h"
#include "UIMessageManager.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "GameSocket.h"
#include "Exception.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "LoginPresentationHost.h"
#include "WorldEventsHost.h"
#include "DialogWindowHost.h"
#include "UIDialog.h"
#include "MChatManager.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "TempInformation.h"
#include "MParty.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MMusic.h"
#include "MMusicTable.h"
#include "DXLib/CDirectSound.h"
#include "DXLib/COGGSTREAM.h"
#include "FrameClock.h"
#include "InputState.h"
#include "CrtCompat.h"
#include "Properties.h"
#include "UserOption.h"
#include "KeyAccelerator.h"
#include "AcceleratorDef.h"
#include "ColorSetDef.h"
#include "CServerInformation.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_GlobalResource.h"
#include "VS_UI_Dialog.h"
#include "VS_UI_Message.h"
#include "pi_core.h"
#include "u_window.h"
#include "timer2.h"
#include "Fl2.h"
#include "Ci.h"
#include "Cpackets/CGConnectSetKey.h"
#include "Cpackets/CGPartySay.h"
#include "Cpackets/CLCreatePC.h"
#include "Cpackets/CLDeletePC.h"
#include "Cpackets/CLLogin.h"
#include "Cpackets/CLQueryPlayerID.h"
#include "Cpackets/CLRegisterPlayer.h"
#include "Cpackets/CLSelectPC.h"
#include "Cpackets/CLVersionCheck.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#include <vector>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ASAN_SEES_FREES 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(ASAN_SEES_FREES)
#define ASAN_SEES_FREES 1
#endif
#ifdef ASAN_SEES_FREES
#include <sanitizer/asan_interface.h>
#endif

extern BYTE g_macAddress[6];
extern BOOL g_bSetHotKey;
extern C_VS_UI_DIALOG* g_msg_free_text;
extern CSpriteSurface* g_pBack;
extern CSpriteSurface* g_pLast;
void PlayTitleMusic();
void PlayGameMusic();
void ExecuteLogout();

// The UI's results on the title and character screens (task 4.258 moved
// Client/UIMessageManager.cpp into gameclient), its lines 1-2108: the
// hosts, the table Init fills and Execute's dispatch with its guard in the
// game, the title and game music, the logout outside the game, and the
// handlers of the login and character screens (a new character, the
// registration, the ID check, a deletion, quitting, the login, leaving the
// character screen, entering the game) and the chat's UTF-8 cut. Every
// packet is serialised for real and compared byte for byte, so a field the
// handler sets wrong, or a write() that throws, shows. gC_vs_ui is never
// initialised here: its window calls do nothing.
//
// Not run:
// - ExecuteLogout in the game: it always starts the title UI
//   (gC_vs_ui.StartTitle builds every title window, which reads the skin
//   manager and the sprite packs, and leaves it in the process-wide
//   gC_vs_ui, whose EndTitle is private). It needs a seam first.
// - A NULL void_ptr to NEW_CHARACTER, CHECK_EXIST_ID (IsValidID formats a
//   NULL "%s"), DELETE_CHARACTER and LOGIN (read even in the wrong mode):
//   recorded defect, each dereferences it unchecked.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

bool Same(const std::vector<std::string>& expected)
{
	if (calls == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

std::string Mode(CLIENT_MODE mode) { return "mode|" + N(mode); }
std::string Send(const Packet& packet) { return "send|" + N(packet.getPacketID()); }

// The game connection: what is sent is recorded in the calls and kept,
// serialised, in its output stream.
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override
	{
		calls.push_back(Send(*pPacket));
		Player::sendPacket(pPacket);
	}
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};
Wire* current = nullptr;

// The packets as one stream sends them (each header carries the stream's
// running sequence number).
std::vector<unsigned char> Encoded(const std::vector<const Packet*>& packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

// The login and the registration send the client's version first only
// in builds without _DEBUG (UIMessageManager.cpp's
// `#if !defined(_DEBUG) && !defined(OUTPUT_DEBUG)`); MSVC's Debug builds
// define _DEBUG.
#if !defined(_DEBUG) && !defined(OUTPUT_DEBUG)
const bool kVersionCheck = true;
#else
const bool kVersionCheck = false;
#endif

// The packets, or the calls, without the version where it is not sent.
std::vector<const Packet*> Sends(std::vector<const Packet*> packets, const Packet* version)
{
	if (!kVersionCheck)
		packets.erase(std::remove(packets.begin(), packets.end(), version), packets.end());
	return packets;
}
std::vector<std::string> Calls(std::vector<std::string> calls, const std::string& version)
{
	if (!kVersionCheck)
		calls.erase(std::remove(calls.begin(), calls.end(), version), calls.end());
	return calls;
}


BOOL socketAnswer = TRUE;
const UIMessages::Host uiHost = []() {
	UIMessages::Host host{};
	host.InitSocket = []() { calls.push_back("init socket"); return socketAnswer; };
	host.LoadingAddonSPK = [](bool bLoading) { calls.push_back("addon|" + N(bLoading)); return true; };
	host.ReleaseGameObject = []() { calls.push_back("release"); };
	host.StopLoadingThread = []() { calls.push_back("stop loading"); };
	host.SaveLastSelectedCharacter = [](int slot) { calls.push_back("last|" + N(slot)); };
	host.SetMode = [](CLIENT_MODE mode) { calls.push_back(Mode(mode)); };
	return host;
}();

// FlushSocketOutput says how many bytes the connection had queued.
const LoginPresentation::Host loginHost = []() {
	LoginPresentation::Host host{};
	host.FlushSocketOutput = []() { calls.push_back("flush|" + N(current != nullptr ? static_cast<long long>(current->Sent().size()) : -1)); };
	return host;
}();

const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.SetWeather = [](int weather, int level) { calls.push_back("weather|" + N(weather) + "|" + N(level)); };
	host.PlayMusicCurrentZone = []() { calls.push_back("zone music"); };
	return host;
}();

// UIDialog's message boxes: one call when a box shows, with its place,
// extra height, buttons and lines.
std::string pending;
int windowCount = 0;
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewMessage = [](int x, int y, int extraHeight, std::uint16_t buttons) {
		pending = "popup|" + N(x) + "|" + N(y) + "|" + N(extraHeight) + "|" + N(buttons);
		return reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(++windowCount));
	};
	host.SetMessage = [](DialogWindow::Window*, const char* const* pLines, int count, bool noFit) {
		for (int i = 0; i < count; ++i) pending += std::string("|") + pLines[i];
		if (!noFit) pending += "|fit";
	};
	host.Start = [](DialogWindow::Window*) { calls.push_back(pending); };
	host.Finish = [](DialogWindow::Window*) { calls.push_back("hide popup"); };
	host.Delete = [](DialogWindow::Window*) { calls.push_back("close popup"); };
	return host;
}();
std::string Popup(const std::string& text, int buttons = UI_DIALOG_OK) { return "popup|-1|-1|0|" + N(buttons) + "|" + text; }

// The key packet goes through the client's own dispatcher.
void KeyDispatched(Packet* pPacket, Player* pPlayer)
{
	calls.push_back("dispatch|" + N(pPacket->getPacketID()) + (pPlayer == g_pSocket ? "|socket" : "|other"));
}
PacketID_t KeyID() { return CGConnectSetKey().getPacketID(); }

// The table and the handlers, which are protected.
struct Probe : UIMessageManager
{
	using UIMessageManager::Execute_UI_NEW_CHARACTER;
	using UIMessageManager::Execute_UI_RUN_NEWUSER_REGISTRATION;
	using UIMessageManager::Execute_UI_CHECK_EXIST_ID;
	using UIMessageManager::Execute_UI_INFO_CLOSE;
	using UIMessageManager::Execute_UI_DELETE_CHARACTER;
	using UIMessageManager::Execute_UI_TERMINATION;
	using UIMessageManager::Execute_UI_LOGIN;
	using UIMessageManager::Execute_UI_CHARACTER_MANAGER_FINISHED;
	using UIMessageManager::Execute_UI_CONNECT;
	using UIMessageManager::Execute_UI_CHAT_RETURN;
	UI_MESSAGE_FUNCTION Slot(int message) const { return m_UIMessageFunction[message]; }
	void Plant(int message, UI_MESSAGE_FUNCTION fn) { m_UIMessageFunction[message] = fn; }
};

int probeRuns = 0;
void ProbeHandler(intptr_t left, intptr_t right, void* pVoid)
{
	++probeRuns;
	calls.push_back("probe|" + N(left) + "|" + N(right) + "|" + N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pVoid))));
}

const char* const kBadNameWords = "ui_messages_1_bad_names.txt";
const char* const kBadNameFile = "ui_messages_1_bad_names.bin";
const char* const kChatWords = "ui_messages_1_chat_words.txt";
const char* const kOptionFile = "ui_messages_1_user_option.set";

void WriteFile(const char* path, const std::string& bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
bool FileExists(const char* path)
{
	std::ifstream in(path, std::ios::binary);
	return in.good();
}

// Whether a string the handler took was freed. Only ASan can tell, by
// the poison its quarantine keeps on a freed block; without it this says
// yes, so the check is the sanitized trees' alone.
bool Freed(const void* p)
{
#ifdef ASAN_SEES_FREES
	return __asan_address_is_poisoned(p) != 0;
#else
	(void)p;
	return true;
#endif
}

// Strings as the UI hands them: new[] for the login, malloc() for the
// registration (whose handlers free() them).
char* NewArray(const char* text)
{
	const std::size_t size = std::strlen(text) + 1;
	char* p = new char[size];
	std::memcpy(p, text, size);
	return p;
}
char* Malloced(const char* text)
{
	const std::size_t size = std::strlen(text) + 1;
	char* p = static_cast<char*>(std::malloc(size));
	std::memcpy(p, text, size);
	return p;
}

const char* const kIDLength = "Names take %d to %d letters.";
const char* const kInvalidID = "Invalid name.";
const char* const kHomepage = "Register on the web.";
const char* const kDeny = "Registration is closed.";
const char* const kPasswordSpecial = "No quotes in passwords.";
const char* const kConnecting = "Connecting...";

// The title and character screens: the login connection, the hosts, the
// message boxes, the curse lists (the chat's has "darn", the bad-name
// file "heck"), the files, the strings, and a manager Init filled.
struct Login : GameModelWorld
{
	CLIENT_MODE oldMode = g_Mode;
	ClientPlayer* oldSocket = g_pSocket;
	UIDialog* oldDialog = g_pUIDialog;
	DWORD oldLock = UIDialog::IsLockInput();
	MChatManager* oldChat = g_pChatManager;
	const MChatHost* oldChatHost = MChatManager::SetHost(NULL);
	Properties* oldFiles = g_pFileDef;
	UserOption* oldOption = g_pUserOption;
	MInventory* oldInventory = g_pInventory;
	CServerInformation* oldServers = g_pServerInformation;
	MParty* oldParty = g_pParty;
	DWORD oldSeqL = g_dwSeqNumL;
	DWORD oldSeqG = g_dwSeqNumG;
	BYTE oldMac[6];
	const UIMessages::Host* oldUiHost = UIMessages::SetHost(&uiHost);
	const LoginPresentation::Host* oldLoginHost = LoginPresentation::SetHost(&loginHost);
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	PacketDispatcher::HandlerFn oldKey = PacketDispatcher::handlerFor(KeyID());
	Wire wire;
	UIDialog dialog;
	MChatManager chat;
	Properties files;
	MInventory inventory;
	Probe manager;

	Login()
	{
		calls.clear();
		windowCount = 0;
		socketAnswer = TRUE;
		std::memcpy(oldMac, g_macAddress, sizeof(oldMac));
		const BYTE mac[6] = {0x0A, 0x1B, 0x2C, 0x3D, 0x4E, 0x5F};
		std::memcpy(g_macAddress, mac, sizeof(mac));
		g_pSocket = &wire;
		current = &wire;
		dialog.Init();
		g_pUIDialog = &dialog;
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		WriteFile(kChatWords, "darn\n");
		chat.LoadFromFileCurse(kChatWords);
		g_pChatManager = &chat;
		{
			MChatManager badNames;
			WriteFile(kBadNameWords, "heck\n");
			badNames.LoadFromFileCurse(kBadNameWords);
			badNames.SaveToFile(kBadNameFile);
		}
		std::remove(kOptionFile);
		files.setProperty("FILE_INFO_CHAT_2", kBadNameFile);
		files.setProperty("FILE_INFO_USEROPTION", kOptionFile);
		g_pFileDef = &files;
		g_pUserOption = NULL;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		g_pServerInformation = NULL;
		g_pParty = NULL;
		g_pGameStringTable->Init(MAX_GAME_STRING + 1);
		g_pGameStringTable->Set(STRING_USER_REGISTER_ID_LENGTH, kIDLength);
		g_pGameStringTable->Set(STRING_USER_REGISTER_INVALID_ID, kInvalidID);
		g_pGameStringTable->Set(STRING_USER_REGISTER_HOMEPAGE, kHomepage);
		g_pGameStringTable->Set(STRING_USER_REGISTER_DENY, kDeny);
		g_pGameStringTable->Set(STRING_USER_REGISTER_PASSWORD_SPECIAL, kPasswordSpecial);
		g_pGameStringTable->Set(STRING_MESSAGE_CONNECTING_SERVER, kConnecting);
		g_pGameStringTable->Set(UI_STRING_MESSAGE_MASTER_NAME, "GM");
		g_pUserInformation->GameVersion = 1234;
		g_pClientConfig->NEW_USER_REGISTERATION_MODE = ClientConfig::NUR_CLIENT;
		manager.Init();
		// The dispatcher's key slot is this file's while the test runs.
		if (oldKey != &KeyDispatched)
		{
			if (oldKey != NULL) PacketDispatcherTestAccess::Clear();
			PacketDispatcher::registerHandler(KeyID(), &KeyDispatched);
		}
	}
	~Login()
	{
		if (oldKey != &KeyDispatched)
		{
			PacketDispatcherTestAccess::Clear();
			if (oldKey != NULL) PacketDispatcher::registerHandler(KeyID(), oldKey);
		}
		dialog.Release();
		if (oldLock & FLAG_UIDIALOG_LOCK_PCTALK) UIDialog::SetLockInputPCTalk();
		if (oldLock & FLAG_UIDIALOG_LOCK_MESSAGE) UIDialog::SetLockInputMessage();
		inventory.Release();
		std::remove(kChatWords);
		std::remove(kBadNameWords);
		std::remove(kBadNameFile);
		std::remove(kOptionFile);
		DialogWindow::SetHost(oldWindows);
		LoginPresentation::SetHost(oldLoginHost);
		UIMessages::SetHost(oldUiHost);
		std::memcpy(g_macAddress, oldMac, sizeof(oldMac));
		g_dwSeqNumG = oldSeqG;
		g_dwSeqNumL = oldSeqL;
		g_pParty = oldParty;
		g_pServerInformation = oldServers;
		g_pInventory = oldInventory;
		g_pUserOption = oldOption;
		g_pFileDef = oldFiles;
		MChatManager::SetHost(oldChatHost);
		g_pChatManager = oldChat;
		g_pUIDialog = oldDialog;
		current = nullptr;
		g_pSocket = oldSocket;
		g_Mode = oldMode;
	}

	void Run(DWORD message, intptr_t left = 0, intptr_t right = 0, void* pVoid = NULL) { manager.Execute(message, left, right, pVoid); }
	// Closes the box a test opened, then forgets the calls.
	void Clear() { dialog.CloseMessageDlg(); calls.clear(); }
	std::vector<unsigned char> Sent() const { return wire.Sent(); }
};

// The message g_ShowMessage shows needs the UI's base, windows and global
// resource; its lines, wrapped to the box, are joined again.
void Received(DWORD, intptr_t, intptr_t, void*) {}
void NoTimer() {}
struct DialogPeek : C_VS_UI_DIALOG
{
	static const std::vector<std::string>& Lines(C_VS_UI_DIALOG& dialog) { return dialog.*(&DialogPeek::m_vs_msg); }
};
struct MessageUi
{
	Base* oldBase = gpC_base;
	CI* oldIme = gC_ci;
	RECT oldRect = g_GameRect;
	PI_ProcessRunner* oldRunner = gpC_process_runner;
	WindowManager* oldWindows = gpC_window_manager;
	C_GLOBAL_RESOURCE* oldResource = gpC_global_resource;
	UserOption* oldOption = g_pUserOption;
	C_VS_UI_DIALOG* oldFreeText = g_msg_free_text;
	CI_KOREAN ime;
	CSpriteSurface back;
	Base base;
	PI_ProcessRunner runner;
	WindowManager windows;
	UserOption option;
	C_GLOBAL_RESOURCE* pResource = nullptr;
	timer_id_t blink = INVALID_TID;
	MessageUi()
	{
		spritectl_init();
		g_GameRect = {0, 0, 800, 600};
		back.Init(10, 8);
		gC_ci = &ime;
		// Base::Init starts the IME's cursor blinker, the timer after this one.
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		blink = probe + 1;
		gC_timer2.Delete(probe);
		base.Init(&back, Received);
		gpC_base = &base;
		gpC_process_runner = &runner;
		gpC_window_manager = &windows;
		g_pUserOption = &option;
		pResource = new C_GLOBAL_RESOURCE;
		gpC_global_resource = pResource;
		g_msg_free_text = NULL;
	}
	~MessageUi()
	{
		DeleteNew(g_msg_free_text);
		g_msg_free_text = oldFreeText;
		delete pResource;
		gpC_global_resource = oldResource;
		g_pUserOption = oldOption;
		gpC_window_manager = oldWindows;
		gpC_process_runner = oldRunner;
		const bool blinkerGone = gC_timer2.Delete(blink);
		CHECK(blinkerGone);
		g_SetFL2Surface(nullptr);
		gpC_base = oldBase;
		gC_ci = oldIme;
		g_GameRect = oldRect;
	}
	// The message shown, its wrapped lines joined, "" for none. The dialog
	// wraps by the platform font's widths, so where a line breaks (and
	// whether a space goes there) differs: compare through Squeezed.
	std::string Shown() const
	{
		if (g_msg_free_text == NULL) return "";
		std::string text;
		for (const std::string& line : DialogPeek::Lines(*g_msg_free_text)) text += (text.empty() ? "" : " ") + line;
		return text;
	}
	static std::string Squeezed(std::string text)
	{
		text.erase(std::remove(text.begin(), text.end(), ' '), text.end());
		return text;
	}
};

} // namespace

//----------------------------------------------------------------------
// UIMessages::SetHost and the forwarders: without a host (or without an
// entry) the socket is not made, so nothing is sent, and no mode is
// entered.
//----------------------------------------------------------------------
TEST(UIMessages1Host, SetHostAnswersTheHostItReplaces)
{
	const UIMessages::Host other{};
	const UIMessages::Host* old = UIMessages::SetHost(&uiHost);
	CHECK(UIMessages::SetHost(&other) == &uiHost);
	CHECK(UIMessages::SetHost(NULL) == &other);
	CHECK(UIMessages::SetHost(old) == NULL);
}

TEST(UIMessages1Host, WithoutAHostOrItsEntriesNothingConnects)
{
	Login scene;
	const UIMessages::Host empty{};
	for (const UIMessages::Host* pHost : {static_cast<const UIMessages::Host*>(NULL), &empty})
	{
		UIMessages::SetHost(pHost);
		calls.clear();
		g_Mode = MODE_MAINMENU;
		LOGIN login = {NewArray("Alpha1"), NewArray("secret1")};
		scene.Run(UI_LOGIN, 0, 0, &login);
		// InitSocket answers FALSE: the strings go, nothing else happens.
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
		CHECK(scene.Sent().empty());
		CHECK(g_pUserInformation->UserID.GetString() == NULL);
		scene.Run(UI_TERMINATION);
		g_Mode = MODE_WAIT_SELECTPC;
		scene.Run(UI_CHARACTER_MANAGER_FINISHED);
		CHECK(Same({}));
		// A character is still chosen: sent and flushed, but nothing
		// loaded, remembered or entered.
		Wire wire;
		g_pSocket = &wire;
		current = &wire;
		g_pUIDialog = NULL;
		g_pUserInformation->Character[0] = "Alpha";
		g_pUserInformation->Race[0] = RACE_SLAYER;
		scene.Run(UI_CONNECT, 0);
		CLSelectPC expected;
		expected.setPCName("Alpha");
		expected.setPCType(PC_SLAYER);
		CHECK(Same({Send(expected), "flush|" + N(static_cast<long long>(Encoded({&expected}).size()))}));
		CHECK(wire.Sent() == Encoded({&expected}));
		g_pUIDialog = &scene.dialog;
		current = &scene.wire;
		g_pSocket = &scene.wire;
	}
}

//----------------------------------------------------------------------
// The table: empty until Init, then 221 handlers in 231 slots.
//----------------------------------------------------------------------
TEST(UIMessages1Table, TheConstructorLeavesEverySlotEmpty)
{
	// Made over garbage, so a slot it left alone shows.
	alignas(Probe) unsigned char memory[sizeof(Probe)];
	std::memset(memory, 0xAB, sizeof(memory));
	Probe* pManager = new (memory) Probe;
	int filled = 0;
	for (int message = 0; message < MAX_UI_MESSAGE; ++message) filled += pManager->Slot(message) != NULL;
	CHECK_EQ(0, filled);
	pManager->~Probe();
}

TEST(UIMessages1Table, InitFillsEverySlotButTen)
{
	CHECK_EQ(231, int(MAX_UI_MESSAGE));
	Probe manager;
	manager.Init();
	const std::vector<int> empty = {INVALID_MESSAGE, UI_CLOSE_BBS, UI_REQUEST_UNION_REQUEST_DENY, UI_SELECT_PERSNALSHOP_SLOT,
		UI_CLOSE_PERSNALSHOP_MESSAGE, UI_INPUT_PERSNALSHOP_MESSAGE,
		// registered only under __TEST_SUB_INVENTORY__, which nothing defines
		UI_CLOSE_INVENTORY_SUB, UI_ITEM_DROP_TO_INVENTORY_SUB, UI_ITEM_PICKUP_FROM_INVENTORY_SUB, UI_ITEM_USE_SUBINVENTORY};
	std::vector<Probe::UI_MESSAGE_FUNCTION> handlers;
	for (int message = 0; message < MAX_UI_MESSAGE; ++message)
	{
		const bool bEmpty = std::find(empty.begin(), empty.end(), message) != empty.end();
		CHECK_EQ(bEmpty, manager.Slot(message) == NULL);
		if (manager.Slot(message) != NULL) handlers.push_back(manager.Slot(message));
	}
	// 221 handlers. Not counted by address: an optimising linker (MSVC's
	// /OPT:ICF in Release) folds identical bodies, such as the empty ones,
	// into one.
	CHECK_EQ(221, int(handlers.size()));
	// The title and character screens' handlers.
	CHECK(manager.Slot(UI_NEW_CHARACTER) == &Probe::Execute_UI_NEW_CHARACTER);
	CHECK(manager.Slot(UI_RUN_NEWUSER_REGISTRATION) == &Probe::Execute_UI_RUN_NEWUSER_REGISTRATION);
	CHECK(manager.Slot(UI_CHECK_EXIST_ID) == &Probe::Execute_UI_CHECK_EXIST_ID);
	CHECK(manager.Slot(UI_INFO_CLOSE) == &Probe::Execute_UI_INFO_CLOSE);
	CHECK(manager.Slot(UI_DELETE_CHARACTER) == &Probe::Execute_UI_DELETE_CHARACTER);
	CHECK(manager.Slot(UI_TERMINATION) == &Probe::Execute_UI_TERMINATION);
	CHECK(manager.Slot(UI_LOGIN) == &Probe::Execute_UI_LOGIN);
	CHECK(manager.Slot(UI_CHARACTER_MANAGER_FINISHED) == &Probe::Execute_UI_CHARACTER_MANAGER_FINISHED);
	CHECK(manager.Slot(UI_CONNECT) == &Probe::Execute_UI_CONNECT);
	CHECK(manager.Slot(UI_CHAT_RETURN) == &Probe::Execute_UI_CHAT_RETURN);
}

//----------------------------------------------------------------------
// Execute: outside the game it runs the slot's handler with the message's
// arguments and checks nothing but the table's bound; an empty slot is
// skipped.
//----------------------------------------------------------------------
TEST(UIMessages1Execute, OutsideTheGameTheHandlerRunsWithTheArguments)
{
	Login scene;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	TempInformation* oldTemp = g_pTempInformation;
	g_pZone = NULL;
	g_pPlayer = NULL;
	g_pInventory = NULL;
	g_pTempInformation = NULL;
	int value = 0;
	scene.manager.Plant(UI_INFO_CLOSE, &ProbeHandler);
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_WAIT_SELECTPC, MODE_WAIT_LOGINOK, MODE_QUIT})
	{
		g_Mode = mode;
		calls.clear();
		scene.Run(UI_INFO_CLOSE, -7, 65536, &value);
		CHECK(Same({"probe|-7|65536|" + N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(&value)))}));
		// The empty slots: nothing.
		calls.clear();
		scene.Run(INVALID_MESSAGE, 1, 2, &value);
		scene.Run(UI_CLOSE_BBS, 1, 2, &value);
		scene.Run(UI_SELECT_PERSNALSHOP_SLOT, 1, 2, &value);
		CHECK(Same({}));
	}
	CHECK(scene.Sent().empty());
	g_pTempInformation = oldTemp;
	g_pPlayer = oldPlayer;
	g_pZone = oldZone;
}

TEST(UIMessages1Execute, OutsideTheGameAMessagePastTheTableIsDropped)
{
	// On the heap, so ASan sees a read past the table.
	Login scene;
	std::unique_ptr<Probe> pManager(new Probe);
	pManager->Init();
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_WAIT_SELECTPC, MODE_QUIT})
	{
		g_Mode = mode;
		pManager->Execute(MAX_UI_MESSAGE, 1, 2, NULL);
		pManager->Execute(0xFFFFFFFFu, 1, 2, NULL);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
}

namespace {
// The game's five: the zone, the player (of each race), the inventory,
// the player's race's gear and the request register.
struct InGame : Login
{
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	std::unique_ptr<MZone> zone;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	TempInformation temp;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	TempInformation* oldTemp = g_pTempInformation;
	MPlayer* me = nullptr;

	InGame()
	{
		creatureTypes.Init(3);
		testfw::MutableRow(creatureTypes, 0).SetCreatureTribe(CREATURETRIBE_SLAYER, 0);
		testfw::MutableRow(creatureTypes, 1).SetCreatureTribe(CREATURETRIBE_VAMPIRE, 0);
		testfw::MutableRow(creatureTypes, 2).SetCreatureTribe(CREATURETRIBE_OUSTERS, 0);
		g_pCreatureTable = &creatureTypes;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		// MZone reads the horn file g_pFileDef names, if any.
		Properties* pFiles = g_pFileDef;
		g_pFileDef = NULL;
		zone.reset(new MZone(12, 9));
		g_pFileDef = pFiles;
		me = new MPlayer;
		g_pPlayer = me;
		me->SetCreatureType(0);
		All();
		g_Mode = MODE_GAME;
		manager.Plant(UI_INFO_CLOSE, &ProbeHandler);
	}
	~InGame()
	{
		delete me;
		g_pPlayer = oldPlayer;
		g_pTempInformation = oldTemp;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pZone = oldZone;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	void All()
	{
		g_pZone = zone.get();
		g_pPlayer = me;
		g_pInventory = &inventory;
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
		g_pTempInformation = &temp;
	}
	// Whether the probe in UI_INFO_CLOSE's slot ran.
	bool Passes()
	{
		const int before = probeRuns;
		Run(UI_INFO_CLOSE, 3, 4, NULL);
		return probeRuns == before + 1;
	}
};
} // namespace

TEST(UIMessages1Execute, InTheGameTheFiveLetTheMessageThrough)
{
	InGame scene;
	// A player of each race needs only his own race's gear.
	for (int type = 0; type < 3; ++type)
	{
		scene.me->SetCreatureType(TYPE_CREATURETYPE(type));
		scene.All();
		CHECK(scene.Passes());
		if (type != 0) g_pSlayerGear = NULL;
		if (type != 1) g_pVampireGear = NULL;
		if (type != 2) g_pOustersGear = NULL;
		CHECK(scene.Passes());
	}
	calls.clear();
	scene.Run(UI_INFO_CLOSE, 3, 4, NULL);
	CHECK(Same({"probe|3|4|0"}));
}

TEST(UIMessages1Execute, InTheGameEachMissingOneDropsTheMessage)
{
	InGame scene;
	for (int type = 0; type < 3; ++type)
	{
		scene.me->SetCreatureType(TYPE_CREATURETYPE(type));
		scene.All();
		g_pZone = NULL;
		CHECK(!scene.Passes());
		scene.All();
		g_pPlayer = NULL;
		CHECK(!scene.Passes());
		scene.All();
		g_pInventory = NULL;
		CHECK(!scene.Passes());
		scene.All();
		g_pTempInformation = NULL;
		CHECK(!scene.Passes());
		scene.All();
		if (type == 0) g_pSlayerGear = NULL;
		if (type == 1) g_pVampireGear = NULL;
		if (type == 2) g_pOustersGear = NULL;
		CHECK(!scene.Passes());
		scene.All();
		CHECK(scene.Passes());
	}
}

TEST(UIMessages1Execute, InTheGameAMessagePastTheTableIsDropped)
{
	InGame scene;
	calls.clear();
	scene.Run(MAX_UI_MESSAGE, 1, 2, NULL);
	scene.Run(0xFFFFFFFFu, 1, 2, NULL);
	// The last slot is in range.
	scene.manager.Plant(MAX_UI_MESSAGE - 1, &ProbeHandler);
	scene.Run(MAX_UI_MESSAGE - 1, 5, 6, NULL);
	CHECK(Same({"probe|5|6|0"}));
	CHECK(scene.Sent().empty());
}

//----------------------------------------------------------------------
// ExecuteLogout outside the game only logs (the game's branch is not run,
// see the top).
//----------------------------------------------------------------------
TEST(UIMessages1Logout, OutsideTheGameNothingHappens)
{
	Login scene;
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const DWORD oldFrame = g_CurrentFrame;
	const BOOL oldHotKey = g_bSetHotKey;
	const BOOL oldUIInput = g_bUIInput;
	g_CurrentFrame = 77;
	g_bSetHotKey = TRUE;
	g_bUIInput = TRUE;
	g_pUserInformation->WhisperID = "Friend";
	const PlayerStatus status = scene.wire.getPlayerStatus();
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_WAIT_SELECTPC, MODE_WAIT_RECONNECT_LOGIN, MODE_QUIT})
	{
		g_Mode = mode;
		ExecuteLogout();
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	CHECK(scene.wire.getPlayerStatus() == status);
	CHECK_EQ(77u, g_CurrentFrame);
	CHECK_EQ(TRUE, g_bSetHotKey);
	CHECK_EQ(TRUE, g_bUIInput);
	CHECK(std::string("Friend") == g_pUserInformation->WhisperID.GetString());
	g_bUIInput = oldUIInput;
	g_bSetHotKey = oldHotKey;
	g_CurrentFrame = oldFrame;
	WorldEvents::SetHost(oldWorld);
}

//----------------------------------------------------------------------
// PlayTitleMusic and PlayGameMusic (CHANGE_OPTION's): the options choose
// the WAV (OGG stream) or the MIDI player (g_Music). g_SDLAudio is never
// initialised here, so the stream is never opened; g_Music's state shows
// what it was asked.
//----------------------------------------------------------------------
namespace {
const char* const kStreamFile = "ui_messages_1_stream.ogg";
struct Music : GameModelWorld
{
	MMusic oldMusic = g_Music;
	FILE* oldFile = g_oggfile;
	UserOption* oldOption = g_pUserOption;
	MUSIC_TABLE* oldTable = g_pMusicTable;
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	UserOption option;
	MUSIC_TABLE table;
	Music()
	{
		calls.clear();
		CHECK(!g_SDLAudio.IsInit());
		g_Music = MMusic();
		g_Music.Init(NULL);
		g_oggfile = NULL;
		g_pUserOption = &option;
		option.VolumeMusic = 3;
		table.Init(3);
		g_pMusicTable = &table;
		g_pClientConfig->MUSIC_THEME = 2;
	}
	~Music()
	{
		if (g_oggfile != NULL) std::fclose(g_oggfile);
		std::remove(kStreamFile);
		WorldEvents::SetHost(oldWorld);
		g_pMusicTable = oldTable;
		g_pUserOption = oldOption;
		g_oggfile = oldFile;
		g_Music = oldMusic;
	}
	// g_Music: playing, paused (loaded, not playing) or stopped, at a volume.
	void Playing() { g_Music.m_bLoad = true; g_Music.m_bPlay = true; g_Music.m_bPause = false; g_Music.m_Volume = 7; }
	void Paused() { g_Music.m_bLoad = true; g_Music.m_bPlay = false; g_Music.m_bPause = true; g_Music.m_Volume = 7; }
	void Stopped() { g_Music.m_bLoad = false; g_Music.m_bPlay = false; g_Music.m_bPause = false; g_Music.m_Volume = 7; }
	std::string State() const
	{
		return std::string(g_Music.m_bLoad ? "loaded" : "empty") + (g_Music.m_bPlay ? "|play" : "") + (g_Music.m_bPause ? "|pause" : "")
			+ "|" + N(g_Music.m_Volume);
	}
	FILE* Stream() { g_oggfile = Basic::OpenFile(kStreamFile, "wb"); CHECK(g_oggfile != NULL); return g_oggfile; }
};
} // namespace

TEST(UIMessages1Music, TheTitleMusicOnTheStream)
{
	Music music;
	music.option.PlayWaveMusic = TRUE;
	// On, with a theme: the MIDI stops and the open stream is closed (it is
	// reopened only with the audio up).
	music.option.PlayMusic = TRUE;
	music.Playing();
	music.Stream();
	PlayTitleMusic();
	CHECK(music.State() == "empty|7");
	CHECK(g_oggfile == NULL);
	// No theme: the stream stays.
	g_pClientConfig->MUSIC_THEME = MUSICID_NULL;
	music.Playing();
	FILE* pStream = music.Stream();
	PlayTitleMusic();
	CHECK(music.State() == "empty|7");
	CHECK(g_oggfile == pStream);
	// Off: the MIDI stops, the stream stays.
	g_pClientConfig->MUSIC_THEME = 2;
	music.option.PlayMusic = FALSE;
	music.Playing();
	PlayTitleMusic();
	CHECK(music.State() == "empty|7");
	CHECK(g_oggfile == pStream);
	CHECK(Same({}));
}

TEST(UIMessages1Music, TheTitleMusicOnTheMidiPlayer)
{
	Music music;
	music.option.PlayWaveMusic = FALSE;
	music.option.PlayMusic = TRUE;
	FILE* pStream = music.Stream();
	// Playing: it goes on, at its volume.
	music.Playing();
	PlayTitleMusic();
	CHECK(music.State() == "loaded|play|7");
	// Paused: resumed at the options' volume (VolumeMusic * 0x1000 + 0xFFF).
	music.Paused();
	PlayTitleMusic();
	CHECK(music.State() == "loaded|play|" + N(3 * 0x1000 + 0x0FFF));
	// Stopped: the theme plays at that volume; without a theme nothing does.
	music.Stopped();
	PlayTitleMusic();
	CHECK(music.State() == "loaded|play|" + N(3 * 0x1000 + 0x0FFF));
	g_pClientConfig->MUSIC_THEME = MUSICID_NULL;
	music.Stopped();
	PlayTitleMusic();
	CHECK(music.State() == "empty|" + N(3 * 0x1000 + 0x0FFF));
	// Recorded defect: the volume is a WORD, so 16 wraps to 0x0FFF.
	music.option.VolumeMusic = 16;
	music.Paused();
	PlayTitleMusic();
	CHECK(music.State() == "loaded|play|" + N(0x0FFF));
	// Off: a playing theme pauses; one not playing is stopped, which
	// MMusic::Stop does only to one playing, so a paused one stays.
	music.option.PlayMusic = FALSE;
	music.Playing();
	PlayTitleMusic();
	CHECK(music.State() == "loaded|pause|7");
	music.Paused();
	PlayTitleMusic();
	CHECK(music.State() == "loaded|pause|7");
	// The stream is left alone (closed only with the audio up).
	CHECK(g_oggfile == pStream);
	CHECK(Same({}));
}

TEST(UIMessages1Music, TheGameMusicIsTheZones)
{
	Music music;
	// The stream: the MIDI stops; with music on, the zone's plays.
	music.option.PlayWaveMusic = TRUE;
	music.option.PlayMusic = TRUE;
	music.Playing();
	PlayGameMusic();
	CHECK(music.State() == "empty|7");
	CHECK(Same({"zone music"}));
	calls.clear();
	music.option.PlayMusic = FALSE;
	music.Playing();
	PlayGameMusic();
	CHECK(music.State() == "empty|7");
	CHECK(Same({}));
	// The MIDI player: playing goes on; else the volume is set and the
	// zone's plays.
	music.option.PlayWaveMusic = FALSE;
	music.option.PlayMusic = TRUE;
	music.Playing();
	PlayGameMusic();
	CHECK(music.State() == "loaded|play|7");
	CHECK(Same({}));
	music.Paused();
	PlayGameMusic();
	CHECK(music.State() == "loaded|pause|" + N(3 * 0x1000 + 0x0FFF));
	CHECK(Same({"zone music"}));
	calls.clear();
	// Off: a playing theme pauses; a paused one is "stopped" (a no-op).
	music.option.PlayMusic = FALSE;
	music.Playing();
	PlayGameMusic();
	CHECK(music.State() == "loaded|pause|7");
	music.Paused();
	PlayGameMusic();
	CHECK(music.State() == "loaded|pause|7");
	CHECK(Same({}));
}

//----------------------------------------------------------------------
// UI_NEW_CHARACTER (character screen): a name of 4 to 10 characters,
// valid (IsValidID, '-' and '_' allowed) and in neither curse list
// (the chat's, then the bad-name file read on every call) is sent with
// the look and the stats; anything else pops up why.
//----------------------------------------------------------------------
namespace {
NEW_CHARACTER Character(char* name, int slot, int face)
{
	NEW_CHARACTER c{};
	c.sz_name = name;
	c.race = RACE_VAMPIRE;
	c.bl_female = true;
	c.STR = 11;
	c.DEX = 12;
	c.INT = 13;
	c.slot = slot;
	c.face = face;
	c.skin_color = 21;
	c.hair_color = 22;
	c.coat_color = 23;
	c.trouser_color = 24;
	return c;
}
CLCreatePC Created(const char* name, Slot slot, Race race, Sex sex, HairStyle hair, int hairColor, int skinColor, int coat,
	int trouser)
{
	CLCreatePC packet;
	packet.setName(name);
	packet.setSlot(slot);
	packet.setRace(race);
	packet.setSex(sex);
	packet.setHairStyle(hair);
	packet.setHairColor(Color_t(hairColor));
	packet.setSkinColor(Color_t(skinColor));
	packet.setShirtColor(Color_t(coat), MAIN_COLOR);
	packet.setShirtColor(Color_t(coat), SUB_COLOR);
	packet.setJeansColor(Color_t(trouser), MAIN_COLOR);
	packet.setJeansColor(Color_t(trouser), SUB_COLOR);
	packet.setSTR(11);
	packet.setDEX(12);
	packet.setINT(13);
	return packet;
}
} // namespace

TEST(UIMessages1NewCharacter, OnlyOnTheCharacterScreen)
{
	Login scene;
	char name[] = "Abcde";
	NEW_CHARACTER c = Character(name, 0, 0);
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_GAME, MODE_WAIT_CREATEPCOK})
	{
		g_Mode = mode;
		scene.manager.Execute_UI_NEW_CHARACTER(0, 0, &c);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	CHECK(g_pUserInformation->Character[0].GetString() == NULL);
}

TEST(UIMessages1NewCharacter, ANameOutsideFourToTenIsRefused)
{
	Login scene;
	g_Mode = MODE_WAIT_SELECTPC;
	const PlayerStatus status = scene.wire.getPlayerStatus();
	for (const char* text : {"", "Abc", "Abcdefghijk"})
	{
		scene.Clear();
		std::string copy(text);
		copy.push_back('\0');
		NEW_CHARACTER c = Character(&copy[0], 1, 0);
		scene.Run(UI_NEW_CHARACTER, 0, 0, &c);
		CHECK(Same({Popup("Names take 4 to 10 letters.")}));
	}
	CHECK(scene.Sent().empty());
	CHECK(scene.wire.getPlayerStatus() == status);
	CHECK(g_pUserInformation->Character[1].GetString() == NULL);
}

TEST(UIMessages1NewCharacter, AnInvalidOrCursedNameIsRefused)
{
	Login scene;
	g_Mode = MODE_WAIT_SELECTPC;
	// A space, a leading digit, a character other than '-' and '_', a curse
	// of the chat's list, a curse of the bad-name file.
	for (const char* text : {"Ab cd", "1abcd", "Ab$cd", "Ab.cd", "Darnit", "Heckle"})
	{
		scene.Clear();
		std::string copy(text);
		copy.push_back('\0');
		NEW_CHARACTER c = Character(&copy[0], 2, 0);
		scene.Run(UI_NEW_CHARACTER, 0, 0, &c);
		CHECK(Same({Popup(kInvalidID)}));
		// The name is not changed by the curse check (it checks a copy).
		CHECK(std::string(text) == c.sz_name);
	}
	CHECK(scene.Sent().empty());
	CHECK(g_pUserInformation->Character[2].GetString() == NULL);
}

TEST(UIMessages1NewCharacter, AValidNameIsCreatedInItsSlot)
{
	// Names of 4 and 10, with '-' and '_'; each slot and face.
	const char* names[] = {"Ab-d", "Ab_defghij", "Zoe9"};
	const Slot slots[] = {SLOT1, SLOT2, SLOT3};
	const HairStyle hairs[] = {HAIR_STYLE1, HAIR_STYLE2, HAIR_STYLE3};
	for (int i = 0; i < 3; ++i)
	{
		Login scene;
		g_Mode = MODE_WAIT_SELECTPC;
		std::string copy(names[i]);
		copy.push_back('\0');
		NEW_CHARACTER c = Character(&copy[0], i, 2 - i);
		c.bl_female = (i != 1);
		scene.Run(UI_NEW_CHARACTER, 0, 0, &c);
		const CLCreatePC expected = Created(names[i], slots[i], RACE_VAMPIRE, i != 1 ? FEMALE : MALE, hairs[2 - i], 22, 21, 23, 24);
		CHECK(Same({Send(expected), Mode(MODE_WAIT_CREATEPCOK)}));
		CHECK(scene.Sent() == Encoded({&expected}));
		CHECK(scene.wire.getPlayerStatus() == CPS_AFTER_SENDING_CL_CREATE_PC);
		CHECK(std::string(names[i]) == g_pUserInformation->Character[i].GetString());
		for (int other = 0; other < 3; ++other)
			if (other != i) CHECK(g_pUserInformation->Character[other].GetString() == NULL);
	}
}

TEST(UIMessages1NewCharacter, AFaceOrSlotPastTheThreeIsRefused)
{
	// Silently: the UI offers only the three. The face first, a read past
	// the three hair styles on the stack, which ASan sees.
	Login scene;
	g_Mode = MODE_WAIT_SELECTPC;
	struct Case { int slot; int face; };
	for (const Case& k : {Case{0, 3}, Case{0, -1}, Case{3, 0}, Case{-1, 0}})
	{
		char name[] = "Abcde";
		NEW_CHARACTER c = Character(name, k.slot, k.face);
		scene.Run(UI_NEW_CHARACTER, 0, 0, &c);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	for (int slot = 0; slot < 3; ++slot) CHECK(g_pUserInformation->Character[slot].GetString() == NULL);
}

TEST(UIMessages1NewCharacter, TheHairAndSkinColoursAreClamped)
{
	// To 0..MAX_COLORSET-1, in the UI's own record too. Recorded defect: the
	// coat and trouser colours are not clamped (and wrap in the packet).
	struct Case { int in; int out; };
	for (const Case& k : {Case{-5, 0}, Case{0, 0}, Case{MAX_COLORSET - 1, MAX_COLORSET - 1}, Case{MAX_COLORSET, MAX_COLORSET - 1}})
	{
		Login scene;
		g_Mode = MODE_WAIT_SELECTPC;
		char name[] = "Abcde";
		NEW_CHARACTER c = Character(name, 0, 0);
		c.hair_color = k.in;
		c.skin_color = k.in;
		c.coat_color = k.in;
		c.trouser_color = k.in;
		scene.Run(UI_NEW_CHARACTER, 0, 0, &c);
		CHECK_EQ(k.out, c.hair_color);
		CHECK_EQ(k.out, c.skin_color);
		CHECK_EQ(k.in, c.coat_color);
		CHECK_EQ(k.in, c.trouser_color);
		const CLCreatePC expected = Created("Abcde", SLOT1, RACE_VAMPIRE, FEMALE, HAIR_STYLE1, k.out, k.out, k.in, k.in);
		CHECK(scene.Sent() == Encoded({&expected}));
	}
}

//----------------------------------------------------------------------
// UI_RUN_NEWUSER_REGISTRATION (title screen), by the configuration's
// registration mode. The strings are the UI's malloc() blocks, which the
// handler free()s. Recorded defect: the UI replaces every non-empty one
// with a new[] block (g_Convert_DBCS_Ascii2SingleByte), which free() then
// meets - not run, the test hands it what it frees.
//----------------------------------------------------------------------
TEST(UIMessages1Registration, OnTheWebOrClosedItOnlySaysSo)
{
	Login scene;
	struct Case { ClientConfig::NUR_MODE mode; const char* text; bool quits; };
	const std::vector<Case> cases = {
#ifndef PLATFORM_WINDOWS
		// On Windows it also opens the web page in Explorer: not run there.
		{ClientConfig::NUR_HOMEPAGE, kHomepage, true},
#endif
		{ClientConfig::NUR_MESSAGE_HOMEPAGE, kHomepage, false},
		{ClientConfig::NUR_DENY, kDeny, false},
	};
	for (const Case& k : cases)
	{
		g_pClientConfig->NEW_USER_REGISTERATION_MODE = k.mode;
		for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_WAIT_SELECTPC})
		{
			g_Mode = mode;
			const std::vector<std::string> expected = k.quits ? std::vector<std::string>{Popup(k.text), Mode(MODE_QUIT)}
				: std::vector<std::string>{Popup(k.text)};
			scene.Clear();
			LOGIN login = {Malloced("Alpha1"), Malloced("secret1")};
			scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, &login);
			// The strings are freed unused.
			CHECK(login.sz_id == NULL && login.sz_password == NULL);
			CHECK(Same(expected));
			// Without the strings, the same.
			scene.Clear();
			scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, NULL);
			CHECK(Same(expected));
		}
	}
	CHECK(scene.Sent().empty());
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
}

TEST(UIMessages1Registration, NothingToRegisterOrNotOnTheTitle)
{
	Login scene;
	MessageUi ui;
	// No LOGIN, or a string missing: nothing, and the other string is
	// freed.
	g_Mode = MODE_MAINMENU;
	scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, NULL);
	char* pID = Malloced("Alpha1");
	LOGIN noPassword = {pID, NULL};
	scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, &noPassword);
	CHECK(noPassword.sz_id == NULL && noPassword.sz_password == NULL);
	CHECK(Freed(pID));
	char* pPassword = Malloced("secret1");
	LOGIN noID = {NULL, pPassword};
	scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, &noID);
	CHECK(noID.sz_id == NULL && noID.sz_password == NULL);
	CHECK(Freed(pPassword));
	// Not on the title: the strings are freed, nothing else.
	for (CLIENT_MODE mode : {MODE_WAIT_SELECTPC, MODE_GAME, MODE_WAIT_REGISTERPLAYEROK})
	{
		g_Mode = mode;
		LOGIN login = {Malloced("Alpha1"), Malloced("secret1")};
		scene.manager.Execute_UI_RUN_NEWUSER_REGISTRATION(0, 0, &login);
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
	}
	CHECK(Same({}));
	CHECK(ui.Shown().empty());
	CHECK(scene.Sent().empty());
}

TEST(UIMessages1Registration, AnInvalidIDOrPasswordIsRefused)
{
	Login scene;
	MessageUi ui;
	g_Mode = MODE_MAINMENU;
	struct Case { const char* id; const char* password; const char* why; };
	const Case cases[] = {
		{"Abc", "secret1", "The ID must be 4 to 10 characters."},
		{"Abcdefghijk", "secret1", "The ID must be 4 to 10 characters."},
		{"Ab-cd", "secret1", "The ID may only contain letters, digits and '_'."},
		{"Ab cd", "secret1", "The ID may only contain letters, digits and '_'."},
		{"Abcde", "short", "The password must be 6 to 10 characters."},
		{"Abcde", "elevenchars", "The password must be 6 to 10 characters."},
		{"Abcde", "sec'ret", "Do not use special characters in the password."},
		{"Abcde", "sec\\ret", "Do not use special characters in the password."},
		{"Darnit", "secret1", "Invalid ID."},
	};
	for (const Case& k : cases)
	{
		DeleteNew(g_msg_free_text);
		calls.clear();
		LOGIN login = {Malloced(k.id), Malloced(k.password)};
		scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, &login);
		CHECK(MessageUi::Squeezed(ui.Shown()) == MessageUi::Squeezed(k.why));
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
		CHECK(Same({}));
	}
	CHECK(scene.Sent().empty());
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
}

TEST(UIMessages1Registration, AValidIDAndPasswordReachTheSocket)
{
	// The bounds and the characters let through (a leading digit too,
	// unlike IsValidID): the socket is asked for, and refused here, so
	// nothing is sent and no message shows.
	Login scene;
	MessageUi ui;
	g_Mode = MODE_MAINMENU;
	socketAnswer = FALSE;
	const char* const ids[] = {"Abcd", "Abcdefghij", "1_Z9z", "_abc"};
	const char* const passwords[] = {"secret", "secret1234", "!@#$%^&*()", "pass word"};
	for (int i = 0; i < 4; ++i)
	{
		calls.clear();
		LOGIN login = {Malloced(ids[i]), Malloced(passwords[i])};
		scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, &login);
		CHECK(Same({"init socket"}));
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
	}
	CHECK(ui.Shown().empty());
	CHECK(scene.Sent().empty());
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
}

TEST(UIMessages1Registration, AValidRegistrationIsSent)
{
	// Sleeps half a second (Sleep(500) after the key).
	Login scene;
	MessageUi ui;
	g_Mode = MODE_MAINMENU;
	LOGIN login = {Malloced("Alpha1"), Malloced("secret1")};
	std::srand(4242);
	const int encryptKey = std::rand();
	const int hashKey = std::rand();
	std::srand(4242);
	scene.Run(UI_RUN_NEWUSER_REGISTRATION, 0, 0, &login);
	CLVersionCheck version;
	version.setVersion(1234);
	CGConnectSetKey key;
	key.setEncryptKey(WORD(encryptKey));
	key.setHashKey(WORD(hashKey));
	CLRegisterPlayer registration;
	registration.setID("Alpha1");
	registration.setPassword("secret1");
	registration.setName("Alpha1");
	registration.setSex(MALE);
	registration.setSSN("000000-0000000");
	registration.setTelephone("NULL");
	registration.setCellular("NULL");
	registration.setZipCode("NULL");
	registration.setAddress("NULL");
	registration.setNation(KOREA);
	registration.setEmail("NULL");
	registration.setHomepage("NULL");
	registration.setProfile("NULL");
	registration.setPublic(false);
	// The version and the key go first, flushed alone, and the key is
	// handed to the client's own dispatcher.
	CHECK(Same(Calls({"init socket", Send(version), Send(key), "flush|" + N(static_cast<long long>(Encoded(Sends({&version, &key}, &version)).size())),
		"dispatch|" + N(KeyID()) + "|socket", Send(registration), Mode(MODE_WAIT_REGISTERPLAYEROK)}, Send(version))));
	CHECK(scene.Sent() == Encoded(Sends({&version, &key, &registration}, &version)));
	CHECK(scene.wire.getPlayerStatus() == CPS_AFTER_SENDING_CL_REGISTER_PLAYER);
	CHECK(std::string("Alpha1") == g_pUserInformation->UserID.GetString());
	CHECK(login.sz_id == NULL && login.sz_password == NULL);
	CHECK(ui.Shown().empty());
}

//----------------------------------------------------------------------
// UI_CHECK_EXIST_ID (nothing sends it today): a valid ID (IsValidID: 4 to
// 10, no '_') is asked of the login server; else a popup. The name is the
// UI's new[] block, deleted here.
//----------------------------------------------------------------------
TEST(UIMessages1CheckExistID, AnInvalidIDPopsUp)
{
	Login scene;
	// Recorded defect: the handler's own 4-to-10 check (with its
	// untranslated message) is dead, IsValidID refuses those first.
	for (const char* id : {"Abc", "Abcdefghijk", "Ab_cd", "1abcd", "Ab cd", ""})
	{
		for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_GAME})
		{
			g_Mode = mode;
			scene.Clear();
			scene.manager.Execute_UI_CHECK_EXIST_ID(0, 0, NewArray(id));
			CHECK(Same({Popup(kInvalidID)}));
		}
	}
	CHECK(scene.Sent().empty());
}

TEST(UIMessages1CheckExistID, AValidIDIsAskedOfTheServer)
{
	for (const char* id : {"Abcd", "Abcdefghij"})
	{
		Login scene;
		g_Mode = MODE_WAIT_SELECTPC;	// any mode
		scene.Run(UI_CHECK_EXIST_ID, 0, 0, NewArray(id));
		CLQueryPlayerID expected;
		expected.setPlayerID(id);
		CHECK(Same({"init socket", Send(expected)}));
		CHECK(scene.Sent() == Encoded({&expected}));
		CHECK(scene.wire.getPlayerStatus() == CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID);
	}
}

TEST(UIMessages1CheckExistID, NoSocketNothingSent)
{
	Login scene;
	socketAnswer = FALSE;
	const PlayerStatus status = scene.wire.getPlayerStatus();
	char* pName = NewArray("Abcde");
	scene.Run(UI_CHECK_EXIST_ID, 0, 0, pName);
	CHECK(Same({"init socket"}));
	CHECK(scene.Sent().empty());
	CHECK(scene.wire.getPlayerStatus() == status);
	// The name is deleted all the same.
	CHECK(Freed(pName));
}

//----------------------------------------------------------------------
// UI_DELETE_CHARACTER (character screen): the slot's character, slot 0, 1
// or 2 as SLOT1, SLOT2, SLOT3, with a placeholder SSN.
//----------------------------------------------------------------------
TEST(UIMessages1DeleteCharacter, TheSlotsCharacterIsDeleted)
{
	const Slot slots[] = {SLOT1, SLOT2, SLOT3};
	const char* const names[] = {"Alpha", "Bravo", "Charlie"};
	for (int slot = 0; slot < 3; ++slot)
	{
		Login scene;
		g_Mode = MODE_WAIT_SELECTPC;
		for (int i = 0; i < 3; ++i) g_pUserInformation->Character[i] = names[i];
		g_pUserInformation->Slot = 9;
		DELETE_CHARACTER request = {slot};
		scene.Run(UI_DELETE_CHARACTER, 0, 0, &request);
		CLDeletePC expected;
		expected.setName(names[slot]);
		expected.setSlot(slots[slot]);
		expected.setSSN("1");
		CHECK(Same({Send(expected), Mode(MODE_WAIT_DELETEPCOK)}));
		CHECK(scene.Sent() == Encoded({&expected}));
		CHECK(scene.wire.getPlayerStatus() == CPS_AFTER_SENDING_CL_DELETE_PC);
		CHECK_EQ(slot, g_pUserInformation->Slot);
		// The name stays until the server answers.
		CHECK(std::string(names[slot]) == g_pUserInformation->Character[slot].GetString());
	}
}

TEST(UIMessages1DeleteCharacter, OnlyOnTheCharacterScreen)
{
	Login scene;
	g_pUserInformation->Character[0] = "Alpha";
	g_pUserInformation->Slot = 9;
	DELETE_CHARACTER request = {0};
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_GAME, MODE_WAIT_DELETEPCOK})
	{
		g_Mode = mode;
		scene.manager.Execute_UI_DELETE_CHARACTER(0, 0, &request);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	CHECK_EQ(9, g_pUserInformation->Slot);
}

TEST(UIMessages1DeleteCharacter, AnEmptySlotOrOnePastTheThreeIsRefused)
{
	// The empty slot first: its name is NULL (an empty MString's always
	// is), which the packet's setName would read.
	Login scene;
	g_Mode = MODE_WAIT_SELECTPC;
	g_pUserInformation->Character[0] = "Alpha";
	g_pUserInformation->Character[2] = "Charlie";
	g_pUserInformation->Slot = 9;
	for (int slot : {1, 3, -1})
	{
		DELETE_CHARACTER request = {slot};
		scene.Run(UI_DELETE_CHARACTER, 0, 0, &request);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	CHECK_EQ(9, g_pUserInformation->Slot);
}

//----------------------------------------------------------------------
// UI_TERMINATION: quits from the title only.
//----------------------------------------------------------------------
TEST(UIMessages1Termination, OnlyTheTitleQuits)
{
	Login scene;
	g_Mode = MODE_MAINMENU;
	scene.Run(UI_TERMINATION);
	CHECK(Same({Mode(MODE_QUIT)}));
	calls.clear();
	for (CLIENT_MODE mode : {MODE_WAIT_SELECTPC, MODE_GAME, MODE_WAIT_LOGINOK, MODE_QUIT})
	{
		g_Mode = mode;
		scene.manager.Execute_UI_TERMINATION(0, 0, NULL);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
}

//----------------------------------------------------------------------
// UI_LOGIN (title screen): the strings are the UI's new[] blocks, always
// deleted. A password without a quote or backslash connects, sends the
// version, the key and the login, and waits for the server.
//----------------------------------------------------------------------
namespace {
// The options file and the key bindings it saves.
struct SavedOptions
{
	UserOption* oldOption = g_pUserOption;
	KeyAccelerator* oldKeys = g_pKeyAccelerator;
	KeyAccelerator keys;
	UserOption option;
	SavedOptions()
	{
		std::remove(kOptionFile);
		keys.Init(MAX_ACCELERATOR);
		g_pKeyAccelerator = &keys;
		g_pUserOption = &option;
	}
	~SavedOptions()
	{
		std::remove(kOptionFile);
		g_pKeyAccelerator = oldKeys;
		g_pUserOption = oldOption;
	}
};
} // namespace

TEST(UIMessages1Login, NotOnTheTitleTheStringsGoAndTheOptionsAreSaved)
{
	// Recorded defect: the sequence numbers are reseeded and the options
	// written before the mode check, so a refused login does both.
	Login scene;
	SavedOptions options;
	for (CLIENT_MODE mode : {MODE_WAIT_SELECTPC, MODE_GAME, MODE_WAIT_LOGINOK})
	{
		g_Mode = mode;
		std::remove(kOptionFile);
		g_dwSeqNumL = 5000;
		g_dwSeqNumG = 77;
		std::srand(99);
		const DWORD seq = DWORD(std::rand() % 1024);
		std::srand(99);
		LOGIN login = {NewArray("Alpha1"), NewArray("secret1")};
		scene.manager.Execute_UI_LOGIN(0, 0, &login);
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
		CHECK_EQ(seq, g_dwSeqNumL);
		CHECK_EQ(0u, g_dwSeqNumG);
		CHECK(FileExists(kOptionFile));
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
}

TEST(UIMessages1Login, AMissingStringOrABadPasswordStopsIt)
{
	Login scene;
	g_Mode = MODE_MAINMENU;
	LOGIN noPassword = {NewArray("Alpha1"), NULL};
	scene.Run(UI_LOGIN, 0, 0, &noPassword);
	CHECK(noPassword.sz_id == NULL);
	LOGIN noID = {NULL, NewArray("secret1")};
	scene.Run(UI_LOGIN, 0, 0, &noID);
	CHECK(noID.sz_password == NULL);
	CHECK(Same({"addon|0", "addon|0"}));
	for (const char* password : {"sec'ret", "sec\\ret"})
	{
		scene.Clear();
		LOGIN login = {NewArray("Alpha1"), NewArray(password)};
		scene.Run(UI_LOGIN, 0, 0, &login);
		CHECK(Same({"addon|0", Popup(kPasswordSpecial)}));
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
	}
	CHECK(scene.Sent().empty());
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
}

TEST(UIMessages1Login, NoSocketNothingSent)
{
	Login scene;
	g_Mode = MODE_MAINMENU;
	socketAnswer = FALSE;
	const PlayerStatus status = scene.wire.getPlayerStatus();
	// Only the lengths the packet carries are checked (a refused socket
	// shows the ID and password are past the checks).
	for (const std::string& id : {std::string("Alpha1"), std::string(30, 'a'), std::string("1 $")})
	{
		calls.clear();
		LOGIN login = {NewArray(id.c_str()), NewArray(std::string(20, 'p').c_str())};
		scene.Run(UI_LOGIN, 0, 0, &login);
		CHECK(Same({"addon|0", "init socket"}));
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
	}
	CHECK(scene.Sent().empty());
	CHECK(scene.wire.getPlayerStatus() == status);
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
}

TEST(UIMessages1Login, NoSocketAfterTheConnectionNothingSent)
{
	// InitSocket answers TRUE but left no socket: as if it had failed.
	Login scene;
	g_Mode = MODE_MAINMENU;
	g_pSocket = NULL;
	LOGIN login = {NewArray("Alpha1"), NewArray("secret1")};
	scene.Run(UI_LOGIN, 0, 0, &login);
	CHECK(Same({"addon|0", "init socket"}));
	CHECK(login.sz_id == NULL && login.sz_password == NULL);
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
	g_pSocket = &scene.wire;
	CHECK(scene.Sent().empty());
}

TEST(UIMessages1Login, ALoginIsSentAndWaitsForTheServer)
{
	// Sleeps half a second.
	Login scene;
	g_Mode = MODE_MAINMENU;
	g_dwSeqNumG = 77;
	LOGIN login = {NewArray("Alpha1"), NewArray("secret1")};
	std::srand(31337);
	const DWORD seq = DWORD(std::rand() % 1024);
	const int encryptKey = std::rand();
	const int hashKey = std::rand();
	std::srand(31337);
	scene.Run(UI_LOGIN, 0, 0, &login);
	CLVersionCheck version;
	version.setVersion(1234);
	CGConnectSetKey key;
	key.setEncryptKey(WORD(encryptKey));
	key.setHashKey(WORD(hashKey));
	CLLogin expected;
	expected.setID("Alpha1");
	expected.setPassword("secret1");
	expected.setMacAddress(g_macAddress);
	CHECK(Same(Calls({"addon|0", "init socket", Send(version), Send(key), "flush|" + N(static_cast<long long>(Encoded(Sends({&version, &key}, &version)).size())),
		"dispatch|" + N(KeyID()) + "|socket", Send(expected), Mode(MODE_WAIT_LOGINOK)}, Send(version))));
	CHECK(scene.Sent() == Encoded(Sends({&version, &key, &expected}, &version)));
	CHECK(scene.wire.getPlayerStatus() == CPS_AFTER_SENDING_CL_LOGIN);
	CHECK(std::string("Alpha1") == g_pUserInformation->UserID.GetString());
	CHECK(login.sz_id == NULL && login.sz_password == NULL);
	CHECK_EQ(seq, g_dwSeqNumL);
	CHECK_EQ(0u, g_dwSeqNumG);
}

TEST(UIMessages1Login, ANetmarbleAutoLoginKeepsTheMode)
{
	// Sleeps half a second. Netmarble's layout (no password, a long ID);
	// an automatic login enters no mode (the login mode it sets is not on
	// the wire).
	Login scene;
	g_Mode = MODE_MAINMENU;
	g_pUserInformation->IsNetmarble = true;
	g_pUserInformation->IsAutoLogIn = true;
	const std::string longID(40, 'n');
	LOGIN login = {NewArray(longID.c_str()), NewArray("secret1")};
	std::srand(7);
	std::rand();
	const int encryptKey = std::rand();
	const int hashKey = std::rand();
	std::srand(7);
	scene.Run(UI_LOGIN, 0, 0, &login);
	CLVersionCheck version;
	version.setVersion(1234);
	CGConnectSetKey key;
	key.setEncryptKey(WORD(encryptKey));
	key.setHashKey(WORD(hashKey));
	CLLogin expected;
	expected.setNetmarble(true);
	expected.setID(longID);
	expected.setPassword("secret1");
	expected.setMacAddress(g_macAddress);
	CHECK(Same(Calls({"addon|0", "init socket", Send(version), Send(key), "flush|" + N(static_cast<long long>(Encoded(Sends({&version, &key}, &version)).size())),
		"dispatch|" + N(KeyID()) + "|socket", Send(expected)}, Send(version))));
	CHECK(scene.Sent() == Encoded(Sends({&version, &key, &expected}, &version)));
	CHECK(scene.wire.getPlayerStatus() == CPS_AFTER_SENDING_CL_LOGIN);
	CHECK(longID == g_pUserInformation->UserID.GetString());
}

TEST(UIMessages1Login, AnIDOrPasswordThePacketCannotCarryIsRefused)
{
	// Before anything is sent: CLLogin::write throws for an empty ID or
	// one past 30 (2048 in Netmarble's layout) and, outside that layout,
	// for an empty password or one past 20.
	Login scene;
	g_Mode = MODE_MAINMENU;
	const PlayerStatus status = scene.wire.getPlayerStatus();
	struct Case { bool netmarble; std::string id; std::string password; const char* why; };
	const Case cases[] = {
		{false, "", "secret1", kInvalidID},
		{false, std::string(31, 'a'), "secret1", kInvalidID},
		{false, "Alpha1", "", kPasswordSpecial},
		{false, "Alpha1", std::string(21, 'p'), kPasswordSpecial},
		{true, "", "secret1", kInvalidID},
		{true, std::string(2049, 'n'), "secret1", kInvalidID},
	};
	for (const Case& k : cases)
	{
		g_pUserInformation->IsNetmarble = k.netmarble;
		scene.Clear();
		LOGIN login = {NewArray(k.id.c_str()), NewArray(k.password.c_str())};
		scene.Run(UI_LOGIN, 0, 0, &login);
		CHECK(Same({"addon|0", Popup(k.why)}));
		CHECK(login.sz_id == NULL && login.sz_password == NULL);
	}
	CHECK(scene.Sent().empty());
	CHECK(scene.wire.getPlayerStatus() == status);
	CHECK(g_pUserInformation->UserID.GetString() == NULL);
}

//----------------------------------------------------------------------
// UI_CHARACTER_MANAGER_FINISHED (character screen): back to the title on
// Netmarble, else to the server list. Nothing is sent (the logout is
// commented out). UI_SetServerList draws nothing without the server
// information (and nothing without a UI anyway).
//----------------------------------------------------------------------
TEST(UIMessages1CharacterManager, BackToTheServersOrTheTitle)
{
	Login scene;
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_CHARACTER_MANAGER_FINISHED);
	CHECK(Same({Mode(MODE_WAIT_SELECT_SERVER)}));
	calls.clear();
	g_pUserInformation->IsNetmarble = true;
	scene.Run(UI_CHARACTER_MANAGER_FINISHED);
	CHECK(Same({Mode(MODE_MAINMENU)}));
	calls.clear();
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_GAME, MODE_WAIT_SELECT_SERVER})
	{
		g_Mode = mode;
		scene.manager.Execute_UI_CHARACTER_MANAGER_FINISHED(0, 0, NULL);
	}
	g_pUserInformation->IsNetmarble = false;
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_GAME, MODE_WAIT_SELECT_SERVER})
	{
		g_Mode = mode;
		scene.manager.Execute_UI_CHARACTER_MANAGER_FINISHED(0, 0, NULL);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
}

//----------------------------------------------------------------------
// UI_CONNECT (character screen, left = the slot): the slot's character is
// chosen and the game server awaited.
//----------------------------------------------------------------------
namespace {
struct Connect : Login
{
	Connect()
	{
		g_Mode = MODE_WAIT_SELECTPC;
		const char* const names[] = {"Alpha", "GMBravo", "Charlie"};
		const Race races[] = {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS};
		for (int i = 0; i < 3; ++i)
		{
			g_pUserInformation->Character[i] = names[i];
			g_pUserInformation->Race[i] = races[i];
			g_pUserInformation->FaceStyleSlot[i] = 10 + i;
		}
		g_pUserInformation->CharacterID = "Old";
		g_pUserInformation->FaceStyle = 5;
		g_pUserInformation->IsMaster = 7;
		// Without the dialogs: no "connecting" box or drawing.
		g_pUIDialog = NULL;
		g_pItemTable->InitClass(ITEM_CLASS_ETC, 2);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_ETC, 0).SetGrid(1, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_ETC, 1).SetGrid(1, 1);
	}
	// An ETC item of the type in the inventory.
	void Etc(int type)
	{
		MItem* pItem = new MItemETC;
		pItem->SetItemType(TYPE_ITEMTYPE(type));
		pItem->SetID(900 + type);
		CHECK(inventory.AddItem(pItem));
	}
};
} // namespace

TEST(UIMessages1Connect, TheSlotsCharacterIsChosen)
{
	const PCType types[] = {PC_SLAYER, PC_VAMPIRE, PC_OUSTERS};
	const char* const names[] = {"Alpha", "GMBravo", "Charlie"};
	for (int slot = 0; slot < 3; ++slot)
	{
		Connect scene;
		scene.Run(UI_CONNECT, slot);
		CLSelectPC expected;
		expected.setPCName(names[slot]);
		expected.setPCType(types[slot]);
		// Sent at once, then the slot remembered.
		CHECK(Same({"addon|0", Send(expected), "flush|" + N(static_cast<long long>(Encoded({&expected}).size())), "last|" + N(slot),
			Mode(MODE_WAIT_RECONNECT)}));
		CHECK(scene.Sent() == Encoded({&expected}));
		CHECK(scene.wire.getPlayerStatus() == CPS_AFTER_SENDING_CL_SELECT_PC);
		CHECK(scene.wire.getPCType() == types[slot]);
		CHECK(scene.wire.getPCName() == names[slot]);
		CHECK(std::string(names[slot]) == g_pUserInformation->CharacterID.GetString());
		CHECK_EQ(10 + slot, g_pUserInformation->FaceStyle);
		// A master: the name starts with the master-name string.
		CHECK_EQ(slot == 1 ? TRUE : FALSE, g_pUserInformation->IsMaster);
	}
}

TEST(UIMessages1Connect, AnEmptyMasterNameMakesNoMaster)
{
	// Its MString holds NULL; a prefix of nothing matches no name.
	Connect scene;
	g_pGameStringTable->Set(UI_STRING_MESSAGE_MASTER_NAME, "");
	for (int slot = 0; slot < 3; ++slot)
	{
		scene.Run(UI_CONNECT, slot);
		CHECK_EQ(FALSE, g_pUserInformation->IsMaster);
	}
}

TEST(UIMessages1Connect, AnEtcItemOfTypeZeroMakesAMaster)
{
	// Recorded defect: the inventory checked is whatever g_pInventory holds
	// at character select (the last game's, if any).
	Connect scene;
	scene.Etc(1);
	scene.Run(UI_CONNECT, 0);
	CHECK_EQ(FALSE, g_pUserInformation->IsMaster);
	scene.Etc(0);
	scene.Run(UI_CONNECT, 0);
	CHECK_EQ(TRUE, g_pUserInformation->IsMaster);
}

TEST(UIMessages1Connect, AnEmptySlotOrOnePastTheThreeIsRefused)
{
	// Before the music stops or the box shows; the empty slot first (its
	// name is NULL, which the packet's setPCName would read).
	Connect scene;
	g_pUserInformation->Character[1].Release();
	for (intptr_t slot : {intptr_t(1), intptr_t(3), intptr_t(-1)})
		scene.Run(UI_CONNECT, slot);
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	CHECK(std::string("Old") == g_pUserInformation->CharacterID.GetString());
	CHECK_EQ(5, g_pUserInformation->FaceStyle);
	CHECK_EQ(7, g_pUserInformation->IsMaster);
}

TEST(UIMessages1Connect, OnlyOnTheCharacterScreen)
{
	Connect scene;
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_GAME, MODE_WAIT_RECONNECT})
	{
		g_Mode = mode;
		scene.manager.Execute_UI_CONNECT(1, 0, NULL);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
	CHECK(std::string("Old") == g_pUserInformation->CharacterID.GetString());
	CHECK_EQ(5, g_pUserInformation->FaceStyle);
	CHECK_EQ(7, g_pUserInformation->IsMaster);
}

TEST(UIMessages1Connect, TheMidiMusicStops)
{
	Connect scene;
	MMusic oldMusic = g_Music;
	UserOption option;
	g_pUserOption = &option;
	g_Music = MMusic();
	g_Music.Init(NULL);
	// The stream (closed only with the audio up): the MIDI plays on.
	option.PlayWaveMusic = TRUE;
	g_Music.m_bLoad = g_Music.m_bPlay = true;
	scene.Run(UI_CONNECT, 0);
	CHECK(g_Music.m_bPlay);
	// The MIDI player stops.
	option.PlayWaveMusic = FALSE;
	scene.Run(UI_CONNECT, 0);
	CHECK(!g_Music.m_bPlay && !g_Music.m_bLoad);
	g_Music = oldMusic;
	g_pUserOption = NULL;
}

TEST(UIMessages1Connect, WithTheDialogsTheConnectingScreenIsDrawn)
{
	// A box says so, the UI draws, the last frame is copied onto the back
	// buffer over the game's rectangle, the screen flips (nothing without a
	// renderer), and the box closes.
	Connect scene;
	CSpriteSurface* oldBack = g_pBack;
	CSpriteSurface* oldLast = g_pLast;
	{
		// The UI's base and windows, for gC_vs_ui.Show.
		MessageUi ui;
		g_pUserOption = NULL;
		CSpriteSurface back;
		CSpriteSurface last;
		back.Init(6, 5);
		last.Init(6, 5);
		back.FillSurface(0x1111);
		last.FillSurface(0x2222);
		g_pBack = &back;
		g_pLast = &last;
		g_GameRect = {0, 0, 4, 3};
		g_pUIDialog = &scene.dialog;
		scene.Run(UI_CONNECT, 2);
		CLSelectPC expected;
		expected.setPCName("Charlie");
		expected.setPCType(PC_OUSTERS);
		CHECK(Same({Popup(kConnecting, 0), "close popup", "addon|0", Send(expected),
			"flush|" + N(static_cast<long long>(Encoded({&expected}).size())), "last|2", Mode(MODE_WAIT_RECONNECT)}));
		auto Pixel = [](CSpriteSurface& surface, int x, int y) {
			spritectl_surface_info_t info{};
			if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0) return WORD(0xDEAD);
			const WORD value = reinterpret_cast<const WORD*>(static_cast<const unsigned char*>(info.pixels) + y * info.pitch)[x];
			spritectl_unlock_surface(surface.GetBackendSurface());
			return value;
		};
		CHECK_EQ(0x2222, Pixel(back, 0, 0));
		CHECK_EQ(0x2222, Pixel(back, 3, 2));
		CHECK_EQ(0x1111, Pixel(back, 4, 0));
		CHECK_EQ(0x1111, Pixel(back, 0, 3));
		// The input lock the box set is gone with it.
		CHECK_EQ(0u, UIDialog::IsLockInput() & FLAG_UIDIALOG_LOCK_MESSAGE);
	}
	g_pLast = oldLast;
	g_pBack = oldBack;
}

//----------------------------------------------------------------------
// The handler whose only effect is in the UI: UI_INFO_CLOSE closes the
// character's info window on the character screen and in the game. With
// no UI it touches nothing.
//----------------------------------------------------------------------
TEST(UIMessages1WithoutUI, TheInfoCloseIsHarmless)
{
	Login scene;
	for (CLIENT_MODE mode : {MODE_WAIT_SELECTPC, MODE_GAME, MODE_MAINMENU})
	{
		g_Mode = mode;
		scene.manager.Execute_UI_INFO_CLOSE(1, 2, NULL);
	}
	CHECK(Same({}));
	CHECK(scene.Sent().empty());
}

//----------------------------------------------------------------------
// TruncateUtf8ToBytes (static; reached through a party chat line): a line
// over 128 bytes is cut on a character boundary.
//----------------------------------------------------------------------
TEST(UIMessages1Truncate, ALongLineIsCutOnACharacter)
{
	Login scene;
	MParty party;
	g_pParty = &party;
	g_Mode = MODE_GAME;
	struct Case { std::string line; std::string sent; };
	const std::string a127(127, 'a');
	const std::string a126(126, 'a');
	const std::vector<Case> cases = {
		{std::string(128, 'a'), std::string(128, 'a')},
		{std::string(129, 'a'), std::string(128, 'a')},
		// A two-byte character across the limit goes whole.
		{a127 + "\xC3\xA9", a127},
		{a127 + "\xC3\xA9" + "bb", a127},
		// A three-byte one.
		{a126 + "\xE2\x82\xAC", a126},
		{a126 + "\xE2\x82\xAC" "c", a126},
		// One that ends at the limit stays.
		{a126 + "\xC3\xA9" + "d", a126 + "\xC3\xA9"},
		// Recorded defect: with nothing but continuation bytes after the
		// first, it cuts at the limit, inside a (malformed) character.
		{"a" + std::string(200, '\x80'), "a" + std::string(127, '\x80')},
	};
	for (const Case& k : cases)
	{
		Wire wire;
		g_pSocket = &wire;
		std::string line = k.line;
		scene.manager.Execute_UI_CHAT_RETURN(CLD_PARTY, 0x123456, &line[0]);
		CGPartySay expected;
		expected.setColor(0x123456);
		expected.setMessage(k.sent);
		CHECK(wire.Sent() == Encoded({&expected}));
		// The line itself is not changed.
		CHECK(line == k.line);
	}
	g_pSocket = &scene.wire;
}
