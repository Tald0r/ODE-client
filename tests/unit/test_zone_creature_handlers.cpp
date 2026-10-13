#include "test_framework.h"
#include "packet_stream_access.h"
#include "ZoneStateHost.h"
#include "PlayerStateHost.h"
#include "ConvertDuration.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MNPCScriptTable.h"
#include "MStatus.h"
#include "MStringArray.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAddHelicopter.h"
#include "Gpackets/GCAttack.h"
#include "Gpackets/GCCreatureDied.h"
#include "Gpackets/GCDeleteEffectFromTile.h"
#include "Gpackets/GCHPRecoveryEndToOthers.h"
#include "Gpackets/GCHPRecoveryStartToOthers.h"
#include "Gpackets/GCKnockBack.h"
#include "Gpackets/GCModifyNickname.h"
#include "Gpackets/GCMove.h"
#include "Gpackets/GCNPCSay.h"
#include "Gpackets/GCNPCSayDynamic.h"
#include "Gpackets/GCStatusCurrentHP.h"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Replies about the zone and the creatures in it reach them through
// ZoneState::Host and CreatureState::Host. MCreature stays the executable's,
// so each creature here is a fake behind the opaque MCreature* the zone
// hands out; the handlers never dereference it. A creature's status array
// is a real MStatus.
namespace {

std::vector<std::string> calls;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	bool alive = true;
	MStatus status;
};

MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature) { return *reinterpret_cast<FakeCreature*>(pCreature); }
std::string Who(MCreature* pCreature) { return Fake(pCreature).name; }
std::string N(long long value) { return std::to_string(value); }

bool zoneExists = true;
bool tileEffectRemoved = true;
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
TYPE_OBJECTID playerID = 0;

const ZoneState::Host zone{
	.Exists = []() { return zoneExists; },
	.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	},
	.RemoveTileEffect = [](TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int effectStatus, int serverID) {
		calls.push_back("removetileeffect|" + N(sX) + "|" + N(sY) + "|" + N(effectStatus) + "|" + N(serverID));
		return tileEffectRemoved;
	},
	.AddHelicopter = [](TYPE_OBJECTID creatureID, int x0, int y0) {
		calls.push_back("addhelicopter|" + N(creatureID) + "|" + N(x0) + "|" + N(y0));
		return true;
	},
	.RemoveHelicopter = [](TYPE_OBJECTID creatureID) { calls.push_back("removehelicopter|" + N(creatureID)); },
	.AddCorpseFromCreature = [](TYPE_OBJECTID id) { calls.push_back("corpse|" + N(id)); return true; },
};

const CreatureState::Host creature{
	.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; },
	.GetID = [](MCreature* p) { return Fake(p).id; },
	.GetName = [](MCreature* p) { return Fake(p).name.c_str(); },
	.IsAlive = [](MCreature* p) { return Fake(p).alive; },
	.SetDead = [](MCreature* p) { calls.push_back(Who(p) + ".dead"); Fake(p).alive = false; },
	.StopRecoveryHP = [](MCreature* p) { calls.push_back(Who(p) + ".stophp"); },
	.SetRecoveryHP = [](MCreature* p, int amount, int times, std::uint32_t delay) {
		calls.push_back(Who(p) + ".hprecovery|" + N(amount) + "|" + N(times) + "|" + N(delay));
	},
	.SetNickName = [](MCreature* p, BYTE type, char* szNickName) {
		calls.push_back(Who(p) + ".nickname|" + N(type) + "|" + (szNickName ? szNickName : "(null)"));
	},
	.SetChatString = [](MCreature* p, const char* str) { calls.push_back(Who(p) + ".says|" + (str ? str : "(null)")); },
	.PacketMove = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE direction) {
		calls.push_back(Who(p) + ".move|" + N(x) + "|" + N(y) + "|" + N(direction));
	},
	.PacketAttackNormal = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, BYTE direction) {
		calls.push_back(Who(p) + ".attack|" + N(x) + "|" + N(y) + "|" + N(direction));
	},
	.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) {
		calls.push_back(Who(p) + ".face|" + N(sX) + "|" + N(sY));
	},
	.SetStop = [](MCreature* p) { calls.push_back(Who(p) + ".stop"); },
	.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back(Who(p) + ".place|" + N(x) + "|" + N(y));
		return true;
	},
	.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) {
		calls.push_back(Who(p) + ".serverpos|" + N(sX) + "|" + N(sY));
	},
	.KnockBackPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) {
		calls.push_back(Who(p) + ".knockback|" + N(sX) + "|" + N(sY));
		return true;
	},
};

const PlayerState::Host player{
	.GetID = []() { return playerID; },
};

