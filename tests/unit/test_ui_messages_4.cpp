#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "UIMessageManager.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MHelicopterManager.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectStatusTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MStorage.h"
#include "MTradeManager.h"
#include "MMoneyManager.h"
#include "MParty.h"
#include "MChatManager.h"
#include "MEventManager.h"
#include "ModifyStatusManager.h"
#include "ShrineInfoManager.h"
#include "TempInformation.h"
#include "TalkBox.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "CServerInformation.h"
#include "UserOption.h"
#include "Properties.h"
#include "GameSocket.h"
#include "ClientPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptOutputStream.h"
#include "Packet/WireHost.h"
#include "VS_UI.h"
#include "VS_UI_item.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_ExtraDialog.h"
#include "VS_UI_GlobalResource.h"
#include "pi_core.h"
#include "Ci.h"
#include "VS_UI_Base.h"
#include "timer2.h"
#include "Fl2.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"
#include "u_window.h"
#include "Cpackets/CGAcceptUnion.h"
#include "Cpackets/CGAddItemToCodeSheet.h"
#include "Cpackets/CGAddItemToItem.h"
#include "Cpackets/CGAddSMSAddress.h"
#include "Cpackets/CGAppointSubmaster.h"
#include "Cpackets/CGBuyStoreItem.h"
#include "Cpackets/CGDeleteSMSAddress.h"
#include "Cpackets/CGDenyUnion.h"
#include "Cpackets/CGDepositPet.h"
#include "Cpackets/CGDisplayItem.h"
#include "Cpackets/CGDonationMoney.h"
#include "Cpackets/CGDownSkill.h"
#include "Cpackets/CGExpelGuild.h"
#include "Cpackets/CGFailQuest.h"
#include "Cpackets/CGGetEventItem.h"
#include "Cpackets/CGGQuestAccept.h"
#include "Cpackets/CGGQuestCancel.h"
#include "Cpackets/CGJoinGuild.h"
#include "Cpackets/CGLearnSkill.h"
#include "Cpackets/CGLotterySelect.h"
#include "Cpackets/CGMixItem.h"
#include "Cpackets/CGModifyGuildIntro.h"
#include "Cpackets/CGModifyGuildMember.h"
#include "Cpackets/CGModifyGuildMemberIntro.h"
#include "Cpackets/CGModifyNickname.h"
#include "Cpackets/CGModifyTaxRatio.h"
#include "Cpackets/CGPartyInvite.h"
#include "Cpackets/CGPartyLeave.h"
#include "Cpackets/CGPetGamble.h"
#include "Cpackets/CGQuitUnion.h"
#include "Cpackets/CGQuitUnionAccept.h"
#include "Cpackets/CGQuitUnionDeny.h"
#include "Cpackets/CGRegistGuild.h"
#include "Cpackets/CGRequestGuildList.h"
#include "Cpackets/CGRequestGuildMemberList.h"
#include "Cpackets/CGRequestPowerPoint.h"
#include "Cpackets/CGRequestStoreInfo.h"
#include "Cpackets/CGRequestUnion.h"
#include "Cpackets/CGRequestUnionInfo.h"
#include "Cpackets/CGResurrect.h"
#include "Cpackets/CGSay.h"
#include "Cpackets/CGSelectGuild.h"
#include "Cpackets/CGSelectGuildMember.h"
#include "Cpackets/CGSelectNickname.h"
#include "Cpackets/CGSelectRankBonus.h"
#include "Cpackets/CGSelectRegenZone.h"
#include "Cpackets/CGSelectWayPoint.h"
#include "Cpackets/CGShopRequestSell.h"
#include "Cpackets/CGSkillToNamed.h"
#include "Cpackets/CGSkillToSelf.h"
#include "Cpackets/CGSMSAddressList.h"
#include "Cpackets/CGSMSSend.h"
#include "Cpackets/CGStoreClose.h"
#include "Cpackets/CGStoreOpen.h"
#include "Cpackets/CGStoreSign.h"
#include "Cpackets/CGSubmitScore.h"
#include "Cpackets/CGTakeOutGood.h"
#include "Cpackets/CGTryJoinGuild.h"
#include "Cpackets/CGTypeStringList.h"
#include "Cpackets/CGUndisplayItem.h"
#include "Cpackets/CGUseItemFromGQuestInventory.h"
#include "Cpackets/CGUseItemFromInventory.h"
#include "Cpackets/CGUseMessageItemFromInventory.h"
#include "Cpackets/CGUsePowerPoint.h"
#include "Cpackets/CGWithdrawPet.h"
#include "Cpackets/CGWithdrawTax.h"
#include "Cpackets/CLSelectServer.h"
#include "Cpackets/CLSelectWorld.h"
#include "Cpackets/CLQueryCharacterName.h"
#include "Gpackets/GCAddStoreItem.h"
#include "Gpackets/GCFriendChatting.h"
#include "Gpackets/GCMiniGameScores.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "Gpackets/GCOtherStoreInfo.h"
#include "Gpackets/GCRemoveStoreItem.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <list>
#include <memory>
#include <string>
#include <vector>

// UIMessageManager.cpp's (no header declares it).
extern bool g_bEventQuestEnding;
// UIDialog.cpp's.
extern bool g_bPartyRunning;

// The UI's results, their fourth part (UIMessageManager.cpp from
// Execute_UI_CLOSE_SLAYER_PORTAL to the end, gameclient's since task
// 4.258): the way points, the party, death, the server choice, the guild
// and its union, the friends, the enchant and the mixing forge, the rank
// bonus, the Christmas tree, the tax, the couple, the lottery, the mini
// games, the regen towers, the pets, SMS, the nicknames, the quests, the
// personal store, the power points and the event items. Each message goes
// through Execute as the UI's callback sends it, in a game whose
// connection serialises what it is asked to send, so a packet is pinned
// by its bytes against the same packet built here with the expected
// fields. gC_vs_ui is never initialised, so its window calls do nothing:
// a handler whose only effect is a window is only shown to be harmless.
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

typedef std::vector<unsigned char> Bytes;

std::string Hex(const Bytes& bytes)
{
	static const char digits[] = "0123456789abcdef";
	std::string out;
	for (unsigned char b : bytes) { out += digits[b >> 4]; out += digits[b & 15]; }
	return out;
}

// The packets as one fresh connection writes them (the client's stream,
// with no encryption set).
Bytes Encoded(std::initializer_list<const Packet*> packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream encoded(&out, 32768);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

template <class P, class F>
Bytes One(F set)
{
	P packet;
	set(packet);
	return Encoded({&packet});
}

template <class P>
Bytes One()
{
	P packet;
	return Encoded({&packet});
}

bool SameBytes(const Bytes& expected, const Bytes& sent)
{
	if (expected == sent) return true;
	std::printf("      sent:   %s\n      wanted: %s\n", Hex(sent).c_str(), Hex(expected).c_str());
	return false;
}

// The game connection: what it is asked to send is written to its
// output, as on the wire, and kept - but for five packets whose write()
// reads what the handler never set, which are named in the calls instead:
// the two guild requests' guild id and Eternity's effect id, which no
// constructor initialises (writing them reads an indeterminate value),
// and the two store infos' StoreInfo, NULL (writing it dereferences NULL).
struct Line : ClientPlayer
{
	Line() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Line() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override
	{
		switch (pPacket->getPacketID())
		{
		case Packet::PACKET_CG_REQUEST_GUILD_MEMBER_LIST:
			calls.push_back("guild member list");
			return;
		case Packet::PACKET_CG_SELECT_GUILD_MEMBER:
			calls.push_back("select guild member|" + static_cast<CGSelectGuildMember*>(pPacket)->getName());
			return;
		case Packet::PACKET_CG_SKILL_TO_SELF:
			calls.push_back("skill to self|" + N(static_cast<CGSkillToSelf*>(pPacket)->getSkillType()));
			return;
		case Packet::PACKET_GC_MY_STORE_INFO:
			calls.push_back(std::string("my store info|") + (static_cast<GCMyStoreInfo*>(pPacket)->getStoreInfo() == NULL ? "no store" : "store"));
			return;
		case Packet::PACKET_GC_OTHER_STORE_INFO:
			calls.push_back("other store info|" + N(static_cast<GCOtherStoreInfo*>(pPacket)->getObjectID()));
			return;
		}
		ClientPlayer::sendPacket(pPacket);
	}
	Bytes Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

// The dialogs: the NPC talk box (kTalk, shown and closed) and the
// message boxes, whose text is kept.
DialogWindow::Window* const kTalk = reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(7));
int messageBoxes = 100;
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewMessage = [](int, int, int, std::uint16_t) {
		return reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(++messageBoxes));
	};
	host.SetMessage = [](DialogWindow::Window*, const char* const* pLines, int count, bool) {
		std::string line = "popup";
		for (int i = 0; i < count; ++i) line += std::string("|") + pLines[i];
		calls.push_back(line);
	};
	host.Start = [](DialogWindow::Window* pWindow) { if (pWindow == kTalk) calls.push_back("talk shown"); };
	host.Delete = [](DialogWindow::Window* pWindow) { if (pWindow == kTalk) calls.push_back("talk closed"); };
	return host;
}();

struct TalkDialog : UIDialog
{
	void Talk() { m_pPCTalkDlg = kTalk; }
	~TalkDialog() { m_pPCTalkDlg = NULL; }
};

// What the view has selected, and the modes entered.
TYPE_OBJECTID selected = OBJECTID_NULL;
const UIMessages::Host messagesHost = []() {
	UIMessages::Host host{};
	host.SetMode = [](CLIENT_MODE mode) { calls.push_back("mode|" + N(mode)); };
	host.GetSelectedCreature = []() { return selected; };
	return host;
}();

// The helicopters: their shadow is deleted as the zone takes it, so one
// sent away goes at once and its propeller stops.
ZONESOUND_NODE* const kPropeller = reinterpret_cast<ZONESOUND_NODE*>(static_cast<std::uintptr_t>(9));
const MHelicopterHost helicopterHost = []() {
	MHelicopterHost host{};
	host.GetCreaturePosition = [](TYPE_OBJECTID, int& x, int& y) { x = 3; y = 4; return true; };
	host.AddEffect = [](MEffect* pEffect) { delete pEffect; };
	host.GetEffect = [](TYPE_OBJECTID) { return static_cast<MEffect*>(NULL); };
	host.RemoveEffect = [](TYPE_OBJECTID) {};
	host.GetMaxEffectFrame = [](BLT_TYPE, TYPE_FRAMEID) { return 1; };
	host.PlayLoop = [](TYPE_SOUNDID, int, int) { return kPropeller; };
	host.DeleteSound = [](ZONESOUND_NODE* pSound) { calls.push_back(pSound == kPropeller ? "propeller off" : "sound off"); };
	return host;
}();

struct PlayerProbe : MPlayer
{
	using MPlayer::m_bAlive;
	using MPlayer::m_MoveDevice;
};

// An item whose quick use is recorded.
struct QuickHelm : MHelm
{
	void UseQuickItem() override { calls.push_back("use quick|" + N(GetID())); }
};

// The creature types: one of each race.
enum { kSlayer = 0, kVampire = 1, kOusters = 2 };
const TYPE_OBJECTID kMe = 77;
const unsigned int kNPC = 4321;
const TYPE_ITEM_OPTION kOption = 1;

// The game, with every pointer Execute checks in it, the player a live
// slayer of guild 321 and its guild's master, nothing waiting, and the
// dialogs, the party, the money, the events and the regen towers the
// handlers reach. Everything goes back as it was.
struct Scene
{
	GameModelWorld world{3};
	CREATURE_TABLE creatures;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MActionInfoTable actions;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MSkillInfoTable skillInfo;
	TempInformation temp;
	MInventory inventory;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MParty party;
	CMessageArray gameMessages;
	MMoneyManager money;
	MEventQueue events;
	MChatManager chat;
	PCTalkBox talkBox;
	RegenTowerInfoManager towers;
	TalkDialog dialog;
	C_VS_UI_ITEM uiItem;
	C_VS_UI_MOUSE_POINTER mouse;
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	TempInformation* oldTemp = g_pTempInformation;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MParty* oldParty = g_pParty;
	CMessageArray* oldGameMessages = g_pGameMessage;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MEventQueue* oldEvents = g_pEventManager;
	MChatManager* oldChat = g_pChatManager;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	RegenTowerInfoManager* oldTowers = g_pRegenTowerInfoManager;
	UIDialog* oldDialog = g_pUIDialog;
	MStorage* oldStorage = g_pStorage;
	MStorage* oldStorage2 = g_pStorage2;
	MTradeManager* oldTrade = g_pTradeManager;
	CServerInformation* oldServers = g_pServerInformation;
	C_VS_UI_ITEM* oldUiItem = gpC_item;
	C_VS_UI_MOUSE_POINTER* oldMouse = gpC_mouse_pointer;
	ClientPlayer* oldSocket = g_pSocket;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	CLIENT_MODE oldMode = g_Mode;
	bool oldPartyRunning = g_bPartyRunning;
	bool oldQuestEnding = g_bEventQuestEnding;
	DWORD oldLock = UIDialog::IsLockInput();
	int oldRefPoints[ITEM_REF_POINT_COUNT][2];
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	const UIMessages::Host* oldMessages = UIMessages::SetHost(&messagesHost);
	const MHelicopterHost* oldHelicopters = MHelicopter::SetHost(&helicopterHost);
	std::unique_ptr<MZone> zone;
	std::unique_ptr<Line> line;
	PlayerProbe* me = nullptr;
	UIMessageManager manager;

