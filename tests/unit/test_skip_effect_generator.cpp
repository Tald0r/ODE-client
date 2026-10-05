#include "test_framework.h"
#include "MSkipEffectGenerator.h"
#include "MSkipEffect.h"
#include "WorldTileGeometry.h"
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

// Regression guards for the extracted generator; the executable-only original
// was inspected, not run against these tests.
namespace {
std::vector<std::unique_ptr<MEffect>> effects;
std::vector<int> slots, removed;
int mask, attempts, destroyed, resultsDestroyed;
bool metadata;
DWORD now;
MFixedZoneEffectSprite sprite;
const MEffectHost effectHost{.CurrentFrame = [] { return now; }};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removed.push_back(id); }};
const MFixedZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& value) { CHECK_EQ(17, type); value = sprite; return metadata; },
	.Queue = [](std::unique_ptr<MEffect> effect) {
		const int slot = attempts++;
		CHECK(dynamic_cast<MSkipEffect*>(effect.get()) != nullptr);
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!(mask & (1 << slot))) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};
struct ResultNode : MActionResultNode
{
	~ResultNode() override { ++resultsDestroyed; }
	void Execute() override {}
};
struct Target : MEffectTarget
{
	using MEffectTarget::operator=;
	Target() : MEffectTarget(4) { NextPhase(); Set(777,888,999,123); SetServerID(321); SetDelayFrame(15); SetResultTime(); m_EffectID = 73; SetResult(new MActionResult); GetResult()->Add(new ResultNode); }
	~Target() override { ++destroyed; }
};
struct World
{
	const MFixedZoneEffectHost* old = MSkipEffectGenerator::SetHost(&host);
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* oldTarget = MEffectTarget::SetHost(&targetHost);
	MSkipEffectGenerator generator;
	World() { Reset(); sprite = {BLT_EFFECT,12,3}; metadata = true; now = 100; }
	void Reset() { effects.clear(); slots.clear(); removed.clear(); mask = 511; attempts = destroyed = resultsDestroyed = 0; }
	~World() { effects.clear(); MSkipEffectGenerator::SetHost(old); MEffect::SetHost(oldEffect); MEffectTarget::SetHost(oldTarget); }
};
EFFECTGENERATOR_INFO Info(int grade)
{
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = 17; info.temp1 = static_cast<BYTE>(grade);
	info.x0 = 487; info.y0 = 247; info.z0 = 77; info.direction = 6; info.step = 11;
	info.count = 30; info.linkCount = 5; info.power = 7; info.nActionInfo = 42; return info;
}
const int positions[4][9][2] = {
	{{487,247}},
	{{432,240},{480,240},{528,240}},
	{{432,240},{480,216},{528,240},{480,264},{480,240}},
	{{432,216},{432,240},{432,264},{480,216},{480,240},{480,264},{528,216},{528,240},{528,264}},
};
const int counts[] = {1,3,5,9};
void CheckCopy(const MEffectTarget& copy)
{
	CHECK_EQ(777, copy.GetX()); CHECK_EQ(888, copy.GetY()); CHECK_EQ(999, copy.GetZ()); CHECK_EQ(123, copy.GetID());
	CHECK_EQ(1, copy.GetCurrentPhase()); CHECK_EQ(4, copy.GetMaxPhase()); CHECK_EQ(15, copy.GetDelayFrame());
	CHECK_EQ(73, copy.GetEffectID()); CHECK_EQ(OBJECTID_NULL, copy.GetServerID()); CHECK(copy.IsResultEmpty()); CHECK(!copy.IsResultTime());
}
}

