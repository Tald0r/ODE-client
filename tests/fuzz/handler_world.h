//----------------------------------------------------------------------
// handler_world.h
//----------------------------------------------------------------------
//
// The world the packet-handler fuzz targets dispatch into: the game's
// model as it stands once the player has entered a zone, built afresh
// for every run so no input sees what an earlier one changed, and the
// client's own receive loop over it. Where client_stream_reader.h
// mirrors ClientPlayer::processCommand's gates and stops before
// dispatch, this runs processCommand itself on a ClientPlayer whose
// socket never connected: the same gates, PacketDispatcher::dispatch
// into the handlers PacketHandlerRegistry.cpp binds, and the player
// status a handler sets carrying over to the next frame.
//
//   fuzz_client_stream_handlers.cpp        the game connection
//   fuzz_client_login_stream_handlers.cpp  the login connection
//
// Input: [encrypt code byte][raw stream], as the read-only targets
// take it, so the golden seed corpus serves both. The stream is read
// once, from the start of the input ring (the wrap point is the
// read-only targets' concern); processCommand stops after the client
// config's MAX_PROCESS_PACKET frames, and the run calls it again, as
// the next frame of the game loop would, while it still consumes
// bytes, at most kMaxCommandCalls times.
//
// What the world holds, in the order the client builds it:
//
//   - the data tables InitInfomation loads, empty but sized as the
//     game's are where code reads a row it assumes exists: the action,
//     effect-status and effect-sprite tables to their enums, every item
//     class kItemTypes rows, the creature table kCreatureTypes rows with
//     the player races' tribes. An id past a table reads its default
//     row in the game too (CTypeTable), so a crash on one is a crash a
//     server can cause. The game strings are the English build's
//     (InitGameStringTable), so the operator name prefix the chat
//     replies test is "GM", and the zone's row masks another race's
//     chat. The ones no run changes are made once per process
//     (MakeStaticTables); the skill-info and rank-bonus tables, which
//     handlers write, and the managers are made for every run;
//   - the message logs and string tables InitGame makes, and the peer
//     roster InitSocket makes (not its UDP manager nor its peer file
//     manager, which bind a socket and write files);
//   - InitGameObject itself, the production function: the player, the
//     inventory, the gear, the skill tree and the other objects it
//     makes, and every host it installs;
//   - in the game target, a zone of kZoneSize by kZoneSize sectors (a
//     wire coordinate is a byte, so it can name a cell past it, as in
//     any zone smaller than 256 a side), its row in the zone table, the
//     player standing in its middle under the ID kPlayerID, and the
//     inventory and the gear cleared the way GCUpdateInfo's
//     SetInventoryInfo and SetGearInfo clear them; the player is a
//     slayer, a vampire or an ousters. The login target has no zone.
//
// The hosts. InitGameObject installs the executable's adapters; most
// of them draw, play sounds or open dialogs through a UI and a view
// that a harness cannot build, so the run removes them all
// (ReleaseGameObjectHosts) and puts back only the model's: ZoneState,
// CreatureState, PlayerState and MItem's host, which read and change
// the zone, the creatures and the player, with the entries that reach
// the UI, a sound or a peer over UDP left empty (see InstallHosts).
// The wire and price hosts, which ReleaseGameObjectHosts keeps, stay.
// A missing host answers as a missing config would, so a presentation
// call is skipped, as every unit test runs them; what an adapter does
// with the value a handler hands it (GameMain's SetWeather, a dialog's
// text) is not reached.
//
// Two handlers are not dispatched: GCReconnectLogin and LCReconnect
// reconnect the socket to the address the packet names, which a
// harness must not do; they are read and dropped. The player's socket
// aborts the run if anything disconnects it, which only they do, so a
// lost drop fails their regression inputs before a connection opens.
//
// Outcomes are client_stream_reader.h's: a Throwable is a rejected
// input (UpdateSocketInput catches it and the client disconnects);
// anything else, a std::exception included, escapes as a crash, and so
// does a sanitizer report. DE_FUZZ_ABORT_ON_ASSERT turns a failed
// Assert into an abort. A crash can still be the harness's: a global
// the game always sets and this world does not. Such a crash is fixed
// here, never in the handler; docs/protocol/fuzzing.md says how each
// one was told apart.
//
// Reset between runs: every object a run makes is deleted and its
// global set back to NULL, the storages and the trade a reply makes
// included; the flags, counters and gear pointers the replies and the
// model write are put back (KeepScalars, from a sweep of the handlers'
// and gamemodel's assignments to globals); the hosts are removed,
// g_Mode is put back and rand() reseeded. State the sweep did not name
// (a function's static, a frame clock) is not reset; the replay ctest
// reads its inputs in one fixed order, so it is deterministic either
// way, and a crash libFuzzer reports is confirmed by replaying its
// input alone.
//
//----------------------------------------------------------------------

