#include "test_framework.h"
#include "MEffect.h"
#include "MViewDef.h"

#include <cstring>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>

namespace {

DWORD currentFrame;
int lightValue;
std::vector<int> calls;
struct LightRequest
{
	BYTE blt;
	TYPE_FRAMEID id;
	BYTE direction;
	BYTE frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
const MEffectHost host{
	.CurrentFrame = []() { calls.push_back(1); return currentFrame; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(2);
		lights.push_back({blt, id, direction, frame});
		return lightValue;
	},
};
const MEffectHost otherHost{
	.CurrentFrame = []() { return currentFrame + 100; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 17; },
};

struct World
{
	const MEffectHost* previous = MEffect::SetHost(&host);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(nullptr);
	World() { currentFrame = 100; lightValue = 7; calls.clear(); lights.clear(); }
	~World()
	{
		MEffect::SetHost(previous);
		MEffectTarget::SetHost(previousTarget);
	}
};

struct EffectProbe : MEffect
{
	using MEffect::MEffect;
	void SetSubpixel(float x, float y, float z)
	{
		m_PixelX = x; m_PixelY = y; m_PixelZ = z;
		AffectPosition();
	}
};

struct Target : MEffectTarget
{
	using MEffectTarget::operator=;
	int& destroyed;
	explicit Target(int& count) : MEffectTarget(3), destroyed(count) {}
	~Target() override { ++destroyed; }
};

} // namespace

TEST(EffectBase, DefaultStateRetainsTheBaseObjectAndAnimationContracts)
{
	World world;
	MEffect effect(BLT_EFFECT);
	CHECK_EQ(MObject::TYPE_EFFECT, effect.GetObjectType());
	CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
	CHECK_EQ(SECTORPOSITION_NULL, effect.GetX());
	CHECK_EQ(SECTORPOSITION_NULL, effect.GetY());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetDirection());
	CHECK_EQ(0, effect.GetLight());
	CHECK_EQ(0, effect.GetStepPixel());
	CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(0, effect.GetEndLinkFrame());
	CHECK(effect.GetEffectTarget() == nullptr);
	CHECK(effect.GetResourceContainer() == nullptr);
	CHECK(!effect.IsSelectable());
	CHECK(!effect.IsMulti());
	CHECK(!effect.IsSkipDraw());
	CHECK(effect.IsEnd());
	CHECK(!effect.IsDelayFrame());
	CHECK(!effect.IsWaitFrame());
	CHECK(lights.empty());
}

TEST(EffectBase, BothConstructorsAllocateConsecutiveObjectIDs)
{
	World world;
	MEffect first(BLT_EFFECT);
	MEffect second(BLT_NORMAL, nullptr);
	CHECK_EQ(first.GetID() + 1, second.GetID());
	CHECK_EQ(MObject::TYPE_EFFECT, second.GetObjectType());
	CHECK_EQ(BLT_NORMAL, second.GetBltType());
	CHECK(second.GetResourceContainer() == nullptr);
}

TEST(EffectBase, ResourceContainerIsOnlyBorrowedAndCanBeReplacedOrCleared)
{
	World world;
	// Opaque identity tokens: MEffect must neither dereference nor delete them.
	char first, second;
	auto* a = reinterpret_cast<EffectResourceContainer*>(&first);
	auto* b = reinterpret_cast<EffectResourceContainer*>(&second);
	{
		MEffect effect(BLT_EFFECT, a);
		CHECK(effect.GetResourceContainer() == a);
		effect.SetResourceContainer(b);
		CHECK(effect.GetResourceContainer() == b);
		effect.SetResourceContainer(nullptr);
		CHECK(effect.GetResourceContainer() == nullptr);
		effect.SetResourceContainer(a);
	}
	CHECK(calls.empty());
}

TEST(EffectBase, MetadataAndSelectableSubclassRemainIndependent)
{
	World world;
	MSelectableEffect effect(BLT_EFFECT);
	CHECK(effect.IsSelectable());
	effect.SetEst(-19);
	effect.SetPower(255);
	effect.SetStepPixel(65535);
	effect.SetDirection(254);
	effect.SetLight(-12);
	effect.SetMulti(true);
	effect.SetDrawSkip(true);
	CHECK_EQ(-19, effect.GetEst());
	CHECK_EQ(255, effect.GetPower());
	CHECK_EQ(65535, effect.GetStepPixel());
	CHECK_EQ(254, effect.GetDirection());
	CHECK_EQ(-12, effect.GetLight());
	CHECK(effect.IsMulti());
	CHECK(effect.IsSkipDraw());
	effect.SetMulti(false);
	effect.SetDrawSkip(false);
	CHECK(!effect.IsMulti());
	CHECK(!effect.IsSkipDraw());
	CHECK(calls.empty());
}

