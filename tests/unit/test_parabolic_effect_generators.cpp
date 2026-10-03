#include "test_framework.h"
#include "MAttackZoneParabolaEffectGenerator.h"
#include "MAttackZoneBombEffectGenerator.h"
#include "MParabolaEffect.h"
#include "MathTable.h"
#include "SkillDef.h"

#include <algorithm>
#include <array>
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
MZoneParabolaEffectSprite sprite;
bool spriteAvailable, acceptQueue;
int requestedSprite;
std::vector<int> calls, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct LightRequest
{
	BYTE blt; TYPE_FRAMEID id; BYTE direction, frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
struct Smoke { std::unique_ptr<MEffect> effect; DWORD wait; };
std::vector<Smoke> smoke;
struct Impact
{
	TYPE_SECTORPOSITION x, y;
	bool operator==(const Impact&) const = default;
};
std::vector<Impact> impacts;

const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(2); lights.push_back({blt, id, direction, frame}); return 7;
	},
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MParabolaEffectHost outputHost{
	.SmokeSprite = [](MParabolaSmokeSprite& result) {
		calls.push_back(5); result = {BLT_EFFECT, 90, 5}; return true;
	},
	.QueueSmoke = [](std::unique_ptr<MEffect> effect, DWORD wait) {
		calls.push_back(6); smoke.push_back({std::move(effect), wait});
	},
	.CannonadeImpact = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back(7); impacts.push_back({x, y});
	},
};

template<class Sprite>
bool ReadSprite(TYPE_EFFECTSPRITETYPE type, Sprite& result)
{
	calls.push_back(1); requestedSprite = type;
	result = {sprite.bltType, sprite.frameID, sprite.maxFrames}; return spriteAvailable;
}

bool Queue(std::unique_ptr<MEffect> effect)
{
	calls.push_back(4); CHECK(effect->GetEffectTarget() == nullptr);
	CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
	if (!acceptQueue) return false;
	effects.push_back(std::move(effect)); return true;
}

const MZoneParabolaEffectHost parabolaHost{.Sprite = ReadSprite<MZoneParabolaEffectSprite>, .Queue = Queue};
const MZoneBombEffectHost bombHost{.Sprite = ReadSprite<MZoneBombEffectSprite>, .Queue = Queue};

struct World
{
	Tables tables;
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MParabolaEffectHost* previousOutput = MParabolaEffect::SetHost(&outputHost);
	const MZoneParabolaEffectHost* previousParabola = MAttackZoneParabolaEffectGenerator::SetHost(&parabolaHost);
	const MZoneBombEffectHost* previousBomb = MAttackZoneBombEffectGenerator::SetHost(&bombHost);
	MAttackZoneParabolaEffectGenerator parabola;
	MAttackZoneBombEffectGenerator bomb;
	std::array<MEffectGenerator*, 2> Generators() { return {&parabola, &bomb}; }
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptQueue = true;
		requestedSprite = -1; effects.clear(); smoke.clear(); impacts.clear();
		calls.clear(); lights.clear(); removedTargets.clear();
	}
	~World()
	{
		effects.clear(); smoke.clear();
		MAttackZoneBombEffectGenerator::SetHost(previousBomb);
		MAttackZoneParabolaEffectGenerator::SetHost(previousParabola);
		MParabolaEffect::SetHost(previousOutput); MEffectTarget::SetHost(previousTarget);
		MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = info.y0 = info.z0 = 0; info.x1 = 144; info.y1 = 0; info.z1 = 24;
	info.direction = DIRECTION_RIGHT; info.step = 48;
	info.count = 30; info.linkCount = 5; info.power = 77;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3);
	target->NextPhase(); target->m_EffectID = id; target->Set(777, 888, 999, 456);
	return target;
}

bool Arrive(MEffect& effect, int limit = 64)
{
	for (int i = 0; i < limit; ++i) if (!effect.Update()) return true;
	return false;
}

} // namespace

