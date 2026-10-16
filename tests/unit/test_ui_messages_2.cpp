#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "UIMessageManager.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MZoneTable.h"
#include "MParty.h"
#include "MChatManager.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MActionInfoTable.h"
#include "RankBonusTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "MSkillManager.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MQuickSlot.h"
#include "MTradeManager.h"
#include "MWarManager.h"
#include "MEventManager.h"
#include "TempInformation.h"
#include "UserOption.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "HelpEvent.h"
#include "WorldEventsHost.h"
#include "PlayerStateHost.h"
#include "PlayerPresentationHost.h"
#include "ClientMode.h"
#include "ZoneFlags.h"
#include "FrameClock.h"
#include "MonotonicClock.h"
#include "GameSocket.h"
#include "GameMessages.h"
#include "CMessageArray.h"
#include "ClientPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptOutputStream.h"
#include "Exception.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_item.h"
#include "VS_UI_mouse_pointer.h"
#include "Cpackets/CGAddMouseToGear.h"
#include "Cpackets/CGAddMouseToInventory.h"
#include "Cpackets/CGAddMouseToQuickSlot.h"
#include "Cpackets/CGAddMouseToZone.h"
#include "Cpackets/CGGlobalChat.h"
#include "Cpackets/CGGuildChat.h"
#include "Cpackets/CGPartySay.h"
#include "Cpackets/CGRangerSay.h"
#include "Cpackets/CGRelicToObject.h"
#include "Cpackets/CGSay.h"
#include "Cpackets/CGTameMonster.h"
#include "Cpackets/CGTradeAddItem.h"
#include "Cpackets/CGTradeRemoveItem.h"
#include "Cpackets/CGUseMessageItemFromInventory.h"
#include "Cpackets/CGWhisper.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// UIMessageManager.cpp's (gameclient's since task 4.258), declared by no
// header: the master command toggle, the flag check, the item factory.
extern int g_C2G;
bool IsExistCorpseFromPlayer(MCreature* OriginCreature, int creature_type);
MItem* NewItemOfClass(ITEM_CLASS itemClass);

// The UI's results, second part (UIMessageManager.cpp lines 2109-4020 at
// 7282659c): the chat line and its commands, the logout, the skill
// selection, the flag-corpse search and the four drops of the mouse's
// item (to the zone, the inventory, the quick slot and the gear). Each
// test runs the handler through Execute in MODE_GAME, with the model the
// game has then: the player, the zone, the inventory, the three gears and
// the request register. The socket renders every packet it is given and
// serialises it for real, so a packet that cannot be written throws as it
// would in the game. gC_vs_ui is never initialised here, so its windows
// do nothing and its IsRunning* questions answer false: what is pinned is
// what each message sends, what it does to the model and which hosts it
// calls. The chat history's lines, the ESC key the logout presses, the
// slayer's quick item size and the trade window's branches reach only the
// UI: they run here and do nothing.
//
// Not run: the immediate logout (ExecuteLogout starts the title screen);
// the Korean '@' commands (lowering them passes negative chars to tolower
// off Windows, L2505-2506: undefined); a competence player in a zone the
// zone table lacks (a NULL dereference, L2889); a gear slot outside its
// enum (the cast at L3873-3889 is undefined).
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

std::string Render(Packet& packet)
{
	switch (packet.getPacketID())
	{
	case Packet::PACKET_CG_SAY: {
		CGSay& p = static_cast<CGSay&>(packet);
		return "say|" + N(p.getColor()) + "|" + p.getMessage();
	}
	case Packet::PACKET_CG_PARTY_SAY: {
		CGPartySay& p = static_cast<CGPartySay&>(packet);
		return "party|" + N(p.getColor()) + "|" + p.getMessage();
	}
	case Packet::PACKET_CG_GUILD_CHAT: {
		CGGuildChat& p = static_cast<CGGuildChat&>(packet);
		return "guild|" + N(p.GetType()) + "|" + N(p.getColor()) + "|" + p.getMessage();
	}
	case Packet::PACKET_CG_RANGER_SAY: {
		CGRangerSay& p = static_cast<CGRangerSay&>(packet);
		return "ranger|" + p.getMessage();
	}
	case Packet::PACKET_CG_GLOBAL_CHAT: {
		CGGlobalChat& p = static_cast<CGGlobalChat&>(packet);
		return "shout|" + N(p.getColor()) + "|" + p.getMessage();
	}
	case Packet::PACKET_CG_WHISPER: {
		CGWhisper& p = static_cast<CGWhisper&>(packet);
		return "whisper|" + p.getName() + "|" + N(p.getColor()) + "|" + p.getMessage();
	}
	case Packet::PACKET_CG_USE_MESSAGE_ITEM_FROM_INVENTORY: {
		CGUseMessageItemFromInventory& p = static_cast<CGUseMessageItemFromInventory&>(packet);
		return "toall|" + N(p.getObjectID()) + "|" + N(p.getX()) + "|" + N(p.getY()) + "|" + N(p.getInventoryItemObjectID()) + "|" + p.getMessage();
	}
	case Packet::PACKET_CG_RELIC_TO_OBJECT: {
		CGRelicToObject& p = static_cast<CGRelicToObject&>(packet);
		// X and Y are left out: neither the handler nor the constructor
		// sets them (recorded in the drop tests).
		return "relic|" + N(p.getItemObjectID()) + "|" + N(p.getObjectID());
	}
	case Packet::PACKET_CG_TAME_MONSTER: {
		CGTameMonster& p = static_cast<CGTameMonster&>(packet);
		return "tame|" + N(p.getObjectID());
	}
	case Packet::PACKET_CG_ADD_MOUSE_TO_ZONE: {
		CGAddMouseToZone& p = static_cast<CGAddMouseToZone&>(packet);
		return "to zone|" + N(p.getObjectID());
	}
	case Packet::PACKET_CG_ADD_MOUSE_TO_INVENTORY: {
		CGAddMouseToInventory& p = static_cast<CGAddMouseToInventory&>(packet);
		return "to inventory|" + N(p.getObjectID()) + "|" + N(p.getInvenX()) + "|" + N(p.getInvenY()) + "|" + N(p.getInventoryItemObjectID());
	}
	case Packet::PACKET_CG_ADD_MOUSE_TO_QUICKSLOT: {
		CGAddMouseToQuickSlot& p = static_cast<CGAddMouseToQuickSlot&>(packet);
		return "to quickslot|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_ADD_MOUSE_TO_GEAR: {
		CGAddMouseToGear& p = static_cast<CGAddMouseToGear&>(packet);
		return "to gear|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_TRADE_REMOVE_ITEM: {
		CGTradeRemoveItem& p = static_cast<CGTradeRemoveItem&>(packet);
		return "trade remove|" + N(p.getTargetObjectID()) + "|" + N(p.getItemObjectID());
	}
	case Packet::PACKET_CG_TRADE_ADD_ITEM: {
		CGTradeAddItem& p = static_cast<CGTradeAddItem&>(packet);
		return "trade add|" + N(p.getTargetObjectID()) + "|" + N(p.getItemObjectID());
	}
	default:
		return "packet|" + N(packet.getPacketID());
	}
}

// The game server's connection: each packet is rendered, then written to
// the output stream as the game writes it (write() throws there).
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override
	{
		calls.push_back(Render(*pPacket));
		ClientPlayer::sendPacket(pPacket);
	}
	void processOutput() override { calls.push_back("flush"); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

// The packets as a fresh connection's stream writes them (encrypt code 0,
// as the Wire's).
std::vector<unsigned char> Encoded(const std::vector<const Packet*>& packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream encoded(&out);
	encoded.setEncryptCode(0);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

// What the view has selected.
TYPE_OBJECTID selectedCreature = OBJECTID_NULL;
TYPE_OBJECTID selectedItem = OBJECTID_NULL;
const UIMessages::Host messagesHost = []() {
	UIMessages::Host host{};
	host.GetSelectedCreature = []() { calls.push_back("selected creature?"); return selectedCreature; };
	host.GetSelectedItemID = []() { calls.push_back("selected item?"); return selectedItem; };
	host.SetMode = [](CLIENT_MODE mode) { calls.push_back("mode|" + N(mode)); };
	host.StopLoadingThread = []() { calls.push_back("stop loading"); };
	host.ReleaseGameObject = []() { calls.push_back("release game"); };
	return host;
}();
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.SetWeather = [](int weather, int level) { calls.push_back("weather|" + N(weather) + "|" + N(level)); };
	return host;
}();
MonotonicClock::TimePoint now = MonotonicClock::TimePoint() + MonotonicClock::Millis(9000000);
MonotonicClock::TimePoint Now() { return now; }
const MItemHost itemHost = []() {
	MItemHost host{};
	host.PlayItemSound = [](TYPE_SOUNDID soundID) { calls.push_back("item sound|" + N(soundID)); };
	host.RecalculateStatus = []() { calls.push_back("recalculate"); };
	host.ResetQuickItemSlot = []() { calls.push_back("quick slots reset"); };
	host.pCurrentTime = &now;
	return host;
}();
const MHelpHost helpHost = []() {
	MHelpHost host{};
	host.AddHelpMail = [](HELP_EVENT he) { calls.push_back("help|" + N(he)); };
	return host;
}();
const PlayerPresentation::Host presentationHost = []() {
	PlayerPresentation::Host host{};
	host.LockItem = []() { calls.push_back("lock item"); };
	return host;
}();
// The skill set's refresh asks the player's status first: that question
// is how a test sees it.
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Status = []() -> MStatus* { calls.push_back("skills refreshed"); return NULL; };
	return host;
}();
// The message boxes: their text.
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewMessage = [](int, int, int, std::uint16_t) { return reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(1)); };
	host.SetMessage = [](DialogWindow::Window*, const char* const* pLines, int count, bool) {
		std::string line = "popup";
		for (int i = 0; i < count; ++i) line += std::string("|") + pLines[i];
		calls.push_back(line);
	};
	return host;
}();

// The player, with what the tests set directly.
struct Me : MPlayer
{
	using MCreature::m_bEffectStatus;
	using MCreature::m_bAlive;
	using MCreature::m_bInCasket;
	using MCreature::m_Action;
};

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_mapCreature;
	using MZone::m_mapItem;
	using MZone::SectorAt;
};

const int kZoneID = 60;
const TYPE_SECTORPOSITION kX = 10, kY = 10;	// the player's cell
const DWORD kColor = 0x123456;
const char* const kMe = "Hero";

TYPE_SOUNDID InventorySound(int itemClass) { return static_cast<TYPE_SOUNDID>(300 + itemClass); }
TYPE_SOUNDID GearSound(int itemClass) { return static_cast<TYPE_SOUNDID>(600 + itemClass); }

