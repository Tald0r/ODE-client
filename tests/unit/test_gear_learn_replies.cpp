#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "../basic/CMessageArray.h"
#include "BonusSkillHost.h"
#include "DialogPresentationHost.h"
#include "GameMessages.h"
#include "MGameStringTable.h"
#include "MOustersGear.h"
#include "MSkillManager.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "TempInformation.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCCannotAdd.h"
#include "Gpackets/GCLearnSkillOK.h"
#include "Gpackets/GCRemoveFromGear.h"
#include <string>
#include <vector>

// A learned skill joins its domain and the usable set, starts at level 1
// with its full reuse delay, and is announced; an ousters that already
// knows it raises its level instead. An item the server would not take
// leaves the item check buffer, back on the mouse when it was being
// dropped to the zone. A broken piece of gear comes off the player: it is
// announced with its options (not a couple ring, except for an ousters),
// the player stops and the addon it drew comes off, its description
// closes, and it is freed.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
bool ousters = false;
Race race = RACE_SLAYER;
PlayerState::ITEM_CHECK_BUFFER checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
MItem* checkItem = nullptr;
int freed = 0;
MonotonicClock::TimePoint now{MonotonicClock::Duration(1000)};

struct FakeCreature
{
	TYPE_OBJECTID id = 7;
};
FakeCreature me;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }

struct Gear : MItem
{
	ITEM_CLASS m_Class;
	Gear(ITEM_CLASS itemClass, TYPE_OBJECTID id) : m_Class(itemClass) { SetID(id); SetCurrentDurability(100); }
	~Gear() override { freed++; }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsGearItem() const override { return true; }
	int GetMaxDurability() const override { return 100; }
};
struct Helm : Gear
{
	explicit Helm(TYPE_OBJECTID id) : Gear(ITEM_CLASS_HELM, id) {}
	bool IsGearSlotHelm() const override { return true; }
};
struct Necklace : Gear
{
	explicit Necklace(TYPE_OBJECTID id) : Gear(ITEM_CLASS_NECKLACE, id) {}
	bool IsGearSlotNecklace() const override { return true; }
};
struct CoupleRing : Gear
{
	explicit CoupleRing(TYPE_OBJECTID id, ITEM_CLASS itemClass = ITEM_CLASS_COUPLE_RING) : Gear(itemClass, id) {}
	bool IsGearSlotRing() const override { return true; }
	bool IsGearSlotVampireRing() const override { return true; }
	bool IsGearSlotOustersRing() const override { return true; }
};
struct TwoHandSword : Gear
{
	explicit TwoHandSword(TYPE_OBJECTID id) : Gear(ITEM_CLASS_SWORD, id) {}
	bool IsGearSlotRightHand() const override { return true; }
	bool IsGearSlotTwoHand() const override { return true; }
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

const MItemHost items = []() {
	MItemHost host{};
	host.pCurrentTime = &now;
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetID = []() { return me.id; };
	host.GetRace = []() { return race; };
	host.GetItemCheckBufferStatus = []() { return checkStatus; };
	host.GetItemCheckBuffer = []() { return checkItem; };
	host.ClearItemCheckBuffer = []() { calls.push_back("cleared"); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsOusters = [](MCreature*) { return ousters; };
	host.SetStop = [](MCreature*) { calls.push_back("stop"); };
	host.RemoveAddon = [](MCreature*, int addon) {
		calls.push_back("addon off|" + N(addon));
		return true;
	};
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PickUpItem = [](MItem* pItem) { calls.push_back("mouse|" + N(pItem->GetID())); };
	host.RemoveDescriptor = [](const void* pItem) { calls.push_back("descriptor|" + N(static_cast<const MItem*>(pItem)->GetID())); };
	return host;
}();

const BonusSkills::Host bonus = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { calls.push_back("refresh"); };
	return host;
}();

constexpr ACTIONINFO kSkill = SKILL_DOUBLE_IMPACT;

struct World : GameModelWorld
{
	MSkillInfoTable infos;
	MSkillManager domains;
	MSkillSet usable;
	TempInformation temp;
	CMessageArray log;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MSkillInfoTable* oldInfos = g_pSkillInfoTable;
	MSkillManager* oldDomains = g_pSkillManager;
	MSkillSet* oldUsable = g_pSkillAvailable;
	TempInformation* oldTemp = g_pTempInformation;
	CMessageArray* oldLog = g_pGameMessage;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const BonusSkills::Host* oldBonus = BonusSkills::SetHost(&bonus);
	World() : GameModelWorld(3)
	{
		InitGameStringTable();
		calls.clear();
		hasPlayer = true;
		ousters = false;
		race = RACE_SLAYER;
		checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
		checkItem = nullptr;
		freed = 0;
		MItem::SetHost(&items);
		auto& row = testfw::MutableRow(infos, kSkill);
		row.Set(0, "Double Impact", 0, 0, 0, "Double Impact");
		row.maxDelay = 2500;	// under 1800 reads as no delay
		g_pSkillInfoTable = &infos;
		g_pSkillManager = &domains;
		g_pSkillAvailable = &usable;
		domains.CTypeTable<MSkillDomain>::Init(MAX_SKILLDOMAIN);
		Domain().SetRootSkill(kSkill);
		temp.SetMode(TempInformation::MODE_SKILL_LEARN);
		g_pTempInformation = &temp;
		log.Init(8, 128);
		g_pGameMessage = &log;
		const ITEM_CLASS classes[] = {ITEM_CLASS_HELM, ITEM_CLASS_NECKLACE, ITEM_CLASS_COUPLE_RING, ITEM_CLASS_VAMPIRE_COUPLE_RING,
			ITEM_CLASS_SWORD, ITEM_CLASS_VAMPIRE_COAT, ITEM_CLASS_OUSTERS_CIRCLET};
		for (ITEM_CLASS itemClass : classes)
		{
			g_pItemTable->InitClass(itemClass, 1);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).HName = "Piece";
			testfw::MutableRow(*g_pItemTable, itemClass, 0).SetGrid(1, 1);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).Race = FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS;
		}
		testfw::MutableRow(*g_pItemOptionTable, 1).Name = "Sharp";
		testfw::MutableRow(*g_pItemOptionTable, 2).Name = "Hard";
		slayerGear.Init();
		vampireGear.Init();
		oustersGear.Init();
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
	}
	~World()
	{
		BonusSkills::SetHost(oldBonus);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		MItem::SetHost(nullptr);
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pGameMessage = oldLog;
		g_pTempInformation = oldTemp;
		g_pSkillAvailable = oldUsable;
		g_pSkillManager = oldDomains;
		g_pSkillInfoTable = oldInfos;
	}
	MSkillDomain& Domain() { return testfw::MutableRow(domains, SKILLDOMAIN_BLADE); }
	SKILLINFO_NODE& Info() { return testfw::MutableRow(infos, kSkill); }
	std::vector<std::string> Messages()
	{
		std::vector<std::string> out;
		// The array holds its capacity; the messages are its filled rows.
		for (int i = 0; i < log.GetSize(); i++)
			if (log[i] != nullptr && log[i][0] != '\0') out.push_back(log[i]);
		return out;
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

void ReceiveLearned(int domain, int skill)
{
	GCLearnSkillOK original;
	original.setSkillDomainType(static_cast<SkillDomainType_t>(domain));
	original.setSkillType(static_cast<SkillType_t>(skill));
	GCLearnSkillOK packet;
	Decode(packet, original);
	GCLearnSkillOKHandler::execute(&packet, nullptr);
}

void ReceiveCannotAdd()
{
	GCCannotAdd original;
	original.setObjectID(12);
	GCCannotAdd packet;
	Decode(packet, original);
	GCCannotAddHandler::execute(&packet, nullptr);
}

void ReceiveBroken(int slot)
{
	GCRemoveFromGear original;
	original.setSlotID(static_cast<SlotID_t>(slot));
	GCRemoveFromGear packet;
	Decode(packet, original);
	GCRemoveFromGearHandler::execute(&packet, nullptr);
}

const std::string kLearned = "You learned the [Double Impact] skill.";

// A broken piece's record: its stop and addon (when it draws one), its
// description, in that order.
std::vector<std::string> Broken(int addon, TYPE_OBJECTID id)
{
	std::vector<std::string> out;
	if (addon != ADDON_NULL)
	{
		out.push_back("stop");
		out.push_back("addon off|" + N(addon));
	}
	out.push_back("descriptor|" + N(id));
	return out;
}

} // namespace

