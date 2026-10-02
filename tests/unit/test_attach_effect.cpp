#include "test_framework.h"
#include "MAttachEffect.h"
#include "MViewDef.h"

#include <limits>
#include <vector>

namespace {

DWORD frameNow;
bool spriteAvailable, liveAvailable, fakeAvailable, corpseAvailable, directAvailable;
MAttachEffectSprite spriteInfo;
MAttachCreaturePosition livePosition, fakePosition, corpsePosition, directPosition;
std::vector<int> calls;
std::vector<TYPE_OBJECTID> lookups;
std::vector<TYPE_EFFECTSPRITETYPE> spriteRequests;
struct LightRequest
{
	BYTE blt;
	TYPE_FRAMEID id;
	BYTE direction, frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
int nextLight;
// The model only passes this identity to the reader; it never dereferences it.
int creatureIdentity;
MCreature* Creature() { return reinterpret_cast<MCreature*>(&creatureIdentity); }
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(3);
		lights.push_back({blt, id, direction, frame});
		return nextLight++;
	},
};
const MAttachEffectHost attachHost{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAttachEffectSprite& sprite) {
		calls.push_back(2); spriteRequests.push_back(type);
		if (!spriteAvailable) return false;
		sprite = spriteInfo;
		return true;
	},
	.LiveCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
		calls.push_back(4); lookups.push_back(id);
		if (!liveAvailable) return false;
		position = livePosition;
		return true;
	},
	.FakeCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
		calls.push_back(5); lookups.push_back(id);
		if (!fakeAvailable) return false;
		position = fakePosition;
		return true;
	},
	.CorpseCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
		calls.push_back(6); lookups.push_back(id);
		if (!corpseAvailable) return false;
		position = corpsePosition;
		return true;
	},
	.CreaturePosition = [](const MCreature* creature, MAttachCreaturePosition& position) {
		calls.push_back(7);
		CHECK(creature == Creature());
		if (!directAvailable) return false;
		position = directPosition;
		return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* previousAttach = MAttachEffect::SetHost(&attachHost);
	World()
	{
		frameNow = 100; nextLight = 7;
		spriteAvailable = true; liveAvailable = true;
		fakeAvailable = corpseAvailable = directAvailable = true;
		spriteInfo = {BLT_EFFECT, 12, 3};
		livePosition = {101, 96, 48, 12};
		fakePosition = {202, 144, 72, 24};
		corpsePosition = {303, 192, 96, -12};
		directPosition = {404, 240, 120, 36};
		calls.clear(); lights.clear(); lookups.clear(); spriteRequests.clear();
	}
	~World()
	{
		MAttachEffect::SetHost(previousAttach);
		MEffect::SetHost(previousEffect);
	}
};

void ClearCalls() { calls.clear(); lights.clear(); lookups.clear(); }

void At(MAttachEffect& effect, TYPE_OBJECTID id, int x, int y, int z)
{
	CHECK_EQ(id, effect.GetAttachCreatureID());
	CHECK_EQ(x, effect.GetPixelX());
	CHECK_EQ(y, effect.GetPixelY());
	CHECK_EQ(z, effect.GetPixelZ());
}

} // namespace

TEST(AttachEffect, ConstructorResolvesSpriteAndInitializesAttachedState)
{
	World world;
	MAttachEffect effect(17, 10);
	CHECK_EQ(MEffect::EFFECT_ATTACH, effect.GetEffectType());
	CHECK_EQ(OBJECTID_NULL, effect.GetAttachCreatureID());
	CHECK(effect.IsEffectSprite());
	CHECK(!effect.IsEffectColor());
	CHECK_EQ(17, effect.GetEffectSpriteType());
	CHECK_EQ(ADDON_NULL, effect.GetEffectColorPart());
	CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(12, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(109, effect.GetEndFrame());
	CHECK_EQ(109, effect.GetEndLinkFrame());
	CHECK_EQ(8, effect.GetLight());
	CHECK(spriteRequests == std::vector<TYPE_EFFECTSPRITETYPE>({17}));
	CHECK(calls == std::vector<int>({2, 1, 3, 3}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_LEFT, 0},
		{BLT_EFFECT, 12, DIRECTION_LEFT, 0}}));
}