TEST(EffectBase, SectorPositionUpdatesPixelsWithoutChangingHeight)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetZ(-7);
	effect.SetPosition(13, 24);
	CHECK_EQ(13, effect.GetX());
	CHECK_EQ(24, effect.GetY());
	CHECK_EQ(13 * TILE_X, effect.GetPixelX());
	CHECK_EQ(24 * TILE_Y, effect.GetPixelY());
	CHECK_EQ(-7, effect.GetPixelZ());
	effect.SetX(65535);
	CHECK_EQ(65535, effect.GetX());
	CHECK_EQ(65535 * TILE_X, effect.GetPixelX());
	CHECK_EQ(24 * TILE_Y, effect.GetPixelY());
	effect.SetY(65535);
	CHECK_EQ(65535, effect.GetY());
	CHECK_EQ(65535 * TILE_Y, effect.GetPixelY());
	CHECK_EQ(65535 * TILE_X, effect.GetPixelX());
	CHECK_EQ(-7, effect.GetPixelZ());
}

TEST(EffectBase, PixelPositionTruncatesTowardZeroBeforeSectorConversion)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetPixelPosition(TILE_X + 1, 2 * TILE_Y - 1, 39);
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetY());
	CHECK_EQ(TILE_X + 1, effect.GetPixelX());
	CHECK_EQ(2 * TILE_Y - 1, effect.GetPixelY());
	CHECK_EQ(39, effect.GetPixelZ());
	effect.SetPixelPosition(-TILE_X + 1, -TILE_Y + 1, -17);
	CHECK_EQ(0, effect.GetX());
	CHECK_EQ(0, effect.GetY());
	CHECK_EQ(-17, effect.GetPixelZ());
	effect.SetPixelPosition(-TILE_X, -2 * TILE_Y, 0);
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(-1), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(-2), effect.GetY());
}

TEST(EffectBase, DerivedMovementKeepsFractionalPixelsUntilProjection)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	effect.SetSubpixel(47.875f, -24.875f, -0.875f);
	CHECK_EQ(47, effect.GetPixelX());
	CHECK_EQ(-24, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(-1), effect.GetY());
}

TEST(EffectBase, FrameSelectionReadsTheCurrentDirectionAndFrameZeroLight)
{
	World world;
	MEffect effect(BLT_SHADOW);
	effect.SetDirection(6);
	effect.SetFrameID(1234, 4);
	CHECK_EQ(1234, effect.GetFrameID());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(4, effect.GetMaxFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK(lights == std::vector<LightRequest>({{BLT_SHADOW, 1234, 6, 0}}));
	effect.SetDirection(2);
	CHECK_EQ(1, lights.size());
	CHECK_EQ(7, effect.GetLight());
}

TEST(EffectBase, UpdateAdvancesAndWrapsBeforeRefreshingLightAndCheckingTime)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetFrameID(12, 3);
	effect.SetCount(10);
	effect.SetDirection(4);
	calls.clear(); lights.clear();
	for (int frame : {1, 2, 0, 1})
	{
		lightValue = 20 + frame;
		CHECK(effect.Update());
		CHECK_EQ(frame, effect.GetFrame());
		CHECK_EQ(lightValue, effect.GetLight());
		CHECK(lights.back() == (LightRequest{BLT_EFFECT, 12, 4, static_cast<BYTE>(frame)}));
	}
	CHECK(calls == std::vector<int>({2, 1, 2, 1, 2, 1, 2, 1}));
}

TEST(EffectBase, ExpiredUpdateStillAdvancesAndReadsLighting)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetFrameID(42, 3);
	effect.SetCount(2);
	currentFrame = 101;
	lightValue = -9;
	calls.clear();
	CHECK(!effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(-9, effect.GetLight());
	CHECK(calls == std::vector<int>({2, 1}));
}

TEST(EffectBase, SelectingAnotherFrameResetsAnimationButPreservesLifetime)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetCount(20, 7);
	effect.SetFrameID(1, 4);
	CHECK(effect.Update());
	effect.SetFrameID(2, 2);
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(2, effect.GetFrameID());
	CHECK_EQ(2, effect.GetMaxFrame());
	CHECK_EQ(119, effect.GetEndFrame());
	CHECK_EQ(106, effect.GetEndLinkFrame());
}

