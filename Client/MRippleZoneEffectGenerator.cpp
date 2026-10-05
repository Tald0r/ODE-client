// MRippleZoneEffectGenerator.cpp
#include "Client_PCH.h"
#include "MRippleZoneEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include <utility>

const MRippleZoneEffectHost* MRippleZoneEffectGenerator::s_pHost = nullptr;

const MRippleZoneEffectHost* MRippleZoneEffectGenerator::SetHost(const MRippleZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MRippleZoneEffectGenerator::ReadBounds(TYPE_SECTORPOSITION& width, TYPE_SECTORPOSITION& height)
{
	width = height = 0;
	return s_pHost && s_pHost->Bounds && s_pHost->Bounds(width, height);
}

bool MRippleZoneEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MRippleZoneEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect, bool ground)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect), ground);
}

bool MRippleZoneEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	TYPE_SECTORPOSITION x = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	TYPE_SECTORPOSITION y = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));
	WorldTileGeometry::Step(x, y, egInfo.direction);

	TYPE_SECTORPOSITION width, height;
	if (!ReadBounds(width, height) || x >= width || y >= height) return false;

	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	auto effect = std::make_unique<MEffect>(sprite.bltType);
	MEffect* pEffect = effect.get();
	pEffect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
	pEffect->SetPosition(x, y);
	pEffect->SetDirection(egInfo.direction);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount(egInfo.count, egInfo.linkCount);
	pEffect->SetPower(egInfo.power);

	const bool ground = egInfo.effectSpriteType == EFFECTSPRITETYPE_EARTHQUAKE_1
		|| egInfo.effectSpriteType == EFFECTSPRITETYPE_EARTHQUAKE_2
		|| egInfo.effectSpriteType == EFFECTSPRITETYPE_EARTHQUAKE_3
		|| egInfo.effectSpriteType == EFFECTSPRITETYPE_POWER_OF_LAND_STONE_1
		|| egInfo.effectSpriteType == EFFECTSPRITETYPE_POWER_OF_LAND_STONE_2
		|| egInfo.effectSpriteType == EFFECTSPRITETYPE_POWER_OF_LAND_STONE_3;
	const bool accepted = QueueEffect(std::move(effect), ground);
	if (accepted) pEffect->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
	return accepted;
}
