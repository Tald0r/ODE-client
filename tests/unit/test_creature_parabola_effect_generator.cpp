#include "test_framework.h"
#include "MAttackCreatureParabolaEffectGenerator.h"
#include "MAttackZoneParabolaEffectGenerator.h"
#include "MParabolaEffect.h"
#include "MathTable.h"
#include "SkillDef.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct Tables
{
	std::array<int, MathTable::MAX_ANGLE> sine, cosine;
	std::array<int, MathTable::MAX_ANGLE + 1> tangent;
	Tables()
	{
		std::copy_n(MathTable::FSinTab, sine.size(), sine.begin());
		std::copy_n(MathTable::FCosTab, cosine.size(), cosine.begin());
		std::copy_n(MathTable::FArcTanTab, tangent.size(), tangent.begin());
		if (MathTable::FCosTab[0] != 65536) MathTable::FCreateSines();
	}
	~Tables()
	{
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
	}
};

DWORD frameNow;
MCreatureParabolaEffectSprite sprite;
MCreatureParabolaPosition creature;
bool spriteAvailable, creatureAvailable, framesAvailable, acceptEffect;
int maxFrames, requestedSprite, submissions;
TYPE_OBJECTID requestedCreature;
std::vector<int> calls, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects, smoke;
std::vector<DWORD> waits;
std::vector<std::array<int, 2>> impacts;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(5); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(4); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MParabolaEffectHost outputHost{
	.SmokeSprite = [](MParabolaSmokeSprite& result) { calls.push_back(7); result = {BLT_EFFECT, 90, 5}; return true; },
	.QueueSmoke = [](std::unique_ptr<MEffect> effect, DWORD wait) { calls.push_back(8); smoke.push_back(std::move(effect)); waits.push_back(wait); },
	.CannonadeImpact = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back(9); impacts.push_back({x, y}); },
};
const MCreatureParabolaEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MCreatureParabolaEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Creature = [](TYPE_OBJECTID id, MCreatureParabolaPosition& position) {
		calls.push_back(2); requestedCreature = id; position = creature; return creatureAvailable;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) {
		calls.push_back(3); CHECK_EQ(sprite.bltType, blt); CHECK_EQ(sprite.frameID, frameID); count = maxFrames; return framesAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(6); ++submissions; CHECK_EQ(MEffect::EFFECT_PARABOLA, effect->GetEffectType());
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!acceptEffect) return false;
		effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	Tables tables;
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MParabolaEffectHost* previousOutput = MParabolaEffect::SetHost(&outputHost);
	const MCreatureParabolaEffectHost* previousGenerator = MAttackCreatureParabolaEffectGenerator::SetHost(&host);
	MAttackCreatureParabolaEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12}; creature = {4, 0, 0}; maxFrames = 3;
		spriteAvailable = creatureAvailable = framesAvailable = acceptEffect = true; requestedSprite = -1; requestedCreature = 0; submissions = 0;
		effects.clear(); smoke.clear(); calls.clear(); removedTargets.clear(); waits.clear(); impacts.clear();
	}
	~World()
	{
		effects.clear(); smoke.clear(); MAttackCreatureParabolaEffectGenerator::SetHost(previousGenerator);
		MParabolaEffect::SetHost(previousOutput); MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = info.y0 = info.z0 = 0; info.x1 = 900; info.y1 = 800; info.z1 = 700;
	info.direction = DIRECTION_LEFTUP; info.step = 48; info.count = 30; info.linkCount = 5; info.power = 77; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3); target->NextPhase(); target->m_EffectID = id;
	target->Set(777, 888, 999, 456); target->SetServerID(789); target->SetDelayFrame(31);
	target->SetResultTime(); target->SetResult(new MActionResult); return target;
}

void CheckTarget(const MEffectTarget& target)
{
	CHECK_EQ(777, target.GetX()); CHECK_EQ(888, target.GetY()); CHECK_EQ(999, target.GetZ()); CHECK_EQ(456, target.GetID());
	CHECK_EQ(789, target.GetServerID()); CHECK_EQ(1, target.GetCurrentPhase()); CHECK_EQ(3, target.GetMaxPhase()); CHECK_EQ(31, target.GetDelayFrame());
	CHECK(target.IsExistResult()); CHECK(target.IsResultTime());
}