TEST(SkipEffectGenerator, EveryGradeAndAcceptanceMaskPreservesOrderAndTransfersOnlyFirstAccepted)
{
	World world; CHECK_EQ(EFFECTGENERATORID_SKIP_DRAW, world.generator.GetID());
	for (int grade = 0; grade < 4; ++grade) for (int accepted = 0; accepted < (1 << counts[grade]); ++accepted)
	{
		world.Reset(); mask = accepted; auto info = Info(grade); auto* original = new Target; info.pEffectTarget = original;
		{
			MEffectTargetOwner owner(original);
			CHECK_EQ(accepted != 0, world.generator.Generate(info)); CHECK_EQ(counts[grade], attempts);
			CHECK_EQ(0, destroyed); CHECK_EQ(0, resultsDestroyed); CHECK(removed.empty());
			CHECK_EQ(777, original->GetX()); CHECK_EQ(888, original->GetY()); CHECK_EQ(999, original->GetZ());
			CHECK_EQ(321, original->GetServerID()); CHECK(original->IsResultTime()); CHECK_EQ(1, original->GetResult()->GetSize());
			for (size_t i = 0; i < effects.size(); ++i)
			{
				auto& effect = *effects[i]; const int slot = slots[i];
				CHECK_EQ(positions[grade][slot][0], effect.GetPixelX()); CHECK_EQ(positions[grade][slot][1], effect.GetPixelY()); CHECK_EQ(77, effect.GetPixelZ());
				CHECK_EQ(11, effect.GetStepPixel()); CHECK_EQ(7, effect.GetPower()); CHECK_EQ(6, effect.GetDirection()); CHECK_EQ(42, effect.GetActionInfo());
				CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
				CHECK_EQ(i == 0, effect.GetEffectTarget() == original);
				if (i != 0) { CheckCopy(*effect.GetEffectTarget()); for (size_t j = 0; j < i; ++j) CHECK(effect.GetEffectTarget() != effects[j]->GetEffectTarget()); }
			}
			while (effects.size() > 1) effects.pop_back();
			CHECK(removed.empty()); CHECK_EQ(0, destroyed); CHECK_EQ(0, resultsDestroyed);
		}
		effects.clear(); CHECK_EQ(1, destroyed); CHECK_EQ(1, resultsDestroyed); CHECK(removed == std::vector<int>{73});
	}
}

TEST(SkipEffectGenerator, UnsupportedGradesAndMissingServicesLeaveTargetWithCaller)
{
	World world;
	for (int grade = 4; grade < 256; ++grade)
	{
		world.Reset(); auto info = Info(grade); Target target; info.pEffectTarget = &target;
		CHECK(!world.generator.Generate(info)); CHECK_EQ(0, attempts); CHECK_EQ(0, destroyed);
	}
	const MFixedZoneEffectHost empty{}, noQueue{.Sprite = host.Sprite};
	for (const auto* services : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &noQueue})
	{
		world.Reset(); MSkipEffectGenerator::SetHost(services); Target target; auto info = Info(3); info.pEffectTarget = &target;
		CHECK(!world.generator.Generate(info)); CHECK_EQ(0, attempts); CHECK_EQ(0, destroyed);
	}
	MSkipEffectGenerator::SetHost(&host); metadata = false; world.Reset();
	CHECK(!world.generator.Generate(Info(3))); CHECK_EQ(0, attempts);
}

TEST(SkipEffectGenerator, TargetlessPatternsRetainFirstAcceptedActionAndDoNotConsumeRandomness)
{
	World world;
	for (int grade = 0; grade < 4; ++grade)
	{
		world.Reset(); mask = 510; if (!grade) mask = 1;
		std::srand(51); const int next = std::rand(); std::srand(51);
		CHECK(world.generator.Generate(Info(grade))); CHECK_EQ(next, std::rand());
		for (size_t i = 0; i < effects.size(); ++i) { CHECK(effects[i]->GetEffectTarget() == nullptr); CHECK_EQ(i == 0 ? 42 : ACTIONINFO_NULL, effects[i]->GetActionInfo()); }
		auto* skip = dynamic_cast<MSkipEffect*>(effects.front().get()); CHECK(skip != nullptr);
		if (skip) { CHECK_EQ(3, skip->GetSkipValue()); CHECK(skip->Update()); CHECK_EQ(1, skip->GetFrame()); now = 125; CHECK(!skip->Update()); now = 100; }
	}
}

