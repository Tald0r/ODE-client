#include "test_framework.h"
#include "MSector.h"
#include "MItem.h"
#include "MEffect.h"
#include "MImageObject.h"
#include "ZoneStateHost.h"
#include "EffectSpriteTypeDef.h"
#include "EffectSpriteTypeHelper.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A map cell (task 4.233, gamemodel's since then): its tile, block and
// safety flags, light, and what is on it - one item, creatures by layer
// (underground, ground, flying) in their own key ranges, image objects,
// effects, portals and sounds. Creatures go through CreatureState as
// opaque handles; here they are plain MObjects.
namespace {

// Creature types by object ID, read through CreatureState::GetCreatureType.
std::map<TYPE_OBJECTID, int> creatureTypes;
// The creature each corpse holds, read through MItem::CorpseCreature.
std::map<const MItem*, MCreature*> corpseCreatures;

// Creatures are handed out as tokens of their own, not as their objects'
// addresses: a sector that reached a creature past the host would hold or
// hand back the wrong pointer.
alignas(16) char tokens[16 * 256];
std::map<const MCreature*, MObject*> objectOfHandle;
std::map<const MObject*, MCreature*> handleOfObject;

void ForgetHandles() { objectOfHandle.clear(); handleOfObject.clear(); }

MCreature* Handle(MObject& object)
{
	const auto found = handleOfObject.find(&object);
	if (found != handleOfObject.end()) return found->second;
	CHECK(handleOfObject.size() < 256);
	MCreature* handle = reinterpret_cast<MCreature*>(&tokens[16 * (handleOfObject.size() % 256)]);
	handleOfObject[&object] = handle;
	objectOfHandle[handle] = &object;
	return handle;
}

MObject* ObjectOfHandle(const MCreature* pCreature)
{
	const auto found = objectOfHandle.find(pCreature);
	return found == objectOfHandle.end() ? nullptr : found->second;
}

const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature* pCreature) {
		MObject* pObject = ObjectOfHandle(pCreature);
		const auto found = pObject == nullptr ? creatureTypes.end() : creatureTypes.find(pObject->GetID());
		return found == creatureTypes.end() ? 0 : found->second;
	};
	host.ObjectOf = [](const MCreature* pCreature) { return ObjectOfHandle(pCreature); };
	host.CreatureOf = [](MObject* pObject) {
		const auto found = handleOfObject.find(pObject);
		return found == handleOfObject.end() ? static_cast<MCreature*>(nullptr) : found->second;
	};
	return host;
}();

const MItemHost itemHost = []() {
	MItemHost host{};
	host.CorpseCreature = [](const MItem* pCorpse) {
		const auto found = corpseCreatures.find(pCorpse);
		return found == corpseCreatures.end() ? static_cast<MCreature*>(NULL) : found->second;
	};
	return host;
}();

struct Scene
{
	const CreatureState::Host* previousCreatures = CreatureState::SetHost(&creatureHost);
	Scene()
	{
		MItem::SetHost(&itemHost);
		creatureTypes.clear();
		corpseCreatures.clear();
		ForgetHandles();
	}
	~Scene()
	{
		CreatureState::SetHost(previousCreatures);
		MItem::SetHost(NULL);
	}
};

struct Creature final : MObject
{
	explicit Creature(TYPE_OBJECTID id, int type = 1) { SetID(id); creatureTypes[id] = type; }
};

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	explicit Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id) : m_Class(itemClass) { SetID(id); }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
};

struct Image : MImageObject
{
	explicit Image(TYPE_OBJECTID id) { SetID(id); }
};

std::unique_ptr<MEffect> Effect(TYPE_OBJECTID id, BYTE blt, int frame)
{
	auto effect = std::make_unique<MEffect>(blt);
	effect->SetID(id);
	effect->SetFrameID(static_cast<TYPE_FRAMEID>(frame), 1);
	return effect;
}

// The sector's objects in key order, by ID.
std::vector<TYPE_OBJECTID> Objects(const MSector& sector, OBJECT_MAP::const_iterator from, int count)
{
	std::vector<TYPE_OBJECTID> ids;
	for (int i = 0; i < count && from != sector.GetObjectEnd(); ++i, ++from)
		ids.push_back(from->second->GetID());
	return ids;
}

std::vector<BYTE> Keys(const MSector& sector, OBJECT_MAP::const_iterator from, int count)
{
	std::vector<BYTE> keys;
	for (int i = 0; i < count && from != sector.GetObjectEnd(); ++i, ++from)
		keys.push_back(from->first);
	return keys;
}

std::string TempPath()
{
	static unsigned sequence = 0;
	return (std::filesystem::temp_directory_path() / ("darkeden_sector_"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
		+ "_" + std::to_string(sequence++) + ".bin")).string();
}

} // namespace

TEST(Sector, ANewSectorHoldsNothing)
{
	Scene scene;
	MSector sector(7);
	CHECK_EQ(7, sector.GetSpriteID());
	CHECK(sector.IsEmpty());
	CHECK(!sector.IsExistObject());
	CHECK(sector.IsObjectEmpty());
	CHECK(!sector.IsExistItem());
	CHECK(!sector.IsExistAnyCreature());
	CHECK_EQ(0, sector.GetCreatureSize());
	CHECK_EQ(0, sector.IsExistImageObject());
	CHECK(sector.GetItem() == NULL);
	CHECK(sector.GetCreature() == NULL);
	CHECK(sector.GetGroundCreature() == NULL);
	CHECK(sector.GetCreatureIterator() == sector.GetObjectEnd());
	CHECK(sector.GetImageObjectIterator() == sector.GetObjectEnd());
	CHECK(!sector.IsExistEffect());
	CHECK_EQ(0, sector.GetPortalSize());
	CHECK_EQ(0, sector.GetSectorSoundSize());
	CHECK_EQ(0, sector.GetLight());
	CHECK_EQ(0, sector.GetSafe());
	CHECK(sector.CanStandUndergroundCreature());
	CHECK(sector.CanStandGroundCreature());
	CHECK(sector.CanStandFlyingCreature());
	CHECK(!sector.HasDarkness());
	CHECK(!sector.HasSanctuary());
	CHECK(!sector.IsVisitedFlag());
	CHECK_EQ(0, MSector().GetSpriteID());
}

