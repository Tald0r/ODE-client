#include "test_framework.h"
#include "MStopInventoryEffectGenerator.h"
#include "MScreenEffect.h"
#include "MScreenEffectManager.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MInventoryEffectPlacement placement;
MInventoryEffectSprite sprite;
bool placementAvailable, spriteAvailable, throwOnAdd;
MScreenEffectManager* destination;
std::vector<int> calls;
int requestedX, requestedY, requestedSprite, targetsDestroyed;
MEffectTarget* targetAtInsertion;
TYPE_ACTIONINFO actionAtInsertion;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(5); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE direction, BYTE frame) {
		calls.push_back(4);
		CHECK_EQ(0, direction);
		CHECK_EQ(0, frame);
		return 9;
	},
};
const MInventoryEffectHost host{
	.Placement = [](int x, int y, MInventoryEffectPlacement& result) {
		calls.push_back(1); requestedX = x; requestedY = y;
		result = placement;
		return placementAvailable;
	},
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MInventoryEffectSprite& result) {
		calls.push_back(2); requestedSprite = type;
		result = sprite;
		return spriteAvailable;
	},
	.Manager = []() { calls.push_back(3); return destination; },
};

struct Manager : MScreenEffectManager
{
	void AddEffect(MEffect* effect) override
	{
		calls.push_back(6);
		targetAtInsertion = effect->GetEffectTarget();
		actionAtInsertion = effect->GetActionInfo();
		if (throwOnAdd) throw std::runtime_error("Queue unavailable");
		MScreenEffectManager::AddEffect(effect);
	}
	MScreenEffect* First() { return static_cast<MScreenEffect*>(*GetEffects()); }
};

struct World
{
	Manager manager;
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(nullptr);
	const MInventoryEffectHost* previousGenerator = MStopInventoryEffectGenerator::SetHost(&host);
	const MScreenEffectManagerHost* previousManager = MScreenEffectManager::SetHost(nullptr);
	int basisX = MScreenEffect::m_ScreenBasisX, basisY = MScreenEffect::m_ScreenBasisY;
	World()
	{
		frameNow = 100;
		placement = {{100, 200}, {160, 290}, 2, 3, 30, 30};
		sprite = {BLT_EFFECT, 12, 3};
		placementAvailable = spriteAvailable = true; throwOnAdd = false;
		destination = &manager;
		requestedX = requestedY = requestedSprite = -1; targetsDestroyed = 0;
		targetAtInsertion = nullptr; actionAtInsertion = 0;
		calls.clear();
	}
	~World()
	{
		manager.Release();
		MScreenEffect::SetScreenBasis(basisX, basisY);
		MScreenEffectManager::SetHost(previousManager);
		MStopInventoryEffectGenerator::SetHost(previousGenerator);
		MEffectTarget::SetHost(previousTarget);
		MEffect::SetHost(previousEffect);
	}
};

struct Target : MEffectTarget
{
	using MEffectTarget::operator=;
	Target() : MEffectTarget(3) { NextPhase(); }
	~Target() override { ++targetsDestroyed; }
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 900; info.y0 = 800; info.z0 = 700;
	info.x1 = 2; info.y1 = 3; info.z1 = 99;
	info.direction = DIRECTION_DOWN; info.step = 12;
	info.count = 10; info.linkCount = 5; info.power = 77;
	return info;
}

} // namespace

TEST(InventoryEffectGenerator, GeneratesAConfiguredProductionScreenEffect)
{
	World world;
	MStopInventoryEffectGenerator generator;
	const auto info = Info();
	CHECK_EQ(EFFECTGENERATORID_STOP_INVENTORY, generator.GetID());
	CHECK(generator.Generate(info));
	CHECK_EQ(1, world.manager.GetSize());
	auto* effect = world.manager.First();
	CHECK_EQ(MEffect::EFFECT_SCREEN, effect->GetEffectType());
	CHECK_EQ(BLT_EFFECT, effect->GetBltType());
	CHECK_EQ(12, effect->GetFrameID());
	CHECK_EQ(3, effect->GetMaxFrame());
	CHECK_EQ(0, effect->GetFrame());
	CHECK_EQ(9, effect->GetLight());
	CHECK_EQ(175, effect->GetScreenX());
	CHECK_EQ(320, effect->GetScreenY());
	CHECK_EQ(75, effect->GetPixelX());
	CHECK_EQ(120, effect->GetPixelY());
	CHECK_EQ(0, effect->GetPixelZ());
	CHECK_EQ(2, effect->GetX());
	CHECK_EQ(3, effect->GetY());
	CHECK_EQ(0, effect->GetStepPixel());
	CHECK_EQ(0, effect->GetDirection());
	CHECK_EQ(77, effect->GetPower());
	CHECK_EQ(109, effect->GetEndFrame());
	CHECK_EQ(104, effect->GetEndLinkFrame());
	CHECK_EQ(42, effect->GetActionInfo());
	CHECK_EQ(2, requestedX); CHECK_EQ(3, requestedY); CHECK_EQ(17, requestedSprite);
}