	Scene()
	{
		calls.clear();
		selected = OBJECTID_NULL;
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) { oldRefPoints[i][0] = g_item_ref_point[i].x; oldRefPoints[i][1] = g_item_ref_point[i].y; }
		creatures.Init(3);
		testfw::MutableRow(creatures, kSlayer).SetCreatureTribe(CREATURETRIBE_SLAYER, 0);
		testfw::MutableRow(creatures, kVampire).SetCreatureTribe(CREATURETRIBE_VAMPIRE, 0);
		testfw::MutableRow(creatures, kOusters).SetCreatureTribe(CREATURETRIBE_OUSTERS, 0);
		// No dying motion (SetDead would act it out of the empty action table).
		for (int type : {kSlayer, kVampire, kOusters}) testfw::MutableRow(creatures, type).DeadActionInfo = ACTIONINFO_NULL;
		g_pCreatureTable = &creatures;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		rankBonuses.Init(3);
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		g_pTempInformation = &temp;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		vampireGear.Init();
		g_pVampireGear = &vampireGear;
		oustersGear.Init();
		g_pOustersGear = &oustersGear;
		g_pParty = &party;
		gameMessages.Init(3, 128);
		g_pGameMessage = &gameMessages;
		money.SetMoney(25000);
		g_pMoneyManager = &money;
		g_pEventManager = &events;
		g_pChatManager = &chat;
		talkBox.SetNPCID(kNPC);
		g_pPCTalkBox = &talkBox;
		towers.Init(3);
		g_pRegenTowerInfoManager = &towers;
		InitGameStringTable();
		dialog.Init();
		g_pUIDialog = &dialog;
		g_pStorage = NULL;
		g_pStorage2 = NULL;
		g_pTradeManager = NULL;
		g_pServerInformation = NULL;
		gpC_item = &uiItem;
		gpC_mouse_pointer = &mouse;
		g_Mode = MODE_GAME;
		g_bPartyRunning = false;
		g_bEventQuestEnding = false;
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		g_pUserInformation->GuildGrade = 1;
		g_pUserInformation->dwUnionID = 0;
		g_pUserInformation->bUnionGrade = 0;
		zone.reset(new MZone(20, 20));
		g_pZone = zone.get();
		me = new PlayerProbe;
		g_pPlayer = me;
		me->SetCreatureType(kSlayer);
		me->SetID(kMe);
		me->SetGuildNumber(321);
		Fresh();
		manager.Init();
	}
	~Scene()
	{
		mouse.DropItem();
		delete me;	// clears g_pPlayer
		g_pPlayer = oldPlayer;
		zone.reset();
		g_pZone = oldZone;
		line.reset();
		g_pSocket = oldSocket;
		delete g_pStorage;
		g_pStorage = oldStorage;
		delete g_pStorage2;
		g_pStorage2 = oldStorage2;
		g_pTradeManager = oldTrade;
		g_pServerInformation = oldServers;
		inventory.Release();
		slayerGear.Release();
		vampireGear.Release();
		oustersGear.Release();
		dialog.Release();
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		if (oldLock & FLAG_UIDIALOG_LOCK_PCTALK) UIDialog::SetLockInputPCTalk();
		if (oldLock & FLAG_UIDIALOG_LOCK_MESSAGE) UIDialog::SetLockInputMessage();
		MHelicopter::SetHost(oldHelicopters);
		UIMessages::SetHost(oldMessages);
		DialogWindow::SetHost(oldWindows);
		g_bEventQuestEnding = oldQuestEnding;
		g_bPartyRunning = oldPartyRunning;
		g_Mode = oldMode;
		gpC_mouse_pointer = oldMouse;
		gpC_item = oldUiItem;
		g_pUIDialog = oldDialog;
		g_pRegenTowerInfoManager = oldTowers;
		g_pPCTalkBox = oldTalkBox;
		g_pChatManager = oldChat;
		g_pEventManager = oldEvents;
		g_pMoneyManager = oldMoney;
		g_pGameMessage = oldGameMessages;
		g_pParty = oldParty;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pTempInformation = oldTemp;
		g_pSkillInfoTable = oldSkillInfo;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pActionInfoTable = oldActions;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatures;
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) g_item_ref_point[i].Set(oldRefPoints[i][0], oldRefPoints[i][1]);
		calls.clear();
	}

	void Run(DWORD message, intptr_t left = 0, intptr_t right = 0, void* ptr = NULL) { manager.Execute(message, left, right, ptr); }
	// A new connection, so the next packet is the first on it.
	void Fresh() { line.reset(new Line); g_pSocket = line.get(); }
	Bytes Sent() const { return line->Sent(); }
	bool Nothing() const { return line->Sent().empty(); }
	// A short game string, so its message box keeps it on one line.
	void Say(int id, const char* text) { g_pGameStringTable->Set(id, text); }

	// An item class of that many types, sized and for those races.
	void Class(int itemClass, int types, int width, int height, int race)
	{
		g_pItemTable->InitClass(itemClass, types);
		for (int t = 0; t < types; ++t)
		{
			testfw::MutableRow(*g_pItemTable, itemClass, t).SetGrid(BYTE(width), BYTE(height));
			testfw::MutableRow(*g_pItemTable, itemClass, t).Race = BYTE(race);
		}
	}
	// The item, of that type and id, in the inventory at (x, y).
	template <class Item>
	Item* Carried(TYPE_ITEMTYPE type, TYPE_OBJECTID id, BYTE x, BYTE y)
	{
		Item* pItem = new Item;
		pItem->SetItemType(type);
		pItem->SetID(id);
		const bool added = inventory.AddItem(pItem, x, y);
		CHECK(added);
		return pItem;
	}
	bool Quiet() const
	{
		return temp.GetMode() == TempInformation::MODE_NULL && me->IsWaitVerifyNULL() && me->IsItemCheckBufferNULL();
	}
};

const int kRace = FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS;

} // namespace

//----------------------------------------------------------------------
// The plain senders: one packet, every field from the arguments or the
// player, no change to the game. A NULL string sends nothing; the guild
// registrations without their text open the refusal's message box; the
// union requests a non-master or a member makes open theirs.
//----------------------------------------------------------------------
TEST(UIMessages4Senders, EachSendsItsPacketWithEveryField)
{
	Scene scene;
	scene.Say(STRING_MESSAGE_GUILD_REGIST_FAIL_INTRO, "No intro.");
	char name[] = "Kim";
	char team[] = "Wolves";
	char intro[] = "We hunt.";
	char cell[] = "0101234";
	char sign[] = "Cheap swords";
	struct Row
	{
		const char* what;
		DWORD message;
		intptr_t left, right;
		void* ptr;
		Bytes packet;
		std::vector<std::string> calls;
	};
	const Row rows[] = {
		{"guild info", UI_REQUEST_GUILD_INFO, 9, 9, NULL, One<CGSelectGuild>([](CGSelectGuild& p) { p.setGuildID(321); }), {}},
		// Recorded defect: the guild id is never set (L8814, L8843), and no
		// constructor sets it, so two indeterminate bytes go out with it.
		{"guild members", UI_REQUEST_GUILD_MEMBER_LIST, 9, 9, NULL, {}, {"guild member list"}},
		{"modify member", UI_MODIFY_GUILD_MEMBER, 0x12345, 0x1FF, name, One<CGModifyGuildMember>([](CGModifyGuildMember& p) { p.setGuildID(0x2345); p.setGuildMemberRank(0xFF); p.setName("Kim"); }), {}},
		{"select member", UI_SELECT_TEAM_MEMBER_LIST, 9, 9, name, {}, {"select guild member|Kim"}},
		{"select member, none", UI_SELECT_TEAM_MEMBER_LIST, 9, 9, NULL, {}, {}},
		{"select ready team", UI_SELECT_READY_TEAM_LIST, 0x10007, 9, team, One<CGSelectGuild>([](CGSelectGuild& p) { p.setGuildID(7); }), {}},
		{"select ready team, none", UI_SELECT_READY_TEAM_LIST, 7, 9, NULL, {}, {}},
		{"select regist team", UI_SELECT_REGIST_TEAM_LIST, 0x10008, 9, team, One<CGSelectGuild>([](CGSelectGuild& p) { p.setGuildID(8); }), {}},
		{"select regist team, none", UI_SELECT_REGIST_TEAM_LIST, 8, 9, NULL, {}, {}},
		{"friends update", UI_FRIEND_CHATTING_UPDATE, 9, 9, NULL, One<GCFriendChatting>([](GCFriendChatting& p) { p.setCommand(CG_UPDATE); }), {}},
		{"join ready team", UI_JOIN_READY_TEAM, 0x10009, 9, team, One<CGTryJoinGuild>([](CGTryJoinGuild& p) { p.setGuildID(9); p.setGuildMemberRank(2); }), {}},
		{"join regist team", UI_JOIN_REGIST_TEAM, 10, 9, team, One<CGTryJoinGuild>([](CGTryJoinGuild& p) { p.setGuildID(10); p.setGuildMemberRank(3); }), {}},
		{"regist member", UI_REGIST_GUILD_MEMBER, 0x1000B, 0x105, intro, One<CGJoinGuild>([](CGJoinGuild& p) { p.setGuildID(11); p.setGuildMemberRank(5); p.setGuildMemberIntro("We hunt."); }), {}},
		{"regist member, no intro", UI_REGIST_GUILD_MEMBER, 11, 5, NULL, {}, {"popup|No intro."}},
		{"regist team", UI_REGIST_GUILD_TEAM, reinterpret_cast<intptr_t>(team), 9, intro, One<CGRegistGuild>([](CGRegistGuild& p) { p.setGuildName("Wolves"); p.setGuildIntro("We hunt."); }), {}},
		{"regist team, no intro", UI_REGIST_GUILD_TEAM, reinterpret_cast<intptr_t>(team), 9, NULL, {}, {"popup|No intro."}},
		{"regist team, no name", UI_REGIST_GUILD_TEAM, 0, 9, intro, {}, {"popup|No intro."}},
		{"team intro", UI_MODIFY_TEAM_INFO, 0x1000C, 9, intro, One<CGModifyGuildIntro>([](CGModifyGuildIntro& p) { p.setGuildID(12); p.setGuildIntro("We hunt."); }), {}},
		{"team intro, none", UI_MODIFY_TEAM_INFO, 12, 9, NULL, {}, {}},
		{"member intro", UI_MODIFY_GUILD_MEMBER_INTRO, 0x1000D, 9, intro, One<CGModifyGuildMemberIntro>([](CGModifyGuildMemberIntro& p) { p.setGuildID(13); p.setGuildMemberIntro("We hunt."); }), {}},
		{"member intro, none", UI_MODIFY_GUILD_MEMBER_INTRO, 13, 9, NULL, {}, {}},
		{"lottery card", UI_LOTTERY_CARD_STATUS, 3, 0x12345678, NULL, One<CGLotterySelect>([](CGLotterySelect& p) { p.setType(TYPE_SELECT_LOTTERY); p.setQuestLevel(2); p.setGiftID(0x12345678); }), {}},
		// Recorded defect: step 0 is level 0 - 1, 0xFFFFFFFF on the wire (L9554).
		{"lottery card, step 0", UI_LOTTERY_CARD_STATUS, 0, 4, NULL, One<CGLotterySelect>([](CGLotterySelect& p) { p.setType(TYPE_SELECT_LOTTERY); p.setQuestLevel(0xFFFFFFFF); p.setGiftID(4); }), {}},
		{"scratch finished", UI_FINISH_SCRATCH_LOTTERY, 9, 9, NULL, One<CGLotterySelect>([](CGLotterySelect& p) { p.setType(TYPE_FINISH_SCRATCH); p.setQuestLevel(0); p.setGiftID(0); }), {}},
		{"take out good", UI_SELECT_ITEM_FROM_SHOP, 0x87654321, 9, NULL, One<CGTakeOutGood>([](CGTakeOutGood& p) { p.setObjectID(0x87654321); }), {}},
		{"crazy mine closed", UI_CLOSE_CRAZY_MINE, 9, 9, NULL, One<CGFailQuest>([](CGFailQuest& p) { p.setFail(false); }), {}},
		{"arrow tile closed", UI_CLOSE_ARROW_TILE, 9, 9, NULL, One<CGFailQuest>([](CGFailQuest& p) { p.setFail(false); }), {}},
		{"arrow stage", UI_CLEAR_STAGE, 0x0103, 12345, NULL, One<CGSubmitScore>([](CGSubmitScore& p) { p.setGameType(GAME_ARROW); p.setLevel(3); p.setScore(1234); }), {}},
		{"mine stage", UI_CLEAR_STAGE, (1 << 16) | 7, 655359, NULL, One<CGSubmitScore>([](CGSubmitScore& p) { p.setGameType(GAME_MINE); p.setLevel(7); p.setScore(65535); }), {}},
		{"mine stage, capped", UI_CLEAR_STAGE, (5 << 16) | 2, 655369, NULL, One<CGSubmitScore>([](CGSubmitScore& p) { p.setGameType(GAME_MINE); p.setLevel(2); p.setScore(65535); }), {}},
		{"popup ok 1", UI_POPUP_MESSAGE_OK, 1, 9, NULL, One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(1122); p.setX(0); p.setY(0); }), {}},
		{"popup ok 2", UI_POPUP_MESSAGE_OK, 2, 9, NULL, One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(1122); p.setX(0); p.setY(0); }), {}},
		{"popup ok 4", UI_POPUP_MESSAGE_OK, 4, 9, NULL, One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(1131); p.setX(0); p.setY(0); }), {}},
		{"popup ok 6", UI_POPUP_MESSAGE_OK, 6, 9, NULL, One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(72); p.setX(0); p.setY(0); }), {}},
		// Recorded defect: any other popup's OK, the welcome popup's
		// default id among them, asks for zone 0 (L10107).
		{"popup ok 3", UI_POPUP_MESSAGE_OK, 3, 9, NULL, One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(0); p.setX(0); p.setY(0); }), {}},
		{"popup ok default", UI_POPUP_MESSAGE_OK, 0xFFFFFFFF, 9, NULL, One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(0); p.setX(0); p.setY(0); }), {}},
		{"pet gamble", UI_PET_GAMBLE, 9, 9, NULL, One<CGPetGamble>(), {}},
		{"union guild list", UI_REQUEST_UNION_REQUEST_GUILD_LIST, 0x102, 9, NULL, One<CGRequestGuildList>([](CGRequestGuildList& p) { p.setGuildType(2); }), {}},
		{"union info", UI_REQUEST_UNION_REQUEST_INFO, 9, 9, NULL, One<CGRequestUnionInfo>(), {}},
		{"union accept", UI_REQUEST_UNION_ACCEPT, 0x10014, 9, NULL, One<CGAcceptUnion>([](CGAcceptUnion& p) { p.setGuildID(0x14); }), {}},
		{"union deny", UI_REQUEST_UNION_DENY, 21, 9, NULL, One<CGDenyUnion>([](CGDenyUnion& p) { p.setGuildID(21); }), {}},
		{"union quit accept", UI_REQUEST_UNION_QUIT_ACCEPT, 22, 9, NULL, One<CGQuitUnionAccept>([](CGQuitUnionAccept& p) { p.setGuildID(22); }), {}},
		{"union quit deny", UI_REQUEST_UNION_QUIT_DENY, 23, 9, NULL, One<CGQuitUnionDeny>([](CGQuitUnionDeny& p) { p.setGuildID(23); }), {}},
		{"union request", UI_REQUEST_UNION, 0x10018, 9, NULL, One<CGRequestUnion>([](CGRequestUnion& p) { p.setGuildID(24); }), {}},
		{"union quit", UI_REQUEST_UNION_QUIT, 25, 0x101, NULL, One<CGQuitUnion>([](CGQuitUnion& p) { p.setGuildID(25); p.setQuitMethod(1); }), {}},
		{"union expel", UI_REQUEST_UNION_EXPERGUILD, 26, 9, NULL, One<CGExpelGuild>([](CGExpelGuild& p) { p.setGuildID(26); }), {}},
		{"tax", UI_MODIFY_TAX, 7, 9, NULL, One<CGModifyTaxRatio>([](CGModifyTaxRatio& p) { p.setRatio(7); }), {}},
		{"submaster", UI_APPOINT_SUBMASTER, 0x1001B, 9, name, One<CGAppointSubmaster>([](CGAppointSubmaster& p) { p.setGuildID(27); p.setName("Kim"); }), {}},
		{"store open", UI_STORE_OPEN, 9, 9, NULL, One<CGStoreOpen>(), {}},
		{"store close", UI_STORE_CLOSE, 9, 9, NULL, One<CGStoreClose>(), {}},
		{"store sign", UI_STORE_SIGN, 9, 9, sign, One<CGStoreSign>([](CGStoreSign& p) { p.setSign("Cheap swords"); }), {}},
		{"store info", UI_REQUEST_STORE_INFO, 0x55, 0, NULL, One<CGRequestStoreInfo>([](CGRequestStoreInfo& p) { p.setOwnerObjectID(0x55); }), {}},
		{"store info, my shop", UI_REQUEST_STORE_INFO, 0x56, 1, NULL, One<CGRequestStoreInfo>([](CGRequestStoreInfo& p) { p.setOwnerObjectID(0x56); }), {}},
		{"remove store item", UI_REMOVE_STORE_ITEM, 9, 9, NULL, One<GCRemoveStoreItem>([](GCRemoveStoreItem& p) { p.setIndex(0); p.setOwnerObjectID(kMe); }), {}},
		{"add store item", UI_ADD_STORE_ITEM, 9, 9, NULL, One<GCAddStoreItem>([](GCAddStoreItem& p) { p.setIndex(0); p.setOwnerObjectID(kMe); }), {}},
		{"power point", UI_REQUEST_POWER_JJANG_POINT, 9, 9, cell, One<CGRequestPowerPoint>([](CGRequestPowerPoint& p) { p.setCellNum("0101234"); }), {}},
		{"power gamble", UI_POWER_JJANG_GAMBLE, 9, 9, NULL, One<CGUsePowerPoint>(), {}},
		{"advance skill", UI_LEARN_ADVANCE_SKILL, 0x10123, 0x105, NULL, One<CGLearnSkill>([](CGLearnSkill& p) { p.setSkillType(0x123); p.setSkillDomainType(5); }), {}},
		{"event item", UI_REQUEST_EVENT_ITEM, 0x103, 9, NULL, One<CGGetEventItem>([](CGGetEventItem& p) { p.setEventType(3); }), {}},
		{"gquest accept", UI_GQUEST_ACCEPT, 77, 9, NULL, One<CGGQuestAccept>([](CGGQuestAccept& p) { p.setQuestID(77); }), {}},
		{"gquest give up", UI_GQUEST_GIVEUP, 78, 9, NULL, One<CGGQuestCancel>([](CGGQuestCancel& p) { p.setQuestID(78); }), {}},
		{"select nickname", UI_SELECT_NAMING, 0x10005, 9, NULL, One<CGSelectNickname>([](CGSelectNickname& p) { p.setNicknameID(5); }), {}},
		{"sms list", UI_SMS_OPEN_LIST, 9, 9, NULL, One<CGSMSAddressList>(), {}},
	};
	for (const Row& row : rows)
	{
		scene.Fresh();
		calls.clear();
		scene.Run(row.message, row.left, row.right, row.ptr);
		if (!SameBytes(row.packet, scene.Sent()) || !Same(row.calls) || !scene.Quiet())
		{
			std::printf("      row: %s\n", row.what);
			CHECK(false);
		}
	}
	// The arguments, borrowed, are left as they were.
	CHECK(std::string(name) == "Kim");
	CHECK(std::string(intro) == "We hunt.");
}

