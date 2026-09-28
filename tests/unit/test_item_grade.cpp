//----------------------------------------------------------------------
// test_item_grade.cpp
//----------------------------------------------------------------------
//
// What a gear item's grade does to it, class by class. The server
// keeps one table of it (decore::gradePolicyOf, hasDurability and
// gradeOffsets, vendored in third_party/decore and pinned by the
// item_grade.tsv rows decore_tests asserts): each wire item class has a
// grade policy, which says how far each grade step moves the item's
// durability, damage, critical, defense and protection and its luck,
// and either keeps a durability of its own or does not.
//
// Every gear class is built here through the real MItem subclass, or a
// stand-in of its family that answers its class where the real one
// cannot be linked into a test binary: the two couple rings, whose use
// bodies are executable-side, and the four guns (see kGunsAddNothing).
// Every getter the grade moves is compared, at grades on both sides of
// 4, with the server's rule for that class. The client shows only what a policy
// moves: damage and critical for a weapon policy (-1 otherwise, which
// the item description hides), luck for an accessory policy (-9999
// otherwise), and for the armor policies the defense and protection
// with the offset and floored at 0; any other gear shows the table's
// defense and protection as they are.
//
// kClientRules lists the classes whose client rule is not the server's
// table, with the policy and durability the client applies to them.
// kInfoWithoutDurability lists the classes whose server item info has
// no durability to read, so their maximum starts from 1, not the table.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MPriceManager.h"

#include "domain/ItemDurability.h"
#include "domain/ItemGrade.h"

#include <algorithm>
#include <string>
#include <type_traits>