TEST(InventoryEffectGenerator, ResolvesSnapshotsThenConstructsBeforeTransferringTheTarget)
{
	World world;
	MStopInventoryEffectGenerator generator;
	auto target = std::make_unique<Target>();
	auto info = Info(); info.pEffectTarget = target.get();
	CHECK(generator.Generate(info));
	target.release();
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5, 6}));
	CHECK(targetAtInsertion == nullptr);
	CHECK_EQ(ACTIONINFO_NULL, actionAtInsertion);
	CHECK(world.manager.First()->GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(1, world.manager.First()->GetLinkSize());
	CHECK_EQ(0, targetsDestroyed);
	world.manager.Release();
	CHECK_EQ(1, targetsDestroyed);
}

TEST(InventoryEffectGenerator, MissingItemsUseTheCellOriginWithoutCentering)
{
	World world;
	MStopInventoryEffectGenerator generator;
	placement.itemWidth = placement.itemHeight = 1;
	CHECK(generator.Generate(Info()));
	CHECK_EQ(160, world.manager.First()->GetScreenX());
	CHECK_EQ(290, world.manager.First()->GetScreenY());
}

TEST(InventoryEffectGenerator, CentersEveryItemFootprintUsingTheSuppliedCellDimensions)
{
	World world;
	MStopInventoryEffectGenerator generator;
	placement.cellWidth = 31; placement.cellHeight = 25;
	for (int width = 1; width <= 3; ++width)
		for (int height = 1; height <= 4; ++height)
		{
			placement.itemWidth = width; placement.itemHeight = height;
			CHECK(generator.Generate(Info()));
			const int xOffsets[]{0, 15, 31};
			const int yOffsets[]{0, 12, 25, 37};
			CHECK_EQ(160 + xOffsets[width - 1], world.manager.First()->GetScreenX());
			CHECK_EQ(290 + yOffsets[height - 1], world.manager.First()->GetScreenY());
		}
}

TEST(InventoryEffectGenerator, SpriteMetadataIsReadForEveryGenerationAndFramesNarrowToByte)
{
	World world;
	MStopInventoryEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	auto* first = world.manager.First();
	sprite = {BLT_NORMAL, 23, 258};
	CHECK(generator.Generate(Info()));
	auto* second = world.manager.First();
	CHECK_EQ(12, first->GetFrameID());
	CHECK_EQ(23, second->GetFrameID());
	CHECK_EQ(2, second->GetMaxFrame());
	CHECK_EQ(BLT_NORMAL, second->GetBltType());
	CHECK_EQ(9, second->GetLight());
}

TEST(InventoryEffectGenerator, EachGenerationUpdatesTheSharedScreenBasis)
{
	World world;
	MStopInventoryEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	auto* first = world.manager.First();
	placement.basis = {300, 400}; placement.cell = {350, 450};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(375, first->GetScreenX());
	CHECK_EQ(520, first->GetScreenY());
	CHECK_EQ(365, world.manager.First()->GetScreenX());
	CHECK_EQ(480, world.manager.First()->GetScreenY());
}

TEST(InventoryEffectGenerator, CountAndLinkRemainIndependentFiniteDeadlines)
{
	World world;
	MStopInventoryEffectGenerator generator;
	auto info = Info(); info.count = 0xFFFF; info.linkCount = MAX_LINKCOUNT;
	CHECK(generator.Generate(info));
	CHECK_EQ(65634, world.manager.First()->GetEndFrame());
	CHECK_EQ(65634, world.manager.First()->GetEndLinkFrame());
	info.count = 10; info.linkCount = 20;
	CHECK(generator.Generate(info));
	CHECK_EQ(109, world.manager.First()->GetEndFrame());
	CHECK_EQ(119, world.manager.First()->GetEndLinkFrame());
}