TEST(EffectBase, ZeroFrameCountRetainsByteWrapProgression)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetFrameID(5, 0);
	effect.SetCount(1000);
	for (int i = 1; i <= 256; ++i)
	{
		CHECK(effect.Update());
		CHECK_EQ(static_cast<BYTE>(i), effect.GetFrame());
	}
}

TEST(EffectBase, CountUsesTheLiveFrameAndPreservesMinusOneDeadlines)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetCount(5, 2);
	CHECK_EQ(104, effect.GetEndFrame());
	CHECK_EQ(101, effect.GetEndLinkFrame());
	CHECK(!effect.IsEnd());
	currentFrame = 103;
	CHECK(!effect.IsEnd());
	currentFrame = 104;
	CHECK(effect.IsEnd());
	effect.SetCount(2);
	CHECK_EQ(105, effect.GetEndFrame());
	CHECK_EQ(105, effect.GetEndLinkFrame());
	CHECK(!effect.IsEnd());
}

TEST(EffectBase, DeadlineWrapAndZeroCountRetainUnsignedComparisons)
{
	World world;
	MEffect effect(BLT_EFFECT);
	currentFrame = (std::numeric_limits<DWORD>::max)() - 1;
	effect.SetCount(4);
	CHECK_EQ(1, effect.GetEndFrame());
	CHECK(effect.IsEnd());
	currentFrame = 0;
	CHECK(!effect.IsEnd());
	effect.SetCount(0);
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), effect.GetEndFrame());
	CHECK(!effect.IsEnd());
}

TEST(EffectBase, DelayAndWaitUseIndependentLiveDeadlines)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetCount(30);
	effect.SetDelayFrame(2);
	effect.SetWaitFrame(4);
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	currentFrame = 102;
	CHECK(!effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	currentFrame = 104;
	CHECK(!effect.IsWaitFrame());
	CHECK(!effect.IsEnd());
	effect.SetDelayFrame(0);
	effect.SetWaitFrame(0);
	CHECK(!effect.IsDelayFrame());
	CHECK(!effect.IsWaitFrame());
}

TEST(EffectBase, NoHostMeansNoDelayWaitOrLighting)
{
	World world;
	MEffect::SetHost(nullptr);
	MEffect effect(BLT_EFFECT);
	effect.SetCount(10);
	effect.SetDelayFrame(5);
	effect.SetWaitFrame(7);
	CHECK_EQ(9, effect.GetEndFrame());
	CHECK_EQ(9, effect.GetEndLinkFrame());
	CHECK(effect.IsEnd());
	CHECK(!effect.IsDelayFrame());
	CHECK(!effect.IsWaitFrame());
	effect.SetLight(12);
	effect.SetFrameID(32, 4);
	CHECK_EQ(0, effect.GetLight());
	CHECK(!effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK(calls.empty());
}

TEST(EffectBase, EmptyHostEntriesHaveTheSameFallbackAsNoHost)
{
	World world;
	const MEffectHost empty{};
	MEffect::SetHost(&empty);
	MEffect effect(BLT_EFFECT);
	effect.SetCount(10);
	effect.SetDelayFrame(5);
	effect.SetWaitFrame(7);
	effect.SetFrameID(1, 2);
	CHECK(effect.IsEnd());
	CHECK(!effect.IsDelayFrame());
	CHECK(!effect.IsWaitFrame());
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetLight());
	CHECK(calls.empty());
}

TEST(EffectBase, ServicesAreIndependentAndHostReplacementAffectsExistingEffects)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetCount(50);
	effect.SetFrameID(1, 3);
	CHECK(MEffect::SetHost(&otherHost) == &host);
	CHECK(effect.IsEnd());
	CHECK(!effect.Update());
	CHECK_EQ(17, effect.GetLight());
	CHECK(MEffect::SetHost(nullptr) == &otherHost);
	const MEffectHost clockOnly{.CurrentFrame = host.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetLight());
	const MEffectHost lightOnly{.CurrentFrame = nullptr, .Light = host.Light};
	MEffect::SetHost(&lightOnly);
	CHECK(!effect.Update());
	CHECK_EQ(7, effect.GetLight());
}

