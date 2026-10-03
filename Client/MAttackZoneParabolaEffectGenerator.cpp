//----------------------------------------------------------------------
// MAttackZoneParabolaEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttackZoneParabolaEffectGenerator.h"
#include "MParabolaEffect.h"
#include "WorldTileGeometry.h"
#include "SkillDef.h"
#include "MViewDef.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace {

int OffsetCoordinate(int coordinate, int offset)
{
	const auto value = static_cast<std::int64_t>(coordinate) + offset;
	return static_cast<int>(std::clamp<std::int64_t>(value,
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
}

} // namespace

const MZoneParabolaEffectHost* MAttackZoneParabolaEffectGenerator::s_pHost = nullptr;

const MZoneParabolaEffectHost* MAttackZoneParabolaEffectGenerator::SetHost(const MZoneParabolaEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAttackZoneParabolaEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MZoneParabolaEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAttackZoneParabolaEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MAttackZoneParabolaEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MZoneParabolaEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	auto effect = std::make_unique<MParabolaEffect>(sprite.bltType);
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));

	// The projectile endpoint extends one tile beyond the requested destination.
	const POINT offset = WorldTileGeometry::DirectionOffset(egInfo.direction);
	int tx = OffsetCoordinate(egInfo.x1, offset.x * TILE_X);
	int ty = OffsetCoordinate(egInfo.y1, offset.y * TILE_Y);

	if (egInfo.nActionInfo == SKILL_CANNONADE)
	{
		effect->SetPixelPosition(egInfo.x0, egInfo.y0, egInfo.z0);
		// Impact uses the original destination tile, before extension.
		effect->SetTargetTile(WorldTileGeometry::PixelToTileX(egInfo.x1),
			WorldTileGeometry::PixelToTileY(egInfo.y1));
	}
	else
		effect->SetPixelPosition(egInfo.x0, egInfo.y0, OffsetCoordinate(egInfo.z0, TILE_Y * 2));

	// Target selection overrides the supplied facing, as before.
	effect->SetDirection(egInfo.direction);
	effect->SetTarget(tx, ty, OffsetCoordinate(egInfo.z1, TILE_Y), egInfo.step);
	effect->SetCount(egInfo.count, egInfo.linkCount);
	effect->SetPower(egInfo.power);

	MParabolaEffect* submitted = effect.get();
	if (!QueueEffect(std::move(effect))) return false;
	submitted->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
	return true;
}
