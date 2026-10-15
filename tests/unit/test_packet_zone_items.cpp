#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "PacketFunction.h"
#include "AddonDef.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MSector.h"
#include "MCreature.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MEffectGeneratorTable.h"
#include "MEffectTarget.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "MParty.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MQuickSlot.h"
#include "TempInformation.h"
#include "UserOption.h"
#include "SystemAvailabilities.h"
#include "FrameClock.h"
#include "SkillDef.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "DialogPresentationHost.h"
#include "PlayerPresentationHost.h"
#include "NoticePresentationHost.h"
#include "BonusSkillHost.h"
#include "VS_UI_GameCommon.h"
#include "Player.h"
#include "Cpackets/CGSay.h"
#include "Gpackets/GCDropItemToZone.h"
#include "SubItemInfo.h"

#include <cstdio>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The packet handlers' helpers for items and portals (PacketFunction.cpp,
// gameclient's since task 4.252): an item put in the zone, from the
// player's hand or new from a packet; an item or a skill card used up; a
// vampire portal opened, and a creature coming out of one or going in. A
// real zone stands behind ZoneState as g_pZone, a real MPlayer as
// g_pPlayer with a real inventory, and a generator that records each
// effect it is asked to start stands in the effect generator table; the
// UI and the view are hosts that record.
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

// What the generator was asked to start (as test_packet_skill_shows.cpp
// records it), with the portal a vampire portal's target names.
struct Started
{
	TYPE_ACTIONINFO action = 0;
	int x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
	TYPE_OBJECTID creatureID = 0;
	int direction = 0, step = 0, power = 0;
	DWORD delay = 0;
	bool result = false;
	bool portal = false;
	int serverID = 0;
	std::string owner;
	int zoneID = 0, zoneX = 0, zoneY = 0;
};
std::vector<Started> started;

const TYPE_EFFECTGENERATORID kGenerator = 7;

struct Recorder : MEffectGenerator
{
	TYPE_EFFECTGENERATORID GetID() override { return kGenerator; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override
	{
		Started s;
		s.action = info.nActionInfo;
		s.x0 = info.x0; s.y0 = info.y0; s.z0 = info.z0;
		s.x1 = info.x1; s.y1 = info.y1; s.z1 = info.z1;
		s.creatureID = info.creatureID;
		s.direction = info.direction;
		s.step = info.step;
		s.power = info.power;
		// Every action here has three phases and starts at most from the
		// second, so the target always goes on to a next phase.
		if (const MEffectTarget* pTarget = info.pEffectTarget)
		{
			s.delay = pTarget->GetDelayFrame();
			s.result = pTarget->IsExistResult();
			if (pTarget->GetEffectTargetType() == MEffectTarget::EFFECT_TARGET_PORTAL)
			{
				const MPortalEffectTarget* pPortal = static_cast<const MPortalEffectTarget*>(pTarget);
				s.portal = true;
				s.serverID = int(pPortal->GetServerID());
				s.owner = pPortal->GetOwnerName() != NULL ? pPortal->GetOwnerName() : "";
				s.zoneID = pPortal->GetZoneID();
				s.zoneX = pPortal->GetZoneX();
				s.zoneY = pPortal->GetZoneY();
			}
		}
		started.push_back(s);
		return true;
	}
};

struct Generators : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
};

// What a creature keeps to itself, read through member pointers so the
// creature need not be of the reading type.
struct CreatureAccess : MCreature
{
	using MCreature::m_CutHeightCountInc;
	using MCreature::m_CutHeightCountLast;
};
int CutInc(const MCreature* p) { return p->*(&CreatureAccess::m_CutHeightCountInc); }
int CutLast(const MCreature* p) { return p->*(&CreatureAccess::m_CutHeightCountLast); }

// The zone's own creatures report themselves through ZoneState, as
// GameInit's entries forward them to g_pZone.
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return g_pZone != NULL; };
	host.GetID = []() { return g_pZone != NULL ? int(g_pZone->GetID()) : int(ZONEID_NULL); };
	host.GetCreature = [](TYPE_OBJECTID id) { return g_pZone->GetCreature(id); };
	host.GetWidth = []() { return g_pZone != NULL ? int(g_pZone->GetWidth()) : 0; };
	host.GetHeight = []() { return g_pZone != NULL ? int(g_pZone->GetHeight()) : 0; };
	host.GetSector = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) -> const MSector* { return &g_pZone->GetSector(x, y); };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { return g_pZone != NULL ? g_pZone->GetFakeCreature(id) : NULL; };
	host.RemoveFakeCreature = [](TYPE_OBJECTID id) { return g_pZone->RemoveFakeCreature(id); };
	host.SetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { pZone->SetServerBlock(BYTE(moveType), x, y); };
	host.UnSetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { pZone->UnSetServerBlock(BYTE(moveType), x, y); };
	host.CanMove = [](int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return g_pZone->CanMove(BYTE(moveType), x, y); };
	host.CanMoveIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return pZone->CanMove(BYTE(moveType), x, y); };
	host.GetWidthIn = [](MZone* pZone) { return int(pZone->GetWidth()); };
	host.GetHeightIn = [](MZone* pZone) { return int(pZone->GetHeight()); };
	host.GetCreatureIn = [](MZone* pZone, TYPE_OBJECTID id) { return pZone->GetCreature(id); };
	host.DeleteCreature = [](MCreature* pCreature) { delete pCreature; };
	return host;
}();
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.Status = []() -> MStatus* { return g_pPlayer; };
	host.GetID = []() { return g_pPlayer != NULL ? g_pPlayer->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer != NULL && g_pPlayer->HasEffectStatus(status); };
	host.GetItemCheckBuffer = []() { return g_pPlayer->GetItemCheckBuffer(); };
	host.ClearItemCheckBuffer = []() { g_pPlayer->ClearItemCheckBuffer(); };
	return host;
}();
// The view: the height of a creature's sprite, as a test sets it.
int spriteHeight = -1;
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.CreatureSpriteHeight = [](int body, int action, int direction) {
		calls.push_back("height|" + N(body) + "|" + N(action) + "|" + N(direction));
		return spriteHeight;
	};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, int, int, TYPE_OBJECTID,
		TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, int, std::uint32_t, MActionResult* pResult, bool, int, int) {
		calls.push_back("world skill|" + N(actionInfo));
		delete pResult;
	};
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	host.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	host.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
	host.IsSlayer = [](MCreature* pCreature) { return pCreature->IsSlayer(); };
	host.IsVampire = [](MCreature* pCreature) { return pCreature->IsVampire(); };
	host.IsOusters = [](MCreature* pCreature) { return pCreature->IsOusters(); };
	return host;
}();
const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.DropItem = []() { calls.push_back("drop item"); };
	host.UnlockGear = []() { calls.push_back("unlock gear"); };
	host.UnlockItem = []() { calls.push_back("unlock item"); };
	host.UnlockItemTrade = []() { calls.push_back("unlock trade"); };
	return host;
}();
// The item descriptors the UI let go of (by address only: the item goes
// next).
std::vector<const void*> descriptorsRemoved;
const PlayerPresentation::Host presentationHost = []() {
	PlayerPresentation::Host host{};
	host.RemoveDescriptor = [](void* pPtr) { descriptorsRemoved.push_back(pPtr); };
	host.LockItem = []() { calls.push_back("lock item"); };
	return host;
}();
const NoticePresentation::Host noticeHost = []() {
	NoticePresentation::Host host{};
	host.PopupMessage = [](int gameStringID) { calls.push_back("popup|" + N(gameStringID)); };
	return host;
}();
int skillRefreshes = 0;
const BonusSkills::Host bonusHost = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { ++skillRefreshes; };
	return host;
}();
const MEffectHost effectHost = []() {
	MEffectHost host{};
	host.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return host;
}();
const MAttachEffectHost attachHost = []() {
	MAttachEffectHost host{};
	host.LiveCreature = [](TYPE_OBJECTID, MAttachCreaturePosition&) { return true; };
	host.CreaturePosition = [](const MCreature*, MAttachCreaturePosition&) { return true; };
	return host;
}();
std::vector<int> itemsDestroyed;
const MItemHost itemHost = []() {
	MItemHost host{};
	host.NewItem = NewItemOfClass;
	host.DropFrameCount = [](TYPE_FRAMEID) { return 4; };
	host.ItemDestroyed = [](MItem* pItem) noexcept { itemsDestroyed.push_back(int(pItem->GetID())); };
	return host;
}();

// The server a bug report goes to: the chat lines it is sent.
std::vector<std::string> reports;
struct Server : Player
{
	void sendPacket(Packet* pPacket) override { reports.push_back(static_cast<CGSay*>(pPacket)->getMessage()); }
};
Player* reportTarget = nullptr;
const WireHost wireHost = []() {
	WireHost host{};
	host.BugReportTarget = []() { return reportTarget; };
	return host;
}();

const int Slayer = 5, Vampire = 6, Ousters = 7;

struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	MActionInfoTable actions;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MEventQueue events;
	MParty party;
	Generators generators;
	MInventory inventory;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	TempInformation temp;
	UserOption options;
	SystemAvailabilitiesManager systems;
	Server server;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MEventQueue* oldEvents = g_pEventManager;
	MEffectGeneratorTable* oldGenerators = g_pEffectGeneratorTable;
	MParty* oldParty = g_pParty;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MBelt* oldQuickSlot = g_pQuickSlot;
	MOustersArmsBand* oldArmsBand1 = g_pArmsBand1;
	MOustersArmsBand* oldArmsBand2 = g_pArmsBand2;
	TempInformation* oldTemp = g_pTempInformation;
	UserOption* oldOptions = g_pUserOption;
	SystemAvailabilitiesManager* oldSystems = g_pSystemAvailableManager;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	DWORD oldFrame = g_CurrentFrame;
	// The character slot the sex change rewrites.
	bool oldFemale = g_char_slot_ingame.bl_female;
	MAN_INFO oldMan = g_char_slot_ingame.man_info;
	WOMAN_INFO oldWoman = g_char_slot_ingame.woman_info;
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialogHost);
	const PlayerPresentation::Host* oldPresentation = PlayerPresentation::SetHost(&presentationHost);
	const NoticePresentation::Host* oldNotice = NoticePresentation::SetHost(&noticeHost);
	const BonusSkills::Host* oldBonus = BonusSkills::SetHost(&bonusHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);
	// The zone last but the player: it frees what it holds before the
	// tables go.
	std::unique_ptr<MZone> zone;
	MPlayer* me = nullptr;

	explicit Scene(int race = Slayer)
	{
		calls.clear();
		started.clear();
		descriptorsRemoved.clear();
		itemsDestroyed.clear();
		reports.clear();
		skillRefreshes = 0;
		spriteHeight = -1;
		g_CurrentFrame = 1000;
		MItem::SetHost(&itemHost);
		reportTarget = &server;
		Wire::SetHost(&wireHost);
		creatureTypes.Init(1000);
		g_pCreatureTable = &creatureTypes;
		Tribe(Slayer);
		Tribe(Vampire, CREATURETRIBE_VAMPIRE);
		Tribe(Ousters, CREATURETRIBE_OUSTERS);
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
		{
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
			actions.GetMutable(info)->SetSoundID(SOUNDID_NULL);
		}
		actions.SetMinResultActionInfo(MIN_RESULT_ACTIONINFO);
		g_pActionInfoTable = &actions;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pEventManager = &events;
		g_pParty = &party;
		InitGameStringTable();
		generators.Add(new Recorder);
		g_pEffectGeneratorTable = &generators;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		vampireGear.Init();
		g_pVampireGear = &vampireGear;
		g_pQuickSlot = NULL;
		g_pArmsBand1 = g_pArmsBand2 = NULL;
		g_pTempInformation = &temp;
		options.UseForceFeel = FALSE;
		g_pUserOption = &options;
		g_pSystemAvailableManager = &systems;
		zone.reset(new MZone(40, 30));
		g_pZone = zone.get();
		// The player stands at (20, 15); the zone does not hold it.
		me = new MPlayer;
		g_pPlayer = me;
		me->SetZone(g_pZone);
		me->SetCreatureType(TYPE_CREATURETYPE(race));
		me->SetID(77);
		me->SetPosition(20, 15);
		me->SetServerPosition(20, 15);
		calls.clear();
	}
	~Scene()
	{
		delete me;
		g_pPlayer = oldPlayer;
		zone.reset();
		g_pZone = oldZone;
		Wire::SetHost(NULL);
		reportTarget = nullptr;
		MItem::SetHost(NULL);
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		BonusSkills::SetHost(oldBonus);
		NoticePresentation::SetHost(oldNotice);
		PlayerPresentation::SetHost(oldPresentation);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		WorldEvents::SetHost(oldWorld);
		PlayerState::SetHost(oldPlayerHost);
		ZoneState::SetHost(oldZoneHost);
		g_char_slot_ingame.bl_female = oldFemale;
		g_char_slot_ingame.man_info = oldMan;
		g_char_slot_ingame.woman_info = oldWoman;
		g_pSystemAvailableManager = oldSystems;
		g_pUserOption = oldOptions;
		g_pTempInformation = oldTemp;
		g_pArmsBand1 = oldArmsBand1;
		g_pArmsBand2 = oldArmsBand2;
		g_pQuickSlot = oldQuickSlot;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		vampireGear.Release();
		slayerGear.Release();
		inventory.Release();
		g_pParty = oldParty;
		g_pEffectGeneratorTable = oldGenerators;
		g_pEventManager = oldEvents;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
		g_CurrentFrame = oldFrame;
	}
	// A creature type of the tribe, with a count for each action.
	void Tribe(int type, CREATURETRIBE tribe = CREATURETRIBE_SLAYER)
	{
		const int actionCount = tribe == CREATURETRIBE_VAMPIRE ? ACTION_MAX_VAMPIRE : (tribe == CREATURETRIBE_OUSTERS ? ACTION_MAX_OUSTERS : ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, actionCount);
		testfw::MutableRow(creatureTypes, type).MoveTimes = 8;
		testfw::MutableRow(creatureTypes, type).MoveTimesMotor = 8;
		testfw::MutableRow(creatureTypes, type).MoveRatio = 1;
		for (int action = 0; action < actionCount; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
	// An action of three phases by the recording generator, the phase's
	// step its number plus one.
	void Show(TYPE_ACTIONINFO action, int mainNode = MAIN_NODE_NULL)
	{
		MActionInfo* info = actions.GetMutable(action);
		info->Set("", ACTION_MAGIC, EFFECTSPRITETYPE_NULL, 0, 0, FLAG_ACTIONINFO_START_USER);
		info->Init(3);
		for (int phase = 0; phase < 3; ++phase)
		{
			info->GetMutable(phase)->EffectGeneratorID = kGenerator;
			info->GetMutable(phase)->Step = WORD(phase + 1);
			info->GetMutable(phase)->Count = 5;
		}
		info->SetMainNode(mainNode);
	}
	// `types` item types of the class, each a cell big, dropping with frame
	// 9 and making use sound 30 plus its type.
	void Items(ITEM_CLASS itemClass, int types)
	{
		g_pItemTable->InitClass(itemClass, types);
		for (int type = 0; type < types; ++type)
		{
			ITEMTABLE_INFO& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
			row.SetGrid(1, 1);
			row.DropFrameID = 9;
			row.UseSoundID = TYPE_SOUNDID(30 + type);
		}
	}
	MItem* NewItem(ITEM_CLASS itemClass, TYPE_OBJECTID id, int type = 0, int number = 1)
	{
		MItem* pItem = MItem::NewItem(itemClass);
		pItem->SetItemType(TYPE_ITEMTYPE(type));
		pItem->SetID(id);
		pItem->SetNumber(TYPE_ITEM_NUMBER(number));
		return pItem;
	}
	// An item of the class in the inventory, in the player's hand to use.
	MItem* Hold(ITEM_CLASS itemClass, TYPE_OBJECTID id, int type = 0, int number = 1,
		MPlayer::ITEM_CHECK_BUFFER status = MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)
	{
		MItem* pItem = NewItem(itemClass, id, type, number);
		CHECK(inventory.AddItem(pItem));
		me->SetItemCheckBuffer(pItem, status);
		calls.clear();
		return pItem;
	}
	bool Destroyed(int id) const
	{
		for (int destroyed : itemsDestroyed) if (destroyed == id) return true;
		return false;
	}
	bool DescriptorRemoved(const void* pItem) const
	{
		return descriptorsRemoved.size() == 1u && descriptorsRemoved[0] == pItem;
	}
};

int PixelX(int x) { return WorldTileGeometry::TileToPixelX(x); }
int PixelY(int y) { return WorldTileGeometry::TileToPixelY(y); }

// A packet putting an item of the class and type at the cell.
std::unique_ptr<GCDropItemToZone> Drop(TYPE_OBJECTID id, int itemClass, int type, int x, int y)
{
	std::unique_ptr<GCDropItemToZone> pPacket(new GCDropItemToZone);
	pPacket->setObjectID(id);
	pPacket->setItemClass(BYTE(itemClass));
	pPacket->setItemType(ItemType_t(type));
	pPacket->setX(Coord_t(x));
	pPacket->setY(Coord_t(y));
	pPacket->setDurability(0);
	pPacket->setSilver(0);
	pPacket->setGrade(0);
	pPacket->setEnchantLevel(0);
	pPacket->setItemNum(1);
	return pPacket;
}

void AddSub(GCAddItemToZone& packet, TYPE_OBJECTID id, int itemClass, int type, int number, int slot)
{
	SubItemInfo* pSub = new SubItemInfo;
	pSub->setObjectID(id);
	pSub->setItemClass(BYTE(itemClass));
	pSub->setItemType(ItemType_t(type));
	pSub->setItemNum(ItemNum_t(number));
	pSub->setSlotID(SlotID_t(slot));
	packet.addListElement(pSub);
	packet.setListNum(BYTE(packet.getListNum() + 1));
}

} // namespace