TEST(EffectBase, RemovingTheHostClearsDelayAndWaitDecisionsImmediately)
{
	World world;
	MEffect effect(BLT_EFFECT);
	effect.SetCount(100);
	effect.SetDelayFrame(100);
	effect.SetWaitFrame(100);
	MEffect::SetHost(nullptr);
	CHECK(effect.IsEnd());
	CHECK(!effect.IsDelayFrame());
	CHECK(!effect.IsWaitFrame());
	MEffect::SetHost(&host);
	CHECK(!effect.IsEnd());
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
}

TEST(EffectBase, LightingRetainsTheExistingCharNarrowing)
{
	World world;
	MEffect effect(BLT_EFFECT);
	lightValue = 255;
	effect.SetFrameID(1, 2);
	CHECK_EQ(static_cast<char>(255), effect.GetLight());
	lightValue = 256;
	effect.Update();
	CHECK_EQ(static_cast<char>(256), effect.GetLight());
}

TEST(EffectBase, LinkReplacementAndDestructionEachDeleteTheOwnedTarget)
{
	World world;
	int first = 0, second = 0;
	{
		MEffect effect(BLT_EFFECT);
		auto* a = new Target(first);
		effect.SetLink(12, a);
		CHECK(effect.GetEffectTarget() == a);
		CHECK_EQ(12, effect.GetActionInfo());
		auto* b = new Target(second);
		effect.SetLink(27, b);
		CHECK_EQ(1, first);
		CHECK_EQ(0, second);
		CHECK(effect.GetEffectTarget() == b);
		CHECK_EQ(27, effect.GetActionInfo());
	}
	CHECK_EQ(1, first);
	CHECK_EQ(1, second);
}

TEST(EffectBase, NullLinkClearsOwnershipAndStillUpdatesActionInfo)
{
	World world;
	int destroyed = 0;
	MEffect effect(BLT_EFFECT);
	effect.SetLink(1, new Target(destroyed));
	effect.SetLink(2, nullptr);
	CHECK_EQ(1, destroyed);
	CHECK(effect.GetEffectTarget() == nullptr);
	CHECK_EQ(2, effect.GetActionInfo());
	CHECK_EQ(0, effect.GetLinkSize());
	effect.SetLink(3, nullptr);
	CHECK_EQ(1, destroyed);
	CHECK_EQ(3, effect.GetActionInfo());
}

TEST(EffectBase, DetachingTransfersTargetOwnershipWithoutDeletingIt)
{
	World world;
	int destroyed = 0;
	auto* target = new Target(destroyed);
	{
		MEffect effect(BLT_EFFECT);
		effect.SetLink(17, target);
		effect.SetEffectTargetNULL();
		effect.SetEffectTargetNULL();
		CHECK(effect.GetEffectTarget() == nullptr);
		CHECK_EQ(17, effect.GetActionInfo());
		CHECK_EQ(0, destroyed);
	}
	CHECK_EQ(0, destroyed);
	delete target;
	CHECK_EQ(1, destroyed);
}

TEST(EffectBase, LinkSizeReportsCurrentPhaseUntilTheTargetEnds)
{
	World world;
	int destroyed = 0;
	MEffect effect(BLT_EFFECT);
	CHECK_EQ(0, effect.GetLinkSize());
	auto* target = new Target(destroyed);
	effect.SetLink(1, target);
	CHECK_EQ(0, effect.GetLinkSize());
	target->NextPhase();
	CHECK_EQ(1, effect.GetLinkSize());
	target->NextPhase();
	CHECK_EQ(2, effect.GetLinkSize());
	target->NextPhase();
	CHECK_EQ(0, effect.GetLinkSize());
	CHECK(effect.GetEffectTarget() == target);
	CHECK_EQ(0, destroyed);
}

TEST(EffectBase, DefaultConstructorInitializesMetadataInPoisonedStorage)
{
	World world;
	alignas(MEffect) unsigned char storage[sizeof(MEffect)];
	for (int pattern : {0x00, 0x55, 0xaa, 0xcc, 0xcd, 0xff})
	{
		std::memset(storage, pattern, sizeof(storage));
		auto* effect = new (storage) MEffect(BLT_EFFECT);
		CHECK_EQ(0, effect->GetEst());
		CHECK_EQ(0, effect->GetPower());
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		effect->~MEffect();
	}
}

