#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "UIMessageManager.h"
#include "UIFunction.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MSector.h"
#include "FrameClock.h"
#include "MonotonicClock.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectStatusTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "RankBonusTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MQuickSlot.h"
#include "MItemLimits.h"
#include "MTradeManager.h"
#include "MStorage.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MPriceManager.h"
#include "MMoneyManager.h"
#include "MSkillManager.h"
#include "MGameStringTable.h"
#include "MTimeItemManager.h"
#include "TempInformation.h"
#include "TalkBox.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "HelpEvent.h"
#include "UserOption.h"
#include "KeyAccelerator.h"
#include "AcceleratorManager.h"
#include "Properties.h"
#include "ClientMode.h"
#include "InputState.h"
#include "MMusic.h"
#include "DXLib/CDirectSound.h"
#include "AudioTypes.h"
#include "SoundDef.h"
#include "DisplaySettings.h"
#include "VS_UI_title.h"
#include "VS_UI_item.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_GameCommon.h"
#include "GameSocket.h"
#include "ClientPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Cpackets/CGAddQuickSlotToMouse.h"
#include "Cpackets/CGAddInventoryToMouse.h"
#include "Cpackets/CGAddGearToMouse.h"
#include "Cpackets/CGAddMouseToInventory.h"
#include "Cpackets/CGAddMouseToGear.h"
#include "Cpackets/CGAddMouseToQuickSlot.h"
#include "Cpackets/CGTradeAddItem.h"
#include "Cpackets/CGTradeRemoveItem.h"
#include "Cpackets/CGTradeMoney.h"
#include "Cpackets/CGTradeFinish.h"
#include "Cpackets/CGTradePrepare.h"
#include "Cpackets/CGShopRequestBuy.h"
#include "Cpackets/CGShopRequestSell.h"
#include "Cpackets/CGUseBonusPoint.h"
#include "Cpackets/CGRequestRepair.h"
#include "Cpackets/CGSilverCoating.h"
#include "Cpackets/CGStashRequestBuy.h"
#include "Cpackets/CGStashToMouse.h"
#include "Cpackets/CGMouseToStash.h"
#include "Cpackets/CGStashDeposit.h"
#include "Cpackets/CGStashWithdraw.h"
#include "Cpackets/CGRequestNewbieItem.h"
#include "Cpackets/CGSelectPortal.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

extern bool g_bPartyRunning;
extern BOOL g_bEnable3DHAL;
class CSoundPartManager;
extern CSoundPartManager* g_pSoundManager;

// The UI's results, third part (Client/UIMessageManager.cpp lines
// 4021-8052, gameclient's since task 4.258): lifting items from the quick
// slots, the inventory and the gear, merging the held pile into another,
// using items, the shop (buy, sell, sell all), repair and silvering, the
// storage and its money, the trade, the options, the small close dialogs
// and the elevator. Each test stands up the model the handler reads, runs
// it through UIMessageManager::Execute as the UI's callback does, and
// pins what goes to the server (the packet and every field), what the
// model becomes, and what the dialogs, the sounds and the hosts are asked.
// gC_vs_ui is never initialised here, so its windows do nothing and
// IsRunningExchange answers false: the branches for an open trade window
// (the pick-up at L4158-4195 and the insert at L4615-4657) cannot be
// reached, and the window-only handlers are only shown to be harmless.
// No UIMessages host entry is called in this range; the scene's host
// records any call, so every expectation below also says "none".
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

// What the game server is sent, field by field.
std::string Render(Packet& packet)
{
	switch (packet.getPacketID())
	{
	case Packet::PACKET_CG_ADD_QUICKSLOT_TO_MOUSE: {
		CGAddQuickSlotToMouse& p = static_cast<CGAddQuickSlotToMouse&>(packet);
		return "quickslot to mouse|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_ADD_INVENTORY_TO_MOUSE: {
		CGAddInventoryToMouse& p = static_cast<CGAddInventoryToMouse&>(packet);
		return "inventory to mouse|" + N(p.getObjectID()) + "|" + N(p.getX()) + "|" + N(p.getY()) + "|" + N(p.getInventoryItemObjectID());
	}
	case Packet::PACKET_CG_ADD_GEAR_TO_MOUSE: {
		CGAddGearToMouse& p = static_cast<CGAddGearToMouse&>(packet);
		return "gear to mouse|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_ADD_MOUSE_TO_INVENTORY: {
		CGAddMouseToInventory& p = static_cast<CGAddMouseToInventory&>(packet);
		return "mouse to inventory|" + N(p.getObjectID()) + "|" + N(p.getInvenX()) + "|" + N(p.getInvenY()) + "|" + N(p.getInventoryItemObjectID());
	}
	case Packet::PACKET_CG_ADD_MOUSE_TO_GEAR: {
		CGAddMouseToGear& p = static_cast<CGAddMouseToGear&>(packet);
		return "mouse to gear|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_ADD_MOUSE_TO_QUICKSLOT: {
		CGAddMouseToQuickSlot& p = static_cast<CGAddMouseToQuickSlot&>(packet);
		return "mouse to quickslot|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_TRADE_ADD_ITEM: {
		CGTradeAddItem& p = static_cast<CGTradeAddItem&>(packet);
		return "trade add|" + N(p.getTargetObjectID()) + "|" + N(p.getItemObjectID());
	}
	case Packet::PACKET_CG_TRADE_REMOVE_ITEM: {
		CGTradeRemoveItem& p = static_cast<CGTradeRemoveItem&>(packet);
		return "trade remove|" + N(p.getTargetObjectID()) + "|" + N(p.getItemObjectID());
	}
	case Packet::PACKET_CG_TRADE_MONEY: {
		CGTradeMoney& p = static_cast<CGTradeMoney&>(packet);
		return "trade money|" + N(p.getTargetObjectID()) + "|" + N(p.getAmount()) + "|" + N(p.getCode());
	}
	case Packet::PACKET_CG_TRADE_FINISH: {
		CGTradeFinish& p = static_cast<CGTradeFinish&>(packet);
		return "trade finish|" + N(p.getTargetObjectID()) + "|" + N(p.getCode());
	}
	case Packet::PACKET_CG_TRADE_PREPARE: {
		CGTradePrepare& p = static_cast<CGTradePrepare&>(packet);
		return "trade prepare|" + N(p.getTargetObjectID()) + "|" + N(p.getCode());
	}
	case Packet::PACKET_CG_SHOP_REQUEST_BUY: {
		CGShopRequestBuy& p = static_cast<CGShopRequestBuy&>(packet);
		return "buy|" + N(p.getObjectID()) + "|" + N(p.getShopType()) + "|" + N(p.getShopIndex()) + "|" + N(p.getX()) + "|" + N(p.getY()) + "|" + N(p.getItemNum());
	}
	case Packet::PACKET_CG_SHOP_REQUEST_SELL: {
		CGShopRequestSell& p = static_cast<CGShopRequestSell&>(packet);
		return "sell|" + N(p.getObjectID()) + "|" + N(p.getOpCode()) + "|" + N(p.getItemObjectID());
	}
	case Packet::PACKET_CG_USE_BONUS_POINT: {
		CGUseBonusPoint& p = static_cast<CGUseBonusPoint&>(packet);
		return "bonus|" + N(p.getWhich());
	}
	case Packet::PACKET_CG_REQUEST_REPAIR: {
		CGRequestRepair& p = static_cast<CGRequestRepair&>(packet);
		return "repair|" + N(p.getObjectID());
	}
	case Packet::PACKET_CG_SILVER_COATING: {
		CGSilverCoating& p = static_cast<CGSilverCoating&>(packet);
		return "silver|" + N(p.getObjectID());
	}
	case Packet::PACKET_CG_STASH_REQUEST_BUY:
		return "stash buy";
	case Packet::PACKET_CG_STASH_TO_MOUSE: {
		CGStashToMouse& p = static_cast<CGStashToMouse&>(packet);
		return "stash to mouse|" + N(p.getObjectID()) + "|" + N(p.getRack()) + "|" + N(p.getIndex());
	}
	case Packet::PACKET_CG_MOUSE_TO_STASH: {
		CGMouseToStash& p = static_cast<CGMouseToStash&>(packet);
		return "mouse to stash|" + N(p.getObjectID()) + "|" + N(p.getRack()) + "|" + N(p.getIndex());
	}
	case Packet::PACKET_CG_STASH_DEPOSIT: {
		CGStashDeposit& p = static_cast<CGStashDeposit&>(packet);
		return "deposit|" + N(p.getAmount());
	}
	case Packet::PACKET_CG_STASH_WITHDRAW: {
		CGStashWithdraw& p = static_cast<CGStashWithdraw&>(packet);
		return "withdraw|" + N(p.getAmount());
	}
	case Packet::PACKET_CG_REQUEST_NEWBIE_ITEM: {
		CGRequestNewbieItem& p = static_cast<CGRequestNewbieItem&>(packet);
		return "newbie|" + N(p.getItemClass());
	}
	case Packet::PACKET_CG_SELECT_PORTAL: {
		CGSelectPortal& p = static_cast<CGSelectPortal&>(packet);
		return "portal|" + N(p.getZoneID());
	}
	default:
		return "packet|" + N(packet.getPacketID());
	}
}

// The game server's connection: it keeps what is sent.
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override { calls.push_back(Render(*pPacket)); }
};

// Every entry records: none is expected in this range.
const UIMessages::Host uiMessagesHost = []() {
	UIMessages::Host host{};
	host.InitSocket = []() { calls.push_back("host InitSocket"); return BOOL(TRUE); };
	host.LoadingAddonSPK = [](bool loading) { calls.push_back("host LoadingAddonSPK|" + N(loading)); return true; };
	host.ReleaseGameObject = []() { calls.push_back("host ReleaseGameObject"); };
	host.StopLoadingThread = []() { calls.push_back("host StopLoadingThread"); };
	host.SaveLastSelectedCharacter = [](int slot) { calls.push_back("host SaveLastSelectedCharacter|" + N(slot)); };
	host.SetMode = [](CLIENT_MODE mode) { calls.push_back("host SetMode|" + N(mode)); };
	host.GetSelectedCreature = []() { calls.push_back("host GetSelectedCreature"); return TYPE_OBJECTID(OBJECTID_NULL); };
	host.GetSelectedItemID = []() { calls.push_back("host GetSelectedItemID"); return TYPE_OBJECTID(OBJECTID_NULL); };
	return host;
}();

const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.PlayMusicCurrentZone = []() { calls.push_back("zone music"); };
	return host;
}();

// The dialogs, numbered as they open: the scene opens the NPC's talk
// window first (1), so message boxes are 2 and on.
int windowCount = 0;
std::string W(DialogWindow::Window* pWindow) { return N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow))); }
DialogWindow::Window* NewWindow() { return reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(++windowCount)); }
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewTalk = [](int, const char*) { return NewWindow(); };
	host.NewMessage = [](int, int, int, std::uint16_t) { return NewWindow(); };
	// A message box's text, its lines joined again (it breaks a long one
	// at a space).
	host.SetMessage = [](DialogWindow::Window*, const char* const* pLines, int count, bool) {
		std::string text;
		for (int i = 0; i < count; ++i) text += (i > 0 ? " " : "") + std::string(pLines[i]);
		calls.push_back("popup|" + text);
	};
	host.Start = [](DialogWindow::Window* pWindow) { calls.push_back("start " + W(pWindow)); };
	host.Finish = [](DialogWindow::Window* pWindow) { calls.push_back("finish " + W(pWindow)); };
	host.Delete = [](DialogWindow::Window* pWindow) { calls.push_back("delete " + W(pWindow)); };
	return host;
}();

const MHelpHost helpHost = []() {
	MHelpHost host{};
	host.AddHelpMail = [](HELP_EVENT he) { calls.push_back("help|" + N(he)); };
	return host;
}();

// The item's clock, which the trade's accept delay reads.
MonotonicClock::TimePoint now = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
const MItemHost itemHost = []() {
	MItemHost host{};
	host.pCurrentTime = &now;
	host.UsePotionFromInventory = [](MItem* pItem) { calls.push_back("use|" + N(pItem->GetID())); };
	return host;
}();

// The skill check after a change of gear or storage: in zone 3001 it only
// empties the set, which is how a test sees that it ran.
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.Status = []() -> MStatus* { return g_pPlayer; };
	host.GetID = []() { return g_pPlayer->GetID(); };
	host.GetRace = []() { return g_pPlayer->GetRace(); };
	return host;
}();
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return g_pZone != NULL; };
	host.GetID = []() { return 3001; };
	return host;
}();

// The player, who can die or be on the move.
struct Me : MPlayer
{
	void Kill() { m_bAlive = false; }
	void Revive() { m_bAlive = true; }
	void Walk() { m_Action = ACTION_MOVE; }
	void Stand() { m_Action = ACTION_STAND; }
};

// A quick item whose use from the slot is recorded.
struct QuickPotion : MPotion
{
	void UseQuickItem() override { calls.push_back("quick use|" + N(GetID())); }
};
struct QuickPupa : MOustersPupa
{
	void UseQuickItem() override { calls.push_back("quick use|" + N(GetID())); }
};

// An item of any class, for the storage's refusals.
struct Thing : MItem
{
	ITEM_CLASS m_Class;
	explicit Thing(ITEM_CLASS itemClass) : m_Class(itemClass) {}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
};

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_mapCreature;
	using MZone::m_ppSector;
};

const TYPE_OBJECTID kOther = 500;	// the trade's other side
const TYPE_OBJECTID kNPC = 300;		// the shopkeeper
const int kNPCType = 700;

// The sound each class makes in the inventory.
enum
{
	kPotionSound = 11, kPupaSound = 12, kHelmSound = 15, kNecklaceSound = 16, kCircletSound = 17,
	kMoneySound = 18, kMagazineSound = 19, kSwordSound = 20,
};

// The game strings the handlers pop up, short so a box is one line.
const char* const kNoMoney = "no money";

struct Scene
{
	GameModelWorld world{ 4 };
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	MActionInfoTable actions;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MSkillSet skills;
	MSkillInfoTable skillInfo;
	MSkillManager skillManager;
	TempInformation temp;
	MInventory inventory;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MMoneyManager money;
	MPriceManager prices;
	MStorage storage;
	PCTalkBox talkBox;
	UIDialog dialog;
	C_VS_UI_ITEM uiItem;
	C_VS_UI_MOUSE_POINTER mouse;
	UserOption option;
	Wire wire;
	UIMessageManager manager;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MSkillSet* oldSkills = g_pSkillAvailable;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	MSkillManager* oldSkillManager = g_pSkillManager;
	TempInformation* oldTemp = g_pTempInformation;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MBelt* oldQuickSlot = g_pQuickSlot;
	MOustersArmsBand* oldArmsBand1 = g_pArmsBand1;
	MOustersArmsBand* oldArmsBand2 = g_pArmsBand2;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MPriceManager* oldPrices = g_pPriceManager;
	MStorage* oldStorage = g_pStorage;
	MTradeManager* oldTrade = g_pTradeManager;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	UIDialog* oldDialog = g_pUIDialog;
	C_VS_UI_ITEM* oldUiItem = gpC_item;
	C_VS_UI_MOUSE_POINTER* oldMouse = gpC_mouse_pointer;
	UserOption* oldOption = g_pUserOption;
	ClientPlayer* oldSocket = g_pSocket;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	CLIENT_MODE oldMode = g_Mode;
	bool oldWatch = g_bWatchMode;
	BOOL oldUIInput = g_bUIInput;
	int oldActive = g_bActiveGame;
	bool oldParty = g_bPartyRunning;
	MonotonicClock::TimePoint oldFrameNow = g_FrameNow;
	MonotonicClock::TimePoint oldNow = now;
	int oldRefPoints[ITEM_REF_POINT_COUNT][2];
	const UIMessages::Host* oldUIMessagesHost = UIMessages::SetHost(&uiMessagesHost);
	const WorldEvents::Host* oldWorldHost = WorldEvents::SetHost(&worldHost);
	const DialogWindow::Host* oldWindowHost = DialogWindow::SetHost(&windowHost);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	std::unique_ptr<ZoneProbe> zone;
	std::unique_ptr<MCreature> other;
	std::unique_ptr<MBelt> belt;
	std::unique_ptr<MOustersArmsBand> band1, band2;
	std::unique_ptr<MShop> shop;
	// Items the test made that no container holds (or that it frees).
	std::vector<std::unique_ptr<MItem>> loose;
	Me* me = nullptr;