// Everything Execute and the handlers read, saved and given back after.
struct Scene : GameModelWorld
{
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MActionInfoTable actions;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	CZoneTable zones;
	MWarManager war;
	MEventQueue events;
	CMessageArray systemLog;
	MChatManager chat;
	UserOption option;
	UIDialog dialog;
	TempInformation temp;
	MInventory inventory;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MSkillSet skills;
	MBelt belt;
	MOustersArmsBand band1, band2;
	C_VS_UI_ITEM uiItem;
	C_VS_UI_MOUSE_POINTER mouse;
	Wire wire;
	UIMessageManager manager;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	CZoneTable* oldZones = g_pZoneTable;
	MWarManager* oldWar = g_pWarManager;
	MEventQueue* oldEvents = g_pEventManager;
	CMessageArray* oldSystemLog = g_pSystemMessage;
	MChatManager* oldChat = g_pChatManager;
	UserOption* oldOption = g_pUserOption;
	UIDialog* oldDialog = g_pUIDialog;
	TempInformation* oldTemp = g_pTempInformation;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MSkillSet* oldSkills = g_pSkillAvailable;
	MBelt* oldBelt = g_pQuickSlot;
	MOustersArmsBand* oldBand1 = g_pArmsBand1;
	MOustersArmsBand* oldBand2 = g_pArmsBand2;
	MTradeManager* oldTrade = g_pTradeManager;
	MParty* oldParty = g_pParty;
	C_VS_UI_ITEM* oldUiItem = gpC_item;
	C_VS_UI_MOUSE_POINTER* oldMouse = gpC_mouse_pointer;
	ClientPlayer* oldSocket = g_pSocket;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	CLIENT_MODE oldMode = g_Mode;
	bool oldWatch = g_bWatchMode;
	bool oldZoneSafe = g_bZoneSafe;
	int oldC2G = g_C2G;
	MonotonicClock::TimePoint oldFrameNow = g_FrameNow;
	MonotonicClock::ScopedTestSource clock{&Now};
	DWORD oldLock = UIDialog::IsLockInput();
	int oldRefPoints[ITEM_REF_POINT_COUNT][2];
	const UIMessages::Host* oldMessages = UIMessages::SetHost(&messagesHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const PlayerPresentation::Host* oldPresentation = PlayerPresentation::SetHost(&presentationHost);
	const PlayerState::Host* oldPlayerState = PlayerState::SetHost(&playerHost);
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	std::unique_ptr<ZoneProbe> zone;
	Me* me = nullptr;
	std::vector<MItem*> items;				// every item a test makes
	std::vector<MCreature*> creatures;		// every creature a test makes

	Scene()
	{
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) { oldRefPoints[i][0] = g_item_ref_point[i].x; oldRefPoints[i][1] = g_item_ref_point[i].y; }
		InitGameStringTable();
		// Every type a creature takes needs its action rows; the monsters'
		// tribe does not matter here.
		creatureTypes.Init(800);
		for (int type = 0; type < 800; ++type)
			testfw::MutableRow(creatureTypes, type).SetCreatureTribe(CREATURETRIBE_VAMPIRE, 0);
		testfw::MutableRow(creatureTypes, CREATURETYPE_SLAYER_MALE).SetCreatureTribe(CREATURETRIBE_SLAYER, 0);
		testfw::MutableRow(creatureTypes, CREATURETYPE_OUSTERS).SetCreatureTribe(CREATURETRIBE_OUSTERS, 0);
		g_pCreatureTable = &creatureTypes;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(100);
		g_pActionInfoTable = &actions;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
		pInfo->ID = kZoneID;
		pInfo->CompetenceZone = false;
		CHECK(zones.Add(pInfo));
		g_pZoneTable = &zones;
		g_pWarManager = &war;
		g_pEventManager = &events;
		systemLog.Init(3, 512);
		g_pSystemMessage = &systemLog;
		g_pChatManager = &chat;
		option.FilteringCurse = TRUE;
		option.ChatWhite = FALSE;
		g_pUserOption = &option;
		dialog.Init();
		g_pUIDialog = &dialog;
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		g_pTempInformation = &temp;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		vampireGear.Init();
		oustersGear.Init();
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
		g_pSkillAvailable = &skills;
		static_cast<MSlotItemManager&>(belt).Init(4);
		static_cast<MSlotItemManager&>(band1).Init(3);
		static_cast<MSlotItemManager&>(band2).Init(3);
		g_pQuickSlot = NULL;
		g_pArmsBand1 = NULL;
		g_pArmsBand2 = NULL;
		g_pTradeManager = NULL;
		g_pParty = NULL;
		gpC_item = &uiItem;
		gpC_mouse_pointer = &mouse;
		g_pSocket = &wire;
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(20), TYPE_SECTORPOSITION(20)));
		zone->SetCurrentZoneID(kZoneID);
		g_pZone = zone.get();
		g_Mode = MODE_GAME;
		g_bWatchMode = false;
		g_bZoneSafe = false;
		g_C2G = 0;
		g_FrameNow = now;
		g_pUserInformation->CharacterID = kMe;
		g_pClientConfig->TRADE_ACCEPT_DELAY_TIME = 1000;
		me = new Me;
		g_pPlayer = me;
		me->SetCreatureType(CREATURETYPE_SLAYER_MALE);
		me->SetZone(zone.get());
		me->SetPosition(kX, kY);
		selectedCreature = OBJECTID_NULL;
		selectedItem = OBJECTID_NULL;
		MItem::SetHost(&itemHost);
		SetHelpHost(&helpHost);
		manager.Init();
		calls.clear();
	}
	~Scene()
	{
		SetHelpHost(NULL);
		MItem::SetHost(NULL);
		delete me;	// clears g_pPlayer
		g_pPlayer = oldPlayer;
		for (auto& entry : zone->m_mapItem)
		{
			MSector* pSector = zone->SectorAt(entry.second->GetX(), entry.second->GetY());
			if (pSector != NULL) pSector->RemoveItem();
		}
		zone->m_mapItem.clear();
		zone->m_mapCreature.clear();
		zone.reset();
		g_pZone = oldZone;
		mouse.DropItem();
		for (MItem* pItem : items)
		{
			if (!Held(pItem))
			{
				if (pItem->GetItemClass() == ITEM_CLASS_CORPSE) static_cast<MCorpse*>(pItem)->SetCreature(NULL);
				delete pItem;
			}
		}
		for (MCreature* pCreature : creatures) delete pCreature;
		inventory.Release();
		slayerGear.Release();
		vampireGear.Release();
		oustersGear.Release();
		belt.MSlotItemManager::Release();
		band1.MSlotItemManager::Release();
		band2.MSlotItemManager::Release();
		delete g_pTradeManager;
		g_pTradeManager = oldTrade;
		g_pParty = oldParty;
		g_pSocket = oldSocket;
		gpC_mouse_pointer = oldMouse;
		gpC_item = oldUiItem;
		g_pArmsBand2 = oldBand2;
		g_pArmsBand1 = oldBand1;
		g_pQuickSlot = oldBelt;
		g_pSkillAvailable = oldSkills;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pTempInformation = oldTemp;
		dialog.Release();
		if (oldLock & FLAG_UIDIALOG_LOCK_PCTALK) UIDialog::SetLockInputPCTalk(); else UIDialog::UnSetLockInputPCTalk();
		if (oldLock & FLAG_UIDIALOG_LOCK_MESSAGE) UIDialog::SetLockInputMessage(); else UIDialog::UnSetLockInputMessage();
		g_pUIDialog = oldDialog;
		g_pUserOption = oldOption;
		g_pChatManager = oldChat;
		g_pSystemMessage = oldSystemLog;
		g_pEventManager = oldEvents;
		g_pWarManager = oldWar;
		g_pZoneTable = oldZones;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pActionInfoTable = oldActions;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
		DialogWindow::SetHost(oldWindows);
		PlayerState::SetHost(oldPlayerState);
		PlayerPresentation::SetHost(oldPresentation);
		WorldEvents::SetHost(oldWorld);
		UIMessages::SetHost(oldMessages);
		g_FrameNow = oldFrameNow;
		g_C2G = oldC2G;
		g_bZoneSafe = oldZoneSafe;
		g_bWatchMode = oldWatch;
		g_Mode = oldMode;
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) g_item_ref_point[i].Set(oldRefPoints[i][0], oldRefPoints[i][1]);
	}

	bool Held(MItem* pItem)
	{
		MItemManager* containers[] = {&inventory, &slayerGear, &vampireGear, &oustersGear, &belt, &band1, &band2};
		for (MItemManager* pContainer : containers)
			if (pContainer->GetItem(pItem->GetID()) == pItem) return true;
		return false;
	}

	void Execute(DWORD message, intptr_t left = 0, intptr_t right = 0, void* pVoid = NULL)
	{
		manager.Execute(message, left, right, pVoid);
	}
	// A chat line as the UI queues it: a writable string.
	void Say(const std::string& line, int tab = CLD_NORMAL, DWORD color = kColor)
	{
		std::vector<char> buffer(line.begin(), line.end());
		buffer.push_back('\0');
		Execute(UI_CHAT_RETURN, tab, color, buffer.data());
	}

	// An item of the class, its row in the item table one cell big.
	MItem* Item(int itemClass, TYPE_OBJECTID id, int itemType = 0, int race = FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS)
	{
		if ((*g_pItemTable)[itemClass].GetSize() == 0)
		{
			g_pItemTable->InitClass(itemClass, 50);
			for (int type = 0; type < 50; ++type)
			{
				ITEMTABLE_INFO& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
				row.SetGrid(1, 1);
				row.InventorySoundID = InventorySound(itemClass);
				row.GearSoundID = GearSound(itemClass);
				row.Race = BYTE(race);
			}
		}
		MItem* pItem = NewItemOfClass(static_cast<ITEM_CLASS>(itemClass));
		CHECK(pItem != NULL);
		pItem->SetItemType(static_cast<TYPE_ITEMTYPE>(itemType));
		pItem->SetID(id);
		items.push_back(pItem);
		return pItem;
	}
	MItem* OnMouse(int itemClass, TYPE_OBJECTID id, int itemType = 0)
	{
		MItem* pItem = Item(itemClass, id, itemType);
		mouse.PickUpItem(pItem, 0, 0);
		return pItem;
	}
	MCreature* Creature(TYPE_OBJECTID id, int creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
	{
		MCreature* pCreature = new MCreature;
		creatures.push_back(pCreature);
		pCreature->SetCreatureType(static_cast<TYPE_CREATURETYPE>(creatureType));
		pCreature->SetID(id);
		pCreature->SetPosition(x, y);
		return pCreature;
	}
	// A creature in the zone.
	MCreature* Monster(TYPE_OBJECTID id, int creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
	{
		MCreature* pCreature = Creature(id, creatureType, x, y);
		zone->m_mapCreature[id] = pCreature;
		return pCreature;
	}
	// A corpse in the zone, in the sector of (x, y), of a creature of the
	// type standing there (id + 10000).
	MCorpse* Corpse(TYPE_OBJECTID id, int creatureType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y)
	{
		MCorpse* pCorpse = static_cast<MCorpse*>(Item(ITEM_CLASS_CORPSE, id));
		pCorpse->SetCreature(Creature(id + 10000, creatureType, x, y));
		pCorpse->SetPosition(x, y);
		zone->m_mapItem[id] = pCorpse;
		CHECK(zone->SectorAt(x, y)->AddItem(pCorpse));
		return pCorpse;
	}

	// The player's words over his head, row by row.
	std::string Said()
	{
		std::string rows;
		for (int i = 0; i < g_pClientConfig->MAX_CHATSTRING; ++i)
		{
			const char* row = me->GetChatString(static_cast<BYTE>(i));
			if (row != NULL && row[0] != '\0') rows += (rows.empty() ? "" : "/") + std::string(row);
		}
		return rows;
	}
	std::vector<std::string> System()
	{
		std::vector<std::string> lines;
		for (int i = 0; i < systemLog.GetSize(); ++i)
			if (systemLog[i] != nullptr && systemLog[i][0] != '\0') lines.push_back(systemLog[i]);
		return lines;
	}
};

const std::string kChatHelp = "help|" + N(HELP_EVENT_CHAT);
std::string Say(const std::string& message, DWORD color = kColor) { return "say|" + N(color) + "|" + message; }

} // namespace

//----------------------------------------------------------------------
// The chat line: what reaches the handler at all
//----------------------------------------------------------------------
TEST(UIMessages2Chat, NoLineOrNoGameSendsNothing)
{
	Scene scene;
	// No line: not even the help event.
	scene.Execute(UI_CHAT_RETURN, CLD_NORMAL, kColor, NULL);
	CHECK(calls.empty());
	// Outside the game the handler returns; Execute checks nothing there.
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_WAIT_SELECTPC, MODE_QUIT})
	{
		g_Mode = mode;
		scene.Say("hello");
		scene.Say("hello", CLD_PARTY);
	}
	CHECK(calls.empty());
	CHECK(scene.Said().empty());
	CHECK(scene.wire.Sent().empty());
}

//----------------------------------------------------------------------
// The party and guild tabs: the line as typed, cut to 128 bytes
//----------------------------------------------------------------------
TEST(UIMessages2Chat, ThePartyTabSendsTheUntrimmedLineOnlyWithAParty)
{
	MParty party;
	Scene scene;
	scene.Say("  hi there ", CLD_PARTY);
	CHECK(Same({kChatHelp}));
	g_pParty = &party;
	calls.clear();
	scene.Say("  hi there ", CLD_PARTY);
	scene.Say("", CLD_PARTY, 7);
	CHECK(Same({"party|" + N(kColor) + "|  hi there ", kChatHelp, "party|7|", kChatHelp}));
	// Recorded defect: the master command exclusion is commented out
	// (L2142), so a '*' line - a GM command, "*mc N" - is party chat.
	calls.clear();
	scene.Say("*goes", CLD_PARTY);
	scene.Say("*mc 1", CLD_PARTY);
	CHECK(Same({"party|" + N(kColor) + "|*goes", kChatHelp, "party|" + N(kColor) + "|*mc 1", kChatHelp}));
	CHECK(scene.Said().empty());
	// A whisper is a whisper on any tab.
	calls.clear();
	scene.Say("/Nyxie psst", CLD_PARTY);
	CHECK(Same({"whisper|Nyxie|" + N(kColor) + "|psst", kChatHelp}));
	CGPartySay first, second;
	first.setColor(kColor);
	first.setMessage("  hi there ");
	second.setColor(7);
	second.setMessage("");
	CGPartySay third, fourth;
	third.setColor(kColor);
	third.setMessage("*goes");
	fourth.setColor(kColor);
	fourth.setMessage("*mc 1");
	CGWhisper whisper;
	whisper.setName("Nyxie");
	whisper.setColor(kColor);
	whisper.setMessage("psst");
	CHECK(scene.wire.Sent() == Encoded({&first, &second, &third, &fourth, &whisper}));
}

