//----------------------------------------------------------------------
// test_player_gear.cpp
//----------------------------------------------------------------------
//
// The gear (docs/RESTRUCTURING.md task 4.4, third slice): MPlayerGear,
// the slot container a player wears its items in, which grades each
// piece by its remaining durability; the three races' slot rules over
// it (the slayer's in depth, the others where they differ); and MShop,
// a shelf per type. What the gear asks of the executable - the
// player's affect check and stat recalculation, the gear sound, the
// quick-slot rebuild, the repair hint, a magazine for a gun - goes
// through MItem's host, which these tests drive by hand.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MPlayerGear.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MQuickSlot.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MGameDef.h"
#include "RaceType.h"

#include <vector>

namespace {

int		s_Alive = 0;

//----------------------------------------------------------------------
// A host that counts what the gear asks for.
//----------------------------------------------------------------------
int					s_Refreshed = 0;
int					s_Recalculated = 0;
int					s_QuickSlotResets = 0;
int					s_RepairHints = 0;
std::vector<int>	s_Sounds;

DWORD	s_Frame = 0;
int		DropFrameCount(TYPE_FRAMEID)		{ return 0; }
void	RefreshAffect(MItem*)				{ s_Refreshed++; }
void	PlayItemSound(TYPE_SOUNDID sound)	{ s_Sounds.push_back(sound); }
void	RecalculateStatus()					{ s_Recalculated++; }
void	ResetQuickItemSlot()				{ s_QuickSlotResets++; }
void	RepairHint()						{ s_RepairHints++; }
MMagazine*	EmptyMagazineFor(MItem*)		{ return NULL; }

const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, NULL,
							RecalculateStatus, ResetQuickItemSlot, RepairHint, EmptyMagazineFor,
							NULL, NULL };

//----------------------------------------------------------------------
// The tables: each class carries the race that may wear it and a gear
// sound; the belt and the arms band carry their pocket count.
//----------------------------------------------------------------------
struct GearWorld : GameModelWorld
{
	GearWorld()
	{
		s_Alive = 0;
		s_Refreshed = 0;
		s_Recalculated = 0;
		s_QuickSlotResets = 0;
		s_RepairHints = 0;
		s_Sounds.clear();

		g_pItemTable->InitClass(ITEM_CLASS_HELM, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 0).Race = FLAG_RACE_SLAYER;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 0).SetSoundID(10, 20, 81, 40);
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).Race = FLAG_RACE_SLAYER;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetSoundID(10, 20, 82, 40);
		g_pItemTable->InitClass(ITEM_CLASS_RING, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_RING, 0).Race = FLAG_RACE_SLAYER;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_RING, 0).SetSoundID(10, 20, 83, 40);
		g_pItemTable->InitClass(ITEM_CLASS_CORE_ZAP, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_CORE_ZAP, 0).Race = FLAG_RACE_SLAYER;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_CORE_ZAP, 0).SetSoundID(10, 20, 84, 40);
		g_pItemTable->InitClass(ITEM_CLASS_BELT, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BELT, 0).Race = FLAG_RACE_SLAYER;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BELT, 0).SetSoundID(10, 20, 85, 40);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BELT, 0).SetValue(0, 0, 3);		// Value3: pockets
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_COAT, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 0).Race = FLAG_RACE_VAMPIRE;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 0).SetSoundID(10, 20, 86, 40);
		g_pItemTable->InitClass(ITEM_CLASS_OUSTERS_ARMSBAND, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).Race = FLAG_RACE_OUSTERS;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).SetSoundID(10, 20, 87, 40);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).SetValue(0, 0, 2);
		g_pItemTable->InitClass(ITEM_CLASS_GLOVE, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_GLOVE, 0).Race = FLAG_RACE_SLAYER;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_GLOVE, 0).SetSoundID(10, 20, 88, 40);
		// A slayer's potion, so the gear refuses it for not being gear
		// rather than for the race.
		g_pItemTable->InitClass(ITEM_CLASS_POTION, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).Race = FLAG_RACE_SLAYER;

		// A piece is "somewhat broken" at a quarter of its durability
		// and "almost broken" at a tenth.
		g_pClientConfig->PERCENTAGE_ITEM_SOMEWHAT_BROKEN = 25;
		g_pClientConfig->PERCENTAGE_ITEM_ALMOST_BROKEN = 10;

		MItem::SetHost(&s_Host);
	}
};