TEST(UIMessages4Union, OnlyAMasterOutsideAUnionAsks)
{
	Scene scene;
	scene.Say(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_1, "Masters only.");
	scene.Say(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_2, "In a union.");
	scene.Say(UI_STRING_MESSAGE_REQUEST_UNION_ERROR_3, "The union's master.");
	// Not the guild's master: no request, no quit, no expulsion.
	g_pUserInformation->GuildGrade = 2;
	for (DWORD message : {UI_REQUEST_UNION, UI_REQUEST_UNION_QUIT, UI_REQUEST_UNION_EXPERGUILD})
	{
		calls.clear();
		scene.Run(message, 24, 1);
		CHECK(Same({"popup|Masters only."}));
	}
	CHECK(scene.Nothing());
	// A master in a union asks for no other, but may quit.
	g_pUserInformation->GuildGrade = 1;
	g_pUserInformation->dwUnionID = 5;
	calls.clear();
	scene.Run(UI_REQUEST_UNION, 24);
	CHECK(Same({"popup|In a union."}));
	CHECK(scene.Nothing());
	// The union's own master expels no guild.
	g_pUserInformation->dwUnionID = 0;
	g_pUserInformation->bUnionGrade = 1;
	calls.clear();
	scene.Run(UI_REQUEST_UNION_EXPERGUILD, 26);
	CHECK(Same({"popup|The union's master."}));
	CHECK(scene.Nothing());
	// The union's ordinary member (grade 1) is not its master either.
	g_pUserInformation->bUnionGrade = 0;
	scene.Run(UI_REQUEST_UNION_EXPERGUILD, 26);
	CHECK(SameBytes(One<CGExpelGuild>([](CGExpelGuild& p) { p.setGuildID(26); }), scene.Sent()));
}

//----------------------------------------------------------------------
// The guards: each of these sends in the game with nothing waiting, and
// nothing outside the game or (where it asks) while the request
// register waits for another answer - which it leaves alone.
//----------------------------------------------------------------------
TEST(UIMessages4Guards, OutsideTheGameOrWhileARequestWaitsNothingIsSent)
{
	Scene scene;
	char caller[] = "0101111";
	char text[] = "Hi";
	char number[] = "0102222";
	char who[] = "Lee";
	char nick[] = "Ace";
	std::list<std::string> to = {"0103333"};
	scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
	MItem* pHelm = scene.Carried<MHelm>(0, 601, 2, 1);
	enum Guard { GAME, WAITING, BOTH };
	struct Row
	{
		const char* what;
		Guard guard;
		DWORD message;
		intptr_t left, right;
		void* ptr;
	};
	const Row rows[] = {
		{"close slayer portal", GAME, UI_CLOSE_SLAYER_PORTAL, 0, 0, NULL},
		{"slayer portal", GAME, UI_SLAYER_PORTAL, 61, 0x00050006, NULL},
		{"party request timed out", GAME, UI_FINISH_REQUEST_PARTY_BUTTON, 1, 0, NULL},
		{"party accept", GAME, UI_PARTY_ACCEPT, 1, 0, NULL},
		{"ousters skill", BOTH, UI_OUSTERS_LEARN_SKILL, 300, 0, NULL},
		{"keep pet item", BOTH, UI_KEEP_PETITEM, 0, 3, pHelm},
		{"get kept pet item", BOTH, UI_GET_KEEP_PETITEM, 0, 3, pHelm},
		{"sms send", BOTH, UI_SEND_SMS_MESSAGE, reinterpret_cast<intptr_t>(caller), reinterpret_cast<intptr_t>(text), &to},
		{"sms list", BOTH, UI_SMS_OPEN_LIST, 0, 0, NULL},
		{"sms record", BOTH, UI_SMS_RECORD, reinterpret_cast<intptr_t>(who), reinterpret_cast<intptr_t>(text), number},
		{"sms delete", BOTH, UI_SMS_DELETE, 4, 0, NULL},
		{"select nickname", BOTH, UI_SELECT_NAMING, 5, 0, NULL},
		{"custom nickname", BOTH, UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(nick), 0, NULL},
		{"gquest accept", BOTH, UI_GQUEST_ACCEPT, 77, 0, NULL},
		{"gquest give up", BOTH, UI_GQUEST_GIVEUP, 78, 0, NULL},
		{"gquest item", GAME, UI_ITEM_USE_GQUEST_ITEM, 2, 0, pHelm},
		{"swap advancement", BOTH, UI_SWAPADVANCEMENTITEM, 2, 1, NULL},
		{"bring fee", WAITING, UI_SEND_BRING_FEE, 1000, 0, NULL},
	};
	for (const Row& row : rows)
	{
		bool ok = true;
		// Out of the game (Execute itself checks nothing there).
		if (row.guard != WAITING)
		{
			scene.Fresh();
			g_Mode = MODE_WAIT_SELECTPC;
			scene.Run(row.message, row.left, row.right, row.ptr);
			ok = ok && scene.Nothing() && scene.Quiet();
			g_Mode = MODE_GAME;
		}
		// Another request waiting.
		if (row.guard != GAME)
		{
			scene.Fresh();
			scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
			scene.Run(row.message, row.left, row.right, row.ptr);
			ok = ok && scene.Nothing() && scene.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST
				&& scene.me->IsWaitVerifyNULL() && scene.me->IsItemCheckBufferNULL();
			scene.temp.SetMode(TempInformation::MODE_NULL);
		}
		// In the game with nothing waiting, it sends.
		scene.Fresh();
		scene.Run(row.message, row.left, row.right, row.ptr);
		ok = ok && !scene.Nothing();
		scene.temp.SetMode(TempInformation::MODE_NULL);
		scene.temp.pValue = NULL;
		scene.me->SetWaitVerifyNULL();
		scene.me->ClearItemCheckBuffer();
		if (!ok)
		{
			std::printf("      row: %s\n", row.what);
			CHECK(false);
		}
	}
	calls.clear();
}

//----------------------------------------------------------------------
// The way points
//----------------------------------------------------------------------
TEST(UIMessages4WayPoint, ClosingThePortalSendsNoWhereAndSendsTheHelicopterAway)
{
	Scene scene;
	CHECK(scene.zone->AddHelicopter(kMe, 1, 2));
	CHECK(scene.zone->AddHelicopter(kMe + 1, 1, 2));
	calls.clear();
	scene.Run(UI_CLOSE_SLAYER_PORTAL, 5, 6);
	CHECK(SameBytes(One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(0); p.setX(0); p.setY(0); }), scene.Sent()));
	// The player's helicopter, not another's.
	CHECK(Same({"propeller off"}));
	CHECK(scene.zone->GetHelicopter(kMe) == NULL);
	CHECK(scene.zone->GetHelicopter(kMe + 1) != NULL);
}

TEST(UIMessages4WayPoint, EveryRaceAsksForThePointAndASlayersHelicopterGoes)
{
	Scene scene;
	const Bytes expected = One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(1003); p.setX(0x12); p.setY(0x34); });
	for (int race : {kSlayer, kVampire, kOusters})
	{
		scene.me->SetCreatureType(race);
		CHECK(scene.zone->AddHelicopter(kMe, 1, 2));
		scene.Fresh();
		calls.clear();
		// The zone, then x and y in the high and low words.
		scene.Run(UI_SLAYER_PORTAL, 1003, (0x12 << 16) | 0x34);
		CHECK(SameBytes(expected, scene.Sent()));
		CHECK(Same(race == kSlayer ? std::vector<std::string>{"propeller off"} : std::vector<std::string>{}));
		CHECK((scene.zone->GetHelicopter(kMe) == NULL) == (race == kSlayer));
		scene.zone->RemoveHelicopter(kMe);
	}
}

TEST(UIMessages4WayPoint, ThePopupsOKAlsoClosesThePopup)
{
	// (The close is a window's; what is pinned is that the way point
	// still goes out once.)
	Scene scene;
	scene.Run(UI_POPUP_MESSAGE_OK, 6);
	CHECK(SameBytes(One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(72); p.setX(0); p.setY(0); }), scene.Sent()));
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// The party
//----------------------------------------------------------------------
TEST(UIMessages4Party, ATimedOutRequestIsRejectedAndAClosedOneIsNot)
{
	Scene scene;
	scene.temp.PartyInviter = 900;
	scene.Run(UI_FINISH_REQUEST_PARTY_BUTTON, 0);
	CHECK(scene.Nothing());
	for (intptr_t timeout : {1, 2, -1})
	{
		scene.Fresh();
		scene.Run(UI_FINISH_REQUEST_PARTY_BUTTON, timeout);
		CHECK(SameBytes(One<CGPartyInvite>([](CGPartyInvite& p) { p.setTargetObjectID(900); p.setCode(CG_PARTY_INVITE_REJECT); }), scene.Sent()));
	}
	CHECK(scene.Quiet());
}

TEST(UIMessages4Party, TheAnswerGoesToTheInviterWhateverIsWaiting)
{
	Scene scene;
	scene.temp.PartyInviter = 901;
	scene.Run(UI_PARTY_ACCEPT, 1);
	CHECK(SameBytes(One<CGPartyInvite>([](CGPartyInvite& p) { p.setTargetObjectID(901); p.setCode(CG_PARTY_INVITE_ACCEPT); }), scene.Sent()));
	scene.Fresh();
	scene.Run(UI_PARTY_ACCEPT, 0);
	CHECK(SameBytes(One<CGPartyInvite>([](CGPartyInvite& p) { p.setTargetObjectID(901); p.setCode(CG_PARTY_INVITE_REJECT); }), scene.Sent()));
	// Recorded defect: with no request pending (its check is commented
	// out, L8244) the answer still goes, to the last inviter, and nothing
	// waiting is touched.
	scene.Fresh();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_PARTY_ACCEPT, 5);
	CHECK(SameBytes(One<CGPartyInvite>([](CGPartyInvite& p) { p.setTargetObjectID(901); p.setCode(CG_PARTY_INVITE_ACCEPT); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_TRADE_REQUEST), int(scene.temp.GetMode()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
}

TEST(UIMessages4Party, ACancelledRequestTellsTheOneAskedAndStopsWaiting)
{
	Scene scene;
	// No request: nothing.
	scene.Run(UI_PARTY_REQUEST_CANCEL);
	CHECK(scene.Nothing());
	// The request, waited on.
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	scene.temp.Value1 = 902;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.Run(UI_PARTY_REQUEST_CANCEL, 7, 8);
	CHECK(SameBytes(One<CGPartyInvite>([](CGPartyInvite& p) { p.setTargetObjectID(902); p.setCode(CG_PARTY_INVITE_CANCEL); }), scene.Sent()));
	CHECK(scene.Quiet());
	// Out of the game: nothing, the request kept.
	scene.Fresh();
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_PARTY_REQUEST_CANCEL);
	g_Mode = MODE_GAME;
	CHECK(scene.Nothing());
	CHECK_EQ(int(TempInformation::MODE_PARTY_REQUEST), int(scene.temp.GetMode()));
	// Something else waited on is not this to clear (the party window's
	// close, UI_ClosePartyCancel, clears only a party wait too).
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_PARTY_REQUEST_CANCEL);
	CHECK(SameBytes(One<CGPartyInvite>([](CGPartyInvite& p) { p.setTargetObjectID(902); p.setCode(CG_PARTY_INVITE_CANCEL); }), scene.Sent()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
}

namespace {
MonotonicClock::TimePoint partyNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(10000000);
const MPartyHost partyClock = []() {
	MPartyHost host{};
	host.CurrentTime = []() { return partyNow; };
	return host;
}();
} // namespace

TEST(UIMessages4Party, LeavingAndKickingWaitForTheServer)
{
	Scene scene;
	scene.Say(STRING_MESSAGE_NO_AUTHORITY, "Not yet.");
	// Alone: no leaving.
	scene.Run(UI_AWAY_PARTY, -1);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	PARTY_INFO* pPal = new PARTY_INFO;
	pPal->Name = "Pal";
	pPal->ID = 900;
	CHECK(scene.party.AddMember(pPal));
	// Something already waited on: nothing.
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_AWAY_PARTY, -1);
	scene.Run(UI_AWAY_PARTY, 0);
	CHECK(scene.Nothing());
	scene.me->SetWaitVerifyNULL();
	// Leaving: the packet names nobody.
	scene.Run(UI_AWAY_PARTY, -1);
	CHECK(SameBytes(One<CGPartyLeave>(), scene.Sent()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_PARTY_LEAVE), int(scene.me->GetWaitVerify()));
	// Kicking member 0 names it; there is no member 1.
	scene.me->SetWaitVerifyNULL();
	scene.Fresh();
	scene.Run(UI_AWAY_PARTY, 1);
	CHECK(scene.Nothing());
	CHECK(scene.me->IsWaitVerifyNULL());
	scene.Run(UI_AWAY_PARTY, 0);
	CHECK(SameBytes(One<CGPartyLeave>([](CGPartyLeave& p) { p.setTargetName("Pal"); }), scene.Sent()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_PARTY_LEAVE), int(scene.me->GetWaitVerify()));
	// Too soon after joining: no authority, said in the game messages.
	scene.me->SetWaitVerifyNULL();
	scene.Fresh();
	const MPartyHost* oldHost = MParty::SetHost(&partyClock);
	scene.party.SetJoinTime(partyNow);
	scene.Run(UI_AWAY_PARTY, 0);
	MParty::SetHost(oldHost);
	CHECK(scene.Nothing());
	CHECK(scene.me->IsWaitVerifyNULL());
	CHECK(std::string(scene.gameMessages[scene.gameMessages.GetSize() - 1]) == "Not yet.");
	// Leaving needs no time.
	MParty::SetHost(&partyClock);
	scene.Run(UI_AWAY_PARTY, -1);
	MParty::SetHost(oldHost);
	CHECK(SameBytes(One<CGPartyLeave>(), scene.Sent()));
	// Out of the game: nothing.
	scene.me->SetWaitVerifyNULL();
	scene.Fresh();
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_AWAY_PARTY, -1);
	g_Mode = MODE_GAME;
	CHECK(scene.Nothing());
	CHECK(scene.me->IsWaitVerifyNULL());
}

//----------------------------------------------------------------------
// Death
//----------------------------------------------------------------------
TEST(UIMessages4Death, TheDeadGoToTheirResurrectZoneOrRiseWhereTheyAre)
{
	Scene scene;
	// Alive: nothing.
	scene.Run(UI_FINISH_REQUEST_DIE_BUTTON);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	scene.me->m_bAlive = false;
	// Already waiting: nothing.
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_FINISH_REQUEST_DIE_BUTTON);
	CHECK(scene.Nothing());
	scene.me->SetWaitVerifyNULL();
	// A resurrect zone: a way point there, once.
	scene.me->SetResurrectZoneID(1234);
	scene.Run(UI_FINISH_REQUEST_DIE_BUTTON);
	CHECK(SameBytes(One<CGSelectWayPoint>([](CGSelectWayPoint& p) { p.setZoneID(1234); p.setX(0); p.setY(0); }), scene.Sent()));
	CHECK_EQ(0, scene.me->GetResurrectZoneID());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_RESURRECT), int(scene.me->GetWaitVerify()));
	// None: a resurrection.
	scene.me->SetWaitVerifyNULL();
	scene.Fresh();
	scene.Run(UI_FINISH_REQUEST_DIE_BUTTON);
	CHECK(SameBytes(One<CGResurrect>(), scene.Sent()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_RESURRECT), int(scene.me->GetWaitVerify()));
	// Out of the game: nothing.
	scene.me->SetWaitVerifyNULL();
	scene.Fresh();
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_FINISH_REQUEST_DIE_BUTTON);
	g_Mode = MODE_GAME;
	CHECK(scene.Nothing());
	CHECK(scene.me->IsWaitVerifyNULL());
	CHECK(calls.empty());
}