TEST(UIMessages2Chat, TheGuildAndUnionTabsSendTheUntrimmedLineWithAGuild)
{
	Scene scene;
	scene.Say("hey", CLD_GUILD);
	scene.Say("hey", CLD_UNION);
	CHECK(Same({kChatHelp, kChatHelp}));
	g_pUserInformation->GuildName = "Order";
	calls.clear();
	scene.Say(" hey ", CLD_GUILD);
	scene.Say(" hey ", CLD_UNION, 9);
	CHECK(Same({"guild|0|" + N(kColor) + "| hey ", kChatHelp, "guild|1|9| hey ", kChatHelp}));
	// '*' and '/' lines are read as on the normal tab.
	calls.clear();
	scene.Say("*goes", CLD_GUILD);
	scene.Say("/Nyxie psst", CLD_UNION);
	CHECK(Same({Say("*warp 8000 75 77"), kChatHelp, "whisper|Nyxie|" + N(kColor) + "|psst", kChatHelp}));
	CHECK(scene.Said().empty());
}

TEST(UIMessages2Chat, AnEmptyGuildLineThrowsOutOfTheHandler)
{
	// Recorded defect (L2172-2178): the guild line is neither trimmed nor
	// checked, and CGGuildChat::write refuses an empty message: the throw
	// leaves Execute, before the help event.
	Scene scene;
	g_pUserInformation->GuildName = "Order";
	bool thrown = false;
	try
	{
		scene.Say("", CLD_UNION);
	}
	catch (InvalidProtocolException&)
	{
		thrown = true;
	}
	CHECK(thrown);
	CHECK(Same({"guild|1|" + N(kColor) + "|"}));
}

TEST(UIMessages2Chat, ALineIsCutTo128BytesOnACharacterBoundary)
{
	MParty party;
	Scene scene;
	g_pParty = &party;
	g_pUserInformation->GuildName = "Order";
	const std::string over(200, 'a');
	const std::string exact(128, 'b');
	// The 128th byte is the middle of a three-byte character: it goes whole.
	const std::string split = std::string(127, 'c') + "\xEA\xB0\x80" "d";
	// Only continuation bytes (malformed): no boundary, cut at 128 anyway.
	const std::string broken(130, '\x80');
	const std::vector<std::string> lines = {over, exact, split, broken};
	const std::vector<std::string> cut = {std::string(128, 'a'), exact, std::string(127, 'c'), std::string(128, '\x80')};
	for (size_t i = 0; i < lines.size(); ++i)
	{
		calls.clear();
		scene.Say(lines[i], CLD_PARTY);
		scene.Say(lines[i], CLD_GUILD);
		scene.Say(lines[i], CLD_ZONECHAT);
		scene.Say(lines[i]);
		CHECK(Same({"party|" + N(kColor) + "|" + cut[i], kChatHelp, "guild|0|" + N(kColor) + "|" + cut[i], kChatHelp,
			"shout|" + N(kColor) + "|" + cut[i], kChatHelp, Say(cut[i]), kChatHelp}));
	}
}

//----------------------------------------------------------------------
// The normal and zone tabs
//----------------------------------------------------------------------
TEST(UIMessages2Chat, AnOrdinaryLineIsTrimmedSaidAndShownOverTheHead)
{
	Scene scene;
	scene.Say("  hello world  ");
	CHECK(Same({Say("hello world"), kChatHelp}));
	CHECK(scene.Said() == "hello world");
	CHECK_EQ(static_cast<long long>(kColor), static_cast<long long>(scene.me->GetChatColor()));
	CGSay expected;
	expected.setColor(kColor);
	expected.setMessage("hello world");
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	// A blank or empty line: only the help.
	calls.clear();
	scene.Say("     ");
	scene.Say("");
	CHECK(Same({kChatHelp, kChatHelp}));
	// '!' has no branch of its own (SYMBOL_GLOBALCHAT is used only in a
	// comment): the line is said with it.
	calls.clear();
	scene.Say("!all of you", CLD_NORMAL, 5);
	CHECK(Same({Say("!all of you", 5), kChatHelp}));
	CHECK(scene.Said() == "!all of you");
	CHECK_EQ(5, static_cast<int>(scene.me->GetChatColor()));
	// Any other tab number is the normal tab.
	calls.clear();
	scene.Say("hm", CLD_WHISPER);
	scene.Say("hm", CLD_INFO);
	CHECK(Same({Say("hm"), kChatHelp, Say("hm"), kChatHelp}));
}

TEST(UIMessages2Chat, AHiddenOrAnimalPlayerSaysNothing)
{
	Scene scene;
	auto silent = [&]() {
		calls.clear();
		scene.Say("hello");
		scene.Say("hello", CLD_ZONECHAT);
		CHECK(Same({kChatHelp, kChatHelp}));
		CHECK(scene.Said().empty());
	};
	const MCreature::MOVE_TYPE walking = scene.me->GetMoveType();
	scene.me->SetMoveType(MCreature::CREATURE_UNDERGROUND);
	silent();
	scene.me->SetMoveType(MCreature::CREATURE_FAKE_UNDERGROUND);
	silent();
	scene.me->SetMoveType(walking);
	scene.me->m_bInCasket = true;
	silent();
	scene.me->m_bInCasket = false;
	for (int type : {CREATURETYPE_BAT, CREATURETYPE_WOLF, CREATURETYPE_WER_WOLF})
	{
		scene.me->SetCreatureType(static_cast<TYPE_CREATURETYPE>(type));
		silent();
	}
	// Commands and whispers still go.
	calls.clear();
	scene.Say("*goes");
	scene.Say("/Nyxie psst");
	CHECK(Same({Say("*warp 8000 75 77"), kChatHelp, "whisper|Nyxie|" + N(kColor) + "|psst", kChatHelp}));
	// A vampire in his own form speaks.
	scene.me->SetCreatureType(CREATURETYPE_VAMPIRE_MALE1);
	calls.clear();
	scene.Say("hello");
	scene.Say("hello", CLD_ZONECHAT);
	CHECK(Same({Say("hello"), kChatHelp, "shout|" + N(kColor) + "|hello", kChatHelp}));
}

TEST(UIMessages2Chat, TheZoneTabShoutsTheTrimmedLine)
{
	Scene scene;
	scene.Say("  hello all ", CLD_ZONECHAT, 3);
	CHECK(Same({"shout|3|hello all", kChatHelp}));
	CHECK(scene.Said() == "hello all");
	CHECK_EQ(3, static_cast<int>(scene.me->GetChatColor()));
	CGGlobalChat expected;
	expected.setColor(3);
	expected.setMessage("hello all");
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	// '!' is shouted with the line; '/', '@' and '*' are read as on the
	// normal tab.
	scene.option.FilteringCurse = FALSE;
	calls.clear();
	scene.Say("!hey", CLD_ZONECHAT);
	scene.Say("/Nyxie psst", CLD_ZONECHAT);
	scene.Say("*goes", CLD_ZONECHAT);
	scene.Say("@filter", CLD_ZONECHAT);
	CHECK(Same({"shout|" + N(kColor) + "|!hey", kChatHelp, "whisper|Nyxie|" + N(kColor) + "|psst", kChatHelp,
		Say("*warp 8000 75 77"), kChatHelp, kChatHelp}));
	CHECK_EQ(TRUE, scene.option.FilteringCurse);
}

//----------------------------------------------------------------------
// The ranger's and the shout item's prefixes (game strings)
//----------------------------------------------------------------------
TEST(UIMessages2Chat, UnderDragonEyesTheRangerPrefixSpeaksToTheRangers)
{
	Scene scene;
	// Without the effect the prefix is words like any other.
	scene.Say("Ranger/hello");
	CHECK(Same({Say("Ranger/hello"), kChatHelp}));
	scene.me->m_bEffectStatus[EFFECTSTATUS_DRAGON_EYES] = true;
	calls.clear();
	scene.Say("  Ranger/hello  ", CLD_NORMAL, 4);
	// No help event: the branch returns.
	CHECK(Same({"ranger|hello"}));
	// Over the head with the prefix, in white: the colour is not passed.
	CHECK(scene.Said() == "Ranger/hello");
	CHECK_EQ(static_cast<long long>(RGB(255, 255, 255)), static_cast<long long>(scene.me->GetChatColor()));
	CGRangerSay expected;
	expected.setMessage("hello");
	CGSay before;
	before.setColor(kColor);
	before.setMessage("Ranger/hello");
	CHECK(scene.wire.Sent() == Encoded({&before, &expected}));
	// The zone tab too; the prefix is case-sensitive.
	calls.clear();
	scene.Say("Ranger/to all", CLD_ZONECHAT);
	scene.Say("ranger/lower");
	scene.Say("hello");
	CHECK(Same({"ranger|to all", Say("ranger/lower"), kChatHelp, Say("hello"), kChatHelp}));
}

TEST(UIMessages2Chat, AnEmptyRangerLineThrowsOutOfTheHandler)
{
	// Recorded defect (L2246-2251): "Ranger/" alone makes an empty
	// CGRangerSay, whose write() throws out of Execute, before the words
	// go over the head.
	Scene scene;
	scene.me->m_bEffectStatus[EFFECTSTATUS_DRAGON_EYES] = true;
	bool thrown = false;
	try
	{
		scene.Say("Ranger/");
	}
	catch (InvalidProtocolException&)
	{
		thrown = true;
	}
	CHECK(thrown);
	CHECK(Same({"ranger|"}));
	CHECK(scene.Said().empty());
}