TEST(AttachEffect, NonAlphaConstructorAndUpdatesKeepTheirLightingPolicy)
{
	World world;
	for (const auto blt : {BLT_NORMAL, BLT_SHADOW, BLT_SCREEN})
	{
		spriteInfo.bltType = static_cast<BYTE>(blt);
		ClearCalls();
		MAttachEffect effect(17, 10);
		CHECK_EQ(blt, effect.GetBltType());
		CHECK_EQ(0, effect.GetLight());
		CHECK(calls == std::vector<int>({2, 1, 3}));
		effect.SetLight(19);
		ClearCalls();
		CHECK(effect.Update());
		CHECK_EQ(19, effect.GetLight());
		CHECK_EQ(1, effect.GetFrame());
		CHECK(calls == std::vector<int>({1}));
	}
}

TEST(AttachEffect, UnknownSpriteUsesTheLegacyFallbackWithoutChangingItsType)
{
	World world;
	spriteAvailable = false;
	MAttachEffect effect(65535, 10);
	CHECK_EQ(65535, effect.GetEffectSpriteType());
	CHECK(effect.IsEffectSprite());
	CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(0, effect.GetFrameID());
	CHECK_EQ(1, effect.GetMaxFrame());
	CHECK_EQ(0, effect.GetLight());
	CHECK_EQ(109, effect.GetEndFrame());
	CHECK(calls == std::vector<int>({2, 1, 3}));
}

TEST(AttachEffect, MissingSpriteHostOrEntryUsesTheUnknownTypeDefaults)
{
	World world;
	const MAttachEffectHost empty{};
	for (const auto* host : {static_cast<const MAttachEffectHost*>(nullptr), &empty})
	{
		MAttachEffect::SetHost(host);
		MAttachEffect effect(17, 10);
		CHECK_EQ(BLT_EFFECT, effect.GetBltType());
		CHECK_EQ(0, effect.GetFrameID());
		CHECK_EQ(1, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetLight());
		CHECK_EQ(109, effect.GetEndFrame());
	}
	CHECK(spriteRequests.empty());
}

TEST(AttachEffect, SpriteMetadataIsRefreshedForEachConstructor)
{
	World world;
	MAttachEffect first(17, 10);
	spriteInfo = {BLT_NORMAL, 23, 258};
	MAttachEffect second(18, 10);
	CHECK_EQ(12, first.GetFrameID());
	CHECK_EQ(3, first.GetMaxFrame());
	CHECK_EQ(23, second.GetFrameID());
	CHECK_EQ(2, second.GetMaxFrame());
	CHECK_EQ(BLT_NORMAL, second.GetBltType());
	CHECK(spriteRequests == std::vector<TYPE_EFFECTSPRITETYPE>({17, 18}));
}

TEST(AttachEffect, PermanentDurationAndIndependentLinkDeadlineRemainDistinct)
{
	World world;
	MAttachEffect permanent(17, 0xFFFF);
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), permanent.GetEndFrame());
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), permanent.GetEndLinkFrame());
	MAttachEffect shortLink(17, 0xFFFF, 5);
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), shortLink.GetEndFrame());
	CHECK_EQ(104, shortLink.GetEndLinkFrame());
	MAttachEffect finite(17, 10, 20);
	CHECK_EQ(109, finite.GetEndFrame());
	CHECK_EQ(119, finite.GetEndLinkFrame());
	MAttachEffect longFinite(17, 65536);
	CHECK_EQ(65635, longFinite.GetEndFrame());
}

TEST(AttachEffect, ActiveUpdateAnimatesAndLightsWithoutLookingUpOrMovingACreature)
{
	World world;
	MAttachEffect effect(17, 10);
	effect.SetAttachCreatureID(7);
	ClearCalls();
	livePosition = {101, 300, 400, 50};
	CHECK(effect.Update());
	At(effect, 101, 96, 48, 12);
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetY());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(9, effect.GetLight());
	CHECK(calls == std::vector<int>({1, 3}));
	CHECK(lookups.empty());
}

