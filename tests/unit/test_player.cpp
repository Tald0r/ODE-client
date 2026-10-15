#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MSector.h"
#include "MCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MZoneTable.h"
#include "MWarManager.h"
#include "MParty.h"
#include "Exception.h"
#include "MSkillManager.h"
#include "MGameStringTable.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "MItem.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "FrameClock.h"
#include "InputState.h"
#include "WorldTileGeometry.h"
#include "ZoneFlags.h"
#include "GameSocket.h"
#include "ClientPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "PlayerPresentationHost.h"
#include "StatusPresentationHost.h"
#include "DialogPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "ModifyStatusManager.h"
#include "MHelpDef.h"
#include "ClientConfig.h"
#include "TempInformation.h"
#include "MObjectSelector.h"
#include "MEffectGeneratorTable.h"
#include "MJusticeAttackManager.h"
#include "MOustersGear.h"
#include "MStorage.h"
#include "MFakeCreature.h"
#include "MSoundTable.h"
#include "SoundDef.h"
#include "MPortal.h"
#include "ClientMode.h"
#include "MStatusManager.h"
#include "NoticePresentationHost.h"
#include "PetWindowInfo.h"
#include "MActionResult.h"
#include "ClientCommunicationManager.h"
#include "SystemAvailabilities.h"
#include "ZoneLoadingHost.h"
#include "GameInfoPresentationHost.h"
#include "MSlayerGear.h"
#include "MInventory.h"
#include "UserInformation.h"
#include "SkillDef.h"
#include "MGuildType.h"
#include "SocketEncryptOutputStream.h"

#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The player (task 4.247, gamemodel's since then): the creature the user
// moves, MPlayer, standing in a real zone (g_pZone behind ZoneState), with
// the UI, the view and the socket as hosts that answer and record.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }
using Calls = std::vector<std::string>;

// The calls made in this order, others between them or not.
bool InOrder(const Calls& expected)
{
	size_t at = 0;
	for (const std::string& call : calls)
		if (at < expected.size() && call == expected[at]) ++at;
	if (at == expected.size()) return true;
	std::printf("      missing from [%s] on\n", expected[at].c_str());
	return false;
}

int Count(const std::string& call)
{
	int count = 0;
	for (const std::string& made : calls) count += made == call;
	return count;
}

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

// The socket: each packet sent, as its id and its body's bytes.
std::vector<std::string> sent;
std::string Render(const Packet& packet)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream encoded(&out);
	packet.write(encoded);
	std::string text = N(packet.getPacketID()) + ":";
	char hex[3];
	for (unsigned char byte : SocketOutputStreamTestAccess::Bytes(encoded))
	{
		std::snprintf(hex, sizeof(hex), "%02x", byte);
		text += hex;
	}
	return text;
}
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override { sent.push_back(Render(*pPacket)); }
};

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
	host.SetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { pZone->SetServerBlock(BYTE(moveType), x, y); };
	host.UnSetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { pZone->UnSetServerBlock(BYTE(moveType), x, y); };
	host.MoveGroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveGroundCreature(pCreature, xo, yo, xn, yn);
	};
	host.MoveFlyingCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveFlyingCreature(pCreature, xo, yo, xn, yn);
	};
	host.MoveUndergroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveUndergroundCreature(pCreature, xo, yo, xn, yn);
	};
	host.CanMove = [](int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return g_pZone->CanMove(BYTE(moveType), x, y); };
	host.CanMoveIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return pZone->CanMove(BYTE(moveType), x, y); };
	host.GetWidthIn = [](MZone* pZone) { return int(pZone->GetWidth()); };
	host.GetHeightIn = [](MZone* pZone) { return int(pZone->GetHeight()); };
	host.GetCreatureIn = [](MZone* pZone, TYPE_OBJECTID id) { return pZone->GetCreature(id); };
	host.DeleteCreature = [](MCreature* pCreature) { delete pCreature; };
	return host;
}();

// The player itself, as GameInit hands it out.
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.Status = []() -> MStatus* { return g_pPlayer; };
	host.GetID = []() { return g_pPlayer != NULL ? g_pPlayer->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer->HasEffectStatus(status); };
	host.GetItemCheckBuffer = []() { return g_pPlayer->GetItemCheckBuffer(); };
	host.ClearItemCheckBuffer = []() { g_pPlayer->ClearItemCheckBuffer(); };
	return host;
}();

// The view: a recorded selection and fade, and a fixed sector under the
// cursor.
POINT selectedSector = { 7, 8 };
POINT cursorPixel = { 0, 0 };
int viewRequest = MRequestMode::REQUEST_NULL;
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	host.GetFirstSector = []() { POINT p = { 10, 5 }; return p; };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID,
		TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		calls.push_back("skill|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "," + N(startZ) + "|" + N(direction) + "|" + N(targetID)
			+ "|" + N(targetX) + "," + N(targetY) + "," + N(targetZ) + "|" + N(delayFrame) + "|" + (pResult != NULL ? "result" : "none")
			+ "|" + (bStartFromMainNode ? "main" : "first") + "|" + N(sX) + "," + N(sY));
		delete pResult;
	};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool repeat, int x, int y) { calls.push_back("sound at|" + N(soundID) + "|" + N(repeat) + "|" + N(x) + "," + N(y)); };
	host.SetSelectedSectorNULL = []() { calls.push_back("unselect sector"); };
	host.SetSelectedNULL = []() { calls.push_back("unselect"); };
	host.SetSelectedCreature = [](TYPE_OBJECTID id) { calls.push_back("select creature|" + N(id)); };
	host.GetSelectedSector = [](int, int) { return selectedSector; };
	host.ScreenToPixel = [](int, int) { return cursorPixel; };
	host.GetViewRequestMode = []() { return viewRequest; };
	host.IsViewRequestMode = []() { return viewRequest != MRequestMode::REQUEST_NULL; };
	host.SetFadeStart = [](signed char start, signed char end, signed char step, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint16_t delay) {
		calls.push_back("fade|" + N(start) + "," + N(end) + "," + N(step) + "|" + N(r) + "," + N(g) + "," + N(b) + "|" + N(delay));
	};
	host.SetFadeEnd = []() { calls.push_back("fade end"); };
	return host;
}();

// The UI: each call recorded; its windows open as a test sets them.
bool inventoryOpen = false;
bool mineProgress = false;
bool forceFeel = false;
std::string inputString;
bool gearOpen = false;
MItem* pickedUp = NULL;
const PlayerPresentation::Host presentationHost = []() {
	PlayerPresentation::Host host{};
	host.FinishRequestDie = []() { calls.push_back("finish die"); };
	host.FinishRequestResurrect = []() { calls.push_back("finish resurrect"); };
	host.CloseRequestShrineMinimap = []() { calls.push_back("close shrine minimap"); };
	host.SetChangeVampireTime = [](int time) { calls.push_back("vampire time|" + N(time)); };
	host.SetSilverDamage = [](int minDamage, int maxDamage) { calls.push_back("silver|" + N(minDamage) + "," + N(maxDamage)); };
	host.SetCharInfoDAM = [](int value, int value2) { calls.push_back("dam|" + N(value) + "," + N(value2)); };
	host.AddEffectStatus = [](int status, std::uint32_t delayFrame) { calls.push_back("ui status|" + N(status) + "|" + N(delayFrame)); };
	host.RemoveEffectStatus = [](int status) { calls.push_back("ui status off|" + N(status)); };
	host.LockItem = []() { calls.push_back("lock item"); };
	host.FinishLevelUpButton = []() { calls.push_back("finish level up"); };
	host.GetInventoryOpenState = []() { return inventoryOpen; };
	host.GetGearOpenState = []() { return gearOpen; };
	host.GetPickUpItem = []() { return pickedUp; };
	host.SetCursorAttack = []() { calls.push_back("cursor attack"); };
	host.RunElevator = []() { calls.push_back("elevator"); };
	host.RunTraceWindow = []() { calls.push_back("trace window"); };
	host.SetCannotTrace = []() { calls.push_back("cannot trace"); };
	host.IsInstallMineProgress = []() { return mineProgress; };
	host.StartInstallMineProgress = [](int gridX, int gridY) { calls.push_back("install mine|" + N(gridX) + "," + N(gridY)); };
	host.EndInstallMineProgress = []() { calls.push_back("end mine"); };
	host.ForceFeelReady = []() { return forceFeel; };
	host.ForceAction = [](int soundID) { calls.push_back("force|" + N(soundID)); };
	host.RunPetInfo = [](PETINFO* pInfo) {
		calls.push_back("pet|" + N(pInfo->ITEM_ID) + "|" + N(pInfo->LEVEL) + "|" + N(pInfo->ATTR) + "|" + N(pInfo->ATTR_VALUE)
			+ "|" + N(pInfo->OPTION) + "|" + pInfo->NICK_NAME + "|" + N(pInfo->ChangeAbleNickName));
	};
	host.GetInputString = []() { return inputString.c_str(); };
	host.AddInputString = [](const char* pString) { calls.push_back(std::string("input|") + pString); };
	host.RunExchangeCancel = [](const char* pName) { calls.push_back(std::string("exchange cancel|") + pName); };
	host.RunPartyCancel = [](const char* pName) { calls.push_back(std::string("party cancel|") + pName); };
	host.CloseSelectWayPoint = []() { calls.push_back("close waypoints"); };
	host.LockItemTrade = []() { calls.push_back("lock trade"); };
	return host;
}();
const StatusPresentation::Host statusHost = []() {
	StatusPresentation::Host host{};
	host.SetHP = [](int current, int maximum) { calls.push_back("hp bar|" + N(current) + "/" + N(maximum)); };
	host.SetCharInfoDefense = [](int value) { calls.push_back("defense|" + N(value)); };
	host.SetCharInfoProtection = [](int value) { calls.push_back("protection|" + N(value)); };
	host.SetCharInfoTOHIT = [](int value) { calls.push_back("tohit|" + N(value)); };
	return host;
}();
const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.UnlockItem = []() { calls.push_back("unlock item"); };
	host.UnlockItemTrade = []() { calls.push_back("unlock trade"); };
	host.UnlockGear = []() { calls.push_back("unlock gear"); };
	return host;
}();
const VsUiPresentation::Host vsUiHost = []() {
	VsUiPresentation::Host host{};
	host.LockGear = []() { calls.push_back("lock gear"); };
	host.SelectSkill = [](int skillID) { calls.push_back("select skill|" + N(skillID)); };
	host.UnselectSkill = []() { calls.push_back("unselect skill"); };
	return host;
}();
const GameInfoPresentation::Host gameInfoHost = []() {
	GameInfoPresentation::Host host{};
	host.ExecuteHelpEvent = [](int helpEvent) { calls.push_back("help|" + N(helpEvent)); };
	host.RunOtherInfo = [](MCreature* pCreature) { calls.push_back("other info|" + N(pCreature->GetID())); };
	return host;
}();
const NoticePresentation::Host noticeHost = []() {
	NoticePresentation::Host host{};
	host.RunHorn = [](int zoneID) { calls.push_back("horn|" + N(zoneID)); };
	return host;
}();
const ZoneLoading::Host loadingHost = []() {
	ZoneLoading::Host host{};
	host.ChangeInterfaceRace = [](Race race) { calls.push_back("interface|" + N(race)); };
	return host;
}();
// Whom it may attack, by its tribe and the self-defence roster; no war.
const MObjectSelectorHost selectorHost = []() {
	MObjectSelectorHost host{};
	host.ForceAttack = []() { return false; };
	host.PlayerHasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer->HasEffectStatus(status); };
	host.PlayerCanAttackTribe = [](MCreature* pCreature) { return g_pPlayer->CanAttackTribe(pCreature); };
	host.PlayerCanAttackGuild = [](MCreature* pCreature) { return g_pPlayer->CanAttackGuild(pCreature); };
	host.IsNonPK = []() { return false; };
	host.IsNPC = [](MCreature* pCreature) { return pCreature->IsNPC(); };
	host.GetName = [](MCreature* pCreature) { return pCreature->GetName(); };
	host.HasPlayerOnlySprite = [](MCreature*) { return false; };
	host.HasZone = []() { return false; };
	host.JusticeAttackManager = []() { return g_pJusticeAttackManager; };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	host.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	host.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
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
// A target the effects let go of leaves the player's list, as GameInit's
// entry has it.
const MEffectTargetHost targetHost = []() {
	MEffectTargetHost host{};
	host.RemoveFromPlayer = [](BYTE id) { if (g_pPlayer != NULL) g_pPlayer->RemoveEffectTarget(id); };
	return host;
}();
const MItemHost itemHost = []() {
	MItemHost host{};
	host.NewItem = NewItemOfClass;
	host.DropFrameCount = [](TYPE_FRAMEID) { return 4; };
	return host;
}();

// The effects started, by their action and how many targets they have.
struct FakeGenerator : MEffectGenerator
{
	TYPE_EFFECTGENERATORID GetID() override { return 7; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override
	{
		calls.push_back("effect|" + N(info.nActionInfo) + "|" + N(info.x1) + "," + N(info.y1) + "|" + N(info.creatureID));
		return false;
	}
};

struct Generators : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
};

