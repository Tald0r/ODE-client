#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "PlayerStateHost.h"
#include "ConvertDuration.h"
#include "ClientPlayer.h"
#include "MInventory.h"
#include "MOustersGear.h"
#include "MSlayerGear.h"
#include "MStatus.h"
#include "MVampireGear.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAddGearToInventory.h"
#include "Gpackets/GCGetOffMotorCycleFailed.h"
#include "Gpackets/GCHPRecoveryEndToSelf.h"
#include "Gpackets/GCHPRecoveryStartToSelf.h"
#include "Gpackets/GCMPRecoveryEnd.h"
#include "Gpackets/GCMPRecoveryStart.h"
#include "Gpackets/GCModifyInformation.h"
#include "Gpackets/GCMoveError.h"
#include "Gpackets/GCMoveOK.h"
#include "Gpackets/GCRideMotorCycleFailed.h"
#include "Gpackets/GCTimeLimitItemInfo.h"
#include "Gpackets/GCUnburrowFail.h"
#include "Gpackets/GCUntransformFail.h"
#include "Gpackets/GCUseBonusPointOK.h"
#include "Gpackets/GCVisibleFail.h"
#include "Gpackets/GCVisibleOK.h"
#include <cstdint>
#include <string>
#include <vector>

// Replies about the client's own player read and change it through
// PlayerState::Host: what it waits for the server to verify, its recovery,
// its status array (a real MStatus here), its gear and its movement. Gear
// moves go through the real gear and inventory containers.
namespace {

std::vector<std::string> calls;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
Race race = RACE_SLAYER;
MItemManager* gear = nullptr;

// The player's status array, recording every change made through it. Its
// effect status starts empty, as MCreature's constructor sets it; the other
// values keep MStatus's MODIFY_NULL.
struct RecordingStatus : MStatus
{
	RecordingStatus() { MStatus::SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL); }
	void SetStatus(DWORD n, DWORD value) override
	{
		calls.push_back("status|" + std::to_string(n) + "|" + std::to_string(value));
		MStatus::SetStatus(n, value);
	}
};
RecordingStatus* player = nullptr;

const PlayerState::Host host{
	.Status = []() -> MStatus* { return player; },
	.GetWaitVerify = []() { calls.push_back("wait?"); return waitVerify; },
	.SetWaitVerifyNULL = []() { calls.push_back("clearwait"); waitVerify = PlayerState::WAIT_VERIFY_NULL; },
	.GetRace = []() { return race; },
	.GetGear = []() { return gear; },
	.SetAddonItem = [](MItem* pItem) { calls.push_back("addon|" + std::to_string(pItem->GetID())); return true; },
	.AddEffectStatus = [](EFFECTSTATUS status, std::uint32_t delayFrame) {
		calls.push_back("effect|" + std::to_string(status) + "|" + std::to_string(delayFrame));
		return true;
	},
	.SetRecoveryHP = [](int amount, int times, std::uint32_t delay) {
		calls.push_back("hprecovery|" + std::to_string(amount) + "|" + std::to_string(times) + "|" + std::to_string(delay));
	},
	.SetRecoveryMP = [](int amount, int times, std::uint32_t delay) {
		calls.push_back("mprecovery|" + std::to_string(amount) + "|" + std::to_string(times) + "|" + std::to_string(delay));
	},
	.StopRecoveryHP = []() { calls.push_back("stophp"); },
	.StopRecoveryMP = []() { calls.push_back("stopmp"); },
	.SetVisible = []() { calls.push_back("visible"); },
	.SetServerPosition = [](TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) {
		calls.push_back("serverpos|" + std::to_string(sX) + "|" + std::to_string(sY));
	},
	.PacketMoveOK = [](TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t direction) {
		calls.push_back("moveok|" + std::to_string(sX) + "|" + std::to_string(sY) + "|" + std::to_string(direction));
	},
	.PacketMoveNO = []() { calls.push_back("moveno"); },
};

// A piece of gear of any class, claiming one slot.
struct Gear : MItem
{
	ITEM_CLASS m_Class;
	Gear(ITEM_CLASS itemClass, TYPE_OBJECTID id) : m_Class(itemClass) { SetID(id); SetCurrentDurability(100); }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsGearItem() const override { return true; }
	int GetMaxDurability() const override { return 100; }
};
struct Helm : Gear
{
	explicit Helm(TYPE_OBJECTID id) : Gear(ITEM_CLASS_HELM, id) {}
	bool IsGearSlotHelm() const override { return true; }
};
struct VampireCoat : Gear
{
	explicit VampireCoat(TYPE_OBJECTID id) : Gear(ITEM_CLASS_VAMPIRE_COAT, id) {}
	bool IsGearSlotVampireCoat() const override { return true; }
};
struct Circlet : Gear
{
	explicit Circlet(TYPE_OBJECTID id) : Gear(ITEM_CLASS_OUSTERS_CIRCLET, id) {}
	bool IsGearSlotOustersCirclet() const override { return true; }
};