TEST(UIMessages4Death, TheResurrectButtonsUseAScrollEternityOrTheRegenZone)
{
	Scene scene;
	scene.Class(ITEM_CLASS_RESURRECT_ITEM, 2, 1, 1, kRace);
	// No scroll of the type: nothing.
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, 0);
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, 1);
	CHECK(scene.Nothing());
	MItem* pScroll = scene.Carried<MResurrectItem>(1, 701, 4, 3);
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, 0);
	CHECK(scene.Nothing());
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, 1);
	CHECK(SameBytes(One<CGUseItemFromInventory>([](CGUseItemFromInventory& p) { p.setObjectID(701); p.setX(4); p.setY(3); }), scene.Sent()));
	CHECK(scene.me->GetItemCheckBuffer() == pScroll);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	scene.me->ClearItemCheckBuffer();
	scene.Fresh();
	// Recorded defect: Eternity's effect id is never set (L9706), so an
	// indeterminate one goes with it.
	calls.clear();
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, 2);
	CHECK(Same({"skill to self|" + N(SKILL_ETERNITY)}));
	CHECK(scene.Nothing());
	scene.Fresh();
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, 3);
	CHECK(SameBytes(One<CGSelectRegenZone>([](CGSelectRegenZone& p) { p.setRegenZoneID(14); }), scene.Sent()));
	scene.Fresh();
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, 4);
	scene.Run(UI_FINISH_REQUEST_RESURRECT_BUTTON, -1);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
}

TEST(UIMessages4Death, ARegenTowerOfThePlayersRaceIsTheWayOut)
{
	Scene scene;
	testfw::MutableRow(scene.towers, 0).owner = RACE_VAMPIRE;
	testfw::MutableRow(scene.towers, 1).owner = RACE_SLAYER;
	testfw::MutableRow(scene.towers, 2).owner = RACE_SLAYER;
	// Warping: the tower asked for, if the player's race owns it.
	scene.Run(UI_WARP_REGEN_TOWER, 0);
	scene.Run(UI_WARP_REGEN_TOWER, 3);
	scene.Run(UI_WARP_REGEN_TOWER, -1);
	CHECK(scene.Nothing());
	scene.Run(UI_WARP_REGEN_TOWER, 2);
	CHECK(SameBytes(One<CGSelectRegenZone>([](CGSelectRegenZone& p) { p.setRegenZoneID(2); }), scene.Sent()));
	g_pRegenTowerInfoManager = NULL;
	scene.Fresh();
	scene.Run(UI_WARP_REGEN_TOWER, 2);
	CHECK(scene.Nothing());
	g_pRegenTowerInfoManager = &scene.towers;
	// Closing the minimap: the first tower the race owns.
	scene.Run(UI_CLOSE_SHRINE_MINIMAP);
	CHECK(SameBytes(One<CGSelectRegenZone>([](CGSelectRegenZone& p) { p.setRegenZoneID(1); }), scene.Sent()));
	scene.me->SetCreatureType(kVampire);
	scene.Fresh();
	scene.Run(UI_CLOSE_SHRINE_MINIMAP);
	CHECK(SameBytes(One<CGSelectRegenZone>([](CGSelectRegenZone& p) { p.setRegenZoneID(0); }), scene.Sent()));
	// None: the death button's answer (here, the dead player rises).
	scene.me->SetCreatureType(kOusters);
	scene.me->m_bAlive = false;
	scene.Fresh();
	scene.Run(UI_CLOSE_SHRINE_MINIMAP);
	CHECK(SameBytes(One<CGResurrect>(), scene.Sent()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_RESURRECT), int(scene.me->GetWaitVerify()));
	// Recorded defect: the tower path checks no death, wait or mode
	// (L10116) - a live player in the game sends it every time.
	scene.me->SetCreatureType(kSlayer);
	scene.me->m_bAlive = true;
	scene.Fresh();
	scene.Run(UI_CLOSE_SHRINE_MINIMAP);
	scene.Run(UI_CLOSE_SHRINE_MINIMAP);
	CGSelectRegenZone tower;
	tower.setRegenZoneID(1);
	CHECK(SameBytes(Encoded({&tower, &tower}), scene.Sent()));
}

//----------------------------------------------------------------------
// The server choice (no game: Execute checks nothing)
//----------------------------------------------------------------------
TEST(UIMessages4Login, AWorldThenAServerIsChosen)
{
	Scene scene;
	CServerInformation servers;
	ServerGroup* pGroup = new ServerGroup;
	pGroup->AddData(3, new SERVER_INFO);
	servers.AddData(7, pGroup);
	g_pServerInformation = &servers;
	// A world, while the worlds are shown.
	g_Mode = MODE_WAIT_SELECT_WORLD;
	scene.Run(UI_CONNECT_SERVER, TRUE, 7);
	CHECK(SameBytes(One<CLSelectWorld>([](CLSelectWorld& p) { p.setWorldID(7); }), scene.Sent()));
	CHECK_EQ(7, g_pUserInformation->WorldID);
	CHECK_EQ(7u, servers.GetServerGroupID());
	CHECK(Same({"mode|" + N(MODE_WAIT_SERVER_LIST)}));
	// Not a server then, nor a world while the servers are shown.
	scene.Fresh();
	calls.clear();
	scene.Run(UI_CONNECT_SERVER, FALSE, 3);
	g_Mode = MODE_WAIT_SELECT_SERVER;
	scene.Run(UI_CONNECT_SERVER, TRUE, 7);
	CHECK(scene.Nothing());
	CHECK(calls.empty());
	// A server: the character list is asked for.
	scene.Run(UI_CONNECT_SERVER, FALSE, 3);
	CHECK(SameBytes(One<CLSelectServer>([](CLSelectServer& p) { p.setServerGroupID(3); }), scene.Sent()));
	CHECK_EQ(3, g_pUserInformation->ServerID);
	CHECK_EQ(3u, servers.GetServerID());
	CHECK_EQ(int(CPS_AFTER_SENDING_CL_GET_PC_LIST), int(scene.line->getPlayerStatus()));
	CHECK(Same({"mode|" + N(MODE_WAIT_PCLIST)}));
	// No connection: nothing.
	calls.clear();
	g_pSocket = NULL;
	scene.Run(UI_CONNECT_SERVER, FALSE, 3);
	g_pSocket = scene.line.get();
	CHECK(calls.empty());
	g_pServerInformation = NULL;
}

TEST(UIMessages4Login, BackFromTheWorldsIsTheMenuAndFromTheServersTheWorlds)
{
	Scene scene;
	g_Mode = MODE_WAIT_SELECT_SERVER;
	scene.Run(UI_CLOSE_SERVER_SELECT, TRUE);
	CHECK(Same({"mode|" + N(MODE_MAINMENU)}));
	calls.clear();
	// (Without server information the world list is not shown again.)
	scene.Run(UI_CLOSE_SERVER_SELECT, FALSE);
	CHECK(Same({"mode|" + N(MODE_WAIT_SELECT_WORLD)}));
	CHECK(scene.Nothing());
}

TEST(UIMessages4Login, TheBillingPageQuitsOnlyOnYes)
{
	Scene scene;
	scene.Run(UI_GO_BILING_PAGE, FALSE);
	scene.Run(UI_GO_BILING_PAGE, 2);
	CHECK(calls.empty());
	scene.Run(UI_GO_BILING_PAGE, TRUE);
	CHECK(Same({"mode|" + N(MODE_QUIT)}));
	CHECK(scene.Nothing());
}

namespace {
const char* const kBadNames = "ui_messages_4_bad_names.bin";
const char* const kCurses = "ui_messages_4_curses.txt";
// The curse list, as a text file of words.
void Curses(MChatManager& chat, const char* words)
{
	FILE* pFile = std::fopen(kCurses, "wb");
	CHECK(pFile != NULL);
	if (pFile == NULL) return;
	std::fputs(words, pFile);
	std::fclose(pFile);
	chat.LoadFromFileCurse(kCurses);
	std::remove(kCurses);
}
// The bad-name file the check reads each time (FILE_INFO_CHAT_2): one
// English word, "evil", in MChatManager's binary form.
struct BadNames
{
	Properties* oldFiles = g_pFileDef;
	Properties files;
	BadNames()
	{
		const std::string bytes = std::string("\1\0\0\0", 4) + "\1" + std::string("\4\0\0\0", 4) + "evil";
		FILE* pFile = std::fopen(kBadNames, "wb");
		CHECK(pFile != NULL);
		if (pFile != NULL) { std::fwrite(bytes.data(), 1, bytes.size(), pFile); std::fclose(pFile); }
		files.setProperty("FILE_INFO_CHAT_2", kBadNames);
		g_pFileDef = &files;
	}
	~BadNames()
	{
		std::remove(kBadNames);
		g_pFileDef = oldFiles;
	}
};
} // namespace

TEST(UIMessages4Login, ANewCharactersNameIsCheckedThenAskedAboutOnce)
{
	Scene scene;
	BadNames badNames;
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Say(STRING_USER_REGISTER_ID_LENGTH, "Use %d to %d.");
	scene.Say(STRING_USER_REGISTER_INVALID_ID, "Bad name.");
	Curses(scene.chat, "darn\n");
	scene.Run(UI_NEWCHARACTER_CHECK);
	CHECK(calls.empty());
	// 4 to 10 letters, digits, '-' and '_', no curse, no bad name.
	char shortName[] = "Kim";
	char longName[] = "Kimkimkimki";
	char oddName[] = "Ki$m";
	char cursed[] = "Darnit";
	char bad[] = "Evilone";
	for (char* name : {shortName, longName}) scene.Run(UI_NEWCHARACTER_CHECK, 0, 0, name);
	for (char* name : {oddName, cursed, bad}) scene.Run(UI_NEWCHARACTER_CHECK, 0, 0, name);
	CHECK(Same({"popup|Use 4 to 10.", "popup|Use 4 to 10.", "popup|Bad name.", "popup|Bad name.", "popup|Bad name."}));
	CHECK(scene.Nothing());
	CHECK_EQ(0, int(g_pUserInformation->WhisperID.GetLength()));
	// The caller's name is filtered in a copy.
	CHECK(std::string(cursed) == "Darnit");
	calls.clear();
	char good[] = "Kim_-1234";
	scene.Run(UI_NEWCHARACTER_CHECK, 0, 0, good);
	CHECK(SameBytes(One<CLQueryCharacterName>([](CLQueryCharacterName& p) { p.setCharacterName("Kim_-1234"); }), scene.Sent()));
	CHECK(std::string(g_pUserInformation->WhisperID.GetString()) == "Kim_-1234");
	CHECK(calls.empty());
	// Recorded defect: the name doubles as a latch (L8551) - until the
	// answer clears it no other name is checked, or even refused.
	scene.Fresh();
	char other[] = "Leeeee";
	scene.Run(UI_NEWCHARACTER_CHECK, 0, 0, other);
	scene.Run(UI_NEWCHARACTER_CHECK, 0, 0, shortName);
	CHECK(scene.Nothing());
	CHECK(calls.empty());
	// Without the bad-name file, the curses still apply.
	g_pUserInformation->WhisperID = "";
	g_pFileDef->setProperty("FILE_INFO_CHAT_2", "ui_messages_4_no_such_file.bin");
	scene.Run(UI_NEWCHARACTER_CHECK, 0, 0, bad);
	CHECK(SameBytes(One<CLQueryCharacterName>([](CLQueryCharacterName& p) { p.setCharacterName("Evilone"); }), scene.Sent()));
}

//----------------------------------------------------------------------
// The guild's team windows and the talk box
//----------------------------------------------------------------------
TEST(UIMessages4Team, ClosingTheTeamWindowsGivesTheTalkBack)
{
	Scene scene;
	scene.dialog.Talk();
	// The team list unlocks the talk box's input.
	UIDialog::SetLockInputPCTalk();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_CLOSE_TEAM_LIST);
	CHECK(UIDialog::IsLockInput() & FLAG_UIDIALOG_LOCK_PCTALK);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_CLOSE_TEAM_LIST);
	CHECK(UIDialog::IsLockInput() & FLAG_UIDIALOG_LOCK_PCTALK);
	g_Mode = MODE_GAME;
	scene.Run(UI_CLOSE_TEAM_LIST);
	CHECK(!(UIDialog::IsLockInput() & FLAG_UIDIALOG_LOCK_PCTALK));
	// The registration shows the talk box again (no member window open).
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_CLOSE_TEAM_REGIST);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_CLOSE_TEAM_REGIST);
	g_Mode = MODE_GAME;
	CHECK(calls.empty());
	scene.Run(UI_CLOSE_TEAM_REGIST);
	CHECK(Same({"talk shown"}));
	// So does the tax window, whatever the mode, while nothing waits.
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_WAIT_BRING_FEE);
	scene.Run(UI_CLOSE_BRING_FEE_WINDOW);
	CHECK(calls.empty());
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_CLOSE_BRING_FEE_WINDOW);
	g_Mode = MODE_GAME;
	CHECK(Same({"talk shown"}));
	// The item list closes the talk box.
	calls.clear();
	scene.Run(UI_CLOSE_ITEM_LIST);
	CHECK(Same({"talk closed"}));
	CHECK(scene.Nothing());
}