// The player, with what it keeps to itself open to the tests.
struct PlayerProbe : MPlayer
{
	using MPlayer::m_ActionCount;
	using MPlayer::m_ActionCountMax;
	using MPlayer::m_Action;
	using MPlayer::m_MoveAction;
	using MPlayer::m_DeadDelayTime;
	using MPlayer::m_bStopBloodDrain;
	using MPlayer::m_bRepeatAction;
	using MPlayer::m_bTraceCreatureToForceAttack;
	using MPlayer::m_SendMove;
	using MPlayer::m_bKnockBack;
	using MPlayer::m_DestX;
	using MPlayer::m_DestY;
	using MPlayer::m_NextDestX;
	using MPlayer::m_NextDestY;
	using MPlayer::m_DarknessCount;
	using MPlayer::m_DarknessCountInc;
	using MPlayer::m_listDirection;
	using MPlayer::m_listSendDirection;
	using MPlayer::m_BlockDestX;
	using MPlayer::m_BlockDestY;
	using MPlayer::m_fTrace;
	using MPlayer::m_TraceX;
	using MPlayer::m_TraceY;
	using MPlayer::m_TraceDistance;
	using MPlayer::m_nUsedActionInfo;
	using MPlayer::m_bAlive;
	using MPlayer::m_MoveCount;
	using MPlayer::m_MoveCountMax;
	using MPlayer::m_ServerX;
	using MPlayer::m_ServerY;
	using MPlayer::ActionMove;
	using MPlayer::m_Z;
	using MPlayer::m_bInCasket;
	using MPlayer::m_DelayTime;
	using MPlayer::m_pEffectTarget;
	using MPlayer::CalculateDistance;
	using MPlayer::IsItemHiddenInDarkness;
};

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
	MWarManager wars;
	MParty party;
	MSkillSet skills;
	CMessageArray systemLog;
	ModifyStatusManager modifyStatus;
	TempInformation temp;
	MSkillInfoTable skillInfo;
	MInventory inventory;
	MSlayerGear slayerGear;
	MOustersGear oustersGear;
	MObjectSelector selector{ &selectorHost };
	SystemAvailabilitiesManager systems;
	MJusticeAttackManager justice;
	CZoneTable zoneTable;
	Generators generators;
	Wire wire;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MEventQueue* oldEvents = g_pEventManager;
	MWarManager* oldWars = g_pWarManager;
	MParty* oldParty = g_pParty;
	MSkillSet* oldSkills = g_pSkillAvailable;
	CMessageArray* oldSystem = g_pSystemMessage;
	ModifyStatusManager* oldModify = g_pModifyStatusManager;
	TempInformation* oldTemp = g_pTempInformation;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MObjectSelector* oldSelector = g_pObjectSelector;
	MJusticeAttackManager* oldJustice = g_pJusticeAttackManager;
	SystemAvailabilitiesManager* oldSystems = g_pSystemAvailableManager;
	MStorage* oldStorage = g_pStorage;
	MMagazine* oldMagazine = g_pCurrentMagazine;
	CZoneTable* oldZoneTable = g_pZoneTable;
	MEffectGeneratorTable* oldGenerators = g_pEffectGeneratorTable;
	ClientPlayer* oldSocket = g_pSocket;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	bool oldZoneSafe = g_bZoneSafe;
	// What GameMain's input and the zone's arrival set, kept as they were.
	int oldMouseX = g_x, oldMouseY = g_y;
	bool oldLeft = g_bLButtonDown, oldRight = g_bRButtonDown, oldNetGood = g_bNetStatusGood;
	POINT oldSelectSector = g_SelectSector;
	bool oldInLarge = g_bZonePlayerInLarge;
	int oldZoneLarge = g_nZoneLarge, oldZoneSmall = g_nZoneSmall;
	int oldMode = g_Mode;
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const PlayerPresentation::Host* oldPresentation = PlayerPresentation::SetHost(&presentationHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialogHost);
	const StatusPresentation::Host* oldStatus = StatusPresentation::SetHost(&statusHost);
	const VsUiPresentation::Host* oldVsUi = VsUiPresentation::SetHost(&vsUiHost);
	const GameInfoPresentation::Host* oldGameInfo = GameInfoPresentation::SetHost(&gameInfoHost);
	const ZoneLoading::Host* oldLoading = ZoneLoading::SetHost(&loadingHost);
	const NoticePresentation::Host* oldNotice = NoticePresentation::SetHost(&noticeHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);
	const MEffectTargetHost* oldTargetHost = MEffectTarget::SetHost(&targetHost);
	DWORD oldFrame = g_CurrentFrame;
	MonotonicClock::TimePoint oldNow = g_FrameNow;
	std::unique_ptr<MZone> zone;
	PlayerProbe* me = nullptr;

	Scene()
	{
		static const bool moveTables = (MCreature::InitMoveTable(), true);
		(void)moveTables;
		calls.clear();
		sent.clear();
		inventoryOpen = gearOpen = mineProgress = forceFeel = false;
		selectedSector.x = 7;
		selectedSector.y = 8;
		cursorPixel.x = cursorPixel.y = 0;
		inputString.clear();
		viewRequest = MRequestMode::REQUEST_NULL;
		pickedUp = NULL;
		g_CurrentFrame = 1000;
		g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
		MItem::SetHost(&itemHost);
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		Tribe(5);
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		// (MActionInfo leaves its sound unset; the data file sets every
		// row's, and so does this.)
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
		g_pWarManager = &wars;
		g_pParty = &party;
		g_pSkillAvailable = &skills;
		InitGameStringTable();
		systemLog.Init(3, 512);
		g_pSystemMessage = &systemLog;
		g_pModifyStatusManager = &modifyStatus;
		g_pTempInformation = &temp;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		oustersGear.Init();
		g_pOustersGear = &oustersGear;
		g_pObjectSelector = &selector;
		g_pJusticeAttackManager = &justice;
		g_pSystemAvailableManager = &systems;
		g_pStorage = NULL;
		g_pCurrentMagazine = NULL;
		for (int id : { 0, 60, 74, 3001 })
		{
			ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
			pInfo->ID = TYPE_ZONEID(id);
			zoneTable.Add(pInfo);
		}
		g_pZoneTable = &zoneTable;
		generators.Add(new FakeGenerator);
		g_pEffectGeneratorTable = &generators;
		actions.GetMutable(SKILL_ATTACK_MELEE)->Set("", ACTION_ATTACK, EFFECTSPRITETYPE_NULL, 0, 0);
		g_pSocket = &wire;
		g_bZoneSafe = false;
		zone.reset(new MZone(40, 30));
		g_pZone = zone.get();
		me = new PlayerProbe;
		g_pPlayer = me;
		me->SetZone(g_pZone);
		me->SetCreatureType(5);
		me->SetID(77);
		me->SetPosition(20, 15);
		me->SetServerPosition(20, 15);
	}
	~Scene()
	{
		delete me;
		g_pPlayer = oldPlayer;
		zone.reset();
		g_pZone = oldZone;
		MItem::SetHost(NULL);
		MEffectTarget::SetHost(oldTargetHost);
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		NoticePresentation::SetHost(oldNotice);
		ZoneLoading::SetHost(oldLoading);
		GameInfoPresentation::SetHost(oldGameInfo);
		VsUiPresentation::SetHost(oldVsUi);
		StatusPresentation::SetHost(oldStatus);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		PlayerPresentation::SetHost(oldPresentation);
		WorldEvents::SetHost(oldWorld);
		PlayerState::SetHost(oldPlayerHost);
		ZoneState::SetHost(oldZoneHost);
		g_bZoneSafe = oldZoneSafe;
		g_x = oldMouseX;
		g_y = oldMouseY;
		g_bLButtonDown = oldLeft;
		g_bRButtonDown = oldRight;
		g_bNetStatusGood = oldNetGood;
		g_SelectSector = oldSelectSector;
		g_bZonePlayerInLarge = oldInLarge;
		g_nZoneLarge = oldZoneLarge;
		g_nZoneSmall = oldZoneSmall;
		g_Mode = decltype(g_Mode)(oldMode);
		g_pSocket = oldSocket;
		g_pWarManager = oldWars;
		g_pParty = oldParty;
		g_pSkillAvailable = oldSkills;
		g_pSystemMessage = oldSystem;
		g_pModifyStatusManager = oldModify;
		g_pTempInformation = oldTemp;
		g_pEffectGeneratorTable = oldGenerators;
		g_pZoneTable = oldZoneTable;
		g_pJusticeAttackManager = oldJustice;
		g_pSystemAvailableManager = oldSystems;
		delete g_pStorage;
		g_pStorage = oldStorage;
		g_pCurrentMagazine = oldMagazine;
		g_pObjectSelector = oldSelector;
		g_pOustersGear = oldOustersGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pSkillInfoTable = oldSkillInfo;
		oustersGear.Release();
		slayerGear.Release();
		inventory.Release();
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
		g_FrameNow = oldNow;
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
	// The type drawn with a sprite only players wear.
	void PlayerOnly(int type)
	{
		testfw::MutableRow(creatureTypes, type).SpriteTypes.Init(1);
		testfw::MutableRow(creatureTypes, type).SpriteTypes.Set(0, 0);
		sprites.GetMutable(0)->CreatureType = FLAG_CREATURESPRITE_PLAYER_SLAYER;
	}
	// A skill the player has, ready, aimed as the flag says.
	void Skill(int id, BYTE target, BYTE range, ACTIONINFO_PACKET packet, bool withEffect = false)
	{
		actions.GetMutable(id)->Set("", ACTION_MAGIC, EFFECTSPRITETYPE_NULL, range, target);
		if (withEffect)
		{
			actions.GetMutable(id)->Init(1);
			actions.GetMutable(id)->GetMutable(0)->EffectGeneratorID = 7;
			actions.GetMutable(id)->GetMutable(0)->Count = 5;
		}
		actions.GetMutable(id)->SetPacketType(packet);
		skills.AddSkill(ACTIONINFO(id));
		skillInfo.GetMutable(id)->SetEnable(true);
		skillInfo.GetMutable(id)->minRange = range;
		skillInfo.GetMutable(id)->maxRange = range;
	}
	MSector& Sector(int x, int y) { return const_cast<MSector&>(zone->GetSector(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y))); }
	// The system messages shown, oldest first.
	std::vector<std::string> System()
	{
		std::vector<std::string> lines;
		for (int i = 0; i < systemLog.GetSize(); i++)
			if (systemLog[i] != nullptr && systemLog[i][0] != '\0') lines.push_back(systemLog[i]);
		return lines;
	}
};

// Where its path leads from where it stands, or (-1,-1) through a block.
POINT Walk(const PlayerProbe& me)
{
	POINT at = { me.GetX(), me.GetY() };
	for (BYTE direction : me.m_listDirection)
	{
		const POINT step = WorldTileGeometry::DirectionOffset(direction);
		at.x += step.x;
		at.y += step.y;
		if (!g_pZone->CanMove(me.GetMoveType(), TYPE_SECTORPOSITION(at.x), TYPE_SECTORPOSITION(at.y)))
		{
			at.x = at.y = -1;
			break;
		}
	}
	return at;
}

// A creature of the type at the cell in the zone.
MCreature* Make(TYPE_OBJECTID id, int x, int y, int type = 5)
{
	MCreature* pCreature = new MCreature;
	pCreature->SetZone(g_pZone);
	pCreature->SetCreatureType(TYPE_CREATURETYPE(type));
	pCreature->SetID(id);
	pCreature->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	pCreature->SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	return pCreature;
}

} // namespace

//----------------------------------------------------------------------
// What it is
//----------------------------------------------------------------------
TEST(Player, ItStartsIdleAndClearsTheGlobalWhenItGoes)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	POINT dest;
	me.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.y));
	CHECK(me.IsTraceNULL());
	CHECK(me.IsAttackModeNormal());
	CHECK(me.IsItemCheckBufferNULL());
	CHECK(me.IsWaitVerifyNULL());
	CHECK_EQ(1, int(me.GetBasicAttackDistance()));
	CHECK(!me.IsRepeatAction());
	CHECK_EQ(0, int(me.GetSendMove()));
	CHECK_EQ(int(MCreature::CLASS_PLAYER), int(me.GetClassType()));
	// Gone: the global and the user's competence flags are cleared.
	g_pUserInformation->bCompetence = true;
	g_pUserInformation->bCompetenceShape = true;
	delete scene.me;
	scene.me = nullptr;
	CHECK(g_pPlayer == NULL);
	CHECK(!g_pUserInformation->bCompetence);
	CHECK(!g_pUserInformation->bCompetenceShape);
}

TEST(Player, HoldingASkillOnItselfFromTheStartIsNoAttack)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	// A self skill is the first thing held down (SelfSpecialAction then
	// SetRepeatAction): no creature was ever traced, so it is no forced
	// attack, and the view shows no attack cursor.
	me.SetRepeatAction();
	CHECK(!me.OnAttacking());
	CHECK(!me.m_bTraceCreatureToForceAttack);
}

TEST(Player, ItsAttackModeDecidesWhomItMayAttack)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	MCreature slayer, vampire;
	slayer.SetCreatureType(5);
	vampire.SetCreatureType(6);
	// Normal: another tribe only; another guild of the same tribe, a user
	// guild excepted.
	CHECK(!me.CanAttackTribe(CREATURETRIBE_SLAYER));
	CHECK(me.CanAttackTribe(CREATURETRIBE_VAMPIRE));
	CHECK(me.CanAttackGuild(&vampire));
	me.SetGuildNumber(5);
	slayer.SetGuildNumber(6);
	CHECK(me.CanAttackGuild(&slayer));
	slayer.SetGuildNumber(5);
	CHECK(!me.CanAttackGuild(&slayer));
	slayer.SetGuildNumber(GUILDID_VAMPIRE_DEFAULT);
	CHECK(!me.CanAttackGuild(&slayer));
	slayer.SetGuildNumber(MIN_USER_GUILDID);
	CHECK(!me.CanAttackGuild(&slayer));
	slayer.SetGuildNumber(MIN_USER_GUILDID - 1);
	CHECK(me.CanAttackGuild(&slayer));
	// Peace: nobody; aggressive: everybody; guild mode: nobody here.
	me.SetAttackModePeace();
	CHECK(!me.CanAttackTribe(CREATURETRIBE_VAMPIRE));
	CHECK(!me.CanAttackGuild(&vampire));
	me.SetAttackModeAggress();
	CHECK(me.CanAttackTribe(CREATURETRIBE_SLAYER));
	CHECK(me.CanAttackGuild(&slayer));
	me.SetAttackModeGuild();
	CHECK(!me.CanAttackTribe(CREATURETRIBE_VAMPIRE));
	CHECK(!me.CanAttackGuild(&vampire));
}

TEST(Player, ItStopsWhereItStandsAndForgetsWhereItWent)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.m_DestX = 30;
	me.m_DestY = 20;
	me.m_NextDestX = 25;
	me.m_NextDestY = 18;
	me.m_bKnockBack = 3;
	me.SetAction(ACTION_ATTACK);
	me.SetStop();
	CHECK_EQ(ACTION_STAND, int(me.m_Action));
	CHECK_EQ(int(me.m_ActionCountMax), int(me.m_ActionCount));
	CHECK_EQ(0, int(me.m_bKnockBack));
	CHECK(me.IsTraceNULL());
	POINT dest;
	me.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	me.GetNextDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	// On a motorcycle it stops as one.
	me.m_MoveAction = ACTION_SLAYER_MOTOR_MOVE;
	me.SetStop();
	CHECK_EQ(ACTION_SLAYER_MOTOR_STAND, int(me.m_Action));
}