struct World : GameModelWorld
{
	RecordingStatus status;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MInventory inventory;
	TempInformation temp;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MInventory* oldInventory = g_pInventory;
	TempInformation* oldTemp = g_pTempInformation;
	const PlayerState::Host* oldHost = PlayerState::SetHost(&host);
	World()
	{
		const struct { ITEM_CLASS itemClass; BYTE race; } rows[] = {
			{ITEM_CLASS_HELM, FLAG_RACE_SLAYER}, {ITEM_CLASS_VAMPIRE_COAT, FLAG_RACE_VAMPIRE},
			{ITEM_CLASS_OUSTERS_CIRCLET, FLAG_RACE_OUSTERS},
		};
		for (const auto& row : rows)
		{
			g_pItemTable->InitClass(row.itemClass, 1);
			testfw::MutableRow(*g_pItemTable, row.itemClass, 0).Race = row.race;
			testfw::MutableRow(*g_pItemTable, row.itemClass, 0).SetGrid(1, 1);
		}
		slayerGear.Init();
		vampireGear.Init();
		oustersGear.Init();
		inventory.Init(4, 4);
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
		g_pInventory = &inventory;
		g_pTempInformation = &temp;
		player = &status;
		calls.clear();
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		race = RACE_SLAYER;
		gear = &slayerGear;
	}
	~World()
	{
		PlayerState::SetHost(oldHost);
		player = nullptr;
		gear = nullptr;
		g_pTempInformation = oldTemp;
		g_pInventory = oldInventory;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
	}
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

std::string Status(DWORD n, DWORD value) { return "status|" + std::to_string(n) + "|" + std::to_string(value); }

void ReceiveHPRecoveryStart(HP_t quantity, HP_t period, BYTE delay)
{
	GCHPRecoveryStartToSelf original;
	original.setQuantity(quantity);
	original.setPeriod(period);
	original.setDelay(delay);
	GCHPRecoveryStartToSelf packet;
	Decode(packet, original);
	GCHPRecoveryStartToSelfHandler::execute(&packet, nullptr);
}

void ReceiveMPRecoveryStart(MP_t quantity, MP_t period, BYTE delay)
{
	GCMPRecoveryStart original;
	original.setQuantity(quantity);
	original.setPeriod(period);
	original.setDelay(delay);
	GCMPRecoveryStart packet;
	Decode(packet, original);
	GCMPRecoveryStartHandler::execute(&packet, nullptr);
}

void ReceiveHPRecoveryEnd(HP_t hp)
{
	GCHPRecoveryEndToSelf original;
	original.setCurrentHP(hp);
	GCHPRecoveryEndToSelf packet;
	Decode(packet, original);
	GCHPRecoveryEndToSelfHandler::execute(&packet, nullptr);
}

void ReceiveMPRecoveryEnd(MP_t mp)
{
	GCMPRecoveryEnd original;
	original.setCurrentMP(mp);
	GCMPRecoveryEnd packet;
	Decode(packet, original);
	GCMPRecoveryEndHandler::execute(&packet, nullptr);
}

void ReceiveGearToInventory(SlotID_t slot, CoordInven_t x, CoordInven_t y)
{
	GCAddGearToInventory original;
	original.setSlotID(slot);
	original.setInvenX(x);
	original.setInvenY(y);
	GCAddGearToInventory packet;
	Decode(packet, original);
	GCAddGearToInventoryHandler::execute(&packet, nullptr);
}

// The motorcycle and skill verification failures, the visibility reply, and
// the recovery replies that check for a player: they reach every entry but
// the status array, the race and the gear. Six calls with a host.
void RunGuardedHandlers()
{
	GCGetOffMotorCycleFailed getOff;
	GCGetOffMotorCycleFailedHandler::execute(&getOff, nullptr);
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	GCVisibleOK visible;
	GCVisibleOKHandler::execute(&visible, nullptr);
	ReceiveHPRecoveryStart(5, 6, 7);
	ReceiveMPRecoveryStart(5, 6, 7);
}

} // namespace