//----------------------------------------------------------------------
// AddItemToZone: the item in the player's hand
//----------------------------------------------------------------------
TEST(PacketZoneItems, TheItemInHandIsLaidWhereThePacketSays)
{
	Scene scene;
	scene.Items(ITEM_CLASS_HOLYWATER, 2);
	MItem* pItem = scene.NewItem(ITEM_CLASS_HOLYWATER, 300, 1);
	scene.me->SetItemCheckBuffer(pItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	// The packet's class, type and the rest are not read: the hand's item
	// goes down as it is.
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(300, ITEM_CLASS_POTION, 0, 7, 8);
	CHECK(AddItemToZone(pPacket.get(), TRUE) == pItem);
	CHECK(scene.zone->GetItem(300) == pItem);
	CHECK_EQ(300, int(scene.zone->GetItemID(7, 8)));
	CHECK_EQ(7, int(pItem->GetX()));
	CHECK_EQ(8, int(pItem->GetY()));
	CHECK_EQ(int(ITEM_CLASS_HOLYWATER), int(pItem->GetItemClass()));
	CHECK(pItem->IsDropping());
	CHECK(scene.me->GetItemCheckBuffer() == NULL);
	CHECK(scene.me->IsItemCheckBufferNULL());
	// The hand let go of it; a slayer's holy water changes the skill icons.
	CHECK(Same({"unlock gear", "unlock item", "unlock trade", "drop item"}));
	CHECK_EQ(1, skillRefreshes);
	CHECK(itemsDestroyed.empty());
}

TEST(PacketZoneItems, TheItemInHandLaidAtRestIsNotDropping)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	MItem* pItem = scene.NewItem(ITEM_CLASS_POTION, 300);
	scene.me->SetItemCheckBuffer(pItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(300, ITEM_CLASS_POTION, 0, 7, 8);
	CHECK(AddItemToZone(pPacket.get(), FALSE) == pItem);
	CHECK(!pItem->IsDropping());
	// A potion changes no skill icon.
	CHECK_EQ(0, skillRefreshes);
}

TEST(PacketZoneItems, TheItemInHandTheZoneRefusesIsFreed)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	MItem* pItem = scene.NewItem(ITEM_CLASS_POTION, 300);
	scene.me->SetItemCheckBuffer(pItem, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(300, ITEM_CLASS_POTION, 0, 40, 8);
	CHECK(AddItemToZone(pPacket.get(), TRUE) == NULL);
	CHECK(scene.zone->GetItem(300) == NULL);
	CHECK(scene.Destroyed(300));
	CHECK(scene.me->GetItemCheckBuffer() == NULL);
	CHECK(Same({"unlock gear", "unlock item", "unlock trade", "drop item"}));
}

TEST(PacketZoneItems, AnEmptyHandDropsNothing)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	scene.me->SetItemCheckBuffer(NULL, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(300, ITEM_CLASS_POTION, 0, 7, 8);
	CHECK(AddItemToZone(pPacket.get(), TRUE) == NULL);
	CHECK(scene.zone->GetItem(300) == NULL);
	CHECK(calls.empty());
}

TEST(PacketZoneItems, AnotherItemThanTheOneInHandIsMadeNewAndTheHandKept)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 2);
	MItem* pHeld = scene.NewItem(ITEM_CLASS_POTION, 300);
	scene.me->SetItemCheckBuffer(pHeld, MPlayer::ITEM_CHECK_BUFFER_DROP_TO_ZONE);
	calls.clear();
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_POTION, 1, 7, 8);
	MItem* pItem = AddItemToZone(pPacket.get(), TRUE);
	CHECK(pItem != NULL && pItem != pHeld);
	CHECK(scene.zone->GetItem(301) == pItem);
	CHECK(scene.me->GetItemCheckBuffer() == pHeld);
	CHECK(calls.empty());
	// Held items in any other state are not dropped either.
	scene.me->SetItemCheckBuffer(pHeld, MPlayer::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY);
	std::unique_ptr<GCDropItemToZone> same = Drop(300, ITEM_CLASS_POTION, 1, 9, 8);
	MItem* pNew = AddItemToZone(same.get(), TRUE);
	CHECK(pNew != NULL && pNew != pHeld);
	CHECK(scene.me->GetItemCheckBuffer() == pHeld);
	delete pHeld;
}

//----------------------------------------------------------------------
// AddItemToZone: a new item from the packet
//----------------------------------------------------------------------
TEST(PacketZoneItems, ANewItemTakesThePacketsDetails)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 3);
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_POTION, 2, 7, 8);
	pPacket->setOptionType(std::list<OptionType_t>({ 3, 5 }));
	pPacket->setDurability(250);
	pPacket->setSilver(12);
	pPacket->setGrade(4);
	pPacket->setEnchantLevel(6);
	pPacket->setItemNum(9);
	MItem* pItem = AddItemToZone(pPacket.get(), TRUE);
	CHECK(pItem != NULL);
	if (pItem == NULL) return;
	CHECK(scene.zone->GetItem(301) == pItem);
	CHECK_EQ(301, int(scene.zone->GetItemID(7, 8)));
	CHECK_EQ(int(ITEM_CLASS_POTION), int(pItem->GetItemClass()));
	CHECK_EQ(2, int(pItem->GetItemType()));
	CHECK(pItem->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({ 3, 5 }));
	CHECK_EQ(7, int(pItem->GetX()));
	CHECK_EQ(8, int(pItem->GetY()));
	CHECK_EQ(250, int(pItem->GetCurrentDurability()));
	CHECK_EQ(12, pItem->GetSilver());
	CHECK_EQ(4, pItem->GetGrade());
	CHECK_EQ(6, int(pItem->GetEnchantLevel()));
	CHECK_EQ(9, int(pItem->GetNumber()));
	CHECK(pItem->IsDropping());
	CHECK(pItem->GetName() == NULL || std::string(pItem->GetName()).find('(') == std::string::npos);
	// Not the hand's: nothing let go, no icon refreshed.
	CHECK(calls.empty());
	CHECK_EQ(0, skillRefreshes);
	CHECK(reports.empty());
}

TEST(PacketZoneItems, MoneyIsNamedWithItsSumOfItsTwoHalves)
{
	Scene scene;
	scene.Items(ITEM_CLASS_MONEY, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MONEY, 0).HName = "Gold";
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_MONEY, 0, 7, 8);
	// The durability is the low word, the silver the high.
	pPacket->setDurability(500);
	pPacket->setSilver(2);
	pPacket->setItemNum(1);
	MItem* pItem = AddItemToZone(pPacket.get(), FALSE);
	CHECK(pItem != NULL);
	if (pItem == NULL) return;
	CHECK(std::string(pItem->GetName()) == "Gold(131572)");
}

TEST(PacketZoneItems, AGunIsLoadedWithTheFirstMagazineThatFitsIt)
{
	Scene scene;
	scene.Items(ITEM_CLASS_SR, 1);
	scene.Items(ITEM_CLASS_MAGAZINE, 12);
	// Magazine types 0 and 1 are an AR's, 2 and 3 an SR's.
	for (int type = 0; type < 12; ++type)
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, type).Value1 = (type % 4) < 2 ? ITEM_CLASS_AR : ITEM_CLASS_SR;
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_SR, 0, 7, 8);
	pPacket->setItemNum(17);
	MItem* pItem = AddItemToZone(pPacket.get(), FALSE);
	CHECK(pItem != NULL && pItem->IsGunItem());
	if (pItem == NULL || !pItem->IsGunItem()) return;
	const MMagazine* pMagazine = static_cast<MGunItem*>(pItem)->GetMagazine();
	CHECK(pMagazine != NULL);
	if (pMagazine == NULL) return;
	CHECK_EQ(2, int(pMagazine->GetItemType()));
	CHECK_EQ(17, int(pMagazine->GetNumber()));
	CHECK_EQ(0, int(pMagazine->GetID()));
	CHECK(pMagazine->IsEmptyItemOptionList());
	// The gun keeps one; silver bullets are the magazine eight types on.
	CHECK_EQ(1, int(pItem->GetNumber()));
	std::unique_ptr<GCDropItemToZone> silver = Drop(302, ITEM_CLASS_SR, 0, 9, 8);
	silver->setSilver(1);
	MItem* pSilver = AddItemToZone(silver.get(), FALSE);
	CHECK(pSilver != NULL);
	if (pSilver != NULL) CHECK_EQ(10, int(static_cast<MGunItem*>(pSilver)->GetMagazine()->GetItemType()));
}

TEST(PacketZoneItems, AChargedItemKeepsItsChargesWhateverThePacketCounts)
{
	Scene scene;
	scene.Items(ITEM_CLASS_SLAYER_PORTAL_ITEM, 1);
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_SLAYER_PORTAL_ITEM, 0, 7, 8);
	// A portal's charges are its enchant level, not the packet's count.
	pPacket->setEnchantLevel(3);
	pPacket->setItemNum(5);
	MItem* pItem = AddItemToZone(pPacket.get(), FALSE);
	CHECK(pItem != NULL && pItem->IsChargeItem());
	if (pItem != NULL) CHECK_EQ(3, int(pItem->GetNumber()));
}

TEST(PacketZoneItems, AnUnknownClassOrTypeIsReportedAndNotMade)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 2);
	std::unique_ptr<GCDropItemToZone> pastType = Drop(301, ITEM_CLASS_POTION, 2, 7, 8);
	CHECK(AddItemToZone(pastType.get(), FALSE) == NULL);
	std::unique_ptr<GCDropItemToZone> pastClass = Drop(302, int(g_pItemTable->GetSize()), 0, 7, 8);
	CHECK(AddItemToZone(pastClass.get(), FALSE) == NULL);
	CHECK(scene.zone->GetItemID(7, 8) == OBJECTID_NULL);
	CHECK(reports.size() == 2u);
	if (reports.size() == 2u)
	{
		// The class and type close each report.
		const std::string first = N(ITEM_CLASS_POTION) + ",2";
		const std::string second = N(g_pItemTable->GetSize()) + ",0";
		CHECK(reports[0].size() >= first.size() && reports[0].compare(reports[0].size() - first.size(), first.size(), first) == 0);
		CHECK(reports[1].size() >= second.size() && reports[1].compare(reports[1].size() - second.size(), second.size(), second) == 0);
	}
	// One within the table is made.
	std::unique_ptr<GCDropItemToZone> last = Drop(303, ITEM_CLASS_POTION, 1, 7, 8);
	CHECK(AddItemToZone(last.get(), FALSE) != NULL);
	CHECK_EQ(2u, reports.size());
}

