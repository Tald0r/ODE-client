#include "test_framework.h"
#include "MHelicopterManager.h"
#include "MChaseEffect.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "SoundDef.h"
#include "WorldTileGeometry.h"
#include <map>
#include <vector>

// The helicopters (task 4.226). Calling one for a creature in the zone
// starts its propeller sound and puts its shadow, a chase effect, 500
// pixels right and 300 up of the creature; the manager keeps one per
// creature, moves each sound with its shadow, opens the way-point
// selection when the player's arrives, sends one away on request, and
// drops one whose shadow is gone or which arrived after being sent away.
namespace {

struct Sound { TYPE_SOUNDID id; int x, y; bool deleted; };

struct Zone
{
	std::map<TYPE_OBJECTID, std::pair<int, int>> creatures;	// sectors
	std::map<TYPE_OBJECTID, MEffect*> effects;
	std::vector<Sound> sounds;
	TYPE_OBJECTID player = 1;
	bool selecting = false;
	int waypointSelections = 0;
	~Zone() { for (auto& entry : effects) delete entry.second; }
};
Zone* zone = nullptr;

ZONESOUND_NODE* Node(size_t index) { return reinterpret_cast<ZONESOUND_NODE*>(index + 1); }
Sound& SoundOf(ZONESOUND_NODE* pSound) { return zone->sounds[reinterpret_cast<size_t>(pSound) - 1]; }

const MHelicopterHost helicopterHost{
	.GetCreaturePosition = [](TYPE_OBJECTID id, int& x, int& y) {
		const auto entry = zone->creatures.find(id);
		if (entry == zone->creatures.end()) return false;
		x = entry->second.first;
		y = entry->second.second;
		return true;
	},
	.AddEffect = [](MEffect* pEffect) { zone->effects[pEffect->GetID()] = pEffect; },
	.GetEffect = [](TYPE_OBJECTID id) {
		const auto entry = zone->effects.find(id);
		return entry == zone->effects.end() ? nullptr : entry->second;
	},
	.RemoveEffect = [](TYPE_OBJECTID id) {
		const auto entry = zone->effects.find(id);
		if (entry == zone->effects.end()) return;
		delete entry->second;
		zone->effects.erase(entry);
	},
	.GetMaxEffectFrame = [](BLT_TYPE bltType, TYPE_FRAMEID frameID) { return bltType == BLT_SCREEN && frameID == 7 ? 9 : 1; },
	.PlayerID = []() { return zone->player; },
	.IsSelectingWayPoint = []() { return zone->selecting; },
	.SelectWayPoint = []() { ++zone->waypointSelections; zone->selecting = true; },
	.PlayLoop = [](TYPE_SOUNDID id, int x, int y) {
		zone->sounds.push_back({id, x, y, false});
		return Node(zone->sounds.size() - 1);
	},
	.MoveSound = [](ZONESOUND_NODE* pSound, int x, int y) { SoundOf(pSound).x = x; SoundOf(pSound).y = y; },
	.DeleteSound = [](ZONESOUND_NODE* pSound) { SoundOf(pSound).deleted = true; },
};

// A traced creature's pixel position: its sector's.
const MGuidanceEffectHost guidanceHost{
	.CreaturePosition = [](TYPE_OBJECTID id, int& x, int& y, int& z) {
		const auto entry = zone->creatures.find(id);
		if (entry == zone->creatures.end()) return false;
		x = WorldTileGeometry::TileToPixelX(entry->second.first);
		y = WorldTileGeometry::TileToPixelY(entry->second.second);
		z = 0;
		return true;
	},
};

const MEffectHost effectHost{
	.CurrentFrame = []() -> DWORD { return 100; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 0; },
};

struct Fixture
{
	Zone state;
	EFFECTSPRITETYPE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE* oldSprites = g_pEffectSpriteTypeTable;
	const MHelicopterHost* oldHelicopter = MHelicopter::SetHost(&helicopterHost);
	const MGuidanceEffectHost* oldGuidance = MGuidanceEffect::SetHost(&guidanceHost);
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	Fixture()
	{
		zone = &state;
		sprites.Init(EFFECTSPRITETYPE_SUMMON_HELICOPTER + 1);
		sprites.GetMutable(EFFECTSPRITETYPE_SUMMON_HELICOPTER)->BltType = BLT_SCREEN;
		sprites.GetMutable(EFFECTSPRITETYPE_SUMMON_HELICOPTER)->FrameID = 7;
		g_pEffectSpriteTypeTable = &sprites;
		state.creatures[1] = {10, 20};		// the player
		state.creatures[2] = {30, 40};
	}
	~Fixture()
	{
		g_pEffectSpriteTypeTable = oldSprites;
		MHelicopter::SetHost(oldHelicopter);
		MGuidanceEffect::SetHost(oldGuidance);
		MEffect::SetHost(oldEffect);
		zone = nullptr;
	}
	MChaseEffect* Shadow(const MHelicopter* pHelicopter)
	{
		MEffect* pEffect = helicopterHost.GetEffect(pHelicopter->GetEffectID());
		return pEffect != nullptr && pEffect->GetEffectType() == MEffect::EFFECT_CHASE
			? static_cast<MChaseEffect*>(pEffect) : nullptr;
	}
	MHelicopter* Call(TYPE_OBJECTID id)
	{
		auto* pHelicopter = new MHelicopter;
		pHelicopter->SetChaseCreature(id, 0, 0);
		return pHelicopter;
	}
	// Runs a shadow until its chase is over; false if it never is.
	static bool Arrive(MChaseEffect* pShadow)
	{
		for (int i = 0; i < 500 && !pShadow->IsChaseOver(); ++i) pShadow->Update();
		return pShadow->IsChaseOver();
	}
};

} // namespace