TEST(InventoryEffectGenerator, FailedPlacementLeavesOwnershipAndScreenBasisUntouched)
{
	World world;
	MStopInventoryEffectGenerator generator;
	auto target = std::make_unique<Target>();
	auto info = Info(); info.pEffectTarget = target.get();
	MScreenEffect::SetScreenBasis(7, 8);
	placementAvailable = false;
	CHECK(!generator.Generate(info));
	CHECK_EQ(0, world.manager.GetSize());
	CHECK_EQ(0, targetsDestroyed);
	CHECK_EQ(7, MScreenEffect::m_ScreenBasisX);
	CHECK_EQ(8, MScreenEffect::m_ScreenBasisY);
	CHECK(calls == std::vector<int>({1}));
}

TEST(InventoryEffectGenerator, FailedSpriteLookupStopsBeforeRequestingAManager)
{
	World world;
	MStopInventoryEffectGenerator generator;
	spriteAvailable = false;
	CHECK(!generator.Generate(Info()));
	CHECK_EQ(0, world.manager.GetSize());
	CHECK(calls == std::vector<int>({1, 2}));
}

TEST(InventoryEffectGenerator, MissingManagerDoesNotConstructOrConsumeTheTarget)
{
	World world;
	MStopInventoryEffectGenerator generator;
	auto target = std::make_unique<Target>();
	auto info = Info(); info.pEffectTarget = target.get();
	destination = nullptr;
	CHECK(!generator.Generate(info));
	CHECK_EQ(0, world.manager.GetSize());
	CHECK_EQ(0, targetsDestroyed);
	CHECK(calls == std::vector<int>({1, 2, 3}));
}

TEST(InventoryEffectGenerator, NullOrPartialHostsRejectWithoutTakingOwnership)
{
	World world;
	MStopInventoryEffectGenerator generator;
	const MInventoryEffectHost empty{};
	const MInventoryEffectHost noSprite{.Placement = host.Placement, .Manager = host.Manager};
	const MInventoryEffectHost noManager{.Placement = host.Placement, .Sprite = host.Sprite};
	for (const auto* service : {static_cast<const MInventoryEffectHost*>(nullptr), &empty, &noSprite, &noManager})
	{
		auto target = std::make_unique<Target>();
		auto info = Info(); info.pEffectTarget = target.get();
		const int destroyedBefore = targetsDestroyed;
		MStopInventoryEffectGenerator::SetHost(service);
		CHECK(!generator.Generate(info));
		CHECK_EQ(0, world.manager.GetSize());
		CHECK_EQ(destroyedBefore, targetsDestroyed);
	}
}

TEST(InventoryEffectGenerator, PlacementCallbackCanReplaceTheRemainingHostServices)
{
	World world;
	MStopInventoryEffectGenerator generator;
	const MInventoryEffectHost changing{
		.Placement = [](int, int, MInventoryEffectPlacement& result) {
			result.cell = {123, 234};
			MStopInventoryEffectGenerator::SetHost(&host);
			return true;
		},
	};
	CHECK(MStopInventoryEffectGenerator::SetHost(&changing) == &host);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(123, world.manager.First()->GetScreenX());
	CHECK_EQ(234, world.manager.First()->GetScreenY());
	CHECK(calls == std::vector<int>({2, 3, 4, 5, 6}));
	CHECK(MStopInventoryEffectGenerator::SetHost(&changing) == &host);
}

TEST(InventoryEffectGenerator, DestinationIsResolvedForEachGeneration)
{
	World world;
	MStopInventoryEffectGenerator generator;
	Manager second;
	CHECK(generator.Generate(Info()));
	destination = &second;
	CHECK(generator.Generate(Info()));
	CHECK_EQ(1, world.manager.GetSize());
	CHECK_EQ(1, second.GetSize());
}

TEST(InventoryEffectGenerator, QueueFailureLeavesTheCallerOwningItsTarget)
{
	World world;
	MStopInventoryEffectGenerator generator;
	auto target = std::make_unique<Target>();
	auto info = Info(); info.pEffectTarget = target.get();
	throwOnAdd = true;
	bool threw = false;
	try { generator.Generate(info); }
	catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(0, world.manager.GetSize());
	CHECK_EQ(0, targetsDestroyed);
	target.reset();
	CHECK_EQ(1, targetsDestroyed);
}

