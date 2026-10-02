//----------------------------------------------------------------------
// MFallingEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MFallingEffectGenerator.h"
#include "MLinearEffect.h"
#include <limits>
#include <utility>

const MFallingEffectHost* MFallingEffectGenerator::s_pHost = nullptr;

const MFallingEffectHost* MFallingEffectGenerator::SetHost(const MFallingEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MFallingEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFallingEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MFallingEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MFallingEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MFallingEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;

	auto effect = std::make_unique<MLinearEffect>(sprite.bltType);
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
	// Start above the destination; the source coordinates are not used.
	const int top = (std::numeric_limits<int>::max)();
	const int startZ = egInfo.z1 > top - 300 ? top : egInfo.z1 + 300;
	effect->SetPixelPosition(egInfo.x1, egInfo.y1, startZ);
	effect->SetDirection(egInfo.direction);
	// Linear target selection retains its own facing calculation.
	effect->SetTarget(egInfo.x1, egInfo.y1, egInfo.z1, egInfo.step);
	effect->SetCount(egInfo.count, egInfo.linkCount);
	effect->SetPower(egInfo.power);

	MLinearEffect* queued = effect.get();
	if (!QueueEffect(std::move(effect))) return false;
	queued->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
	return true;
}