TEST(ParabolicEffectGenerators, ConfigureRealProjectilesBeforeConsumingSubmission)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_ATTACK_ZONE_PARABOLA, world.parabola.GetID());
	CHECK_EQ(EFFECTGENERATORID_ATTACK_ZONE_BOMB, world.bomb.GetID());
	for (auto* generator : world.Generators())
	{
		calls.clear(); lights.clear();
		CHECK(generator->Generate(Info())); auto& effect = *effects.back();
		CHECK_EQ(MEffect::EFFECT_PARABOLA, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID());
		CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
		CHECK_EQ(0, effect.GetPixelX()); CHECK_EQ(0, effect.GetPixelY()); CHECK_EQ(48, effect.GetPixelZ());
		CHECK_EQ(DIRECTION_RIGHT, effect.GetDirection()); CHECK_EQ(48, effect.GetStepPixel());
		CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
		CHECK_EQ(77, effect.GetPower()); CHECK_EQ(42, effect.GetActionInfo()); CHECK_EQ(17, requestedSprite);
		CHECK(!effect.IsMulti()); CHECK(calls == std::vector<int>({1, 2, 3, 4}));
		CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_LEFT, 0}}));
	}
}

TEST(ParabolicEffectGenerators, ParabolaExtendsOneTileInEverySuppliedDirection)
{
	World world;
	struct Case { BYTE direction; int x, y; };
	for (const Case c : {Case{DIRECTION_LEFTDOWN, -48, 24}, {DIRECTION_RIGHTUP, 48, -24},
		{DIRECTION_LEFTUP, -48, -24}, {DIRECTION_RIGHTDOWN, 48, 24}, {DIRECTION_LEFT, -48, 0},
		{DIRECTION_DOWN, 0, 24}, {DIRECTION_UP, 0, -24}, {DIRECTION_RIGHT, 48, 0}, {255, 0, 0}})
	{
		auto info = Info(); info.x0 = info.x1 = 480; info.y0 = info.y1 = 240;
		info.direction = c.direction; info.step = 255;
		CHECK(world.parabola.Generate(info)); CHECK(!effects.back()->Update());
		CHECK_EQ(480 + c.x, effects.back()->GetPixelX()); CHECK_EQ(240 + c.y, effects.back()->GetPixelY());
		CHECK_EQ(48, effects.back()->GetPixelZ());
	}
}

TEST(ParabolicEffectGenerators, BombsKeepTheRequestedDestinationInEveryDirection)
{
	World world;
	for (const int direction : {0, 1, 2, 3, 4, 5, 6, 7, 255})
	{
		auto info = Info(); info.x0 = info.x1 = 480; info.y0 = info.y1 = 240;
		info.direction = static_cast<BYTE>(direction); info.step = 255;
		CHECK(world.bomb.Generate(info)); CHECK(!effects.back()->Update());
		CHECK_EQ(480, effects.back()->GetPixelX()); CHECK_EQ(240, effects.back()->GetPixelY());
		CHECK_EQ(24, effects.back()->GetPixelZ());
	}
}

TEST(ParabolicEffectGenerators, TargetSelectionOverridesTheSuppliedFacing)
{
	World world;
	for (auto* generator : world.Generators())
	{
		auto info = Info(); info.direction = DIRECTION_LEFTUP; info.x1 = info.y1 = 500;
		CHECK(generator->Generate(info)); CHECK_EQ(DIRECTION_RIGHTDOWN, effects.back()->GetDirection());
	}
}

TEST(ParabolicEffectGenerators, OrdinaryParabolaFliesItsArcAndRetainsArrivalOrdering)
{
	World world;
	CHECK(world.parabola.Generate(Info())); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(48, effect.GetPixelX()); CHECK_EQ(81, effect.GetPixelZ());
	CHECK(effect.Update()); CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(81, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetFrame()); CHECK_EQ(2, effect.GetX());
	calls.clear(); CHECK(!effect.Update());
	CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelZ()); CHECK_EQ(2, effect.GetFrame());
	CHECK_EQ(2, effect.GetX()); CHECK_EQ(0, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
	CHECK(calls == std::vector<int>({3})); CHECK(smoke.empty()); CHECK(impacts.empty());
}

