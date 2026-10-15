#include "test_framework.h"
#include "MEffectGeneratorTable.h"
#include "MEffectGenerator.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MEffect.h"
#include "MEffectTarget.h"
#include "WorldEventsHost.h"
#include "WorldTileGeometry.h"
#include "SkillDef.h"
#include "EffectSpriteTypeDef.h"
#include <vector>

// The effect generator table (task 4.219, gamemodel's since then). Init
// holds one generator of each kind, by its ID. Generate starts an action's
// current phase on its target:
// - an action past the table, or no target: nothing;
// - a target whose phases do not match the action's: its result released
//   unrun and the target dropped;
// - an action without phases: the result's time;
// - a phase naming no generator: as a mismatch.
// Otherwise the phase's generator gets the phase's step, count (the
// target's delay on a delay node), link count, sprite (Acid Touch and
// Bloody Nail at power 2 take their own), the start, the target's place
// and ID, and the target unless this was its last phase. On success the
// phase's sound plays at the start's cell, and a result-time phase marks
// the result's time; on failure the result's time comes at once. At the
// result's time or the last phase the result runs and the effect leaves
// the player; when nothing was generated, or at the last phase, the
// target goes. (A target also leaves the player when destroyed, so an
// explicit leave shows as two entries in the record below, the
// destructor's alone as one.) GenerateNext carries an effect's target to its next phase
// the same way, from the effect's place.
namespace {

std::vector<EFFECTGENERATOR_INFO> generated;
bool generatorAnswer = true;
struct Sound { TYPE_SOUNDID id; bool repeat; int x, y; };
std::vector<Sound> sounds;
std::vector<BYTE> leftPlayer;
int executed = 0;
int generatorsDestroyed = 0;

struct FakeGenerator : MEffectGenerator
{
	TYPE_EFFECTGENERATORID id;
	explicit FakeGenerator(TYPE_EFFECTGENERATORID i) : id(i) {}
	~FakeGenerator() override { ++generatorsDestroyed; }
	TYPE_EFFECTGENERATORID GetID() override { return id; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override { generated.push_back(info); return generatorAnswer; }
};

struct Table : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
	MEffectGenerator* At(int id) const { return m_pEffectGenerator[id]; }
};

struct CountingNode : MActionResultNode
{
	void Execute() override { ++executed; }
};

struct Target : MEffectTarget
{
	int& destroyed;
	Target(BYTE max, int& count) : MEffectTarget(max), destroyed(count) { Set(10, 20, 30, 77); }
	~Target() override { ++destroyed; }
	void GiveResult() { auto* result = new MActionResult; result->Add(new CountingNode); SetResult(result); }
};

const WorldEvents::Host worldHost{
	.PlaySoundAt = [](TYPE_SOUNDID id, bool repeat, int x, int y) { sounds.push_back({id, repeat, x, y}); },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { leftPlayer.push_back(id); },
};
const MEffectHost effectHost{
	.CurrentFrame = []() -> DWORD { return 100; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 0; },
};

const TYPE_ACTIONINFO kAction = 5;
const TYPE_EFFECTGENERATORID kGenerator = 7;

struct Fixture
{
	MActionInfoTable actions;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const MEffectTargetHost* oldTarget = MEffectTarget::SetHost(&targetHost);
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	Table table;
	int destroyed = 0;
	Fixture()
	{
		generated.clear(); sounds.clear(); leftPlayer.clear(); executed = 0; generatorAnswer = true;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		table.Add(new FakeGenerator(kGenerator));
	}
	~Fixture()
	{
		g_pActionInfoTable = oldActions;
		WorldEvents::SetHost(oldWorld);
		MEffectTarget::SetHost(oldTarget);
		MEffect::SetHost(oldEffect);
	}
	// An action of `phases` phases, each by the fake generator.
	ACTION_INFO_NODE& Phase(TYPE_ACTIONINFO action, int phase, int phases)
	{
		MActionInfo* info = actions.GetMutable(action);
		if (info->GetSize() != phases) info->Init(phases);
		ACTION_INFO_NODE* node = info->GetMutable(phase);
		node->EffectGeneratorID = kGenerator;
		node->EffectSpriteType = 12;
		node->Step = 4;
		node->Count = 5;
		node->LinkCount = 9;
		return *node;
	}
	Target* NewTarget(BYTE max) { return new Target(max, destroyed); }
};

} // namespace

