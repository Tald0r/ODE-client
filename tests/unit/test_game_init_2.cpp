#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "PacketDispatcherTestAccess.h"
#include "GameInit.h"
#include "GameMain.h"
#include "ClientMode.h"
#include "GameSocket.h"
#include "GameMessages.h"
#include "InputState.h"
#include "FrameClock.h"
#include "Exception.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "ServerSocket.h"
#include "SocketEncryptOutputStream.h"
#include "ClientPlayer.h"
#include "PacketFactory.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "Properties.h"
#include "ServerInfoFileParser.h"
#include "RequestServerPlayer.h"
#include "RequestServerPlayerManager.h"
#include "ClientCommunicationManager.h"
#include "RequestUserManager.h"
#include "RequestFileManager.h"
#include "WireHost.h"
#include "WebSocketTransport.h"
#include "CWaitUIUpdate.h"
#include "CWaitPacketUpdate.h"
#include "CGameUpdate.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "MChatManager.h"
#include "../basic/CMessageArray.h"
#include "MEventQueue.h"
#include "MEventManager.h"
#include "MWeather.h"
#include "MParty.h"
#include "MWarManager.h"
#include "MMusic.h"
#include "UserOption.h"
#include "BasicException.h"
#include "CrtCompat.h"
#include "UiRuntime.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_GlobalResource.h"
#include "VS_UI_GameCommon.h"
#include "SkinManager.h"
#include "pi_core.h"
#include "u_window.h"
#include "Ci.h"
#include "CSpriteSurface.h"
#include "CSprite565.h"
#include "SpriteLibBackend.h"
#include "DXLib/CDirectDraw.h"
#include "DXLib/CDirectSound.h"
#include "DXLib/COGGSTREAM.h"
#include "DXLib/CSoundPartManager.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "DXLib/DXInputHost.h"
#include "Cpackets/CGReady.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MTopView.h"
#include "MScreenEffectManager.h"
#include "MZoneSoundManager.h"
#include "MNPCTable.h"
#include "MStringList.h"
#include "TalkBox.h"
#include "MShopTemplateTable.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MInventory.h"
#include "MNPCScriptTable.h"
#include "CServerInformation.h"
#include "MMonsterKillQuestInfo.h"
#include "AcceleratorManager.h"
#include "ModifyStatusManager.h"
#include "MPriceManager.h"
#include "MItem.h"
// The hosts InitGameObject installs.
#include "MEffectTarget.h"
#include "MEffect.h"
#include "MHelicopterManager.h"
#include "MGuidanceEffect.h"
#include "MParabolaEffect.h"
#include "MAttachEffect.h"
#include "MStopInventoryEffectGenerator.h"
#include "MStopZoneEffectGenerator.h"
#include "MStopZoneCrossEffectGenerator.h"
#include "MStopZoneXEffectGenerator.h"
#include "MStopZoneRhombusEffectGenerator.h"
#include "MStopZoneEmptyCrossEffectGenerator.h"
#include "MStopZoneEmptyRectEffectGenerator.h"
#include "MStopZoneRectEffectGenerator.h"
#include "MStopZoneWallEffectGenerator.h"
#include "MStopZoneMultipleEffectGenerator.h"
#include "MStopZoneRandomEffectGenerator.h"
#include "MStopZoneSelectableEffectGenerator.h"
#include "MSpreadOutEffectGenerator.h"
#include "MAroundZoneEffectGenerator.h"
#include "MFollowPathEffectGenerator.h"
#include "MMeteorDropEffectGenerator.h"
#include "MAttackCreatureParabolaEffectGenerator.h"
#include "MAttackCreatureEffectGenerator.h"
#include "MAttackCreatureHomingEffectGenerator.h"
#include "MRippleZoneWideEffectGenerator.h"
#include "MBloodyBreakerEffectGenerator.h"
#include "MBloodyWallEffectGenerator.h"
#include "MBloodyWaveEffectGenerator.h"
#include "MRippleZoneEffectGenerator.h"
#include "MRippleZonePixelEffectGenerator.h"
#include "MAttachZoneEffectGenerator.h"
#include "MAttachZoneAroundEffectGenerator.h"
#include "MAttachZoneSelectableEffectGenerator.h"
#include "MAttackZoneRectEffectGenerator.h"
#include "MSkipEffectGenerator.h"
#include "RankBonusHandlerHost.h"
#include "RegenZoneStatusHost.h"
#include "BonusSkillHost.h"
#include "SkillDowngradeHost.h"
#include "GuildPresentationHost.h"
#include "CommunicationPresentationHost.h"
#include "QuestWarPresentationHost.h"
#include "NoticePresentationHost.h"
#include "StatusPresentationHost.h"
#include "GameOptionsHost.h"
#include "PlayerPresentationHost.h"
#include "NPCPresentationHost.h"
#include "ZoneLoadingHost.h"
#include "TradePresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "DialogPresentationHost.h"
#include "LoginPresentationHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "VsUiPresentationHost.h"
#include "LoginListHost.h"
#include "MStopZoneEmptyHorizontalWallEffectGenerator.h"
#include "MStopZoneEmptyVerticalEffectGenerator.h"
#include "MFallingEffectGenerator.h"
#include "MAttackZoneEffectGenerator.h"
#include "MFirePiecingEffectGenerator.h"
#include "MPlayingWithFireEffectGenerator.h"
#include "MAttackZoneBombEffectGenerator.h"
#include "MAttackZoneParabolaEffectGenerator.h"
#include "MMultipleFallingEffectGenerator.h"
#include "MRisingEffectGenerator.h"
#include "UIFunction.h"
#include "UIMessageManager.h"
#include "CUsingColorSetTable.h"
#include "ClientConfig.h"
#include "ClientDef.h"
#include "ExperienceTable.h"
#include "FameInfo.h"
#include "GameInitInfo.h"
#include "MActionInfoTable.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MEffectGeneratorTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MGameStringTable.h"
#include "MGameTime.h"
#include "MGuildInfoMapper.h"
#include "MGuildMarkManager.h"
#include "MHelpDisplayer.h"
#include "MHelpStringTable.h"
#include "MInteractionObjectTable.h"
#include "MItemOptionTable.h"
#include "MItemTable.h"
#include "MJusticeAttackManager.h"
#include "MLevelNameTable.h"
#include "MMoneyManager.h"
#include "MMusicTable.h"
#include "MObjectSelector.h"
#include "MSkillManager.h"
#include "MSoundTable.h"
#include "MStorage.h"
#include "MTimeItemManager.h"
#include "MTradeManager.h"
#include "MZoneTable.h"
#include "MonsterNameTable.h"
#include "PCConfigTable.h"
#include "ProfileManager.h"
#include "Profiler.h"
#include "RankBonusTable.h"
#include "ShrineInfoManager.h"
#include "SystemAvailabilities.h"
#include "TempInformation.h"
#include "UserInformation.h"
#include "ZoneFlags.h"
#include "KeyAccelerator.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

extern int g_Dimension;			// ClientMode.cpp's, declared in Client_PCH.h

