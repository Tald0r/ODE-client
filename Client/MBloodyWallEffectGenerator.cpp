// MBloodyWallEffectGenerator.cpp
#include "Client_PCH.h"
#include "MBloodyWallEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include <cstdlib>
#include <iterator>
#include <utility>

const MBloodyWallEffectHost* MBloodyWallEffectGenerator::s_pHost = nullptr;

const MBloodyWallEffectHost* MBloodyWallEffectGenerator::SetHost(const MBloodyWallEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MBloodyWallEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MBloodyWallEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MBloodyWallEffectGenerator::ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count)
{
	count = 0;
	return s_pHost && s_pHost->MaxFrames && s_pHost->MaxFrames(blt, frameID, count);
}

bool MBloodyWallEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool MBloodyWallEffectGenerator::Generate(const EFFECTGENERATOR_INFO& egInfo)
{
	bool bOK = false;
	int est = egInfo.effectSpriteType;

	// Start Bloody Wall variants at a random member of the three-frame family.
	if (est >= EFFECTSPRITETYPE_BLOODY_WALL_1 && est <= EFFECTSPRITETYPE_BLOODY_WALL_3)
		est = EFFECTSPRITETYPE_BLOODY_WALL_1 + std::rand() % 3;

	MBloodyWallEffectSprite sprite;
	if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return false;
	const BYTE bltType = sprite.bltType;
	TYPE_FRAMEID frameID = sprite.frameID;
	const bool repeatFrame = sprite.repeatFrame;
	const int tx = egInfo.x1, ty = egInfo.y1;
	const int lookDirection = egInfo.direction;

	const POINT dirValue[8][5] =
	{
		{ { 0, -2 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } },	// left
		{ { -1, -1 }, { -1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 } },		// leftdown
		{ { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 } },			// down
		{ { 1, -1 }, { 1, 0 }, { 0, 0 }, { 0, 1 }, { -1, 1 } },			// rightdown
		{ { 0, -2 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } },			// right
		{ { 1, 1 }, { 1, 0 }, { 0, 0 }, { 0, -1 }, { -1, -1 } },		// rightup
		{ { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 } },	// up
		{ { 1, -1 }, { 0, -1 }, { 0, 0 }, { -1, 0 }, { -1, 1 } },	// leftup
	};

	const TYPE_SECTORPOSITION tX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	const TYPE_SECTORPOSITION tY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));
	const int z = egInfo.z0;
	int maxFrame;
	if (!ReadMaxFrames(bltType, frameID, maxFrame)) return false;
	if (lookDirection >= static_cast<int>(std::size(dirValue))) return false;

	for (int i = 0; i < 5; ++i)
	{
		const int sX = tX + dirValue[lookDirection][i].x;
		const int sY = tY + dirValue[lookDirection][i].y;
		const int sx = tx + dirValue[lookDirection][i].x * TILE_X;
		const int sy = ty + dirValue[lookDirection][i].y * TILE_Y;
		auto effect = std::make_unique<MEffect>(bltType);
		MEffect* pEffect = effect.get();
		pEffect->SetFrameID(frameID, static_cast<BYTE>(maxFrame));
		pEffect->SetPosition(static_cast<TYPE_SECTORPOSITION>(sX), static_cast<TYPE_SECTORPOSITION>(sY));
		pEffect->SetZ(z);
		pEffect->SetStepPixel(egInfo.step);
		pEffect->SetCount(egInfo.count, egInfo.linkCount);
		pEffect->SetDirection(egInfo.direction);
		pEffect->SetPower(egInfo.power);

		const bool bAdd = QueueEffect(std::move(effect));
		if (bAdd)
		{
			if (!bOK)
			{
				pEffect->SetLink(egInfo.nActionInfo, egInfo.pEffectTarget);
				bOK = true;
			}
			else if (egInfo.pEffectTarget == nullptr)
			{
				pEffect->SetLink(egInfo.nActionInfo, nullptr);
			}
			else
			{
				auto* copy = new MEffectTarget(*egInfo.pEffectTarget);
				pEffect->SetLink(egInfo.nActionInfo, copy);
				copy->Set(sx, sy, z, egInfo.creatureID);
			}
		}

		// Randomize the starting animation only for retained repeating effects.
		if (bAdd && repeatFrame)
		{
			const int num = std::rand() % maxFrame;
			for (int nf = 0; nf < num; ++nf) pEffect->NextFrame();
		}

		if (est >= EFFECTSPRITETYPE_BLOODY_WALL_1 && est <= EFFECTSPRITETYPE_BLOODY_WALL_3)
		{
			if (++est > EFFECTSPRITETYPE_BLOODY_WALL_3) est = EFFECTSPRITETYPE_BLOODY_WALL_1;
		}
		// Refresh after every attempt, including the last, using the original blit type.
		if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) return bOK;
		frameID = sprite.frameID;
		if (!ReadMaxFrames(bltType, frameID, maxFrame)) return bOK;
	}
	return bOK;
}