#ifndef HANDLER_WORLD_H
#define HANDLER_WORLD_H

#include "client_stream_reader.h"
#include "PacketDispatcherTestAccess.h"

#include "Client_PCH.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "GameInit.h"
#include "GameMessages.h"
#include "GameSocket.h"
#include "PacketHandlerRegistry.h"
#include "Properties.h"
#include "ZoneFlags.h"

#include "CUsingColorSetTable.h"
#include "ClientConfig.h"
#include "ExperienceTable.h"
#include "SkillDef.h"
#include "FameInfo.h"
#include "MActionInfoTable.h"
#include "MAttachOrbitEffect.h"
#include "MChatManager.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MEffectGeneratorTable.h"
#include "EffectSpriteTypeDef.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "MGameStringTable.h"
#include "MGuildInfoMapper.h"
#include "MHelpStringTable.h"
#include "MInteractionObjectTable.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemOptionTable.h"
#include "MItemTable.h"
#include "MJusticeAttackManager.h"
#include "MLevelNameTable.h"
#include "MMonsterKillQuestInfo.h"
#include "MMusicTable.h"
#include "MNPCScriptTable.h"
#include "MNPCTable.h"
#include "MObjectSelector.h"
#include "MOustersGear.h"
#include "MParty.h"
#include "MPlayer.h"
#include "MPriceManager.h"
#include "MMoneyManager.h"
#include "MScreenEffectManager.h"
#include "MShopTemplateTable.h"
#include "MSkillManager.h"
#include "MSlayerGear.h"
#include "MSoundTable.h"
#include "MStringArray.h"
#include "MStorage.h"
#include "MTradeManager.h"
#include "MQuickSlot.h"
#include "MorphState.h"
#include "InputState.h"
#include "MTimeItemManager.h"
#include "MVampireGear.h"
#include "MWarManager.h"
#include "MWeather.h"
#include "MGameTime.h"
#include "MZone.h"
#include "MZoneTable.h"
#include "ModifyStatusManager.h"
#include "MonsterNameTable.h"
#include "PlayerStateHost.h"
#include "RankBonusTable.h"
#include "RequestUserManager.h"
#include "ShrineInfoManager.h"
#include "SystemAvailabilities.h"
#include "TalkBox.h"
#include "TempInformation.h"
#include "UIMessageManager.h"
#include "UserInformation.h"
#include "UserOption.h"
#include "ZoneStateHost.h"
#include "CServerInformation.h"
#include "../basic/CMessageArray.h"
#include "VS_UI_title.h"

#include "Gpackets/GCReconnectLogin.h"
#include "Lpackets/LCReconnect.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <vector>

// The two packet-function steps of entering a zone (PacketFunction.cpp).
void	SetInventoryInfo(InventoryInfo* pInventoryInfo);
void	SetGearInfo(GearInfo* pGearInfo, BloodBibleSignInfo* pBloodBibleInfo);

// Globals the replies write that no header declares (GCUpdateInfoHandler.cpp
// and VS_UI's), as PacketFunction.cpp declares them.
extern DWORD	g_PreviousCreatureType;
extern S_SLOT	g_char_slot_ingame;