TEST(UIMessages4Team, AMessageBoxShowsItsGameString)
{
	Scene scene;
	scene.Say(STRING_MESSAGE_NO_AUTHORITY, "Not yet.");
	scene.Run(UI_MESSAGE_BOX, STRING_MESSAGE_NO_AUTHORITY);
	CHECK(Same({"popup|Not yet."}));
	// The last id and past it: nothing.
	calls.clear();
	scene.Run(UI_MESSAGE_BOX, MAX_GAME_STRING);
	scene.Run(UI_MESSAGE_BOX, MAX_GAME_STRING + 1);
	CHECK(calls.empty());
	// Not run: a negative id, which passes the one bound (L9165) and hands
	// the popup the table's empty row, whose string is NULL (strlen(NULL)).
}

//----------------------------------------------------------------------
// The friends
//----------------------------------------------------------------------
TEST(UIMessages4Friends, AMessageIsSentAndFreed)
{
	Scene scene;
	C_VS_UI_FRIEND_CHATTING_INFO::FRIEND_SEND_MESSAGE* pMessage = new C_VS_UI_FRIEND_CHATTING_INFO::FRIEND_SEND_MESSAGE;
	pMessage->Name = "Pal";
	pMessage->Message = "Hello";
	scene.Run(UI_FRIEND_CHATTING_SEND_MESSAGE, 9, 9, pMessage);
	CHECK(SameBytes(One<GCFriendChatting>([](GCFriendChatting& p) { p.setCommand(CG_MESSAGE); p.setPlayerName("Pal"); p.setMessage("Hello"); }), scene.Sent()));
}

namespace {
// The UI's friend dialogs, with their windows' world and the UI's base
// (as in test_game_ui_1.cpp).
void NoTimer() {}
void NoMessage(DWORD, intptr_t, intptr_t, void*) {}
struct FriendDialogs
{
	PI_ProcessRunner* oldRunner = gpC_process_runner;
	WindowManager* oldWindows = gpC_window_manager;
	UserOption* oldOption = g_pUserOption;
	C_GLOBAL_RESOURCE* oldResource = gpC_global_resource;
	Base* oldBase = gpC_base;
	CI* oldIme = gC_ci;
	RECT oldRect = g_GameRect;
	PI_ProcessRunner runner;
	WindowManager windows;
	UserOption option;
	C_GLOBAL_RESOURCE resource;
	CI_KOREAN ime;
	CSpriteSurface back;
	Base base;
	timer_id_t blink = INVALID_TID;
	FriendDialogs()
	{
		gpC_process_runner = &runner;
		gpC_window_manager = &windows;
		g_pUserOption = &option;
		gpC_global_resource = &resource;
		spritectl_init();
		g_GameRect = {0, 0, 10, 8};
		back.Init(10, 8);
		gC_ci = &ime;
		// Base::Init starts the IME's cursor blinker, the timer after this
		// one; it is deleted again at the end.
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		blink = probe + 1;
		gC_timer2.Delete(probe);
		base.Init(&back, NoMessage);
		gpC_base = &base;
	}
	~FriendDialogs()
	{
		const bool blinkerGone = gC_timer2.Delete(blink);
		CHECK(blinkerGone);
		g_SetFL2Surface(nullptr);
		gpC_base = oldBase;
		gC_ci = oldIme;
		g_GameRect = oldRect;
		gpC_global_resource = oldResource;
		g_pUserOption = oldOption;
		gpC_window_manager = oldWindows;
		gpC_process_runner = oldRunner;
	}
	// The ask dialog the UI makes, holding a new[] copy of the name.
	static C_VS_UI_ASK_DIALOG* Ask(C_VS_UI_ASK_DIALOG::TYPE_ASK_DIALOG type, const char* name)
	{
		char* pName = new char[std::strlen(name) + 1];
		std::memcpy(pName, name, std::strlen(name) + 1);
		return new C_VS_UI_ASK_DIALOG(-1, -1, 1, 0, NULL, DIALOG_OK, type, 0, pName);
	}
};
} // namespace

TEST(UIMessages4Friends, ARequestsAnswerIsSentAndItsDialogFreed)
{
	Scene scene;
	FriendDialogs ui;
	const unsigned commands[] = {CG_ADD_FRIEND_AGREE, CG_ADD_FRIEND_REFUSE, CG_ADD_FRIEND_BLACK};
	for (int answer = 0; answer < 3; ++answer)
	{
		scene.Fresh();
		scene.Run(UI_FRIEND_REQUEST_ACCEPT, answer, 9, FriendDialogs::Ask(C_VS_UI_ASK_DIALOG::ASK_FRIEND_REQUEST, "Pal"));
		const unsigned command = commands[answer];
		CHECK(SameBytes(One<GCFriendChatting>([command](GCFriendChatting& p) { p.setCommand(command); p.setPlayerName("Pal"); }), scene.Sent()));
	}
	// Recorded defect: any other answer leaves the command at 0, a new
	// friend request (L8931-8936).
	scene.Fresh();
	scene.Run(UI_FRIEND_REQUEST_ACCEPT, 3, 9, FriendDialogs::Ask(C_VS_UI_ASK_DIALOG::ASK_FRIEND_REQUEST, "Pal"));
	CHECK(SameBytes(One<GCFriendChatting>([](GCFriendChatting& p) { p.setCommand(CG_ADD_FRIEND); p.setPlayerName("Pal"); }), scene.Sent()));
	// A deletion, accepted.
	scene.Fresh();
	scene.Run(UI_FRIEND_DELETE_ACCEPT, 9, 9, FriendDialogs::Ask(C_VS_UI_ASK_DIALOG::ASK_FRIEND_DELETE, "Foe"));
	CHECK(SameBytes(One<GCFriendChatting>([](GCFriendChatting& p) { p.setCommand(CG_FRIEND_DELETE); p.setPlayerName("Foe"); }), scene.Sent()));
	// Closed: freed, nothing sent (not a waiting dialog: that one's close
	// reaches the game UI, which is not here).
	scene.Fresh();
	scene.Run(UI_FRIEND_ASK_CLOSE, 9, 9, FriendDialogs::Ask(C_VS_UI_ASK_DIALOG::ASK_FRIEND_EXIST, "Pal"));
	CHECK(scene.Nothing());
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// The soul chain, the rank bonus, the tax, the couple
//----------------------------------------------------------------------
TEST(UIMessages4Skills, TheSoulChainIsAimedAtTheNameOnce)
{
	Scene scene;
	char name[] = "Prey";
	scene.Run(UI_SEND_NAME_FOR_SOUL_CHAIN, 9, 9, NULL);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_SEND_NAME_FOR_SOUL_CHAIN, 9, 9, name);
	CHECK(scene.Nothing());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
	scene.me->SetWaitVerifyNULL();
	scene.Run(UI_SEND_NAME_FOR_SOUL_CHAIN, 9, 9, name);
	CHECK(SameBytes(One<CGSkillToNamed>([](CGSkillToNamed& p) { p.setSkillType(SKILL_SOUL_CHAIN); p.setTargetName("Prey"); p.setCEffectID(0); }), scene.Sent()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_SKILL_SUCCESS), int(scene.me->GetWaitVerify()));
}

TEST(UIMessages4Skills, ARankBonusIsChosenWhenItCanBeLearnedAndNothingWaits)
{
	Scene scene;
	testfw::MutableRow(scene.rankBonuses, 0).SetStatus(RankBonusInfo::STATUS_CANNOT_LEARN);
	testfw::MutableRow(scene.rankBonuses, 2).SetStatus(RankBonusInfo::STATUS_LEARNED);
	scene.Run(UI_SELECT_GRADE_SKILL, 0);
	scene.Run(UI_SELECT_GRADE_SKILL, 2);
	scene.Run(UI_SELECT_GRADE_SKILL, 3);
	CHECK(scene.Nothing());
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_SELECT_GRADE_SKILL, 1);
	scene.me->SetWaitVerifyNULL();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_SELECT_GRADE_SKILL, 1);
	CHECK(scene.Nothing());
	CHECK_EQ(int(TempInformation::MODE_TRADE_REQUEST), int(scene.temp.GetMode()));
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Run(UI_SELECT_GRADE_SKILL, 1);
	CHECK(SameBytes(One<CGSelectRankBonus>([](CGSelectRankBonus& p) { p.setRankBonusType(1); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_SKILL_LEARN), int(scene.temp.GetMode()));
	// Recorded defect: no lower bound (L9266) - -1 reads the table's empty
	// row, which is learnable, and asks for bonus 0xFFFFFFFF.
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Fresh();
	scene.Run(UI_SELECT_GRADE_SKILL, -1);
	CHECK(SameBytes(One<CGSelectRankBonus>([](CGSelectRankBonus& p) { p.setRankBonusType(0xFFFFFFFF); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_SKILL_LEARN), int(scene.temp.GetMode()));
}

TEST(UIMessages4Skills, AnOustersSkillIsLearnedOrUnlearnedWithinItsLevels)
{
	Scene scene;
	// Learning: remembered, then asked for in the ousters' domain.
	scene.Run(UI_OUSTERS_LEARN_SKILL, 0x10155);
	CHECK(SameBytes(One<CGLearnSkill>([](CGLearnSkill& p) { p.setSkillType(0x155); p.setSkillDomainType(SKILLDOMAIN_OUSTERS); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_SKILL_LEARN), int(scene.temp.GetMode()));
	CHECK_EQ(0x10155, int(scene.temp.Value3));
	// Unlearning: up to level 30, and a level-1 skill only if deletable.
	scene.temp.SetMode(TempInformation::MODE_NULL);
	struct Case { intptr_t skill; int level; BYTE canDelete; bool sent; };
	const Case cases[] = {
		{100, 0, 0, true}, {100, 1, 1, true}, {100, 1, 0, false}, {100, 2, 0, true},
		{100, 30, 0, true}, {100, 31, 1, false}, {-1, 5, 1, false}, {MAX_ACTIONINFO, 5, 1, false},
		{MAX_ACTIONINFO - 1, 5, 1, true},
	};
	for (const Case& c : cases)
	{
		scene.Fresh();
		if (c.skill >= 0 && c.skill < MAX_ACTIONINFO)
		{
			testfw::MutableRow(scene.skillInfo, int(c.skill)).SetExpLevel(c.level);
			testfw::MutableRow(scene.skillInfo, int(c.skill)).CanDelete = c.canDelete;
		}
		scene.Run(UI_OUSTERS_DOWN_SKILL, c.skill);
		const SkillType_t skill = SkillType_t(c.skill);
		const Bytes expected = c.sent ? One<CGDownSkill>([skill](CGDownSkill& p) { p.setSkillType(skill); }) : Bytes();
		if (!SameBytes(expected, scene.Sent()))
		{
			std::printf("      skill %d at level %d\n", int(c.skill), c.level);
			CHECK(false);
		}
	}
	CHECK(scene.Quiet());
}

TEST(UIMessages4Tax, TheFeeIsWithdrawnOnceAndWaitedFor)
{
	Scene scene;
	scene.Run(UI_SEND_BRING_FEE, 123456);
	CHECK(SameBytes(One<CGWithdrawTax>([](CGWithdrawTax& p) { p.setGold(123456); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_WAIT_BRING_FEE), int(scene.temp.GetMode()));
	CHECK_EQ(123456, int(scene.temp.Value1));
	// Waiting: a second is not sent.
	scene.Fresh();
	scene.Run(UI_SEND_BRING_FEE, 5);
	CHECK(scene.Nothing());
	CHECK_EQ(123456, int(scene.temp.Value1));
}

TEST(UIMessages4Couple, TheNameGoesToTheNPCAndTheInputWaits)
{
	Scene scene;
	scene.dialog.Talk();
	char name[] = "Love";
	const MonotonicClock::TimePoint before = MonotonicClock::Now();
	scene.Run(UI_SEND_NAME_FOR_COUPLE, 2, 9, name);
	const MonotonicClock::TimePoint after = MonotonicClock::Now();
	CHECK(SameBytes(One<CGTypeStringList>([](CGTypeStringList& p) { p.setType(CGTypeStringList::StringType(2)); p.addString("Love"); p.setParam(kNPC); }), scene.Sent()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NPC_ASK), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_INPUT_NAME), int(scene.temp.GetMode()));
	CHECK(scene.temp.TimeValue1 >= before && scene.temp.TimeValue1 <= after);
	CHECK(calls.empty());
	// Both waiting: the talk box closes, nothing is sent.
	scene.Fresh();
	scene.Run(UI_SEND_NAME_FOR_COUPLE, 2, 9, name);
	CHECK(scene.Nothing());
	CHECK(Same({"talk closed"}));
	// Recorded defect: one of the two waiting is not enough to refuse
	// (&& for ||, L9378) - it is sent, and both are overwritten.
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.me->SetWaitVerifyNULL();
	scene.Run(UI_SEND_NAME_FOR_COUPLE, 1, 9, name);
	CHECK(SameBytes(One<CGTypeStringList>([](CGTypeStringList& p) { p.setType(CGTypeStringList::StringType(1)); p.addString("Love"); p.setParam(kNPC); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_INPUT_NAME), int(scene.temp.GetMode()));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NPC_ASK), int(scene.me->GetWaitVerify()));
}

TEST(UIMessages4Couple, TheNameWindowClosesUnlessItsAnswerIsAwaitedForLessThanAMinute)
{
	Scene scene;
	// Waiting for the NPC's answer, sent just now: kept.
	scene.dialog.Talk();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
	scene.temp.SetMode(TempInformation::MODE_INPUT_NAME);
	scene.temp.TimeValue1 = MonotonicClock::Now() - MonotonicClock::Millis(59000);
	scene.Run(UI_CLOSE_INPUT_NAME);
	CHECK(calls.empty());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NPC_ASK), int(scene.me->GetWaitVerify()));
	// Another wait in the register: kept too.
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.temp.TimeValue1 = MonotonicClock::Now() - MonotonicClock::Millis(61000);
	scene.Run(UI_CLOSE_INPUT_NAME);
	CHECK(calls.empty());
	// A minute on: closed and forgotten.
	scene.temp.SetMode(TempInformation::MODE_INPUT_NAME);
	scene.temp.TimeValue1 = MonotonicClock::Now() - MonotonicClock::Millis(60000);
	scene.Run(UI_CLOSE_INPUT_NAME);
	CHECK(Same({"talk closed"}));
	CHECK(scene.Quiet());
	// Recorded defect: any other wait is cleared with the window (L9408),
	// a pending trade's among them.
	calls.clear();
	scene.dialog.Talk();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_CLOSE_INPUT_NAME);
	CHECK(Same({"talk closed"}));
	CHECK(scene.me->IsWaitVerifyNULL());
	CHECK_EQ(int(TempInformation::MODE_TRADE_REQUEST), int(scene.temp.GetMode()));
	CHECK(scene.Nothing());
}

//----------------------------------------------------------------------
// Items: the Christmas tree, the gear, the enchant, the mixing forge, the
// code sheet, the pets, the quest items
//----------------------------------------------------------------------
TEST(UIMessages4Items, TheTreesMessageIsSentWithTheItem)
{
	Scene scene;
	scene.Say(STRING_MESSAGE_XMAS_CARD_CANNOT_USE, "Write something.");
	scene.Class(ITEM_CLASS_POTION, 1, 1, 1, kRace);
	MItem* pTree = scene.Carried<MPotion>(0, 801, 5, 2);
	char text[] = "Merry";
	scene.Run(UI_USE_XMAS_TREE, 0, 0, text);
	CHECK(scene.Nothing());
	scene.Run(UI_USE_XMAS_TREE, reinterpret_cast<intptr_t>(pTree), 0, NULL);
	CHECK(Same({"popup|Write something."}));
	CHECK(scene.Nothing());
	for (intptr_t window : {1, 0})
	{
		scene.Fresh();
		scene.Run(UI_USE_XMAS_TREE, reinterpret_cast<intptr_t>(pTree), window, text);
		CHECK(SameBytes(One<CGUseMessageItemFromInventory>([](CGUseMessageItemFromInventory& p) { p.setObjectID(801); p.setX(5); p.setY(2); p.setMessage("Merry"); }), scene.Sent()));
		CHECK(scene.me->GetItemCheckBuffer() == pTree);
		CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
		// Waiting for it: not again.
		scene.Fresh();
		scene.Run(UI_USE_XMAS_TREE, reinterpret_cast<intptr_t>(pTree), window, text);
		CHECK(scene.Nothing());
		scene.me->ClearItemCheckBuffer();
	}
}

TEST(UIMessages4Items, AGearItemOfThePlayersRaceIsUsed)
{
	Scene scene;
	scene.Class(ITEM_CLASS_HELM, 2, 1, 1, FLAG_RACE_SLAYER);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 1).Race = BYTE(FLAG_RACE_VAMPIRE);
	QuickHelm slayers;
	slayers.SetItemType(0);
	slayers.SetID(901);
	QuickHelm vampires;
	vampires.SetItemType(1);
	vampires.SetID(902);
	// Not usable now: nothing.
	slayers.UnSetAffectStatus();
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &slayers);
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, NULL);
	CHECK(calls.empty());
	slayers.SetAffectStatus();
	vampires.SetAffectStatus();
	// Another race's: nothing.
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &vampires);
	CHECK(calls.empty());
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &slayers);
	CHECK(Same({"use quick|901"}));
	// Something waiting, dead or out of the game: nothing.
	calls.clear();
	scene.me->SetItemCheckBuffer(&vampires, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &slayers);
	scene.me->ClearItemCheckBuffer();
	scene.me->m_bAlive = false;
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &slayers);
	scene.me->m_bAlive = true;
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &slayers);
	g_Mode = MODE_GAME;
	CHECK(calls.empty());
	// A vampire uses the vampire's (its gear is there).
	scene.me->SetCreatureType(kVampire);
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &slayers);
	scene.Run(UI_ITEM_USE_GEAR, 1, 0, &vampires);
	CHECK(Same({"use quick|902"}));
	CHECK(scene.Nothing());
}

