// MAttackZoneRectEffectGenerator.cpp
#include "Client_PCH.h"
#include "MAttackZoneRectEffectGenerator.h"
#include "MEffect.h"
#include "MLinearEffect.h"
#include "WorldTileGeometry.h"
#include <utility>

const MFixedZoneEffectHost* MAttackZoneRectEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MAttackZoneRectEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAttackZoneRectEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAttackZoneRectEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MAttackZoneRectEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const bool burst = egInfo.pEffectTarget != nullptr && egInfo.pEffectTarget->GetCurrentPhase() == 2;
	const int count = burst ? 8 : 1;
	bool accepted = false;
	for (int i = 0; i < count; ++i)
	{
		auto effect = std::make_unique<MLinearEffect>(sprite.bltType);
		MLinearEffect* pEffect = effect.get();
		pEffect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
		if (burst)
		{
			pEffect->SetPixelPosition(egInfo.x1, egInfo.y1, 0);
			int x = WorldTileGeometry::PixelToTileX(egInfo.x1);
			int y = WorldTileGeometry::PixelToTileY(egInfo.y1);
			const POINT offset = WorldTileGeometry::DirectionOffset(i);
			x += static_cast<int>(offset.x);
			y += static_cast<int>(offset.y);
			pEffect->SetTarget(WorldTileGeometry::TileToPixelX(x), WorldTileGeometry::TileToPixelY(y), 0, egInfo.step);
			pEffect->SetDirection(static_cast<BYTE>(i));
		}
		else
		{
			pEffect->SetPixelPosition(egInfo.x0, egInfo.y0, 0);
			pEffect->SetTarget(egInfo.x1, egInfo.y1, egInfo.z0, egInfo.step);
			pEffect->SetDirection(egInfo.direction);
		}
		pEffect->SetCount(egInfo.count, egInfo.linkCount);
		pEffect->SetPower(egInfo.power);
		if (QueueEffect(std::move(effect)))
		{
			pEffect->SetLink(egInfo.nActionInfo, accepted ? nullptr : egInfo.pEffectTarget);
			accepted = true;
		}
	}
	return accepted;
}
