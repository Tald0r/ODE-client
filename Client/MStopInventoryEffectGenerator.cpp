//----------------------------------------------------------------------
// MStopInventoryEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MStopInventoryEffectGenerator.h"
#include "MScreenEffect.h"
#include "MScreenEffectManager.h"
#include <memory>

const MInventoryEffectHost* MStopInventoryEffectGenerator::s_pHost = nullptr;

const MInventoryEffectHost* MStopInventoryEffectGenerator::SetHost(const MInventoryEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopInventoryEffectGenerator::ReadPlacement(int x, int y, MInventoryEffectPlacement& placement)
{
	placement = {};
	return s_pHost && s_pHost->Placement && s_pHost->Placement(x, y, placement);
}

bool MStopInventoryEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MInventoryEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

MScreenEffectManager* MStopInventoryEffectGenerator::ReadManager()
{
	return s_pHost && s_pHost->Manager ? s_pHost->Manager() : nullptr;
}

bool MStopInventoryEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MInventoryEffectPlacement placement;
	MInventoryEffectSprite sprite;
	if (!ReadPlacement(egInfo.x1, egInfo.y1, placement) ||
		!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	MScreenEffectManager* manager = ReadManager();
	if (!manager) return false;

	const POINT gridPlus{
		(placement.itemWidth - 1) * placement.cellWidth / 2,
		(placement.itemHeight - 1) * placement.cellHeight / 2,
	};
	auto effect = std::make_unique<MScreenEffect>(sprite.bltType);
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
	effect->SetScreenBasis(placement.basis.x, placement.basis.y);
	// SetPosition projects to pixels, so assign screen coordinates afterwards.
	effect->SetPosition(egInfo.x1, egInfo.y1);
	effect->SetScreenPosition(placement.cell.x + gridPlus.x, placement.cell.y + gridPlus.y);
	effect->SetStepPixel(0);
	effect->SetCount(egInfo.count, egInfo.linkCount);
	effect->SetDirection(0);
	effect->SetPower(egInfo.power);

	MScreenEffect* queued = effect.get();
	manager->AddEffect(queued);
	effect.release();
	queued->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
	return true;
}
