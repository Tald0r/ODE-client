#include "test_framework.h"
#include "MBloodyWaveEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
MBloodyWaveEffectSprite sprite;
int frames, acceptance, submissions;
bool metadataAvailable;
DWORD now;
std::vector<int> requests, frameRequests;
std::vector<std::unique_ptr<MEffect>> effects;
const MBloodyWaveEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyWaveEffectSprite& result) {
		requests.push_back(type); result = sprite; return metadataAvailable;
	},
	.MaxFrames = [](BYTE, TYPE_FRAMEID id, int& count) {
		frameRequests.push_back(id); count = frames; return true;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		CHECK(effect->GetEffectTarget() == nullptr);
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!(acceptance & (1 << submissions++))) return false;
		effects.push_back(std::move(effect)); return true;
	},
};
const MEffectHost effectHost{.CurrentFrame = [] { return now; }};
struct World
{
	const MBloodyWaveEffectHost* old = MBloodyWaveEffectGenerator::SetHost(&host);
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	MBloodyWaveEffectGenerator generator;
	World() { Reset(); sprite = {BLT_EFFECT, 12, false}; frames = 3; metadataAvailable = true; now = 100; }
	void Reset() { effects.clear(); requests.clear(); frameRequests.clear(); acceptance = 255; submissions = 0; }
	~World() { effects.clear(); MBloodyWaveEffectGenerator::SetHost(old); MEffect::SetHost(oldEffect); }
};
EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = 17; info.nActionInfo = 42;
	info.x0 = 999; info.y0 = 999; info.z0 = 999;
	info.x1 = 240; info.y1 = 120; info.z1 = 17;
	info.direction = DIRECTION_LEFTUP; info.step = 11; info.power = 7; info.count = 30; info.linkCount = 5;
	return info;
}
std::unique_ptr<MEffectTarget> Target(int phase)
{
	auto target = std::make_unique<MEffectTarget>(255);
	for (int i = 0; i < phase; ++i) target->NextPhase();
	target->Set(777, 888, 999, 123); return target;
}
bool Generate(World& world, EFFECTGENERATOR_INFO info, std::unique_ptr<MEffectTarget>& target)
{
	info.pEffectTarget = target.get(); const bool result = world.generator.Generate(info);
	for (const auto& effect : effects) if (effect->GetEffectTarget() == target.get()) { target.release(); break; }
	return result;
}
struct Point { int x, y; bool operator==(const Point&) const = default; };
std::vector<Point> Positions()
{
	std::vector<Point> result;
	for (const auto& effect : effects) result.push_back({effect->GetX(), effect->GetY()});
	return result;
}
const std::vector<Point> patterns[] = {
	{{5,2},{5,8},{8,5},{2,5}},
	{{5,4},{5,6},{6,5},{4,5}},
	{{6,4},{6,6},{4,4},{4,6}},
	{{5,3},{5,7},{7,5},{3,5}},
	{{6,3},{7,4},{6,7},{7,6},{4,3},{3,4},{4,7},{3,6}},
};
}

TEST(BloodyWaveEffectGenerator, EveryPhaseUsesTheOriginalPatternAndDestination)
{
	World world; CHECK_EQ(EFFECTGENERATORID_BLOODY_WAVE, world.generator.GetID());
	for (int phase = 0; phase < 256; ++phase)
	{
		world.Reset(); auto target = Target(phase); auto* original = target.get();
		CHECK(Generate(world, Info(), target)); CHECK(Positions() == patterns[phase < 5 ? phase : 0]);
		CHECK_EQ(phase, original->GetCurrentPhase()); CHECK_EQ(777, original->GetX());
		CHECK_EQ(effects.size() + 1, requests.size()); CHECK_EQ(requests.size(), frameRequests.size());
		for (size_t i = 0; i < effects.size(); ++i)
		{
			auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
			CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
			CHECK_EQ(17, effect.GetPixelZ()); CHECK_EQ(11, effect.GetStepPixel()); CHECK_EQ(7, effect.GetPower());
			CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection()); CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
			CHECK_EQ(42, effect.GetActionInfo()); CHECK_EQ(i == 0, effect.GetEffectTarget() == original);
			if (i) CHECK(effect.GetEffectTarget() == nullptr);
		}
	}
}