TEST(PacketZoneItems, ABeltComesWithItsPocketsFilled)
{
	Scene scene;
	scene.Items(ITEM_CLASS_BELT, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BELT, 0).Value3 = 3;
	scene.Items(ITEM_CLASS_POTION, 2);
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_BELT, 0, 7, 8);
	AddSub(*pPacket, 401, ITEM_CLASS_POTION, 1, 4, 2);
	// An unknown type is reported and skipped; the rest still go in. (The
	// skipped SubItemInfo is not freed: PacketFunction.cpp:1111 and 1160
	// `continue` past the delete. A leak, which no check here sees.)
	AddSub(*pPacket, 402, ITEM_CLASS_POTION, 2, 1, 1);
	AddSub(*pPacket, 403, ITEM_CLASS_POTION, 0, 7, 0);
	MItem* pItem = AddItemToZone(pPacket.get(), FALSE);
	CHECK(pItem != NULL && pItem->GetItemClass() == ITEM_CLASS_BELT);
	if (pItem == NULL || pItem->GetItemClass() != ITEM_CLASS_BELT) return;
	const MBelt* pBelt = static_cast<const MBelt*>(pItem);
	const MItem* pSecond = pBelt->GetItem(2);
	const MItem* pFirst = pBelt->GetItem(0);
	CHECK(pSecond != NULL && pFirst != NULL);
	CHECK(pBelt->GetItem(1) == NULL);
	if (pSecond == NULL || pFirst == NULL) return;
	CHECK_EQ(401, int(pSecond->GetID()));
	CHECK_EQ(1, int(pSecond->GetItemType()));
	CHECK_EQ(4, int(pSecond->GetNumber()));
	CHECK_EQ(403, int(pFirst->GetID()));
	CHECK_EQ(7, int(pFirst->GetNumber()));
	CHECK_EQ(1u, reports.size());
}

TEST(PacketZoneItems, AnArmsBandComesWithItsPocketsFilled)
{
	Scene scene;
	scene.Items(ITEM_CLASS_OUSTERS_ARMSBAND, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).Value3 = 2;
	scene.Items(ITEM_CLASS_PUPA, 1);
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_OUSTERS_ARMSBAND, 0, 7, 8);
	AddSub(*pPacket, 401, ITEM_CLASS_PUPA, 0, 3, 1);
	AddSub(*pPacket, 402, int(g_pItemTable->GetSize()), 0, 1, 0);
	MItem* pItem = AddItemToZone(pPacket.get(), FALSE);
	CHECK(pItem != NULL && pItem->GetItemClass() == ITEM_CLASS_OUSTERS_ARMSBAND);
	if (pItem == NULL || pItem->GetItemClass() != ITEM_CLASS_OUSTERS_ARMSBAND) return;
	const MOustersArmsBand* pBand = static_cast<const MOustersArmsBand*>(pItem);
	CHECK(pBand->GetItem(0) == NULL);
	const MItem* pPupa = pBand->GetItem(1);
	CHECK(pPupa != NULL);
	if (pPupa != NULL)
	{
		CHECK_EQ(401, int(pPupa->GetID()));
		CHECK_EQ(3, int(pPupa->GetNumber()));
	}
	CHECK_EQ(1u, reports.size());
}

TEST(PacketZoneItems, AnyOtherItemsSubItemsAreLeftInThePacket)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<GCDropItemToZone> pPacket = Drop(301, ITEM_CLASS_POTION, 0, 7, 8);
	AddSub(*pPacket, 401, ITEM_CLASS_POTION, 0, 3, 0);
	CHECK(AddItemToZone(pPacket.get(), FALSE) != NULL);
	CHECK_EQ(1, int(pPacket->getListNum()));
	CHECK(pPacket->popFrontListElement() != NULL);
}

//----------------------------------------------------------------------
// AddItemToZone: a new item the zone refuses
//----------------------------------------------------------------------
TEST(PacketZoneItems, AnotherItemOnTheCellMakesWayButTheNewOneIsNotReturned)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<GCDropItemToZone> first = Drop(301, ITEM_CLASS_POTION, 0, 7, 8);
	CHECK(AddItemToZone(first.get(), FALSE) != NULL);
	std::unique_ptr<GCDropItemToZone> second = Drop(302, ITEM_CLASS_POTION, 0, 7, 8);
	// Recorded defect (PacketFunction.cpp:1219-1278): the new item is in
	// the zone, yet NULL comes back, so GCAddInstalledMineToZone's handler
	// never marks a mine laid this way installed.
	CHECK(AddItemToZone(second.get(), FALSE) == NULL);
	CHECK(scene.zone->GetItem(301) == NULL);
	CHECK(scene.Destroyed(301));
	CHECK(scene.zone->GetItem(302) != NULL);
	CHECK_EQ(302, int(scene.zone->GetItemID(7, 8)));
}

TEST(PacketZoneItems, TheSameItemAgainOnItsCellIsDropped)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<GCDropItemToZone> first = Drop(301, ITEM_CLASS_POTION, 0, 7, 8);
	MItem* pOld = AddItemToZone(first.get(), FALSE);
	std::unique_ptr<GCDropItemToZone> again = Drop(301, ITEM_CLASS_POTION, 0, 7, 8);
	CHECK(AddItemToZone(again.get(), FALSE) == NULL);
	CHECK(scene.zone->GetItem(301) == pOld);
	CHECK_EQ(1u, itemsDestroyed.size());
}

TEST(PacketZoneItems, TheSameItemOnAnotherCellMovesThere)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<GCDropItemToZone> first = Drop(301, ITEM_CLASS_POTION, 0, 7, 8);
	MItem* pOld = AddItemToZone(first.get(), FALSE);
	std::unique_ptr<GCDropItemToZone> moved = Drop(301, ITEM_CLASS_POTION, 0, 9, 8);
	// Recorded defect, as above (PacketFunction.cpp:1256-1278): laid, yet
	// NULL.
	CHECK(AddItemToZone(moved.get(), FALSE) == NULL);
	MItem* pNow = scene.zone->GetItem(301);
	CHECK(pNow != NULL && pNow != pOld);
	CHECK_EQ(301, int(scene.zone->GetItemID(9, 8)));
	CHECK(scene.zone->GetItemID(7, 8) == OBJECTID_NULL);
	CHECK_EQ(1u, itemsDestroyed.size());
}

TEST(PacketZoneItems, AnItemThatCannotBeLaidAfterAllIsFreed)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	// Off the zone, unknown elsewhere: freed.
	std::unique_ptr<GCDropItemToZone> off = Drop(301, ITEM_CLASS_POTION, 0, 40, 8);
	CHECK(AddItemToZone(off.get(), FALSE) == NULL);
	CHECK(scene.zone->GetItem(301) == NULL);
	CHECK(scene.Destroyed(301));
	// Off the zone, known elsewhere: the old one goes, and the new one
	// still finds no cell.
	std::unique_ptr<GCDropItemToZone> first = Drop(302, ITEM_CLASS_POTION, 0, 7, 8);
	CHECK(AddItemToZone(first.get(), FALSE) != NULL);
	std::unique_ptr<GCDropItemToZone> offAgain = Drop(302, ITEM_CLASS_POTION, 0, 7, 30);
	CHECK(AddItemToZone(offAgain.get(), FALSE) == NULL);
	CHECK(scene.zone->GetItem(302) == NULL);
	CHECK_EQ(3u, itemsDestroyed.size());
	// Another on the cell and itself elsewhere: the one on the cell goes,
	// the old one stays and the new one is freed.
	std::unique_ptr<GCDropItemToZone> a = Drop(303, ITEM_CLASS_POTION, 0, 7, 8);
	std::unique_ptr<GCDropItemToZone> b = Drop(304, ITEM_CLASS_POTION, 0, 9, 8);
	MItem* pB = AddItemToZone(b.get(), FALSE);
	CHECK(AddItemToZone(a.get(), FALSE) != NULL);
	std::unique_ptr<GCDropItemToZone> bOnA = Drop(304, ITEM_CLASS_POTION, 0, 7, 8);
	CHECK(AddItemToZone(bOnA.get(), FALSE) == NULL);
	CHECK(scene.zone->GetItem(303) == NULL);
	CHECK(scene.zone->GetItem(304) == pB);
	CHECK(scene.zone->GetItemID(7, 8) == OBJECTID_NULL);
	CHECK_EQ(5u, itemsDestroyed.size());
}

