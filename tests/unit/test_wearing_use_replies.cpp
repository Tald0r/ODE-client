#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "AffectModifyInfo.h"
#include "BonusSkillHost.h"
#include "MOustersGear.h"
#include "MSlayerGear.h"
#include "MStatus.h"
#include "MVampireGear.h"
#include "PlayerStateHost.h"
#include "Gpackets/GCRealWearingInfo.h"
#include "Gpackets/GCUseOK.h"
#include <string>
#include <vector>

// Which worn items count: a bit per gear slot says whether the item worn
// there adds its values. Each item of the player's race gear is marked
// to count or not (a blood bible sign is left alone); a slayer's item
// also adds to or leaves the player, and a weapon that changes whether
// it counts refreshes the skills the player can use. Then the player's
// status is worked out again. An item use the server allowed spends the
// item and changes the player's status; a skill card it allowed is spent.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
Race race = RACE_SLAYER;
MStatus playerStatus;
int me;

std::string N(long long value) { return std::to_string(value); }

struct Worn : MItem
{
	ITEM_CLASS m_Class;
	Worn(ITEM_CLASS itemClass, TYPE_OBJECTID id) : m_Class(itemClass) { SetID(id); }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsGearItem() const override { return true; }
};
struct Helm : Worn
{
	explicit Helm(TYPE_OBJECTID id) : Worn(ITEM_CLASS_HELM, id) {}
	bool IsGearSlotHelm() const override { return true; }
};
struct Sword : Worn
{
	explicit Sword(TYPE_OBJECTID id) : Worn(ITEM_CLASS_SWORD, id) {}
	bool IsGearSlotRightHand() const override { return true; }
};
struct Bible : Worn
{
	explicit Bible(TYPE_OBJECTID id) : Worn(ITEM_CLASS_BLOOD_BIBLE_SIGN, id) {}
	bool IsGearSlotBloodBible() const override { return true; }
};
struct VampireCoat : Worn
{
	explicit VampireCoat(TYPE_OBJECTID id) : Worn(ITEM_CLASS_VAMPIRE_COAT, id) {}
	bool IsGearSlotVampireCoat() const override { return true; }
};
struct Circlet : Worn
{
	explicit Circlet(TYPE_OBJECTID id) : Worn(ITEM_CLASS_OUSTERS_CIRCLET, id) {}
	bool IsGearSlotOustersCirclet() const override { return true; }
};
struct OustersCoat : Worn
{
	explicit OustersCoat(TYPE_OBJECTID id) : Worn(ITEM_CLASS_OUSTERS_COAT, id) {}
	bool IsGearSlotOustersCoat() const override { return true; }
};

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.GetRace = []() { return race; };
	host.Status = []() { return &playerStatus; };
	host.SetAddonItem = [](MItem* pItem) { calls.push_back("adds|" + N(pItem->GetID())); return true; };
	host.RemoveAddonItem = [](MItem* pItem) { calls.push_back("no longer adds|" + N(pItem->GetID())); return true; };
	host.CalculateStatus = []() { calls.push_back("status worked out"); };
	host.UseItemOK = []() { calls.push_back("item spent"); };
	host.UseSkillCardOK = [](int cardType) { calls.push_back("card spent|" + N(cardType)); };
	return host;
}();

const BonusSkills::Host skills = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { calls.push_back("skills refreshed"); };
	return host;
}();

struct World : GameModelWorld
{
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	Helm* helm = new Helm(41);
	Sword* sword = new Sword(42);
	Bible* bible = new Bible(43);
	VampireCoat* coat = new VampireCoat(51);
	Circlet* circlet = new Circlet(61);
	OustersCoat* oustersCoat = new OustersCoat(62);
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const BonusSkills::Host* oldSkills = BonusSkills::SetHost(&skills);
	World()
	{
		calls.clear();
		hasPlayer = true;
		race = RACE_SLAYER;
		playerStatus = MStatus();
		for (ITEM_CLASS itemClass : {ITEM_CLASS_HELM, ITEM_CLASS_SWORD, ITEM_CLASS_BLOOD_BIBLE_SIGN, ITEM_CLASS_VAMPIRE_COAT, ITEM_CLASS_OUSTERS_CIRCLET, ITEM_CLASS_OUSTERS_COAT})
		{
			g_pItemTable->InitClass(itemClass, 1);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).SetGrid(1, 1);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).Race = FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS;
		}
		slayerGear.Init();
		vampireGear.Init();
		oustersGear.Init();
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
		CHECK(slayerGear.AddItem(helm, MSlayerGear::GEAR_SLAYER_HELM));
		CHECK(slayerGear.AddItem(sword, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
		CHECK(slayerGear.AddItem(bible, MSlayerGear::GEAR_SLAYER_BLOOD_BIBLE1));
		CHECK(vampireGear.AddItem(coat, MVampireGear::GEAR_VAMPIRE_COAT));
		CHECK(oustersGear.AddItem(circlet, MOustersGear::GEAR_OUSTERS_CIRCLET));
		CHECK(oustersGear.AddItem(oustersCoat, MOustersGear::GEAR_OUSTERS_COAT));
		for (MItem* pItem : {static_cast<MItem*>(helm), static_cast<MItem*>(sword), static_cast<MItem*>(bible),
			static_cast<MItem*>(coat), static_cast<MItem*>(circlet), static_cast<MItem*>(oustersCoat)})
			pItem->UnSetAffectStatus();
		calls.clear();
	}
	~World()
	{
		BonusSkills::SetHost(oldSkills);
		PlayerState::SetHost(oldPlayer);
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
	}
};