TEST(Player, ItsPathCostIsTheLongerSideSquaredAndASteppingOff)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.m_DestX = 10;
	me.m_DestY = 10;
	CHECK_EQ(0, me.CalculateDistance(10, 10));
	CHECK_EQ(9 + 1, me.CalculateDistance(13, 10));
	CHECK_EQ(16 + 2, me.CalculateDistance(14, 12));
	CHECK_EQ(25 + 2, me.CalculateDistance(8, 15));
	CHECK_EQ(4 + 1, me.CalculateDistance(10, 8));
}

TEST(Player, AnItemInDarknessIsHiddenFromALightlessSlayer)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	std::unique_ptr<MItem> item(NewItemOfClass(ITEM_CLASS_SWORD));
	item->SetPosition(3, 4);
	CHECK(!me.IsItemHiddenInDarkness(item.get()));
	scene.Sector(3, 4).SetDarkness();
	CHECK(me.IsItemHiddenInDarkness(item.get()));
	// Light, a ghost's sight, or a vampire's eyes see it.
	me.AddEffectStatus(EFFECTSTATUS_LIGHTNESS, 0xFFFF);
	CHECK(!me.IsItemHiddenInDarkness(item.get()));
	me.RemoveEffectStatus(EFFECTSTATUS_LIGHTNESS);
	me.AddEffectStatus(EFFECTSTATUS_GHOST, 0xFFFF);
	CHECK(!me.IsItemHiddenInDarkness(item.get()));
	me.RemoveEffectStatus(EFFECTSTATUS_GHOST);
	me.SetCreatureType(6);
	CHECK(!me.IsItemHiddenInDarkness(item.get()));
	// But not in zone 3001, where darkness hides from all but ghosts.
	scene.zone->SetCurrentZoneID(3001);
	CHECK(me.IsItemHiddenInDarkness(item.get()));
}

// Which way its darkness fades from halfway: -1 darker, 1 lighter, 0 as
// it was.
int Fading(PlayerProbe& me)
{
	me.m_DarknessCount = 5;
	me.m_DarknessCountInc = 0;
	me.CheckInDarkness();
	return me.m_DarknessCountInc;
}

TEST(Player, ItDarkensOnADarkCellUnlessItSeesThroughIt)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	CHECK_EQ(1, Fading(me));
	scene.Sector(20, 15).SetDarkness();
	CHECK_EQ(-1, Fading(me));
	// From full light it starts at the darkest.
	me.m_DarknessCount = -1;
	me.CheckInDarkness();
	CHECK(me.IsInDarkness());
	// Lightness, a ghost or a turret installed: it lightens.
	for (EFFECTSTATUS status : {EFFECTSTATUS_LIGHTNESS, EFFECTSTATUS_GHOST, EFFECTSTATUS_INSTALL_TURRET})
	{
		me.AddEffectStatus(status, 0xFFFF);
		CHECK_EQ(1, Fading(me));
		me.RemoveEffectStatus(status);
		CHECK_EQ(-1, Fading(me));
	}
	// A vampire's eyes: as it was, but in zone 3001, where lightness does
	// not help a slayer either.
	me.SetCreatureType(6);
	CHECK_EQ(0, Fading(me));
	scene.zone->SetCurrentZoneID(3001);
	CHECK_EQ(-1, Fading(me));
	me.SetCreatureType(5);
	me.AddEffectStatus(EFFECTSTATUS_LIGHTNESS, 0xFFFF);
	CHECK_EQ(-1, Fading(me));
}

TEST(Player, ItIsSafeOnACellSafeForItsRaceOrForAll)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	CHECK_EQ(0, me.IsInSafeSector());
	scene.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER);
	CHECK_EQ(1, me.IsInSafeSector());
	me.SetCreatureType(6);
	CHECK_EQ(0, me.IsInSafeSector());
	scene.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_VAMPIRE);
	CHECK_EQ(1, me.IsInSafeSector());
	me.SetCreatureType(7);
	CHECK_EQ(0, me.IsInSafeSector());
	scene.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_OUSTERS);
	CHECK_EQ(1, me.IsInSafeSector());
	// Safe for all, or a safe zone: 2; safe for the three races alone: 1.
	scene.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER | FLAG_SECTOR_SAFE_VAMPIRE | FLAG_SECTOR_SAFE_OUSTERS);
	CHECK_EQ(1, me.IsInSafeSector());
	scene.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_COMMON);
	CHECK_EQ(2, me.IsInSafeSector());
	scene.Sector(20, 15).SetSafe(0);
	g_bZoneSafe = true;
	CHECK_EQ(2, me.IsInSafeSector());
}

TEST(Player, ADrinkCutShortJumpsToItsEnd)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	testfw::MutableRow(scene.creatureTypes, 6).SetActionCount(ACTION_VAMPIRE_DRAIN, 70);
	me.SetCreatureType(6);
	me.SetAction(ACTION_VAMPIRE_DRAIN);
	CHECK_EQ(70, int(me.m_ActionCountMax));
	me.SetStopBloodDrain();
	// Too early (the first ten frames) or too late (past 60): as it was.
	me.m_ActionCount = 9;
	me.StopBloodDrain();
	CHECK_EQ(9, int(me.m_ActionCount));
	me.m_ActionCount = 60;
	me.StopBloodDrain();
	CHECK_EQ(60, int(me.m_ActionCount));
	CHECK_EQ(1, int(me.m_bStopBloodDrain));
	// Between: to its end (frame 60), the stop counted off.
	me.m_ActionCount = 30;
	me.StopBloodDrain();
	CHECK_EQ(60, int(me.m_ActionCount));
	CHECK_EQ(0, int(me.m_bStopBloodDrain));
	// Not a vampire, not draining, or a short drain: nothing.
	me.SetCreatureType(5);
	me.m_ActionCount = 30;
	me.StopBloodDrain();
	CHECK_EQ(30, int(me.m_ActionCount));
}

TEST(Player, ItsDeathDelayIsCountedInSecondsLeft)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.m_DeadDelayTime = g_FrameNow + MonotonicClock::Millis(4500);
	CHECK_EQ(4, int(me.GetDeadDelayLast()));
	CHECK(!me.IsNotDeadDelay());
	me.m_DeadDelayTime = g_FrameNow;
	CHECK_EQ(0, int(me.GetDeadDelayLast()));
	CHECK(me.IsNotDeadDelay());
	me.m_DeadDelayTime = g_FrameNow - MonotonicClock::Millis(10);
	CHECK_EQ(0, int(me.GetDeadDelayLast()));
}

TEST(Player, ResettingItsSentMovesTrustsTheNetworkAgain)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.m_SendMove = 3;
	g_bNetStatusGood = false;
	me.ResetSendMove();
	CHECK_EQ(0, int(me.GetSendMove()));
	CHECK(g_bNetStatusGood);
	// It attacks when it repeats a forced attack.
	CHECK(!me.OnAttacking());
	me.m_bRepeatAction = TRUE;
	CHECK(!me.OnAttacking());
	me.m_bTraceCreatureToForceAttack = true;
	CHECK(me.OnAttacking());
}

//----------------------------------------------------------------------
// Where it goes
//----------------------------------------------------------------------
TEST(Player, ItTakesTheNextDestinationItCanReach)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	POINT dest;
	CHECK(me.SetNextDestination(25, 15));
	me.GetNextDestination(dest);
	CHECK_EQ(25, int(dest.x));
	CHECK_EQ(15, int(dest.y));
	// Not where it stands, nowhere, or where it found no way before.
	CHECK(!me.SetNextDestination(20, 15));
	CHECK(!me.SetNextDestination(SECTORPOSITION_NULL, 15));
	CHECK(!me.SetNextDestination(20, SECTORPOSITION_NULL));
	me.m_BlockDestX = 30;
	me.m_BlockDestY = 3;
	CHECK(!me.SetNextDestination(30, 3));
	CHECK(me.SetNextDestination(30, 4));
	// A blocked cell: the nearest open one on the way back to it.
	scene.Sector(26, 18).SetBlockGround();
	scene.Sector(25, 17).SetBlockGround();
	CHECK(me.SetNextDestination(26, 18));
	me.GetNextDestination(dest);
	CHECK_EQ(24, int(dest.x));
	CHECK_EQ(16, int(dest.y));
	// A row blocked up to it on one side: the steps stop at its column.
	for (int x = 21; x <= 25; ++x) scene.Sector(x, 15).SetBlockGround();
	CHECK(!me.SetNextDestination(25, 15));
	// Off the map: nowhere to walk back from.
	CHECK(!me.SetNextDestination(45, 15));
	// Following something, it takes the cell as it is.
	me.m_fTrace = FLAG_TRACE_CREATURE_BASIC;
	CHECK(me.SetNextDestination(25, 15));
	me.GetNextDestination(dest);
	CHECK_EQ(25, int(dest.x));
}

TEST(Player, ItTakesNoDestinationDeadOrAsleepUnlessAGhost)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.AddEffectStatus(EFFECTSTATUS_SLEEP, 0xFFFF);
	CHECK(!me.SetNextDestination(25, 15));
	CHECK(!me.SetMovePosition(25, 15));
	CHECK(!me.SetDestination(25, 15));
	me.AddEffectStatus(EFFECTSTATUS_GHOST, 0xFFFF);
	CHECK(me.SetNextDestination(25, 15));
	CHECK(me.SetMovePosition(25, 16));
	CHECK(me.SetDestination(25, 15));
	me.RemoveEffectStatus(EFFECTSTATUS_GHOST);
	me.RemoveEffectStatus(EFFECTSTATUS_SLEEP);
	// Dead: the next destination waits for a ghost (which the dead take
	// on no more); a path waits for life.
	me.m_bAlive = false;
	CHECK(!me.SetNextDestination(25, 15));
	CHECK(!me.SetMovePosition(25, 15));
	me.m_bAlive = true;
	me.AddEffectStatus(EFFECTSTATUS_GHOST, 0xFFFF);
	me.m_bAlive = false;
	CHECK(me.SetNextDestination(25, 15));
	CHECK(!me.SetDestination(25, 15));
}

TEST(Player, ANewDestinationDropsTheSkillItWasAboutToUse)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.m_fTrace = FLAG_TRACE_CREATURE_SPECIAL;
	me.m_nUsedActionInfo = SKILL_DOUBLE_IMPACT;
	me.SetAction(ACTION_ATTACK);
	CHECK(me.SetNextDestination(25, 15));
	CHECK(me.IsTraceNULL());
	// Still in the act: the skill goes on.
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(me.m_nUsedActionInfo));
	me.m_ActionCount = me.m_ActionCountMax;
	CHECK(me.SetNextDestination(26, 15));
	CHECK_EQ(int(ACTIONINFO_NULL), int(me.m_nUsedActionInfo));
}

TEST(Player, ItFindsAPathAroundAWall)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	CHECK(me.SetDestination(24, 15));
	CHECK_EQ(4, int(me.m_listDirection.size()));
	for (BYTE direction : me.m_listDirection) CHECK_EQ(int(DIRECTION_RIGHT), int(direction));
	POINT next;
	CHECK(me.GetNextPosition(next));
	CHECK_EQ(21, int(next.x));
	CHECK_EQ(15, int(next.y));
	// A wall across the way: around it, through no blocked cell.
	for (int y = 12; y <= 18; ++y) scene.Sector(22, y).SetBlockGround();
	CHECK(me.SetDestination(24, 15));
	POINT end = Walk(me);
	CHECK_EQ(24, int(end.x));
	CHECK_EQ(15, int(end.y));
	CHECK(me.m_listDirection.size() >= 5);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(me.m_BlockDestX));
}

TEST(Player, ItRemembersAPlaceItFoundNoWayTo)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	// (30,20) walled in.
	for (int x = 29; x <= 31; ++x)
		for (int y = 19; y <= 21; ++y)
			if (x != 30 || y != 20) scene.Sector(x, y).SetBlockGround();
	CHECK(me.SetDestination(30, 20));
	CHECK(me.m_listDirection.empty());
	CHECK_EQ(30, int(me.m_BlockDestX));
	CHECK_EQ(20, int(me.m_BlockDestY));
	POINT dest;
	me.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	CHECK(!me.SetNextDestination(30, 20));
	// A way found clears it.
	CHECK(me.SetDestination(24, 15));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(me.m_BlockDestX));
	CHECK(me.GetNextPosition(dest));
	me.m_listDirection.clear();
	CHECK(!me.GetNextPosition(dest));
}

TEST(Player, ItGoesNowhereBlockedOffTheMapOrFromASanctuary)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Sector(24, 15).SetBlockGround();
	me.m_MoveCount = 0;
	CHECK(!me.SetDestination(24, 15));
	CHECK_EQ(int(me.m_MoveCountMax), int(me.m_MoveCount));
	POINT dest;
	me.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	CHECK(!me.SetDestination(40, 15));
	CHECK(!me.SetDestination(20, 30));
	CHECK(me.SetDestination(23, 15));
	scene.Sector(20, 15).SetSanctuary();
	CHECK(!me.SetDestination(23, 15));
	CHECK(me.m_listDirection.empty());
}

TEST(Player, ItCarriesNoRelicFlagOrBibleIntoItsSafeZone)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Sector(24, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER);
	scene.Sector(24, 16).SetSafe(FLAG_SECTOR_SAFE_VAMPIRE);
	CHECK(me.SetDestination(24, 15));
	const struct { EFFECTSTATUS status; int message; } carried[] = {
		{ EFFECTSTATUS_HAS_SLAYER_RELIC, STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_RELIC },
		{ EFFECTSTATUS_HAS_VAMPIRE_RELIC, STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_RELIC },
		{ EFFECTSTATUS_HAS_BLOOD_BIBLE_ARMEGA, STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_BLOOD_BIBLE },
		{ EFFECTSTATUS_HAS_BLOOD_BIBLE_CHASPA, STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_BLOOD_BIBLE },
		{ EFFECTSTATUS_HAS_FLAG, STRING_MESSAGE_CANNOT_MOVE_SAFETY_ZONE_FLAG },
	};
	for (const auto& c : carried)
	{
		scene.systemLog.Clear();
		me.AddEffectStatus(c.status, 0xFFFF);
		CHECK(!me.SetDestination(24, 15));
		CHECK(me.m_listDirection.empty());
		CHECK(scene.System() == std::vector<std::string>{ (*g_pGameStringTable)[c.message].GetString() });
		// Another race's safe cell is open to it.
		scene.systemLog.Clear();
		CHECK(me.SetDestination(24, 16));
		CHECK(scene.System().empty());
		me.RemoveEffectStatus(c.status);
	}
}