TEST(UIMessages2Chat, TheShoutItemPrefixUsesTheFirstShoutItem)
{
	Scene scene;
	scene.Say("toall/hello");
	// No item: a message box, and no help event (the branch returns).
	CHECK(Same({"popup|" + std::string(GetGameString(UI_STRING_NO_ITEM_MESSAGE))}));
	CHECK(scene.me->IsItemCheckBufferNULL());
	// Another type of effect item is not a shout item.
	CHECK(scene.inventory.AddItem(scene.Item(ITEM_CLASS_EFFECT_ITEM, 513, 13), 9, 5));
	MItem* pYellow = scene.Item(ITEM_CLASS_EFFECT_ITEM, 512, 12);
	CHECK(scene.inventory.AddItem(pYellow, 5, 1));
	calls.clear();
	scene.Say("toall/hello");
	CHECK(Same({"toall|512|5|1|0|Hero>hello", "lock item"}));
	CHECK(scene.me->GetItemCheckBuffer() == pYellow);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	CHECK(scene.Said().empty());
	MItem* pBlue = scene.Item(ITEM_CLASS_EFFECT_ITEM, 511, 11);
	CHECK(scene.inventory.AddItem(pBlue, 2, 3));
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.Say("toall/hello");
	CHECK(Same({"toall|511|2|3|0|Hero>hello", "lock item"}));
	MItem* pGreen = scene.Item(ITEM_CLASS_EFFECT_ITEM, 510, 10);
	CHECK(scene.inventory.AddItem(pGreen, 0, 0));
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.Say("toall/hello", CLD_ZONECHAT);
	CHECK(Same({"toall|510|0|0|0|Hero>hello", "lock item"}));
	CHECK(scene.me->GetItemCheckBuffer() == pGreen);
	// Recorded defect (L2305): the check buffer is set without asking
	// whether it is free: an item waiting for the server is replaced.
	scene.me->SetItemCheckBuffer(pYellow, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	scene.Say("toall/again");
	CHECK(Same({"toall|510|0|0|0|Hero>again", "lock item"}));
	CHECK(scene.me->GetItemCheckBuffer() == pGreen);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	CGUseMessageItemFromInventory expected;
	expected.setObjectID(512);
	expected.setX(5);
	expected.setY(1);
	expected.setMessage("Hero>hello");
	const std::vector<unsigned char> sent = scene.wire.Sent();
	const std::vector<unsigned char> first = Encoded({&expected});
	CHECK(sent.size() > first.size() && std::equal(first.begin(), first.end(), sent.begin()));
}

TEST(UIMessages2Chat, AShoutItemMessageKeepsSixtyBytes)
{
	Scene scene;
	CHECK(scene.inventory.AddItem(scene.Item(ITEM_CLASS_EFFECT_ITEM, 510, 10), 0, 0));
	const std::string sixty(60, 'x');
	const std::vector<std::string> lines = {"toall/" + sixty, "toall/" + sixty + "y", "toall/",
		// Recorded defect (L2289-2291): the cut is at a byte count, so it
		// can split a character.
		"toall/" + std::string(59, 'z') + "\xEA\xB0\x80"};
	const std::vector<std::string> kept = {sixty, sixty, "", std::string(59, 'z') + "\xEA"};
	for (size_t i = 0; i < lines.size(); ++i)
	{
		scene.me->ClearItemCheckBuffer();
		calls.clear();
		scene.Say(lines[i]);
		CHECK(Same({"toall|510|0|0|0|Hero>" + kept[i], "lock item"}));
	}
	// On the party and guild tabs the prefix is party and guild chat.
	MParty party;
	g_pParty = &party;
	g_pUserInformation->GuildName = "Order";
	calls.clear();
	scene.Say("toall/x", CLD_PARTY);
	scene.Say("toall/x", CLD_GUILD);
	g_pParty = NULL;
	CHECK(Same({"party|" + N(kColor) + "|toall/x", kChatHelp, "guild|0|" + N(kColor) + "|toall/x", kChatHelp}));
}

//----------------------------------------------------------------------
// Whispers
//----------------------------------------------------------------------
TEST(UIMessages2Chat, AWhisperNeedsANameOfFourToTenAndAMessage)
{
	Scene scene;
	scene.Say("/ Nyxie   hello there ", CLD_NORMAL, 8);
	CHECK(Same({"whisper|Nyxie|8|hello there", kChatHelp}));
	CHECK(std::string(g_pUserInformation->WhisperID.GetString()) == "Nyxie ");
	CHECK(scene.Said().empty());
	CGWhisper expected;
	expected.setName("Nyxie");
	expected.setColor(8);
	expected.setMessage("hello there");
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	// The name's length, both ends.
	calls.clear();
	scene.Say("/Abcd hi");
	scene.Say("/Abcdefghij hi");
	CHECK(Same({"whisper|Abcd|" + N(kColor) + "|hi", kChatHelp, "whisper|Abcdefghij|" + N(kColor) + "|hi", kChatHelp}));
	CHECK(std::string(g_pUserInformation->WhisperID.GetString()) == "Abcdefghij ");
	// Too short, too long, myself, no message, no name: only the help, and
	// the last whisper's name stays.
	calls.clear();
	for (const char* line : {"/Abc hi", "/Abcdefghijk hi", "/Hero hi", "/Nyxie", "/Nyxie   ", "/", "  /  "})
		scene.Say(line);
	CHECK(Same(std::vector<std::string>(7, kChatHelp)));
	CHECK(std::string(g_pUserInformation->WhisperID.GetString()) == "Abcdefghij ");
}

//----------------------------------------------------------------------
// The '@' commands
//----------------------------------------------------------------------
TEST(UIMessages2Chat, IgnoreAloneIgnoresEveryoneButMe)
{
	Scene scene;
	CHECK(scene.chat.AddID("Before"));	// refused in accept mode
	scene.Say("@ignore");
	CHECK(Same({kChatHelp}));
	CHECK(scene.chat.IsIgnoreMode());
	CHECK(scene.chat.IsAcceptID(kMe));
	// The list was cleared: in ignore mode only listed names are heard.
	CHECK(!scene.chat.IsAcceptID("Before"));
	CHECK(!scene.chat.IsAcceptID("Other"));
	// The command is lowered first.
	scene.Say("@accept");
	CHECK(scene.chat.IsAcceptMode());
	scene.Say("@IGNORE");
	CHECK(scene.chat.IsIgnoreMode());
	CHECK(scene.chat.IsAcceptID(kMe));
	CHECK(scene.wire.Sent().empty());
}

TEST(UIMessages2Chat, IgnoreANameListsOrUnlistsItByTheMode)
{
	Scene scene;
	// Accept mode: the name is listed, so refused.
	scene.Say("@ignore Nyxie");
	CHECK(scene.chat.IsAcceptMode());
	CHECK(!scene.chat.IsAcceptID("Nyxie"));
	CHECK(scene.chat.IsAcceptID("Other"));
	// Ignore mode: the name is unlisted, so refused too.
	scene.Say("@ignore");
	scene.chat.AddID("Nyxie");
	CHECK(scene.chat.IsAcceptID("Nyxie"));
	scene.Say("@ignore Nyxie");
	CHECK(!scene.chat.IsAcceptID("Nyxie"));
	CHECK(scene.chat.IsAcceptID(kMe));
	// The name's length, both ends: the rest of the line is the name.
	scene.Say("@accept");
	scene.Say("@ignore Abcd");
	scene.Say("@ignore Abcdefghij");
	scene.Say("@ignore Abc");
	scene.Say("@ignore Abcdefghijk");
	scene.Say("@ignore Nyxie extra");
	CHECK(!scene.chat.IsAcceptID("Abcd"));
	CHECK(!scene.chat.IsAcceptID("Abcdefghij"));
	CHECK(scene.chat.IsAcceptID("Abc"));
	CHECK(scene.chat.IsAcceptID("Abcdefghijk"));
	CHECK(scene.chat.IsAcceptID("Nyxie"));
	CHECK(scene.chat.IsAcceptID("Nyxie extra"));
	CHECK(scene.wire.Sent().empty());
}

TEST(UIMessages2Chat, AcceptIsTheMirrorOfIgnore)
{
	Scene scene;
	scene.Say("@ignore");
	CHECK(scene.chat.IsIgnoreMode());
	// Ignore mode: the name is listed, so heard.
	scene.Say("@accept Nyxie");
	CHECK(scene.chat.IsAcceptID("Nyxie"));
	CHECK(scene.chat.IsIgnoreMode());
	// Alone: everyone heard, the list cleared (without me in it).
	scene.chat.AddID("Listed");
	scene.Say("@ACCEPT");
	CHECK(scene.chat.IsAcceptMode());
	CHECK(scene.chat.IsAcceptID("Listed"));
	CHECK(scene.chat.IsAcceptID("Nyxie"));
	CHECK(scene.chat.IsAcceptID(kMe));
	// Accept mode: the name is unlisted, so heard.
	scene.chat.AddID("Nyxie");
	CHECK(!scene.chat.IsAcceptID("Nyxie"));
	scene.Say("@accept Nyxie");
	CHECK(scene.chat.IsAcceptID("Nyxie"));
	// The name's length, both ends.
	for (const char* name : {"Abc", "Abcd", "Abcdefghij", "Abcdefghijk"})
		scene.chat.AddID(name);
	scene.Say("@accept Abc");
	scene.Say("@accept Abcd");
	scene.Say("@accept Abcdefghij");
	scene.Say("@accept Abcdefghijk");
	CHECK(!scene.chat.IsAcceptID("Abc"));
	CHECK(scene.chat.IsAcceptID("Abcd"));
	CHECK(scene.chat.IsAcceptID("Abcdefghij"));
	CHECK(!scene.chat.IsAcceptID("Abcdefghijk"));
	CHECK(scene.wire.Sent().empty());
}

TEST(UIMessages2Chat, WhatAndFilterSwitchTheCurseFilter)
{
	Scene scene;
	scene.Say("@what?");
	CHECK_EQ(FALSE, scene.option.FilteringCurse);
	scene.Say("@filter please");
	CHECK_EQ(TRUE, scene.option.FilteringCurse);
	scene.Say("@WHAT?");
	CHECK_EQ(FALSE, scene.option.FilteringCurse);
	scene.Say("@Filter");
	CHECK_EQ(TRUE, scene.option.FilteringCurse);
	CHECK(Same(std::vector<std::string>(4, kChatHelp)));
	CHECK(scene.chat.IsAcceptMode());
}

TEST(UIMessages2Chat, AnUnknownOrDebugCommandDoesNothing)
{
	Scene scene;
	scene.option.FilteringCurse = FALSE;
	for (const char* line : {"@", "@foo bar", "@connect 127.0.0.1 Nyxie", "@say hi", "@gamma 3", "@ignoreall", "@filters"})
		scene.Say(line);
	CHECK(Same(std::vector<std::string>(7, kChatHelp)));
	CHECK(scene.chat.IsAcceptMode());
	CHECK(scene.chat.IsAcceptID(kMe));
	CHECK_EQ(FALSE, scene.option.FilteringCurse);
	CHECK(scene.Said().empty());
}

//----------------------------------------------------------------------
// The '*' (master) commands: said as typed, a few rewritten first
//----------------------------------------------------------------------
TEST(UIMessages2Chat, TheInvincibleCommandTogglesEachTime)
{
	Scene scene;
	// The toggle is the handler's static: it carries over from earlier
	// tests, so the first answer is read; four lines bring it back.
	scene.Say("*iddqd");
	scene.Say("*Tnrrkt", CLD_ZONECHAT);
	scene.Say("*\xEC\x91\xA5\xEA\xB0\x93");
	scene.Say("*iddqd", CLD_GUILD);
	CHECK(calls.size() == 8);
	const std::string on = Say("*command invincible on"), off = Say("*command invincible off");
	const bool startsOn = !calls.empty() && calls[0] == on;
	CHECK(Same({startsOn ? on : off, kChatHelp, startsOn ? off : on, kChatHelp, startsOn ? on : off, kChatHelp, startsOn ? off : on, kChatHelp}));
	// Case and spacing matter.
	calls.clear();
	scene.Say("*IDDQD");
	scene.Say("*iddqd now");
	CHECK(Same({Say("*IDDQD"), kChatHelp, Say("*iddqd now"), kChatHelp}));
	CHECK(scene.Said().empty());
}

TEST(UIMessages2Chat, TheGhostCommandAsksForTheOtherState)
{
	Scene scene;
	const char* const aliases[] = {"*gaonashi", "*gaonasi", "*rkdhsktl", "*\xEA\xB0\x80\xEC\x98\xA4\xEB\x82\x98\xEC\x8B\x9C", "*ghost"};
	for (const char* alias : aliases)
		scene.Say(alias);
	scene.me->m_bEffectStatus[EFFECTSTATUS_GHOST] = true;
	for (const char* alias : aliases)
		scene.Say(alias, CLD_NORMAL, 2);
	const std::string on = Say("*command ghost on"), off = Say("*command ghost off", 2);
	CHECK(Same({on, kChatHelp, on, kChatHelp, on, kChatHelp, on, kChatHelp, on, kChatHelp,
		off, kChatHelp, off, kChatHelp, off, kChatHelp, off, kChatHelp, off, kChatHelp}));
	calls.clear();
	scene.Say("*goes");
	scene.Say("*goes 2");
	scene.Say("*say hello");
	CHECK(Same({Say("*warp 8000 75 77"), kChatHelp, Say("*goes 2"), kChatHelp, Say("*say hello"), kChatHelp}));
}

TEST(UIMessages2Chat, C2GCountsCreatesOnlyOnceSwitchedOn)
{
	Scene scene;
	auto c2g = [&](const char* line, int expected) {
		calls.clear();
		scene.Say(line);
		CHECK(Same({Say(line), kChatHelp}));
		CHECK_EQ(expected, g_C2G);
	};
	c2g("*create 1", 0);
	c2g("*C2G on", 1);
	c2g("*create 1", 2);
	c2g("*createx", 3);
	c2g("*creat", 3);
	c2g("*C2G off", 2);
	c2g("*C2Gx", 1);			// five bytes, not "on": down
	c2g("*C2G", 1);				// four bytes: a plain command
	c2g("*C2Gxon", 1);			// the fifth byte is not read: on
	c2g("*C2G  on", 0);
	c2g("*C2G off", -1);
	c2g("*create 1", -1);		// only above zero
	c2g("*c2g on", -1);
}

TEST(UIMessages2Chat, AnOperatorOutOfShapeCannotTurnHisGhostOffOutsideACompetenceZone)
{
	Scene scene;
	scene.me->m_bEffectStatus[EFFECTSTATUS_GHOST] = true;
	const std::string off = Say("*command ghost off");
	const std::string offNow = Say("*x *command ghost off now");
	const std::string on = Say("*command ghost on");
	auto run = [&]() {
		calls.clear();
		scene.Say("*ghost");
		scene.Say("*command ghost off");
		scene.Say("*x *command ghost off now");
		scene.Say("*command ghost on");
	};
	// An operator out of his operator's shape, outside a competence zone:
	// every line that turns the ghost off is swallowed.
	g_pUserInformation->bCompetence = true;
	g_pUserInformation->bCompetenceShape = false;
	run();
	CHECK(Same({kChatHelp, kChatHelp, kChatHelp, on, kChatHelp}));
	// In the operator's shape, in a competence zone, or no operator: said.
	g_pUserInformation->bCompetenceShape = true;
	run();
	CHECK(Same({off, kChatHelp, off, kChatHelp, offNow, kChatHelp, on, kChatHelp}));
	g_pUserInformation->bCompetenceShape = false;
	scene.zones.Get(kZoneID)->CompetenceZone = true;
	run();
	CHECK(Same({off, kChatHelp, off, kChatHelp, offNow, kChatHelp, on, kChatHelp}));
	scene.zones.Get(kZoneID)->CompetenceZone = false;
	g_pUserInformation->bCompetence = false;
	run();
	CHECK(Same({off, kChatHelp, off, kChatHelp, offNow, kChatHelp, on, kChatHelp}));
}

TEST(UIMessages2Chat, AMasterCommandFileRunsItsLinesOnTheSameTab)
{
	Scene scene;
	const char* const path = "MasterCommand7.txt";
	CHECK(!std::ifstream("MasterCommand8.txt"));
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file << "hello there\n*goes\n\n  /Nyxie psst  \n";
	}
	struct Remove { const char* path; ~Remove() { std::remove(path); } } remove{path};
	scene.Say("*mc 7");
	const std::string whisper = "whisper|Nyxie|" + N(kColor) + "|psst";
	// Each line runs as its own chat line (with its help), then the
	// selector's own help; the selector itself is not sent.
	CHECK(Same({Say("hello there"), kChatHelp, Say("*warp 8000 75 77"), kChatHelp, whisper, kChatHelp, kChatHelp}));
	CHECK(scene.Said() == "hello there");
	calls.clear();
	scene.Say("*mc 7", CLD_ZONECHAT);
	CHECK(Same({"shout|" + N(kColor) + "|hello there", kChatHelp, Say("*warp 8000 75 77"), kChatHelp, whisper, kChatHelp, kChatHelp}));
	// No such file: only the help. Not a selector: said as typed.
	calls.clear();
	scene.Say("*mc 8");
	scene.Say("*mc 77");
	CHECK(Same({kChatHelp, Say("*mc 77"), kChatHelp}));
}