namespace {
// An item on the mouse, dropped again after.
struct Held
{
	Scene& scene;
	MItem* pItem;
	Held(Scene& s, MItem* p) : scene(s), pItem(p) { scene.mouse.PickUpItem(pItem, 0, 0); }
	~Held() { scene.mouse.DropItem(); delete pItem; }
};
} // namespace

TEST(UIMessages4Items, AStarIsPutIntoTheItemOnceNothingElseWaits)
{
	for (DWORD message : {UI_ENCHANT_ACCEPT, UI_TRANS_ITEM_ACCEPT})
	{
		Scene scene;
		scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
		scene.Class(ITEM_CLASS_POTION, 1, 1, 1, kRace);
		MItem* pTarget = scene.Carried<MHelm>(0, 611, 6, 4);
		// No item on the mouse: nothing.
		scene.Run(message, 6, 4, pTarget);
		CHECK(scene.Nothing());
		MPotion* pStar = new MPotion;
		pStar->SetItemType(0);
		pStar->SetID(612);
		Held held(scene, pStar);
		scene.Run(message, 6, 4, NULL);
		CHECK(scene.Nothing());
		// Trading, waiting, dead, out of the game: nothing.
		MTradeManager trade;
		g_pTradeManager = &trade;
		scene.Run(message, 6, 4, pTarget);
		g_pTradeManager = NULL;
		scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
		scene.Run(message, 6, 4, pTarget);
		scene.temp.SetMode(TempInformation::MODE_NULL);
		scene.me->SetItemCheckBuffer(pTarget, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
		scene.Run(message, 6, 4, pTarget);
		scene.me->ClearItemCheckBuffer();
		scene.me->m_bAlive = false;
		scene.Run(message, 6, 4, pTarget);
		scene.me->m_bAlive = true;
		g_Mode = MODE_WAIT_SELECTPC;
		scene.Run(message, 6, 4, pTarget);
		g_Mode = MODE_GAME;
		CHECK(scene.Nothing());
		CHECK(scene.Quiet());
		// The star, into the item at its place (not the arguments').
		scene.Run(message, 1, 1, pTarget);
		CHECK(SameBytes(One<CGAddItemToItem>([](CGAddItemToItem& p) { p.setObjectID(612); p.setX(6); p.setY(4); }), scene.Sent()));
		CHECK(scene.me->GetItemCheckBuffer() == pStar);
		CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM), int(scene.me->GetItemCheckBufferStatus()));
		CHECK_EQ(int(TempInformation::MODE_ITEM_TO_ITEM), int(scene.temp.GetMode()));
		CHECK_EQ(6, int(scene.temp.Value1));
		CHECK_EQ(4, int(scene.temp.Value2));
		scene.me->ClearItemCheckBuffer();
	}
}

TEST(UIMessages4Items, TheForgeMixesTwoLikeItemsOfOneOption)
{
	Scene scene;
	scene.Say(STRING_ERROR_ETC_ERROR, "Forge error.");
	scene.Class(ITEM_CLASS_HELM, 2, 1, 1, kRace);
	scene.Class(ITEM_CLASS_POTION, 1, 1, 1, kRace);
	MItem* pMixer = scene.Carried<MPotion>(0, 620, 0, 0);
	MItem* pFirst = scene.Carried<MHelm>(0, 621, 2, 0);
	MItem* pSecond = scene.Carried<MHelm>(0, 622, 4, 1);
	MItem* pOther = scene.Carried<MHelm>(1, 623, 6, 2);
	MItem* pTwo = scene.Carried<MHelm>(0, 624, 8, 3);
	for (MItem* pItem : {pFirst, pSecond, pOther, pTwo}) pItem->AddItemOption(kOption);
	pTwo->AddItemOption(kOption + 1);
	// Not the forge's use: nothing.
	scene.Run(UI_USE_MIXING_FORGE, 621, 622);
	CHECK(scene.Nothing());
	scene.me->SetItemCheckBuffer(pMixer, MPlayer::ITEM_CHECK_BUFFER_USE_MIXING_ITEM);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_USE_MIXING_FORGE, 621, 622);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	// Missing, of another type, of two options, a quest's or a unique: no.
	scene.Run(UI_USE_MIXING_FORGE, 621, 999);
	scene.Run(UI_USE_MIXING_FORGE, 999, 622);
	scene.Run(UI_USE_MIXING_FORGE, 621, 623);
	scene.Run(UI_USE_MIXING_FORGE, 624, 622);
	scene.Run(UI_USE_MIXING_FORGE, 621, 624);
	pSecond->SetQuestFlag();
	scene.Run(UI_USE_MIXING_FORGE, 621, 622);
	scene.Run(UI_USE_MIXING_FORGE, 622, 621);
	pSecond->SetQuestFlag(false);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 0).ItemStyle = 1;
	scene.Run(UI_USE_MIXING_FORGE, 621, 622);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 0).ItemStyle = 0;
	CHECK(scene.Nothing());
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_MIXING_ITEM), int(scene.me->GetItemCheckBufferStatus()));
	scene.Run(UI_USE_MIXING_FORGE, 621, 622);
	CGMixItem mix;
	mix.setObjectID(620);
	mix.setX(0);
	mix.setY(0);
	mix.setTargetObjectID(0, 621);
	mix.setTargetObjectID(1, 622);
	CHECK(SameBytes(Encoded({&mix}), scene.Sent()));
	CHECK(scene.me->GetItemCheckBuffer() == pMixer);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	CHECK_EQ(int(TempInformation::MODE_ITEM_MIXING), int(scene.temp.GetMode()));
	CHECK_EQ(2, int(scene.temp.Value1));
	CHECK_EQ(0, int(scene.temp.Value2));
	CHECK_EQ(4, int(scene.temp.Value3));
	CHECK_EQ(1, int(scene.temp.Value4));
	CHECK(scene.temp.pValue == pMixer);
	// Recorded defect: one item as both targets is accepted (L9743).
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.me->SetItemCheckBuffer(pMixer, MPlayer::ITEM_CHECK_BUFFER_USE_MIXING_ITEM);
	scene.Fresh();
	scene.Run(UI_USE_MIXING_FORGE, 621, 621);
	mix.setTargetObjectID(1, 621);
	CHECK(SameBytes(Encoded({&mix}), scene.Sent()));
	// Closing the forge forgets the use; an error id opens its string.
	calls.clear();
	scene.Run(UI_CLOSE_MIXING_FORGE, STRING_ERROR_ETC_ERROR);
	CHECK(Same({"popup|Forge error."}));
	CHECK(!scene.Quiet());
	scene.Run(UI_CLOSE_MIXING_FORGE, 0);
	CHECK(scene.Quiet());
	// Recorded defect: closed while a mix waits, its wait is dropped too
	// (L9727-9728).
	scene.me->SetItemCheckBuffer(pMixer, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	scene.temp.SetMode(TempInformation::MODE_ITEM_MIXING);
	scene.Run(UI_CLOSE_MIXING_FORGE, 0);
	CHECK(scene.Quiet());
}

TEST(UIMessages4Items, AnOptionIsRemovedFromTheItemWithTheMouses)
{
	Scene scene;
	scene.Say(STRING_ERROR_ETC_ERROR, "Forge error.");
	scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
	MItem* pRare = scene.Carried<MHelm>(0, 631, 3, 5);
	MHelm tool;
	tool.SetItemType(0);
	tool.SetID(632);
	for (intptr_t option : {-1, 2})
	{
		calls.clear();
		scene.Run(UI_SEND_REMOVE_OPTION, option, 631, &tool);
		CHECK(Same({"popup|Forge error."}));
	}
	calls.clear();
	scene.Run(UI_SEND_REMOVE_OPTION, 0, 999, &tool);
	scene.Run(UI_SEND_REMOVE_OPTION, 0, 631, NULL);
	CHECK(Same({"popup|Forge error.", "popup|Forge error."}));
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	for (intptr_t option : {0, 1})
	{
		scene.Fresh();
		// Recorded defect: nothing waiting is checked first (L9791) - a
		// request waited on is overwritten.
		scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
		scene.Run(UI_SEND_REMOVE_OPTION, option, 631, &tool);
		CGMixItem mix;
		mix.setObjectID(632);
		mix.setX(3);
		mix.setY(5);
		mix.setTargetObjectID(0, 631);
		mix.setTargetObjectID(1, ObjectID_t(option));
		CHECK(SameBytes(Encoded({&mix}), scene.Sent()));
		CHECK(scene.me->GetItemCheckBuffer() == &tool);
		CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM), int(scene.me->GetItemCheckBufferStatus()));
		CHECK_EQ(int(TempInformation::MODE_ITEM_TO_ITEM), int(scene.temp.GetMode()));
		CHECK_EQ(3, int(scene.temp.Value1));
		CHECK_EQ(5, int(scene.temp.Value2));
		CHECK_EQ(int(option), int(scene.temp.Value3));
		scene.me->ClearItemCheckBuffer();
	}
	(void)pRare;
}

TEST(UIMessages4Items, AnItemGoesIntoOneOfTheCodeSheetsFiveSlots)
{
	Scene scene;
	scene.Class(ITEM_CLASS_POTION, 1, 1, 1, kRace);
	MPotion* pHeld = new MPotion;
	pHeld->SetItemType(0);
	pHeld->SetID(641);
	Held held(scene, pHeld);
	MPotion stone;
	stone.SetItemType(0);
	stone.SetID(642);
	const intptr_t at = (7 << 16) | 9;
	scene.Run(UI_ADD_ITEM_TO_CODE_SHEET, at, 0, NULL);
	scene.Run(UI_ADD_ITEM_TO_CODE_SHEET, at, -1, &stone);
	scene.Run(UI_ADD_ITEM_TO_CODE_SHEET, at, 5, &stone);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	for (intptr_t slot : {0, 4})
	{
		scene.Fresh();
		// Recorded defect: nothing waiting is checked first (L10025).
		scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
		scene.Run(UI_ADD_ITEM_TO_CODE_SHEET, at, slot, &stone);
		CHECK(SameBytes(One<CGAddItemToCodeSheet>([](CGAddItemToCodeSheet& p) { p.setX(7); p.setY(9); p.setObjectID(642); }), scene.Sent()));
		CHECK(scene.me->GetItemCheckBuffer() == &stone);
		CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM), int(scene.me->GetItemCheckBufferStatus()));
		CHECK_EQ(int(TempInformation::MODE_ITEM_CODE_SHEET), int(scene.temp.GetMode()));
		CHECK_EQ(int(slot), int(scene.temp.Value1));
		CHECK_EQ(int(at), int(scene.temp.Value2));
		CHECK(scene.temp.pValue == pHeld);
		scene.me->ClearItemCheckBuffer();
	}
}

TEST(UIMessages4Items, APetItemIsKeptOrTakenBackByItsSlot)
{
	Scene scene;
	scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
	MItem* pPet = scene.Carried<MHelm>(0, 651, 1, 2);
	// Keeping: any slot but -1, cut to a byte.
	scene.Run(UI_KEEP_PETITEM, 0, 3, NULL);
	scene.Run(UI_KEEP_PETITEM, 0, -1, pPet);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	for (intptr_t slot : {3, 256})
	{
		scene.Fresh();
		scene.temp.SetMode(TempInformation::MODE_NULL);
		scene.Run(UI_KEEP_PETITEM, 0, slot, pPet);
		// Recorded defect: 256 is slot 0 (L10277; taking back checks 0..19).
		const BYTE index = BYTE(slot);
		CHECK(SameBytes(One<CGDepositPet>([index](CGDepositPet& p) { p.setObjectID(651); p.setIndex(index); }), scene.Sent()));
		CHECK_EQ(int(TempInformation::MODE_PETITEM_MOVETO_PETSTORAGE), int(scene.temp.GetMode()));
		CHECK_EQ(int(slot), int(scene.temp.Value1));
		CHECK_EQ(1, int(scene.temp.Value2));
		CHECK_EQ(2, int(scene.temp.Value3));
		CHECK(scene.temp.pValue == pPet);
	}
	// Taking back: slots 0 to 19.
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.temp.pValue = NULL;
	scene.Fresh();
	scene.Run(UI_GET_KEEP_PETITEM, 0, 20, pPet);
	scene.Run(UI_GET_KEEP_PETITEM, 0, -1, pPet);
	scene.Run(UI_GET_KEEP_PETITEM, 0, 5, NULL);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	for (intptr_t slot : {0, 19})
	{
		scene.Fresh();
		scene.temp.SetMode(TempInformation::MODE_NULL);
		scene.Run(UI_GET_KEEP_PETITEM, 0, slot, pPet);
		const BYTE index = BYTE(slot);
		CHECK(SameBytes(One<CGWithdrawPet>([index](CGWithdrawPet& p) { p.setObjectID(651); p.setIndex(index); }), scene.Sent()));
		CHECK_EQ(int(TempInformation::MODE_PETITEM_MOVETO_INVENTORY), int(scene.temp.GetMode()));
		CHECK_EQ(int(slot), int(scene.temp.Value1));
		CHECK_EQ(1, int(scene.temp.Value2));
		CHECK_EQ(2, int(scene.temp.Value3));
		CHECK(scene.temp.pValue == pPet);
	}
	scene.temp.pValue = NULL;
}

