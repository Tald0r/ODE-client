#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "TalkBox.h"
#include "TempInformation.h"
#include "MCreatureTable.h"
#include "ClientMode.h"
#include "GameSocket.h"
#include "PlayerStateHost.h"
#include "DialogPresentationHost.h"
#include "TradePresentationHost.h"
#include "CommunicationPresentationHost.h"
#include "ClientPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Cpackets/CGLearnSkill.h"
#include "Cpackets/CGNPCAskAnswer.h"
#include "Cpackets/CGSelectBloodBible.h"
#include "Cpackets/CGSelectQuest.h"

#include <cstdio>
#include <string>
#include <vector>

extern bool g_bPartyRunning;

// The NPC talk box and the message box (task 4.238, gamemodel's since
// then). The windows, the other windows, the party window, the player
// and the socket are fakes that record each call in order.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

// The windows are numbered in the order they open.
std::vector<int> windows;
DialogWindow::Window* NewWindow()
{
	windows.push_back(static_cast<int>(windows.size()) + 1);
	return reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(windows.back()));
}
std::string W(DialogWindow::Window* pWindow) { return N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow))); }

std::string Render(Packet& packet)
{
	switch (packet.getPacketID())
	{
	case Packet::PACKET_CG_NPC_ASK_ANSWER: {
		CGNPCAskAnswer& p = static_cast<CGNPCAskAnswer&>(packet);
		return "answer|" + N(p.getObjectID()) + "|" + N(p.getScriptID()) + "|" + N(p.getAnswerID());
	}
	case Packet::PACKET_CG_LEARN_SKILL: {
		CGLearnSkill& p = static_cast<CGLearnSkill&>(packet);
		return "learn|" + N(p.getSkillType()) + "|" + N(p.getSkillDomainType());
	}
	case Packet::PACKET_CG_SELECT_QUEST: {
		CGSelectQuest& p = static_cast<CGSelectQuest&>(packet);
		return "quest|" + N(p.getQuestID()) + "|" + N(p.getNPCObjectID());
	}
	case Packet::PACKET_CG_SELECT_BLOOD_BIBLE: {
		CGSelectBloodBible& p = static_cast<CGSelectBloodBible&>(packet);
		return "bible|" + N(p.getBloodBibleID());
	}
	default:
		return "packet|" + N(packet.getPacketID());
	}
}

struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override { calls.push_back(Render(*pPacket)); }
};

const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewTalk = [](int creatureType, const char* name) {
		DialogWindow::Window* pWindow = NewWindow();
		calls.push_back("talk " + W(pWindow) + "|" + N(creatureType) + "|" + (name != NULL ? name : "(null)"));
		return pWindow;
	};
	host.NewMessage = [](int x, int y, int extraHeight, std::uint16_t buttons) {
		DialogWindow::Window* pWindow = NewWindow();
		calls.push_back("box " + W(pWindow) + "|" + N(x) + "|" + N(y) + "|" + N(extraHeight) + "|" + N(buttons));
		return pWindow;
	};
	host.SetMenu = [](DialogWindow::Window* pWindow, const DialogWindow::MenuItem* pItems, int count) {
		std::string line = "menu " + W(pWindow);
		for (int i = 0; i < count; ++i) line += "|" + N(pItems[i].id) + ":" + pItems[i].text;
		calls.push_back(line);
	};
	host.SetMessage = [](DialogWindow::Window* pWindow, const char* const* pLines, int count, bool noFit) {
		std::string line = "text " + W(pWindow) + (noFit ? " nofit" : " fit");
		for (int i = 0; i < count; ++i) line += std::string("|") + pLines[i];
		calls.push_back(line);
	};
	host.Start = [](DialogWindow::Window* pWindow) { calls.push_back("start " + W(pWindow)); };
	host.Finish = [](DialogWindow::Window* pWindow) { calls.push_back("finish " + W(pWindow)); };
	host.Run = [](DialogWindow::Window* pWindow, std::uint32_t id) { calls.push_back("run " + W(pWindow) + "|" + N(id)); };
	host.Delete = [](DialogWindow::Window* pWindow) { calls.push_back("delete " + W(pWindow)); };
	return host;
}();