	Scene()
	{
		calls.clear();
		windowCount = 0;
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) { oldRefPoints[i][0] = g_item_ref_point[i].x; oldRefPoints[i][1] = g_item_ref_point[i].y; }
		MItem::SetHost(&itemHost);
		SetHelpHost(&helpHost);
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		Type(CREATURETYPE_SLAYER_MALE, CREATURETRIBE_SLAYER);
		Type(CREATURETYPE_VAMPIRE_MALE1, CREATURETRIBE_VAMPIRE);
		Type(CREATURETYPE_OUSTERS, CREATURETRIBE_OUSTERS);
		Type(kNPCType, CREATURETRIBE_NPC);
		testfw::MutableRow(creatureTypes, kNPCType).Name = "Merchant";
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pSkillAvailable = &skills;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		skillManager.CTypeTable<MSkillDomain>::Init(MAX_SKILLDOMAIN);
		g_pSkillManager = &skillManager;
		Items();
		Strings();
		g_pClientConfig->TRADE_ACCEPT_DELAY_TIME = 1000;
		g_pClientConfig->AFTER_TRADE_ITEM_DROP_DELAY = 700;
		g_pClientConfig->MUSIC_THEME = MUSICID_NULL;
		g_pTempInformation = &temp;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		vampireGear.Init();
		g_pVampireGear = &vampireGear;
		oustersGear.Init();
		g_pOustersGear = &oustersGear;
		g_pQuickSlot = NULL;
		g_pArmsBand1 = NULL;
		g_pArmsBand2 = NULL;
		g_pMoneyManager = &money;
		g_pPriceManager = &prices;
		storage.Init(3);
		g_pStorage = &storage;
		g_pTradeManager = NULL;
		gpC_item = &uiItem;
		gpC_mouse_pointer = &mouse;
		g_pUserOption = &option;
		g_pSocket = &wire;
		g_Mode = MODE_GAME;
		g_bWatchMode = false;
		g_bUIInput = FALSE;
		g_bActiveGame = 1;
		g_bPartyRunning = false;
		g_FrameNow = now;
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(20), TYPE_SECTORPOSITION(20)));
		g_pZone = zone.get();
		other.reset(new MCreature);
		other->SetID(kOther);
		other->SetName("Trader");
		zone->m_mapCreature[kOther] = other.get();
		me = new Me;
		g_pPlayer = me;
		me->SetCreatureType(CREATURETYPE_SLAYER_MALE);
		me->SetID(77);
		me->SetPosition(5, 5);
		// The NPC the player talks to, whose window the handlers return to.
		talkBox.SetNPCID(kNPC);
		talkBox.SetCreatureType(kNPCType);
		talkBox.SetType(PCTalkBox::NORMAL);
		talkBox.SetContent("Welcome");
		talkBox.AddString("Buy");
		g_pPCTalkBox = &talkBox;
		dialog.Init();
		g_pUIDialog = &dialog;
		dialog.PopupPCTalkDlg();
		manager.Init();
		calls.clear();
	}
	~Scene()
	{
		// The held item goes unless a container still has it.
		MItem* pHeld = mouse.GetPickUpItem();
		mouse.DropItem();
		if (pHeld != NULL && !Owned(pHeld)) delete pHeld;
		delete g_pTradeManager;
		g_pTradeManager = oldTrade;
		zone->m_mapCreature.clear();
		zone.reset();
		g_pZone = oldZone;
		delete me;
		g_pPlayer = oldPlayer;
		other.reset();
		dialog.Release();
		talkBox.Release();
		storage.Release();
		inventory.Release();
		slayerGear.Release();
		vampireGear.Release();
		oustersGear.Release();
		belt.reset();
		band1.reset();
		band2.reset();
		shop.reset();
		ZoneState::SetHost(oldZoneHost);
		PlayerState::SetHost(oldPlayerHost);
		DialogWindow::SetHost(oldWindowHost);
		WorldEvents::SetHost(oldWorldHost);
		UIMessages::SetHost(oldUIMessagesHost);
		SetHelpHost(NULL);
		MItem::SetHost(NULL);
		now = oldNow;
		g_FrameNow = oldFrameNow;
		g_bPartyRunning = oldParty;
		g_bActiveGame = oldActive;
		g_bUIInput = oldUIInput;
		g_bWatchMode = oldWatch;
		g_Mode = oldMode;
		g_pSocket = oldSocket;
		g_pUserOption = oldOption;
		gpC_mouse_pointer = oldMouse;
		gpC_item = oldUiItem;
		g_pUIDialog = oldDialog;
		g_pPCTalkBox = oldTalkBox;
		g_pStorage = oldStorage;
		g_pPriceManager = oldPrices;
		g_pMoneyManager = oldMoney;
		g_pArmsBand2 = oldArmsBand2;
		g_pArmsBand1 = oldArmsBand1;
		g_pQuickSlot = oldQuickSlot;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pTempInformation = oldTemp;
		g_pSkillManager = oldSkillManager;
		g_pSkillInfoTable = oldSkillInfo;
		g_pSkillAvailable = oldSkills;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) g_item_ref_point[i].Set(oldRefPoints[i][0], oldRefPoints[i][1]);
	}
	void Type(int type, CREATURETRIBE tribe)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, 0);
		testfw::MutableRow(creatureTypes, type).bMale = true;
	}
	void Class(int itemClass, int types, int width, int height, int race, int sound)
	{
		g_pItemTable->InitClass(itemClass, types);
		for (int type = 0; type < types; ++type)
		{
			testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(BYTE(width), BYTE(height));
			testfw::MutableRow(*g_pItemTable, itemClass, type).Race = BYTE(race);
			testfw::MutableRow(*g_pItemTable, itemClass, type).InventorySoundID = TYPE_SOUNDID(sound);
		}
	}
	ITEMTABLE_INFO& Row(int itemClass, int type = 0) { return testfw::MutableRow(*g_pItemTable, itemClass, type); }
	void Items()
	{
		const int all = FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS;
		// Potion type 0 is a slayer's, type 1 a vampire's.
		Class(ITEM_CLASS_POTION, 2, 1, 1, FLAG_RACE_SLAYER, kPotionSound);
		Row(ITEM_CLASS_POTION, 1).Race = FLAG_RACE_VAMPIRE;
		Class(ITEM_CLASS_PUPA, 1, 1, 1, FLAG_RACE_OUSTERS, kPupaSound);
		Class(ITEM_CLASS_BELT, 1, 1, 2, FLAG_RACE_SLAYER, 13);
		Row(ITEM_CLASS_BELT).Value3 = 3;
		Class(ITEM_CLASS_OUSTERS_ARMSBAND, 1, 1, 2, FLAG_RACE_OUSTERS, 14);
		Row(ITEM_CLASS_OUSTERS_ARMSBAND).Value3 = 2;
		Class(ITEM_CLASS_HELM, 1, 2, 2, FLAG_RACE_SLAYER, kHelmSound);
		Row(ITEM_CLASS_HELM).Price = 1000;
		Row(ITEM_CLASS_HELM).Value1 = 1000;
		Class(ITEM_CLASS_VAMPIRE_NECKLACE, 1, 1, 1, FLAG_RACE_VAMPIRE, kNecklaceSound);
		Class(ITEM_CLASS_OUSTERS_CIRCLET, 1, 1, 1, FLAG_RACE_OUSTERS, kCircletSound);
		Class(ITEM_CLASS_MONEY, 1, 1, 1, all, kMoneySound);
		Class(ITEM_CLASS_MAGAZINE, 1, 1, 1, FLAG_RACE_SLAYER, kMagazineSound);
		Class(ITEM_CLASS_SWORD, 1, 1, 3, FLAG_RACE_SLAYER, kSwordSound);
		Row(ITEM_CLASS_SWORD).Price = 1000;
		Row(ITEM_CLASS_SWORD).Value1 = 1000;
		Row(ITEM_CLASS_SWORD).SilverMax = 50;
		Class(ITEM_CLASS_KEY, 2, 1, 1, all, 21);
		Row(ITEM_CLASS_KEY, 1).ItemStyle = 1;
		// Vampire amulet type 0 is unique (style 1), so it is not "normal";
		// type 1 is normal. Key type 1 is a unique item of no refused class.
		Class(ITEM_CLASS_VAMPIRE_AMULET, 2, 1, 1, FLAG_RACE_VAMPIRE, 22);
		Row(ITEM_CLASS_VAMPIRE_AMULET, 0).ItemStyle = 1;
		Class(ITEM_CLASS_EVENT_GIFT_BOX, 3, 1, 1, all, 23);
		Class(ITEM_CLASS_EVENT_STAR, 5, 1, 1, all, 24);
		// The classes the storage refuses, each with the types it tells apart.
		Class(ITEM_CLASS_COUPLE_RING, 1, 1, 1, all, 25);
		Class(ITEM_CLASS_VAMPIRE_COUPLE_RING, 1, 1, 1, all, 26);
		Class(ITEM_CLASS_EVENT_TREE, 14, 1, 1, all, 27);
		Class(ITEM_CLASS_EVENT_ITEM, 1, 1, 1, all, 28);
		Class(ITEM_CLASS_CODE_SHEET, 1, 1, 1, all, 29);
		Class(ITEM_CLASS_LUCKY_BAG, 4, 1, 1, all, 30);
		// Row 0 is "no option" (no star colour); row 1 is a strength
		// option, whose star price is red (type 1).
		testfw::MutableRow(*g_pItemOptionTable, 0).Part = ITEMOPTION_TABLE::PART_HP;
		testfw::MutableRow(*g_pItemOptionTable, 1).Part = ITEMOPTION_TABLE::PART_STR;
	}
	void Strings()
	{
		InitGameStringTable();
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY, "cannot buy");
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_NO_MONEY, kNoMoney);
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_NO_STAR, "no star");
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_NO_SPACE, "no space");
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_SELL, "cannot sell");
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_REPAIR, "cannot repair");
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_SILVERING, "cannot silver");
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_STORAGE, "cannot store");
		g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_TRADE_NO_SPACE, "trade no space");
	}
	// Whether a container still holds the item.
	bool Owned(const MItem* pItem)
	{
		const TYPE_OBJECTID id = pItem->GetID();
		if (inventory.MItemManager::GetItem(id) == pItem) return true;
		if (slayerGear.MItemManager::GetItem(id) == pItem) return true;
		if (vampireGear.MItemManager::GetItem(id) == pItem) return true;
		if (oustersGear.MItemManager::GetItem(id) == pItem) return true;
		if (belt && belt->MItemManager::GetItem(id) == pItem) return true;
		if (band1 && band1->MItemManager::GetItem(id) == pItem) return true;
		if (band2 && band2->MItemManager::GetItem(id) == pItem) return true;
		const unsigned int rack = storage.GetCurrent();
		bool stored = false;
		for (unsigned int r = 0; r < storage.GetSize(); ++r)
		{
			storage.SetCurrent(r);
			for (int s = 0; s < STORAGE_SLOT; ++s) stored = stored || storage.GetItem(s) == pItem;
		}
		storage.SetCurrent(rack);
		return stored;
	}
	void Run(DWORD message, intptr_t left = 0, intptr_t right = 0, void* pVoid = NULL) { manager.Execute(message, left, right, pVoid); }
	void Vampire() { me->SetCreatureType(CREATURETYPE_VAMPIRE_MALE1); }
	void Ousters() { me->SetCreatureType(CREATURETYPE_OUSTERS); }
	MBelt* Belt()
	{
		belt.reset(new MBelt);
		belt->SetItemType(0);
		g_pQuickSlot = belt.get();
		return belt.get();
	}
	// The two arms bands of two pockets each.
	void Bands()
	{
		band1.reset(new MOustersArmsBand);
		band1->SetItemType(0);
		band2.reset(new MOustersArmsBand);
		band2->SetItemType(0);
		g_pArmsBand1 = band1.get();
		g_pArmsBand2 = band2.get();
	}
	MTradeManager* Trade(bool acceptMine = false, bool acceptOther = false)
	{
		g_pTradeManager = new MTradeManager;
		g_pTradeManager->Init();
		g_pTradeManager->SetOtherID(kOther);
		if (acceptMine) g_pTradeManager->AcceptMyTrade();
		if (acceptOther) g_pTradeManager->AcceptOtherTrade();
		return g_pTradeManager;
	}
	// Whether the trade's OK waits (the accept delay was restarted).
	bool Waits() const { return !g_pTradeManager->IsAcceptTime(); }
	void Hold(MItem* pItem) { mouse.PickUpItem(pItem); }
	MItem* Held() { return mouse.GetPickUpItem(); }
	bool Waiting() const { return temp.GetMode() != TempInformation::MODE_NULL; }
	void Busy() { temp.SetMode(TempInformation::MODE_SHOP_BUY); }
	// The skill set, given an entry the skill check would clear.
	void Skills() { skills.clear(); skills.insert(MSkillSet::value_type(SKILL_ATTACK_MELEE, SKILLID_NODE(SKILL_ATTACK_MELEE))); }
};

template <class T> T* Make(TYPE_OBJECTID id, TYPE_ITEMTYPE type = 0, int number = 1)
{
	T* pItem = new T;
	pItem->SetItemType(type);
	pItem->SetID(id);
	pItem->SetNumber(TYPE_ITEM_NUMBER(number));
	return pItem;
}

std::string Popup(const char* text) { return std::string("popup|") + text; }

} // namespace

//----------------------------------------------------------------------
// UI_ITEM_PICKUP_FROM_QUICKSLOT (L4028): left = the slot, void_ptr = the
// item. In MODE_NULL it tells the server, takes the item off the belt or
// the arms band and puts it on the mouse.
//----------------------------------------------------------------------
TEST(UIMessages3QuickSlotPickUp, ASlayerLiftsTheItemOffHisBelt)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(41);
	CHECK(scene.Belt()->AddItem(pPotion, 2));
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 2, 0, pPotion);
	CHECK(Same({"quickslot to mouse|41|2", "sound|" + N(kPotionSound)}));
	CHECK(scene.belt->GetItem(2) == NULL);
	CHECK(scene.Held() == pPotion);
	CHECK(!scene.Waiting());
}

TEST(UIMessages3QuickSlotPickUp, NothingWhileWaitingDeadOrOutOfTheGame)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(41);
	CHECK(scene.Belt()->AddItem(pPotion, 2));
	scene.Busy();
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 2, 0, pPotion);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_MAINMENU;
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 2, 0, pPotion);
	g_Mode = MODE_GAME;
	scene.me->Kill();
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 2, 0, pPotion);
	CHECK(calls.empty());
	CHECK(scene.belt->GetItem(2) == pPotion);
	CHECK(scene.Held() == NULL);
}

TEST(UIMessages3QuickSlotPickUp, AnOustersPocketBelowTheFirstBandsSizeIsTheFirstBand)
{
	Scene scene;
	scene.Ousters();
	scene.Bands();
	MOustersPupa* pFirst = Make<MOustersPupa>(51);
	CHECK(scene.band1->AddItem(pFirst, 1));
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 1, 0, pFirst);
	CHECK(Same({"quickslot to mouse|51|1", "sound|" + N(kPupaSound)}));
	CHECK(scene.band1->GetItem(1) == NULL);
	CHECK(scene.Held() == pFirst);
}

TEST(UIMessages3QuickSlotPickUp, FromTheFirstBandsSizeOnItIsTheSecondBandsSlotPlusThree)
{
	Scene scene;
	scene.Ousters();
	scene.Bands();
	MOustersPupa* pSecond = Make<MOustersPupa>(52);
	CHECK(scene.band2->AddItem(pSecond, 1));
	// Pocket 2 is the first band's size: the second band.
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 2, 0, pSecond);
	CHECK(Same({"quickslot to mouse|52|4", "sound|" + N(kPupaSound)}));
	CHECK(scene.band2->GetItem(1) == NULL);
	CHECK(scene.Held() == pSecond);
}

TEST(UIMessages3QuickSlotPickUp, WithoutAFirstBandTheSecondIsUsedAndWithoutEitherNothing)
{
	Scene scene;
	scene.Ousters();
	scene.Bands();
	MOustersPupa* pSecond = Make<MOustersPupa>(52);
	CHECK(scene.band2->AddItem(pSecond, 0));
	g_pArmsBand1 = NULL;
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 0, 0, pSecond);
	CHECK(Same({"quickslot to mouse|52|3", "sound|" + N(kPupaSound)}));
	CHECK(scene.band2->GetItem(0) == NULL);
	CHECK(scene.Held() == pSecond);
	// No band at all: nothing.
	scene.mouse.DropItem();
	CHECK(scene.band2->AddItem(pSecond, 0));
	g_pArmsBand2 = NULL;
	calls.clear();
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 0, 0, pSecond);
	CHECK(calls.empty());
	CHECK(scene.band2->GetItem(0) == pSecond);
	CHECK(scene.Held() == NULL);
}

TEST(UIMessages3QuickSlotPickUp, TheBandComesFromThePocketAndTheSlotFromTheItem)
{
	Scene scene;
	scene.Ousters();
	scene.Bands();
	MOustersPupa* pSecond = Make<MOustersPupa>(52);
	CHECK(scene.band2->AddItem(pSecond, 1));
	// Recorded defect (L4052-4078): a second-band item lifted with a
	// first-band pocket goes out as the first band's slot 1, is removed
	// from the first band (where it is not) and so stays in the second
	// band while it is on the mouse.
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 0, 0, pSecond);
	CHECK(Same({"quickslot to mouse|52|1", "sound|" + N(kPupaSound)}));
	CHECK(scene.band2->GetItem(1) == pSecond);
	CHECK(scene.Held() == pSecond);
}

TEST(UIMessages3QuickSlotPickUp, AVampireHasNoSlotsSoTheItemIsOnlyLifted)
{
	Scene scene;
	scene.Vampire();
	scene.Belt();
	MPotion* pPotion = Make<MPotion>(43, 1);
	pPotion->SetItemSlot(1);
	// Whatever the belt holds, a vampire's item is taken from nowhere.
	scene.Run(UI_ITEM_PICKUP_FROM_QUICKSLOT, 1, 0, pPotion);
	CHECK(Same({"quickslot to mouse|43|1", "sound|" + N(kPotionSound)}));
	CHECK(scene.Held() == pPotion);
}