struct World
{
	FakeCreature ogre, vanda, self;
	MChatManager chat;
	MNPCScriptTable scripts;
	MStringArray nicknames;
	MChatManager* oldChat = g_pChatManager;
	MNPCScriptTable* oldScripts = g_pNPCScriptTable;
	MStringArray* oldNicknames = g_pNickNameStringTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		ogre.id = 1001; ogre.name = "Ogre";
		vanda.id = 1002; vanda.name = "Vanda";
		self.id = 7; self.name = "Self";
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}, {self.id, &self}};
		playerID = self.id;
		zoneExists = true;
		tileEffectRemoved = true;
		calls.clear();
		chat.SetAcceptMode();
		g_pChatManager = &chat;
		auto row = std::make_unique<NPC_SCRIPT>();
		row->OwnerID = "Vanda";
		row->SubjectTable.Init(2);
		row->SubjectTable.Set(0, "Welcome, traveller.");
		row->SubjectTable.Set(1, "Mind the dark.");
		row->ContentTable.Init(0);
		if (scripts.AddData(40, row.get())) row.release();
		g_pNPCScriptTable = &scripts;
		nicknames.Init(3);
		nicknames.Set(0, "Novice");
		nicknames.Set(1, "Slayer of Ogres");
		nicknames.Set(2, "Night Walker");
		g_pNickNameStringTable = &nicknames;
	}
	~World()
	{
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pNickNameStringTable = oldNicknames;
		g_pNPCScriptTable = oldScripts;
		g_pChatManager = oldChat;
		creatures.clear();
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

void ReceiveDied(ObjectID_t id)
{
	GCCreatureDied original;
	original.setObjectID(id);
	GCCreatureDied packet;
	Decode(packet, original);
	GCCreatureDiedHandler::execute(&packet, nullptr);
}

void ReceiveCurrentHP(ObjectID_t id, HP_t hp)
{
	GCStatusCurrentHP original;
	original.setObjectID(id);
	original.setCurrentHP(hp);
	GCStatusCurrentHP packet;
	Decode(packet, original);
	GCStatusCurrentHPHandler::execute(&packet, nullptr);
}

void ReceiveRecoveryStart(ObjectID_t id)
{
	GCHPRecoveryStartToOthers original;
	original.setObjectID(id);
	original.setQuantity(9);
	original.setPeriod(4);
	original.setDelay(20);
	GCHPRecoveryStartToOthers packet;
	Decode(packet, original);
	GCHPRecoveryStartToOthersHandler::execute(&packet, nullptr);
}

void ReceiveRecoveryEnd(ObjectID_t id, HP_t hp)
{
	GCHPRecoveryEndToOthers original;
	original.setObjectID(id);
	original.setCurrentHP(hp);
	GCHPRecoveryEndToOthers packet;
	Decode(packet, original);
	GCHPRecoveryEndToOthersHandler::execute(&packet, nullptr);
}

void ReceiveNickname(ObjectID_t id, BYTE type, WORD index, const std::string& name)
{
	GCModifyNickname original;
	original.setObjectID(id);
	original.getNicknameInfo().setNicknameID(5);
	original.getNicknameInfo().setNicknameType(type);
	original.getNicknameInfo().setNicknameIndex(index);
	original.getNicknameInfo().setNickname(name);
	GCModifyNickname packet;
	Decode(packet, original);
	GCModifyNicknameHandler::execute(&packet, nullptr);
}

void ReceiveMove(ObjectID_t id, Coord_t x, Coord_t y, Dir_t dir)
{
	GCMove original;
	original.setObjectID(id);
	original.setX(x);
	original.setY(y);
	original.setDir(dir);
	GCMove packet;
	Decode(packet, original);
	GCMoveHandler::execute(&packet, nullptr);
}

void ReceiveAttack(ObjectID_t id, Coord_t x, Coord_t y, Dir_t dir)
{
	GCAttack original;
	original.setObjectID(id);
	original.setX(x);
	original.setY(y);
	original.setDir(dir);
	GCAttack packet;
	Decode(packet, original);
	GCAttackHandler::execute(&packet, nullptr);
}

void ReceiveSay(ObjectID_t id, BYTE subject)
{
	GCNPCSay original;
	original.setObjectID(id);
	original.setScriptID(40);
	original.setSubjectID(subject);
	GCNPCSay packet;
	Decode(packet, original);
	GCNPCSayHandler::execute(&packet, nullptr);
}

void ReceiveSayDynamic(ObjectID_t id, const std::string& message)
{
	GCNPCSayDynamic original;
	original.setObjectID(id);
	original.setMessage(message);
	GCNPCSayDynamic packet;
	Decode(packet, original);
	GCNPCSayDynamicHandler::execute(&packet, nullptr);
}

void ReceiveKnockBack(ObjectID_t id)
{
	GCKnockBack original;
	original.setObjectID(id);
	original.setOrigin(20, 21);
	original.setTarget(23, 24);
	GCKnockBack packet;
	Decode(packet, original);
	GCKnockBackHandler::execute(&packet, nullptr);
}

void ReceiveTileEffectRemoval()
{
	GCDeleteEffectFromTile original;
	original.setXY(12, 13);
	original.setEffectID(77);
	original.setObjectID(5005);
	GCDeleteEffectFromTile packet;
	Decode(packet, original);
	GCDeleteEffectFromTileHandler::execute(&packet, nullptr);
}

void ReceiveHelicopter(ObjectID_t id, BYTE code)
{
	GCAddHelicopter original;
	original.setObjectID(id);
	original.setCode(code);
	GCAddHelicopter packet;
	Decode(packet, original);
	GCAddHelicopterHandler::execute(&packet, nullptr);
}

// Every handler once, each about a creature in the zone where it names one.
void RunEveryHandler()
{
	ReceiveTileEffectRemoval();
	ReceiveHelicopter(1001, 0);
	ReceiveDied(1001);
	ReceiveCurrentHP(1002, 50);
	ReceiveRecoveryStart(1002);
	ReceiveRecoveryEnd(1002, 60);
	ReceiveNickname(1002, NicknameInfo::NICK_CUSTOM, 0, "Lantern");
	ReceiveMove(1002, 3, 4, 2);
	ReceiveAttack(1002, 5, 6, 1);
	ReceiveSay(1002, 0);
	ReceiveSayDynamic(1002, "Hello");
	ReceiveKnockBack(1002);
}

} // namespace