TEST(InventoryEffectGenerator, GeneratedEffectsExpireThroughTheRealManager)
{
	World world;
	MStopInventoryEffectGenerator generator;
	auto target = std::make_unique<Target>();
	auto info = Info(); info.pEffectTarget = target.get(); info.count = 1;
	CHECK(generator.Generate(info));
	target.release();
	CHECK_EQ(0, targetsDestroyed);
	world.manager.Update();
	CHECK_EQ(0, world.manager.GetSize());
	CHECK_EQ(1, targetsDestroyed);
}

TEST(InventoryEffectGenerator, MissingBaseClockAndLightKeepTheirEstablishedFallbacks)
{
	World world;
	MStopInventoryEffectGenerator generator;
	MEffect::SetHost(nullptr);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(9, world.manager.First()->GetEndFrame());
	CHECK_EQ(4, world.manager.First()->GetEndLinkFrame());
	CHECK_EQ(0, world.manager.First()->GetLight());
	world.manager.Update();
	CHECK_EQ(0, world.manager.GetSize());
}

TEST(InventoryEffectGenerator, WrappedCountsRetainAbsoluteUnsignedTiming)
{
	World world;
	MStopInventoryEffectGenerator generator;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	auto info = Info(); info.count = 4; info.linkCount = 2;
	CHECK(generator.Generate(info));
	CHECK_EQ(1, world.manager.First()->GetEndFrame());
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), world.manager.First()->GetEndLinkFrame());
}

TEST(InventoryEffectGenerator, GridCoordinatesNarrowBeforeTheScreenPositionOverridesPixels)
{
	World world;
	MStopInventoryEffectGenerator generator;
	auto info = Info(); info.x1 = -1; info.y1 = 65536;
	CHECK(generator.Generate(info));
	CHECK_EQ(-1, requestedX); CHECK_EQ(65536, requestedY);
	CHECK_EQ(65535, world.manager.First()->GetX());
	CHECK_EQ(0, world.manager.First()->GetY());
	CHECK_EQ(175, world.manager.First()->GetScreenX());
	CHECK_EQ(320, world.manager.First()->GetScreenY());
}

TEST(InventoryEffectGenerator, LargeFootprintMultiplicationKeepsExactCancellation)
{
	World world;
	MStopInventoryEffectGenerator generator;
	const int high = (std::numeric_limits<int>::max)();
	placement = {{0, 0}, {-high, -high}, 3, 3, high, high};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(0, world.manager.First()->GetScreenX());
	CHECK_EQ(0, world.manager.First()->GetScreenY());
}

TEST(InventoryEffectGenerator, ExtremeDimensionsDoNotOverflowBeforeCentering)
{
	World world;
	MStopInventoryEffectGenerator generator;
	const int low = (std::numeric_limits<int>::min)();
	placement = {{0, 0}, {0, 0}, low, low, 1, 1};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(-1073741824, world.manager.First()->GetScreenX());
	CHECK_EQ(-1073741824, world.manager.First()->GetScreenY());
}

TEST(InventoryEffectGenerator, AddingTheCenterOffsetSaturatesAtIntegerLimits)
{
	World world;
	MStopInventoryEffectGenerator generator;
	const int high = (std::numeric_limits<int>::max)();
	const int low = (std::numeric_limits<int>::min)();
	placement = {{0, 0}, {high, low}, 3, 3, 30, -30};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(high, world.manager.First()->GetScreenX());
	CHECK_EQ(low, world.manager.First()->GetScreenY());
}

TEST(InventoryEffectGenerator, NegativeHalfCellOffsetsStillTruncateTowardZero)
{
	World world;
	MStopInventoryEffectGenerator generator;
	placement = {{0, 0}, {10, 20}, 0, 0, 3, -3};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(9, world.manager.First()->GetScreenX());
	CHECK_EQ(21, world.manager.First()->GetScreenY());
}

TEST(InventoryEffectGenerator, FullIntegerDimensionsKeepTheWideProductRepresentable)
{
	World world;
	MStopInventoryEffectGenerator generator;
	const int high = (std::numeric_limits<int>::max)();
	const int low = (std::numeric_limits<int>::min)();
	for (int dimension : {low, high})
		for (int pixels : {low, high})
		{
			placement = {{0, 0}, {0, 0}, dimension, dimension, pixels, pixels};
			CHECK(generator.Generate(Info()));
			const int expected = (dimension < 0) == (pixels < 0) ? high : low;
			CHECK_EQ(expected, world.manager.First()->GetScreenX());
			CHECK_EQ(expected, world.manager.First()->GetScreenY());
		}
}