namespace {

//----------------------------------------------------------------------
// Two table rows for every class: type 0 with ordinary values, type 1
// with a durability of 0 and negative values, which the floors at 1
// and 0 act on and a raw getter passes through.
//----------------------------------------------------------------------
struct Row
{
	int		durability;		// Value1
	int		protection;		// Value2
	int		minDamage;		// Value3
	int		maxDamage;		// Value4
	int		defense;		// Value6
	int		critical;		// CriticalHit
};

const Row	kRows[] = {
	{ 3000, 10, 20, 30, 12, 5 },
	{ 0, -3, -2, -1, -3, -4 },
};
const int	kRowCount = (int)(sizeof(kRows) / sizeof(kRows[0]));

const int	kGrades[] = { -1, 0, 1, 3, 4, 5, 6, 7, 10 };

// Every row's table price.
const int	kPrice = 100000;

struct GradeWorld : GameModelWorld
{
	GradeWorld()
	{
		for (int c = 0; c < MAX_ITEM_CLASS; c++)
		{
			g_pItemTable->InitClass(c, kRowCount);
			for (int t = 0; t < kRowCount; t++)
			{
				ITEMTABLE_INFO& info = testfw::MutableRow(*g_pItemTable, c, t);
				info.Value1 = kRows[t].durability;
				info.Value2 = kRows[t].protection;
				info.Value3 = kRows[t].minDamage;
				info.Value4 = kRows[t].maxDamage;
				info.Value6 = kRows[t].defense;
				info.CriticalHit = kRows[t].critical;
				info.Price = kPrice;
			}
		}
	}
};

//----------------------------------------------------------------------
// A stand-in for a class whose real type cannot be built in a test
// binary: its family, answering its class.
//----------------------------------------------------------------------
template <class Family, ITEM_CLASS Class>
struct StandIn : public Family
{
	ITEM_CLASS	GetItemClass() const	{ return Class; }
	static MItem*	NewItem()			{ return new StandIn; }
};

//----------------------------------------------------------------------
// The guns stand in as their family, MWeaponItem. A real gun's vtable
// is emitted wherever the gun is built, and it carries MGunItem's
// inline GetMagazineSize and destructor, which call through an
// MMagazine*; under UBSan's vptr check that needs MMagazine's typeinfo,
// which the executable alone defines (MItemUse.cpp holds its key
// function), so unit_tests would not link. A stand-in has none of that,
// and the grade reaches a gun only through the getters it inherits:
// each one below is declared above MGunItem (a pointer to member names
// the class that declares it, so an override in a gun class would fail
// the assertion). decltype evaluates nothing, so no vtable is emitted.
//----------------------------------------------------------------------
template <class Gun>
constexpr bool	AddsNothingTheGradeMoves()
{
	return std::is_same_v<decltype(&Gun::GetMaxDurability), decltype(&MWeaponItem::GetMaxDurability)>
		&& std::is_same_v<decltype(&Gun::GetMinDamage), decltype(&MWeaponItem::GetMinDamage)>
		&& std::is_same_v<decltype(&Gun::GetMaxDamage), decltype(&MWeaponItem::GetMaxDamage)>
		&& std::is_same_v<decltype(&Gun::GetCriticalHit), decltype(&MWeaponItem::GetCriticalHit)>
		&& std::is_same_v<decltype(&Gun::GetDefenseValue), decltype(&MWeaponItem::GetDefenseValue)>
		&& std::is_same_v<decltype(&Gun::GetProtectionValue), decltype(&MWeaponItem::GetProtectionValue)>
		&& std::is_same_v<decltype(&Gun::GetLucky), decltype(&MWeaponItem::GetLucky)>
		&& std::is_same_v<decltype(&Gun::SetGrade), decltype(&MWeaponItem::SetGrade)>
		&& std::is_same_v<decltype(&Gun::GetGrade), decltype(&MWeaponItem::GetGrade)>
		&& std::is_same_v<decltype(&Gun::SetItemType), decltype(&MWeaponItem::SetItemType)>
		&& std::is_same_v<decltype(&Gun::IsGearItem), decltype(&MWeaponItem::IsGearItem)>;
}

constexpr bool	kGunsAddNothing = AddsNothingTheGradeMoves<MGunSG>()
								&& AddsNothingTheGradeMoves<MGunSMG>()
								&& AddsNothingTheGradeMoves<MGunAR>()
								&& AddsNothingTheGradeMoves<MGunTR>();
static_assert(kGunsAddNothing, "a gun class overrides a getter the grade moves: build it for real");

struct GearClass
{
	ITEM_CLASS	itemClass;
	const char*	name;
	MItem*		(*make)();
};

const GearClass	kGearClasses[] = {
	{ ITEM_CLASS_RING,					"Ring",					&MRing::NewItem },
	{ ITEM_CLASS_BRACELET,				"Bracelet",				&MBracelet::NewItem },
	{ ITEM_CLASS_NECKLACE,				"Necklace",				&MNecklace::NewItem },
	{ ITEM_CLASS_COAT,					"Coat",					&MCoat::NewItem },
	{ ITEM_CLASS_TROUSER,				"Trouser",				&MTrouser::NewItem },
	{ ITEM_CLASS_SHOES,					"Shoes",				&MShoes::NewItem },
	{ ITEM_CLASS_SWORD,					"Sword",				&MSword::NewItem },
	{ ITEM_CLASS_BLADE,					"Blade",				&MBlade::NewItem },
	{ ITEM_CLASS_SHIELD,				"Shield",				&MShield::NewItem },
	{ ITEM_CLASS_CROSS,					"Cross",				&MCross::NewItem },
	{ ITEM_CLASS_GLOVE,					"Glove",				&MGlove::NewItem },
	{ ITEM_CLASS_HELM,					"Helm",					&MHelm::NewItem },
	{ ITEM_CLASS_SG,					"SG",					&StandIn<MWeaponItem, ITEM_CLASS_SG>::NewItem },
	{ ITEM_CLASS_SMG,					"SMG",					&StandIn<MWeaponItem, ITEM_CLASS_SMG>::NewItem },
	{ ITEM_CLASS_AR,					"AR",					&StandIn<MWeaponItem, ITEM_CLASS_AR>::NewItem },
	{ ITEM_CLASS_SR,					"SR",					&StandIn<MWeaponItem, ITEM_CLASS_SR>::NewItem },
	{ ITEM_CLASS_BELT,					"Belt",					&MBelt::NewItem },
	{ ITEM_CLASS_VAMPIRE_RING,			"VampireRing",			&MVampireRing::NewItem },
	{ ITEM_CLASS_VAMPIRE_BRACELET,		"VampireBracelet",		&MVampireBracelet::NewItem },
	{ ITEM_CLASS_VAMPIRE_NECKLACE,		"VampireNecklace",		&MVampireNecklace::NewItem },
	{ ITEM_CLASS_VAMPIRE_COAT,			"VampireCoat",			&MVampireCoat::NewItem },
	{ ITEM_CLASS_MACE,					"Mace",					&MMace::NewItem },
	{ ITEM_CLASS_VAMPIRE_EARRING,		"VampireEarring",		&MVampireEarRing::NewItem },
	{ ITEM_CLASS_VAMPIRE_WEAPON,		"VampireWeapon",		&MVampireWeapon::NewItem },
	{ ITEM_CLASS_VAMPIRE_AMULET,		"VampireAmulet",		&MVampireAmulet::NewItem },
	{ ITEM_CLASS_COUPLE_RING,			"CoupleRing",			&StandIn<MRing, ITEM_CLASS_COUPLE_RING>::NewItem },
	{ ITEM_CLASS_VAMPIRE_COUPLE_RING,	"VampireCoupleRing",	&StandIn<MVampireRing, ITEM_CLASS_VAMPIRE_COUPLE_RING>::NewItem },
	{ ITEM_CLASS_OUSTERS_ARMSBAND,		"OustersArmsband",		&MOustersArmsBand::NewItem },
	{ ITEM_CLASS_OUSTERS_BOOTS,			"OustersBoots",			&MOustersBoots::NewItem },
	{ ITEM_CLASS_OUSTERS_CHAKRAM,		"OustersChakram",		&MOustersChakram::NewItem },
	{ ITEM_CLASS_OUSTERS_CIRCLET,		"OustersCirclet",		&MOustersCirclet::NewItem },
	{ ITEM_CLASS_OUSTERS_COAT,			"OustersCoat",			&MOustersCoat::NewItem },
	{ ITEM_CLASS_OUSTERS_PENDENT,		"OustersPendent",		&MOustersPendent::NewItem },
	{ ITEM_CLASS_OUSTERS_RING,			"OustersRing",			&MOustersRing::NewItem },
	{ ITEM_CLASS_OUSTERS_STONE,			"OustersStone",			&MOustersStone::NewItem },
	{ ITEM_CLASS_OUSTERS_WRISTLET,		"OustersWristlet",		&MOustersWristlet::NewItem },
	{ ITEM_CLASS_CORE_ZAP,				"CoreZap",				&MCoreZap::NewItem },
	{ ITEM_CLASS_CARRYING_RECEIVER,		"CarryingReceiver",		&MCarryingReceiver::NewItem },
	{ ITEM_CLASS_SHOULDER_ARMOR,		"ShoulderArmor",		&MShoulderArmor::NewItem },
	{ ITEM_CLASS_DERMIS,				"Dermis",				&MDermis::NewItem },
	{ ITEM_CLASS_PERSONA,				"Persona",				&MPersona::NewItem },
	{ ITEM_CLASS_FASCIA,				"Fascia",				&MFascia::NewItem },
	{ ITEM_CLASS_MITTEN,				"Mitten",				&MMitten::NewItem },
};

//----------------------------------------------------------------------
// The classes whose client rule is not the server's table.
//----------------------------------------------------------------------
struct ClientRule
{
	ITEM_CLASS				itemClass;
	decore::GradePolicy		policy;
	bool					hasDurability;
};

const ClientRule	kClientRules[] = {
	// The server builds the couple rings outside ConcreteItem, so its
	// table has no grade and no durability for them; the client keeps
	// the ring's rule, and quotes no repair from it
	// (TheCoupleRingsQuoteNoRepair; docs/compiler-warnings-2026-09-27.md,
	// "Shop prices").
	{ ITEM_CLASS_COUPLE_RING,			decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_VAMPIRE_COUPLE_RING,	decore::GradePolicy::Accessory,	true },
};

//----------------------------------------------------------------------
// The gear classes whose server item info reads no durability column:
// Dermis, Fascia and CarryingReceiver load GearInfoNoDurabilityRow
// (loadGearInfosNoDurability), CoreZap's info has no durability field,
// and none of the four overrides ItemInfo::getDurability, which returns
// 1. ConcreteItem::getMaxDurability starts from that 1, whatever the
// item table holds; the client's row is not read.
//----------------------------------------------------------------------
const ITEM_CLASS	kInfoWithoutDurability[] = {
	ITEM_CLASS_CORE_ZAP,
	ITEM_CLASS_CARRYING_RECEIVER,
	ITEM_CLASS_DERMIS,
	ITEM_CLASS_FASCIA,
};

// The durability the server's item info gives the class's row.
int	InfoDurability(ITEM_CLASS itemClass, const Row& row)
{
	for (ITEM_CLASS infoless : kInfoWithoutDurability)
	{
		if (infoless == itemClass)
			return 1;
	}
	return row.durability;
}

ClientRule	RuleOf(ITEM_CLASS itemClass)
{
	for (const ClientRule& rule : kClientRules)
	{
		if (rule.itemClass == itemClass)
			return rule;
	}
	ClientRule rule = { itemClass, decore::gradePolicyOf(itemClass), decore::hasDurability(itemClass) };
	return rule;
}

//----------------------------------------------------------------------
// What the client shows for one class, type and grade.
//----------------------------------------------------------------------
struct Shown
{
	int		maxDurability;
	int		minDamage;
	int		maxDamage;
	int		critical;
	int		defense;
	int		protection;
	int		luck;
};

Shown	Expected(const ClientRule& rule, const Row& row, int grade)
{
	const decore::GradeOffsets offsets = decore::gradeOffsets(rule.policy, grade);
	const bool weapon = rule.policy == decore::GradePolicy::Weapon;
	const bool armor = rule.policy == decore::GradePolicy::Cloth || rule.policy == decore::GradePolicy::Grocery;

	Shown shown;
	shown.maxDurability = (int)decore::maxDurabilityBase((unsigned)InfoDurability(rule.itemClass, row), rule.hasDurability,
		offsets.durability);
	shown.minDamage = weapon ? (std::max)(1, row.minDamage + offsets.damage) : -1;
	shown.maxDamage = weapon ? (std::max)(1, row.maxDamage + offsets.damage) : -1;
	shown.critical = weapon ? (std::max)(0, row.critical + offsets.critical) : -1;
	shown.defense = armor ? (std::max)(0, row.defense + offsets.defense) : row.defense;
	shown.protection = armor ? (std::max)(0, row.protection + offsets.protection) : row.protection;
	shown.luck = rule.policy == decore::GradePolicy::Accessory ? offsets.luck : -9999;
	return shown;
}

void	CheckShown(const char* name, int type, int grade, const char* what, int expected, int actual)
{
	::testfw::RecordCheck();
	if (expected != actual)
	{
		const std::string message = std::string(name) + " type " + std::to_string(type) + " grade "
			+ std::to_string(grade) + ": " + what + " expected " + std::to_string(expected)
			+ ", actual " + std::to_string(actual);
		::testfw::RecordFailure(__FILE__, __LINE__, message.c_str());
	}
}

} // namespace