//----------------------------------------------------------------------
// UseSkillCardOK
//----------------------------------------------------------------------
TEST(PacketZoneItems, ASkillCardOfTheRacesTypeIsUsedUp)
{
	const int races[3] = { Slayer, Vampire, Ousters };
	const int cards[3] = { 5, 6, 7 };
	for (int i = 0; i < 3; ++i)
	{
		Scene scene(races[i]);
		scene.Items(ITEM_CLASS_MOON_CARD, 8);
		// Another race's card is not the one.
		MItem* pOther = scene.NewItem(ITEM_CLASS_MOON_CARD, 300, cards[(i + 1) % 3], 2);
		CHECK(scene.inventory.AddItem(pOther));
		CHECK(!UseSkillCardOK(0));
		CHECK_EQ(2, int(pOther->GetNumber()));
		MItem* pCard = scene.NewItem(ITEM_CLASS_MOON_CARD, 301, cards[i], 2);
		CHECK(scene.inventory.AddItem(pCard));
		// A timed card stays as it is.
		CHECK(UseSkillCardOK(1));
		CHECK_EQ(2, int(pCard->GetNumber()));
		CHECK(UseSkillCardOK(0));
		CHECK_EQ(1, int(pCard->GetNumber()));
		CHECK(descriptorsRemoved.empty());
		// The last one goes from the inventory, the UI and memory.
		CHECK(UseSkillCardOK(2));
		CHECK(scene.inventory.GetItem(301) == NULL);
		CHECK(scene.DescriptorRemoved(pCard));
		CHECK(scene.Destroyed(301));
		CHECK(!UseSkillCardOK(0));
		CHECK_EQ(2, int(pOther->GetNumber()));
	}
}

//----------------------------------------------------------------------
// UseItemOK
//----------------------------------------------------------------------
TEST(PacketZoneItems, UsingNothingFails)
{
	Scene scene;
	CHECK(!UseItemOK());
	CHECK(calls.empty());
}

TEST(PacketZoneItems, AnItemInHandForAnotherReasonIsNotUsed)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 1);
	MItem* pItem = scene.Hold(ITEM_CLASS_POTION, 300, 0, 3, MPlayer::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY);
	CHECK(!UseItemOK());
	// Its sound plays all the same; the hand keeps it.
	CHECK(Same({"sound|30"}));
	CHECK(scene.me->GetItemCheckBuffer() == pItem);
	CHECK_EQ(3, int(pItem->GetNumber()));
}

TEST(PacketZoneItems, APileUsedFromTheInventoryLosesOneAndGoesWithTheLast)
{
	Scene scene;
	scene.Items(ITEM_CLASS_POTION, 3);
	MItem* pItem = scene.Hold(ITEM_CLASS_POTION, 300, 2, 2);
	CHECK(UseItemOK());
	CHECK(Same({"sound|32", "unlock gear", "unlock item", "unlock trade"}));
	CHECK(scene.me->GetItemCheckBuffer() == NULL);
	CHECK_EQ(1, int(pItem->GetNumber()));
	CHECK(scene.inventory.GetItem(300) == pItem);
	CHECK(descriptorsRemoved.empty());
	scene.me->SetItemCheckBuffer(pItem, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	CHECK(UseItemOK());
	CHECK(scene.inventory.GetItem(300) == NULL);
	CHECK(scene.DescriptorRemoved(pItem));
	CHECK(scene.Destroyed(300));
}

TEST(PacketZoneItems, AUseAtNoneLeavesTheCountAtNone)
{
	// The count is unsigned: one less than none is none, not 4294967295.
	{
		// A spent portal used again stays, at none.
		Scene scene;
		scene.Items(ITEM_CLASS_SLAYER_PORTAL_ITEM, 1);
		MItem* pPortal = scene.Hold(ITEM_CLASS_SLAYER_PORTAL_ITEM, 300, 0, 0);
		CHECK(UseItemOK());
		CHECK_EQ(0, int(pPortal->GetNumber()));
		CHECK(scene.inventory.GetItem(300) == pPortal);
	}
	{
		// A pile at none goes, as the last of a pile does.
		Scene scene;
		scene.Items(ITEM_CLASS_POTION, 1);
		MItem* pPile = scene.Hold(ITEM_CLASS_POTION, 300, 0, 0);
		CHECK(UseItemOK());
		CHECK(scene.inventory.GetItem(300) == NULL);
		CHECK(scene.DescriptorRemoved(pPile));
		CHECK(scene.Destroyed(300));
	}
	{
		// A skill card at none goes, as the last card does.
		Scene scene(Slayer);
		scene.Items(ITEM_CLASS_MOON_CARD, 8);
		MItem* pCard = scene.NewItem(ITEM_CLASS_MOON_CARD, 301, 5, 0);
		CHECK(scene.inventory.AddItem(pCard));
		CHECK(UseSkillCardOK(0));
		CHECK(scene.inventory.GetItem(301) == NULL);
		CHECK(scene.DescriptorRemoved(pCard));
		CHECK(scene.Destroyed(301));
	}
}

TEST(PacketZoneItems, ChargedPortalsSummonsAndPetsStayWhenSpent)
{
	Scene scene;
	scene.Items(ITEM_CLASS_SLAYER_PORTAL_ITEM, 1);
	scene.Items(ITEM_CLASS_OUSTERS_SUMMON_ITEM, 1);
	scene.Items(ITEM_CLASS_PET_ITEM, 1);
	// A slayer's portal loses a charge and stays at none.
	MItem* pPortal = scene.Hold(ITEM_CLASS_SLAYER_PORTAL_ITEM, 300, 0, 1);
	CHECK(UseItemOK());
	CHECK_EQ(0, int(pPortal->GetNumber()));
	CHECK(scene.inventory.GetItem(300) == pPortal);
	MItem* pSummon = scene.Hold(ITEM_CLASS_OUSTERS_SUMMON_ITEM, 301, 0, 1);
	CHECK(UseItemOK());
	CHECK(scene.inventory.GetItem(301) == pSummon);
	MItem* pPet = scene.Hold(ITEM_CLASS_PET_ITEM, 302, 0, 1);
	CHECK(UseItemOK());
	CHECK(scene.inventory.GetItem(302) == pPet);
	CHECK(descriptorsRemoved.empty());
	CHECK(itemsDestroyed.empty());
}

namespace {
// The player's hair colour, coat colour, body colour and the two the user
// information keeps.
std::string Looks(Scene& scene)
{
	return N(scene.me->GetAddonInfo(ADDON_HAIR).ColorSet1) + "|" + N(scene.me->GetAddonInfo(ADDON_COAT).ColorSet1)
		+ "|" + N(scene.me->GetBodyColor1()) + "|" + N(g_pUserInformation->HairColor) + "|" + N(g_pUserInformation->SkinColor);
}
// The player's dye potion of the type used up.
void Dye(Scene& scene, int type, TYPE_OBJECTID id = 300)
{
	scene.Hold(ITEM_CLASS_DYE_POTION, id, type, 1);
	CHECK(UseItemOK());
	CHECK(scene.inventory.GetItem(id) == NULL);
	CHECK(scene.Destroyed(int(id)));
}
}

TEST(PacketZoneItems, AHairDyeColoursASlayersHairOrAdvancedCoatAndAnOustersBody)
{
	{
		Scene scene(Slayer);
		scene.Items(ITEM_CLASS_DYE_POTION, 64);
		scene.me->SetAddonHair(ADDONID_HAIR2_MALE, 1);
		g_pUserInformation->HairColor = 1;
		Dye(scene, 0);
		// The hair as it was, in the dye's colour (400).
		CHECK_EQ(int(ADDONID_HAIR2_MALE), int(scene.me->GetHairFrameID()));
		CHECK(Looks(scene) == "400|" + N(scene.me->GetAddonInfo(ADDON_COAT).ColorSet1) + "|" + N(scene.me->GetBodyColor1()) + "|400|" + N(g_pUserInformation->SkinColor));
		Dye(scene, 23, 301);
		CHECK_EQ(57, int(scene.me->GetAddonInfo(ADDON_HAIR).ColorSet1));
		CHECK_EQ(57, int(g_pUserInformation->HairColor));
		// An advanced slayer's dye goes on the coat.
		scene.me->MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		Dye(scene, 1, 302);
		CHECK_EQ(359, int(scene.me->GetAddonInfo(ADDON_COAT).ColorSet1));
		CHECK_EQ(57, int(scene.me->GetAddonInfo(ADDON_HAIR).ColorSet1));
		CHECK_EQ(359, int(g_pUserInformation->HairColor));
	}
	{
		Scene scene(Ousters);
		scene.Items(ITEM_CLASS_DYE_POTION, 64);
		Dye(scene, 2);
		CHECK_EQ(312, int(scene.me->GetBodyColor1()));
		CHECK_EQ(312, int(g_pUserInformation->HairColor));
	}
	{
		// A vampire's hair takes no dye; the potion goes all the same.
		Scene scene(Vampire);
		scene.Items(ITEM_CLASS_DYE_POTION, 64);
		g_pUserInformation->HairColor = 1;
		const WORD body = scene.me->GetBodyColor1();
		Dye(scene, 3);
		CHECK_EQ(1, int(g_pUserInformation->HairColor));
		CHECK_EQ(int(body), int(scene.me->GetBodyColor1()));
	}
}

TEST(PacketZoneItems, ASkinDyeColoursASlayersCoatAndAnyOtherBody)
{
	{
		Scene scene(Slayer);
		scene.Items(ITEM_CLASS_DYE_POTION, 64);
		g_pUserInformation->HairColor = 1;
		Dye(scene, 24);
		CHECK_EQ(165, int(scene.me->GetAddonInfo(ADDON_COAT).ColorSet1));
		CHECK_EQ(165, int(g_pUserInformation->SkinColor));
		CHECK_EQ(1, int(g_pUserInformation->HairColor));
		Dye(scene, 47, 301);
		CHECK_EQ(494, int(scene.me->GetAddonInfo(ADDON_COAT).ColorSet1));
	}
	for (int race : { Vampire, Ousters })
	{
		Scene scene(race);
		scene.Items(ITEM_CLASS_DYE_POTION, 64);
		Dye(scene, 25);
		CHECK_EQ(381, int(scene.me->GetBodyColor1()));
		CHECK_EQ(381, int(g_pUserInformation->SkinColor));
	}
}

TEST(PacketZoneItems, ASexChangeTurnsAVampiresBodyAndFlag)
{
	Scene scene(Vampire);
	scene.Tribe(CREATURETYPE_VAMPIRE_MALE1, CREATURETRIBE_VAMPIRE);
	scene.Tribe(CREATURETYPE_VAMPIRE_FEMALE1, CREATURETRIBE_VAMPIRE);
	testfw::MutableRow(scene.creatureTypes, CREATURETYPE_VAMPIRE_MALE1).bMale = true;
	testfw::MutableRow(scene.creatureTypes, CREATURETYPE_VAMPIRE_FEMALE1).bMale = false;
	scene.me->SetCreatureType(CREATURETYPE_VAMPIRE_MALE1);
	CHECK(scene.me->IsMale());
	scene.Items(ITEM_CLASS_DYE_POTION, 64);
	g_char_slot_ingame.bl_female = false;
	g_char_slot_ingame.man_info.hair = CHAR_MAN(3);
	g_char_slot_ingame.man_info.face = CHAR_MAN(2);
	Dye(scene, 48);
	CHECK_EQ(int(CREATURETYPE_VAMPIRE_FEMALE1), int(scene.me->GetCreatureType()));
	CHECK(scene.me->IsFemale());
	CHECK(g_char_slot_ingame.bl_female);
	CHECK_EQ(3, int(g_char_slot_ingame.woman_info.hair));
	CHECK_EQ(2, int(g_char_slot_ingame.woman_info.face));
}

TEST(PacketZoneItems, ASexChangeKeepsASlayersHairStyleAndColour)
{
	// Each of the three styles, and a hair that is none of them (the
	// first style), for either sex, bare-headed and under a helmet (which
	// takes short hair off: MCreatureWear::SetAddonItem's ADDON_HELM).
	for (const bool helmet : {false, true})
	for (const bool male : {true, false})
	for (int style = 0; style < 4; ++style)
	{
		Scene scene(Slayer);
		scene.Tribe(CREATURETYPE_SLAYER_MALE);
		scene.Tribe(CREATURETYPE_SLAYER_FEMALE);
		testfw::MutableRow(scene.creatureTypes, CREATURETYPE_SLAYER_MALE).bMale = true;
		testfw::MutableRow(scene.creatureTypes, CREATURETYPE_SLAYER_FEMALE).bMale = false;
		scene.me->SetCreatureType(male ? CREATURETYPE_SLAYER_MALE : CREATURETYPE_SLAYER_FEMALE);
		CHECK_EQ(male, scene.me->IsMale());
		const int* oldHair = male ? g_PacketHairMaleID : g_PacketHairFemaleID;
		const int* newHair = male ? g_PacketHairFemaleID : g_PacketHairMaleID;
		scene.me->SetAddonHair(TYPE_FRAMEID(style < 3 ? oldHair[style] : 999), 77);
		if (helmet)
			scene.me->RemoveAddon(ADDON_HAIR);
		scene.Items(ITEM_CLASS_DYE_POTION, 64);
		g_char_slot_ingame.bl_female = !male;
		Dye(scene, 48);
		CHECK_EQ(int(male ? CREATURETYPE_SLAYER_FEMALE : CREATURETYPE_SLAYER_MALE), int(scene.me->GetCreatureType()));
		CHECK_EQ(!male, scene.me->IsMale());
		const MCreatureWear::ADDON_INFO& hair = scene.me->GetAddonInfo(ADDON_HAIR);
		CHECK(hair.bAddon);
		CHECK_EQ(newHair[style < 3 ? style : 0], int(hair.FrameID));
		CHECK_EQ(77, int(hair.ColorSet1));
		CHECK_EQ(male, g_char_slot_ingame.bl_female);
	}
}

TEST(PacketZoneItems, AnOusterTakesNoSexChange)
{
	Scene scene(Ousters);
	scene.Items(ITEM_CLASS_DYE_POTION, 64);
	g_char_slot_ingame.bl_female = false;
	const bool male = scene.me->IsMale();
	Dye(scene, 48);
	CHECK_EQ(int(Ousters), int(scene.me->GetCreatureType()));
	CHECK_EQ(male, scene.me->IsMale());
	CHECK(!g_char_slot_ingame.bl_female);
}

TEST(PacketZoneItems, ABatDyeColoursTheBatAndSaysSo)
{
	Scene scene(Vampire);
	scene.Items(ITEM_CLASS_DYE_POTION, 64);
	const int colours[9] = { 23, 2, 33, 82, 380, 50, 151, 395, 0xFFFF };
	for (int type = 49; type <= 57; ++type)
	{
		calls.clear();
		Dye(scene, type, TYPE_OBJECTID(300 + type));
		CHECK_EQ(colours[type - 49], int(scene.me->GetBatColor()));
		CHECK(Same({"sound|" + N(30 + type), "unlock gear", "unlock item", "unlock trade", "popup|" + N(STRING_MESSAGE_SUCCESS_CHANGED_BAT_COLOR)}));
	}
	// Past the bat dyes the colour stays.
	Dye(scene, 58, 400);
	CHECK_EQ(0xFFFF, int(scene.me->GetBatColor()));
}

TEST(PacketZoneItems, AMasterEffectDyeChangesAGrandMastersEffect)
{
	Scene scene(Slayer);
	scene.Items(ITEM_CLASS_DYE_POTION, 64);
	// Every system open, the grand masters' effect among them.
	scene.systems.SetFlag(0xFFFFFFFF);
	scene.me->AddEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER, 0xffff);
	CHECK(scene.me->HasEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER));
	// 58 to 62 are the effects 1 to 5, but 61 is the plain one, 0.
	const int effects[5] = { 1, 2, 3, 0, 5 };
	for (int type = 58; type <= 62; ++type)
	{
		calls.clear();
		Dye(scene, type, TYPE_OBJECTID(300 + type));
		CHECK_EQ(effects[type - 58], int(scene.me->GetMasterEffectType()));
		CHECK(Same({"sound|" + N(30 + type), "unlock gear", "unlock item", "unlock trade", "popup|" + N(STRING_MESSAGE_SUCCESS_CHANGE)}));
	}
	// Past them nothing changes and nothing is said.
	calls.clear();
	Dye(scene, 63, 400);
	CHECK_EQ(5, int(scene.me->GetMasterEffectType()));
	CHECK(Same({"sound|93", "unlock gear", "unlock item", "unlock trade"}));
}