//----------------------------------------------------------------------
// Test items: a piece of gear of any class with the durability the
// test gives it, claiming one slot.
//----------------------------------------------------------------------
struct Gear : public MItem
{
	ITEM_CLASS	m_Class;
	int			m_MaxDurability;

	Gear(ITEM_CLASS itemClass, TYPE_OBJECTID id, int maxDurability = 100, TYPE_ITEM_DURATION current = 100)
		: m_Class(itemClass), m_MaxDurability(maxDurability)
	{
		SetID(id);
		SetCurrentDurability(current);
		s_Alive++;
	}
	~Gear()										{ s_Alive--; }

	ITEM_CLASS	GetItemClass() const			{ return m_Class; }
	bool		IsGearItem() const				{ return true; }
	int			GetMaxDurability() const		{ return m_MaxDurability; }
};

struct Helm : public Gear
{
	Helm(TYPE_OBJECTID id, int maxDurability = 100, TYPE_ITEM_DURATION current = 100)
		: Gear(ITEM_CLASS_HELM, id, maxDurability, current) {}
	bool	IsGearSlotHelm() const				{ return true; }
};

struct EverlastingHelm : public Helm
{
	EverlastingHelm(TYPE_OBJECTID id, TYPE_ITEM_DURATION current) : Helm(id, 100, current) {}
	bool	IsDurationAlwaysOkay() const		{ return true; }
};

struct TwoHander : public Gear
{
	TwoHander(TYPE_OBJECTID id) : Gear(ITEM_CLASS_SWORD, id) {}
	bool	IsGearSlotTwoHand() const			{ return true; }
};

struct RightHander : public Gear
{
	RightHander(TYPE_OBJECTID id) : Gear(ITEM_CLASS_SWORD, id) {}
	bool	IsGearSlotRightHand() const			{ return true; }
};

struct Ring : public Gear
{
	Ring(TYPE_OBJECTID id) : Gear(ITEM_CLASS_RING, id) {}
	bool	IsGearSlotRing() const				{ return true; }
};

struct CoreZap : public Gear
{
	CoreZap(TYPE_OBJECTID id) : Gear(ITEM_CLASS_CORE_ZAP, id) {}
	bool	IsGearSlotCoreZap() const			{ return true; }
};

struct Glove : public Gear
{
	Glove(TYPE_OBJECTID id) : Gear(ITEM_CLASS_GLOVE, id) {}
	bool	IsGearSlotGlove() const				{ return true; }
};

struct VampireCoat : public Gear
{
	VampireCoat(TYPE_OBJECTID id) : Gear(ITEM_CLASS_VAMPIRE_COAT, id) {}
	bool	IsGearSlotVampireCoat() const		{ return true; }
};

struct Potion : public MItem
{
	Potion(TYPE_OBJECTID id)					{ SetID(id); s_Alive++; }
	~Potion()									{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const			{ return ITEM_CLASS_POTION; }
};

// The real belt and arms band: gear that is also a slot container, so
// the quick-slot side effects are the library's own.
struct CountedBelt : public MBelt
{
	CountedBelt(TYPE_OBJECTID id)				{ SetID(id); SetItemType(0); s_Alive++; }
	~CountedBelt()								{ s_Alive--; }
};

struct CountedArmsBand : public MOustersArmsBand
{
	CountedArmsBand(TYPE_OBJECTID id)			{ SetID(id); SetItemType(0); s_Alive++; }
	~CountedArmsBand()							{ s_Alive--; }
};

} // namespace