void ClearEffects()
{
	effects.clear(); smoke.clear(); calls.clear(); waits.clear(); impacts.clear(); submissions = 0;
}

bool Arrive(MEffect& effect)
{
	for (int i = 0; i < 64; ++i) if (!effect.Update()) return true;
	return false;
}

} // namespace

TEST(CreatureParabolaEffectGenerator, ResolvesMetadataAndCreatureBeforeConfiguringARealProjectile)
{
	World world; CHECK_EQ(EFFECTGENERATORID_ATTACK_CREATURE_PARABOLA, world.generator.GetID());
	std::srand(67); const int next = std::rand(); std::srand(67); CHECK(world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
	CHECK_EQ(17, requestedSprite); CHECK_EQ(123, requestedCreature); CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions);
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5, 6})); auto& effect = *effects.front();
	CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
	CHECK_EQ(0, effect.GetPixelX()); CHECK_EQ(0, effect.GetPixelY()); CHECK_EQ(24, effect.GetPixelZ()); CHECK_EQ(0, effect.GetX()); CHECK_EQ(0, effect.GetY());
	CHECK_EQ(DIRECTION_RIGHT, effect.GetDirection()); CHECK_EQ(48, effect.GetStepPixel()); CHECK_EQ(77, effect.GetPower()); CHECK_EQ(42, effect.GetActionInfo());
	CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame()); CHECK(effect.GetEffectTarget() == nullptr);
}