// GameInit.cpp (task 4.264 moved it into gameclient), group 2: the
// connection to the login server and the teardown.
// - InitSocket releases any connection first. It reads the login server's
//   settings: a foreign client's from FILE_INFO_SERVERINFO (a
//   ServerInfoFileParser, read later, block g_Dimension), the Korean one's
//   from FILE_INFO_GAMECLIENT, or FILE_INFO_GAMECLIENT2 in the second
//   dimension (a Properties, loaded at once). It makes and initialises the
//   packet factories and validator and binds the packet handlers, then
//   picks the endpoint: DARKEDEN_LOGIN_HOST and DARKEDEN_LOGIN_PORT first,
//   then the launcher's g_FutecIP:g_FutecPort, then the settings, whose
//   MaxLoginServerAddress addresses are tried in turn by a count of failed
//   tries kept from call to call. A name that does not start with a digit
//   is resolved. It connects, makes the socket non-blocking with lingering
//   off, makes the ClientPlayer, and makes the peer managers afresh
//   (the request service on Windows only). Any failure goes to the main
//   menu, disconnected, and answers FALSE; a failure in picking, resolving
//   or connecting also moves the count on.
// - ReleaseSocket disconnects the connection (what is queued is sent
//   first) and frees it, then the settings, the factories, the validator
//   and the four peer managers, each reset; with nothing there it does
//   nothing.
// - ReleaseAllObjects ends the game and frees everything the start-up made,
//   in its order, then resets 80 of the 84 hosts the start-up installs.
//
// The main menu and the disconnect a failed InitSocket goes to are the real
// GameMain's (test_game_main_2.cpp pins them), over a recording
// GameMainHost whose ReleaseSocket names what is held, then releases it as
// the game's does; the UI is the real one, its title on the test's skins;
// the game is inactive, so the disconnect box does not wait for a key. The
// connections are real loopback ones, to a listener of the test's.
// Objects whose class has a virtual destructor are counted by subclasses
// that record their destruction; the others are seen only being reset.
//
// Not run:
// - The request service InitSocket starts on Windows when
//   MAX_REQUEST_SERVICE is above 0 (it listens on a port and starts a
//   thread): every test sets it to 0, so all platforms make none.
// - A resolved name on Windows before anything started Winsock: the test
//   binary starts it first (EnsureSocketsInitialised), and it stays up for
//   the process (the fourth defect below).
// - A connection error that is not a ConnectException (inet_addr's
//   INADDR_NONE for a name starting with a digit, which each platform
//   refuses differently): it would end the attempt without moving the
//   count.
// - Whether InitSocket binds the packet handlers: the registry fills the
//   table only the first time it runs in the process, which may have been
//   another test's (PacketDispatcherTestAccess::KeepRegistry).
// - The frees of objects whose class has no virtual destructor (most
//   tables, the managers, the surfaces, the zones, the view): deleting a
//   counting subclass through them would be undefined, so only their
//   reset is pinned; CSDLGraphics::ReleaseAll, which has no window to
//   destroy here; the audio's release with the audio down.
// - A name that resolves (GameInit.cpp:1213-1221): the second defect below.
//
// Recorded defects:
// - ReleaseAllObjects leaves four hosts installed (the hosts' test).
// - GameInit.cpp:1219 reads h->h_addr from gethostbyname's hostent. macOS's
//   resolver hands back the h_addr_list array at an address that is not
//   8-byte aligned, so the load is a misaligned read of a char* - undefined
//   (macOS 27's UBSan stops there on "localhost"; not run). gethostbyname
//   is also IPv4 only and not thread-safe; getaddrinfo has neither
//   problem.
// - GameInit.cpp:1209 resolves only names that do not start with a digit: a
//   host name like "3dlogin.example" goes to inet_addr, which answers
//   INADDR_NONE, and the connection is tried to 255.255.255.255 (not run: the
//   platforms refuse it with different errors).
// - GameInit.cpp:1213 runs gethostbyname before anything in InitSocket has
//   made a socket, and on Windows sockets start Winsock (SocketAPI.cpp's
//   lazy WSAStartup): a first log-in to a name may find no Winsock and fail
//   as "address not found" (not run: the test binary starts Winsock first).
// - GameInit.cpp:1234-1243: a failure of DARKEDEN_LOGIN_HOST/PORT's endpoint
//   (or the launcher's) still moves the count of failed tries, although
//   those endpoints do not rotate: the next try without them starts one
//   address on (the rotation's tests).
// - GameInit.cpp:1282-1305 delete the four peer managers if they are there,
//   but the ReleaseSocket at the top has deleted and reset them already, so
//   the branches never run (three of them would leave the pointer dangling
//   until it is assigned again a few lines on).
namespace {

std::string calls;
std::string N(long long value) { return std::to_string(value); }
void Call(const std::string& call) { calls += call + ";"; }

// The calls equal, or both printed.
bool Same(const std::string& expected, const std::string& actual)
{
	if (expected != actual)
		std::fprintf(stderr, "    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
	return expected == actual;
}
#define CHECK_SAME(expected, actual) CHECK(Same((expected), (actual)))

//----------------------------------------------------------------------
// Counted objects: each records its destruction ("~name").
//----------------------------------------------------------------------
template <typename T>
struct Counted : T
{
	const char* name;
	explicit Counted(const char* n) : name(n) {}
	~Counted() override { Call(std::string("~") + name); }
};
// A table whose Release (virtual) is recorded too.
template <typename T>
struct Released : Counted<T>
{
	using Counted<T>::Counted;
	void Release() override { Call(std::string(this->name) + ".Release"); T::Release(); }
};

// A socket of the test's, and the game's connection over it: disconnect
// is recorded with its argument.
struct CountedSocket : Socket
{
	using Socket::Socket;
	~CountedSocket() override { Call("~socket"); }
};
struct Connection : ClientPlayer
{
	using ClientPlayer::ClientPlayer;
	~Connection() override { Call("~connection"); }
	void disconnect(bool bDisconnected) override
	{
		Call(std::string("disconnect(") + (bDisconnected == UNDISCONNECTED ? "undisconnected" : "disconnected") + ")");
		ClientPlayer::disconnect(bDisconnected);
	}
};

// A packet factory of its own, at the last id (the managers it goes into
// are never initialised, so nothing else is there).
struct CountedFactory : PacketFactory
{
	~CountedFactory() override { Call("~factory"); }
	Packet* createPacket() override { return nullptr; }
	PacketID_t getPacketID() const override { return Packet::PACKET_MAX - 1; }
	PacketSize_t getPacketMaxSize() const override { return 0; }
};

// A peer, on a socket that never connected.
struct Peer : RequestServerPlayer
{
	Peer() : RequestServerPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) { setPlayerStatus(CPS_END_SESSION); }
	~Peer() override { Call("~peer"); }
};
// The manager's registration is protected: reached through a derived
// class's name, on the manager itself.
struct PeerAccess : RequestServerPlayerManager
{
	static bool Add(RequestServerPlayerManager& manager, RequestServerPlayer* pPlayer)
	{
		return (manager.*(&PeerAccess::AddRequestServerPlayer))(pPlayer);
	}
};

//----------------------------------------------------------------------
// Loopback
//----------------------------------------------------------------------
uint BoundPort(SOCKET s)
{
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
	int len = (int)sizeof(addr);
#else
	socklen_t len = (socklen_t)sizeof(addr);
#endif
	if (getsockname(s, (struct sockaddr*)&addr, &len) != 0)
		return 0;
	return ntohs(addr.sin_port);
}

// SO_LINGER off: a close returns at once and the system sends what is left.
bool LingerOff(SOCKET s)
{
	struct linger value;
	std::memset(&value, 0xFF, sizeof(value));
#ifdef _WIN32
	int len = (int)sizeof(value);
#else
	socklen_t len = (socklen_t)sizeof(value);
#endif
	if (getsockopt(s, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&value), &len) != 0)
		return false;
	return value.l_onoff == 0;
}