bool storage, shop, bookcase, repair, sell, swapping, partyUp, exchange;
const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.IsRunningStorage = []() { return storage; };
	host.IsRunningShop = []() { return shop; };
	host.IsRunningBookcase = []() { return bookcase; };
	host.IsRunningRepair = []() { return repair; };
	host.IsRunningSell = []() { return sell; };
	host.IsRunningSwapAdvancementItem = []() { return swapping; };
	host.IsRunningParty = []() { return partyUp; };
	host.CloseAllDialog = []() { calls.push_back("close all"); };
	return host;
}();
const TradePresentation::Host tradeHost = []() {
	TradePresentation::Host host{};
	host.IsRunningExchange = []() { return exchange; };
	return host;
}();
const CommunicationPresentation::Host communicationHost = []() {
	CommunicationPresentation::Host host{};
	host.RunParty = []() { calls.push_back("party"); };
	return host;
}();

PlayerState::WAIT_VERIFY waiting = PlayerState::WAIT_VERIFY_NULL;
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.GetWaitVerify = []() { return waiting; };
	host.SetWaitVerify = [](PlayerState::WAIT_VERIFY verify) { calls.push_back("verify|" + N(verify)); };
	host.SetWaitVerifyNULL = []() { calls.push_back("verify null"); };
	return host;
}();

using Calls = std::vector<std::string>;

bool Same(const Calls& expected)
{
	if (calls == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

const std::string Exit = N(DialogWindow::EXECID_EXIT);

struct Scene
{
	GameModelWorld world;
	Wire wire;
	PCTalkBox talkBox;
	TempInformation temp;
	CREATURE_TABLE creatures;
	UIDialog dialog;
	ClientPlayer* oldSocket = g_pSocket;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	TempInformation* oldTemp = g_pTempInformation;
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	CLIENT_MODE oldMode = g_Mode;
	int oldActive = g_bActiveGame;
	bool oldParty = g_bPartyRunning;
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialogHost);
	const TradePresentation::Host* oldTrade = TradePresentation::SetHost(&tradeHost);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communicationHost);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);

	Scene()
	{
		calls.clear();
		windows.clear();
		storage = shop = bookcase = repair = sell = swapping = partyUp = exchange = false;
		waiting = PlayerState::WAIT_VERIFY_NULL;
		g_pSocket = &wire;
		g_pPCTalkBox = &talkBox;
		g_pTempInformation = &temp;
		creatures.Init(10);
		testfw::MutableRow(creatures, 7).Name = "Merchant";
		g_pCreatureTable = &creatures;
		g_Mode = MODE_GAME;
		g_bActiveGame = 1;
		g_bPartyRunning = false;
		talkBox.SetNPCID(300);
		talkBox.SetScriptID(40);
		talkBox.SetCreatureType(7);
		talkBox.SetContent("Hello there");
		dialog.Init();
	}
	~Scene()
	{
		dialog.Release();
		PlayerState::SetHost(oldPlayer);
		CommunicationPresentation::SetHost(oldCommunication);
		TradePresentation::SetHost(oldTrade);
		DialogPresentation::SetHost(oldDialog);
		DialogWindow::SetHost(oldWindows);
		g_bPartyRunning = oldParty;
		g_bActiveGame = oldActive;
		g_Mode = oldMode;
		g_pCreatureTable = oldCreatures;
		g_pTempInformation = oldTemp;
		g_pPCTalkBox = oldTalkBox;
		g_pSocket = oldSocket;
		talkBox.Release();
	}
	// The talk box opened with these lines.
	void Talk(PCTalkBox::TALKBOX_TYPE type, std::initializer_list<const char*> lines)
	{
		talkBox.Release();
		talkBox.SetType(type);
		for (const char* line : lines) talkBox.AddString(line);
		dialog.PopupPCTalkDlg();
		calls.clear();
	}
	DialogWindow::Window* Window(int number) { return reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(number)); }
};

} // namespace