TEST(ZoneCreatureHandlers, TileEffectsAndHelicoptersGoToTheZoneOnlyWhenThereIsOne)
{
	World world;
	ReceiveTileEffectRemoval();
	tileEffectRemoved = false;
	ReceiveTileEffectRemoval();
	ReceiveHelicopter(1001, 0);
	ReceiveHelicopter(1001, 1);
	ReceiveHelicopter(1002, 7);
	CHECK(calls == std::vector<std::string>({"removetileeffect|12|13|77|5005", "removetileeffect|12|13|77|5005",
		"addhelicopter|1001|0|0", "removehelicopter|1001", "removehelicopter|1002"}));

	calls.clear();
	zoneExists = false;
	ReceiveTileEffectRemoval();
	ReceiveHelicopter(1001, 0);
	CHECK(calls.empty());
}

TEST(ZoneCreatureHandlers, ADeadCreatureDiesOnceAndLeavesACorpse)
{
	World world;
	ReceiveDied(1001);
	CHECK(!world.ogre.alive);
	ReceiveDied(1001);
	ReceiveDied(4242);
	CHECK(calls == std::vector<std::string>({"Ogre.dead", "corpse|1001", "corpse|1001"}));
}

TEST(ZoneCreatureHandlers, HitPointsAndRecoveryApplyToTheNamedCreature)
{
	World world;
	ReceiveCurrentHP(1002, 50);
	CHECK_EQ(50, static_cast<long long>(world.vanda.status.GetStatus(MODIFY_CURRENT_HP)));
	ReceiveRecoveryStart(1002);
	ReceiveRecoveryEnd(1002, 60);
	CHECK_EQ(60, static_cast<long long>(world.vanda.status.GetStatus(MODIFY_CURRENT_HP)));
	CHECK(world.ogre.status.GetStatus(MODIFY_CURRENT_HP) == static_cast<DWORD>(MODIFY_NULL));
	CHECK(calls == std::vector<std::string>({"Vanda.hprecovery|9|4|" + N(ConvertDurationToMillisecond(20)), "Vanda.stophp"}));

	// Unknown creatures and a missing zone change nothing.
	calls.clear();
	ReceiveCurrentHP(4242, 1);
	ReceiveRecoveryStart(4242);
	ReceiveRecoveryEnd(4242, 1);
	zoneExists = false;
	ReceiveCurrentHP(1002, 1);
	ReceiveRecoveryStart(1002);
	ReceiveRecoveryEnd(1002, 1);
	CHECK(calls.empty());
	CHECK_EQ(60, static_cast<long long>(world.vanda.status.GetStatus(MODIFY_CURRENT_HP)));
}