TEST(Helicopter, WithoutAHostThereIsNothingToChase)
{
	const MHelicopterHost* old = MHelicopter::SetHost(nullptr);
	{
		MHelicopter helicopter;
		CHECK(!helicopter.SetChaseCreature(1, 0, 0));
		CHECK_EQ(OBJECTID_NULL, helicopter.GetCreatureID());
		helicopter.UpdateSound(1, 2);
		MHelicopterManager manager;
		manager.Update();
		manager.RemoveHelicopter(1);
	}
	MHelicopter::SetHost(old);
}

TEST(Helicopter, NoCreatureNoHelicopter)
{
	Fixture f;
	MHelicopter helicopter;
	CHECK(!helicopter.SetChaseCreature(99, 5, 6));
	CHECK_EQ(OBJECTID_NULL, helicopter.GetCreatureID());
	CHECK_EQ(OBJECTID_NULL, helicopter.GetEffectID());
	CHECK(f.state.effects.empty());
	CHECK(f.state.sounds.empty());
}

TEST(Helicopter, ItsShadowChasesTheCreatureWithAPropeller)
{
	Fixture f;
	MHelicopter helicopter;
	CHECK(helicopter.SetChaseCreature(2, 5, 6));
	CHECK_EQ(2, helicopter.GetCreatureID());
	CHECK(f.state.sounds.size() == 1u);
	if (f.state.sounds.size() == 1u)
	{
		CHECK_EQ(SOUND_WORLD_PROPELLER, f.state.sounds[0].id);
		CHECK(f.state.sounds[0].x == 5 && f.state.sounds[0].y == 6);
	}
	CHECK_EQ(1u, f.state.effects.size());
	MChaseEffect* pShadow = f.Shadow(&helicopter);
	CHECK(pShadow != nullptr);
	if (pShadow == nullptr) return;
	CHECK_EQ(BLT_SCREEN, pShadow->GetBltType());
	CHECK_EQ(7, pShadow->GetFrameID());
	CHECK_EQ(9, pShadow->GetMaxFrame());
	CHECK_EQ(WorldTileGeometry::TileToPixelX(30) + 500, pShadow->GetPixelX());
	CHECK_EQ(WorldTileGeometry::TileToPixelY(40) - 300, pShadow->GetPixelY());
	CHECK_EQ(0, pShadow->GetPixelZ());
	CHECK_EQ(2, pShadow->GetTraceCreatureID());
	CHECK_EQ(20, pShadow->GetStepPixel());
	CHECK(Fixture::Arrive(pShadow));
	CHECK_EQ(WorldTileGeometry::TileToPixelX(30), pShadow->GetPixelX());
	CHECK_EQ(WorldTileGeometry::TileToPixelY(40), pShadow->GetPixelY());
}