//----------------------------------------------------------------------
// UI_ITEM_PICKUP_FROM_INVENTORY (L4103): left/right = the grid cell the
// UI took it from (kept only for the trade's check), void_ptr = the item.
//----------------------------------------------------------------------
TEST(UIMessages3InventoryPickUp, OutsideATradeTheItemIsLiftedAndTheServerTold)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(61);
	CHECK(scene.inventory.AddItem(pPotion, 3, 2));
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 7, 9, pPotion);
	// The packet carries the item's own cell, not left/right.
	CHECK(Same({"sound|" + N(kPotionSound), "inventory to mouse|61|3|2|0"}));
	CHECK(scene.Held() == pPotion);
	CHECK(!scene.Waiting());
	CHECK(scene.me->IsItemCheckBufferNULL());
	// The handler leaves the inventory to the UI.
	CHECK(scene.inventory.GetItem(3, 2) == pPotion);
}

TEST(UIMessages3InventoryPickUp, ATradeNotAcceptedLiftsAsUsual)
{
	Scene scene;
	scene.Trade(false, true);
	MPotion* pPotion = Make<MPotion>(61);
	pPotion->SetTrade();
	CHECK(scene.inventory.AddItem(pPotion, 3, 2));
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 3, 2, pPotion);
	// With no trade window open even a marked item goes without a trade
	// remove, and the other side's OK stays.
	CHECK(Same({"sound|" + N(kPotionSound), "inventory to mouse|61|3|2|0"}));
	CHECK(scene.Held() == pPotion);
	CHECK(g_pTradeManager->IsAcceptOtherTrade());
	CHECK(!scene.Waits());
}

TEST(UIMessages3InventoryPickUp, AfterMyOKTheServerChecksTheLiftFirst)
{
	Scene scene;
	scene.Trade(true);
	MPotion* pPotion = Make<MPotion>(61);
	CHECK(scene.inventory.AddItem(pPotion, 3, 2));
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 7, 9, pPotion);
	CHECK(Same({"inventory to mouse|61|3|2|0"}));
	CHECK(scene.Held() == NULL);
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY), int(scene.temp.GetMode()));
	CHECK_EQ(61, int(scene.temp.Value1));
	CHECK_EQ(7, int(scene.temp.Value2));
	CHECK_EQ(9, int(scene.temp.Value3));
	CHECK(scene.temp.pValue == pPotion);
	CHECK(scene.me->GetItemCheckBuffer() == pPotion);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_PICKUP_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	CHECK(!scene.Waits());
}

TEST(UIMessages3InventoryPickUp, AfterMyOKAMarkedItemIsTakenOffTheTradeToo)
{
	Scene scene;
	scene.Trade(true);
	MPotion* pPotion = Make<MPotion>(61);
	pPotion->SetTrade();
	CHECK(scene.inventory.AddItem(pPotion, 3, 2));
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 3, 2, pPotion);
	CHECK(Same({"trade remove|500|61", "inventory to mouse|61|3|2|0"}));
	CHECK(scene.Waits());
	CHECK(scene.Held() == NULL);
	CHECK(pPotion->IsTrade());
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY), int(scene.temp.GetMode()));
}

TEST(UIMessages3InventoryPickUp, AfterMyOKNothingWhileARequestOrACheckWaits)
{
	Scene scene;
	scene.Trade(true);
	MPotion* pPotion = Make<MPotion>(61);
	pPotion->SetTrade();
	CHECK(scene.inventory.AddItem(pPotion, 3, 2));
	scene.Busy();
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 3, 2, pPotion);
	CHECK_EQ(int(TempInformation::MODE_SHOP_BUY), int(scene.temp.GetMode()));
	scene.temp.SetMode(TempInformation::MODE_NULL);
	MPotion* pOther = Make<MPotion>(62);
	CHECK(scene.inventory.AddItem(pOther, 0, 0));
	scene.me->SetItemCheckBuffer(pOther, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 3, 2, pPotion);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
	CHECK(scene.me->GetItemCheckBuffer() == pOther);
	CHECK(scene.Held() == NULL);
	CHECK(!scene.Waits());
	// Dead or out of the game: nothing either.
	scene.me->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_NULL);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 3, 2, pPotion);
	g_Mode = MODE_GAME;
	scene.me->Kill();
	scene.Run(UI_ITEM_PICKUP_FROM_INVENTORY, 3, 2, pPotion);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

//----------------------------------------------------------------------
// UI_ITEM_PICKUP_FROM_GEAR (L4240): left = the gear slot, void_ptr = the
// worn item. The look comes off, the slot is emptied, what came out is
// lifted and the server told; the skills are checked again.
//----------------------------------------------------------------------
TEST(UIMessages3GearPickUp, ASlayerTakesOffHisHelm)
{
	Scene scene;
	scene.Skills();
	MHelm* pHelm = Make<MHelm>(71);
	CHECK(scene.slayerGear.AddItem(pHelm, MSlayerGear::GEAR_SLAYER_HELM));
	scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MSlayerGear::GEAR_SLAYER_HELM, 0, pHelm);
	CHECK(Same({"sound|" + N(kHelmSound), "gear to mouse|71|" + N(MSlayerGear::GEAR_SLAYER_HELM)}));
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == NULL);
	CHECK(scene.Held() == pHelm);
	CHECK(scene.skills.empty());
}

TEST(UIMessages3GearPickUp, AVampireAndAnOustersTakeFromTheirOwnGear)
{
	{
		Scene scene;
		scene.Vampire();
		scene.Skills();
		MVampireNecklace* pNecklace = Make<MVampireNecklace>(72);
		CHECK(scene.vampireGear.AddItem(pNecklace, MVampireGear::GEAR_VAMPIRE_NECKLACE));
		scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MVampireGear::GEAR_VAMPIRE_NECKLACE, 0, pNecklace);
		CHECK(Same({"sound|" + N(kNecklaceSound), "gear to mouse|72|" + N(MVampireGear::GEAR_VAMPIRE_NECKLACE)}));
		CHECK(scene.vampireGear.GetItem(MVampireGear::GEAR_VAMPIRE_NECKLACE) == NULL);
		CHECK(scene.Held() == pNecklace);
		CHECK(scene.skills.empty());
	}
	{
		Scene scene;
		scene.Ousters();
		scene.Skills();
		MOustersCirclet* pCirclet = Make<MOustersCirclet>(73);
		CHECK(scene.oustersGear.AddItem(pCirclet, MOustersGear::GEAR_OUSTERS_CIRCLET));
		scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MOustersGear::GEAR_OUSTERS_CIRCLET, 0, pCirclet);
		CHECK(Same({"sound|" + N(kCircletSound), "gear to mouse|73|" + N(MOustersGear::GEAR_OUSTERS_CIRCLET)}));
		CHECK(scene.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_CIRCLET) == NULL);
		CHECK(scene.Held() == pCirclet);
		CHECK(scene.skills.empty());
	}
}

TEST(UIMessages3GearPickUp, AnEmptyOrUnknownSlotLiftsNothingButTheSkillsAreChecked)
{
	Scene scene;
	MHelm* pHelm = Make<MHelm>(71);
	CHECK(scene.slayerGear.AddItem(pHelm, MSlayerGear::GEAR_SLAYER_HELM));
	// Recorded defect (L4271-4298): the look comes off before the slot is
	// checked against the item, so a slot that does not hold it takes the
	// helm's look off while the helm stays worn.
	for (intptr_t slot : {intptr_t(MSlayerGear::GEAR_SLAYER_BELT), intptr_t(-1), intptr_t(MSlayerGear::MAX_GEAR_SLAYER), intptr_t(999)})
	{
		scene.Skills();
		scene.Run(UI_ITEM_PICKUP_FROM_GEAR, slot, 0, pHelm);
		CHECK(calls.empty());
		CHECK(scene.skills.empty());
		CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == pHelm);
		CHECK(scene.Held() == NULL);
	}
}

TEST(UIMessages3GearPickUp, NothingInWatchModeWithoutAnItemOrOnTheMove)
{
	Scene scene;
	MHelm* pHelm = Make<MHelm>(71);
	CHECK(scene.slayerGear.AddItem(pHelm, MSlayerGear::GEAR_SLAYER_HELM));
	auto untouched = [&]() {
		return calls.empty() && !scene.skills.empty() && scene.Held() == NULL
			&& scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == pHelm;
	};
	scene.Skills();
	g_bWatchMode = true;
	scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MSlayerGear::GEAR_SLAYER_HELM, 0, pHelm);
	CHECK(untouched());
	g_bWatchMode = false;
	scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MSlayerGear::GEAR_SLAYER_HELM, 0, NULL);
	CHECK(untouched());
	// The look comes off only standing.
	scene.me->Walk();
	scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MSlayerGear::GEAR_SLAYER_HELM, 0, pHelm);
	CHECK(untouched());
	scene.me->Stand();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MSlayerGear::GEAR_SLAYER_HELM, 0, pHelm);
	CHECK(untouched());
	g_Mode = MODE_GAME;
	scene.me->Kill();
	scene.Run(UI_ITEM_PICKUP_FROM_GEAR, MSlayerGear::GEAR_SLAYER_HELM, 0, pHelm);
	CHECK(untouched());
}

//----------------------------------------------------------------------
// UI_ITEM_USE (L4347): left = the item's ID in the inventory. An item of
// the player's own race is used when no check waits.
//----------------------------------------------------------------------
TEST(UIMessages3ItemUse, TheInventorysItemOfThePlayersRaceIsUsed)
{
	Scene scene;
	MPotion* pSlayers = Make<MPotion>(81, 0);
	MPotion* pVampires = Make<MPotion>(82, 1);
	CHECK(scene.inventory.AddItem(pSlayers, 0, 0));
	CHECK(scene.inventory.AddItem(pVampires, 1, 0));
	scene.Run(UI_ITEM_USE, 81, 0, pVampires);
	scene.Run(UI_ITEM_USE, 82);
	CHECK(Same({"use|81"}));
	calls.clear();
	scene.Vampire();
	scene.Run(UI_ITEM_USE, 81);
	scene.Run(UI_ITEM_USE, 82);
	CHECK(Same({"use|82"}));
	calls.clear();
	scene.Ousters();
	MOustersPupa* pPupa = Make<MOustersPupa>(83);
	CHECK(scene.inventory.AddItem(pPupa, 2, 0));
	scene.Run(UI_ITEM_USE, 81);
	scene.Run(UI_ITEM_USE, 83);
	CHECK(Same({"use|83"}));
}

TEST(UIMessages3ItemUse, NothingForAnUnknownIDWhileACheckWaitsOrDead)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(81);
	CHECK(scene.inventory.AddItem(pPotion, 0, 0));
	scene.Run(UI_ITEM_USE, 80);
	scene.me->SetItemCheckBuffer(pPotion, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	scene.Run(UI_ITEM_USE, 81);
	scene.me->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_NULL);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_USE, 81);
	g_Mode = MODE_GAME;
	scene.me->Kill();
	scene.Run(UI_ITEM_USE, 81);
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// UI_ITEM_USE_QUICKSLOT (L4397): left = the pocket. A slayer's belt; an
// ousters' first band below its size, the second band past it.
//----------------------------------------------------------------------
TEST(UIMessages3QuickSlotUse, ASlayerUsesTheBeltsPocket)
{
	Scene scene;
	QuickPotion* pPotion = Make<QuickPotion>(91);
	CHECK(scene.Belt()->AddItem(pPotion, 1));
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 0);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 2);
	CHECK(Same({"quick use|91"}));
	// Recorded defect (L4442): the pocket is cut to a byte, so 257 is 1.
	calls.clear();
	scene.Run(UI_ITEM_USE_QUICKSLOT, 257);
	CHECK(Same({"quick use|91"}));
}

TEST(UIMessages3QuickSlotUse, AnOustersPocketPastTheFirstBandIsTheSecondBands)
{
	Scene scene;
	scene.Ousters();
	scene.Bands();
	CHECK(scene.band1->AddItem(Make<QuickPotion>(97), 0));
	CHECK(scene.band1->AddItem(Make<QuickPupa>(92), 1));
	CHECK(scene.band2->AddItem(Make<QuickPupa>(93), 0));
	CHECK(scene.band2->AddItem(Make<QuickPupa>(94), 1));
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 2);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 3);
	// An ousters does not use a slayer's potion.
	scene.Run(UI_ITEM_USE_QUICKSLOT, 0);
	CHECK(Same({"quick use|92", "quick use|93", "quick use|94"}));
	// Past the first band with no second: nothing.
	calls.clear();
	g_pArmsBand2 = NULL;
	scene.Run(UI_ITEM_USE_QUICKSLOT, 2);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 3);
	CHECK(calls.empty());
	g_pArmsBand2 = scene.band2.get();
	// Without the first band the pocket is the second band's own.
	calls.clear();
	g_pArmsBand1 = NULL;
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 3);
	CHECK(Same({"quick use|94"}));
	// Without either band, nothing.
	calls.clear();
	g_pArmsBand2 = NULL;
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	CHECK(calls.empty());
}

TEST(UIMessages3QuickSlotUse, NothingForAVampireAnotherRaceOrWhileACheckWaits)
{
	Scene scene;
	QuickPotion* pVampires = Make<QuickPotion>(95, 1);
	QuickPotion* pSlayers = Make<QuickPotion>(96, 0);
	CHECK(scene.Belt()->AddItem(pVampires, 0));
	CHECK(scene.belt->AddItem(pSlayers, 1));
	// A slayer does not use a vampire's potion.
	scene.Run(UI_ITEM_USE_QUICKSLOT, 0);
	// A slayer without a belt does nothing.
	g_pQuickSlot = NULL;
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	g_pQuickSlot = scene.belt.get();
	// A check waiting: nothing.
	scene.me->SetItemCheckBuffer(pSlayers, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	scene.me->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_NULL);
	// Dead or out of the game: nothing.
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	g_Mode = MODE_GAME;
	scene.me->Kill();
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	scene.me->Revive();
	CHECK(calls.empty());
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	CHECK(Same({"quick use|96"}));
	calls.clear();
	// A vampire has no quick slots, belt or not.
	scene.Vampire();
	scene.Run(UI_ITEM_USE_QUICKSLOT, 0);
	scene.Run(UI_ITEM_USE_QUICKSLOT, 1);
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// UI_ITEM_INSERT_FROM_INVENTORY (L4497): the held pile onto a pile of the
// same class and type in the inventory (void_ptr), merged at once unless
// my OK in a trade makes the server check it first.
//----------------------------------------------------------------------
TEST(UIMessages3InventoryInsert, TheHeldPileJoinsThePileAndIsGone)
{
	Scene scene;
	MPotion* pTarget = Make<MPotion>(101, 0, 10);
	CHECK(scene.inventory.AddItem(pTarget, 4, 1));
	scene.Hold(Make<MPotion>(102, 0, 5));
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 4, 1, pTarget);
	CHECK(Same({"sound|" + N(kPotionSound), "mouse to inventory|102|4|1|0"}));
	CHECK_EQ(15, int(pTarget->GetNumber()));
	CHECK(scene.Held() == NULL);
	CHECK(!scene.Waiting());
	CHECK(scene.me->IsItemCheckBufferNULL());
}

TEST(UIMessages3InventoryInsert, UpToTheMaximumTheRestStaysHeld)
{
	Scene scene;
	MPotion* pTarget = Make<MPotion>(101, 0, 28);
	CHECK(scene.inventory.AddItem(pTarget, 4, 1));
	MPotion* pHeld = Make<MPotion>(102, 0, 5);
	scene.Hold(pHeld);
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 4, 1, pTarget);
	CHECK(Same({"sound|" + N(kPotionSound), "mouse to inventory|102|4|1|0"}));
	CHECK_EQ(MAX_POTION_PILE_NUMBER, int(pTarget->GetNumber()));
	CHECK_EQ(3, int(pHeld->GetNumber()));
	CHECK(scene.Held() == pHeld);
	// Exactly the maximum is all of it.
	calls.clear();
	pTarget->SetNumber(27);
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 4, 1, pTarget);
	CHECK_EQ(MAX_POTION_PILE_NUMBER, int(pTarget->GetNumber()));
	CHECK(scene.Held() == NULL);
	CHECK(Same({"sound|" + N(kPotionSound), "mouse to inventory|102|4|1|0"}));
}

TEST(UIMessages3InventoryInsert, MoneyPilesStopAt99UnlessTheOtherSideAccepted)
{
	Scene scene;
	MMoney* pTarget = Make<MMoney>(103, 0, 90);
	CHECK(scene.inventory.AddItem(pTarget, 0, 0));
	MMoney* pHeld = Make<MMoney>(104, 0, 20);
	scene.Hold(pHeld);
	// Recorded defect (L4577-4592): money stops at a hard-coded 99 while
	// the other side has not accepted, at its own maximum when it has.
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, pTarget);
	CHECK(Same({"sound|" + N(kMoneySound), "mouse to inventory|104|0|0|0"}));
	CHECK_EQ(99, int(pTarget->GetNumber()));
	CHECK_EQ(11, int(pHeld->GetNumber()));
	CHECK(scene.Held() == pHeld);
	// 99 exactly takes all.
	pTarget->SetNumber(88);
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, pTarget);
	CHECK_EQ(99, int(pTarget->GetNumber()));
	CHECK(scene.Held() == NULL);
	// The other side's OK lifts the cap.
	calls.clear();
	scene.Trade(false, true);
	pTarget->SetNumber(90);
	scene.Hold(Make<MMoney>(105, 0, 20));
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, pTarget);
	CHECK(Same({"sound|" + N(kMoneySound), "mouse to inventory|105|0|0|0"}));
	CHECK_EQ(110, int(pTarget->GetNumber()));
	CHECK(scene.Held() == NULL);
	CHECK(g_pTradeManager->IsAcceptOtherTrade());
}