TEST(Sector, TheTileCanBeHidden)
{
	MSector sector;
	sector.Set(12, FLAG_SECTOR_BLOCK_GROUND);
	CHECK_EQ(12, sector.GetSpriteID());
	CHECK(sector.IsBlockGround());
	sector.SetDisableTileImage();
	CHECK(sector.HasDisableTileImage());
	CHECK_EQ(SPRITEID_NULL, sector.GetSpriteID());
	sector.UnSetDisableTileImage();
	CHECK_EQ(12, sector.GetSpriteID());
}

TEST(Sector, EachLayerIsBlockedOnItsOwn)
{
	MSector sector;
	sector.SetBlockUnderground();
	CHECK(sector.IsBlockUnderground());
	CHECK(sector.IsBlockAny());
	CHECK(!sector.IsBlockGround());
	CHECK(!sector.CanStandUndergroundCreature());
	CHECK(sector.CanStandGroundCreature());
	CHECK(sector.CanStandFlyingCreature());
	sector.UnSetBlockUnderground();
	CHECK(!sector.IsBlockAny());
	sector.SetBlockGround();
	CHECK(!sector.CanStandGroundCreature());
	CHECK(sector.CanStandUndergroundCreature());
	sector.UnSetBlockGround();
	sector.SetBlockFlying();
	CHECK(!sector.CanStandFlyingCreature());
	CHECK(sector.CanStandGroundCreature());
	CHECK(sector.IsBlockAny());
	sector.UnSetBlockFlying();
	CHECK(!sector.IsBlockAny());
	sector.SetBlockAll();
	CHECK_EQ(FLAG_SECTOR_BLOCK_ALL, sector.IsBlockAll());
	CHECK(sector.IsBlockUnderground() && sector.IsBlockGround() && sector.IsBlockFlying());
	sector.UnSetBlockAll();
	CHECK(sector.IsEmpty());
}

TEST(Sector, TheServerBlocksEachLayerApart)
{
	MSector sector;
	sector.SetBlockServerUnderground();
	CHECK(sector.IsBlockServerUnderground());
	CHECK(!sector.CanStandUndergroundCreature());
	CHECK(sector.CanStandGroundCreature());
	CHECK(!sector.IsBlockAny());				// not the map's own block
	CHECK(sector.IsEmpty());					// a second flag byte
	sector.UnSetBlockServerUnderground();
	CHECK(sector.CanStandUndergroundCreature());
	sector.SetBlockServerGround();
	CHECK(sector.IsBlockServerGround());
	CHECK(!sector.CanStandGroundCreature());
	CHECK(sector.CanStandFlyingCreature());
	sector.UnSetBlockServerGround();
	sector.SetBlockServerFlying();
	CHECK(sector.IsBlockServerFlying());
	CHECK(!sector.CanStandFlyingCreature());
	CHECK(sector.CanStandUndergroundCreature());
	sector.UnSetBlockServerFlying();
	CHECK(sector.CanStandFlyingCreature());
	CHECK_EQ(0, sector.GetSafe());
}

TEST(Sector, AnOccupiedLayerCannotBeEntered)
{
	Scene scene;
	MSector sector;
	Creature under(1), ground(2), flying(3);
	CHECK(sector.AddUndergroundCreature(Handle(under)));
	CHECK(!sector.CanStandUndergroundCreature());
	CHECK(sector.CanStandGroundCreature());
	CHECK(sector.AddGroundCreature(Handle(ground)));
	CHECK(!sector.CanStandGroundCreature());
	CHECK(sector.CanStandFlyingCreature());
	CHECK(sector.AddFlyingCreature(Handle(flying)));
	CHECK(!sector.CanStandFlyingCreature());
}

TEST(Sector, OneItemAtATime)
{
	Scene scene;
	MSector sector;
	Ware first(ITEM_CLASS_POTION, 10), second(ITEM_CLASS_POTION, 11);
	CHECK(sector.AddItem(&first));
	CHECK(sector.IsExistItem());
	CHECK(!sector.AddItem(&second));
	CHECK(sector.GetItem() == &first);
	CHECK(sector.IsExistObject());
	CHECK(!sector.IsEmpty());
	CHECK(sector.RemoveItem());
	CHECK(!sector.IsExistItem());
	CHECK(sector.GetItem() == NULL);
	CHECK(!sector.RemoveItem());
	CHECK(sector.AddItem(&second));
	CHECK(sector.GetItem() == &second);
	// Removed and handed back, whatever its ID.
	MItem* pItem = NULL;
	CHECK(sector.RemoveItem(pItem));
	CHECK(pItem == &second);
	CHECK(!sector.IsExistItem());
	CHECK(sector.IsObjectEmpty());
	pItem = NULL;
	CHECK(!sector.RemoveItem(pItem));
	CHECK(pItem == NULL);
}

TEST(Sector, AnItemIsRemovedByItsID)
{
	Scene scene;
	MSector sector;
	Ware item(ITEM_CLASS_POTION, 10);
	MItem* pItem = NULL;
	CHECK(!sector.RemoveItem(10, pItem));
	CHECK(pItem == NULL);
	CHECK(!sector.RemoveItem(10));
	sector.AddItem(&item);
	// Another ID: not removed, though the item is handed back.
	CHECK(!sector.RemoveItem(11, pItem));
	CHECK(pItem == &item);
	CHECK(!sector.RemoveItem(11));
	CHECK(sector.IsExistItem());
	pItem = NULL;
	CHECK(sector.RemoveItem(10, pItem));
	CHECK(pItem == &item);
	CHECK(!sector.IsExistItem());
	CHECK(sector.IsObjectEmpty());
	sector.AddItem(&item);
	CHECK(sector.RemoveItem(10));
	CHECK(!sector.IsExistItem());
	CHECK(sector.IsObjectEmpty());
}