TEST(AttachEffect, ExpiryPreservesFrameLightAndPosition)
{
	World world;
	MAttachEffect effect(17, 10);
	effect.SetAttachCreatureID(7);
	frameNow = 109;
	ClearCalls();
	CHECK(!effect.Update());
	At(effect, 101, 96, 48, 12);
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(8, effect.GetLight());
	CHECK(calls == std::vector<int>({1}));
}

TEST(AttachEffect, LiveCreatureTakesPriorityOverFakeAndCorpseMatches)
{
	World world;
	MAttachEffect effect(17, 10);
	ClearCalls();
	effect.SetAttachCreatureID(7);
	At(effect, 101, 96, 48, 12);
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetY());
	CHECK_EQ(109, effect.GetEndFrame());
	CHECK(calls == std::vector<int>({4}));
	CHECK(lookups == std::vector<TYPE_OBJECTID>({7}));
}

TEST(AttachEffect, FakeCreatureIsUsedBeforeTheCorpseFallback)
{
	World world;
	MAttachEffect effect(17, 10);
	liveAvailable = false;
	ClearCalls();
	effect.SetAttachCreatureID(7);
	At(effect, 202, 144, 72, 24);
	CHECK(calls == std::vector<int>({4, 5}));
	CHECK(lookups == std::vector<TYPE_OBJECTID>({7, 7}));
}

TEST(AttachEffect, CorpseCreatureIsTheLastLookupAndSuppliesItsOwnID)
{
	World world;
	MAttachEffect effect(17, 10);
	liveAvailable = fakeAvailable = false;
	ClearCalls();
	effect.SetAttachCreatureID(7);
	At(effect, 303, 192, 96, -12);
	CHECK(calls == std::vector<int>({4, 5, 6}));
	CHECK(lookups == std::vector<TYPE_OBJECTID>({7, 7, 7}));
}

TEST(AttachEffect, MissingTargetEndsLifetimeButKeepsPriorIdentityAndPosition)
{
	World world;
	MAttachEffect effect(17, 10, 20);
	effect.SetAttachCreatureID(7);
	liveAvailable = fakeAvailable = corpseAvailable = false;
	ClearCalls();
	effect.SetAttachCreatureID(8);
	At(effect, 101, 96, 48, 12);
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(119, effect.GetEndLinkFrame());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(calls == std::vector<int>({4, 5, 6}));
	CHECK(lookups == std::vector<TYPE_OBJECTID>({8, 8, 8}));
}

TEST(AttachEffect, ExplicitCreatureReaderAppliesPositionWithoutZoneLookup)
{
	World world;
	MAttachEffect effect(17, 10);
	ClearCalls();
	CHECK(effect.SetAttachCreature(Creature()));
	At(effect, 404, 240, 120, 36);
	CHECK_EQ(5, effect.GetX());
	CHECK_EQ(5, effect.GetY());
	CHECK_EQ(109, effect.GetEndFrame());
	CHECK(calls == std::vector<int>({7}));
	CHECK(lookups.empty());
	directPosition = {405, 288, 144, 42};
	CHECK(effect.SetAttachCreature(Creature()));
	At(effect, 405, 288, 144, 42);
}

TEST(AttachEffect, NullCreatureStopsWithoutCallingThePositionReader)
{
	World world;
	MAttachEffect effect(17, 10);
	effect.SetAttachCreatureID(7);
	ClearCalls();
	CHECK(!effect.SetAttachCreature(nullptr));
	At(effect, 101, 96, 48, 12);
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(109, effect.GetEndLinkFrame());
	CHECK(calls.empty());
}

TEST(AttachEffect, FailedDirectReaderDoesNotApplyItsPosition)
{
	World world;
	MAttachEffect effect(17, 10);
	effect.SetAttachCreatureID(7);
	directAvailable = false;
	ClearCalls();
	CHECK(!effect.SetAttachCreature(Creature()));
	At(effect, 101, 96, 48, 12);
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK(calls == std::vector<int>({7}));
}

