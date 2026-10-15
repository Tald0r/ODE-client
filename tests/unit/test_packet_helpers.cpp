#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "PacketFunction.h"
#include "DirectionSelection.h"
#include "WorldEventsHost.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "MGameStringTable.h"
#include "MItemTable.h"
#include "CServerInformation.h"
#include "ClientConfig.h"
#include "CrashReport.h"
#include "MCrashReportManager.h"
#include "GameSocket.h"
#include "ClientPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Cpackets/CGCrashReport.h"
#include "Ci.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// The packet handlers' small helpers PacketFunction.cpp holds (gameclient's
// since task 4.253): the facing direction, frames from milliseconds, the
// login errors' message boxes, the server group's name and status, the
// fades, the flag's look, the crash reports sent to the server, and the
// MAC address and screen-mode check on Windows.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

// The message boxes, numbered as they open.
int windowCount = 0;
std::string W(DialogWindow::Window* pWindow) { return N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow))); }
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewMessage = [](int x, int y, int extraHeight, std::uint16_t buttons) {
		DialogWindow::Window* pWindow = reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(++windowCount));
		calls.push_back("box " + W(pWindow) + "|" + N(x) + "|" + N(y) + "|" + N(extraHeight) + "|" + N(buttons));
		return pWindow;
	};
	host.SetMessage = [](DialogWindow::Window* pWindow, const char* const* pLines, int count, bool noFit) {
		std::string line = "text " + W(pWindow) + (noFit ? " nofit" : " fit");
		for (int i = 0; i < count; ++i) line += std::string("|") + pLines[i];
		calls.push_back(line);
	};
	host.Start = [](DialogWindow::Window* pWindow) { calls.push_back("start " + W(pWindow)); };
	host.Finish = [](DialogWindow::Window* pWindow) { calls.push_back("finish " + W(pWindow)); };
	host.Delete = [](DialogWindow::Window* pWindow) { calls.push_back("delete " + W(pWindow)); };
	return host;
}();

// A ClientPlayer that keeps what it is asked to send.
struct SentPackets : ClientPlayer
{
	SentPackets() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~SentPackets() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override
	{
		if (pPacket->getPacketID() == Packet::PACKET_CG_CRASH_REPORT)
		{
			CGCrashReport& report = static_cast<CGCrashReport&>(*pPacket);
			calls.push_back("crash|" + report.getAddress() + "|" + report.getCallStack() + "|" + report.getExecutableTime()
				+ "|" + report.getMessage() + "|" + report.getOS() + "|" + N(report.getVersion()));
		}
		else
		{
			calls.push_back("packet|" + N(pPacket->getPacketID()));
		}
	}
};

bool viewExists = true;
const WorldEvents::Host viewHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return viewExists; };
	host.SetFadeStart = [](signed char start, signed char end, signed char step, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint16_t delay) {
		calls.push_back("fade|" + N(start) + "|" + N(end) + "|" + N(step) + "|" + N(r) + "|" + N(g) + "|" + N(b) + "|" + N(delay));
	};
	host.SetFadeEnd = []() { calls.push_back("fade end"); };
	host.SetFlagSprites = [](bool bTae) { calls.push_back(bTae ? "flag sprites tae" : "flag sprites original"); };
	return host;
}();

struct ViewScope
{
	const WorldEvents::Host* old = WorldEvents::SetHost(&viewHost);
	ViewScope() { calls.clear(); viewExists = true; }
	~ViewScope() { WorldEvents::SetHost(old); }
};

bool FileExists(const char* path) { std::error_code error; return std::filesystem::exists(path, error); }

// Whether a log the client deleted is gone. Off Windows DeleteFile is
// basic/Platform.h's stub, which deletes nothing (the crash handler writes
// no log there).
bool Forgotten(const char* path)
{
#ifdef PLATFORM_WINDOWS
	return !FileExists(path);
#else
	return FileExists(path);
#endif
}

} // namespace