TEST(UIMessages4Items, ClosingThePetStorageDropsIt)
{
	Scene scene;
	g_pStorage = new MStorage;
	g_pStorage->Init(1);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_CLOSE_PETSTORAGE);
	g_Mode = MODE_GAME;
	CHECK(g_pStorage != NULL);
	scene.Run(UI_CLOSE_PETSTORAGE);
	CHECK(g_pStorage == NULL);
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_CLOSE_PETSTORAGE);
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK(scene.Nothing());
}

TEST(UIMessages4Items, AQuestItemIsUsedByItsIndex)
{
	Scene scene;
	MHelm item;
	item.SetID(661);
	scene.Run(UI_ITEM_USE_GQUEST_ITEM, 2, 0, NULL);
	scene.me->m_bAlive = false;
	scene.Run(UI_ITEM_USE_GQUEST_ITEM, 2, 0, &item);
	scene.me->m_bAlive = true;
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	scene.Run(UI_ITEM_USE_GQUEST_ITEM, 0x105, 0, &item);
	CHECK(SameBytes(One<CGUseItemFromGQuestInventory>([](CGUseItemFromGQuestInventory& p) { p.setIndex(5); }), scene.Sent()));
	CHECK(scene.me->GetItemCheckBuffer() == &item);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GQUEST_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	scene.me->ClearItemCheckBuffer();
}