TEST(UIMessages3InventoryInsert, OnlyAPileOfTheSameKindBelowItsMaximumTakesIt)
{
	Scene scene;
	MPotion* pHeld = Make<MPotion>(102, 0, 5);
	MPotion* pTarget = Make<MPotion>(101, 0, 10);
	MPotion* pOtherType = Make<MPotion>(106, 1, 10);
	MOustersPupa* pOtherClass = Make<MOustersPupa>(107, 0, 10);
	MHelm* pHelm = Make<MHelm>(108);
	MPotion* pFull = Make<MPotion>(109, 0, MAX_POTION_PILE_NUMBER);
	MPotion* pQuest = Make<MPotion>(110, 0, 10);
	pQuest->SetQuestFlag();
	CHECK(scene.inventory.AddItem(pTarget, 0, 0));
	CHECK(scene.inventory.AddItem(pOtherType, 1, 0));
	CHECK(scene.inventory.AddItem(pOtherClass, 2, 0));
	CHECK(scene.inventory.AddItem(pHelm, 3, 0));
	CHECK(scene.inventory.AddItem(pFull, 5, 0));
	CHECK(scene.inventory.AddItem(pQuest, 6, 0));
	// Nothing held: nothing.
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, pTarget);
	scene.Hold(pHeld);
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, NULL);
	for (MItem* pItem : std::vector<MItem*>{pOtherType, pOtherClass, pHelm, pFull, pQuest})
		scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, pItem);
	CHECK(calls.empty());
	CHECK_EQ(10, int(pTarget->GetNumber()));
	CHECK_EQ(10, int(pOtherType->GetNumber()));
	CHECK_EQ(MAX_POTION_PILE_NUMBER, int(pFull->GetNumber()));
	CHECK_EQ(10, int(pQuest->GetNumber()));
	CHECK_EQ(5, int(pHeld->GetNumber()));
	CHECK(scene.Held() == pHeld);
	// Out of the game or dead: nothing.
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, pTarget);
	g_Mode = MODE_GAME;
	scene.me->Kill();
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 0, 0, pTarget);
	CHECK(calls.empty());
	CHECK_EQ(10, int(pTarget->GetNumber()));
}

TEST(UIMessages3InventoryInsert, AfterMyOKTheServerChecksTheMergeFirst)
{
	Scene scene;
	scene.Trade(true);
	MPotion* pTarget = Make<MPotion>(101, 0, 10);
	CHECK(scene.inventory.AddItem(pTarget, 4, 1));
	MPotion* pHeld = Make<MPotion>(102, 0, 5);
	scene.Hold(pHeld);
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 6, 2, pTarget);
	CHECK(Same({"mouse to inventory|102|4|1|0"}));
	CHECK_EQ(10, int(pTarget->GetNumber()));
	CHECK(scene.Held() == pHeld);
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_INSERT_TO_INVENTORY), int(scene.temp.GetMode()));
	CHECK_EQ(101, int(scene.temp.Value1));
	CHECK_EQ(6, int(scene.temp.Value2));
	CHECK_EQ(2, int(scene.temp.Value3));
	CHECK(scene.temp.pValue == pTarget);
	CHECK(scene.me->GetItemCheckBuffer() == pTarget);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	// With the check waiting, a second one does nothing.
	calls.clear();
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 6, 2, pTarget);
	CHECK(calls.empty());
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Run(UI_ITEM_INSERT_FROM_INVENTORY, 6, 2, pTarget);
	CHECK(calls.empty());
	CHECK_EQ(10, int(pTarget->GetNumber()));
}

//----------------------------------------------------------------------
// UI_ITEM_INSERT_FROM_GEAR (L4692): only a magazine (void_ptr) takes the
// held item, and the server checks it.
//----------------------------------------------------------------------
TEST(UIMessages3GearInsert, AMagazineTakesTheHeldItemOnceTheServerAgrees)
{
	Scene scene;
	MMagazine* pTarget = Make<MMagazine>(111, 0, 10);
	pTarget->SetItemSlot(3);
	scene.loose.emplace_back(pTarget);
	MMagazine* pHeld = Make<MMagazine>(112, 0, 5);
	scene.Hold(pHeld);
	scene.Run(UI_ITEM_INSERT_FROM_GEAR, 9, 9, pTarget);
	CHECK(Same({"mouse to gear|112|3"}));
	CHECK(scene.me->GetItemCheckBuffer() == pTarget);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_INSERT_FROM_GEAR), int(scene.me->GetItemCheckBufferStatus()));
	CHECK_EQ(10, int(pTarget->GetNumber()));
	CHECK(scene.Held() == pHeld);
	// The check waiting: nothing more.
	calls.clear();
	scene.Run(UI_ITEM_INSERT_FROM_GEAR, 9, 9, pTarget);
	CHECK(calls.empty());
}

TEST(UIMessages3GearInsert, NothingButAMagazineNothingInWatchMode)
{
	Scene scene;
	MHelm* pHelm = Make<MHelm>(113);
	scene.loose.emplace_back(pHelm);
	MMagazine* pTarget = Make<MMagazine>(111);
	scene.loose.emplace_back(pTarget);
	scene.Hold(Make<MMagazine>(112));
	scene.Run(UI_ITEM_INSERT_FROM_GEAR, 0, 0, pHelm);
	g_bWatchMode = true;
	scene.Run(UI_ITEM_INSERT_FROM_GEAR, 0, 0, pTarget);
	g_bWatchMode = false;
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_INSERT_FROM_GEAR, 0, 0, pTarget);
	g_Mode = MODE_GAME;
	scene.me->Kill();
	scene.Run(UI_ITEM_INSERT_FROM_GEAR, 0, 0, pTarget);
	CHECK(calls.empty());
	CHECK(scene.me->IsItemCheckBufferNULL());
}

//----------------------------------------------------------------------
// UI_ITEM_INSERT_FROM_QUICKSLOT (L4759): the held pile onto the pocket's
// item (void_ptr) of the same class and type: the server is told and the
// counts merge at once.
//----------------------------------------------------------------------
TEST(UIMessages3QuickSlotInsert, ASlayersPocketTakesThePile)
{
	Scene scene;
	MPotion* pTarget = Make<MPotion>(121, 0, 10);
	CHECK(scene.Belt()->AddItem(pTarget, 2));
	scene.Hold(Make<MPotion>(122, 0, 5));
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	CHECK(Same({"mouse to quickslot|122|2", "sound|" + N(kPotionSound)}));
	CHECK_EQ(15, int(pTarget->GetNumber()));
	CHECK(scene.Held() == NULL);
	// Past the maximum the rest stays held.
	calls.clear();
	pTarget->SetNumber(28);
	MPotion* pHeld = Make<MPotion>(123, 0, 5);
	scene.Hold(pHeld);
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	CHECK(Same({"mouse to quickslot|123|2", "sound|" + N(kPotionSound)}));
	CHECK_EQ(MAX_POTION_PILE_NUMBER, int(pTarget->GetNumber()));
	CHECK_EQ(3, int(pHeld->GetNumber()));
	CHECK(scene.Held() == pHeld);
	// Recorded defect (L4780-4827): a full pocket is not refused; the
	// server is told and nothing moves.
	calls.clear();
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	CHECK(Same({"mouse to quickslot|123|2", "sound|" + N(kPotionSound)}));
	CHECK_EQ(MAX_POTION_PILE_NUMBER, int(pTarget->GetNumber()));
	CHECK_EQ(3, int(pHeld->GetNumber()));
}

TEST(UIMessages3QuickSlotInsert, AnOustersSlotIsThePocketOnTheFirstBandElseThePocketsSlotPlusThree)
{
	Scene scene;
	scene.Ousters();
	scene.Bands();
	MOustersPupa* pFirst = Make<MOustersPupa>(124, 0, 10);
	MOustersPupa* pSecond = Make<MOustersPupa>(125, 0, 10);
	CHECK(scene.band1->AddItem(pFirst, 1));
	CHECK(scene.band2->AddItem(pSecond, 1));
	scene.Hold(Make<MOustersPupa>(126, 0, 1));
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 1, 0, pFirst);
	scene.Hold(Make<MOustersPupa>(127, 0, 1));
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 2, 0, pSecond);
	g_pArmsBand1 = NULL;
	scene.Hold(Make<MOustersPupa>(128, 0, 1));
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pSecond);
	const std::string sound = "sound|" + N(kPupaSound);
	CHECK(Same({"mouse to quickslot|126|1", sound, "mouse to quickslot|127|4", sound, "mouse to quickslot|128|4", sound}));
	CHECK_EQ(11, int(pFirst->GetNumber()));
	CHECK_EQ(12, int(pSecond->GetNumber()));
}

TEST(UIMessages3QuickSlotInsert, NothingForAnotherKindOrWhileACheckWaits)
{
	Scene scene;
	MPotion* pTarget = Make<MPotion>(121, 0, 10);
	CHECK(scene.Belt()->AddItem(pTarget, 2));
	// Another class of the same type, then another type of the class.
	MOustersPupa* pPupa = Make<MOustersPupa>(129, 0, 5);
	scene.Hold(pPupa);
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	scene.mouse.DropItem();
	scene.loose.emplace_back(pPupa);
	MPotion* pHeld = Make<MPotion>(122, 1, 5);
	scene.Hold(pHeld);
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	pHeld->SetItemType(0);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	g_Mode = MODE_GAME;
	scene.me->SetItemCheckBuffer(pTarget, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	scene.me->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_NULL);
	scene.me->Kill();
	scene.Run(UI_ITEM_INSERT_FROM_QUICKSLOT, 0, 0, pTarget);
	CHECK(calls.empty());
	CHECK_EQ(10, int(pTarget->GetNumber()));
	CHECK_EQ(5, int(pHeld->GetNumber()));
	CHECK(scene.Held() == pHeld);
}

//----------------------------------------------------------------------
// The NPC's windows that end back at its talk window: UI_CLOSE_SHOP
// (L4848), UI_ITEM_SELL_FINISHED (L5058), UI_ITEM_REPAIR_FINISHED
// (L6170) and UI_ITEM_SILVERING_FINISHED (L6200) show it again when no
// request waits for the server.
//----------------------------------------------------------------------
TEST(UIMessages3Talk, ClosingTheNPCsWindowsShowsTheTalkAgainUnlessARequestWaits)
{
	Scene scene;
	for (DWORD message : {DWORD(UI_CLOSE_SHOP), DWORD(UI_ITEM_SELL_FINISHED), DWORD(UI_ITEM_REPAIR_FINISHED), DWORD(UI_ITEM_SILVERING_FINISHED)})
	{
		calls.clear();
		scene.Run(message);
		CHECK(Same({"start 1"}));
		calls.clear();
		scene.Busy();
		scene.Run(message);
		scene.temp.SetMode(TempInformation::MODE_NULL);
		g_Mode = MODE_WAIT_UPDATEINFO;
		scene.Run(message);
		g_Mode = MODE_GAME;
		CHECK(calls.empty());
	}
}

//----------------------------------------------------------------------
// UI_BUY_ITEM (L4886): left = the shelf slot, right = how many, void_ptr
// = the shop. A normal shop sells for money (with the castle's tax), an
// event shop for stars of the item's option colour; the item goes where
// the inventory has room for that many, and the request is remembered.
//----------------------------------------------------------------------
namespace {
struct ShopScene : Scene
{
	MPotion* pWare = nullptr;
	ShopScene()
	{
		shop.reset(new MShop);
		shop->Init(3);
		MShopShelf* pShelf = new MShopFixedShelf;
		pWare = Make<MPotion>(131);
		CHECK(pShelf->SetItem(4, pWare));
		CHECK(shop->SetShelf(1, pShelf));
		shop->SetCurrent(1);
		// A helm in the corner, so the fit is not the first cell, and a
		// pile that takes up to two more of the ware.
		CHECK(inventory.AddItem(Make<MHelm>(132), 0, 0));
		CHECK(inventory.AddItem(Make<MPotion>(139, 0, MAX_POTION_PILE_NUMBER - 2), 5, 4));
	}
	// Where the inventory has room for count of the item.
	POINT Fit(MItem* pItem, int count)
	{
		const TYPE_ITEM_NUMBER old = pItem->GetNumber();
		pItem->SetNumber(TYPE_ITEM_NUMBER(count));
		POINT point = {-1, -1};
		CHECK(inventory.GetFitPosition(pItem, point));
		pItem->SetNumber(old);
		return point;
	}
};
} // namespace