//----------------------------------------------------------------------
// The logout. Its immediate branch (a safe zone, a safe sector, zone
// 8000) runs ExecuteLogout, whose title screen this binary cannot start;
// what is pinned is the countdown and the cases that do not log out.
//----------------------------------------------------------------------
namespace {
// The logout countdown the message schedules.
#ifdef _DEBUG
const long long kLogoutDelay = 2000;
#else
const long long kLogoutDelay = 5000;
#endif
long long MsAfterNow(MonotonicClock::TimePoint t)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(t - now).count();
}
bool LogoutEvent(Scene& scene, int delay)
{
	const MEvent* pEvent = scene.events.GetEvent(EVENTID_LOGOUT);
	if (pEvent == NULL) return false;
	return scene.events.GetEventCount() == 1
		&& pEvent->eventDelay == delay
		&& pEvent->eventFlag == EVENTFLAG_SHOW_DELAY_STRING
		&& pEvent->eventType == EVENTTYPE_ZONE
		&& pEvent->m_StringsID == std::vector<int>{STRING_MESSAGE_LOGOUT_AFTER_SECOND}
		&& pEvent->eventStartTickCount == now;
}
} // namespace

TEST(UIMessages2Logout, OutsideTheGameOrWhileWaitingNothingHappens)
{
	Scene scene;
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Execute(UI_LOGOUT);
	g_Mode = MODE_GAME;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_SKILL_SUCCESS);
	scene.Execute(UI_LOGOUT);
	CHECK(calls.empty());
	CHECK(scene.events.IsEmptyEvent());
	CHECK(!g_pUserInformation->IsLogoutScheduled());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_SKILL_SUCCESS), int(scene.me->GetWaitVerify()));
}

TEST(UIMessages2Logout, AwayFromSafetyTheLogoutCountsDown)
{
	Scene scene;
	scene.Execute(UI_LOGOUT, 1, 2, &scene);
	CHECK(calls.empty());
	CHECK(scene.wire.Sent().empty());
	CHECK_EQ(kLogoutDelay, MsAfterNow(g_pUserInformation->LogoutTime));
	CHECK(LogoutEvent(scene, 5000));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_LOGOUT), int(scene.me->GetWaitVerify()));
	// Waiting for it now: a second message does nothing.
	scene.events.RemoveAllEvent();
	scene.Execute(UI_LOGOUT);
	CHECK(scene.events.IsEmptyEvent());
}

TEST(UIMessages2Logout, AScheduledLogoutShowsTheWholeSecondsLeft)
{
	Scene scene;
	struct Case { long long left; int delay; };
	// From one second left the countdown shows again; under one, nothing.
	const Case cases[] = {{3500, 3000}, {1000, 1000}, {999, 0}, {0, 0}, {-1, 0}};
	for (const Case& c : cases)
	{
		scene.events.RemoveAllEvent();
		scene.me->SetWaitVerifyNULL();
		g_pUserInformation->LogoutTime = now + MonotonicClock::Duration(c.left);
		scene.Execute(UI_LOGOUT);
		CHECK_EQ(c.left, MsAfterNow(g_pUserInformation->LogoutTime));
		if (c.delay > 0)
		{
			CHECK(LogoutEvent(scene, c.delay));
			CHECK_EQ(int(MPlayer::WAIT_VERIFY_LOGOUT), int(scene.me->GetWaitVerify()));
		}
		else
		{
			CHECK(scene.events.IsEmptyEvent());
			CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
		}
	}
	CHECK(calls.empty());
}

TEST(UIMessages2Logout, OnlyZone8000AndTheRacesOwnSafeSectorAreSafe)
{
	Scene scene;
	auto countsDown = [&]() {
		scene.events.RemoveAllEvent();
		scene.me->SetWaitVerifyNULL();
		g_pUserInformation->CancelLogout();
		scene.Execute(UI_LOGOUT);
		return LogoutEvent(scene, 5000);
	};
	for (int zoneID : {7999, 8001})
	{
		scene.zone->SetCurrentZoneID(static_cast<WORD>(zoneID));
		CHECK(countsDown());
	}
	scene.zone->SetCurrentZoneID(kZoneID);
	for (BYTE flag : {BYTE(FLAG_SECTOR_SAFE_VAMPIRE), BYTE(FLAG_SECTOR_SAFE_OUSTERS), BYTE(FLAG_SECTOR_SAFE_NO_PK_ZONE)})
	{
		scene.zone->SectorAt(kX, kY)->SetSafe(flag);
		CHECK(countsDown());
	}
	// Another cell's safety is not the player's.
	scene.zone->SectorAt(kX, kY)->SetSafe(0);
	scene.zone->SectorAt(kX + 1, kY)->SetSafe(FLAG_SECTOR_SAFE_COMMON);
	CHECK(countsDown());
	CHECK(calls.empty());
}

TEST(UIMessages2Logout, ADeadPlayerIsToldHeCannot)
{
	Scene scene;
	scene.me->m_bAlive = false;
	scene.Execute(UI_LOGOUT);
	CHECK(scene.System() == std::vector<std::string>{GetGameString(STRING_MESSAGE_CANNOT_LOGOUT_DIED)});
	CHECK(scene.events.IsEmptyEvent());
	CHECK(!g_pUserInformation->IsLogoutScheduled());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// The skill selection
//----------------------------------------------------------------------
namespace {
struct Probe : UIMessageManager
{
	using UIMessageManager::Execute_UI_SELECT_SKILL;
	using UIMessageManager::Execute_UI_CANCEL_SELECT_SKILL;
};
} // namespace

TEST(UIMessages2Skill, TheSelectedSkillBecomesTheSpecialAction)
{
	Scene scene;
	scene.Execute(UI_SELECT_SKILL, 5, 7, &scene);
	CHECK_EQ(5, int(scene.me->GetSpecialActionInfo()));
	scene.Execute(UI_SELECT_SKILL, 99);
	CHECK_EQ(99, int(scene.me->GetSpecialActionInfo()));
	// From the first result action on, the player refuses it.
	scene.Execute(UI_SELECT_SKILL, 100);
	scene.Execute(UI_SELECT_SKILL, 0xFFFF);
	CHECK_EQ(99, int(scene.me->GetSpecialActionInfo()));
	// Recorded defect (L3105): left is cut to 16 bits before the player
	// checks it, so 0x10005 selects action 5.
	scene.Execute(UI_SELECT_SKILL, 0x10005);
	CHECK_EQ(5, int(scene.me->GetSpecialActionInfo()));
	scene.Execute(UI_SELECT_SKILL, 0);
	CHECK_EQ(0, int(scene.me->GetSpecialActionInfo()));
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Execute(UI_SELECT_SKILL, 7);
	CHECK_EQ(0, int(scene.me->GetSpecialActionInfo()));
	CHECK(calls.empty());
}

TEST(UIMessages2Skill, CancellingClearsTheSpecialActionInTheGameOnly)
{
	Scene scene;
	scene.Execute(UI_SELECT_SKILL, 5);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Execute(UI_CANCEL_SELECT_SKILL);
	CHECK_EQ(5, int(scene.me->GetSpecialActionInfo()));
	g_Mode = MODE_GAME;
	scene.Execute(UI_CANCEL_SELECT_SKILL, 1, 2, &scene);
	CHECK_EQ(int(ACTIONINFO_NULL), int(scene.me->GetSpecialActionInfo()));
	CHECK(calls.empty());
}

TEST(UIMessages2Skill, WithoutAPlayerBothAreHarmless)
{
	Scene scene;
	// Execute drops every message in the game without a player; the
	// handlers check for one themselves too.
	g_pPlayer = NULL;
	scene.Execute(UI_SELECT_SKILL, 5);
	Probe::Execute_UI_SELECT_SKILL(5, 0, NULL);
	Probe::Execute_UI_CANCEL_SELECT_SKILL(0, 0, NULL);
	g_pPlayer = scene.me;
	CHECK_EQ(int(ACTIONINFO_NULL), int(scene.me->GetSpecialActionInfo()));
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// IsExistCorpseFromPlayer: a corpse of the type in the nine cells round
// the creature
//----------------------------------------------------------------------
TEST(UIMessages2Corpse, ACorpseOfTheTypeNextToTheCreatureIsFound)
{
	Scene scene;
	MCreature* pAt = scene.Creature(900, CREATURETYPE_SLAYER_MALE, 5, 5);
	CHECK(!IsExistCorpseFromPlayer(pAt, 670));
	scene.Corpse(700, 670, 7, 5);	// two cells away
	scene.Corpse(701, 670, 5, 3);
	CHECK(!IsExistCorpseFromPlayer(pAt, 670));
	scene.Corpse(702, 671, 6, 6);	// another type
	CHECK(!IsExistCorpseFromPlayer(pAt, 670));
	CHECK(IsExistCorpseFromPlayer(pAt, 671));
	// Not a corpse; a corpse without its creature.
	MItem* pPotion = scene.Item(ITEM_CLASS_POTION, 703);
	pPotion->SetPosition(4, 4);
	scene.zone->m_mapItem[703] = pPotion;
	CHECK(scene.zone->SectorAt(4, 4)->AddItem(pPotion));
	MCorpse* pEmpty = scene.Corpse(704, 670, 4, 5);
	MCreature* pGone = pEmpty->SetCreature(NULL);
	CHECK(!IsExistCorpseFromPlayer(pAt, 670));
	pEmpty->SetCreature(pGone);
	CHECK(IsExistCorpseFromPlayer(pAt, 670));
	// Its own cell counts, and every one of the nine.
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy)
		{
			MCreature* pNear = scene.Creature(910, CREATURETYPE_SLAYER_MALE, TYPE_SECTORPOSITION(14 + dx), TYPE_SECTORPOSITION(14 + dy));
			CHECK(!IsExistCorpseFromPlayer(pNear, 672));
		}
	scene.Corpse(705, 672, 14, 14);
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy)
		{
			MCreature* pNear = scene.Creature(911, CREATURETYPE_SLAYER_MALE, TYPE_SECTORPOSITION(14 + dx), TYPE_SECTORPOSITION(14 + dy));
			CHECK(IsExistCorpseFromPlayer(pNear, 672));
		}
	CHECK(!IsExistCorpseFromPlayer(scene.Creature(912, CREATURETYPE_SLAYER_MALE, 16, 14), 672));
}

TEST(UIMessages2Corpse, CellsOffTheZoneAreSkipped)
{
	Scene scene;
	scene.Corpse(700, 670, 0, 0);
	scene.Corpse(701, 671, 19, 19);
	CHECK(IsExistCorpseFromPlayer(scene.Creature(900, CREATURETYPE_SLAYER_MALE, 0, 0), 670));
	CHECK(IsExistCorpseFromPlayer(scene.Creature(901, CREATURETYPE_SLAYER_MALE, 19, 19), 671));
	CHECK(IsExistCorpseFromPlayer(scene.Creature(902, CREATURETYPE_SLAYER_MALE, 1, 0), 670));
	CHECK(!IsExistCorpseFromPlayer(scene.Creature(903, CREATURETYPE_SLAYER_MALE, 0, 19), 670));
	CHECK(!IsExistCorpseFromPlayer(scene.Creature(904, CREATURETYPE_SLAYER_MALE, 19, 0), 671));
}

//----------------------------------------------------------------------
// The mouse's item dropped on the zone
//----------------------------------------------------------------------
namespace {
// The drop as the zone's server is asked for it, with the item waiting.
std::vector<std::string> ToZone(TYPE_OBJECTID id) { return {"to zone|" + N(id), "lock item"}; }
// What a relic-like item asks the view first.
const std::vector<std::string> kAsked = {"selected creature?", "selected item?"};
std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}
} // namespace

TEST(UIMessages2DropToClient, APlainItemIsAskedOfTheServerAndWaits)
{
	Scene scene;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	// left, right and void_ptr are not read.
	scene.Execute(UI_ITEM_DROP_TO_CLIENT, 3, 4, &scene);
	CHECK(Same(ToZone(801)));
	CHECK(scene.me->GetItemCheckBuffer() == pPotion);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE), int(scene.me->GetItemCheckBufferStatus()));
	// The item stays on the mouse until the server answers.
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
	CGAddMouseToZone expected;
	expected.setObjectID(801);
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	// Waiting for it: a second drop does nothing.
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(calls.empty());
}