// Clicked at the cell, as CGameUpdate sets a walk going.
void Go(PlayerProbe& me, int x, int y)
{
	CHECK(me.SetMovePosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y)));
	me.TraceNULL();
	me.SetNextActionToMove();
}

void Frames(PlayerProbe& me, int count)
{
	for (int frame = 0; frame < count; ++frame) me.Action();
}

// A CGMove: the direction, then the cell it leaves.
std::string Move(int direction, int x, int y)
{
	char text[16];
	std::snprintf(text, sizeof(text), "%d:%02x%02x%02x", int(Packet::PACKET_CG_MOVE), direction, x, y);
	return text;
}

TEST(Player, CarryingARelicItReadsNoCellOffTheMap)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.AddEffectStatus(EFFECTSTATUS_HAS_SLAYER_RELIC, 0xFFFF);
	me.AddEffectStatus(EFFECTSTATUS_HAS_FLAG, 0xFFFF);
	// The first row and column past the map: refused, with no relic or
	// flag message and no cell read there (ASan watches the row array).
	CHECK(!me.SetDestination(20, 30));
	CHECK(!me.SetDestination(40, 15));
	CHECK(scene.System().empty());
	POINT dest;
	me.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	// A sector trace takes the cell as it is; walking there asks again.
	me.SetBasicActionInfo(SKILL_ATTACK_MELEE);
	CHECK(me.TraceSectorToBasicAction(20, 30));
	Frames(me, 30);
	CHECK_EQ(20, int(me.GetX()));
	CHECK_EQ(15, int(me.GetY()));
	CHECK(sent.empty());
	CHECK(scene.System().empty());
}

TEST(Player, ItWalksACellAStepAndTellsTheServerEachOne)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	Go(me, 23, 15);
	// The first frame sets out; the second steps, from the cell it leaves.
	Frames(me, 1);
	CHECK(sent.empty());
	Frames(me, 1);
	CHECK_EQ(21, int(me.GetX()));
	CHECK(sent == std::vector<std::string>{ Move(DIRECTION_RIGHT, 20, 15) });
	CHECK_EQ(1, int(me.GetSendMove()));
	CHECK_EQ(1, int(me.m_listSendDirection.size()));
	// A step takes its action's frames (11 here).
	Frames(me, 10);
	CHECK_EQ(21, int(me.GetX()));
	Frames(me, 1);
	CHECK_EQ(22, int(me.GetX()));
	Frames(me, 30);
	CHECK_EQ(23, int(me.GetX()));
	CHECK_EQ(15, int(me.GetY()));
	CHECK(sent == (std::vector<std::string>{ Move(DIRECTION_RIGHT, 20, 15), Move(DIRECTION_RIGHT, 21, 15), Move(DIRECTION_RIGHT, 22, 15) }));
	CHECK_EQ(3, int(me.GetSendMove()));
	CHECK(me.IsStop());
	CHECK(g_bNetStatusGood);
}

TEST(Player, ItWaitsForTheServerPastItsUnansweredMoves)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	g_pClientConfig->MAX_CLIENT_MOVE = 2;
	Go(me, 27, 15);
	Frames(me, 80);
	// Three sent (0, 1 and 2 unanswered), then it waits.
	CHECK_EQ(3, int(sent.size()));
	CHECK_EQ(23, int(me.GetX()));
	CHECK(!g_bNetStatusGood);
	// An answer lets it go on.
	me.PacketMoveOK(21, 15, DIRECTION_RIGHT);
	CHECK_EQ(2, int(me.GetSendMove()));
	CHECK_EQ(21, int(me.GetServerX()));
	CHECK(g_bNetStatusGood);
	Frames(me, 12);
	CHECK_EQ(4, int(sent.size()));
	CHECK(sent.back() == Move(DIRECTION_RIGHT, 23, 15));
}

TEST(Player, TheServerPlacesItWhereItHasNoMoveUnanswered)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	// Unasked: it is put there and forgets where it was going.
	me.m_NextDestX = 30;
	me.m_NextDestY = 20;
	me.m_fTrace = FLAG_TRACE_SECTOR_BASIC;
	me.PacketMoveOK(18, 12, DIRECTION_LEFTUP);
	CHECK_EQ(18, int(me.GetX()));
	CHECK_EQ(12, int(me.GetY()));
	CHECK_EQ(18, int(me.GetServerX()));
	CHECK_EQ(12, int(me.GetServerY()));
	CHECK(me.IsTraceNULL());
	CHECK_EQ(int(SECTORPOSITION_NULL), int(me.m_NextDestX));
	CHECK(me.m_listDirection.empty());
	CHECK_EQ(0, int(me.GetSendMove()));
}

TEST(Player, ARefusedMoveTakesItBackToTheServersCell)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	Go(me, 24, 15);
	Frames(me, 24);
	CHECK_EQ(23, int(me.GetX()));
	CHECK_EQ(3, int(me.GetSendMove()));
	me.PacketMoveNO();
	CHECK_EQ(20, int(me.GetX()));
	CHECK_EQ(15, int(me.GetY()));
	CHECK_EQ(0, int(me.GetSendMove()));
	CHECK_EQ(2, int(me.m_listSendDirection.size()));
	// Mid-walk: it stops going there.
	CHECK(me.m_listDirection.empty());
	POINT dest;
	me.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	me.GetNextDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	CHECK_EQ(int(me.m_MoveCountMax) - 1, int(me.m_MoveCount));
}

TEST(Player, ItsServerCellKeepsTheZoneAroundItInSight)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	MCreature* pFar = Make(20000, 5, 15);
	MCreature* pNear = Make(20001, 25, 15);
	MCreature* pOwn = Make(9000, 2, 2);
	CHECK(scene.zone->AddCreature(pFar));
	CHECK(scene.zone->AddCreature(pNear));
	CHECK(scene.zone->AddCreature(pOwn));
	me.m_SendMove = 2;
	me.m_listSendDirection.push_back(DIRECTION_RIGHT);
	g_bNetStatusGood = false;
	me.SetServerPosition(30, 15);
	CHECK_EQ(0, int(me.GetSendMove()));
	CHECK(me.m_listSendDirection.empty());
	CHECK(g_bNetStatusGood);
	CHECK_EQ(30, int(me.GetServerX()));
	// The server's creature out of sight is gone; the client's own stay.
	CHECK(scene.zone->GetCreature(20000) == NULL);
	CHECK(scene.zone->GetCreature(20001) == pNear);
	CHECK(scene.zone->GetCreature(9000) == pOwn);
	me.PacketMoveOK(10, 15, DIRECTION_LEFT);
	CHECK(scene.zone->GetCreature(20001) == NULL);
}

TEST(Player, ItSeesAdamsCloudsInTheNorthOfZone74)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.SetShowAdamCloud(true);
	me.PacketMoveOK(20, 10, DIRECTION_UP);
	CHECK(!me.IsShowAdamCloud());
	scene.zone->SetCurrentZoneID(74);
	me.PacketMoveOK(20, 10, DIRECTION_UP);
	CHECK(me.IsShowAdamCloud());
	scene.zone.reset(new MZone(40, 70));
	g_pZone = scene.zone.get();
	me.SetZone(g_pZone);
	scene.zone->SetCurrentZoneID(74);
	me.PacketMoveOK(20, 59, DIRECTION_UP);
	CHECK(me.IsShowAdamCloud());
	me.PacketMoveOK(20, 60, DIRECTION_DOWN);
	CHECK(!me.IsShowAdamCloud());
}

//----------------------------------------------------------------------
// Life and death
//----------------------------------------------------------------------
MEvent Event(EVENT_ID id)
{
	MEvent event;
	event.eventID = id;
	event.eventFlag = 0;
	event.eventDelay = -1;
	event.eventType = EVENTTYPE_NULL;
	return event;
}

TEST(Player, DyingItDropsWhatItWasDoingAndWaitsAtTheServersCell)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	testfw::MutableRow(scene.creatureTypes, 5).DeadActionInfo = SKILL_DOUBLE_IMPACT;
	scene.actions.GetMutable(SKILL_DOUBLE_IMPACT)->Set("", ACTION_DIE, EFFECTSPRITETYPE_NULL, 0, 0);
	me.SetStatus(MODIFY_CURRENT_HP, 100);
	me.AddEffectStatus(EFFECTSTATUS_BLOOD_DRAIN, 0xFFFF);
	me.AddEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3, 0xFFFF);
	me.AddEffectStatus(EFFECTSTATUS_BERSERKER, 0xFFFF);
	me.AddEffectStatus(EFFECTSTATUS_POISON, 0xFFFF);
	g_pUserInformation->LogoutTime = g_FrameNow + MonotonicClock::Millis(5000);
	MEvent logout = Event(EVENTID_LOGOUT), loveChain = Event(EVENTID_LOVECHAIN), metamorph = Event(EVENTID_RESURRECT);
	scene.events.AddEvent(logout);
	scene.events.AddEvent(loveChain);
	scene.events.AddEvent(metamorph);
	me.SetWaitVerify(MPlayer::WAIT_VERIFY_ITEM_USE);
	me.SetRepeatAction();
	Go(me, 23, 15);
	Frames(me, 13);
	CHECK_EQ(22, int(me.GetX()));
	calls.clear();
	me.SetDead();
	CHECK(me.IsDead());
	CHECK(InOrder({ "finish level up", "lock item", "lock gear", "lock trade", "close waypoints", "help|" + N(HELP_EVENT_DIE) }));
	// Where the server has it.
	CHECK_EQ(20, int(me.GetX()));
	CHECK_EQ(15, int(me.GetY()));
	CHECK(me.m_listDirection.empty());
	// Its death throes, aimed at itself.
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(me.m_nUsedActionInfo));
	CHECK_EQ(ACTION_DIE, int(me.m_Action));
	CHECK_EQ(77, int(me.GetTraceID()));
	CHECK_EQ(0, int(me.GetStatus(MODIFY_CURRENT_HP)));
	CHECK(me.m_DeadDelayTime == g_FrameNow + MonotonicClock::Duration(g_pClientConfig->DELAY_PLAYER_DEAD));
	CHECK(!g_pUserInformation->IsLogoutScheduled());
	CHECK(scene.events.GetEvent(EVENTID_LOGOUT) == NULL);
	CHECK(scene.events.GetEvent(EVENTID_LOVECHAIN) == NULL);
	CHECK(scene.events.GetEvent(EVENTID_RESURRECT) != NULL);
	CHECK(me.IsWaitVerifyNULL());
	CHECK(!me.IsRepeatAction());
	// Its statuses are taken off, but a drain and a siege mark, which the
	// attached effects' clearing after it takes all the same.
	CHECK_EQ(1, Count("ui status off|" + N(EFFECTSTATUS_BERSERKER)));
	CHECK_EQ(1, Count("ui status off|" + N(EFFECTSTATUS_POISON)));
	CHECK_EQ(0, Count("ui status off|" + N(EFFECTSTATUS_BLOOD_DRAIN)));
	CHECK_EQ(0, Count("ui status off|" + N(EFFECTSTATUS_SIEGE_ATTACKER_3)));
	CHECK(!me.HasEffectStatus(EFFECTSTATUS_BERSERKER));
	CHECK(!me.HasEffectStatus(EFFECTSTATUS_BLOOD_DRAIN));
	CHECK(!me.HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3));
	// Dead already: nothing more.
	calls.clear();
	me.SetDead();
	CHECK(calls.empty());
}

TEST(Player, BackToLifeItUnlocksItsThingsAndStands)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.SetDead();
	calls.clear();
	me.SetAlive();
	CHECK(me.IsAlive());
	// (Standing up from its death throes unlocks the gear once more.)
	CHECK(Same({ "unlock item", "unlock gear", "unlock trade", "unlock gear", "fade end" }));
	CHECK_EQ(ACTION_STAND, int(me.m_Action));
	// A flier rises to its height.
	testfw::MutableRow(scene.creatureTypes, 5).FlyingHeight = 30;
	me.SetMoveType(MCreature::CREATURE_FLYING);
	me.SetDead();
	CHECK_EQ(0, int(me.m_Z));
	me.SetAlive();
	CHECK_EQ(30, int(me.m_Z));
}

//----------------------------------------------------------------------
// What it is under
//----------------------------------------------------------------------
TEST(Player, TheUIShowsEachStatusButASlayersCriticalWounds)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	me.AddEffectStatus(EFFECTSTATUS_POISON, 50);
	CHECK(me.HasEffectStatus(EFFECTSTATUS_POISON));
	// (The wounds stop it, which unlocks the gear.)
	me.AddEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS, 40);
	// A pass to the GDR lair lasts ten times as long.
	me.AddEffectStatus(EFFECTSTATUS_CAN_ENTER_GDR_LAIR, 7);
	CHECK(Same({ "ui status|" + N(EFFECTSTATUS_POISON) + "|50", "unlock gear", "ui status|" + N(EFFECTSTATUS_CAN_ENTER_GDR_LAIR) + "|70" }));
	me.SetCreatureType(6);
	calls.clear();
	me.AddEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS, 40);
	CHECK(Same({ "unlock gear", "ui status|" + N(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS) + "|40" }));
	// Taken off, had or not, the UI is told.
	calls.clear();
	me.RemoveEffectStatus(EFFECTSTATUS_POISON);
	CHECK(!me.HasEffectStatus(EFFECTSTATUS_POISON));
	CHECK(!me.RemoveEffectStatus(EFFECTSTATUS_BERSERKER));
	CHECK(Same({ "ui status off|" + N(EFFECTSTATUS_POISON), "ui status off|" + N(EFFECTSTATUS_BERSERKER) }));
}