TEST(Sector, ASweepersCorpseBlocksTheGround)
{
	Scene scene;
	MSector sector;
	Creature sweeper(5, 672), other(6, 673);
	Ware corpse(ITEM_CLASS_CORPSE, 20);
	corpseCreatures[&corpse] = Handle(sweeper);
	CHECK(sector.AddItem(&corpse));
	CHECK(sector.IsBlockGround());
	CHECK(!sector.CanStandGroundCreature());
	CHECK(sector.RemoveItem());
	CHECK(!sector.IsBlockGround());
	// Each removal lifts it, even where the map blocks the ground.
	sector.SetBlockGround();
	sector.AddItem(&corpse);
	CHECK(sector.RemoveItem(20));
	CHECK(!sector.IsBlockGround());
	sector.AddItem(&corpse);
	MItem* pItem = NULL;
	CHECK(sector.RemoveItem(20, pItem));
	CHECK(!sector.IsBlockGround());
	sector.AddItem(&corpse);
	CHECK(sector.RemoveItem(pItem));
	CHECK(!sector.IsBlockGround());
	// Another ID leaves it.
	sector.AddItem(&corpse);
	CHECK(!sector.RemoveItem(21));
	CHECK(!sector.RemoveItem(21, pItem));
	CHECK(sector.IsBlockGround());
	CHECK(sector.RemoveItem(20));
	// Another creature's corpse, one with no creature, and the sweeper in
	// an item of another class do not block.
	Ware otherCorpse(ITEM_CLASS_CORPSE, 21), emptyCorpse(ITEM_CLASS_CORPSE, 22), notCorpse(ITEM_CLASS_POTION, 23);
	corpseCreatures[&otherCorpse] = Handle(other);
	corpseCreatures[&notCorpse] = Handle(sweeper);
	for (MItem* item : {static_cast<MItem*>(&otherCorpse), static_cast<MItem*>(&emptyCorpse), static_cast<MItem*>(&notCorpse)})
	{
		CHECK(sector.AddItem(item));
		CHECK(!sector.IsBlockGround());
		sector.SetBlockGround();
		CHECK(sector.RemoveItem());
		CHECK(sector.IsBlockGround());
		sector.UnSetBlockGround();
	}
	// Without an item host no corpse is known.
	MItem::SetHost(NULL);
	CHECK(sector.AddItem(&corpse));
	CHECK(!sector.IsBlockGround());
}

TEST(Sector, CreaturesFillTheirLayersInOrder)
{
	Scene scene;
	MSector sector;
	Creature a(1), b(2), c(3), d(4);
	CHECK(sector.AddGroundCreature(Handle(a)));
	CHECK(sector.AddGroundCreature(Handle(b)));
	CHECK(sector.AddFlyingCreature(Handle(c)));
	CHECK(sector.AddUndergroundCreature(Handle(d)));
	CHECK(sector.IsExistGroundCreature());
	CHECK(sector.IsExistFlyingCreature());
	CHECK(sector.IsExistUndergroundCreature());
	CHECK_EQ(2, sector.GetGroundCreatureSize());
	CHECK_EQ(1, sector.GetFlyingCreatureSize());
	CHECK_EQ(1, sector.GetUndergroundCreatureSize());
	CHECK_EQ(4, sector.GetCreatureSize());
	CHECK((Keys(sector, sector.GetGroundCreatureIterator(), 2) == std::vector<BYTE>{MSector::POSITION_GROUNDCREATURE, MSector::POSITION_GROUNDCREATURE + 1}));
	CHECK((Keys(sector, sector.GetFlyingCreatureIterator(), 1) == std::vector<BYTE>{MSector::POSITION_FLYINGCREATURE}));
	CHECK((Keys(sector, sector.GetUndergroundCreatureIterator(), 1) == std::vector<BYTE>{MSector::POSITION_UNDERGROUNDCREATURE}));
	// Underground first, then the ground, then the air.
	CHECK((Objects(sector, sector.GetCreatureIterator(), 4) == std::vector<TYPE_OBJECTID>{4, 1, 2, 3}));
	CHECK(sector.GetCreature() == Handle(d));
	CHECK(sector.GetUndergroundCreature() == Handle(d));
	CHECK(sector.GetGroundCreature() == Handle(a));
	CHECK(sector.GetFlyingCreature() == Handle(c));
	// Through a const sector, as the view reads it.
	const MSector& view = sector;
	CHECK(view.GetCreatureIterator()->second == &d);
	CHECK(view.GetUndergroundCreatureIterator()->second == &d);
	CHECK(view.GetGroundCreatureIterator()->second == &a);
	CHECK(view.GetFlyingCreatureIterator()->second == &c);
	// The mutable iterators agree.
	MSector& mutableSector = sector;
	CHECK(mutableSector.GetCreatureIterator()->second == &d);
	CHECK(mutableSector.GetUndergroundCreatureIterator()->second == &d);
	CHECK(mutableSector.GetGroundCreatureIterator()->second == &a);
	CHECK(mutableSector.GetFlyingCreatureIterator()->second == &c);
	// Without the underground one the ground comes first, then the air.
	CHECK(sector.RemoveUndergroundCreature(4));
	CHECK(sector.GetCreature() == Handle(a));
	CHECK(sector.RemoveGroundCreature(1));
	CHECK(sector.RemoveGroundCreature(2));
	CHECK(sector.GetCreature() == Handle(c));
	CHECK(mutableSector.GetCreatureIterator()->second == &c);
	CHECK(view.GetCreatureIterator()->second == &c);
	CHECK(mutableSector.GetGroundCreatureIterator() == mutableSector.GetObjectEnd());
	CHECK(view.GetGroundCreatureIterator() == view.GetObjectEnd());
	CHECK(mutableSector.GetUndergroundCreatureIterator() == mutableSector.GetObjectEnd());
	CHECK(view.GetUndergroundCreatureIterator() == view.GetObjectEnd());
	CHECK(sector.RemoveFlyingCreature(3));
	CHECK(mutableSector.GetCreatureIterator() == mutableSector.GetObjectEnd());
	CHECK(view.GetCreatureIterator() == view.GetObjectEnd());
	CHECK(mutableSector.GetFlyingCreatureIterator() == mutableSector.GetObjectEnd());
	CHECK(view.GetFlyingCreatureIterator() == view.GetObjectEnd());
	CHECK(!sector.IsExistAnyCreature());
}

TEST(Sector, EachLayerHasItsCapacity)
{
	Scene scene;
	struct Layer { bool (MSector::*add)(const MCreature*); BYTE (MSector::*size)() const; int capacity; };
	const Layer layers[] = {
		{&MSector::AddUndergroundCreature, &MSector::GetUndergroundCreatureSize, 13},
		{&MSector::AddGroundCreature, &MSector::GetGroundCreatureSize, 35},
		{&MSector::AddFlyingCreature, &MSector::GetFlyingCreatureSize, 30},
	};
	for (const Layer& layer : layers)
	{
		MSector sector;
		std::vector<std::unique_ptr<Creature>> creatures;
		for (int i = 0; i < layer.capacity; ++i)
		{
			creatures.push_back(std::make_unique<Creature>(100 + i));
			CHECK((sector.*layer.add)(Handle(*creatures.back())));
		}
		Creature extra(999);
		CHECK(!(sector.*layer.add)(Handle(extra)));
		CHECK_EQ(layer.capacity, (sector.*layer.size)());
		CHECK_EQ(layer.capacity, sector.GetCreatureSize());
	}
}