TEST(EffectGeneratorTable, InitHoldsEachGeneratorByItsID)
{
	Table table;
	table.Init();
	int held = 0;
	for (int id = 0; id < MAX_EFFECTGENERATORID; ++id)
	{
		if (MEffectGenerator* generator = table.At(id))
		{
			++held;
			CHECK_EQ(id, generator->GetID());
		}
	}
	CHECK_EQ(43, held);
	table.Release();
	for (int id = 0; id < MAX_EFFECTGENERATORID; ++id)
		CHECK(table.At(id) == nullptr);
}

TEST(EffectGeneratorTable, ASecondGeneratorOfAnIDIsDropped)
{
	generatorsDestroyed = 0;
	{
		Table table;
		auto* first = new FakeGenerator(3);
		table.Add(first);
		table.Add(new FakeGenerator(3));
		CHECK_EQ(1, generatorsDestroyed);
		CHECK(table.At(3) == first);
	}
	CHECK_EQ(2, generatorsDestroyed);
}

TEST(EffectGeneratorTable, NothingStartsPastTheTableOrWithoutATarget)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 1);
	Target* target = fixture.NewTarget(1);
	fixture.table.Generate(0, 0, 0, 0, 1, fixture.actions.GetSize(), target);
	CHECK_EQ(1, fixture.destroyed);
	fixture.table.Generate(0, 0, 0, 0, 1, kAction, nullptr);
	CHECK(generated.empty());
	CHECK(sounds.empty());
}

TEST(EffectGeneratorTable, AMismatchedOrUnboundPhaseDropsTheTargetUnrun)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 2);
	fixture.Phase(kAction, 1, 2);
	Target* mismatched = fixture.NewTarget(3);
	mismatched->GiveResult();
	fixture.table.Generate(0, 0, 0, 0, 1, kAction, mismatched);
	CHECK_EQ(1, fixture.destroyed);
	CHECK_EQ(0, executed);

	fixture.Phase(kAction + 1, 0, 1).EffectGeneratorID = kGenerator + 1;	// no such generator
	Target* unbound = fixture.NewTarget(1);
	unbound->GiveResult();
	fixture.table.Generate(0, 0, 0, 0, 1, kAction + 1, unbound);
	CHECK_EQ(2, fixture.destroyed);
	CHECK_EQ(0, executed);
	CHECK(generated.empty());
}

TEST(EffectGeneratorTable, AnActionWithoutPhasesRunsTheResult)
{
	Fixture fixture;
	Target* target = fixture.NewTarget(0);
	target->GiveResult();
	fixture.table.Generate(0, 0, 0, 0, 1, kAction, target);
	CHECK_EQ(1, executed);
	CHECK_EQ(2u, leftPlayer.size());
	CHECK_EQ(1, fixture.destroyed);
	CHECK(generated.empty());
}

TEST(EffectGeneratorTable, APhaseIsGeneratedWithItsNodeAndTheTarget)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 2).SoundID = 33;
	fixture.Phase(kAction, 1, 2);
	Target* target = fixture.NewTarget(2);
	target->GiveResult();
	fixture.table.Generate(192, 64, 1, 3, 1, kAction, target, 8, 9);
	CHECK(generated.size() == 1u);
	if (generated.size() != 1u) return;
	const EFFECTGENERATOR_INFO& info = generated[0];
	CHECK_EQ(kAction, info.nActionInfo);
	CHECK(info.pEffectTarget == target);		// not its last phase
	CHECK_EQ(12, info.effectSpriteType);
	CHECK(info.x0 == 192 && info.y0 == 64 && info.z0 == 1);
	CHECK(info.x1 == 10 && info.y1 == 20 && info.z1 == 30);
	CHECK_EQ(77, info.creatureID);
	CHECK_EQ(3, info.direction);
	CHECK_EQ(4, info.step);
	CHECK_EQ(5, info.count);
	CHECK_EQ(9, info.linkCount);
	CHECK_EQ(1, info.power);
	CHECK(info.pPreviousEffect == nullptr);
	CHECK(info.temp1 == 8 && info.temp2 == 9);
	CHECK(sounds.size() == 1u);
	if (!sounds.empty())
	{
		CHECK_EQ(33, sounds[0].id);
		CHECK(!sounds[0].repeat);
		CHECK_EQ(WorldTileGeometry::PixelToTileX(192), sounds[0].x);
		CHECK_EQ(WorldTileGeometry::PixelToTileY(64), sounds[0].y);
	}
	// Not the result's time: nothing runs, and the effect stays the
	// player's. No generator took the target, so its owner drops it.
	CHECK_EQ(0, executed);
	CHECK_EQ(1u, leftPlayer.size());		// the destructor's only
	CHECK_EQ(1, fixture.destroyed);
}