//----------------------------------------------------------------------
// The message box
//----------------------------------------------------------------------
TEST(UIDialog, AShortMessageIsOneLine)
{
	Scene scene;
	scene.dialog.PopupFreeMessageDlg("Not enough money.");
	CHECK(Same({"box 1|-1|-1|0|1", "text 1 nofit|Not enough money.", "start 1"}));
	CHECK_EQ(FLAG_UIDIALOG_LOCK_MESSAGE, int(UIDialog::IsLockInput()));
	// Where and with which buttons, as asked; a new one replaces it.
	calls.clear();
	scene.dialog.PopupFreeMessageDlg("Again", 30, 40, 0x0006);
	CHECK(Same({"delete 1", "box 2|30|40|0|6", "text 2 nofit|Again", "start 2"}));
	// Forty characters are still one line.
	calls.clear();
	const std::string forty(40, 'x');
	scene.dialog.PopupFreeMessageDlg(forty.c_str());
	CHECK(Same({"delete 2", "box 3|-1|-1|0|1", "text 3 nofit|" + forty, "start 3"}));
}

TEST(UIDialog, ALongMessageBreaksAtTheLastSpaceBeforeForty)
{
	Scene scene;
	// 41 characters: the space at 34 breaks it, and the space goes.
	const std::string text = "The storage is full, so the item is kept";
	CHECK_EQ(40u, text.size());
	const std::string longer = text + "!";
	scene.dialog.PopupFreeMessageDlg(longer.c_str());
	CHECK(Same({"box 1|-1|-1|1|1", "text 1 nofit|The storage is full, so the item is|kept!", "start 1"}));
	// A space at 40 itself breaks there.
	calls.clear();
	const std::string atForty = std::string(40, 'a') + " tail";
	scene.dialog.PopupFreeMessageDlg(atForty.c_str());
	CHECK(Same({"delete 1", "box 2|-1|-1|1|1", "text 2 nofit|" + std::string(40, 'a') + "|tail", "start 2"}));
	// No space at all: the first line is empty and the second the whole
	// message (to the line's 256 bytes).
	calls.clear();
	const std::string word(60, 'w');
	scene.dialog.PopupFreeMessageDlg(word.c_str());
	CHECK(Same({"delete 2", "box 3|-1|-1|1|1", "text 3 nofit||" + word, "start 3"}));
	const std::string huge(400, 'h');
	calls.clear();
	scene.dialog.PopupFreeMessageDlg(huge.c_str());
	CHECK(Same({"delete 3", "box 4|-1|-1|1|1", "text 4 nofit||" + std::string(255, 'h'), "start 4"}));
}