TEST(Helicopter, CallingItAgainReplacesItsSound)
{
	Fixture f;
	MHelicopter helicopter;
	helicopter.SetChaseCreature(2, 5, 6);
	helicopter.SetChaseCreature(1, 7, 8);
	CHECK_EQ(1, helicopter.GetCreatureID());
	CHECK(f.state.sounds.size() == 2u);
	if (f.state.sounds.size() == 2u)
	{
		CHECK(f.state.sounds[0].deleted);
		CHECK(!f.state.sounds[1].deleted);
		CHECK(f.state.sounds[1].x == 7 && f.state.sounds[1].y == 8);
	}
	// A failed call releases it.
	CHECK(!helicopter.SetChaseCreature(99, 0, 0));
	CHECK(f.state.sounds[1].deleted);
	CHECK_EQ(OBJECTID_NULL, helicopter.GetCreatureID());
	CHECK_EQ(OBJECTID_NULL, helicopter.GetEffectID());
}

TEST(Helicopter, ReleasingItRemovesItsShadowAndSound)
{
	Fixture f;
	{
		MHelicopter helicopter;
		helicopter.SetChaseCreature(2, 5, 6);
		helicopter.UpdateSound(11, 12);
		CHECK(f.state.sounds[0].x == 11 && f.state.sounds[0].y == 12);
		helicopter.Release();
		CHECK(f.state.effects.empty());
		CHECK(f.state.sounds[0].deleted);
		CHECK_EQ(OBJECTID_NULL, helicopter.GetCreatureID());
		CHECK_EQ(OBJECTID_NULL, helicopter.GetEffectID());
		helicopter.UpdateSound(13, 14);			// no sound any more
		CHECK(f.state.sounds[0].x == 11);
		helicopter.SetChaseCreature(1, 0, 0);
	}
	// The destructor releases it too.
	CHECK(f.state.effects.empty());
	CHECK(f.state.sounds[1].deleted);
}

TEST(HelicopterManager, OnePerCreature)
{
	Fixture f;
	MHelicopterManager manager;
	MHelicopter* first = f.Call(2);
	CHECK(manager.AddHelicopter(first));
	CHECK(manager.GetHelicopter(2) == first);
	CHECK(manager.GetHelicopter(1) == nullptr);
	MHelicopter* second = f.Call(2);
	CHECK(manager.AddHelicopter(second));
	CHECK(manager.GetHelicopter(2) == second);
	CHECK(f.state.sounds[0].deleted);			// the first is gone
	CHECK_EQ(1u, f.state.effects.size());
	manager.Release();
	CHECK(manager.GetHelicopter(2) == nullptr);
	CHECK(f.state.effects.empty());
	CHECK(f.state.sounds[1].deleted);
}

TEST(HelicopterManager, RemovingSoonDeletesAtOnce)
{
	Fixture f;
	MHelicopterManager manager;
	manager.AddHelicopter(f.Call(2));
	manager.RemoveHelicopterSoon(1);			// none for the player
	CHECK(manager.GetHelicopter(2) != nullptr);
	manager.RemoveHelicopterSoon(2);
	CHECK(manager.GetHelicopter(2) == nullptr);
	CHECK(f.state.effects.empty());
	CHECK(f.state.sounds[0].deleted);
}