//----------------------------------------------------------------------
// Every gear class, every attribute the grade moves
//----------------------------------------------------------------------
TEST(ItemGrade, EveryGearClassFollowsItsGradeRule)
{
	GradeWorld world;

	for (const GearClass& gear : kGearClasses)
	{
		const ClientRule rule = RuleOf(gear.itemClass);
		for (int type = 0; type < kRowCount; type++)
		{
			for (int grade : kGrades)
			{
				MItem* pItem = gear.make();
				CHECK_EQ((int)gear.itemClass, (int)pItem->GetItemClass());
				CHECK(pItem->IsGearItem());
				pItem->SetItemType(type);
				pItem->SetGrade(grade);

				const Shown shown = Expected(rule, kRows[type], grade);
				CheckShown(gear.name, type, grade, "max durability", shown.maxDurability, pItem->GetMaxDurability());
				CheckShown(gear.name, type, grade, "min damage", shown.minDamage, pItem->GetMinDamage());
				CheckShown(gear.name, type, grade, "max damage", shown.maxDamage, pItem->GetMaxDamage());
				CheckShown(gear.name, type, grade, "critical", shown.critical, pItem->GetCriticalHit());
				CheckShown(gear.name, type, grade, "defense", shown.defense, pItem->GetDefenseValue());
				CheckShown(gear.name, type, grade, "protection", shown.protection, pItem->GetProtectionValue());
				CheckShown(gear.name, type, grade, "luck", shown.luck, pItem->GetLucky());

				delete pItem;
			}
		}
	}
}