TEST(BloodyWaveEffectGenerator, EveryAcceptanceMaskTransfersOnlyToTheFirstAcceptedEffect)
{
	World world;
	for (int mask = 0; mask < 256; ++mask)
	{
		world.Reset(); acceptance = mask; auto target = Target(4); auto* original = target.get();
		CHECK_EQ(mask != 0, Generate(world, Info(), target)); CHECK_EQ(8, submissions); CHECK_EQ(mask == 0, target != nullptr);
		CHECK_EQ(777, original->GetX());
		for (size_t i = 0; i < effects.size(); ++i) CHECK_EQ(i == 0, effects[i]->GetEffectTarget() == original);
	}
}

TEST(BloodyWaveEffectGenerator, TargetlessCallsContinueThePhaseAcrossGeneratorInstances)
{
	World world; auto target = Target(0); CHECK(Generate(world, Info(), target));
	for (int phase = 1; phase <= 8; ++phase)
	{
		world.Reset(); MBloodyWaveEffectGenerator other; CHECK(other.Generate(Info()));
		CHECK(Positions() == patterns[phase < 5 ? phase : 0]);
		for (const auto& effect : effects) CHECK(effect->GetEffectTarget() == nullptr);
	}
}

TEST(BloodyWaveEffectGenerator, FailedSubmissionStillAdvancesTheTargetlessPhase)
{
	World world; auto target = Target(2); acceptance = 0; CHECK(!Generate(world, Info(), target));
	world.Reset(); CHECK(world.generator.Generate(Info())); CHECK(Positions() == patterns[3]);
}

TEST(BloodyWaveEffectGenerator, VariantsCycleAfterRejectedAttemptsAndAfterTheFinalAttempt)
{
	World world; acceptance = 10; auto target = Target(1); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_BLOODY_WALL_2;
	std::srand(91); const int initial = std::rand() % 3, next = std::rand(); std::srand(91);
	CHECK(Generate(world, info, target)); CHECK_EQ(next, std::rand()); CHECK_EQ(5, requests.size());
	for (size_t i = 0; i < requests.size(); ++i) CHECK_EQ(EFFECTSPRITETYPE_BLOODY_WALL_1 + (initial + static_cast<int>(i)) % 3, requests[i]);
}

TEST(BloodyWaveEffectGenerator, RepeatingAnimationConsumesRandomnessOnlyForAcceptedPositiveLengths)
{
	World world; sprite.repeatFrame = true;
	for (int length : {-3, 0, 1, 3, 256, 258})
	{
		world.Reset(); frames = length; acceptance = 10; auto target = Target(1); std::srand(13);
		const int cycle = static_cast<BYTE>(length) == 0 ? 256 : static_cast<BYTE>(length);
		const int first = length > 0 ? (std::rand() % length) % cycle : 0;
		const int second = length > 0 ? (std::rand() % length) % cycle : 0;
		const int next = std::rand(); std::srand(13); CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand());
		CHECK_EQ(2, effects.size()); CHECK_EQ(first, effects[0]->GetFrame()); CHECK_EQ(second, effects[1]->GetFrame());
		CHECK_EQ(static_cast<BYTE>(length), effects[0]->GetMaxFrame());
	}
}

TEST(BloodyWaveEffectGenerator, MissingServicesRetainCallerOwnership)
{
	World world; const MBloodyWaveEffectHost empty{}, noFrames{.Sprite = host.Sprite}, noQueue{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames};
	for (const auto* services : {static_cast<const MBloodyWaveEffectHost*>(nullptr), &empty, &noFrames, &noQueue})
	{
		world.Reset(); MBloodyWaveEffectGenerator::SetHost(services); auto target = Target(1);
		CHECK(!Generate(world, Info(), target)); CHECK(target != nullptr); CHECK_EQ(0, submissions); CHECK(effects.empty());
	}
}

