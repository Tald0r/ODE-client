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
// Every gear class is built here through the real MItem subclass (or,
// for the two couple rings, whose use bodies are executable-side, a
// stand-in of their family that answers their class), and every getter
// the grade moves is compared, at grades on both sides of 4, with the
// server's rule for that class. The client shows only what a policy
// moves: damage and critical for a weapon policy (-1 otherwise, which
// the item description hides), luck for an accessory policy (-9999
// otherwise), and for the armor policies the defense and protection
// with the offset and floored at 0; any other gear shows the table's
// defense and protection as they are.
//
// kClientRules lists the classes whose client rule is not the server's
// table, with the policy and durability the client applies to them.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"

#include "domain/ItemDurability.h"
#include "domain/ItemGrade.h"

#include <algorithm>
#include <string>

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
	{ ITEM_CLASS_SG,					"SG",					&MGunSG::NewItem },
	{ ITEM_CLASS_SMG,					"SMG",					&MGunSMG::NewItem },
	{ ITEM_CLASS_AR,					"AR",					&MGunAR::NewItem },
	{ ITEM_CLASS_SR,					"SR",					&MGunTR::NewItem },
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
	// the ring's rule (docs/compiler-warnings-2026-09-27.md, "Shop
	// prices").
	{ ITEM_CLASS_COUPLE_RING,			decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_VAMPIRE_COUPLE_RING,	decore::GradePolicy::Accessory,	true },
	// Drift from the server's table, each fixed by its own commit.
	{ ITEM_CLASS_VAMPIRE_AMULET,		decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_CORE_ZAP,				decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_CARRYING_RECEIVER,		decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_SHOULDER_ARMOR,		decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_DERMIS,				decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_PERSONA,				decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_FASCIA,				decore::GradePolicy::Accessory,	true },
	{ ITEM_CLASS_MITTEN,				decore::GradePolicy::Cloth,		true },
};

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
	shown.maxDurability = (int)decore::maxDurabilityBase((unsigned)row.durability, rule.hasDurability, offsets.durability);
	shown.minDamage = weapon ? std::max(1, row.minDamage + offsets.damage) : -1;
	shown.maxDamage = weapon ? std::max(1, row.maxDamage + offsets.damage) : -1;
	shown.critical = weapon ? std::max(0, row.critical + offsets.critical) : -1;
	shown.defense = armor ? std::max(0, row.defense + offsets.defense) : row.defense;
	shown.protection = armor ? std::max(0, row.protection + offsets.protection) : row.protection;
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
				std::max(1000, row.durability + (grade - 4) * 1000), sign.GetMaxDurability());
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