//----------------------------------------------------------------------
// The blood bible sign: an item the client makes itself
//----------------------------------------------------------------------
// The client builds a blood bible sign from GCBloodBibleSignInfo; the
// server has no such item, so the table has no rule for it. It keeps
// the gear rule it was written with: a durability the grade moves 1000
// a step, and nothing else graded.
//----------------------------------------------------------------------
TEST(ItemGrade, TheBloodBibleSignKeepsTheGearRule)
{
	GradeWorld world;

	for (int type = 0; type < kRowCount; type++)
	{
		for (int grade : kGrades)
		{
			MBloodBibleSign sign;
			sign.SetItemType(type);
			sign.SetGrade(grade);

			const Row& row = kRows[type];
			CheckShown("BloodBibleSign", type, grade, "max durability",
				(std::max)(1000, row.durability + (grade - 4) * 1000), sign.GetMaxDurability());
			CheckShown("BloodBibleSign", type, grade, "min damage", -1, sign.GetMinDamage());
			CheckShown("BloodBibleSign", type, grade, "critical", -1, sign.GetCriticalHit());
			CheckShown("BloodBibleSign", type, grade, "defense", row.defense, sign.GetDefenseValue());
			CheckShown("BloodBibleSign", type, grade, "protection", row.protection, sign.GetProtectionValue());
			CheckShown("BloodBibleSign", type, grade, "luck", -9999, sign.GetLucky());
		}
	}
}