TEST(AttachEffect, AttachingToAnExpiredEffectDoesNotRestartItsLifetime)
{
	World world;
	MAttachEffect effect(17, 1);
	CHECK(!effect.Update());
	CHECK(effect.SetAttachCreature(Creature()));
	At(effect, 404, 240, 120, 36);
	CHECK_EQ(100, effect.GetEndFrame());
	CHECK(!effect.Update());
}

TEST(AttachEffect, APartialHostCanSupplyOnlyTheCorpseFallback)
{
	World world;
	MAttachEffect effect(17, 10);
	const MAttachEffectHost corpseOnly{.CorpseCreature = attachHost.CorpseCreature};
	MAttachEffect::SetHost(&corpseOnly);
	ClearCalls();
	effect.SetAttachCreatureID(7);
	At(effect, 303, 192, 96, -12);
	CHECK(calls == std::vector<int>({6}));
}

TEST(AttachEffect, FailedLiveLookupCanReplaceTheRemainingLookupServices)
{
	World world;
	MAttachEffect effect(17, 10);
	const MAttachEffectHost changing{
		.LiveCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
			CHECK_EQ(7, id);
			position = {999, -100, -200, -300};
			MAttachEffect::SetHost(&attachHost);
			return false;
		},
	};
	CHECK(MAttachEffect::SetHost(&changing) == &attachHost);
	ClearCalls();
	effect.SetAttachCreatureID(7);
	At(effect, 202, 144, 72, 24);
	CHECK(calls == std::vector<int>({5}));
	CHECK(MAttachEffect::SetHost(&changing) == &attachHost);
}

TEST(AttachEffect, MissingWorldServicesStopBothAttachmentEntryPoints)
{
	World world;
	const MAttachEffectHost empty{};
	for (const auto* host : {static_cast<const MAttachEffectHost*>(nullptr), &empty})
	{
		MAttachEffect::SetHost(&attachHost);
		MAttachEffect byID(17, 10);
		MAttachEffect direct(17, 10);
		byID.SetAttachCreatureID(7);
		direct.SetAttachCreatureID(7);
		MAttachEffect::SetHost(host);
		ClearCalls();
		byID.SetAttachCreatureID(8);
		CHECK(!direct.SetAttachCreature(Creature()));
		At(byID, 101, 96, 48, 12);
		At(direct, 101, 96, 48, 12);
		CHECK_EQ(0, byID.GetEndFrame());
		CHECK_EQ(0, direct.GetEndFrame());
		CHECK(calls.empty());
	}
}

TEST(AttachEffect, NullObjectIDStillUsesTheOrderedLookup)
{
	World world;
	MAttachEffect effect(17, 10);
	liveAvailable = fakeAvailable = corpseAvailable = false;
	ClearCalls();
	effect.SetAttachCreatureID(OBJECTID_NULL);
	CHECK(lookups == std::vector<TYPE_OBJECTID>({OBJECTID_NULL, OBJECTID_NULL, OBJECTID_NULL}));
	CHECK_EQ(0, effect.GetEndFrame());
}

TEST(AttachEffect, SpriteAndColorSettersDoNotReloadFramesOrResetLifetime)
{
	World world;
	MAttachEffect effect(17, 10);
	effect.SetEffectColor(65000);
	CHECK(effect.IsEffectColor());
	CHECK(!effect.IsEffectSprite());
	CHECK_EQ(65000, effect.GetEffectColor());
	effect.SetEffectColorPart(static_cast<ADDON>(3));
	CHECK_EQ(3, effect.GetEffectColorPart());
	effect.SetEffectSprite(18);
	CHECK(effect.IsEffectSprite());
	CHECK(!effect.IsEffectColor());
	CHECK_EQ(18, effect.GetEffectSpriteType());
	CHECK_EQ(12, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(109, effect.GetEndFrame());
	CHECK_EQ(3, effect.GetEffectColorPart());
	CHECK(spriteRequests == std::vector<TYPE_EFFECTSPRITETYPE>({17}));
}

TEST(AttachEffect, MissingClockUsesZeroForStoredLifetimeAndStopsUpdates)
{
	World world;
	MEffect::SetHost(nullptr);
	MAttachEffect effect(17, 10, 5);
	CHECK_EQ(9, effect.GetEndFrame());
	CHECK_EQ(4, effect.GetEndLinkFrame());
	CHECK_EQ(0, effect.GetLight());
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	MAttachEffect permanent(17, 0xFFFF);
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), permanent.GetEndFrame());
	CHECK(!permanent.Update());
	MEffect::SetHost(&effectHost);
	CHECK(permanent.Update());
}