TEST(EffectGeneratorTable, ADelayNodeCountsTheTargetsDelay)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 2).SetDelayNode();
	fixture.Phase(kAction, 1, 2);
	Target* delayed = fixture.NewTarget(2);
	delayed->SetDelayFrame(40);
	fixture.table.Generate(0, 0, 0, 0, 1, kAction, delayed);
	Target* undelayed = fixture.NewTarget(2);
	fixture.table.Generate(0, 0, 0, 0, 1, kAction, undelayed);
	CHECK(generated.size() == 2u);
	if (generated.size() == 2u)
	{
		CHECK_EQ(40, generated[0].count);
		CHECK_EQ(5, generated[1].count);
	}
}

TEST(EffectGeneratorTable, AcidTouchAndBloodyNailAtPowerTwoTakeTheirOwnSprite)
{
	Fixture fixture;
	for (TYPE_ACTIONINFO action : {TYPE_ACTIONINFO(MAGIC_ACID_TOUCH), TYPE_ACTIONINFO(MAGIC_BLOODY_NAIL), kAction})
	{
		fixture.Phase(action, 0, 2);
		fixture.Phase(action, 1, 2);
		for (BYTE power : {BYTE(1), BYTE(2)})
			fixture.table.Generate(0, 0, 0, 0, power, action, fixture.NewTarget(2));
	}
	CHECK(generated.size() == 6u);
	if (generated.size() != 6u) return;
	CHECK_EQ(12, generated[0].effectSpriteType);
	CHECK_EQ(EFFECTSPRITETYPE_ACID_TOUCH, generated[1].effectSpriteType);
	CHECK_EQ(12, generated[2].effectSpriteType);
	CHECK_EQ(EFFECTSPRITETYPE_BLOODY_NAIL, generated[3].effectSpriteType);
	CHECK_EQ(12, generated[4].effectSpriteType);
	CHECK_EQ(12, generated[5].effectSpriteType);
}

TEST(EffectGeneratorTable, AFailedPhaseRunsTheResultSilently)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 2).SoundID = 33;
	fixture.Phase(kAction, 1, 2);
	generatorAnswer = false;
	Target* target = fixture.NewTarget(2);
	target->GiveResult();
	fixture.table.Generate(0, 0, 0, 0, 1, kAction, target);
	CHECK_EQ(1u, generated.size());
	CHECK(sounds.empty());
	CHECK_EQ(1, executed);
	CHECK_EQ(2u, leftPlayer.size());
	CHECK_EQ(1, fixture.destroyed);
}

TEST(EffectGeneratorTable, AResultTimePhaseOrTheLastPhaseRunsTheResult)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 2).bResultTime = true;
	fixture.Phase(kAction, 1, 2);
	Target* timed = fixture.NewTarget(2);
	timed->GiveResult();
	fixture.table.Generate(0, 0, 0, 0, 1, kAction, timed);
	CHECK_EQ(1, executed);
	CHECK_EQ(2u, leftPlayer.size());

	fixture.Phase(kAction + 1, 0, 1);
	Target* last = fixture.NewTarget(1);
	last->GiveResult();
	fixture.table.Generate(0, 0, 0, 0, 1, kAction + 1, last);
	CHECK(generated.size() == 2u);
	if (generated.size() == 2u) CHECK(generated[1].pEffectTarget == nullptr);
	CHECK_EQ(2, executed);
	CHECK_EQ(4u, leftPlayer.size());
	CHECK_EQ(2, fixture.destroyed);
}