TEST(Sector, AFreedSlotIsReused)
{
	Scene scene;
	struct Layer {
		bool (MSector::*add)(const MCreature*);
		bool (MSector::*remove)(TYPE_OBJECTID);
		OBJECT_MAP::const_iterator (MSector::*first)() const;
		BYTE (MSector::*size)() const;
		BYTE base;
	};
	const Layer layers[] = {
		{&MSector::AddUndergroundCreature, &MSector::RemoveUndergroundCreature, &MSector::GetUndergroundCreatureIterator, &MSector::GetUndergroundCreatureSize, MSector::POSITION_UNDERGROUNDCREATURE},
		{&MSector::AddGroundCreature, &MSector::RemoveGroundCreature, &MSector::GetGroundCreatureIterator, &MSector::GetGroundCreatureSize, MSector::POSITION_GROUNDCREATURE},
		{&MSector::AddFlyingCreature, &MSector::RemoveFlyingCreature, &MSector::GetFlyingCreatureIterator, &MSector::GetFlyingCreatureSize, MSector::POSITION_FLYINGCREATURE},
	};
	for (const Layer& layer : layers)
	{
		MSector sector;
		Creature a(1), b(2), c(3), d(4), e(5), f(6);
		(sector.*layer.add)(Handle(a));
		(sector.*layer.add)(Handle(b));
		(sector.*layer.add)(Handle(c));
		// The middle one leaves: the next takes its slot.
		CHECK((sector.*layer.remove)(2));
		CHECK((sector.*layer.add)(Handle(d)));
		CHECK((Objects(sector, (sector.*layer.first)(), 3) == std::vector<TYPE_OBJECTID>{1, 4, 3}));
		const BYTE base = layer.base;
		CHECK((Keys(sector, (sector.*layer.first)(), 3) == std::vector<BYTE>{base, BYTE(base + 1), BYTE(base + 2)}));
		// The first leaves: the next goes first.
		CHECK((sector.*layer.remove)(1));
		CHECK((sector.*layer.add)(Handle(e)));
		CHECK((Objects(sector, (sector.*layer.first)(), 3) == std::vector<TYPE_OBJECTID>{5, 4, 3}));
		// The last leaves: the next goes last.
		CHECK((sector.*layer.remove)(3));
		CHECK((sector.*layer.add)(Handle(f)));
		CHECK((Objects(sector, (sector.*layer.first)(), 3) == std::vector<TYPE_OBJECTID>{5, 4, 6}));
		CHECK_EQ(3, (sector.*layer.size)());
	}
}

TEST(Sector, TwoSlotsFreedBelowTheCount)
{
	// With slots 1 and 2 of 0-3 free, the count (2) points at slot 2: the
	// next creature goes there, the one after into slot 1.
	Scene scene;
	MSector sector;
	Creature a(1), b(2), c(3), d(4), e(5), f(6);
	for (Creature* creature : {&a, &b, &c, &d})
		sector.AddGroundCreature(Handle(*creature));
	CHECK(sector.RemoveGroundCreature(2));
	CHECK(sector.RemoveGroundCreature(3));
	CHECK(sector.AddGroundCreature(Handle(e)));
	CHECK(sector.AddGroundCreature(Handle(f)));
	CHECK((Objects(sector, sector.GetGroundCreatureIterator(), 4) == std::vector<TYPE_OBJECTID>{1, 6, 5, 4}));
}

TEST(Sector, ACreatureIsRemovedFromItsOwnLayer)
{
	Scene scene;
	MSector sector;
	Creature under(1), ground(2), flying(3);
	sector.AddUndergroundCreature(Handle(under));
	sector.AddGroundCreature(Handle(ground));
	sector.AddFlyingCreature(Handle(flying));
	MCreature* pCreature = Handle(under);
	// Not in that layer: nothing removed, and NULL handed back.
	CHECK(!sector.RemoveGroundCreature(1, pCreature));
	CHECK(pCreature == NULL);
	pCreature = Handle(under);
	CHECK(!sector.RemoveFlyingCreature(2, pCreature));
	CHECK(pCreature == NULL);
	pCreature = Handle(under);
	CHECK(!sector.RemoveUndergroundCreature(3, pCreature));
	CHECK(pCreature == NULL);
	CHECK(!sector.RemoveGroundCreature(1));
	CHECK(!sector.RemoveFlyingCreature(2));
	CHECK(!sector.RemoveUndergroundCreature(3));
	CHECK_EQ(3, sector.GetCreatureSize());
	// In it: removed, handed back, and the layer's flag goes with the last.
	CHECK(sector.RemoveUndergroundCreature(1, pCreature));
	CHECK(pCreature == Handle(under));
	CHECK(!sector.IsExistUndergroundCreature());
	CHECK(sector.RemoveGroundCreature(2, pCreature));
	CHECK(pCreature == Handle(ground));
	CHECK(!sector.IsExistGroundCreature());
	CHECK(sector.RemoveFlyingCreature(3, pCreature));
	CHECK(pCreature == Handle(flying));
	CHECK(!sector.IsExistFlyingCreature());
	CHECK(sector.IsObjectEmpty());
	// An empty layer hands back NULL too.
	pCreature = Handle(under);
	CHECK(!sector.RemoveGroundCreature(2, pCreature));
	CHECK(pCreature == NULL);
}

TEST(Sector, AFlagStaysWhileTheLayerHoldsAny)
{
	Scene scene;
	MSector sector;
	Creature a(1), b(2), c(3), d(4), e(5), f(6);
	sector.AddUndergroundCreature(Handle(a));
	sector.AddUndergroundCreature(Handle(b));
	sector.AddGroundCreature(Handle(c));
	sector.AddGroundCreature(Handle(d));
	sector.AddFlyingCreature(Handle(e));
	sector.AddFlyingCreature(Handle(f));
	MCreature* pCreature = NULL;
	CHECK(sector.RemoveUndergroundCreature(2, pCreature));
	CHECK(sector.IsExistUndergroundCreature());
	CHECK(sector.RemoveGroundCreature(4));
	CHECK(sector.IsExistGroundCreature());
	CHECK(sector.RemoveFlyingCreature(5, pCreature));
	CHECK(sector.IsExistFlyingCreature());
	CHECK(sector.RemoveUndergroundCreature(1));
	CHECK(!sector.IsExistUndergroundCreature());
	CHECK(sector.RemoveGroundCreature(3, pCreature));
	CHECK(!sector.IsExistGroundCreature());
	CHECK(sector.RemoveFlyingCreature(6));
	CHECK(!sector.IsExistFlyingCreature());
}