TEST(PacketHelpers, TheFacingDirectionIsTheModelsSelection)
{
	// GetDirectionToPosition answers what SelectFacingDirection does, all
	// round the origin and on it.
	for (int dx = -2; dx <= 2; ++dx)
		for (int dy = -2; dy <= 2; ++dy)
			CHECK_EQ(int(SelectFacingDirection(10, 20, 10 + dx, 20 + dy)), int(GetDirectionToPosition(10, 20, 10 + dx, 20 + dy)));
}

TEST(PacketHelpers, MillisecondsBecomeFramesAtTheConfiguredRate)
{
	GameModelWorld world;
	g_pClientConfig->FPS = 16;
	CHECK_EQ(16u, ConvertMillisecondToFrame(1000));
	CHECK_EQ(15u, ConvertMillisecondToFrame(999));
	CHECK_EQ(0u, ConvertMillisecondToFrame(62));
	CHECK_EQ(1u, ConvertMillisecondToFrame(63));
	g_pClientConfig->FPS = 30;
	CHECK_EQ(3u, ConvertMillisecondToFrame(100));
}

TEST(PacketHelpers, EachLoginErrorHasItsMessageBox)
{
	// Each error opens the message box of its game string: an OK box,
	// centred, that leaves the game on its close (IsAfterExit) - the box a
	// direct PopupFreeMessageDlg with that text opens. An error with no case
	// of its own opens the general error's box, the plain one.
	GameModelWorld world;
	g_pGameStringTable->Init(MAX_GAME_STRING);
	for (int id = 0; id < MAX_GAME_STRING; ++id) g_pGameStringTable->GetInternalPointer()[id] = ("text " + N(id)).c_str();
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	g_pUIDialog = new UIDialog;
	g_pUIDialog->Init();
	auto opened = [](auto&& open) {
		calls.clear();
		windowCount = 0;
		open();
		std::vector<std::string> seen = calls;
		g_pUIDialog->CloseMessageDlg();
		return seen;
	};
	struct Case { ErrorID error; int string; bool exits; };
	const Case cases[] = {
		{INVALID_ID_PASSWORD, STRING_ERROR_INVALID_ID_PASSWORD, true}, {ALREADY_CONNECTED, STRING_ERROR_ALREADY_CONNECTED, true},
		{ALREADY_REGISTER_ID, STRING_ERROR_ALREADY_REGISTER_ID, true}, {ALREADY_REGISTER_SSN, STRING_ERROR_ALREADY_REGISTER_SSN, true},
		{EMPTY_ID, STRING_ERROR_EMPTY_ID, true}, {SMALL_ID_LENGTH, STRING_ERROR_SMALL_ID_LENGTH, true},
		{EMPTY_PASSWORD, STRING_ERROR_EMPTY_PASSWORD, true}, {SMALL_PASSWORD_LENGTH, STRING_ERROR_SMALL_PASSWORD_LENGTH, true},
		{EMPTY_NAME, STRING_ERROR_EMPTY_NAME, true}, {EMPTY_SSN, STRING_ERROR_EMPTY_SSN, true},
		{INVALID_SSN, STRING_ERROR_INVALID_SSN, true}, {NOT_FOUND_PLAYER, STRING_ERROR_NOT_FOUND_PLAYER, true},
		{NOT_FOUND_ID, STRING_ERROR_NOT_FOUND_ID, true}, {NOT_ALLOW_ACCOUNT, STRING_ERROR_NOT_ALLOW_ACCOUNT, true},
		{NOT_PAY_ACCOUNT, STRING_ERROR_NOT_PAY_ACCOUNT, true}, {CANNOT_AUTHORIZE_BILLING, STRING_ERROR_CANNOT_AUTHORIZE_BILLING, true},
		{CANNOT_CREATE_PC_BILLING, STRING_ERROR_CANNOT_CREATE_PC_BILLING, true}, {KEY_EXPIRED, STRING_ERROR_KEY_EXPIRED, true},
		{NOT_FOUND_KEY, STRING_ERROR_NOT_FOUND_KEY, true},
		// The login refused for another reason reads as a denied login.
		{ETC_ERROR, STRING_ERROR_LOGIN_DENY, true}, {IP_DENYED, STRING_ERROR_IP_DENY, true},
		// Recorded, not changed: the version error reads the game string
		// table at a UI string's index.
		{CHECK_VERSION_ERROR, UI_STRING_MESSAGE_CHECK_VERSION_ERROR, true},
		{CHILDGUARD_DENYED, STRING_ERROR_ETC_ERROR, false},
	};
	for (const Case& c : cases)
	{
		const std::vector<std::string> seen = opened([&] { PopupErrorMessage(c.error); });
		const std::string text = (*g_pGameStringTable)[c.string].GetString();
		const std::vector<std::string> expected = opened([&] {
			if (c.exits) g_pUIDialog->PopupFreeMessageDlg(text.c_str(), -1, -1, UI_DIALOG_OK, true);
			else g_pUIDialog->PopupFreeMessageDlg(text.c_str());
		});
		CHECK(!seen.empty());
		const bool same = seen == expected;
		if (!same) std::printf("      error %d differs\n", int(c.error));
		CHECK(same);
	}
	delete g_pUIDialog;
	g_pUIDialog = NULL;
	DialogWindow::SetHost(oldWindows);
}