//----------------------------------------------------------------------
// The base: statuses by durability
//----------------------------------------------------------------------
TEST(PlayerGear, InitStartsEverySlotOkAndReleaseOwnsTheItems)
{
	GearWorld world;
	MPlayerGear gear;
	gear.Init(3);

	CHECK_EQ(false, (bool)gear.HasBrokenItem());
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(0));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(2));

	Helm* whole = new Helm(1, 100, 100);
	Helm* worn = new Helm(2, 100, 20);
	Helm* refused = new Helm(3);
	CHECK(gear.AddItem(whole, 0));
	CHECK(gear.AddItem(worn, 1));
	CHECK_EQ(false, gear.AddItem(refused, 5));			// past the slots
	CHECK_EQ(2, gear.GetItemNum());
	CHECK(gear.GetItem(1) == worn);
	CHECK(gear.GetItem(5) == NULL);
	CHECK(gear.HasBrokenItem());

	gear.Release();
	CHECK_EQ(1, s_Alive);		// the refused helm is still the caller's
	delete refused;
	CHECK_EQ(0, s_Alive);
}

TEST(PlayerGear, AddGradesTheItemByItsRemainingDurability)
{
	GearWorld world;
	MPlayerGear gear;
	gear.Init(8);

	// Above a quarter is fine; a quarter or less is somewhat broken; a
	// tenth or less is almost broken.
	CHECK(gear.AddItem(new Helm(1, 100, 26), 0));
	CHECK(gear.AddItem(new Helm(2, 100, 25), 1));
	CHECK(gear.AddItem(new Helm(3, 100, 11), 2));
	CHECK(gear.AddItem(new Helm(4, 100, 10), 3));
	CHECK(gear.AddItem(new Helm(5, 100, 0), 4));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(0));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_SOMEWHAT_BROKEN, (int)gear.GetItemStatus(1));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_SOMEWHAT_BROKEN, (int)gear.GetItemStatus(2));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_ALMOST_BROKEN, (int)gear.GetItemStatus(3));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_ALMOST_BROKEN, (int)gear.GetItemStatus(4));

	// No durability to lose (zero, or MItem's -1 for an item that has
	// none), or one that never counts: always fine.
	CHECK(gear.AddItem(new Helm(6, 0, 0), 5));
	CHECK(gear.AddItem(new Helm(7, -1, 0), 6));
	CHECK(gear.AddItem(new EverlastingHelm(8, 0), 7));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(5));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(6));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(7));
	CHECK(gear.HasBrokenItem());

	// The thresholds are the configuration's.
	g_pClientConfig->PERCENTAGE_ITEM_SOMEWHAT_BROKEN = 50;
	g_pClientConfig->PERCENTAGE_ITEM_ALMOST_BROKEN = 20;
	gear.CheckItemStatusAll();
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_SOMEWHAT_BROKEN, (int)gear.GetItemStatus(0));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_ALMOST_BROKEN, (int)gear.GetItemStatus(2));

	gear.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(PlayerGear, RemovingABrokenItemClearsItsStatusAndTheCount)
{
	GearWorld world;
	MPlayerGear gear;
	gear.Init(3);
	Helm* first = new Helm(1, 100, 5);
	Helm* second = new Helm(2, 100, 5);
	CHECK(gear.AddItem(first, 0));
	CHECK(gear.AddItem(second, 2));
	CHECK(gear.HasBrokenItem());

	CHECK(gear.RemoveItem((BYTE)0) == first);
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(0));
	CHECK(gear.HasBrokenItem());
	CHECK(gear.RemoveItem((TYPE_OBJECTID)2) == second);
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(2));
	CHECK_EQ(false, (bool)gear.HasBrokenItem());
	CHECK(gear.RemoveItem((BYTE)1) == NULL);
	CHECK_EQ(0, gear.GetItemNum());

	// Removed items are the caller's again.
	delete first;
	delete second;
	CHECK_EQ(0, s_Alive);

	// Replacing hands back the old item with its status cleared; the
	// newcomer is not graded here (the race gears do that), so a worn
	// replacement reads as fine until the next check.
	Helm* fine = new Helm(3, 100, 100);
	Helm* worn = new Helm(4, 100, 5);
	CHECK(gear.AddItem(fine, 1));
	MItem* pOld = NULL;
	CHECK(gear.ReplaceItem(worn, 1, pOld));
	CHECK(pOld == fine);
	CHECK(gear.GetItem(1) == worn);
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(1));
	gear.CheckItemStatusAll();
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_ALMOST_BROKEN, (int)gear.GetItemStatus(1));
	delete fine;
	gear.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(PlayerGear, ModifyDurabilityClampsRegradesAndHintsOnceAsGearStartsToBreak)
{
	GearWorld world;
	MPlayerGear gear;
	gear.Init(2);
	Helm* helm = new Helm(1, 100, 100);
	CHECK(gear.AddItem(helm, 0));

	// The value is the new durability, clamped to the item's range.
	CHECK(gear.ModifyDurability(0, 150));
	CHECK_EQ(100, (int)helm->GetCurrentDurability());
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(0));
	CHECK_EQ(0, s_RepairHints);

	// Leaving OK raises the repair hint, once; getting worse does not.
	CHECK(gear.ModifyDurability(0, 20));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_SOMEWHAT_BROKEN, (int)gear.GetItemStatus(0));
	CHECK_EQ(1, s_RepairHints);
	CHECK(gear.ModifyDurability(0, 5));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_ALMOST_BROKEN, (int)gear.GetItemStatus(0));
	CHECK_EQ(1, s_RepairHints);
	CHECK(gear.ModifyDurability(0, -3));
	CHECK_EQ(0, (int)helm->GetCurrentDurability());
	CHECK(gear.HasBrokenItem());

	// Repaired, it counts as fine again, and breaking again hints again.
	CHECK(gear.ModifyDurability(0, 90));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(0));
	CHECK_EQ(false, (bool)gear.HasBrokenItem());
	CHECK(gear.ModifyDurability(0, 20));
	CHECK_EQ(2, s_RepairHints);

	// An empty slot, or one past the end, changes nothing.
	CHECK_EQ(false, gear.ModifyDurability(1, 50));
	CHECK_EQ(false, gear.ModifyDurability(2, 50));
	CHECK_EQ(2, s_RepairHints);

	// An item with no durability has no maximum to clamp to, so the
	// value stands as given.
	Helm* noWear = new Helm(2, -1, 0);
	CHECK(gear.AddItem(noWear, 1));
	CHECK(gear.ModifyDurability(1, 40));
	CHECK_EQ(40, (int)noWear->GetCurrentDurability());
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(1));

	gear.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(PlayerGear, ANegativeThresholdDoesNotCondemnEveryItem)
{
	GearWorld world;
	MPlayerGear gear;
	gear.Init(1);

	// The thresholds are read from the configuration file unchecked.
	// Compared as unsigned, a negative one made every item the worst
	// grade; compared as written, nothing clears a negative bar.
	g_pClientConfig->PERCENTAGE_ITEM_SOMEWHAT_BROKEN = -1;
	g_pClientConfig->PERCENTAGE_ITEM_ALMOST_BROKEN = -1;
	CHECK(gear.AddItem(new Helm(1, 100, 100), 0));
	CHECK_EQ((int)MPlayerGear::ITEM_STATUS_OK, (int)gear.GetItemStatus(0));
	CHECK_EQ(false, (bool)gear.HasBrokenItem());

	gear.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// The slayer's slots
//----------------------------------------------------------------------
TEST(SlayerGear, WearsSlayerGearInItsOwnSlotAndTellsTheHost)
{
	GearWorld world;
	MSlayerGear gear;
	gear.Init();

	// The helm goes on: the player re-checks it, the gear sound plays,
	// the stats follow.
	Helm* helm = new Helm(1);
	CHECK(gear.AddItem(helm, MSlayerGear::GEAR_SLAYER_HELM));
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == helm);
	CHECK_EQ(1, s_Refreshed);
	CHECK_EQ(1, (int)s_Sounds.size());
	CHECK_EQ(81, s_Sounds[0]);
	CHECK_EQ(1, s_Recalculated);

	// The slot is taken; a helm is not a coat; a vampire's coat is not
	// a slayer's; a potion is not gear.
	Helm* second = new Helm(2);
	VampireCoat* coat = new VampireCoat(3);
	Potion* potion = new Potion(4);
	CHECK_EQ(false, gear.AddItem(second, MSlayerGear::GEAR_SLAYER_HELM));
	CHECK_EQ(false, gear.AddItem(second, MSlayerGear::GEAR_SLAYER_COAT));
	CHECK_EQ(false, gear.AddItem(coat, MSlayerGear::GEAR_SLAYER_COAT));
	CHECK_EQ(false, gear.AddItem(potion, MSlayerGear::GEAR_SLAYER_BELT));
	CHECK_EQ(false, gear.AddItem(second));				// no slot fits a second helm
	CHECK_EQ(1, (int)s_Sounds.size());
	CHECK_EQ(1, s_Recalculated);

	// Taking it off recomputes the stats and hands the helm back.
	CHECK(gear.RemoveItem(MSlayerGear::GEAR_SLAYER_HELM) == helm);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == NULL);
	CHECK_EQ(2, s_Recalculated);
	CHECK(gear.AddItem(second));							// now it fits, by search
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == second);

	delete helm;
	delete coat;
	delete potion;
	gear.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(SlayerGear, ATwoHandedWeaponTakesBothHands)
{
	GearWorld world;
	MSlayerGear gear;
	gear.Init();
	TwoHander* claymore = new TwoHander(1);
	RightHander* sword = new RightHander(2);

	// Offered to either hand, it fills both, as one item.
	CHECK(gear.AddItem(claymore, MSlayerGear::GEAR_SLAYER_LEFTHAND));
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_LEFTHAND) == claymore);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND) == claymore);
	CHECK_EQ(1, gear.GetItemNum());
	CHECK_EQ(1, (int)s_Sounds.size());
	CHECK_EQ(false, gear.AddItem(sword, MSlayerGear::GEAR_SLAYER_RIGHTHAND));

	// Removing it from one hand frees both.
	CHECK(gear.RemoveItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND) == claymore);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_LEFTHAND) == NULL);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND) == NULL);
	CHECK_EQ(0, gear.GetItemNum());

	// With a hand taken it cannot go on, but it could replace what is there.
	CHECK(gear.AddItem(sword, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	CHECK_EQ(false, gear.AddItem(claymore, MSlayerGear::GEAR_SLAYER_LEFTHAND));
	MItem* pOld = NULL;
	CHECK(gear.CanReplaceItem(claymore, MSlayerGear::GEAR_SLAYER_LEFTHAND, pOld));
	CHECK(pOld == sword);

	delete claymore;
	gear.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(SlayerGear, ABeltBecomesTheQuickSlotAndResetsIt)
{
	GearWorld world;
	MSlayerGear gear;
	gear.Init();
	CHECK(g_pQuickSlot == NULL);

	CountedBelt* belt = new CountedBelt(1);
	CHECK_EQ(3, belt->GetSize());						// its pockets, from the table
	CHECK(gear.AddItem(belt, MSlayerGear::GEAR_SLAYER_BELT));
	CHECK(g_pQuickSlot == belt);
	CHECK_EQ(1, s_QuickSlotResets);
	CHECK_EQ(85, s_Sounds[0]);

	CHECK(gear.RemoveItem(MSlayerGear::GEAR_SLAYER_BELT) == belt);
	CHECK(g_pQuickSlot == NULL);
	CHECK_EQ(1, s_QuickSlotResets);						// the removal recomputes stats, no reset
	CHECK_EQ(2, s_Recalculated);

	delete belt;
	CHECK_EQ(0, s_Alive);
}

TEST(SlayerGear, ACoreZapSitsOnItsRingAndComesOffFirst)
{
	GearWorld world;
	MSlayerGear gear;
	gear.Init();
	Ring* ring = new Ring(1);
	CoreZap* zap = new CoreZap(2);
	Ring* another = new Ring(3);
	CoreZap* loose = new CoreZap(4);

	// A zap offered to its ring's slot lands in the zap slot behind it.
	CHECK(gear.AddItem(ring, MSlayerGear::GEAR_SLAYER_RING1));
	CHECK(gear.AddItem(zap, MSlayerGear::GEAR_SLAYER_RING1));
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_ZAP1) == zap);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_RING1) == ring);
	CHECK_EQ((int)MSlayerGear::GEAR_SLAYER_ZAP1, (int)zap->GetItemSlot());

	// A ring cannot take a slot that has a ring or a zap; a zap slot
	// takes a zap on its own.
	CHECK_EQ(false, gear.AddItem(another, MSlayerGear::GEAR_SLAYER_RING1));
	CHECK(gear.AddItem(another, MSlayerGear::GEAR_SLAYER_RING2));
	CHECK(gear.AddItem(loose, MSlayerGear::GEAR_SLAYER_ZAP3));
	CHECK_EQ(4, gear.GetItemNum());

	// A zap needs the ring under it, and nothing else worn stands in
	// for one: the glove sits five slots below RING4, which is where
	// this branch used to look.
	Glove* glove = new Glove(6);
	CoreZap* ringless = new CoreZap(7);
	CHECK(gear.AddItem(glove, MSlayerGear::GEAR_SLAYER_GLOVE));
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_RING4) == NULL);
	CHECK_EQ(false, gear.AddItem(ringless, MSlayerGear::GEAR_SLAYER_RING4));
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_ZAP4) == NULL);
	delete ringless;

	// Taking from the ring's slot takes the zap first, then the ring.
	CHECK(gear.RemoveItem(MSlayerGear::GEAR_SLAYER_RING1) == zap);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_RING1) == ring);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_ZAP1) == NULL);
	CHECK(gear.RemoveItem(MSlayerGear::GEAR_SLAYER_RING1) == ring);
	CHECK(gear.GetItem(MSlayerGear::GEAR_SLAYER_RING1) == NULL);

	delete ring;
	delete zap;
	gear.Release();			// the rest is the gear's
	CHECK_EQ(0, s_Alive);
}