TEST(Sector, RemoveCreatureSearchesEveryLayer)
{
	Scene scene;
	MSector sector;
	Creature under(1), under2(4), ground(2), ground2(5), flying(3), flying2(6);
	sector.AddUndergroundCreature(Handle(under));
	sector.AddUndergroundCreature(Handle(under2));
	sector.AddGroundCreature(Handle(ground));
	sector.AddGroundCreature(Handle(ground2));
	sector.AddFlyingCreature(Handle(flying));
	sector.AddFlyingCreature(Handle(flying2));
	MCreature* pCreature = Handle(under);
	CHECK(!sector.RemoveCreature(9, pCreature));
	CHECK(pCreature == NULL);
	CHECK(!sector.RemoveCreature(9));
	CHECK_EQ(6, sector.GetCreatureSize());
	// Each layer's count and flag follow.
	CHECK(sector.RemoveCreature(5, pCreature));
	CHECK(pCreature == Handle(ground2));
	CHECK_EQ(1, sector.GetGroundCreatureSize());
	CHECK(sector.IsExistGroundCreature());
	CHECK(sector.RemoveCreature(2));
	CHECK_EQ(0, sector.GetGroundCreatureSize());
	CHECK(!sector.IsExistGroundCreature());
	CHECK(sector.RemoveCreature(4));
	CHECK(sector.IsExistUndergroundCreature());
	CHECK(sector.RemoveCreature(1, pCreature));
	CHECK(pCreature == Handle(under));
	CHECK(!sector.IsExistUndergroundCreature());
	CHECK(sector.RemoveCreature(3, pCreature));
	CHECK(pCreature == Handle(flying));
	CHECK(sector.IsExistFlyingCreature());
	CHECK(sector.RemoveCreature(6));
	CHECK(!sector.IsExistFlyingCreature());
	CHECK(!sector.IsExistAnyCreature());
	CHECK_EQ(0, sector.GetCreatureSize());
	// Items and image objects are not creatures.
	Ware item(ITEM_CLASS_POTION, 7);
	Image image(8);
	sector.AddItem(&item);
	sector.AddImageObject(&image);
	sector.AddFlyingCreature(Handle(flying));
	CHECK(!sector.RemoveCreature(7));
	CHECK(!sector.RemoveCreature(8, pCreature));
	CHECK(pCreature == NULL);
	CHECK(sector.RemoveCreature(3));
	CHECK(sector.IsExistItem());
	CHECK_EQ(1, sector.IsExistImageObject());
}

TEST(Sector, WithoutACreatureHostCreaturesAreHeldAsNull)
{
	// The executable always installs it; without, a creature is held as
	// the NULL object the host gives, and read back as NULL.
	ForgetHandles();
	MSector sector;
	const CreatureState::Host* previous = CreatureState::SetHost(nullptr);
	Creature creature(1);
	CHECK(sector.AddGroundCreature(Handle(creature)));
	CHECK_EQ(1, sector.GetGroundCreatureSize());
	CHECK(sector.GetGroundCreatureIterator()->second == NULL);
	CHECK(sector.GetGroundCreature() == NULL);
	CHECK(sector.GetCreature() == NULL);
	// A host that only gives objects: held, but not read back.
	CreatureState::Host objectsOnly{};
	objectsOnly.ObjectOf = creatureHost.ObjectOf;
	CreatureState::SetHost(&objectsOnly);
	MSector other;
	CHECK(other.AddFlyingCreature(Handle(creature)));
	CHECK(other.GetFlyingCreatureIterator()->second == &creature);
	CHECK(other.GetFlyingCreature() == NULL);
	CreatureState::SetHost(previous);
}

TEST(Sector, ImageObjectsAreFoundByID)
{
	MSector sector;
	Image a(1), b(2), c(3);
	CHECK(sector.GetImageObject(1) == NULL);
	MImageObject* pImage = NULL;
	CHECK(!sector.RemoveImageObject(1, pImage));
	CHECK(!sector.RemoveImageObject(1));
	CHECK(sector.AddImageObject(&a));
	CHECK(sector.AddImageObject(&b));
	CHECK(sector.AddImageObject(&c));
	CHECK_EQ(3, sector.GetImageObjectSize());
	CHECK_EQ(3, sector.IsExistImageObject());
	CHECK(sector.GetImageObject(2) == &b);
	CHECK(sector.GetImageObject(3) == &c);
	CHECK(sector.GetImageObject(4) == NULL);
	CHECK((Objects(sector, sector.GetImageObjectIterator(), 3) == std::vector<TYPE_OBJECTID>{1, 2, 3}));
	CHECK(sector.GetImageObjectIterator()->first == MSector::POSITION_IMAGEOBJECT);
	CHECK(!sector.RemoveImageObject(4, pImage));
	CHECK(pImage == NULL);
	CHECK(!sector.RemoveImageObject(4));
	// The last one leaves, then the next.
	CHECK(sector.RemoveImageObject(3, pImage));
	CHECK(pImage == &c);
	CHECK(sector.RemoveImageObject(2));
	CHECK_EQ(1, sector.GetImageObjectSize());
	CHECK(sector.GetImageObject(1) == &a);
	// Image objects take no creature or item flag.
	CHECK(sector.IsEmpty());
}

TEST(Sector, ImageObjectsFillTheirRange)
{
	MSector sector;
	std::vector<std::unique_ptr<Image>> images;
	for (int i = 0; i < MSector::POSITION_EFFECT - MSector::POSITION_IMAGEOBJECT; ++i)
	{
		images.push_back(std::make_unique<Image>(100 + i));
		CHECK(sector.AddImageObject(images.back().get()));
	}
	Image extra(999);
	CHECK(!sector.AddImageObject(&extra));
	CHECK_EQ(69, sector.GetImageObjectSize());
	CHECK(sector.GetImageObject(168) == images.back().get());
}