TEST(PacketHelpers, TheServerGroupsNameAndStatusAreTheServerInformations)
{
	CServerInformation information;
	CServerInformation* old = g_pServerInformation;
	g_pServerInformation = &information;
	SetServerGroupName("Bathory");
	SetServerGroupStatus(3);
	CHECK(std::string(information.GetServerGroupName()) == "Bathory");
	CHECK_EQ(3, information.GetServerGroupStatus());
	g_pServerInformation = old;
}

TEST(PacketHelpers, TheFadesAreTheViews)
{
	ViewScope view;
	SetFadeStart(-1, 31, 2, 10, 20, 30, 400);
	SetFadeEnd();
	CHECK(calls == std::vector<std::string>({"fade|-1|31|2|10|20|30|400", "fade end"}));
}

TEST(PacketHelpers, TheFlagTakesItsTeamsLook)
{
	// The flag item (event item 27) drops and is carried as the team's or
	// the original flag, and the view's flag sprites follow.
	GameModelWorld world;
	ViewScope view;
	g_pItemTable->GetMutable(ITEM_CLASS_EVENT_ITEM)->Init(28);
	ITEMTABLE_INFO& flag = *(*g_pItemTable).GetMutable(ITEM_CLASS_EVENT_ITEM)->GetMutable(27);
	auto frames = [&]() {
		return N(flag.TileFrameID) + "|" + N(flag.InventoryFrameID) + "|" + N(flag.GearFrameID) + "|" + N(flag.DropFrameID);
	};
	flag.SetFrameID(1, 2, 3);
	flag.SetDropFrameID(4);
	SetFlagTo(true);
	CHECK(frames() == "856|879|0|856");
	SetFlagTo(false);
	CHECK(frames() == "750|764|0|750");
	CHECK(calls == std::vector<std::string>({"flag sprites tae", "flag sprites original"}));
	// Without the view nothing changes.
	calls.clear();
	viewExists = false;
	SetFlagTo(true);
	CHECK(frames() == "750|764|0|750");
	CHECK(calls.empty());
	// Without the flag's row the sprites change all the same.
	viewExists = true;
	g_pItemTable->GetMutable(ITEM_CLASS_EVENT_ITEM)->Init(0);
	SetFlagTo(true);
	CHECK(calls == std::vector<std::string>({"flag sprites tae"}));
}