TEST(PlayerGear, EveryRaceRefusesAGearSlotPastItsRack)
{
	GearWorld world;
	MSlayerGear slayer;
	MVampireGear vampire;
	MOustersGear ousters;
	slayer.Init();
	vampire.Init();
	ousters.Init();

	// The slot arrives in GCRemoveFromGear, so a value past the rack -
	// or a negative one - must not index the slot array. Passed as the
	// handler passes it, the raw int: a cast to the enum is undefined
	// for an out-of-range value before the check can run.
	CHECK(slayer.RemoveItem(MSlayerGear::MAX_GEAR_SLAYER) == NULL);
	CHECK(slayer.RemoveItem(200) == NULL);
	CHECK(slayer.RemoveItem(-1) == NULL);
	CHECK(vampire.RemoveItem(MVampireGear::MAX_GEAR_VAMPIRE) == NULL);
	CHECK(vampire.RemoveItem(-1) == NULL);
	CHECK(ousters.RemoveItem(MOustersGear::MAX_GEAR_OUSTERS) == NULL);
	CHECK(ousters.RemoveItem(-1) == NULL);

	// What is worn is still worn.
	Helm* helm = new Helm(1);
	CHECK(slayer.AddItem(helm, MSlayerGear::GEAR_SLAYER_HELM));
	CHECK(slayer.RemoveItem(200) == NULL);
	CHECK(slayer.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == helm);

	slayer.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// The other races, where they differ
//----------------------------------------------------------------------
TEST(VampireGear, WearsVampireGearOnlyAndTellsTheHost)
{
	GearWorld world;
	MVampireGear gear;
	gear.Init();
	VampireCoat* coat = new VampireCoat(1);
	Helm* helm = new Helm(2);

	CHECK(gear.AddItem(coat, MVampireGear::GEAR_VAMPIRE_COAT));
	CHECK(gear.GetItem(MVampireGear::GEAR_VAMPIRE_COAT) == coat);
	CHECK_EQ(86, s_Sounds[0]);
	CHECK_EQ(1, s_Recalculated);
	CHECK_EQ(false, gear.AddItem(helm, MVampireGear::GEAR_VAMPIRE_COAT));
	CHECK_EQ(false, gear.AddItem(helm));

	delete helm;
	gear.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(OustersGear, AnArmsBandBecomesAQuickSlotAndResetsIt)
{
	GearWorld world;
	MOustersGear gear;
	gear.Init();
	CHECK(g_pArmsBand1 == NULL);
	CountedArmsBand* band = new CountedArmsBand(1);
	CHECK_EQ(2, band->GetSize());

	// On: the band is the first arms band, the quick slots reset. The
	// gear plays the sound and recomputes the stats twice on the way
	// (today's behaviour, pinned).
	CHECK(gear.AddItem(band, MOustersGear::GEAR_OUSTERS_ARMSBAND1));
	CHECK(g_pArmsBand1 == band);
	CHECK(g_pArmsBand2 == NULL);
	CHECK_EQ(1, s_QuickSlotResets);
	CHECK_EQ(2, (int)s_Sounds.size());
	CHECK_EQ(87, s_Sounds[0]);
	CHECK_EQ(2, s_Recalculated);

	// A slayer's helm is not Ousters gear.
	Helm* helm = new Helm(2);
	CHECK_EQ(false, gear.AddItem(helm, MOustersGear::GEAR_OUSTERS_CIRCLET));

	CHECK(gear.RemoveItem(MOustersGear::GEAR_OUSTERS_ARMSBAND1) == band);
	CHECK(g_pArmsBand1 == NULL);

	delete band;
	delete helm;
	gear.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// The shop
//----------------------------------------------------------------------
TEST(Shop, SettingTheCurrentShelfAsksThePlayerAboutEverythingOnIt)
{
	GearWorld world;
	MShop shop;
	shop.Init(2);
	CHECK_EQ(2, (int)shop.GetSize());
	CHECK(shop.GetShelf(0) == NULL);
	CHECK(shop.GetShelf(2) == NULL);

	MShopShelf* shelf = MShopShelf::NewShelf(MShopShelf::SHELF_UNKNOWN);
	CHECK(shelf != NULL);
	CHECK(shelf->AddItem(new Potion(1)));
	CHECK(shelf->AddItem(new Potion(2)));
	CHECK_EQ(false, shop.SetShelf(1, NULL));
	CHECK(shop.SetShelf(0, shelf));
	CHECK(shop.GetShelf(0) == shelf);

	s_Refreshed = 0;
	shop.SetCurrent(0);
	CHECK_EQ(0, (int)shop.GetCurrent());
	CHECK(shop.GetCurrentShelf() == shelf);
	CHECK_EQ(2, s_Refreshed);

	// A shelf that does not exist is ignored; an empty slot refreshes nothing.
	shop.SetCurrent(7);
	CHECK_EQ(0, (int)shop.GetCurrent());
	shop.SetCurrent(1);
	CHECK_EQ(1, (int)shop.GetCurrent());
	CHECK_EQ(2, s_Refreshed);

	// The shop owns its shelves, and they their items.
	shop.Release();
	CHECK_EQ(0, s_Alive);
	CHECK_EQ(0, (int)shop.GetSize());
	CHECK(shop.GetCurrentShelf() == NULL);		// nothing to read after a release
}

TEST(Shop, ARefusedShelfStaysTheCallersToDelete)
{
	GearWorld world;
	MShop shop;
	shop.Init(2);

	// The header promises a shelf number past the shop's own is
	// refused, and three call sites delete the shelf on false; before
	// the bound it wrote past the array instead.
	MShopShelf* past = MShopShelf::NewShelf(MShopShelf::SHELF_FIXED);
	CHECK(past != NULL);
	CHECK_EQ(false, shop.SetShelf(2, past));
	CHECK_EQ(false, shop.SetShelf(4000, past));
	delete past;

	// A shop that was never given its shelves refuses everything.
	MShop empty;
	MShopShelf* orphan = MShopShelf::NewShelf(MShopShelf::SHELF_FIXED);
	CHECK_EQ(false, empty.SetShelf(0, orphan));
	CHECK(empty.GetCurrentShelf() == NULL);
	delete orphan;

	CHECK_EQ(0, s_Alive);
}