TEST(EffectGeneratorTable, GenerateNextNeedsAnEffectWithATarget)
{
	Fixture fixture;
	fixture.table.GenerateNext(nullptr);
	MEffect effect(BLT_EFFECT);
	fixture.table.GenerateNext(&effect);
	CHECK(generated.empty());
}

TEST(EffectGeneratorTable, GenerateNextFinishesAnEndedTarget)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 1);
	Target* target = fixture.NewTarget(1);
	target->NextPhase();
	target->GiveResult();
	MEffect effect(BLT_EFFECT);
	effect.SetLink(kAction, target);
	fixture.table.GenerateNext(&effect);
	CHECK(generated.empty());
	CHECK_EQ(1, executed);
	CHECK_EQ(2u, leftPlayer.size());
	CHECK_EQ(1, fixture.destroyed);
	CHECK(effect.GetEffectTarget() == nullptr);
}

TEST(EffectGeneratorTable, GenerateNextStartsTheNextPhaseFromTheEffect)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 2);
	fixture.Phase(kAction, 1, 2).SoundID = 44;
	Target* target = fixture.NewTarget(2);
	target->NextPhase();
	MEffect effect(BLT_EFFECT);
	effect.SetPixelPosition(256, 96, 2);
	effect.SetDirection(5);
	effect.SetPower(2);
	effect.SetLink(kAction, target);
	fixture.table.GenerateNext(&effect);
	CHECK(generated.size() == 1u);
	if (generated.size() != 1u) return;
	const EFFECTGENERATOR_INFO& info = generated[0];
	CHECK(info.x0 == 256 && info.y0 == 96 && info.z0 == 2);
	CHECK(info.x1 == 10 && info.y1 == 20 && info.z1 == 30);
	CHECK_EQ(5, info.direction);
	CHECK_EQ(2, info.power);
	CHECK(info.pPreviousEffect == &effect);
	CHECK(info.pEffectTarget == nullptr);		// its last phase
	CHECK(info.temp1 == 0 && info.temp2 == 0);
	CHECK(sounds.size() == 1u);
	if (!sounds.empty())
	{
		CHECK_EQ(44, sounds[0].id);
		CHECK_EQ(WorldTileGeometry::PixelToTileX(256), sounds[0].x);
		CHECK_EQ(WorldTileGeometry::PixelToTileY(96), sounds[0].y);
	}
	// Its last phase: the effect leaves the player (and again as the
	// target is destroyed).
	CHECK_EQ(2u, leftPlayer.size());
	CHECK_EQ(1, fixture.destroyed);
}

TEST(EffectGeneratorTable, GenerateNextAtAResultTimePhaseRunsTheResult)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 3);
	fixture.Phase(kAction, 1, 3).bResultTime = true;
	fixture.Phase(kAction, 2, 3);
	Target* target = fixture.NewTarget(3);
	target->NextPhase();
	target->GiveResult();
	MEffect effect(BLT_EFFECT);
	effect.SetLink(kAction, target);
	fixture.table.GenerateNext(&effect);
	CHECK_EQ(1u, generated.size());
	CHECK_EQ(1, executed);
	CHECK_EQ(2u, leftPlayer.size());
}

TEST(EffectGeneratorTable, GenerateNextFailingRunsTheResult)
{
	Fixture fixture;
	fixture.Phase(kAction, 0, 3);
	fixture.Phase(kAction, 1, 3).SoundID = 44;
	fixture.Phase(kAction, 2, 3);
	generatorAnswer = false;
	Target* target = fixture.NewTarget(3);
	target->NextPhase();
	target->GiveResult();
	MEffect effect(BLT_EFFECT);
	effect.SetLink(kAction, target);
	fixture.table.GenerateNext(&effect);
	CHECK_EQ(1u, generated.size());
	CHECK(sounds.empty());
	CHECK_EQ(1, executed);
	CHECK_EQ(1, fixture.destroyed);
}