TEST(PacketHelpers, AKoreanClientSendsItsCrashReportsAndForgetsThem)
{
	// The crash log's reports go to the server one by one, and both logs
	// are deleted; only the Korean client does it.
	CI* oldCI = gC_ci;
	ClientPlayer* oldSocket = g_pSocket;
	{
		MCrashReportManager log;
		log.Init(2);
		MCrashReport* pFirst = log.GetMutable(0);
		pFirst->SetAddress("0x0000000000001234");
		pFirst->SetCallStack(" 0x1 0x2");
		pFirst->SetExcutableTime("2026-10-15 12:00:00");
		pFirst->SetMessage("an Access Violation(0xc0000005)");
		pFirst->SetOS("Windows 10/11");
		pFirst->SetVersion(42);
		log.GetMutable(1)->SetVersion(7);
		log.SaveToFile(CRASH_LOG_FILENAME);
		std::FILE* pOld = std::fopen(OLD_CRASH_LOG_FILENAME, "wb");
		if (pOld != nullptr) std::fclose(pOld);
	}
	SentPackets wire;
	g_pSocket = &wire;
	// Another language's client keeps them and sends nothing.
	CI_CHINESE chinese;
	gC_ci = &chinese;
	calls.clear();
	SendCrashReport();
	CHECK(calls.empty());
	CHECK(FileExists(CRASH_LOG_FILENAME) && FileExists(OLD_CRASH_LOG_FILENAME));
	CI_KOREAN korean;
	gC_ci = &korean;
	SendCrashReport();
	CHECK(calls == std::vector<std::string>({
		"crash|0x0000000000001234| 0x1 0x2|2026-10-15 12:00:00|an Access Violation(0xc0000005)|Windows 10/11|42",
		"crash||||||7"}));
	CHECK(Forgotten(CRASH_LOG_FILENAME) && Forgotten(OLD_CRASH_LOG_FILENAME));
	// With no connection the reports are still forgotten.
	{
		MCrashReportManager log;
		log.Init(1);
		log.SaveToFile(CRASH_LOG_FILENAME);
	}
	g_pSocket = NULL;
	calls.clear();
	SendCrashReport();
	CHECK(calls.empty());
	CHECK(Forgotten(CRASH_LOG_FILENAME));
	g_pSocket = oldSocket;
	gC_ci = oldCI;
	std::remove(CRASH_LOG_FILENAME);
	std::remove(OLD_CRASH_LOG_FILENAME);
}

TEST(PacketHelpers, TheMacAddressComesFromNetBIOSOnWindowsOnly)
{
	BYTE address[6] = { 1, 2, 3, 4, 5, 6 };
#ifdef PLATFORM_WINDOWS
	// No buffer: refused. A real one is filled or the call says it failed.
	CHECK(!GetMacAddressFromNetBIOS(NULL));
	(void)GetMacAddressFromNetBIOS(address);
#else
	// Elsewhere there is no NetBIOS: refused, the buffer untouched.
	CHECK(!GetMacAddressFromNetBIOS(address));
	CHECK(address[0] == 1 && address[5] == 6);
#endif
}

#ifdef PLATFORM_WINDOWS
extern BYTE g_macAddress[6];
extern bool g_bFullScreen;
std::string GetLocalIP();
void CheckMacScreenMode();

TEST(PacketHelpers, TheLocalAddressIsDottedOrNothing)
{
	const std::string ip = GetLocalIP();
	int parts = ip.empty() ? 4 : 1;
	for (char c : ip)
	{
		if (c == '.') ++parts;
		else CHECK(c >= '0' && c <= '9');
	}
	CHECK_EQ(4, parts);
}

TEST(PacketHelpers, OnlyTheDevelopersMachinesAskForAWindow)
{
	// A MAC address off the developers' list, and one on it at another
	// address than its listed one, leave the screen mode alone.
	BYTE oldMac[6];
	std::memcpy(oldMac, g_macAddress, 6);
	const bool oldFullScreen = g_bFullScreen;
	const BYTE unlisted[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
	const BYTE listed[6] = { 0x00, 0x0E, 0xA6, 0x74, 0x38, 0xF7 };
	for (const BYTE* mac : { unlisted, listed })
		for (bool fullScreen : { true, false })
		{
			std::memcpy(g_macAddress, mac, 6);
			g_bFullScreen = fullScreen;
			CheckMacScreenMode();
			CHECK_EQ(fullScreen, g_bFullScreen);
		}
	std::memcpy(g_macAddress, oldMac, 6);
	g_bFullScreen = oldFullScreen;
}
#endif