TEST(Sector, AnImageObjectIsNotMeantToLeaveFirst)
{
	// Recorded, not changed: image objects are looked up from the first
	// key, so once the first is gone none is found.
	MSector sector;
	Image a(1), b(2), c(3);
	sector.AddImageObject(&a);
	sector.AddImageObject(&b);
	// The first leaves: the second is no longer found.
	CHECK(sector.RemoveImageObject(1));
	CHECK(sector.GetImageObject(2) == NULL);
	CHECK(!sector.RemoveImageObject(2));
	CHECK(sector.GetImageObjectIterator() == sector.GetObjectEnd());
	// The next one lands on the second's key: counted, but not held.
	CHECK(sector.AddImageObject(&c));
	CHECK_EQ(2, sector.GetImageObjectSize());
	CHECK(sector.GetImageObject(3) == NULL);
}

TEST(Sector, AnImageObjectMayLeaveMidwayButTheNextIsDropped)
{
	// Recorded, not changed: with the middle one gone the others are still
	// found (each walk stops at the count), but the next one is added at
	// the count's key, which the last one holds: it is dropped, yet
	// counted, and from then on a walk for an ID not there runs past the
	// map's end.
	MSector sector;
	Image a(1), b(2), c(3), d(4);
	sector.AddImageObject(&a);
	sector.AddImageObject(&b);
	sector.AddImageObject(&c);
	CHECK(sector.RemoveImageObject(2));
	CHECK(sector.GetImageObject(1) == &a);
	CHECK(sector.GetImageObject(3) == &c);
	CHECK(sector.AddImageObject(&d));
	CHECK_EQ(3, sector.GetImageObjectSize());
	CHECK((Objects(sector, sector.GetImageObjectIterator(), 2) == std::vector<TYPE_OBJECTID>{1, 3}));
}

TEST(Sector, OustersSafetyAndTheServersUndergroundBlockShareABit)
{
	// Recorded, not changed: both are 0x10 of the second flag byte, and
	// setting the safety replaces the server's blocks.
	MSector sector;
	sector.SetSafe(FLAG_SECTOR_SAFE_OUSTERS);
	CHECK(sector.IsBlockServerUnderground());
	CHECK(!sector.CanStandUndergroundCreature());
	MSector other;
	other.SetBlockServerUnderground();
	CHECK(other.IsSafeOusters());
	other.UnSetBlockServerUnderground();
	CHECK(!other.IsSafeOusters());
	other.SetBlockServerGround();
	other.SetSafe(FLAG_SECTOR_SAFE_SLAYER);
	CHECK(!other.IsBlockServerGround());
	CHECK(other.CanStandGroundCreature());
}

TEST(Sector, EffectsAreKeptInOrder)
{
	MSector sector;
	sector.AddEffect(NULL);
	CHECK(!sector.IsExistEffect());
	auto a = Effect(1, BLT_EFFECT, 0), b = Effect(2, BLT_EFFECT, 0), c = Effect(3, BLT_EFFECT, 0);
	sector.AddEffect(a.get());
	sector.AddEffect(b.get());
	sector.AddEffect(c.get());
	CHECK_EQ(3, sector.GetEffectSize());
	CHECK(*sector.GetEffectIterator() == a.get());
	CHECK(sector.GetEffect(2) == b.get());
	CHECK(sector.GetEffect(4) == NULL);
	MEffect* pEffect = NULL;
	CHECK(!sector.RemoveEffect(4, pEffect));
	CHECK(pEffect == NULL);
	CHECK(!sector.RemoveEffect(4));
	CHECK(sector.RemoveEffect(2, pEffect));
	CHECK(pEffect == b.get());
	CHECK(sector.RemoveEffect(1));
	CHECK_EQ(1, sector.GetEffectSize());
	CHECK(*sector.GetEffectIterator() == c.get());
	sector.ClearEffect();
	CHECK(!sector.IsExistEffect());
	// Effects are not objects of the map.
	CHECK(sector.IsObjectEmpty());
}

TEST(Sector, DarknessAndSanctuaryComeWithTheirEffects)
{
	struct Range { int first, last; bool (MSector::*has)() const; };
	const Range ranges[] = {
		{EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_2_5, &MSector::HasDarkness},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_5, &MSector::HasDarkness},
		{EFFECTSPRITETYPE_SANCTUARY_1, EFFECTSPRITETYPE_SANCTUARY_3, &MSector::HasSanctuary},
	};
	for (const Range& range : ranges)
	{
		for (int type : {range.first, range.last, range.first - 1, range.last + 1})
		{
			const bool inside = type >= range.first && type <= range.last;
			// By the effect's own frame (BLT_EFFECT adds nothing to it).
			for (bool handBack : {false, true})
			{
				MSector sector;
				auto effect = Effect(1, BLT_EFFECT, type);
				sector.AddEffect(effect.get());
				CHECK_EQ(inside, (sector.*range.has)());
				if (inside)
				{
					MEffect* pEffect = NULL;
					CHECK(handBack ? sector.RemoveEffect(1, pEffect) : sector.RemoveEffect(1));
					CHECK(!(sector.*range.has)());
				}
			}
		}
	}
}

TEST(Sector, EachBlendShiftsTheSpriteType)
{
	// An effect's sprite type is its frame, after the earlier blends'
	// sprites: alpha (BLT_EFFECT), screen, normal, then shadow.
	CHECK_EQ(12, GetEffectSpriteType(BLT_EFFECT, 12));
	CHECK_EQ(12 + MAX_EFFECTSPRITETYPE_ALPHAEFFECT, GetEffectSpriteType(BLT_SCREEN, 12));
	CHECK_EQ(12 + MAX_EFFECTSPRITETYPE_SCREENEFFECT, GetEffectSpriteType(BLT_NORMAL, 12));
	CHECK_EQ(12 + MAX_EFFECTSPRITETYPE_NORMALEFFECT, GetEffectSpriteType(BLT_SHADOW, 12));
	// A screen effect's frame reaches the sanctuary sprites.
	const int frame = EFFECTSPRITETYPE_SANCTUARY_1 - MAX_EFFECTSPRITETYPE_ALPHAEFFECT;
	MSector sector;
	auto screen = Effect(1, BLT_SCREEN, frame);
	sector.AddEffect(screen.get());
	CHECK(sector.HasSanctuary());
	CHECK(sector.RemoveEffect(1));
	CHECK(!sector.HasSanctuary());
	// The same frame as an alpha effect is another sprite.
	auto alpha = Effect(2, BLT_EFFECT, frame);
	sector.AddEffect(alpha.get());
	CHECK(!sector.HasSanctuary());
	CHECK(!sector.HasDarkness());
}