TEST(BloodyWaveEffectGenerator, HostRemovalAfterAcceptancePreservesTheAcceptedTarget)
{
	World world; const MBloodyWaveEffectHost removing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { const bool result = host.Queue(std::move(effect)); MBloodyWaveEffectGenerator::SetHost(nullptr); return result; },
	};
	MBloodyWaveEffectGenerator::SetHost(&removing); auto target = Target(4); auto* original = target.get();
	CHECK(Generate(world, Info(), target)); CHECK_EQ(1, submissions); CHECK(effects[0]->GetEffectTarget() == original);
}

TEST(BloodyWaveEffectGenerator, MetadataRefreshRetainsInitialBlitAndRepeatPolicy)
{
	World world; const MBloodyWaveEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_SHADOW, 21, true}; frames = 258; return host.Queue(std::move(effect)); },
	};
	MBloodyWaveEffectGenerator::SetHost(&changing); auto target = Target(1);
	std::srand(15); const int next = std::rand(); std::srand(15); CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(BLT_EFFECT, effects[i]->GetBltType()); CHECK_EQ(i == 0 ? 12 : 21, effects[i]->GetFrameID()); CHECK_EQ(i == 0 ? 3 : 2, effects[i]->GetMaxFrame());
	}
}

TEST(BloodyWaveEffectGenerator, SectorEdgesRetainUnsignedWrappingAndRealAnimation)
{
	World world; auto info = Info(); info.x1 = info.y1 = 0; auto target = Target(1); CHECK(Generate(world, info, target));
	CHECK(Positions() == std::vector<Point>({{0,65535},{0,1},{1,0},{65535,0}}));
	CHECK(effects[0]->Update()); CHECK_EQ(1, effects[0]->GetFrame()); CHECK_EQ(0, effects[0]->GetPixelX());
	now = 129; CHECK(!effects[0]->Update());
}

namespace {
int destroyedWaveMarkers;
struct WaveMarker : MEffectTarget
{
	using MEffectTarget::operator=;
	WaveMarker() : MEffectTarget(1) {}
	~WaveMarker() override { ++destroyedWaveMarkers; }
};
}

TEST(BloodyWaveEffectGenerator, QueueThrowBeforeAcceptanceDestroysTheSubmittedEffectAndKeepsTheOriginal)
{
	World world; destroyedWaveMarkers = 0;
	const MBloodyWaveEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			effect->SetLink(42, new WaveMarker); throw std::runtime_error("queue");
		},
	};
	MBloodyWaveEffectGenerator::SetHost(&throwing); auto target = Target(4); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, destroyedWaveMarkers); CHECK(effects.empty());
	CHECK_EQ(777, target->GetX()); CHECK_EQ(4, target->GetCurrentPhase()); CHECK_EQ(1, frameRequests.size());
}

TEST(BloodyWaveEffectGenerator, QueueThrowAfterAcceptancePreservesTheFirstEffectsOriginalTarget)
{
	World world; destroyedWaveMarkers = 0;
	const MBloodyWaveEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 1) { effect->SetLink(42, new WaveMarker); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect));
		},
	};
	MBloodyWaveEffectGenerator::SetHost(&throwing); auto target = Target(4); auto* original = target.get();
	auto info = Info(); info.pEffectTarget = original; bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	// Follow actual ownership even if an assertion below fails.
	for (const auto& effect : effects) if (effect->GetEffectTarget() == target.get()) { target.release(); break; }
	CHECK(threw); CHECK_EQ(1, destroyedWaveMarkers); CHECK_EQ(1, effects.size()); CHECK_EQ(2, frameRequests.size());
	if (!effects.empty()) CHECK(effects.front()->GetEffectTarget() == original);
	CHECK_EQ(777, original->GetX()); CHECK_EQ(4, original->GetCurrentPhase());
}