TEST(AttachEffect, AClockOnlyBaseHostAllowsAnimationWithoutLight)
{
	World world;
	const MEffectHost clockOnly{.CurrentFrame = effectHost.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	MAttachEffect effect(17, 10);
	CHECK_EQ(0, effect.GetLight());
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(0, effect.GetLight());
	CHECK(lights.empty());
}

TEST(AttachEffect, FiniteLifetimeRetainsAbsoluteUnsignedClockWrap)
{
	World world;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	MAttachEffect effect(17, 4);
	CHECK_EQ(1, effect.GetEndFrame());
	CHECK(!effect.Update());
	frameNow = 0;
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	frameNow = 1;
	CHECK(!effect.Update());
}

TEST(AttachEffect, ZeroDurationAndPermanentDeadlinesRetainTheirBoundaryRules)
{
	World world;
	MAttachEffect expired(17, 0);
	CHECK_EQ(99, expired.GetEndFrame());
	CHECK(!expired.Update());
	frameNow = 0;
	MAttachEffect wrapped(17, 0);
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), wrapped.GetEndFrame());
	CHECK(wrapped.Update());
	frameNow = (std::numeric_limits<DWORD>::max)();
	MAttachEffect permanent(17, 0xFFFF);
	CHECK(!permanent.Update());
}

TEST(AttachEffect, PermanentSentinelAppliesOnlyToAttachedConstruction)
{
	World world;
	MAttachEffect effect(17, 0xFFFF);
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), effect.GetEndFrame());
	effect.SetCount(0xFFFF);
	CHECK_EQ(65634, effect.GetEndFrame());
	CHECK_EQ(65634, effect.GetEndLinkFrame());
}

TEST(AttachEffect, LinkDelayWaitAndDrawSkipDoNotGateAnimation)
{
	World world;
	MAttachEffect effect(17, 10, 1);
	effect.SetDelayFrame(100);
	effect.SetWaitFrame(100);
	effect.SetDrawSkip(true);
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(100, effect.GetEndLinkFrame());
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	CHECK(effect.IsSkipDraw());
}

TEST(AttachEffect, UpdateDoesNotReprojectUnrefreshedPosition)
{
	World world;
	struct Effect : MAttachEffect
	{
		Effect() : MAttachEffect(17, 10) {}
		void ChangePixels() { m_PixelX = 240; m_PixelY = 120; }
	} effect;
	effect.SetPixelPosition(96, 48, 0);
	effect.ChangePixels();
	CHECK(effect.Update());
	CHECK_EQ(240, effect.GetPixelX());
	CHECK_EQ(120, effect.GetPixelY());
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetY());
}

TEST(AttachEffect, ConstructorKeepsOneSpriteSnapshotWhenItsHostChanges)
{
	World world;
	const MAttachEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MAttachEffectSprite& sprite) {
			sprite = {BLT_NORMAL, 20, 7};
			MAttachEffect::SetHost(&attachHost);
			return true;
		},
	};
	MAttachEffect::SetHost(&changing);
	MAttachEffect first(17, 10);
	MAttachEffect second(18, 10);
	CHECK_EQ(BLT_NORMAL, first.GetBltType());
	CHECK_EQ(20, first.GetFrameID());
	CHECK_EQ(7, first.GetMaxFrame());
	CHECK_EQ(BLT_EFFECT, second.GetBltType());
	CHECK_EQ(12, second.GetFrameID());
	CHECK_EQ(3, second.GetMaxFrame());
}

TEST(AttachEffect, ResolvedIntegerLimitCoordinatesUseTheBoundedBaseProjection)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	livePosition = {101, high, low, high};
	MAttachEffect effect(17, 10);
	effect.SetAttachCreatureID(7);
	At(effect, 101, high, low, high);
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(high / TILE_X), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(low / TILE_Y), effect.GetY());
}
