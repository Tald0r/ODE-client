//----------------------------------------------------------------------
// MStopInventoryEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MStopInventoryEffectGenerator.h"
#include "MScreenEffect.h"
#include "MScreenEffectManager.h"
#include <memory>
#include <cstdint>
#include <limits>

namespace {
int CenteredCoordinate(int cell, int itemCells, int cellPixels)
{
	const std::int64_t offset = (static_cast<std::int64_t>(itemCells) - 1) * cellPixels / 2;
	const std::int64_t position = static_cast<std::int64_t>(cell) + offset;
	if (position >= (std::numeric_limits<int>::max)())
		return (std::numeric_limits<int>::max)();
	if (position <= (std::numeric_limits<int>::min)())
		return (std::numeric_limits<int>::min)();
	return static_cast<int>(position);
}
} // namespace

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

	auto effect = std::make_unique<MScreenEffect>(sprite.bltType);
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
	effect->SetScreenBasis(placement.basis.x, placement.basis.y);
	// SetPosition projects to pixels, so assign screen coordinates afterwards.
	effect->SetPosition(egInfo.x1, egInfo.y1);
	effect->SetScreenPosition(
		CenteredCoordinate(placement.cell.x, placement.itemWidth, placement.cellWidth),
		CenteredCoordinate(placement.cell.y, placement.itemHeight, placement.cellHeight));
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