TEST(PacketZoneItems, AQuickSlotPileLosesOneThenLeavesTheBelt)
{
	Scene scene(Slayer);
	scene.Items(ITEM_CLASS_BELT, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BELT, 0).Value3 = 3;
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<MItem> belt(scene.NewItem(ITEM_CLASS_BELT, 200));
	MBelt* pBelt = static_cast<MBelt*>(belt.get());
	g_pQuickSlot = pBelt;
	MItem* pPotion = scene.NewItem(ITEM_CLASS_POTION, 300, 0, 2);
	CHECK(pBelt->AddItem(pPotion, 1));
	scene.me->SetItemCheckBuffer(pPotion, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	calls.clear();
	CHECK(UseItemOK());
	CHECK_EQ(1, int(pPotion->GetNumber()));
	CHECK(pBelt->GetItem(1) == pPotion);
	CHECK(Same({"sound|30", "unlock gear", "unlock item", "unlock trade", "unlock gear"}));
	scene.me->SetItemCheckBuffer(pPotion, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	CHECK(UseItemOK());
	CHECK(pBelt->GetItem(1) == NULL);
	CHECK(scene.DescriptorRemoved(pPotion));
	CHECK(scene.Destroyed(300));
	g_pQuickSlot = NULL;
}

TEST(PacketZoneItems, AQuickSlotKeyIsKept)
{
	Scene scene(Slayer);
	scene.Items(ITEM_CLASS_KEY, 1);
	std::unique_ptr<MItem> key(scene.NewItem(ITEM_CLASS_KEY, 300));
	scene.me->SetItemCheckBuffer(key.get(), MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	calls.clear();
	CHECK(UseItemOK());
	CHECK(Same({"sound|30", "unlock gear", "unlock item", "unlock trade", "unlock gear"}));
	CHECK(descriptorsRemoved.empty());
	CHECK(itemsDestroyed.empty());
}

TEST(PacketZoneItems, AnOustersLastPupaLeavesTheArmsBandHoldingIt)
{
	Scene scene(Ousters);
	scene.Items(ITEM_CLASS_OUSTERS_ARMSBAND, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).Value3 = 2;
	scene.Items(ITEM_CLASS_PUPA, 1);
	std::unique_ptr<MItem> band1(scene.NewItem(ITEM_CLASS_OUSTERS_ARMSBAND, 200));
	std::unique_ptr<MItem> band2(scene.NewItem(ITEM_CLASS_OUSTERS_ARMSBAND, 201));
	MOustersArmsBand* pBand1 = static_cast<MOustersArmsBand*>(band1.get());
	MOustersArmsBand* pBand2 = static_cast<MOustersArmsBand*>(band2.get());
	g_pArmsBand1 = pBand1;
	g_pArmsBand2 = pBand2;
	MItem* pKept = scene.NewItem(ITEM_CLASS_PUPA, 299, 0, 1);
	CHECK(pBand1->AddItem(pKept, 1));
	MItem* pPupa = scene.NewItem(ITEM_CLASS_PUPA, 300, 0, 1);
	CHECK(pBand2->AddItem(pPupa, 1));
	scene.me->SetItemCheckBuffer(pPupa, MPlayer::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	CHECK(UseItemOK());
	CHECK(pBand2->GetItem(1) == NULL);
	CHECK(pBand1->GetItem(1) == pKept);
	CHECK(scene.DescriptorRemoved(pPupa));
	CHECK(scene.Destroyed(300));
	g_pArmsBand1 = g_pArmsBand2 = NULL;
}

TEST(PacketZoneItems, ACoupleRingFromTheGearStartsTheLoveChain)
{
	Scene scene(Slayer);
	scene.Items(ITEM_CLASS_COUPLE_RING, 1);
	std::unique_ptr<MItem> ring(scene.NewItem(ITEM_CLASS_COUPLE_RING, 300));
	scene.me->SetItemCheckBuffer(ring.get(), MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GEAR);
	CHECK(UseItemOK());
	CHECK(scene.me->GetItemCheckBuffer() == NULL);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_LOVE_CHAIN), int(scene.me->GetWaitVerify()));
	CHECK(scene.me->HasEffectStatus(EFFECTSTATUS_LOVE_CHAIN));
	const MEvent* pEvent = scene.events.GetEvent(EVENTID_LOVECHAIN);
	CHECK(pEvent != NULL);
	if (pEvent != NULL)
	{
		CHECK_EQ(10000, pEvent->eventDelay);
		CHECK_EQ(int(EVENTFLAG_SHOW_DELAY_STRING), int(pEvent->eventFlag));
		CHECK_EQ(int(EVENTTYPE_ZONE), int(pEvent->eventType));
		CHECK(pEvent->m_StringsID == std::vector<int>({ STRING_MESSAGE_COUPLE_MOVE_START, STRING_MESSAGE_MOVE_DELAY_SEC }));
	}
	CHECK(itemsDestroyed.empty());
}

TEST(PacketZoneItems, ACoupleRingWhileWaitingFailsWithTheHandEmptied)
{
	Scene scene(Vampire);
	scene.Items(ITEM_CLASS_VAMPIRE_COUPLE_RING, 1);
	std::unique_ptr<MItem> ring(scene.NewItem(ITEM_CLASS_VAMPIRE_COUPLE_RING, 300));
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_SKILL_SUCCESS);
	scene.me->SetItemCheckBuffer(ring.get(), MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GEAR);
	CHECK(!UseItemOK());
	CHECK(scene.me->GetItemCheckBuffer() == NULL);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_SKILL_SUCCESS), int(scene.me->GetWaitVerify()));
	CHECK(scene.events.GetEvent(EVENTID_LOVECHAIN) == NULL);
	// Any other gear does nothing more.
	scene.me->SetWaitVerifyNULL();
	scene.Items(ITEM_CLASS_RING, 1);
	std::unique_ptr<MItem> plain(scene.NewItem(ITEM_CLASS_RING, 301));
	scene.me->SetItemCheckBuffer(plain.get(), MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GEAR);
	CHECK(UseItemOK());
	CHECK(scene.me->IsWaitVerifyNULL());
	CHECK(scene.events.GetEvent(EVENTID_LOVECHAIN) == NULL);
}