namespace HandlerFuzz {

// Rows per item class: types below it read a row, as the game's do.
inline const int		kItemTypes = 32;
// Rows of the creature table; the highest player type is 812.
inline const int		kCreatureTypes = 1024;
// The size of three of the game's zones; the server's map files hold
// zones from 17 by 25 to 256 by 256 sectors.
inline const TYPE_SECTORPOSITION	kZoneSize = 32;
inline const TYPE_ZONEID	kZoneID = 61;
inline const TYPE_OBJECTID	kPlayerID = 1000;
// processCommand calls one run makes at most.
inline const int		kMaxCommandCalls = 16;

// The player's race in a run.
enum Race { RACE_RUN_SLAYER, RACE_RUN_VAMPIRE, RACE_RUN_OUSTERS, RACE_RUN_MAX };

// The file InitGameObject opens before anything else: its four bytes are
// read and not used. Written in the working directory, a scratch one.
inline const char		kVersionFile[] = "fuzz_handlers_version.inf";

inline Properties*		g_pHarnessFileDef = NULL;

//----------------------------------------------------------------------
// The ClientPlayer the client reads with, its input ring reachable.
//----------------------------------------------------------------------
class HarnessPlayer : public ClientPlayer
{
public:
	explicit HarnessPlayer(Socket* pSocket) : ClientPlayer(pSocket) {}

	SocketEncryptInputStream&	Input()
	{
		return *static_cast<SocketEncryptInputStream*>(m_pInputStream);
	}

	// Only GCReconnectLogin's and LCReconnect's handlers disconnect the
	// client's socket, before they reconnect it, and the harness drops
	// both; reaching this means one was dispatched, so the run stops here,
	// before a connection is opened. Their inputs under tests/fuzz/
	// regressions pin the drop.
	void	disconnect(bool) override
	{
		std::fprintf(stderr, "handler_world: a handler disconnected the socket to reconnect it\n");
		std::abort();
	}
};

//----------------------------------------------------------------------
// One run's objects; the destructor deletes them in reverse and sets
// each global back to NULL.
//----------------------------------------------------------------------
class World
{
public:
	// In a zone, or at the login server, where the player InitGameObject
	// made waits, a slayer, with no zone yet.
	World(Race race, bool bInZone)
	{
		std::srand(1);
		g_Mode = bInZone ? MODE_GAME : MODE_WAIT_LOGINOK;
		KeepScalars();
		MakeTables();
		MakeLogs();
		MakeGameObjects();
		InstallHosts();
		if (bInZone)
			EnterZone(race);
		// Taken last, so they go first, while what their items read is
		// still there.
		Own(g_pStorage);
		Own(g_pStorage2);
		Own(g_pTradeManager);
	}

	~World()
	{
		// The zone first, while the hosts that free what lies in it are
		// installed: the player out of it, as ReleaseAllObjects takes it.
		if (g_pZone != NULL && g_pPlayer != NULL)
			g_pZone->RemoveCreature(g_pPlayer->GetID());
		for (auto it = m_Undo.rbegin(); it != m_Undo.rend(); ++it)
			(*it)();
		ReleaseGameObjectHosts();
	}

	World(const World&) = delete;
	World& operator=(const World&) = delete;

private:
	std::vector<std::function<void()>>	m_Undo;

	// Takes the object the global holds now, made here or by the game.
	template <class T>
	void	Own(T*& global)
	{
		T** slot = &global;
		m_Undo.push_back([slot]() { delete *slot; *slot = NULL; });
	}

	template <class T>
	void	Make(T*& global, T* value)
	{
		global = value;
		Own(global);
	}

	// Puts the global back to the value it holds now when the run ends.
	template <class T>
	void	Keep(T& global)
	{
		T* slot = &global;
		m_Undo.push_back([slot, value = global]() { *slot = value; });
	}

	//------------------------------------------------------------------
	// What the replies, and the model code they reach, write outside the
	// objects the world makes: flags and counters, the client's character
	// slot, and pointers into the gear the world deletes (the quick slot,
	// the arms bands, the current magazine). Each goes back to its
	// start-up value; the storages and the trade a reply makes are
	// deleted at the end of the constructor's list.
	//------------------------------------------------------------------
	void	KeepScalars()
	{
		Keep(g_ModeNext);
		Keep(g_bNeedUpdate);
		Keep(g_LeftPremiumDays);
		Keep(g_bFamily);
		Keep(g_MorphCreatureType);
		Keep(g_PreviousCreatureType);
		Keep(g_bZoneSafe);
		Keep(g_bHolyLand);
		Keep(g_ZoneCreatureColorSet);
		Keep(g_nZoneSmall);
		Keep(g_nZoneLarge);
		Keep(g_bZonePlayerInLarge);
		Keep(g_SelectSector);
		Keep(g_bNetStatusGood);
		Keep(g_char_slot_ingame);
		Keep(g_pQuickSlot);
		Keep(g_pArmsBand1);
		Keep(g_pArmsBand2);
		Keep(g_pCurrentMagazine);
	}