TEST(Player, ADrainCountsItsSlayerDownToConversion)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	// Past any blink an earlier run left in UpdateConversionTime's static.
	static int runs = 0;
	g_FrameNow += MonotonicClock::Duration(++runs * 24LL * 60 * 60 * 1000);
	// 16 frames a second: 160 frames are ten seconds.
	me.AddEffectStatus(EFFECTSTATUS_BLOOD_DRAIN, 160);
	CHECK(InOrder({ "help|" + N(HELP_EVENT_DRAIN_BLOOD), "ui status|" + N(EFFECTSTATUS_BLOOD_DRAIN) + "|160" }));
	CHECK(me.GetConversionDelayTime() == g_FrameNow + MonotonicClock::Millis(10000));
	// The minutes left, and a red blink, sooner the closer it gets.
	me.SetConversionDelay(3 * 60 * 60 * 1000 + 1000);
	calls.clear();
	me.UpdateConversionTime();
	CHECK(Same({ "vampire time|180", "fade|25,31,2|31,0,0|0" }));
	g_FrameNow += MonotonicClock::Millis(4 * 60 * 1000);
	calls.clear();
	me.UpdateConversionTime();
	CHECK(Same({ "vampire time|176" }));
	g_FrameNow += MonotonicClock::Millis(1);
	calls.clear();
	me.UpdateConversionTime();
	CHECK(Same({ "vampire time|176", "fade|25,31,2|31,0,0|0" }));
	// Under ten seconds: every 0.3 seconds.
	g_FrameNow += MonotonicClock::Millis(5 * 60 * 1000);
	me.SetConversionDelay(9000);
	me.UpdateConversionTime();
	g_FrameNow += MonotonicClock::Millis(300);
	calls.clear();
	me.UpdateConversionTime();
	CHECK(Same({ "vampire time|0" }));
	g_FrameNow += MonotonicClock::Millis(1);
	calls.clear();
	me.UpdateConversionTime();
	CHECK(Same({ "vampire time|0", "fade|25,31,2|31,0,0|0" }));
	// Run out: cleared.
	g_FrameNow += MonotonicClock::Millis(9000);
	calls.clear();
	me.UpdateConversionTime();
	CHECK(Same({ "vampire time|0" }));
	CHECK(me.GetConversionDelayTime() == MonotonicClock::TimePoint());
	// The drain's end stops it and redraws the HP bar.
	me.SetStatus(MODIFY_MAX_HP, 90);
	me.SetStatus(MODIFY_CURRENT_HP, 40);
	me.SetConversionDelay(60000);
	calls.clear();
	me.RemoveEffectStatus(EFFECTSTATUS_BLOOD_DRAIN);
	CHECK(Same({ "ui status off|" + N(EFFECTSTATUS_BLOOD_DRAIN), "hp bar|40/90" }));
	CHECK(me.GetConversionDelayTime() == MonotonicClock::TimePoint());
	// A vampire has nothing to count down.
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	me.SetCreatureType(6);
	me.SetConversionDelay(60000);
	calls.clear();
	me.UpdateConversionTime();
	CHECK(calls.empty());
}

TEST(Player, ATurretHoldsItAndOffersOnlyItsFire)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.AddEffectStatus(EFFECTSTATUS_INSTALL_TURRET, 100);
	CHECK(Same({ "unselect skill", "unlock gear", "ui status|" + N(EFFECTSTATUS_INSTALL_TURRET) + "|100", "select skill|" + N(SKILL_TURRET_FIRE) }));
	CHECK(!me.IsNotDelay());
	CHECK(me.m_DelayTime == g_FrameNow + MonotonicClock::Millis(1000));
	g_FrameNow += MonotonicClock::Millis(1000);
	CHECK(me.IsNotDelay());
	calls.clear();
	me.RemoveEffectStatus(EFFECTSTATUS_INSTALL_TURRET);
	CHECK(Same({ "unlock gear", "ui status off|" + N(EFFECTSTATUS_INSTALL_TURRET),
		"skill|" + N(RESULT_SKILL_INSTALL_TURRET) + "|20,15,0|0|77|20,15,0|0|none|first|0,0" }));
	CHECK(!me.IsNotDelay());
}

TEST(Player, OutOfAComaItStopsWaitingToBeRaised)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.temp.SetMode(TempInformation::MODE_WAIT_RESURRECT);
	me.SetWaitVerify(MPlayer::WAIT_VERIFY_RESURRECT);
	me.AddEffectStatus(EFFECTSTATUS_COMA, 100);
	calls.clear();
	me.RemoveEffectStatus(EFFECTSTATUS_COMA);
	// (Out of a coma is alive again.)
	CHECK(Same({ "unlock item", "unlock gear", "unlock trade", "unlock gear", "fade end",
		"ui status off|" + N(EFFECTSTATUS_COMA), "finish die", "finish resurrect", "close shrine minimap" }));
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(me.IsWaitVerifyNULL());
	// Waiting for something else, it waits on.
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	me.SetWaitVerify(MPlayer::WAIT_VERIFY_ITEM_USE);
	me.RemoveEffectStatus(EFFECTSTATUS_COMA);
	CHECK(scene.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_ITEM_USE);
}

TEST(Player, AGhostFliesAndASylphCarriesIt)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	me.AddEffectStatus(EFFECTSTATUS_GHOST, 100);
	CHECK(me.IsFlyingCreature());
	me.RemoveEffectStatus(EFFECTSTATUS_GHOST);
	CHECK(me.IsGroundCreature());
	// A bat stays in the air.
	testfw::MutableRow(scene.creatureTypes, 5).bFlyingCreature = true;
	me.AddEffectStatus(EFFECTSTATUS_GHOST, 100);
	me.RemoveEffectStatus(EFFECTSTATUS_GHOST);
	CHECK(me.IsFlyingCreature());
	me.SetCreatureType(7);
	me.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_GREEN, 100);
	CHECK_EQ(int(MCreature::MOVE_DEVICE_SUMMON_SYLPH), int(me.GetMoveDevice()));
	me.SetWaitVerify(MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF);
	me.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_GREEN);
	CHECK_EQ(int(MCreature::MOVE_DEVICE_WALK), int(me.GetMoveDevice()));
	CHECK(me.IsWaitVerifyNULL());
}

//----------------------------------------------------------------------
// Attacking
//----------------------------------------------------------------------
// A packet as its id and the little-endian bytes of its fields.
std::string Bytes(int id, std::initializer_list<std::pair<unsigned long long, int>> fields)
{
	std::string text = N(id) + ":";
	char hex[3];
	for (const auto& field : fields)
		for (int i = 0; i < field.second; ++i)
		{
			std::snprintf(hex, sizeof(hex), "%02x", int((field.first >> (8 * i)) & 0xFF));
			text += hex;
		}
	return text;
}

// A CGAttack: the target, then the cell it strikes from and its facing.
std::string Attack(TYPE_OBJECTID id, int x, int y, int direction)
{
	return Bytes(Packet::PACKET_CG_ATTACK, { { id, 4 }, { x, 1 }, { y, 1 }, { direction, 1 } });
}

TEST(Player, ItWalksUpToAnEnemyAndStrikesItOnce)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	CHECK(scene.zone->AddCreature(Make(20000, 23, 15, 6)));
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	CHECK(me.IsTraceCreature());
	CHECK_EQ(20000, int(me.GetTraceID()));
	CHECK_EQ(1, int(me.m_TraceDistance));
	Frames(me, 23);
	CHECK_EQ(22, int(me.GetX()));
	CHECK(sent == (std::vector<std::string>{ Move(DIRECTION_RIGHT, 20, 15), Move(DIRECTION_RIGHT, 21, 15) }));
	CHECK_EQ(ACTION_ATTACK, int(me.m_Action));
	// The blow is sent as the attack starts, and once.
	Frames(me, 1);
	CHECK(sent.size() == 3 && sent[2] == Attack(20000, 22, 15, DIRECTION_RIGHT));
	Frames(me, 40);
	CHECK_EQ(3, int(sent.size()));
	CHECK(me.IsTraceNULL());
	CHECK_EQ(ACTION_STAND, int(me.m_Action));
}

TEST(Player, AFriendIsFollowedAndAForcedAttackStrikesIt)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	CHECK(scene.zone->AddCreature(Make(20000, 23, 15, 5)));
	// (Following answers false: there is no blow to repeat.)
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	CHECK(me.IsTraceCreature());
	Frames(me, 60);
	// Beside it, it keeps following and strikes nothing.
	CHECK_EQ(22, int(me.GetX()));
	CHECK_EQ(2, int(sent.size()));
	CHECK(me.IsTraceCreature());
	// It moves on; so does the follower.
	g_pZone->MoveGroundCreature(g_pZone->GetCreature(20000), 23, 15, 25, 15);
	g_pZone->GetCreature(20000)->SetPosition(25, 15);
	Frames(me, 30);
	CHECK_EQ(24, int(me.GetX()));
	// Forced (a held key), the friend is struck.
	me.SetStop();
	sent.clear();
	CHECK(me.TraceCreatureToBasicAction(20000, true, true));
	Frames(me, 3);
	CHECK(sent == std::vector<std::string>{ Attack(20000, 24, 15, DIRECTION_RIGHT) });
	// A user of the self-defence roster is an enemy too.
	scene.justice.AddCreature("rival");
	MCreature* pRival = Make(20001, 24, 16, 5);
	pRival->SetName("rival");
	CHECK(scene.zone->AddCreature(pRival));
	Frames(me, 40);
	me.SetStop();
	sent.clear();
	CHECK(me.TraceCreatureToBasicAction(20001, false, true));
	Frames(me, 3);
	CHECK(sent == std::vector<std::string>{ Attack(20001, 24, 15, DIRECTION_DOWN) });
}

TEST(Player, ItAttacksNothingItCannotSeeOrReach)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	MCreature* pEnemy = Make(20000, 23, 15, 6);
	CHECK(scene.zone->AddCreature(pEnemy));
	// Not there.
	CHECK(!me.TraceCreatureToBasicAction(20002, false, true));
	// Dead.
	pEnemy->SetDead();
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	pEnemy->SetAlive();
	// In darkness, to a slayer without light.
	scene.Sector(23, 15).SetDarkness();
	pEnemy->CheckInDarkness();
	CHECK(pEnemy->IsInDarkness());
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	me.AddEffectStatus(EFFECTSTATUS_LIGHTNESS, 0xFFFF);
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	me.SetStop();
	me.TraceNULL();
	// Asleep or with its head blocked, still waiting, or with no blow.
	me.AddEffectStatus(EFFECTSTATUS_SLEEP, 0xFFFF);
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	me.RemoveEffectStatus(EFFECTSTATUS_SLEEP);
	me.AddEffectStatus(EFFECTSTATUS_BLOCK_HEAD, 0xFFFF);
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	me.RemoveEffectStatus(EFFECTSTATUS_BLOCK_HEAD);
	me.SetDelay(500);
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	g_FrameNow += MonotonicClock::Millis(500);
	me.SetBasicActionInfo(ACTIONINFO_NULL);
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	me.SetBasicActionInfo(SKILL_ATTACK_MELEE);
	// In the middle of another skill.
	me.m_nUsedActionInfo = SKILL_DOUBLE_IMPACT;
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	me.m_nUsedActionInfo = ACTIONINFO_NULL;
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	CHECK(sent.size() <= 1);
}

TEST(Player, InItsSafeZoneItLeavesOneOfItsOwnOutsideAlone)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.PlayerOnly(5);
	scene.Tribe(9);
	testfw::MutableRow(scene.creatureTypes, 9).SpriteTypes.Init(1);
	testfw::MutableRow(scene.creatureTypes, 9).SpriteTypes.Set(0, 1);
	scene.sprites.GetMutable(1)->CreatureType = FLAG_CREATURESPRITE_MONSTER;
	CHECK(scene.zone->AddCreature(Make(20000, 23, 15, 5)));
	scene.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER);
	me.SetRepeatAction();
	CHECK(!me.TraceCreatureToBasicAction(20000, true, true));
	CHECK(!me.IsRepeatAction());
	// Both in it, or the other not a player: as anywhere.
	scene.Sector(23, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER);
	CHECK(me.TraceCreatureToBasicAction(20000, true, true));
	me.SetStop();
	me.TraceNULL();
	CHECK(scene.zone->AddCreature(Make(20001, 17, 15, 9)));
	CHECK(me.TraceCreatureToBasicAction(20001, true, true));
}

TEST(Player, ItGreetsAnNPCFromFourCellsAway)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(8, CREATURETRIBE_NPC);
	scene.PlayerOnly(5);
	CHECK(scene.zone->AddCreature(Make(30000, 27, 15, 8)));
	CHECK(me.TraceCreatureToBasicAction(30000, false, true));
	CHECK_EQ(4, int(me.m_TraceDistance));
	Frames(me, 50);
	CHECK_EQ(23, int(me.GetX()));
	CHECK_EQ(4, int(sent.size()));
	CHECK(sent.back() == Bytes(Packet::PACKET_CG_NPC_TALK, { { 30000, 4 } }));
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_NPC_ASK);
	CHECK(me.IsTraceNULL());
	// Waiting already: no second greeting.
	me.SetStop();
	sent.clear();
	CHECK(me.TraceCreatureToBasicAction(30000, false, true));
	Frames(me, 5);
	CHECK(sent.empty());
}

//----------------------------------------------------------------------
// Picking things up
//----------------------------------------------------------------------
// An item of the class on the ground at the cell.
MItem* Drop(Scene& scene, ITEM_CLASS itemClass, TYPE_OBJECTID id, int x, int y)
{
	if ((*g_pItemTable)[itemClass].GetSize() == 0)
	{
		g_pItemTable->InitClass(itemClass, 1);
		testfw::MutableRow(*g_pItemTable, itemClass, 0).SetGrid(1, 2);
		testfw::MutableRow(*g_pItemTable, itemClass, 0).SetDropFrameID(3);
		testfw::MutableRow(*g_pItemTable, itemClass, 0).Race = itemClass == ITEM_CLASS_VAMPIRE_RING ? FLAG_RACE_VAMPIRE : FLAG_RACE_SLAYER;
	}
	MItem* pItem = NewItemOfClass(itemClass);
	pItem->SetItemType(0);
	pItem->SetID(id);
	pItem->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	CHECK(scene.zone->AddItem(pItem));
	return pItem;
}