TEST(CreatureParabolaEffectGenerator, RealMotionFliesTheArcToTheCreatureTileAndHeight)
{
	World world; CHECK(world.generator.Generate(Info())); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(48, effect.GetPixelX()); CHECK_EQ(0, effect.GetPixelY()); CHECK_EQ(57, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight());
	CHECK(effect.Update()); CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(57, effect.GetPixelZ()); CHECK_EQ(2, effect.GetFrame());
	calls.clear(); CHECK(!effect.Update()); CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ(0, effect.GetPixelY()); CHECK_EQ(24, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(2, effect.GetFrame()); CHECK_EQ(9, effect.GetLight());
	CHECK(calls == std::vector<int>{5}); CHECK(smoke.empty()); CHECK(impacts.empty());
}

TEST(CreatureParabolaEffectGenerator, DestinationInputsAreIgnoredAndTheCreatureIsSampledOnce)
{
	World world; auto info = Info(); info.x1 = info.y1 = info.z1 = (std::numeric_limits<int>::min)();
	CHECK(world.generator.Generate(info)); creature = {10, 20, 100}; CHECK(Arrive(*effects.front()));
	CHECK_EQ(192, effects.front()->GetPixelX()); CHECK_EQ(0, effects.front()->GetPixelY()); CHECK_EQ(24, effects.front()->GetPixelZ());
	CHECK_EQ(1, std::count(calls.begin(), calls.end(), 2));
}

TEST(CreatureParabolaEffectGenerator, EveryInputDirectionIsOverriddenByTheTrajectoryFacing)
{
	World world;
	for (int direction = 0; direction < 256; ++direction)
	{
		ClearEffects(); auto info = Info(); info.direction = static_cast<BYTE>(direction); CHECK(world.generator.Generate(info));
		CHECK_EQ(DIRECTION_RIGHT, effects.front()->GetDirection()); CHECK(Arrive(*effects.front())); CHECK_EQ(192, effects.front()->GetPixelX());
	}
}

TEST(CreatureParabolaEffectGenerator, AllCreatureDirectionsRetainTheExactTileDestination)
{
	World world; struct Destination { TYPE_SECTORPOSITION x, y; BYTE direction; };
	for (const Destination point : {Destination{9, 10, DIRECTION_LEFT}, {9, 11, DIRECTION_LEFTDOWN}, {10, 11, DIRECTION_DOWN},
		{11, 11, DIRECTION_RIGHTDOWN}, {11, 10, DIRECTION_RIGHT}, {11, 9, DIRECTION_RIGHTUP}, {10, 9, DIRECTION_UP}, {9, 9, DIRECTION_LEFTUP}})
	{
		ClearEffects(); auto info = Info(); info.x0 = 480; info.y0 = 240; info.step = 255; creature = {point.x, point.y, 0};
		CHECK(world.generator.Generate(info)); CHECK_EQ(point.direction, effects.front()->GetDirection()); CHECK(Arrive(*effects.front()));
		CHECK_EQ(point.x * 48, effects.front()->GetPixelX()); CHECK_EQ(point.y * 24, effects.front()->GetPixelY()); CHECK_EQ(24, effects.front()->GetPixelZ());
	}
}

TEST(CreatureParabolaEffectGenerator, FullSectorAndCreatureHeightRangesKeepTheirPixelMeaning)
{
	World world;
	for (const TYPE_SECTORPOSITION tile : {static_cast<TYPE_SECTORPOSITION>(0), static_cast<TYPE_SECTORPOSITION>(65535)})
	{
		for (const short height : {(std::numeric_limits<short>::min)(), (std::numeric_limits<short>::max)()})
		{
			ClearEffects(); creature = {tile, tile, height}; auto info = Info(); info.x0 = tile * 48 - 192; info.y0 = tile * 24; info.z0 = height;
			CHECK(world.generator.Generate(info)); CHECK(Arrive(*effects.front())); CHECK_EQ(tile * 48, effects.front()->GetPixelX());
			CHECK_EQ(tile * 24, effects.front()->GetPixelY()); CHECK_EQ(height + 24, effects.front()->GetPixelZ());
		}
	}
}

TEST(CreatureParabolaEffectGenerator, ZeroSpeedAnimatesAtTheElevatedSourceUntilExpiry)
{
	World world; auto info = Info(); info.step = 0; CHECK(world.generator.Generate(info)); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(0, effect.GetPixelX()); CHECK_EQ(24, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame());
	frameNow = 129; CHECK(!effect.Update()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(0, effect.GetPixelX());
}

TEST(CreatureParabolaEffectGenerator, CannonadeRetainsSmokeAndTheDefaultImpactTile)
{
	World world; auto info = Info(); info.nActionInfo = SKILL_CANNONADE; CHECK(world.generator.Generate(info)); CHECK(Arrive(*effects.front()));
	CHECK_EQ(3, smoke.size()); CHECK(waits == std::vector<DWORD>(3, 10)); CHECK(impacts == (std::vector<std::array<int, 2>>{{0, 0}}));
	CHECK_EQ(192, effects.front()->GetPixelX()); CHECK_EQ(24, effects.front()->GetPixelZ());
}

TEST(CreatureParabolaEffectGenerator, AcceptanceTransfersTheOriginalWithoutRetargetingOrLosingMetadata)
{
	World world; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); auto* result = target->GetResult();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CheckTarget(*info.pEffectTarget);
	CHECK(info.pEffectTarget->GetResult() == result); CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(CreatureParabolaEffectGenerator, RejectionLeavesTheOriginalWithTheCaller)
{
	World world; acceptEffect = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK(effects.empty()); CheckTarget(*target); CHECK(removedTargets.empty());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5, 6}));
}

TEST(CreatureParabolaEffectGenerator, MissingSpriteMetadataSkipsCreatureAndAnimationLookup)
{
	World world; const MCreatureParabolaEffectHost empty{};
	for (const auto* service : {static_cast<const MCreatureParabolaEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); MAttackCreatureParabolaEffectGenerator::SetHost(service); spriteAvailable = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK_EQ(0, submissions); CheckTarget(*target);
		CHECK(calls == (service == &host ? std::vector<int>{1} : std::vector<int>{}));
	}
}

TEST(CreatureParabolaEffectGenerator, MissingCreatureStopsBeforeAnimationLookupAndEffectConfiguration)
{
	World world; const MCreatureParabolaEffectHost noCreature{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Queue = host.Queue};
	for (const auto* service : {&noCreature, &host})
	{
		ClearEffects(); MAttackCreatureParabolaEffectGenerator::SetHost(service); creatureAvailable = false;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info));
		CHECK(calls == (service == &host ? std::vector<int>{1, 2} : std::vector<int>{1})); CHECK(effects.empty()); CheckTarget(*target);
	}
}