TEST(PlayerStateHandlers, AMotorcycleFailureAlwaysStopsWaiting)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_MOTORCYCLE_GETON;
	GCRideMotorCycleFailed ride;
	GCRideMotorCycleFailedHandler::execute(&ride, nullptr);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_NULL);
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	GCGetOffMotorCycleFailed getOff;
	GCGetOffMotorCycleFailedHandler::execute(&getOff, nullptr);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_NULL);
	CHECK(calls == std::vector<std::string>({"clearwait", "clearwait"}));
}

TEST(PlayerStateHandlers, ASkillFailureStopsWaitingOnlyForASkill)
{
	World world;
	const PlayerState::WAIT_VERIFY waits[] = {PlayerState::WAIT_VERIFY_SKILL_SUCCESS, PlayerState::WAIT_VERIFY_ITEM_USE};
	for (PlayerState::WAIT_VERIFY wait : waits)
	{
		const bool skill = wait == PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
		const std::vector<std::string> expected = skill ? std::vector<std::string>{"wait?", "clearwait"}
		                                                : std::vector<std::string>{"wait?"};
		calls.clear();
		waitVerify = wait;
		GCUnburrowFail unburrow;
		GCUnburrowFailHandler::execute(&unburrow, nullptr);
		CHECK(calls == expected);
		calls.clear();
		waitVerify = wait;
		GCUntransformFail untransform;
		GCUntransformFailHandler::execute(&untransform, nullptr);
		CHECK(calls == expected);
		calls.clear();
		waitVerify = wait;
		GCVisibleFail visibleFail;
		GCVisibleFailHandler::execute(&visibleFail, nullptr);
		CHECK(calls == expected);
		CHECK(waitVerify == (skill ? PlayerState::WAIT_VERIFY_NULL : wait));

		// Becoming visible stops a skill wait, then shows the player either way.
		calls.clear();
		waitVerify = wait;
		GCVisibleOK visible;
		GCVisibleOKHandler::execute(&visible, nullptr);
		std::vector<std::string> shown = expected;
		shown.push_back("visible");
		CHECK(calls == shown);
	}
}

TEST(PlayerStateHandlers, RecoveryStartsWithTheConvertedDelayAndEndsAtTheServersValue)
{
	World world;
	ReceiveHPRecoveryStart(12, 5, 30);
	ReceiveMPRecoveryStart(8, 3, 4);
	ReceiveHPRecoveryEnd(321);
	ReceiveMPRecoveryEnd(123);
	const std::vector<std::string> expected = {
		"hprecovery|12|5|" + std::to_string(ConvertDurationToMillisecond(30)),
		"mprecovery|8|3|" + std::to_string(ConvertDurationToMillisecond(4)),
		"stophp", Status(MODIFY_CURRENT_HP, 321),
		"stopmp", Status(MODIFY_CURRENT_MP, 123),
	};
	CHECK(calls == expected);
	CHECK(ConvertDurationToMillisecond(30) != 30);
	CHECK_EQ(321, static_cast<long long>(world.status.GetStatus(MODIFY_CURRENT_HP)));
	CHECK_EQ(123, static_cast<long long>(world.status.GetStatus(MODIFY_CURRENT_MP)));

	// Without a player the HP replies and the MP start do nothing.
	calls.clear();
	player = nullptr;
	ReceiveHPRecoveryStart(12, 5, 30);
	ReceiveMPRecoveryStart(8, 3, 4);
	ReceiveHPRecoveryEnd(321);
	CHECK(calls.empty());
}

TEST(PlayerStateHandlers, AModifiedStatusAppliesItsValuesThenStartsAnyEffectItCarries)
{
	World world;
	GCModifyInformation original;
	original.addShortData(MODIFY_CURRENT_HP, 77);
	GCModifyInformation plain;
	Decode(plain, original);
	GCModifyInformationHandler::execute(&plain, nullptr);
	CHECK(calls == std::vector<std::string>{Status(MODIFY_CURRENT_HP, 77)});

	calls.clear();
	original.addShortData(MODIFY_EFFECT_STAT, EFFECTSTATUS_HAS_SLAYER_RELIC);
	original.addLongData(MODIFY_DURATION, 50);
	GCModifyInformation withEffect;
	Decode(withEffect, original);
	GCModifyInformationHandler::execute(&withEffect, nullptr);
	CHECK_EQ(5u, calls.size());
	CHECK(calls.size() == 5 && calls[3] == "effect|" + std::to_string(EFFECTSTATUS_HAS_SLAYER_RELIC) + "|"
		+ std::to_string(ConvertDurationToFrame(50)));
	CHECK(calls.size() == 5 && calls[4] == Status(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL));
	CHECK_EQ(static_cast<long long>(EFFECTSTATUS_NULL), static_cast<long long>(world.status.GetEFFECT_STAT()));
	CHECK_EQ(77, static_cast<long long>(world.status.GetStatus(MODIFY_CURRENT_HP)));
}