TEST(SkipEffectGenerator, QueueExceptionsCleanPendingEffectsAndPreserveTransferredOriginal)
{
	World world; static int throwAt;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { if (attempts == throwAt) throw std::runtime_error("queue"); return host.Queue(std::move(effect)); },
	};
	for (int slot : {0,1,4})
	{
		world.Reset(); throwAt = slot; MSkipEffectGenerator::SetHost(&throwing); bool threw = false;
		try { auto info = Info(3); info.pEffectTarget = new Target; MEffectTargetOwner owner(info.pEffectTarget); world.generator.Generate(info); }
		catch (const std::runtime_error&) { threw = true; }
		CHECK(threw); CHECK_EQ(slot, attempts); CHECK_EQ(slot == 0 ? 1 : 0, destroyed);
		effects.clear(); CHECK_EQ(1, destroyed); CHECK_EQ(1, resultsDestroyed); CHECK(removed == std::vector<int>{73});
	}
}

TEST(SkipEffectGenerator, CopiesRemainIndependentAfterQueueDestroysOriginal)
{
	World world;
	const MFixedZoneEffectHost deleting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { if (attempts == 1) effects.clear(); return host.Queue(std::move(effect)); },
	};
	MSkipEffectGenerator::SetHost(&deleting); auto info = Info(3); info.pEffectTarget = new Target;
	MEffectTargetOwner owner(info.pEffectTarget); CHECK(world.generator.Generate(info));
	CHECK_EQ(1, destroyed); CHECK_EQ(1, resultsDestroyed); CHECK_EQ(8, effects.size());
	for (const auto& effect : effects) CheckCopy(*effect->GetEffectTarget());
	effects.clear(); CHECK(removed == std::vector<int>{73});
}

TEST(SkipEffectGenerator, ServicesAreRereadAfterMetadataAndEachSubmission)
{
	World world;
	const MFixedZoneEffectHost fromSprite{.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& value) { MSkipEffectGenerator::SetHost(&host); return host.Sprite(type,value); }};
	MSkipEffectGenerator::SetHost(&fromSprite); CHECK(world.generator.Generate(Info(3))); CHECK_EQ(9, attempts);
	world.Reset();
	const MFixedZoneEffectHost removing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { const bool result = host.Queue(std::move(effect)); MSkipEffectGenerator::SetHost(nullptr); return result; },
	};
	MSkipEffectGenerator::SetHost(&removing); auto info = Info(3); info.pEffectTarget = new Target;
	{ MEffectTargetOwner owner(info.pEffectTarget); CHECK(world.generator.Generate(info)); CHECK_EQ(1, attempts); CHECK_EQ(0, destroyed); }
	effects.clear(); CHECK_EQ(1, destroyed);
}

TEST(SkipEffectGenerator, NegativeAndExtremeCoordinatesUseSafeTileOrigins)
{
	World world;
	for (const int coordinate : {-1, -49, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
	{
		world.Reset(); auto info = Info(3); info.x0 = info.y0 = coordinate;
		CHECK(world.generator.Generate(info)); CHECK_EQ(9, effects.size());
		const int tx = WorldTileGeometry::PixelToTileX(coordinate), ty = WorldTileGeometry::PixelToTileY(coordinate);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			if (coordinate == (std::numeric_limits<int>::min)() || coordinate == (std::numeric_limits<int>::max)())
			{
				// Pin the existing float rounding near the int endpoints.
				const bool low = coordinate < 0;
				const int expectedX = low && i >= 6 ? -2147483520 : !low && i < 3 ? 2147483520 : coordinate;
				CHECK_EQ(expectedX, effects[i]->GetPixelX()); CHECK_EQ(coordinate, effects[i]->GetPixelY());
			}
			else
			{
				CHECK_EQ(WorldTileGeometry::TileToPixelX(tx + static_cast<int>(i / 3) - 1), effects[i]->GetPixelX());
				CHECK_EQ(WorldTileGeometry::TileToPixelY(ty + static_cast<int>(i % 3) - 1), effects[i]->GetPixelY());
			}
		}
	}
}
