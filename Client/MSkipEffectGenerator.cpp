// Generate blinking effects aligned with the ground tiles.
#include "Client_PCH.h"
#include "MSkipEffectGenerator.h"
#include "MSkipEffect.h"
#include "WorldTileGeometry.h"
#include <optional>
#include <utility>

const MFixedZoneEffectHost* MSkipEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MSkipEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MSkipEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MSkipEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MSkipEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	const int grade = egInfo.temp1;
	if (grade > 3) return false;
	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;

	// Preserve the ordered line, cross and square patterns. Grade zero uses
	// the supplied pixel coordinates without snapping them to a tile.
	static constexpr int offsets[4][9][2] = {
		{{0,0}},
		{{-1,0},{0,0},{1,0}},
		{{-1,0},{0,-1},{1,0},{0,1},{0,0}},
		{{-1,-1},{-1,0},{-1,1},{0,-1},{0,0},{0,1},{1,-1},{1,0},{1,1}},
	};
	static constexpr int counts[] = {1,3,5,9};
	const int tx = WorldTileGeometry::PixelToTileX(egInfo.x0);
	const int ty = WorldTileGeometry::PixelToTileY(egInfo.y0);

	// Capture visual state before transferring the original. A later queue
	// call can retire an earlier effect, including its target and result.
	std::optional<MEffectTarget> continuation;
	if (grade != 0 && egInfo.pEffectTarget) continuation.emplace(*egInfo.pEffectTarget);
	bool accepted = false;
	for (int i = 0; i < counts[grade]; ++i)
	{
		auto effect = std::make_unique<MSkipEffect>(sprite.bltType);
		MEffect* retained = effect.get();
		effect->SetFrameID(sprite.frameID, static_cast<BYTE>(sprite.maxFrames));
		// Pixel-to-tile division leaves room for +/-1; tile-to-pixel conversion
		// saturates at the int endpoints for the outermost patterns.
		const int x = grade == 0 ? egInfo.x0 : WorldTileGeometry::TileToPixelX(tx + offsets[grade][i][0]);
		const int y = grade == 0 ? egInfo.y0 : WorldTileGeometry::TileToPixelY(ty + offsets[grade][i][1]);
		effect->SetPixelPosition(x, y, egInfo.z0);
		effect->SetStepPixel(egInfo.step);
		effect->SetCount(egInfo.count, egInfo.linkCount);
		effect->SetDirection(egInfo.direction);
		effect->SetPower(egInfo.power);

		// Allocate before submission so exceptions leave no retained, unlinked
		// continuation. Both objects stay locally owned on queue rejection.
		std::unique_ptr<MEffectTarget> copy;
		if (accepted && continuation) copy = std::make_unique<MEffectTarget>(*continuation);
		if (!QueueEffect(std::move(effect))) continue;
		if (!accepted) retained->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
		else if (copy) retained->SetLink(egInfo.nActionInfo, copy.release());
		accepted = true;
	}
	return accepted;
}