TEST(EffectBase, ResourceConstructorInitializesMetadataInPoisonedStorage)
{
	World world;
	alignas(MEffect) unsigned char storage[sizeof(MEffect)];
	for (int pattern : {0x00, 0x55, 0xaa, 0xcc, 0xcd, 0xff})
	{
		std::memset(storage, pattern, sizeof(storage));
		auto* effect = new (storage) MEffect(BLT_EFFECT, nullptr);
		CHECK_EQ(0, effect->GetEst());
		CHECK_EQ(0, effect->GetPower());
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		effect->~MEffect();
	}
}

TEST(EffectBase, ReusingObjectStorageDoesNotInheritPreviousMetadata)
{
	World world;
	alignas(MEffect) unsigned char storage[sizeof(MEffect)];
	auto* previous = new (storage) MEffect(BLT_EFFECT);
	previous->SetEst(123);
	previous->SetPower(255);
	previous->SetLink(456, nullptr);
	previous->~MEffect();
	auto* effect = new (storage) MEffect(BLT_NORMAL, nullptr);
	CHECK_EQ(0, effect->GetEst());
	CHECK_EQ(0, effect->GetPower());
	CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
	effect->~MEffect();
}

TEST(EffectBase, RelinkingTheOwnedTargetUpdatesActionWithoutDeletingTheTarget)
{
	World world;
	int destroyed = 0;
	{
		MEffect effect(BLT_EFFECT);
		auto* target = new Target(destroyed);
		effect.SetLink(123, target);
		effect.SetLink(456, target);
		CHECK_EQ(0, destroyed);
		CHECK_EQ(456, effect.GetActionInfo());
		CHECK(effect.GetEffectTarget() == target);
		// The pre-fix implementation deletes target. Discard its dangling
		// alias so the failed ownership assertion does not double-delete.
		if (destroyed != 0) effect.SetEffectTargetNULL();
	}
	CHECK_EQ(1, destroyed);
}

TEST(EffectBase, OwningEffectsCannotBeImplicitlyCopied)
{
	CHECK(!std::is_copy_constructible_v<MEffect>);
	CHECK(!std::is_copy_assignable_v<MEffect>);
}

TEST(EffectBase, MaximumIntegerPixelsRemainRepresentableAfterFloatStorage)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	const int low = (std::numeric_limits<int>::min)();
	MEffect effect(BLT_EFFECT);
	effect.SetPixelPosition(high, high, high);
	CHECK_EQ(high, effect.GetPixelX());
	CHECK_EQ(high, effect.GetPixelY());
	CHECK_EQ(high, effect.GetPixelZ());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(high / TILE_X), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(high / TILE_Y), effect.GetY());
	effect.SetPixelPosition(low, low, low);
	CHECK_EQ(low, effect.GetPixelX());
	CHECK_EQ(low, effect.GetPixelY());
	CHECK_EQ(low, effect.GetPixelZ());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(low / TILE_X), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(low / TILE_Y), effect.GetY());
}

TEST(EffectBase, OversizedAndInfinitePixelsSaturateBeforeProjection)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	const int low = (std::numeric_limits<int>::min)();
	EffectProbe effect(BLT_EFFECT);
	effect.SetSubpixel(std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(), (std::numeric_limits<float>::max)());
	CHECK_EQ(high, effect.GetPixelX());
	CHECK_EQ(low, effect.GetPixelY());
	CHECK_EQ(high, effect.GetPixelZ());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(high / TILE_X), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(low / TILE_Y), effect.GetY());
	effect.SetSubpixel(-(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(), -std::numeric_limits<float>::infinity());
	CHECK_EQ(low, effect.GetPixelX());
	CHECK_EQ(high, effect.GetPixelY());
	CHECK_EQ(low, effect.GetPixelZ());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(low / TILE_X), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(high / TILE_Y), effect.GetY());
}

TEST(EffectBase, UnorderedPixelsProjectToTheOrigin)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	const float nan = std::numeric_limits<float>::quiet_NaN();
	effect.SetSubpixel(nan, nan, nan);
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetX());
	CHECK_EQ(0, effect.GetY());
}

TEST(EffectBase, PixelProjectionPreservesTheLargestInRangeFloatsAndHeightSetter)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	effect.SetSubpixel(2147483520.0f, -2147483520.0f, -0.75f);
	CHECK_EQ(2147483520, effect.GetPixelX());
	CHECK_EQ(-2147483520, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(2147483520 / TILE_X), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(-2147483520 / TILE_Y), effect.GetY());
	effect.SetZ((std::numeric_limits<int>::max)());
	CHECK_EQ((std::numeric_limits<int>::max)(), effect.GetPixelZ());
}