TEST(UIMessages2DropToClient, NothingDropsOutsideTheGameDeadWatchingTradingOrLocked)
{
	Scene scene;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	g_Mode = MODE_GAME;
	scene.me->m_bAlive = false;
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	scene.me->m_bAlive = true;
	g_bWatchMode = true;
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	g_bWatchMode = false;
	// Dropping is locked until after ItemDropEnableTime.
	g_pUserInformation->ItemDropEnableTime = now;
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	g_pUserInformation->ItemDropEnableTime = now - MonotonicClock::Millis(1);
	// A trade, even one not shown.
	g_pTradeManager = new MTradeManager;
	g_pTradeManager->Init();
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	delete g_pTradeManager;
	g_pTradeManager = NULL;
	CHECK(calls.empty());
	CHECK(scene.me->IsItemCheckBufferNULL());
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(ToZone(801)));
	CHECK(scene.me->GetItemCheckBuffer() == pPotion);
	// Nothing on the mouse: nothing.
	scene.me->ClearItemCheckBuffer();
	scene.mouse.DropItem();
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(calls.empty());
	CHECK(scene.me->IsItemCheckBufferNULL());
}

TEST(UIMessages2DropToClient, NextToTheFlagpoleOnlyTheFlagMayBeDropped)
{
	Scene scene;
	scene.Corpse(700, 670, kX + 1, kY - 1);
	const std::vector<std::string> refused = {GetGameString(STRING_MESSAGE_CANNOT_DROP_ITEM_BY_FLAG)};
	scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(calls.empty());
	CHECK(scene.System() == refused);
	// Recorded defect (L3201-3208): the flagpole is looked for before the
	// mouse, the watch mode and the drop lock: with nothing on the mouse
	// the message shows too.
	scene.mouse.DropItem();
	g_bWatchMode = true;
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	g_bWatchMode = false;
	CHECK(scene.System() == std::vector<std::string>(2, refused[0]));
	// Another event item is no flag.
	scene.OnMouse(ITEM_CLASS_EVENT_ITEM, 802, 26);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(scene.System().size() == 3);
	CHECK(calls.empty());
	// The flag (event item 27) goes past; nothing selected, it falls.
	scene.OnMouse(ITEM_CLASS_EVENT_ITEM, 803, 27);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(Then(kAsked, ToZone(803))));
	CHECK(scene.System().size() == 3);
}

TEST(UIMessages2DropToClient, AFlagpoleTwoCellsAwayStopsNothing)
{
	Scene scene;
	scene.Corpse(700, 670, kX + 2, kY);
	scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(ToZone(801)));
	CHECK(scene.System().empty());
}

TEST(UIMessages2DropToClient, SomeItemsNeverReachTheGround)
{
	Scene scene;
	struct Case { int itemClass; int itemType; bool dropped; };
	const Case cases[] = {
		{ITEM_CLASS_EVENT_GIFT_BOX, 1, false}, {ITEM_CLASS_EVENT_GIFT_BOX, 0, true}, {ITEM_CLASS_EVENT_GIFT_BOX, 2, true},
		{ITEM_CLASS_COUPLE_RING, 0, false},
		{ITEM_CLASS_LUCKY_BAG, 3, false}, {ITEM_CLASS_LUCKY_BAG, 2, true}, {ITEM_CLASS_LUCKY_BAG, 4, true},
		{ITEM_CLASS_EVENT_ITEM, 28, false}, {ITEM_CLASS_EVENT_ITEM, 29, true},
		// Recorded defect (L3330-3331): the exclusion asks for an item of
		// both classes at once, so it never holds: a vampire couple ring
		// and a code sheet (the quest inventory shut) are dropped.
		{ITEM_CLASS_VAMPIRE_COUPLE_RING, 0, true}, {ITEM_CLASS_CODE_SHEET, 0, true},
		// The rare-item option window cannot be open here.
		{ITEM_CLASS_MIXING_ITEM, 0, true},
	};
	TYPE_OBJECTID id = 801;
	for (const Case& c : cases)
	{
		scene.me->ClearItemCheckBuffer();
		calls.clear();
		scene.OnMouse(c.itemClass, id, c.itemType);
		scene.Execute(UI_ITEM_DROP_TO_CLIENT);
		const bool same = Same(c.dropped ? ToZone(id) : std::vector<std::string>());
		if (!same) std::printf("      class %d type %d\n", c.itemClass, c.itemType);
		CHECK(same);
		++id;
	}
	// A quest item.
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.OnMouse(ITEM_CLASS_POTION, id)->SetQuestFlag();
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(calls.empty());
	CHECK(scene.me->IsItemCheckBufferNULL());
}

TEST(UIMessages2DropToClient, ARelicWithNothingSelectedFallsToTheGround)
{
	Scene scene;
	struct Case { int itemClass; int itemType; bool dropped; };
	const Case cases[] = {
		{ITEM_CLASS_RELIC, 0, true}, {ITEM_CLASS_BLOOD_BIBLE, 0, true}, {ITEM_CLASS_CASTLE_SYMBOL, 0, true},
		{ITEM_CLASS_SWEEPER, 0, true}, {ITEM_CLASS_PET_FOOD, 0, true}, {ITEM_CLASS_EVENT_ITEM, 27, true},
		// The spiral is a relic that never falls.
		{ITEM_CLASS_EVENT_ITEM, 31, false},
	};
	TYPE_OBJECTID id = 801;
	for (const Case& c : cases)
	{
		scene.me->ClearItemCheckBuffer();
		calls.clear();
		scene.OnMouse(c.itemClass, id, c.itemType);
		scene.Execute(UI_ITEM_DROP_TO_CLIENT);
		CHECK(Same(c.dropped ? Then(kAsked, ToZone(id)) : kAsked));
		++id;
	}
	// A selected creature that is not in the zone: the same, without the
	// item asked for.
	selectedCreature = 4321;
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.OnMouse(ITEM_CLASS_RELIC, id);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(Then({"selected creature?"}, ToZone(id))));
	// An item waiting for the server: nothing, after the view is asked.
	selectedCreature = OBJECTID_NULL;
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(kAsked));
}

TEST(UIMessages2DropToClient, ARelicGoesIntoTheSelectedShelfNearby)
{
	Scene scene;
	struct Case { int corpseType; int itemClass; int itemType; bool shelf; };
	const Case cases[] = {
		{370, ITEM_CLASS_RELIC, 0, false}, {371, ITEM_CLASS_RELIC, 0, true}, {376, ITEM_CLASS_RELIC, 0, true}, {377, ITEM_CLASS_RELIC, 0, false},
		{525, ITEM_CLASS_BLOOD_BIBLE, 0, false}, {526, ITEM_CLASS_BLOOD_BIBLE, 0, true}, {549, ITEM_CLASS_CASTLE_SYMBOL, 0, true}, {550, ITEM_CLASS_CASTLE_SYMBOL, 0, false},
		{559, ITEM_CLASS_RELIC, 0, false}, {560, ITEM_CLASS_RELIC, 0, true}, {563, ITEM_CLASS_RELIC, 0, true}, {564, ITEM_CLASS_RELIC, 0, false},
		// The flagpole takes event items, the sweeper's shelf sweepers.
		{670, ITEM_CLASS_EVENT_ITEM, 27, true}, {670, ITEM_CLASS_RELIC, 0, false},
		{672, ITEM_CLASS_SWEEPER, 0, true}, {672, ITEM_CLASS_RELIC, 0, false},
		{671, ITEM_CLASS_SWEEPER, 0, false},
	};
	TYPE_OBJECTID id = 801;
	for (const Case& c : cases)
	{
		// Two cells off: within reach, and off the flagpole's ring.
		MCorpse* pShelf = scene.Corpse(id + 100, c.corpseType, kX + 2, kY - 2);
		selectedItem = id + 100;
		scene.me->ClearItemCheckBuffer();
		calls.clear();
		MItem* pItem = scene.OnMouse(c.itemClass, id, c.itemType);
		scene.Execute(UI_ITEM_DROP_TO_CLIENT);
		const bool same = Same(Then(kAsked, c.shelf ? std::vector<std::string>{"relic|" + N(id) + "|" + N(id + 100), "lock item"} : ToZone(id)));
		if (!same) std::printf("      corpse type %d\n", c.corpseType);
		CHECK(same);
		CHECK(scene.me->GetItemCheckBuffer() == pItem);
		CHECK_EQ(int(c.shelf ? MPlayer::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE : MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE), int(scene.me->GetItemCheckBufferStatus()));
		scene.zone->SectorAt(kX + 2, kY - 2)->RemoveItem();
		scene.zone->m_mapItem.erase(pShelf->GetID());
		++id;
	}
	CHECK(scene.System().empty());
	// A shelf selected while an item waits: nothing but the questions.
	scene.Corpse(990, 371, kX + 1, kY + 1);
	selectedItem = 990;
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(kAsked));
	// An item that is no corpse selected: the relic falls.
	MItem* pGround = scene.Item(ITEM_CLASS_POTION, 991);
	scene.zone->m_mapItem[991] = pGround;
	selectedItem = 991;
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(Then(kAsked, ToZone(id - 1))));
	// The relic packet's X and Y are never set (Recorded defect, L3273 and
	// L3308): CGRelicToObject's constructor leaves them uninitialised and
	// write() sends them, so the wire carries two indeterminate bytes.
}

TEST(UIMessages2DropToClient, AShelfThreeCellsAwayIsTooFar)
{
	Scene scene;
	struct Case { TYPE_SECTORPOSITION x, y; bool reached; };
	const Case cases[] = {{kX + 2, kY + 2, true}, {kX - 2, kY - 2, true}, {kX + 3, kY, false}, {kX, kY + 3, false}, {kX - 3, kY + 1, false}, {kX + 1, kY - 3, false}};
	TYPE_OBJECTID id = 801;
	for (const Case& c : cases)
	{
		MCorpse* pShelf = scene.Corpse(id + 100, 371, c.x, c.y);
		selectedItem = id + 100;
		scene.me->ClearItemCheckBuffer();
		calls.clear();
		scene.OnMouse(ITEM_CLASS_RELIC, id);
		scene.Execute(UI_ITEM_DROP_TO_CLIENT);
		CHECK(Same(Then(kAsked, c.reached ? std::vector<std::string>{"relic|" + N(id) + "|" + N(id + 100), "lock item"} : std::vector<std::string>())));
		CHECK(c.reached != scene.me->IsItemCheckBufferNULL());
		scene.zone->SectorAt(c.x, c.y)->RemoveItem();
		scene.zone->m_mapItem.erase(pShelf->GetID());
		++id;
	}
	const std::string tooFar = GetGameString(UI_STRING_MESSAGE_TOO_FAR);
	CHECK(scene.System() == std::vector<std::string>(3, tooFar));
}

TEST(UIMessages2DropToClient, OnePetFoodTamesTheSelectedCreatureNearby)
{
	Scene scene;
	MCreature* pBeast = scene.Monster(901, 687, kX + 2, kY + 2);
	selectedCreature = 901;
	MItem* pFood = scene.OnMouse(ITEM_CLASS_PET_FOOD, 801);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same({"selected creature?", "tame|901", "lock item"}));
	CHECK(scene.me->GetItemCheckBuffer() == pFood);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_DROP_TO_CREATURE), int(scene.me->GetItemCheckBufferStatus()));
	CGTameMonster expected;
	expected.setObjectID(901);
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	// Two of them, or another creature: they fall to the ground.
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	pFood->SetNumber(2);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(Then({"selected creature?"}, ToZone(801))));
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	pFood->SetNumber(1);
	pBeast->SetCreatureType(686);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(Then({"selected creature?"}, ToZone(801))));
	// Too far: told so.
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.Monster(902, 687, kX, kY + 3);
	selectedCreature = 902;
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same({"selected creature?"}));
	CHECK(scene.System() == std::vector<std::string>{GetGameString(UI_STRING_MESSAGE_TOO_FAR)});
	CHECK(scene.me->IsItemCheckBufferNULL());
}