TEST(UIMessages4Items, AnAdvancementItemIsSwappedAtTheNPC)
{
	Scene scene;
	scene.Say(UI_STRING_MESSAGE_SWAP_ADVANCEMENT_ITEM_ERROR, "Cannot swap.");
	scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
	MItem* pItem = scene.Carried<MHelm>(0, 671, 2, 1);
	scene.Run(UI_SWAPADVANCEMENTITEM, 3, 3);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	pItem->SetQuestFlag();
	scene.Run(UI_SWAPADVANCEMENTITEM, 2, 1);
	pItem->SetQuestFlag(false);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 0).ItemStyle = 1;
	scene.Run(UI_SWAPADVANCEMENTITEM, 2, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 0).ItemStyle = 0;
	CHECK(Same({"popup|Cannot swap.", "popup|Cannot swap."}));
	CHECK(scene.Nothing());
	// The grid's place, each cut to a byte.
	scene.Run(UI_SWAPADVANCEMENTITEM, 0x102, 0x101);
	CHECK(SameBytes(One<CGShopRequestSell>([](CGShopRequestSell& p) { p.setObjectID(kNPC); p.setOpCode(SHOP_REQUEST_SWAP_ADVANCEMENT_ITEM); p.setItemObjectID(671); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_SHOP_SELL), int(scene.temp.GetMode()));
	CHECK(scene.temp.pValue == pItem);
	scene.temp.pValue = NULL;
}

//----------------------------------------------------------------------
// SMS and the nicknames
//----------------------------------------------------------------------
TEST(UIMessages4SMS, AMessageGoesToAtMostFiveNumbersCutToItsLength)
{
	Scene scene;
	char caller[] = "0101111";
	char text[] = "Hi there";
	std::list<std::string> six = {"1", "2", "3", "4", "5", "6"};
	scene.Run(UI_SEND_SMS_MESSAGE, reinterpret_cast<intptr_t>(caller), reinterpret_cast<intptr_t>(text), &six);
	scene.Run(UI_SEND_SMS_MESSAGE, reinterpret_cast<intptr_t>(caller), reinterpret_cast<intptr_t>(text), NULL);
	CHECK(scene.Nothing());
	six.pop_back();
	scene.Run(UI_SEND_SMS_MESSAGE, reinterpret_cast<intptr_t>(caller), reinterpret_cast<intptr_t>(text), &six);
	CHECK(SameBytes(One<CGSMSSend>([](CGSMSSend& p) {
		p.setCallerNumber("0101111");
		p.setMessage("Hi there");
		for (const char* n : {"1", "2", "3", "4", "5"}) p.addString(n);
	}), scene.Sent()));
	// A message of MAX_MESSAGE_LENGTH bytes or more keeps one less.
	const std::string longest(MAX_MESSAGE_LENGTH + 3, 'm');
	std::list<std::string> none;
	for (const std::string& message : {std::string(MAX_MESSAGE_LENGTH - 1, 'a'), std::string(MAX_MESSAGE_LENGTH, 'b'), longest})
	{
		scene.Fresh();
		scene.Run(UI_SEND_SMS_MESSAGE, reinterpret_cast<intptr_t>(caller), reinterpret_cast<intptr_t>(message.c_str()), &none);
		const std::string kept = message.substr(0, MAX_MESSAGE_LENGTH - 1);
		CHECK(SameBytes(One<CGSMSSend>([kept](CGSMSSend& p) { p.setCallerNumber("0101111"); p.setMessage(kept); }), scene.Sent()));
	}
	CHECK(scene.Quiet());
}

TEST(UIMessages4SMS, AnAddressIsAddedOrDeletedAndItsAnswerWaitedFor)
{
	Scene scene;
	std::string name = "Lee", custom = "Brother", number = "0102222";
	scene.Run(UI_SMS_RECORD, reinterpret_cast<intptr_t>(&name[0]), reinterpret_cast<intptr_t>(&custom[0]), &number[0]);
	CHECK(SameBytes(One<CGAddSMSAddress>([](CGAddSMSAddress& p) { p.setCharacterName("Lee"); p.setCustomName("Brother"); p.setNumber("0102222"); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_SMS_ADD_ADDRESS), int(scene.temp.GetMode()));
	// Copies, kept after the dialog's strings change.
	name = "XXX";
	custom = "YYYYYYY";
	number = "ZZZZZZZ";
	CHECK(scene.temp.StrValue1 == "Lee");
	CHECK(scene.temp.StrValue2 == "Brother");
	CHECK(scene.temp.StrValue3 == "0102222");
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Fresh();
	scene.Run(UI_SMS_DELETE, 0x87654321);
	CHECK(SameBytes(One<CGDeleteSMSAddress>([](CGDeleteSMSAddress& p) { p.setElementID(0x87654321); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_SMS_DELETE_ADDRESS), int(scene.temp.GetMode()));
	CHECK_EQ(intptr_t(0x87654321), scene.temp.Value1);
}

TEST(UIMessages4Nickname, ACustomNicknameIsFilteredCutAndSentWithItsPen)
{
	Scene scene;
	scene.Say(UI_STRING_MESSAGE_FAIL_SEARCH_ITEM, "No pen.");
	scene.Class(ITEM_CLASS_EVENT_GIFT_BOX, 26, 1, 1, kRace);
	MItem* pPen = scene.Carried<MEventGiftBoxItem>(22, 681, 0, 0);
	MItem* pTimedPen = scene.Carried<MEventGiftBoxItem>(25, 682, 1, 0);
	char nick[] = "Ace";
	char empty[] = "";
	scene.Run(UI_CHANGE_CUSTOM_NAMING, 0, 0);
	scene.Run(UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(empty), 0);
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	// No pen needed (a wolverine's): pen id 0.
	scene.Run(UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(nick), 0);
	CHECK(SameBytes(One<CGModifyNickname>([](CGModifyNickname& p) { p.setNickname("Ace"); p.setNicknameID(0); }), scene.Sent()));
	CHECK_EQ(int(TempInformation::MODE_NICKNAME_CHANGE_CUSTOM), int(scene.temp.GetMode()));
	CHECK(scene.temp.StrValue1 == "Ace");
	CHECK_EQ(0, int(scene.temp.Value2));
	CHECK(scene.me->IsItemCheckBufferNULL());
	// A pen: its id; a one-off pen is used, a timed one (type 25) is not.
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Fresh();
	scene.Run(UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(nick), 681);
	CHECK(SameBytes(One<CGModifyNickname>([](CGModifyNickname& p) { p.setNickname("Ace"); p.setNicknameID(681); }), scene.Sent()));
	CHECK_EQ(22, int(scene.temp.Value2));
	CHECK(scene.me->GetItemCheckBuffer() == pPen);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	scene.me->ClearItemCheckBuffer();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Fresh();
	scene.Run(UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(nick), 682);
	CHECK(SameBytes(One<CGModifyNickname>([](CGModifyNickname& p) { p.setNickname("Ace"); p.setNicknameID(682); }), scene.Sent()));
	CHECK_EQ(25, int(scene.temp.Value2));
	CHECK(scene.me->IsItemCheckBufferNULL());
	// No such pen: its message.
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Fresh();
	scene.Run(UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(nick), 999);
	CHECK(Same({"popup|No pen."}));
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	// Too long: cut to MAX_NICKNAME_SIZE bytes, from a copy of at most 63.
	const std::string longName(100, 'n');
	scene.Run(UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(longName.c_str()), 0);
	const std::string cut(MAX_NICKNAME_SIZE, 'n');
	CHECK(SameBytes(One<CGModifyNickname>([cut](CGModifyNickname& p) { p.setNickname(cut); p.setNicknameID(0); }), scene.Sent()));
	CHECK(scene.temp.StrValue1 == cut);
	(void)pTimedPen;
}

TEST(UIMessages4Nickname, ThePetsNamingPenNeedsAPetOut)
{
	Scene scene;
	scene.Say(UI_STRING_MESSAGE_PET_NAMING_SUMMON, "Call your pet.");
	scene.Class(ITEM_CLASS_EVENT_GIFT_BOX, 26, 1, 1, kRace);
	scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
	MEventGiftBoxItem petPen;
	petPen.SetItemType(23);
	MHelm notAPen;
	notAPen.SetItemType(0);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_RUN_NAMING_CHANGE, reinterpret_cast<intptr_t>(&petPen));
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_RUN_NAMING_CHANGE, reinterpret_cast<intptr_t>(&petPen));
	g_Mode = MODE_GAME;
	scene.Run(UI_RUN_NAMING_CHANGE, reinterpret_cast<intptr_t>(&notAPen));
	CHECK(calls.empty());
	scene.Run(UI_RUN_NAMING_CHANGE, reinterpret_cast<intptr_t>(&petPen));
	CHECK(Same({"popup|Call your pet."}));
	// (The player's own pens open the naming window: the UI's.)
	calls.clear();
	for (TYPE_ITEMTYPE type : {22, 24, 25})
	{
		petPen.SetItemType(type);
		scene.Run(UI_RUN_NAMING_CHANGE, reinterpret_cast<intptr_t>(&petPen));
	}
	CHECK(calls.empty());
	CHECK(scene.Nothing());
	CHECK(scene.Quiet());
	// Not run: a NULL item, dereferenced unchecked (L10688).
}

//----------------------------------------------------------------------
// The personal store
//----------------------------------------------------------------------
TEST(UIMessages4Store, AnItemIsDisplayedInTheFirstEmptySlot)
{
	Scene scene;
	scene.Say(STRING_MESSAGE_CANNOT_STORAGE, "Not that.");
	scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
	scene.Class(ITEM_CLASS_EVENT_GIFT_BOX, 1, 1, 1, kRace);
	MItem* pItem = scene.Carried<MHelm>(0, 691, 4, 2);
	pItem->SetPersnalPrice(5000);
	// No stall: nothing.
	scene.Run(UI_DISPLAY_ITEM, 0, 0, pItem);
	CHECK(scene.Nothing());
	g_pStorage2 = new MStorage;
	g_pStorage2->Init(2);
	g_pStorage2->SetCurrent(1);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_DISPLAY_ITEM, 0, 0, pItem);
	g_Mode = MODE_GAME;
	CHECK(scene.Nothing());
	CHECK_EQ(1u, g_pStorage2->GetCurrent());
	// Slots 0 and 1 taken: slot 2, on the first shelf.
	g_pStorage2->SetCurrent(0);
	for (unsigned slot : {0u, 1u})
	{
		MHelm* pTaken = new MHelm;
		pTaken->SetItemType(0);
		CHECK(g_pStorage2->SetItem(slot, pTaken));
	}
	g_pStorage2->SetCurrent(1);
	scene.Run(UI_DISPLAY_ITEM, 0, 0, pItem);
	CHECK(SameBytes(One<CGDisplayItem>([](CGDisplayItem& p) { p.setIndex(2); p.setItemObjectID(691); p.setPrice(5000); p.setXY(4, 2); }), scene.Sent()));
	CHECK_EQ(0u, g_pStorage2->GetCurrent());
	// A full stall: nothing.
	for (unsigned slot = 2; slot < STORAGE_SLOT; ++slot)
	{
		MHelm* pTaken = new MHelm;
		pTaken->SetItemType(0);
		CHECK(g_pStorage2->SetItem(slot, pTaken));
	}
	scene.Fresh();
	scene.Run(UI_DISPLAY_ITEM, 0, 0, pItem);
	CHECK(scene.Nothing());
	// A gift box is never displayed.
	MEventGiftBoxItem box;
	box.SetItemType(0);
	scene.Run(UI_DISPLAY_ITEM, 0, 0, &box);
	CHECK(Same({"popup|Not that."}));
	CHECK(scene.Nothing());
	// Not run: a NULL item, dereferenced unchecked (L11113).
}

TEST(UIMessages4Store, AnItemIsTakenOffTheStallBeforeTheServerAnswers)
{
	Scene scene;
	scene.Class(ITEM_CLASS_HELM, 1, 1, 1, kRace);
	MHelm mouseItem;
	mouseItem.SetItemType(0);
	mouseItem.SetGridXY(6, 3);
	scene.Run(UI_UNDISPLAY_ITEM, 1, 0, &mouseItem);
	CHECK(scene.Nothing());
	g_pStorage2 = new MStorage;
	g_pStorage2->Init(1);
	MHelm* pShown = new MHelm;
	pShown->SetItemType(0);
	pShown->SetID(695);
	CHECK(g_pStorage2->SetItem(1, pShown));
	scene.Run(UI_UNDISPLAY_ITEM, 1, 0, NULL);
	scene.Run(UI_UNDISPLAY_ITEM, 0, 0, &mouseItem);
	scene.Run(UI_UNDISPLAY_ITEM, -1, 0, &mouseItem);
	CHECK(scene.Nothing());
	CHECK(g_pStorage2->GetItem(1) == pShown);
	scene.Run(UI_UNDISPLAY_ITEM, 1, 0, &mouseItem);
	CHECK(SameBytes(One<CGUndisplayItem>([](CGUndisplayItem& p) { p.setIndex(1); p.setItemObjectID(695); p.setXY(6, 3); }), scene.Sent()));
	// Recorded defect: removed at once, before any answer, and the item
	// it hands back is never deleted (L11180) - freed here.
	CHECK(g_pStorage2->GetItem(1) == NULL);
	delete pShown;
}

TEST(UIMessages4Store, ClosingTheShopDropsTheStall)
{
	Scene scene;
	g_pStorage2 = new MStorage;
	g_pStorage2->Init(1);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Run(UI_CLOSE_PERSNALSHOP);
	CHECK(g_pStorage2 == NULL);
	scene.Run(UI_CLOSE_PERSNALSHOP);
	CHECK(scene.Nothing());
}

TEST(UIMessages4Store, TheSignIsCutToEightyBytesAndFiltered)
{
	Scene scene;
	Curses(scene.chat, "darn\n");
	std::string sign = "Darn cheap " + std::string(80, 's');
	scene.Run(UI_STORE_SIGN, 0, 0, &sign[0]);
	const std::string sent = "xxxx cheap " + std::string(69, 's');
	CHECK(SameBytes(One<CGStoreSign>([sent](CGStoreSign& p) { p.setSign(sent); }), scene.Sent()));
	// The caller's text is not touched.
	CHECK(sign.compare(0, 4, "Darn") == 0);
	// So is a nickname.
	scene.Fresh();
	char nick[] = "darnit";
	scene.Run(UI_CHANGE_CUSTOM_NAMING, reinterpret_cast<intptr_t>(nick), 0);
	CHECK(SameBytes(One<CGModifyNickname>([](CGModifyNickname& p) { p.setNickname("xxxxit"); p.setNicknameID(0); }), scene.Sent()));
}

TEST(UIMessages4Store, TheStoreInfoIsNotAskedForOffTheGround)
{
	Scene scene;
	const Bytes expected = One<CGRequestStoreInfo>([](CGRequestStoreInfo& p) { p.setOwnerObjectID(0x55); });
	for (MCreature::MOVE_TYPE type : {MCreature::CREATURE_FLYING, MCreature::CREATURE_FAKE_FLYING, MCreature::CREATURE_UNDERGROUND,
		MCreature::CREATURE_FAKE_UNDERGROUND, MCreature::CREATURE_FAKE_NO_BLOCK, MCreature::CREATURE_FAKE_GROUND})
	{
		scene.me->SetMoveType(type);
		scene.Run(UI_REQUEST_STORE_INFO, 0x55, 0);
	}
	// (Set directly: SetMoveDevice changes the player's action too.)
	scene.me->SetMoveType(MCreature::CREATURE_GROUND);
	scene.me->m_MoveDevice = MCreature::MOVE_DEVICE_RIDE;
	scene.Run(UI_REQUEST_STORE_INFO, 0x55, 0);
	scene.me->m_MoveDevice = MCreature::MOVE_DEVICE_SUMMON_SYLPH;
	scene.Run(UI_REQUEST_STORE_INFO, 0x55, 0);
	CHECK(scene.Nothing());
	scene.me->m_MoveDevice = MCreature::MOVE_DEVICE_WALK;
	scene.Run(UI_REQUEST_STORE_INFO, 0x55, 0);
	CHECK(SameBytes(expected, scene.Sent()));
}

TEST(UIMessages4Store, AStoreItemIsBoughtFromTheShopShown)
{
	Scene scene;
	MHelm item;
	item.SetID(697);
	const ObjectID_t owner = gC_vs_ui.GetOtherObjectID();
	// Recorded defect: nothing selected (OBJECTID_NULL) passes the check,
	// which looks for 0 (L11334)...
	scene.Run(UI_BUY_STORE_ITEM, 0x103, 0, &item);
	CHECK(SameBytes(One<CGBuyStoreItem>([owner](CGBuyStoreItem& p) { p.setIndex(3); p.setItemObjectID(697); p.setOwnerObjectID(owner); }), scene.Sent()));
	// ...and a creature of id 0 is refused.
	scene.Fresh();
	selected = 0;
	scene.Run(UI_BUY_STORE_ITEM, 3, 0, &item);
	CHECK(scene.Nothing());
	selected = 300;
	scene.Run(UI_BUY_STORE_ITEM, 3, 0, NULL);
	CHECK(scene.Nothing());
	scene.Run(UI_BUY_STORE_ITEM, 4, 0, &item);
	CHECK(SameBytes(One<CGBuyStoreItem>([owner](CGBuyStoreItem& p) { p.setIndex(4); p.setItemObjectID(697); p.setOwnerObjectID(owner); }), scene.Sent()));
}

TEST(UIMessages4Store, TheStoreInfoPacketsGoOutWithNoStore)
{
	// Recorded defect (latent, nothing sends these messages): both are
	// server-to-client packets built with no StoreInfo, whose write()
	// reads it through NULL (L11288-11326); they are named, not written.
	Scene scene;
	scene.Run(UI_MY_STORE_INFO, 1, 2);
	scene.Run(UI_OTHER_STORE_INFO, 1, 2);
	selected = 310;
	scene.Run(UI_OTHER_STORE_INFO, 1, 2);
	CHECK(Same({"my store info|no store", "other store info|" + N(OBJECTID_NULL), "other store info|310"}));
	CHECK(scene.Nothing());
}

//----------------------------------------------------------------------
// Money, the bug report, the quests' events, the lottery
//----------------------------------------------------------------------
TEST(UIMessages4Campaign, ADonationIsInTenThousandsTheWalletCovers)
{
	Scene scene;
	scene.Say(STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY, "Not enough.");
	scene.Run(UI_CAMPAIGN_HELP, 2, 0x103);
	CHECK(SameBytes(One<CGDonationMoney>([](CGDonationMoney& p) { p.setGold(20000); p.setDonationType(3); }), scene.Sent()));
	CHECK_EQ(25000, scene.money.GetMoney());
	scene.Fresh();
	scene.Run(UI_CAMPAIGN_HELP, 3, 1);
	CHECK(Same({"popup|Not enough."}));
	calls.clear();
	scene.Run(UI_CAMPAIGN_HELP, 0, 1);
	scene.Run(UI_CAMPAIGN_HELP, -5, 1);
	CHECK(calls.empty());
	CHECK(scene.Nothing());
}

namespace {
Player* bugTarget = NULL;
const WireHost bugHost = []() {
	WireHost host{};
	host.BugReportTarget = []() { return bugTarget; };
	return host;
}();
} // namespace

TEST(UIMessages4Report, ABugReportGoesToTheServerAsAChatCommand)
{
	Scene scene;
	bugTarget = scene.line.get();
	Wire::SetHost(&bugHost);
	char text[] = "stuck";
	scene.Run(UI_SEND_BUG_REPORT, 5, 0, text);
	Wire::SetHost(NULL);
	bugTarget = NULL;
	CHECK(SameBytes(One<CGSay>([](CGSay& p) { p.setMessage("*bug_report [UI_BUG] 5, stuck"); p.setColor(0); }), scene.Sent()));
}

TEST(UIMessages4Quest, AQuestCanKillThePlayerAndChooseWhereHeRises)
{
	Scene scene;
	// (Dying zeroes the HP through the status reactions: none here.)
	ModifyStatusManager reactions;
	reactions.MFunctionManager::Release();
	ModifyStatusManager* oldReactions = g_pModifyStatusManager;
	g_pModifyStatusManager = &reactions;
	scene.me->SetZone(g_pZone);
	scene.Run(UI_GQUEST_SET_ACTION, 0, 1500);
	scene.Run(UI_GQUEST_SET_ACTION, 2, 1500);
	CHECK(!scene.me->IsDead());
	CHECK_EQ(0, scene.me->GetResurrectZoneID());
	scene.Run(UI_GQUEST_SET_ACTION, 1, 1500);
	g_pModifyStatusManager = oldReactions;
	CHECK(scene.me->IsDead());
	CHECK_EQ(1500, scene.me->GetResurrectZoneID());
	CHECK(scene.Nothing());
}

TEST(UIMessages4Quest, TheAdvancementEndingShowsTheRacesBackground)
{
	Scene scene;
	scene.Run(UI_GQUEST_ENDING_EVENT, 0);
	scene.Run(UI_GQUEST_ENDING_EVENT, 4);
	CHECK_EQ(0, scene.events.GetEventCount());
	const DWORD flags = EVENTFLAG_ONLY_EVENT_BACKGROUND | EVENTFLAG_NOT_DRAW_UI | EVENTFLAG_NOT_DRAW_CREATURE | EVENTFLAG_NOT_DRAW_INFORMATION
		| EVENTFLAG_NOT_DRAW_CREATURE_SHADOW | EVENTFLAG_NOT_DRAW_ITEM | EVENTFLAG_NOT_DRAW_MOUSE_POINTER | EVENTFLAG_NOT_DRAW_EFFECT
		| EVENTFLAG_DENY_INPUT | EVENTFLAG_NOT_FADE_SCREEN | EVENTFLAG_NOT_PLAY_SOUND;
	const DWORD backgrounds[] = {1, 3, 5};
	for (int race = 1; race <= 3; ++race)
	{
		scene.Run(UI_GQUEST_ENDING_EVENT, race);
		const MEvent* pEvent = scene.events.GetEvent(EVENTID_ADVANCEMENT_QUEST_ENDING);
		CHECK(pEvent != NULL);
		if (pEvent == NULL) continue;
		CHECK_EQ(int(EVENTTYPE_ZONE), int(pEvent->eventType));
		CHECK_EQ(flags, pEvent->eventFlag);
		CHECK_EQ(0u, pEvent->parameter1);
		CHECK_EQ(backgrounds[race - 1], pEvent->parameter4);
		scene.events.RemoveEvent(EVENTID_ADVANCEMENT_QUEST_ENDING);
	}
	CHECK(scene.Nothing());
}

TEST(UIMessages4Quest, ClosingTheLotteryCardEndsTheQuestEnding)
{
	// Not run: the ending itself (the flag set and right == 5), whose
	// UI_CloseAllDialog rebuilds the UI's message boxes (see
	// test_game_ui_1.cpp); and UI_REQUEST_FORCE_DIE, which always calls
	// C_VS_UI::CloseAllDialog before its CGFailQuest(true).
	Scene scene;
	g_bEventQuestEnding = true;
	scene.Run(UI_CLOSE_LOTTERY_CARD, 0, 4);
	CHECK(!g_bEventQuestEnding);
	scene.Run(UI_CLOSE_LOTTERY_CARD, 0, 5);
	CHECK(!g_bEventQuestEnding);
	CHECK_EQ(0, scene.events.GetEventCount());
	CHECK(scene.Nothing());
	CHECK(calls.empty());
}

TEST(UIMessages4Recall, WithoutTheTeamWindowNoOneIsRecalled)
{
	Scene scene;
	scene.Run(UI_RECALL_BY_NAME);
	CHECK(scene.Nothing());
}

//----------------------------------------------------------------------
// The windows' own messages: a window closes (or opens) and nothing in
// the game changes, in the game or not and whatever waits.
//----------------------------------------------------------------------
TEST(UIMessages4Windows, ClosingAWindowChangesNothingInTheGame)
{
	Scene scene;
	char number[] = "0103333";
	const struct { DWORD message; intptr_t left; void* ptr; } windows[] = {
		{UI_CLOSE_PARTY_MANAGER, 0, NULL}, {UI_CLOSE_TEAM_INFO, 0, NULL}, {UI_CLOSE_TEAM_MEMBER_INFO, 0, NULL},
		{UI_CLOSE_TEAM_MEMBER_LIST, 0, NULL}, {UI_CLOSE_FRIEND_CHATTING_INFO, 0, NULL}, {UI_OPEN_FRIEND_CHATTING_INFO, 0, NULL},
		{UI_CLOSE_FILE_DIALOG, C_VS_UI_FILE_DIALOG::MODE_PROFILE_SELECT, number}, {UI_CLOSE_FILE_DIALOG, 99, NULL},
		{UI_ENCHANT_CANCEL, 0, NULL}, {UI_CLOSE_OTHER_INFO, 0, NULL}, {UI_CLOSE_TRACE_WINDOW, 0, NULL}, {UI_RUN_CONNECT, 0, NULL},
		{UI_CLOSE_XMAS_CARD_WINDOW, 0, NULL}, {UI_CLOSE_WAR_LIST_WINDOW, 0, NULL}, {UI_CLOSE_BLOOD_BIBLE_STATUS, 0, NULL},
		{UI_CLOSE_POPUP_MESSAGE, 0, NULL}, {UI_CLOSE_QUEST_STATUS, 0, NULL}, {UI_CLOSE_IMAGE_NOTICE, 0, NULL},
		{UI_CLOSE_BULLETIN_BOARD_WINDOW, 0, NULL}, {UI_TRANS_ITEM_CANCEL, 0, NULL}, {UI_CLOSE_REMOVE_OPTION, 0, NULL},
		{UI_CLOSE_OUSTERS_SKILL_INFO, 0, NULL}, {UI_RUN_LEVELUP, 0, NULL}, {UI_CLOSE_HORN, 0, NULL},
		{UI_CLEAR_ALL_STAGE, 0, NULL}, {UI_CLEAR_ALL_STAGE, 1, NULL}, {UI_CLEAR_ALL_STAGE, 2, NULL},
		{UI_CLOSE_FINDING_MINE, 0, NULL}, {UI_CLOSE_NEMONEMO, 0, NULL}, {UI_CLOSE_PUSHPUSH, 0, NULL},
		{UI_GO_TO_BEGINNER_ZONE, 0, NULL}, {UI_CLOSE_MAILBOX, 0, NULL}, {UI_CLOSE_PET_INFO, 0, NULL},
		{UI_CLOSE_USE_PET_FOOD, 0, NULL}, {UI_CLOSE_KEEP_PETITEM, 0, NULL}, {UI_CLOSE_GET_KEEP_PETITEM, 0, NULL},
		{UI_CLOSE_SMS_MESSAGE, 0, NULL}, {UI_CLOSE_SMS_LIST, 0, NULL}, {UI_CLOSE_SMS_RECORD, 0, NULL},
		{UI_SMS_ADD_SEND_LIST, reinterpret_cast<intptr_t>(number), NULL}, {UI_CLOSE_NAMING, 0, NULL}, {UI_CLOSE_NAMING_CHANGE, 0, NULL},
		{UI_CLOSE_QUEST_MANAGER, 0, NULL}, {UI_CLOSE_QUEST_LIST, 0, NULL}, {UI_CLOSE_QUEST_DETAIL, 0, NULL},
		{UI_CLOSE_QUEST_MISSION, 0, NULL}, {UI_CLOSE_QUEST_ITEM, 0, NULL}, {UI_CLOSE_QUEST_ICON, 3, NULL},
		{UI_CLOSE_POWER_JJANG, 0, NULL}, {UI_CLOSE_SWAPADVANCEMENTITEM, 0, NULL}, {UI_RUN_NEXT_GQUEST_EXCUTE_ELEMENT, 0, NULL},
	};
	const DWORD lock = UIDialog::IsLockInput();
	for (CLIENT_MODE mode : {MODE_GAME, MODE_WAIT_SELECTPC})
	{
		for (TempInformation::TEMP_MODE waiting : {TempInformation::MODE_NULL, TempInformation::MODE_TRADE_REQUEST})
		{
			g_Mode = mode;
			scene.temp.SetMode(waiting);
			for (const auto& window : windows) scene.Run(window.message, window.left, 0, window.ptr);
			g_Mode = MODE_GAME;
			CHECK_EQ(int(waiting), int(scene.temp.GetMode()));
		}
	}
	scene.temp.SetMode(TempInformation::MODE_NULL);
	CHECK(scene.Nothing());
	CHECK(calls.empty());
	CHECK(scene.Quiet());
	CHECK_EQ(lock, UIDialog::IsLockInput());
	CHECK(scene.me->IsAlive());
}