// A listener on an ephemeral port of every interface.
struct Listener
{
	bool sockets = (EnsureSocketsInitialised(), true);
	ServerSocket server{0};
	uint port = BoundPort(server.getSOCKET());
	std::vector<std::unique_ptr<Socket>> accepted;
	Listener() { server.setNonBlocking(true); }
	// The next connection, waited for at most two seconds; null if none.
	Socket* Accept()
	{
		for (int i = 0; i < 200; ++i)
		{
			if (Socket* pSocket = server.accept())
			{
				pSocket->setNonBlocking(true);
				accepted.emplace_back(pSocket);
				return pSocket;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return nullptr;
	}
};

// A port nothing listens on: a listener's, closed.
uint RefusedPort()
{
	EnsureSocketsInitialised();
	ServerSocket closed{0};
	return BoundPort(closed.getSOCKET());
}

// What the peer reads until the connection ends ("|end" then), waited for
// at most two seconds.
std::string Drain(Socket& peer)
{
	std::string bytes;
	for (int i = 0; i < 200; ++i)
	{
		char buffer[256];
		try {
			const uint got = peer.receive(buffer, sizeof(buffer));
			bytes.append(buffer, got);
			continue;
		} catch (NonBlockingIOException&) {
		} catch (ConnectException&) {
			return bytes + "|end";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return bytes;
}

// The bytes the game's encrypting stream (code 0) sends for a packet.
std::string Encoded(const Packet& packet)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream encoded(&out);
	encoded.write(&packet);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	return std::string(bytes.begin(), bytes.end());
}

//----------------------------------------------------------------------
// The environment
//----------------------------------------------------------------------
void PutEnvironment(const char* name, const std::optional<std::string>& value)
{
#ifdef _WIN32
	_putenv_s(name, value ? value->c_str() : "");	// "" removes it
#else
	if (value) setenv(name, value->c_str(), 1);
	else unsetenv(name);
#endif
}
// A variable removed for a test (or set), and put back after it.
struct Environment
{
	const char* name;
	std::optional<std::string> saved;
	explicit Environment(const char* n) : name(n), saved(NetworkTransport::ReadEnvironment(n)) { PutEnvironment(name, std::nullopt); }
	~Environment() { PutEnvironment(name, saved); }
	void Set(const std::string& value) { PutEnvironment(name, value); }
};

// A directory of the test's own, removed with what is in it.
struct TempDir
{
	std::filesystem::path path;
	TempDir()
	{
		static int serial = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_game_init_2_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(++serial));
		std::filesystem::create_directories(path);
	}
	~TempDir() { std::error_code error; std::filesystem::remove_all(path, error); }
	std::string Write(const std::string& name, const std::string& text) const
	{
		std::ofstream out(path / name, std::ios::binary | std::ios::trunc);
		out << text;
		return (path / name).string();
	}
};

//----------------------------------------------------------------------
// The scene a failed InitSocket goes through
//----------------------------------------------------------------------
// What is held, as the release finds it.
std::string Held()
{
	std::string held;
	auto add = [&held](bool there, const char* name) { if (there) held += (held.empty() ? "" : ",") + std::string(name); };
	add(g_pSocket != NULL, "socket");
	add(g_pConfigKorean != NULL, "korean");
	add(g_pConfigForeign != NULL, "foreign");
	add(g_pPacketFactoryManager != NULL, "factories");
	add(g_pPacketValidator != NULL, "validator");
	add(g_pRequestServerPlayerManager != NULL, "peers");
	add(g_pClientCommunicationManager != NULL, "datagrams");
	add(g_pRequestUserManager != NULL, "users");
	add(g_pRequestFileManager != NULL, "files");
	return "[" + held + "]";
}

// The game's: the release names what is held, then releases it.
const GameMainHost mainHost{
	.InitDraw = []() -> BOOL { Call("draw"); return TRUE; },
	.InitSurface = []() -> BOOL { Call("surface"); return TRUE; },
	.InitSound = []() -> BOOL { Call("sound"); return TRUE; },
	.ReleaseSocket = []() { Call("release socket" + Held()); ReleaseSocket(); },
};

const CWaitUIHost uiHost{
	.UpdateMouse = []() { Call("ui.mouse"); },
	.UpdateInput = []() { Call("ui.input"); },
	.CheckInvalidProcess = []() { Call("ui.check"); return true; },
	.UpdateSocketInput = []() { Call("ui.in"); return true; },
	.UpdateSocketOutput = []() { Call("ui.out"); return true; },
	.SetMode = [](CLIENT_MODE mode) { Call("ui.mode" + N(mode)); },
	.DrawTitleLoading = []() { Call("ui.title"); },
	.EndTitleLoading = [](bool) { Call("ui.end"); },
};
struct WaitUI : CWaitUIUpdate
{
	WaitUI() : CWaitUIUpdate(&uiHost) {}
	void Init() override { Call("ui init"); CWaitUIUpdate::Init(); }
};

// The UI draws no cursor (the pointer needs the game's sprites).
const UiRuntime::Host uiRuntime{ .EventFlagActive = [](std::uint32_t flag) { return flag == EVENTFLAG_NOT_DRAW_MOUSE_POINTER; } };

spritectl_surface_t Flipped() { Call("flip"); return SPRITECTL_INVALID_SURFACE; }

// The title's missing data files are reported here, not by ending the
// process.
void Reporter(EXCEPTION_CODE, const char*, const ExceptionSite&) {}

// The datagram socket binds an ephemeral port.
const WireHost wireHost = {
	.MaxProcessPacket = NULL,
	.MaxRequestService = NULL,
	.ClientCommunicationUDPPort = []() -> uint { return 0; },
	.BugReportTarget = NULL,
	.EncryptZoneID = NULL,
	.EncryptServerID = NULL,
	.EncryptUsesEnglishSeed = NULL,
	.CurrentTime = NULL,
	.SendOtherRequest = NULL,
	.HasOtherRequest = NULL,
	.RemoveOtherRequest = NULL,
};

// A failed InitSocket, inactive: the main menu (its sound, its UI loop)
// then the disconnect (the release, the box flipped, the UI flipped).
std::string Failed(const std::string& held) { return "sound;ui init;release socket" + held + ";flip;flip;"; }

// Everything the scene and InitSocket change, saved and given back.
struct Saved
{
	CLIENT_MODE mode = g_Mode;
	int activeGame = g_bActiveGame;
	CWinUpdate* update = g_pUpdate;
	int updateDelay = g_UpdateDelay;
	DWORD currentFrame = g_CurrentFrame;
	BOOL uiInput = g_bUIInput;
	BOOL left = g_bLButtonDown, right = g_bRButtonDown, centre = g_bCButtonDown;
	RECT gameRect = g_GameRect;
	int dimension = g_Dimension;
	char futecIP[sizeof(g_FutecIP)];
	unsigned int futecPort = g_FutecPort;
	CSpriteSurface* back = g_pBack;
	CSpriteSurface* last = g_pLast;
	CSDLInput* input = g_pSDLInput;
	CWaitUIUpdate* waitUI = g_pCWaitUIUpdate;
	UserOption* option = g_pUserOption;
	UIDialog* dialog = g_pUIDialog;
	MChatManager* chat = g_pChatManager;
	CMessageArray* systemLog = g_pSystemMessage;
	CMessageArray* playerLog = g_pPlayerMessage;
	CMessageArray* gameLog = g_pGameMessage;
	CMessageArray* noticeLog = g_pNoticeMessage;
	MMusic music = g_Music;
	Properties* files = g_pFileDef;
	MWeather* weather = g_pWeather;
	MParty* party = g_pParty;
	MEventQueue* events = g_pEventManager;
	MWarManager* wars = g_pWarManager;
	MPlayer* player = g_pPlayer;
	MZone* zone = g_pZone;
	ClientPlayer* socket = g_pSocket;
	Properties* korean = g_pConfigKorean;
	ServerInfoFileParser* foreign = g_pConfigForeign;
	PacketFactoryManager* factories = g_pPacketFactoryManager;
	PacketValidator* validator = g_pPacketValidator;
	RequestServerPlayerManager* peers = g_pRequestServerPlayerManager;
	ClientCommunicationManager* datagrams = g_pClientCommunicationManager;
	RequestUserManager* users = g_pRequestUserManager;
	RequestFileManager* fileRequests = g_pRequestFileManager;
	// The UI's.
	PI_ProcessRunner* runner = gpC_process_runner;
	WindowManager* windows = gpC_window_manager;
	Base* base = gpC_base;
	C_VS_UI_MOUSE_POINTER* pointer = gpC_mouse_pointer;
	C_GLOBAL_RESOURCE* resource = gpC_global_resource;
	SkinManager* skins = g_pSkinManager;
	C_VS_UI_WINDOW_MANAGER* uiWindows = gpC_vs_ui_window_manager;
	C_VS_UI_ITEM* item = gpC_item;
	CI* ime = gC_ci;
	// The hosts and the environment.
	const GameMainHost* mainHost = SetGameMainHost(&::mainHost);
	const UiRuntime::Host* runtime = UiRuntime::SetHost(&uiRuntime);
	BasicExceptionReporter reporter = g_SetBasicExceptionReporter(Reporter);
	Environment host{"DARKEDEN_LOGIN_HOST"};
	Environment port{"DARKEDEN_LOGIN_PORT"};
	Saved()
	{
		std::memcpy(futecIP, g_FutecIP, sizeof(futecIP));
		Wire::SetHost(&wireHost);
		ResetGameMainState();
	}
	~Saved()
	{
		ResetGameMainState();
		Wire::SetHost(NULL);
		CSDLGraphics::SetBackBufferSource(nullptr);
		g_SetBasicExceptionReporter(reporter);
		UiRuntime::SetHost(runtime);
		SetGameMainHost(mainHost);
		gC_ci = ime;
		gpC_item = item;
		gpC_vs_ui_window_manager = uiWindows;
		g_pSkinManager = skins;
		gpC_global_resource = resource;
		gpC_mouse_pointer = pointer;
		gpC_base = base;
		gpC_window_manager = windows;
		gpC_process_runner = runner;
		g_pRequestFileManager = fileRequests;
		g_pRequestUserManager = users;
		g_pClientCommunicationManager = datagrams;
		g_pRequestServerPlayerManager = peers;
		g_pPacketValidator = validator;
		g_pPacketFactoryManager = factories;
		g_pConfigForeign = foreign;
		g_pConfigKorean = korean;
		g_pSocket = socket;
		g_pZone = zone;
		g_pPlayer = player;
		g_pWarManager = wars;
		g_pEventManager = events;
		g_pParty = party;
		g_pWeather = weather;
		g_pFileDef = files;
		g_Music = music;
		g_pNoticeMessage = noticeLog;
		g_pGameMessage = gameLog;
		g_pPlayerMessage = playerLog;
		g_pSystemMessage = systemLog;
		g_pChatManager = chat;
		g_pUIDialog = dialog;
		g_pUserOption = option;
		g_pCWaitUIUpdate = waitUI;
		g_pSDLInput = input;
		g_pLast = last;
		g_pBack = back;
		g_FutecPort = futecPort;
		std::memcpy(g_FutecIP, futecIP, sizeof(futecIP));
		g_Dimension = dimension;
		g_GameRect = gameRect;
		g_bCButtonDown = centre;
		g_bRButtonDown = right;
		g_bLButtonDown = left;
		g_bUIInput = uiInput;
		g_CurrentFrame = currentFrame;
		g_UpdateDelay = updateDelay;
		g_pUpdate = update;
		g_bActiveGame = activeGame;
		g_Mode = mode;
	}
};

// Four spellings of 127.0.0.1 that inet_addr reads everywhere (a.b.c.d,
// a.b, a.b.c, a): the socket keeps the spelling it was given, so the
// connection names the address it was made to.
const char* const kSpellings[] = {"127.0.0.1", "127.1", "127.0.1", "2130706433"};
const int kSpellingCount = 4;

// The client at the title, inactive, with nothing connected: the main
// menu's and the disconnect's objects, the UI with its skins, the screen
// (two small surfaces), the settings files in a directory of the test's,
// a listener, and the client foreign in the first dimension with no
// launcher address and no request service.
struct Scene : GameModelWorld
{
	Saved saved;
	TempDir dir;
	Listener listener;
	WaitUI waitUI;
	CSDLInput input;
	UIDialog dialog;
	UserOption option;
	MChatManager chat;
	CMessageArray systemLog, playerLog, gameLog, noticeLog;
	Properties files;
	MWeather weather;
	MParty party;
	MEventQueue events;
	MWarManager wars;
	CI_KOREAN ime;
	CSpriteSurface back, last;

	Scene()
	{
		calls.clear();
		g_pSocket = NULL;
		g_pConfigKorean = NULL;
		g_pConfigForeign = NULL;
		g_pPacketFactoryManager = NULL;
		g_pPacketValidator = NULL;
		g_pRequestServerPlayerManager = NULL;
		g_pClientCommunicationManager = NULL;
		g_pRequestUserManager = NULL;
		g_pRequestFileManager = NULL;
		g_pCWaitUIUpdate = &waitUI;
		g_pUpdate = NULL;
		CHECK(input.Init(nullptr, nullptr));
		dxlib_input_virtual_reset();
		input.Clear();
		g_pSDLInput = &input;
		dialog.Init();
		g_pUIDialog = &dialog;
		option.PlayWaveMusic = FALSE;
		option.PlayMusic = FALSE;
		g_pUserOption = &option;
		g_pChatManager = &chat;
		for (CMessageArray* log : {&systemLog, &playerLog, &gameLog, &noticeLog}) log->Init(3, 128);
		g_pSystemMessage = &systemLog;
		g_pPlayerMessage = &playerLog;
		g_pGameMessage = &gameLog;
		g_pNoticeMessage = &noticeLog;
		g_Music = MMusic();
		g_Music.Init(NULL);
		CHECK(!g_SDLAudio.IsInit());
		g_pPlayer = NULL;
		g_pZone = NULL;
		// The disconnect box's sprites are not there: logged and not drawn.
		files.setProperty("FILE_SPRITEINDEX_UI", (dir.path / "absent.spki").string());
		files.setProperty("FILE_SPRITE_UI", (dir.path / "absent.spk").string());
		g_pFileDef = &files;
		g_pWeather = &weather;
		g_pParty = &party;
		g_pEventManager = &events;
		g_pWarManager = &wars;
		// The UI, on the heap: C_VS_UI::Release frees it.
		gpC_process_runner = new PI_ProcessRunner;
		gpC_window_manager = new WindowManager;
		gpC_base = new Base;
		gpC_mouse_pointer = new C_VS_UI_MOUSE_POINTER;
		gpC_global_resource = new C_GLOBAL_RESOURCE;
		g_pSkinManager = new SkinManager;
		for (int i = 0; i < SkinManager::INTERFACE_MAX; ++i)
			for (int n = 0; n < 64; ++n)
			{
				testfw::MutableRow(*g_pSkinManager, i).LoadFromLinePointList("0 0");
				testfw::MutableRow(*g_pSkinManager, i).LoadFromLineRectList("0 0 1 1");
			}
		gpC_vs_ui_window_manager = NULL;
		gpC_item = NULL;
		gC_ci = &ime;
		spritectl_init();
		back.Init(32, 32);
		last.Init(32, 32);
		g_pBack = &back;
		g_pLast = &last;
		gpC_base->m_p_DDSurface_back = &back;
		g_GameRect = {0, 0, 30, 30};
		CSDLGraphics::SetBackBufferSource(Flipped);
		g_Mode = MODE_NULL;
		g_bActiveGame = FALSE;
		g_pUserInformation->bKorean = false;
		g_pUserInformation->IsAutoLogIn = false;
		g_pUserInformation->pLogInClientPlayer = NULL;
		g_pClientConfig->MAX_REQUEST_SERVICE = 0;
		g_Dimension = 0;
		g_FutecIP[0] = '\0';
		g_FutecPort = 0;
		CHECK(!NetworkTransport::UsesWebSocket());
	}
	~Scene()
	{
		// Whatever InitSocket left.
		ReleaseSocket();
		SDL_StopTextInput();
		dxlib_input_virtual_reset();
		input.Clear();
		// The title, if started, and the UI's objects.
		gC_vs_ui.Release();
		dialog.Release();
	}

	// The foreign settings: one block per dimension, each its lines.
	void Foreign(const std::vector<std::string>& blocks)
	{
		std::string text;
		for (const std::string& block : blocks) text += "@\n" + block + "@\n";
		files.setProperty("FILE_INFO_SERVERINFO", dir.Write("serverinfo.inf", text));
	}
	// The Korean settings, under a file definition's key.
	void Korean(const char* key, const std::string& lines)
	{
		g_pUserInformation->bKorean = true;
		files.setProperty(key, dir.Write(std::string(key) + ".inf", lines));
	}
	// One login server: the address at the port (the listener's when 0).
	std::string Server(const std::string& address, uint port = 0) const
	{
		return "LoginServerAddress:" + address + "\nLoginServerPort:" + N(port != 0 ? port : listener.port) + "\n";
	}
	// The four spellings of the loopback address, one per try.
	std::string Rotation(int missing = -1) const
	{
		std::string text = "MaxLoginServerAddress:" + N(kSpellingCount) + "\nLoginServerPort:" + N(listener.port) + "\n";
		for (int i = 0; i < kSpellingCount; ++i)
			if (i != missing)
				text += (i == 0 ? std::string("LoginServerAddress") : "LoginServerAddress" + N(i)) + ":" + kSpellings[i] + "\n";
		return text;
	}

	// InitSocket on a clear dispatch table, cleared again after.
	BOOL Init()
	{
		calls.clear();
		PacketDispatcherTestAccess::Clear();
		const BOOL result = InitSocket();
		PacketDispatcherTestAccess::KeepRegistry();
		PacketDispatcherTestAccess::Clear();
		return result;
	}
	// The address the connection was made to, accepted; "" when none.
	std::string Connected()
	{
		if (g_pSocket == NULL || listener.Accept() == nullptr) return "";
		return g_pSocket->getSocket()->getHost();
	}
	// The try the rotation is at: connected with every address there,
	// which moves nothing.
	int Try()
	{
		Korean("FILE_INFO_GAMECLIENT", Rotation());
		if (!Init()) return -1;
		const std::string address = Connected();
		for (int i = 0; i < kSpellingCount; ++i)
			if (address == kSpellings[i]) return i;
		return -1;
	}
};

} // namespace

//======================================================================
// ReleaseSocket
//======================================================================
TEST(GameInit2ReleaseSocket, WithNothingThereNothingHappensTwice)
{
	Scene scene;
	ReleaseSocket();
	ReleaseSocket();
	CHECK_SAME(std::string("[]"), Held());
	CHECK_SAME(std::string(""), calls);
}

TEST(GameInit2ReleaseSocket, TheConnectionIsDisconnectedWithItsOutputSentThenEverythingIsFreedAndReset)
{
	Scene scene;
	// A connection with a packet queued, never flushed.
	CountedSocket* pSocket = new CountedSocket("127.0.0.1", scene.listener.port);
	pSocket->connect();
	Socket* pPeer = scene.listener.Accept();
	CHECK(pPeer != nullptr);
	Connection* pConnection = new Connection(pSocket);
	CGReady ready;
	pConnection->sendPacket(&ready);
	g_pSocket = pConnection;
	g_pConfigKorean = new Properties;
	g_pConfigForeign = new ServerInfoFileParser("absent.inf");
	g_pPacketFactoryManager = new PacketFactoryManager;
	g_pPacketFactoryManager->addFactory(new CountedFactory);
	g_pPacketValidator = new PacketValidator;
	g_pRequestServerPlayerManager = new RequestServerPlayerManager;
	CHECK(PeerAccess::Add(*g_pRequestServerPlayerManager, new Peer));
	g_pClientCommunicationManager = new ClientCommunicationManager;
	g_pRequestUserManager = new RequestUserManager;
	g_pRequestFileManager = new RequestFileManager;
	CHECK_SAME(std::string("[socket,korean,foreign,factories,validator,peers,datagrams,users,files]"), Held());
	calls.clear();
	ReleaseSocket();
	// Disconnected (sending what was queued) then freed with its socket; the
	// factory freed with the factories, the peer with the peer manager.
	CHECK_SAME(std::string("disconnect(undisconnected);~connection;~socket;~factory;~peer;"), calls);
	CHECK_SAME(std::string("[]"), Held());
	// The peer read the queued packet, then the end of the connection.
	if (pPeer != nullptr)
		CHECK_SAME(Encoded(ready) + "|end", Drain(*pPeer));
	// Again: nothing left to do.
	calls.clear();
	ReleaseSocket();
	CHECK_SAME(std::string(""), calls);
}

//======================================================================
// InitSocket: the settings
//======================================================================
TEST(GameInit2InitSocket, AForeignClientConnectsToTheServerInfoBlockOfItsDimension)
{
	for (int dimension : {0, 1})
	{
		Scene scene;
		// The other dimension's block names no address.
		const std::string good = scene.Server("127.0.0.1"), none = "LoginServerPort:" + N(scene.listener.port) + "\n";
		scene.Foreign(dimension == 0 ? std::vector<std::string>{good, none} : std::vector<std::string>{none, good});
		g_Dimension = dimension;
		CHECK_EQ(TRUE, scene.Init());
		CHECK_SAME(std::string(""), calls);
		CHECK_SAME(std::string("127.0.0.1"), scene.Connected());
		CHECK_SAME(std::string("[socket,foreign,factories,validator,datagrams,users,files]"), Held());
		CHECK_EQ(int(MODE_NULL), int(g_Mode));
	}
}

TEST(GameInit2InitSocket, AKoreanClientReadsGameClientOrInTheSecondDimensionGameClient2)
{
	for (int dimension : {0, 1, 2})
	{
		Scene scene;
		// The other file names a port nothing listens on.
		const bool second = dimension == 1;
		scene.Korean("FILE_INFO_GAMECLIENT", scene.Server("127.0.0.1", second ? RefusedPort() : 0));
		scene.Korean("FILE_INFO_GAMECLIENT2", scene.Server("127.0.0.1", second ? 0 : RefusedPort()));
		g_Dimension = dimension;
		CHECK_EQ(TRUE, scene.Init());
		CHECK_SAME(std::string(""), calls);
		CHECK_SAME(std::string("127.0.0.1"), scene.Connected());
		CHECK_SAME(std::string("[socket,korean,factories,validator,datagrams,users,files]"), Held());
	}
}

TEST(GameInit2InitSocket, ASettingsFileNotDefinedGoesToTheMainMenuDisconnected)
{
	for (bool korean : {false, true})
	{
		Scene scene;
		g_pUserInformation->bKorean = korean;
		g_bLButtonDown = TRUE;
		CHECK_EQ(FALSE, scene.Init());
		// Nothing was made.
		CHECK_SAME(Failed("[]"), calls);
		CHECK_SAME(std::string("[]"), Held());
		CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
		CHECK(g_pUpdate == &scene.waitUI);
		CHECK(gC_vs_ui.IsRunningTitle());
		CHECK_EQ(FALSE, g_bLButtonDown);
	}
}

TEST(GameInit2InitSocket, AKoreanSettingsFileThatCannotBeLoadedIsLeftForTheRelease)
{
	for (const char* text : {(const char*)nullptr, "LoginServerAddress 127.0.0.1\n"})
	{
		Scene scene;
		g_pUserInformation->bKorean = true;
		if (text == nullptr)
			scene.files.setProperty("FILE_INFO_GAMECLIENT", (scene.dir.path / "absent.inf").string());
		else
			scene.Korean("FILE_INFO_GAMECLIENT", text);	// no separator
		CHECK_EQ(FALSE, scene.Init());
		// The settings were made before the load failed; the release frees them.
		CHECK_SAME(Failed("[korean]"), calls);
		CHECK_SAME(std::string("[]"), Held());
	}
}

TEST(GameInit2InitSocket, AForeignServerInfoFileThatIsNotThereFailsAtTheEndpoint)
{
	Scene scene;
	scene.files.setProperty("FILE_INFO_SERVERINFO", (scene.dir.path / "absent.inf").string());
	CHECK_EQ(FALSE, scene.Init());
	// The parser reads nothing until the endpoint: no address.
	CHECK_SAME(Failed("[foreign,factories,validator]"), calls);
	CHECK_SAME(std::string("[]"), Held());
}

//======================================================================
// InitSocket: the connection
//======================================================================
TEST(GameInit2InitSocket, TheConnectionIsNonBlockingWithLingerOffAndSendsNothing)
{
	Scene scene;
	scene.Foreign({scene.Server("127.0.0.1")});
	CHECK_EQ(TRUE, scene.Init());
	CHECK(g_pSocket != NULL);
	if (g_pSocket == NULL) return;
	Socket* pPeer = scene.listener.Accept();
	CHECK(pPeer != nullptr);
	Socket* pSocket = g_pSocket->getSocket();
	CHECK_EQ(scene.listener.port, pSocket->getPort());
	// setLinger(0) turns lingering off (SocketImpl::setLinger).
	CHECK(LingerOff(pSocket->getSOCKET()));
#ifndef _WIN32
	// Windows cannot read a socket's mode back (SocketAPI: UnsupportedError).
	CHECK(pSocket->isNonBlocking());
#endif
	CHECK_EQ(int(CPS_NONE), int(g_pSocket->getPlayerStatus()));
	// The factories and the validator are initialised.
	CHECK(g_pPacketFactoryManager->getPacketMaxSize(Packet::PACKET_LC_LOGIN_OK) > 0);
	CHECK(g_pPacketValidator->isValidPacketID(CPS_AFTER_SENDING_CL_LOGIN, Packet::PACKET_LC_LOGIN_OK));
	// Nothing sent: the peer has nothing to read.
	if (pPeer != nullptr)
		CHECK_EQ(0u, pPeer->available());
}

TEST(GameInit2InitSocket, WhatWasThereIsReleasedFirstAndThePeerManagersAreNew)
{
	Scene scene;
	scene.Foreign({scene.Server("127.0.0.1")});
	// The last connection, its factories and its peer managers.
	CountedSocket* pSocket = new CountedSocket("127.0.0.1", scene.listener.port);
	pSocket->connect();
	Socket* pOldPeer = scene.listener.Accept();
	g_pSocket = new Connection(pSocket);
	g_pPacketFactoryManager = new PacketFactoryManager;
	g_pPacketFactoryManager->addFactory(new CountedFactory);
	g_pRequestServerPlayerManager = new RequestServerPlayerManager;
	CHECK(PeerAccess::Add(*g_pRequestServerPlayerManager, new Peer));
	g_pRequestUserManager = new RequestUserManager;
	g_pRequestUserManager->AddRequestUser("Ann", "10.0.0.1");
	g_pRequestFileManager = new RequestFileManager;
	CHECK(g_pRequestFileManager->AddOtherRequest(new RequestSendInfo("Ann")));
	CHECK_EQ(TRUE, scene.Init());
	CHECK_SAME(std::string("disconnect(undisconnected);~connection;~socket;~factory;~peer;"), calls);
	if (pOldPeer != nullptr)
		CHECK_SAME(std::string("|end"), Drain(*pOldPeer));
	CHECK_SAME(std::string("127.0.0.1"), scene.Connected());
	// No peer manager (no request service); the others new and empty.
	CHECK_SAME(std::string("[socket,foreign,factories,validator,datagrams,users,files]"), Held());
	CHECK(!g_pRequestUserManager->HasRequestUser("Ann"));
	CHECK(!g_pRequestFileManager->HasOtherRequest("Ann"));
}

TEST(GameInit2InitSocket, ARefusedConnectionGoesToTheMainMenuDisconnected)
{
	Scene scene;
	scene.Foreign({scene.Server("127.0.0.1", RefusedPort())});
	CHECK_EQ(FALSE, scene.Init());
	CHECK_SAME(Failed("[foreign,factories,validator]"), calls);
	CHECK_SAME(std::string("[]"), Held());
	CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
}

TEST(GameInit2InitSocket, ANameThatCannotBeResolvedGoesToTheMainMenuDisconnected)
{
	Scene scene;
	// Longer than any name can be: refused by every resolver at once.
	scene.Foreign({scene.Server(std::string(300, 'a'))});
	CHECK_EQ(FALSE, scene.Init());
	CHECK_SAME(Failed("[foreign,factories,validator]"), calls);
}

//======================================================================
// InitSocket: the endpoint's overrides
//======================================================================
TEST(GameInit2InitSocket, TheEnvironmentNamesTheHostAndThePortBeforeTheSettings)
{
	struct Case { bool host; bool port; };
	for (const Case c : {Case{true, true}, Case{true, false}, Case{false, true}})
	{
		Scene scene;
		// What the environment does not name, the settings do; what it does
		// name, the settings name wrongly.
		const std::string address = c.host ? std::string(300, 'a') : "127.0.0.1";
		scene.Foreign({scene.Server(address, c.port ? RefusedPort() : 0)});
		if (c.host) scene.saved.host.Set("127.0.0.1");
		if (c.port) scene.saved.port.Set(N(scene.listener.port));
		CHECK_EQ(TRUE, scene.Init());
		CHECK_SAME(std::string("127.0.0.1"), scene.Connected());
	}
}

TEST(GameInit2InitSocket, TheLaunchersAddressComesBeforeTheSettingsAndAfterTheEnvironment)
{
	{
		Scene scene;
		scene.Foreign({"LoginServerPort:" + N(RefusedPort()) + "\n"});
		Basic::CopyBounded(g_FutecIP, "127.0.0.1", sizeof(g_FutecIP));
		g_FutecPort = scene.listener.port;
		CHECK_EQ(TRUE, scene.Init());
		CHECK_SAME(std::string("127.0.0.1"), scene.Connected());
	}
	{
		Scene scene;
		scene.Foreign({"LoginServerPort:" + N(RefusedPort()) + "\n"});
		Basic::CopyBounded(g_FutecIP, "127.0.0.1", sizeof(g_FutecIP));
		g_FutecPort = RefusedPort();
		scene.saved.host.Set("127.0.0.1");
		scene.saved.port.Set(N(scene.listener.port));
		CHECK_EQ(TRUE, scene.Init());
		CHECK_SAME(std::string("127.0.0.1"), scene.Connected());
	}
}

TEST(GameInit2InitSocket, AMalformedPortInTheEnvironmentGoesToTheMainMenuDisconnected)
{
	Scene scene;
	scene.Foreign({scene.Server("127.0.0.1")});
	scene.saved.port.Set("0");
	CHECK_EQ(FALSE, scene.Init());
	CHECK_SAME(Failed("[foreign,factories,validator]"), calls);
}

//======================================================================
// InitSocket: the rotation through the login servers
//======================================================================
TEST(GameInit2Rotation, AConnectionMovesNothing)
{
	Scene scene;
	const int at = scene.Try();
	CHECK(at >= 0);
	CHECK_EQ(at, scene.Try());
	CHECK_EQ(at, scene.Try());
}

TEST(GameInit2Rotation, EachFailedTryMovesToTheNextAddressAndTheLastWrapsToTheFirst)
{
	int at = -1;
	{
		Scene scene;
		at = scene.Try();
		CHECK(at >= 0);
	}
	if (at < 0) return;
	// Fail at each address in turn, around twice, each in a scene of its own:
	// the count is kept from call to call whatever else changes.
	for (int step = 0; step < 2 * kSpellingCount; ++step)
	{
		{
			Scene scene;
			scene.Korean("FILE_INFO_GAMECLIENT", scene.Rotation(at));
			CHECK_EQ(FALSE, scene.Init());
			CHECK_SAME(Failed("[korean,factories,validator]"), calls);
		}
		Scene scene;
		const int next = scene.Try();
		CHECK_EQ((at + 1) % kSpellingCount, next);
		at = next;
	}
}

TEST(GameInit2Rotation, EveryFailureInPickingResolvingOrConnectingMovesIt)
{
	for (int failure = 0; failure < 4; ++failure)
	{
		int at = -1;
		{
			Scene scene;
			at = scene.Try();
		}
		{
			Scene scene;
			switch (failure)
			{
				case 0: scene.Korean("FILE_INFO_GAMECLIENT", scene.Server("127.0.0.1", RefusedPort())); break;
				case 1: scene.Korean("FILE_INFO_GAMECLIENT", scene.Server(std::string(300, 'a'))); break;
				// Recorded defect: an override that fails moves the rotation of
				// addresses it never used; the next try without it skips one.
				case 2: scene.Korean("FILE_INFO_GAMECLIENT", scene.Rotation()); scene.saved.port.Set("0"); break;
				case 3: scene.Korean("FILE_INFO_GAMECLIENT", scene.Rotation()); scene.saved.host.Set("127.0.0.1"); scene.saved.port.Set(N(RefusedPort())); break;
			}
			CHECK_EQ(FALSE, scene.Init());
		}
		Scene scene;
		CHECK_EQ((at + 1) % kSpellingCount, scene.Try());
	}
}

TEST(GameInit2Rotation, AFailureBeforeTheEndpointMovesNothing)
{
	int at = -1;
	{
		Scene scene;
		at = scene.Try();
	}
	{
		Scene scene;
		g_pUserInformation->bKorean = true;	// no FILE_INFO_GAMECLIENT
		CHECK_EQ(FALSE, scene.Init());
	}
	Scene scene;
	CHECK_EQ(at, scene.Try());
}

//======================================================================
// ReleaseAllObjects
//======================================================================
namespace {

// Every global ReleaseAllObjects (with EndTitleLoading, ReleaseGameUpdate
// and ReleaseSocket) frees or resets.
#define GI2_RELEASED(X) \
	X(g_pTitleLoadingSprite) X(g_pCGameUpdate) X(g_pCWaitPacketUpdate) X(g_pCWaitUIUpdate) \
	X(g_pOGG) X(g_oggfile) X(g_pSoundBufferForOGG) X(g_pUIDialog) X(g_pWeather) \
	X(g_pInventoryEffectManager) X(g_pPlayer) X(g_pZone) X(g_pZoneSmall) X(g_pZoneLarge) \
	X(g_pSDLInput) X(g_pZoneSoundTable) X(g_pZoneSoundManager) X(g_pTopView) X(g_pBack) X(g_pLast) \
	X(g_pSocket) X(g_pConfigKorean) X(g_pConfigForeign) X(g_pPacketFactoryManager) X(g_pPacketValidator) \
	X(g_pRequestServerPlayerManager) X(g_pClientCommunicationManager) X(g_pRequestFileManager) X(g_pRequestUserManager) \
	X(g_pGuildMarkManager) GI2_PLAIN(X) GI2_COUNTED(X)
// The ones with a virtual destructor, planted counted.
#define GI2_COUNTED(X) \
	X(g_pNPCTable) X(g_pNPCTalkBox) X(g_pPCTalkBox) X(g_pShopTemplateTable) X(g_pSlayerGear) X(g_pVampireGear) \
	X(g_pOustersGear) X(g_pInventory) X(g_pSoundManager) X(g_pNPCScriptTable) X(g_pServerInformation) \
	X(g_pQuestInfoManager) X(g_pAcceleratorManager) X(g_pModifyStatusManager)
// The rest, planted as new default objects: only their reset is seen
// (the event queue's removal of its events aside).
#define GI2_PLAIN(X) \
	X(g_pActionInfoTable) X(g_pCreatureSpriteTable) X(g_pCreatureTable) X(g_pCreatureSpriteTypeMapper) \
	X(g_pEffectSpriteTypeTable) X(g_pActionEffectSpriteTypeTable) X(g_pEffectStatusTable) X(g_pGameMessage) \
	X(g_pGameStringTable) X(g_pInteractionObjectTable) X(g_pItemOptionTable) X(g_pSkillAvailable) \
	X(g_pSkillInfoTable) X(g_pSkillManager) X(g_pSoundTable) X(g_pSystemMessage) X(g_pPlayerMessage) \
	X(g_pUsingColorSetTable) X(g_pZoneTable) X(g_pNickNameStringTable) X(g_pRegenTowerInfoManager) \
	X(g_pSystemAvailableManager) X(g_pWorldPlayerConfigTable) X(g_pUserInformation) X(g_pTempInformation) \
	X(g_pHelpMessage) X(g_pNoticeMessage) X(g_pClientConfig) X(g_pUserOption) X(g_pProfileManager) \
	X(g_pGuildInfoMapper) X(g_pEventManager) X(g_pItemTable) X(g_pJusticeAttackManager) \
	X(g_pRankBonusTable) X(g_pExperienceTable) X(g_pMusicTable) X(g_pLevelNameTable) X(g_pParty) \
	X(g_pTradeManager) X(g_pStorage) X(g_pStorage2) X(g_pGameTime) X(g_pKeyAccelerator) \
	X(g_pEffectGeneratorTable) X(g_pPriceManager) X(g_pMoneyManager) X(g_pChatManager) X(g_pHelpStringTable) \
	X(g_pHelpDisplayer) X(g_pObjectSelector) X(g_pUIMessageManager) X(g_pMonsterNameTable) \
	X(g_pTimeItemManager) X(g_pFameInfoTable) X(g_pWarManager) X(g_pProfiler) X(g_pFileDef)

#define GI2_SAVE(g) decltype(g) saved_##g = g;
#define GI2_CLEAR(g) g = NULL;
#define GI2_RESTORE(g) g = saved_##g;
#define GI2_PLANT(g) g = new std::remove_pointer_t<decltype(g)>;
#define GI2_HELD(g) if (g != NULL) held += std::string(held.empty() ? "" : ",") + #g;

// One of the hosts the start-up installs: Take installs one of the test's
// own (every entry empty) or none, keeping what was there; Give puts that
// back and answers whether the test's own was still installed.
struct HostSlot
{
	const char* name;
	void (*take)(bool own);
	bool (*give)();
};

template <typename T> struct HostOf;
template <typename T> struct HostOf<const T* (*)(const T*)> { using type = T; };

// The hosts whose SetHost answers the one it replaces.
template <auto Set>
struct Slot
{
	using Host = typename HostOf<decltype(Set)>::type;
	static inline const Host own{};
	static inline const Host* before = nullptr;
	static void Take(bool mine) { before = Set(mine ? &own : nullptr); }
	static bool Give() { return Set(before) == &own; }
};
#define GI2_SLOT(Owner) HostSlot{ #Owner, &Slot<&Owner::SetHost>::Take, &Slot<&Owner::SetHost>::Give }

// MItem's answers nothing, but it has a getter.
const MItemHost ownItemHost{};
const MItemHost* itemBefore = nullptr;
// Wire's and MPriceManager's have neither: their own hosts are told by an
// answer, and what was there is taken to be none, as every test leaves it.
const WireHost ownWireHost = []() { WireHost host{}; host.MaxProcessPacket = []() { return 77; }; return host; }();
const MPriceHost ownPriceHost = []() { MPriceHost host{}; host.Race = []() { return 2; }; return host; }();
struct PriceProbe : MPriceManager
{
	using MPriceManager::HostRace;
};
// DXInput's is copied in, and read back by value.
void OwnMousePosition(int, int) {}
DXInput::Host inputBefore;
// GameMain's answers the one it replaces.
const GameMainHost ownMainHost{};
const GameMainHost* mainBefore = nullptr;

const HostSlot kHostSlots[] = {
	{ "MItem", [](bool mine) { itemBefore = MItem::GetHost(); MItem::SetHost(mine ? &ownItemHost : nullptr); },
		[]() { const bool kept = MItem::GetHost() == &ownItemHost; MItem::SetHost(itemBefore); return kept; } },
	{ "Wire", [](bool mine) { Wire::SetHost(mine ? &ownWireHost : nullptr); },
		[]() { const bool kept = Wire::MaxProcessPacket() == 77; Wire::SetHost(nullptr); return kept; } },
	{ "MPriceManager", [](bool mine) { MPriceManager::SetHost(mine ? &ownPriceHost : nullptr); },
		[]() { const bool kept = PriceProbe::HostRace() == 2; MPriceManager::SetHost(nullptr); return kept; } },
	{ "DXInput", [](bool mine) { inputBefore = DXInput::GetHost(); DXInput::Host host{}; if (mine) host.mousePosition = OwnMousePosition; DXInput::SetHost(host); },
		[]() { const bool kept = DXInput::GetHost().mousePosition == OwnMousePosition; DXInput::SetHost(inputBefore); return kept; } },
	{ "SetGameMainHost", [](bool mine) { mainBefore = SetGameMainHost(mine ? &ownMainHost : nullptr); },
		[]() { return SetGameMainHost(mainBefore) == &ownMainHost; } },
	GI2_SLOT(MChatManager), GI2_SLOT(MWeather), GI2_SLOT(MWarManager), GI2_SLOT(MEffectTarget), GI2_SLOT(MEffect),
	GI2_SLOT(MHelicopter), GI2_SLOT(ZONESOUND_NODE), GI2_SLOT(MGuidanceEffect), GI2_SLOT(MParabolaEffect),
	GI2_SLOT(MAttachEffect), GI2_SLOT(MScreenEffectManager), GI2_SLOT(MStopInventoryEffectGenerator),
	GI2_SLOT(MStopZoneEffectGenerator), GI2_SLOT(MStopZoneCrossEffectGenerator), GI2_SLOT(MStopZoneXEffectGenerator),
	GI2_SLOT(MStopZoneRhombusEffectGenerator), GI2_SLOT(MStopZoneEmptyCrossEffectGenerator),
	GI2_SLOT(MStopZoneEmptyRectEffectGenerator), GI2_SLOT(MStopZoneRectEffectGenerator), GI2_SLOT(MStopZoneWallEffectGenerator),
	GI2_SLOT(MStopZoneMultipleEffectGenerator), GI2_SLOT(MStopZoneRandomEffectGenerator),
	GI2_SLOT(MStopZoneSelectableEffectGenerator), GI2_SLOT(MSpreadOutEffectGenerator), GI2_SLOT(MAroundZoneEffectGenerator),
	GI2_SLOT(MFollowPathEffectGenerator), GI2_SLOT(MMeteorDropEffectGenerator), GI2_SLOT(MAttackCreatureParabolaEffectGenerator),
	GI2_SLOT(MAttackCreatureEffectGenerator), GI2_SLOT(MAttackCreatureHomingEffectGenerator),
	GI2_SLOT(MRippleZoneWideEffectGenerator), GI2_SLOT(MBloodyBreakerEffectGenerator), GI2_SLOT(MBloodyWallEffectGenerator),
	GI2_SLOT(MBloodyWaveEffectGenerator), GI2_SLOT(MRippleZoneEffectGenerator), GI2_SLOT(MRippleZonePixelEffectGenerator),
	GI2_SLOT(MAttachZoneEffectGenerator), GI2_SLOT(MAttachZoneAroundEffectGenerator),
	GI2_SLOT(MAttachZoneSelectableEffectGenerator), GI2_SLOT(MAttackZoneRectEffectGenerator), GI2_SLOT(MSkipEffectGenerator),
	GI2_SLOT(RankBonusHandlers), GI2_SLOT(RegenZoneStatus), GI2_SLOT(BonusSkills), GI2_SLOT(SkillDowngrade),
	GI2_SLOT(GuildPresentation), GI2_SLOT(CommunicationPresentation), GI2_SLOT(QuestWarPresentation),
	GI2_SLOT(NoticePresentation), GI2_SLOT(StatusPresentation), GI2_SLOT(GameOptions), GI2_SLOT(PlayerPresentation),
	GI2_SLOT(NPCPresentation), GI2_SLOT(ZoneLoading), GI2_SLOT(TradePresentation), GI2_SLOT(GameInfoPresentation),
	GI2_SLOT(DialogPresentation), GI2_SLOT(DialogWindow), GI2_SLOT(LoginPresentation), GI2_SLOT(PlayerState),
	GI2_SLOT(ZoneState), GI2_SLOT(CreatureState), GI2_SLOT(WorldEvents), GI2_SLOT(VsUiPresentation), GI2_SLOT(LoginLists),
	GI2_SLOT(MStopZoneEmptyHorizontalWallEffectGenerator), GI2_SLOT(MStopZoneEmptyVerticalWallEffectGenerator),
	GI2_SLOT(MFallingEffectGenerator), GI2_SLOT(MAttackZoneEffectGenerator), GI2_SLOT(MFirePiecingEffectGenerator),
	GI2_SLOT(MPlayingWithFireEffectGenerator), GI2_SLOT(MAttackZoneBombEffectGenerator),
	GI2_SLOT(MAttackZoneParabolaEffectGenerator), GI2_SLOT(MMultipleFallingEffectGenerator), GI2_SLOT(MRisingEffectGenerator),
	GI2_SLOT(MParty), GI2_SLOT(UiRuntime), GI2_SLOT(GameUI), GI2_SLOT(UIMessages),
};

// The names of GameInit.cpp's lines that install a host ("Owner::SetHost(&s_..."
// or "({", and SetGameMainHost) or reset one ("Owner::SetHost(nullptr);"),
// read from the tree this test was built from.
std::set<std::string> SourceHosts(bool installs)
{
	std::ifstream in(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Client" / "GameInit.cpp");
	CHECK(in.good());
	const std::regex install("^\\s*([A-Za-z_][A-Za-z0-9_]*)::SetHost\\((&s_|\\{)");
	const std::regex mainHostInstall("^\\s*SetGameMainHost\\(&s_");
	const std::regex reset("^\\s*([A-Za-z_][A-Za-z0-9_]*)::SetHost\\(nullptr\\);");
	std::set<std::string> names;
	std::string line;
	std::smatch match;
	while (std::getline(in, line))
	{
		if (installs && std::regex_search(line, match, install)) names.insert(match[1].str());
		else if (installs && std::regex_search(line, mainHostInstall)) names.insert("SetGameMainHost");
		else if (!installs && std::regex_search(line, match, reset)) names.insert(match[1].str());
	}
	return names;
}

// The names in one set and not the other, printed.
bool SameNames(const std::set<std::string>& expected, const std::set<std::string>& actual)
{
	for (const std::string& name : expected)
		if (actual.count(name) == 0) std::fprintf(stderr, "    missing %s\n", name.c_str());
	for (const std::string& name : actual)
		if (expected.count(name) == 0) std::fprintf(stderr, "    extra   %s\n", name.c_str());
	return expected == actual;
}

// The move tables MCreature keeps for every creature.
struct MoveTables : MCreature
{
	static bool Ready() { return m_cXTable[0][0] != NULL && m_cYTable[MAX_FRAME_MOVE - 1][MAX_DIRECTION - 1] != NULL; }
};

// The globals not reset, by name.
std::string StillHeld()
{
	std::string held;
	GI2_RELEASED(GI2_HELD)
	return held;
}

// Every global above saved, then none, and every host the start-up
// installs saved, then none (or one of the test's own); all given back
// after. The game is active, the title loading, the music playing and the
// move tables made.
struct Teardown
{
	GI2_RELEASED(GI2_SAVE)
	int activeGame = g_bActiveGame;
	bool titleLoading = g_bTitleLoading;
	MMusic music = g_Music;
	bool moveTables = MoveTables::Ready();
	bool hostsGiven = false;
	explicit Teardown(bool ownHosts = false)
	{
		GI2_RELEASED(GI2_CLEAR)
		for (const HostSlot& slot : kHostSlots) slot.take(ownHosts);
		calls.clear();
		g_bActiveGame = TRUE;
		g_bTitleLoading = true;
		g_Music = MMusic();
		g_Music.Init(NULL);
		g_Music.m_bLoad = true;
		g_Music.m_bPlay = true;
		CHECK(!g_SDLAudio.IsInit());
		MCreature::InitMoveTable();
	}
	// The hosts given back: the names of the test's own still installed.
	std::string GiveHosts()
	{
		std::string kept;
		for (const HostSlot& slot : kHostSlots)
			if (slot.give()) kept += std::string(kept.empty() ? "" : ",") + slot.name;
		hostsGiven = true;
		return kept;
	}
	~Teardown()
	{
		if (!hostsGiven) GiveHosts();
		GI2_RELEASED(GI2_RESTORE)
		g_Music = music;
		g_bTitleLoading = titleLoading;
		g_bActiveGame = activeGame;
		if (moveTables) MCreature::InitMoveTable();
		else MCreature::ReleaseMoveTable();
	}
};

// The three loops, over hosts that are never called.
const CGameUpdateHost noGameHost{};
const CWaitPacketHost noPacketHost{};
struct GameLoop : CGameUpdate { GameLoop() : CGameUpdate(&noGameHost) {} ~GameLoop() override { Call("~game loop"); } };
struct PacketWait : CWaitPacketUpdate { PacketWait() : CWaitPacketUpdate(&noPacketHost) {} ~PacketWait() override { Call("~packet wait"); } };
struct UiWait : CWaitUIUpdate { UiWait() : CWaitUIUpdate(&uiHost) {} ~UiWait() override { Call("~ui wait"); } };

// The player, which says whether the zone still held it when it went.
const TYPE_OBJECTID kPlayerID = 1234;
struct PlayerProbe : MPlayer
{
	~PlayerProbe() override { Call(std::string("~player") + (g_pZone != NULL && g_pZone->GetCreature(kPlayerID) != NULL ? "(in the zone)" : "")); }
};

// The event queue's removal of an event resets the gamma.
const MEventHost eventHost = []() {
	MEventHost host{};
	host.SetAddGammaRamp = [](WORD, WORD, WORD) { Call("events.gamma"); };
	return host;
}();

} // namespace

TEST(GameInit2ReleaseAllObjects, WithNothingThereItEndsTheGameStopsTheMusicAndFreesTheMoveTables)
{
	Teardown teardown;
	ReleaseAllObjects();
	CHECK_EQ(FALSE, g_bActiveGame);
	CHECK_EQ(false, g_bTitleLoading);
	CHECK(!g_Music.m_bLoad && !g_Music.m_bPlay);
	CHECK(!MoveTables::Ready());
	CHECK_SAME(std::string(""), StillHeld());
	CHECK_SAME(std::string(""), calls);
}

TEST(GameInit2ReleaseAllObjects, EverythingIsFreedInItsOrderAndReset)
{
	Teardown teardown;
	// The zones first: a zone made without file definitions reads no file.
	g_pZoneSmall = new MZone(4, 4);
	g_pZoneLarge = new MZone(4, 4);
	g_pZone = g_pZoneSmall;
	GI2_PLAIN(GI2_PLANT)
	g_pTitleLoadingSprite = new Counted<CSprite565>("title");
	g_pCGameUpdate = new GameLoop;
	g_pCWaitPacketUpdate = new PacketWait;
	g_pCWaitUIUpdate = new UiWait;
	g_pOGG = new COGGSTREAM(NULL, NULL, 0, 0, 0, 0);
	g_oggfile = Basic::TemporaryFile();
	CHECK(g_oggfile != NULL);
	char buffer = 0;
	g_pSoundBufferForOGG = reinterpret_cast<CDirectSoundBuffer*>(&buffer);	// never read
	g_pUIDialog = new UIDialog;
	g_pUIDialog->Init();
	g_pWeather = new MWeather;
	g_pGuildMarkManager = new MGuildMarkManager("absent.spk", "absent.inf", "absent.spk", "absent.spk", "absent.spk", "absent.spk");
	g_pInventoryEffectManager = new Counted<MScreenEffectManager>("inventory effects");
	PlayerProbe* pPlayer = new PlayerProbe;
	pPlayer->SetID(kPlayerID);
	pPlayer->MObject::SetPosition(1, 1);
	CHECK(g_pZone->AddCreature(pPlayer));
	g_pPlayer = pPlayer;
	g_pSDLInput = new CSDLInput;
	g_pZoneSoundTable = new Counted<MZoneSoundTable>("zone sounds");
	g_pZoneSoundManager = new Released<MZoneSoundManager>("zone sound nodes");
	g_pTopView = new MTopView;
	g_pBack = new CSpriteSurface;
	g_pLast = new CSpriteSurface;
	g_pSocket = new Connection(new CountedSocket((EnsureSocketsInitialised(), new SocketImpl())));
	g_pConfigKorean = new Properties;
	g_pConfigForeign = new ServerInfoFileParser("absent.inf");
	g_pPacketFactoryManager = new PacketFactoryManager;
	g_pPacketFactoryManager->addFactory(new CountedFactory);
	g_pPacketValidator = new PacketValidator;
	g_pRequestServerPlayerManager = new RequestServerPlayerManager;
	CHECK(PeerAccess::Add(*g_pRequestServerPlayerManager, new Peer));
	{
		const WireHost* none = NULL;
		Wire::SetHost(&wireHost);	// the datagram socket on an ephemeral port
		g_pClientCommunicationManager = new ClientCommunicationManager;
		Wire::SetHost(none);
	}
	g_pRequestFileManager = new RequestFileManager;
	g_pRequestUserManager = new RequestUserManager;
	g_pNPCTable = new Released<MNPCTable>("npcs");
	g_pNPCTalkBox = new Counted<MStringList>("npc talk");
	g_pPCTalkBox = new Counted<PCTalkBox>("pc talk");
	g_pShopTemplateTable = new Released<MShopTemplateTable>("shop templates");
	g_pSlayerGear = new Counted<MSlayerGear>("slayer gear");
	g_pVampireGear = new Counted<MVampireGear>("vampire gear");
	g_pOustersGear = new Counted<MOustersGear>("ousters gear");
	g_pInventory = new Counted<MInventory>("inventory");
	g_pSoundManager = new Counted<CSoundPartManager>("sounds");
	g_pNPCScriptTable = new Released<MNPCScriptTable>("npc scripts");
	g_pServerInformation = new Released<CServerInformation>("servers");
	g_pQuestInfoManager = new Released<MQuestInfoManager>("quests");
	g_pAcceleratorManager = new Counted<AcceleratorManager>("accelerators");
	g_pModifyStatusManager = new Counted<ModifyStatusManager>("status reactions");
	delete g_pEventManager;
	g_pEventManager = new MEventQueue(&eventHost);
	MEvent event;
	event.eventID = EVENTID_HALLUCINATION;
	event.eventFlag = 0;
	g_pEventManager->AddEvent(event);
	CHECK_SAME(std::string(""), calls);

	ReleaseAllObjects();
	CHECK_SAME(std::string(
		// The title, the loops, the inventory's effects, the player (out of
		// the zone first) and the zone's sounds.
		"~title;~game loop;~packet wait;~ui wait;~inventory effects;~player;~zone sounds;~zone sound nodes;"
		// The connection.
		"disconnect(undisconnected);~connection;~socket;~factory;~peer;"
		// The tables released (the zone sounds' Release is never reached:
		// they went above).
		"npcs.Release;shop templates.Release;npc scripts.Release;"
		// Then freed, in the order of the list.
		"~servers;events.gamma;~npcs;~npc scripts;~shop templates;~quests;~sounds;"
		"~slayer gear;~vampire gear;~ousters gear;~inventory;~accelerators;~status reactions;~pc talk;~npc talk;"),
		calls);
	CHECK_SAME(std::string(""), StillHeld());
	CHECK_EQ(FALSE, g_bActiveGame);
	CHECK(!MoveTables::Ready());
}

TEST(GameInit2ReleaseAllObjects, TheSlotsHereAreEveryHostTheStartUpInstallsAndResets)
{
	std::set<std::string> slots;
	for (const HostSlot& slot : kHostSlots) slots.insert(slot.name);
	CHECK_EQ(84, int(sizeof(kHostSlots) / sizeof(kHostSlots[0])));
	CHECK_EQ(84, int(slots.size()));
	// InitGameObject's and InitInput's installs, the 84 here.
	CHECK(SameNames(slots, SourceHosts(true)));
	// ReleaseAllObjects' resets, the same but four (the next test's defect).
	std::set<std::string> reset = slots;
	for (const char* left : {"Wire", "MPriceManager", "DXInput", "SetGameMainHost"}) reset.erase(left);
	CHECK_EQ(80, int(reset.size()));
	CHECK(SameNames(reset, SourceHosts(false)));
}

TEST(GameInit2ReleaseAllObjects, EightyOfTheEightyFourHostsAreReset)
{
	Teardown teardown(true);
	ReleaseAllObjects();
	const std::string kept = teardown.GiveHosts();
	// Recorded defect: GameInit.cpp:386 (DXInput::SetHost in InitInput),
	// 4041 (Wire), 4042 (MPriceManager) and 4123 (SetGameMainHost) are not
	// undone by ReleaseAllObjects (1692-1771): the four keep pointing at
	// GameInit's lambdas, which read the globals it has just freed. The
	// process exits after it today; a teardown followed by another start-up
	// in one process (a test, a relaunch) would call through them.
	CHECK_SAME(std::string("Wire,MPriceManager,DXInput,SetGameMainHost"), kept);
	CHECK_SAME(std::string(""), calls);
}

TEST(GameInit2ReleaseAllObjects, AWaveFileIsClosedOnlyWithItsStream)
{
	Teardown teardown;
	std::FILE* pFile = Basic::TemporaryFile();
	CHECK(pFile != NULL);
	char buffer = 0;
	CDirectSoundBuffer* const pBuffer = reinterpret_cast<CDirectSoundBuffer*>(&buffer);	// never read
	g_oggfile = pFile;
	g_pSoundBufferForOGG = pBuffer;
	ReleaseAllObjects();
	// No stream: the file and the buffer are left as they were.
	CHECK(g_oggfile == pFile);
	CHECK(g_pSoundBufferForOGG == pBuffer);
	if (pFile != NULL) std::fclose(pFile);
	g_oggfile = NULL;
	g_pSoundBufferForOGG = NULL;
}