TEST(CreatureParabolaEffectGenerator, MissingAnimationLengthDiscardsTheUnlinkedProjectile)
{
	World world; const MCreatureParabolaEffectHost noFrames{.Sprite = host.Sprite, .Creature = host.Creature, .Queue = host.Queue};
	for (const auto* service : {&noFrames, &host})
	{
		ClearEffects(); MAttackCreatureParabolaEffectGenerator::SetHost(service); framesAvailable = false;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info));
		CHECK(calls == (service == &host ? std::vector<int>{1, 2, 3} : std::vector<int>{1, 2})); CHECK(effects.empty()); CheckTarget(*target);
	}
}

TEST(CreatureParabolaEffectGenerator, MissingQueueDiscardsTheConfiguredProjectile)
{
	World world; const MCreatureParabolaEffectHost noQueue{.Sprite = host.Sprite, .Creature = host.Creature, .MaxFrames = host.MaxFrames};
	MAttackCreatureParabolaEffectGenerator::SetHost(&noQueue); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(calls == std::vector<int>({1, 2, 3, 4, 5})); CHECK(effects.empty()); CHECK(removedTargets.empty()); CheckTarget(*target);
}

TEST(CreatureParabolaEffectGenerator, SpriteCallbackCanReplaceTheRemainingServices)
{
	World world; const MCreatureParabolaEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MCreatureParabolaEffectSprite& result) { MAttackCreatureParabolaEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, effects.size());
}

TEST(CreatureParabolaEffectGenerator, CreatureCallbackCanReplaceAnimationAndQueueServices)
{
	World world; const MCreatureParabolaEffectHost replacing{
		.Sprite = host.Sprite,
		.Creature = [](TYPE_OBJECTID id, MCreatureParabolaPosition& position) { MAttackCreatureParabolaEffectGenerator::SetHost(&host); return host.Creature(id, position); },
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, effects.size());
}

TEST(CreatureParabolaEffectGenerator, AnimationCallbackUsesTheAlreadySampledCreaturePosition)
{
	World world; const MCreatureParabolaEffectHost moving{
		.Sprite = host.Sprite, .Creature = host.Creature,
		.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) { creature = {10, 20, 100}; return host.MaxFrames(blt, frameID, count); },
		.Queue = host.Queue,
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&moving); CHECK(world.generator.Generate(Info())); CHECK(Arrive(*effects.front()));
	CHECK_EQ(192, effects.front()->GetPixelX()); CHECK_EQ(0, effects.front()->GetPixelY()); CHECK_EQ(24, effects.front()->GetPixelZ());
}