TEST(UIMessages3Buy, WithTheMoneyTheRequestGoesOutAndIsRemembered)
{
	ShopScene scene;
	const unsigned price = scene.prices.GetPurchasePrice(scene.pWare, 3);
	CHECK(price > 0);
	// The fit is for the count bought: two would join the pile, three not.
	const POINT fit = scene.Fit(scene.pWare, 3);
	CHECK(fit.x != 0 || fit.y != 0);
	CHECK(fit.x != 5 || fit.y != 4);
	CHECK_EQ(5, int(scene.Fit(scene.pWare, 2).x));
	CHECK_EQ(4, int(scene.Fit(scene.pWare, 2).y));
	scene.money.SetMoney(int(price));
	scene.Run(UI_BUY_ITEM, 4, 3, scene.shop.get());
	CHECK(Same({"buy|300|1|4|" + N(fit.x) + "|" + N(fit.y) + "|3"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_BUY), int(scene.temp.GetMode()));
	CHECK_EQ(1, int(scene.temp.Value1));
	CHECK_EQ(4, int(scene.temp.Value2));
	CHECK_EQ(int(fit.x), int(scene.temp.Value3));
	CHECK_EQ(int(fit.y), int(scene.temp.Value4));
	CHECK(scene.temp.pValue == scene.shop.get());
	// The shelf's item keeps its own count; the money is the server's to take.
	CHECK_EQ(1, int(scene.pWare->GetNumber()));
	CHECK_EQ(int(price), scene.money.GetMoney());
}

TEST(UIMessages3Buy, ACoinShortSaysNoMoney)
{
	ShopScene scene;
	const unsigned price = scene.prices.GetPurchasePrice(scene.pWare, 3);
	scene.money.SetMoney(int(price) - 1);
	scene.Run(UI_BUY_ITEM, 4, 3, scene.shop.get());
	CHECK(Same({Popup(kNoMoney), "start 2"}));
	CHECK(!scene.Waiting());
	CHECK_EQ(1, int(scene.pWare->GetNumber()));
}

TEST(UIMessages3Buy, NoRoomSaysSoAndSuggestsTheStorage)
{
	ShopScene scene;
	MInventory full;
	full.Init(2, 2);
	CHECK(full.AddItem(Make<MHelm>(133), 0, 0));
	g_pInventory = &full;
	scene.money.SetMoney(int(scene.prices.GetPurchasePrice(scene.pWare, 3)));
	scene.Run(UI_BUY_ITEM, 4, 3, scene.shop.get());
	g_pInventory = &scene.inventory;
	CHECK(Same({Popup("no space"), "start 2", "help|" + N(HELP_EVENT_STORAGE_BUY)}));
	CHECK(!scene.Waiting());
	CHECK_EQ(1, int(scene.pWare->GetNumber()));
}

TEST(UIMessages3Buy, TheCountIsNotChecked)
{
	ShopScene scene;
	// Recorded defect (L4901-4993): the count is not validated. 256 is
	// priced in full but goes out as a byte, 0; 0 costs nothing and is
	// requested as such.
	scene.money.SetMoney(int(scene.prices.GetPurchasePrice(scene.pWare, 256)));
	const POINT fit = scene.Fit(scene.pWare, 256);
	scene.Run(UI_BUY_ITEM, 4, 256, scene.shop.get());
	CHECK(Same({"buy|300|1|4|" + N(fit.x) + "|" + N(fit.y) + "|0"}));
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	CHECK_EQ(0u, scene.prices.GetPurchasePrice(scene.pWare, 0));
	scene.money.SetMoney(0);
	const POINT fit0 = scene.Fit(scene.pWare, 0);
	scene.Run(UI_BUY_ITEM, 4, 0, scene.shop.get());
	CHECK(Same({"buy|300|1|4|" + N(fit0.x) + "|" + N(fit0.y) + "|0"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_BUY), int(scene.temp.GetMode()));
}

TEST(UIMessages3Buy, AStarShopCountsTheStarsOfTheOptionsColour)
{
	ShopScene scene;
	scene.shop->SetShopType(MShop::SHOP_EVENT_STAR);
	// A type-2 item with a strength option costs 20 red stars (type 1).
	MEventGiftBoxItem* pPrize = Make<MEventGiftBoxItem>(134, 2);
	pPrize->AddItemOption(1);
	CHECK(scene.shop->GetCurrentShelf()->SetItem(5, pPrize));
	MEventStarItem* pRed = Make<MEventStarItem>(135, 1, 19);
	MEventStarItem* pBlue = Make<MEventStarItem>(136, 2, 20);
	CHECK(scene.inventory.AddItem(pRed, 5, 5));
	CHECK(scene.inventory.AddItem(pBlue, 6, 5));
	scene.Run(UI_BUY_ITEM, 5, 1, scene.shop.get());
	CHECK(Same({Popup("no star"), "start 2"}));
	CHECK(!scene.Waiting());
	calls.clear();
	pRed->SetNumber(20);
	const POINT fit = scene.Fit(pPrize, 1);
	scene.Run(UI_BUY_ITEM, 5, 1, scene.shop.get());
	CHECK(Same({"buy|300|1|5|" + N(fit.x) + "|" + N(fit.y) + "|1"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_BUY), int(scene.temp.GetMode()));
	CHECK_EQ(5, int(scene.temp.Value2));
	// Money does not matter there.
	CHECK_EQ(0, scene.money.GetMoney());
}

TEST(UIMessages3Buy, AStarShopItemWithoutAColourCannotBeBought)
{
	ShopScene scene;
	scene.shop->SetShopType(MShop::SHOP_EVENT_STAR);
	CHECK(scene.inventory.AddItem(Make<MEventStarItem>(135, 1, 99), 5, 5));
	scene.money.SetMoney(1000000);
	// No option: no star price; a type-1 item: a price of no stars.
	scene.Run(UI_BUY_ITEM, 4, 1, scene.shop.get());
	MEventGiftBoxItem* pFree = Make<MEventGiftBoxItem>(137, 1);
	pFree->AddItemOption(1);
	CHECK(scene.shop->GetCurrentShelf()->SetItem(6, pFree));
	scene.Run(UI_BUY_ITEM, 6, 1, scene.shop.get());
	CHECK(Same({Popup("cannot buy"), "start 2", "delete 2", Popup("cannot buy"), "start 3"}));
	CHECK(!scene.Waiting());
	// Recorded defect (L4953-4963): the star count is (type - 1) * 20, so
	// a type-0 item with a colour costs -20 stars, which any player has.
	calls.clear();
	delete scene.inventory.RemoveItem(TYPE_OBJECTID(135));
	MEventGiftBoxItem* pNegative = Make<MEventGiftBoxItem>(138, 0);
	pNegative->AddItemOption(1);
	CHECK(scene.shop->GetCurrentShelf()->SetItem(7, pNegative));
	const POINT fit = scene.Fit(pNegative, 1);
	scene.Run(UI_BUY_ITEM, 7, 1, scene.shop.get());
	CHECK(Same({"buy|300|1|7|" + N(fit.x) + "|" + N(fit.y) + "|1"}));
}

TEST(UIMessages3Buy, NothingWithoutAShopShelfOrWareOrWhileARequestWaits)
{
	ShopScene scene;
	scene.money.SetMoney(1000000);
	scene.Run(UI_BUY_ITEM, 4, 1, NULL);
	scene.Run(UI_BUY_ITEM, 5, 1, scene.shop.get());
	scene.Run(UI_BUY_ITEM, -1, 1, scene.shop.get());
	scene.Busy();
	scene.Run(UI_BUY_ITEM, 4, 1, scene.shop.get());
	scene.temp.SetMode(TempInformation::MODE_NULL);
	// Shelf 0 is empty.
	scene.shop->SetCurrent(0);
	CHECK(scene.shop->GetCurrentShelf() == NULL);
	scene.Run(UI_BUY_ITEM, 4, 1, scene.shop.get());
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.shop->SetCurrent(1);
	scene.Run(UI_BUY_ITEM, 4, 1, scene.shop.get());
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

//----------------------------------------------------------------------
// UI_SELL_ITEM (L5092): left/right = the item's inventory cell. A unique
// item sells only as a timed one.
//----------------------------------------------------------------------
TEST(UIMessages3Sell, TheItemInTheCellIsOfferedAndRemembered)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(141);
	CHECK(scene.inventory.AddItem(pPotion, 5, 3));
	scene.Run(UI_SELL_ITEM, 5, 3, NULL);
	CHECK(Same({"sell|300|" + N(SHOP_REQUEST_SELL_NORMAL) + "|141"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_SELL), int(scene.temp.GetMode()));
	CHECK(scene.temp.pValue == pPotion);
	// Recorded defect (L5109): the cell is cut to a byte, so 261 is 5.
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Run(UI_SELL_ITEM, 261, 3, NULL);
	CHECK(Same({"sell|300|" + N(SHOP_REQUEST_SELL_NORMAL) + "|141"}));
}

TEST(UIMessages3Sell, AUniqueItemSellsOnlyWhenTimed)
{
	Scene scene;
	MVampireAmulet* pAmulet = Make<MVampireAmulet>(142);
	CHECK(scene.inventory.AddItem(pAmulet, 6, 3));
	scene.Run(UI_SELL_ITEM, 6, 3, NULL);
	CHECK(Same({Popup("cannot sell"), "start 2"}));
	CHECK(!scene.Waiting());
	calls.clear();
	g_pTimeItemManager->AddTimeItem(142, 60);
	scene.Run(UI_SELL_ITEM, 6, 3, NULL);
	CHECK(Same({"sell|300|" + N(SHOP_REQUEST_SELL_NORMAL) + "|142"}));
	CHECK(scene.temp.pValue == pAmulet);
}

TEST(UIMessages3Sell, NothingForAnEmptyCellOrWhileARequestWaits)
{
	Scene scene;
	CHECK(scene.inventory.AddItem(Make<MPotion>(141), 5, 3));
	scene.Run(UI_SELL_ITEM, 4, 3, NULL);
	scene.Busy();
	scene.Run(UI_SELL_ITEM, 5, 3, NULL);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_SELL_ITEM, 5, 3, NULL);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

// UI_SELL_ALL_ITEM (L5163): every skull, in one request.
TEST(UIMessages3Sell, SellingAllSkullsIsOneRequest)
{
	Scene scene;
	scene.Run(UI_SELL_ALL_ITEM, 7, 8, NULL);
	CHECK(Same({"sell|300|" + N(SHOP_REQUEST_SELL_ALL_SKULL) + "|0"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_SELL_ALL_SKULL), int(scene.temp.GetMode()));
	calls.clear();
	scene.Run(UI_SELL_ALL_ITEM);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Run(UI_SELL_ALL_ITEM);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

//----------------------------------------------------------------------
// UI_REMOVE_BACKGROUND_MOUSE_FOCUS and UI_BACKGROUND_MOUSE_FOCUS (L5211,
// L5225): whether the UI has the mouse, in the game only.
//----------------------------------------------------------------------
TEST(UIMessages3MouseFocus, TheUIHasTheMouseOrTheGameHasIt)
{
	Scene scene;
	scene.Run(UI_REMOVE_BACKGROUND_MOUSE_FOCUS);
	CHECK_EQ(TRUE, g_bUIInput);
	scene.Run(UI_BACKGROUND_MOUSE_FOCUS, 3, 4);
	CHECK_EQ(FALSE, g_bUIInput);
	g_Mode = MODE_MAINMENU;
	scene.Run(UI_REMOVE_BACKGROUND_MOUSE_FOCUS);
	CHECK_EQ(FALSE, g_bUIInput);
	g_bUIInput = TRUE;
	scene.Run(UI_BACKGROUND_MOUSE_FOCUS);
	CHECK_EQ(TRUE, g_bUIInput);
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// The handlers whose only effect is a VS_UI window, and the registered
// no-ops: UI_GAMEMENU_CONTINUE (L5498), UI_DROP_MONEY (L5537, an
// unconditional return before its CGDropMoney), UI_INFO (L5657, its body
// commented out), UI_FINISH_LEVELUP_BUTTON (L6231), UI_CLOSE_BOOKCASE
// (L7578), UI_CLOSE_BRIEFING (L7602), UI_CLOSE_DESC_DIALOG (L7698),
// UI_CLOSE_ELEVATOR (L7722) and UI_ITEM_TO_QUICKITEMSLOT (L7847, its body
// commented out). Without a game UI they change nothing here.
//----------------------------------------------------------------------
TEST(UIMessages3WindowOnly, TheWindowOnlyMessagesChangeNothingHere)
{
	Scene scene;
	scene.money.SetMoney(500);
	MPotion* pPotion = Make<MPotion>(151);
	CHECK(scene.inventory.AddItem(pPotion, 0, 0));
	for (CLIENT_MODE mode : {MODE_GAME, MODE_MAINMENU})
	{
		g_Mode = mode;
		for (DWORD message : {DWORD(UI_GAMEMENU_CONTINUE), DWORD(UI_DROP_MONEY), DWORD(UI_INFO), DWORD(UI_FINISH_LEVELUP_BUTTON),
				DWORD(UI_CLOSE_BOOKCASE), DWORD(UI_CLOSE_BRIEFING), DWORD(UI_CLOSE_DESC_DIALOG), DWORD(UI_CLOSE_ELEVATOR),
				DWORD(UI_ITEM_TO_QUICKITEMSLOT)})
			scene.Run(message, 100, 0, pPotion);
	}
	CHECK(calls.empty());
	CHECK_EQ(500, scene.money.GetMoney());
	CHECK(!scene.Waiting());
	CHECK(scene.inventory.GetItem(0, 0) == pPotion);
	CHECK(scene.Held() == NULL);
}

//----------------------------------------------------------------------
// UI_CLICK_BONUS_POINT (L5589): left = 0 strength, 1 dexterity, 2
// intelligence, while the player has a bonus point.
//----------------------------------------------------------------------
TEST(UIMessages3BonusPoint, EachButtonAsksForItsStat)
{
	Scene scene;
	scene.me->MStatus::SetStatus(MODIFY_BONUS_POINT, 2);
	const int which[] = {INC_STR, INC_DEX, INC_INT};
	for (int left = 0; left < 3; ++left)
	{
		calls.clear();
		scene.temp.SetMode(TempInformation::MODE_NULL);
		scene.temp.Value1 = -1;
		scene.Run(UI_CLICK_BONUS_POINT, left);
		CHECK(Same({"bonus|" + N(which[left])}));
		CHECK_EQ(int(TempInformation::MODE_BONUSPOINT_USE), int(scene.temp.GetMode()));
		CHECK_EQ(which[left], int(scene.temp.Value1));
	}
	// The points are the server's to take.
	CHECK_EQ(2u, scene.me->GetBonusPoint());
	// Recorded defect (L5635/L5644): left is not range-checked; a value
	// outside 0..2 reads past the 3-entry table (not run here).
}

TEST(UIMessages3BonusPoint, NothingWithoutAPointOrWhileARequestWaits)
{
	Scene scene;
	scene.me->MStatus::SetStatus(MODIFY_BONUS_POINT, 0);
	scene.Run(UI_CLICK_BONUS_POINT, 0);
	scene.me->MStatus::SetStatus(MODIFY_BONUS_POINT, 1);
	scene.Busy();
	scene.Run(UI_CLICK_BONUS_POINT, 0);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_CLICK_BONUS_POINT, 0);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

//----------------------------------------------------------------------
// UI_REPAIR_ITEM (L5823): left = the item's ID, 0 for everything worn.
// The item is looked for in the inventory, then in the race's gear; the
// price is the repair price, a key's being the motorcycle's.
//----------------------------------------------------------------------
TEST(UIMessages3Repair, RepairingEverythingAsksWithoutAMoneyCheck)
{
	Scene scene;
	// Recorded defect (L5847-5869): repairing everything sends with no
	// money at all.
	scene.Run(UI_REPAIR_ITEM, 0);
	CHECK(Same({"repair|0"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_REPAIR), int(scene.temp.GetMode()));
	CHECK_EQ(0, int(scene.temp.Value1));
	CHECK_EQ(1, int(scene.temp.Value2));
	CHECK(scene.temp.pValue == NULL);
}

TEST(UIMessages3Repair, AnInventoryItemIsRepairedWhenThePlayerCanPay)
{
	Scene scene;
	MSword* pSword = Make<MSword>(152);
	pSword->SetCurrentDurability(500);
	CHECK(scene.inventory.AddItem(pSword, 7, 0));
	const int price = scene.prices.GetItemPrice(pSword, MPriceManager::REPAIR);
	CHECK(price > 0);
	scene.money.SetMoney(price - 1);
	scene.Run(UI_REPAIR_ITEM, 152);
	CHECK(Same({Popup(kNoMoney), "start 2"}));
	CHECK(!scene.Waiting());
	calls.clear();
	scene.money.SetMoney(price);
	scene.Run(UI_REPAIR_ITEM, 152);
	CHECK(Same({"repair|152"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_REPAIR), int(scene.temp.GetMode()));
	CHECK_EQ(1, int(scene.temp.Value1));
	CHECK_EQ(0, int(scene.temp.Value2));
	CHECK(scene.temp.pValue == pSword);
	CHECK_EQ(price, scene.money.GetMoney());
}

TEST(UIMessages3Repair, AWornItemIsFoundInTheGear)
{
	Scene scene;
	MHelm* pHelm = Make<MHelm>(153);
	pHelm->SetCurrentDurability(100);
	CHECK(scene.slayerGear.AddItem(pHelm, MSlayerGear::GEAR_SLAYER_HELM));
	const int price = scene.prices.GetItemPrice(pHelm, MPriceManager::REPAIR);
	CHECK(price > 0);
	scene.money.SetMoney(price);
	scene.Run(UI_REPAIR_ITEM, 153);
	CHECK(Same({"repair|153"}));
	CHECK_EQ(0, int(scene.temp.Value1));
	CHECK_EQ(1, int(scene.temp.Value2));
	CHECK(scene.temp.pValue == pHelm);
}

TEST(UIMessages3Repair, AVampiresAndAnOustersGearIsLookedInToo)
{
	{
		Scene scene;
		scene.Vampire();
		MVampireNecklace* pNecklace = Make<MVampireNecklace>(154);
		CHECK(scene.vampireGear.AddItem(pNecklace, MVampireGear::GEAR_VAMPIRE_NECKLACE));
		const int price = scene.prices.GetItemPrice(pNecklace, MPriceManager::REPAIR);
		CHECK(price > 0);
		scene.money.SetMoney(price);
		scene.Run(UI_REPAIR_ITEM, 154);
		CHECK(Same({"repair|154"}));
		CHECK_EQ(0, int(scene.temp.Value1));
		CHECK_EQ(1, int(scene.temp.Value2));
		CHECK(scene.temp.pValue == pNecklace);
	}
	{
		Scene scene;
		scene.Ousters();
		MOustersCirclet* pCirclet = Make<MOustersCirclet>(155);
		CHECK(scene.oustersGear.AddItem(pCirclet, MOustersGear::GEAR_OUSTERS_CIRCLET));
		const int price = scene.prices.GetItemPrice(pCirclet, MPriceManager::REPAIR);
		CHECK(price > 0);
		scene.money.SetMoney(price);
		scene.Run(UI_REPAIR_ITEM, 155);
		CHECK(Same({"repair|155"}));
		CHECK_EQ(0, int(scene.temp.Value1));
		CHECK_EQ(1, int(scene.temp.Value2));
		CHECK(scene.temp.pValue == pCirclet);
	}
}

TEST(UIMessages3Repair, NothingToRepairAndAKeysFreeRepair)
{
	Scene scene;
	MSword* pFresh = Make<MSword>(156);
	pFresh->SetCurrentDurability(TYPE_ITEM_DURATION(pFresh->GetMaxDurability()));
	CHECK(scene.inventory.AddItem(pFresh, 0, 0));
	CHECK_EQ(0, scene.prices.GetItemPrice(pFresh, MPriceManager::REPAIR));
	scene.money.SetMoney(1000);
	scene.Run(UI_REPAIR_ITEM, 156);
	CHECK(Same({Popup("cannot repair"), "start 2"}));
	CHECK(!scene.Waiting());
	// A key costs nothing but needs some money.
	calls.clear();
	MKey* pKey = Make<MKey>(157);
	CHECK(scene.inventory.AddItem(pKey, 1, 0));
	CHECK_EQ(0, scene.prices.GetItemPrice(pKey, MPriceManager::REPAIR));
	scene.money.SetMoney(0);
	scene.Run(UI_REPAIR_ITEM, 157);
	CHECK(Same({"delete 2", Popup(kNoMoney), "start 3"}));
	calls.clear();
	scene.money.SetMoney(1);
	scene.Run(UI_REPAIR_ITEM, 157);
	CHECK(Same({"repair|157"}));
	CHECK(scene.temp.pValue == pKey);
}

TEST(UIMessages3Repair, TheRepairableTestRefusesOnlyAUniqueQuestAmuletAndSaysTheSame)
{
	Scene scene;
	// Recorded defect (L5936-5941): the test is an OR of negations, false
	// only for a unique quest vampire amulet with no durability that is
	// not charged; its box (L6003) is the zero-price box (L5995) anyway.
	Thing* pAmulet = new Thing(ITEM_CLASS_VAMPIRE_AMULET);
	pAmulet->SetItemType(0);
	pAmulet->SetID(158);
	pAmulet->SetQuestFlag();
	CHECK(scene.inventory.AddItem(pAmulet, 0, 0));
	scene.money.SetMoney(1000);
	scene.Run(UI_REPAIR_ITEM, 158);
	pAmulet->SetQuestFlag(false);
	scene.Run(UI_REPAIR_ITEM, 158);
	CHECK(Same({Popup("cannot repair"), "start 2", "delete 2", Popup("cannot repair"), "start 3"}));
	CHECK(!scene.Waiting());
}

TEST(UIMessages3Repair, AnUnknownItemOrAWaitingRequestDoesNothing)
{
	Scene scene;
	MSword* pSword = Make<MSword>(152);
	pSword->SetCurrentDurability(500);
	CHECK(scene.inventory.AddItem(pSword, 7, 0));
	scene.money.SetMoney(1000000);
	scene.Run(UI_REPAIR_ITEM, 999);
	scene.Busy();
	scene.Run(UI_REPAIR_ITEM, 152);
	scene.Run(UI_REPAIR_ITEM, 0);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_REPAIR_ITEM, 152);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

//----------------------------------------------------------------------
// UI_SILVERING_ITEM (L6018): left = the item's ID; a class that takes a
// coat, priced by the silver it lacks.
//----------------------------------------------------------------------
TEST(UIMessages3Silvering, ACoatableItemIsCoatedWhenThePlayerCanPay)
{
	Scene scene;
	MSword* pSword = Make<MSword>(161);
	pSword->SetSilver(10);
	CHECK(scene.inventory.AddItem(pSword, 7, 0));
	const int price = scene.prices.GetItemPrice(pSword, MPriceManager::SILVERING);
	CHECK_EQ(50, price);
	scene.money.SetMoney(price - 1);
	scene.Run(UI_SILVERING_ITEM, 161);
	scene.money.SetMoney(0);
	scene.Run(UI_SILVERING_ITEM, 161);
	CHECK(Same({Popup(kNoMoney), "start 2", "delete 2", Popup(kNoMoney), "start 3"}));
	CHECK(!scene.Waiting());
	calls.clear();
	scene.money.SetMoney(price);
	scene.Run(UI_SILVERING_ITEM, 161);
	CHECK(Same({"silver|161"}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_SILVERING), int(scene.temp.GetMode()));
	CHECK_EQ(1, int(scene.temp.Value1));
	CHECK_EQ(0, int(scene.temp.Value2));
	CHECK(scene.temp.pValue == pSword);
}

TEST(UIMessages3Silvering, AFullCoatOrAnUncoatableItemCannotBeCoated)
{
	Scene scene;
	MSword* pSword = Make<MSword>(161);
	pSword->SetSilver(50);
	CHECK(scene.inventory.AddItem(pSword, 7, 0));
	scene.money.SetMoney(1000);
	scene.Run(UI_SILVERING_ITEM, 161);
	CHECK(Same({Popup("cannot silver"), "start 2"}));
	// A helm worn takes no coat: found in the gear, refused.
	calls.clear();
	CHECK(scene.slayerGear.AddItem(Make<MHelm>(162), MSlayerGear::GEAR_SLAYER_HELM));
	scene.Run(UI_SILVERING_ITEM, 162);
	CHECK(Same({"delete 2", Popup("cannot silver"), "start 3"}));
	CHECK(!scene.Waiting());
}

TEST(UIMessages3Silvering, TheVampiresAndOustersGearAndNothingElse)
{
	{
		Scene scene;
		scene.Vampire();
		CHECK(scene.vampireGear.AddItem(Make<MVampireNecklace>(163), MVampireGear::GEAR_VAMPIRE_NECKLACE));
		scene.money.SetMoney(1000);
		scene.Run(UI_SILVERING_ITEM, 163);
		CHECK(Same({Popup("cannot silver"), "start 2"}));
	}
	{
		Scene scene;
		scene.Ousters();
		CHECK(scene.oustersGear.AddItem(Make<MOustersCirclet>(164), MOustersGear::GEAR_OUSTERS_CIRCLET));
		scene.money.SetMoney(1000);
		scene.Run(UI_SILVERING_ITEM, 164);
		CHECK(Same({Popup("cannot silver"), "start 2"}));
		// Not found (0 included): silence; a waiting request: nothing.
		calls.clear();
		scene.Run(UI_SILVERING_ITEM, 999);
		scene.Run(UI_SILVERING_ITEM, 0);
		scene.Busy();
		scene.Run(UI_SILVERING_ITEM, 164);
		scene.temp.SetMode(TempInformation::MODE_NULL);
		g_Mode = MODE_WAIT_UPDATEINFO;
		scene.Run(UI_SILVERING_ITEM, 164);
		CHECK(calls.empty());
		CHECK(!scene.Waiting());
	}
}

//----------------------------------------------------------------------
// UI_RUNNING_GAMEMENU (L6255): the game menu closes the message box and
// the NPC's talk window.
//----------------------------------------------------------------------
TEST(UIMessages3GameMenu, TheGameMenuClosesTheBoxAndTheTalk)
{
	Scene scene;
	scene.dialog.PopupFreeMessageDlg("hello");
	calls.clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_RUNNING_GAMEMENU);
	CHECK(calls.empty());
	g_Mode = MODE_GAME;
	scene.Run(UI_RUNNING_GAMEMENU);
	CHECK(Same({"delete 2", "delete 1"}));
	calls.clear();
	scene.Run(UI_RUNNING_GAMEMENU);
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// UI_STORAGE_BUY (L6282): left = buy or not, after the NPC offered a
// storage box at the price in Value1.
//----------------------------------------------------------------------
TEST(UIMessages3StorageBuy, WithThePriceTheRequestGoesOutAndWaits)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_STORAGE_BUY);
	scene.temp.Value1 = 1000;
	scene.money.SetMoney(1000);
	scene.Run(UI_STORAGE_BUY, TRUE);
	CHECK(Same({"stash buy"}));
	CHECK_EQ(int(TempInformation::MODE_STORAGE_BUY_WAIT), int(scene.temp.GetMode()));
	CHECK_EQ(1000, scene.money.GetMoney());
}

TEST(UIMessages3StorageBuy, ACoinShortBackToTheTalkWithNoMoney)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_STORAGE_BUY);
	scene.temp.Value1 = 1000;
	scene.money.SetMoney(999);
	scene.Run(UI_STORAGE_BUY, TRUE);
	CHECK(Same({"start 1", Popup(kNoMoney), "start 2"}));
	CHECK(!scene.Waiting());
}

TEST(UIMessages3StorageBuy, BuyingWithoutAnOfferDoesNothingAndDecliningAlwaysEndsTheRequest)
{
	Scene scene;
	scene.money.SetMoney(1000000);
	scene.Run(UI_STORAGE_BUY, TRUE);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
	// Recorded defect (L6348): declining ends whatever request waits.
	scene.Busy();
	scene.Run(UI_STORAGE_BUY, FALSE);
	CHECK(Same({"start 1"}));
	CHECK(!scene.Waiting());
	calls.clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.temp.SetMode(TempInformation::MODE_STORAGE_BUY);
	scene.Run(UI_STORAGE_BUY, FALSE);
	scene.Run(UI_STORAGE_BUY, TRUE);
	CHECK(calls.empty());
	CHECK_EQ(int(TempInformation::MODE_STORAGE_BUY), int(scene.temp.GetMode()));
}

//----------------------------------------------------------------------
// UI_SELECT_STORAGE_SLOT (L6360): left = the rack, right = the slot. An
// empty mouse lifts the slot's item; a held one is put in, piled on, or
// swapped, unless it is a kind the storage refuses.
//----------------------------------------------------------------------
namespace {
// The storage's rack's slot.
const MItem* Stored(MStorage& storage, unsigned int rack, int slot)
{
	const unsigned int old = storage.GetCurrent();
	storage.SetCurrent(rack);
	const MItem* pItem = storage.GetItem(slot);
	storage.SetCurrent(old);
	return pItem;
}
void Store(MStorage& storage, unsigned int rack, int slot, MItem* pItem)
{
	const unsigned int old = storage.GetCurrent();
	storage.SetCurrent(rack);
	CHECK(storage.SetItem(slot, pItem));
	storage.SetCurrent(old);
}
} // namespace

TEST(UIMessages3Storage, AnEmptyMouseLiftsTheSlotsItem)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(171);
	Store(scene.storage, 1, 5, pPotion);
	scene.Run(UI_SELECT_STORAGE_SLOT, 1, 5);
	CHECK(Same({"sound|" + N(kPotionSound), "stash to mouse|171|1|5"}));
	CHECK_EQ(1u, scene.storage.GetCurrent());
	CHECK(Stored(scene.storage, 1, 5) == NULL);
	CHECK(scene.Held() == pPotion);
	// An empty slot: nothing. A rack past the storage keeps the rack.
	calls.clear();
	scene.mouse.DropItem();
	scene.loose.emplace_back(pPotion);
	scene.Run(UI_SELECT_STORAGE_SLOT, 3, 5);
	CHECK_EQ(1u, scene.storage.GetCurrent());
	scene.Run(UI_SELECT_STORAGE_SLOT, 2, 5);
	CHECK_EQ(2u, scene.storage.GetCurrent());
	CHECK(calls.empty());
}

TEST(UIMessages3Storage, AHeldItemGoesIntoAnEmptySlot)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(172);
	scene.Hold(pPotion);
	scene.Run(UI_SELECT_STORAGE_SLOT, 1, 6);
	CHECK(Same({"mouse to stash|172|1|6"}));
	CHECK(Stored(scene.storage, 1, 6) == pPotion);
	CHECK(scene.Held() == NULL);
}

TEST(UIMessages3Storage, AHeldPilePilesOnUpToItsMaximum)
{
	Scene scene;
	MPotion* pStored = Make<MPotion>(173, 0, 10);
	Store(scene.storage, 1, 7, pStored);
	scene.Hold(Make<MPotion>(174, 0, 5));
	scene.Run(UI_SELECT_STORAGE_SLOT, 1, 7);
	CHECK(Same({"sound|" + N(kPotionSound), "mouse to stash|174|1|7"}));
	CHECK(Stored(scene.storage, 1, 7) == pStored);
	CHECK_EQ(15, int(pStored->GetNumber()));
	CHECK(scene.Held() == NULL);
	calls.clear();
	pStored->SetNumber(28);
	MPotion* pHeld = Make<MPotion>(175, 0, 5);
	scene.Hold(pHeld);
	scene.Run(UI_SELECT_STORAGE_SLOT, 1, 7);
	CHECK(Same({"sound|" + N(kPotionSound), "mouse to stash|175|1|7"}));
	CHECK_EQ(MAX_POTION_PILE_NUMBER, int(pStored->GetNumber()));
	CHECK_EQ(3, int(pHeld->GetNumber()));
	CHECK(scene.Held() == pHeld);
	// A full pile takes nothing and the server is not told.
	calls.clear();
	scene.Run(UI_SELECT_STORAGE_SLOT, 1, 7);
	CHECK(calls.empty());
	CHECK_EQ(3, int(pHeld->GetNumber()));
	CHECK(Stored(scene.storage, 1, 7) == pStored);
}

TEST(UIMessages3Storage, AnotherKindIsSwapped)
{
	Scene scene;
	MHelm* pHelm = Make<MHelm>(176);
	Store(scene.storage, 1, 8, pHelm);
	MPotion* pPotion = Make<MPotion>(177);
	scene.Hold(pPotion);
	scene.Run(UI_SELECT_STORAGE_SLOT, 1, 8);
	CHECK(Same({"sound|" + N(kHelmSound), "mouse to stash|177|1|8"}));
	CHECK(scene.Held() == pHelm);
	CHECK(Stored(scene.storage, 1, 8) == pPotion);
}

TEST(UIMessages3Storage, TheRefusedKindsStayOnTheMouse)
{
	Scene scene;
	struct Kind { ITEM_CLASS itemClass; int type; bool quest; bool refused; };
	const Kind kinds[] = {
		{ITEM_CLASS_EVENT_GIFT_BOX, 0, false, true},
		{ITEM_CLASS_VAMPIRE_AMULET, 1, false, true},
		{ITEM_CLASS_KEY, 1, false, true},
		{ITEM_CLASS_KEY, 0, false, false},
		{ITEM_CLASS_COUPLE_RING, 0, false, true},
		{ITEM_CLASS_VAMPIRE_COUPLE_RING, 0, false, true},
		{ITEM_CLASS_POTION, 0, true, true},
		{ITEM_CLASS_EVENT_TREE, 13, false, true},
		{ITEM_CLASS_EVENT_TREE, 12, false, false},
		{ITEM_CLASS_EVENT_ITEM, 0, false, true},
		{ITEM_CLASS_CODE_SHEET, 0, false, true},
		{ITEM_CLASS_LUCKY_BAG, 3, false, true},
		{ITEM_CLASS_LUCKY_BAG, 2, false, false},
	};
	int slot = 0;
	for (const Kind& kind : kinds)
	{
		scene.dialog.CloseMessageDlg();
		calls.clear();
		Thing* pThing = new Thing(kind.itemClass);
		pThing->SetItemType(TYPE_ITEMTYPE(kind.type));
		pThing->SetID(TYPE_OBJECTID(180 + slot));
		pThing->SetQuestFlag(kind.quest);
		scene.Hold(pThing);
		scene.Run(UI_SELECT_STORAGE_SLOT, 0, slot);
		if (kind.refused)
		{
			CHECK(Same({Popup("cannot store"), "start " + N(windowCount)}));
			CHECK(scene.Held() == pThing);
			CHECK(Stored(scene.storage, 0, slot) == NULL);
			scene.mouse.DropItem();
			scene.loose.emplace_back(pThing);
		}
		else
		{
			CHECK(Same({"mouse to stash|" + N(180 + slot) + "|0|" + N(slot)}));
			CHECK(Stored(scene.storage, 0, slot) == pThing);
		}
		++slot;
	}
}

TEST(UIMessages3Storage, ASlotPastTheRackDropsTheItemAndStillTellsTheServer)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(178);
	scene.Hold(pPotion);
	// Recorded defect (L6433-6437): a slot past the rack reads as empty,
	// so the mouse lets go, the storage refuses the item (it leaks) and
	// the server is told anyway.
	scene.Run(UI_SELECT_STORAGE_SLOT, 1, STORAGE_SLOT);
	CHECK(Same({"mouse to stash|178|1|" + N(STORAGE_SLOT)}));
	CHECK(scene.Held() == NULL);
	CHECK(!scene.Owned(pPotion));
	scene.loose.emplace_back(pPotion);
}

TEST(UIMessages3Storage, NothingWithoutAStorageOrOutOfTheGame)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(171);
	Store(scene.storage, 0, 5, pPotion);
	g_pStorage = NULL;
	scene.Run(UI_SELECT_STORAGE_SLOT, 0, 5);
	g_pStorage = &scene.storage;
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_SELECT_STORAGE_SLOT, 0, 5);
	CHECK(calls.empty());
	CHECK(Stored(scene.storage, 0, 5) == pPotion);
	CHECK(scene.Held() == NULL);
}