//----------------------------------------------------------------------
// The motorcycle: a durability the server never bounds
//----------------------------------------------------------------------
// The server builds the motorcycle outside ConcreteItem and reports a
// maximum durability of 1 for it, a placeholder its price reads as "no
// maximum" (getPrice discounts wear only above 1); it stores and sends
// the durability itself unbounded. The client shows the table's
// durability scaled by the durability options instead, capped at 65000,
// and the grade moves nothing. It is not maxDurabilityBase, which would
// floor it at 1000 and give no cap.
//----------------------------------------------------------------------
TEST(ItemGrade, TheMotorcycleShowsTheTableDurabilityWhateverTheGrade)
{
	GradeWorld world;

	for (int grade : kGrades)
	{
		MMotorcycle motorcycle;
		motorcycle.SetItemType(0);
		motorcycle.SetGrade(grade);
		CheckShown("Motorcycle", 0, grade, "max durability", 3000, motorcycle.GetMaxDurability());

		MMotorcycle zero;
		zero.SetItemType(1);
		zero.SetGrade(grade);
		CheckShown("Motorcycle", 1, grade, "max durability", 0, zero.GetMaxDurability());
	}

	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MOTORCYCLE, 0).Value1 = 70000;
	MMotorcycle strong;
	strong.SetItemType(0);
	CHECK_EQ(65000, strong.GetMaxDurability());
}

//----------------------------------------------------------------------
// A class without durability is priced without wear
//----------------------------------------------------------------------
// The server keeps no durability for VampireAmulet: its durability
// reads 1, whatever the item has been through, and that is what it
// sends, while its maximum is the table's durability, untouched by the
// grade. So the shop prices it at 1 / maximum of its price (getPrice)
// and quotes a repair of nearly a tenth of it (getRepairPrice), and the
// repair changes nothing. The expected prices are the server's rule
// (decore::itemPrice and repairPrice) on that maximum: at grade 0 the
// table price 100000 is 80000 before the wear.
//----------------------------------------------------------------------
TEST(ItemGrade, TheVampireAmuletIsPricedOnTheTableDurability)
{
	GradeWorld world;
	MPriceManager prices;

	MVampireAmulet amulet;
	amulet.SetItemType(0);
	amulet.SetGrade(0);
	amulet.SetCurrentDurability(1);

	// 3000, not the 1000 the grade used to leave after the floor.
	CHECK_EQ(3000, amulet.GetMaxDurability());
	// 80000 * 1 / 3000 is 26.7: 26, where 1000 made it 80.
	CHECK_EQ(26, prices.GetItemPrice(&amulet, MPriceManager::NPC_TO_PC));
	// (80000 - 26.7) / 10 is 7997.3: 7997, where 1000 made it 7992.
	CHECK_EQ(7997, prices.GetItemPrice(&amulet, MPriceManager::REPAIR));
}