TEST(Player, ItPicksAnItemBesideItIntoItsInventory)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	MItem* pSword = Drop(scene, ITEM_CLASS_SWORD, 501, 21, 16);
	CHECK(me.TraceItem(501));
	CHECK(sent == std::vector<std::string>{ Bytes(Packet::PACKET_CG_ADD_ZONE_TO_INVENTORY, { { 501, 4 }, { 21, 1 }, { 16, 1 }, { 0, 1 }, { 0, 1 } }) });
	CHECK(me.GetItemCheckBuffer() == pSword);
	CHECK(me.IsItemCheckBufferPickupToInventory());
	CHECK_EQ(0, int(pSword->GetGridX()));
	CHECK(me.IsTraceNULL());
	// Unconfirmed, it picks up nothing more.
	Drop(scene, ITEM_CLASS_SWORD, 502, 19, 15);
	CHECK(me.TraceItem(502));
	CHECK_EQ(1, int(sent.size()));
}

TEST(Player, ItWalksToAnItemAndPicksItUp)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	Drop(scene, ITEM_CLASS_SWORD, 501, 24, 15);
	CHECK(me.TraceItem(501));
	CHECK(me.IsTraceItem());
	Frames(me, 40);
	CHECK_EQ(23, int(me.GetX()));
	CHECK(sent == (std::vector<std::string>{ Move(DIRECTION_RIGHT, 20, 15), Move(DIRECTION_RIGHT, 21, 15), Move(DIRECTION_RIGHT, 22, 15),
		Bytes(Packet::PACKET_CG_ADD_ZONE_TO_INVENTORY, { { 501, 4 }, { 24, 1 }, { 15, 1 }, { 0, 1 }, { 0, 1 } }) }));
	// Gone before it got there: it stops at the next cell, its trace
	// left set (the step's end asks for the item again and is refused).
	me.ClearItemCheckBuffer();
	sent.clear();
	Drop(scene, ITEM_CLASS_SWORD, 503, 27, 15);
	CHECK(me.TraceItem(503));
	Frames(me, 2);
	CHECK(scene.zone->RemoveItem(503));
	Frames(me, 40);
	CHECK_EQ(1, int(sent.size()));
	CHECK_EQ(24, int(me.GetX()));
	CHECK(me.IsStop());
	CHECK(me.IsTraceItem());
}

TEST(Player, WhatDoesNotFitOrIsNotItsRacesStaysOnTheGround)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.inventory.Release();
	scene.inventory.Init(1, 1);
	MItem* pSword = Drop(scene, ITEM_CLASS_SWORD, 501, 21, 15);
	CHECK(me.TraceItem(501));
	CHECK(sent.empty());
	CHECK(pSword->IsDropping());
	CHECK(Same({ "help|" + N(HELP_EVENT_STORAGE_BUY) }));
	CHECK(me.IsItemCheckBufferNULL());
	scene.inventory.Release();
	scene.inventory.Init(10, 6);
	calls.clear();
	MItem* pClaw = Drop(scene, ITEM_CLASS_VAMPIRE_RING, 502, 19, 15);
	CHECK(me.TraceItem(502));
	CHECK(sent.empty());
	CHECK(pClaw->IsDropping());
}

TEST(Player, ItPicksUpMoneyAndToItsHandWithItsBagOpen)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	MItem* pMoney = Drop(scene, ITEM_CLASS_MONEY, 601, 21, 15);
	CHECK(me.TraceItem(601));
	CHECK(sent == std::vector<std::string>{ Bytes(Packet::PACKET_CG_PICKUP_MONEY, { { 601, 4 }, { 21, 1 }, { 15, 1 } }) });
	CHECK(me.GetItemCheckBuffer() == pMoney);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_PICKUP_MONEY), int(me.GetItemCheckBufferStatus()));
	me.ClearItemCheckBuffer();
	sent.clear();
	inventoryOpen = true;
	Drop(scene, ITEM_CLASS_SWORD, 501, 20, 16);
	CHECK(me.TraceItem(501));
	CHECK(sent == std::vector<std::string>{ Bytes(Packet::PACKET_CG_ADD_ZONE_TO_MOUSE, { { 501, 4 }, { 20, 1 }, { 16, 1 } }) });
	CHECK(me.IsItemCheckBufferPickupToMouse());
	// Something in its hand already: nothing.
	me.ClearItemCheckBuffer();
	sent.clear();
	pickedUp = pMoney;
	CHECK(me.TraceItem(501));
	CHECK(sent.empty());
}

TEST(Player, ASlayerMountsAMotorcycleItReaches)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	Drop(scene, ITEM_CLASS_MOTORCYCLE, 701, 21, 14);
	CHECK(me.TraceItem(701));
	CHECK(sent == std::vector<std::string>{ Bytes(Packet::PACKET_CG_RIDE_MOTORCYCLE, { { 701, 4 }, { 21, 1 }, { 14, 1 } }) });
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_MOTORCYCLE_GETON);
	// Riding already: not another.
	sent.clear();
	me.SetMoveDevice(MCreature::MOVE_DEVICE_RIDE);
	CHECK(me.TraceItem(701));
	CHECK(sent.empty());
	// Not a vampire.
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	me.SetCreatureType(6);
	me.SetMoveDevice(MCreature::MOVE_DEVICE_WALK);
	CHECK(me.TraceItem(701));
	CHECK(sent.empty());
}

TEST(Player, ItReachesForNothingHiddenOrWhileBusy)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	Drop(scene, ITEM_CLASS_SWORD, 501, 21, 15);
	CHECK(!me.TraceItem(999));
	scene.Sector(21, 15).SetDarkness();
	CHECK(!me.TraceItem(501));
	me.AddEffectStatus(EFFECTSTATUS_LIGHTNESS, 0xFFFF);
	me.SetDelay(100);
	CHECK(!me.TraceItem(501));
	g_FrameNow += MonotonicClock::Millis(100);
	me.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH, 0xFFFF);
	CHECK(me.TraceItem(501));
	CHECK(sent.empty());
	me.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH);
	CHECK(me.TraceItem(501));
	CHECK_EQ(1, int(sent.size()));
	// Mid-swing it waits its turn.
	me.ClearItemCheckBuffer();
	me.SetAction(ACTION_ATTACK);
	CHECK(!me.IsStop());
	CHECK(!me.TraceItem(501));
}

//----------------------------------------------------------------------
// Skills
//----------------------------------------------------------------------
// The id the next effect target will take.
int NextEffectID()
{
	MEffectTarget probe(0);
	probe.NewEffectID();
	return BYTE(probe.GetEffectID() + 1);
}

std::string SkillToSelf(int skill, int effectID)
{
	return Bytes(Packet::PACKET_CG_SKILL_TO_SELF, { { unsigned(skill), 2 }, { unsigned(effectID), 2 } });
}

std::string SkillToObject(int skill, int effectID, TYPE_OBJECTID target)
{
	return Bytes(Packet::PACKET_CG_SKILL_TO_OBJECT, { { unsigned(skill), 2 }, { unsigned(effectID), 2 }, { target, 4 } });
}

std::string SkillToTile(int skill, int effectID, int x, int y)
{
	return Bytes(Packet::PACKET_CG_SKILL_TO_TILE, { { unsigned(skill), 2 }, { unsigned(effectID), 2 }, { unsigned(x), 1 }, { unsigned(y), 1 } });
}

TEST(Player, ASkillOnItselfGoesOutAtOnceAndWaitsForTheServer)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(MAGIC_LIGHT, FLAG_ACTIONINFO_TARGET_SELF, 0, ACTIONINFO_PACKET_SELF);
	me.SetSpecialActionInfo(MAGIC_LIGHT);
	const int effectID = NextEffectID();
	CHECK(me.SelfSpecialAction());
	// The first frame starts the cast; the second sends it.
	Frames(me, 1);
	CHECK_EQ(ACTION_MAGIC, int(me.m_Action));
	CHECK(sent.empty());
	Frames(me, 1);
	CHECK(sent == std::vector<std::string>{ SkillToSelf(MAGIC_LIGHT, effectID) });
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_SKILL_SUCCESS);
	CHECK_EQ(int(MAGIC_LIGHT), int(me.GetWaitVerifyActionInfo()));
	// Once.
	Frames(me, 30);
	CHECK_EQ(1, int(sent.size()));
}

TEST(Player, ASkillItLacksOrCannotUseYetIsRefused)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(MAGIC_LIGHT, FLAG_ACTIONINFO_TARGET_SELF, 0, ACTIONINFO_PACKET_SELF);
	me.SetSpecialActionInfo(MAGIC_LIGHT);
	me.SetRepeatAction();
	scene.skillInfo.GetMutable(MAGIC_LIGHT)->SetPassive();
	CHECK(!me.SelfSpecialAction());
	CHECK(!me.IsRepeatAction());
	scene.skillInfo.GetMutable(MAGIC_LIGHT)->UnSetPassive();
	scene.skillInfo.GetMutable(MAGIC_LIGHT)->SetEnable(false);
	CHECK(!me.SelfSpecialAction());
	scene.skillInfo.GetMutable(MAGIC_LIGHT)->SetEnable(true);
	scene.skills.RemoveSkill(ACTIONINFO(MAGIC_LIGHT));
	CHECK(!me.SelfSpecialAction());
	scene.skills.AddSkill(ACTIONINFO(MAGIC_LIGHT));
	// Asleep, waiting, riding, in a sanctuary for all, or with nothing chosen.
	me.AddEffectStatus(EFFECTSTATUS_SLEEP, 0xFFFF);
	CHECK(!me.SelfSpecialAction());
	me.RemoveEffectStatus(EFFECTSTATUS_SLEEP);
	me.SetDelay(10);
	CHECK(!me.SelfSpecialAction());
	g_FrameNow += MonotonicClock::Millis(10);
	me.SetMoveDevice(MCreature::MOVE_DEVICE_RIDE);
	CHECK(!me.SelfSpecialAction());
	me.SetMoveDevice(MCreature::MOVE_DEVICE_WALK);
	scene.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_COMMON);
	CHECK(!me.SelfSpecialAction());
	scene.Sector(20, 15).SetSafe(0);
	// (No skill is no choice: the one chosen stays.)
	me.SetSpecialActionInfo(ACTIONINFO_NULL);
	CHECK_EQ(int(MAGIC_LIGHT), int(me.GetSpecialActionInfo()));
	// Not a skill on itself.
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 3, ACTIONINFO_PACKET_OTHER);
	me.SetSpecialActionInfo(SKILL_DOUBLE_IMPACT);
	CHECK(!me.SelfSpecialAction());
	CHECK(sent.empty());
	// A result's info is no skill to choose, the first of them neither.
	me.SetSpecialActionInfo(RESULT_MAGIC_LIGHT);
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(me.GetSpecialActionInfo()));
	me.SetSpecialActionInfo(MIN_RESULT_ACTIONINFO);
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(me.GetSpecialActionInfo()));
	me.SetSpecialActionInfo(MIN_RESULT_ACTIONINFO - 1);
	CHECK_EQ(int(MIN_RESULT_ACTIONINFO - 1), int(me.GetSpecialActionInfo()));
}

TEST(Player, ItClosesToItsSkillsRangeAndUsesItOnTheCreature)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 3, ACTIONINFO_PACKET_OTHER);
	CHECK(scene.zone->AddCreature(Make(20000, 26, 15, 6)));
	me.SetSpecialActionInfo(SKILL_DOUBLE_IMPACT);
	const int effectID = NextEffectID();
	CHECK(me.TraceCreatureToSpecialAction(20000, false));
	CHECK_EQ(3, int(me.m_TraceDistance));
	Frames(me, 80);
	CHECK_EQ(23, int(me.GetX()));
	CHECK_EQ(4, int(sent.size()));
	CHECK(sent.back() == SkillToObject(SKILL_DOUBLE_IMPACT, effectID, 20000));
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_SKILL_SUCCESS);
}

TEST(Player, ItClosesToItsSkillsRangeAndUsesItOnTheCell)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2, ACTIONINFO_PACKET_ZONE);
	me.SetSpecialActionInfo(SKILL_SNAKE_COMBO);
	const int effectID = NextEffectID();
	CHECK(me.TraceSectorToSpecialAction(15, 15));
	CHECK(me.IsTraceSector());
	Frames(me, 80);
	CHECK_EQ(17, int(me.GetX()));
	CHECK_EQ(4, int(sent.size()));
	CHECK(sent.back() == SkillToTile(SKILL_SNAKE_COMBO, effectID, 15, 15));
	// Nowhere, or in a safe zone: refused.
	me.SetWaitVerifyNULL();
	Frames(me, 40);
	sent.clear();
	CHECK(!me.TraceSectorToSpecialAction(SECTORPOSITION_NULL, 15));
	// A safe zone refuses it, the button held all the same; a cell safe
	// for all lets go of the button too.
	me.SetRepeatAction();
	g_bZoneSafe = true;
	CHECK(!me.TraceSectorToSpecialAction(16, 15));
	CHECK(me.IsRepeatAction());
	g_bZoneSafe = false;
	scene.Sector(me.GetX(), me.GetY()).SetSafe(FLAG_SECTOR_SAFE_COMMON);
	CHECK(!me.TraceSectorToSpecialAction(16, 15));
	CHECK(!me.IsRepeatAction());
	CHECK(sent.empty());
}

//----------------------------------------------------------------------
// Requests to another player
//----------------------------------------------------------------------
TEST(Player, ItOffersATradeToOneOfItsOwnRaceTwoCellsAway)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.PlayerOnly(5);
	MCreature* pOther = Make(20000, 25, 15, 5);
	pOther->SetName("trader");
	CHECK(scene.zone->AddCreature(pOther));
	viewRequest = MRequestMode::REQUEST_TRADE;
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	CHECK(me.IsRequestMode());
	CHECK_EQ(2, int(me.m_TraceDistance));
	Frames(me, 50);
	CHECK_EQ(23, int(me.GetX()));
	CHECK(sent.back() == Bytes(Packet::PACKET_CG_TRADE_PREPARE, { { 20000, 4 }, { 0, 1 } }));
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_TRADE);
	CHECK(scene.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
	CHECK_EQ(20000, int(scene.temp.Value1));
	CHECK_EQ(1, Count("exchange cancel|trader"));
	CHECK(!me.IsRequestMode());
	// Asked once only.
	const size_t count = sent.size();
	me.SetStop();
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	Frames(me, 20);
	CHECK_EQ(int(count), int(sent.size()));
}