TEST(PacketZoneItems, AQuestInventoryItemOnlyEmptiesTheHand)
{
	Scene scene(Slayer);
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<MItem> item(scene.NewItem(ITEM_CLASS_POTION, 300, 0, 2));
	scene.me->SetItemCheckBuffer(item.get(), MPlayer::ITEM_CHECK_BUFFER_USE_FROM_GQUEST_INVENTORY);
	CHECK(UseItemOK());
	CHECK(scene.me->GetItemCheckBuffer() == NULL);
	CHECK_EQ(2, int(item->GetNumber()));
	CHECK(itemsDestroyed.empty());
}

TEST(PacketZoneItems, AnItemPutToAnotherOnlyEmptiesTheHandOutsideTheCodeSheet)
{
	Scene scene(Slayer);
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<MItem> item(scene.NewItem(ITEM_CLASS_POTION, 300, 0, 2));
	item->SetItemOptionList(std::list<TYPE_ITEM_OPTION>({ 0x12 }));
	scene.me->SetItemCheckBuffer(item.get(), MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM);
	calls.clear();
	CHECK(UseItemOK());
	CHECK(Same({"sound|30", "unlock gear", "unlock item", "unlock trade"}));
	CHECK(scene.me->GetItemCheckBuffer() == NULL);
	CHECK(item->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({ 0x12 }));
	CHECK_EQ(2, int(item->GetNumber()));
}

TEST(PacketZoneItems, ACodeSheetTakesTheTypeIntoTheNibbleOfItsCell)
{
	Scene scene(Slayer);
	scene.Items(ITEM_CLASS_POTION, 1);
	std::unique_ptr<MItem> sheet(scene.NewItem(ITEM_CLASS_POTION, 300));
	// Thirty options, two cells each: 0x11, 0x12, ...
	std::list<TYPE_ITEM_OPTION> options;
	for (int i = 0; i < 30; ++i) options.push_back(TYPE_ITEM_OPTION(0x11 + i));
	struct Case { int x, y; int option; TYPE_ITEM_OPTION value; };
	// Cell (x, y) is offset y*10+x; an even offset takes the high nibble of
	// option offset/2, an odd one the low.
	const Case cases[] = { { 4, 0, 2, 0x63 }, { 5, 0, 2, 0x16 }, { 9, 5, 29, 0x26 }, { 0, 0, 0, 0x61 } };
	for (const Case& c : cases)
	{
		sheet->SetItemOptionList(options);
		MItem* pMouse = scene.NewItem(ITEM_CLASS_POTION, 301);
		scene.temp.SetMode(TempInformation::MODE_ITEM_CODE_SHEET);
		scene.temp.Value1 = 5;		// the type is one more: 6
		scene.temp.Value2 = (c.x << 16) | c.y;
		scene.temp.pValue = pMouse;
		scene.me->SetItemCheckBuffer(sheet.get(), MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM);
		itemsDestroyed.clear();
		calls.clear();
		CHECK(UseItemOK());
		std::list<TYPE_ITEM_OPTION> wanted = options;
		std::list<TYPE_ITEM_OPTION>::iterator at = wanted.begin();
		std::advance(at, c.option);
		*at = c.value;
		CHECK(sheet->GetItemOptionList() == wanted);
		// The mouse's item goes, the mode ends, the hand is let go.
		CHECK(scene.Destroyed(301));
		CHECK(scene.temp.pValue == NULL);
		CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
		CHECK(Same({"sound|30", "unlock gear", "unlock item", "unlock trade", "drop item"}));
	}
	// A sheet that is not thirty options, or a cell past the sixty, is not
	// written; the rest happens.
	std::list<TYPE_ITEM_OPTION> few({ 0x11, 0x22 });
	sheet->SetItemOptionList(few);
	scene.temp.SetMode(TempInformation::MODE_ITEM_CODE_SHEET);
	scene.temp.Value1 = 5;
	scene.temp.Value2 = 0;
	scene.temp.pValue = NULL;
	scene.me->SetItemCheckBuffer(sheet.get(), MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM);
	CHECK(UseItemOK());
	CHECK(sheet->GetItemOptionList() == few);
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	sheet->SetItemOptionList(options);
	scene.temp.SetMode(TempInformation::MODE_ITEM_CODE_SHEET);
	scene.temp.Value2 = 6;		// (0, 6): offset 60
	scene.me->SetItemCheckBuffer(sheet.get(), MPlayer::ITEM_CHECK_BUFFER_ITEM_TO_ITEM);
	CHECK(UseItemOK());
	CHECK(sheet->GetItemOptionList() == options);
}

//----------------------------------------------------------------------
// The vampire portal
//----------------------------------------------------------------------
TEST(PacketZoneItems, AVampirePortalOpensAtItsCellLeadingWhereItSays)
{
	Scene scene;
	scene.Show(RESULT_MAGIC_BLOODY_TUNNEL, 1);
	AddVampirePortal(4321, "Dracula", 6, 9, 120, 1003, 33, 44, true);
	AddVampirePortal(4322, "Mina", 7, 9, 0, 1004, 35, 46, false);
	CHECK(started.size() == 2u);
	if (started.size() != 2u) return;
	const Started& s = started[0];
	CHECK_EQ(int(RESULT_MAGIC_BLOODY_TUNNEL), int(s.action));
	CHECK_EQ(PixelX(6), s.x0);
	CHECK_EQ(PixelY(9), s.y0);
	CHECK_EQ(0, s.z0);
	CHECK(s.x1 == 0 && s.y1 == 0 && s.z1 == 0);
	CHECK_EQ(int(OBJECTID_NULL), int(s.creatureID));
	CHECK_EQ(int(DIRECTION_DOWN), s.direction);
	CHECK_EQ(1, s.power);
	CHECK_EQ(120, int(s.delay));
	CHECK(!s.result);
	CHECK(s.portal);
	CHECK_EQ(4321, s.serverID);
	CHECK(s.owner == "Dracula");
	CHECK_EQ(1003, s.zoneID);
	CHECK_EQ(33, s.zoneX);
	CHECK_EQ(44, s.zoneY);
	// From its main node when asked, else from the start.
	CHECK_EQ(2, s.step);
	CHECK_EQ(1, started[1].step);
	CHECK_EQ(4322, started[1].serverID);
	CHECK(started[1].owner == "Mina");
	CHECK_EQ(PixelX(7), started[1].x0);
}