TEST(UIMessages2DropToClient, TheSpiralGoesToItsGuardianFromAnywhere)
{
	Scene scene;
	TYPE_OBJECTID id = 801;
	for (int type : {793, 794, 795})
	{
		// Recorded defect (L3305-3315): no distance check, and the
		// guardian's wounds stop before the server answers.
		MCreature* pGuardian = scene.Monster(id + 100, type, 19, 0);
		pGuardian->SetCauseCriticalWounds(10);
		selectedCreature = id + 100;
		scene.me->ClearItemCheckBuffer();
		calls.clear();
		MItem* pSpiral = scene.OnMouse(ITEM_CLASS_EVENT_ITEM, id, 31);
		scene.Execute(UI_ITEM_DROP_TO_CLIENT);
		CHECK(Same({"selected creature?", "relic|" + N(id) + "|" + N(id + 100), "lock item"}));
		CHECK(scene.me->GetItemCheckBuffer() == pSpiral);
		CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE), int(scene.me->GetItemCheckBufferStatus()));
		CHECK(!pGuardian->IsCauseCriticalWounds());
		++id;
	}
	// Another creature: nothing, the spiral never falls.
	MCreature* pOther = scene.Monster(id + 100, 796, kX + 1, kY);
	pOther->SetCauseCriticalWounds(10);
	selectedCreature = id + 100;
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.OnMouse(ITEM_CLASS_EVENT_ITEM, id, 31);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same({"selected creature?"}));
	CHECK(pOther->IsCauseCriticalWounds());
	// Another item on a guardian falls to the ground.
	selectedCreature = 901;
	calls.clear();
	scene.OnMouse(ITEM_CLASS_RELIC, id + 1);
	scene.Execute(UI_ITEM_DROP_TO_CLIENT);
	CHECK(Same(Then({"selected creature?"}, ToZone(id + 1))));
}

//----------------------------------------------------------------------
// The mouse's item dropped in the inventory
//----------------------------------------------------------------------
TEST(UIMessages2DropToInventory, TheItemGoesToTheCellAndTheServerIsTold)
{
	Scene scene;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 3, 2, NULL);
	CHECK(Same({"to inventory|801|3|2|0", "item sound|" + N(InventorySound(ITEM_CLASS_POTION))}));
	CHECK(scene.inventory.GetItem(3, 2) == pPotion);
	CHECK(scene.mouse.GetPickUpItem() == NULL);
	CHECK(scene.me->IsItemCheckBufferNULL());
	CGAddMouseToInventory expected;
	expected.setObjectID(801);
	expected.setInvenX(3);
	expected.setInvenY(2);
	CHECK(scene.wire.Sent() == Encoded({&expected}));
}

TEST(UIMessages2DropToInventory, TheItemThereComesUpOnTheMouse)
{
	Scene scene;
	MItem* pThere = scene.Item(ITEM_CLASS_EVENT_ITEM, 802, 3);
	CHECK(scene.inventory.AddItem(pThere, 4, 1));
	calls.clear();
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 4, 1, pThere);
	CHECK(Same({"to inventory|801|4|1|0", "item sound|" + N(InventorySound(ITEM_CLASS_POTION)), "sound|" + N(InventorySound(ITEM_CLASS_EVENT_ITEM))}));
	CHECK(scene.inventory.GetItem(4, 1) == pPotion);
	CHECK(scene.mouse.GetPickUpItem() == pThere);
}

TEST(UIMessages2DropToInventory, APlaceTheInventoryRefusesIsStillSent)
{
	// Recorded defect (L3536-3575): the packet goes before the inventory
	// is asked, whatever it answers, and left and right are cut to a byte.
	Scene scene;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 10, 0);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 0, 6);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, -1, 2);
	CHECK(Same({"to inventory|801|10|0|0", "to inventory|801|0|6|0", "to inventory|801|255|2|0"}));
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
	CHECK_EQ(0, scene.inventory.GetItemNum());
	// Two items under it: refused.
	CHECK(scene.inventory.AddItem(scene.Item(ITEM_CLASS_EVENT_ITEM, 802, 3), 0, 0));
	pPotion->SetItemType(1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 1).SetGrid(2, 1);
	CHECK(scene.inventory.AddItem(scene.Item(ITEM_CLASS_EVENT_ITEM, 803, 3), 1, 0));
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 0, 0);
	CHECK(Same({"to inventory|801|0|0|0"}));
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
	// 256 is cell 0.
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 256, 257);
	CHECK(Same({"to inventory|801|0|1|0", "item sound|" + N(InventorySound(ITEM_CLASS_POTION))}));
	CHECK(scene.inventory.GetItem(0, 1) == pPotion);
}

TEST(UIMessages2DropToInventory, NothingMovesOutsideTheGameDeadOrWhileWaiting)
{
	Scene scene;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 1, 1);
	g_Mode = MODE_GAME;
	scene.me->m_bAlive = false;
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 1, 1);
	scene.me->m_bAlive = true;
	scene.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 1, 1);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	MItem* pOther = scene.Item(ITEM_CLASS_POTION, 802);
	scene.me->SetItemCheckBuffer(pOther, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 1, 1);
	scene.me->ClearItemCheckBuffer();
	CHECK(calls.empty());
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
	CHECK_EQ(0, scene.inventory.GetItemNum());
	// Watching does not stop it.
	g_bWatchMode = true;
	scene.mouse.DropItem();
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 1, 1);
	CHECK(calls.empty());
	scene.mouse.PickUpItem(pPotion, 0, 0);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 1, 1);
	CHECK(Same({"to inventory|801|1|1|0", "item sound|" + N(InventorySound(ITEM_CLASS_POTION))}));
}

TEST(UIMessages2DropToInventory, WithMyTradeAcceptedTheDropWaitsForTheServer)
{
	Scene scene;
	g_pTradeManager = new MTradeManager;
	g_pTradeManager->Init();
	g_pTradeManager->SetOtherID(500);
	g_pTradeManager->AcceptMyTrade();
	MItem* pThere = scene.Item(ITEM_CLASS_EVENT_ITEM, 802, 3);
	pThere->SetTrade();
	CHECK(scene.inventory.AddItem(pThere, 4, 1));
	calls.clear();
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	CHECK(g_pTradeManager->IsAcceptTime());
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 4, 1, pThere);
	// The item there is taken out of the trade first.
	CHECK(Same({"lock item", "trade remove|500|802", "to inventory|801|4|1|0"}));
	CHECK_EQ(int(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY), int(scene.temp.GetMode()));
	CHECK_EQ(801, int(scene.temp.Value1));
	CHECK_EQ(4, int(scene.temp.Value2));
	CHECK_EQ(1, int(scene.temp.Value3));
	CHECK(scene.temp.pValue == pPotion);
	CHECK(scene.me->GetItemCheckBuffer() == pPotion);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_DROP_TO_INVENTORY), int(scene.me->GetItemCheckBufferStatus()));
	CHECK(!g_pTradeManager->IsAcceptTime());
	CHECK(g_pTradeManager->IsAcceptMyTrade());
	// Nothing moved yet.
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
	CHECK(scene.inventory.GetItem(4, 1) == pThere);
	CGTradeRemoveItem remove;
	remove.setTargetObjectID(500);
	remove.setItemObjectID(802);
	CGAddMouseToInventory add;
	add.setObjectID(801);
	add.setInvenX(4);
	add.setInvenY(1);
	CHECK(scene.wire.Sent() == Encoded({&remove, &add}));
}

TEST(UIMessages2DropToInventory, WithMyTradeAcceptedAnUntradedPlaceIsOnlyTold)
{
	Scene scene;
	g_pTradeManager = new MTradeManager;
	g_pTradeManager->Init();
	g_pTradeManager->SetOtherID(500);
	g_pTradeManager->AcceptMyTrade();
	MItem* pThere = scene.Item(ITEM_CLASS_EVENT_ITEM, 802, 3);
	CHECK(scene.inventory.AddItem(pThere, 4, 1));
	calls.clear();
	scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 4, 1, pThere);
	CHECK(Same({"lock item", "to inventory|801|4|1|0"}));
	CHECK(g_pTradeManager->IsAcceptTime());
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 7, 5, NULL);
	CHECK(Same({"lock item", "to inventory|801|7|5|0"}));
	CHECK_EQ(1, scene.inventory.GetItemNum());
	// Only the other side accepted: an ordinary drop (refusing the other
	// side's OK belongs to the trade window's branch, never open here).
	g_pTradeManager->RefuseMyTrade();
	g_pTradeManager->AcceptOtherTrade();
	scene.mouse.GetPickUpItem()->SetTrade();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.me->ClearItemCheckBuffer();
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 7, 5, NULL);
	CHECK(Same({"to inventory|801|7|5|0", "item sound|" + N(InventorySound(ITEM_CLASS_POTION))}));
	CHECK(g_pTradeManager->IsAcceptOtherTrade());
	CHECK(scene.me->IsItemCheckBufferNULL());
}

TEST(UIMessages2DropToInventory, TheTwelfthPartOfATreeAsksForTheMerge)
{
	// The twelve parts (types 0-11, or 13-24, or 29-40, mixed) in a 3x4
	// block, each in its own cell: placing the last asks for the merge.
	for (int variant : {0, 13, 29})
	{
		Scene scene;
		const int x0 = 2, y0 = 1, last = 4;
		for (int part = 0; part < 12; ++part)
		{
			if (part == last) continue;
			const int type = part + (part % 2 == 0 ? variant : 0);
			CHECK(scene.inventory.AddItem(scene.Item(ITEM_CLASS_EVENT_TREE, 900 + part, type), BYTE(x0 + part % 3), BYTE(y0 + part / 3)));
		}
		calls.clear();
		scene.OnMouse(ITEM_CLASS_EVENT_TREE, 801, last + variant);
		scene.Execute(UI_ITEM_DROP_TO_INVENTORY, x0 + last % 3, y0 + last / 3);
		CHECK(Same({"to inventory|801|3|2|0", "item sound|" + N(InventorySound(ITEM_CLASS_EVENT_TREE)), "lock item"}));
		CHECK(scene.me->GetItemCheckBuffer() == NULL);
		CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_TREE_MERGE), int(scene.me->GetItemCheckBufferStatus()));
	}
}

TEST(UIMessages2DropToInventory, AnIncompleteOrMisplacedTreeIsNoMerge)
{
	Scene scene;
	const int x0 = 2, y0 = 1;
	for (int part = 0; part < 12; ++part)
		if (part != 4 && part != 11)
			CHECK(scene.inventory.AddItem(scene.Item(ITEM_CLASS_EVENT_TREE, 900 + part, part), BYTE(x0 + part % 3), BYTE(y0 + part / 3)));
	calls.clear();
	// One part still missing.
	scene.OnMouse(ITEM_CLASS_EVENT_TREE, 801, 4);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 3, 2);
	CHECK(Same({"to inventory|801|3|2|0", "item sound|" + N(InventorySound(ITEM_CLASS_EVENT_TREE))}));
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_NULL), int(scene.me->GetItemCheckBufferStatus()));
	// The last part in another cell of the block.
	calls.clear();
	scene.OnMouse(ITEM_CLASS_EVENT_TREE, 802, 11);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 9, 5);
	CHECK(Same({"to inventory|802|9|5|0", "item sound|" + N(InventorySound(ITEM_CLASS_EVENT_TREE))}));
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_NULL), int(scene.me->GetItemCheckBufferStatus()));
	// A part whose block would start off the inventory reads nothing there.
	calls.clear();
	scene.OnMouse(ITEM_CLASS_EVENT_TREE, 803, 8);
	scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 0, 0);
	CHECK(Same({"to inventory|803|0|0|0", "item sound|" + N(InventorySound(ITEM_CLASS_EVENT_TREE))}));
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_NULL), int(scene.me->GetItemCheckBufferStatus()));
}

TEST(UIMessages2DropToInventory, OtherTreeTypesAreNotParts)
{
	// Types 12, 25-28 and 41 are not parts: no merge is looked for even in
	// a full block.
	for (int type : {12, 25, 26, 27, 28, 41})
	{
		Scene scene;
		for (int part = 0; part < 12; ++part)
			if (part != 4)
				CHECK(scene.inventory.AddItem(scene.Item(ITEM_CLASS_EVENT_TREE, 900 + part, part), BYTE(2 + part % 3), BYTE(1 + part / 3)));
		calls.clear();
		scene.OnMouse(ITEM_CLASS_EVENT_TREE, 801, type);
		scene.Execute(UI_ITEM_DROP_TO_INVENTORY, 3, 2);
		CHECK(Same({"to inventory|801|3|2|0", "item sound|" + N(InventorySound(ITEM_CLASS_EVENT_TREE))}));
		CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_NULL), int(scene.me->GetItemCheckBufferStatus()));
	}
}

//----------------------------------------------------------------------
// The mouse's item dropped in the quick slots
//----------------------------------------------------------------------
TEST(UIMessages2DropToQuickSlot, ASlayerPutsTheItemInHisBelt)
{
	Scene scene;
	g_pQuickSlot = &scene.belt;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 2, 9, &scene);
	CHECK(Same({"to quickslot|801|2", "sound|" + N(InventorySound(ITEM_CLASS_POTION))}));
	CHECK(scene.belt.GetItem(2) == pPotion);
	CHECK(scene.mouse.GetPickUpItem() == NULL);
	CGAddMouseToQuickSlot expected;
	expected.setObjectID(801);
	expected.setSlotID(2);
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	// The item there comes up on the mouse, silently; the sound is the
	// dropped item's.
	MItem* pOther = scene.OnMouse(ITEM_CLASS_MAGAZINE, 802, 1);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 2);
	CHECK(Same({"to quickslot|802|2", "sound|" + N(InventorySound(ITEM_CLASS_MAGAZINE))}));
	CHECK(scene.belt.GetItem(2) == pOther);
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
}