TEST(Player, ItInvitesToAPartyOnlyWhenThePartySystemIsOpen)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.PlayerOnly(5);
	MCreature* pOther = Make(20000, 22, 15, 5);
	pOther->SetName("friend");
	CHECK(scene.zone->AddCreature(pOther));
	viewRequest = MRequestMode::REQUEST_PARTY;
	scene.systems.SetFlag(0);
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	Frames(me, 20);
	CHECK(sent.empty());
	CHECK(me.IsWaitVerifyNULL());
	scene.systems.SetFlag(1);	// bit 0: the party system
	me.SetStop();
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	Frames(me, 20);
	CHECK(sent == std::vector<std::string>{ Bytes(Packet::PACKET_CG_PARTY_INVITE, { { 20000, 4 }, { 0, 1 } }) });
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_PARTY);
	CHECK(scene.temp.GetMode() == TempInformation::MODE_PARTY_REQUEST);
	CHECK_EQ(1, Count("party cancel|friend"));
}

TEST(Player, ItAsksAfterAnotherPlayerOrNamesItInAWhisper)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.PlayerOnly(5);
	MCreature* pOther = Make(20000, 30, 20, 5);
	pOther->SetName("stranger");
	CHECK(scene.zone->AddCreature(pOther));
	viewRequest = MRequestMode::REQUEST_INFO;
	// At once, wherever it is.
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	CHECK(sent == std::vector<std::string>{ Bytes(Packet::PACKET_CG_REQUEST_INFO, { { 0, 1 }, { 20000, 4 } }) });
	CHECK(Same({ "other info|20000" }));
	CHECK(!me.IsRequestMode());
	CHECK(me.IsTraceNULL());
	// A whisper being typed takes its name instead.
	sent.clear();
	calls.clear();
	inputString = "*";
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	CHECK(sent.empty());
	CHECK(Same({ "input|stranger" }));
}

//----------------------------------------------------------------------
// Moved by force
//----------------------------------------------------------------------
TEST(Player, AFastMoveTakesItThereAndDropsWhatItWasDoing)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.SetRepeatAction();
	me.m_nUsedActionInfo = SKILL_DOUBLE_IMPACT;
	me.m_SendMove = 2;
	CHECK(me.FastMovePosition(25, 17, true));
	CHECK(Same({ "unselect sector", "lock gear" }));
	CHECK_EQ(25, int(me.GetServerX()));
	CHECK_EQ(17, int(me.GetServerY()));
	CHECK_EQ(ACTION_STAND, int(me.m_Action));
	CHECK(!me.IsRepeatAction());
	CHECK_EQ(0, int(me.GetSendMove()));
	CHECK_EQ(int(ACTIONINFO_NULL), int(me.m_nUsedActionInfo));
	// The next frame lands it there, at twice a step's pace, unsent.
	CHECK_EQ(20, int(me.GetX()));
	Frames(me, 1);
	CHECK_EQ(25, int(me.GetX()));
	CHECK_EQ(17, int(me.GetY()));
	CHECK_EQ(3, int(me.m_MoveCount));
	Frames(me, 4);
	CHECK(me.IsStop());
	CHECK(sent.empty());
	// Not told by the server: its cell there stays.
	CHECK(me.FastMovePosition(27, 17, false));
	CHECK_EQ(25, int(me.GetServerX()));
	// On a motorcycle it stands as a rider.
	me.m_MoveAction = ACTION_SLAYER_MOTOR_MOVE;
	CHECK(me.FastMovePosition(28, 17, true));
	CHECK_EQ(ACTION_SLAYER_MOTOR_STAND, int(me.m_Action));
}

TEST(Player, AKnockBackForgetsItsMovesAndItsSkill)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.m_nUsedActionInfo = SKILL_DOUBLE_IMPACT;
	me.m_SendMove = 2;
	g_bNetStatusGood = false;
	CHECK(me.KnockBackPosition(22, 15, ACTION_DAMAGED));
	CHECK_EQ(0, int(me.GetSendMove()));
	CHECK(g_bNetStatusGood);
	CHECK_EQ(int(ACTIONINFO_NULL), int(me.m_nUsedActionInfo));
}

TEST(Player, ItFindsAnEnemyWithinReachOfItsButton)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	scene.Tribe(8, CREATURETRIBE_NPC);
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 3, ACTIONINFO_PACKET_OTHER);
	CHECK(scene.zone->AddCreature(Make(20000, 23, 15, 6)));
	CHECK(scene.zone->AddCreature(Make(20001, 21, 15, 5)));
	CHECK(scene.zone->AddCreature(Make(20002, 21, 16, 8)));
	g_bLButtonDown = false;
	g_bRButtonDown = false;
	CHECK_EQ(int(OBJECTID_NULL), int(me.FindEnemy()));
	// The left button reaches as far as its blow: one cell.
	g_bLButtonDown = true;
	CHECK_EQ(int(OBJECTID_NULL), int(me.FindEnemy()));
	me.SetBasicAttackDistance(3);
	CHECK_EQ(20000, int(me.FindEnemy()));
	// The right as far as its skill.
	g_bLButtonDown = false;
	g_bRButtonDown = true;
	me.SetSpecialActionInfo(SKILL_DOUBLE_IMPACT);
	CHECK_EQ(20000, int(me.FindEnemy()));
	scene.skillInfo.GetMutable(SKILL_DOUBLE_IMPACT)->maxRange = 2;
	scene.skillInfo.GetMutable(SKILL_DOUBLE_IMPACT)->minRange = 2;
	CHECK_EQ(int(OBJECTID_NULL), int(me.FindEnemy()));
	g_bRButtonDown = false;
}

//----------------------------------------------------------------------
// Changing race
//----------------------------------------------------------------------
TEST(Player, BecomingAVampireDropsTheSlayersThings)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(CREATURETYPE_SLAYER_MALE);
	scene.Tribe(CREATURETYPE_SLAYER_FEMALE);
	scene.Tribe(CREATURETYPE_VAMPIRE_MALE1, CREATURETRIBE_VAMPIRE);
	scene.Tribe(CREATURETYPE_VAMPIRE_FEMALE1, CREATURETRIBE_VAMPIRE);
	me.SetCreatureType(CREATURETYPE_SLAYER_MALE);
	MMagazine magazine;
	g_pCurrentMagazine = &magazine;
	g_pStorage = new MStorage;
	me.SetConversionDelay(60000);
	me.SetWaitVerify(MPlayer::WAIT_VERIFY_ITEM_USE);
	me.m_SendMove = 2;
	calls.clear();
	CHECK(me.ChangeToVampire());
	CHECK(me.IsVampire());
	CHECK(g_pCurrentMagazine == NULL);
	CHECK(g_pStorage == NULL);
	CHECK(me.GetConversionDelayTime() == MonotonicClock::TimePoint());
	CHECK(me.IsWaitVerifyNULL());
	CHECK_EQ(0, int(me.GetSendMove()));
	CHECK(InOrder({ "fade|1,31,6|31,0,0|0", "interface|" + N(RACE_VAMPIRE) }));
	// A vampire already.
	calls.clear();
	CHECK(!me.ChangeToVampire());
	CHECK(calls.empty());
	// And back, in blue.
	CHECK(me.ChangeToSlayer());
	CHECK(me.IsSlayer());
	CHECK(InOrder({ "fade|1,31,6|5,5,31|0", "interface|" + N(RACE_SLAYER) }));
	CHECK(!me.ChangeToSlayer());
}

TEST(Player, AnOperatorsShapeIsTheUsersToo)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(CREATURETYPE_SLAYER_OPERATOR);
	me.SetCreatureType(CREATURETYPE_SLAYER_OPERATOR);
	CHECK(g_pUserInformation->bCompetenceShape);
	me.SetCreatureType(5);
	CHECK(!g_pUserInformation->bCompetenceShape);
}

//----------------------------------------------------------------------
// What its actions bring about
//----------------------------------------------------------------------
// A result that says when it is carried out.
struct Outcome : MActionResultNode
{
	explicit Outcome(const char* pName) : name(pName) {}
	void Execute() override { calls.push_back(std::string("result|") + name); }
	std::string name;
};

MActionResult* Result(const char* pName)
{
	MActionResult* pResult = new MActionResult;
	pResult->Add(new Outcome(pName));
	return pResult;
}

TEST(Player, AResultWaitsForTheSkillInHandOrComesAtOnce)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	// Nothing in hand: at once.
	CHECK(!me.PacketAddActionResult(0, Result("now")));
	CHECK(Same({ "result|now" }));
	CHECK(!me.PacketAddActionResult(0, NULL));
	// A skill under way: on its target, the one held before carried out.
	MEffectTarget* pTarget = new MEffectTarget(0);
	me.AddEffectTarget(pTarget);
	me.m_nUsedActionInfo = SKILL_DOUBLE_IMPACT;
	calls.clear();
	CHECK(me.PacketAddActionResult(0, Result("first")));
	CHECK(calls.empty());
	CHECK(me.PacketAddActionResult(0, Result("second")));
	CHECK(Same({ "result|first" }));
	CHECK(pTarget->IsExistResult());
	// A result's info in hand is no skill: at once.
	me.m_nUsedActionInfo = RESULT_MAGIC_LIGHT;
	calls.clear();
	CHECK(!me.PacketAddActionResult(0, Result("third")));
	CHECK(Same({ "result|third" }));
	me.RemoveEffectTarget(pTarget->GetEffectID());
	delete pTarget;
}

TEST(Player, ItsLightReachesByTimeItemsWingsAndSkills)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.SetTimeLightSight(4);
	CHECK_EQ(4, me.GetLightSight());
	me.SetItemLightSight(2);
	CHECK_EQ(6, me.GetLightSight());
	me.SetFlyingCreature();
	CHECK_EQ(9, me.GetLightSight());
	me.SetGroundCreature();
	CHECK_EQ(6, me.GetLightSight());
	scene.skills.AddSkill(SKILL_HOLYLAND_BLOOD_BIBLE_GREGORI);
	scene.skills.AddSkill(SKILL_SWEEPER_BONUS_12);
	me.CalculateLightSight();
	CHECK_EQ(16, me.GetLightSight());
	// The observing eye adds one, and one more per fifty levels: counted
	// at the next reckoning, not as it opens.
	scene.skills.RemoveSkill(SKILL_HOLYLAND_BLOOD_BIBLE_GREGORI);
	scene.skills.RemoveSkill(SKILL_SWEEPER_BONUS_12);
	me.AddEffectStatus(EFFECTSTATUS_OBSERVING_EYE, 0xFFFF);
	CHECK_EQ(6, me.GetLightSight());
	me.CalculateLightSight();
	CHECK_EQ(7, me.GetLightSight());
}

TEST(Player, ItTellsItsPartyWhereItIsWhenItMoves)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	ClientCommunicationManager peers;
	ClientCommunicationManager* oldPeers = g_pClientCommunicationManager;
	g_pClientCommunicationManager = &peers;
	g_pUserInformation->bKorean = false;
	g_bZonePlayerInLarge = false;
	const int oldSmall = g_nZoneSmall;
	g_nZoneSmall = 61;
	me.SetStatus(MODIFY_MAX_HP, 120);
	me.SetStatus(MODIFY_CURRENT_HP, 80);
	sent.clear();
	// Alone, it tells nobody.
	me.SetServerPosition(31, 9);
	CHECK(sent.empty());
	PARTY_INFO* pMember = new PARTY_INFO;
	pMember->Name = "mate";
	CHECK(scene.party.AddMember(pMember));
	me.SetServerPosition(32, 9);
	CHECK(sent == std::vector<std::string>{ Bytes(Packet::PACKET_CG_PARTY_POSITION, { { 61, 2 }, { 20, 2 }, { 15, 2 }, { 120, 2 }, { 80, 2 } }) });
	// Each time, from the same cell too: only the peer-to-peer report
	// remembers what it sent.
	me.SetServerPosition(32, 9);
	CHECK_EQ(2, int(sent.size()));
	CHECK(sent[1] == sent[0]);
	g_nZoneSmall = oldSmall;
	g_pClientCommunicationManager = oldPeers;
}

//----------------------------------------------------------------------
// Held down
//----------------------------------------------------------------------
TEST(Player, HeldDownItStrikesAgainUntilLetGo)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	CHECK(scene.zone->AddCreature(Make(20000, 21, 15, 6)));
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	me.SetRepeatAction();
	for (int frame = 0; frame < 60; ++frame)
	{
		++g_CurrentFrame;
		me.Action();
	}
	const std::string blow = Attack(20000, 20, 15, DIRECTION_RIGHT);
	CHECK(sent.size() >= 3);
	for (const std::string& packet : sent) CHECK(packet == blow);
	// Let go: the blow under way ends it.
	me.UnSetRepeatAction();
	Frames(me, 30);
	const size_t count = sent.size();
	Frames(me, 30);
	CHECK_EQ(int(count), int(sent.size()));
	CHECK(me.IsStop());
}

TEST(Player, ASkillOnACellAimsItsEffectAtThatCellsPixel)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 5, ACTIONINFO_PACKET_ZONE, true);
	me.SetSpecialActionInfo(SKILL_SNAKE_COMBO);
	CHECK(me.TraceSectorToSpecialAction(17, 12));
	Frames(me, 20);
	CHECK_EQ(1, int(sent.size()));
	CHECK_EQ(1, Count("effect|" + N(SKILL_SNAKE_COMBO) + "|" + N(WorldTileGeometry::TileToPixelX(17)) + "," + N(WorldTileGeometry::TileToPixelY(12))
		+ "|" + N(TYPE_OBJECTID(OBJECTID_NULL))));
}

TEST(Player, HeldDownAGroundSkillFollowsTheCursor)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 5, ACTIONINFO_PACKET_ZONE);
	me.SetSpecialActionInfo(SKILL_SNAKE_COMBO);
	CHECK(me.TraceSectorToSpecialAction(17, 12));
	me.SetRepeatAction();
	// The cursor over a pixel inside cell (22,13).
	cursorPixel.x = WorldTileGeometry::TileToPixelX(22) + 5;
	cursorPixel.y = WorldTileGeometry::TileToPixelY(13) + 3;
	for (int frame = 0; frame < 40 && sent.size() < 2; ++frame)
	{
		++g_CurrentFrame;
		me.Action();
		me.SetWaitVerifyNULL();
	}
	CHECK_EQ(2, int(sent.size()));
	CHECK(sent.size() == 2 && sent[1].substr(sent[1].size() - 4) == "160d");
	CHECK(sent.size() == 2 && sent[0].substr(sent[0].size() - 4) == "110c");
}