TEST(PacketZoneItems, AVampirePortalWithoutItsShowOpensNothing)
{
	Scene scene;
	AddVampirePortal(4321, "Dracula", 6, 9, 120, 1003, 33, 44, true);
	CHECK(started.empty());
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// Coming out of and going into a portal
//----------------------------------------------------------------------
namespace {
MCreature* Riser(Scene& scene, int type, TYPE_OBJECTID id, int x, int y)
{
	scene.Tribe(type, CREATURETRIBE_VAMPIRE);
	// The body is sprite type 2, frame 33.
	testfw::MutableRow(scene.creatureTypes, type).SpriteTypes.Init(1);
	testfw::MutableRow(scene.creatureTypes, type).SpriteTypes.Set(0, 2);
	scene.sprites.GetMutable(2)->FrameID = 33;
	MCreature* pCreature = new MCreatureWear;
	pCreature->SetZone(g_pZone);
	pCreature->SetCreatureType(TYPE_CREATURETYPE(type));
	pCreature->SetID(id);
	pCreature->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	pCreature->SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	CHECK(g_pZone->AddCreature(pCreature));
	return pCreature;
}
}

TEST(PacketZoneItems, ACreatureRisesFromAPortalAsTallAsItsSprite)
{
	Scene scene;
	scene.Show(RESULT_MAGIC_BLOODY_TUNNEL_INTO, 1);
	MCreature* pCreature = Riser(scene, Vampire, 500, 6, 9);
	struct Case { int height; int start; int inc; };
	// The height rounded down to sixteen after adding fifteen; a sixteenth
	// of that a frame, at least one.
	const Case cases[] = { { 0, 0, 1 }, { 1, 16, 1 }, { 16, 16, 1 }, { 17, 32, 2 }, { 100, 112, 7 }, { 200, 208, 13 } };
	for (const Case& c : cases)
	{
		spriteHeight = c.height;
		calls.clear();
		started.clear();
		ComeFromPortal(pCreature);
		CHECK(Same({"height|33|" + N(ACTION_STAND) + "|2"}));
		CHECK(pCreature->IsCutHeight());
		CHECK_EQ(c.start, pCreature->GetCutHeightCount());
		CHECK_EQ(-c.inc, CutInc(pCreature));
		CHECK_EQ(0, CutLast(pCreature));
		// Under the bloody tunnel at its cell, from its first phase.
		CHECK(started.size() == 1u);
		if (started.size() != 1u) continue;
		const Started& s = started[0];
		CHECK_EQ(int(RESULT_MAGIC_BLOODY_TUNNEL_INTO), int(s.action));
		CHECK_EQ(PixelX(6), s.x0);
		CHECK_EQ(PixelY(9), s.y0);
		CHECK_EQ(0, s.z0);
		CHECK_EQ(PixelX(6), s.x1);
		CHECK_EQ(PixelY(9), s.y1);
		CHECK_EQ(0, s.z1);
		CHECK_EQ(500, int(s.creatureID));
		CHECK_EQ(int(DIRECTION_DOWN), s.direction);
		CHECK_EQ(1, s.step);
		CHECK_EQ(0, int(s.delay));
		CHECK(!s.result);
	}
}

TEST(PacketZoneItems, ACreatureWithoutASpriteRisesFromEightyButNeverShows)
{
	Scene scene;
	MCreature* pCreature = Riser(scene, Vampire, 500, 6, 9);
	spriteHeight = -1;
	ComeFromPortal(pCreature);
	// Recorded defect (PacketFunction.cpp:4165): the default increment is
	// -5 and is negated again, so the cut is set to grow from 80 to 0 and
	// ends on its first update; the creature appears at once instead of
	// rising.
	CHECK(pCreature->IsCutHeight());
	CHECK_EQ(80, pCreature->GetCutHeightCount());
	CHECK_EQ(5, CutInc(pCreature));
	CHECK_EQ(0, CutLast(pCreature));
	pCreature->UpdateCutHeight();
	CHECK(!pCreature->IsCutHeight());
	// Without the view's entry it is the same.
	WorldEvents::Host bare{};
	const WorldEvents::Host* old = WorldEvents::SetHost(&bare);
	ComeFromPortal(pCreature);
	WorldEvents::SetHost(old);
	CHECK_EQ(80, pCreature->GetCutHeightCount());
	CHECK_EQ(5, CutInc(pCreature));
}

TEST(PacketZoneItems, AWolverineAndTheGreatRuffianRiseUnderTheirOwnEffects)
{
	Scene scene;
	scene.Show(RESULT_MAGIC_BLOODY_TUNNEL_INTO);
	scene.Show(RESULT_SUMMON_ULBERINE);
	scene.Show(SKILL_CLIENT_GREAT_RUFFIAN_ALIVE);
	struct Case { int type; int action; };
	const Case cases[] = {
		{ 692, RESULT_MAGIC_BLOODY_TUNNEL_INTO }, { 693, RESULT_SUMMON_ULBERINE }, { 697, RESULT_SUMMON_ULBERINE },
		{ 698, RESULT_MAGIC_BLOODY_TUNNEL_INTO }, { 764, RESULT_MAGIC_BLOODY_TUNNEL_INTO },
		{ 765, SKILL_CLIENT_GREAT_RUFFIAN_ALIVE }, { 766, RESULT_MAGIC_BLOODY_TUNNEL_INTO },
	};
	TYPE_OBJECTID id = 500;
	for (const Case& c : cases)
	{
		MCreature* pCreature = Riser(scene, c.type, id, 6, int(id - 495));
		started.clear();
		ComeFromPortal(pCreature);
		CHECK(started.size() == 1u);
		if (started.size() == 1u)
		{
			CHECK_EQ(c.action, int(started[0].action));
			CHECK_EQ(int(id), int(started[0].creatureID));
			CHECK_EQ(PixelY(int(id - 495)), started[0].y1);
			CHECK_EQ(int(DIRECTION_DOWN), started[0].direction);
			CHECK_EQ(1, started[0].step);
		}
		++id;
	}
}

TEST(PacketZoneItems, NoCreatureComesFromAPortal)
{
	Scene scene;
	scene.Show(RESULT_MAGIC_BLOODY_TUNNEL_INTO);
	ComeFromPortal(NULL);
	CHECK(calls.empty());
	CHECK(started.empty());
}

// (A copy the zone refuses is freed and then read, PacketFunction.cpp:4271
// and 4289; not exercised, as a new fake's ID is always free.)
TEST(PacketZoneItems, ACreatureGoesIntoAPortalAsAShrinkingCopy)
{
	Scene scene;
	scene.Show(RESULT_MAGIC_BLOODY_TUNNEL_INTO, 1);
	MCreature* pCreature = Riser(scene, Vampire, 500, 6, 9);
	pCreature->SetDirection(DIRECTION_LEFT);
	pCreature->SetCurrentDirection(DIRECTION_LEFT);
	MoveIntoPortal(pCreature);
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
	if (scene.zone->GetFakeCreatureNumber() != 1) return;
	MFakeCreature* pCopy = static_cast<MFakeCreature*>(scene.zone->GetFakeCreatureBegin()->second);
	CHECK(pCopy != NULL);
	CHECK_EQ(int(Vampire), int(pCopy->GetCreatureType()));
	CHECK_EQ(6, int(pCopy->GetX()));
	CHECK_EQ(9, int(pCopy->GetY()));
	CHECK_EQ(int(DIRECTION_LEFT), int(pCopy->GetDirection()));
	CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_TO_PORTAL), int(pCopy->GetFakeCreatureType()));
	// Cut from nothing to 80, five at a time.
	CHECK(pCopy->IsCutHeight());
	CHECK_EQ(0, pCopy->GetCutHeightCount());
	CHECK_EQ(5, CutInc(pCopy));
	CHECK_EQ(80, CutLast(pCopy));
	// The creature itself stays.
	CHECK(scene.zone->GetCreature(500) == pCreature);
	CHECK(started.size() == 1u);
	if (started.size() != 1u) return;
	const Started& s = started[0];
	CHECK_EQ(int(RESULT_MAGIC_BLOODY_TUNNEL_INTO), int(s.action));
	CHECK_EQ(PixelX(6), s.x0);
	CHECK_EQ(PixelY(9), s.y0);
	CHECK_EQ(0, s.z0);
	CHECK_EQ(PixelX(6), s.x1);
	CHECK_EQ(PixelY(9), s.y1);
	CHECK_EQ(int(pCopy->GetID()), int(s.creatureID));
	CHECK_EQ(int(DIRECTION_DOWN), s.direction);
	CHECK_EQ(1, s.step);
	CHECK_EQ(0, int(s.delay));
	CHECK(!s.result);
	CHECK(calls.empty());
}