TEST(GearLearnReplies, ALearnedSkillJoinsItsDomainAndTheUsableSetAtLevelOneWithItsFullDelay)
{
	World world;
	world.Domain().SetNewSkill();
	world.Info().SetExpLevel(4);
	world.Info().SetSkillExp(99);
	ReceiveLearned(SKILLDOMAIN_BLADE, kSkill);
	CHECK(world.Domain().GetSkillStatus(kSkill) == MSkillDomain::SKILLSTATUS_LEARNED);
	CHECK(world.usable.IsEnableSkill(kSkill));
	CHECK_EQ(1, world.Info().GetExpLevel());
	CHECK_EQ(0, static_cast<int>(world.Info().GetSkillExp()));
	CHECK_EQ(2500u, static_cast<unsigned>(world.Info().GetDelayTime()));
	CHECK(!world.Info().IsAvailableTime());
	CHECK(calls == std::vector<std::string>{"refresh"});
	CHECK(world.Messages() == std::vector<std::string>{kLearned});
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(GearLearnReplies, AnOustersThatKnowsTheSkillRaisesItsLevel)
{
	World world;
	world.Domain().SetNewSkill();
	CHECK(world.Domain().LearnSkill(kSkill));
	world.Info().SetExpLevel(4);
	ousters = true;
	ReceiveLearned(SKILLDOMAIN_BLADE, kSkill);
	CHECK_EQ(5, world.Info().GetExpLevel());
	CHECK(world.Info().IsAvailableTime());
	CHECK(calls == std::vector<std::string>{"refresh"});
	CHECK(world.Messages() == std::vector<std::string>{kLearned});
	// Any other race, or no player, takes it as new again, usable again
	// though the domain has it already.
	calls.clear();
	ousters = false;
	CHECK(world.usable.RemoveSkill(kSkill));
	ReceiveLearned(SKILLDOMAIN_BLADE, kSkill);
	CHECK_EQ(1, world.Info().GetExpLevel());
	CHECK(world.usable.IsEnableSkill(kSkill));
	world.Info().SetExpLevel(4);
	hasPlayer = false;
	ousters = true;
	ReceiveLearned(SKILLDOMAIN_BLADE, kSkill);
	CHECK_EQ(1, world.Info().GetExpLevel());
}

TEST(GearLearnReplies, AnUnknownDomainOrSkillOnlyEndsTheLearning)
{
	World world;
	ReceiveLearned(MAX_SKILLDOMAIN, kSkill);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	world.temp.SetMode(TempInformation::MODE_SKILL_LEARN);
	ReceiveLearned(SKILLDOMAIN_BLADE, MIN_RESULT_ACTIONINFO);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(calls.empty());
	CHECK(world.Messages().empty());
}

TEST(GearLearnReplies, AnItemTheServerWouldNotTakeLeavesTheBuffer)
{
	World world;
	Helm helm(12);
	checkItem = &helm;
	const PlayerState::ITEM_CHECK_BUFFER cleared[] = {PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY,
		PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY,
		PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE, PlayerState::ITEM_CHECK_BUFFER_DROP_TO_CREATURE,
		PlayerState::ITEM_CHECK_BUFFER_NULL, PlayerState::ITEM_CHECK_BUFFER_TRADE_ADD};
	for (PlayerState::ITEM_CHECK_BUFFER status : cleared)
	{
		calls.clear();
		checkStatus = status;
		ReceiveCannotAdd();
		CHECK(calls == std::vector<std::string>{"cleared"});
	}
	// Dropped to the zone, it goes back on the mouse first.
	calls.clear();
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_ZONE;
	ReceiveCannotAdd();
	CHECK(calls == std::vector<std::string>({"mouse|12", "cleared"}));
	calls.clear();
	hasPlayer = false;
	ReceiveCannotAdd();
	CHECK(calls.empty());
}

TEST(GearLearnReplies, ABrokenSlayerPieceIsAnnouncedTakenOffAndFreed)
{
	World world;
	CHECK(world.slayerGear.AddItem(new Helm(21), MSlayerGear::GEAR_SLAYER_HELM));
	ReceiveBroken(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(calls == Broken(ADDON_HELM, 21));
	CHECK(world.Messages() == std::vector<std::string>{"Piece The item broke."});
	CHECK_EQ(1, freed);
	CHECK(world.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == nullptr);
	// A necklace draws nothing; options lead its name, each with a space.
	calls.clear();
	Necklace* necklace = new Necklace(22);
	necklace->AddItemOption(1);
	necklace->AddItemOption(2);
	CHECK(world.slayerGear.AddItem(necklace, MSlayerGear::GEAR_SLAYER_NECKLACE));
	ReceiveBroken(MSlayerGear::GEAR_SLAYER_NECKLACE);
	CHECK(calls == Broken(ADDON_NULL, 22));
	CHECK(world.Messages() == std::vector<std::string>({"Piece The item broke.", "Sharp Hard Piece The item broke."}));
	// A couple ring goes unannounced.
	calls.clear();
	CHECK(world.slayerGear.AddItem(new CoupleRing(23), MSlayerGear::GEAR_SLAYER_RING1));
	ReceiveBroken(MSlayerGear::GEAR_SLAYER_RING1);
	CHECK(calls == Broken(ADDON_NULL, 23));
	CHECK_EQ(static_cast<size_t>(2), world.Messages().size());
	CHECK_EQ(3, freed);
}

TEST(GearLearnReplies, ABrokenTwoHandedWeaponComesOffTheRightHand)
{
	World world;
	CHECK(world.slayerGear.AddItem(new TwoHandSword(24), MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	ReceiveBroken(MSlayerGear::GEAR_SLAYER_LEFTHAND);
	CHECK(calls == Broken(ADDON_RIGHTHAND, 24));
	CHECK_EQ(1, freed);
}

TEST(GearLearnReplies, ABrokenVampireOrOustersPieceComesOffItsGear)
{
	World world;
	race = RACE_VAMPIRE;
	CHECK(world.vampireGear.AddItem(new VampireCoat(31), MVampireGear::GEAR_VAMPIRE_COAT));
	ReceiveBroken(MVampireGear::GEAR_VAMPIRE_COAT);
	CHECK(calls == Broken(ADDON_COAT, 31));
	calls.clear();
	CHECK(world.vampireGear.AddItem(new CoupleRing(32, ITEM_CLASS_VAMPIRE_COUPLE_RING), MVampireGear::GEAR_VAMPIRE_RING1));
	ReceiveBroken(MVampireGear::GEAR_VAMPIRE_RING1);
	CHECK(calls == Broken(ADDON_NULL, 32));
	CHECK_EQ(static_cast<size_t>(1), world.Messages().size());
	// An ousters announces even a couple ring.
	calls.clear();
	race = RACE_OUSTERS;
	CHECK(world.oustersGear.AddItem(new Circlet(33), MOustersGear::GEAR_OUSTERS_CIRCLET));
	CHECK(world.oustersGear.AddItem(new CoupleRing(34), MOustersGear::GEAR_OUSTERS_RING1));
	ReceiveBroken(MOustersGear::GEAR_OUSTERS_CIRCLET);
	ReceiveBroken(MOustersGear::GEAR_OUSTERS_RING1);
	CHECK(calls == std::vector<std::string>({"descriptor|33", "descriptor|34"}));
	CHECK_EQ(static_cast<size_t>(3), world.Messages().size());
	CHECK_EQ(4, freed);
}

TEST(GearLearnReplies, AnEmptySlotOrNoPlayerTakesNothingOff)
{
	World world;
	ReceiveBroken(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(world.slayerGear.AddItem(new Helm(21), MSlayerGear::GEAR_SLAYER_HELM));
	hasPlayer = false;
	ReceiveBroken(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(calls.empty());
	CHECK(world.Messages().empty());
	CHECK(world.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) != nullptr);
}
