//----------------------------------------------------------------------
// MAttackZoneBombEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttackZoneBombEffectGenerator.h"
#include "MParabolaEffect.h"
#include "MViewDef.h"
#include <utility>

const MZoneBombEffectHost* MAttackZoneBombEffectGenerator::s_pHost = nullptr;

const MZoneBombEffectHost* MAttackZoneBombEffectGenerator::SetHost(const MZoneBombEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAttackZoneBombEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MZoneBombEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAttackZoneBombEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MAttackZoneBombEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MZoneBombEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	auto effect = std::make_unique<MParabolaEffect>(sprite.bltType);
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));

	// Bombs start two tiles higher and keep the requested destination unchanged.
	effect->SetPixelPosition(egInfo.x0, egInfo.y0, egInfo.z0 + (TILE_Y << 1));
	// Target selection overrides the supplied facing, as before.
	effect->SetDirection(egInfo.direction);
	effect->SetTarget(egInfo.x1, egInfo.y1, egInfo.z1, egInfo.step);
	effect->SetCount(egInfo.count, egInfo.linkCount);
	effect->SetPower(egInfo.power);

	MParabolaEffect* submitted = effect.get();
	if (!QueueEffect(std::move(effect))) return false;
	submitted->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
	return true;
}