TEST(ParabolicEffectGenerators, BombFliesToItsUnextendedEndpoint)
{
	World world;
	auto info = Info(); info.x1 = 192; info.z1 = 48;
	CHECK(world.bomb.Generate(info)); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(48, effect.GetPixelX()); CHECK_EQ(81, effect.GetPixelZ());
	CHECK(effect.Update()); CHECK_EQ(96, effect.GetPixelX());
	CHECK(!effect.Update()); CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelZ());
	CHECK(smoke.empty()); CHECK(impacts.empty());
}

TEST(ParabolicEffectGenerators, CannonadeKeepsSourceHeightAndImpactsTheOriginalTile)
{
	World world;
	auto info = Info(); info.nActionInfo = SKILL_CANNONADE; info.z1 = -24;
	CHECK(world.parabola.Generate(info)); auto& effect = *effects.front();
	CHECK_EQ(0, effect.GetPixelZ()); CHECK(effect.Update()); CHECK(effect.Update());
	CHECK(!effect.Update()); CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ(0, effect.GetPixelZ());
	CHECK(impacts == std::vector<Impact>({{3, 0}})); CHECK_EQ(3, smoke.size());
	CHECK_EQ(48, smoke[0].effect->GetPixelX()); CHECK_EQ(33, smoke[0].effect->GetPixelZ());
	CHECK_EQ(144, smoke[2].effect->GetPixelX()); CHECK_EQ(-1, smoke[2].effect->GetPixelZ());
	for (const auto& puff : smoke)
	{
		CHECK_EQ(10, puff.wait); CHECK_EQ(90, puff.effect->GetFrameID());
		CHECK_EQ(108, puff.effect->GetEndFrame()); CHECK(puff.effect->IsMulti());
	}
}

TEST(ParabolicEffectGenerators, CannonadeImpactUsesTruncationAndUnsignedTileNarrowing)
{
	World world;
	struct Case { int x, y, tileX, tileY; };
	for (const Case c : {Case{-1, -1, 0, 0}, {-48, -24, 65535, 65535},
		{3145728, 1572864, 0, 0}, {95, 47, 1, 1}})
	{
		auto info = Info(); info.nActionInfo = SKILL_CANNONADE; info.z1 = -24;
		info.x0 = info.x1 = c.x; info.y0 = info.y1 = c.y; info.direction = 255; info.step = 255;
		CHECK(world.parabola.Generate(info)); CHECK(!effects.back()->Update());
		CHECK_EQ(c.tileX, impacts.back().x); CHECK_EQ(c.tileY, impacts.back().y);
	}
}

TEST(ParabolicEffectGenerators, BombCannonadeActionKeepsItsRaisedSourceAndDefaultImpactTile)
{
	World world;
	auto info = Info(); info.nActionInfo = SKILL_CANNONADE; info.x1 = 192; info.z1 = 48;
	CHECK(world.bomb.Generate(info)); CHECK_EQ(48, effects.front()->GetPixelZ());
	CHECK(Arrive(*effects.front())); CHECK_EQ(192, effects.front()->GetPixelX());
	CHECK(impacts == std::vector<Impact>({{0, 0}})); CHECK_EQ(3, smoke.size());
}