	//------------------------------------------------------------------
	// The tables a run reads and may change, and the managers.
	//------------------------------------------------------------------
	void	MakeTables()
	{
		Make(g_pClientConfig, new ClientConfig);
		Make(g_pUserOption, new UserOption);
		Make(g_pNickNameStringTable, new MStringArray);
		Make(g_pSkillInfoTable, new MSkillInfoTable);
		Make(g_pRankBonusTable, new RankBonusTable);
		Make(g_pNPCScriptTable, new MNPCScriptTable);
		Make(g_pQuestInfoManager, new MQuestInfoManager);
		Make(g_pRegenTowerInfoManager, new RegenTowerInfoManager);
		Make(g_pGuildInfoMapper, new MGuildInfoMapper);
		Make(g_pChatManager, new MChatManager);
		Make(g_pWarManager, new MWarManager);
		Make(g_pTimeItemManager, new MTimeItemManager);
		Make(g_pModifyStatusManager, new ModifyStatusManager);
		Make(g_pJusticeAttackManager, new MJusticeAttackManager);
		Make(g_pEventManager, new MEventQueue);
		Make(g_pEffectGeneratorTable, new MEffectGeneratorTable);
		g_pEffectGeneratorTable->Init();

		// InitSocket's peer roster. Not its UDP manager nor its peer file
		// manager, which bind a socket and write files: a handler finds
		// those NULL, and the ones that reach them test for it.
		Make(g_pRequestUserManager, new RequestUserManager);
	}

	void	MakeLogs()
	{
		Make(g_pSystemMessage, new CMessageArray);
		Make(g_pPlayerMessage, new CMessageArray);
		Make(g_pGameMessage, new CMessageArray);
		Make(g_pHelpMessage, new CMessageArray);
		Make(g_pNoticeMessage, new CMessageArray);
	}

	void	MakeGameObjects()
	{
		// Made before InitGameObject, which then loads no filter file: the
		// game's only switches systems off.
		Make(g_pSystemAvailableManager, new SystemAvailabilitiesManager);
		g_pFileDef = g_pHarnessFileDef;
		if (!InitGameObject())
		{
			std::fprintf(stderr, "handler_world: InitGameObject failed (is %s in the working directory?)\n", kVersionFile);
			std::abort();
		}
		Own(g_pServerInformation);
		Own(g_pUserInformation);
		Own(g_pTempInformation);
		Own(g_pSkillManager);
		Own(g_pSkillAvailable);
		Own(g_pSlayerGear);
		Own(g_pVampireGear);
		Own(g_pOustersGear);
		Own(g_pInventory);
		Own(g_pParty);
		Own(g_pGameTime);
		Own(g_pPriceManager);
		Own(g_pMoneyManager);
		Own(g_pPCTalkBox);
		Own(g_pNPCTalkBox);
		Own(g_pUIMessageManager);
		Own(g_pObjectSelector);
		Own(g_pWeather);
		Own(g_pInventoryEffectManager);
		Own(g_pPlayer);
	}

	//------------------------------------------------------------------
	// The model's hosts back, the rest none; see the file header.
	//------------------------------------------------------------------
	static void	InstallHosts()
	{
		static ZoneState::Host		zone;
		static CreatureState::Host	creature;
		static PlayerState::Host	player;
		static MItemHost		item;

		zone = *ZoneState::SetHost(nullptr);
		creature = *CreatureState::SetHost(nullptr);
		player = *PlayerState::SetHost(nullptr);
		item = *MItem::GetHost();
		ReleaseGameObjectHosts();

		// The UI's: an item's descriptor, the quick slot, the repair
		// hint, a potion or a pet used from the inventory, and the item
		// sound.
		item.ItemDestroyed = nullptr;
		item.ResetQuickItemSlot = nullptr;
		item.RepairHint = nullptr;
		item.UsePotionFromInventory = nullptr;
		item.UsePetFromInventory = nullptr;
		item.PlayItemSound = nullptr;
		// The UI's item and skill-card replies, and the party's UDP.
		player.UseItemOK = nullptr;
		player.UseSkillCardOK = nullptr;
		player.SendInfoToParty = nullptr;
		player.SendPositionInfoToParty = nullptr;
		player.SendStatusInfoToParty = nullptr;

		ZoneState::SetHost(&zone);
		CreatureState::SetHost(&creature);
		PlayerState::SetHost(&player);
		MItem::SetHost(&item);
	}