TEST(UIMessages2DropToQuickSlot, APlaceTheBeltRefusesIsStillSent)
{
	// Recorded defect (L3747-3795): what the belt answers is not read: the
	// packet and the sound go anyway, the slot cut to a byte.
	Scene scene;
	g_pQuickSlot = &scene.belt;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 4);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, -1);
	const std::string sound = "sound|" + N(InventorySound(ITEM_CLASS_POTION));
	CHECK(Same({"to quickslot|801|4", sound, "to quickslot|801|255", sound}));
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
	CHECK_EQ(0, scene.belt.GetItemNum());
	// Not a quick item.
	calls.clear();
	scene.OnMouse(ITEM_CLASS_NECKLACE, 802);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	CHECK(Same({"to quickslot|802|0", "sound|" + N(InventorySound(ITEM_CLASS_NECKLACE))}));
	CHECK_EQ(0, scene.belt.GetItemNum());
	// 256 is slot 0.
	calls.clear();
	scene.mouse.PickUpItem(pPotion, 0, 0);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 256);
	CHECK(Same({"to quickslot|801|0", sound}));
	CHECK(scene.belt.GetItem(0) == pPotion);
}

TEST(UIMessages2DropToQuickSlot, WithTheRegisterBusyTheItemMovesButNothingIsSent)
{
	// Recorded defect (L3747-3770): the item moves before the register is
	// read, so the client puts it in the belt without telling the server.
	Scene scene;
	g_pQuickSlot = &scene.belt;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 1);
	CHECK(calls.empty());
	CHECK(scene.belt.GetItem(1) == pPotion);
	CHECK(scene.mouse.GetPickUpItem() == NULL);
	// The check buffer is not read at all.
	scene.temp.SetMode(TempInformation::MODE_NULL);
	MItem* pOther = scene.OnMouse(ITEM_CLASS_POTION, 802);
	scene.me->SetItemCheckBuffer(pPotion, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 3);
	CHECK(Same({"to quickslot|802|3", "sound|" + N(InventorySound(ITEM_CLASS_POTION))}));
	CHECK(scene.belt.GetItem(3) == pOther);
}

TEST(UIMessages2DropToQuickSlot, NoBeltNoItemOrAVampireMovesNothing)
{
	Scene scene;
	MItem* pPotion = scene.OnMouse(ITEM_CLASS_POTION, 801);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	g_pQuickSlot = &scene.belt;
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	g_Mode = MODE_GAME;
	scene.me->m_bAlive = false;
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	scene.me->m_bAlive = true;
	scene.me->SetCreatureType(CREATURETYPE_VAMPIRE_MALE1);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	CHECK(calls.empty());
	CHECK(scene.mouse.GetPickUpItem() == pPotion);
	CHECK_EQ(0, scene.belt.GetItemNum());
	scene.me->SetCreatureType(CREATURETYPE_SLAYER_MALE);
	scene.mouse.DropItem();
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	CHECK(calls.empty());
	CHECK_EQ(0, scene.belt.GetItemNum());
	// Watching does not stop it.
	g_bWatchMode = true;
	scene.mouse.PickUpItem(pPotion, 0, 0);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	CHECK(Same({"to quickslot|801|0", "sound|" + N(InventorySound(ITEM_CLASS_POTION))}));
}

TEST(UIMessages2DropToQuickSlot, AnOustersSlotsRunAcrossBothArmsBands)
{
	Scene scene;
	scene.Item(ITEM_CLASS_OUSTERS_ARMSBAND, 990);	// the band's row
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).Value3 = 3;
	scene.me->SetCreatureType(CREATURETYPE_OUSTERS);
	g_pArmsBand1 = &scene.band1;
	g_pArmsBand2 = &scene.band2;
	const std::string sound = "sound|" + N(InventorySound(ITEM_CLASS_PUPA));
	struct Case { int left; MOustersArmsBand* pBand; int slot; int sent; };
	const Case cases[] = {{0, &scene.band1, 0, 0}, {2, &scene.band1, 2, 2}, {3, &scene.band2, 0, 3}, {5, &scene.band2, 2, 5}};
	TYPE_OBJECTID id = 801;
	for (const Case& c : cases)
	{
		calls.clear();
		MItem* pPupa = scene.OnMouse(ITEM_CLASS_PUPA, id);
		scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, c.left);
		CHECK(Same({"to quickslot|" + N(id) + "|" + N(c.sent), sound}));
		CHECK(c.pBand->GetItem(BYTE(c.slot)) == pPupa);
		++id;
	}
	// Past the second band: refused, and sent anyway (recorded above).
	calls.clear();
	MItem* pPupa = scene.OnMouse(ITEM_CLASS_PUPA, id);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 6);
	CHECK(Same({"to quickslot|" + N(id) + "|6", sound}));
	CHECK(scene.mouse.GetPickUpItem() == pPupa);
	// Without the first band the slot is the second band's and is sent
	// three on.
	g_pArmsBand1 = NULL;
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 1);
	CHECK(Same({"to quickslot|" + N(id) + "|4", sound}));
	CHECK(scene.band2.GetItem(1) == pPupa);
	// Without the second band, its slots go nowhere.
	g_pArmsBand1 = &scene.band1;
	g_pArmsBand2 = NULL;
	calls.clear();
	pPupa = scene.OnMouse(ITEM_CLASS_PUPA, id + 1);
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 3);
	g_pArmsBand1 = NULL;
	scene.Execute(UI_ITEM_DROP_TO_QUICKSLOT, 0);
	CHECK(calls.empty());
	CHECK(scene.mouse.GetPickUpItem() == pPupa);
}

//----------------------------------------------------------------------
// The mouse's item dropped on the gear
//----------------------------------------------------------------------
TEST(UIMessages2DropToGear, TheItemIsWornAndTheServerTold)
{
	Scene scene;
	MItem* pNecklace = scene.OnMouse(ITEM_CLASS_NECKLACE, 801);
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MSlayerGear::GEAR_SLAYER_NECKLACE, 9, &scene);
	CHECK(Same({"item sound|" + N(GearSound(ITEM_CLASS_NECKLACE)), "recalculate", "skills refreshed", "to gear|801|" + N(MSlayerGear::GEAR_SLAYER_NECKLACE)}));
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_NECKLACE) == pNecklace);
	CHECK(scene.mouse.GetPickUpItem() == NULL);
	CGAddMouseToGear expected;
	expected.setObjectID(801);
	expected.setSlotID(MSlayerGear::GEAR_SLAYER_NECKLACE);
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	// The one worn comes up on the mouse.
	MItem* pOther = scene.OnMouse(ITEM_CLASS_NECKLACE, 802, 1);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MSlayerGear::GEAR_SLAYER_NECKLACE);
	CHECK(Same({"item sound|" + N(GearSound(ITEM_CLASS_NECKLACE)), "recalculate", "sound|" + N(InventorySound(ITEM_CLASS_NECKLACE)), "skills refreshed", "to gear|802|" + N(MSlayerGear::GEAR_SLAYER_NECKLACE)}));
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_NECKLACE) == pOther);
	CHECK(scene.mouse.GetPickUpItem() == pNecklace);
}

TEST(UIMessages2DropToGear, AnItemTheGearRefusesLeavesTheMouseAnyway)
{
	// Recorded defect (L3865-3919): what the gear answers is not read: the
	// mouse is emptied (the item is then in no container), the skills are
	// refreshed and the packet goes with the slot the item last had.
	Scene scene;
	MItem* pNecklace = scene.OnMouse(ITEM_CLASS_NECKLACE, 801);
	pNecklace->SetItemSlot(7);
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(Same({"skills refreshed", "to gear|801|7"}));
	CHECK(scene.mouse.GetPickUpItem() == NULL);
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == NULL);
	CHECK_EQ(0, scene.slayerGear.GetItemNum());
	// A slot past the gear (still one the enum holds).
	calls.clear();
	scene.mouse.PickUpItem(pNecklace, 0, 0);
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MSlayerGear::MAX_GEAR_SLAYER);
	CHECK(Same({"skills refreshed", "to gear|801|7"}));
	CHECK(scene.mouse.GetPickUpItem() == NULL);
	CHECK_EQ(0, scene.slayerGear.GetItemNum());
	// Another race's item.
	calls.clear();
	MItem* pVampires = scene.Item(ITEM_CLASS_VAMPIRE_NECKLACE, 802, 0, FLAG_RACE_VAMPIRE);
	scene.mouse.PickUpItem(pVampires, 0, 0);
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MSlayerGear::GEAR_SLAYER_NECKLACE);
	CHECK(Same({"skills refreshed", "to gear|802|0"}));
	CHECK(scene.mouse.GetPickUpItem() == NULL);
	CHECK_EQ(0, scene.slayerGear.GetItemNum());
}

TEST(UIMessages2DropToGear, NothingIsWornOutsideTheGameDeadWatchingWaitingOrMoving)
{
	Scene scene;
	MItem* pNecklace = scene.OnMouse(ITEM_CLASS_NECKLACE, 801);
	const intptr_t slot = MSlayerGear::GEAR_SLAYER_NECKLACE;
	g_Mode = MODE_WAIT_SELECTPC;
	scene.Execute(UI_ITEM_DROP_TO_GEAR, slot);
	g_Mode = MODE_GAME;
	scene.me->m_bAlive = false;
	scene.Execute(UI_ITEM_DROP_TO_GEAR, slot);
	scene.me->m_bAlive = true;
	g_bWatchMode = true;
	scene.Execute(UI_ITEM_DROP_TO_GEAR, slot);
	g_bWatchMode = false;
	scene.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY);
	scene.Execute(UI_ITEM_DROP_TO_GEAR, slot);
	scene.temp.SetMode(TempInformation::MODE_NULL);
	// The player changes clothes only standing.
	scene.me->m_Action = ACTION_MOVE;
	scene.Execute(UI_ITEM_DROP_TO_GEAR, slot);
	scene.me->m_Action = ACTION_STAND;
	MItem* pOther = scene.Item(ITEM_CLASS_POTION, 802);
	scene.me->SetItemCheckBuffer(pOther, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_GEAR, slot);
	scene.me->ClearItemCheckBuffer();
	CHECK(calls.empty());
	CHECK(scene.mouse.GetPickUpItem() == pNecklace);
	CHECK_EQ(0, scene.slayerGear.GetItemNum());
	scene.mouse.DropItem();
	scene.Execute(UI_ITEM_DROP_TO_GEAR, slot);
	CHECK(calls.empty());
}

TEST(UIMessages2DropToGear, EachRaceWearsInItsOwnGear)
{
	Scene scene;
	scene.me->SetCreatureType(CREATURETYPE_VAMPIRE_MALE1);
	MItem* pVampires = scene.Item(ITEM_CLASS_VAMPIRE_NECKLACE, 801, 0, FLAG_RACE_VAMPIRE);
	scene.mouse.PickUpItem(pVampires, 0, 0);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MVampireGear::GEAR_VAMPIRE_NECKLACE);
	CHECK(Same({"item sound|" + N(GearSound(ITEM_CLASS_VAMPIRE_NECKLACE)), "recalculate", "skills refreshed", "to gear|801|" + N(MVampireGear::GEAR_VAMPIRE_NECKLACE)}));
	CHECK(scene.vampireGear.GetItem(MVampireGear::GEAR_VAMPIRE_NECKLACE) == pVampires);
	scene.me->SetCreatureType(CREATURETYPE_OUSTERS);
	MItem* pOusters = scene.Item(ITEM_CLASS_OUSTERS_PENDENT, 802, 0, FLAG_RACE_OUSTERS);
	scene.mouse.PickUpItem(pOusters, 0, 0);
	calls.clear();
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MOustersGear::GEAR_OUSTERS_PENDENT2);
	CHECK(Same({"item sound|" + N(GearSound(ITEM_CLASS_OUSTERS_PENDENT)), "recalculate", "skills refreshed", "to gear|802|" + N(MOustersGear::GEAR_OUSTERS_PENDENT2)}));
	CHECK(scene.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_PENDENT2) == pOusters);
	CHECK_EQ(0, scene.slayerGear.GetItemNum());
	CHECK(scene.mouse.GetPickUpItem() == NULL);
}

TEST(UIMessages2DropToGear, ASlayersBeltIsWornLikeAnyItem)
{
	// The belt's slot also resets the UI's quick item size (no UI here).
	Scene scene;
	MItem* pBelt = scene.OnMouse(ITEM_CLASS_BELT, 801);
	scene.Execute(UI_ITEM_DROP_TO_GEAR, MSlayerGear::GEAR_SLAYER_BELT);
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_BELT) == pBelt);
	CHECK(Same({"item sound|" + N(GearSound(ITEM_CLASS_BELT)), "recalculate", "skills refreshed", "to gear|801|" + N(MSlayerGear::GEAR_SLAYER_BELT)}));
	CHECK(scene.mouse.GetPickUpItem() == NULL);
}