TEST(ParabolicEffectGenerators, AcceptanceTransfersAnUnmodifiedCallerTarget)
{
	World world;
	for (auto* generator : world.Generators())
	{
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		const auto removed = removedTargets.size();
		CHECK(generator->Generate(info)); target.release(); CHECK(effects.back()->GetEffectTarget() == info.pEffectTarget);
		CHECK_EQ(1, effects.back()->GetLinkSize()); CHECK_EQ(777, info.pEffectTarget->GetX());
		CHECK_EQ(888, info.pEffectTarget->GetY()); CHECK_EQ(999, info.pEffectTarget->GetZ()); CHECK_EQ(456, info.pEffectTarget->GetID());
		effects.clear(); CHECK_EQ(removed + 1, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
	}
}

TEST(ParabolicEffectGenerators, RejectionRetainsTheCallerTarget)
{
	World world;
	acceptQueue = false;
	for (auto* generator : world.Generators())
	{
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		const auto removed = removedTargets.size();
		CHECK(!generator->Generate(info)); CHECK(effects.empty()); CHECK_EQ(removed, removedTargets.size());
		CHECK_EQ(777, target->GetX()); target.reset(); CHECK_EQ(removed + 1, removedTargets.size());
	}
}

TEST(ParabolicEffectGenerators, RejectingQueuesDestroyTheirOwnAttachedMarkers)
{
	World world;
	const auto reject = +[](std::unique_ptr<MEffect> effect) {
		effect->SetLink(43, Target(74).release()); return false;
	};
	const MZoneParabolaEffectHost parabola{.Sprite = parabolaHost.Sprite, .Queue = reject};
	const MZoneBombEffectHost bomb{.Sprite = bombHost.Sprite, .Queue = reject};
	MAttackZoneParabolaEffectGenerator::SetHost(&parabola); MAttackZoneBombEffectGenerator::SetHost(&bomb);
	for (auto* generator : world.Generators())
	{
		removedTargets.clear(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!generator->Generate(info)); CHECK(removedTargets == std::vector<int>({74}));
		target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
	}
}

TEST(ParabolicEffectGenerators, QueueExceptionsReleaseEffectsAndKeepCallerOwnership)
{
	World world;
	const auto fail = +[](std::unique_ptr<MEffect> effect) -> bool {
		effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure");
	};
	const MZoneParabolaEffectHost parabola{.Sprite = parabolaHost.Sprite, .Queue = fail};
	const MZoneBombEffectHost bomb{.Sprite = bombHost.Sprite, .Queue = fail};
	MAttackZoneParabolaEffectGenerator::SetHost(&parabola); MAttackZoneBombEffectGenerator::SetHost(&bomb);
	for (auto* generator : world.Generators())
	{
		removedTargets.clear(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		bool threw = false;
		try { generator->Generate(info); } catch (const std::runtime_error&) { threw = true; }
		CHECK(threw); CHECK(removedTargets == std::vector<int>({74}));
		target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
	}
}

TEST(ParabolicEffectGenerators, MissingMetadataRejectsBeforeConstruction)
{
	World world;
	spriteAvailable = false;
	for (auto* generator : world.Generators())
	{
		calls.clear(); CHECK(!generator->Generate(Info())); CHECK(calls == std::vector<int>({1}));
		CHECK(effects.empty()); CHECK(lights.empty());
	}
}

TEST(ParabolicEffectGenerators, MissingOrPartialMetadataHostsLeaveTargetsUntouched)
{
	World world;
	const MZoneParabolaEffectHost emptyParabola{}, queueParabola{.Queue = Queue};
	const MZoneBombEffectHost emptyBomb{}, queueBomb{.Queue = Queue};
	const MZoneParabolaEffectHost* parabolaHosts[] = {nullptr, &emptyParabola, &queueParabola};
	const MZoneBombEffectHost* bombHosts[] = {nullptr, &emptyBomb, &queueBomb};
	for (int i = 0; i < 3; ++i)
	{
		MAttackZoneParabolaEffectGenerator::SetHost(parabolaHosts[i]); MAttackZoneBombEffectGenerator::SetHost(bombHosts[i]);
		for (auto* generator : world.Generators())
		{
			auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
			const auto removed = removedTargets.size();
			CHECK(!generator->Generate(info)); CHECK_EQ(removed, removedTargets.size()); CHECK_EQ(777, target->GetX());
		}
	}
	CHECK(calls.empty()); CHECK(effects.empty());
}

TEST(ParabolicEffectGenerators, MissingQueuesReleaseUnlinkedEffects)
{
	World world;
	const MZoneParabolaEffectHost parabola{.Sprite = parabolaHost.Sprite};
	const MZoneBombEffectHost bomb{.Sprite = bombHost.Sprite};
	MAttackZoneParabolaEffectGenerator::SetHost(&parabola); MAttackZoneBombEffectGenerator::SetHost(&bomb);
	for (auto* generator : world.Generators())
	{
		calls.clear(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		const auto removed = removedTargets.size();
		CHECK(!generator->Generate(info)); CHECK_EQ(removed, removedTargets.size()); CHECK(effects.empty());
		CHECK(calls == std::vector<int>({1, 2, 3}));
	}
}

TEST(ParabolicEffectGenerators, MetadataRefreshesAndAnimationCountsNarrow)
{
	World world;
	for (auto* generator : world.Generators())
	{
		sprite = {BLT_EFFECT, 12, 3}; CHECK(generator->Generate(Info()));
		sprite = {BLT_NORMAL, 23, 258}; CHECK(generator->Generate(Info()));
		CHECK_EQ(12, effects[effects.size() - 2]->GetFrameID()); CHECK_EQ(23, effects.back()->GetFrameID());
		CHECK_EQ(BLT_NORMAL, effects.back()->GetBltType()); CHECK_EQ(2, effects.back()->GetMaxFrame());
		const auto lightCount = lights.size(); CHECK(effects.back()->Update()); CHECK_EQ(lightCount, lights.size());
	}
}

TEST(ParabolicEffectGenerators, SpriteCallbacksCanReplaceSubmissionServices)
{
	World world;
	const MZoneParabolaEffectHost parabola{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MZoneParabolaEffectSprite& result) {
			result = {BLT_NORMAL, 20, 2}; MAttackZoneParabolaEffectGenerator::SetHost(&parabolaHost); return true;
		},
	};
	const MZoneBombEffectHost bomb{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MZoneBombEffectSprite& result) {
			result = {BLT_NORMAL, 20, 2}; MAttackZoneBombEffectGenerator::SetHost(&bombHost); return true;
		},
	};
	CHECK(MAttackZoneParabolaEffectGenerator::SetHost(&parabola) == &parabolaHost);
	CHECK(MAttackZoneBombEffectGenerator::SetHost(&bomb) == &bombHost);
	for (auto* generator : world.Generators())
	{
		calls.clear(); CHECK(generator->Generate(Info())); CHECK_EQ(20, effects.back()->GetFrameID());
		CHECK_EQ(2, effects.back()->GetMaxFrame()); CHECK(calls == std::vector<int>({2, 3, 4}));
	}
	CHECK(MAttackZoneParabolaEffectGenerator::SetHost(nullptr) == &parabolaHost);
	CHECK(MAttackZoneBombEffectGenerator::SetHost(nullptr) == &bombHost);
}

TEST(ParabolicEffectGenerators, ClockCallbacksCanRemoveQueuesBeforeSubmission)
{
	World world;
	const MEffectHost changing{
		.CurrentFrame = []() {
			MAttackZoneParabolaEffectGenerator::SetHost(nullptr); MAttackZoneBombEffectGenerator::SetHost(nullptr); return frameNow;
		},
		.Light = effectHost.Light,
	};
	MEffect::SetHost(&changing);
	for (auto* generator : world.Generators())
	{
		MAttackZoneParabolaEffectGenerator::SetHost(&parabolaHost); MAttackZoneBombEffectGenerator::SetHost(&bombHost);
		calls.clear(); CHECK(!generator->Generate(Info())); CHECK(effects.empty());
		CHECK(calls == std::vector<int>({1, 2}));
	}
}

TEST(ParabolicEffectGenerators, MissingBaseServicesKeepStoredCountsAndInactiveEffects)
{
	World world;
	MEffect::SetHost(nullptr);
	for (auto* generator : world.Generators())
	{
		CHECK(generator->Generate(Info())); CHECK_EQ(29, effects.back()->GetEndFrame());
		CHECK_EQ(4, effects.back()->GetEndLinkFrame()); CHECK_EQ(0, effects.back()->GetLight());
		CHECK(!effects.back()->Update()); CHECK_EQ(48, effects.back()->GetPixelZ());
	}
}

TEST(ParabolicEffectGenerators, ZeroSpeedAnimatesInPlaceAndExpiredCountsDoNotAdvance)
{
	World world;
	for (auto* generator : world.Generators())
	{
		auto info = Info(); info.step = 0;
		CHECK(generator->Generate(info)); CHECK(effects.back()->Update());
		CHECK_EQ(0, effects.back()->GetPixelX()); CHECK_EQ(48, effects.back()->GetPixelZ()); CHECK_EQ(1, effects.back()->GetFrame());
		info = Info(); info.count = 1;
		CHECK(generator->Generate(info)); CHECK(!effects.back()->Update());
		CHECK_EQ(0, effects.back()->GetPixelX()); CHECK_EQ(48, effects.back()->GetPixelZ()); CHECK_EQ(0, effects.back()->GetFrame());
	}
}

TEST(ParabolicEffectGenerators, CountsStayFiniteAndLinkDeadlinesIndependent)
{
	World world;
	for (auto* generator : world.Generators())
	{
		auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT;
		CHECK(generator->Generate(info)); CHECK_EQ(65634, effects.back()->GetEndFrame()); CHECK_EQ(65634, effects.back()->GetEndLinkFrame());
		info.count = 10; info.linkCount = 20;
		CHECK(generator->Generate(info)); CHECK_EQ(109, effects.back()->GetEndFrame()); CHECK_EQ(119, effects.back()->GetEndLinkFrame());
	}
}

TEST(ParabolicEffectGenerators, CountsRetainTheirAbsoluteClockWrap)
{
	World world;
	for (auto* generator : world.Generators())
	{
		frameNow = (std::numeric_limits<DWORD>::max)() - 1;
		auto info = Info(); info.count = 4;
		CHECK(generator->Generate(info)); CHECK_EQ(1, effects.back()->GetEndFrame()); CHECK(!effects.back()->Update());
		frameNow = 0; CHECK(effects.back()->Update()); CHECK_EQ(1, effects.back()->GetFrame());
	}
}

TEST(ParabolicEffectGenerators, RaisedSourcesClampBeforeFloatStorage)
{
	World world;
	const int top = (std::numeric_limits<int>::max)();
	for (auto* generator : world.Generators())
		for (const int source : {top, top - 1, top - 47})
		{
			auto info = Info(); info.z0 = source;
			CHECK(generator->Generate(info)); CHECK_EQ(top, effects.back()->GetPixelZ());
		}
}

TEST(ParabolicEffectGenerators, ExtendedEndpointsClampAtEveryPixelBoundary)
{
	World world;
	const int top = (std::numeric_limits<int>::max)(), bottom = (std::numeric_limits<int>::min)();
	struct Case { int sx, sy, tx, ty; BYTE direction; };
	for (const Case c : {Case{top - 255, 0, top, 0, DIRECTION_RIGHT},
		{bottom + 256, 0, bottom, 0, DIRECTION_LEFT}, {0, top - 255, 0, top, DIRECTION_DOWN},
		{0, bottom + 256, 0, bottom, DIRECTION_UP}, {top - 255, top - 255, top, top, DIRECTION_RIGHTDOWN},
		{bottom + 256, bottom + 256, bottom, bottom, DIRECTION_LEFTUP}})
	{
		auto info = Info(); info.x0 = c.sx; info.y0 = c.sy; info.x1 = c.tx; info.y1 = c.ty;
		info.direction = c.direction; info.step = 255;
		CHECK(world.parabola.Generate(info)); CHECK(!effects.back()->Update());
		CHECK_EQ(c.tx, effects.back()->GetPixelX()); CHECK_EQ(c.ty, effects.back()->GetPixelY());
		CHECK_EQ(48, effects.back()->GetPixelZ()); CHECK_EQ(104, effects.back()->GetEndLinkFrame());
	}
}

TEST(ParabolicEffectGenerators, LiftedParabolaTargetHeightClampsToTheIntegerRange)
{
	World world;
	const int top = (std::numeric_limits<int>::max)();
	for (const int height : {top, top - 1, top - 23})
	{
		auto info = Info(); info.x1 = 0; info.direction = 255; info.z1 = height; info.step = 255;
		CHECK(world.parabola.Generate(info)); CHECK(!effects.back()->Update());
		CHECK_EQ(top, effects.back()->GetPixelZ()); CHECK_EQ(0, effects.back()->GetEndFrame());
	}
}

TEST(ParabolicEffectGenerators, SaturatedCannonadeEndpointKeepsTheOriginalImpactTile)
{
	World world;
	const int top = (std::numeric_limits<int>::max)();
	auto info = Info(); info.x0 = info.y0 = top - 255; info.x1 = info.y1 = info.z1 = top;
	info.z0 = 7; info.direction = DIRECTION_RIGHTDOWN; info.step = 255; info.nActionInfo = SKILL_CANNONADE;
	auto target = Target(); info.pEffectTarget = target.get();
	CHECK(world.parabola.Generate(info)); target.release(); CHECK_EQ(7, effects.front()->GetPixelZ());
	CHECK(!effects.front()->Update()); CHECK_EQ(top, effects.front()->GetPixelX());
	CHECK_EQ(top, effects.front()->GetPixelY()); CHECK_EQ(top, effects.front()->GetPixelZ());
	CHECK(impacts == std::vector<Impact>({{43690, 21845}})); CHECK_EQ(1, smoke.size());
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(777, info.pEffectTarget->GetX());
	effects.clear(); CHECK(removedTargets == std::vector<int>({73}));
}

TEST(ParabolicEffectGenerators, RepresentableSourceLiftsRetainFloatRounding)
{
	World world;
	const int top = (std::numeric_limits<int>::max)(), bottom = (std::numeric_limits<int>::min)();
	struct Case { int source, expected; };
	for (auto* generator : world.Generators())
		for (const Case c : {Case{top - 48, top}, {top - 175, top - 127}, {bottom, bottom},
			{-48, 0}, {-49, -1}, {-1, 47}})
		{
			auto info = Info(); info.z0 = c.source;
			CHECK(generator->Generate(info)); CHECK_EQ(c.expected, effects.back()->GetPixelZ());
		}
}

TEST(ParabolicEffectGenerators, CannonadeSourceHeightRemainsUnliftedAtIntegerLimits)
{
	World world;
	for (const int source : {(std::numeric_limits<int>::max)(), (std::numeric_limits<int>::min)(), -48, 0, 48})
	{
		auto info = Info(); info.nActionInfo = SKILL_CANNONADE; info.z0 = source;
		CHECK(world.parabola.Generate(info)); CHECK_EQ(source, effects.back()->GetPixelZ());
	}
}

TEST(ParabolicEffectGenerators, BombDestinationHeightRemainsUnliftedAtIntegerLimits)
{
	World world;
	const int top = (std::numeric_limits<int>::max)(), bottom = (std::numeric_limits<int>::min)();
	struct Case { int source, target; };
	for (const Case c : {Case{top - 175, top}, {bottom + 80, bottom}})
	{
		auto info = Info(); info.x1 = 0; info.z0 = c.source; info.z1 = c.target; info.step = 255;
		CHECK(world.bomb.Generate(info)); CHECK(!effects.back()->Update()); CHECK_EQ(c.target, effects.back()->GetPixelZ());
	}
}