TEST(Player, AnUnconfirmedItemLocksItsWindowsUntilCleared)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	MItem* pSword = Drop(scene, ITEM_CLASS_SWORD, 501, 25, 15);
	calls.clear();
	me.SetItemCheckBuffer(pSword, MPlayer::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	CHECK(Same({ "lock item", "lock gear", "lock trade" }));
	CHECK(me.GetItemCheckBuffer() == pSword);
	calls.clear();
	me.ClearItemCheckBuffer();
	CHECK(Same({ "unlock gear", "unlock item", "unlock trade" }));
	CHECK(me.IsItemCheckBufferNULL());
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_NULL), int(me.GetItemCheckBufferStatus()));
	// In a casket the gear stays locked.
	me.m_bInCasket = true;
	me.SetItemCheckBuffer(pSword, MPlayer::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	calls.clear();
	me.ClearItemCheckBuffer();
	CHECK(Same({ "unlock item", "unlock trade" }));
}

TEST(Player, ASkillsRangeIsItsTablesItsWeaponsOrItsOwn)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 4, ACTIONINFO_PACKET_OTHER);
	CHECK_EQ(4, me.GetActionInfoRange(SKILL_DOUBLE_IMPACT));
	// A child skill reaches as its parent.
	scene.actions.GetMutable(SKILL_TRIPLE_SLASHER)->SetParentActionInfo(SKILL_DOUBLE_IMPACT);
	CHECK_EQ(4, me.GetActionInfoRange(SKILL_TRIPLE_SLASHER));
	// As far as the weapon in hand.
	scene.actions.GetMutable(SKILL_DOUBLE_IMPACT)->SetAffectCurrentWeaponRange();
	me.SetBasicAttackDistance(2);
	CHECK_EQ(2, me.GetActionInfoRange(SKILL_DOUBLE_IMPACT));
	CHECK_EQ(3, me.GetActionInfoRange(SKILL_HEAD_SHOT));
	// Rapid gliding: two and one per fifty dexterity, at most six.
	me.MStatus::SetStatus(MODIFY_CURRENT_DEX, 149);
	CHECK_EQ(4, me.GetActionInfoRange(MAGIC_RAPID_GLIDING));
	me.MStatus::SetStatus(MODIFY_CURRENT_DEX, 250);
	CHECK_EQ(6, me.GetActionInfoRange(MAGIC_RAPID_GLIDING));
	me.MStatus::SetStatus(MODIFY_CURRENT_DEX, 400);
	CHECK_EQ(6, me.GetActionInfoRange(MAGIC_RAPID_GLIDING));
	// Bloody zenith: two, a strength per 80, a dexterity per 120, an
	// intelligence per 240, at most six.
	me.MStatus::SetStatus(MODIFY_CURRENT_DEX, 119);
	me.MStatus::SetStatus(MODIFY_CURRENT_STR, 160);
	me.MStatus::SetStatus(MODIFY_CURRENT_INT, 240);
	CHECK_EQ(5, me.GetActionInfoRange(MAGIC_BLOODY_ZENITH));
	me.MStatus::SetStatus(MODIFY_CURRENT_DEX, 120);
	CHECK_EQ(6, me.GetActionInfoRange(MAGIC_BLOODY_ZENITH));
	me.MStatus::SetStatus(MODIFY_CURRENT_STR, 800);
	CHECK_EQ(6, me.GetActionInfoRange(MAGIC_BLOODY_ZENITH));
}

TEST(Player, AWeaponInHandSetsTheReachOfItsBlow)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	g_pItemTable->InitClass(ITEM_CLASS_AR, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_AR, 0).Value5 = 4;
	std::unique_ptr<MItem> gun(NewItemOfClass(ITEM_CLASS_AR));
	gun->SetItemType(0);
	CHECK(me.SetAddonItem(gun.get()));
	CHECK_EQ(4, int(me.GetBasicAttackDistance()));
	me.SetBasicActionInfo(SKILL_DOUBLE_IMPACT);
	CHECK(me.RemoveAddonItem(gun.get()));
	CHECK_EQ(1, int(me.GetBasicAttackDistance()));
	CHECK_EQ(int(SKILL_ATTACK_MELEE), int(me.GetBasicActionInfo()));
}

//----------------------------------------------------------------------
// What the review of #535 found unpinned
//----------------------------------------------------------------------
TEST(Player, InLockModeItTakesTheEnemyInReachItself)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	CHECK(scene.zone->AddCreature(Make(20000, 21, 15, 6)));
	me.SetLockMode();
	g_bLButtonDown = true;
	me.Action();
	CHECK(InOrder({ "select creature|20000", "cursor attack", "unselect sector" }));
	CHECK_EQ(20000, int(me.GetTraceID()));
	// A forced blow at it.
	Frames(me, 2);
	CHECK(sent == std::vector<std::string>{ Attack(20000, 20, 15, DIRECTION_RIGHT) });
	// Nobody in reach: it lets go of all it held.
	scene.zone->RemoveCreature(20000);
	me.SetStop();
	me.SetRepeatAction();
	calls.clear();
	me.Action();
	CHECK_EQ(1, Count("unselect"));
	CHECK_EQ(0, Count("select creature|20000"));
	CHECK(me.IsTraceNULL());
	CHECK(!me.IsRepeatAction());
}

TEST(Player, ASlayerSteppingOnAnElevatorRunsItsWindow)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Sector(21, 15).SetPortal();
	CHECK(scene.Sector(21, 15).AddPortal(MPortal::TYPE_MULTI_PORTAL, 61));
	Go(me, 23, 15);
	Frames(me, 2);
	CHECK_EQ(21, int(me.GetX()));
	CHECK(InOrder({ "elevator", "unselect sector" }));
	// It goes no further.
	Frames(me, 40);
	CHECK_EQ(21, int(me.GetX()));
	// Not a slayer: no elevator.
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	me.SetCreatureType(6);
	calls.clear();
	Go(me, 20, 15);
	Frames(me, 40);
	Go(me, 22, 15);
	Frames(me, 40);
	CHECK_EQ(0, Count("elevator"));
}

TEST(Player, ASoulChainAsksForItsTargetInAWindow)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_SOUL_CHAIN, FLAG_ACTIONINFO_TARGET_SELF, 0, ACTIONINFO_PACKET_SELF);
	me.SetSpecialActionInfo(SKILL_SOUL_CHAIN);
	CHECK(!me.SelfSpecialAction());
	CHECK(Same({ "trace window" }));
	CHECK(sent.empty());
	// Waiting for the server: no window.
	me.SetWaitVerify(MPlayer::WAIT_VERIFY_ITEM_USE);
	calls.clear();
	CHECK(!me.SelfSpecialAction());
	CHECK(calls.empty());
	me.SetCannotTrace();
	CHECK(Same({ "cannot trace" }));
}

TEST(Player, InstallingAMineShowsItsProgressAndAStepEndsIt)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_INSTALL_MINE, FLAG_ACTIONINFO_TARGET_SELF, 0, ACTIONINFO_PACKET_SELF);
	g_pItemTable->InitClass(ITEM_CLASS_MINE, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MINE, 0).SetGrid(1, 1);
	MItem* pMine = NewItemOfClass(ITEM_CLASS_MINE);
	pMine->SetItemType(0);
	pMine->SetID(801);
	CHECK(scene.inventory.AddItem(pMine, 3, 2));
	me.SetSpecialActionInfo(SKILL_INSTALL_MINE);
	me.SetRepeatAction();
	CHECK(!me.SelfSpecialAction());
	CHECK(Same({ "install mine|3,2" }));
	CHECK(!me.IsRepeatAction());
	// No mine in the bag: nothing to show.
	CHECK(scene.inventory.RemoveItem(3, 2) == pMine);
	delete pMine;
	calls.clear();
	CHECK(!me.SelfSpecialAction());
	CHECK(calls.empty());
	// Walking away while it runs ends it.
	mineProgress = true;
	Go(me, 22, 15);
	Frames(me, 2);
	CHECK_EQ(1, Count("end mine"));
}

TEST(Player, APetsWindowShowsWhatItsItemSays)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	g_pItemTable->InitClass(ITEM_CLASS_PET_ITEM, 1);
	MItem* pPet = NewItemOfClass(ITEM_CLASS_PET_ITEM);
	pPet->SetItemType(0);
	pPet->SetID(901);
	pPet->SetNumber(7);
	pPet->SetEnchantLevel(3);
	pPet->SetSilver(12);
	MFakeCreature* pFake = new MFakeCreature;
	pFake->SetZone(g_pZone);
	pFake->SetCreatureType(5);
	pFake->SetID(40000);
	pFake->SetPosition(22, 15);
	pFake->SetOwnerID(77);
	pFake->SetPetItem(static_cast<MPetItem*>(pPet));
	char nick[] = "Rex";
	pFake->SetNickName(0, nick);
	CHECK(scene.zone->AddFakeCreature(pFake));
	CHECK(!me.TraceCreatureToBasicAction(40000, false, true));
	CHECK(Same({ "pet|901|7|3|12|-1|Rex|1" }));
	CHECK(me.IsTraceNULL());
	// Another's pet: its name cannot be changed.
	pFake->SetOwnerID(78);
	calls.clear();
	CHECK(!me.TraceCreatureToBasicAction(40000, false, true));
	CHECK(Same({ "pet|901|7|3|12|-1|Rex|0" }));
}

TEST(Player, ItsBlowSoundsWhereItStandsAndShakesAReadyPad)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	testfw::MutableRow(scene.creatureTypes, 5).SetActionSound(ACTION_ATTACK, 14);
	testfw::MutableRow(scene.creatureTypes, 5).SetActionSound(ACTION_MOVE, 15);
	SOUND_TABLE sounds;
	sounds.Init(20);
	SOUND_TABLE* oldSounds = g_pSoundTable;
	g_pSoundTable = &sounds;
	CHECK(scene.zone->AddCreature(Make(20000, 22, 15, 6)));
	forceFeel = true;
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	Frames(me, 14);
	// A step's sound from the cell it steps to; the blow's from its cell,
	// and on the pad.
	CHECK(InOrder({ "sound at|15|0|21,15", "sound at|14|0|21,15", "force|14" }));
	// A sound past the pad's table is not played on it.
	testfw::MutableRow(scene.creatureTypes, 5).SetActionSound(ACTION_ATTACK, 25);
	Frames(me, 40);
	me.SetStop();
	calls.clear();
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	Frames(me, 3);
	CHECK_EQ(1, Count("sound at|25|0|21,15"));
	CHECK_EQ(0, Count("force|25"));
	g_pSoundTable = oldSounds;
}

TEST(Player, AnEmptyGunClicksAndWaits)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	scene.actions.GetMutable(SKILL_ATTACK_MELEE)->Set("", ACTION_ATTACK, EFFECTSPRITETYPE_NULL, 0, 0, FLAG_ACTIONINFO_START_USER, 400);
	scene.actions.GetMutable(SKILL_ATTACK_MELEE)->SetWeaponType(FLAG_ACTIONINFO_WEAPON_GUN_SR);
	MMagazine magazine;
	magazine.SetNumber(0);
	g_pCurrentMagazine = &magazine;
	CHECK(scene.zone->AddCreature(Make(20000, 22, 15, 6)));
	me.SetRepeatAction();
	CHECK(!me.TraceCreatureToBasicAction(20000, false, true));
	CHECK(Same({ "sound|" + N(SOUND_ITEM_NO_MAGAZINE) }));
	CHECK(!me.IsRepeatAction());
	CHECK(me.m_DelayTime == g_FrameNow + MonotonicClock::Millis(400));
	CHECK(me.IsTraceNULL());
	// Loaded, it attacks.
	magazine.SetNumber(5);
	g_FrameNow += MonotonicClock::Millis(400);
	calls.clear();
	CHECK(me.TraceCreatureToBasicAction(20000, false, true));
	CHECK_EQ(0, Count("sound|" + N(SOUND_ITEM_NO_MAGAZINE)));
	g_pCurrentMagazine = NULL;
}

TEST(Player, TheHornOfTheEarthSpiritOpensItsWindow)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	scene.PlayerOnly(5);
	scene.Tribe(659, CREATURETRIBE_NPC);
	scene.zone->SetCurrentZoneID(60);
	CHECK(scene.zone->AddCreature(Make(30000, 21, 15, 659)));
	CHECK(me.TraceCreatureToBasicAction(30000, false, true));
	// A horn is spoken to from the next cell.
	CHECK_EQ(1, int(me.m_TraceDistance));
	Frames(me, 3);
	CHECK_EQ(1, Count("horn|60"));
	CHECK(sent.empty());
	CHECK(me.IsWaitVerifyNULL());
}

TEST(Player, ItsCharacterWindowShowsItsDamageAndDefences)
{
	Scene scene;
	PlayerProbe& me = *scene.me;
	me.MStatus::SetStatus(MODIFY_MIN_DAMAGE, 3);
	me.MStatus::SetStatus(MODIFY_MAX_DAMAGE, 9);
	me.MStatus::SetStatus(MODIFY_DEFENSE, 11);
	me.MStatus::SetStatus(MODIFY_PROTECTION, 12);
	me.MStatus::SetStatus(MODIFY_TOHIT, 13);
	g_Mode = MODE_GAME;
	calls.clear();
	me.CalculateStatus();
	// The most, then the least; silver none without a weapon.
	CHECK(InOrder({ "dam|9,3", "silver|0,0", "defense|11", "protection|12", "tohit|13" }));
	// Choosing a character: the values worked out from its stats.
	g_Mode = MODE_WAIT_PCLIST;
	calls.clear();
	me.CalculateStatus();
	CHECK(InOrder({ "dam|9,3", "defense|" + N(g_StatusManager.GetDefense()), "protection|" + N(g_StatusManager.GetProtection()),
		"tohit|" + N(g_StatusManager.GetTOHIT()) }));
}