TEST(PlayerStateHandlers, AnAcceptedBonusPointClearsItsRequestAndSpendsOnePoint)
{
	World world;
	world.status.SetStatus(MODIFY_BONUS_POINT, 3);
	calls.clear();
	// The status change is popped off the packet as it applies, so each
	// reply is decoded afresh.
	auto Receive = []() {
		GCUseBonusPointOK original;
		original.addShortData(MODIFY_CURRENT_STR, 41);
		GCUseBonusPointOK packet;
		Decode(packet, original);
		GCUseBonusPointOKHandler::execute(&packet, nullptr);
	};

	world.temp.SetMode(TempInformation::MODE_BONUSPOINT_USE);
	Receive();
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(calls == std::vector<std::string>({Status(MODIFY_CURRENT_STR, 41), Status(MODIFY_BONUS_POINT, 2)}));

	// Not the bonus-point request: the values apply, the request and points stay.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_SHOP_BUY);
	Receive();
	CHECK(world.temp.GetMode() == TempInformation::MODE_SHOP_BUY);
	CHECK(calls == std::vector<std::string>{Status(MODIFY_CURRENT_STR, 41)});

	// The last point is spent too.
	world.status.SetStatus(MODIFY_BONUS_POINT, 1);
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_BONUSPOINT_USE);
	Receive();
	CHECK(calls == std::vector<std::string>({Status(MODIFY_CURRENT_STR, 41), Status(MODIFY_BONUS_POINT, 0)}));

	// No point left to spend: the request clears, the count stays at zero.
	calls.clear();
	world.status.SetStatus(MODIFY_BONUS_POINT, 0);
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_BONUSPOINT_USE);
	Receive();
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(calls == std::vector<std::string>{Status(MODIFY_CURRENT_STR, 41)});
	CHECK_EQ(0, static_cast<long long>(world.status.GetBonusPoint()));
}

TEST(PlayerStateHandlers, GearTakenOffGoesFromTheRacesGearToTheNamedInventoryCell)
{
	World world;
	Helm* helm = new Helm(11);
	VampireCoat* coat = new VampireCoat(12);
	Circlet* circlet = new Circlet(13);
	CHECK(world.slayerGear.AddItem(helm, MSlayerGear::GEAR_SLAYER_HELM));
	CHECK(world.vampireGear.AddItem(coat, MVampireGear::GEAR_VAMPIRE_COAT));
	CHECK(world.oustersGear.AddItem(circlet, MOustersGear::GEAR_OUSTERS_CIRCLET));

	// Only the player's own race's gear is read.
	race = RACE_VAMPIRE;
	ReceiveGearToInventory(MSlayerGear::GEAR_SLAYER_HELM, 0, 0);
	CHECK(world.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == helm);
	CHECK(world.inventory.GetItem(0, 0) == nullptr);

	race = RACE_SLAYER;
	ReceiveGearToInventory(MSlayerGear::GEAR_SLAYER_HELM, 1, 2);
	CHECK(world.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == nullptr);
	CHECK(world.inventory.GetItem(1, 2) == helm);

	race = RACE_VAMPIRE;
	ReceiveGearToInventory(MVampireGear::GEAR_VAMPIRE_COAT, 3, 0);
	CHECK(world.vampireGear.GetItem(MVampireGear::GEAR_VAMPIRE_COAT) == nullptr);
	CHECK(world.inventory.GetItem(3, 0) == coat);

	race = RACE_OUSTERS;
	ReceiveGearToInventory(MOustersGear::GEAR_OUSTERS_CIRCLET, 2, 3);
	CHECK(world.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_CIRCLET) == nullptr);
	CHECK(world.inventory.GetItem(2, 3) == circlet);

	// An empty slot moves nothing.
	ReceiveGearToInventory(MOustersGear::GEAR_OUSTERS_CIRCLET, 0, 0);
	CHECK(world.inventory.GetItem(0, 0) == nullptr);
	CHECK(calls.empty());
}