TEST(UIDialog, TheMessageBoxUnlocksWhenAnsweredOrClosed)
{
	Scene scene;
	scene.dialog.PopupFreeMessageDlg("One");
	UIDialog::ProcessMessageDlg(scene.Window(1), 0);
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	scene.dialog.PopupFreeMessageDlg("Two");
	calls.clear();
	scene.dialog.CloseMessageDlg();
	CHECK(Same({"delete 2"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	// Closing nothing does nothing.
	calls.clear();
	UIDialog::SetLockInputMessage();
	scene.dialog.CloseMessageDlg();
	CHECK(Same({}));
	CHECK_EQ(FLAG_UIDIALOG_LOCK_MESSAGE, int(UIDialog::IsLockInput()));
	UIDialog::UnSetLockInputMessage();
}

//----------------------------------------------------------------------
// Opening the talk box
//----------------------------------------------------------------------
TEST(UIDialog, ATalkBoxListsTheLinesAsAnswers)
{
	Scene scene;
	scene.talkBox.SetType(PCTalkBox::NORMAL);
	scene.talkBox.AddString("Buy");
	scene.talkBox.AddString("Sell");
	scene.dialog.PopupFreeMessageDlg("before");
	calls.clear();
	scene.dialog.PopupPCTalkDlg();
	CHECK(Same({"close all", "delete 1", "talk 2|7|Merchant", "menu 2|1:Buy|2:Sell", "text 2 fit|Hello there", "start 2"}));
	CHECK_EQ(FLAG_UIDIALOG_LOCK_PCTALK, int(UIDialog::IsLockInput()));
	// Opened again, the old one goes first.
	calls.clear();
	scene.dialog.PopupPCTalkDlg();
	CHECK(Same({"delete 2", "close all", "talk 3|7|Merchant", "menu 3|1:Buy|2:Sell", "text 3 fit|Hello there", "start 3"}));
}

TEST(UIDialog, ATalkBoxNeedsTheGameActiveAndALine)
{
	Scene scene;
	scene.talkBox.AddString("Buy");
	g_bActiveGame = 0;
	scene.dialog.PopupPCTalkDlg();
	CHECK(Same({}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	g_bActiveGame = 1;
	scene.talkBox.Release();
	scene.dialog.PopupPCTalkDlg();
	CHECK(Same({"close all"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
}

TEST(UIDialog, SkillAndBibleLinesStartWithAThreeDigitID)
{
	Scene scene;
	int window = 0;
	for (PCTalkBox::TALKBOX_TYPE type : {PCTalkBox::SKILL_LEARN, PCTalkBox::BLOOD_BIBLE_SIGN})
	{
		calls.clear();
		scene.talkBox.Release();
		scene.talkBox.SetType(type);
		scene.talkBox.AddString("012Fire Ball");
		scene.talkBox.AddString("99");
		scene.talkBox.AddString("999Leave");
		scene.dialog.PopupPCTalkDlg();
		// A line shorter than its ID is left out.
		Calls expected;
		if (window > 0) expected.push_back("delete " + N(window));
		++window;
		expected.insert(expected.end(), {"close all", "talk " + N(window) + "|7|Merchant",
			"menu " + N(window) + "|12:Fire Ball|999:Leave", "text " + N(window) + " fit|Hello there", "start " + N(window)});
		CHECK(Same(expected));
		// The lines keep the name alone.
		CHECK(std::string((*scene.talkBox.begin())->GetString()) == "Fire Ball");
	}
}

TEST(UIDialog, QuestLinesStartWithAFourDigitID)
{
	Scene scene;
	scene.talkBox.SetType(PCTalkBox::SELECT_QUEST);
	scene.talkBox.AddString("0042Find the ring");
	scene.talkBox.AddString("123");
	scene.talkBox.AddString("9999Never mind");
	scene.dialog.PopupPCTalkDlg();
	CHECK(Same({"close all", "talk 1|7|Merchant", "menu 1|42:Find the ring|9999:Never mind", "text 1 fit|Hello there", "start 1"}));
}

TEST(UIDialog, ThePartyWindowIsRememberedWhileTalking)
{
	Scene scene;
	scene.talkBox.AddString("Buy");
	partyUp = true;
	scene.dialog.PopupPCTalkDlg();
	CHECK(g_bPartyRunning);
	// Closed, the party window comes back once.
	calls.clear();
	scene.dialog.ClosePCTalkDlg();
	CHECK(Same({"delete 1", "party"}));
	CHECK(!g_bPartyRunning);
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	// Already remembered, it stays so even with the window gone.
	partyUp = false;
	g_bPartyRunning = true;
	scene.dialog.PopupPCTalkDlg();
	CHECK(g_bPartyRunning);
	g_bPartyRunning = false;
	scene.dialog.PopupPCTalkDlg();
	CHECK(!g_bPartyRunning);
	calls.clear();
	scene.dialog.ClosePCTalkDlg();
	CHECK(Same({"delete 3"}));
	// Closing nothing does nothing.
	calls.clear();
	g_bPartyRunning = true;
	scene.dialog.ClosePCTalkDlg();
	CHECK(Same({}));
	CHECK(g_bPartyRunning);
}

TEST(UIDialog, TheTalkBoxHidesAndShows)
{
	Scene scene;
	scene.dialog.HidePCTalkDlg();
	scene.dialog.ShowPCTalkDlg();
	CHECK(Same({}));
	scene.talkBox.AddString("Buy");
	scene.dialog.PopupPCTalkDlg();
	calls.clear();
	scene.dialog.HidePCTalkDlg();
	scene.dialog.ShowPCTalkDlg();
	CHECK(Same({"finish 1", "start 1"}));
}

TEST(UIDialog, ReleaseFreesBothWindowsAndTheLock)
{
	Scene scene;
	scene.talkBox.AddString("Buy");
	scene.dialog.PopupPCTalkDlg();
	scene.dialog.PopupFreeMessageDlg("Hi");
	calls.clear();
	scene.dialog.Release();
	CHECK(Same({"delete 1", "delete 2"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	scene.dialog.Init();
}

//----------------------------------------------------------------------
// Answering
//----------------------------------------------------------------------
TEST(UIDialog, APlainAnswerIsSentAndAwaited)
{
	Scene scene;
	scene.Talk(PCTalkBox::NORMAL, {"Buy", "Sell"});
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 2);
	CHECK(Same({"answer|300|40|2", "verify|" + N(PlayerState::WAIT_VERIFY_NPC_ASK)}));
	CHECK_EQ(2u, scene.talkBox.GetAnswerID());
	// A menu line maps to the answer the server listed it for.
	calls.clear();
	scene.talkBox.m_AnswerIDMap = {4, 6};
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 2);
	CHECK(Same({"answer|300|40|7", "verify|" + N(PlayerState::WAIT_VERIFY_NPC_ASK)}));
}

TEST(UIDialog, AnAnswerWaitsForTheGameTheServerAndOtherWindows)
{
	Scene scene;
	scene.Talk(PCTalkBox::NORMAL, {"Buy"});
	g_Mode = MODE_WAIT_UPDATEINFO;
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 1);
	g_Mode = MODE_GAME;
	waiting = PlayerState::WAIT_VERIFY_TRADE;
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 1);
	waiting = PlayerState::WAIT_VERIFY_NULL;
	bool* windowsUp[] = {&storage, &exchange, &shop, &bookcase, &repair, &sell, &swapping};
	for (bool* pUp : windowsUp)
	{
		*pUp = true;
		UIDialog::ProcessPCTalkDlg(scene.Window(1), 1);
		*pUp = false;
	}
	CHECK(Same({}));
	// The party window does not count.
	partyUp = true;
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 1);
	CHECK_EQ(2u, calls.size());
}

TEST(UIDialog, LeavingTheTalkFreesThePlayer)
{
	Scene scene;
	scene.Talk(PCTalkBox::NORMAL, {"Buy"});
	g_bPartyRunning = true;
	scene.temp.SetMode(TempInformation::MODE_SKILL_LEARN);
	UIDialog::UnSetLockInputPCTalk();
	UIDialog::SetLockInputPCTalk();
	g_Mode = MODE_WAIT_UPDATEINFO;
	UIDialog::ProcessPCTalkDlg(scene.Window(1), DialogWindow::EXECID_EXIT);
	CHECK(Same({"verify null", "party"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(!g_bPartyRunning);
	// In the game, a plain talk sends the leaving as an answer first.
	// Recorded, not changed: the answer is a byte, so the exit ID goes as
	// answer 0.
	calls.clear();
	g_Mode = MODE_GAME;
	UIDialog::ProcessPCTalkDlg(scene.Window(1), DialogWindow::EXECID_EXIT);
	CHECK(Same({"answer|300|40|0", "verify|" + N(PlayerState::WAIT_VERIFY_NPC_ASK), "verify null"}));
}

TEST(UIDialog, LearningASkillSendsItWithTheDomain)
{
	Scene scene;
	scene.Talk(PCTalkBox::SKILL_LEARN, {"012Fire Ball"});
	scene.temp.SetMode(TempInformation::MODE_SKILL_LEARN);
	scene.temp.Value1 = 3;
	UIDialog::SetLockInputPCTalk();
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 12);
	CHECK(Same({"learn|12|3", "run 1|" + Exit, "verify null"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	// Not learning: nothing.
	calls.clear();
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 12);
	CHECK(Same({}));
	// 999 leaves.
	UIDialog::SetLockInputPCTalk();
	scene.temp.SetMode(TempInformation::MODE_SKILL_LEARN);
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 999);
	CHECK(Same({"run 1|" + Exit, "verify null"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(UIDialog, SelectingAQuestSendsItForTheNPC)
{
	Scene scene;
	scene.Talk(PCTalkBox::SELECT_QUEST, {"0042Find the ring"});
	scene.temp.SetMode(TempInformation::MODE_SELECT_QUEST);
	UIDialog::SetLockInputPCTalk();
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 42);
	CHECK(Same({"quest|42|300", "run 1|" + Exit, "verify null"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_SKILL_LEARN);
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 42);
	CHECK(Same({}));
	// 9999 leaves.
	UIDialog::SetLockInputPCTalk();
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 9999);
	CHECK(Same({"run 1|" + Exit, "verify null"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(UIDialog, SigningABloodBibleKeepsTheTalkOpen)
{
	Scene scene;
	scene.Talk(PCTalkBox::BLOOD_BIBLE_SIGN, {"005Armega"});
	scene.temp.SetMode(TempInformation::MODE_SKILL_LEARN);
	UIDialog::SetLockInputPCTalk();
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 5);
	CHECK(Same({"bible|5"}));
	CHECK_EQ(FLAG_UIDIALOG_LOCK_PCTALK, int(UIDialog::IsLockInput()));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_SKILL_LEARN);
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_SELECT_QUEST);
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 5);
	CHECK(Same({}));
	UIDialog::ProcessPCTalkDlg(scene.Window(1), 999);
	CHECK(Same({"run 1|" + Exit, "verify null"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(UIDialog, WithoutAWindowHostNothingOpens)
{
	Scene scene;
	DialogWindow::SetHost(nullptr);
	CHECK(DialogWindow::NewTalk(7, "Merchant") == nullptr);
	CHECK(DialogWindow::NewMessage(-1, -1, 0, 1) == nullptr);
	DialogWindow::SetMenu(nullptr, nullptr, 0);
	DialogWindow::SetMessage(nullptr, nullptr, 0, false);
	DialogWindow::Start(nullptr);
	DialogWindow::Finish(nullptr);
	DialogWindow::Run(nullptr, 1);
	DialogWindow::Delete(nullptr);
	DialogPresentation::SetHost(nullptr);
	CHECK(!DialogPresentation::IsRunningStorage());
	CHECK(!DialogPresentation::IsRunningShop());
	CHECK(!DialogPresentation::IsRunningBookcase());
	CHECK(!DialogPresentation::IsRunningRepair());
	CHECK(!DialogPresentation::IsRunningSell());
	CHECK(!DialogPresentation::IsRunningParty());
	DialogPresentation::CloseAllDialog();
	CHECK(Same({}));
	DialogPresentation::SetHost(&dialogHost);
	DialogWindow::SetHost(&windowHost);
	// With them, each answers.
	storage = shop = bookcase = repair = sell = partyUp = true;
	CHECK(DialogPresentation::IsRunningStorage());
	CHECK(DialogPresentation::IsRunningShop());
	CHECK(DialogPresentation::IsRunningBookcase());
	CHECK(DialogPresentation::IsRunningRepair());
	CHECK(DialogPresentation::IsRunningSell());
	CHECK(DialogPresentation::IsRunningParty());
}

TEST(UIDialog, LeavingASkillQuestOrBibleTalkSendsNothing)
{
	// Leaving is no choice, even while one is awaited.
	Scene scene;
	const std::pair<PCTalkBox::TALKBOX_TYPE, TempInformation::TEMP_MODE> talks[] = {
		{PCTalkBox::SKILL_LEARN, TempInformation::MODE_SKILL_LEARN},
		{PCTalkBox::SELECT_QUEST, TempInformation::MODE_SELECT_QUEST},
		{PCTalkBox::BLOOD_BIBLE_SIGN, TempInformation::MODE_SKILL_LEARN},
	};
	for (const auto& talk : talks)
	{
		scene.Talk(talk.first, {"0042Line"});
		scene.temp.SetMode(talk.second);
		UIDialog::SetLockInputPCTalk();
		UIDialog::ProcessPCTalkDlg(scene.Window(int(windows.size())), DialogWindow::EXECID_EXIT);
		CHECK(Same({"verify null"}));
		CHECK_EQ(0, int(UIDialog::IsLockInput()));
		CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	}
}