TEST(CreatureParabolaEffectGenerator, AnimationCallbackCanRemoveSubmission)
{
	World world; const MCreatureParabolaEffectHost removing{
		.Sprite = host.Sprite, .Creature = host.Creature,
		.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) { MAttackCreatureParabolaEffectGenerator::SetHost(nullptr); return host.MaxFrames(blt, frameID, count); },
		.Queue = host.Queue,
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&removing); CHECK(!world.generator.Generate(Info())); CHECK(effects.empty());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(CreatureParabolaEffectGenerator, AcceptedQueueRemovalStillLinksTheOriginalTarget)
{
	World world; const MCreatureParabolaEffectHost removing{
		.Sprite = host.Sprite, .Creature = host.Creature, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { MAttackCreatureParabolaEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&removing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CheckTarget(*info.pEffectTarget);
	CHECK(!world.generator.Generate(Info())); CHECK_EQ(1, effects.size());
}

TEST(CreatureParabolaEffectGenerator, EachGenerationReadsFreshSpriteCreatureAndFrameMetadata)
{
	World world; CHECK(world.generator.Generate(Info())); ClearEffects(); sprite = {BLT_SHADOW, 21}; creature = {8, 4, 12}; maxFrames = 258;
	CHECK(world.generator.Generate(Info())); CHECK_EQ(BLT_SHADOW, effects.front()->GetBltType()); CHECK_EQ(21, effects.front()->GetFrameID()); CHECK_EQ(2, effects.front()->GetMaxFrame());
	CHECK(Arrive(*effects.front())); CHECK_EQ(384, effects.front()->GetPixelX()); CHECK_EQ(96, effects.front()->GetPixelY()); CHECK_EQ(36, effects.front()->GetPixelZ());
}

TEST(CreatureParabolaEffectGenerator, InstallerIsIndependentOfZoneParabolaGeneration)
{
	World world; const MZoneParabolaEffectHost sentinel{}; const auto* previous = MAttackZoneParabolaEffectGenerator::SetHost(&sentinel);
	CHECK(MAttackCreatureParabolaEffectGenerator::SetHost(nullptr) == &host); CHECK(MAttackZoneParabolaEffectGenerator::SetHost(previous) == &sentinel);
	CHECK(!world.generator.Generate(Info())); CHECK(calls.empty());
}

TEST(CreatureParabolaEffectGenerator, RejectingQueueDestroysItsMarkerWithoutTakingTheOriginal)
{
	World world; const MCreatureParabolaEffectHost rejecting{
		.Sprite = host.Sprite, .Creature = host.Creature, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(42, Target(94).release()); return false; },
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&rejecting); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>{94}); CheckTarget(*target);
}

TEST(CreatureParabolaEffectGenerator, QueueExceptionConsumesItsEffectWithoutTakingTheOriginal)
{
	World world; const MCreatureParabolaEffectHost throwing{
		.Sprite = host.Sprite, .Creature = host.Creature, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); },
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>{94}); CheckTarget(*target);
}

TEST(CreatureParabolaEffectGenerator, AnimationLookupExceptionLeavesTheCallerTargetUntouched)
{
	World world; const MCreatureParabolaEffectHost throwing{
		.Sprite = host.Sprite, .Creature = host.Creature,
		.MaxFrames = [](BYTE, TYPE_FRAMEID, int&) -> bool { throw std::runtime_error("frames"); }, .Queue = host.Queue,
	};
	MAttackCreatureParabolaEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(calls == std::vector<int>({1, 2})); CHECK(effects.empty()); CHECK(removedTargets.empty()); CheckTarget(*target);
}

TEST(CreatureParabolaEffectGenerator, AnimationCountsNarrowWithoutChangingDurationOrPower)
{
	World world;
	for (const int frames : {-1, 0, 256, 258}) for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); maxFrames = frames; auto info = Info(); info.power = power; CHECK(world.generator.Generate(info));
		CHECK_EQ(static_cast<BYTE>(frames), effects.front()->GetMaxFrame()); CHECK_EQ(power, effects.front()->GetPower()); CHECK_EQ(129, effects.front()->GetEndFrame());
	}
}

TEST(CreatureParabolaEffectGenerator, CountAndLinkSentinelsRemainFiniteAndIndependent)
{
	World world; struct Timing { WORD count, link; DWORD end, endLink; };
	for (const Timing time : {Timing{0, 5, 99, 104}, Timing{65535, MAX_LINKCOUNT, 65634, 65634}, Timing{1, 65534, 100, 65633}})
	{
		ClearEffects(); auto info = Info(); info.count = time.count; info.linkCount = time.link; CHECK(world.generator.Generate(info));
		CHECK_EQ(time.end, effects.front()->GetEndFrame()); CHECK_EQ(time.endLink, effects.front()->GetEndLinkFrame());
	}
}

TEST(CreatureParabolaEffectGenerator, ClockWrapAndMissingBaseServicesKeepExistingFallbacks)
{
	World world; frameNow = 0xFFFFFFFEu; CHECK(world.generator.Generate(Info())); CHECK_EQ(27, effects.front()->GetEndFrame());
	CHECK_EQ(2, effects.front()->GetEndLinkFrame()); CHECK(effects.front()->IsEnd());
	ClearEffects(); MEffect::SetHost(nullptr); CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update()); CHECK_EQ(24, effects.front()->GetPixelZ());
}