TEST(Sector, OneEffectsRemovalClearsTheFlag)
{
	// Recorded, not changed: the flags are not counted, so removing one
	// of two darkness effects clears darkness.
	MSector sector;
	auto first = Effect(1, BLT_EFFECT, EFFECTSPRITETYPE_DARKNESS_2_1);
	auto second = Effect(2, BLT_EFFECT, EFFECTSPRITETYPE_DARKNESS_2_5);
	sector.AddEffect(first.get());
	sector.AddEffect(second.get());
	CHECK(sector.RemoveEffect(1));
	CHECK(!sector.HasDarkness());
	CHECK_EQ(1, sector.GetEffectSize());
	// A plain effect's removal leaves both flags.
	auto dark = Effect(3, BLT_EFFECT, EFFECTSPRITETYPE_DARKNESS_2_1);
	auto holy = Effect(4, BLT_EFFECT, EFFECTSPRITETYPE_SANCTUARY_1);
	auto plain = Effect(5, BLT_EFFECT, 0);
	sector.AddEffect(dark.get());
	sector.AddEffect(holy.get());
	sector.AddEffect(plain.get());
	CHECK(sector.HasDarkness());
	CHECK(sector.HasSanctuary());
	CHECK(sector.RemoveEffect(5));
	CHECK(sector.HasDarkness());
	CHECK(sector.HasSanctuary());
	// With both set, the next is not looked at; removing it still is.
	auto third = Effect(6, BLT_EFFECT, EFFECTSPRITETYPE_GRAY_DARKNESS_2_1);
	sector.AddEffect(third.get());
	MEffect* pEffect = NULL;
	CHECK(sector.RemoveEffect(6, pEffect));
	CHECK(!sector.HasDarkness());
	CHECK(sector.HasSanctuary());
}

TEST(Sector, TheEffectFlagsCanBeSetByHand)
{
	MSector sector;
	sector.SetDarkness();
	sector.SetSanctuary();
	sector.SetGroundElemental();
	sector.SetDarknessForbidden();
	sector.SetFuryOfGnome();
	sector.SetVisitedFlag();
	CHECK(sector.HasDarkness() && sector.HasSanctuary() && sector.HasGroundElemental());
	CHECK(sector.HasDarknessForbidden() && sector.HasFuryOfGnome() && sector.IsVisitedFlag());
	sector.UnSetDarkness();
	sector.UnSetSanctuary();
	sector.UnSetGroundElemental();
	sector.UnSetDarknessForbidden();
	sector.UnSetFuryOfGnome();
	sector.UnSetVisitedFlag();
	CHECK(!sector.HasDarkness() && !sector.HasSanctuary() && !sector.HasGroundElemental());
	CHECK(!sector.HasDarknessForbidden() && !sector.HasFuryOfGnome() && !sector.IsVisitedFlag());
}

TEST(Sector, PortalsAreKeptByZoneOnce)
{
	MSector sector;
	CHECK(sector.AddPortal(1, 30));
	CHECK(sector.IsPortal());
	CHECK(sector.AddPortal(2, 10));
	CHECK(sector.AddPortal(3, 20));
	CHECK(sector.AddPortal(4, 40));
	CHECK(!sector.AddPortal(5, 20));		// the same zone, another type
	CHECK_EQ(4, sector.GetPortalSize());
	std::vector<std::pair<int, int>> portals;
	auto portal = sector.GetPortalBegin();
	for (int i = 0; i < sector.GetPortalSize(); ++i, ++portal)
		portals.push_back({portal->Type, portal->ZoneID});
	CHECK((portals == std::vector<std::pair<int, int>>{{2, 10}, {3, 20}, {1, 30}, {4, 40}}));
	// Added before others when the flag was cleared but the list kept.
	sector.UnSetPortal();
	CHECK(sector.AddPortal(6, 5));
	CHECK(sector.IsPortal());
	sector.ClearPortal();
	CHECK_EQ(0, sector.GetPortalSize());
	CHECK(!sector.IsPortal());
	sector.SetPortal();
	CHECK(sector.IsPortal());
	sector.UnSetPortal();
	CHECK(!sector.IsPortal());
}

TEST(Sector, SoundsAreKeptByIDOnce)
{
	MSector sector;
	CHECK(sector.AddSectorSound(30, 1, 2));
	CHECK(sector.AddSectorSound(SECTORSOUND_INFO(10, 3, 4)));
	CHECK(sector.AddSectorSound(20, 0x105, 0x206));		// the cell in a byte
	CHECK(sector.AddSectorSound(40, 7, 8));
	CHECK(!sector.AddSectorSound(20, 9, 9));
	CHECK(!sector.AddSectorSound(SECTORSOUND_INFO(40, 9, 9)));
	CHECK_EQ(4, sector.GetSectorSoundSize());
	std::vector<std::vector<int>> sounds;
	for (const SECTORSOUND_INFO& info : sector.GetSectorSoundList())
		sounds.push_back({info.ZoneSoundID, info.X, info.Y});
	CHECK((sounds == std::vector<std::vector<int>>{{10, 3, 4}, {20, 5, 6}, {30, 1, 2}, {40, 7, 8}}));
	CHECK_EQ(10, sector.GetSectorSoundBegin()->ZoneSoundID);
	sector.ClearSectorSound();
	CHECK_EQ(0, sector.GetSectorSoundSize());
	CHECK(!sector.IsPortal());				// sounds take no flag
}

TEST(Sector, SafetyIsItsOwnFlags)
{
	MSector sector;
	sector.SetSafe(FLAG_SECTOR_SAFE_SLAYER);
	CHECK(sector.IsSafeSlayer());
	CHECK(!sector.IsSafeVampire());
	CHECK(!sector.IsSafeOusters());
	CHECK(!sector.IsSafeComplete());
	sector.AddSafe(FLAG_SECTOR_SAFE_VAMPIRE);
	CHECK(sector.IsSafeVampire());
	sector.AddSafe(FLAG_SECTOR_SAFE_OUSTERS);
	CHECK(sector.IsSafeOusters());
	sector.RemoveSafe(FLAG_SECTOR_SAFE_SLAYER);
	CHECK(!sector.IsSafeSlayer());
	CHECK_EQ(FLAG_SECTOR_SAFE_VAMPIRE | FLAG_SECTOR_SAFE_OUSTERS, sector.GetSafe());
	sector.SetSafe(FLAG_SECTOR_SAFE_COMMON);
	CHECK(sector.IsSafeComplete());
	CHECK(sector.IsSafeSlayer() && sector.IsSafeVampire() && sector.IsSafeOusters());
	CHECK(sector.IsEmpty());
}

