// MAttachZoneSelectableEffectGenerator.cpp
#include "Client_PCH.h"
#include "MAttachZoneSelectableEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

const MFixedZoneEffectHost* MAttachZoneSelectableEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MAttachZoneSelectableEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAttachZoneSelectableEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAttachZoneSelectableEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

int MAttachZoneSelectableEffectGenerator::OffsetCoordinate(int coordinate, int offset)
{
	return static_cast<int>(std::clamp<std::int64_t>(static_cast<std::int64_t>(coordinate) + offset,
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
}

bool MAttachZoneSelectableEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	int est = egInfo.effectSpriteType;
	int x = egInfo.x0, y = egInfo.y0;
	if (est == EFFECTSPRITETYPE_BLOOD_GROUND_2_1)
	{
		est += std::rand() % 4;
		x = OffsetCoordinate(x, std::rand() % 24 - 12);
		y = OffsetCoordinate(y, std::rand() % 24 - 12);
	}
	else if (est == EFFECTSPRITETYPE_BLOOD_GROUND_1_1)
	{
		est += std::rand() % 5;
		x = OffsetCoordinate(x, std::rand() % 24 - 12);
		y = OffsetCoordinate(y, std::rand() % 24 - 12);
	}
	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return false;
	auto effect = std::make_unique<MSelectableEffect>(sprite.bltType);
	MEffect* pEffect = effect.get();
	pEffect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
	pEffect->SetPixelPosition(x, y, egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount(egInfo.count, egInfo.linkCount);
	pEffect->SetDirection(egInfo.direction);
	pEffect->SetPower(egInfo.power);
	if (!QueueEffect(std::move(effect))) return false;
	pEffect->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
	return true;
}