//----------------------------------------------------------------------
// UI_DEPOSIT_MONEY (L6534) and UI_WITHDRAW_MONEY (L6581): left = the
// amount. The server is told, then the money moves at once.
//----------------------------------------------------------------------
TEST(UIMessages3StorageMoney, ADepositMovesTheMoneyIntoTheStorage)
{
	Scene scene;
	scene.money.SetMoney(1000);
	scene.storage.GetMoneyManager()->SetMoney(50);
	scene.Run(UI_DEPOSIT_MONEY, 300);
	CHECK(Same({"deposit|300"}));
	CHECK_EQ(700, scene.money.GetMoney());
	CHECK_EQ(350, scene.storage.GetMoneyManager()->GetMoney());
	// All of it.
	calls.clear();
	scene.Run(UI_DEPOSIT_MONEY, 700);
	CHECK(Same({"deposit|700"}));
	CHECK_EQ(0, scene.money.GetMoney());
	CHECK_EQ(1050, scene.storage.GetMoneyManager()->GetMoney());
}

TEST(UIMessages3StorageMoney, AWithdrawalMovesItOut)
{
	Scene scene;
	scene.storage.GetMoneyManager()->SetMoney(1000);
	scene.money.SetMoney(5);
	scene.Run(UI_WITHDRAW_MONEY, 400);
	CHECK(Same({"withdraw|400"}));
	CHECK_EQ(600, scene.storage.GetMoneyManager()->GetMoney());
	CHECK_EQ(405, scene.money.GetMoney());
}

TEST(UIMessages3StorageMoney, MoreThanThereIsIsCreditedAnyway)
{
	Scene scene;
	scene.money.SetMoney(100);
	scene.storage.GetMoneyManager()->SetMoney(0);
	// Recorded defect (L6559-6565, L6606-6612): the server is told first
	// and the managers' answers are ignored, so money no one had appears
	// on the other side.
	scene.Run(UI_DEPOSIT_MONEY, 500);
	CHECK(Same({"deposit|500"}));
	CHECK_EQ(100, scene.money.GetMoney());
	CHECK_EQ(500, scene.storage.GetMoneyManager()->GetMoney());
	calls.clear();
	scene.Run(UI_WITHDRAW_MONEY, 800);
	CHECK(Same({"withdraw|800"}));
	CHECK_EQ(500, scene.storage.GetMoneyManager()->GetMoney());
	CHECK_EQ(900, scene.money.GetMoney());
}

TEST(UIMessages3StorageMoney, NothingForNoMoneyOrWithoutBothManagers)
{
	Scene scene;
	scene.money.SetMoney(1000);
	scene.storage.GetMoneyManager()->SetMoney(1000);
	for (DWORD message : {DWORD(UI_DEPOSIT_MONEY), DWORD(UI_WITHDRAW_MONEY)})
	{
		scene.Run(message, 0);
		scene.Run(message, -5);
		g_pStorage = NULL;
		scene.Run(message, 10);
		g_pStorage = &scene.storage;
		g_pMoneyManager = NULL;
		scene.Run(message, 10);
		g_pMoneyManager = &scene.money;
		g_Mode = MODE_WAIT_UPDATEINFO;
		scene.Run(message, 10);
		g_Mode = MODE_GAME;
	}
	CHECK(calls.empty());
	CHECK_EQ(1000, scene.money.GetMoney());
	CHECK_EQ(1000, scene.storage.GetMoneyManager()->GetMoney());
}