//----------------------------------------------------------------------
// Gear the server keeps no durability for is priced at full
//----------------------------------------------------------------------
// Dermis, Fascia, CarryingReceiver and CoreZap keep no durability on the
// server, and their item info has none to read (kInfoWithoutDurability):
// the maximum is ItemInfo's 1, whatever the item table's row holds, and
// the durability the server sends is 1. So the shop never discounts
// wear (getPrice discounts only above a maximum of 1), and a repair
// costs 0 (getRepairPrice returns 0 when the durability is the
// maximum). The client's table rows are not the server's, so both rows
// are checked: a durability of 3000 and one of 0 give the same answer.
// The grade is 4, so the price is the table's 100000 as it is. The
// server also refuses a single repair of the first three
// (isRepairableItem); the quote of 0 is what its repair-all charges.
//----------------------------------------------------------------------
TEST(ItemGrade, GearWithoutDurabilityIsPricedWithoutWear)
{
	GradeWorld world;
	MPriceManager prices;

	MDermis dermis;
	MFascia fascia;
	MCarryingReceiver receiver;
	MCoreZap coreZap;
	const struct { MItem* pItem; const char* name; } infoless[] = {
		{ &dermis, "Dermis" },
		{ &fascia, "Fascia" },
		{ &receiver, "CarryingReceiver" },
		{ &coreZap, "CoreZap" },
	};

	for (const auto& gear : infoless)
	{
		MItem* const pItem = gear.pItem;
		const char* const name = gear.name;
		for (int type = 0; type < kRowCount; type++)
		{
			pItem->SetItemType(type);		// table durability 3000, then 0
			pItem->SetGrade(4);
			pItem->SetCurrentDurability(1);

			CheckShown(name, type, 4, "max durability", 1, pItem->GetMaxDurability());
			CheckShown(name, type, 4, "buy price", kPrice, prices.GetItemPrice(pItem, MPriceManager::NPC_TO_PC));
			CheckShown(name, type, 4, "sell price", kPrice / 4, prices.GetItemPrice(pItem, MPriceManager::PC_TO_NPC));
			CheckShown(name, type, 4, "repair", 0, prices.GetItemPrice(pItem, MPriceManager::REPAIR));
		}
	}
}

//----------------------------------------------------------------------
// A couple ring's repair costs nothing
//----------------------------------------------------------------------
// The server builds the couple rings outside ConcreteItem, so Item's
// defaults give them a grade of -1, a durability of 1 and a maximum of
// 1, and that is what it sends. Its repair price is 0 (getRepairPrice:
// the durability is the maximum). Its repair of all the worn gear
// (CGRequestRepairHandler::executeAll) adds that 0 for a worn couple
// ring, and it refuses to repair one on its own (isRepairableItem). The
// client keeps the ring's rule for their maximum (kClientRules), at
// least 1000, so it quoted a repair of nearly a tenth of the price. The
// gear window adds every worn item's quote into the repair-all total it
// asks the player to confirm (VS_UI_GameCommon.cpp), and opens that
// dialog whenever the total is above 0, so the ring alone opened it.
// The quote is the server's 0 at any durability the client holds.
//----------------------------------------------------------------------
TEST(ItemGrade, TheCoupleRingsQuoteNoRepair)
{
	GradeWorld world;
	MPriceManager prices;

	StandIn<MRing, ITEM_CLASS_COUPLE_RING> ring;
	StandIn<MVampireRing, ITEM_CLASS_VAMPIRE_COUPLE_RING> vampireRing;
	const struct { MItem* pItem; const char* name; } rings[] = {
		{ &ring, "CoupleRing" },
		{ &vampireRing, "VampireCoupleRing" },
	};

	for (const auto& couple : rings)
	{
		MItem* const pItem = couple.pItem;
		pItem->SetItemType(0);
		pItem->SetGrade(-1);

		// The client's maximum is the ring's, floored at 1000.
		CheckShown(couple.name, 0, -1, "max durability", 1000, pItem->GetMaxDurability());

		// The durability the server sends, none, and the maximum the
		// client sets after a repair: the quote is 0 at each.
		const int durabilities[] = { 1, 0, 1000 };
		for (int durability : durabilities)
		{
			pItem->SetCurrentDurability(durability);
			CheckShown(couple.name, 0, -1, "repair", 0, prices.GetItemPrice(pItem, MPriceManager::REPAIR));
		}
	}

	// The gear window's repair-all total for a fresh sword and the two
	// rings as the server sends them: nothing to repair, so no dialog.
	MSword sword;
	sword.SetItemType(0);
	sword.SetGrade(4);
	sword.SetCurrentDurability(sword.GetMaxDurability());
	ring.SetCurrentDurability(1);
	vampireRing.SetCurrentDurability(1);

	int sum = 0;
	MItem* const pWorn[] = { &sword, &ring, &vampireRing };
	for (MItem* pItem : pWorn)
		sum += prices.GetItemPrice(pItem, MPriceManager::REPAIR);
	CHECK_EQ(0, sum);
}