TEST(HelicopterManager, RemovingSendsItAwayUntilItArrives)
{
	Fixture f;
	MHelicopterManager manager;
	MHelicopter* pHelicopter = f.Call(2);
	manager.AddHelicopter(pHelicopter);
	MChaseEffect* pShadow = f.Shadow(pHelicopter);
	if (pShadow == nullptr) { CHECK(false); return; }
	const int x = pShadow->GetPixelX(), y = pShadow->GetPixelY();
	manager.RemoveHelicopter(2);
	CHECK(manager.GetHelicopter(2) == pHelicopter);
	CHECK_EQ(OBJECTID_NULL, pShadow->GetTraceCreatureID());
	CHECK(!pShadow->IsChaseOver());
	manager.Update();
	CHECK(manager.GetHelicopter(2) == pHelicopter);	// still flying away
	// Off to 1000 left and 600 down; asking again changes nothing.
	CHECK(Fixture::Arrive(pShadow));
	manager.RemoveHelicopter(2);
	CHECK_EQ(x - 1000, pShadow->GetPixelX());
	CHECK_EQ(y + 600, pShadow->GetPixelY());
	pShadow->Update();							// still there
	CHECK(pShadow->IsChaseOver());
	manager.Update();
	CHECK(manager.GetHelicopter(2) == nullptr);
	CHECK(f.state.effects.empty());
	CHECK(f.state.sounds[0].deleted);
}

TEST(HelicopterManager, WithoutItsShadowItGoes)
{
	Fixture f;
	MHelicopterManager manager;
	MHelicopter* first = f.Call(2);
	MHelicopter* second = f.Call(1);
	manager.AddHelicopter(first);
	manager.AddHelicopter(second);
	helicopterHost.RemoveEffect(first->GetEffectID());
	manager.RemoveHelicopter(2);					// at once
	CHECK(manager.GetHelicopter(2) == nullptr);
	CHECK(f.state.sounds[0].deleted);
	helicopterHost.RemoveEffect(second->GetEffectID());
	manager.Update();
	CHECK(manager.GetHelicopter(1) == nullptr);
	CHECK(f.state.sounds[1].deleted);
}

TEST(HelicopterManager, ItsSoundFollowsItsShadow)
{
	Fixture f;
	MHelicopterManager manager;
	MHelicopter* pHelicopter = f.Call(2);
	manager.AddHelicopter(pHelicopter);
	MChaseEffect* pShadow = f.Shadow(pHelicopter);
	if (pShadow == nullptr) { CHECK(false); return; }
	pShadow->Update();
	manager.Update();
	CHECK_EQ(pShadow->GetX(), f.state.sounds[0].x);
	CHECK_EQ(pShadow->GetY(), f.state.sounds[0].y);
	CHECK(Fixture::Arrive(pShadow));
	manager.Update();
	// Arriving at another creature ends nothing.
	CHECK(manager.GetHelicopter(2) == pHelicopter);
	CHECK_EQ(0, f.state.waypointSelections);
}

TEST(HelicopterManager, ThePlayersArrivalOpensTheWayPoints)
{
	Fixture f;
	MHelicopterManager manager;
	MHelicopter* pHelicopter = f.Call(1);
	manager.AddHelicopter(pHelicopter);
	MChaseEffect* pShadow = f.Shadow(pHelicopter);
	if (pShadow == nullptr) { CHECK(false); return; }
	manager.Update();
	CHECK_EQ(0, f.state.waypointSelections);		// not there yet
	CHECK(Fixture::Arrive(pShadow));
	manager.Update();
	CHECK_EQ(1, f.state.waypointSelections);
	manager.Update();								// already open
	CHECK_EQ(1, f.state.waypointSelections);
	CHECK(manager.GetHelicopter(1) == pHelicopter);
}

TEST(HelicopterManager, AnotherKindOfEffectIsLeftAlone)
{
	Fixture f;
	MHelicopterManager manager;
	MHelicopter* pHelicopter = f.Call(2);
	manager.AddHelicopter(pHelicopter);
	// Its ID now names a plain effect.
	const TYPE_OBJECTID id = pHelicopter->GetEffectID();
	auto* pPlain = new MEffect(BLT_EFFECT);
	pPlain->SetID(id);
	delete f.state.effects[id];
	f.state.effects[id] = pPlain;
	manager.RemoveHelicopter(2);
	manager.Update();
	CHECK(manager.GetHelicopter(2) == pHelicopter);
	CHECK(f.state.sounds[0].x == 0 && f.state.sounds[0].y == 0);
}
