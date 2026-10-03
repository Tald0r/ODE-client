// MAttackCreatureParabolaEffectGenerator.cpp
#include "Client_PCH.h"
#include "MAttackCreatureParabolaEffectGenerator.h"
#include "MParabolaEffect.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include <utility>

const MCreatureParabolaEffectHost* MAttackCreatureParabolaEffectGenerator::s_pHost = nullptr;

const MCreatureParabolaEffectHost* MAttackCreatureParabolaEffectGenerator::SetHost(const MCreatureParabolaEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAttackCreatureParabolaEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MCreatureParabolaEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAttackCreatureParabolaEffectGenerator::ReadCreature(TYPE_OBJECTID id, MCreatureParabolaPosition& position)
{
	position = {};
	return s_pHost && s_pHost->Creature && s_pHost->Creature(id, position);
}

bool MAttackCreatureParabolaEffectGenerator::ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count)
{
	count = 0;
	return s_pHost && s_pHost->MaxFrames && s_pHost->MaxFrames(blt, frameID, count);
}

bool MAttackCreatureParabolaEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MAttackCreatureParabolaEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	MCreatureParabolaEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	MCreatureParabolaPosition creature;
	if (!ReadCreature(egInfo.creatureID, creature)) return false;

	// Sample the creature's tile position before looking up animation length.
	const int cx = WorldTileGeometry::TileToPixelX(creature.x);
	const int cy = WorldTileGeometry::TileToPixelY(creature.y);
	const int cz = creature.z + TILE_Y;
	auto effect = std::make_unique<MParabolaEffect>(sprite.bltType);
	int maxFrame;
	if (!ReadMaxFrames(sprite.bltType, sprite.frameID, maxFrame)) return false;
	effect->SetFrameID(sprite.frameID, static_cast<BYTE>(maxFrame));
	effect->SetPixelPosition(egInfo.x0, egInfo.y0, egInfo.z0 + TILE_Y);
	// Target selection retains its calculated facing, overriding the input.
	effect->SetDirection(egInfo.direction);
	effect->SetTarget(cx, cy, cz, egInfo.step);
	effect->SetCount(egInfo.count, egInfo.linkCount);
	effect->SetPower(egInfo.power);

	MParabolaEffect* queued = effect.get();
	if (!QueueEffect(std::move(effect))) return false;
	queued->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
	return true;
}