// UI_CLOSE_STORAGE (L6626): the storage stops, the skills are checked
// again (its items may be skill items) and the talk comes back.
TEST(UIMessages3StorageClose, ClosingTheStorageStopsItAndShowsTheTalk)
{
	Scene scene;
	scene.storage.SetActive();
	scene.Skills();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_CLOSE_STORAGE);
	CHECK(calls.empty());
	CHECK(scene.storage.IsActive());
	g_Mode = MODE_GAME;
	scene.Run(UI_CLOSE_STORAGE);
	CHECK(Same({"start 1"}));
	CHECK(!scene.storage.IsActive());
	CHECK(scene.skills.empty());
}

//----------------------------------------------------------------------
// UI_EXCHANGE_MONEY (L6655): left = giving (TRUE) or taking back, right =
// the amount. The server is always told; after my OK it checks the
// change first, and taking back restarts the OK's delay.
//----------------------------------------------------------------------
TEST(UIMessages3TradeMoney, OutsideMyOKGivingAndTakingGoStraightOut)
{
	Scene scene;
	scene.money.SetMoney(1000);
	scene.Trade();
	scene.Run(UI_EXCHANGE_MONEY, TRUE, 300);
	CHECK(Same({"trade money|500|300|" + N(CG_TRADE_MONEY_INCREASE)}));
	CHECK(!scene.Waits());
	CHECK(!scene.Waiting());
	calls.clear();
	scene.Run(UI_EXCHANGE_MONEY, FALSE, 200);
	CHECK(Same({"trade money|500|200|" + N(CG_TRADE_MONEY_DECREASE)}));
	CHECK(scene.Waits());
	CHECK(!scene.Waiting());
	// The money moves when the server answers.
	CHECK_EQ(1000, scene.money.GetMoney());
	CHECK_EQ(0, g_pTradeManager->GetMyMoneyManager()->GetMoney());
}

TEST(UIMessages3TradeMoney, AfterMyOKTheServerChecksTheChange)
{
	Scene scene;
	scene.Trade(true);
	scene.Run(UI_EXCHANGE_MONEY, TRUE, 300);
	CHECK(Same({"trade money|500|300|" + N(CG_TRADE_MONEY_INCREASE)}));
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_ADD_MONEY), int(scene.temp.GetMode()));
	CHECK_EQ(300, int(scene.temp.Value1));
	CHECK(!scene.Waits());
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.Run(UI_EXCHANGE_MONEY, FALSE, 200);
	CHECK(Same({"trade money|500|200|" + N(CG_TRADE_MONEY_DECREASE)}));
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_REMOVE_MONEY), int(scene.temp.GetMode()));
	CHECK_EQ(200, int(scene.temp.Value1));
	CHECK(scene.Waits());
	CHECK(g_pTradeManager->IsAcceptMyTrade());
}

TEST(UIMessages3TradeMoney, TheAmountAndAWaitingRequestAreNotChecked)
{
	Scene scene;
	scene.Trade(true);
	scene.Busy();
	scene.temp.Value1 = 7;
	// Recorded defect (L6680-6756): with my OK and another request
	// waiting, the change goes out unchecked by the client.
	scene.Run(UI_EXCHANGE_MONEY, TRUE, 300);
	CHECK(Same({"trade money|500|300|" + N(CG_TRADE_MONEY_INCREASE)}));
	CHECK_EQ(int(TempInformation::MODE_SHOP_BUY), int(scene.temp.GetMode()));
	CHECK_EQ(7, int(scene.temp.Value1));
	// Recorded defect: the amount is not validated; -5 goes out as a
	// DWORD.
	calls.clear();
	g_pTradeManager->RefuseMyTrade();
	scene.Run(UI_EXCHANGE_MONEY, TRUE, -5);
	CHECK(Same({"trade money|500|" + N(Gold_t(-5)) + "|" + N(CG_TRADE_MONEY_INCREASE)}));
}

TEST(UIMessages3TradeMoney, NothingWithoutATrade)
{
	Scene scene;
	scene.Run(UI_EXCHANGE_MONEY, TRUE, 300);
	scene.Trade();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_EXCHANGE_MONEY, TRUE, 300);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

//----------------------------------------------------------------------
// UI_ITEM_SELECT_EXCHANGE (L6839): void_ptr = an inventory item clicked
// in the trade (left/right its cell). Outside my OK a marked item is
// unmarked and an unmarked one marked, each OK refused; after my OK the
// server checks the change first. Gift boxes have their own rules.
//----------------------------------------------------------------------
TEST(UIMessages3TradeSelect, OutsideMyOKAnItemIsMarkedAndOffered)
{
	Scene scene;
	scene.Trade();
	MPotion* pPotion = Make<MPotion>(181);
	CHECK(scene.inventory.AddItem(pPotion, 2, 3));
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	CHECK(Same({"trade add|500|181"}));
	CHECK(pPotion->IsTrade());
	CHECK(!scene.Waits());
	CHECK(!scene.Waiting());
	CHECK(scene.me->IsItemCheckBufferNULL());
	// The other side's OK is refused when anything changes.
	calls.clear();
	pPotion->UnSetTrade();
	g_pTradeManager->AcceptOtherTrade();
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	CHECK(Same({"trade add|500|181"}));
	CHECK(!g_pTradeManager->IsAcceptOtherTrade());
	CHECK(scene.Waits());
}

TEST(UIMessages3TradeSelect, OutsideMyOKAMarkedItemIsUnmarkedAndWithdrawn)
{
	Scene scene;
	scene.Trade();
	MPotion* pPotion = Make<MPotion>(181);
	pPotion->SetTrade();
	CHECK(scene.inventory.AddItem(pPotion, 2, 3));
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	CHECK(Same({"trade remove|500|181"}));
	CHECK(!pPotion->IsTrade());
	CHECK(scene.Waits());
	CHECK(!scene.Waiting());
	// The other side's OK is refused here too.
	calls.clear();
	pPotion->SetTrade();
	g_pTradeManager->AcceptOtherTrade();
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	CHECK(Same({"trade remove|500|181"}));
	CHECK(!g_pTradeManager->IsAcceptOtherTrade());
	// Out of the game: nothing.
	calls.clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	CHECK(calls.empty());
	CHECK(!pPotion->IsTrade());
}

TEST(UIMessages3TradeSelect, GiftBoxesWaitForTheServerAndTheRedOneStays)
{
	Scene scene;
	scene.Trade(false, true);
	MEventGiftBoxItem* pRed = Make<MEventGiftBoxItem>(182, 1);
	MEventGiftBoxItem* pBox = Make<MEventGiftBoxItem>(183, 2);
	CHECK(scene.inventory.AddItem(pRed, 0, 0));
	CHECK(scene.inventory.AddItem(pBox, 1, 0));
	// The red box is never traded.
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 0, 0, pRed);
	CHECK(calls.empty());
	// Another box is offered and the server checks it; it is not marked
	// and the other side's OK stays.
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 1, 0, pBox);
	CHECK(Same({"trade add|500|183"}));
	CHECK(!pBox->IsTrade());
	CHECK(g_pTradeManager->IsAcceptOtherTrade());
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM), int(scene.temp.GetMode()));
	CHECK(scene.temp.pValue == pBox);
	CHECK(scene.me->GetItemCheckBuffer() == pBox);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_TRADE_ADD), int(scene.me->GetItemCheckBufferStatus()));
	// A box of type 2 or more while one is in the trade: nothing.
	calls.clear();
	MEventGiftBoxItem* pSecond = Make<MEventGiftBoxItem>(184, 2);
	CHECK(scene.inventory.AddItem(pSecond, 2, 0));
	pBox->SetTrade();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.me->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_NULL);
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 0, pSecond);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
}

TEST(UIMessages3TradeSelect, AGiftBoxOverwritesAWaitingRequest)
{
	Scene scene;
	scene.Trade();
	MEventGiftBoxItem* pBox = Make<MEventGiftBoxItem>(185, 0);
	CHECK(scene.inventory.AddItem(pBox, 1, 0));
	MPotion* pPending = Make<MPotion>(186);
	CHECK(scene.inventory.AddItem(pPending, 2, 0));
	scene.Busy();
	scene.temp.Value1 = 9;
	scene.me->SetItemCheckBuffer(pPending, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	// Recorded defect (L6973-6979): the box's check is set over whatever
	// request and check were waiting, and Value1 keeps the old request's.
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 1, 0, pBox);
	CHECK(Same({"trade add|500|185"}));
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM), int(scene.temp.GetMode()));
	CHECK_EQ(9, int(scene.temp.Value1));
	CHECK(scene.temp.pValue == pBox);
	CHECK(scene.me->GetItemCheckBuffer() == pBox);
}

TEST(UIMessages3TradeSelect, AfterMyOKTheServerChecksTheChangeFirst)
{
	Scene scene;
	scene.Trade(true);
	MPotion* pPotion = Make<MPotion>(181);
	CHECK(scene.inventory.AddItem(pPotion, 2, 3));
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 4, 5, pPotion);
	CHECK(Same({"trade add|500|181"}));
	CHECK(!pPotion->IsTrade());
	CHECK(!scene.Waits());
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM), int(scene.temp.GetMode()));
	CHECK_EQ(181, int(scene.temp.Value1));
	CHECK_EQ(4, int(scene.temp.Value2));
	CHECK_EQ(5, int(scene.temp.Value3));
	CHECK(scene.temp.pValue == pPotion);
	CHECK(scene.me->GetItemCheckBuffer() == pPotion);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_TRADE_ADD), int(scene.me->GetItemCheckBufferStatus()));
	CHECK(g_pTradeManager->IsAcceptMyTrade());
	// A marked one is withdrawn the same way.
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.me->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_NULL);
	pPotion->SetTrade();
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 4, 5, pPotion);
	CHECK(Same({"trade remove|500|181"}));
	CHECK(pPotion->IsTrade());
	CHECK(scene.Waits());
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_REMOVE_ITEM), int(scene.temp.GetMode()));
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_TRADE_REMOVE), int(scene.me->GetItemCheckBufferStatus()));
}

TEST(UIMessages3TradeSelect, AfterMyOKNothingWhileARequestOrCheckWaitsAndNothingWithoutATrade)
{
	Scene scene;
	MPotion* pPotion = Make<MPotion>(181);
	CHECK(scene.inventory.AddItem(pPotion, 2, 3));
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	scene.Trade(true);
	scene.Busy();
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.me->SetItemCheckBuffer(pPotion, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	scene.Run(UI_ITEM_SELECT_EXCHANGE, 2, 3, pPotion);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
	CHECK(!pPotion->IsTrade());
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
}

//----------------------------------------------------------------------
// UI_OK_EXCHANGE (L7019): my OK, once its delay is over, when everything
// the other side offers fits and no gift box is held.
//----------------------------------------------------------------------
TEST(UIMessages3TradeOK, WithRoomMyOKGoesOut)
{
	Scene scene;
	scene.Trade();
	scene.Hold(Make<MPotion>(191));
	scene.Run(UI_OK_EXCHANGE);
	CHECK(Same({"trade finish|500|" + N(CG_TRADE_FINISH_ACCEPT)}));
	CHECK(g_pTradeManager->IsAcceptMyTrade());
}

TEST(UIMessages3TradeOK, NotBeforeTheDelayIsOver)
{
	Scene scene;
	scene.Trade();
	g_pTradeManager->SetNextAcceptTime();
	now += MonotonicClock::Millis(999);
	scene.Run(UI_OK_EXCHANGE);
	CHECK(calls.empty());
	CHECK(!g_pTradeManager->IsAcceptMyTrade());
	now += MonotonicClock::Millis(1);
	scene.Run(UI_OK_EXCHANGE);
	CHECK(Same({"trade finish|500|" + N(CG_TRADE_FINISH_ACCEPT)}));
	CHECK(g_pTradeManager->IsAcceptMyTrade());
}

TEST(UIMessages3TradeOK, AHeldGiftBoxOrNoRoomSaysThereIsNoSpace)
{
	Scene scene;
	scene.Trade();
	scene.Hold(Make<MEventGiftBoxItem>(192, 2));
	scene.Run(UI_OK_EXCHANGE);
	CHECK(Same({Popup("trade no space"), "start 2"}));
	CHECK(!g_pTradeManager->IsAcceptMyTrade());
	// What the other side gives must fit the wallet.
	calls.clear();
	MItem* pBox = scene.Held();
	scene.mouse.DropItem();
	scene.loose.emplace_back(pBox);
	scene.money.SetMoney(1);
	g_pTradeManager->GetOtherMoneyManager()->SetMoney(scene.money.GetMoneyLimit());
	scene.Run(UI_OK_EXCHANGE);
	CHECK(Same({"delete 2", Popup("trade no space"), "start 3"}));
	CHECK(!g_pTradeManager->IsAcceptMyTrade());
}

TEST(UIMessages3TradeOK, NothingWithoutATrade)
{
	Scene scene;
	scene.Run(UI_OK_EXCHANGE);
	scene.Trade();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_OK_EXCHANGE);
	CHECK(calls.empty());
	CHECK(!g_pTradeManager->IsAcceptMyTrade());
}

//----------------------------------------------------------------------
// UI_CANCEL_EXCHANGE (L7079): taking my OK back; after my OK the server
// checks it.
//----------------------------------------------------------------------
TEST(UIMessages3TradeCancel, AfterMyOKTheServerChecksTheTakeBack)
{
	Scene scene;
	scene.Trade(true);
	scene.Run(UI_CANCEL_EXCHANGE);
	CHECK(Same({"trade finish|500|" + N(CG_TRADE_FINISH_RECONSIDER)}));
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_CANCEL), int(scene.temp.GetMode()));
	CHECK(g_pTradeManager->IsAcceptMyTrade());
	// While a request waits: nothing.
	calls.clear();
	scene.Run(UI_CANCEL_EXCHANGE);
	CHECK(calls.empty());
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_CANCEL), int(scene.temp.GetMode()));
}

TEST(UIMessages3TradeCancel, WithoutMyOKItStillGoesOut)
{
	Scene scene;
	scene.Trade();
	scene.Run(UI_CANCEL_EXCHANGE);
	CHECK(Same({"trade finish|500|" + N(CG_TRADE_FINISH_RECONSIDER)}));
	CHECK(!scene.Waiting());
	CHECK(!scene.Waits());
	CHECK(!g_pTradeManager->IsAcceptMyTrade());
	// Out of the game, or no trade: nothing.
	calls.clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_CANCEL_EXCHANGE);
	g_Mode = MODE_GAME;
	delete g_pTradeManager;
	g_pTradeManager = NULL;
	scene.Run(UI_CANCEL_EXCHANGE);
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// UI_CLOSE_EXCHANGE (L7153): after my OK the server checks the close and
// the trade stays; otherwise the trade ends here and the money offered
// comes back.
//----------------------------------------------------------------------
TEST(UIMessages3TradeClose, AfterMyOKTheServerChecksTheClose)
{
	Scene scene;
	MTradeManager* pTrade = scene.Trade(true);
	scene.Run(UI_CLOSE_EXCHANGE);
	CHECK(Same({"trade finish|500|" + N(CG_TRADE_FINISH_REJECT)}));
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_CLOSE), int(scene.temp.GetMode()));
	CHECK(g_pTradeManager == pTrade);
	// Recorded defect (L7177-7190): with a request waiting the close is
	// dropped silently and the window stays.
	calls.clear();
	scene.Run(UI_CLOSE_EXCHANGE);
	CHECK(calls.empty());
	CHECK(g_pTradeManager == pTrade);
}

TEST(UIMessages3TradeClose, OtherwiseTheTradeEndsAndItsMoneyComesBack)
{
	Scene scene;
	scene.money.SetMoney(100);
	scene.Trade();
	g_pTradeManager->GetMyMoneyManager()->SetMoney(300);
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Busy();
	scene.Run(UI_CLOSE_EXCHANGE);
	CHECK(Same({"trade finish|500|" + N(CG_TRADE_FINISH_REJECT)}));
	CHECK(g_pTradeManager == NULL);
	CHECK_EQ(400, scene.money.GetMoney());
	CHECK(!scene.Waiting());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK(g_pUserInformation->ItemDropEnableTime == g_FrameNow + MonotonicClock::Millis(700));
}

TEST(UIMessages3TradeClose, WithoutATradeTheRequestEndsAndNothingGoesOut)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	const MonotonicClock::TimePoint dropTime = g_pUserInformation->ItemDropEnableTime;
	scene.Run(UI_CLOSE_EXCHANGE);
	CHECK(calls.empty());
	CHECK(!scene.Waiting());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK(g_pUserInformation->ItemDropEnableTime == dropTime);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_CLOSE_EXCHANGE);
	CHECK_EQ(int(TempInformation::MODE_TRADE_REQUEST), int(scene.temp.GetMode()));
}

//----------------------------------------------------------------------
// UI_EXCHANGE_ACCEPT (L7240): left = whether the player takes the trade
// someone asked for (Value1 = who, in MODE_TRADE_REQUEST).
//----------------------------------------------------------------------
TEST(UIMessages3TradeRequest, AcceptingOpensTheTradeAndAnswers)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.temp.Value1 = kOther;
	scene.Run(UI_EXCHANGE_ACCEPT, TRUE);
	CHECK(Same({"trade prepare|500|" + N(CG_TRADE_PREPARE_CODE_ACCEPT)}));
	CHECK(g_pTradeManager != NULL);
	CHECK_EQ(int(kOther), int(g_pTradeManager->GetOtherID()));
	CHECK(std::string("Trader") == g_pTradeManager->GetOtherName());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
	CHECK(!scene.Waiting());
}