	//------------------------------------------------------------------
	// The zone the game loads, the player in it, and the inventory and
	// gear GCUpdateInfo clears before it fills them.
	//------------------------------------------------------------------
	void	EnterZone(Race race)
	{
		static const TYPE_CREATURETYPE kRaceTypes[RACE_RUN_MAX] = {
			CREATURETYPE_SLAYER_MALE, CREATURETYPE_VAMPIRE_MALE1, CREATURETYPE_OUSTERS,
		};
		g_pPlayer->SetCreatureType(kRaceTypes[race]);
		g_pPlayer->SetID(kPlayerID);
		g_pPlayer->SetPosition(kZoneSize / 2, kZoneSize / 2);
		g_pPlayer->SetServerPosition(kZoneSize / 2, kZoneSize / 2);

		// A zone made with no file definitions reads no horn file.
		g_pFileDef = NULL;
		Make(g_pZoneSmall, new MZone(kZoneSize, kZoneSize));
		g_pFileDef = g_pHarnessFileDef;
		g_pZoneSmall->SetCurrentZoneID(kZoneID);
		g_pZone = g_pZoneSmall;
		m_Undo.push_back([]() { g_pZone = NULL; });
		g_nZoneSmall = kZoneID;
		g_bZonePlayerInLarge = false;

		ZoneState::SetPlayer();
		SetInventoryInfo(NULL);
		SetGearInfo(NULL, NULL);
	}
};

inline void	SetTribe(int type, CREATURETRIBE tribe, int maxAction)
{
	if (CREATURETABLE_INFO* pInfo = g_pCreatureTable->GetMutable(type))
		pInfo->SetCreatureTribe(tribe, maxAction);
}

//----------------------------------------------------------------------
// The data tables no run changes, made once: the code that writes them
// after loading (SetGoreLevel's child mode, SetFlagTo's event item) runs
// only from a presentation host, which the world leaves out.
//----------------------------------------------------------------------
inline void	MakeStaticTables()
{
	g_pItemTable = new ITEMCLASS_TABLE;
	g_pItemTable->Init(MAX_ITEM_CLASS);
	for (int c = 0; c < MAX_ITEM_CLASS; c++)
		g_pItemTable->InitClass(c, kItemTypes);
	g_pItemOptionTable = new ITEMOPTION_TABLE;
	g_pItemOptionTable->Init(1);

	g_pCreatureTable = new CREATURE_TABLE;
	g_pCreatureTable->Init(kCreatureTypes);
	for (int type = 0; type < kCreatureTypes; type++)
		SetTribe(type, CREATURETRIBE_VAMPIRE, ACTION_MAX_VAMPIRE);
	for (int type : { CREATURETYPE_SLAYER_MALE, CREATURETYPE_SLAYER_FEMALE, CREATURETYPE_SLAYER_OPERATOR })
		SetTribe(type, CREATURETRIBE_SLAYER, ACTION_MAX_SLAYER);
	for (int type : { CREATURETYPE_OUSTERS, CREATURETYPE_OUSTERS_OPERATOR })
		SetTribe(type, CREATURETRIBE_OUSTERS, ACTION_MAX_OUSTERS);
	g_pCreatureSpriteTypeMapper = new CreatureSpriteTypeMapper;
	g_pCreatureSpriteTable = new CREATURESPRITE_TABLE;

	g_pActionInfoTable = new MActionInfoTable;
	g_pActionInfoTable->Init(MAX_ACTIONINFO);
	g_pActionInfoTable->SetMinResultActionInfo(MIN_RESULT_ACTIONINFO);
	g_pEffectStatusTable = new EFFECTSTATUS_TABLE;
	g_pEffectStatusTable->Init(EFFECTSTATUS_MAX);
	g_pEffectSpriteTypeTable = new EFFECTSPRITETYPE_TABLE;
	g_pEffectSpriteTypeTable->Init(MAX_EFFECTSPRITETYPE);
	g_pActionEffectSpriteTypeTable = new MActionEffectSpriteTypeTable;

	g_pInteractionObjectTable = new INTERACTIONOBJECT_TABLE;
	g_pNPCTable = new MNPCTable;
	g_pShopTemplateTable = new MShopTemplateTable;
	g_pExperienceTable = new ExperienceTable;
	g_pSoundTable = new SOUND_TABLE;
	g_pMusicTable = new MUSIC_TABLE;
	g_pUsingColorSetTable = new USINGCOLORSET_TABLE;
	g_pHelpStringTable = new MHelpStringTable;
	g_pLevelNameTable = new MLevelNameTable;
	g_pMonsterNameTable = new MonsterNameTable;
	g_pFameInfoTable = new FameInfoTable;

	g_pZoneTable = new CZoneTable;
	ZONETABLE_INFO* pZoneInfo = new ZONETABLE_INFO;
	pZoneInfo->ID = kZoneID;
	// Some zones mask another race's chat, and the replies that check
	// read the race the packet names.
	pZoneInfo->ChatMaskByRace = true;
	g_pZoneTable->Add(pZoneInfo);

	// The English build's strings, as InitGame fills them: an empty table
	// would leave UI_STRING_MESSAGE_MASTER_NAME empty, which every chat
	// reply takes for an operator's name prefix, so no line would be a
	// player's.
	g_pGameStringTable = new MStringArray;
	InitGameStringTable();
}

//----------------------------------------------------------------------
// Once per process: the packet tables, the handler registry with the
// two reconnecting handlers dropped, the static movement tables, the
// tables no run changes, and the file definitions InitGameObject reads.
//----------------------------------------------------------------------
inline void	Initialize()
{
	ClientStreamFuzz::Initialize();
	registerClientPacketHandlers();

	struct Drop { static void call(Packet*, Player*) {} };
	PacketDispatcherTestAccess::Set(GCReconnectLogin().getPacketID(), &Drop::call);
	PacketDispatcherTestAccess::Set(LCReconnect().getPacketID(), &Drop::call);

	MCreature::InitMoveTable();
	MAttachOrbitEffect::InitOrbitPosition();
	MakeStaticTables();

	{
		std::ofstream version(kVersionFile, std::ios::binary);
		const char bytes[4] = { 7, 0, 0, 0 };
		version.write(bytes, sizeof(bytes));
	}
	g_pHarnessFileDef = new Properties;
	g_pHarnessFileDef->setProperty("FILE_INFO_VERSION", kVersionFile);
	// Absent: no data pack, no filter file, no domain experience.
	g_pHarnessFileDef->setProperty("FILE_INFO_DATA", "fuzz_handlers_absent.rpk");
	g_pHarnessFileDef->setProperty("FILE_FILTER_INFO", "fuzz_handlers_absent.txt");
	g_pHarnessFileDef->setProperty("FILE_INFO_SKILL_DOMAIN_EXP", "fuzz_handlers_absent.inf");
}

//----------------------------------------------------------------------
// One run: a fresh world with the player of `race`, a ClientPlayer in
// `status` holding the stream, and the receive loop over it.
//----------------------------------------------------------------------
inline void	Run(const std::uint8_t* data, std::size_t size, PlayerStatus status, Race race, bool bInZone)
{
	if (size < 1 || size - 1 >= ClientStreamFuzz::kInputBufferLen)
		return;

	World world(race, bInZone);
	HarnessPlayer* pPlayer = new HarnessPlayer(new Socket(new SocketImpl()));
	g_pSocket = pPlayer;
	pPlayer->setPlayerStatus(status);
	pPlayer->Input().setEncryptCode(data[0]);
	SocketInputStreamTestAccess::Preload(pPlayer->Input(), data + 1, (unsigned int)(size - 1));

	try {
		unsigned int left = pPlayer->Input().length();
		for (int call = 0; call < kMaxCommandCalls && left > 0; call++)
		{
			pPlayer->processCommand();
			const unsigned int now = pPlayer->Input().length();
			if (now == left)
				break;
			left = now;
		}
	} catch (AssertionError&) {
		if (ClientStreamFuzz::g_bAbortOnAssert)
			std::abort();
	} catch (Throwable&) {
	}

	pPlayer->setPlayerStatus(CPS_END_SESSION);
	delete pPlayer;
	g_pSocket = NULL;
}

} // namespace HandlerFuzz

#endif // HANDLER_WORLD_H