TEST(ZoneCreatureHandlers, ANicknameIsTheCustomTextOrTheIndexedTitleFallingBackToTheFirst)
{
	World world;
	ReceiveNickname(1002, NicknameInfo::NICK_CUSTOM, 0, "Lantern");
	ReceiveNickname(1002, NicknameInfo::NICK_CUSTOM_FORCED, 0, "Warden");
	ReceiveNickname(1002, NicknameInfo::NICK_QUEST, 1, "");
	ReceiveNickname(1002, NicknameInfo::NICK_BUILT_IN, 3, "");
	ReceiveNickname(4242, NicknameInfo::NICK_CUSTOM, 0, "Nobody");
	CHECK(calls == std::vector<std::string>({
		"Vanda.nickname|" + N(NicknameInfo::NICK_CUSTOM) + "|Lantern",
		"Vanda.nickname|" + N(NicknameInfo::NICK_CUSTOM_FORCED) + "|Warden",
		"Vanda.nickname|" + N(NicknameInfo::NICK_QUEST) + "|Slayer of Ogres",
		"Vanda.nickname|" + N(NicknameInfo::NICK_BUILT_IN) + "|Novice"}));
}

TEST(ZoneCreatureHandlers, MovesAndAttacksAreHandedToTheCreatureFacingItsTarget)
{
	World world;
	ReceiveMove(1002, 3, 4, 2);
	ReceiveAttack(1001, 5, 6, 1);
	ReceiveMove(4242, 1, 1, 1);
	ReceiveAttack(4242, 1, 1, 1);
	CHECK(calls == std::vector<std::string>({"Vanda.move|3|4|2", "Ogre.face|5|6", "Ogre.attack|5|6|1"}));
}

TEST(ZoneCreatureHandlers, NPCsSpeakTheirScriptOrMessageUnlessTheirNameIsRefused)
{
	World world;
	ReceiveSay(1002, 1);
	ReceiveSay(1002, 9);
	ReceiveSayDynamic(1002, "Fine weather.");
	world.chat.AddID("Vanda");
	ReceiveSay(1002, 0);
	ReceiveSayDynamic(1002, "Fine weather.");
	CHECK(calls == std::vector<std::string>({"Vanda.says|Mind the dark.", "Vanda.says|Fine weather.",
		"Vanda.says|......", "Vanda.says|......"}));

	// The longest line the wire carries (a one-byte length) fits the
	// handler's 256-byte buffer whole.
	calls.clear();
	ReceiveSayDynamic(1001, std::string(255, 'a'));
	CHECK(calls == std::vector<std::string>{"Ogre.says|" + std::string(255, 'a')});
}

TEST(ZoneCreatureHandlers, AKnockBackResetsAnotherCreatureBeforeMovingItButOnlyPushesThePlayer)
{
	World world;
	ReceiveKnockBack(1002);
	ReceiveKnockBack(7);
	ReceiveKnockBack(4242);
	CHECK(calls == std::vector<std::string>({"Vanda.stop", "Vanda.place|20|21", "Vanda.serverpos|20|21",
		"Vanda.knockback|23|24", "Self.knockback|23|24"}));
}

TEST(ZoneCreatureHandlers, MissingHostsAndEntriesMeanNoZoneAndNoCreature)
{
	World world;
	ZoneState::SetHost(nullptr);
	RunEveryHandler();
	CHECK(calls.empty());
	CHECK(!ZoneState::Exists());
	CHECK(ZoneState::GetCreature(1001) == nullptr);
	CHECK(!ZoneState::RemoveTileEffect(1, 2, 3, 4));
	CHECK(!ZoneState::AddHelicopter(1, 2, 3));
	CHECK(!ZoneState::AddCorpseFromCreature(1));
	ZoneState::RemoveHelicopter(1);

	// With a zone but no creature service the zone still acts; the creatures do not.
	CHECK(ZoneState::SetHost(&zone) == nullptr);
	const CreatureState::Host empty{};
	CHECK(CreatureState::SetHost(&empty) == &creature);
	MCreature* ogre = Handle(world.ogre);
	CHECK(CreatureState::Status(ogre) == nullptr);
	CHECK_EQ(0, static_cast<long long>(CreatureState::GetID(ogre)));
	CHECK(std::string(CreatureState::GetName(ogre)).empty());
	CHECK(!CreatureState::IsAlive(ogre));
	CHECK(!CreatureState::MovePosition(ogre, 1, 2));
	CHECK(!CreatureState::KnockBackPosition(ogre, 1, 2));
	calls.clear();
	ReceiveTileEffectRemoval();
	ReceiveHelicopter(1001, 0);
	ReceiveDied(1001);
	ReceiveRecoveryStart(1002);
	ReceiveNickname(1002, NicknameInfo::NICK_CUSTOM, 0, "Lantern");
	ReceiveMove(1002, 3, 4, 2);
	ReceiveAttack(1002, 5, 6, 1);
	ReceiveSayDynamic(1002, "Hello");
	CHECK(world.ogre.alive);
	CHECK(calls == std::vector<std::string>({"removetileeffect|12|13|77|5005", "addhelicopter|1001|0|0", "corpse|1001"}));

	CHECK(CreatureState::SetHost(&creature) == &empty);
	calls.clear();
	RunEveryHandler();
	CHECK_EQ(16u, calls.size());
}