TEST(Sector, TheLightCountsUpAndDown)
{
	MSector sector;
	sector.AddLight();
	sector.AddLight();
	CHECK_EQ(2, sector.GetLight());
	sector.RemoveLight();
	sector.RemoveLight();
	sector.RemoveLight();
	CHECK_EQ(0, sector.GetLight());
	sector.SetLight(0xFF);
	sector.AddLight();
	CHECK_EQ(0xFF, sector.GetLight());
}

TEST(Sector, AFileKeepsTheTileTheBlocksThePortalAndTheLight)
{
	Scene scene;
	MSector sector(0x1234);
	sector.SetBlockUnderground();
	sector.SetBlockFlying();
	sector.SetPortal();
	sector.SetLight(9);
	Ware item(ITEM_CLASS_POTION, 1);
	Creature creature(2);
	sector.AddItem(&item);
	sector.AddGroundCreature(Handle(creature));
	sector.SetSafe(FLAG_SECTOR_SAFE_COMMON);
	sector.SetBlockServerGround();
	const std::string path = TempPath();
	{
		std::ofstream file(path, std::ios::binary);
		sector.SaveToFile(file);
		sector.SetBlockGround();
		sector.UnSetBlockFlying();
		sector.SaveToFile(file);
	}
	std::ifstream file(path, std::ios::binary);
	std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	CHECK((bytes == std::vector<unsigned char>{0x34, 0x12, FLAG_SECTOR_BLOCK_UNDERGROUND | FLAG_SECTOR_BLOCK_FLYING | FLAG_SECTOR_PORTAL, 9,
		0x34, 0x12, FLAG_SECTOR_BLOCK_UNDERGROUND | FLAG_SECTOR_BLOCK_GROUND | FLAG_SECTOR_PORTAL, 9}));
	file.clear();
	file.seekg(0);
	// Loading takes the flags as saved; what the sector holds stays.
	MSector loaded(7);
	loaded.SetLight(3);
	loaded.AddItem(&item);
	loaded.LoadFromFile(file);
	CHECK_EQ(0x1234, loaded.GetSpriteID());
	CHECK(loaded.IsBlockUnderground());
	CHECK(loaded.IsBlockFlying());
	CHECK(!loaded.IsBlockGround());
	CHECK(loaded.IsPortal());
	CHECK(!loaded.IsExistItem());			// the flag is gone
	CHECK(loaded.IsExistObject());			// the item is not
	CHECK_EQ(9, loaded.GetLight());
	loaded.LoadFromFile(file);
	CHECK(loaded.IsBlockGround());
	CHECK(!loaded.IsBlockFlying());
	file.close();
	std::remove(path.c_str());
}

TEST(Sector, AssigningCopiesTheTileAndAddsTheBlocks)
{
	Scene scene;
	MSector from(40);
	from.SetBlockUnderground();
	from.SetBlockGround();
	from.SetPortal();
	from.SetLight(5);
	from.SetSafe(FLAG_SECTOR_SAFE_COMMON);
	MSector to(3);
	to.SetBlockFlying();
	Ware item(ITEM_CLASS_POTION, 1);
	to.AddItem(&item);
	to = from;
	CHECK_EQ(40, to.GetSpriteID());
	CHECK(to.IsBlockUnderground() && to.IsBlockGround() && to.IsBlockFlying());
	CHECK(!to.IsPortal());
	CHECK(to.IsExistItem());
	CHECK_EQ(0, to.GetLight());
	CHECK_EQ(0, to.GetSafe());
	// Only the underground block, then only the flying one.
	MSector under, flying, target;
	under.SetBlockUnderground();
	target = under;
	CHECK(target.IsBlockUnderground() && !target.IsBlockGround() && !target.IsBlockFlying());
	flying.SetBlockFlying();
	MSector target2;
	target2 = flying;
	CHECK(target2.IsBlockFlying() && !target2.IsBlockGround() && !target2.IsBlockUnderground());
}

TEST(Sector, RemovingAllObjectsClearsTheFlags)
{
	Scene scene;
	MSector sector(6);
	Ware item(ITEM_CLASS_POTION, 1);
	Creature under(2), ground(3), flying(4);
	Image image(5);
	sector.AddItem(&item);
	sector.AddUndergroundCreature(Handle(under));
	sector.AddGroundCreature(Handle(ground));
	sector.AddFlyingCreature(Handle(flying));
	sector.AddImageObject(&image);
	sector.SetBlockAll();
	sector.SetSafe(FLAG_SECTOR_SAFE_COMMON);
	sector.SetLight(4);
	sector.AddPortal(1, 2);
	auto effect = Effect(1, BLT_EFFECT, 0);
	sector.AddEffect(effect.get());
	sector.RemoveAllObject();
	CHECK(sector.IsObjectEmpty());
	CHECK(sector.IsEmpty());
	CHECK_EQ(0, sector.GetSafe());
	CHECK_EQ(0, sector.GetCreatureSize());
	CHECK_EQ(0, sector.GetUndergroundCreatureSize());
	CHECK_EQ(0, sector.GetFlyingCreatureSize());
	CHECK_EQ(0, sector.GetImageObjectSize());
	CHECK(!sector.IsPortal());
	// The tile, the light, the effects and the portal list stay.
	CHECK_EQ(6, sector.GetSpriteID());
	CHECK_EQ(4, sector.GetLight());
	CHECK_EQ(1, sector.GetEffectSize());
	CHECK_EQ(1, sector.GetPortalSize());
	// Each layer fills from its first key again.
	CHECK(sector.AddGroundCreature(Handle(ground)));
	CHECK(sector.GetGroundCreatureIterator()->first == MSector::POSITION_GROUNDCREATURE);
	// Set keeps the objects.
	sector.Set(8);
	CHECK_EQ(8, sector.GetSpriteID());
	CHECK(sector.IsExistObject());
	CHECK(sector.IsEmpty());
}