TEST(PlayerStateHandlers, TimeLimitsAreRecordedAndWornItemsReappliedAsAddons)
{
	World world;
	// The player is a vampire: only the vampire's gear is the player's.
	VampireCoat* coat = new VampireCoat(77);
	Helm* helm = new Helm(88);
	CHECK(world.vampireGear.AddItem(coat, MVampireGear::GEAR_VAMPIRE_COAT));
	CHECK(world.slayerGear.AddItem(helm, MSlayerGear::GEAR_SLAYER_HELM));
	gear = &world.vampireGear;
	coat->UnSetAffectStatus();
	helm->UnSetAffectStatus();
	GCTimeLimitItemInfo packet;
	packet.addTimeLimit(77, 3600);
	packet.addTimeLimit(88, 60);
	packet.addTimeLimit(99, 30);
	GCTimeLimitItemInfoHandler::execute(&packet, nullptr);
	CHECK(g_pTimeItemManager->IsExist(77));
	CHECK(g_pTimeItemManager->IsExist(88));
	CHECK(g_pTimeItemManager->IsExist(99));
	CHECK(calls == std::vector<std::string>{"addon|77"});
	CHECK(coat->IsAffectStatus());
	CHECK(!helm->IsAffectStatus());
	CHECK(packet.m_TimeLimitItemInfos.empty());

	// Without the time-item manager nothing is read or recorded.
	calls.clear();
	MTimeItemManager* manager = g_pTimeItemManager;
	g_pTimeItemManager = nullptr;
	packet.addTimeLimit(66, 60);
	GCTimeLimitItemInfoHandler::execute(&packet, nullptr);
	g_pTimeItemManager = manager;
	CHECK(!g_pTimeItemManager->IsExist(66));
	CHECK_EQ(1u, packet.m_TimeLimitItemInfos.size());
	CHECK(calls.empty());
}

TEST(PlayerStateHandlers, AConfirmedMoveUpdatesTheConnectionThenThePlayerAndARefusalResetsIt)
{
	struct GamePlayer : ClientPlayer
	{
		GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
		~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
	};
	World world;
	GamePlayer connection;
	GCMoveOK ok;
	ok.setX(31);
	ok.setY(47);
	ok.setDir(5);
	GCMoveOKHandler::execute(&ok, &connection);
	CHECK_EQ(31, static_cast<int>(connection.getX()));
	CHECK_EQ(47, static_cast<int>(connection.getY()));
	CHECK_EQ(5, static_cast<int>(connection.getDir()));

	GCMoveError error;
	error.setX(30);
	error.setY(46);
	GCMoveErrorHandler::execute(&error, nullptr);
	CHECK(calls == std::vector<std::string>({"moveok|31|47|5", "serverpos|30|46", "moveno"}));
}

TEST(PlayerStateHandlers, MissingHostsAndEntriesMeanNoPlayer)
{
	World world;
	PlayerState::SetHost(nullptr);
	RunGuardedHandlers();
	CHECK(calls.empty());
	CHECK(PlayerState::Status() == nullptr);
	CHECK(PlayerState::GetWaitVerify() == PlayerState::WAIT_VERIFY_NULL);
	CHECK(PlayerState::GetRace() == RACE_SLAYER);
	CHECK(PlayerState::GetGear() == nullptr);
	CHECK(!PlayerState::SetAddonItem(nullptr));
	CHECK(!PlayerState::AddEffectStatus(EFFECTSTATUS_HAS_SLAYER_RELIC, 1));
	PlayerState::StopRecoveryHP();
	PlayerState::StopRecoveryMP();
	PlayerState::SetServerPosition(1, 2);
	PlayerState::PacketMoveOK(1, 2, 3);
	PlayerState::PacketMoveNO();
	CHECK(calls.empty());

	const PlayerState::Host empty{};
	CHECK(PlayerState::SetHost(&empty) == nullptr);
	RunGuardedHandlers();
	CHECK(calls.empty());
	CHECK(PlayerState::Status() == nullptr);
	CHECK(PlayerState::GetWaitVerify() == PlayerState::WAIT_VERIFY_NULL);
	CHECK(PlayerState::GetGear() == nullptr);

	CHECK(PlayerState::SetHost(&host) == &empty);
	RunGuardedHandlers();
	CHECK(calls == std::vector<std::string>({"clearwait", "wait?", "clearwait", "visible",
		"hprecovery|5|6|" + std::to_string(ConvertDurationToMillisecond(7)),
		"mprecovery|5|6|" + std::to_string(ConvertDurationToMillisecond(7))}));
	CHECK(PlayerState::GetRace() == RACE_SLAYER);
	race = RACE_OUSTERS;
	CHECK(PlayerState::GetRace() == RACE_OUSTERS);
}