TEST(UIMessages3TradeRequest, AcceptingSomeoneWhoLeftStillAnswers)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.temp.Value1 = kOther + 1;
	scene.Run(UI_EXCHANGE_ACCEPT, TRUE);
	CHECK(Same({"trade prepare|501|" + N(CG_TRADE_PREPARE_CODE_ACCEPT)}));
	CHECK(g_pTradeManager == NULL);
	CHECK(!scene.Waiting());
}

TEST(UIMessages3TradeRequest, DecliningEndsTheTradeWaitOnly)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.temp.Value1 = kOther;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_EXCHANGE_ACCEPT, FALSE);
	CHECK(Same({"trade prepare|500|" + N(CG_TRADE_PREPARE_CODE_REJECT)}));
	CHECK(g_pTradeManager == NULL);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK(!scene.Waiting());
	// Another wait stays.
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.Run(UI_EXCHANGE_ACCEPT, FALSE);
	CHECK(Same({"trade prepare|500|" + N(CG_TRADE_PREPARE_CODE_REJECT)}));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_PARTY), int(scene.me->GetWaitVerify()));
}

TEST(UIMessages3TradeRequest, NothingWithoutARequest)
{
	Scene scene;
	scene.temp.Value1 = kOther;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_EXCHANGE_ACCEPT, TRUE);
	scene.Run(UI_EXCHANGE_ACCEPT, FALSE);
	scene.Run(UI_EXCHANGE_REQUEST_CANCEL);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.Run(UI_EXCHANGE_ACCEPT, TRUE);
	scene.Run(UI_EXCHANGE_REQUEST_CANCEL);
	CHECK(calls.empty());
	CHECK(g_pTradeManager == NULL);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_TRADE_REQUEST), int(scene.temp.GetMode()));
}

// UI_EXCHANGE_REQUEST_CANCEL (L7303): the player withdraws his own
// request.
TEST(UIMessages3TradeRequest, WithdrawingMyRequestTellsTheOtherSide)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.temp.Value1 = kOther;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.Run(UI_EXCHANGE_REQUEST_CANCEL);
	CHECK(Same({"trade prepare|500|" + N(CG_TRADE_PREPARE_CODE_CANCEL)}));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK(!scene.Waiting());
	calls.clear();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.Run(UI_EXCHANGE_REQUEST_CANCEL);
	CHECK(Same({"trade prepare|500|" + N(CG_TRADE_PREPARE_CODE_CANCEL)}));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_PARTY), int(scene.me->GetWaitVerify()));
	CHECK(!scene.Waiting());
}

//----------------------------------------------------------------------
// UI_CHANGE_OPTION (L7348): left = the option, right = its value, in any
// mode. The music and sound state is DXLib's stubs', saved and restored.
//----------------------------------------------------------------------
namespace {
struct OptionScene : Scene
{
	MMusic oldMusic = g_Music;
	bool oldMute = g_SDLAudio.IsMute();
	LONG oldLimit = g_SDLAudio.GetVolumeLimit();
	CSoundPartManager* oldSounds = g_pSoundManager;
	OptionScene()
	{
		// With SDL audio off the OGG stream is never touched.
		CHECK(!g_SDLAudio.IsInit());
		g_pSoundManager = NULL;
		g_Music = MMusic();
		option.PlayWaveMusic = FALSE;
		option.PlayMusic = TRUE;
		option.PlaySound = TRUE;
		option.VolumeMusic = 3;
		option.VolumeSound = 4;
		g_SDLAudio.UnSetMute();
		g_SDLAudio.SetVolumeLimit(-1234);
	}
	~OptionScene()
	{
		g_pSoundManager = oldSounds;
		g_SDLAudio.SetVolumeLimit(oldLimit);
		if (oldMute) g_SDLAudio.SetMute(); else g_SDLAudio.UnSetMute();
		g_Music = oldMusic;
	}
	// Music that plays.
	void Playing() { g_Music.m_bInit = true; g_Music.Play("theme.mid"); }
};
} // namespace

TEST(UIMessages3ChangeOption, TheWaveSwitchPlaysTheZonesMusicInTheGameOnly)
{
	OptionScene scene;
	scene.Playing();
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_WAV, TRUE);
	CHECK_EQ(TRUE, scene.option.PlayWaveMusic);
	CHECK(Same({"zone music"}));
	CHECK(!g_Music.IsPlay());
	// At the main menu, the title's (no theme here: nothing more).
	calls.clear();
	g_Mode = MODE_MAINMENU;
	scene.Playing();
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_WAV, TRUE);
	CHECK(calls.empty());
	CHECK(!g_Music.IsPlay());
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_WAV, FALSE);
	CHECK_EQ(FALSE, scene.option.PlayWaveMusic);
	CHECK(calls.empty());
}

TEST(UIMessages3ChangeOption, TheMusicSwitchAlwaysPlaysTheTitlesMusic)
{
	OptionScene scene;
	scene.option.PlayWaveMusic = TRUE;
	scene.option.PlayMusic = FALSE;
	scene.Playing();
	// Recorded defect (L7443-7446): in the game too, the title's music
	// (PlayTitleMusic) is played, not the zone's.
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_MUSIC, TRUE);
	CHECK_EQ(TRUE, scene.option.PlayMusic);
	CHECK(calls.empty());
	CHECK(!g_Music.IsPlay());
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_MUSIC, FALSE);
	CHECK_EQ(FALSE, scene.option.PlayMusic);
	CHECK(calls.empty());
}

TEST(UIMessages3ChangeOption, TheSoundSwitchMutesAndUnmutes)
{
	OptionScene scene;
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_SOUND, FALSE);
	CHECK_EQ(FALSE, scene.option.PlaySound);
	CHECK(g_SDLAudio.IsMute());
	g_Mode = MODE_MAINMENU;
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_SOUND, TRUE);
	CHECK_EQ(TRUE, scene.option.PlaySound);
	CHECK(!g_SDLAudio.IsMute());
	CHECK(calls.empty());
}

TEST(UIMessages3ChangeOption, TheSoundVolumeIsZeroToFifteenAndClicks)
{
	OptionScene scene;
	for (int value : {0, 7, 15})
	{
		calls.clear();
		scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_SOUND_VOLUME, value);
		CHECK_EQ(value, scene.option.VolumeSound);
		CHECK_EQ(LONG(value * SOUND_DEGREE + SOUND_MIN), g_SDLAudio.GetVolumeLimit());
		CHECK(Same({"sound|" + N(SOUND_SLAYER_BUTTON)}));
	}
	calls.clear();
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_SOUND_VOLUME, 16);
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_SOUND_VOLUME, -1);
	CHECK(calls.empty());
	CHECK_EQ(15, scene.option.VolumeSound);
	CHECK_EQ(LONG(15 * SOUND_DEGREE + SOUND_MIN), g_SDLAudio.GetVolumeLimit());
}

TEST(UIMessages3ChangeOption, TheMusicVolumeSetsTheMidiVolumeOrOnlyTheOption)
{
	OptionScene scene;
	scene.Playing();
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME, 5);
	CHECK_EQ(5, scene.option.VolumeMusic);
	CHECK_EQ(0x5FFF, int(g_Music.GetVolume()));
	CHECK(g_Music.IsPlay());
	// Paused music only takes the volume, and stays paused.
	g_Music.Pause();
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME, 6);
	CHECK_EQ(0x6FFF, int(g_Music.GetVolume()));
	CHECK(g_Music.IsPause());
	CHECK(!g_Music.IsPlay());
	scene.option.PlayMusic = FALSE;
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME, 0);
	CHECK_EQ(0x0FFF, int(g_Music.GetVolume()));
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME, 15);
	CHECK_EQ(0xFFFF, int(g_Music.GetVolume()));
	// Out of range: nothing.
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME, 16);
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME, -1);
	CHECK_EQ(15, scene.option.VolumeMusic);
	CHECK_EQ(0xFFFF, int(g_Music.GetVolume()));
	// The wave music's volume is the stream's, which is off here.
	scene.option.PlayWaveMusic = TRUE;
	scene.Run(UI_CHANGE_OPTION, C_VS_UI_OPTION::CHECK_VALUE_MUSIC_VOLUME, 2);
	CHECK_EQ(2, scene.option.VolumeMusic);
	CHECK_EQ(0xFFFF, int(g_Music.GetVolume()));
	CHECK(calls.empty());
}

TEST(UIMessages3ChangeOption, AnOptionItDoesNotKnowChangesNothing)
{
	OptionScene scene;
	const UserOption before = scene.option;
	scene.Run(UI_CHANGE_OPTION, 999, 1);
	scene.Run(UI_CHANGE_OPTION, -1, 1);
	CHECK(calls.empty());
	CHECK_EQ(before.PlayWaveMusic, scene.option.PlayWaveMusic);
	CHECK_EQ(before.PlayMusic, scene.option.PlayMusic);
	CHECK_EQ(before.PlaySound, scene.option.PlaySound);
	CHECK_EQ(before.VolumeMusic, scene.option.VolumeMusic);
	CHECK_EQ(before.VolumeSound, scene.option.VolumeSound);
	CHECK_EQ(LONG(-1234), g_SDLAudio.GetVolumeLimit());
}

//----------------------------------------------------------------------
// UI_CLOSE_OPTION (L7524): saves the display settings, the client
// configuration and the options, in any mode; at the main menu a change
// of 3D acceleration restarts the graphics (MODE_CHANGE_OPTION).
//----------------------------------------------------------------------
namespace {
struct CloseOptionScene : Scene
{
	std::filesystem::path oldDir = std::filesystem::current_path();
	std::filesystem::path dir = std::filesystem::temp_directory_path() / ("darkeden_uimsg3_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
	Properties files;
	KeyAccelerator keys;
	Properties* oldFiles = g_pFileDef;
	KeyAccelerator* oldKeys = g_pKeyAccelerator;
	BOOL old3D = g_bEnable3DHAL;
	CloseOptionScene()
	{
		std::error_code error;
		std::filesystem::remove_all(dir, error);
		std::filesystem::create_directories(dir);
		std::filesystem::current_path(dir);
		files.setProperty("FILE_INFO_CLIENTCONFIG", "client.cfg");
		files.setProperty("FILE_INFO_USEROPTION", "user.set");
		g_pFileDef = &files;
		keys.Init(MAX_ACCELERATOR);
		g_pKeyAccelerator = &keys;
	}
	~CloseOptionScene()
	{
		g_bEnable3DHAL = old3D;
		g_pKeyAccelerator = oldKeys;
		g_pFileDef = oldFiles;
		std::error_code error;
		std::filesystem::current_path(oldDir, error);
		std::filesystem::remove_all(dir, error);
	}
	static std::string Read(const char* path)
	{
		std::ifstream in(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
};
} // namespace

TEST(UIMessages3CloseOption, EverythingIsSavedAndTheGameKeepsItsMode)
{
	CloseOptionScene scene;
	scene.option.VolumeMusic = 9;
	g_bEnable3DHAL = TRUE;
	scene.option.Use3DHAL = TRUE;
	scene.Run(UI_CLOSE_OPTION);
	CHECK(CloseOptionScene::Read("UserSet/Display.ini").find("Fullscreen=") == 0);
	CHECK(!CloseOptionScene::Read("client.cfg").empty());
	CHECK(CloseOptionScene::Read("user.set").find("9\tVolumeMusic\n") != std::string::npos);
	CHECK_EQ(int(MODE_GAME), int(g_Mode));
	CHECK(calls.empty());
}

TEST(UIMessages3CloseOption, AtTheMainMenuTheAccelerationChoiceMayRestartTheGraphics)
{
	CloseOptionScene scene;
	for (BOOL enabled : {FALSE, TRUE})
		for (BOOL use : {FALSE, TRUE})
		{
			g_Mode = MODE_MAINMENU;
			g_bEnable3DHAL = enabled;
			scene.option.Use3DHAL = use;
			scene.Run(UI_CLOSE_OPTION);
#ifdef PLATFORM_WINDOWS
			// Recorded defect (L7552-7562): Windows compiles "if (true)",
			// so every close with acceleration off (the default) restarts
			// the graphics, and turning it on never does.
			const bool restart = !use;
#else
			const bool restart = enabled && use;
#endif
			CHECK_EQ(int(restart ? MODE_CHANGE_OPTION : MODE_MAINMENU), int(g_Mode));
		}
	CHECK(calls.empty());
}

TEST(UIMessages3CloseOption, WithoutTheFileNamesTheSaveThrowsAfterTheDisplaySettings)
{
	CloseOptionScene scene;
	Properties none;
	g_pFileDef = &none;
	bool threw = false;
	try
	{
		scene.Run(UI_CLOSE_OPTION);
	}
	catch (...)
	{
		threw = true;
	}
	g_pFileDef = &scene.files;
	CHECK(threw);
	CHECK(!CloseOptionScene::Read("UserSet/Display.ini").empty());
	CHECK(CloseOptionScene::Read("client.cfg").empty());
	CHECK(CloseOptionScene::Read("user.set").empty());
}

//----------------------------------------------------------------------
// UI_CLOSE_COMPUTER (L7626): back to the talk, and the connection the
// computer kept is let go.
//----------------------------------------------------------------------
TEST(UIMessages3CloseComputer, ClosingTheComputerLetsTheConnectionGo)
{
	Scene scene;
	g_pUserInformation->KeepConnection = TRUE;
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_CLOSE_COMPUTER);
	CHECK(calls.empty());
	CHECK_EQ(TRUE, g_pUserInformation->KeepConnection);
	g_Mode = MODE_GAME;
	scene.Run(UI_CLOSE_COMPUTER);
	CHECK(Same({"start 1"}));
	CHECK_EQ(FALSE, g_pUserInformation->KeepConnection);
}

//----------------------------------------------------------------------
// UI_CLOSE_TUTORIAL_EXIT (L7649): left = the weapon chosen, 0 to 7.
//----------------------------------------------------------------------
TEST(UIMessages3TutorialExit, EachChoiceAsksForItsWeapon)
{
	Scene scene;
	const ITEM_CLASS weapons[] = {ITEM_CLASS_BLADE, ITEM_CLASS_SWORD, ITEM_CLASS_MACE, ITEM_CLASS_CROSS,
		ITEM_CLASS_AR, ITEM_CLASS_SR, ITEM_CLASS_SMG, ITEM_CLASS_SG};
	for (int arms = 0; arms < 8; ++arms)
	{
		calls.clear();
		scene.Run(UI_CLOSE_TUTORIAL_EXIT, arms);
		CHECK(Same({"newbie|" + N(weapons[arms])}));
	}
	calls.clear();
	scene.Run(UI_CLOSE_TUTORIAL_EXIT, -1);
	scene.Run(UI_CLOSE_TUTORIAL_EXIT, 8);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_CLOSE_TUTORIAL_EXIT, 0);
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// UI_SELECT_ELEVATOR (L7746): left = the floor button, 0 the top (4) to 4
// the basement; the floors are the portals of the player's cell, in zone
// order, the top button the last.
//----------------------------------------------------------------------
namespace {
void Floors(Scene& scene, std::initializer_list<int> zones)
{
	for (int zoneID : zones) CHECK(scene.zone->m_ppSector[5][5].AddPortal(0, zoneID));
}
} // namespace

TEST(UIMessages3Elevator, EachButtonGoesToItsFloor)
{
	Scene scene;
	Floors(scene, {30, 10, 50, 20, 40});
	for (int left = 0; left <= 4; ++left)
	{
		calls.clear();
		scene.Run(UI_SELECT_ELEVATOR, left);
		CHECK(Same({"portal|" + N(50 - 10 * left)}));
	}
	// Recorded defect (L7766-7768): a button past the basement makes a
	// negative count, which goes to the first floor; one above the top
	// is ignored.
	calls.clear();
	scene.Run(UI_SELECT_ELEVATOR, 5);
	CHECK(Same({"portal|10"}));
	calls.clear();
	scene.Run(UI_SELECT_ELEVATOR, -1);
	CHECK(calls.empty());
}

TEST(UIMessages3Elevator, FewerFloorsCountFromTheBottom)
{
	Scene scene;
	Floors(scene, {10, 20, 30});
	scene.Run(UI_SELECT_ELEVATOR, 2);
	scene.Run(UI_SELECT_ELEVATOR, 3);
	scene.Run(UI_SELECT_ELEVATOR, 4);
	CHECK(Same({"portal|30", "portal|20", "portal|10"}));
	// Recorded defect (L7776-7785): a button whose count reaches the
	// number of floors (left 0 or 1 here) steps to the list's end and reads
	// it; not run.
}

TEST(UIMessages3Elevator, NothingOffAPortal)
{
	Scene scene;
	scene.Run(UI_SELECT_ELEVATOR, 2);
	// Floors in the next cell do not count.
	for (int zoneID : {10, 20, 30}) CHECK(scene.zone->m_ppSector[5][6].AddPortal(0, zoneID));
	scene.Run(UI_SELECT_ELEVATOR, 2);
	Floors(scene, {10, 20, 30, 40, 50});
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Run(UI_SELECT_ELEVATOR, 2);
	CHECK(calls.empty());
}