constexpr DWORD kHelmBit = 1u << MSlayerGear::GEAR_SLAYER_HELM;
constexpr DWORD kWeaponBit = 1u << MSlayerGear::GEAR_SLAYER_RIGHTHAND;

void ReceiveWearing(DWORD flag)
{
	GCRealWearingInfo packet;
	packet.setInfo(flag);
	GCRealWearingInfoHandler::execute(&packet, nullptr);
}

} // namespace

TEST(WearingUseReplies, ASlayersItemsCountAsTheBitsSay)
{
	World world;
	static_assert(kWeaponBit == 0x10, "the weapon is the right hand's bit");
	// Everything counts: each item adds, the weapon that did not count
	// now does, so the skills are refreshed; the bible is left alone.
	ReceiveWearing(0xFFFFFFFF);
	CHECK(calls == std::vector<std::string>({"adds|41", "adds|42", "skills refreshed", "status worked out"}));
	CHECK(world.helm->IsAffectStatus() && world.sword->IsAffectStatus());
	CHECK(!world.bible->IsAffectStatus());
	// Only the helm counts: the weapon stops, so the skills are refreshed.
	calls.clear();
	ReceiveWearing(kHelmBit);
	CHECK(calls == std::vector<std::string>({"adds|41", "no longer adds|42", "skills refreshed", "status worked out"}));
	CHECK(world.helm->IsAffectStatus() && !world.sword->IsAffectStatus());
	// The weapon still does not count: no refresh.
	calls.clear();
	ReceiveWearing(0);
	CHECK(calls == std::vector<std::string>({"no longer adds|41", "no longer adds|42", "status worked out"}));
	CHECK(!world.helm->IsAffectStatus());
	// The weapon's bit alone: only it counts, and the skills are refreshed.
	calls.clear();
	ReceiveWearing(kWeaponBit);
	CHECK(calls == std::vector<std::string>({"no longer adds|41", "adds|42", "skills refreshed", "status worked out"}));
}

TEST(WearingUseReplies, AVampiresOrOustersItemsOnlyCountOrNot)
{
	World world;
	race = RACE_VAMPIRE;
	ReceiveWearing(1u << MVampireGear::GEAR_VAMPIRE_COAT);
	CHECK(world.coat->IsAffectStatus());
	ReceiveWearing(~(1u << MVampireGear::GEAR_VAMPIRE_COAT));
	CHECK(!world.coat->IsAffectStatus());
	race = RACE_OUSTERS;
	ReceiveWearing(1u << MOustersGear::GEAR_OUSTERS_CIRCLET);
	CHECK(world.circlet->IsAffectStatus());
	ReceiveWearing(0);
	CHECK(!world.circlet->IsAffectStatus());
	// Each slot reads its own bit.
	ReceiveWearing(1u << MOustersGear::GEAR_OUSTERS_COAT);
	CHECK(!world.circlet->IsAffectStatus());
	CHECK(world.oustersCoat->IsAffectStatus());
	// Only the status is worked out again; the slayer's gear is untouched.
	CHECK(calls == std::vector<std::string>(5, "status worked out"));
	CHECK(!world.helm->IsAffectStatus());
}

TEST(WearingUseReplies, WithoutAPlayerNothingCounts)
{
	World world;
	hasPlayer = false;
	ReceiveWearing(0xFFFFFFFF);
	CHECK(calls.empty());
	CHECK(!world.helm->IsAffectStatus());
}

TEST(WearingUseReplies, AnAllowedItemUseIsSpentAndChangesThePlayer)
{
	World world;
	GCUseOK packet;
	packet.addShortData(MODIFY_CURRENT_HP, 77);
	GCUseOKHandler::execute(&packet, nullptr);
	CHECK(calls == std::vector<std::string>{"item spent"});
	CHECK_EQ(77, static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)));
	// Without the player's status the item is still spent.
	calls.clear();
	PlayerState::Host noStatus = player;
	noStatus.Status = nullptr;
	PlayerState::SetHost(&noStatus);
	GCUseOK other;
	other.addShortData(MODIFY_CURRENT_HP, 12);
	GCUseOKHandler::execute(&other, nullptr);
	CHECK(calls == std::vector<std::string>{"item spent"});
	CHECK_EQ(77, static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)));
}

TEST(WearingUseReplies, AnAllowedSkillCardIsSpent)
{
	World world;
	const long long hp = static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP));
	GCUseSkillCardOK packet;
	packet.setCardType(3);
	GCUseSkillCardOKHandler::execute(&packet, nullptr);
	CHECK(calls == std::vector<std::string>{"card spent|3"});
	CHECK_EQ(hp, static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)));
	// Without a host, nothing.
	calls.clear();
	PlayerState::SetHost(nullptr);
	GCUseSkillCardOKHandler::execute(&packet, nullptr);
	GCUseOK use;
	GCUseOKHandler::execute(&use, nullptr);
	CHECK(calls.empty());
	CHECK(!PlayerState::RemoveAddonItem(world.helm));
}
